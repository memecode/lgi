/*
**	FILE:			GPanel.h
**	AUTHOR:			Matthew Allen
**	DATE:			29/8/99
**	DESCRIPTION:	Scribe Mail Object and UI
**
**	Copyright (C) 1998, Matthew Allen
**		fret@memecode.com
*/

#ifndef __GPANEL_H
#define __GPANEL_H

#include "lgi/common/DisplayString.h"

class LgiClass GPanel : public GLayout
{
protected:
	bool IsOpen;
	int Align;
	int ClosedSize;
	int OpenSize;
	LRect ThumbPos;
	LDisplayString *Ds;

	void RePour();
	void SetChildrenVisibility(bool i);
	virtual int CalcWidth();

public:
	GPanel(const char *name, int size, bool open = true);
	~GPanel();

	const char *GetClass() { return "GPanel"; }

	bool Open();
	virtual void Open(bool i);
	int GetClosedSize();
	void SetClosedSize(int i);
	int GetOpenSize();
	void SetOpenSize(int i);
	int Alignment(); // GV_EDGE_TOP | GV_EDGE_RIGHT | GV_EDGE_BOTTOM | GV_EDGE_LEFT
	void Alignment(int i);

	bool Attach(LViewI *Wnd);
	bool Pour(LRegion &r);
	int OnNotify(LViewI *Ctrl, int Flags);
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
};

#endif
