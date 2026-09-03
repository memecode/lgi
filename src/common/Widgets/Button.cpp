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
#include "lgi/common/Uri.h"
#include "lgi/common/GdcTools.h"

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

enum ImageLoadState
{
	TCheckCss,
	TLoadFailed,
	TImgReferenced,
	TImgOwned,
};

class LButtonPrivate : public LStringLayout
{
public:
	LButton *view;
	int Pressed = 0;
	bool KeyDown = false;
	bool Over = false;
	bool WantsDefault = false;
	bool Toggle = false;

	LRect TxtSz, DefaultPad, Pad, Border;
	LPoint imgSz; // scaled image size
	LSurface *Img = nullptr;
	ImageLoadState ImgState = TCheckCss;
	
	LButtonPrivate(LButton *v)
		: LStringLayout(nullptr)
		, view(v)
	{
		AmpersandToUnderline = true;
		SetWrap(false);
		SetFontCache(LAppInst->GetFontCache());
	}

	~LButtonPrivate()
	{
		if (ImgState == TImgOwned)
			DeleteObj(Img);
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
	
	LRect r(x,
			y,
			x + (cx <= 0 ? d->TxtSz.X() + Overhead.x : cx) - 1,
			y + (cy <= 0 ? d->TxtSz.Y() + Overhead.y : cy) - 1);
	LAssert(r.Valid());	
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

int LButton::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	if (Ctrl == (LViewI*)this && n.Type == LNotifyActivate)
	{
		LMouse m;
		if (n.IsMouseEvent())
			m = n.GetMouseEvent();
		else
			GetMouse(m);
		OnClick(m);
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

void LButton::OnStyleChange()
{
	// This was causing a LTableLayout loop:
	// LTableLayout::InvalidateLayout
	//  OnPaint_LButton
	//   LButton::SetFont
	//    LButton::OnStyleChange, and then fnt->null

	// SetFont(NULL);
	// GetFont();

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
	LView::SetFont(Fnt, OwnIt);
	if (Fnt)
	{
		OnStyleChange();
		Invalidate();
	}
}

LMessage::Result LButton::OnEvent(LMessage *Msg)
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
			OnClick(m);
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
					OnClick(m);
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
		if (!LAppInst->SkinEngine)
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
		if (!LAppInst->SkinEngine)
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
					d->Pressed++;
				else
					d->Pressed--;

				Invalidate();

				if (!k.Down() && d->Pressed == 0)
				{
					LMouse m;
					GetMouse(m);
					OnClick(m);
				}
			}

			return true;
			break;
		}
	}
	
	return false;
}

void LButton::OnClick(const LMouse &m)
{
	if (onClickFn)
		onClickFn(m);
	else
		SendNotify(LNotification(m));
}

void LButton::OnFocus(bool f)
{
	Invalidate();
}

void LButton::OnPaint(LSurface *pDC)
{
	// Do we need a scaled form of the image?
	LSurface *img = nullptr;
	LAutoPtr<LMemDC> memDC;
	if (d->Img)
	{
		float scale = 1.0f;
		if (d->imgSz.x < d->Img->X() ||
			d->imgSz.y < d->Img->Y())
		{
			// scale the image down to fit...
			scale = std::min((float)d->imgSz.x / d->Img->X(),
							 (float)d->imgSz.y / d->Img->Y());
		}

		if (scale != 1.0f)
		{
			if (memDC.Reset(new LMemDC(	_FL,
										(int)(d->Img->X() * scale),
										(int)(d->Img->Y() * scale),
										System32BitColourSpace)))
			{
				if (ResampleDC(memDC.Get(), d->Img))
				{
					img = memDC.Get();
				}
				else
				{
					memDC.Reset();
					img = d->Img;
				}
			}
		}
		else
		{
			img = d->Img; // no scaling
		}
	}

	if (LApp::SkinEngine &&
		TestFlag(LApp::SkinEngine->GetFeatures(), GSKIN_BUTTON))
	{
		LSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;

		State.Image = img;
		
		if (X() < GdcD->X() && Y() < GdcD->Y())
			LApp::SkinEngine->OnPaint_LButton(this, &State);
		
		LPoint pt;
		LRect r = GetClient();
		pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
		pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
		d->Paint(pDC, pt, LColour(), r, Enabled(), false);
		if (Focus())
		{
			LRect r = GetClient();
			r.Inset(5, 3);
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
			r.Inset(1, 1);
		}
		LWideBorder(pDC, r, d->Pressed ? DefaultSunkenEdge : DefaultRaisedEdge);

		LPoint pt;
		pt.x = r.x1 + ((r.X()-d->TxtSz.X())/2) + (d->Pressed != 0);
		pt.y = r.y1 + ((r.Y()-d->TxtSz.Y())/2) + (d->Pressed != 0);
		d->Paint(pDC, pt, Back, r, Enabled(), false);
	}
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
	d->SetFontCache(LAppInst->GetFontCache());
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

	int Ix = d->Img ? d->Img->X() : 0;
	int Iy = d->Img ? d->Img->Y() : 0;
	int Cx = d->TxtSz.X() + Ix + (d->TxtSz.X() && d->Img ? LTableLayout::CellSpacing : 0);
	int Cy = MAX(d->TxtSz.Y(), Iy);
	
	r.SetSize((x > 0 ? x : Cx + Overhead.x),
			  (y > 0 ? y : Cy + Overhead.y));

	SetPos(r);
}

