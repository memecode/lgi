/// \file
/// \author Matthew Allen
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Popup.h"
#include "lgi/common/MonthView.h"
#include "lgi/common/List.h"
#include "lgi/common/Edit.h"
#include "lgi/common/DateTimeCtrls.h"

/// This class is the list of times in a popup, it is used by LTimeDropDown
LTimeDropDown::LTimeDropDown() :
	LDropDown(-1, 0, 0, 10, 10, 0),
	ResObject(Res_Custom)
{
	SetPopup(Drop = new LTimePopup(this));
}

int LTimeDropDown::OnNotify(LViewI *Ctrl, LNotification n)
{
	LViewI *DateSrc = GetNotify();
	if (Ctrl == (LViewI*)Drop && DateSrc)
	{
		auto a = Drop->GetTime();
		
		LDateTime ts;
		const char *str = DateSrc->Name();
		ts.Set(str);
		ts.SetTime(a);
		
		DateSrc->Name(ts.Get());
		return true;
	}
	
	return LDropDown::OnNotify(Ctrl, n);
}

void LTimeDropDown::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	if (Wnd == (LViewI*)Drop && !Attaching)
	{
		Drop = NULL;
	}	
}

bool LTimeDropDown::OnLayout(LViewLayoutInfo &Inf)
{
    if (!Inf.Width.Max)
    {
        Inf.Width.Min = Inf.Width.Max = 20;
    }
    else if (!Inf.Height.Max)
    {
        Inf.Height.Min = Inf.Height.Max = LSysFont->GetHeight() + 6;
    }
    else return false;
    
    return true;
}

void LTimeDropDown::SetDate(char *d)
{
	LViewI *n = GetNotify();
	if (n && d)
	{
		LDateTime New;
		New.SetNow();
		const char *Old = n->Name();
		if (ValidStr(Old))
		{
			New.Set(Old);
		}
		else
		{
			Old = n->Name();
			if (ValidStr(Old))
			{
				New.Set(Old);
			}
		}
		New.SetTime(d);
			
		char Buf[256];
		New.Get(Buf, sizeof(Buf));

		n->Name(Buf);

		LViewI *Nn = n->GetNotify() ? n->GetNotify() : n->GetParent();
		if (Nn)
			Nn->OnNotify(n, LNotifyValueChanged);
	}
}

void LTimeDropDown::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		if (Drop)
		{
			// Set it's time from our current value
			LViewI *n = GetNotify();
			if (n)
			{
				LDateTime New;
				const char *Old = n->Name();
				if (Old &&
					New.Set(Old))
				{
					Drop->SetTime(&New);
				}
			}
		}
	}

	LDropDown::OnMouseClick(m);
}

class TimeList : public LList
{
	LView *Owner;

public:
	bool Key, Mouse;
	
	TimeList(LView *owner) : LList(100, 1, 1, 50, 50, "TimeList")
	{
		Owner = owner;
		Key = Mouse = false;
	}
	
	bool OnKey(LKey &k)
	{
		bool b = false;
		
		if (k.vkey == LK_UP ||
			k.vkey == LK_DOWN ||
			k.vkey == LK_ESCAPE ||
			k.vkey == LK_RETURN)
		{
			Key = true;
			LList::OnKey(k);
			b = true;
			Key = false;
		}

		if (!b && Owner)
			b = Owner->OnKey(k);

		return b;
	}

	void OnMouseClick(LMouse &m)
	{
		Mouse = true;
		LList::OnMouseClick(m);
		Mouse = false;
	}
};

