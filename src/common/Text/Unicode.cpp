#include "lgi/common/Lgi.h"
#include "lgi/common/Unicode.h"

wchar_t *Utf8ToWide(const char *In, ssize_t InLen)
{
	if (!In)
		return NULL;

	#ifdef WIN32
		auto r = MultiByteToWideChar(CP_UTF8, 0, In, (int)InLen, NULL, 0);
		if (r <= 0) return NULL;
		auto s = new wchar_t[r + 1];
		if (!s) return NULL;
		r = MultiByteToWideChar(CP_UTF8, 0, In, (int)InLen, s, r);
		if (r <= 0) return NULL;
		s[r] = 0;
		return s;	
	#else
		return (wchar_t*) LNewConvertCp(LGI_WideCharset, In, "utf-8", InLen);
	#endif
}

char *WideToUtf8(const wchar_t *In, ptrdiff_t InLen)
{
	if (!In)
		return NULL;

	#ifdef WIN32
		auto r = WideCharToMultiByte(CP_UTF8, 0, In, (int)InLen, NULL, 0, 0, NULL);
		if (r <= 0) return NULL;
		auto s = new char[r + 1];
		// LStackTrace("WideToUtf8 %p\n", s);
		if (!s) return NULL;
		r = WideCharToMultiByte(CP_UTF8, 0, In, (int)InLen, s, r, 0, NULL);
		if (r <= 0) return NULL;
		s[r] = 0;
		return s;	
	#else
		return (char*) LNewConvertCp("utf-8", In, LGI_WideCharset, InLen*sizeof(*In));
	#endif
}

/////////////////////////////////////////////////////////////////////////////////////
bool LIsUtf8(const void *buf, ssize_t len)
{
	#define LenCheck(Need) \
		if (len >= 0 && (len - (s - Start)) < Need) \
			goto Utf8Error;
	#define TrailCheck() \
		if (!IsUtf8_Trail(*s)) \
			goto Utf8Error; \
		s++;

	auto s = (const uint8_t *) buf;
	if (!s || *s == 0)
		return true;

	auto Start = s;
	while
		(
			(
				len < 0 ||
				((s - Start) < len)
				)
			&&
			*s
			)
	{
		if (IsUtf8_1Byte(*s))
		{
			s++;
		}
		else if (IsUtf8_2Byte(*s))
		{
			s++;
			LenCheck(1);
			TrailCheck();
		}
		else if (IsUtf8_3Byte(*s))
		{
			s++;
			LenCheck(2);
			TrailCheck();
			TrailCheck();
		}
		else if (IsUtf8_4Byte(*s))
		{
			s++;
			LenCheck(3);
			TrailCheck();
			TrailCheck();
			TrailCheck();
		}
		else goto Utf8Error;
	}

	return true;

Utf8Error:
	#if 1
	LgiTrace("%s:%i - Invalid utf @ offset=%i, bytes=", _FL, (int) (s - Start));
	auto end = len < 0 ? NULL : Start + len;
	for (auto i = 0; i < 16; i++)
	{
		if
			(
				(end && s >= end)
				||
				*s == 0
				)
			break;
		LgiTrace("%02.2x,", (uint8_t)*s++);
	}
	LgiTrace("\n");
	#endif
	return false;
}

int32 LgiUtf8To32(uint8_t *&i, ssize_t &Len)
{
	int32 Out = 0;

	#define InvalidUtf()		{ Len--; i++; return -1; }

	if (Len > 0)
	{
		if (!*i)
		{
			Len = 0;
			return 0;
		}

		if (IsUtf8_1Byte(*i))
		{
			// 1 byte UTF-8
			Len--;
			return *i++;
		}
		else if (IsUtf8_2Byte(*i))
		{
			// 2 byte UTF-8
			if (Len > 1)
			{
				Out = ((int)(*i++ & 0x1f)) << 6;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= *i++ & 0x3f;
					Len--;
				}
				else InvalidUtf()
			}
		}
		else if (IsUtf8_3Byte(*i))
		{
			// 3 byte UTF-8
			if (Len > 2)
			{
				Out = ((int)(*i++ & 0x0f)) << 12;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= ((int)(*i++ & 0x3f)) << 6;
					Len--;

					if (IsUtf8_Trail(*i))
					{
						Out |= *i++ & 0x3f;
						Len--;
					}
					else InvalidUtf()
				}
				else InvalidUtf()
			}
		}
		else if (IsUtf8_4Byte(*i))
		{
			// 4 byte UTF-8
			if (Len > 3)
			{
				Out = ((int)(*i++ & 0x07)) << 18;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= ((int)(*i++ & 0x3f)) << 12;
					Len--;

					if (IsUtf8_Trail(*i))
					{
						Out |= ((int)(*i++ & 0x3f)) << 6;
						Len--;

						if (IsUtf8_Trail(*i))
						{
							Out |= *i++ & 0x3f;
							Len--;
						}
						else InvalidUtf()
					}
					else InvalidUtf()
				}
				else InvalidUtf()
			}
		}
		else InvalidUtf()
	}

	return Out;
}

