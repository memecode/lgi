#if !defined(_WIN32) || (XP_BUTTON != 0)

#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Button.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/StringLayout.h"
#include "lgi/common/CssTools.h"

#define DOWN_MOUSE		0x1
#define DOWN_KEY		0x2

#if 0
#define DEBUG_LOG(...) printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif

// Size of extra pixels, beyond the size of the text itself.
LPoint LButton::Overhead =
    LPoint(
        // Extra width needed
        #if defined(MAC) && !defined(LGI_SDL)
        24,
        #else
        16,
        #endif
        // Extra height needed
        6
    );

class LButtonPrivate : public LStringLayout
{
public:
	int Pressed;
	bool KeyDown;
	bool Over;
	bool WantsDefault;
	bool Toggle;
	
	LRect TxtSz;
	LSurface *Image;
	bool OwnImage;
	
	LButtonPrivate() : LStringLayout(LgiApp->GetFontCache())
	{
		AmpersandToUnderline = true;
		Pressed = 0;
		KeyDown = false;
		Toggle = false;
		Over = 0;
		WantsDefault = false;
		Image = NULL;
		OwnImage = false;
		SetWrap(false);
	}

	~LButtonPrivate()
	{
		if (OwnImage)
			DeleteObj(Image);
	}

	void Layout(LCss *css, const char *s)
	{
		Empty();

		LCss c(*css);
		c.FontWeight(LCss::FontWeightBold);
		
		Add(s, &c);

		int32 MinX, MaxX;
		DoPreLayout(MinX, MaxX);
		DoLayout(MaxX);
		TxtSz = GetBounds();
	}
};

LButton::LButton(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Button)
{
	d = new LButtonPrivate;
	Name(name);
	
	LRect r(x,
			y,
			x + (cx <= 0 ? d->TxtSz.X() + Overhead.x : cx) - 1,
			y + (cy <= 0 ? d->TxtSz.Y() + Overhead.y : cy) - 1);
	LgiAssert(r.Valid());
	SetPos(r);
	SetId(id);
	SetTabStop(true);
}

LButton::~LButton()
{
	if (GetWindow() &&
		GetWindow()->_Default == this)
	{
		GetWindow()->_Default = 0;
	}

	DeleteObj(d);
}

int LButton::OnNotify(LViewI *Ctrl, int Flags)
{
	if (Ctrl == (LViewI*)this && Flags == GNotify_Activate)
	{
		OnClick();
	}

	return 0;
}

bool LButton::Default()
{
	if (GetWindow())
		return GetWindow()->_Default == this;
	
	return false;
}

void LButton::Default(bool b)
{
	if (GetWindow())
	{
		GetWindow()->_Default = b ? this : 0;
		if (IsAttached())
		{
			Invalidate();
		}
	}
	else
	{
		d->WantsDefault = b;
	}
}

bool LButton::GetIsToggle()
{
	return d->Toggle;
}

void LButton::SetIsToggle(bool toggle)
{
	d->Toggle = toggle;
}

LSurface *LButton::GetImage()
{
	return d->Image;
}

bool LButton::SetImage(const char *FileName)
{
	if (d->OwnImage)
		DeleteObj(d->Image);
	d->Image = GdcD->Load(FileName);
	Invalidate();
	return d->OwnImage = d->Image != NULL;
}

bool LButton::SetImage(LSurface *Img, bool OwnIt)
{
	if (d->OwnImage)
		DeleteObj(d->Image);
	d->Image = Img;
	d->OwnImage = d->Image != NULL && OwnIt;
	Invalidate();
	return d->OwnImage;
}

void LButton::OnStyleChange()
{
	d->Layout(GetCss(true), LBase::Name());
}

bool LButton::Name(const char *n)
{
	bool Status = LView::Name(n);
	OnStyleChange();
	return Status;
}

bool LButton::NameW(const char16 *n)
{
	bool Status = LView::NameW(n);
	OnStyleChange();
	return Status;
}

void LButton::SetFont(LFont *Fnt, bool OwnIt)
{
	LgiAssert(Fnt && Fnt->Handle());
	LView::SetFont(Fnt, OwnIt);
	OnStyleChange();
	Invalidate();
}

GMessage::Result LButton::OnEvent(GMessage *Msg)
{
	return LView::OnEvent(Msg);
}

