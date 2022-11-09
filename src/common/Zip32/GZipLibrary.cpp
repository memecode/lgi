#include "Lgi.h"
#include "LZipLibrary.h"

#ifdef WIN32
class LZipLibPrivate : public LLibrary
{
public:
	typedef void	(EXPENTRY *_ZpVersion)(ZpVer far *);
	typedef int		(EXPENTRY *_ZpInit)(ZIPUSERFUNCTIONS far * lpZipUserFunc);
	typedef BOOL	(EXPENTRY *_ZpSetOptions)(ZPOPT *Opts);
	typedef ZPOPT	(EXPENTRY *_ZpGetOptions)(void);
	typedef int		(EXPENTRY *_ZpArchive)(ZCL C);

	_ZpVersion ZpVersion;
	_ZpInit ZpInit;
	_ZpSetOptions ZpSetOptions;
	_ZpGetOptions ZpGetOptions;
	_ZpArchive ZpArchive;

	ZIPUSERFUNCTIONS UserFuncs;
	int InitResult;

	static LLogTarget *Log;

	static int WINAPI DllPrint(LPSTR str, unsigned long i)
	{
		if (Log)
		{
			char Buf[256];
			char *Out = Buf;
			for (char *In = str; 1; In++)
			{
				if (!*In OR *In == '\n')
				{
					*Out++ = 0;

					if (Buf[0] AND
						!stristr(Buf, "adding:") AND
						!stristr(Buf, "deflated"))
					{
						Log->Log(Buf, stristr(Buf, "zip error") ? Rgb24(128, 0, 0) : Rgb24(0x80, 0x80, 0x80));
					}

					Out = Buf;

					if (!*In) break;
				}
				else if (*In == '\t')
				{
					*Out++ = ' ';
					*Out++ = ' ';
					*Out++ = ' ';
				}
				else
				{
					*Out++ = *In;
				}
			}
		}
		return 0;
	}

	static int WINAPI DllComment(LPSTR s)
	{
		return 0;
	}

	static int WINAPI DllPassword(LPSTR str, int i, LPCSTR a, LPCSTR b)
	{
		return 0;
	}

	static int WINAPI DllService(LPCSTR str, unsigned long i)
	{
		return 0;
	}
	
	LZipLibPrivate() : LLibrary("Zip32")
	{
		#ifdef _DEBUG
		if (!IsLoaded())
		{
			// try some other paths...
			if (!Load("\\CodeLib\\Zip23\\windll\\Debug\\Zip32"))
			{
				Load("..\\Zip\\Zip32");
			}
		}
		#endif

		UserFuncs.print = DllPrint;
		UserFuncs.comment = DllComment;
		UserFuncs.password = DllPassword;
		UserFuncs.ServiceApplication = DllService;
		InitResult = 0;

		if (IsLoaded())
		{
			ZpVersion =		(_ZpVersion)	GetAddress("ZpVersion");
			ZpInit =		(_ZpInit)		GetAddress("ZpInit");
			ZpSetOptions =	(_ZpSetOptions)	GetAddress("ZpSetOptions");
			ZpGetOptions =	(_ZpGetOptions)	GetAddress("ZpGetOptions");
			ZpArchive =		(_ZpArchive)	GetAddress("ZpArchive");

			if (IsOk())
			{
				InitResult = ZpInit(&UserFuncs);
			}
		}
	}
	
	bool IsOk()
	{
		bool Status =	IsLoaded() AND
						ZpVersion AND
						ZpInit AND
						ZpSetOptions AND
						ZpGetOptions AND
						ZpArchive;
		LAssert(Status);
		return Status;
	}

	bool Zip(char *ZipFile, char *BaseDir, List<char> &Extensions, char *AfterDate)
	{
		bool Status = false;
		if (ZipFile AND
			BaseDir AND
			IsOk())
		{
			ZPOPT Opt;

			ZeroObj(Opt);
			Opt.fRecurse = 2;
			Opt.szRootDir =  BaseDir;
			Opt.Date = AfterDate;
			Opt.fIncludeDate = AfterDate != 0;

			ZpSetOptions(&Opt);

			char **Ext = new char*[Extensions.Length()];
			if (Ext)
			{
				int n=0;
				for (char *e=Extensions.First(); e; e=Extensions.Next())
				{
					Ext[n++] = e;
				}

				ZCL Cmd;
				Cmd.argc = Extensions.Length();
				Cmd.lpszZipFN = ZipFile;
				Cmd.FNV = Ext;

				Status = !ZpArchive(Cmd);
			}
		}

		return Status;
	}
};
#else
#include "LToken.h"
class LZipLibPrivate
{
	bool HasZip;

public:
	static LLogTarget *Log;
	
	LZipLibPrivate()
	{
		HasZip = 0;
		
		LToken Path(getenv("PATH"), ":");
		for (int i=0; i<Path.Length(); i++)
		{
			char p[256];
			LMakePath(p, sizeof(p), Path[i], "zip");
			if (FileExists(p))
			{
				HasZip = true;
				break;
			}
		}
	}
	
	bool IsOk()
	{
		return HasZip;
	}

	bool Zip(char *ZipFile, char *BaseDir, List<char> &Extensions, char *AfterDate)
	{
		bool Status = false;
		
		if (ZipFile AND
			BaseDir AND
			IsOk())
		{
			char Cmd[512] = "zip -r -9";
			
			if (AfterDate)
			{
				sprintf(Cmd+strlen(Cmd), " -t %s", AfterDate);
			}
			sprintf(Cmd+strlen(Cmd), " %s . -i", ZipFile, BaseDir);
			for (char *e = Extensions.First(); e; e = Extensions.Next())
			{
				sprintf(Cmd+strlen(Cmd), " '%s'", e);
			}
			
			chdir(BaseDir);
			int r = system(Cmd);
			printf("Executing '%s' = %i\n", Cmd, r);
			Status = r == 0;
		}
		
		return Status;
	}
};
#endif

//////////////////////////////////////////////////////////////////////////////
// Zip lib support

LLogTarget *LZipLibPrivate::Log = 0;

LZipLibrary::LZipLibrary(LLogTarget *log)
{
	d = new LZipLibPrivate;
	d->Log = log;
}

bool LZipLibrary::IsOk()
{
	return d->IsOk();
}

bool LZipLibrary::Zip(char *ZipFile, char *BaseDir, List<char> &Extensions, char *AfterDate)
{
	return d->Zip(ZipFile, BaseDir, Extensions, AfterDate);
}