bool LgiUtf32To8(uint32_t u32, uint8_t *&outBuf, ssize_t &outBufSize, bool warn = true)
{
	if ((u32 & ~0x7f) == 0)
	{
		if (outBufSize > 0)
		{
			*outBuf++ = u32;
			outBufSize--;
			return true;
		}
	}
	else if ((u32 & ~0x7ff) == 0)
	{
		if (outBufSize > 1)
		{
			*outBuf++ = 0xc0 | (u32 >> 6);
			*outBuf++ = 0x80 | (u32 & 0x3f);
			outBufSize -= 2;
			return true;
		}
	}
	else if ((u32 & 0xffff0000) == 0)
	{
		if (outBufSize > 2)
		{
			*outBuf++ = 0xe0 | (u32 >> 12);
			*outBuf++ = 0x80 | ((u32 & 0x0fc0) >> 6);
			*outBuf++ = 0x80 | (u32 & 0x3f);
			outBufSize -= 3;
			return true;
		}
	}
	else
	{
		if (outBufSize > 3)
		{
			*outBuf++ = 0xf0 | (u32 >> 18);
			*outBuf++ = 0x80 | ((u32 & 0x3f000) >> 12);
			*outBuf++ = 0x80 | ((u32 & 0xfc0) >> 6);
			*outBuf++ = 0x80 | (u32 & 0x3f);
			outBufSize -= 4;
			return true;
		}
	}

	if (warn)
		LAssert(!"Buffer size too small");
	return false;
}

uint32_t LgiUtf16To32(const uint16_t *&i, ssize_t &Bytes)
{
	if (Bytes > 1)
	{
		if (!*i)
		{
			Bytes = 0;
			return 0;
		}

		int n = *i & 0xfc00;
		if (n == 0xd800 || n == 0xdc00)
		{
			// 2 word UTF
			if (Bytes > 3)
			{
				Bytes -= sizeof(uint16)<<1;
				int w = (*i & 0x3c0) >> 6;
				int zy = *i++ & 0x3f;
				return ((w + 1) << 16) | (zy << 10) | (*i++ & 0x3ff);
			}
		}

		// 1 word UTF
		Bytes -= sizeof(uint16);
		return *i++;
	}

	return 0;
}

bool LgiUtf16To8(const uint16_t *&in, ssize_t &inBytes, uint8_t *&out, ssize_t &outSize)
{
	if (!in || !out || outSize <= 0)
		return false;
		
	if (!*in)
	{
		*out = 0;
		return true;
	}

	uint32_t u32 = LgiUtf16To32(in, inBytes);
	return LgiUtf32To8(u32, out, outSize);
}

bool LgiUtf32To16(uint32_t c, uint16_t *&i, ssize_t &Len)
{
	if (c >= 0x10000)
	{
		// 2 word UTF
		if (Len < 4)
			return false;

		int w = c - 0x10000;
		*i++ = 0xd800 + (w >> 10);
		*i++ = 0xdc00 + (c & 0x3ff);
		Len -= sizeof(*i) << 1;
	}
	else
	{
		if (Len < 2)
			return false;
		if (c > 0xD7FF && c < 0xE000)
			return false;

		// 1 word UTF
		*i++ = c;
		Len -= sizeof(*i);
		return true;
	}
	
	return false;
}