bool LButton::OnLayout(LViewLayoutInfo &Inf)
{
	d->CheckImage();
	
	auto Css = GetCss();
	auto Font = GetFont();
	LCssTools Tools(Css, Font);
	auto c = GetClient();
	auto TxtMin = d->GetMin();
	auto TxtMax = d->GetMax();
	const int MAX_SIZE = 100000;

	auto Wnd = GetWindow();
	auto Dpi = Wnd ? Wnd->GetDpi() : LScreenDpi();
	double Scale = (double)Dpi.x / 96.0;

	d->DefaultPad.Set((int)(Scale*Overhead.x/2),
					  (int)(Scale*Overhead.y/2),
					  (int)(Scale*Overhead.x/2),
					  (int)(Scale*Overhead.y/2));
	d->Pad = Tools.GetPadding(c, &d->DefaultPad);
	d->Border = Tools.GetBorder(c);

	LCss::Len MinX, MaxX, Wid;
	LCss::Len MinY, MaxY, Height;
	if (Css)
	{
		Wid = Css->Width();
		MinX = Css->MinWidth();
		MaxX = Css->MaxWidth();

		Height = Css->Height();
		MinY = Css->MinHeight();
		MaxY = Css->MaxHeight();
	}

	int baseX = d->Pad.x1 + d->Pad.x2 + d->Border.x1 + d->Border.x2;
	int baseY = d->Pad.y1 + d->Pad.y2 + d->Border.y1 + d->Border.y2;
	if (d->Img)
	{
		d->imgSz.x = d->Img->X();
		d->imgSz.y = d->Img->Y();
	}
	else
	{
		d->imgSz.Set(0, 0);
	}

	int contentX = Wid ? Wid.ToPx(c.X(), Font) - baseX : MAX(d->imgSz.x, TxtMin.x);
	int spaceY = d->imgSz.y && TxtMin.y ? LTableLayout::CellSpacing : 0;
	int contentY = Height ? Height.ToPx(c.Y(), Font) - baseY : d->imgSz.y + spaceY + TxtMin.y;
	
	if (d->Img)
	{
		if (contentX < d->Img->X() || contentY < d->Img->Y())
		{
			// scale the image down to fit...
			auto scale = std::min((float)contentX / d->Img->X(), (float)contentY / d->Img->Y());
			d->imgSz.x = (int)(d->Img->X() * scale);
			d->imgSz.y = (int)(d->Img->Y() * scale);
		}
	}

	if (!Inf.Width.Min)
	{
		int contentX = Wid ? Wid.ToPx(c.X(), Font) : baseX + MAX(d->imgSz.x, TxtMin.x);
		int minX = MinX ? MinX.ToPx(c.X(), Font) : 0;
		int maxX = MaxX ? MaxX.ToPx(c.X(), Font) : MAX_SIZE;

		Inf.Width.Min = MAX(minX, contentX);
		Inf.Width.Max = MIN(maxX, contentX);
		
		#if 0
		LgiTrace("%i.Layout.Btn.x = %i, %i  valid=%i,%i c=%s, base=%i, img=%i\n", GetId(), 
			Inf.Width.Min, Inf.Width.Max,
			(bool)MinX, (bool)MaxX,
			c.GetStr(),
			baseX, d->imgSz.x);
		#endif
	}
	else
	{
		int contentY = Height ? Height.ToPx(c.Y(), Font) : baseY + d->imgSz.y + spaceY + TxtMin.y;
		int minY = MinY ? MinY.ToPx(c.Y(), Font) : 0;
		int maxY = MaxY ? MaxY.ToPx(c.Y(), Font) : MAX_SIZE;
		Inf.Height.Min = MAX(minY, contentY);
		Inf.Height.Max = MIN(maxY, contentY);
	}

	return true;
}

#endif
