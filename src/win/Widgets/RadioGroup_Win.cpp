// \file
// \author Matthew Allen
// \brief Native Win32 Radio Group and Button
#if !XP_BUTTON

#include <stdlib.h>
#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Css.h"
#include "lgi/common/LgiRes.h"

#define RADIO_GRID  2
static int PadXPx = 30;
#ifdef MAC
static int PadYPx = 6;
#else
static int PadYPx = 4;
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// Radio group
class LRadioGroupPrivate
{
public:
	static int NextId;
	int64 InitVal;
	int MaxLayoutWidth;
    LHashTbl<PtrKey<void*>,LViewLayoutInfo*> Info;

	LRadioGroupPrivate()
	{
		InitVal = 0;
		MaxLayoutWidth = 0;
	}

	~LRadioGroupPrivate()
	{
        Info.DeleteObjects();
	}
};

int LRadioGroupPrivate::NextId = 10000;

LRadioGroup::LRadioGroup(int id, int x, int y, int cx, int cy, const char *name, int Init)
	: ResObject(Res_Group)
{
	d = new LRadioGroupPrivate;
	d->InitVal = Init;

	Name(name);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_GROUPBOX | WS_GROUP | WS_CLIPCHILDREN);

	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = LWindowsClass::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LgiAssert(!"No subclass?");
}

LRadioGroup::~LRadioGroup()
{
	DeleteObj(d);
}

void LRadioGroup::OnAttach()
{
}

GMessage::Result LRadioGroup::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_ERASEBKGND:
		{
			LScreenDC Dc((HDC)Msg->A(), _View);
			GColour cBack = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
			Dc.Colour(cBack);
			Dc.Rectangle();
			return true;
			break;
		}
   		case WM_DESTROY:
		{
			d->InitVal = Value();
			break;
		}
	}

	return LControl::OnEvent(Msg);
}

bool LRadioGroup::Name(const char *n)
{
	return LView::Name(n);
}

bool LRadioGroup::NameW(const char16 *n)
{
	return LView::NameW(n);
}

void LRadioGroup::SetFont(LFont *Fnt, bool OwnIt)
{
	LView::SetFont(Fnt);
}

void LRadioGroup::OnCreate()
{
	SetFont(SysFont);
	AttachChildren();
	Value(d->InitVal);
}

int64 LRadioGroup::Value()
{
	if (Handle())
	{
		int n = 0;
		for (auto c: Children)
		{
			LRadioButton *Check = dynamic_cast<LRadioButton*>(c);
			if (Check)
			{
				if (Check->Value())
					return n;
				n++;
			}
		}

		return -1;
	}
	else
	{
		return d->InitVal;
	}
}

void LRadioGroup::Value(int64 Which)
{
	if (Handle())
	{
		int n = 0;
		for (auto c: Children)
		{
			LRadioButton *Check = dynamic_cast<LRadioButton*>(c);
			if (Check)
			{
				Check->Value(n == Which);
				n++;
			}
		}
	}
	else
	{
		d->InitVal = Which;
	}
}

int LRadioGroup::OnNotify(LViewI *Ctrl, int Flags)
{
	LViewI *n = GetNotify() ? GetNotify() : GetParent();
	if (n)
	{
		if (dynamic_cast<LRadioButton*>(Ctrl))
		{
			return n->OnNotify(this, Flags);
		}
		else
		{
			return n->OnNotify(Ctrl, Flags);
		}
	}
	return 0;
}

void LRadioGroup::OnPaint(LSurface *pDC)
{
}

LRadioButton *LRadioGroup::Append(int x, int y, const char *name)
{
	LRadioButton *But = new LRadioButton(d->NextId++, x, y, -1, -1, name);
	if (But)
	{
		Children.Insert(But);
	}

	return But;
}

