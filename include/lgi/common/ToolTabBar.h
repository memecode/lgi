/*hdr
**      FILE:           ToolTabBar.h
**      AUTHOR:         Matthew Allen
**      DATE:           22/3/2000
**      DESCRIPTION:    Toolbar of tabs
**
**      Copyright (C) 1997-1998 Matthew Allen
**              fret@memecode.com
*/

#ifndef __TOOL_TAB_BAR_H
#define __TOOL_TAB_BAR_H

#include "lgi/common/ToolBar.h"

class LToolTab : public LToolButton
{
	friend class LToolTabBar;
	LRect TabPos;
	bool First;

	bool SetPos(LRect &r, bool Repaint = false);
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m) {}
	void OnMouseMove(LMouse &m) {}
	void OnMouseEnter(LMouse &m) {}
	void OnMouseExit(LMouse &m) {}

public:
	LToolTab();
	~LToolTab();

	const char *GetClass() { return "LToolTab"; }

	/// Override this event to attach controls to the current view.
	virtual void OnSelect() {}
};

class LToolTabBar : public LToolBar
{
	friend class LToolTab;

	LRect Client;
	LRect Tab;
	LToolTab *Current;
	bool FitToArea;
	bool Border;
	bool InOnChangeEvent;

	void _PaintTab(LSurface *pDC, LToolTab *Tab);

public:
	LToolTabBar(int Id = -1);
	~LToolTabBar();

	const char *GetClass() { return "LToolTabBar"; }

	int64 Value();
	void Value(int64 i);

	bool IsOk();

	bool IsFitToArea() { return FitToArea; }
	void IsFitToArea(bool b) { FitToArea = b; }
	bool HasBorder() { return Border; }
	void HasBorder(bool b) { Border = b; }

	bool Pour(LRegion &r);

	void OnButtonClick(LToolButton *Btn);
	void OnChange(LToolButton *Btn);
	void OnPaint(LSurface *pDC);
	void OnCreate();
	void OnMouseClick(LMouse &m);
	int OnNotify(LViewI *c, const LNotification &n) override;
	void OnPosChange();
};

#endif
