/*
**	FILE:			LProgressStatusPane.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			13/12/1999
**	DESCRIPTION:	Progress meter for the status bar
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/ProgressStatusPane.h"

////////////////////////////////////////////////////////////////////////////////
LProgressStatusPane::LProgressStatusPane() : DoEvery(200)
{
	SetWidth(100);
	Sunken(true);
	_BorderSize = 1;
}

LProgressStatusPane::~LProgressStatusPane()
{
}

void LProgressStatusPane::OnPaint(LSurface *pDC)
{
	LRect r = GetClient();

	pDC->Colour(L_MED);
	pDC->Box(&r);
	r.Inset(1, 1);
	pDC->Box(&r);
	r.Inset(1, 1);

	double Pos = (High != Low) ? (double) Val / ((double) High - (double) Low) : 0;
	int x = (int) (r.X() * Pos);
	if (x < r.X())
	{
		pDC->Rectangle(r.x1+x, r.y1, r.x2, r.y2);
	}

	pDC->Colour(L_FOCUS_SEL_BACK);
	pDC->Rectangle(r.x1, r.y1, r.x1+x-1, r.y2);
}

int64 LProgressStatusPane::Value()
{
	return Progress::Value();
}

void LProgressStatusPane::Value(int64 v)
{
	Progress::Value(v);

	if (DoNow() || !v)
	{
		Invalidate();
		LYield();
	}
}

