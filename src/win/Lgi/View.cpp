/*hdr
**      FILE:           LView.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           23/4/98
**      DESCRIPTION:    Win32 LView Implementation
**
**      Copyright (C) 1998-2003, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <time.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Com.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/DropFiles.h"
#include "lgi/common/GdiLeak.h"
#include "lgi/common/Css.h"
#include "lgi/common/Edit.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Menu.h"
#include "lgi/common/Thread.h"

#include "ViewPriv.h"

#define DEBUG_MOUSE_CLICKS		0
#define DEBUG_OVER				0
#define OLD_WM_CHAR_MODE		1

////////////////////////////////////////////////////////////////////////////////////////////////////
bool In_SetWindowPos = false;
HWND LViewPrivate::hPrevCapture = 0;

LViewPrivate::LViewPrivate(LView *view) : View(view)
{
	WndStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
	IsThemed = LResources::DefaultColours;
}

LViewPrivate::~LViewPrivate()
{
	while (EventTargets.Length())
		delete EventTargets[0];

	if (hTheme)
	{
		CloseThemeData(hTheme);
		hTheme = NULL;
	}
	if (FontOwnType == GV_FontOwned)
	{
		DeleteObj(Font);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Stuff
#include "zmouse.h"

int MouseRollMsg = 0;

#ifdef __GNUC__
#define MSH_WHEELMODULE_CLASS	"MouseZ"
#define MSH_WHEELMODULE_TITLE	"Magellan MSWHEEL"
#define MSH_SCROLL_LINES		"MSH_SCROLL_LINES_MSG" 
#endif

int _lgi_mouse_wheel_lines()
{
	UINT nScrollLines;
	if (SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, (PVOID) &nScrollLines, 0))
		return nScrollLines;
	return 3;
}

#define SetKeyFlag(v, k, f)		if (GetKeyState(k)&0xFF00) { v |= f; }

int _lgi_get_key_flags()
{
	int Flags = 0;
	
	if (LGetOs() == LGI_OS_WIN9X)
	{
		SetKeyFlag(Flags, VK_MENU, LGI_EF_ALT);

		SetKeyFlag(Flags, VK_SHIFT, LGI_EF_SHIFT);

		SetKeyFlag(Flags, VK_CONTROL, LGI_EF_CTRL);
	}
	else // is NT/2K/XP
	{
		SetKeyFlag(Flags, VK_LMENU, LGI_EF_LALT);
		SetKeyFlag(Flags, VK_RMENU, LGI_EF_RALT);

		SetKeyFlag(Flags, VK_LSHIFT, LGI_EF_LSHIFT);
		SetKeyFlag(Flags, VK_RSHIFT, LGI_EF_RSHIFT);

		SetKeyFlag(Flags, VK_LCONTROL, LGI_EF_LCTRL);
		SetKeyFlag(Flags, VK_RCONTROL, LGI_EF_RCTRL);
	}

	if (GetKeyState(VK_CAPITAL))
		SetFlag(Flags, LGI_EF_CAPS_LOCK);

	return Flags;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int GetInputACP()
{
	char16 Str[16];
	LCID Lcid = (NativeInt)GetKeyboardLayout(LCurrentThreadId()) & 0xffff;
	GetLocaleInfo(Lcid, LOCALE_IDEFAULTANSICODEPAGE, Str, sizeof(Str));
	return _wtoi(Str);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
bool CastHwnd(T *&Ptr, HWND hWnd)
{
	#if _MSC_VER >= _MSC_VER_VS2005
	LONG_PTR user = GetWindowLongPtr(hWnd, GWLP_USERDATA);
	#else
	LONG user = GetWindowLong(hWnd, GWL_USERDATA);
	#endif
	LONG magic = GetWindowLong(hWnd, GWL_LGI_MAGIC);

	if (magic != LGI_GViewMagic)
	{
		TCHAR ClsName[256] = {0};
		int Ch = GetClassName(hWnd, ClsName, CountOf(ClsName));
		LString Cls = ClsName;
		// LgiTrace("%s:%i - Error: hWnd=%p/%s, GWL_LGI_MAGIC=%i\n", _FL, hWnd, Cls.Get(), magic);
		return false;
	}
	Ptr = dynamic_cast<T*>((LViewI*)user);
	return Ptr != NULL;
}

bool SetLgiMagic(HWND hWnd)
{
	SetLastError(0);
	LONG res = SetWindowLong(hWnd, GWL_LGI_MAGIC, LGI_GViewMagic);
	bool Status = res != 0;
	if (!Status)
	{
		DWORD err = GetLastError();
		Status = err == 0;
	}

	LONG v = GetWindowLong(hWnd, GWL_LGI_MAGIC);
	// LgiTrace("set LGI_GViewMagic for %p, %i, %i\n", hWnd, Status, v);
	
	return Status;
}

LRESULT CALLBACK LWindowsClass::Redir(HWND hWnd, UINT m, WPARAM a, LPARAM b)
{
	if (m == WM_NCCREATE)
	{
		LPCREATESTRUCT Info = (LPCREATESTRUCT) b;
		
		LViewI *ViewI = (LViewI*) Info->lpCreateParams;
		if (ViewI)
		{
			LView *View = ViewI->GetLView();
			if (View) View->_View = hWnd;

			#if _MSC_VER >= _MSC_VER_VS2005
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ViewI);
			#else
			SetWindowLong(hWnd, GWL_USERDATA, (LONG)ViewI);
			#endif
			SetLgiMagic(hWnd);
		}
	}

	LViewI *Wnd = (LViewI*)
		#if _MSC_VER >= _MSC_VER_VS2005
		GetWindowLongPtr(hWnd, GWLP_USERDATA);
		#else
		GetWindowLong(hWnd, GWL_USERDATA);
		#endif
	if (Wnd)
	{
		LMessage Msg(m, a, b);
		Msg.hWnd = hWnd;
		return Wnd->OnEvent(&Msg);
	}

	return DefWindowProcW(hWnd, m, a, b);
}

LRESULT CALLBACK LWindowsClass::SubClassRedir(HWND hWnd, UINT m, WPARAM a, LPARAM b)
{
	if (m == WM_NCCREATE)
	{
		LPCREATESTRUCT Info = (LPCREATESTRUCT) b;
		LViewI *ViewI = 0;
		if (Info->lpCreateParams)
		{
			if (ViewI = (LViewI*) Info->lpCreateParams)
			{
				LView *View = ViewI->GetLView();
				if (View)
					View->_View = hWnd;
			}
		}
		#if _MSC_VER >= _MSC_VER_VS2005
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) ViewI);
		#else
		SetWindowLong(hWnd, GWL_USERDATA, (LONG) ViewI);
		#endif
		SetLgiMagic(hWnd);
	}

	LViewI *Wnd = (LViewI*)
		#if _MSC_VER >= _MSC_VER_VS2005
		GetWindowLongPtr(hWnd, GWLP_USERDATA);
		#else
		GetWindowLong(hWnd, GWL_USERDATA);
		#endif
	if (Wnd)
	{
		LMessage Msg(m, a, b);
		Msg.hWnd = hWnd;
		LMessage::Result Status = Wnd->OnEvent(&Msg);
		return Status;
	}

	return DefWindowProcW(hWnd, m, a, b);
}

LWindowsClass::LWindowsClass(const char *name)
{
	Name(name);

	ZeroObj(Class);

	Class.lpfnWndProc = (WNDPROC) Redir;
	Class.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	Class.cbWndExtra = GWL_EXTRA_BYTES;
	Class.cbSize = sizeof(Class);

	ParentProc = 0;
}

LWindowsClass::~LWindowsClass()
{
	UnregisterClassW(NameW(), LProcessInst());
	Class.lpszClassName = NULL;
}

LWindowsClass *LWindowsClass::Create(const char *ClassName)
{
	if (!LAppInst)
		return NULL;

	LApp::ClassContainer *Classes = LAppInst->GetClasses();
	if (!Classes)
		return NULL;

	LWindowsClass *c = Classes->Find(ClassName);
	if (!c)
	{
		c = new LWindowsClass(ClassName);
		if (c)
			Classes->Add(ClassName, c);
	}

	return c;
}

bool LWindowsClass::IsSystem(const char *Cls)
{
	if (!_stricmp(Cls, WC_BUTTONA) ||
		!_stricmp(Cls, WC_COMBOBOXA) ||
		!_stricmp(Cls, WC_STATICA)||
		!_stricmp(Cls, WC_LISTBOXA)||
		!_stricmp(Cls, WC_SCROLLBARA)||
		!_stricmp(Cls, WC_HEADERA)||
		!_stricmp(Cls, WC_LISTVIEWA)||
		!_stricmp(Cls, WC_TREEVIEWA)||
		!_stricmp(Cls, WC_COMBOBOXEXA)||
		!_stricmp(Cls, WC_TABCONTROLA)||
		!_stricmp(Cls, WC_IPADDRESSA)||
		!_stricmp(Cls, WC_EDITA))
	{
		return true;
	}
	return false;
}

bool LWindowsClass::Register()
{
	bool Status = false;
	
	if (IsSystem(Name()))
	{
		ZeroObj(Class);
		Class.cbSize = sizeof(Class);
		Status = GetClassInfoExW(LProcessInst(), NameW(), &Class) != 0;
		LAssert(Status);
	}
	else // if (!Class.lpszClassName)
	{
		Class.hInstance = LProcessInst();
		if (!Class.lpszClassName)
			Class.lpszClassName = NameW();
		Status = RegisterClassExW(&Class) != 0;
		if (!Status)
		{
			auto err = GetLastError();
			if (err == 1410)
				Status = true;
			else
				LAssert(Status);
		}
	}

	return Status;
}

bool LWindowsClass::SubClass(char *Parent)
{
	bool Status = false;

	if (!Class.lpszClassName)
	{
		HBRUSH hBr = Class.hbrBackground;
		LAutoWString p(Utf8ToWide(Parent));
		if (p)
		{
			if (GetClassInfoExW(LProcessInst(), p, &Class))
			{
				ParentProc = Class.lpfnWndProc;
				if (hBr)
				{
					Class.hbrBackground = hBr;
				}

				Class.cbWndExtra = max(Class.cbWndExtra, GWL_EXTRA_BYTES);
				Class.hInstance = LProcessInst();
				Class.lpfnWndProc = (WNDPROC) SubClassRedir;

				Class.lpszClassName = NameW();
				Status = RegisterClassExW(&Class) != 0;
				LAssert(Status);
			}
		}
	}
	else Status = true;

	return Status;
}

LRESULT CALLBACK LWindowsClass::CallParent(HWND hWnd, UINT m, WPARAM a, LPARAM b)
{
	if (!ParentProc)
		return 0;

	if (IsWindowUnicode(hWnd))
	{
		return CallWindowProcW(ParentProc, hWnd, m, a, b);
	}
	else
	{
		return CallWindowProcA(ParentProc, hWnd, m, a, b);
	}
}

//////////////////////////////////////////////////////////////////////////////
LViewI *LWindowFromHandle(HWND hWnd)
{
	if (hWnd)
	{
		SetLastError(0);
		int32 m = GetWindowLong(hWnd, GWL_LGI_MAGIC);
		#if 0 //def _DEBUG
		DWORD err = GetLastError();
		if (err == 1413)
		{
			TCHAR name[256];
			if (GetClassName(hWnd, name, sizeof(name)))
			{
				WNDCLASSEX cls;
				ZeroObj(cls);
				cls.cbSize = sizeof(WNDCLASSEX);
				if (GetClassInfoEx(LAppInst->GetInstance(), name, &cls))
				{
					if (cls.cbWndExtra >= 8)
					{
						LAssert(!"Really?");
					}
				}
			}
		}
		#endif

		if (m == LGI_GViewMagic)
		{
			return (LViewI*)
				#if _MSC_VER >= _MSC_VER_VS2005
				GetWindowLongPtr(hWnd, GWLP_USERDATA);
				#else
				GetWindowLong(hWnd, GWL_USERDATA);
				#endif
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
const char *LView::GetClass()
{
	return "LView";
}

void LView::_Delete()
{
	if (_View && d->DropTarget)
	{
		RevokeDragDrop(_View);
	}

	#ifdef _DEBUG
	// Sanity check..
	// LArray<LViewI*> HasView;
	for (auto c: Children)
	{
		auto par = c->GetParent();
		bool ok = ((LViewI*)par) == this || par == NULL;
		if (!ok)
			LAssert(!"heirachy error");
	}
	#endif

	// Delete myself out of my parent's list
	if (d->Parent)
	{
		d->Parent->OnChildrenChanged(this, false);
		d->Parent->DelView(this);
		d->Parent = 0;
		d->ParentI = 0;
	}

	// Delete all children
	LViewI *c;
	while (c = Children[0])
	{
		// If it has no parent, remove the pointer from the child list,
		// Because the child isn't going to do it...
		if (c->GetParent() == 0)
			Children.Delete(c);

		// Delete the child view
		DeleteObj(c);
	}

	// Delete the OS representation of myself
	if (_View && IsWindow(_View))
	{
		WndFlags |= GWF_DESTRUCTOR;
		BOOL Status = DestroyWindow(_View);
		LAssert(Status != 0);
	}
	
	// NULL my handles and flags
	_View = 0;
	WndFlags = 0;

	// Remove static references to myself
	if (_Over == this) _Over = 0;
	if (_Capturing == this)
	{
		#if DEBUG_CAPTURE
		LgiTrace("%s:%i - _Capturing %p/%s -> NULL\n",
			_FL, this, GetClass());
		#endif
		_Capturing = 0;
	}

	LWindow *Wnd = GetWindow();
	if (Wnd)
		Wnd->SetFocus(this, LWindow::ViewDelete);

	// this should only exist in an ex-LWindow, due to the way
	// C++ deletes objects it needs to be here.
	DeleteObj(_Lock);
}

void LView::Quit(bool DontDelete)
{
	if (_View)
	{
		if (!DontDelete)
		{
			WndFlags |= GWF_QUIT_WND;
		}

		DestroyWindow(_View);
	}
}

uint32_t LView::GetDlgCode()
{
	return d->WndDlgCode;
}

void LView::SetDlgCode(uint32_t i)
{
	d->WndDlgCode = i;
}

uint32_t LView::GetStyle()
{
	return d->WndStyle;
}

void LView::SetStyle(uint32_t i)
{
	d->WndStyle = i;
}

uint32_t LView::GetExStyle()
{
	return d->WndExStyle;
}

void LView::SetExStyle(uint32_t i)
{
	d->WndExStyle = i;
}

const char *LView::GetClassW32()
{
	return d->WndClass;
}

void LView::SetClassW32(const char *c)
{
	d->WndClass = c;
}

LWindowsClass *LView::CreateClassW32(const char *Class, HICON Icon, int AddStyles)
{
	if (Class)
	{
		SetClassW32(Class);
	}

	if (GetClassW32())
	{
		LWindowsClass *c = LWindowsClass::Create(GetClassW32());
		if (c)
		{
			if (Icon)
			{
				c->Class.hIcon = Icon;
			}

			if (AddStyles)
			{
				c->Class.style |= AddStyles;
			}

			c->Register();
			return c;
		}
	}

	return 0;
}

bool LView::IsAttached()
{
	return _View && IsWindow(_View);
}

bool LView::Attach(LViewI *p)
{
	bool Status = false;

	SetParent(p);
	LView *Parent = d->GetParent();
	auto IsWnd = dynamic_cast<LWindow*>(this);
	if (Parent && !_Window)
		_Window = Parent->_Window;

    const char *ClsName = GetClassW32();
	if (!ClsName)
        ClsName = GetClass();
        
	if (ClsName)
	{
		// Real window with HWND
		bool Enab = Enabled();

        // Check the class is created
		bool IsSystemClass = LWindowsClass::IsSystem(ClsName);
        LWindowsClass *Cls = LWindowsClass::Create(ClsName);
        if (Cls)
		{
            auto r = Cls->Register();
			if (!r)
			{
				LAssert(0);
			}
		}
        else if (!IsSystemClass)
            return false;

		if (!IsWnd)
			LAssert(!Parent || Parent->Handle() != 0);

		DWORD Style	  = GetStyle();
		DWORD ExStyle = GetExStyle() & ~WS_EX_CONTROLPARENT;
		if (!TestFlag(WndFlags, GWF_SYS_BORDER))
			ExStyle &= ~(WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE);
							
		auto Text = LBase::NameW();
		LAutoWString WCls(Utf8ToWide(ClsName));
		bool hasCaption = (GetStyle() & WS_CAPTION) != 0;
		int Shadow = WINDOWS_SHADOW_AMOUNT;

		/*
		LgiTrace("%p/%s::Attach %s\n", this, Name(), Pos.GetStr());
		LRect r = Pos;
		r.x2 += Shadow;
		r.y2 += Shadow;
		LgiTrace("    %s\n", r.GetStr());
		*/

		_View = CreateWindowExW(ExStyle,
								WCls,
								Text,
								Style,
								
								Pos.x1,
								Pos.y1,
								Pos.X(),
								Pos.Y(),
								
								Parent ? Parent->Handle() : 0,
								NULL,
								LProcessInst(),
								(LViewI*) this);

		#ifdef _DEBUG
		if (!_View)
		{
			DWORD e = GetLastError();
			LgiTrace("%s:%i - CreateWindowExW failed with 0x%x\n", _FL, e);
			LAssert(!"CreateWindowEx failed");
		}
		#endif

		if (_View)
		{
			Status = (_View != NULL);

			if (d->Font)
				SendMessage(_View, WM_SETFONT, (WPARAM) d->Font->Handle(), 0);

			if (d->DropTarget)
				RegisterDragDrop(_View, d->DropTarget);
			
			if (TestFlag(WndFlags, GWF_FOCUS))
				SetFocus(_View);

			if (d->WantsPulse > 0)
			{
				SetPulse(d->WantsPulse);
				d->WantsPulse = -1;
			}
		}

		OnAttach();

	}
	else
	{
		// Virtual window (no HWND)
		Status = true;
	}

	if (Status && d->Parent)
	{
		auto isWnd = dynamic_cast<LWindow*>(this);
		if (!isWnd)
		{
			if (!d->Parent->HasView(this))
				d->Parent->AddView(this);
			d->Parent->OnChildrenChanged(this, true);
		}
	}

	return Status;
}

