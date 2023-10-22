#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/StringLayout.h"
#include "lgi/common/LgiRes.h"

static int PadX1Px = 20;
static int PadX2Px = 6;
#ifdef MAC
static int PadYPx = 8;
static int TextYOffset = 6;
#else
static int PadYPx = 0;
static int TextYOffset = 0;
#endif
static int MinYSize = 16;

class LCheckBoxPrivate : public LMutex, public LStringLayout
{
	LCheckBox *Ctrl = NULL;
	
public:
	int64 Val = 0;
	bool Over = false;
	bool Three = false;
	LRect ValuePos;
	#ifdef HAIKU
	OsThreadId CreateThread = -1;
	#endif

	LCheckBoxPrivate(LCheckBox *ctrl) :
		LMutex("LCheckBoxPrivate"),
		Ctrl(ctrl),
		#ifdef HAIKU
		// This is most likely running in the constructor of the window. Which is NOT the BWindow's thread.
		// Therefor wait for the actually thread before getting a thread specific font cache.
		LStringLayout(NULL),
		CreateThread(LCurrentThreadId())
		#else
		LStringLayout(LAppInst->GetFontCache())
		#endif
	{	
		Ctrl = ctrl;
		Wrap = true;
		AmpersandToUnderline = true;
		ValuePos.ZOff(-1, -1);
		SetFontCache(LAppInst->GetFontCache());
	}

	bool PreLayout(int32 &Min, int32 &Max)
	{
		if (Lock(_FL))
		{
			DoPreLayout(Min, Max);
			Unlock();
		}
		else return false;
		return true;
	}

	bool Layout(int Px)
	{		
		if (Lock(_FL))
		{
			DoLayout(Px, MinYSize);
			Unlock();
		}
		else return false;
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
// Check box
LCheckBox::LCheckBox(int id, int x, int y, int cx, int cy, const char *name, int InitState) :
	ResObject(Res_CheckBox)
{
	d = new LCheckBoxPrivate(this);
	Name(name);
	LPoint Max = d->GetMax();
	if (cx < 0) cx = Max.x + PadX1Px + PadX2Px;
	if (cy < 0) cy = MAX(Max.y, MinYSize) + PadYPx;

	d->Val = InitState;
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
}

LCheckBox::~LCheckBox()
{
	DeleteObj(d);
}

#if defined(WINNATIVE)
	int LCheckBox::SysOnNotify(int Msg, int Code)
	{
		return 0;
	}
#elif defined(HAIKU)
	void LCheckBox::OnCreate()
	{
		d->SetFontCache(LAppInst->GetFontCache());
	}
#endif

void LCheckBox::OnAttach()
{
	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();
}

void LCheckBox::OnStyleChange()
{
	if (d->Lock(_FL))
	{
		d->Empty();
		d->Add(LView::Name(), GetCss());
		d->DoLayout(X());
		d->Unlock();
		Invalidate();
	}
}

int LCheckBox::OnNotify(LViewI *Ctrl, LNotification n)
{
	if (Ctrl == (LViewI*)this && n.Type == LNotifyActivate)
	{
		Value(!Value());
	}

	return 0;
}

LMessage::Result LCheckBox::OnEvent(LMessage *m)
{
	return LView::OnEvent(m);
}

bool LCheckBox::ThreeState()
{
	return d->Three;
}

void LCheckBox::ThreeState(bool t)
{
	d->Three = t;
}

int64 LCheckBox::Value()
{
	return d->Val;
}

void LCheckBox::Value(int64 i)
{
	if (d->Val != i)
	{
		d->Val = i;
		Invalidate(&d->ValuePos);

		SendNotify(LNotifyValueChanged);
	}
}

bool LCheckBox::Name(const char *n)
{
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = LView::Name(n);
		
		d->Empty();
		d->Add(n, GetCss());
		d->SetBaseFont(GetFont());
		
		auto x = X();
		d->DoLayout(x ? x : GdcD->X());
		
		d->Unlock();
	}

	return Status;
}

bool LCheckBox::NameW(const char16 *n)
{
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = LView::NameW(n);

		d->Empty();
		d->Add(LBase::Name(), GetCss());
		d->SetBaseFont(GetFont());

		auto x = X();
		d->DoLayout(x ? x : GdcD->X());

		d->Unlock();
	}

