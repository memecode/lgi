#ifndef _GSPLITTER_H_
#define _GSPLITTER_H_

#include "lgi/common/Layout.h"

/// Displays 2 views side by side
class LgiClass GSplitter : public GLayout
{
	class GSplitterPrivate *d;

	void		CalcRegions(bool Follow = false);
	bool		OverSplit(int x, int y);

public:
	GSplitter();
	~GSplitter();

	const char *GetClass() override { return "GSplitter"; }

	/// Get the position of the split in px
	int64 Value() override; // Use to set/get the split position
	
	/// Sets the position of the split
	void Value(int64 i) override;

	/// True if the split is vertical
	bool IsVertical();
	
	/// Sets the split to horizontal or vertical
	void IsVertical(bool v);

	/// True if the split follows the opposite 
	bool DoesSplitFollow();
	
	/// Sets the split to follow the opposite 
	void DoesSplitFollow(bool i);

	/// Return the left/top view
	LView *GetViewA();

	/// Detach the left/top view
	void DetachViewA();

	/// Sets the left/top view
	void SetViewA(LView *a, bool Border = true);

	/// Return the right/bottom view
	LView *GetViewB();

	/// Detach the right/bottom view
	void DetachViewB();

	/// Sets the right/bottom view
	void SetViewB(LView *b, bool Border = true);

	/// Get the size of the bar that splits the views
	int BarSize();
	
	/// Set the bar size
	void BarSize(int i);

	#if LGI_VIEW_HANDLE
	LViewI *FindControl(OsView hCtrl) override;
	#endif

	bool Attach(LViewI *p) override;
	bool Pour(LRegion &r) override;
	void OnPaint(LSurface *pDC) override;
	void OnPosChange() override;
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	void OnMouseExit(LMouse &m) override;
	int OnHitTest(int x, int y) override;
	void OnChildrenChanged(LViewI *Wnd, bool Attaching) override;
	LgiCursor GetCursor(int x, int y) override;
};

#endif