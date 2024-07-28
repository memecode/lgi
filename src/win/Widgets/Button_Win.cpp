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
#include "lgi/common/Uri.h"
#include "lgi/common/GdcTools.h"

// Size of extra pixels, beyond the size of the text itself.
LPoint LButton::Overhead =
    LPoint(	16,	// Extra width needed
			8);	// Extra height needed

static int IsWin7 = -1;
static int IsWin10 = -1;

enum ImageLoadState
{
	TCheckCss,
	TLoadFailed,
	TImgReferenced,
	TImgOwned,
};

class LButtonPrivate
{
public:
	LButton *view;
	WNDPROC ButtonClassProc = NULL;
	bool Toggle = false;
	bool WantsDefault = false;
	int64 Value = 0;
	
	double scaling = 1.0;
	LSurface *Img = NULL;
	ImageLoadState ImgState = TCheckCss;

	LButtonPrivate(LButton *v) : view(v)
	{
	}

	~LButtonPrivate()
	{
		if (ImgState == TImgOwned)
			DeleteObj(Img);
	}

	void CheckImage()
	{
		auto css = view->GetCss();
		if (ImgState != TCheckCss || !css)
			return;

		auto backgroundImage = css->BackgroundImage();
		switch (backgroundImage.Type)
		{
			case LCss::ImageUri:
			{
				// Try and load the image..
				LUri u(backgroundImage.Uri);
				if (!u.sProtocol || u.IsProtocol("file"))
				{
					LString path = u.sPath ? u.sPath : u.sHost;
					if (!LFileExists(path))
						path = LFindFile(path);
					if (!LFileExists(path))
					{
						ImgState = TLoadFailed;
						return;
					}
					Img = GdcD->Load(path);
					ImgState = Img ? TImgOwned : TLoadFailed;						
				}
				else
				{
					LAssert(!"impl network fetch?");
				}
				break;
			}
			case LCss::ImageOwn:
			{
				// Take ownership of the image
				ImgState = TImgOwned;
				backgroundImage.Type = LCss::ImageRef;
				Img = backgroundImage.Img;
				css->BackgroundImage(backgroundImage);
				break;
			}
			case LCss::ImageRef:
			{
				ImgState = TImgReferenced;
				Img = backgroundImage.Img;
				break;
			}
			default:
				break;
		}
	}
};

LButton::LButton(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Button)
{
	d = new LButtonPrivate(this);
	Name(name);
	
	if ((cx < 0 || cy < 0))
	{
		LDisplayString ds(LSysFont, ValidStr(name)?name:"1");
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
		LAssert(!"No subclass?");
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

void LButton::OnStyleChange()
{
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
	SetFont(LSysFont);
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

int LButton::OnNotify(LViewI *Ctrl, LNotification n)
{
	if (Ctrl == (LViewI*)this && n.Type == LNotifyActivate)
	{
		LMouse m;
		GetMouse(m);
		OnClick(m);
	}

	return 0;
}

int LButton::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED)
	{
		LMouse m;
		GetMouse(m);
		OnClick(m);
	}
	
	return 0;
}

bool LButton::OnLayout(LViewLayoutInfo &Inf)
{
	d->CheckImage();

	LPoint sz;
	auto txt = LView::Name();
	if (txt)
	{	
		auto fnt = GetFont();
		LDisplayString ds(fnt, txt);
		sz.x += ds.X();
		sz.y += ds.Y();
	}
	if (d->Img)
	{
		sz.x += d->Img->X();
		sz.y = MAX(sz.y, d->Img->Y());
	}

	if (auto css = GetCss())
	{
		auto height = css->Height();
		if (height.Type != LCss::LenInherit)
		{
			auto dpi = LScreenDpi();
			auto px = height.ToPx(Y(), GetFont(), dpi.y);
			if (px > 0 && px != sz.y + LButton::Overhead.y)
			{
				d->scaling = (double)(px - LButton::Overhead.y) / sz.y;
				sz.x = (int)(sz.x * d->scaling);
				sz.y = (int)(sz.y * d->scaling);
			}
		}
	}

	if (!Inf.Width.Max)
	{
		Inf.Width.Min = sz.x + LButton::Overhead.x;
		Inf.Width.Max = sz.x + LButton::Overhead.x;
	}
	else
	{
		Inf.Height.Min = sz.y + LButton::Overhead.y;
		Inf.Height.Max = sz.y + LButton::Overhead.y;
	}

	return true;
}

LMessage::Result LButton::OnEvent(LMessage *Msg)
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
			if (!GetCss())
				break; // Just let the system draw the control
			
			d->CheckImage();

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
			if (!bk.IsValid() && !d->Img)
			 	break;

			// Then create a screen device context for painting
			LScreenDC dc(this);
			
			// Get the control to draw itself into a bitmap
			LRect c = GetPos();
			
			// Create a HBITMAP in the same size as the control 
			// and the same bit depth as the screen
			LMemDC m(c.X(), c.Y(), GdcD->GetColourSpace());
			m.Colour(LColour::Red);
			m.Rectangle();

			// Create a HDC for the bitmap
			HDC hdc = m.StartDC();
			if (!hdc)
				break;
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

				if (d->scaling != 1.0)
				{
					LMemDC res(d->Img->X() * d->scaling, d->Img->Y() * d->scaling, System32BitColourSpace);
					if (ResampleDC(&res, d->Img))
					{
						int cx = (m.X()-res.X()) >> 1;
						int cy = (m.Y()-res.Y()) >> 1;
						m.Blt(cx, cy, &res);
					}
				}
				else
				{
					int cx = (m.X()-d->Img->X()) >> 1;
					int cy = (m.Y()-d->Img->Y()) >> 1;
					m.Blt(cx, cy, d->Img);
				}
			}

			// Now stick it on the screen
			dc.Blt(0, 0, &m);

			// Skip over calling the parent class' callback procedure.
			return true;
			break;
		}
	}

	LMessage::Result r = LControl::OnEvent(Msg);
	
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
			{
				LMouse m;
				GetMouse(m);
				OnClick(m);
			}
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
		pDC->Colour(LColour::Red);
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

void LButton::OnClick(const LMouse &m)
{
	SendNotify(LNotification(m));
}

void LButton::SetPreferredSize(int x, int y)
{
}

#endif