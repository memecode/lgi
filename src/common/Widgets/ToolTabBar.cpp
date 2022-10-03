/*hdr
**      FILE:           ToolTabBar.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           22/3/2000
**      DESCRIPTION:    Toolbar of tabs
**
**      Copyright (C) 1997-1998 Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/ToolTabBar.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Css.h"

/////////////////////////////////////////////////////////////////////////
LToolTab::LToolTab() : LToolButton(16, 16)
{
	First = true;
	#ifdef WINDOWS
	SetClassW32("LToolTab");
	#endif
	GetCss(true)->BackgroundColor(LColour(L_MED));
}

LToolTab::~LToolTab()
{
}

void LToolTab::OnPaint(LSurface *pDC)
{
	#if 1
	pDC->Colour(L_MED);
	#else
	pDC->Colour(LColour(255, 0, 255));
	#endif
	pDC->Rectangle();

	#if 0
	auto c = GetClient();
	pDC->Colour(LColour::Red);
	pDC->Line(c.x1, c.y1, c.x2, c.y2);
	pDC->Line(c.x2, c.y1, c.x1, c.y2);
	#endif
}

bool LToolTab::SetPos(LRect &r, bool Repaint)
{
	return LToolButton::SetPos(r, Repaint);
}

/////////////////////////////////////////////////////////////////////////
LToolTabBar::LToolTabBar(int Id)
{
	LRect r(0, 0, 100, 100);
	if (Id >= 0)
		SetId(Id);
	SetPos(r);
	Current = 0;
	FitToArea = false;
	Border = true;
	InOnChangeEvent = false;
}

LToolTabBar::~LToolTabBar()
{
}

int64 LToolTabBar::Value()
{
	auto it = IterateViews();
	for (int n=0; n<it.Length(); n++)
	{
		LToolTab *tt = dynamic_cast<LToolTab*>(it[n]);
		if (tt && tt->Value())
			return n;
	}

	return -1;
}

void LToolTabBar::Value(int64 new_value)
{
	auto it = IterateViews();
	for (int n=0; n<it.Length(); n++)
	{
		if (n == new_value)
		{
			LToolButton *b = dynamic_cast<LToolButton*>(it[n]);
			if (b)
			{
				OnChange(b);
				break;
			}
		}	
	}
}

void LToolTabBar::OnButtonClick(LToolButton *Btn)
{
	LToolBar::OnButtonClick(Btn);
	
	LToolTab *b = dynamic_cast<LToolTab*>(Btn);
	if (b && b != Current)
		OnChange(Btn);
}

int LToolTabBar::OnNotify(LViewI *c, LNotification n)
{
	if (n.Type == LNotifyValueChanged)
	{
		LToolTab *b = dynamic_cast<LToolTab*>(c);
		if (b && b != Current)
			OnChange(b);
	}
	
	return LLayout::OnNotify(c, n);
}

bool LToolTabBar::IsOk()
{
	int Count[2] = {0, 0};
	for (LViewI *c: IterateViews())
	{
		LToolTab *tt = dynamic_cast<LToolTab*>(c);
		if (tt && tt->GetId() > 0)
		{
			bool Dn = tt->Value() != 0;
			Count[Dn != 0]++;
		}
	}
	LAssert(Count[1] == 0 || Count[1] == 1);
	return Count[1] == 0 || Count[1] == 1;
}

void LToolTabBar::OnChange(LToolButton *Btn)
{
	 // Current->Value(...) causes recursive events.
	if (InOnChangeEvent)
		return;
	InOnChangeEvent = true;

	// Detach the old button
	if (Current)
	{
		Invalidate(&Current->TabPos);
		Current->Visible(false);
		Current = NULL;
	}

	// Attach new button
	if ((Current = dynamic_cast<LToolTab*>(Btn)))
	{
		Current->Value(true);
		Current->Visible(true);
		OnPosChange();

		if (Current->First)
		{
			Current->OnSelect();
			Current->First = false;
		}

		Invalidate(&Current->TabPos);
	}

	IsOk();
	InOnChangeEvent = false;
}

void LToolTabBar::_PaintTab(LSurface *pDC, LToolTab *Tab)
{
	LRect t = Tab->TabPos;
	LColour CtrlBackground(L_MED);
	LColour TabBackground(L_MED);

	if (Tab->Value())
	{
		LCss::ColorDef c;
		if (Tab->GetCss())
			c = Tab->GetCss()->Color();
		if (!c.IsValid())
		{
			c.Type = LCss::ColorRgb;
			c.Rgb32 = TabBackground.c32();
		}
		
		pDC->Colour(c.Rgb32, 32);
	}
	else
	{
		TabBackground = TabBackground.Mix(LColour(L_BLACK), 0.06f);
		pDC->Colour(TabBackground);
	}
	
	if (!Tab->Value())
	{
		t.Inset(2, 2);
		if (IsVertical())
			t.x2++;
		else
			t.y2++;
		pDC->Rectangle(&t);
	}
	else
	{
		pDC->Rectangle(&t);
		pDC->Colour(CtrlBackground);
		if (IsVertical())
			pDC->Rectangle(t.x1+1, t.y1+1, t.x2, t.y2-1);
		else
			pDC->Rectangle(t.x1+1, t.y1+1, t.x2-1, t.y2);
	}

	// draw lines around tab
	pDC->Colour(L_HIGH);
	if (IsVertical())
	{
		pDC->Line(t.x1, t.y1+1, t.x1, t.y2-1);
		pDC->Line(t.x1+1, t.y1, t.x2, t.y1);
		pDC->Colour(L_LOW);
		pDC->Line(t.x1+1, t.y2, t.x2, t.y2);
	}
	else
	{
		pDC->Line(t.x1+1, t.y1, t.x2-1, t.y1);
		pDC->Line(t.x1, t.y1+1, t.x1, t.y2);
		pDC->Colour(L_LOW);
		pDC->Line(t.x2, t.y1+1, t.x2, t.y2);
	}

	// draw icon
	int Off = Tab->Value() ? 2 : 1;
	if (GetImageList())
		GetImageList()->Draw(pDC, t.x1 + Off, t.y1 + Off, Tab->Image(), TabBackground);
}

void LToolTabBar::OnPaint(LSurface *pDC)
{
	LRect r = GetClient();

	#ifdef WIN32
	LDoubleBuffer Buf(pDC);
	#endif
	#if 0 // Coverage test
	pDC->Colour(LColour(255, 0, 255));
	pDC->Rectangle();
	#endif

	int Off = 4 + ((Border) ? 6 : 0);

	if (Border)
	{
		// Draw border
		LThinBorder(pDC, r, DefaultRaisedEdge);
		pDC->Colour(L_MED);
		pDC->Rectangle(&r);
		r.Inset(5, 5);
	}
	else
	{
		pDC->Colour(L_MED);
		if (IsVertical())
			pDC->Rectangle(r.x1, r.y1, r.x1 + GetBx() + Off, r.y2);
		else
			pDC->Rectangle(r.x1, r.y1, r.x2, r.y1 + GetBy() + Off);
	}

	// Draw rest of the box
	if (IsVertical())
		r.x1 = GetBx() + Off;
	else
		r.y1 = GetBy() + Off;

	LThinBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(L_MED);
	pDC->Rectangle(&r);

	// Draw tabs
	LArray<LToolTab*> Down;
	for (auto It = Children.rbegin(); It != Children.end(); It--)
	{
		LToolTab *Tab = dynamic_cast<LToolTab*>(*It);
		if (Tab)
		{
			if (!Tab->Value())
				_PaintTab(pDC, Tab);
			else
				Down.Add(Tab);
		}
	}

	LColour c;
	if (Down.Length() > 1)
		c.Rgb(255, 0, 0);
	else
		c.Rgb(255, 255, 255);

	for (unsigned i=0; i<Down.Length(); i++)
	{
		// Down[i]->GetCss(true)->Color(LCss::ColorDef(LCss::ColorRgb, c.c32()));
		_PaintTab(pDC, Down[i]);
	}
}

void LToolTabBar::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		for (auto c: Children)
		{
			LToolTab *Tab = dynamic_cast<LToolTab*>(c);
			if (Tab)
			{
				if (Tab->TabPos.Overlap(m.x, m.y))
				{
					if (Current != Tab)
					{
						OnButtonClick(Tab);
						break;
					}
				}
			}
		}
	}
}

void LToolTabBar::OnPosChange()
{
	Client = GetClient();
	if (IsVertical())
		Client.x1 += GetBx() + 4;
	else
		Client.y1 += GetBx() + 4;
	Client.Inset(1, 1);

	auto v = Value();
	if (v >= 0)
	{
		LToolTab *btn = dynamic_cast<LToolTab*>(Children[v]);
		if (btn)
		{
			btn->SetPos(Client);
		}
	}
}

bool LToolTabBar::Pour(LRegion &r)
{
	bool Status = false;

	Tab.x1 = Tab.y1 = Tab.x2 = Tab.y2 = (Border) ? 6 : 0;
	int x = (IsVertical()) ? Tab.y1 : Tab.x1;
	int y = (IsVertical()) ? Tab.x1 : Tab.y1;
	
	for (auto w: Children)
	{
		LToolTab *Btn = dynamic_cast<LToolTab*>(w);
		if (Btn)
		{
			if (Btn->GetId() == IDM_SEPARATOR)
			{
				if (IsVertical())
					y += 10;
				else
					x += 10;
			}
			else
			{
				if (Btn->GetId() == IDM_BREAK)
				{
					if (IsVertical())
					{
						x += GetBx() + 4;
						y = Tab.y1;
					}
					else
					{
						x =  Tab.x1;
						y += GetBy() + 4;
					}
				}

				LRect Rgn(x, y, x + GetBx() + 4, y + GetBy() + 4);
				Btn->TabPos = Rgn;
				if (IsVertical())
					y += GetBy() + 1;
				else
					x += GetBx() + 1;
			}

			Btn->Visible(Btn->Value() != 0);
		}
	}

	if (IsVertical())
	{
		Tab.x2 = x + ((Border) ? 12 : 0);
		Tab.y2 = y + 50;
	}
	else
	{
		Tab.x2 = x + 50;
		Tab.y2 = y + ((Border) ? 12 : 0);
	}

	LRect *Best = FindLargest(r);
	if (Best)
	{
		LRect p;
		if (FitToArea)
		{
			p.Set(0, 0, Best->X()-1, Best->Y()-1);
		}
		else
		{
			if (IsVertical())
				p.Set(0, 0, 200, Tab.Y());
			else
				p.Set(0, 0, Tab.X(), 200);
		}

		p.Offset(Best->x1, Best->y1);
		p.Bound(Best);
		SetPos(p);

		Status = true;
	}

	return Status;
}

void LToolTabBar::OnCreate()
{
	AttachChildren();
	
	LToolButton *Btn = dynamic_cast<LToolButton*>(Children[0]);
	if (Btn)
	{
		Btn->Value(true);
		OnChange(Btn);
	}
}
