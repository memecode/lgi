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
#include "Lgi.h"
#include "LScrollBar.h"

//////////////////////////////////////////////////////////////////////////////
LLayout::LLayout() :
	LView(new BViewRedir(this))
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
			SetPos(*Best, true);
			return true;
		}
	}
	
	return false;
}

void LLayout::GetScrollPos(int &x, int &y)
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

void LLayout::SetScrollPos(int x, int y)
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
	
	if (_View)
	{
		GLocker Locker(Handle(), _FL);
		Locker.Lock();
		if (HScroll)
		{
			if (HScroll->Handle() &&
				HScroll->Handle()->Parent())
			{
				HScroll->Handle()->RemoveSelf();
			}
			
			Handle()->AddChild(HScroll->Handle());
		}
		
		if (VScroll)
		{
			if (VScroll->Handle() &&
				VScroll->Handle()->Parent())
			{
				VScroll->Handle()->RemoveSelf();
			}
	
			Handle()->AddChild(VScroll->Handle());
		}
	}
	
	return Status;
}

bool LLayout::Detach()
{
	return LView::Detach();
}

bool LLayout::SetScrollBars(bool x, bool y)
{
	if (( (HScroll!=0) ^ x ) ||
		( (VScroll!=0) ^ y ))
	{
		if (x)
		{
			if (!HScroll)
			{
				HScroll = new LScrollBar(IDC_HSCROLL, 0, 0, 100, 10, "LLayout->HScroll");
				if (HScroll)
				{
					HScroll->SetNotify(this);
					HScroll->Visible(false);
					
					Handle()->AddChild(HScroll->Handle());
					HScroll->SetParent(this);
				}
			}
		}
		else if (HScroll)
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
					VScroll->SetNotify(this);
					VScroll->Visible(false);
					
					Handle()->AddChild(VScroll->Handle());
					VScroll->SetParent(this);
				}
			}
		}
		else if (VScroll)
		{
			DeleteObj(VScroll);
		}
		
		OnPosChange();
		Invalidate();
	}
	
	return true;
}

void LLayout::OnPosChange()
{
	LRect r = LView::GetClient();	
	LRect v(r.x2-B_V_SCROLL_BAR_WIDTH+1, r.y1, r.x2, r.y2);
	LRect h(r.x1, r.y2-B_H_SCROLL_BAR_HEIGHT+1, r.x2, r.y2);

	if (VScroll)
	{
		if (!VScroll->IsAttached())
			VScroll->Attach(this);
		if (HScroll)
			v.y2 = h.y1 - 1;
		VScroll->SetPos(v, true);
		VScroll->Visible(true);
	}
	
	if (HScroll)
	{
		if (!HScroll->IsAttached())
			HScroll->Attach(this);
		if (HScroll)
			h.x2 = v.x1 - 1;
		HScroll->SetPos(h, true);
		HScroll->Visible(true);
	}
}

LRect &LLayout::GetClient(bool ClientSpace)
{
	static LRect r;

	r = LView::GetClient(ClientSpace);
	
	if (VScroll)
	{
		r.x2 = VScroll->GetPos().x1 - 1;
	}
	
	if (HScroll)
	{
		r.y2 = HScroll->GetPos().y1 - 1;
	}

	return r;
}

void LLayout::OnNcPaint(LSurface *pDC, LRect &r)
{
	LView::OnNcPaint(pDC, r);
}

int LLayout::OnNotify(LViewI *v, int f)
{
	return LView::OnNotify(v, f);
}

LMessage::Result LLayout::OnEvent(LMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_VSCROLL:
		{
			if (VScroll)
			{
				OnNotify(VScroll, MsgA(Msg));
			}
			break;
		}
		case M_HSCROLL:
		{
			if (HScroll)
			{
				OnNotify(HScroll, MsgA(Msg));
			}
			break;
		}
	}
	
	if (VScroll) VScroll->OnEvent(Msg);
	if (HScroll) HScroll->OnEvent(Msg);
	return LView::OnEvent(Msg);
}

LViewI *LLayout::FindControl(int Id)
{
	if (VScroll && VScroll->GetId() == Id)
		return VScroll;
	if (HScroll && HScroll->GetId() == Id)
		return HScroll;

	return LView::FindControl(Id);
}

