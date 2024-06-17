#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "lgi/common/Gdc2.h"
#include "lgi/common/Ftp.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/LgiCommon.h"
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/Variant.h"

#include "FtpListParser.cpp"

///////////////////////////////////////////////////////////////////
enum FtpTransferMode
{
	FT_Unknown,
	FT_Ascii,
	FT_Binary
};

struct IFtpPrivate
{
	char OutBuf[256] = "";
	char InBuf[256] = "";
	List<char> In;
	FtpTransferMode CurMode = FT_Unknown;
	LAutoPtr<LSocket> Listen;	// listen for data
	LAutoPtr<LSocketI> Data;	// get data
	LAutoPtr<LFile> F;
	LString Charset = DefaultFtpCharset;
	LString ErrBuf;
	IFtpCallback *Callback = NULL;
	bool UseTLS = false;
	
	LString Host;
	int Port = 0;

	IFtpPrivate(IFtpCallback *cb) : Callback(cb)
	{
	}
};

///////////////////////////////////////////////////////////////////
int LookupMonth(char *m)
{
	for (int i=0; i<12; i++)
	{
		if (m && _stricmp(m, LDateTime::MonthsShort[i]) == 0)
			return i + 1;
	}
	return 1;
}

///////////////////////////////////////////////////////////////////
IFtpEntry::IFtpEntry()
{
	Attributes = 0;
	Size = 0;
	UserData = NULL;
}

IFtpEntry::IFtpEntry(IFtpEntry *Entry)
{
	UserData = NULL;
	if (Entry)
		*this = *Entry;
}

bool IFtpEntry::PermissionsFromStr(const char *perm)
{
	if (!perm)
		return false;
	if (strlen(perm) < 10)
		return false;

	Perms.IsWindows = false;
	Perms.u32 = 0;

	if (perm[0] == 'd' ||
		perm[0] == 'l') Attributes |= IFTP_DIR;
			
	Perms.Unix.UserRead = perm[1] != '-';
	Perms.Unix.UserWrite = perm[2] != '-';
	Perms.Unix.UserExecute = perm[3] != '-';

	Perms.Unix.GroupRead = perm[4] != '-';
	Perms.Unix.GroupWrite = perm[5] != '-';
	Perms.Unix.GroupExecute = perm[6] != '-';

	Perms.Unix.GlobalRead = perm[7] != '-';
	Perms.Unix.GlobalWrite = perm[8] != '-';
	Perms.Unix.GlobalExecute = perm[9] != '-';

	if (perm[0] == 'l')
	{
		// link...
		Attributes |= IFTP_SYM_LINK;
		if (Name)
		{
			ssize_t Arrow = Name.Find("->");
			if (Arrow > 0)
				Name.Length(Arrow-1);
		}
	}

	return true;
}

IFtpEntry::IFtpEntry(struct ftpparse *Fp, const char *Cs)
{
	UserData = NULL;
	Attributes = 0;
	Size = 0;
	if (Fp)
	{
		Name.Set(Fp->name, Fp->namelen);
		
		Size = (Fp->sizetype == FTPPARSE_SIZE_BINARY) ? Fp->size : -1;
		
		if (Fp->mtimetype != FTPPARSE_MTIME_UNKNOWN)
		{
			#ifdef _MSC_VER
			struct tm local_val, *local = &local_val;
			auto err = localtime_s(local, &Fp->mtime);
			#else
			struct tm *local = localtime(&Fp->mtime);
			#endif
			if (local)
				Date = local;
		}

		PermissionsFromStr(Fp->perms);
	}
}

