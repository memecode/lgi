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
class LgiClass LGlobalMem {

	int64 Size = 0;
	HGLOBAL hMem = nullptr;
	void *pMem = nullptr;

public:
	LGlobalMem(int64 size, int flags = GMEM_FIXED)
	{
		Size = size;
		hMem = GlobalAlloc(flags, (DWORD)Size);
	}

	LGlobalMem(HGLOBAL hmem)
	{
		if (hMem = hmem)
			Size = GlobalSize(hMem);
		else
			LAssert(!"null handle");
	}

	~LGlobalMem()
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

	HGLOBAL Release()
	{
		UnLock();

		auto h = hMem;
		hMem = nullptr;
		Size = 0;

		return h;
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
			pMem = nullptr;
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
			pMem = nullptr;
		}
		hMem = nullptr;
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
