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

	if (!h)
	{
		// Handle wasn't allocated
		LAssert(!"Handle wasn't allocated");
		return;
	}

	if (h->Type == t || (h->Type >= HndBrush && t == HndAnyGdi))
	{
		Handles.DeleteAt(i);
	}
	else
	{
		// Wrong type of handle
		LAssert(!"Wrong type of handle?");
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

	HDC hdc;
	AddHandle(File, Line, hdc = CreateCompatibleDC(hInDC), HndDcDelete);
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
	HDC hdc;
	AddHandle(File, Line, hdc = GetDC(hWnd), HndDcRelease);
	return hdc;
}

HDC Lgi_GetWindowDC(const char *File, int Line, HWND hWnd)
{
	#undef GetWindowDC
	HDC hdc;
	AddHandle(File, Line, hdc = GetWindowDC(hWnd), HndDcRelease);
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
	HBRUSH h;
	AddHandle(File, Line, h = CreateBrushIndirect(LogBrush), HndBrush);
	return h;
}

HBRUSH Lgi_CreateDIBPatternBrush(const char *File, int Line, HGLOBAL hglbDIBPacked, UINT fuColorSpec)
{
	#undef CreateDIBPatternBrush
	HBRUSH h;
	AddHandle(File, Line, h = CreateDIBPatternBrush(hglbDIBPacked, fuColorSpec), HndBrush);
	return h;
}

HBRUSH Lgi_CreateDIBPatternBrushPt(const char *File, int Line, CONST VOID *lpPackedDIB, UINT iUsage)
{
	#undef CreateDIBPatternBrushPt
	HBRUSH h;
	AddHandle(File, Line, h = CreateDIBPatternBrushPt(lpPackedDIB, iUsage), HndBrush);
	return h;
}

HBRUSH Lgi_CreateHatchBrush(const char *File, int Line, int fnStyle, COLORREF clrref)
{
	#undef CreateHatchBrush
	HBRUSH h;
	AddHandle(File, Line, h = CreateHatchBrush(fnStyle, clrref), HndBrush);
	return h;
}

HBRUSH Lgi_CreatePatternBrush(const char *File, int Line, HBITMAP hbmp)
{
	#undef CreatePatternBrush
	HBRUSH h;
	AddHandle(File, Line, h = CreatePatternBrush(hbmp), HndBrush);
	return h;
}

HBRUSH Lgi_CreateSolidBrush(const char *File, int Line, COLORREF crColor)
{
	#undef CreateSolidBrush
	HBRUSH h;
	AddHandle(File, Line, h = CreateSolidBrush(crColor), HndBrush);
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
	HFONT fnt;
	AddHandle(File, Line,
		fnt = CreateFontW(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline,
						  fdwStrikeOut, fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, 
						  fdwPitchAndFamily, lpszFace),
			HndFont);
	return fnt;
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
	HPEN pen;
	AddHandle(File, Line, pen = CreatePen(fnPenStyle, nWidth, crColor), HndPen);
	return pen;
}

HPEN Lgi_CreatePenIndirect(const char *File, int Line, CONST LOGPEN *lplgpn)
{
	#undef CreatePenIndirect
	HPEN pen;
	AddHandle(File, Line, pen = CreatePenIndirect(lplgpn), HndPen);
	return pen;
}

HPEN Lgi_ExtCreatePen(const char *File, int Line, DWORD dwPenStyle, DWORD dwWidth, CONST LOGBRUSH *lplb, DWORD dwStyleCount, CONST DWORD *lpStyle)
{
	#undef ExtCreatePen
	HPEN pen;
	AddHandle(File, Line, pen = ExtCreatePen(dwPenStyle, dwWidth, lplb, dwStyleCount, lpStyle), HndPen);
	return pen;
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
	return h;
}

HBITMAP Lgi_CreateDIBSection(const char *File, int Line, HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage, VOID **ppvBits, HANDLE hSection, DWORD dwOffset)
{
	#undef CreateDIBSection
	HBITMAP h;
	AddHandle(File, Line, h = CreateDIBSection(hdc, pbmi, iUsage, ppvBits, hSection, dwOffset), HndBitmap);
	return h;
}

HRGN Lgi_CreateRectRgn(const char *File, int Line, int a, int b, int c, int d)
{
	#undef CreateRectRgn
	HRGN h = CreateRectRgn(a, b, c, d);
	AddHandle(File, Line, h = CreateRectRgn(a, b, c, d), HndRegion);
	return h;
}

HPALETTE Lgi_CreatePalette(const char *File, int Line, CONST LOGPALETTE *lplgpl)
{
	#undef CreatePalette
	HPALETTE h = CreatePalette(lplgpl);
	AddHandle(File, Line, h = CreatePalette(lplgpl), HndPalette);
	return h;
}

#endif