bool LView::Detach()
{
	bool Status = false;

	LAssert(InThread());

	if (_Window)
	{
		LWindow *Wnd = dynamic_cast<LWindow*>(_Window);
		if (Wnd)
			Wnd->SetFocus(this, LWindow::ViewDelete);
		_Window = NULL;
	}
	if (d->Parent)
	{
		d->Parent->DelView(this);
		d->Parent->OnChildrenChanged(this, false);
		d->Parent = 0;
		d->ParentI = 0;
		Status = true;
		WndFlags &= ~GWF_FOCUS;

		if (_Capturing == this)
		{
			if (_View)
				ReleaseCapture();
			#if DEBUG_CAPTURE
			LgiTrace("%s:%i - _Capturing %p/%s -> NULL\n",
				_FL, this, GetClass());
			#endif
			_Capturing = 0;
		}
		if (_View)
		{
			WndFlags &= ~GWF_QUIT_WND;
			BOOL Status = DestroyWindow(_View);
			DWORD Err = GetLastError();
			LAssert(Status != 0);
		}
	}
	return Status;
}

LRect &LView::GetClient(bool InClientSpace)
{
	static LRect Client;

	if (_View)
	{
		RECT rc;
		GetClientRect(_View, &rc);
		Client = rc;
	}
	else
	{
		Client.Set(0, 0, Pos.X()-1, Pos.Y()-1);

		if (dynamic_cast<LWindow*>(this) ||
			dynamic_cast<LDialog*>(this))
		{
			Client.x1 += GetSystemMetrics(SM_CXFRAME);
			Client.x2 -= GetSystemMetrics(SM_CXFRAME);
			
			Client.y1 += GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);
			Client.y2 -= GetSystemMetrics(SM_CYFRAME);
		}
		else if (Sunken() || Raised())
		{
			Client.Inset(_BorderSize, _BorderSize);
		}
	}

	if (InClientSpace)
		Client.Offset(-Client.x1, -Client.y1);

	return Client;
}

