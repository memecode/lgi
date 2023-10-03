#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "lgi/common/LgiInc.h"
#include "lgi/common/GdiLeak.h"

#if GDI_LEAK_DETECT

#include "lgi/common/Mem.h"
#include "lgi/common/StringClass.h"

enum HndType
{
	HndNull,
	HndAnyGdi,
	HndDcPaint,
	HndDcDelete,
	HndDcRelease,
	HndBrush,
	HndFont,
	HndPen,
	HndBitmap,
	HndRegion,
	HndPalette,
};

struct HndInfo
{
	HndType Type;
	HANDLE Hnd;
	const char *File;
	int Line;
};

static LArray<HndInfo> Handles;
LgiFunc void LgiTrace(char *Format, ...);

class GdiCleanup
{
public:
	~GdiCleanup()
	{
		if (Handles.Length() > 0)
		{
			for (int i=0; i<Handles.Length(); i++)
			{
				HndInfo *h = &Handles[i];
				LgiTrace("GdiResourceLeak: Type=%i @ %s:%i\n", h->Type, h->File, h->Line);
			}
		}
	}
} Cleanup;

static HndInfo *GetHandle(HANDLE h)
{
	for (int i=0; i<Handles.Length(); i++)
	{
		if (Handles[i].Hnd == h)
		{
			return &Handles[i];
		}
	}

	return 0;
}

static void AddHandle(const char *f, int l, HANDLE hnd, HndType t)
{
	if (!hnd)
		return;

	auto &h = Handles.New();
	h.Type = t;
	h.File = f;
	h.Line = l;
	h.Hnd = hnd;
}

static void DeleteHandle(const char *f, int l, HANDLE hnd, HndType t)
{
	size_t i;
	HndInfo *h = NULL;
	
	if (!hnd || Handles.Length() == 0)
		return;

	for (i=0; i<Handles.Length(); i++)
	{
		if (Handles[i].Hnd == hnd)
		{
			h = Handles.AddressOf(i);
			break;
		}
	}

	if (h)
	{
		if (h->Type == t || (h->Type >= HndBrush && t == HndAnyGdi))
		{
			Handles.DeleteAt(i);
		}
		else
		{
			// Wrong type of handle
			LAssert(0);
		}
	}
	else
	{
		// Handle wasn't allocated
		LAssert(0);
	}
}

HDC Lgi_BeginPaint(const char *File, int Line, HWND hWnd, LPPAINTSTRUCT lpPaint)
{
	#undef BeginPaint

	HDC hdc = BeginPaint(hWnd, lpPaint);
	if (hdc)
	{
		AddHandle(File, Line, hdc, HndDcPaint);
	}
	else
	{
		LAssert(0);
	}

	return hdc;
}

BOOL Lgi_EndPaint(const char *File, int Line, HWND hWnd, CONST PAINTSTRUCT *lpPaint)
{
	#undef EndPaint
	DeleteHandle(File, Line, lpPaint->hdc, HndDcPaint);

	// LgiTrace("Handles=%i\n", Handles.Length());

	return EndPaint(hWnd, lpPaint);
}

HDC Lgi_CreateCompatibleDC(const char *File, int Line, HDC hInDC)
{
	#undef CreateCompatibleDC

	HDC hdc = CreateCompatibleDC(hInDC);
	if (hdc)
	{
		AddHandle(File, Line, hdc, HndDcDelete);
	}
	else
	{
		LAssert(0);
	}

	return hdc;
}

BOOL Lgi_DeleteDC(const char *File, int Line, HDC hdc)
{
	#undef DeleteDC
	DeleteHandle(File, Line, hdc, HndDcDelete);
	return DeleteDC(hdc);
}

HDC Lgi_GetDC(const char *File, int Line, HWND hWnd)
{
	#undef GetDC
	HDC hdc = GetDC(hWnd);
	if (hdc)
	{
		AddHandle(File, Line, hdc, HndDcRelease);
	}
	else
	{
		LAssert(0);
	}
	return hdc;
}

HDC Lgi_GetWindowDC(const char *File, int Line, HWND hWnd)
{
	#undef GetWindowDC
	HDC hdc = GetWindowDC(hWnd);
	if (hdc)
	{
		AddHandle(File, Line, hdc, HndDcRelease);
	}
	else
	{
		LAssert(0);
	}
	return hdc;
}

int Lgi_ReleaseDC(const char *File, int Line, HWND hWnd, HDC hDC)
{
	#undef ReleaseDC
	DeleteHandle(File, Line, hDC, HndDcRelease);
	return ReleaseDC(hWnd, hDC);
}

