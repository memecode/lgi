/*
**	FILE:			LLayout.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			18/7/1998
**	DESCRIPTION:	Standard Views
**
**	Copyright (C) 1998-2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "LScrollBar.h"

//////////////////////////////////////////////////////////////////////////////
LLayout::LLayout()
{
	_PourLargest = false;
	VScroll = 0;
	HScroll = 0;
}

LLayout::~LLayout()
{
	DeleteObj(HScroll);
	DeleteObj(VScroll);
}

void LLayout::OnCreate()
{
}

LViewI *LLayout::FindControl(int Id)
{
	if (VScroll && VScroll->GetId() == Id)
		return VScroll;
	if (HScroll && HScroll->GetId() == Id)
		return HScroll;

	return LView::FindControl(Id);
}

bool LLayout::GetPourLargest()
{
	return _PourLargest;
}

void LLayout::SetPourLargest(bool i)
{
	_PourLargest = i;
}

bool LLayout::Pour(LRegion &r)
{
	if (_PourLargest)
	{
		LRect *Best = FindLargest(r);
		if (Best)
		{
			SetPos(*Best);
			return true;
		}
	}

	return false;
}

void LLayout::GetScrollPos(int64 &x, int64 &y)
{
	if (HScroll)
	{
		x = HScroll->Value();
	}
	else
	{
		x = 0;
	}

	if (VScroll)
	{
		y = VScroll->Value();
	}
	else
	{
		y = 0;
	}
}

void LLayout::SetScrollPos(int64 x, int64 y)
{
	if (HScroll)
	{
		HScroll->Value(x);
	}

	if (VScroll)
	{
		VScroll->Value(y);
	}
}

bool LLayout::Attach(LViewI *p)
{
	bool Status = LView::Attach(p);
	AttachScrollBars();
	return Status;
}

bool LLayout::Detach()
{
	return LView::Detach();
}

void LLayout::AttachScrollBars()
{
	if (HScroll && !HScroll->IsAttached())
	{
		// LRect r = HScroll->GetPos();
		HScroll->Attach(this);
		HScroll->SetNotify(this);
	}

	if (VScroll && !VScroll->IsAttached())
	{
		// LRect r = VScroll->GetPos();
		VScroll->Attach(this);
		VScroll->SetNotify(this);
	}
}

bool LLayout::SetScrollBars(bool x, bool y)
{
	static bool Processing = false;

	if (!Processing &&
		(((HScroll!=0) ^ x ) || ((VScroll!=0) ^ y )) )
	{
		Processing = true;
		if (x)
		{
			if (!HScroll)
			{
				HScroll = new LScrollBar(IDC_HSCROLL, 0, 0, 100, 10, "LLayout->HScroll");
				if (HScroll)
				{
					HScroll->SetVertical(false);
					HScroll->Visible(false);
				}
			}
		}
		else
		{
			DeleteObj(HScroll);
		}
		if (y)
		{
			if (!VScroll)
			{
				VScroll = new LScrollBar(IDC_VSCROLL, 0, 0, 10, 100, "LLayout->VScroll");
				if (VScroll)
				{
					VScroll->Visible(false);
				}
			}
		}
		else if (VScroll)
		{
			DeleteObj(VScroll);
		}

		AttachScrollBars();
		OnPosChange();
		Invalidate();

		Processing = false;
	}

	return true;
}

int LLayout::OnNotify(LViewI *c, int f)
{
	return LView::OnNotify(c, f);
}

void LLayout::OnPosChange()
{
	// int Edge = (Sunken() || Raised()) ? _BorderSize : 0;
	LRect r = LView::GetClient();
	#ifndef MAC
	r.Offset(Edge, Edge);
	#endif
	LRect v(r.x2-SCROLL_BAR_SIZE+1, r.y1, r.x2, r.y2);
	LRect h(r.x1, r.y2-SCROLL_BAR_SIZE+1, r.x2, r.y2);
	
	if (VScroll && HScroll)
	{
		h.x2 = v.x1 - 1;
		v.y2 = h.y1 - 1;
	}
	
	if (VScroll)
	{
		VScroll->SetPos(v, true);
		if (VScroll)
			VScroll->Visible(true);
	}
	if (HScroll)
	{
		HScroll->SetPos(h, true);
		if (HScroll)
			HScroll->Visible(true);
	}
}

void LLayout::OnNcPaint(LSurface *pDC, LRect &r)
{
	LView::OnNcPaint(pDC, r);
	
	if (VScroll && VScroll->Visible())
	{
		r.x2 = VScroll->GetPos().x1 - 1;
	}

	if (HScroll && HScroll->Visible())
	{
		r.y2 = HScroll->GetPos().y1 - 1;
	}
	
	if (VScroll && VScroll->Visible() &&
		HScroll && HScroll->Visible())
	{
		// Draw square at the end of each scroll bar
		LRect s(	VScroll->GetPos().x1, HScroll->GetPos().y1,
					VScroll->GetPos().x2, HScroll->GetPos().y2);
		pDC->Colour(LC_MED);
		pDC->Rectangle(&s);
	}
}

LRect &LLayout::GetClient(bool ClientSpace)
{
	static LRect r;
	r = LView::GetClient(ClientSpace);

	if (VScroll && VScroll->Visible())
	{
		r.x2 = VScroll->GetPos().x1 - 1;
	}

	if (HScroll && HScroll->Visible())
	{
		r.y2 = HScroll->GetPos().y1 - 1;
	}
	
	return r;
}

GMessage::Result LLayout::OnEvent(GMessage *Msg)
{
    if (Msg->Msg() != M_INVALIDATE)
    {
        if (VScroll) VScroll->OnEvent(Msg);
        if (HScroll) HScroll->OnEvent(Msg);
    }
	int Status = LView::OnEvent(Msg);
	if (Msg->Msg() == M_CHANGE &&
		Status == -1 &&
		GetParent())
	{
		Status = GetParent()->OnEvent(Msg);
	}

	return Status;
}