LCursor LView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

#ifndef GCL_HCURSOR
#define GCL_HCURSOR -12
#endif

bool LgiToWindowsCursor(OsView Hnd, LCursor Cursor)
{
	char16 *Set = 0;
	switch (Cursor)
	{
		case LCUR_UpArrow:
			Set = IDC_UPARROW;
			break;
		case LCUR_Cross:
			Set = IDC_CROSS;
			break;
		case LCUR_Wait:
			Set = IDC_WAIT;
			break;
		case LCUR_Ibeam:
			Set = IDC_IBEAM;
			break;
		case LCUR_SizeVer:
			Set = IDC_SIZENS;
			break;
		case LCUR_SizeHor:
			Set = IDC_SIZEWE;
			break;
		case LCUR_SizeBDiag:
			Set = IDC_SIZENESW;
			break;
		case LCUR_SizeFDiag:
			Set = IDC_SIZENWSE;
			break;
		case LCUR_SizeAll:
			Set = IDC_SIZEALL;
			break;
		case LCUR_PointingHand:
		{
			LArray<int> Ver;
			int Os = LGetOs(&Ver);
			if
			(
				(
					Os == LGI_OS_WIN32
					||
					Os == LGI_OS_WIN64
				)
				&&
				Ver[0] >= 5)
			{
				#ifndef IDC_HAND
				#define IDC_HAND MAKEINTRESOURCE(32649)
				#endif
				Set = IDC_HAND;
			}
			// else not supported
			break;
		}
		case LCUR_Forbidden:
			Set = IDC_NO;
			break;

		// Not impl
		case LCUR_SplitV:
			break;
		case LCUR_SplitH:
			break;
		case LCUR_Blank:
			break;
	}

	HCURSOR cur = LoadCursor(0, Set ? Set : IDC_ARROW);
	SetCursor(cur);
	if (Hnd)
		SetWindowLongPtr(Hnd, GCL_HCURSOR, (LONG_PTR)cur);

	return true;
}

