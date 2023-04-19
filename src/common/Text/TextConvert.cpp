#include "lgi/common/Lgi.h"
#include "lgi/common/TextConvert.h"
#include "lgi/common/Mime.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Charset.h"

// return true if there are any characters with the 0x80 bit set
bool Is8Bit(const char *Text)
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

char *DecodeBase64Str(char *Str, ssize_t Len)
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

LString LDecodeBase64Str(LString Str)
{
	LString r;
	ssize_t BinLen = BufferLen_64ToBin(Str.Length());
	if (Str && r.Length(BinLen) > 0)
	{
		ssize_t Converted = ConvertBase64ToBinary((uchar*)r.Get(), r.Length(), Str.Get(), Str.Length());
		if (Converted >= 0)
			r.Get()[Converted] = 0;
		else
			r.Empty();
	}

	return r;
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

LString LDecodeQuotedPrintableStr(LString InStr)
{
	if (!InStr.Get())
		return InStr;

	LString OutStr = InStr.Get();
	if (OutStr)
	{
		auto Out = OutStr.Get();
		auto In = InStr.Get();
		auto End = In + InStr.Length();

		while (In < End)
		{
			if (*In == '=')
			{
				In++;
				if (*In == '\r' || *In == '\n')
				{
					if (*In == '\r') In++;
					if (*In == '\n') In++;
				}
				else
				{
					char c = ConvHexToBin(*In++) << 4;
					c     |= ConvHexToBin(*In++) & 0xF;
					*Out++ = c;
				}
			}
			else
			{
				*Out++ = *In++;
			}
		}

		// Correct the size and NULL terminate.
		OutStr.Length(Out - OutStr.Get());
	}

	return OutStr;
}

char *DecodeRfc2047(char *Str)
{
	if (!Str)
		return NULL;
	
	LStringPipe p(256);
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
				LString Cp(Start, First - Start);
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

					LString Block(Second, End - Second);
					if (Block)
					{
						switch (Type)
						{
							case CONTENT_BASE64:
								Block = LDecodeBase64Str(Block);
								break;
							case CONTENT_QUOTED_PRINTABLE:
								Block = LDecodeQuotedPrintableStr(Block);
								break;
						}

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
							p.Write(Block);
						}
						else
						{
							auto Inst = LCharsetSystem::Inst();
							LString Detect = Inst && Inst->DetectCharset ? Inst->DetectCharset(Block) : LString();

							LAutoString Utf8((char*)LNewConvertCp("utf-8", Block, Detect ? Detect : Cp, Block.Length()));
							if (Utf8)
							{
								if (LIsUtf8(Utf8))
									p.Write((uchar*)Utf8.Get(), strlen(Utf8));
							}
							else
							{
								p.Write(Block);
							}
						}
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
			LAssert(*e == 0);
			if (e > s)
				p.Write(s, e - s);
			break;
		}
	}

	DeleteArray(Str);
	return p.NewStr();
}

#define MIME_MAX_LINE		76

static void EncodeRfc2047_Impl(	char *Str, size_t Length,
								const char *Charset,
								List<char> *CharsetPrefs,
								ssize_t LineLength,
								std::function<void(char*, LStringPipe&)> Process)
{
	if (!Str)
		return;

	if (!Charset)
		Charset = "utf-8";

	LStringPipe p(256);
	if (Is8Bit(Str))
	{
		// pick an encoding
		bool Base64 = false;
		const char *DestCp = "utf-8";
		if (Stricmp(Charset, "utf-8") == 0)
			DestCp = LUnicodeToCharset(Str, Length, CharsetPrefs);

		int Chars = 0;
		for (unsigned i=0; i<Length; i++)
		{
			if (Str[i] & 0x80)
				Chars++;
		}
		
		if
		(
			stristr(DestCp, "utf") ||
			(
				Length > 0 &&
				((double)Chars/Length) > 0.4
			)
		)
		{
			Base64 = true;
		}

		char *Buf = (char*)LNewConvertCp(DestCp, Str, Charset, Length);
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

		Process(Str, p);
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
			
			Process(Str, p);
		}
	}

	// It's not an error to not call 'OutputStr', in that case 
	// the input is passed through to the output unchanged.
}

// Old heap string encode method (will eventually remove this...)
char *EncodeRfc2047(char *Str, const char *Charset, List<char> *CharsetPrefs, ssize_t LineLength)
{
	char *Out = Str;

	EncodeRfc2047_Impl(	Str, Strlen(Str),
						Charset,
						CharsetPrefs,
						LineLength,
						[&Out](auto s, auto &pipe)
						{
							DeleteArray(s);
							Out = pipe.NewStr();
						});

	return Out;
}

// New LString encode method
LString LEncodeRfc2047(LString Str, const char *Charset, List<char> *CharsetPrefs, ssize_t LineLength)
{
	EncodeRfc2047_Impl(	Str.Get(), Str.Length(),
						Charset,
						CharsetPrefs,
						LineLength,
						[&Str](auto s, auto &pipe)
						{
							Str = pipe.NewLStr();
						});

	return Str;
}

