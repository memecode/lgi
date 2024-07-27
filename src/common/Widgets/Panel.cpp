/*
**	FILE:			LPanel.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			29/8/99
**	DESCRIPTION:	Scribe Mail Object and UI
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/
#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/Panel.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/CssTools.h"

//////////////////////////////////////////////////////////////////////////////
LPanel::LPanel(const char *name, int size, bool open)
{
	Ds = 0;
	if (name)
		Name(name);
	IsOpen = open;
	_IsToolBar = !IsOpen;
	ClosedSize = LSysFont->GetHeight() + 3;
	OpenSize = size;
	Align = GV_EDGE_TOP;
	
	_BorderSize = 1;
	Raised(true);
	LResources::StyleElement(this);
}

LPanel::~LPanel()
{
	DeleteObj(Ds);
}

int LPanel::CalcWidth()
{
	if (!Ds)
		Ds = new LDisplayString(GetFont(), Name());

	return 30 + (Ds ? Ds->X() : 0);
}

bool LPanel::Open()
{
	return IsOpen;
}

void LPanel::Open(bool i)
{
	if (i != IsOpen)
	{
		IsOpen = i;
		_IsToolBar = !IsOpen;
		SetChildrenVisibility(IsOpen);
		RePour();
	}
}

int LPanel::Alignment()
{
	return Align;
}

void LPanel::Alignment(int i)
{
	Align = i;
}

int LPanel::GetClosedSize()
{
	return ClosedSize;
}

void LPanel::SetClosedSize(int i)
{
	ClosedSize = i;
}

int LPanel::GetOpenSize()
{
	return OpenSize;
}

void LPanel::SetOpenSize(int i)
{
	OpenSize = i;
}

bool LPanel::Attach(LViewI *Wnd)
{
	bool Status = LLayout::Attach(Wnd);
	SetChildrenVisibility(IsOpen);
	if (Status)
		AttachChildren();

	return Status;
}

bool LPanel::Pour(LRegion &rgn)
{
	int Sx = CalcWidth();
	LRect *Best = 0;
	if (Open())
	{
		Best = FindLargest(rgn);
	}
	else
	{
		Best = FindSmallestFit(rgn, Sx, ClosedSize);
		// LgiTrace("%s:%i - panel %s FindSmallestFit=%s.\n", _FL, Name(), Best ? Best->GetStr() : "(null)");
		if (!Best)
		{
			Best = FindLargest(rgn);
			// LgiTrace("%s:%i - panel %s FindLargest=%s.\n", _FL, Name(), Best ? Best->GetStr() : "(null)");
		}
	}

	if (!Best)
	{
		LgiTrace("%s:%i - No best rect.\n", _FL);
		return false;
	}
	
	LRect rc = *Best;
	// LgiTrace("%s:%i - open=%i panel %s best=%s, OpenSize=%i, ClosedSize=%i, Align=%x.\n", _FL, Open(), Name(), rc.GetStr(), OpenSize, ClosedSize, Align);
	if (OpenSize > 0)
	{
		int Size = ((Open()) ? OpenSize : ClosedSize);
		int Limit = 30;
		if (TestFlag(Align, GV_EDGE_RIGHT) ||
			TestFlag(Align, GV_EDGE_LEFT))
		{
			Limit = MIN(Size, rc.X()-1);
		}
		else /* if (TestFlag(Align, GV_EDGE_BOTTOM) ||
				 TextFlag(Align, GV_EDGE_TOP)) */
		{
			Limit = MIN(Size, rc.Y()-1);
		}
		
		if (Limit <= 0)
		{
			LAssert(!"Invalid limit/size");
		}
		
		if (Align & GV_EDGE_RIGHT)
		{
			rc.x1 = rc.x2 - Limit;
		}
		else if (Align & GV_EDGE_BOTTOM)
		{
			rc.y1 = rc.y2 - Limit;
		}
		else if (Align & GV_EDGE_LEFT)
		{
			rc.x2 = rc.x1 + Limit;
		}
		else // if (Align & GV_EDGE_TOP)
		{
			rc.y2 = rc.y1 + Limit;
		}
		
		if (!Open())
		{
			rc.x2 = rc.x1 + Sx - 1;
		}
	}
	else
	{
		rc.y2 = rc.y1 - OpenSize;
	}

	SetPos(rc, true);
	
	if (IsOpen)
	{
		for (auto v: Children)
		{
			auto css = v->GetCss();
			if (css &&
				css->Width() == LCss::LenAuto &&
				css->Height() == LCss::LenAuto)
			{
				LRect c = GetClient();
				LCssTools tools(css, v->GetFont());
				c = tools.ApplyMargin(c);
				v->SetPos(c);
				break;
			}
		}
	}

	return true;
}

int LPanel::OnNotify(LViewI *Ctrl, LNotification n)
{
	if (GetParent())
	{
		return GetParent()->OnNotify(Ctrl, n);
	}

	return 0;
}

void LPanel::OnPaint(LSurface *pDC)
{
	auto r = GetClient();
	LCssTools Tools(this);
	LColour cFore = Tools.GetFore();
	LColour cBack = Tools.GetBack();
	Tools.PaintContent(pDC, r);

	auto BackImg = Tools.GetBackImage();
	auto Fnt = GetFont();
	Fnt->Transparent(BackImg ? true : false);
	Fnt->Fore(cFore);
	Fnt->Back(cBack);
	if (!Open())
	{
		// title
		if (!Ds)
			Ds = new LDisplayString(Fnt, Name());
		if (Ds)
			Ds->Draw(pDC, r.x1 + 20, r.y1 + 1);
	}

	if (OpenSize > 0)
	{
		// Draw thumb
		auto scale = GetWindow()->GetDpiScale();
		scale *= 1.2;
		auto sizePx = (int)(8 * scale.x);
		if (sizePx % 2 == 0)
			sizePx++;
		auto gapPx = (int)(scale.x * 2);
		ThumbPos.ZOff(sizePx-1, sizePx-1);
		ThumbPos.Offset(r.x1 + 3, r.y1 + 3);
		
		pDC->Colour(L_LOW);
		pDC->Box(&ThumbPos);
		pDC->Colour(L_WHITE);
		pDC->Rectangle(ThumbPos.x1+1, ThumbPos.y1+1, ThumbPos.x2-1, ThumbPos.y2-1);
		pDC->Colour(L_SHADOW);
		
		auto center = ThumbPos.Center();
		pDC->Line(	ThumbPos.x1+gapPx,
					center.y,
					ThumbPos.x2-gapPx,
					center.y);
		
		if (!Open())
		{
			pDC->Line(	center.x,
						ThumbPos.y1+gapPx,
						center.x,
						ThumbPos.y2-gapPx);
		}
	}
}

void LPanel::OnMouseClick(LMouse &m)
{
	if (OpenSize > 0 &&
		m.Left() &&
		m.Down())
	{
		if (ThumbPos.Overlap(m.x, m.y))
		{
			Open(!IsOpen);
			if (GetParent())
				OnNotify(this, LNotifyItemClick);
		}
		else LgiTrace("%s:%i - Not over %i,%i - %s\n", _FL, m.x, m.y, ThumbPos.GetStr());
	}
}

void LPanel::RePour()
{
	if (auto Top = GetWindow())
		Top->PourAll();
}

void LPanel::SetChildrenVisibility(bool i)
{
	for (auto w: Children)
	{
		if (i &&
			!w->IsAttached())
		{
			w->Attach(this);
		}

		w->Visible(i);
	}
}

