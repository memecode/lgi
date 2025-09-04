// \file
// \author Matthew Allen
// \brief Native Win32 Check Box Class

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "LSkinEngine.h"
#include "LCheckBox.h"
#include "LgiRes.h"
#include "LCssTools.h"

class LCheckBoxPrivate
{
public:
	int64 InitState;

	LCheckBoxPrivate()
	{
		InitState = 0;
	}

	~LCheckBoxPrivate()
	{
	}
};

LCheckBox::LCheckBox(int id, int x, int y, int cx, int cy, const char *name, int initstate) :
	ResObject(Res_CheckBox)
{
	d = new LCheckBoxPrivate;
	d->InitState = initstate;

	Name(name);
	
	if (cx < 0 || cy < 0)
	{
		LDisplayString ds(LSysFont, name);
		if (cx < 0)
			cx = 18 + ds.X();
		if (cy < 0)
			cy = ds.Y();
	}
	
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_AUTOCHECKBOX);

	#if 1
	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = LWindowsClass::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LAssert(!"No subclass?");
	#else
	SetClassW32("BUTTON");
	#endif
}

LCheckBox::~LCheckBox()
{
	DeleteObj(d);
}

#include "Uxtheme.h"
void LCheckBox::OnAttach()
{
	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();

	SetFont(LSysFont);
	Value(d->InitState);
}

void LCheckBox::OnStyleChange()
{
	Invalidate();
}

bool LCheckBox::ThreeState()
{
	return TestFlag(GetStyle(), BS_3STATE);
}

void LCheckBox::ThreeState(bool t)
{
	auto Cur = GetStyle();
	if (t)
		SetStyle((Cur & ~BS_AUTOCHECKBOX) | BS_AUTO3STATE);
	else
		SetStyle((Cur & ~BS_AUTO3STATE) | BS_AUTOCHECKBOX);
}

bool LCheckBox::Name(const char *n)
{
	return LView::Name(n);
}

bool LCheckBox::NameW(const char16 *n)
{
	return LView::NameW(n);
}

void LCheckBox::SetFont(LFont *Fnt, bool OwnIt)
{
	LView::SetFont(Fnt, OwnIt);
}

int LCheckBox::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED)
	{
		LMouse ms;
		GetMouse(ms);
		OnClick(ms);
	}
	
	return 0;
}

void LCheckBox::OnClick(const LMouse &m)
{
	if (onClickFn)
		onClickFn(m);
	else
	{
		if (d->Three)
		{
			switch (d->Val)
			{
				case 0:
					Value(2); break;
				case 2:
					Value(1); break;
				default:
					Value(0); break;
			}
		}
		else Value(!d->Val);

		SendNotify((int)Value());
	}
}

int LCheckBox::OnNotify(LViewI *Ctrl, int Flags)
{
	if (Ctrl == (LViewI*)this && Flags == LNotifyActivate)
	{
		Value(!Value());
	}

	return 0;
}

LMessage::Result LCheckBox::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_CREATE:
		{
			HRESULT r = SetWindowTheme(Handle(), NULL, NULL);
			break;			
		}
		case WM_DESTROY:
		{
			// Copy state back into a local var
			d->InitState = SendMessage(Handle(), BM_GETCHECK, 0, 0);
			break;
		}
		case WM_ERASEBKGND:
		{
			LScreenDC Dc((HDC)Msg->A(), _View);
			LCssTools Tools(this);
			Dc.Colour(Tools.GetBack());
			Dc.Rectangle();
			return true;
			break;
		}
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			if (SysOnKey(this, Msg))
				return 0;
			break;
		}
	}

	return LControl::OnEvent(Msg);
}

void LCheckBox::OnMouseClick(LMouse &m)
{
}

void LCheckBox::OnMouseEnter(LMouse &m)
{
}

void LCheckBox::OnMouseExit(LMouse &m)
{
}

bool LCheckBox::OnKey(LKey &k)
{
	return false;
}

void LCheckBox::OnFocus(bool f)
{
}

void LCheckBox::OnPaint(LSurface *pDC)
{
}

int64 LCheckBox::Value()
{
	if (Handle())
	{
		return SendMessage(Handle(), BM_GETCHECK, 0, 0);
	}

	return d->InitState;
}

void LCheckBox::Value(int64 i)
{
	if (Handle())
	{
		SendMessage(Handle(), BM_SETCHECK, i, 0);
	}
	else
	{
		d->InitState = i;
	}
}

bool LCheckBox::OnLayout(LViewLayoutInfo &Inf)
{
	if (Inf.Width.Max)
	{
		int y = GetFont()->GetHeight();
		Inf.Height.Min = y;
		Inf.Height.Max = y;
	}
	else
	{
		LDisplayString s(GetFont(), Name());
		int x = s.X() + 32;
		Inf.Width.Min = x;
		Inf.Width.Max = x;
	}

	return true;
}

void LCheckBox::OnPosChange()
{
}