IFtpEntry::IFtpEntry(char *Entry, const char *Cs)
{
	UserData = NULL;
	Attributes = 0;
	Size = 0;
	if (Entry)
	{
		const char *Ws = " \t";
		auto T = LString(Entry).SplitDelimit(Ws);
		if (T.Length() > 0)
		{
			char *_Size = 0, *_Perm = 0;
			int _Name = 0;

			if (isdigit(*T[0]))
			{
				// M$ format
				_Size = T[2];
				if (_stricmp(_Size, "<dir>") == 0)
				{
					Attributes |= IFTP_DIR;
				}

				_Name = 3;
				Date.SetDate(T[0]);
				Date.SetTime(T[1]);
			}
			else
			{
				// Unix format
				int SizeElement = T.Length() > 4 && isdigit(*T[4]) ? 4 : 3;
				_Size = T.Length() > SizeElement ? T[SizeElement].Get() : NULL;
				_Name = (int)T.Length() - 1;
				_Perm = T[0];
				if (T.Length() > SizeElement-2)
					User = T[SizeElement-2];
				if (T.Length() > SizeElement-1)
					Group = T[SizeElement-1];

				char *MonthStr = T.Length() > SizeElement + 1 ? T[SizeElement+1].Get() : NULL;
				char *YearOrTime = T.Length() > SizeElement + 3 ? T[SizeElement+3].Get() : NULL;
				if (MonthStr && YearOrTime)
				{
					int Day = T.Length() > SizeElement+2 ? atoi(T[SizeElement+2]) : -1;
					int Month = LookupMonth(MonthStr);
					int Year = 0;

					if (strchr(YearOrTime, ':'))
					{
						// It's time
						Date.SetTime(YearOrTime);

						// Default the year
						#if defined(WIN32) && !defined(PLATFORM_MINGW)
						__time64_t now = _time64(NULL);
						struct tm ptm;
						errno_t err = _localtime64_s(&ptm, &now);
						if (err == 0)
							Year = 1900 + ptm.tm_year;
						#else
						time_t now = time(NULL);
						tm *ptm = localtime(&now);
						if (ptm)
							Year = 1900 + ptm->tm_year;
						#endif
					}
					else 
					{
						// It's a year
						Year = atoi(YearOrTime);
					}

					char DateStr[64];
					sprintf_s(DateStr, sizeof(DateStr), "%i/%i/%i", Day, Month, Year);
					Date.SetDate(DateStr);
				}
			}

			if (_Name)
			{
				char *n = Entry;
				for (int i=0; i<_Name; i++)
				{
					for (; *n && !strchr(Ws, *n); n++);
					for (; *n && strchr(Ws, *n); n++);
				}
				
				LAutoString Utf((char*) LNewConvertCp("utf-8", n, Cs));
				Name = Utf.Get();
			}

			if (_Perm && strlen(_Perm) == 10)
				PermissionsFromStr(_Perm);

			if (_Size)
			{
				Size = atoi64(_Size);
			}
		}
	}
}

IFtpEntry::~IFtpEntry()
{
}

IFtpEntry &IFtpEntry::operator =(const IFtpEntry &e)
{
	Attributes = e.Attributes;
	Perms = e.Perms;
	Size = e.Size;
	Date = e.Date;
	UserData = e.UserData;

	// Make copies for thread safety
	Name = e.Name.Get();
	Path = e.Path.Get();
	User = e.User.Get();
	Group = e.Group.Get();
	
	return *this;
}

///////////////////////////////////////////////////////////////////
#define Verify(i, ret) { ssize_t Code = i; if (Code != (ret)) throw Code; }
#define VerifyRange(i, range) { ssize_t Code = i; if ((Code/100) != range) throw Code; }

IFtp::IFtp(LCancel *cancel, IFtpCallback *cb, bool useTLS) : IFileProtocol(cancel)
{
	d = new IFtpPrivate(cb);
	d->UseTLS = useTLS;	
}

IFtp::~IFtp()
{
	DeleteObj(d);
}

const char *IFtp::GetError()
{
	return d->ErrBuf;
}

LAutoString IFtp::ToFtpCs(const char *s)
{
	return LAutoString((char*)LNewConvertCp(GetCharset(), s, "utf-8"));
}

LAutoString IFtp::FromFtpCs(const char *s)
{
	return LAutoString((char*)LNewConvertCp("utf-8", s, GetCharset()));
}

