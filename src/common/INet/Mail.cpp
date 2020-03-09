/*hdr
**      FILE:           Mail.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           28/5/98
**      DESCRIPTION:    Mail app
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "Lgi.h"
#include "Mail.h"
#include "GToken.h"
#include "Base64.h"
#include "INetTools.h"
#include "LDateTime.h"
#include "GDocView.h"
#include "Store3Defs.h"
#include "LgiRes.h"
#include "../Hash/md5/md5.h"

const char *sTextPlain = "text/plain";
const char *sTextHtml = "text/html";
const char *sTextXml = "text/xml";
const char *sApplicationInternetExplorer = "application/internet-explorer";
const char sMultipartMixed[] = "multipart/mixed";
const char sMultipartEncrypted[] = "multipart/encrypted";
const char sMultipartSigned[] = "multipart/signed";
const char sMultipartAlternative[] = "multipart/alternative";
const char sMultipartRelated[] = "multipart/related";
const char sAppOctetStream[] = "application/octet-stream";

//////////////////////////////////////////////////////////////////////////////////////////////////
LogEntry::LogEntry(GColour col)
{
	c = col;
}

bool LogEntry::Add(const char *t, ssize_t len)
{
	if (!t)
		return false;

	if (len < 0)
		len = strlen(t);

	/*
	// Strip off any whitespace on the end of the line.
	while (len > 0 && strchr(" \t\r\n", t[len-1]))
		len--;
	*/

	GAutoWString w(Utf8ToWide(t, len));
	if (!w)
		return false;

	size_t ch = StrlenW(w);
	return Txt.Add(w, ch);	
}
	
bool Base64Str(GString &s)
{
	GString b64;
	ssize_t Base64Len = BufferLen_BinTo64(s.Length());
	if (!b64.Set(NULL, Base64Len))
		return false;
	
	#ifdef _DEBUG
	ssize_t Ch =
	#endif
	ConvertBinaryToBase64(b64.Get(), b64.Length(), (uchar*)s.Get(), s.Length());
	LgiAssert(Ch == b64.Length());
	s = b64;
	return true;
}

