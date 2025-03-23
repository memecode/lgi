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
#include "lgi/common/Lgi.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Notifications.h"

#define M_SET_SCROLL		(M_USER + 0x2000)

#if 0
#define LOG(...)			LgiTrace(__VA_ARGS__)
#else
#define LOG(...)
#endif

//////////////////////////////////////////////////////////////////////////////
LLayout::LLayout()
{
	_PourLargest = false;
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
		auto Best = FindLargest(r);
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
		x = HScroll->Value();
	else
		x = 0;

	if (VScroll)
		y = VScroll->Value();
	else
		y = 0;
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
	if (HScroll)
	{
		if (!HScroll->IsAttached())
		{
			HScroll->Attach(this);
			HScroll->SetNotify(this);
			HScroll->SendNotify(LNotifyScrollBarCreate);
			LOG("%s:%i - HScroll attaching\n", _FL);
		}
		else LOG("%s:%i - HScroll already attached\n", _FL);
	}

	if (VScroll)
	{
		if (!VScroll->IsAttached())
		{
			VScroll->Attach(this);
			VScroll->SetNotify(this);
			VScroll->SendNotify(LNotifyScrollBarCreate);
			LOG("%s:%i - VScroll attaching\n", _FL);
		}
		else LOG("%s:%i - VScroll already attached\n", _FL);
	}
}

bool LLayout::SetScrollBars(bool x, bool y)
{
	#ifdef M_SET_SCROLL

		if (!PostEvent(M_SET_SCROLL, x, y))
			return false;

	#else

		_SetScrollBars(x, y);

	#endif

	return true;
}

bool LLayout::_SetScrollBars(bool x, bool y)
{
	static bool Processing = false;

	if
	(
		!Processing &&
		(
			( (HScroll != nullptr) ^ x )
			||
			( (VScroll != nullptr) ^ y )
		)
	)
	{
		Processing = true;

		if (x)
		{
			if (!HScroll)
			{
				if ((HScroll = new LScrollBar(IDC_HSCROLL, 0, 0, 100, 10, "LLayout->HScroll")))
				{
					HScroll->SetVertical(false);
					HScroll->Visible(false);
					LOG("%s:%i - %s: created hscroll\n", _FL, __FUNCTION__);
				}
				else LOG("%s:%i - alloc failed.\n", _FL);
			}
		}
		else
		{
			DeleteObj(HScroll);
			LOG("%s:%i - %s: deleted hscroll\n", _FL, __FUNCTION__);
		}

		if (y)
		{
			if (!VScroll)
			{
				if ((VScroll = new LScrollBar(IDC_VSCROLL, 0, 0, 10, 100, "LLayout->VScroll")))
				{
					VScroll->Visible(false);
					LOG("%s:%i - %s: created vscroll\n", _FL, __FUNCTION__);
				}
				else LOG("%s:%i - alloc failed.\n", _FL);
			}
		}
		else if (VScroll)
		{
			DeleteObj(VScroll);
			LOG("%s:%i - %s: deleted vscroll\n", _FL, __FUNCTION__);
		}

		AttachScrollBars();
		OnPosChange();
		Invalidate();

		Processing = false;
	}
	else
	{
		LOG("%s:%i - %s: nothing to do %i,%i,%i\n",
			_FL,
			__FUNCTION__,
			Processing, x, y);
	}
	
	return true;
}

int LLayout::OnNotify(LViewI *c, const LNotification &n)
{
	return LView::OnNotify(c, n);
}

void LLayout::OnPosChange()
{
	auto r = LView::GetClient();
	int px = LScrollBar::GetScrollSize();
	LRect v(r.x2 - px + 1, r.y1, r.x2, r.y2);
	LRect h(r.x1, r.y2 - px + 1, r.x2, r.y2);
	if (VScroll && HScroll)
	{
		h.x2 = v.x1 - 1;
		v.y2 = h.y1 - 1;
	}
	
	if (VScroll)
	{
		VScroll->Visible(true);
		VScroll->SetPos(v, true);
		LOG("%s:%i - %s: vscroll=%s\n", _FL, __FUNCTION__, v.GetStr());
	}
	if (HScroll)
	{
		HScroll->Visible(true);
		HScroll->SetPos(h, true);
		LOG("%s:%i - %s: hscroll=%s\n", _FL, __FUNCTION__, h.GetStr());
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
		r.x2 = VScroll->GetPos().x1 - 1;
	}

	if (HScroll && HScroll->Visible())
	{
		r.y2 = HScroll->GetPos().y1 - 1;
	}
	
	return r;
}

LMessage::Param LLayout::OnEvent(LMessage *Msg)
{
	#ifdef M_SET_SCROLL
	if (Msg->Msg() == M_SET_SCROLL)
	{
		LOG("%s:%i - got M_SET_SCROLL(%i,%i)\n", _FL, (int)Msg->A(), (int)Msg->B());
		_SetScrollBars(Msg->A()!=0, Msg->B()!=0);
		return 0;
	}
	#endif

	auto Status = LView::OnEvent(Msg);
	if (Msg->Msg() == M_CHANGE &&
		Status == -1 &&
		GetParent())
	{
		Status = GetParent()->OnEvent(Msg);
	}

	return Status;
}

