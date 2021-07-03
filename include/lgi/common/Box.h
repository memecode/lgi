#ifndef _GBOX_H_
#define _GBOX_H_

#include "lgi/common/Css.h"

/// This is a vertical or horizontal layout box, similar to the
/// old GSplitter control except it can handle any number of children
class LgiClass GBox : public GView
{
	struct GBoxPriv *d;

protected:

public:
	struct Spacer
	{
		LRect Pos; // Position on screen in view coords
		GColour Colour; // Colour of the spacer
		uint32_t SizePx; // Size in pixels
	};

	GBox(int Id = -1, bool Vertical = false, const char *name = NULL);
	~GBox();

	const char *GetClass() { return "GBox"; }

	bool IsVertical();
	void SetVertical(bool v);
	Spacer *GetSpacer(int i);
	GViewI *GetViewAt(int i);
	bool SetViewAt(uint32_t i, GViewI *v);
	int64 Value();
	void Value(int64 i);
	
	void OnCreate();
	void OnPaint(GSurface *pDC);
	void OnPosChange();
	void OnMouseClick(LMouse &m);
	bool OnViewMouse(GView *v, LMouse &m);
	void OnMouseMove(LMouse &m);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	GMessage::Result OnEvent(GMessage *Msg);
	bool Pour(LRegion &r);
	LgiCursor GetCursor(int x, int y);
	bool OnLayout(GViewLayoutInfo &Inf);
	int OnNotify(GViewI *Ctrl, int Flags);

	bool Serialize(GDom *Dom, const char *OptName, bool Write);	
	bool SetSize(int ViewIndex, LCss::Len Size);
};

#endif