bool IFtp::GetUseTLS()
{
	return d->UseTLS;
}

void IFtp::SetUseTLS(bool b)
{
	d->UseTLS = b;
}

const char *IFtp::GetCharset()
{
	return d->Charset;
}

void IFtp::SetCharset(const char *cs)
{
	d->Charset = cs ? cs : DefaultFtpCharset;
}

ssize_t IFtp::WriteLine(char *Msg)
{
	ssize_t Status = 0;

	if (IsOpen())
	{
		char *b = Msg ? Msg : d->OutBuf;
		size_t l = strlen(b);
		Status = Socket->Write(b, l, 0);
	}

	return Status;	
}

ssize_t IFtp::ReadLine(char *Msg, ssize_t MsgSize)
{
	ssize_t Status = 0;
	while (Socket)
	{
		// look through the input list for result codes
		char *s;
		auto It = d->In.begin();
		while ((s = *It))
		{
			int i = 0;
			if (isdigit(*s))
			{
				char *e = s;
				while (isdigit(*e)) e++;

				if (*e != '-')
				{
					i = atoi(s);

					if (Msg)
					{
						char *Sp = strchr(s, ' ');
						if (Sp)
						{
							strcpy_s(Msg, MsgSize, Sp + 1);
						}
					}
				}
			}

			d->In.Delete(It);
			d->ErrBuf.Empty();
			
			if (i)
			{
				return i;
			}
		}

		// Ok, no result code in the input list so
		// Read data off socket
		ssize_t Len = strlen(d->InBuf);
		ssize_t Read = 0;

		if (IsOpen())
		{
			Read = Socket->Read(d->InBuf+Len, sizeof(d->InBuf)-Len-1, 0);
		}

		if (Read > 0)
		{
			// process input into lines
			char *Eol = 0;
			d->InBuf[Len+Read] = 0;
			do
			{
				Eol = strstr(d->InBuf, "\r\n");
				if (Eol)
				{
					// copy line into input list
					char *New = 0;
					d->In.Insert(New = NewStr(d->InBuf, Eol-d->InBuf));
					Eol += 2;

					// move data after line up to the start of the buffer
					ssize_t EolLen = strlen(Eol);
					if (EolLen > 0)
					{
						memmove(d->InBuf, Eol, EolLen+1);
					}
					else
					{
						d->InBuf[0] = 0;
						Eol = 0;
					}
				}
			}
			while (Eol);
		}
		else break; // the read failed....

	} // loop around to check results

	return Status;
}

bool IFtp::IsOpen()
{
	return Socket ? Socket->IsOpen() : false;
}

void IFtp::GetHost(LString *Host, int *Port)
{
	*Host = d->Host.Get();
	*Port = d->Port;
}