	return Status;
}

void LCheckBox::SetFont(LFont *Fnt, bool OwnIt)
{
	LAssert(Fnt && Fnt->Handle());

	if (d->Lock(_FL))
	{
		LView::SetFont(Fnt, OwnIt);
		d->Unlock();
	}
	d->Layout(X());
	Invalidate();
}

void LCheckBox::OnMouseClick(LMouse &m)
{
	if (Enabled())
	{
		int Click = IsCapturing();

		Capture(d->Over = m.Down());
		if (m.Down())
		{
			Focus(true);
		}
		
		LRect r(0, 0, X()-1, Y()-1);
		if (!m.Down() &&
			r.Overlap(m.x, m.y) &&
			Click)
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
			else
			{
				Value(!d->Val);
			}
		}
		else
		{
			Invalidate(&d->ValuePos);
		}
	}
}

void LCheckBox::OnMouseEnter(LMouse &m)
{
	if (IsCapturing())
	{
		d->Over = true;
		Invalidate(&d->ValuePos);
	}
}

void LCheckBox::OnMouseExit(LMouse &m)
{
	if (IsCapturing())
	{
		d->Over = false;
		Invalidate(&d->ValuePos);
	}
}

bool LCheckBox::OnKey(LKey &k)
{
	switch (k.vkey)
	{
		case LK_SPACE:
		{
			if (!k.Down())
				Value(!Value());

			return true;
		}
	}
	
	return false;
}

void LCheckBox::OnFocus(bool f)
{
	Invalidate();
}

void LCheckBox::OnPosChange()
{
	d->Layout(X());
}

bool LCheckBox::OnLayout(LViewLayoutInfo &Inf)
{
	if (!Inf.Width.Min)
	{
		auto n = Name();
		if (n)
		{
			d->PreLayout(Inf.Width.Min,
						 Inf.Width.Max);
		
			// FIXME: no wrapping support so....
			Inf.Width.Min = Inf.Width.Max;
		
			Inf.Width.Min += PadX1Px + PadX2Px;
			Inf.Width.Max += PadX1Px + PadX2Px;
		}
		else
		{
			auto Fnt = GetFont();			
			Inf.Width.Min = Inf.Width.Max = (int32)(Fnt->Ascent() + 2);
		}
	}
	else
	{
		d->Layout(Inf.Width.Max);
		Inf.Height.Min = d->GetMin().y + PadYPx;
		Inf.Height.Max = d->GetMax().y + PadYPx;
	}
	return true;
}

int LCheckBox::BoxSize()
{
	auto Fnt = GetFont();
	int Px = (int) Fnt->Ascent() + 2;
	if (Px > Y())
		Px = Y();
	return Px;
}

