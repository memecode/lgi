// \file
// \author Matthew Allen
// \brief Native Win32 Button Class
#if !XP_BUTTON

#include <stdlib.h>
#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Button.h"
#include "lgi/common/LgiRes.h"

// Size of extra pixels, beyond the size of the text itself.
LPoint LButton::Overhead =
    LPoint(	16,	// Extra width needed
			8);	// Extra height needed

static int IsWin7 = -1;
static int IsWin10 = -1;

class LButtonPrivate
{
public:
	WNDPROC ButtonClassProc;
	bool Toggle;
	bool WantsDefault;
	int64 Value;
	
	LSurface *Img;
	bool ImgOwned;

	LButtonPrivate()
	{
		Toggle = false;
		WantsDefault = false;
		ButtonClassProc = NULL;
		Value = 0;
		Img = NULL;
		ImgOwned = false;
	}

	~LButtonPrivate()
	{
		if (ImgOwned)
			DeleteObj(Img);
	}
};

LButton::LButton(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Button)
{
	d = new LButtonPrivate;
	Name(name);
	
	if ((cx < 0 || cy < 0))
	{
		LDisplayString ds(SysFont, ValidStr(name)?name:"1");
		if (cx < 0) cx = ds.X() + Overhead.x;
		if (cy < 0) cy = ds.Y() + Overhead.y;
	}
	
	LRect r(x, y, x+cx-1, y+cy-1);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	
	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = LWindowsClass::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LgiAssert(!"No subclass?");
}

LButton::~LButton()
{
	LWindow *w = GetWindow();
	if (w && w->GetDefault() == (LViewI*)this)
	{
		w->SetDefault(NULL);
	}
	
	DeleteObj(d);
}

bool LButton::GetIsToggle()
{
	return d->Toggle;
}

void LButton::SetIsToggle(bool toggle)
{
	d->Toggle = toggle;
	
	int Tog = BS_PUSHLIKE | BS_AUTOCHECKBOX;
	if (_View)
	{
		DWORD Style = (DWORD)GetWindowLong(_View, GWL_STYLE);
		if (toggle)
			SetWindowLong(_View, GWL_STYLE, Style | Tog);
		else
			SetWindowLong(_View, GWL_STYLE, Style & ~Tog);
	}
	else
	{
		if (toggle)
			SetStyle(GetStyle() | Tog);
		else
			SetStyle(GetStyle() & ~Tog);
	}
}

void LButton::OnCreate()
{
	SetFont(SysFont);
	if (d->WantsDefault && GetWindow())
	{
		GetWindow()->SetDefault(this);
	}
}

void LButton::OnAttach()
{
	LResources::StyleElement(this);
}

bool LButton::Default()
{
	if (_View)
	{
		DWORD Style = (DWORD)GetWindowLong(_View, GWL_STYLE);
		return Style & BS_DEFPUSHBUTTON;
	}
	
	return d->WantsDefault;
}

void LButton::Default(bool b)
{
	d->WantsDefault = b;
	
	if (_View)
	{
		DWORD Style = (DWORD)GetWindowLong(_View, GWL_STYLE);
		if (b)
			Style |= BS_DEFPUSHBUTTON;
		else
			Style &= ~BS_DEFPUSHBUTTON;
		SetWindowLong(_View, GWL_STYLE, Style);
	}
}

bool LButton::Name(const char *n)
{
	return LView::Name(n);
}

bool LButton::NameW(const char16 *n)
{
	return LView::NameW(n);
}

void LButton::SetFont(LFont *Fnt, bool OwnIt)
{
	LView::SetFont(Fnt, OwnIt);
}

int LButton::OnNotify(LViewI *Ctrl, int Flags)
{
	if (Ctrl == (LViewI*)this && Flags == GNotify_Activate)
	{
		OnClick();
	}

	return 0;
}

int LButton::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED)
	{
		OnClick();
	}
	
	return 0;
}