bool LRadioGroup::OnLayout(LViewLayoutInfo &Inf)
{
    auto it = IterateViews();
    const int BORDER_PX = 2;
    LDisplayString Txt(GetFont(), Name());
    if (!Inf.Width.Max)
    {
        // Work out the width...
		int TextPx = Txt.X();
		int MinPx = (RADIO_GRID + BORDER_PX) * 2;
		
        d->Info.DeleteObjects();
        Inf.Width.Min = 16 + TextPx;
        Inf.Width.Max = RADIO_GRID + BORDER_PX * 2;
	    for (LViewI *w: it)
	    {
	        LAutoPtr<LViewLayoutInfo> c(new LViewLayoutInfo);
            if (w->OnLayout(*c))
            {
                // Layout enabled control
                Inf.Width.Min = max(Inf.Width.Min, c->Width.Min + MinPx);
                Inf.Width.Max += c->Width.Max + RADIO_GRID;
                d->Info.Add(w, c.Release());
            }
            else
            {
                // Non layout enabled control
                Inf.Width.Min = max(Inf.Width.Min, w->X() + (RADIO_GRID << 1));
                Inf.Width.Max += w->X() + RADIO_GRID;
            }
	    }
	    
		if (Inf.Width.Max < Inf.Width.Min)
			Inf.Width.Max = Inf.Width.Min;
		d->MaxLayoutWidth = Inf.Width.Max;
    }
    else
    {
        // Working out the height, and positioning the controls
        Inf.Height.Min = Txt.Y();
        
        bool Horiz = d->MaxLayoutWidth <= Inf.Width.Max;
        int Cx = BORDER_PX + RADIO_GRID, Cy = Txt.Y();
        int LastY = 0;
	    for (LViewI *w: it)
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
                    LastY = max(LastY, r.y2);
                }
                else LgiAssert(!"This shouldn't fail.");
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
                LastY = max(LastY, r.y2);
            }
	    }
	    
	    Inf.Height.Min = Inf.Height.Max = LastY + RADIO_GRID * 2 + BORDER_PX;
    }
    
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Radio button
class LRadioButtonPrivate
{
public:
	DWORD ParentProc;
	int64 InitVal;

	LRadioButtonPrivate()
	{
		ParentProc = 0;
		InitVal = 0;
	}

	~LRadioButtonPrivate()
	{
	}
};

LRadioButton::LRadioButton(int id, int x, int y, int cx, int cy, const char *name)
	: ResObject(Res_RadioBox)
{
	d = new LRadioButtonPrivate;

	Name(name);

	LDisplayString t(SysFont, name);
	if (cx < 0) cx = t.X() + 26;
	if (cy < 0) cy = t.Y() + 4;

	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_AUTORADIOBUTTON);

	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = LWindowsClass::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LgiAssert(!"No subclass?");
}

LRadioButton::~LRadioButton()
{
	DeleteObj(d);
}

void LRadioButton::OnAttach()
{
	SetFont(SysFont);

	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();

	Value(d->InitVal);
}

void LRadioButton::OnStyleChange()
{
}
int LRadioButton::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED &&
		GetParent())
	{
		Value(true);
	}
	
	return 0;
}
bool LRadioButton::Name(const char *n)
{
	return LView::Name(n);
}

bool LRadioButton::NameW(const char16 *n)
{
	return LView::NameW(n);
}

int64 LRadioButton::Value()
{
	if (Handle())
		return SendMessage(Handle(), BM_GETCHECK, 0, 0);

	return d->InitVal;
}

void LRadioButton::Value(int64 i)
{
	static bool InValue = false;
	if (!InValue)
	{
		InValue = true;
		if (Handle())
		{
			if (i)
			{
				// Turn off any other radio buttons in the current group
				for (LViewI *c: GetParent()->IterateViews())
				{
					LRadioButton *b = dynamic_cast<LRadioButton*>(c);
					if (b &&
						b != this &&
						b-Value())
					{
						b->Value(false);
					}
				}
			}

			// Change the value of this button
			SendMessage(Handle(), BM_SETCHECK, i, 0);

			if (i)
			{
				// If gaining selection, tell the parent
				SendNotify(true);
			}
		}
		else
		{
			d->InitVal = i;
		}
		InValue = false;
	}
}

bool LRadioButton::OnLayout(LViewLayoutInfo &Inf)
{
	LDisplayString Txt(GetFont(), Name());
    if (!Inf.Width.Max)
    {
        Inf.Width.Min =
            Inf.Width.Max =
            Txt.X() + PadXPx;
    }
    else
    {
		int y = Txt.Y() + PadYPx;
        Inf.Height.Min = max(Inf.Height.Min, y);
        Inf.Height.Max = max(Inf.Height.Max, y);
    }
	
    return true;    
}
int LRadioButton::OnNotify(LViewI *Ctrl, int Flags)
{
	if (Ctrl == (LViewI*)this && Flags == GNotify_Activate)
	{
		Value(true);
	}
	return 0;
}
bool LRadioButton::OnKey(LKey &k)
{
	bool Status = false;
	int Move = 0;
	switch (k.vkey)
	{
		case VK_UP:
		case VK_LEFT:
		{
			if (k.Down())
			{
				Move = -1;
			}
			Status = true;
			break;
		}
		case VK_RIGHT:
		case VK_DOWN:
		{
			if (k.Down())
			{
				Move = 1;
			}
			Status = true;
			break;
		}
        case ' ':
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
		auto L = GetParent()->IterateViews();
		for (LViewI *c: L)
		{
			LRadioButton *b = dynamic_cast<LRadioButton*>(c);
			if (b) Btns.Insert(b);
		}
		if (Btns.Length() > 1)
		{
			auto Index = Btns.IndexOf(this);
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
#endif