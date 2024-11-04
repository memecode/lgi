#if !defined(_WIN32) || (XP_BUTTON != 0)

#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/StringLayout.h"

#define RADIO_GRID	4

///////////////////////////////////////////////////////////////////////////////////////////
// Radio group
static int MinYSize = 16;

class LRadioGroupPrivate : public LMutex, public LStringLayout
{
	LRadioGroup *Ctrl;
	
public:
	static int NextId;
	int Val;
	int MaxLayoutWidth;
	LHashTbl<PtrKey<LViewI*>,LViewLayoutInfo*> Info;

	LRadioGroupPrivate(LRadioGroup *g) :
		LMutex("LRadioGroupPrivate"),
		LStringLayout(LAppInst->GetFontCache())
	{
		Ctrl = g;
		Val = 0;
		MaxLayoutWidth = 0;
		AmpersandToUnderline = true;
		SetFontCache(LAppInst->GetFontCache());
	}

	~LRadioGroupPrivate()
	{
		Info.DeleteObjects();
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

int LRadioGroupPrivate::NextId = 10000;
LRadioGroup::LRadioGroup(int id, const char *name, int Init)
	: ResObject(Res_Group)
{
	d = new LRadioGroupPrivate(this);
	Name(name);
	SetId(id);
	d->Val = Init;
	LResources::StyleElement(this);
}

LRadioGroup::~LRadioGroup()
{
	DeleteObj(d);
}

void LRadioGroup::OnStyleChange()
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

bool LRadioGroup::OnLayout(LViewLayoutInfo &Inf)
{
	auto children = IterateViews();
	const int BORDER_PX = 2;
	int MinPx = (RADIO_GRID + BORDER_PX) * 2;

	if (!Inf.Width.Max)
	{
		// Work out the width...

		d->PreLayout(Inf.Width.Min, Inf.Width.Max);
		Inf.Width.Min += MinPx;
		Inf.Width.Max += MinPx;
		
		d->Info.DeleteObjects();
		// Inf.Width.Min = 16 + TextPx;
		// Inf.Width.Max = RADIO_GRID + BORDER_PX * 2;
		
		for (LViewI *w: children)
		{
			LAutoPtr<LViewLayoutInfo> c(new LViewLayoutInfo);
			if (w->OnLayout(*c))
			{
				// Layout enabled control
				Inf.Width.Min = MAX(Inf.Width.Min, c->Width.Min + MinPx);
				Inf.Width.Max += c->Width.Max + RADIO_GRID;
				d->Info.Add(w, c.Release());
			}
			else
			{
				// Non layout enabled control
				Inf.Width.Min = MAX(Inf.Width.Min, w->X() + (RADIO_GRID << 1));
				Inf.Width.Max += w->X() + RADIO_GRID;
			}
		}
		
		if (Inf.Width.Max < Inf.Width.Min)
			Inf.Width.Max = Inf.Width.Min;
		
		d->MaxLayoutWidth = Inf.Width.Max;
	}
	else
	{
		d->Layout(Inf.Width.Max);
		Inf.Height.Min = d->GetMin().y + MinPx;
		Inf.Height.Max = d->GetMax().y + MinPx;
		
		// Working out the height, and positioning the controls
		// Inf.Height.Min = d->Txt ? d->Txt->Y() : 16;
		
		bool Horiz = d->MaxLayoutWidth <= Inf.Width.Max;
		int Cx = BORDER_PX + RADIO_GRID, Cy = d->GetMin().y;
		int LastY = 0;
		for (LViewI *w: children)
		{
			LViewLayoutInfo *c = d->Info.Find(w);
			if (c)
			{
				if (w->OnLayout(*c))
				{
					LRect r(Cx, Cy, Cx + c->Width.Max - 1, Cy + c->Height.Max - 1);
					w->SetPos(r);

					if (Horiz)
						// Horizontal layout
						Cx += r.X() + RADIO_GRID;
					else
						// Vertical layout
						Cy += r.Y() + RADIO_GRID;

					LastY = MAX(LastY, r.y2);
				}
				else LAssert(!"This shouldn't fail.");
			}
			else
			{
				// Non layout control... just use existing size
				LRect r = w->GetPos();
				r.Offset(Cx - r.x1, Cy - r.y1);
				w->SetPos(r);

				if (Horiz)
					// Horizontal layout
					Cx += r.X() + RADIO_GRID;
				else
					// Vertical layout
					Cy += r.Y() + RADIO_GRID;

				LastY = MAX(LastY, r.y2);
			}
		}
		
		Inf.Height.Min = Inf.Height.Max = LastY + RADIO_GRID * 2 + BORDER_PX;
	}
	
	return true;
}

void LRadioGroup::OnAttach()
{
	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();
}

LMessage::Result LRadioGroup::OnEvent(LMessage *m)
{
	return LView::OnEvent(m);
}

bool LRadioGroup::Name(const char *n)
{
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = LView::Name(n);

		d->Empty();
		d->Add(n, GetCss());
		d->SetBaseFont(GetFont());
		d->DoLayout(X());
		
		d->Unlock();
	}

	return Status;
}

bool LRadioGroup::NameW(const char16 *n)
{
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = LView::NameW(n);

		d->Empty();
		d->Add(LBase::Name(), GetCss());
		d->SetBaseFont(GetFont());
		d->DoLayout(X());

		d->Unlock();
	}