void LButton::OnMouseClick(LMouse &m)
{
	if (!Enabled())
	{
		DEBUG_LOG("Not enabled\n");
		return;
	}

	if (d->Toggle)
	{
		DEBUG_LOG("OnMouseClick: Toggle=true, m.Down=%i\n", m.Down());
		if (m.Down())
		{
			Value(!Value());
			OnClick();
		}
	}
	else
	{
		bool Click = IsCapturing();
		DEBUG_LOG("OnMouseClick: Toggle=false, Click=%i, m.Down()=%i\n", Click, m.Down());
		Capture(m.Down());
		
		if (Click ^ m.Down())
		{
			DEBUG_LOG("d->Over=%i\n", d->Over);
			if (d->Over)
			{
				if (m.Down())
				{
					d->Pressed++;
					Focus(true);
				}
				else
				{
					d->Pressed--;
				}
				
				Invalidate();

				DEBUG_LOG("m.Down()=%i d->Pressed=%i\n", m.Down(), d->Pressed);
				if (!m.Down() &&
					d->Pressed == 0)
				{
					// This may delete ourself, so do it last.
					OnClick();
				}
			}
		}
	}
}

void LButton::OnMouseEnter(LMouse &m)
{
	DEBUG_LOG("OnMouseEnter\n");
	d->Over = true;
	if (IsCapturing())
	{
		Value(d->Pressed + 1);
	}
	else if (Enabled())
	{
		if (!LgiApp->SkinEngine)
			Invalidate();
	}
}

void LButton::OnMouseExit(LMouse &m)
{
	DEBUG_LOG("OnMouseExit\n");
	d->Over = false;
	if (IsCapturing())
	{
		Value(d->Pressed - 1);
	}
	else if (Enabled())
	{
		if (!LgiApp->SkinEngine)
			Invalidate();
	}
}

bool LButton::OnKey(LKey &k)
{
	if (
		#ifdef WINNATIVE
		k.IsChar ||
		#endif
		!Enabled())
	{
		return false;
	}
	
	switch (k.vkey)
	{
		case LK_ESCAPE:
		{
			if (GetId() != IDCANCEL)
			{
				break;
			}
			// else fall thru
		}
		case LK_SPACE:
		case LK_RETURN:
		#ifndef WINDOWS
		case LK_KEYPADENTER:
		#endif
		{
			if (d->KeyDown ^ k.Down())
			{
				d->KeyDown = k.Down();
				if (k.Down())
				{
					d->Pressed++;
				}
				else
				{
					d->Pressed--;
				}

				Invalidate();

				if (!k.Down() && d->Pressed == 0)
				{
					OnClick();
				}
			}

			return true;
			break;
		}
	}
	
	return false;
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
			#if LGI_VIEW_HANDLE
			if (Handle())
			#else
			if (IsAttached())
			#endif
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

void LButton::OnFocus(bool f)
{
	Invalidate();
}

void LButton::OnPaint(LSurface *pDC)
{
	#if defined LGI_CARBON

		LColour NoPaintColour = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
		if (!NoPaintColour.IsTransparent())
		{
			pDC->Colour(NoPaintColour);
			pDC->Rectangle();
		}
	
		LRect rc = GetClient();
		rc.x1 += 2;
		rc.y2 -= 1;
		rc.x2 -= 1;
		HIRect Bounds = rc;
		HIThemeButtonDrawInfo Info;
		HIRect LabelRect;

		Info.version = 0;
		Info.state = d->Pressed ? kThemeStatePressed : (Enabled() ? kThemeStateActive : kThemeStateInactive);
		Info.kind = kThemePushButton;
		Info.value = /*Default() ? kThemeButtonOn :*/ kThemeButtonOff;
		Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;

		OSStatus e = HIThemeDrawButton(	  &Bounds,
										  &Info,
										  pDC->Handle(),
										  kHIThemeOrientationNormal,
										  &LabelRect);

		if (e) printf("%s:%i - HIThemeDrawButton failed %li\n", _FL, e);
		else
		{
			LPoint pt;
			LRect r = GetClient();
			pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
			pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
			d->Paint(pDC, pt, LColour(), r, Enabled(), Info.state == kThemeStatePressed);
		}
	
	#else

		if (GApp::SkinEngine &&
			TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_BUTTON))
		{
			LSkinState State;
			State.pScreen = pDC;
			State.MouseOver = d->Over;

			State.Image = d->Image;
			
			if (X() < GdcD->X() && Y() < GdcD->Y())
				GApp::SkinEngine->OnPaint_LButton(this, &State);
			
			LPoint pt;
			LRect r = GetClient();
			pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
			pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
			d->Paint(pDC, pt, LColour(), r, Enabled(), false);
			if (Focus())
			{
				LRect r = GetClient();
				r.Size(5, 3);
				pDC->Colour(LColour(180, 180, 180));
				pDC->LineStyle(LSurface::LineAlternate);
				pDC->Box(&r);
			}
		}
		else
		{
			LColour Back(d->Over ? L_HIGH : L_MED);
			LRect r(0, 0, X()-1, Y()-1);
			if (Default())
			{
				pDC->Colour(L_BLACK);
				pDC->Box(&r);
				r.Size(1, 1);
			}
			LWideBorder(pDC, r, d->Pressed ? DefaultSunkenEdge : DefaultRaisedEdge);

			LPoint pt;
			pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
			pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
			d->Paint(pDC, pt, Back, r, Enabled(), false);
		}
	
	#endif
}

