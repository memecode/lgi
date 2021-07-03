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

class GToolTab : public GToolButton
{
	friend class GToolTabBar;
	LRect TabPos;
	bool First;

	bool SetPos(LRect &r, bool Repaint = false);
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m) {}
	void OnMouseMove(LMouse &m) {}
	void OnMouseEnter(LMouse &m) {}
	void OnMouseExit(LMouse &m) {}

public:
	GToolTab();
	~GToolTab();

	const char *GetClass() { return "GToolTab"; }

	/// Override this event to attach controls to the current view.
	virtual void OnSelect() {}
};

class GToolTabBar : public GToolBar
{
	friend class GToolTab;

	LRect Client;
	LRect Tab;
	GToolTab *Current;
	bool FitToArea;
	bool Border;
	bool InOnChangeEvent;

	void _PaintTab(LSurface *pDC, GToolTab *Tab);

public:
	GToolTabBar(int Id = -1);
	~GToolTabBar();

	const char *GetClass() { return "GToolTabBar"; }

	int64 Value();
	void Value(int64 i);

	bool IsOk();

	bool IsFitToArea() { return FitToArea; }
	void IsFitToArea(bool b) { FitToArea = b; }
	bool HasBorder() { return Border; }
	void HasBorder(bool b) { Border = b; }

	bool Pour(LRegion &r);

	void OnButtonClick(GToolButton *Btn);
	void OnChange(GToolButton *Btn);
	void OnPaint(LSurface *pDC);
	void OnCreate();
	void OnMouseClick(LMouse &m);
	int OnNotify(GViewI *c, int f);
};

#endif