	return Status;
}

void LRadioGroup::SetFont(LFont *Fnt, bool OwnIt)
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

void LRadioGroup::OnCreate()
{
	AttachChildren();
	Value(d->Val);
}

int64 LRadioGroup::Value()
{
	int i=0;
	for (auto w: Children)
	{
		LRadioButton *But = dynamic_cast<LRadioButton*>(w);
		if (But)
		{
			if (But->Value())
			{
				d->Val = i;
				break;
			}
			i++;
		}
	}

	return d->Val;
}

void LRadioGroup::Value(int64 Which)
{
	d->Val = (int)Which;

	int i=0;
	for (auto w: Children)
	{
		LRadioButton *But = dynamic_cast<LRadioButton*>(w);
		if (But)
		{
			if (i == Which)
			{
				But->Value(true);
				break;
			}
			i++;
		}
	}
}

int LRadioGroup::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	LViewI *v = GetNotify() ? GetNotify() : GetParent();
	if (v)
	{
		if (dynamic_cast<LRadioButton*>(Ctrl))
		{
			return v->OnNotify(this, n);
		}
		else
		{
			return v->OnNotify(Ctrl, n);
		}
	}
	return 0;
}

void LRadioGroup::OnPaint(LSurface *pDC)
{
	if (LApp::SkinEngine &&
		TestFlag(LApp::SkinEngine->GetFeatures(), GSKIN_GROUP))
	{
		LSkinState State;
		State.pScreen = pDC;
		State.MouseOver = false;
		State.aText = d->GetStrs();
		LApp::SkinEngine->OnPaint_LRadioGroup(this, &State);
	}
	else
	{
		// LColour Fore = StyleColour(LCss::PropColor, LC_TEXT);
		LColour Back = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
		if (!Back.IsTransparent())
		{
			pDC->Colour(Back);
			pDC->Rectangle();
		}

		int y = d->GetMin().y;
		LRect b(0, y/2, X()-1, Y()-1);
		LWideBorder(pDC, b, EdgeXpChisel);

		LPoint TxtPt(6, 0);
		LRect TxtRc = d->GetBounds();
		TxtRc.Offset(TxtPt.x, TxtPt.y);		
		d->Paint(pDC, TxtPt, Back, TxtRc, Enabled(), false);
	}
}

LRadioButton *LRadioGroup::Append(const char *name)
{
	if (auto But = new LRadioButton(d->NextId++, name))
	{
		Children.Insert(But);
		return But;
	}
	
	return NULL;
}

