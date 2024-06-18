/*
**	FILE:			Layout.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			1/12/2021
**	DESCRIPTION:	Standard Views
**
**	Copyright (C) 2021, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Notifications.h"

//////////////////////////////////////////////////////////////////////////////
LLayout::LLayout()
{
}

LLayout::~LLayout()
{
	DeleteObj(HScroll);
	DeleteObj(VScroll);
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
	x = HScroll ? HScroll->Value() : 0;
	y = VScroll ? VScroll->Value() : 0;
	
	printf("GetScrollPos=%i,%i\n", (int)x, (int)y);
}

void LLayout::SetScrollPos(int64 x, int64 y)
{
	if (HScroll)
		HScroll->Value(x);
	if (VScroll)
		VScroll->Value(y);
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

void LLayout::OnCreate()
{
	AttachScrollBars();
	OnPosChange();
}

void LLayout::AttachScrollBars()
{
	if (HScroll && !HScroll->IsAttached())
	{
		HScroll->Visible(true);
		HScroll->Attach(this);
		HScroll->SetNotify(this);
	}

	if (VScroll && !VScroll->IsAttached())
	{
		VScroll->Visible(true);
		VScroll->Attach(this);
		VScroll->SetNotify(this);
	}
}

bool LLayout::SetScrollBars(bool x, bool y)
{
	if (x ^ (HScroll != NULL)
		||
		y ^ (VScroll != NULL))
	{
		// printf("%s:%i - setScroll %i,%i attached=%i\n", _FL, x, y, IsAttached());

		if (!IsAttached())
		{
			_SetScrollBars(x, y);
		}
		else if (_SetScroll.x != x ||
				 _SetScroll.y != y ||
				!_SetScroll.SentMsg)
		{
			// This is to filter out masses of duplicate messages
			// before they have a chance to be processed. Esp important on
			// GTK systems where the message handling isn't very fast.
			_SetScroll.x = x;
			_SetScroll.y = y;
			_SetScroll.SentMsg = true;
			
			auto r = PostEvent(M_SET_SCROLL, x, y);
			if (!r)
				printf("%s:%i - sending M_SET_SCROLL(%i,%i) to myself=%s, r=%i, attached=%i.\n", _FL, x, y, GetClass(), r, IsAttached());
			return r;
		}
		
		// Duplicate... ignore...
		return true;
	}

	return true;
}

bool LLayout::_SetScrollBars(bool x, bool y)
{
	static bool Processing = false;

	// printf("%s:%i - _setScroll %i,%i %i\n", _FL, x, y, Processing);

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

int LLayout::OnNotify(LViewI *c, LNotification n)
{
	return LView::OnNotify(c, n.Type);
}

void LLayout::OnPosChange()
{
	LRect r = LView::GetClient();
	auto Px = LScrollBar::GetScrollSize();
	LRect v(r.x2-Px+1, r.y1, r.x2, r.y2);
	LRect h(r.x1, r.y2-Px+1, r.x2, r.y2);
	if (VScroll && HScroll)
	{
		h.x2 = v.x1 - 1;
		v.y2 = h.y1 - 1;
	}

	if (VScroll)
	{
		VScroll->Visible(true);
		VScroll->SetPos(v, true);
	}
	if (HScroll)
	{
		HScroll->Visible(true);
		HScroll->SetPos(h, true);
	}
}

void LLayout::OnNcPaint(LSurface *pDC, LRect &r)
{
	LView::OnNcPaint(pDC, r);
	
	if (VScroll && VScroll->Visible())
	{
		r.x2 -= VScroll->X();
	}

	if (HScroll && HScroll->Visible())
	{
		r.y2 -= HScroll->Y();
	}
	
	if (VScroll && VScroll->Visible() &&
		HScroll && HScroll->Visible())
	{
		// Draw square at the end of each scroll bar
		LRect s(	VScroll->GetPos().x1, HScroll->GetPos().y1,
					VScroll->GetPos().x2, HScroll->GetPos().y2);
		pDC->Colour(L_MED);
		pDC->Rectangle(&s);
	}
}

LRect &LLayout::GetClient(bool ClientSpace)
{
	static LRect r;
	r = LView::GetClient(ClientSpace);

	if (VScroll && VScroll->Visible())
	{
		// printf("\tLayout.GetCli r=%s -> ", r.GetStr());
		r.x2 = VScroll->GetPos().x1 - 1;
		// printf("%s\n", r.GetStr());
	}

	if (HScroll && HScroll->Visible())
		r.y2 = HScroll->GetPos().y1 - 1;
	
	return r;
}

LMessage::Param LLayout::OnEvent(LMessage *Msg)
{
	if (Msg->Msg() == M_SET_SCROLL)
	{
		_SetScroll.SentMsg = false;
		// printf("%s:%i - receiving M_SET_SCROLL myself=%s.\n", _FL, GetClass());
		_SetScrollBars(Msg->A(), Msg->B());
		
		if (HScroll)
			HScroll->SendNotify(LNotifyScrollBarCreate);
		if (VScroll)
			VScroll->SendNotify(LNotifyScrollBarCreate);
		
		return 0;
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

