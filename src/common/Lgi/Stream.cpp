#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Stream.h"

//////////////////////////////////////////////////////////////////////
int LPrintf(LString &Str, const char *Format, va_list &Arg)
{
	int Bytes = 0;
	
	if (Format)
	{
		#ifndef WIN32
		va_list Cpy;
		va_copy(Cpy, Arg);
		Bytes = vsnprintf(0, 0, Format, Cpy);
		#else
		Bytes = _vscprintf(Format, Arg);
		#endif

		if (Bytes > 0 && Str.Set(NULL, Bytes))
			vsprintf_s(Str.Get(), Bytes+1, Format, Arg);
	}

	return Bytes;
}

ssize_t LPrintf(LAutoString &Str, const char *Format, va_list &Arg)
{
	ssize_t Bytes = 0;
	
	if (Format)
	{
		#ifndef WIN32
		va_list Cpy;
		va_copy(Cpy, Arg);
		Bytes = vsnprintf(0, 0, Format, Cpy);
		#else
		Bytes = _vscprintf(Format, Arg);
		#endif

		if (Bytes > 0 && Str.Reset(new char[Bytes+1]))
			vsprintf_s(Str, Bytes+1, Format, Arg);
	}

	return Bytes;
}

ssize_t LStreamPrintf(LStreamI *Stream, int Flags, const char *Format, va_list &Arg)
{
	if (!Stream || !Format)
		return 0;

	LAutoString a;
	ssize_t Bytes = LPrintf(a, Format, Arg);
	if (!a || Bytes == 0)
		return 0;
	
	return Stream->Write(a, Bytes, Flags);
}

ssize_t LStreamPrint(LStreamI *s, const char *fmt, ...)
{
	va_list Arg;
	va_start(Arg, fmt);
    ssize_t Ch = LStreamPrintf(s, 0, fmt, Arg);
	va_end(Arg);
	return Ch;
}

ssize_t LStream::Print(const char *Format, ...)
{
	va_list Arg;
	va_start(Arg, Format);
    ssize_t Ch = LStreamPrintf(this, 0, Format, Arg);
	va_end(Arg);
	return Ch;
}

/////////////////////////////////////////////////////////////////
LStreamOp::LStreamOp(int64 BufSz)
{
	StartTime = 0;
	EndTime = 0;
	Total = 0;
	if (BufSz > 0)
		Buffer.Length(BufSz);
}

ssize_t LStreamOp::GetRate()
{
	return (Total * GetElapsedTime()) / 1000;
}

ssize_t LStreamOp::GetTotal()
{
	return Total;
}

ssize_t LStreamOp::GetElapsedTime()
{
	if (EndTime)
	{
		return EndTime - StartTime;
	}
	
	return LCurrentTime() - StartTime;
}

////////////////////////////////////////////////////////////////
// Stateful parser that matches the start of lines to the 'prefix'
LHtmlLinePrefix::LHtmlLinePrefix(const char *p, bool eol)
{
	Start = true;
	At = 0;
	Pos = 0;
	Eol = eol;
	Prefix = NewStr(p);
}

LHtmlLinePrefix::~LHtmlLinePrefix()
{
	DeleteArray(Prefix);
}

void LHtmlLinePrefix::Reset()
{
	Start = true;
	At = 0;
	Pos = 0;
}

ssize_t LHtmlLinePrefix::IsEnd(void *v, ssize_t Len)
{
	if (Prefix)
	{
		char *s = (char*)v;
		for (int i=0; i<Len; i++, Pos++)
		{
			if (At)
			{
				if (toupper(*At) == toupper(s[i]))
				{
					At++;
					if (!*At)
					{
						// Sequence found
						return Pos + 1 - (Eol ? 0 : (int)strlen(Prefix));
					}
				}
				else At = 0;
			}
			else if (s[i] == '\n')
			{
				Start = true;
			}
			else if (Start)
			{
				if (Prefix[0] &&
					s[i] == Prefix[0])
				{
					At = Prefix + 1;
				}

				Start = false;
			}
		}
	}

	return -1;
}

ssize_t LEndOfLine::IsEnd(void *s, ssize_t Len)
{
	for (int i=0; i<Len; i++)
	{
		if (((char*)s)[i] == '\n')
		{
			return i;
		}
	}

	return -1;
}

/////////////////////////////////////////////////////////////////////////////////
ssize_t LCopyStreamer::Copy(LStreamI *Source, LStreamI *Dest, LStreamEnd *End)
{
	if (!Source || !Dest)
		return -1;

	int64 Bytes = 0;
	ssize_t r, w, e = -1;
	StartTime = LCurrentTime();

	if (Buffer.Length() == 0)
		Buffer.Length(4 << 10);

	while (e < 0)
	{
		if ((r = Source->Read(Buffer.AddressOf(), Buffer.Length())) > 0)
		{
			if (End)
			{
				e = End->IsEnd(Buffer.AddressOf(), r);
			}

			if ((w = Dest->Write(Buffer.AddressOf(), e >= 0 ? MIN(e, r) : r)) > 0)
			{
				Bytes += w;
				Total += w;
			}
			else break;
		}
		else break;
	}
	
	EndTime = LCurrentTime();

	return Bytes;
}

