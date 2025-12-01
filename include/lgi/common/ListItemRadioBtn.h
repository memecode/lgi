/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A radio button for a list item

#ifndef __LLIST_ITEM_RADIO_H
#define __LLIST_ITEM_RADIO_H

/// A radio button control for use in a LListItem. It will select one option amongst many rows.
/// (Not one option amongst many columns)
class LListItemRadioBtn : public LListItemColumn
{
public:
	enum RadioExclusion
	{
		ListExclusive, // select one option in the list (column based)
		ItemExclusive, // select one option amoungst those in the item (row based)
	}	Type;

	/// Constructor
	LListItemRadioBtn
	(
		/// The host list item.
		LListItem *host,
		/// The column to draw in.
		int column,
		/// Type of exclusion
		RadioExclusion type,
		/// The initial value.
		bool value = false
	)
	:   LListItemColumn(host, column)
	{
		Type = type;
		LListItemColumn::Value(value);
	}

	void OnPaintColumn(ItemPaintCtx &r, int i, LItemColumn *Col)
	{
		LSurface *pDC = r.pDC;
		LRect c(0, 0, 10, 10);
		c.Offset(r.x1 + ((r.X()-c.X())/2), r.y1 + ((r.Y()-c.Y())/2));

		// Box
		pDC->Colour(L_WORKSPACE);
		pDC->FilledCircle(c.x1 + 5, c.y1 + 5, 5);

		pDC->Colour(L_TEXT);
		pDC->Circle(c.x1 + 5, c.y1 + 5, 5);

		// Value
		if (Value())
		{
			pDC->Colour(L_TEXT);
			pDC->FilledCircle(c.x1 + 5, c.y1 + 5, 2);
		}
	}

	void OnMouseClick(LMouse &m)
	{
		if (m.Down() && m.Left())
		{
			Value(!Value());
		}
	}

	int64 Value()
	{
		return LListItemColumn::Value();
	}

	void Value(int64 i)
	{
		// Set me
		LListItemColumn::Value(i);

		if (i)
		{
			// Switch off any other controls in the row / column...
			if (Type == ListExclusive)
			{
				for (auto item: *GetAllItems())
				{
					for (auto c: *item->GetItemCols())
					{
						if (c->GetColumn() == GetColumn())
						{
							auto r = dynamic_cast<LListItemRadioBtn*>(c);
							if (r != this)
							{
								r->Value(0);
								break;
							}
						}
					}
				}
			}
			else if (Type == ItemExclusive)
			{
    			LArray<LListItem*> sel;
    			if (GetList())
    			    GetList()->GetSelection(sel);
    			else
    			    sel.Add(GetItem());

				for (auto item: sel)
				{
    				for (auto col: *item->GetItemCols())
    				{
    					if (auto radioBtn = dynamic_cast<LListItemRadioBtn*>(col))
    					{
        					radioBtn->Value(radioBtn->GetColumn() == GetColumn());
       					}
    				}
    			}
			}

			if (GetList())
				GetList()->SendNotify(LNotifyItemChange);
		}
	}
};

#endif