LRadioButton *LRadioGroup::Selected()
{
	for (auto w: Children)
	{
		if (auto btn = dynamic_cast<LRadioButton*>(w))
		{
			if (btn->Value())
				return btn;
		}
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Radio button
class LRadioButtonPrivate : public LMutex, public LStringLayout
{
public:
	LRadioButton *Ctrl;
	bool Val;
	bool Over;
	LRect Btn;
	LColour BackCol;
	LArray<int> GroupIDs;

	LRadioButtonPrivate(LRadioButton *c) :
		LMutex("LRadioButtonPrivate"),
		LStringLayout(LAppInst->GetFontCache())
	{
		Btn.ZOff(-1,-1);
		Ctrl = c;
		Val = 0;
		Over = 0;
		AmpersandToUnderline = true;
		SetFontCache(LAppInst->GetFontCache());
	}

	~LRadioButtonPrivate()
	{
	}

	LRect GetBtn(int BoxPx)
	{
		auto Fnt = Ctrl->GetFont();
		int Px = (int) (Fnt->Ascent() + 0.5);
		if (BoxPx > 0 && Px > BoxPx)
			Px = BoxPx;

		Btn.ZOff(Px-1, Px-1);
		if (BoxPx > 0)
			Btn.Offset(0, (BoxPx-Btn.Y())>>1);

		return Btn;
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
			DoLayout(Px);
			Unlock();
			/*
			if (Min.y < MinYSize)
				Min.y = MinYSize;
			if (Max.y < MinYSize)
				Max.y = MinYSize;
				*/
		}
		else return false;
		return true;
	}
};

static int PadXPx = 24; // 13px for circle, 11px padding to text.
#ifdef MAC
static int PadYPx = 6;
#else
static int PadYPx = 4;
#endif

LRadioButton::LRadioButton(int id, const char *name)
	: ResObject(Res_RadioBox)
{
	d = new LRadioButtonPrivate(this);
	Name(name);
	auto cx = d->GetBounds().X() + PadXPx;
	auto cy = d->GetBounds().Y() + PadYPx;

	LRect r(0, 0, cx-1, cy-1);
	SetPos(r);
	SetId(id);
	d->Val = false;
	d->Over = false;
	SetTabStop(true);

	#if WINNATIVE
	SetDlgCode(GetDlgCode() | DLGC_WANTARROWS);
	#endif
}

LRadioButton::~LRadioButton()
{
	DeleteObj(d);
}

bool LRadioButton::SetGroup(LArray<int> CtrlIds)
{
	auto w = GetWindow();
	if (!w)
		return false;

	// This ctrl should be in the ID list.
	auto id = GetId();
	if (!CtrlIds.HasItem(id))
		CtrlIds.Add(id);

	for (auto i: CtrlIds)
	{
		LRadioButton *button;
		if (!w->GetViewById(i, button))
			return false;

		button->d->GroupIDs = CtrlIds;
	}

	return true;
}

void LRadioButton::OnAttach()
{
	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();
}

void LRadioButton::OnStyleChange()
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

bool LRadioButton::Name(const char *n)
{
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = LView::Name(n);

		d->Empty();
		d->Add(n, GetCss());
		d->SetBaseFont(GetFont());
		d->DoLayout(X());

		d->Unlock();
	}

	return Status;
}

bool LRadioButton::NameW(const char16 *n)
{
	bool Status = false;
	if (d->Lock(_FL))
	{
		Status = LView::NameW(n);

		d->Empty();
		d->Add(LBase::Name(), GetCss());
		d->SetBaseFont(GetFont());
		d->DoLayout(X());

		d->Unlock();
	}

	return Status;
}

void LRadioButton::SetFont(LFont *Fnt, bool OwnIt)
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

bool LRadioButton::OnLayout(LViewLayoutInfo &Inf)
{
	auto Btn = d->GetBtn(-1);
	int Pad = Btn.X() + 4;

	if (!Inf.Width.Max)
	{
		d->PreLayout(Inf.Width.Min, Inf.Width.Max);
		
		// FIXME: Wrapping labels not supported yet. So use the max width.
		Inf.Width.Min = Inf.Width.Max;
		
		Inf.Width.Min += Pad;
		Inf.Width.Max += Pad;
	}
	else
	{
		d->Layout(Inf.Width.Max);		
		Inf.Height.Min = Inf.Height.Max = MAX(d->GetMin().y, Btn.Y());
	}
	
	return true;	
}

