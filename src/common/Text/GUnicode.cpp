#include "Lgi.h"
#include "GUnicode.h"

wchar_t *Utf8ToWide(const char *In, ssize_t InLen)
{
	if (!In)
		return NULL;

	#ifdef WIN32
		auto r = MultiByteToWideChar(CP_UTF8, 0, In, InLen, NULL, 0);
		if (r <= 0) return NULL;
		auto s = new wchar_t[r + 1];
		if (!s) return NULL;
		r = MultiByteToWideChar(CP_UTF8, 0, In, InLen, s, r);
		if (r <= 0) return NULL;
		s[r] = 0;
		return s;	
	#else
		return (wchar_t*) LgiNewConvertCp(LGI_WideCharset, In, "utf-8", InLen);
	#endif
}

char *WideToUtf8(const wchar_t *In, ptrdiff_t InLen)
{
	if (!In)
		return NULL;

	#ifdef WIN32
		auto r = WideCharToMultiByte(CP_UTF8, 0, In, InLen, NULL, 0, 0, NULL);
		if (r <= 0) return NULL;
		auto s = new char[r + 1];
		// LgiStackTrace("WideToUtf8 %p\n", s);
		if (!s) return NULL;
		r = WideCharToMultiByte(CP_UTF8, 0, In, InLen, s, r, 0, NULL);
		if (r <= 0) return NULL;
		s[r] = 0;
		return s;	
	#else
		return (char*) LgiNewConvertCp("utf-8", In, LGI_WideCharset, InLen*sizeof(*In));
	#endif
}

