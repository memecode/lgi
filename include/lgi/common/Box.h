#ifndef _GBOX_H_
#define _GBOX_H_

#include "lgi/common/Css.h"

/// This is a vertical or horizontal layout box, similar to the
/// old LSplitter control except it can handle any number of children
class LgiClass LBox : public LView
{
	struct LBoxPriv *d;

protected:

public:
	struct Spacer
	{
		LRect Pos; // Position on screen in view coords
		LColour Colour; // Colour of the spacer
		uint32_t SizePx; // Size in pixels
	};

	LBox(int Id = -1, bool Vertical = false, const char *name = NULL);
	~LBox();

	const char *GetClass() { return "LBox"; }

	bool IsVertical();
	void SetVertical(bool v);
	Spacer *GetSpacer(int i);
	LViewI *GetViewAt(int i);
	bool SetViewAt(uint32_t i, LViewI *v);
	int64 Value();
	void Value(int64 i);
	
	void OnCreate();
	void OnPaint(LSurface *pDC);
	void OnPosChange();
	void OnMouseClick(LMouse &m);
	bool OnViewMouse(LView *v, LMouse &m);
	void OnMouseMove(LMouse &m);
	void OnChildrenChanged(LViewI *Wnd, bool Attaching);
	GMessage::Result OnEvent(GMessage *Msg);
	bool Pour(LRegion &r);
	LCursor GetCursor(int x, int y);
	bool OnLayout(LViewLayoutInfo &Inf);
	int OnNotify(LViewI *Ctrl, int Flags);

	bool Serialize(GDom *Dom, const char *OptName, bool Write);	
	bool SetSize(int ViewIndex, LCss::Len Size);
};

#endif