int LRadioButton::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	if (Ctrl == (LViewI*)this && n.Type == LNotifyActivate)
	{
		Value(true);
	}

	return 0;
}

int64 LRadioButton::Value()
{
	return d->Val;
}

void LRadioButton::Value(int64 i)
{
	if (d->Val != (i != 0))
	{
		if (i)
		{
			// remove the value from the currently selected radio value
			if (d->GroupIDs.Length())
			{
				if (auto w = GetWindow())
				{
					for (auto id: d->GroupIDs)
					{
						if (id == GetId())
							continue;

						LRadioButton *button;
						if (w->GetViewById(id, button))
						{
							if (button->Value())
								button->Value(false);
						}
					}
				}
			}
			else
			{
				// Use the automatic mode... iterate through sibling views.
				if (auto p = GetParent())
				{
					for (auto c: p->IterateViews())
					{
						LRadioButton *b = dynamic_cast<LRadioButton*>(c);
						if (b && b != this && b->d->Val)
						{
							b->d->Val = false;
							b->Invalidate();
						}
					}
				}
			}
		}

		d->Val = i != 0;
		Invalidate();

		if (i)
			SendNotify();
	}
}

void LRadioButton::OnMouseClick(LMouse &m)
{
	if (Enabled())
	{
		bool WasCapturing = IsCapturing();

		if (m.Down()) Focus(true);
		Capture(m.Down());
		d->Over = m.Down();
	
		LRect r(0, 0, X()-1, Y()-1);
		if (!m.Down() &&
			r.Overlap(m.x, m.y) &&
			WasCapturing)
		{
			Value(true);
		}
		else
		{
			Invalidate();
		}
	}
}

void LRadioButton::OnMouseEnter(LMouse &m)
{
	if (Enabled() && IsCapturing())
	{
		d->Over = true;
		Invalidate();
	}
}

void LRadioButton::OnMouseExit(LMouse &m)
{
	if (Enabled() && IsCapturing())
	{
		d->Over = false;
		Invalidate();
	}
}

bool LRadioButton::OnKey(LKey &k)
{
	bool Status = false;
	int Move = 0;

	switch (k.vkey)
	{
		case LK_UP:
		case LK_LEFT:
		{
			if (k.Down())
			{
				Move = -1;
			}
			Status = true;
			break;
		}
		case LK_RIGHT:
		case LK_DOWN:
		{
			if (k.Down())
			{
				Move = 1;
			}
			Status = true;
			break;
		}
		case LK_SPACE:
		{
			if (k.Down())
			{
				Value(1);
			}
			return true;
		}
	}

	if (Move)
	{
		List<LRadioButton> Btns;
		for (LViewI *c: GetParent()->IterateViews())
		{
			LRadioButton *b = dynamic_cast<LRadioButton*>(c);
			if (b) Btns.Insert(b);
		}
		if (Btns.Length() > 1)
		{
			ssize_t Index = Btns.IndexOf(this);
			if (Index >= 0)
			{
				LRadioButton *n = Btns[(Index + Move + Btns.Length()) % Btns.Length()];
				if (n)
				{
					n->Focus(true);
				}
			}
		}
	}

	return Status;
}

void LRadioButton::OnFocus(bool f)
{
	Invalidate();
}

