/*
**	FILE:		GuiViews.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		18/7/98
**	DESCRIPTION:	Standard Views
**
**	Copyright (C) 1998, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Css.h"
#include "lgi/common/Layout.h"

//////////////////////////////////////////////////////////////////////////////
LLayout::LLayout() : LView(0)
{
	_SettingScrollBars = 0;
	_PourLargest = 0;
	VScroll = 0;
	HScroll = 0;

    #if WINNATIVE
	SetExStyle(GetExStyle() | WS_EX_CONTROLPARENT);
	#endif
}

LLayout::~LLayout()
{
	DeleteObj(VScroll);
	DeleteObj(HScroll);
}

LViewI *LLayout::FindControl(int Id)
{
	if (VScroll && VScroll->GetId() == Id)
		return VScroll;
	if (HScroll && HScroll->GetId() == Id)
		return HScroll;

	return LView::FindControl(Id);
}

LPoint LLayout::GetScrollPos()
{
	int64 x, y;
	GetScrollPos(x, y);
	return LPoint(x, y);
}

void LLayout::GetScrollPos(int64 &x, int64 &y)
{
	x = HScroll ? HScroll->Value() : 0;
	y = VScroll ? VScroll->Value() : 0;
}

void LLayout::SetScrollPos(int64 x, int64 y)
{
	if (HScroll)
		HScroll->Value(x);

	if (VScroll)
		VScroll->Value(y);
}

bool LLayout::SetScrollBars(bool x, bool y)
{
	bool Status = false;

	if (!_SettingScrollBars)
	{
		_SettingScrollBars = true;

		if (y)
		{
			if (!VScroll)
			{
				VScroll = new LScrollBar;
				if (VScroll)
				{
					VScroll->SetVertical(true);
					VScroll->SetParent(this);
					VScroll->SetId(IDC_VSCROLL);
					Status = true;
				}
			}
		}
		else
		{
			if (VScroll)
			{
				DeleteObj(VScroll);
				Status = true;
			}
		}

		if (x)
		{
			if (!HScroll)
			{
				HScroll = new LScrollBar;
				if (HScroll)
				{
					HScroll->SetVertical(false);
					HScroll->SetParent(this);
					HScroll->SetId(IDC_HSCROLL);
					Status = true;
				}
			}
		}
		else
		{
			if (HScroll)
			{
				DeleteObj(HScroll);
				Status = true;
			}
		}

		_SettingScrollBars = false;
	}

	return Status;
}

bool LLayout::GetPourLargest()
{
	return _PourLargest;
}

void LLayout::SetPourLargest(bool i)
{
	_PourLargest = i;
}

LCss::Len &SelectValid(LCss::Len &a, LCss::Len &b, LCss::Len &c)
{
	if (a.IsValid()) return a;
	if (b.IsValid()) return b;
	return c;
}

bool LLayout::Pour(LRegion &r)
{
	if (!_PourLargest)
		return false;

	LRect *Largest = FindLargest(r);
	if (!Largest)
		return false;

	LRect p = *Largest;
	LCss *css = GetCss();
	if (css)
	{
		LCss::Len margin = css->Margin();
		LCss::Len s;
		LCss::Len zero;
		LFont *f = GetFont();
		
		s = css->MarginTop();
		p.x1 += SelectValid(s, margin, zero).ToPx(r.X(), f);
		
		s = css->MarginTop();
		p.y1 += SelectValid(s, margin, zero).ToPx(r.Y(), f);
		
		s = css->MarginRight();
		p.x2 -= SelectValid(s, margin, zero).ToPx(r.X(), f);
		
		s = css->MarginBottom();
		p.y2 -= SelectValid(s, margin, zero).ToPx(r.Y(), f);
	
		if ((s = css->Width()).IsValid())
			p.x2 = p.x1 + s.ToPx(r.X(), f) - 1;
		if ((s = css->Height()).IsValid())
			p.y2 = p.y1 + s.ToPx(r.Y(), f) - 1;
	}
	
	if (p.Valid())
	{	
		SetPos(p, true);
	}
	else
	{
		LAssert(0);
		return false;
	}

	return true;
}

LMessage::Result LLayout::OnEvent(LMessage *Msg)
{
	if (VScroll) VScroll->OnEvent(Msg);
	if (HScroll) HScroll->OnEvent(Msg);
	LMessage::Result Status = LView::OnEvent(Msg);
	if (Msg->Msg() == M_CHANGE &&
		Status == -1 &&
		GetParent())
	{
		Status = GetParent()->OnEvent(Msg);
	}
	return Status;
}