FtpOpenStatus IFtp::Open(LSocketI *S, char *RemoteHost, int Port, char *User, char *Password)
{
	const char *CurCmd = NULL;
	try
	{
		SslSocket *SslSock = dynamic_cast<SslSocket*>(S);
		if (d->UseTLS && !SslSock)
		{
			LAssert(!"Must use SslSocket for TLS based connections.");
			d->ErrBuf = "Must use SslSocket for TLS based connections.";
			return FO_Error;
		}

		Socket.Reset(S);
		if (S->GetTimeout() < 0)
			S->SetTimeout(15 * 1000);
		Authenticated = false;	

		if (!Port)
			Port = 21;

		d->Host = RemoteHost;
		d->Port = Port;

		if (!Socket || !Socket->Open(RemoteHost, Port))
		{
			d->ErrBuf = "Socket failed to connect.";
			return FO_ConnectFailed;
		}

		if (d->Callback)
			d->Callback->OnSocketConnect();

		Verify(ReadLine(), 220);

		if (d->UseTLS)
		{
			// https://datatracker.ietf.org/doc/html/rfc4217
			// https://datatracker.ietf.org/doc/html/rfc2228

			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s\r\n", CurCmd = "AUTH TLS");
			if (!WriteLine())
			{
				d->ErrBuf = "Failed to write AUTH cmd.";
				return FO_Error;
			}

			auto Result = ReadLine();
			if (Result != 234)
			{
				d->ErrBuf.Printf("AUTH cmd responded with %i", Result);
				return FO_Error;
			}

			// Switch on TLS
			LVariant v = "ssl";
			auto IsSsl = SslSock->SetValue(LSocket_Protocol, v);
			if (!IsSsl)
			{
				d->ErrBuf.Printf("Couldn't set socket to SSL");
				return FO_Error;
			}

			// Protection buffer size...
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s 0\r\n", CurCmd = "PBSZ");
			if (!WriteLine())
			{
				d->ErrBuf.Printf("Couldn't write PBSZ command");
				return FO_Error;
			}
			Verify(ReadLine(), 200);

			// Protected data transfers...
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s\r\n", CurCmd = "PROT P");
			if (!WriteLine())
			{
				d->ErrBuf.Printf("Couldn't write PROT command");
				return FO_Error;
			}
			Verify(ReadLine(), 200);
		}

		if (User)
		{
			// User/pass
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s %s\r\n", CurCmd = "USER", User);
			if (!WriteLine())
			{
				d->ErrBuf.Printf("Couldn't write USER command");
				return FO_Error;
			}

			Verify(ReadLine(), 331);

			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s %s\r\n",
					CurCmd = "PASS",
					ValidStr(Password) ? Password : (char*)"");
			if (!WriteLine())
			{
				d->ErrBuf.Printf("Couldn't write PASS command");
				return FO_Error;
			}

			Verify(ReadLine(), 230);
			Authenticated = true;
		}
		else
		{
			// Anonymous
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s anonymous\r\n", CurCmd = "USER");
			if (WriteLine())
			{
				ssize_t Code = ReadLine();
				if (Code / 100 > 3)
					throw Code;

				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PASS me@privacy.net\r\n"); // garrenteed to never
														// be allocated to an IP
				if (WriteLine())
				{
					Verify(ReadLine(), 230);
					Authenticated = true;
				}
			}
			else
			{
				d->ErrBuf.Printf("Couldn't write USER command");
				return FO_Error;
			}
		}

		#if 0
		sprintf_s(d->OutBuf, sizeof(d->OutBuf), "FEAT\r\n");
		if (WriteLine())
		{
			int r = ReadLine();
			if (r / 100 == 2)
			{
				// Is utf8 there?
				// FIXME
			}
		}
		#endif

		if (!Authenticated)
		{
			d->ErrBuf = "Authentication failed.";
			return FO_LoginFailed;
		}

		return FO_Connected;
	}
	catch (ssize_t Error)
	{
		d->ErrBuf.Printf("Got error code " LPrintfSSizeT " from cmd %s", Error, CurCmd);
		LgiTrace("%s:%i - %s\n", _FL, d->ErrBuf.Get());
		return FO_Error;
	}
}

bool IFtp::Close()
{
	if (d->F)
		d->F->Close();
	
	if (d->Data)
		d->Data->Close();
	
	if (d->Listen)
		d->Listen->Close();
	
	if (Socket)
		Socket->Close();
	
	return true;
}

void IFtp::Noop()
{
	if (IsOpen())
	{
		sprintf_s(d->OutBuf, sizeof(d->OutBuf), "NOOP\r\n");
		if (WriteLine())
		{
			ReadLine();
		}
	}
}

