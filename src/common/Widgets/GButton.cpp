#if !defined(_WIN32) || (XP_BUTTON != 0)

#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSkinEngine.h"
#include "GButton.h"
#include "GDisplayString.h"
#include "GTableLayout.h"
#include "LgiRes.h"
#include "LStringLayout.h"
#include "GCssTools.h"

#define DOWN_MOUSE		0x1
#define DOWN_KEY		0x2

// Size of extra pixels, beyond the size of the text itself.
GdcPt2 GButton::Overhead =
    GdcPt2(
        // Extra width needed
        #if defined(MAC) && !defined(LGI_SDL)
        24,
        #else
        16,
        #endif
        // Extra height needed
        6
    );

class GButtonPrivate : public LStringLayout
{
public:
	int Pressed;
	bool KeyDown;
	bool Over;
	bool WantsDefault;
	bool Toggle;
	
	GRect TxtSz;
	GSurface *Image;
	bool OwnImage;
	
	GButtonPrivate() : LStringLayout(LgiApp->GetFontCache())
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

	~GButtonPrivate()
	{
		if (OwnImage)
			DeleteObj(Image);
	}

	void Layout(GCss *css, char *s)
	{
		Empty();

		GCss c(*css);
		c.FontWeight(GCss::FontWeightBold);
		
		Add(s, &c);

		int32 MinX, MaxX;
		DoPreLayout(MinX, MaxX);
		DoLayout(MaxX);
		TxtSz = GetBounds();
	}
};

GButton::GButton(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Button)
{
	d = new GButtonPrivate;
	Name(name);
	
	GRect r(x,
			y,
			x + (cx <= 0 ? d->TxtSz.X() + Overhead.x : cx) - 1,
			y + (cy <= 0 ? d->TxtSz.Y() + Overhead.y : cy) - 1);
	LgiAssert(r.Valid());
	SetPos(r);
	SetId(id);
	SetTabStop(true);
}

GButton::~GButton()
{
	if (GetWindow() &&
		GetWindow()->_Default == this)
	{
		GetWindow()->_Default = 0;
	}

	DeleteObj(d);
}

int GButton::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl == (GViewI*)this && Flags == GNotify_Activate)
	{
		OnClick();
	}

	return 0;
}

bool GButton::Default()
{
	if (GetWindow())
		return GetWindow()->_Default == this;
	
	return false;
}

void GButton::Default(bool b)
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

bool GButton::GetIsToggle()
{
	return d->Toggle;
}

void GButton::SetIsToggle(bool toggle)
{
	d->Toggle = toggle;
}

GSurface *GButton::GetImage()
{
	return d->Image;
}

bool GButton::SetImage(const char *FileName)
{
	if (d->OwnImage)
		DeleteObj(d->Image);
	d->Image = GdcD->Load(FileName);
	Invalidate();
	return d->OwnImage = d->Image != NULL;
}

bool GButton::SetImage(GSurface *Img, bool OwnIt)
{
	if (d->OwnImage)
		DeleteObj(d->Image);
	d->Image = Img;
	d->OwnImage = d->Image != NULL && OwnIt;
	Invalidate();
	return d->OwnImage;
}

void GButton::OnStyleChange()
{
	d->Layout(GetCss(true), GBase::Name());
}

bool GButton::Name(const char *n)
{
	bool Status = GView::Name(n);
	OnStyleChange();
	return Status;
}

bool GButton::NameW(const char16 *n)
{
	bool Status = GView::NameW(n);
	OnStyleChange();
	return Status;
}

void GButton::SetFont(GFont *Fnt, bool OwnIt)
{
	LgiAssert(Fnt && Fnt->Handle());
	GView::SetFont(Fnt, OwnIt);
	OnStyleChange();
	Invalidate();
}