bool LView::PointToScreen(LPoint &p)
{
	POINT pt = {p.x, p.y};
	LViewI *t = this;
	while (	t &&
			t->GetParent() &&
			!t->Handle())
	{
		pt.x += t->GetPos().x1;
		pt.y += t->GetPos().y1;
		t = t->GetParent();
	}
	ClientToScreen(t->Handle(), &pt);
	p.x = pt.x;
	p.y = pt.y;
	return true;
}

bool LView::PointToView(LPoint &p)
{
	POINT pt = {p.x, p.y};
	LViewI *t = this;
	while (	t &&
			t->GetParent() &&
			!t->Handle())
	{
		pt.x -= t->GetPos().x1;
		pt.y -= t->GetPos().y1;
		t = t->GetParent();
	}
	ScreenToClient(t->Handle(), &pt);
	p.x = pt.x;
	p.y = pt.y;
	return true;
}

bool LView::GetMouse(LMouse &m, bool ScreenCoords)
{
	// position
	POINT p;
	GetCursorPos(&p);
	if (!ScreenCoords)
	{
		ScreenToClient(_View, &p);
	}
	m.x = p.x;
	m.y = p.y;
	m.Target = this;

	// buttons
	m.Flags =	((GetAsyncKeyState(VK_LBUTTON)&0x8000) ? LGI_EF_LEFT   : 0) |
				((GetAsyncKeyState(VK_MBUTTON)&0x8000) ? LGI_EF_MIDDLE : 0) |
				((GetAsyncKeyState(VK_RBUTTON)&0x8000) ? LGI_EF_RIGHT  : 0) |
				((GetAsyncKeyState(VK_CONTROL)&0x8000) ? LGI_EF_CTRL   : 0) |
				((GetAsyncKeyState(VK_MENU)   &0x8000) ? LGI_EF_ALT    : 0) |
				((GetAsyncKeyState(VK_LWIN)   &0x8000) ? LGI_EF_SYSTEM : 0) |
				((GetAsyncKeyState(VK_RWIN)   &0x8000) ? LGI_EF_SYSTEM : 0) |
				((GetAsyncKeyState(VK_SHIFT)  &0x8000) ? LGI_EF_SHIFT  : 0);

	if (m.Flags & (LGI_EF_LEFT | LGI_EF_MIDDLE | LGI_EF_RIGHT))
	{
		m.Flags |= LGI_EF_DOWN;
	}

	return true;
}

bool LView::SetPos(LRect &p, bool Repaint)
{
	bool Status = true;
	LRect OldPos = Pos;

	if (Pos != p)
	{
		Pos = p;
		if (_View)
		{
			HWND hOld = GetFocus();
			bool WasVis = IsWindowVisible(_View) != 0;
			int Shadow = WINDOWS_SHADOW_AMOUNT;

			In_SetWindowPos = true;
			Status = SetWindowPos(	_View,
									NULL,
									Pos.x1,
									Pos.y1,
									Pos.X() + Shadow,
									Pos.Y() + Shadow,
									// ((Repaint) ? 0 : SWP_NOREDRAW) |
									SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER) != 0;
			In_SetWindowPos = false;
		}
		else if (GetParent())
		{
			OnPosChange();
		}
		
		if (Repaint)
		{
			Invalidate();
		}
	}

	return Status;
}

bool LView::Invalidate(LRect *r, bool Repaint, bool Frame)
{
	if (_View)
	{
		bool Status = false;

		if (Frame)
		{
			RedrawWindow(	_View,
							NULL,
							NULL,
							RDW_FRAME |
							RDW_INVALIDATE |
							RDW_ALLCHILDREN |
							((Repaint) ? RDW_UPDATENOW : 0));
		}
		else
		{
			if (r)
			{
				Status = InvalidateRect(_View, &((RECT)*r), false) != 0;
			}
			else
			{
				RECT c = GetClient();
				Status = InvalidateRect(_View, &c, false) != 0;
			}
		}

		if (Repaint)
		{
			UpdateWindow(_View);
		}

		return Status;
	}
	else
	{
		LRect Up;
		LViewI *p = this;

		if (r)
		{
			Up = *r;
		}
		else
		{
			Up.Set(0, 0, Pos.X()-1, Pos.Y()-1);
		}

		if (dynamic_cast<LWindow*>(this))
			return true;

		while (p && !p->Handle())
		{
			LViewI *Par = p->GetParent();
			LView *VPar = Par?Par->GetLView():0;
			LRect w = p->GetPos();
			LRect c = p->GetClient(false);
			if (Frame && p == this)
				Up.Offset(w.x1, w.y1);
			else				
				Up.Offset(w.x1 + c.x1, w.y1 + c.y1);
			p = Par;
		}

		if (p && p->Handle())
		{
			return p->Invalidate(&Up, Repaint);
		}
	}

	return false;
}

void
CALLBACK
LView::TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, uint32_t dwTime)
{
	auto View = (LView*) idEvent;
	if (View)
	{
		View->OnPulse();

		for (auto t: View->d->EventTargets)
			t->OnPulse();
	}
}

void LView::SetPulse(int Length)
{
	if (_View)
	{
		if (Length > 0)
		{
            d->TimerId = SetTimer(_View, (UINT_PTR) this, Length, (TIMERPROC) TimerProc);
		}
		else
		{
			KillTimer(_View, d->TimerId);
			d->TimerId = 0;
		}
	}
	else
	{
		d->WantsPulse = Length;
	}
}

static int ConsumeTabKey = 0;

bool SysOnKey(LView *w, LMessage *m)
{
	if (m->a == VK_TAB &&
		(m->m == WM_KEYDOWN ||
		m->m == WM_SYSKEYDOWN) )
	{
		if (!TestFlag(w->d->WndDlgCode, DLGC_WANTTAB) &&
			!TestFlag(w->d->WndDlgCode, DLGC_WANTALLKEYS))
		{
			// push the focus to the next control
			bool Shifted = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			LViewI *Wnd = GetNextTabStop(w, Shifted);
			if (Wnd)
			{
				if (In_SetWindowPos)
				{
					assert(0);
					LgiTrace("%s:%i - SetFocus(%p)\\n", _FL, Wnd->Handle());
				}

				ConsumeTabKey = 2;
				::SetFocus(Wnd->Handle());
				return true;
			}
		}
	}

	return false;
}

#ifdef _MSC_VER
#include "vsstyle.h"