LString IFtp::GetDir()
{
	LString p;

	try
	{
		if (IsOpen())
		{
			char Temp[256];

			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PWD\r\n");
			WriteLine();
			Verify(ReadLine(Temp, sizeof(Temp)), 257);

			char *Start = strchr(Temp, '\"');
			char *End = (Start) ? strchr(Start+1, '\"') : 0;
			if (End)
			{
				*End = 0;	
				p = Start + 1;
			}			
		}
	}
	catch (ssize_t Error)
	{
		LgiTrace("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
	}

	return p;
}

bool IFtp::SetDir(const char *Dir)
{
	bool Status = false;

	try
	{
		if (Dir && IsOpen())
		{
			auto f = ToFtpCs(Dir);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "CWD %s\r\n", f.Get());
				WriteLine();
				Verify(ReadLine(), 250);
				Status = true;
			}
		}
	}
	catch (ssize_t Error)
	{
		LgiTrace("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
	}

	return Status;
}

bool IFtp::CreateDir(const char *Dir)
{
	bool Status = false;

	try
	{
		if (IsOpen() && Dir)
		{
			auto f = ToFtpCs(Dir);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "MKD %s\r\n", f.Get());
				WriteLine();
				VerifyRange(ReadLine(), 2);
				Status = true;
			}
		}
	}
	catch (ssize_t Error)
	{
		printf("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
		if (IsOpen())
		{
			LAssert(0);
		}
	}

	return Status;
}

bool IFtp::DeleteDir(const char *Dir)
{
	bool Status = false;

	if (IsOpen() && Dir)
	{
		auto f = ToFtpCs(Dir);
		if (f)
		{
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "RMD %s\r\n", f.Get());
			WriteLine();
			VerifyRange(ReadLine(), 2);
			Status = true;
		}
	}

	return Status;
}

bool IFtp::UpDir()
{
	bool Status = false;

	try
	{
		if (IsOpen())
		{
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "CDUP\r\n");
			WriteLine();
			VerifyRange(ReadLine(0), 2);

			Status = true;
		}
	}
	catch (ssize_t Error)
	{
		printf("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
		if (IsOpen())
		{
			LAssert(0);
		}
	}

	return Status;
}

// #include "LgiCommon.h"
// #include <malloc.h>
#ifdef _INC_MALLOC
#define IsHeapOk() (_heapchk() != _HEAPOK)
#else
#define IsHeapOk() (-1)
#endif

bool IFtp::ListDir(LArray<IFtpEntry*> &Dir)
{
	bool Status = false;

	try
	{
		if (IsOpen() &&
			SetupData(true))
		{
			LStringPipe Buf;

			// List command
			strcpy_s(d->OutBuf, sizeof(d->OutBuf), "LIST");
			if (LongList || ShowHidden) strcat(d->OutBuf, " -");
			if (LongList) strcat(d->OutBuf, "l");
			if (ShowHidden) strcat(d->OutBuf, "a");
			strcat(d->OutBuf, "\r\n");
			WriteLine();
				
			// Accept the data connection
			if (ConnectData())
			{
				VerifyRange(ReadLine(), 1);

				if (Meter)
				{
					Meter->SetRange(0);
					Meter->Value(0);
				}

				// Read the data
				uchar Buffer[1024];
				ssize_t Len;
				
				while ((Len = d->Data->Read((char*) Buffer, sizeof(Buffer), 0)) > 0)
				{
					if (Meter)
					{
						Meter->Value(Buf.GetSize());
					}

					Buf.Write(Buffer, Len);
				}

				if (Meter)
				{
					Meter->Value(0);
				}
				
				Status = true;
			}
			
			d->Data.Reset();
			d->Listen.Reset();
			
			// Parse the results
			LString Text = Buf.NewLStr();
			if (Status)
			{
				if (Text)
				{
					#ifdef LOG_LISTINGS
					LFile F;
					if (F.Open("c:\\temp\\ftplog.txt", O_WRITE))
					{
						F.Write(Text, Len);
						F.Close();
					}
					#endif

					LString::Array Ln = Text.Split("\n");

					// Parse lines...
					for (unsigned i=0; i<Ln.Length(); i++)
					{
						LString Line = Ln[i].Strip();

						#if 1					
						struct ftpparse fp;					
						int r = ftpparse(&fp, Line, (int)Line.Length());
						if (!r)
							LgiTrace("Error: %s\n", Line.Get());
						IFtpEntry *e = r ? new IFtpEntry(&fp, GetCharset()) : NULL;
						#else
						IFtpEntry *e = new IFtpEntry(Line, GetCharset());
						#endif
						if (e && e->Name)
						{
							if (strcmp(e->Name, ".") != 0 &&
								strcmp(e->Name, "..") != 0)
							{
								Dir.Add(e);
								e = 0;
							}
						}
							
						DeleteObj(e);
					}
						
					VerifyRange(ReadLine(), 2);
					Status = true;
				}
				else
				{
					VerifyRange(ReadLine(), 2);
					Status = true;
				}
			}
		}
		else printf("%s:%i - SetupData failed.\n", _FL);
	}
	catch (ssize_t Error)
	{
		printf("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
		d->Data.Reset();
		d->Listen.Reset();
	}

	return Status;
}

bool IFtp::RenameFile(const char *From, const char *To)
{
	bool Status = false;

	try
	{
		if (IsOpen() && From && To)
		{
			auto f = ToFtpCs(From);
			auto t = ToFtpCs(To);
			if (f && t)
			{
				// From
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "RNFR %s\r\n", f.Get());
				WriteLine();
				VerifyRange(ReadLine(), 3);

				// To
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "RNTO %s\r\n", t.Get());
				WriteLine();
				VerifyRange(ReadLine(), 2);

				Status = true;
			}
		}
	}
	catch (ssize_t Error)
	{
		printf("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
		if (IsOpen())
		{
			LAssert(0);
		}
	}

	return Status;
}

