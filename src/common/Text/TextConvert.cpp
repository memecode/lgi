#include "lgi/common/Lgi.h"
#include "lgi/common/TextConvert.h"
#include "lgi/common/Mime.h"
#include "lgi/common/Base64.h"

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
							GAutoString Utf8((char*)LNewConvertCp("utf-8", Block, Cp, Len));
							if (Utf8)
							{
								if (LIsUtf8(Utf8))
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

		char *Buf = (char*)LNewConvertCp(DestCp, Str, CodePage, Len);
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