void LRadioButton::OnPaint(LSurface *pDC)
{
	if (LApp::SkinEngine &&
		TestFlag(LApp::SkinEngine->GetFeatures(), GSKIN_RADIO))
	{
		LSkinState State;
		State.pScreen = pDC;
		State.MouseOver = d->Over;
		State.aText = d->GetStrs();
		State.View = this;
		State.Rect = d->GetBtn(Y());

		LColour Back = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
		State.ForceUpdate = d->BackCol != Back;
		d->BackCol = Back;

		LApp::SkinEngine->OnPaint_LRadioButton(this, &State);
	}
	else
	{
		LRect r(0, 0, X()-1, Y()-1);
		LRect c(0, 0, 12, 12);
		// LColour Fore = StyleColour(LCss::PropColor, LC_TEXT, 4);
		LColour Back = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
		
		// bool e = Enabled();
		LRect fill(c.x2 + 1, r.y1, r.x2, r.x2);
		LPoint TxtPt(c.x2 + 11, (r.Y() - d->GetBounds().Y()) >> 1);
		d->Paint(pDC, TxtPt, Back, fill, Enabled(), false);
		
		#if defined LGI_CARBON

		LRect cli = GetClient();
		for (LViewI *v = this; v && !v->Handle(); v = v->GetParent())
		{
			LRect p = v->GetPos();
			cli.Offset(p.x1, p.y1);
		}
		
		pDC->Colour(Back);
		pDC->Rectangle(cli.x1, cli.y1, c.x2, cli.y2);
		
		LRect rc(c.x1, c.y1 + 4, c.x2 - 1, c.y2 - 1);
		HIRect Bounds = rc;
		HIThemeButtonDrawInfo Info;
		HIRect LabelRect;

		Info.version = 0;
		Info.state = d->Val ? kThemeStatePressed : (Enabled() ? kThemeStateActive : kThemeStateInactive);
		Info.kind = kThemeRadioButton;
		Info.value = d->Val ? kThemeButtonOn : kThemeButtonOff;
		Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;

		OSStatus err = HIThemeDrawButton(&Bounds,
										&Info,
										pDC->Handle(),
										kHIThemeOrientationNormal,
										&LabelRect);
		if (err) printf("%s:%i - HIThemeDrawButton failed %li\n", _FL, err);
		
		#else

		// Draw border
		pDC->Colour(L_LOW);
		pDC->Line(c.x1+1, c.y1+9, c.x1+1, c.y1+10);
		pDC->Line(c.x1, c.y1+4, c.x1, c.y1+8);
		pDC->Line(c.x1+1, c.y1+2, c.x1+1, c.y1+3);
		pDC->Line(c.x1+2, c.y1+1, c.x1+3, c.y1+1);
		pDC->Line(c.x1+4, c.y1, c.x1+8, c.y1);
		pDC->Line(c.x1+9, c.y1+1, c.x1+10, c.y1+1);

		pDC->Colour(L_SHADOW);
		pDC->Set(c.x1+2, c.y1+9);
		pDC->Line(c.x1+1, c.y1+4, c.x1+1, c.y1+8);
		pDC->Line(c.x1+2, c.y1+2, c.x1+2, c.y1+3);
		pDC->Set(c.x1+3, c.y1+2);
		pDC->Line(c.x1+4, c.y1+1, c.x1+8, c.y1+1);
		pDC->Set(c.x1+9, c.y1+2);

		pDC->Colour(L_LIGHT);
		pDC->Line(c.x1+11, c.y1+2, c.x1+11, c.y1+3);
		pDC->Line(c.x1+12, c.y1+4, c.x1+12, c.y1+8);
		pDC->Line(c.x1+11, c.y1+9, c.x1+11, c.y1+10);
		pDC->Line(c.x1+9, c.y1+11, c.x1+10, c.y1+11);
		pDC->Line(c.x1+4, c.y1+12, c.x1+8, c.y1+12);
		pDC->Line(c.x1+2, c.y1+11, c.x1+3, c.y1+11);

		/// Draw center
		bool e = Enabled();
		pDC->Colour(d->Over || !e ? L_MED : L_WORKSPACE);
		pDC->Rectangle(c.x1+2, c.y1+4, c.x1+10, c.y1+8);
		pDC->Box(c.x1+3, c.y1+3, c.x1+9, c.y1+9);
		pDC->Box(c.x1+4, c.y1+2, c.x1+8, c.y1+10);

		// Draw value
		if (d->Val)
		{
			pDC->Colour(e ? L_TEXT : L_LOW);
			pDC->Rectangle(c.x1+4, c.y1+5, c.x1+8, c.y1+7);
			pDC->Rectangle(c.x1+5, c.y1+4, c.x1+7, c.y1+8);
		}
		
		#endif
	}
}

#endif