bool IFtp::DeleteFile(const char *Remote)
{
	bool Status = false;

	if (IsOpen() && Remote)
	{
		auto f = ToFtpCs(Remote);
		if (f)
		{
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "DELE %s\r\n", f.Get());
			WriteLine();
			VerifyRange(ReadLine(), 2);
			Status = true;
		}
	}

	return Status;
}

bool IFtp::DownloadFile(const char *Local, IFtpEntry *Remote, bool Binary)
{
	if (Remote && Remote->Name)
	{
		return TransferFile(Local, Remote->Name, Remote->Size, false, Binary);
	}
	return false;
}

bool IFtp::UploadFile(const char *Local, const char *Remote, bool Binary)
{
	return TransferFile(Local, Remote, LFileSize(Local), true, Binary);
}

bool IFtp::ResumeAt(int64 Pos)
{
	sprintf_s(d->OutBuf, sizeof(d->OutBuf), "REST " LPrintfInt64 "\r\n", Pos);

	WriteLine();
	ssize_t FtpStatus = ReadLine();
	bool Status = (FtpStatus/100) < 4;
	RestorePos = (Status) ? Pos : 0;
	return Status;
}

bool IFtp::TransferFile(const char *Local, const char *Remote, int64 Size, bool Upload, bool Binary)
{
	bool Status = false;
	bool Aborted = false;

	d->F.Reset(new LFile);
	
	try
	{
		if (d->F &&
			Socket &&
			Local &&
			Remote)
		{
			if (SetupData(Binary, true))
			{
				// Data command
				auto f = ToFtpCs(Remote);
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s %s\r\n", (Upload)?"STOR":"RETR", f ? f.Get() : Remote);
				WriteLine();

				// Build data connection
				if (ConnectData())
				{
					ssize_t Result = ReadLine();
					ssize_t Range = Result / 100;
					if (Range != 1 && Range != 2)
						throw Result;

					if (!d->F->Open(Local, (Upload)?O_READ:O_WRITE))
					{
						// couldn't open file...
						char Str[256];
						sprintf_s(Str, sizeof(Str), "Error: Couldn't open '%s' for %s", Local, (Upload)?"reading":"writing");
						Socket->OnInformation(Str);
					}
					else
					{
						// do the transfer
						int TempLen = 64 << 10;
						uchar *Temp = new uchar[TempLen];
						if (Temp)
						{
							int64 Processed = 0;
							ssize_t Len = 0;

							if (Meter)
								Meter->SetRange(Size);

							if (!Upload)
								Size -= RestorePos;
							AbortTransfer = false;

							bool Error = false;
							while (	Socket &&
									d->Data &&
									(Size < 0 || Processed < Size) &&
									!Error &&
									!AbortTransfer &&
									(!Meter || !Meter->IsCancelled()))
							{
								if (Meter)
								{
									Meter->Value(0);
								}

								if (Upload)
								{
									// upload loop
									d->Data->SetTimeout(15 * 1000);
									do
									{
										auto filePos = d->F->GetPos();
										Len = d->F->Read(Temp, TempLen);
										for (ssize_t i=0; i<Len; )
										{
											ssize_t WriteLen = 0;
											
											if (d->Data)
												WriteLen = d->Data->Write((char*) Temp, Len, 0);
											else
												break;

											LgiTrace("data write %i, %i, %i\n", (int)filePos, (int)Len, (int)WriteLen);
											
											if (WriteLen > 0)
											{
												Processed += WriteLen;
												i += WriteLen;
												if (Meter)
													Meter->Value(Meter->Value() + WriteLen);
											}
											else
											{
												printf("%s:%i - Data->Write failed, %i of %i bytes written.\n",
													_FL, (int)WriteLen, (int)Len);
												Error = true;
												break;
											}
										}

										if (Meter && Meter->IsCancelled())
										{
											printf("%s:%i - Upload canceled.\n", _FL);
											break;
										}
									}
									while (Len > 0 && !AbortTransfer && !Error);
								}
								else
								{
									// restore support
									d->F->Seek(RestorePos, SEEK_SET);
									if (Meter && RestorePos > 0)
									{
										// Meter->SetParameter(10, (int)RestorePos);
										LVariant v;
										Meter->SetValue("RestorePos", v = RestorePos);
									}
									RestorePos = 0;

									// download loop
									while (d->Data)
									{
										if (d->Data)
										{
											Len = d->Data->Read((char*) Temp, TempLen, 0);
										}

										if (Len <= 0 || AbortTransfer)
										{
											break;
										}

										d->F->Write(Temp, Len);
										Processed += Len;
										if (Meter)
										{
											Meter->Value(Meter->Value() + Len);

											if (Meter->IsCancelled())
												break;
										}
									}

									if (Len <= 0)
										Error = true;
								}
							}
							
							if (AbortTransfer || (Meter && Meter->IsCancelled()))
							{
								// send abort command
								sprintf_s(d->OutBuf, sizeof(d->OutBuf), "ABOR\r\n");
								WriteLine();
								if ((ReadLine()/100) == 4) // temp msg
								{
									// response... we don't care just read it
									ReadLine();
								}

								Aborted = true;
							}

							if (d->Data)
								d->Data->Close();

							if (Size < 0)
								Status = true;
							else
								Status = Size == Processed;

							if (Meter)
								Meter->Value(0);

							DeleteArray(Temp);
						}

						// read eof response
						if (!Aborted)
						{
							VerifyRange(ReadLine(), 2);
						}
					}
				}

				d->Data.Reset();
				d->Listen.Reset();
			}
		}
	}
	catch (ssize_t Error)
	{
		if (IsOpen())
		{
			d->ErrBuf.Printf("%s:%i - TransferFile(%s) error: " LPrintfSSizeT "\n",
				_FL,
				Local,
				Error);			
		}

		Status = false;
	}

	d->F.Reset();
	return Status;
}

