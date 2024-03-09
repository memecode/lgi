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
bool LIsUtf8(const char *s, ssize_t len)
{
	#define LenCheck(Need) \
		if (len >= 0 && (len - (s - Start)) < Need) \
			goto Utf8Error;
	#define TrailCheck() \
		if (!IsUtf8_Trail(*s)) \
			goto Utf8Error; \
		s++;

	if (!s || *s == 0)
		return true;

	const char *Start = s;
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

