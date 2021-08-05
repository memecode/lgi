/// \file
/// \author Matthew Allen <fret@memecode.com>
/// \brief A control layout manager for font sensitive GUI's.
#ifndef _GTABLE_LAYOUT_H_
#define _GTABLE_LAYOUT_H_

#include "lgi/common/Css.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Layout.h"

/// A layout cell, not currently implemented.
class GLayoutCell : public GDom, public LCss
{
public:
	bool Debug;

	GLayoutCell() { Debug = false; }
	virtual ~GLayoutCell() {}

	virtual class LTableLayout *GetTable() = 0;
	virtual bool Add(LView *v) = 0;
	virtual bool Remove(LView *v) = 0;
	virtual LArray<LView*> GetChildren() = 0;
};

/// A table layout control. This uses techniques similar to HTML table layout to set the position
/// and size of the child controls. Child controls exist in a particular cell of the table. Cells
/// can span multiple rows and columns. The only way to create/edit these at the moment is via the lr8
/// resource file and LgiRes.
class LgiClass LTableLayout :
	public LLayout,
	public ResObject,
	public GDom
{
	friend class TableCell;
	class GTableLayoutPrivate *d;

	bool SizeChanged();

public:
	/// The system default cell spacing for all tables. Individual tables can have their own
	/// cell spacing by setting the CSS property 'border-spacing'.
    static int CellSpacing;

	LTableLayout(int id = -1);
	~LTableLayout();

	const char *GetClass() override { return "LTableLayout"; }

	/// Return the number of cells across (columns).
	int CellX();
	/// Returns the number of cell high (rows).
	int CellY();
	/// Returns the cell at a given location.
	GLayoutCell *CellAt(int x, int y);
	/// Returns area being used by cells
	LRect GetUsedArea();
	/// Invalidates the layout, causing the control to relay all the children
	void InvalidateLayout();

    /// Create a cell
    GLayoutCell *GetCell(int x, int y, bool create = true, int colspan = 1, int rowspan = 1);
    /// Clear all cells;
    void Empty(LRect *Range = NULL);

	// Impl
	void OnFocus(bool b) override;
	void OnCreate() override;
	void OnPosChange() override;
	void OnPaint(LSurface *pDC) override;
	bool GetVariant(const char *Name, LVariant &Value, char *Array = 0) override;
	bool SetVariant(const char *Name, LVariant &Value, char *Array = 0) override;
	void OnChildrenChanged(LViewI *Wnd, bool Attaching) override;
	int64 Value() override;
	void Value(int64 v) override;
	int OnNotify(LViewI *c, LNotification &n) override;
	LMessage::Result OnEvent(LMessage *m) override;
};


/// This is just a light-weight layout system for doing things manually.
class GLayoutRect : public LRect
{
	int Spacing;

public:
	GLayoutRect(LViewI *c, int spacing = LTableLayout::CellSpacing)
	{
		Spacing = spacing;
		((LRect&)*this) = c->GetClient();
		Size(Spacing, Spacing);
	}

	GLayoutRect(LRect rc, int spacing = LTableLayout::CellSpacing)
	{
		Spacing = spacing;
		((LRect&)*this) = rc;		
	}

	// Allocate object on left edge
	// ------------------------------------
		GLayoutRect Left(LRect &rc, int Px)
		{
			rc = *this;
			rc.x2 = rc.x1 + Px - 1;
			x1 = rc.x2 + Spacing;
			return *this;
		}

		GLayoutRect Left(LViewI *v, int Px)
		{
			LRect r;
			Left(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}

	// Allocate on right edge...
	// ------------------------------------
		GLayoutRect Right(LRect &rc, int Px)
		{
			rc = *this;
			rc.x1 = rc.x2 - Px + 1;
			x2 = rc.x1 - Spacing;
			return *this;
		}

		GLayoutRect Right(LViewI *v, int Px)
		{
			LRect r;
			Right(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}

	// Allocate on top edge...
	// ------------------------------------
		GLayoutRect Top(LRect &rc, int Px)
		{
			rc = *this;
			rc.y2 = rc.y1 + Px - 1;
			y1 = rc.y2 + Spacing;
			return *this;
		}

		GLayoutRect Top(LViewI *v, int Px)
		{
			LRect r;
			Top(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}

	// Allocate on top edge...
	// ------------------------------------
		GLayoutRect Bottom(LRect &rc, int Px)
		{
			rc = *this;
			rc.y2 = rc.y1 + Px - 1;
			y1 = rc.y2 + Spacing;
			return *this;
		}

		GLayoutRect Bottom(LViewI *v, int Px)
		{
			LRect r;
			Bottom(r, Px);
			if (v) v->SetPos(r);
			return *this;
		}
	
	// Allocate all remaining space.
	// ------------------------------------
	void Remaining(LRect &rc)
	{
		rc = *this;
		ZOff(-1, -1);
	}

	void Remaining(LViewI *v)
	{
		if (v)
			v->SetPos(*this);
		ZOff(-1, -1);
	}

};

#endif