bool UnBase64Str(GString &s)
{
	GString Bin;
	ssize_t BinLen = BufferLen_64ToBin(s.Length());
	if (!Bin.Set(NULL, BinLen))
		return false;
	
	ssize_t Ch = ConvertBase64ToBinary((uchar*)Bin.Get(), Bin.Length(), s.Get(), s.Length());
	LgiAssert(Ch <= (int)Bin.Length());
	s = Bin;
	s.Get()[Ch] = 0;
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// return true if there are any characters with the 0x80 bit set
bool Is8Bit(char *Text)
{
	if (!Text)
		return false;

	while (*Text)
	{
		if (*Text & 0x80)
			return true;
		Text++;
	}

	return false;
}

// returns the maximum length of the lines contained in the string
int MaxLineLen(char *Text)
{
	if (!Text)
		return false;

	int Max = 0;
	int i = 0;
	for (char *c = Text; *c; c++)
	{
		if (*c == '\r')
		{
			// return
		}
		else if (*c == '\n')
		{
			// eol
			Max = MAX(i, Max);
			i = 0;
		}
		else
		{
			// normal char
			i++;
		}
	}

	return Max;
}

bool IsDotLined(char *Text)
{
	if (Text)
	{
		for (char *l = Text; l && *l; )
		{
			if (l[0] == '.')
			{
				if (l[1] == '\n' || l[1] == 0)
				{
					return true;
				}
			}

			l = strchr(l, '\n');
			if (l) l++;
		}
	}

	return false;
}

char ConvHexToBin(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c + 10 - 'a';
	if (c >= 'A' && c <= 'F')
		return c + 10 - 'A';
	return 0;
}

// Is s a valid non-whitespace string?
bool ValidNonWSStr(const char *s)
{
	if (s && *s)
	{
		while (*s && strchr(" \r\t\n", *s))
		{
			s++;
		}

		if (*s)
		{
			return true;
		}
	}

	return false;
}

void TokeniseStrList(char *Str, List<char> &Output, const char *Delim)
{
	if (Str && Delim)
	{
		char *s = Str;
		while (*s)
		{
		    while (*s && strchr(WhiteSpace, *s))
		        s++;

			char *e = s;
			for (; *e; e++)
			{
				if (strchr("\'\"", *e))
				{
					// handle string constant
					char delim = *e++;
					e = strchr(e, delim);
				}
				else if (*e == '<')
				{
					e = strchr(e, '>');
				}
				else
				{
					while (*e && *e != '<' && !IsWhiteSpace(*e) && !strchr(Delim, *e))
						e++;
				}

				if (!e || !*e || strchr(Delim, *e))
				{
					break;
				}
			}

			ssize_t Len = e ? e - s : strlen(s);
			if (Len > 0)
			{
				char *Temp = new char[Len+1];
				if (Temp)
				{
					memcpy(Temp, s, Len);
					Temp[Len] = 0;
					Output.Insert(Temp);
				}
			}

			if (e)
			{
				s = e;
				for (; *s && strchr(Delim, *s); s++);
			}
			else break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
char *DecodeBase64Str(char *Str, int Len)
{
	if (Str)
	{
		ssize_t B64Len = (Len < 0) ? strlen(Str) : Len;
		ssize_t BinLen = BufferLen_64ToBin(B64Len);
		char *s = new char[BinLen+1];
		if (s)
		{
			ssize_t Converted = ConvertBase64ToBinary((uchar*)s, BinLen, Str, B64Len);
			s[Converted] = 0;
			DeleteArray(Str);
			Str = s;
		}
	}
	return Str;
}

char *DecodeQuotedPrintableStr(char *Str, ssize_t Len)
{
	if (Str)
	{
		if (Len < 0) Len = strlen(Str);
		uchar *s = new uchar[Len+1];
		if (s)
		{
			char *Out = (char*) s;
			char *Text = Str;

			for (int i=0; i<Len; i++)
			{
				if (Text[i] == '=')
				{
					if (Text[i+1] == '\r' || Text[i+1] == '\n')
					{
						if (Text[i+1] == '\r') i++;
						if (Text[i+1] == '\n') i++;
					}
					else
					{
						i++;
						char c = ConvHexToBin(Text[i]) << 4;
						i++;
						c |= ConvHexToBin(Text[i]) & 0xF;

						*Out++ = c;
					}
				}
				else
				{
					*Out++ = Text[i];
				}
			}

			*Out++ = 0;
			DeleteArray(Str);
			Str = (char*) s;
		}
	}
	return Str;
}

char *DecodeRfc2047(char *Str)
{
	if (!Str)
		return NULL;
	
	GStringPipe p(256);
	for (char *s = Str; *s; )
	{
		char *e = s;
		bool Decode = 0, Descape = 0;
		while (*e)
		{
			if
			(
				(Decode = (e[0] == '=' && e[1] == '?'))
				||
				(Descape = (e[0] == '\\'))
			)
			{
				// Emit characters between 's' and 'e'
				if (e > s)
					p.Write(s, e - s);
				break;
			}
			e++;
		}

		if (Decode)
		{
			// is there a word remaining
			bool Encoded = false;
			char *Start  = e + 2;
			char *First  = strchr(Start, '?');
			char *Second = First ? strchr(First + 1, '?') : NULL;
			char *End    = Second ? strstr(Second + 1, "?=") : NULL;
			if (End)
			{
				GString Cp(Start, First - Start);
				int Type = CONTENT_NONE;
				bool StripUnderscores = false;
				if (ToUpper(First[1]) == 'B')
				{
					// Base64 encoding
					Type = CONTENT_BASE64;
				}
				else if (ToUpper(First[1]) == 'Q')
				{
					// Quoted printable
					Type = CONTENT_QUOTED_PRINTABLE;
					StripUnderscores = true;
				}

				if (Type != CONTENT_NONE)
				{
					Second++;					
					char *Block = NewStr(Second, End-Second);
					if (Block)
					{
						switch (Type)
						{
							case CONTENT_BASE64:
								Block = DecodeBase64Str(Block);
								break;
							case CONTENT_QUOTED_PRINTABLE:
								Block = DecodeQuotedPrintableStr(Block);
								break;
						}

						size_t Len = strlen(Block);
						if (StripUnderscores)
						{
							for (char *i=Block; *i; i++)
							{
								if (*i == '_')
									*i = ' ';
							}
						}

						if (Cp && !_stricmp(Cp, "utf-8"))
						{
							p.Write((uchar*)Block, Len);
						}
						else
						{
							GAutoString Utf8((char*)LgiNewConvertCp("utf-8", Block, Cp, Len));
							if (Utf8)
							{
								if (LgiIsUtf8(Utf8))
									p.Write((uchar*)Utf8.Get(), strlen(Utf8));
							}
							else
							{
								p.Write((uchar*)Block, Len);
							}
						}

						DeleteArray(Block);
					}
					
					s = End + 2;
					if (*s == '\n')
					{
						s++;
						while (*s && strchr(WhiteSpace, *s)) s++;
					}
					
					Encoded = true;
				}
			}

			if (!Encoded)
			{
				// Encoding error, just emit the raw string and exit.
				size_t Len = strlen(s);
				p.Write((uchar*) s, Len);
				break;
			}
		}
		else if (Descape)
		{
			// Un-escape the string...
			e++;
			if (*e)
				p.Write(e, 1);
			else
				break;
			s = e + 1;
		}
		else
		{
			// Last segment of string...
			LgiAssert(*e == 0);
			if (e > s)
				p.Write(s, e - s);
			break;
		}
	}

	DeleteArray(Str);
	return p.NewStr();
}

#define MIME_MAX_LINE		76

char *EncodeRfc2047(char *Str, const char *CodePage, List<char> *CharsetPrefs, ssize_t LineLength)
{
	if (!CodePage)
	{
		CodePage = "utf-8";
	}

	GStringPipe p(256);

	if (!Str)
		return NULL;

	if (Is8Bit(Str))
	{
		// pick an encoding
		bool Base64 = false;
		const char *DestCp = "utf-8";
		size_t Len = strlen(Str);;
		if (_stricmp(CodePage, "utf-8") == 0)
		{
			DestCp = LgiDetectCharset(Str, Len, CharsetPrefs);
		}

		int Chars = 0;
		for (unsigned i=0; i<Len; i++)
		{
			if (Str[i] & 0x80)
				Chars++;
		}
		if
		(
			stristr(DestCp, "utf") ||
			(
				Len > 0 &&
				((double)Chars/Len) > 0.4
			)
		)
		{
			Base64 = true;
		}

		char *Buf = (char*)LgiNewConvertCp(DestCp, Str, CodePage, Len);
		if (Buf)
		{
			// encode the word
			char Prefix[64];
			int Ch = sprintf_s(Prefix, sizeof(Prefix), "=?%s?%c?", DestCp, Base64 ? 'B' : 'Q');
			p.Write(Prefix, Ch);
			LineLength += Ch;

			if (Base64)
			{
				// Base64
				size_t InLen = strlen(Buf);
				// int EstBytes = BufferLen_BinTo64(InLen);

				char Temp[512];
				ssize_t Bytes = ConvertBinaryToBase64(Temp, sizeof(Temp), (uchar*)Buf, InLen);
				p.Push(Temp, Bytes);
			}
			else
			{
				// Quoted printable
				for (char *w = Buf; *w; w++)
				{
					if (*w == ' ')
					{
						if (LineLength > MIME_MAX_LINE - 3)
						{
							p.Print("?=\r\n\t%s", Prefix);
							LineLength = 1 + strlen(Prefix);
						}

						p.Write((char*)"_", 1);
						LineLength++;
					}
					else if (*w & 0x80 ||
							*w == '_' ||
							*w == '?' ||
							*w == '=')
					{
						if (LineLength > MIME_MAX_LINE - 5)
						{
							p.Print("?=\r\n\t%s", Prefix);
							LineLength = 1 + strlen(Prefix);
						}

						char Temp[16];
						Ch = sprintf_s(Temp, sizeof(Temp), "=%2.2X", (uchar)*w);
						p.Write(Temp, Ch);
						LineLength += Ch;
					}
					else
					{
						if (LineLength > MIME_MAX_LINE - 3)
						{
							p.Print("?=\r\n\t%s", Prefix);
							LineLength = 1 + strlen(Prefix);
						}

						p.Write(w, 1);
						LineLength++;
					}
				}
			}

			p.Push("?=");
			DeleteArray(Buf);
		}

		DeleteArray(Str);
		Str = p.NewStr();
	}
	else
	{
		bool RecodeNewLines = false;
		for (char *s = Str; *s; s++)
		{
			if (*s == '\n' && (s == Str || s[-1] != '\r'))
			{
				RecodeNewLines = true;
				break;
			}
		}
		
		if (RecodeNewLines)
		{
			for (char *s = Str; *s; s++)
			{
				if (*s == '\r')
					;
				else if (*s == '\n')
					p.Write("\r\n", 2);
				else
					p.Write(s, 1);
			}
			
			DeleteArray(Str);
			Str = p.NewStr();
		}
	}

	return Str;
}

//////////////////////////////////////////////////////////////////////////////
void DeNullText(char *in, ssize_t &len)
{
	char *out = in;
	char *end = in + len;
	while (in < end)
	{
		if (*in)
		{
			*out++ = *in;
		}
		else
		{
			len--;
		}
		in++;
	}
}

//////////////////////////////////////////////////////////////////////////////

typedef char CharPair[2];
static CharPair Pairs[] =
{
    {'<', '>'},
    {'(', ')'},
    {'\'', '\''},
    {'\"', '\"'},
    {0, 0},
};

struct MailAddrPart
{
    GAutoString Part;
    bool Brackets;
    bool ValidEmail;
    

    GAutoString RemovePairs(char *Str, ssize_t Len, CharPair *Pairs)
    {
	    char *s = Str;
	    if (Len < 0)
	        Len = strlen(s);
	    while (*s && strchr(WhiteSpace, *s))
	    {
	        s++;
	        Len--;
	    }

	    if (!*s)
		    return GAutoString();

	    // Get the end of the string...
	    char *e = s;
	    if (Len < 0)
	        e += strlen(s);
	    else
	        e += Len;

	    // Seek back over any trailing whitespace
	    while (e > s && strchr(WhiteSpace, e[-1]))
		    e--;

	    for (CharPair *p = Pairs; (*p)[0]; p++)
	    {
		    if ((*p)[0] == *s && (*p)[1] == e[-1])
		    {
			    s++;
			    e--;
			    if (s < e)
			    {
				    // reset search
				    p = Pairs - 1;
			    }
			    else break;
		    }
	    }

	    Len = e - s;
	    if (Len < 0)
		    return GAutoString();

	    return GAutoString(NewStr(s, Len));
    }

    MailAddrPart(char *s, ssize_t len)
    {
        ValidEmail = false;
        Brackets = false;
        if (s)
        {
		    if (len < 0)
		        len = strlen(s);
		    while (strchr(WhiteSpace, *s) && len > 0)
		    {
			    s++;
			    len--;
		    }
            
            Brackets = *s == '<';
            Part = RemovePairs(s, len, Pairs);
            // ValidEmail = IsValidEmail(Part);
        }
    }
    
    int Score()
    {
        if (!Part)
            return 0;
        return (ValidEmail ? 1 : 0) + (Brackets ? 1 : 0);
    }
};

int PartCmp(GAutoPtr<MailAddrPart> *a, GAutoPtr<MailAddrPart> *b)
{
    return (*b)->Score() - (*a)->Score();
}

void DecodeAddrName(const char *Str, GAutoString &Name, GAutoString &Addr, const char *DefaultDomain)
{
	/* Testing code
	char *Input[] =
	{
		"\"Sound&Secure@speedytechnical.com\" <soundandsecure@speedytechnical.com>",
		"\"@MM-Social Mailman List\" <social@cisra.canon.com.au>",
		"'Matthew Allen (fret)' <fret@memecode.com>",
		"Matthew Allen (fret) <fret@memecode.com>",
		"\"'Matthew Allen'\" <fret@memecode.com>",
		"Matthew Allen",
		"fret@memecode.com",
		"\"<Matthew Allen>\" <fret@memecode.com>",
		"<Matthew Allen> (fret@memecode.com)",
		"Matthew Allen <fret@memecode.com>",
		"\"Matthew, Allen\" (fret@memecode.com)",
		"Matt'hew Allen <fret@memecode.com>",
		"john.omalley <john.O'Malley@testing.com>",
		"Bankers' Association (ABA)<survey@aawp.org.au>",
		"'Amy's Mum' <name@domain.com>",
		"\"Philip Doggett (JIRA)\" <jira@audinate.atlassian.net>",
		0
	};

	GAutoString Name, Addr;
	for (char **i = Input; *i; i++)
	{
		Name.Reset();
		Addr.Reset();
		DecodeAddrName(*i, Name, Addr, "name.com");
		LgiTrace("N=%-#32s A=%-32s\n", Name, Addr);
	}
	*/

	if (!Str)
		return;

	GArray< GAutoPtr<MailAddrPart> > Parts;

	GString s = Str;
	GString non;
	GString email;
	GString::Array a = s.SplitDelimit("<>");
	for (unsigned i=0; i<a.Length(); i++)
	{
		if (LIsValidEmail(a[i]))
		{
			email = a[i];
		}
		else
		{
			non += a[i];
		}
	}
	if (!email)
	{
		a = s.SplitDelimit("()");
		non.Empty();
		for (unsigned i=0; i<a.Length(); i++)
		{
			if (LIsValidEmail(a[i]))
			{
				email = a[i];
			}
			else
			{
				non += a[i];
			}
		}	
		if (!email)
		{
			a = s.SplitDelimit(WhiteSpace);
			non.Empty();
			for (unsigned i=0; i<a.Length(); i++)
			{
				if (LIsValidEmail(a[i]))
				{
					email = a[i];
				}
				else
				{
					if (non.Get())
						non += " ";
					non += a[i];
				}
			}	
		}
	}
	
	if (non.Length() > 0)
	{
		const char *ChSet = " \t\r\n\'\"<>";
		do
		{
			non = non.Strip(ChSet);
		}
		while (non.Length() > 0 && strchr(ChSet, non(0)));
	}
	
	Name.Reset(NewStr(non));
	Addr.Reset(NewStr(email.Strip()));
}

void StrCopyToEOL(char *d, char *s)
{
	if (d && s)
	{
		while (*s && *s != '\r' && *s != '\n')
		{
			*d++ = *s++;
		}

		*d = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
MailTransaction::MailTransaction()
{
	Index = -1;
	Flags = 0;
	Status = false;
	Oversize = false;
	Stream = 0;
	UserData = 0;
}

MailTransaction::~MailTransaction()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
FileDescriptor::FileDescriptor()
{
	Embeded = 0;
	Offset = 0;
	Size = 0;
	Data = 0;
	MimeType = 0;
	ContentId = 0;
	Lock = 0;
	OwnEmbeded = false;
}

FileDescriptor::FileDescriptor(GStreamI *embed, int64 offset, int64 size, char *name)
{
	Embeded = embed;
	Offset = offset;
	Size = size;
	Data = 0;
	MimeType = 0;
	Lock = 0;
	ContentId = 0;
	OwnEmbeded = false;

	if (name)
	{
		Name(name);
	}
}

FileDescriptor::FileDescriptor(char *name)
{
	Embeded = 0;
	Offset = 0;
	Size = 0;
	Data = 0;
	MimeType = 0;
	Lock = 0;
	ContentId = 0;
	OwnEmbeded = false;

	if (name)
	{
		Name(name);
		if (File.Open(name, O_READ))
		{
			Size = File.GetSize();
			File.Close();
		}
	}
}

FileDescriptor::FileDescriptor(char *data, int64 len)
{
	Embeded = 0;
	Offset = 0;
	MimeType = 0;
	Lock = 0;
	ContentId = 0;
	Size = len;
	OwnEmbeded = false;

	Data = data ? new uchar[(size_t)Size] : 0;
	if (Data)
	{
		memcpy(Data, data, (size_t)Size);
	}
}

FileDescriptor::~FileDescriptor()
{
	if (OwnEmbeded)
	{
		DeleteObj(Embeded);
	}

	DeleteArray(MimeType);
	DeleteArray(ContentId);
	DeleteArray(Data);
}

void FileDescriptor::SetOwnEmbeded(bool i)
{
	OwnEmbeded = i;
}

void FileDescriptor::SetLock(LMutex *l)
{
	Lock = l;
}

LMutex *FileDescriptor::GetLock()
{
	return Lock;
}

GStreamI *FileDescriptor::GotoObject()
{
	if (Embeded)
	{
		Embeded->SetPos(Offset);
		return Embeded;
	}
	else if (Name() && File.Open(Name(), O_READ))
	{
		return &File;
	}
	else if (Data && Size > 0)
	{
		DataStream.Reset(new GMemStream(Data, Size, false));
		return DataStream;
	}

	return 0;
}

int FileDescriptor::Sizeof()
{
	return (int)Size;
}

uchar *FileDescriptor::GetData()
{
	return Data;
}

bool FileDescriptor::Decode(char *ContentType,
							char *ContentTransferEncoding,
							char *MimeData,
							int MimeDataLen)
{
	bool Status = false;
	int Content = CONTENT_NONE;

	if (ContentType && ContentTransferEncoding)
	{
		// Content-Type: application/octet-stream; name="Scribe.opt"
		Content = CONTENT_OCTET_STREAM;

		if (strnistr(ContentTransferEncoding, "base64", 1000))
		{
			Content = CONTENT_BASE64;
		}

		if (strnistr(ContentTransferEncoding, "quoted-printable", 1000))
		{
			Content = CONTENT_QUOTED_PRINTABLE;
		}

		if (Content != CONTENT_NONE)
		{
			const char *NameKey = "name";
			char *n = strnistr(ContentType, NameKey, 1000);
			if (n)
			{
				char *Equal = strchr(n, '=');
				if (Equal)
				{
					Equal++;
					while (*Equal && *Equal == '\"')
					{
						Equal++;
					}

					char *End = strchr(Equal, '\"');
					if (End)
					{
						*End = 0;
					}

					Name(Equal);
					Status = true;
				}
			}
		}
	}

	if (Status && MimeData && MimeDataLen > 0 && Content != CONTENT_NONE)
	{
		Status = false;
		char *Base64 = new char[MimeDataLen];

		switch (Content)
		{
			case CONTENT_OCTET_STREAM:
			{
				Size = 0;
				DeleteObj(Data);
				Data = new uchar[MimeDataLen];
				if (Data)
				{
					Size = MimeDataLen;
					memcpy(Data, MimeData, (size_t)Size);
					Status = true;
				}
				break;
			}
			case CONTENT_QUOTED_PRINTABLE:
			{
				Size = 0;
				DeleteObj(Data);
				Data = new uchar[MimeDataLen+1];
				if (Data)
				{
					char *Out = (char*) Data;
					for (int i=0; i<MimeDataLen; i++)
					{
						if (MimeData[i] == '=')
						{
							if (MimeData[i+1] == '\r' || MimeData[i+1] == '\n')
							{
								if (MimeData[i+1] == '\r') i++;
								if (MimeData[i+1] == '\n') i++;
							}
							else
							{
								i++;
								char c = ConvHexToBin(MimeData[i]) << 4;
								i++;
								c |= ConvHexToBin(MimeData[i]) & 0xF;

								Size++;
								*Out++ = c;
							}
						}
						else
						{
							Size++;
							*Out++ = MimeData[i];
						}
					}

					*Out++ = 0;
					Status = true;
				}
				break;
			}
			case CONTENT_BASE64:
			{
				int InCount = 0;
				int OutCount = 0;
				char *In = MimeData;
				char *Out = Base64;
				while (InCount < MimeDataLen)
				{
					if (*In != '\r' && *In != '\n')
					{
						*Out++ = *In;
						OutCount++;
					}
					InCount++;
					In++;
				}

				Size = BufferLen_64ToBin(OutCount);
				Data = new uchar[(size_t)Size];
				if (Data)
				{
					ssize_t Converted = ConvertBase64ToBinary(Data, Size, Base64, OutCount);
					Status = Converted <= Size && Converted >= Size - 3;
					if (Status)
					{
						Size = Converted;
					}
					else
					{
						DeleteArray(Data);
						Size = 0;
					}
				}
				break;
			}
		}
	}
	return Status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
AddressDescriptor::AddressDescriptor(AddressDescriptor *Copy)
{
	Data = 0;
	Status = Copy ? Copy->Status : false;
	CC = Copy ? Copy->CC : false;
	Addr = Copy ? NewStr(Copy->Addr) : 0;
	Name = Copy ? NewStr(Copy->Name) : 0;
}

AddressDescriptor::~AddressDescriptor()
{
	_Delete();
}

void AddressDescriptor::_Delete()
{
	Data = 0;
	Status = false;
	CC = 0;
	DeleteArray(Name);
	DeleteArray(Addr);
}

void AddressDescriptor::Print(char *Str, int Len)
{
	if (!Str)
	{
		LgiAssert(0);
		return;
	}

	if (Addr && Name)
	{
		sprintf_s(Str, Len, "%s (%s)", Addr, Name);
	}
	else if (Addr)
	{
		strcpy_s(Str, Len, Addr);
	}
	else if (Name)
	{
		sprintf_s(Str, Len, "(%s)", Name);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
MailProtocol::MailProtocol()
{
	Buffer[0] = 0;
	Logger = 0;
	ErrMsgId = 0;
	SettingStore = NULL;

	Items = 0;
	Transfer = 0;
}

MailProtocol::~MailProtocol()
{
	CharsetPrefs.DeleteArrays();
}

void MailProtocol::Log(const char *Str, GSocketI::SocketMsgType type)
{
	if (Logger && Str)
	{
		char s[1024];
		char *e = s + sizeof(s) - 2;
		const char *i = Str;
		char *o = s;
		while (*i && o < e)
		{
			*o++ = *i++;
		}
		while (o > s && (o[-1] == '\r' || o[-1] == '\n'))
			o--;
		*o++ = '\n';
		*o = 0;

		Logger->Write(s, o - s, type);
	}
}

bool MailProtocol::Error(const char *file, int line, const char *msg, ...)
{
	char s[1024];
	va_list a;
	va_start(a, msg);
	vsprintf_s(s, sizeof(s), msg, a);
	va_end(a);

	Log(s, GSocketI::SocketMsgError);
	LgiTrace("%s:%i - Error: %s", file, line, s);
	return false;
}

bool MailProtocol::Read()
{
	bool Status = false;
	if (Socket)
	{
		Status = Socket->Read(Buffer, sizeof(Buffer), 0) > 0;
	}
	return Status;
}

bool MailProtocol::Write(const char *Buf, bool LogWrite)
{
	bool Status = false;
	if (Socket)
	{
		const char *p = Buf ? Buf : Buffer;
		Status = Socket->Write(p, strlen(p), 0) > 0;
		
		if (LogWrite)
		{
			Log(p, GSocketI::SocketMsgSend);
		}
	}
	return Status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#define	VERIFY_RET_VAL(Arg)						\
{												\
	if (!Arg)									\
	{											\
		LMutex::Auto Lck(&SocketLock, _FL);	\
		Socket.Reset(0);						\
		return NULL;							\
	}											\
}

#define	VERIFY_ONERR(Arg)						\
{												\
	if (!Arg)									\
	{											\
		LMutex::Auto Lck(&SocketLock, _FL);	\
		Socket.Reset(0);						\
		goto CleanUp;							\
	}											\
}

void Reorder(GArray<GString> &a, const char *s)
{
	for (unsigned i=0; i<a.Length(); i++)
	{
		if (a[i].Equals(s) && i > 0)
		{
			a.DeleteAt(i, true);
			a.AddAt(0, s);
			break;
		}
	}
}

MailSmtp::MailSmtp()
{
}

MailSmtp::~MailSmtp()
{
}

bool MailSmtp::Open(GSocketI *S,
					const char *RemoteHost,
					const char *LocalDomain,
					const char *UserName,
					const char *Password,
					int Port,
					int Flags)
{
	char Str[256] = "";
	bool Status = false;

	if (!RemoteHost)
		Error(_FL, "No remote SMTP host.\n");
	else
	{
		strcpy_s(Str, sizeof(Str), RemoteHost);
		char *Colon = strchr(Str, ':');
		if (Colon)
		{
			*Colon = 0;
			Colon++;
			Port = atoi(Colon);
		}
		if (Port == 0)
		{
			if (Flags & MAIL_SSL)
				Port = SMTP_SSL_PORT;
			else
				Port = SMTP_PORT;
		}

		GAutoString Server(TrimStr(Str));
		if (Server)
		{
			if (SocketLock.Lock(_FL))
			{
				Socket.Reset(S);
				SocketLock.Unlock();
			}

			Socket->SetTimeout(30 * 1000);

			char Msg[256];
			sprintf_s(Msg, sizeof(Msg), "Connecting to %s:%i...", Server.Get(), Port);
			Log(Msg, GSocketI::SocketMsgInfo);
	
			if (!Socket->Open(Server, Port))
				Error(_FL, "Failed to connect socket to %s:%i\n", Server.Get(), Port);
			else
			{
				GStringPipe Str;

				// receive signon message
				VERIFY_RET_VAL(ReadReply("220"));

				// Rfc 2554 ESMTP authentication
				SmtpHello:
				sprintf_s(Buffer, sizeof(Buffer), "EHLO %s\r\n", (ValidNonWSStr(LocalDomain)) ? LocalDomain : "default");
				VERIFY_RET_VAL(Write(0, true));
				/*bool HasSmtpExtensions =*/ ReadReply("250", &Str);

				bool Authed = false;
				bool NoAuthTypes = false;
				bool SupportsStartTLS = false;
				GArray<GString> AuthTypes;

				// Look through the response for the auth line
				char *Response = Str.NewStr();
				if (Response)
				{
					GToken Lines(Response, "\n");
					for (uint32_t i=0; i<Lines.Length(); i++)
					{
						char *l = Lines[i];
						char *AuthStr = strstr(l, "AUTH");
						if (AuthStr)
						{
							// walk through AUTH types
							GToken Types(AuthStr + 4, " ,;");
							for (uint32_t a=0; a<Types.Length(); a++)
							{
								AuthTypes.Add(Types[a]);
							}
						}
						if (stristr(l, "STARTTLS"))
						{
							SupportsStartTLS = true;
						}
					}
					DeleteArray(Response);
				}

				if (SupportsStartTLS && TestFlag(Flags, MAIL_USE_STARTTLS))
				{
					strcpy_s(Buffer, sizeof(Buffer), "STARTTLS\r\n");
					VERIFY_RET_VAL(Write(0, true));
					VERIFY_RET_VAL(ReadReply("220", &Str));

					GVariant v;
					if (Socket->SetValue(GSocket_Protocol, v="SSL"))
					{
						Flags &= ~MAIL_USE_STARTTLS;
						goto SmtpHello;
					}
					else
					{
						// SSL init failed... what to do here?
						return false;
					}
				}


				if (TestFlag(Flags, MAIL_USE_AUTH))
				{
					if (!ValidStr(UserName))
					{
						// We need a user name in all authentication types.
						SetError(L_ERROR_ESMTP_NO_USERNAME, "No username for authentication.");
						return false;
					}

					if (AuthTypes.Length() == 0)
					{
						// No auth types? huh?
						if (TestFlag(Flags, MAIL_USE_PLAIN))
							// Force plain type
							AuthTypes.Add("PLAIN");
						else if (TestFlag(Flags, MAIL_USE_LOGIN))
							// Force login type
							AuthTypes.Add("LOGIN");
						else if (TestFlag(Flags, MAIL_USE_CRAM_MD5))
							// Force CRAM MD5 type
							AuthTypes.Add("CRAM-MD5");
						else if (TestFlag(Flags, MAIL_USE_OAUTH2))
							// Force OAUTH2 type
							AuthTypes.Add("XOAUTH2");
						else
						{
							// Try all
							AuthTypes.Add("PLAIN");
							AuthTypes.Add("LOGIN");
							AuthTypes.Add("CRAM-MD5");
							AuthTypes.Add("XOAUTH2");
						}
					}
					else
					{
						// Force user preference
						if (TestFlag(Flags, MAIL_USE_PLAIN))
							Reorder(AuthTypes, "PLAIN");
						else if (TestFlag(Flags, MAIL_USE_LOGIN))
							Reorder(AuthTypes, "LOGIN");
						else if (TestFlag(Flags, MAIL_USE_CRAM_MD5))
							Reorder(AuthTypes, "CRAM-MD5");
						else if (TestFlag(Flags, MAIL_USE_OAUTH2))
							Reorder(AuthTypes, "XOAUTH2");
					}

					for (auto Auth : AuthTypes)
					{
						// Try all their auth types against our internally support types
						if (Auth.Equals("LOGIN"))
						{
							VERIFY_RET_VAL(Write("AUTH LOGIN\r\n", true));
							VERIFY_RET_VAL(ReadReply("334"));

							ZeroObj(Buffer);
							ConvertBinaryToBase64(Buffer, sizeof(Buffer), (uchar*)UserName, strlen(UserName));
							strcat(Buffer, "\r\n");
							VERIFY_RET_VAL(Write(0, true));
							if (ReadReply("334"))
							{
								ZeroObj(Buffer);
								ConvertBinaryToBase64(Buffer, sizeof(Buffer), (uchar*)Password, strlen(Password));
								strcat(Buffer, "\r\n");
								VERIFY_RET_VAL(Write(0, true));
								if (ReadReply("235"))
								{
									Authed = true;
								}
							}
						}
						else if (Auth.Equals("PLAIN"))
						{
							char Tmp[256];
							ZeroObj(Tmp);
							int ch = 1;
							ch += sprintf_s(Tmp+ch, sizeof(Tmp)-ch, "%s", UserName) + 1;
							ch += sprintf_s(Tmp+ch, sizeof(Tmp)-ch, "%s", Password) + 1;

							char B64[256];
							ZeroObj(B64);
							ConvertBinaryToBase64(B64, sizeof(B64), (uint8_t*)Tmp, ch);

							sprintf_s(Buffer, sizeof(Buffer), "AUTH PLAIN %s\r\n", B64);
							VERIFY_RET_VAL(Write(0, true));
							if (ReadReply("235"))
							{
								Authed = true;
							}
						}
						else if (Auth.Equals("CRAM-MD5"))
						{
							sprintf_s(Buffer, sizeof(Buffer), "AUTH CRAM-MD5\r\n");
							VERIFY_RET_VAL(Write(0, true));
							if (ReadReply("334"))
							{
								auto Sp = strchr(Buffer, ' ');
								if (Sp)
								{
									Sp++;

									// Decode the server response:
									uint8_t Txt[128];
									auto InLen = strlen(Sp);
									ssize_t TxtLen = ConvertBase64ToBinary(Txt, sizeof(Txt), Sp, InLen);

									// Calc the hash:
									// https://tools.ietf.org/html/rfc2104
									char Key[64] = {0};
									memcpy(Key, Password, MIN(strlen(Password), sizeof(Key)));
									uint8_t iKey[256];
									char oKey[256];
									for (unsigned i=0; i<64; i++)
									{
										iKey[i] = Key[i] ^ 0x36;
										oKey[i] = Key[i] ^ 0x5c;
									}
									memcpy(iKey+64, Txt, TxtLen);
									md5_state_t md5;
									md5_init(&md5);
									md5_append(&md5, iKey, 64 + TxtLen);
									md5_finish(&md5, oKey + 64);

									md5_init(&md5);
									md5_append(&md5, (uint8_t*)oKey, 64 + 16);
									char digest[16];
									md5_finish(&md5, digest);

									char r[256];
									int ch = sprintf_s(r, sizeof(r), "%s ", UserName);
									for (unsigned i=0; i<16; i++)
										ch += sprintf_s(r+ch, sizeof(r)-ch, "%02x", (uint8_t)digest[i]);

									// Base64 encode
									ssize_t Len = ConvertBinaryToBase64(Buffer, sizeof(Buffer), (uint8_t*)r, ch);
									Buffer[Len++] = '\r';
									Buffer[Len++] = '\n';
									Buffer[Len++] = 0;
									VERIFY_RET_VAL(Write(0, true));
									if (ReadReply("235"))
										Authed = true;
								}
							}
						}
						else if (Auth.Equals("XOAUTH2"))
						{
							LOAuth2 Authenticator(OAuth2, UserName, SettingStore, Socket->GetCancel());
							auto Tok = Authenticator.GetAccessToken();
							if (Tok)
							{
								GString s;
								s.Printf("user=%s\001auth=Bearer %s\001\001\0", UserName, Tok.Get());
								Base64Str(s);

								sprintf_s(Buffer, sizeof(Buffer), "AUTH %s %s\r\n", Auth.Get(), s.Get());
								VERIFY_RET_VAL(Write(0, true));
								Authed = ReadReply("235");
								if (!Authed)
								{
									Authenticator.Refresh();
								}
							}
						}
						else
						{
							LgiTrace("%s:%i - Unsupported auth type '%s'\n", _FL, Auth.Get());
						}

						if (Authed)
							break;
					}

					if (!Authed)
					{
						if (NoAuthTypes)
							SetError(L_ERROR_ESMTP_NO_AUTHS, "The server didn't return the authentication methods it supports.");
						else
						{
							GString p;
							for (auto i : AuthTypes)
							{
								if (p.Get())
									p += ", ";
								p += i;
							}

							SetError(L_ERROR_UNSUPPORTED_AUTH, "Authentication failed, types available:\n\t%s", p);
						}
					}

					Status = Authed;
				}
				else
				{
					Status = true;
				}
			}
		}
	}

	return Status;
}

bool MailSmtp::WriteText(const char *Str)
{
	// we have to convert all strings to CRLF in here
	bool Status = false;
	if (Str)
	{
		GMemQueue Temp;

		const char *Start = Str;
		while (*Str)
		{
			if (*Str == '\n')
			{
				// send a string
				ssize_t Size = Str-Start;
				if (Str[-1] == '\r') Size--;
				Temp.Write((uchar*) Start, Size);
				Temp.Write((uchar*) "\r\n", 2);
				Start = Str + 1;
			}

			Str++;
		}

		// send the final string
		ssize_t Size = Str-Start;
		if (Str[-1] == '\r') Size--;
		Temp.Write((uchar*) Start, (int)Size);

		Size = (int)Temp.GetSize();
		char *Data = new char[(size_t)Size];
		if (Data)
		{
			Temp.Read((uchar*) Data, Size);
			Status = Socket->Write(Data, (int)Size, 0) == Size;
			DeleteArray(Data);
		}
	}

	return Status;
}

char *StripChars(char *Str, const char *Chars = "\r\n")
{
	if (Str)
	{
		char *i = Str;
		char *o = Str;
		while (*i)
		{
			if (strchr(Chars, *i))
				i++;
			else
				*o++ = *i++;
		}
		*o++ = 0;
	}

	return Str;
}

char *CreateAddressTag(List<AddressDescriptor> &l, int Type, List<char> *CharsetPrefs)
{
	char *Result = 0;
	List<AddressDescriptor> Addr;
	for (auto a: l)
	{
		if (a->CC == Type)
		{
			Addr.Insert(a);
		}
	}
	
	if (Addr.Length() > 0)
	{
		GStringPipe StrBuf;
		StrBuf.Push((Type == 0) ? (char*)"To: " : (char*)"Cc: ");

		for (auto It = Addr.begin(); It != Addr.end(); )
		{
			auto a = *It;
			AddressDescriptor *NextA = *(++It);
			char Buffer[256] = "";
			
			StripChars(a->Name);
			StripChars(a->Addr);

			if (a->Addr && strchr(a->Addr, ','))
			{
				// Multiple address format
				GToken t(a->Addr, ",");
				for (uint32_t i=0; i<t.Length(); i++)
				{
					sprintf_s(Buffer, sizeof(Buffer), "<%s>", t[i]);
					if (i < t.Length()-1) strcat(Buffer, ",\r\n\t");
					StrBuf.Push(Buffer);
					Buffer[0] = 0;
				}
			}
			else if (a->Name)
			{
				// Name and addr
				char *Mem = 0;
				char *Name = a->Name;

				if (Is8Bit(Name))
				{
					Name = Mem = EncodeRfc2047(NewStr(Name), 0, CharsetPrefs);
				}

				if (strchr(Name, '\"'))
				    sprintf_s(Buffer, sizeof(Buffer), "'%s' <%s>", Name, a->Addr);
				else
				    sprintf_s(Buffer, sizeof(Buffer), "\"%s\" <%s>", Name, a->Addr);

				DeleteArray(Mem);
			}
			else if (a->Addr)
			{
				// Just addr
				sprintf_s(Buffer, sizeof(Buffer), "<%s>", a->Addr);
			}

			if (NextA) strcat(Buffer, ",\r\n\t");
			StrBuf.Push(Buffer);

			a = NextA;
		}

		StrBuf.Push("\r\n");
		Result = StrBuf.NewStr();
	}

	return Result;
}

// This class implements a pipe that writes to a socket
class SocketPipe : public GStringPipe
{
	GSocketI *s;
	MailProtocolProgress *p;

public:
	bool Status;

	SocketPipe(GSocketI *socket, MailProtocolProgress *progress)
	{
		s = socket;
		p = progress;
		Status = true;
	}

	ssize_t Read(void *Ptr, ssize_t Size, int Flags)
	{
		return false;
	}
	
	int64 SetSize(int64 Size)
	{
		if (p)
		{
			p->Start = LgiCurrentTime();
			p->Range = (int)Size;
			return Size;
		}

		return -1;
	}

	ssize_t Write(const void *InPtr, ssize_t Size, int Flags)
	{
		char *Ptr = (char*)InPtr;
		char *e = Ptr + Size;
		
		while (Ptr < e)
		{
			ssize_t w = s->Write(Ptr, e - Ptr, 0);
			if (w > 0)
			{
				Ptr += w;

				if (p && p->Range && w > 0)
					p->Value += w;
			}
			else break;
		}

		return Ptr - (char*)InPtr;
	}
};

bool MailSmtp::SendToFrom(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	bool AddrOk = false;

	if (To.Length() == 0)
	{
		ErrMsgId = L_ERROR_ESMTP_NO_RECIPIENT;
		ErrMsgFmt = "No recipients to send to.";
		ErrMsgParam.Empty();
		return false;
	}

	// send MAIL message
	if (From && ValidStr(From->Addr))
	{
		sprintf_s(Buffer, sizeof(Buffer), "MAIL FROM: <%s>\r\n", From->Addr);
	}
	else
	{
		ErrMsgId = L_ERROR_ESMTP_NO_FROM;
		ErrMsgFmt = "No 'from' address in email.";
		ErrMsgParam.Empty();
		return false;
	}

	VERIFY_RET_VAL(Write(0, true));
	VERIFY_RET_VAL(ReadReply("250", 0, Err));

	// send RCPT message
	AddrOk = true;
	List<AddressDescriptor>::I Recip = To.begin();
	for (AddressDescriptor *a = *Recip; a; a = *++Recip)
	{
		char *Addr = ValidStr(a->Addr) ? a->Addr : a->Name;
		if (ValidStr(Addr))
		{
			GToken Parts(Addr, ",");
			for (unsigned p=0; p<Parts.Length(); p++)
			{
				sprintf_s(Buffer, sizeof(Buffer), "RCPT TO: <%s>\r\n", Parts[p]);
				VERIFY_RET_VAL(Write(0, true));
				a->Status = ReadReply("25", 0, Err);
				AddrOk |= a->Status != 0; // at least one address is ok
			}
		}
		else if (Err)
		{
			ErrMsgId = L_ERROR_ESMTP_BAD_RECIPIENT;
			ErrMsgFmt = "Invalid recipient '%s'.";
			ErrMsgParam = Addr;
		}
	}

	return AddrOk;
}

GStringPipe *MailSmtp::SendData(MailProtocolError *Err)
{
	// send DATA message
	sprintf_s(Buffer, sizeof(Buffer), "DATA\r\n");
	VERIFY_RET_VAL(Write(0, true));
	VERIFY_RET_VAL(ReadReply("354", 0, Err));

	return new SocketPipe(Socket, Transfer);
}

GStringPipe *MailSmtp::SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	return SendToFrom(To, From, Err) ? SendData(Err) : NULL;
}

bool MailSmtp::SendEnd(GStringPipe *m)
{
	bool Status = false;

	SocketPipe *Msg = dynamic_cast<SocketPipe*>(m);
	if (Msg)
	{
		// send message terminator and receive reply
		if (Msg->Status &&
			Msg->Write((void*)"\r\n.\r\n", 5, 0))
		{
			Status = ReadReply("250");
		}
		// else
		//		just close the connection on them
		//		so nothing gets sent
	}

	DeleteObj(m);

	return Status;
}

/*
bool MailSmtp::Send(MailMessage *Msg, bool Mime)
{
	bool Status = false;

	if (Socket && Msg)
	{
		GStringPipe *Sink = SendStart(Msg->To, Msg->From);
		if (Sink)
		{
			// setup a gui progress meter to send the email,
			// the length is just a guesstimate as we won't know the exact
			// size until we encode it all, and I don't want it hanging around
			// in memory at once, so we encode and send on the fly.
			int Length =	1024 +
									(Msg->GetBody() ? strlen(Msg->GetBody()) : 0);
			for (FileDescriptor *f=Msg->FileDesc.First(); f; f=Msg->FileDesc.Next())
			{
				Length += f->Sizeof() * 4 / 3;
			}

			// encode and send message for transport
			Msg->Encode(*Sink, 0, this);
			Status = SendEnd(Sink);
		}
	}

	return Status;
}
*/

bool MailSmtp::Close()
{
	if (Socket)
	{
		// send QUIT message
		sprintf_s(Buffer, sizeof(Buffer), "QUIT\r\n");
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply("221"));
		
		LMutex::Auto Lock(&SocketLock, _FL);
		Socket.Reset(0);
		return true;
	}

	return false;
}

bool MailSmtp::ReadReply(const char *Str, GStringPipe *Pipe, MailProtocolError *Err)
{
	bool Status = false;
	if (Socket && Str)
	{
		int Pos = 0;
		char *Start = Buffer;
		ZeroObj(Buffer);

		while (Pos < sizeof(Buffer))
		{
			ssize_t Len = Socket->Read(Buffer+Pos, sizeof(Buffer)-Pos, 0);
			if (Len > 0)
			{
				char *Eol = strstr(Start, "\r\n");
				while (Eol)
				{
					// wipe EOL chars
					*Eol++ = 0;
					*Eol++ = 0;

					// process
					if (Pipe)
					{
						if (Pipe->GetSize()) Pipe->Push("\n");
						Pipe->Push(Start);
					}

					if (Start[3] == ' ')
					{
						// end of response
						if (!strncmp(Start, Str, strlen(Str)))
						{
							Status = true;
						}

						if (Err)
						{
							Err->Code = atoi(Start);
							char *Sp = strchr(Start, ' ');
							Err->ErrMsg = Sp ? Sp + 1 : Start;
						}

						// Log
						Log(Start, atoi(Start) >= 400 ? GSocketI::SocketMsgError : GSocketI::SocketMsgReceive);

						// exit loop
						Pos = sizeof(Buffer);
						break;
					}
					else
					{
						Log(Start, GSocketI::SocketMsgReceive);

						// more lines follow
						Start = Eol;
						Eol = strstr(Start, "\r\n");
					}
				}

				Pos += Len;
   			}
			else break;
		}

		if (!Status)
		{
			SetError(L_ERROR_GENERIC, "Error: %s", Buffer);
		}
	}

	return Status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
class Mail2Folder : public GStringPipe
{
	char File[256];
	GFile F;

public:
	Mail2Folder(char *Path, List<AddressDescriptor> &To)
	{
		do
		{
			char n[32];
			sprintf_s(n, sizeof(n), "%u.mail", LgiRand());
			LgiMakePath(File, sizeof(File), Path, n);
		}
		while (FileExists(File));

		if (F.Open(File, O_WRITE))
		{
			F.Print("Forward-Path: ");

			int i = 0;
			for (auto a: To)
			{
				a->Status = true;
				GToken Addrs(a->Addr, ",");
				for (unsigned n=0; n<Addrs.Length(); n++, i++)
				{
					if (i) F.Print(",");
					F.Print("<%s>", Addrs[n]);
				}
			}

			F.Print("\r\n");
		}
	}

	~Mail2Folder()
	{
		F.Close();
	}

	ssize_t Read(void *Buffer, ssize_t Size, int Flags = 0)
	{
		return F.Read(Buffer, Size, Flags);
	}

	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0)
	{
		return F.Write(Buffer, Size, Flags);
	}
};

class MailPostFolderPrivate
{
public:
	char *Path;

	MailPostFolderPrivate()
	{
		Path = 0;
	}

	~MailPostFolderPrivate()
	{
		DeleteArray(Path);
	}
};

MailSendFolder::MailSendFolder(char *Path)
{
	d = new MailPostFolderPrivate;
	d->Path = NewStr(Path);
}

MailSendFolder::~MailSendFolder()
{
	DeleteObj(d);
}

bool MailSendFolder::Open(GSocketI *S, const char *RemoteHost, const char *LocalDomain, const char *UserName, const char *Password, int Port, int Flags)
{
	return DirExists(d->Path);
}

bool MailSendFolder::Close()
{
	return true;
}

GStringPipe *MailSendFolder::SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err)
{
	return new Mail2Folder(d->Path, To);
}

bool MailSendFolder::SendEnd(GStringPipe *Sink)
{
	DeleteObj(Sink);
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
class MailItem
{
public:
	char *File;
	bool Delete;
	
	MailItem(char *f)
	{
		File = NewStr(f);
		Delete = false;
	}

	~MailItem()
	{
		DeleteArray(File);
	}
};

class MailReceiveFolderPrivate
{
public:
	char *Path;
	List<MailItem> Mail;

	MailReceiveFolderPrivate()
	{
		Path = 0;
	}

	~MailReceiveFolderPrivate()
	{
		DeleteArray(Path);
		Mail.DeleteObjects();
	}

	void Empty()
	{
		for (auto m: Mail)
		{
			if (m->Delete)
			{
				FileDev->Delete(m->File, false);
			}
		}

		Mail.DeleteObjects();
	}
};

MailReceiveFolder::MailReceiveFolder(char *Path)
{
	d = new MailReceiveFolderPrivate;
	d->Path = NewStr(Path);
}

MailReceiveFolder::~MailReceiveFolder()
{
	DeleteObj(d);
}

bool MailReceiveFolder::Open(GSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, GDom *SettingStore, int Flags)
{
	// We don't use the socket so just free it here...
	DeleteObj(S);

	// Argument check
	if (!DirExists(d->Path))
		return false;
	
	GDirectory Dir;

	// Loop through files, looking for email
	for (int b = Dir.First(d->Path, LGI_ALL_FILES); b; b = Dir.Next())
	{
		if (!Dir.IsDir())
		{
			if (MatchStr("*.eml", Dir.GetName()) ||
				MatchStr("*.mail", Dir.GetName()))
			{
				char p[300];
				Dir.Path(p, sizeof(p));
				d->Mail.Insert(new MailItem(p));
			}
		}
	}

	return true;
}

bool MailReceiveFolder::Close()
{
	d->Empty();
	return true;
}

int MailReceiveFolder::GetMessages()
{
	return (int)d->Mail.Length();
}

bool MailReceiveFolder::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	bool Status = false;

	for (unsigned i=0; i<Trans.Length(); i++)
	{
		MailTransaction *t = Trans[i];
		if (t && t->Stream)
		{
			t->Status = false;

			MailItem *m = d->Mail[t->Index];
			if (m)
			{
				GFile i;
				if (i.Open(m->File, O_READ))
				{
					GCopyStreamer c;
					if (c.Copy(&i, t->Stream))
					{
						Status = t->Status = true;
						if (Callbacks && Callbacks->OnReceive)
						{
							Callbacks->OnReceive(t, Callbacks->CallbackData);
						}
					}
				}
			}
		}
	}

	return Status;
}

bool MailReceiveFolder::Delete(int Message)
{
	MailItem *m = d->Mail[Message];
	if (m)
	{
		m->Delete = true;
		return false;
	}

	return false;
}

int MailReceiveFolder::Sizeof(int Message)
{
	MailItem *m = d->Mail[Message];
	if (m)
	{
		return (int)LgiFileSize(m->File);
	}

	return 0;
}

bool MailReceiveFolder::GetUid(int Message, char *Id, int IdLen)
{
	if (Id)
	{
		MailItem *m = d->Mail[Message];
		if (m)
		{
			char *s = strrchr(m->File, DIR_CHAR);
			if (s++)
			{
				char *e = strchr(s, '.');
				if (!e) e = s + strlen(s);
				ssize_t Len = e - s;
				memcpy(Id, s, Len);
				Id[Len] = 0;

				return true;
			}		
		}
	}

	return false;
}

bool MailReceiveFolder::GetUidList(List<char> &Id)
{
	bool Status = false;

	for (int i=0; i<d->Mail.Length(); i++)
	{
		char Uid[256];
		if (GetUid(i, Uid, sizeof(Uid)))
		{
			Status = true;
			Id.Insert(NewStr(Uid));
		}
		else
		{
			Id.DeleteArrays();
			Status = false;
			break;
		}
	}

	return Status;
}

char *MailReceiveFolder::GetHeaders(int Message)
{
	MailItem *m = d->Mail[Message];
	if (m)
	{
		GFile i;
		if (i.Open(m->File, O_READ))
		{
			GStringPipe o;
			GCopyStreamer c;
			GLinePrefix e("", false);
			if (c.Copy(&i, &o, &e))
			{
				return o.NewStr();
			}
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
MailPop3::MailPop3()
{
	End = "\r\n.\r\n";
	Marker = End;
	Messages = -1;
}

MailPop3::~MailPop3()
{
}

int MailPop3::GetMessages()
{
	if (Messages < 0)
	{
		if (Socket && Socket->IsOpen())
		{
			// see how many messages there are
			VERIFY_ONERR(Write("STAT\r\n", true));
			VERIFY_ONERR(ReadReply());

			Messages = GetInt();
		}
		else LgiAssert(!"No socket to get message count.");
	}

CleanUp:
	return Messages;
}

int MailPop3::GetInt()
{
	char Buf[32];
	char *Start = strchr(Buffer, ' ');
	if (Start)
	{
		Start++;
		char *End = strchr(Start, ' ');
		if (End)
		{
			int Len = (int) (End - Start);
			memcpy(Buf, Start, Len);
			Buf[Len] = 0;
			return atoi(Buf);
		}
	}
	return 0;
}

bool MailPop3::ReadReply()
{
	bool Status = false;
	if (Socket)
	{
		int Pos = 0;
		ZeroObj(Buffer);
		do
		{
			ssize_t Result = Socket->Read(Buffer+Pos, sizeof(Buffer)-Pos, 0);
			if (Result <= 0) // an error?
			{
				// Leave the loop...
				break;
			}

			Pos += Result;
    	}
		while (	!strstr(Buffer, "\r\n") &&
				sizeof(Buffer)-Pos > 0);

		Status = (Buffer[0] == '+') && strstr(Buffer, "\r\n");

		char *Cr = strchr(Buffer, '\r');
		if (Cr) *Cr = 0;
		if (ValidStr(Buffer))
			Log(Buffer, (Status) ? GSocketI::SocketMsgReceive : GSocketI::SocketMsgError);
		if (Cr) *Cr = '\r';

		if (!Status)
		{
			SetError(L_ERROR_GENERIC, "Error: %s", Buffer);
		}
	}

	return Status;
}

bool MailPop3::ListCmd(const char *Cmd, LHashTbl<ConstStrKey<char,false>, bool> &Results)
{
	sprintf_s(Buffer, sizeof(Buffer), "%s\r\n", Cmd);
	if (!Write(0, true))
		return false;

	char *b = Buffer;
	ssize_t r;
	while ((r = Socket->Read(b, sizeof(Buffer)-(b-Buffer))) > 0)
	{
		b += r;
		if (strnstr(Buffer, "\r\n.\r\n", b-Buffer))
			break;
	}

	if (r <= 0)
		return false;

	GToken t(Buffer, "\r\n");
	for (unsigned i=1; i<t.Length()-1; i++)
	{
		Results.Add(t[i], true);
	}

	return true;
}

#define OPT_Pop3NoApop		"NoAPOP"

bool MailPop3::Open(GSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, GDom *SettingStore, int Flags)
{
	bool Status = false;
	// bool RemoveMail = false;
	char Str[256] = "";
	char *Apop = 0;
	char *Server = 0;
	bool SecureAuth = TestFlag(Flags, MAIL_SECURE_AUTH);

	if (!RemoteHost)
		Error(_FL, "No remote POP host.\n");
	else 
	{
		GXmlTag LocalStore;
		if (!SettingStore)
			SettingStore = &LocalStore;

		if (Port < 1)
		{
			GVariant IsSsl;
			if (S->GetValue("IsSSL", IsSsl) &&
				IsSsl.CastInt32())
				Port = POP3_SSL_PORT;
			else
				Port = POP3_PORT;
		}

		strcpy_s(Str, sizeof(Str), RemoteHost);
		char *Colon = strchr(Str, ':');
		if (Colon)
		{
			*Colon = 0;
			Colon++;
			Port = atoi(Colon);
		}

		if (S &&
			User &&
			Password &&
			(Server = TrimStr(Str)))
		{
			S->SetTimeout(30 * 1000);

			ReStartConnection:
			
			if (SocketLock.Lock(_FL))
			{
				Socket.Reset(S);
				SocketLock.Unlock();
			}

			if (Socket &&
				Socket->Open(Server, Port) &&
				ReadReply())
			{
				GVariant NoAPOP = false;
				if (SettingStore)
					SettingStore->GetValue(OPT_Pop3NoApop, NoAPOP);
				if (!NoAPOP.CastInt32())
				{
					char *s = strchr(Buffer + 3, '<');
					if (s)
					{
						char *e = strchr(s + 1, '>');
						if (e)
						{
							Apop = NewStr(s, e - s + 1);
						}
					}
				}

				// login
				bool Authed = false;
				char *user = (char*) LgiNewConvertCp("iso-8859-1", User, "utf-8");
				char *pass = (char*) LgiNewConvertCp("iso-8859-1", Password, "utf-8");

				if (user && (pass || SecureAuth))
				{
					bool SecurityError = false;

					if (TestFlag(Flags, MAIL_USE_STARTTLS))
					{
						strcpy_s(Buffer, sizeof(Buffer), "STARTTLS\r\n");
						VERIFY_RET_VAL(Write(0, true));
						VERIFY_RET_VAL(ReadReply());

						GVariant v;
						if (Socket->SetValue(GSocket_Protocol, v="SSL"))
						{
							Flags &= ~MAIL_USE_STARTTLS;
						}
						else
						{
							SecurityError = true;
						}
					}

					if (!SecurityError && Apop) // GotKey, not implemented
					{
						// using encrypted password
						unsigned char Digest[16];
						char HexDigest[33];

						// append password
						char Key[256];
						sprintf_s(Key, sizeof(Key), "%s%s", Apop, pass);

						ZeroObj(Digest);
						MDStringToDigest(Digest, Key);
						for (int i = 0; i < 16; i++)
							sprintf_s(HexDigest + (i*2), 3, "%2.2x", Digest[i]);
						HexDigest[32] = 0;

						sprintf_s(Buffer, sizeof(Buffer), "APOP %s %s\r\n", user, HexDigest);
						VERIFY_ONERR(Write(0, true));
						Authed = ReadReply();

						if (!Authed)
						{
							DeleteArray(Apop);
							GVariant NoAPOP = true;
							if (SettingStore)
								SettingStore->SetValue(OPT_Pop3NoApop, NoAPOP);
							S->Close();
							goto ReStartConnection;
						}
					}

					if (!SecurityError && SecureAuth)
					{
						LHashTbl<ConstStrKey<char,false>, bool> AuthTypes, Capabilities;
						if (ListCmd("AUTH", AuthTypes) &&
							ListCmd("CAPA", Capabilities))
						{
							if (AuthTypes.Find("GSSAPI"))
							{
								sprintf_s(Buffer, sizeof(Buffer), "AUTH GSSAPI\r\n");
								VERIFY_ONERR(Write(0, true));
								VERIFY_ONERR(ReadReply());

								// http://www.faqs.org/rfcs/rfc2743.html
							}
						}
					}					
					else if (!SecurityError && !Authed)
					{
						// have to use non-key method
						sprintf_s(Buffer, sizeof(Buffer), "USER %s\r\n", user);
						VERIFY_ONERR(Write(0, true));
						VERIFY_ONERR(ReadReply());

						sprintf_s(Buffer, sizeof(Buffer), "PASS %s\r\n", pass);
						VERIFY_ONERR(Write(0, false));
						Log("PASS *******", GSocketI::SocketMsgSend);

						Authed = ReadReply();
					}

					DeleteArray(user);
					DeleteArray(pass);
				}

				if (Authed)
				{
					Status = true;
				}
				else
				{
					if (SocketLock.Lock(_FL))
					{
						Socket.Reset(0);
						SocketLock.Unlock();
					}
					LgiTrace("%s:%i - Failed auth.\n", _FL);
				}
			}
			else Error(_FL, "Failed to open socket to %s:%i and read reply.\n", Server, Port);
		}
		else Error(_FL, "No user/pass.\n");
	}

CleanUp:
	DeleteArray(Apop);
	DeleteArray(Server);

	return Status;
}

bool MailPop3::MailIsEnd(char *Ptr, ssize_t Len)
{
	for (char *c = Ptr; Len-- > 0; c++)
	{
		if (*c != *Marker)
		{
			Marker = End;
		}

		if (*c == *Marker)
		{
			Marker++;
			if (!*Marker)
			{
				return true;
			}
		}
	}

	return false;
}

bool MailPop3::Receive(GArray<MailTransaction*> &Trans, MailCallbacks *Callbacks)
{
	bool Status = false;

	if (Trans.Length() > 0 &&
		Socket)
	{
		for (unsigned n = 0; n<Trans.Length(); n++)
		{
			int Message = Trans[n]->Index;
			GStreamI *Msg = Trans[n]->Stream;
			if (Msg)
			{
				int Size = 0;
				
				// Transfer is not null when the caller wants info on the bytes comming in
				if (Transfer || Callbacks)
				{
					// get message size
					sprintf_s(Buffer, sizeof(Buffer), "LIST %i\r\n", Message + 1);
					VERIFY_RET_VAL(Write(0, true));
					VERIFY_RET_VAL(ReadReply());

					char *s = strchr(Buffer, ' ');
					if (s)
					{
						s = strchr(s+1, ' ');
						if (s)
						{
							Size = atoi(s);
						}
					}
				}

				MailSrcStatus Action = DownloadAll;
				int TopLines = 100;

				if (Callbacks && Callbacks->OnSrc)
				{
					Action = Callbacks->OnSrc(Trans[n], Size, &TopLines, Callbacks->CallbackData);
				}

				if (Action == DownloadAbort)
				{
					break;
				}

				if (Action == DownloadAll ||
					Action == DownloadTop)
				{
					if (Action == DownloadAll)
					{
						sprintf_s(Buffer, sizeof(Buffer), "RETR %i\r\n", Message + 1);
					}
					else
					{
						sprintf_s(Buffer, sizeof(Buffer), "TOP %i %i\r\n", Message + 1, TopLines);
					}
					VERIFY_RET_VAL(Write(0, true));

					GLinePrefix End(".\r\n");

					if (Transfer)
					{
						Transfer->Value = 0;
						Transfer->Range = Size;
						Transfer->Start = LgiCurrentTime();
					}

					// Read status line
					ZeroObj(Buffer);

					int Used = 0;
					bool Ok = false;
					bool Finished = false;
					int64 DataPos = 0;
					while (Socket->IsOpen())
					{
						ssize_t r = Socket->Read(Buffer+Used, sizeof(Buffer)-Used-1, 0);
						if (r > 0)
						{
							DeNullText(Buffer + Used, r);
							
							if (Transfer)
							{
								Transfer->Value += r;
							}

							char *Eol = strchr(Buffer, '\n');
							if (Eol)
							{
								Eol++;
								Ok = Buffer[0] == '+';
								if (Ok)
								{
									// Log(Buffer, GSocketI::SocketMsgReceive);

									// The Buffer was zero'd at the beginning garrenteeing
									// NULL termination
									size_t Len = strlen(Eol);
									ssize_t EndPos = End.IsEnd(Eol, Len);
									if (EndPos >= 0)
									{
										Msg->Write(Eol, EndPos - 3);
										Status = Trans[n]->Status = true;
										Finished = true;
									}
									else
									{
										Msg->Write(Eol, Len);
										DataPos += Len;
									}
								}
								else
								{
									Log(Buffer, GSocketI::SocketMsgError);
									Finished = true;								
								}
								break;
							}

							Used += r;
						}
						else break;
					}

					if (!Finished)
					{
						if (Ok)
						{
							// Read rest of message
							while (Socket->IsOpen())
							{
								ssize_t r = Socket->Read(Buffer, sizeof(Buffer), 0);
								if (r > 0)
								{
									DeNullText(Buffer, r);
									
									if (Transfer)
									{
										Transfer->Value += r;
									}

									ssize_t EndPos = End.IsEnd(Buffer, r);
									if (EndPos >= 0)
									{
										ssize_t Actual = EndPos - DataPos - 3;
										if (Actual > 0)
										{
											#ifdef _DEBUG
											ssize_t w =
											#endif
											Msg->Write(Buffer, Actual);
											LgiAssert(w == Actual);
										}
										// else the end point was in the last buffer

										Status = Trans[n]->Status = true;
										break;
									}
									else
									{
										#ifdef _DEBUG
										ssize_t w =
										#endif
										Msg->Write(Buffer, r);
										LgiAssert(w == r);
										DataPos += r;
									}
								}
								else
								{
									break;
								}
							}
							
							if (!Status)
							{
								LgiTrace("%s:%i - Didn't get end-of-mail marker.\n", _FL);
							}
						}
						else
						{
							LgiTrace("%s:%i - Didn't get Ok.\n", _FL);
							break;
						}
					}

					if (Callbacks && Callbacks->OnReceive)
					{
						Callbacks->OnReceive(Trans[n], Callbacks->CallbackData);
					}

					if (Transfer)
					{
						Transfer->Empty();
					}
				}
				else
				{
					Trans[n]->Oversize = Status = true;
				}

				if (Items)
				{
					Items->Value++;
				}
			}
			else
			{
				LgiTrace("%s:%i - No stream.\n", _FL);
			}
		}
	}
	else
	{
		LgiTrace("%s:%i - Arg check failed, len=%p, sock=%p.\n", _FL, Trans.Length(), Socket.Get());
	}

	return Status;
}

bool MailPop3::GetSizes(GArray<int> &Sizes)
{
	if (Socket)
	{
		strcpy_s(Buffer, sizeof(Buffer), (char*)"LIST\r\n");
		VERIFY_RET_VAL(Write(0, true));

		char *s = 0;
		if (ReadMultiLineReply(s))
		{
			GToken l(s, "\r\n");
			DeleteArray(s);
			for (unsigned i=0; i<l.Length(); i++)
			{
				char *sp = strrchr(l[i], ' ');
				if (sp)
				{
					Sizes[i] = atoi(sp+1);
				}
			}
		}
	}

	return Sizes.Length() > 0;
}

int MailPop3::Sizeof(int Message)
{
	int Size = 0;
	if (Socket)
	{
		sprintf_s(Buffer, sizeof(Buffer), "LIST %i\r\n", Message + 1);
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply());

		char *s = strchr(Buffer, ' ');
		if (s)
		{
			s = strchr(s+1, ' ');
			if (s)
			{
				Size = atoi(s);
			}
		}
	}

	return Size;
}

bool MailPop3::Delete(int Message)
{
	if (Socket)
	{
		sprintf_s(Buffer, sizeof(Buffer), "DELE %i\r\n", Message + 1);
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply());

		return true;
	}

	return false;
}

bool MailPop3::GetUid(int Index, char *Id, int IdLen)
{
	if (Socket && Id)
	{
		sprintf_s(Buffer, sizeof(Buffer), "UIDL %i\r\n", Index + 1);
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadReply());

		char *Space = strchr(Buffer, ' ');
		if (Space)
		{
			Space = strchr(Space+1, ' ');
			if (Space)
			{
				for (char *s = Space+1; *s; s++)
				{
					if (*s == '\r' || *s == '\n')
					{
						*s = 0;
						break;
					}
				}
				
				strcpy_s(Id, IdLen, Space+1);
				return true;
			}
		}
	}

	return false;
}

bool MailPop3::GetUidList(List<char> &Id)
{
	bool Status = false;
	if (Socket)
	{
		char *Str = 0;

		sprintf_s(Buffer, sizeof(Buffer), "UIDL\r\n");
		VERIFY_RET_VAL(Write(0, true));
		VERIFY_RET_VAL(ReadMultiLineReply(Str));
		if (Str)
		{
			Status = true;
			GToken T(Str, "\r\n");
			for (unsigned i=0; i<T.Length(); i++)
			{
				char *s = T[i];
				if (s && *s != '.')
				{
					char *Space = strchr(s, ' ');
					if (Space++)
					{
						Id.Insert(NewStr(Space));
					}
				}
			}

			DeleteArray(Str);
		}
	}

	return Status;
}

char *MailPop3::GetHeaders(int Message)
{
	char *Header = 0;
	if (Socket)
	{
		sprintf_s(Buffer, sizeof(Buffer), "TOP %i 0\r\n", Message + 1);
		if (Write(0, true))
		{
			ReadMultiLineReply(Header);
		}
	}

	return Header;
}

bool MailPop3::ReadMultiLineReply(char *&Str)
{
	bool Status = false;
	if (Socket)
	{
		GMemQueue Temp;
		ssize_t ReadLen = Socket->Read(Buffer, sizeof(Buffer), 0);
		if (ReadLen > 0 && Buffer[0] == '+')
		{
			// positive response
			char *Eol = strchr(Buffer, '\n');
			if (Eol)
			{
				char *Ptr = Eol + 1;
				ReadLen -= Ptr-Buffer;
				memmove(Buffer, Ptr, ReadLen);
				Temp.Write((uchar*) Buffer, ReadLen);

				while (!MailIsEnd(Buffer, ReadLen))
				{
					ReadLen = Socket->Read(Buffer, sizeof(Buffer), 0);
					if (ReadLen > 0)
					{
						Temp.Write((uchar*) Buffer, ReadLen);
					}
					else break;
				}

				int Len = (int)Temp.GetSize();
				Str = new char[Len+1];
				if (Str)
				{
					Temp.Read((uchar*)Str, Len);
					Str[Len] = 0;
					Status = true;
				}
			}
		}
	}

	return Status;
}

bool MailPop3::Close()
{
	if (Socket)
	{
		// logout
		VERIFY_RET_VAL(Write("QUIT\r\n", true));
		
		// 2 sec timeout, we don't really care about the server's response
		Socket->SetTimeout(2000);
		ReadReply();

		if (SocketLock.Lock(_FL))
		{
			Socket.Reset(0);
			SocketLock.Unlock();
		}
		Messages = 0;
		return true;
	}
	return false;
}