bool IFtp::SetupData(bool Binary, bool Debug)
{
	bool Status = false;
	try
	{
		d->Data.Reset();

		if (IsOpen())
		{
			if (Socket)
			{
				auto nstream = Socket->Clone();
				auto nsock = dynamic_cast<LSocketI*>(nstream);
				LAssert(nsock != NULL);
				d->Data.Reset(nsock);
			}
			else
			{
				if (d->Data.Reset(new LSocket()))
					d->Data->SetCancel(Socket->GetCancel());
			}
		}

		if (!d->Data)
		{
			d->ErrBuf.Printf("No data socket, Socket=%p.", Socket.Get());
			LgiTrace("%s:%i - %s\n", _FL, d->ErrBuf.Get());
		}
		else
		{
			PassiveMode = false;

			if (Debug)
			{
				LVariant True = true;
				d->Data->SetValue("DebugLogging", True);
			}

			if ( (Binary && d->CurMode != FT_Binary) ||
				(!Binary && d->CurMode != FT_Ascii))
			{
				// Type command
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "TYPE %c\r\n", Binary ? 'I' : 'A');
				if (WriteLine())
				{
					VerifyRange(ReadLine(), 2);
					d->CurMode = Binary ? FT_Binary : FT_Ascii;
				}
			}

			if (!ForceActive)
			{
				// Try the PASV command first... it's better for
				// getting out of a firewall to the server
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PASV\r\n");
				WriteLine();

				char Temp[256];
				if (ReadLine(Temp, sizeof(Temp))/100 == 2)
				{
					// Ok we have PASV mode
					// grab the IP and Port
					char *Start = strchr(Temp, '(');
					if (Start)
					{
						Start++;
						char *End = strchr(Start, ')');
						if (End)
						{
							*End = 0;
							auto T = LString(Start).SplitDelimit(",");
							if (T.Length() == 6)
							{
								sprintf_s(Ip, sizeof(Ip), "%s.%s.%s.%s", T[0].Get(), T[1].Get(), T[2].Get(), T[3].Get());
								Port = (int) ((T[4].Int()<<8) | T[5].Int());
								PassiveMode = true;
								return true;
							}
						}
					}
				}
				else
				{
					printf("\tread failed.\n");
					return false;
				}
			}

			// Ok, no PASV so we use the PORT command

			// Port command
			// static int p = (4<<8) | 27;

			bool ListenStatus = false;
			d->Listen.Reset(new LSocket);			// listen
			if (d->Listen)
			{
				int To = Socket->GetTimeout();
				d->Listen->SetTimeout(To);
				if (d->Listen->Listen())
				{
					Socket->GetLocalIp(Ip);				// get current IP
					Port = d->Listen->GetLocalPort();		// get current port

					ListenStatus = true;
				}
			}

			if (ListenStatus)
			{
				// convert IP to PORT's comma delimited format
				for (char *p = Ip; *p; p++)
				{
					if (*p == '.')
					{
						*p = ',';
					}
				}

				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PORT %s,%i,%i\r\n", Ip, Port>>8, Port&0xFF);
				WriteLine();
				VerifyRange(ReadLine(), 2);
				Status = true;
			}
		}
	}
	catch (ssize_t Error)
	{
		LgiTrace("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
	}

	return Status;
}

bool IFtp::ConnectData()
{
	if (PassiveMode)
	{
		return d->Data->Open(Ip, Port) != 0;
	}
	else
	{
		d->Data.Reset(Socket ? dynamic_cast<LSocket*>(Socket->Clone()) : new LSocket());
		if (d->Data)
		{
			int To = Socket->GetTimeout();
			d->Data->SetTimeout(To);
			return d->Listen->Accept(d->Data);
		}
	}

	return false;
}

bool IFtp::SetPerms(const char *File, LPermissions Perms)
{
	bool Status = false;

	if (Perms.IsWindows)
	{
		LAssert(!"Wrong perms type.");
		return false;
	}

	try
	{
		if (IsOpen() && File)
		{
			auto f = ToFtpCs(File);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "SITE CHMOD %03X %s\r\n", Perms.u32 & 0x777, f.Get());
				WriteLine();
				VerifyRange(ReadLine(), 2);
				Status = true;
			}
		}
	}
	catch (ssize_t Error)
	{
		printf("%s:%i - error: " LPrintfSSizeT "\n", _FL, Error);
		if (IsOpen())
		{
			LAssert(0);
		}
	}

	return Status;
}