void LCheckBox::OnPaint(LSurface *pDC)
{
	#if 0
	pDC->Colour(LColour(255, 0, 255));
	pDC->Rectangle();
	#endif
	
	if (LApp::SkinEngine &&
		TestFlag(LApp::SkinEngine->GetFeatures(), GSKIN_CHECKBOX))
	{
		// auto Fnt = GetFont();
		int Px = BoxSize();

		LSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;
		State.aText = d->GetStrs();
		d->ValuePos.Set(0, 0, Px-1, Px-1);
		d->ValuePos.Offset(0, (Y()-d->ValuePos.Y())>>1);
		State.Rect = d->ValuePos;
		LApp::SkinEngine->OnPaint_LCheckBox(this, &State);
	}
	else
	{
		bool en = Enabled();
		LRect r = GetClient();
		
		#if defined MAC && !LGI_COCOA
		d->ValuePos.Set(0, 0, PadX1Px, MinYSize);
		#else
		d->ValuePos.Set(0, 0, 12, 12);
		#endif
		
		if (d->ValuePos.y2 < r.y2)
		{
			pDC->Colour(L_MED);
			pDC->Rectangle(0, d->ValuePos.y2+1, d->ValuePos.x2, r.y2);
		}

		LRect t = r;
		t.x1 = d->ValuePos.x2 + 1;
		// LColour cFore = StyleColour(LCss::PropColor, LC_TEXT);
		LColour cBack = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));

		if (d->Lock(_FL))
		{
			int Ty = MAX(0, r.Y() - d->GetBounds().Y()) >> 1;
			LPoint pt(t.x1, t.y1 + MIN(Ty, TextYOffset));
					
			d->Paint(pDC, pt, cBack, t, en, false);
			d->Unlock();
		}

		#if defined LGI_CARBON

			if (!cBack.IsTransparent())
			{
				pDC->Colour(cBack);
				pDC->Rectangle(d->ValuePos.x1, d->ValuePos.y1, d->ValuePos.x2, Y()-1);
			}
		
			LRect c = GetClient();
			#if 0
			pDC->Colour(LColour(255, 0, 0));
			pDC->Box(&c);
			pDC->Line(c.x1, c.y1, c.x2, c.y2);
			pDC->Line(c.x2, c.y1, c.x1, c.y2);
			#endif

			for (LViewI *v = this; v && !v->Handle(); v = v->GetParent())
			{
				LRect p = v->GetPos();
				c.Offset(p.x1, p.y1);
			}
			
			HIRect Bounds = c;
			HIThemeButtonDrawInfo Info;
			HIRect LabelRect;
	
			Info.version = 0;
			Info.state = d->Val ? kThemeStatePressed : (Enabled() ? kThemeStateActive : kThemeStateInactive);
			Info.kind = kThemeCheckBox;
			Info.value = d->Val ? kThemeButtonOn : kThemeButtonOff;
			Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;
	
			OSStatus e = HIThemeDrawButton(	  &Bounds,
											  &Info,
											  pDC->Handle(),
											  kHIThemeOrientationNormal,
											  &LabelRect);
	
			if (e) printf("%s:%i - HIThemeDrawButton failed %li\n", _FL, e);
		
		
		#else
	
			LWideBorder(pDC, d->ValuePos, DefaultSunkenEdge);
			pDC->Colour(d->Over || !en ? L_MED : L_WORKSPACE);
			pDC->Rectangle(&d->ValuePos);
			pDC->Colour(en ? L_TEXT : L_LOW);

			if (d->Three && d->Val == 2)
			{
				for (int y=d->ValuePos.y1; y<=d->ValuePos.y2; y++)
				{
					for (int x=d->ValuePos.x1; x<=d->ValuePos.x2; x++)
					{
						if ( (x&1) ^ (y&1) )
						{
							pDC->Set(x, y);
						}
					}
				}
			}
			else if (d->Val)
			{
				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y1+1, d->ValuePos.x2-1, d->ValuePos.y2-1);
				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y1+2, d->ValuePos.x2-2, d->ValuePos.y2-1);
				pDC->Line(d->ValuePos.x1+2, d->ValuePos.y1+1, d->ValuePos.x2-1, d->ValuePos.y2-2);

				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y2-1, d->ValuePos.x2-1, d->ValuePos.y1+1);
				pDC->Line(d->ValuePos.x1+1, d->ValuePos.y2-2, d->ValuePos.x2-2, d->ValuePos.y1+1);
				pDC->Line(d->ValuePos.x1+2, d->ValuePos.y2-1, d->ValuePos.x2-1, d->ValuePos.y1+2);
			}
		
		#endif
	}
}

