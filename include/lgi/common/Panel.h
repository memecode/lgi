/*
**	FILE:			LPanel.h
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
#include "lgi/common/Layout.h"

class LgiClass LPanel : public LLayout
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
	LPanel(const char *name, int size, bool open = true);
	~LPanel();

	const char *GetClass() override { return "LPanel"; }

	bool Open();
	virtual void Open(bool i);
	int GetClosedSize();
	void SetClosedSize(int i);
	int GetOpenSize();
	void SetOpenSize(int i);
	int Alignment(); // GV_EDGE_TOP | GV_EDGE_RIGHT | GV_EDGE_BOTTOM | GV_EDGE_LEFT
	void Alignment(int i);

	bool Attach(LViewI *Wnd) override;
	bool Pour(LRegion &r) override;
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
	void OnPaint(LSurface *pDC) override;
	void OnMouseClick(LMouse &m) override;
};

#endif