HBRUSH Lgi_CreateBrushIndirect(const char *File, int Line, CONST LOGBRUSH *LogBrush)
{
	#undef CreateBrushIndirect
	HBRUSH h = CreateBrushIndirect(LogBrush);
	if (h)
	{
		AddHandle(File, Line, h, HndBrush);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateDIBPatternBrush(const char *File, int Line, HGLOBAL hglbDIBPacked, UINT fuColorSpec)
{
	#undef CreateDIBPatternBrush
	HBRUSH h = CreateDIBPatternBrush(hglbDIBPacked, fuColorSpec);
	if (h)
	{
		AddHandle(File, Line, h, HndBrush);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateDIBPatternBrushPt(const char *File, int Line, CONST VOID *lpPackedDIB, UINT iUsage)
{
	#undef CreateDIBPatternBrushPt
	HBRUSH h = CreateDIBPatternBrushPt(lpPackedDIB, iUsage);
	if (h)
	{
		AddHandle(File, Line, h, HndBrush);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateHatchBrush(const char *File, int Line, int fnStyle, COLORREF clrref)
{
	#undef CreateHatchBrush
	HBRUSH h = CreateHatchBrush(fnStyle, clrref);
	if (h)
	{
		AddHandle(File, Line, h, HndBrush);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreatePatternBrush(const char *File, int Line, HBITMAP hbmp)
{
	#undef CreatePatternBrush
	HBRUSH h = CreatePatternBrush(hbmp);
	if (h)
	{
		AddHandle(File, Line, h, HndBrush);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateSolidBrush(const char *File, int Line, COLORREF crColor)
{
	#undef CreateSolidBrush
	HBRUSH h = CreateSolidBrush(crColor);
	if (h)
	{
		AddHandle(File, Line, h, HndBrush);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HFONT Lgi_CreateFontA
(
	char *File, int Line,
	int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight,
	DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet,
	DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality,
	DWORD fdwPitchAndFamily, LPCSTR lpszFace
)
{
	#undef CreateFontA
	HFONT h = CreateFontA(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, lpszFace);
	if (h)
	{
		AddHandle(File, Line, h, HndFont);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HFONT Lgi_CreateFontW
(
	const char *File, int Line,
	int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight,
	DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet,
	DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily,
	LPCWSTR lpszFace
)
{
	#undef CreateFontW
	HFONT h = CreateFontW(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, lpszFace);
	if (h)
	{
		AddHandle(File, Line, h, HndFont);
	}
	else
	{
		LAssert(0);
	}
	return h;
}


HFONT Lgi_CreateFontIndirectA(const char *File, int Line, LOGFONTA *lplf)
{
	#undef CreateFontIndirectA
	HFONT hnd;
	AddHandle(File, Line, hnd = CreateFontIndirectA(lplf), HndFont);
	return hnd;
}

HFONT Lgi_CreateFontIndirectW(const char *File, int Line, LOGFONTW *lplf)
{
	#undef CreateFontIndirectW
	HFONT hnd;
	AddHandle(File, Line, hnd = CreateFontIndirectW(lplf), HndFont);
	return hnd;
}

HPEN Lgi_CreatePen(const char *File, int Line, int fnPenStyle, int nWidth, COLORREF crColor)
{
	#undef CreatePen
	HPEN h = CreatePen(fnPenStyle, nWidth, crColor);
	if (h)
	{
		AddHandle(File, Line, h, HndPen);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HPEN Lgi_CreatePenIndirect(const char *File, int Line, CONST LOGPEN *lplgpn)
{
	#undef CreatePenIndirect
	HPEN h = CreatePenIndirect(lplgpn);
	if (h)
	{
		AddHandle(File, Line, h, HndPen);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HPEN Lgi_ExtCreatePen(const char *File, int Line, DWORD dwPenStyle, DWORD dwWidth, CONST LOGBRUSH *lplb, DWORD dwStyleCount, CONST DWORD *lpStyle)
{
	#undef ExtCreatePen
	HPEN h = ExtCreatePen(dwPenStyle, dwWidth, lplb, dwStyleCount, lpStyle);
	if (h)
	{
		AddHandle(File, Line, h, HndPen);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

BOOL Lgi_DeleteObject(const char *File, int Line, HGDIOBJ hObject)
{
	#undef DeleteObject
	DeleteHandle(File, Line, hObject, HndAnyGdi);
	return DeleteObject(hObject);
}

HGDIOBJ Lgi_SelectObject(const char *File, int Line, HDC hdc, HGDIOBJ hgdiobj)
{
	#undef SelectObject
	HGDIOBJ h = SelectObject(hdc, hgdiobj);
	LAssert(h);
	return h;
}

HBITMAP Lgi_CreateDIBSection(const char *File, int Line, HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage, VOID **ppvBits, HANDLE hSection, DWORD dwOffset)
{
	#undef CreateDIBSection
	HBITMAP h = CreateDIBSection(hdc, pbmi, iUsage, ppvBits, hSection, dwOffset);
	if (h)
	{
		AddHandle(File, Line, h, HndBitmap);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HRGN Lgi_CreateRectRgn(const char *File, int Line, int a, int b, int c, int d)
{
	#undef CreateRectRgn
	HRGN h = CreateRectRgn(a, b, c, d);
	if (h)
	{
		AddHandle(File, Line, h, HndRegion);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

HPALETTE Lgi_CreatePalette(const char *File, int Line, CONST LOGPALETTE *lplgpl)
{
	#undef CreatePalette
	HPALETTE h = CreatePalette(lplgpl);
	if (h)
	{
		AddHandle(File, Line, h, HndPalette);
	}
	else
	{
		LAssert(0);
	}
	return h;
}

#endif