LTimePopup::LTimePopup(LView *owner) : LPopup(owner)
{
	SetParent(owner);
	SetNotify(owner);
	Owner = owner;
	Ignore = true;
	
	Children.Insert(Times = new TimeList(owner));
	if (Times)
	{
		Times->Sunken(false);
		Times->ShowColumnHeader(false);
		Times->AddColumn("");
		Times->MultiSelect(false);

		// Build the list of times...
		LDateTime Dt;
		for (int t=0; t<24; t++)
		{
			int Time;
			
			// Be sensitive to the 24-hr setting...
			if (Dt.GetFormat() & GDTF_24HOUR)
			{
				Time = t;
			}
			else
			{
				Time = t % 12 > 0 ? t % 12 : 12;
			}

			char s[256];
			
			LListItem *i = new LListItem;
			if (i)
			{
				if (Dt.GetFormat() & GDTF_24HOUR)
				{
					sprintf_s(s, sizeof(s), "%i:00", Time);
				}
				else
				{
					sprintf_s(s, sizeof(s), "%i:00 %c", Time, t < 12 ? 'a' : 'p');
				}
				i->SetText(s);
				Times->Insert(i);
			}
			i = new LListItem;
			if (i)
			{
				if (Dt.GetFormat() & GDTF_24HOUR)
				{
					sprintf_s(s, sizeof(s), "%i:30", Time);
				}
				else
				{
					sprintf_s(s, sizeof(s), "%i:30 %c", Time, t < 12 ? 'a' : 'p');
				}
				i->SetText(s);
				Times->Insert(i);
			}
		}
	}

	LDateTime Now;
	if (owner)
		Now.SetTime(owner->Name());
	else
		Now.SetNow();
	SetTime(&Now);

	LRect r(0, 0, 100, 10*16 + 1);
	SetPos(r);
}

LTimePopup::~LTimePopup()
{
	if (Owner)
	{
		Owner->OnChildrenChanged(this, false);
		Owner->Invalidate();
	}
}

void LTimePopup::OnCreate()
{
	Times->Attach(this);
}

void LTimePopup::OnPaint(LSurface *pDC)
{
	// 1px black border
	LRect r = GetClient();
	r.Offset(-r.x1, -r.y1);
	pDC->Colour(L_TEXT);
	pDC->Box(&r);
	r.Inset(1, 1);

	// Client
	if (Times)
	{
		Ignore = true;
		Times->SetPos(r);
		Times->Focus(true);
		Ignore = false;
	}
}

int LTimePopup::OnNotify(LViewI *c, LNotification n)
{
	if (c->GetId() == 100 && !Ignore)
	{
		if 	(n.Type == LNotifyItemSelect || n.Type == LNotifyReturnKey)
		{
			LListItem *Sel = Times->GetSelected();
			if (Sel)
			{
				const char *t = Sel->GetText(0);
				if (t)
				{
					LViewI *v = GetNotify();
					if (v)
					{
						v->Name(t);
						v->OnNotify(this, LNotifyValueChanged);
						
						if (Times->Mouse || n.Type == LNotifyReturnKey)
						{
							v->Focus(true);
							Visible(false);
						}
					}
				}
			}
		}
		else if (n.Type == LNotifyEscapeKey)
		{
			LViewI *v = GetNotify();
			if (v)
				v->Focus(true);			
			Visible(false);
		}
	}

	return 0;
}

LString LTimePopup::GetTime()
{
	if (Times)
	{
		LListItem *s = Times->GetSelected();
		if (s)
			return s->GetText(0);
	}
	return LString();
}

void LTimePopup::SetTime(LDateTime *t)
{
	if (!t || !Times)
		return;

	for (auto i : *Times)
	{
		const char *s = i->GetText(0);
		if (!s)
			continue;

		LDateTime p;
		p.SetTime(s);

		if (p.Hours() != t->Hours())
			continue;

		if (	(p.Minutes() < 30 && t->Minutes() < 30) ||
				(p.Minutes() >= 30 && t->Minutes() >= 30) )
		{
			Ignore = true;
			i->Select(true);
			i->ScrollTo();
			Ignore = false;
			break;
		}
	}
}

class LTimePopupFactory : public LViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (Class &&
			_stricmp(Class, "LTimeDropDown") == 0)
		{
			return new LTimeDropDown;
		}

		return 0;
	}
}	TimePopupFactory;
