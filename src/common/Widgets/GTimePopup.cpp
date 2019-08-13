/// \file
/// \author Matthew Allen
#include <stdio.h>

#include "Lgi.h"
#include "GPopup.h"
#include "MonthView.h"
#include "GToken.h"
#include "LList.h"
#include "GEdit.h"
#include "GDateTimeCtrls.h"

/// This class is the list of times in a popup, it is used by GTimeDropDown
GTimeDropDown::GTimeDropDown() :
	GDropDown(-1, 0, 0, 10, 10, 0),
	ResObject(Res_Custom)
{
	SetPopup(Drop = new GTimePopup(this));
}

int GTimeDropDown::OnNotify(GViewI *Ctrl, int Flags)
{
	GViewI *DateSrc = GetNotify();
	if (Ctrl == (GViewI*)Drop && DateSrc)
	{
		auto a = Drop->GetTime();
		
		LDateTime ts;
		char *str = DateSrc->Name();
		ts.Set(str);
		ts.SetTime(a);
		
		DateSrc->Name(ts.Get());
		return true;
	}
	
	return GDropDown::OnNotify(Ctrl, Flags);
}

void GTimeDropDown::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (Wnd == (GViewI*)Drop && !Attaching)
	{
		Drop = NULL;
	}	
}

bool GTimeDropDown::OnLayout(GViewLayoutInfo &Inf)
{
    if (!Inf.Width.Max)
    {
        Inf.Width.Min = Inf.Width.Max = 20;
    }
    else if (!Inf.Height.Max)
    {
        Inf.Height.Min = Inf.Height.Max = SysFont->GetHeight() + 6;
    }
    else return false;
    
    return true;
}

void GTimeDropDown::SetDate(char *d)
{
	GViewI *n = GetNotify();
	if (n && d)
	{
		LDateTime New;
		New.SetNow();
		char *Old = n->Name();
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

		GViewI *Nn = n->GetNotify() ? n->GetNotify() : n->GetParent();
		if (Nn)
		{
			Nn->OnNotify(n, 0);
		}
	}
}

void GTimeDropDown::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Drop)
		{
			// Set it's time from our current value
			GViewI *n = GetNotify();
			if (n)
			{
				LDateTime New;
				char *Old = n->Name();
				if (Old &&
					New.Set(Old))
				{
					Drop->SetTime(&New);
				}
			}
		}
	}

	GDropDown::OnMouseClick(m);
}

class TimeList : public LList
{
	GView *Owner;

public:
	bool Key, Mouse;
	
	TimeList(GView *owner) : LList(100, 1, 1, 50, 50, "TimeList")
	{
		Owner = owner;
		Key = Mouse = false;
	}
	
	bool OnKey(GKey &k)
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

	void OnMouseClick(GMouse &m)
	{
		Mouse = true;
		LList::OnMouseClick(m);
		Mouse = false;
	}
};

GTimePopup::GTimePopup(GView *owner) : GPopup(owner)
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

	GRect r(0, 0, 100, 10*16 + 1);
	SetPos(r);
}

GTimePopup::~GTimePopup()
{
	if (Owner)
	{
		Owner->OnChildrenChanged(this, false);
		Owner->Invalidate();
	}
}

void GTimePopup::OnCreate()
{
	Times->Attach(this);
}

void GTimePopup::OnPaint(GSurface *pDC)
{
	// 1px black border
	GRect r = GetClient();
	r.Offset(-r.x1, -r.y1);
	pDC->Colour(LC_TEXT, 24);
	pDC->Box(&r);
	r.Size(1, 1);

	// Client
	if (Times)
	{
		Ignore = true;
		Times->SetPos(r);
		Times->Focus(true);
		Ignore = false;
	}
}

int GTimePopup::OnNotify(GViewI *c, int f)
{
	if (c->GetId() == 100 && !Ignore)
	{
		GNotifyType Type = (GNotifyType)f;
		if 	(Type == GNotifyItem_Select || Type == GNotify_ReturnKey)
		{
			LListItem *Sel = Times->GetSelected();
			if (Sel)
			{
				const char *t = Sel->GetText(0);
				if (t)
				{
					GViewI *n = GetNotify();
					if (n)
					{
						n->Name(t);
						n->OnNotify(this, GNotifyValueChanged);
						
						if (Times->Mouse || Type == GNotify_ReturnKey)
						{
							n->Focus(true);
							Visible(false);
						}
					}
				}
			}
		}
		else if (Type == GNotify_EscapeKey)
		{
			GViewI *n = GetNotify();
			if (n)
				n->Focus(true);			
			Visible(false);
		}
	}

	return 0;
}

GString GTimePopup::GetTime()
{
	if (Times)
	{
		LListItem *s = Times->GetSelected();
		if (s)
			return s->GetText(0);
	}
	return GString();
}

void GTimePopup::SetTime(LDateTime *t)
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

class GTimePopupFactory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (Class &&
			_stricmp(Class, "GTimeDropDown") == 0)
		{
			return new GTimeDropDown;
		}

		return 0;
	}
}
	TimePopupFactory;