int64 LButton::Value()
{
	return d->Pressed != 0;
}

void LButton::Value(int64 i)
{
	d->Pressed = (int)i;
	Invalidate();
}

void LButton::OnCreate()
{
}

void LButton::OnAttach()
{
	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();

	if (d->WantsDefault)
	{
		d->WantsDefault = false;
		if (GetWindow())
			GetWindow()->_Default = this;
	}
}

void LButton::SetPreferredSize(int x, int y)
{
	LRect r = GetPos();

	int Ix = d->Image ? d->Image->X() : 0;
	int Iy = d->Image ? d->Image->Y() : 0;
	int Cx = d->TxtSz.X() + Ix + (d->TxtSz.X() && d->Image ? LTableLayout::CellSpacing : 0);
	int Cy = MAX(d->TxtSz.Y(), Iy);
	
	r.Dimension((x > 0 ? x : Cx + Overhead.x) - 1,
				(y > 0 ? y : Cy + Overhead.y) - 1);

	SetPos(r);
}

bool LButton::OnLayout(LViewLayoutInfo &Inf)
{
	LPoint Dpi(96, 96);
	auto Css = GetCss();
	auto Font = GetFont();
	LCssTools Tools(Css, Font);
	auto c = GetClient();
	auto TxtMin = d->GetMin();
	auto TxtMax = d->GetMax();

	auto Wnd = GetWindow();
	if (Wnd)
		Dpi = Wnd->GetDpi();
	double Scale = (double)Dpi.x / 96.0;

	LRect DefaultPad(Scale*Overhead.x/2, Scale*Overhead.y/2, Scale*Overhead.x/2, Scale*Overhead.y/2);
	LRect Pad = Tools.GetPadding(c, &DefaultPad), Border = Tools.GetBorder(c);

	if (!Inf.Width.Min)
	{
		int BaseX = Pad.x1 + Pad.x2 + Border.x1 + Border.x2;
		int ImgX = d->Image ? d->Image->X() + 4/*img->text spacer*/ : 0;
		LCss::Len MinX, MaxX;
		if (Css)
		{
			MinX = Css->MinWidth();
			MaxX = Css->MaxWidth();
		}

		Inf.Width.Min = MinX.IsValid() ? MinX.ToPx(c.X(), Font) : BaseX + ImgX + TxtMin.x;
		Inf.Width.Max = MaxX.IsValid() ? MaxX.ToPx(c.X(), Font) : BaseX + ImgX + TxtMax.x;
		
		/*
		LgiTrace("%i.Layout.Btn.x = %i, %i  valid=%i,%i c=%s, base=%i, img=%i\n", GetId(), 
			Inf.Width.Min, Inf.Width.Max,
			MinX.IsValid(), MaxX.IsValid(),
			c.GetStr(),
			BaseX, ImgX);
		*/
	}
	else
	{
		int BaseY = Pad.y1 + Pad.y2 + Border.y1 + Border.y2;
		int ImgY = d->Image ? d->Image->Y() : 0;
		LCss::Len MinY, MaxY;
		if (Css)
		{
			MinY = Css->MinHeight();
			MaxY = Css->MaxHeight();
		}

		Inf.Height.Min = MinY.IsValid() ? MinY.ToPx(c.Y(), Font) : BaseY + MAX(ImgY, TxtMin.y);
		Inf.Height.Max = MaxY.IsValid() ? MaxY.ToPx(c.Y(), Font) : BaseY + MAX(ImgY, TxtMax.y);

		// LgiTrace("%i.Layout.Btn.y = %i, %i\n", GetId(), Inf.Height.Min, Inf.Height.Max);
	}

	return true;
}

#endif