void LView::DrawThemeBorder(LSurface *pDC, LRect &r)
{
	if (!d->hTheme)
		d->hTheme = OpenThemeData(_View, VSCLASS_EDIT);

	if (d->hTheme)
	{
		RECT rc = r;
		
		int StateId;
		if (!Enabled())
			StateId = EPSN_DISABLED;
		else if (GetFocus() == _View)
			StateId = EPSN_FOCUSED;
		else
			StateId = EPSN_NORMAL;
		
		// LgiTrace("ThemeDraw %s: %i\n", GetClass(), StateId);
		
		RECT clip[4];
		clip[0] = LRect(r.x1, r.y1, r.x1 + 1, r.y2); // left
		clip[1] = LRect(r.x1 + 2, r.y1, r.x2 - 2, r.y1 + 1); // top
		clip[2] = LRect(r.x2 - 1, r.y1, r.x2, r.y2);  // right
		clip[3] = LRect(r.x1 + 2, r.y2 - 1, r.x2 - 2, r.y2); // bottom
		
		LColour cols[4] = 
		{
			LColour(255, 0, 0),
			LColour(0, 255, 0),
			LColour(0, 0, 255),
			LColour(255, 255, 0)
		};
		
		for (int i=0; i<CountOf(clip); i++)
		{
			#if 0
			LRect tmp = clip[i];
			pDC->Colour(cols[i]);
			pDC->Rectangle(&tmp);
			#else
			DrawThemeBackground(d->hTheme,
								pDC->Handle(),
								EP_EDITBORDER_NOSCROLL,
								StateId,
								&rc,
								&clip[i]);
			#endif
		}
		
		pDC->Colour(L_MED);
		pDC->Set(r.x1, r.y1);
		pDC->Set(r.x2, r.y1);
		pDC->Set(r.x1, r.y2);
		pDC->Set(r.x2, r.y2);
		
		r.Inset(2, 2);
	}
	else
	{
		LWideBorder(pDC, r, Sunken() ? DefaultSunkenEdge : DefaultRaisedEdge);
		d->IsThemed = false;
	}
}
#else
void LView::DrawThemeBorder(LSurface *pDC, LRect &r)
{
	LWideBorder(pDC, r, DefaultSunkenEdge);
}
#endif

bool IsKeyChar(LKey &k, int vk)
{
	if (k.Ctrl() || k.Alt() || k.System())
		return false;

	switch (vk)
	{
		case VK_BACK:
		case VK_TAB:
		case VK_RETURN:
		case VK_SPACE:

		case 0xba: // ;
		case 0xbb: // =
		case 0xbc: // ,
		case 0xbd: // -
		case 0xbe: // .
		case 0xbf: // /

		case 0xc0: // `

		case 0xdb: // [
		case 0xdc: // |
		case 0xdd: // ]
		case 0xde: // '
			return true;
	}

	if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE)
		return true;

	if (vk >= '0' && vk <= '9')
		return true;

	if (vk >= 'A' && vk <= 'Z')
		return true;

	return false;
}

#define KEY_FLAGS		(~(MK_LBUTTON | MK_MBUTTON | MK_RBUTTON))