GMessage::Result LButton::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_CREATE:
		{
			if (d->Toggle)
				Value(d->Value);
			break;
		}
		case WM_DESTROY:
		{
			d->Value = SendMessage(Handle(), BM_GETCHECK, 0, 0);
			break;
		}
		case WM_GETDLGCODE:
		{
			return CallWindowProc(d->ButtonClassProc, Handle(), Msg->Msg(), Msg->A(), Msg->B()) |
				DLGC_WANTTAB;
		}
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			if (SysOnKey(this, Msg))
			{
				return 0;
			}
			break;
		}
		case WM_PAINT:
		{
			if (!GetCss() && !d->Img)
				break; // Just let the system draw the control

			if (IsWin7 < 0)
			{
				LArray<int> Ver;
				int Os = LGetOs(&Ver);
				if (Os == LGI_OS_WIN32 ||
					Os == LGI_OS_WIN64)
				{
					IsWin7 = Ver[0] == 6 && Ver[1] == 1;
					IsWin10 = Ver[0] == 10 && Ver[1] == 0;
				}
				else IsWin7 = false;
			}
			if (!IsWin7 && !IsWin10)
				break;

			// This is the effective background of the parent window:
			LCss::ColorDef bk = GetCss()->NoPaintColor();
		
			// If it's not the default...
			if (!bk.IsValid())
				break;

			// Then create a screen device context for painting
			LScreenDC dc(this);
			
			// Get the control to draw itself into a bitmap
			LRect c = GetPos();
			
			// Create a HBITMAP in the same size as the control 
			// and the same bit depth as the screen
			LMemDC m(c.X(), c.Y(), GdcD->GetColourSpace());
			// Create a HDC for the bitmap
			HDC hdc = m.StartDC();					
			// Ask the control to draw itself into the memory bitmap
			SendMessage(_View, WM_PRINT, (WPARAM)hdc, PRF_ERASEBKGND|PRF_CLIENT);
			// End the HDC
			m.EndDC();

			// Draw correct background
			m.Colour(bk.Rgb32, 32);
			// The outside 1px border (unfilled rect)
			m.Box(0, 0, c.X()-1, c.Y()-1);
			if (IsWin7)
			{
				// The 4 pixels at the corners
				m.Set(1, 1);
				m.Set(c.X()-2, 1);
				m.Set(1, c.Y()-2);
				m.Set(c.X()-2, c.Y()-2);
			}

			if (d->Img)
			{
				m.Op(GDC_ALPHA);
				int cx = (m.X()-d->Img->X()) >> 1;
				int cy = (m.Y()-d->Img->Y()) >> 1;
				m.Blt(cx, cy, d->Img);
			}

			// Now stick it on the screen
			dc.Blt(0, 0, &m);

			// Skip over calling the parent class' callback procedure.
			return true;
			break;
		}
	}

	GMessage::Result r = LControl::OnEvent(Msg);
	
	return r;
}

void LButton::OnMouseClick(LMouse &m)
{
}

void LButton::OnMouseEnter(LMouse &m)
{
}

void LButton::OnMouseExit(LMouse &m)
{
}

bool LButton::OnKey(LKey &k)
{
	switch (k.vkey)
	{
		case VK_ESCAPE:
		{
			if (GetId() != IDCANCEL)
				break;
			// else fall thru
		}
		case VK_RETURN:
		{
			if (!k.IsChar && k.Down())
				OnClick();
			return true;
		}
	}
	
	return false;
}

void LButton::OnFocus(bool f)
{
}

void LButton::OnPaint(LSurface *pDC)
{
	if (!_View)
	{
		// Fall back drawing code
		pDC->Colour(L_MED);
		pDC->Rectangle();
		pDC->Colour(GColour::Red);
		LRect c = GetClient();
		pDC->Line(0, 0, c.X()-1, c.Y()-1); 
		pDC->Line(c.X()-1, 0, 0, c.Y()-1); 
	}
}

int64 LButton::Value()
{
	if (d->Toggle)
	{
		if (_View)
			d->Value = SendMessage(Handle(), BM_GETCHECK, 0, 0);
		return d->Value;
	}
	else
	{
		return atoi(Name());
	}
}

void LButton::Value(int64 i)
{
	if (d->Toggle)
	{
		if (_View)
			SendMessage(_View, BM_SETCHECK, i ? BST_CHECKED : BST_UNCHECKED, 0);
		d->Value = i;
	}
}

void LButton::OnClick()
{
	int Id = GetId();
	if (Id)
	{
		LViewI *n = GetNotify();
		LViewI *p = GetParent();
		LViewI *target = n ? n : p;
		if (target)
		{
			if (Handle())
			{
				target->PostEvent(M_CHANGE, (GMessage::Param)Id);
			}
			else if (InThread())
			{
				target->OnNotify(this, 0);
			}
			else
			{
				LgiAssert(!"Out of thread and no handle?");
			}
		}
	}
	else
		SendNotify();
}

bool LButton::SetImage(const char *FileName)
{
	return false;
}

bool LButton::SetImage(LSurface *Img, bool OwnIt)
{
	if (d->ImgOwned)
		DeleteObj(d->Img);
	d->Img = Img;
	d->ImgOwned = OwnIt;

	Invalidate();
	return true;
}

void LButton::SetPreferredSize(int x, int y)
{
}

#endif