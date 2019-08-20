/*
**	FILE:			GPanel.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			29/8/99
**	DESCRIPTION:	Scribe Mail Object and UI
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/
#include <stdio.h>
#include "Lgi.h"
#include "GPanel.h"
#include "GDisplayString.h"
#include "LgiRes.h"
#include "GCssTools.h"

//////////////////////////////////////////////////////////////////////////////
GPanel::GPanel(const char *name, int size, bool open)
{
	Ds = 0;
	if (name) Name(name);
	IsOpen = open;
	_IsToolBar = !IsOpen;
	ClosedSize = SysFont->GetHeight() + 3;
	OpenSize = size;
	Align = GV_EDGE_TOP;
	
	_BorderSize = 1;
	Raised(true);
	LgiResources::StyleElement(this);
}

GPanel::~GPanel()
{
	DeleteObj(Ds);
}

int GPanel::CalcWidth()
{
	if (!Ds)
		Ds = new GDisplayString(GetFont(), Name());

	return 30 + (Ds ? Ds->X() : 0);
}

bool GPanel::Open()
{
	return IsOpen;
}

void GPanel::Open(bool i)
{
	if (i != IsOpen)
	{
		IsOpen = i;
		_IsToolBar = !IsOpen;
		SetChildrenVisibility(IsOpen);
		RePour();

		/*
		int ExStyle = GetWindowLong(Handle(), GWL_EXSTYLE);
		SetWindowLong(	Handle(),
						GWL_EXSTYLE,
						(ExStyle & ~WS_EX_CONTROLPARENT) | (IsOpen ? WS_EX_CONTROLPARENT : 0));
		*/
	}
}

int GPanel::Alignment()
{
	return Align;
}

void GPanel::Alignment(int i)
{
	Align = i;
}

int GPanel::GetClosedSize()
{
	return ClosedSize;
}

void GPanel::SetClosedSize(int i)
{
	ClosedSize = i;
}

int GPanel::GetOpenSize()
{
	return OpenSize;
}

void GPanel::SetOpenSize(int i)
{
	OpenSize = i;
}

bool GPanel::Attach(GViewI *Wnd)
{
	bool Status = GLayout::Attach(Wnd);
	SetChildrenVisibility(IsOpen);
	if (Status)
		AttachChildren();

	return Status;
}

bool GPanel::Pour(GRegion &r)
{
	int Sx = CalcWidth();
	GRect *Best = 0;
	if (Open())
	{
		Best = FindLargest(r);
	}
	else
	{
		Best = FindSmallestFit(r, Sx, ClosedSize);
		if (!Best)
		{
			Best = FindLargest(r);
		}
	}

	if (Best)
	{
		GRect r = *Best;
		if (OpenSize > 0)
		{
			int Size = ((Open()) ? OpenSize : ClosedSize);
			int Limit = 30;
			if (TestFlag(Align, GV_EDGE_RIGHT) ||
				TestFlag(Align, GV_EDGE_LEFT))
			{
				Limit = MIN(Size, r.X()-1);
			}
			else /* if (TestFlag(Align, GV_EDGE_BOTTOM) ||
					 TextFlag(Align, GV_EDGE_TOP)) */
			{
				Limit = MIN(Size, r.Y()-1);
			}
			
			if (Align & GV_EDGE_RIGHT)
			{
				r.x1 = r.x2 - Limit;
			}
			else if (Align & GV_EDGE_BOTTOM)
			{
				r.y1 = r.y2 - Limit;
			}
			else if (Align & GV_EDGE_LEFT)
			{
				r.x2 = r.x1 + Limit;
			}
			else // if (Align & GV_EDGE_TOP)
			{
				r.y2 = r.y1 + Limit;
			}
			
			if (!Open())
			{
				r.x2 = r.x1 + Sx - 1;
			}
		}
		else
		{
			r.y2 = r.y1 - OpenSize;
		}

		SetPos(r, true);
		
		if (IsOpen)
		{
			for (auto v: Children)
			{
				auto css = v->GetCss();
				if (css &&
					css->Width() == GCss::LenAuto &&
					css->Height() == GCss::LenAuto)
				{
					GRect c = GetClient();
					GCssTools tools(css, v->GetFont());
					c = tools.ApplyMargin(c);
					v->SetPos(c);
					break;
				}
			}
		}

		return true;
	}

	return false;
}

int GPanel::OnNotify(GViewI *Ctrl, int Flags)
{
	if (GetParent())
	{
		return GetParent()->OnNotify(Ctrl, Flags);
	}

	return 0;
}

void GPanel::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();
	GColour cFore = StyleColour(GCss::PropColor, LColour(L_TEXT));
	GColour cBack = StyleColour(GCss::PropBackgroundColor, LColour(L_MED));

	pDC->Colour(cBack);
	pDC->Rectangle(&r);
	SysFont->Transparent(false);
	SysFont->Fore(cFore);
	SysFont->Back(cBack);
	if (!Open())
	{
		// title
		if (!Ds)
			Ds = new GDisplayString(GetFont(), Name());
		if (Ds)
			Ds->Draw(pDC, r.x1 + 20, r.y1 + 1);
	}

	if (OpenSize > 0)
	{
		// Draw thumb
		ThumbPos.ZOff(8, 8);
		ThumbPos.Offset(r.x1 + 3, r.y1 + 3);
		
		pDC->Colour(L_LOW);
		pDC->Box(&ThumbPos);
		pDC->Colour(L_WHITE);
		pDC->Rectangle(ThumbPos.x1+1, ThumbPos.y1+1, ThumbPos.x2-1, ThumbPos.y2-1);
		pDC->Colour(L_SHADOW);
		pDC->Line(	ThumbPos.x1+2,
					ThumbPos.y1+4,
					ThumbPos.x1+6,
					ThumbPos.y1+4);
		
		if (!Open())
		{
			pDC->Line(	ThumbPos.x1+4,
						ThumbPos.y1+2,
						ThumbPos.x1+4,
						ThumbPos.y1+6);
		}
	}
}

void GPanel::OnMouseClick(GMouse &m)
{
	if (OpenSize > 0 &&
		m.Left() &&
		m.Down() &&
		ThumbPos.Overlap(m.x, m.y))
	{
		Open(!IsOpen);
		if (GetParent())
		{
			OnNotify(this, 0);
		}
	}
}

void GPanel::RePour()
{
	GWindow *Top = dynamic_cast<GWindow*>(GetWindow());
	if (Top)
	{
		Top->PourAll();
	}
}

void GPanel::SetChildrenVisibility(bool i)
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