LMessage::Result LView::OnEvent(LMessage *Msg)
{
	int Status = 0;

	if (Msg->Msg() == MouseRollMsg)
	{
		HWND hFocus = GetFocus();
		if (_View)
		{
			int Flags = ((GetKeyState(VK_SHIFT)&0xF000) ? VK_SHIFT : 0) | 
						((GetKeyState(VK_CONTROL)&0xF000) ? VK_CONTROL : 0);
			
			PostMessage(hFocus, WM_MOUSEWHEEL, MAKELONG(Flags, (short)Msg->a), Msg->b);
		}
		return 0;
	}

	for (auto target: d->EventTargets)
	{
		if (target->Msgs.Length() == 0 ||
			target->Msgs.Find(Msg->Msg()))
			target->OnEvent(Msg);
	}

	LMessage::Result result;
	if (CommonEvents(result, Msg))
		return result;

	if (_View)
	{
		switch (Msg->m)
		{
			#if 1
			case WM_CTLCOLORBTN:
			case WM_CTLCOLOREDIT:
			case WM_CTLCOLORSTATIC:
			{
				HDC hdc = (HDC)Msg->A();
				HWND hwnd = (HWND)Msg->B();

				LViewI *v = FindControl(hwnd);
				LView *gv = v ? v->GetLView() : NULL;
				if (gv)
				{
					int Depth = dynamic_cast<LEdit*>(gv) ? 1 : 10;
					LColour Fore = gv->StyleColour(LCss::PropColor, LColour(), Depth);
					LColour Back = gv->StyleColour(LCss::PropBackgroundColor, LColour(), Depth);
						
					if (Fore.IsValid())
					{
						COLORREF c = RGB(Fore.r(), Fore.g(), Fore.b());
						SetTextColor(hdc, c);
					}							
					if (Back.IsValid())
					{
						COLORREF c = RGB(Back.r(), Back.g(), Back.b());
						SetBkColor(hdc, c);
						SetDCBrushColor(hdc, c);
					}

					if (Fore.IsValid() || Back.IsValid())
					{
						#if !defined(DC_BRUSH)
						#define DC_BRUSH            18
						#endif
						return (LRESULT) GetStockObject(DC_BRUSH);
					}
				}

				goto ReturnDefaultProc;
				return 0;
			}
			#endif
			case 5700:
			{
				// I forget what this is for...
				break;
			}
			case WM_ERASEBKGND:
			{
				return 1;
			}
			case WM_GETFONT:
			{
				LFont *f = GetFont();
				if (!f || f == LSysFont)
					return (LMessage::Result) LSysFont->Handle();

				return (LMessage::Result) f->Handle();
				break;
			}
			case WM_MENUCHAR:
			case WM_MEASUREITEM:
			{
				return LMenu::_OnEvent(Msg);
				break;
			}
			case WM_DRAWITEM:
			{
				DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)Msg->B();
				if (di)
				{
					if (di->CtlType == ODT_MENU)
					{
						return LMenu::_OnEvent(Msg);
					}
					/*
					else if (di->CtlType == ODT_BUTTON)
					{
						LView *b;
						if (CastHwnd(b, di->hwndItem) &&
							b->GetCss())
						{
							LScreenDC dc(di->hDC, di->hwndItem);
							switch (di->itemAction)
							{
								case ODA_DRAWENTIRE:
								{
									LRect c = di->rcItem;
									LMemDC m(c.X(), c.Y(), GdcD->GetColourSpace());
									HDC hdc = m.StartDC();
									m.Colour(LColour(255, 0, 255));
									m.Line(0, 0, m.X()-1, m.Y()-1);

									LONG s = GetWindowLong(_View, GWL_STYLE);
									SetWindowLong(_View, GWL_STYLE, (s & ~BS_TYPEMASK) | BS_PUSHBUTTON);

									SendMessage(_View, WM_PRINT, (WPARAM)hdc, PRF_ERASEBKGND|PRF_CLIENT);

									SetWindowLong(_View, GWL_STYLE, (s & ~BS_TYPEMASK) | BS_OWNERDRAW);

									m.EndDC();
									dc.Blt(0, 0, &m);									
									break;
								}
								case ODA_FOCUS:
								{
									break;
								}
								case ODA_SELECT:
								{
									break;
								}
							}
							return true;
						}
					}
					*/
				}

				if (!(WndFlags & GWF_DIALOG))
					goto ReturnDefaultProc;
				break;
			}
			case WM_ENABLE:
			{
				Invalidate(&Pos);
				break;
			}
			case WM_HSCROLL:
			case WM_VSCROLL:
			{
				LViewI *Wnd = FindControl((HWND) Msg->b);
				if (Wnd)
				{
					Wnd->OnEvent(Msg);
				}
				break;
			}
			case WM_GETDLGCODE:
			{
				// we handle all tab control stuff
				return DLGC_WANTALLKEYS; // d->WndDlgCode | DLGC_WANTTAB;
			}
			case WM_MOUSEWHEEL:
			{
				// short fwKeys = LOWORD(Msg->a);			// key flags
				short zDelta = (short) HIWORD(Msg->a);	// wheel rotation
				int nScrollLines = - _lgi_mouse_wheel_lines();
				double Lines = ((double)zDelta * (double)nScrollLines) / WHEEL_DELTA;
				if (ABS(Lines) < 1.0)
					Lines *= 1.0 / ABS(Lines);
				
				// LgiTrace("Lines = %g, zDelta = %i, nScrollLines = %i\n", Lines, zDelta, nScrollLines);

				// Try giving the event to the current window...
				if (!OnMouseWheel(Lines))
				{
					// Find the window under the cursor... and try giving it the mouse wheel event
					short xPos = (short) LOWORD(Msg->b);	// horizontal position of pointer
					short yPos = (short) HIWORD(Msg->b);	// vertical position of pointer
					POINT Point = {xPos, yPos};
					HWND hUnder = ::WindowFromPoint(Point);
					HWND hParent = ::GetParent(hUnder);
					if (hUnder &&
						hUnder != _View &&	// Don't want to send ourselves a message...
						hParent != _View)	// WM_MOUSEWHEEL will propagate back up to us and cause an infinite loop
					{
						// Do a post event in case the window is deleting... at least it won't crash.
						PostMessage(hUnder, Msg->m, Msg->a, Msg->b);
					}
				}
				return 0;
			}
			case M_CHANGE:
			{
				LWindow *w = GetWindow();
				LAutoPtr<LNotification> note((LNotification*)Msg->B());
				LViewI *Ctrl = w ? w->FindControl((int)Msg->a) : 0;
				if (Ctrl)
				{
					LAssert(note.Get() != NULL);
					return OnNotify(Ctrl, note ? *note : LNotifyNull);
				}
				else
				{
					LgiTrace("Ctrl %i not found.\n", Msg->a);
				}
				break;
			}
			case M_COMMAND:
			{
				// LViewI *Ci = FindControl((HWND) Msg->b);
				// LView *Ctrl = Ci ? Ci->GetLView() : 0;
				LView *Ctrl;
				if (Msg->b &&
					CastHwnd(Ctrl, (HWND)Msg->b))
				{
					short Code = HIWORD(Msg->a);
					switch (Code)
					{
						case CBN_CLOSEUP:
						{
							PostMessage(_View, WM_COMMAND, MAKELONG(Ctrl->GetId(), CBN_EDITCHANGE), Msg->b);
							break;
						}
						case CBN_EDITCHANGE: // COMBO
						{
							Ctrl->SysOnNotify(Msg->Msg(), Code);
							OnNotify(Ctrl, LNotifyValueChanged);
							break;
						}
						/*
						case BN_CLICKED: // BUTTON
						case EN_CHANGE: // EDIT
						*/
						default:
						{
							Ctrl->SysOnNotify(Msg->Msg(), Code);
							break;
						}
					}
				}
				break;
			}
			case WM_NCDESTROY:
			{
                #if _MSC_VER >= _MSC_VER_VS2005
                SetWindowLongPtr(_View, GWLP_USERDATA, 0);
                #else
				SetWindowLong(_View, GWL_USERDATA, 0);
				#endif
				_View = NULL;
				if (WndFlags & GWF_QUIT_WND)
				{
					delete this;
				}
				break;
			}
	 		case WM_CLOSE:
			{
				if (OnRequestClose(false))
				{
					Quit();
				}
				break;
			}
			case WM_DESTROY:
			{
				OnDestroy();
				break;
			}
			case WM_CREATE:
			{
				SetId(d->CtrlId);

				LWindow *w = GetWindow();
				if (w && w->GetFocus() == this)
				{
					HWND hCur = GetFocus();
					if (hCur != _View)
					{
						if (In_SetWindowPos)
						{
							assert(0);
							LgiTrace("%s:%i - SetFocus(%p) (%s)\\n", __FILE__, __LINE__, Handle(), GetClass());
						}

						SetFocus(_View);
					}
				}

				if (TestFlag(GViewFlags, GWF_DROP_TARGET))
				{
					DropTarget(true);
				}

				OnCreate();
				break;
			}
			case WM_SETFOCUS:
			{
				LWindow *w = GetWindow();
				if (w)
				{
					w->SetFocus(this, LWindow::GainFocus);
				}
				else
				{
					// This can happen in popup sub-trees of views. Where the focus
					// is tracked separately from the main LWindow.
					OnFocus(true);
					Invalidate((LRect*)NULL, false, true);
				}
				break;
			}
			case WM_KILLFOCUS:
			{
				LWindow *w = GetWindow();
				if (w)
				{
					w->SetFocus(this, LWindow::LoseFocus);
				}
				else
				{
					// This can happen when the LWindow is being destroyed
					Invalidate((LRect*)NULL, false, true);
					OnFocus(false);
				}
				break;
			}
			case WM_WINDOWPOSCHANGED:
			{
				if (!IsIconic(_View))
				{
					WINDOWPOS *Info = (LPWINDOWPOS) Msg->b;
					if (Info)
					{
						if (Info->x == -32000 &&
							Info->y == -32000)
						{
							#if 0
							LgiTrace("WM_WINDOWPOSCHANGED %i,%i,%i,%i (icon=%i)\\n",
								Info->x,
								Info->y,
								Info->cx,
								Info->cy,
								IsIconic(Handle()));
							#endif
						}
						else
						{
							int Shadow = WINDOWS_SHADOW_AMOUNT;
	
							LRect r;
							r.ZOff(Info->cx-1, Info->cy-1);
							r.Offset(Info->x, Info->y);
							if (r.Valid() && r != Pos)
							{
								Pos = r;
								/*
								LgiTrace("%p/%s::PosChange %s\n", this, Name(), Pos.GetStr());
								Pos.x2 -= Shadow;
								Pos.y2 -= Shadow;
								LgiTrace("    %s\n", Pos.GetStr());
								*/
							}
						}
					}

					OnPosChange();
				}

				if (!(WndFlags & GWF_DIALOG))
				{
					goto ReturnDefaultProc;
				}
				break;
			}
			case WM_CAPTURECHANGED:
			{
				LViewI *Wnd;
				if (Msg->B() &&
					CastHwnd(Wnd, (HWND)Msg->B()))
				{
					if (Wnd != _Capturing)
					{
						#if DEBUG_CAPTURE
						LgiTrace("%s:%i - _Capturing %p/%s -> %p/%s\n",
							_FL,
							_Capturing, _Capturing?_Capturing->GetClass():0,
							Wnd, Wnd?Wnd->GetClass() : 0);
						#endif
						_Capturing = Wnd;
					}
				}
				else if (_Capturing)
				{
					#if DEBUG_CAPTURE
					LgiTrace("%s:%i - _Capturing %p/%s -> NULL\n",
						_FL,
						_Capturing, _Capturing?_Capturing->GetClass():0);
					#endif
					_Capturing = NULL;
				}
				break;
			}
			case M_MOUSEENTER:
			{
				LMouse Ms;
				Ms.Target = this;
				Ms.x = (short) (Msg->b&0xFFFF);
				Ms.y = (short) (Msg->b>>16);
				Ms.Flags = 0;

				LViewI *MouseOver = WindowFromPoint(Ms.x, Ms.y);
				if (MouseOver &&
					_Over != MouseOver &&
					!(MouseOver == this || MouseOver->Handle() == 0))
				{
					if (_Capturing)
					{
						if (MouseOver == _Capturing)
						{
							Ms = lgi_adjust_click(Ms, _Capturing);
							_Capturing->OnMouseEnter(Ms);
						}
					}
					else
					{
						if (_Over)
						{
							LMouse m = lgi_adjust_click(Ms, _Over);
							_Over->OnMouseExit(m);

							#if DEBUG_OVER
							LgiTrace("Enter.LoseOver=%p/%s '%-20s'\n", _Over, _Over->GetClass(), _Over->Name());
							#endif
						}

						_Over = MouseOver;

						if (_Over)
						{
							#if DEBUG_OVER
							LgiTrace("Enter.GetOver=%p/%s '%-20s'\n", _Over, _Over->GetClass(), _Over->Name());
							#endif

							LMouse m = lgi_adjust_click(Ms, _Over);
							_Over->OnMouseEnter(m);
						}
					}
				}
				break;
			}
			case M_MOUSEEXIT:
			{
				if (_Over)
				{
					LMouse Ms;
					Ms.Target = this;
					Ms.x = (short) (Msg->b&0xFFFF);
					Ms.y = (short) (Msg->b>>16);
					Ms.Flags = 0;

					bool Mine = false;
					if (_Over->Handle())
					{
						Mine = _Over == this;
					}
					else
					{
						for (LViewI *o = _Capturing ? _Capturing : _Over; o; o = o->GetParent())
						{
							if (o == this)
							{
								Mine = true;
								break;
							}
						}
					}

					if (Mine)
					{
						if (_Capturing)
						{
							LMouse m = lgi_adjust_click(Ms, _Capturing);
							_Capturing->OnMouseExit(m);
						}
						else
						{
							#if DEBUG_OVER
							LgiTrace("Exit.LoseOver=%p '%-20s'\n", _Over, _Over->Name());
							#endif

							_Over->OnMouseExit(Ms);
							_Over = 0;
						}
					}
				}
				break;
			}
			case WM_MOUSEMOVE:
			{
				LMouse Ms;
				Ms.Target = this;
				Ms.x = (short) (Msg->b&0xFFFF);
				Ms.y = (short) (Msg->b>>16);
				Ms.Flags = _lgi_get_key_flags();
				Ms.IsMove(true);
				if (TestFlag(Msg->a, MK_LBUTTON)) SetFlag(Ms.Flags, LGI_EF_LEFT);
				if (TestFlag(Msg->a, MK_RBUTTON)) SetFlag(Ms.Flags, LGI_EF_RIGHT);
				if (TestFlag(Msg->a, MK_MBUTTON)) SetFlag(Ms.Flags, LGI_EF_MIDDLE);
				
				SetKeyFlag(Ms.Flags, VK_MENU, MK_ALT);
				Ms.Down((Msg->a & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON)) != 0);

				LViewI *MouseOver = WindowFromPoint(Ms.x, Ms.y);
				if (_Over != MouseOver)
				{
					if (_Over)
					{
						#if DEBUG_OVER
						LgiTrace("Move.LoseOver=%p/%s '%-20s'\n", _Over, _Over->GetClass(), _Over->Name());
						#endif

						LMouse m = lgi_adjust_click(Ms, _Over);
						_Over->OnMouseExit(m);
					}

					_Over = MouseOver;

					if (_Over)
					{
						LMouse m = lgi_adjust_click(Ms, _Over);
						_Over->OnMouseEnter(m);

						#if DEBUG_OVER
						LgiTrace("Move.GetOver=%p/%s '%-20s'\n", _Over, _Over->GetClass(), _Over->Name());
						#endif
					}
				}

				// int CurX = Ms.x, CurY = Ms.y;
				LCursor Cursor = (_Over ? _Over : this)->GetCursor(Ms.x, Ms.y);
				LgiToWindowsCursor(_View, Cursor);

				#if 0
				LgiTrace("WM_MOUSEMOVE %i,%i target=%p/%s, over=%p/%s, cap=%p/%s\n",
					Ms.x, Ms.y,
					Ms.Target, Ms.Target?Ms.Target->GetClass():0,
					_Over, _Over?_Over->GetClass():0,
					_Capturing, _Capturing?_Capturing->GetClass():0);
				#endif

				if (_Capturing)
					Ms = lgi_adjust_click(Ms, _Capturing, true);
				else if (_Over)
					Ms = lgi_adjust_click(Ms, _Over);
				else
					return 0;

				LWindow *Wnd = GetWindow();
				if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<LView*>(Ms.Target), Ms))
				{
					Ms.Target->OnMouseMove(Ms);
				}
				break;
			}
			case WM_NCHITTEST:
			{
				POINT Pt = { LOWORD(Msg->b), HIWORD(Msg->b) };
				ScreenToClient(_View, &Pt);
				int Hit = OnHitTest(Pt.x, Pt.y);
				if (Hit >= 0)
				{
					// LgiTrace("%I64i Hit=%i\n", LCurrentTime(), Hit);
					return Hit;
				}
				if (!(WndFlags & GWF_DIALOG))
				{
					goto ReturnDefaultProc;
				}
				break;
			}
			case WM_LBUTTONDBLCLK:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			{
				LMouse Ms;
				Ms.x = (short) (Msg->b&0xFFFF);
				Ms.y = (short) (Msg->b>>16);
				Ms.Flags = _lgi_get_key_flags() | LGI_EF_LEFT;
				Ms.Down(Msg->m != WM_LBUTTONUP);
				Ms.Double(Msg->m == WM_LBUTTONDBLCLK);

				if (_Capturing)
					Ms = lgi_adjust_click(Ms, _Capturing, true);
				else if (_Over)
					Ms = lgi_adjust_click(Ms, _Over);
				else
					Ms.Target = this;
				
				#if DEBUG_MOUSE_CLICKS
				LString Msg;
				Msg.Printf("%s.Click", Ms.Target->GetClass());
				Ms.Trace(Msg);
				#endif

				LWindow *Wnd = GetWindow();
				if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<LView*>(Ms.Target), Ms))
					Ms.Target->OnMouseClick(Ms);
				break;
			}
			case WM_RBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			{
				LMouse Ms;
				Ms.x = (short) (Msg->b&0xFFFF);
				Ms.y = (short) (Msg->b>>16);
				Ms.Flags = _lgi_get_key_flags() | LGI_EF_RIGHT;
				Ms.Down(Msg->m != WM_RBUTTONUP);
				Ms.Double(Msg->m == WM_RBUTTONDBLCLK);

				if (_Capturing)
					Ms = lgi_adjust_click(Ms, _Capturing, true);
				else if (_Over)
					Ms = lgi_adjust_click(Ms, _Over);
				else
					Ms.Target = this;

				#if DEBUG_MOUSE_CLICKS
				LString Msg;
				Msg.Printf("%s.Click", Ms.Target->GetClass());
				Ms.Trace(Msg);
				#endif

				LWindow *Wnd = GetWindow();
				if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<LView*>(Ms.Target), Ms))
					Ms.Target->OnMouseClick(Ms);
				break;
			}
			case WM_MBUTTONDBLCLK:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			{
				LMouse Ms;
				Ms.x = (short) (Msg->b&0xFFFF);
				Ms.y = (short) (Msg->b>>16);
				Ms.Flags = _lgi_get_key_flags() | LGI_EF_MIDDLE;
 				Ms.Down(Msg->m != WM_MBUTTONUP);
				Ms.Double(Msg->m == WM_MBUTTONDBLCLK);

				if (_Capturing)
					Ms = lgi_adjust_click(Ms, _Capturing, true);
				else if (_Over)
					Ms = lgi_adjust_click(Ms, _Over);
				else
					Ms.Target = this;

				LWindow *Wnd = GetWindow();
				if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<LView*>(Ms.Target), Ms))
					Ms.Target->OnMouseClick(Ms);
				break;
			}
			case WM_XBUTTONDBLCLK:
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
			{
				LMouse Ms;
				int Clicked = (Msg->a >> 16) & 0xffff;
				Ms.x = (short) (Msg->b&0xFFFF);
				Ms.y = (short) (Msg->b>>16);
				Ms.Flags = _lgi_get_key_flags() | LGI_EF_MIDDLE;
				Ms.Button1(TestFlag(Clicked, XBUTTON1));
				Ms.Button2(TestFlag(Clicked, XBUTTON2));
 				Ms.Down(Msg->m != WM_XBUTTONUP);
				Ms.Double(Msg->m == WM_XBUTTONDBLCLK);

				if (_Capturing)
					Ms = lgi_adjust_click(Ms, _Capturing, true);
				else if (_Over)
					Ms = lgi_adjust_click(Ms, _Over);
				else
					Ms.Target = this;

				LWindow *Wnd = GetWindow();
				if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<LView*>(Ms.Target), Ms))
					Ms.Target->OnMouseClick(Ms);
				break;
			}
			case WM_SYSKEYUP:
			case WM_SYSKEYDOWN:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				static char AltCode[32];
				bool IsDialog = TestFlag(WndFlags, GWF_DIALOG);
				bool IsDown = Msg->m == WM_KEYDOWN || Msg->m == WM_SYSKEYDOWN;
				int KeyFlags = _lgi_get_key_flags();
				HWND hwnd = _View;

				if (SysOnKey(this, Msg))
				{
					// LgiTrace("SysOnKey true, Msg=0x%x %x,%x\n", Msg->m, Msg->a, Msg->b);
					return 0;
				}
				else
				{
					// Key
					LKey Key((int)Msg->a, (int)Msg->b);

					Key.Flags = KeyFlags;
					Key.Down(IsDown);
					Key.IsChar = false;

					if (Key.Ctrl())
					{
						Key.c16 = (char16)Msg->a;
					}

					if (Key.c16 == VK_TAB && ConsumeTabKey)
					{
						ConsumeTabKey--;
					}
					else
					{
						LWindow *Wnd = GetWindow();
						if (Wnd)
						{
							if (Key.Alt() ||
								Key.Ctrl() ||
								(Key.c16 < 'A' || Key.c16 > 'Z'))
							{
								Wnd->HandleViewKey(this, Key);
							}
						}
						else
						{
							OnKey(Key);
						}
					}

					if (Msg->m == WM_SYSKEYUP || Msg->m == WM_SYSKEYDOWN)
					{
						if (Key.vkey >= VK_F1 &&
							Key.vkey <= VK_F12 &&
							Key.Alt() == false)
						{
							// So in LgiIde if you press F10 (debug next) you get a hang
							// sometimes in DefWindowProc. Until I figure out what's going
							// on this code exits before calling DefWindowProc without
							// breaking other WM_SYSKEY* functionality (esp Alt+F4).
							return 0;
						}
					}
				}

				if (!IsDialog)
				{
					// required for Alt-Key function (eg Alt-F4 closes window)
					goto ReturnDefaultProc;
				}
				break;
			}
			#if OLD_WM_CHAR_MODE
			case WM_CHAR:
			{
				LKey Key((int)Msg->a, (int)Msg->b);
				Key.Flags = _lgi_get_key_flags();
				Key.Down(true);
				Key.IsChar = true;

				bool Shift = Key.Shift();
				bool Caps = TestFlag(Key.Flags, LGI_EF_CAPS_LOCK);
				if (!(Shift ^ Caps))
				{
					Key.c16 = ToLower(Key.c16);
				}
				else
				{
					Key.c16 = ToUpper(Key.c16);
				}

				if (Key.c16 == LK_TAB && ConsumeTabKey)
				{
					ConsumeTabKey--;
				}
				else
				{
					LWindow *Wnd = GetWindow();
					if (Wnd)
					{
						Wnd->HandleViewKey(this, Key);
					}
					else
					{
						OnKey(Key);
					}
				}
				break;
			}
			#endif
			case M_SET_WND_STYLE:
			{
				SetWindowLong(Handle(), GWL_STYLE, (LONG)Msg->b);
				SetWindowPos(	Handle(),
								0, 0, 0, 0, 0,
								SWP_NOMOVE | SWP_NOZORDER | SWP_NOSIZE | SWP_FRAMECHANGED);
				break;
			}
			case WM_PAINT:
			{
				_Paint();
				break;
			}
			case WM_NCPAINT:
			{
				if (GetWindow() != this &&
					!TestFlag(WndFlags, GWF_SYS_BORDER))
				{
					HDC hDC = GetWindowDC(_View);
					if (!hDC)
						goto ReturnDefaultProc;
					LScreenDC Dc(hDC, _View, true);
					LRect p(0, 0, Dc.X()-1, Dc.Y()-1);
					OnNcPaint(&Dc, p);
				}

				goto ReturnDefaultProc;
				break;
			}
			case WM_NCCALCSIZE:
			{
				LMessage::Param Status = 0;
				int Edge = (Sunken() || Raised()) ? _BorderSize : 0;
				RECT *rc = NULL;
				if (Msg->a)
				{
					NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS*) Msg->b;
					rc = p->rgrc;
				}
				else
				{
					rc = (RECT*)Msg->b;
				}
				
				if (!(WndFlags & GWF_DIALOG))
				{
					Status = DefWindowProcW(_View, Msg->m, Msg->a, Msg->b);
				}

				if (Edge && rc && !TestFlag(WndFlags, GWF_SYS_BORDER))
				{
					rc->left += Edge;
					rc->top += Edge;
					rc->right -= Edge;
					rc->bottom -= Edge;
					return 0;
				}
				
				return Status;
			}
			case WM_NOTIFY:
			{
				NMHDR *Hdr = (NMHDR*)Msg->B();
				if (Hdr)
				{
					LView *Wnd;
					if (CastHwnd(Wnd, Hdr->hwndFrom))
						Wnd->SysOnNotify(Msg->Msg(), Hdr->code);
				}
				break;
			}
			default:
			{
				if (!(WndFlags & GWF_DIALOG))
					goto ReturnDefaultProc;
				break;
			}
		}
	}

	return 0;

ReturnDefaultProc:
	#if 0 // def _DEBUG
	uint64 start = LCurrentTime();
	#endif
	LRESULT r = DefWindowProcW(_View, Msg->m, Msg->a, Msg->b);
	#if 0 // def _DEBUG
	uint64 now = LCurrentTime();
	if (now - start > 1000)
	{
		LgiTrace("DefWindowProc(0x%.4x, %i, %i) took %ims\n",
			Msg->m, Msg->a, Msg->b, (int)(now - start));
	}
	#endif
	return r;
}

LViewI *LView::FindControl(OsView hCtrl)
{
	if (_View == hCtrl)
	{
		return this;
	}

	for (List<LViewI>::I i = Children.begin(); i.In(); i++)
	{
		LViewI *Ctrl = (*i)->FindControl(hCtrl);
		if (Ctrl)
			return Ctrl;
	}

	return 0;
}
