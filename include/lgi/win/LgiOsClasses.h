/// \file
/// \author Matthew Allen

#ifndef __OS_CLASS_H
#define __OS_CLASS_H

class LgiClass OsApplication
{
protected:
	HCURSOR hNormalCursor;

public:
	OsApplication()
	{
		hNormalCursor = 0;
	}
};

// Automatic HDC wrapper:
class GdiAutoDC
{
	HWND hwnd = NULL;
	HDC hdc = NULL;

public:
	HDC Get(HWND hWnd = NULL)
	{
		if (!hdc)
		{
			hdc = GetDC(hwnd = hWnd);
			if (!hdc)
			{
				LError err(GetLastError());
				LgiTrace("%s:%i - GetDC failed: %s\n", _FL, err.GetMsg().Get());
			}
		}
		return hdc;
	}

	operator HDC()
	{
		return hdc;
	}

	~GdiAutoDC()
	{
		if (hdc)
		{
			ReleaseDC(hwnd, hdc);
			hdc = NULL;
		}
	}
};

// Win32 Memory Handler
class LgiClass GMem {

	int64 Size;
	HGLOBAL hMem;
	void *pMem;

public:
	GMem(int64 size)
	{
		Size = size;
		hMem = GlobalAlloc(GMEM_FIXED, (DWORD)Size);
		pMem = 0;
	}

	GMem(HGLOBAL hmem)
	{
		hMem = hmem;
		Size = GlobalSize(hMem);
		pMem = 0;
	}

	~GMem()
	{
		if (pMem)
		{
			GlobalUnlock(hMem);
			pMem = 0;
		}
		if (hMem)
		{
			GlobalFree(hMem);
			hMem = 0;
		}
	}

	void *Lock()
	{
		return pMem = GlobalLock(hMem);
	}

	void UnLock()
	{
		if (pMem)
		{
			GlobalUnlock(hMem);
			pMem = 0;
		}
	}

	int64 GetSize()
	{
		return Size;
	}

	void Detach()
	{
		if (pMem)
		{
			GlobalUnlock(hMem);
			pMem = 0;
		}
		hMem = 0;
		Size = 0;
	}

	HGLOBAL Handle() { return hMem; }
};

// Win32 window class
class LgiClass LWindowsClass : public LBase
{
	friend class LControl;
	friend class LApp;

	WNDPROC ParentProc;

#ifdef __GNUC__
public:
#endif
	static LRESULT CALLBACK Redir(OsView hWnd, UINT m, WPARAM a, LPARAM b);
	static LRESULT CALLBACK SubClassRedir(OsView hWnd, UINT m, WPARAM a, LPARAM b);

public:
	WNDCLASSEXW Class;

	LWindowsClass(const char *Name);
	~LWindowsClass();

	static bool IsSystem(const char *Cls);
	bool Register();
	bool SubClass(char *Parent);
	LRESULT CALLBACK CallParent(OsView hWnd, UINT m, WPARAM a, LPARAM b);

	static LWindowsClass *Create(const char *ClassName);
};

#include "lgi/common/RegKey.h"

#endif