GMessage::Result GButton::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GButton::OnMouseClick(GMouse &m)
{
	if (!Enabled())
		return;

	if (d->Toggle)
	{
		if (m.Down())
		{
			Value(!Value());
			OnClick();
		}
	}
	else
	{
		bool Click = IsCapturing();
		Capture(m.Down());
		
		if (Click ^ m.Down())
		{
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

void GButton::OnMouseEnter(GMouse &m)
{
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

void GButton::OnMouseExit(GMouse &m)
{
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

bool GButton::OnKey(GKey &k)
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

void GButton::OnClick()
{
	int Id = GetId();
	if (Id)
	{
		GViewI *n = GetNotify();
		GViewI *p = GetParent();
		GViewI *target = n ? n : p;
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

void GButton::OnFocus(bool f)
{
	Invalidate();
}

void GButton::OnPaint(GSurface *pDC)
{
	#if defined LGI_CARBON

		GColour NoPaintColour = StyleColour(GCss::PropBackgroundColor, GColour(L_MED));
		if (!NoPaintColour.IsTransparent())
		{
			pDC->Colour(NoPaintColour);
			pDC->Rectangle();
		}
	
		GRect rc = GetClient();
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
			GdcPt2 pt;
			GRect r = GetClient();
			pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
			pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
			d->Paint(pDC, pt, GColour(), r, Enabled(), Info.state == kThemeStatePressed);
		}
	
	#else

		if (GApp::SkinEngine &&
			TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_BUTTON))
		{
			GSkinState State;
			State.pScreen = pDC;
			State.MouseOver = d->Over;

			State.Image = d->Image;
			
			if (X() < GdcD->X() && Y() < GdcD->Y())
				GApp::SkinEngine->OnPaint_GButton(this, &State);
			
			GdcPt2 pt;
			GRect r = GetClient();
			pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
			pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
			d->Paint(pDC, pt, GColour(), r, Enabled(), false);
			if (Focus())
			{
				GRect r = GetClient();
				r.Size(5, 3);
				pDC->Colour(GColour(180, 180, 180));
				pDC->LineStyle(GSurface::LineAlternate);
				pDC->Box(&r);
			}
		}
		else
		{
			GColour Back(d->Over ? L_HIGH : L_MED);
			GRect r(0, 0, X()-1, Y()-1);
			if (Default())
			{
				pDC->Colour(L_BLACK);
				pDC->Box(&r);
				r.Size(1, 1);
			}
			LgiWideBorder(pDC, r, d->Pressed ? DefaultSunkenEdge : DefaultRaisedEdge);

			GdcPt2 pt;
			pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
			pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
			d->Paint(pDC, pt, Back, r, Enabled(), false);
		}
	
	#endif
}

int64 GButton::Value()
{
	return d->Pressed != 0;
}

void GButton::Value(int64 i)
{
	d->Pressed = (int)i;
	Invalidate();
}

void GButton::OnCreate()
{
}

void GButton::OnAttach()
{
	LgiResources::StyleElement(this);
	OnStyleChange();
	GView::OnAttach();

	if (d->WantsDefault)
	{
		d->WantsDefault = false;
		if (GetWindow())
		{
			GetWindow()->_Default = this;
		}
	}
}

void GButton::SetPreferredSize(int x, int y)
{
	GRect r = GetPos();

	int Ix = d->Image ? d->Image->X() : 0;
	int Iy = d->Image ? d->Image->Y() : 0;
	int Cx = d->TxtSz.X() + Ix + (d->TxtSz.X() && d->Image ? GTableLayout::CellSpacing : 0);
	int Cy = MAX(d->TxtSz.Y(), Iy);
	
	r.Dimension((x > 0 ? x : Cx + Overhead.x) - 1,
				(y > 0 ? y : Cy + Overhead.y) - 1);

	SetPos(r);
}

bool GButton::OnLayout(GViewLayoutInfo &Inf)
{
	auto Css = GetCss();
	auto Font = GetFont();
	GCssTools Tools(Css, Font);
	auto c = GetClient();
	auto TxtMin = d->GetMin();
	auto TxtMax = d->GetMax();
	GRect DefaultPad(Overhead.x>>1, Overhead.y>>1, Overhead.x>>1, Overhead.y>>1);
	GRect Pad = Tools.GetPadding(c, &DefaultPad), Border = Tools.GetBorder(c);

	if (!Inf.Width.Min)
	{
		int BaseX = Pad.x1 + Pad.x2 + Border.x1 + Border.x2;
		int ImgX = d->Image ? d->Image->X() + 4/*img->text spacer*/ : 0;
		GCss::Len MinX, MaxX;
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
		GCss::Len MinY, MaxY;
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
