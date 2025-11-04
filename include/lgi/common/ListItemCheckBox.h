/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A checkbox for a list item

#ifndef __LList_ITEM_CHECKBOX_H
#define __LList_ITEM_CHECKBOX_H

#include "lgi/common/List.h"

/// A checkbox suitable for a LListItem.
class LListItemCheckBox : public LListItemColumn
{
public:
	/// Constructor
	LListItemCheckBox
	(
		/// The hosting list item.
		LListItem *host,
		/// The column to draw in.
		int column,
		// The initial value.
		bool value = false
	) : LListItemColumn(host, column)
	{
		Value(value);
	}

	void OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *Col)
	{
		LSurface *pDC = Ctx.pDC;
		int px = (int)((float)Ctx.Y() * 0.8f);
		int pad = (int)((float)Ctx.Y() * 0.1f);
		LRect c(0, 0, px-1, px-1);
		c.Offset(Ctx.x1 + ((Ctx.X()-c.X())/2), Ctx.y1 + ((Ctx.Y()-c.Y())/2));

		// Box
		pDC->Colour(LColour(L_TEXT));
		pDC->Box(&c);
		c.Inset(1, 1);
		pDC->Colour(LColour(L_WORKSPACE));
		pDC->Rectangle(&c);

		// Value
		if (Value())
		{
			pDC->Colour(LColour(L_TEXT));

			pDC->Line(c.x1+pad, c.y1+pad, c.x2-pad, c.y2-pad);
			pDC->Line(c.x1+pad, c.y1+pad+1, c.x2-pad-1, c.y2-pad);
			pDC->Line(c.x1+pad+1, c.y1+pad, c.x2-pad, c.y2-pad-1);

			pDC->Line(c.x1+pad, c.y2-pad, c.x2-pad, c.y1+pad);
			pDC->Line(c.x1+pad, c.y2-pad-1, c.x2-pad-1, c.y1+pad);
			pDC->Line(c.x1+pad+1, c.y2-pad, c.x2-pad, c.y1+pad+1);
		}
	}

	void OnMouseClick(LMouse &m)
	{
		if (m.Down() && m.Left())
		{
			LList *l = GetList();
			List<LListItem> Sel;
			if (l->GetSelection(Sel) && Sel.Length())
			{
				bool v = !Value();
				Sel.Delete(GetItem());

				for (auto i: Sel)
				{
					if (auto c = GetItemCol(i, GetColumn()))
						c->Value(v);
				}

				Value(v);
				GetItem()->OnColumnNotify(GetColumn(), v);
			}
		}
	}
};

#endif
