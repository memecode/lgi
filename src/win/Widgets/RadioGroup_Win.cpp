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
	int64 InitVal = -1;
	int MaxLayoutWidth = 0;
    LHashTbl<PtrKey<void*>,LViewLayoutInfo*> Info;

	LRadioGroupPrivate()
	{
	}

	~LRadioGroupPrivate()
	{
        Info.DeleteObjects();
	}
};

int LRadioGroupPrivate::NextId = 10000;

LRadioGroup::LRadioGroup(int id, const char *name, int64_t Init)
	: ResObject(Res_Group)
{
	d = new LRadioGroupPrivate;
	if (Init >= 0)
		d->InitVal = Init;

	Name(name);
	SetId(id);
	SetTabStop(true);
	SetStyle(GetStyle() | BS_GROUPBOX | WS_GROUP | WS_CLIPCHILDREN);

	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = LWindowsClass::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("BUTTON");
	else
		LAssert(!"No subclass?");
}

LRadioGroup::~LRadioGroup()
{
	DeleteObj(d);
}

void LRadioGroup::OnAttach()
{
}

static void Collect(LArray<LRadioButton*> &btns, LViewI *v)
{
	for (auto c: v->IterateViews())
	{
		if (auto btn = dynamic_cast<LRadioButton*>(c))
			btns.Add(btn);
		Collect(btns, c);
	}
};

LArray<LRadioButton*> LRadioGroup::CollectButtons()
{
	LArray<LRadioButton*> btns;	
	Collect(btns, this);
	return btns;
}

LMessage::Result LRadioGroup::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_ERASEBKGND:
		{
			LScreenDC Dc((HDC)Msg->A(), _View);
			LColour cBack = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
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
	SetFont(LSysFont);
	AttachChildren();

	if (d->InitVal >= 0)
		Value(d->InitVal);
}

int64 LRadioGroup::Value()
{
	if (IsAttached())
	{
		int n = 0;
		for (auto c: CollectButtons())
		{
			if (c->Value())
				return n;
			n++;
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
	if (IsAttached())
	{
		int n = 0;
		for (auto c: CollectButtons())
		{
			c->Value(n == Which);
			n++;
		}
	}
	else
	{
		d->InitVal = Which;
	}
}

LRadioButton *LRadioGroup::Selected(const char *newSelection)
{
	for (auto c: CollectButtons())
	{
		if (newSelection)
		{
			if (!Stricmp(newSelection, c->Name()))
			{
				c->Value(true);
				return c;
			}
		}
		else if (c->Value())
		{
			return c;
		}
	}

	return nullptr;
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
}

LRadioButton *LRadioGroup::Append(const char *name)
{
	auto But = new LRadioButton(d->NextId++, name);
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
	DWORD ParentProc = 0;
	LArray<int> GroupIDs;
	int64 InitVal = -1;
};

LRadioButton::LRadioButton(int id, const char *name)
	: ResObject(Res_RadioBox)
{
	d = new LRadioButtonPrivate;

	Name(name);

	LDisplayString t(LSysFont, name);
	auto cx = t.X() + 26;
	auto cy = t.Y() + 4;

	LRect r(0, 0, cx-1, cy-1);
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
		LAssert(!"No subclass?");
}

LRadioButton::~LRadioButton()
{
	DeleteObj(d);
}

void LRadioButton::OnClick(const LMouse &m)
{
	if (onClickFn)
		onClickFn(m);
	else
		Value(true);
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
	SetFont(LSysFont);

	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();

	if (d->InitVal > 0)
	{
		auto nm = Name();
 		Value(d->InitVal);
	}
}

void LRadioButton::OnStyleChange()
{
}

LMessage::Result LRadioButton::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case WM_DESTROY:
		{
			// Save the value for anyone needing it.
			d->InitVal = Value();
			break;
		}
	}

	return LControl::OnEvent(m);
}

int LRadioButton::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == BN_CLICKED &&
		GetParent())
	{
		LMouse ms;
		GetMouse(ms);
		OnClick(ms);
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
	if (Handle())
	{
		if (i)
		{
			// Turn off any other radio buttons in the current group
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
				for (auto c: GetParent()->IterateViews())
				{
					auto b = dynamic_cast<LRadioButton*>(c);
					if (b &&
						b != this &&
						b-Value())
					{
						auto nm = b->Name();
						b->Value(false);
					}
				}
			}
		}

		// Change the value of this button
		SendMessage(Handle(), BM_SETCHECK, d->InitVal = i, 0);

		if (i)
		{
			// If gaining selection, tell the parent
			SendNotify(LNotifyValueChanged);
		}
	}
	else
	{
		d->InitVal = i;
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

int LRadioButton::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	if (Ctrl == (LViewI*)this && n.Type == LNotifyActivate)
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