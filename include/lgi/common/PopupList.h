#ifndef _POPUP_LIST_H_
#define _POPUP_LIST_H_

#include "lgi/common/List.h"
#include "lgi/common/Css.h"
#include "lgi/common/CssTools.h"

/// This class displays a list of search results in a 
/// popup connected to an editbox. To use override:
/// - ToString
/// - OnSelect
///
/// 'T' is the type of record that maps to one list item.
/// The user can select an item with [Enter] or click on it.
/// [Escape] cancels the popup.
template<typename T>
class LPopupList : public LPopup
{
public:
	enum {
		IDC_BROWSE_LIST = 50,
	};
	enum PositionType
	{
		PopupAbove,
		PopupBelow,
	};

	class Item : public LListItem
	{
	public:
		T *Value;
		
		Item(T *val = NULL)
		{
			Value = val;
		}
	};

protected:
	LList *Lst = NULL;
	LViewI *Edit = NULL;
	bool Registered = false;
	PositionType PosType;

	LWindow *HookWnd()
	{
		#if defined(LGI_CARBON)// || defined(__GTK_H__)
		return GetWindow();
		#else
		return Edit->GetWindow();
		#endif
	}

public:
	LPopupList(LViewI *edit, PositionType pos, int width = 200, int height = 300) :
		LPopup(edit->GetLView())
	{
		PosType = pos;

		LRect r(width - 1, height - 1);
		Edit = edit;
		SetPos(r);
		AddView(Lst = new LList(IDC_BROWSE_LIST, r.x1+1, r.y1+1, r.X()-3, r.Y()-3));
		Lst->Sunken(false);
		Lst->AddColumn("Name", r.X());
		Lst->ColumnHeaders(false);
		Lst->MultiSelect(false);
		
		// Set default border style...
		if (auto Css = GetCss(true))
		{
			LCss::BorderDef b(Css, "1px solid #888;");
			Css->Border(b);
		}

		Attach(Edit);
	}
	
	~LPopupList()
	{
		auto Wnd = HookWnd();
		if (Wnd && Registered)
			Wnd->UnregisterHook(this);
	}

	const char *GetClass() { return "LPopupList"; }

	// Events:
	// ------------------------------------------------------------------------
	
	/// Override this to convert an object to a string for the list items
	virtual LString ToString(T *Obj) = 0;
	
	/// Override this to handle the selection of an object
	virtual void OnSelect(T *Obj) = 0;
	virtual void OnSelect(LListItem *Item) {}
	
	// Implementation:
	// ------------------------------------------------------------------------	
	void SetPosType(PositionType t)
	{
		PosType = t;
	}
	
	void AdjustPosition()
	{
		// Set position relative to editbox
		LRect r = GetPos();
		LPoint p(0, PosType == PopupAbove ? 0 : Edit->Y());
		Edit->PointToScreen(p);
		if (PosType == PopupAbove)
			r.Offset(p.x - r.x1, (p.y - r.Y()) - r.y1);
		else
			r.Offset(p.x - r.x1, p.y - r.y1);

		SetPos(r);				
	}
	
	bool SetItems(LArray<T*> &a)
	{
		Lst->Empty();
		
		List<LListItem> ins;
		for (auto obj: a)
		{
			if (!obj)
				continue;

			if (auto li = new Item(obj))
			{
				if (auto s = ToString(obj))
				{
					li->SetText(s);
					ins.Insert(li);
				}
				else LAssert(!"ToString failed.");
			}
			else LAssert(!"Alloc failed.");
		}
		
		return Lst->Insert(ins);
	}
	
	void OnPaint(LSurface *pDC)
	{
		// Draw the CSS border... (the default value is set in the constructor)
		LCssTools t(GetCss(true), GetFont());
		LRect c = GetClient();
		c = t.PaintBorder(pDC, c);
		
		// Move Lst if needed...
		LRect r = Lst->GetPos();
		if (r != c)
			Lst->SetPos(c);
	}

	bool Visible()
	{
		return LPopup::Visible();
	}

	void Visible(bool i)
	{
		if (i)
			AdjustPosition();
			
		LPopup::Visible(i);

		if (i)
		{
			AttachChildren();
			OnPosChange();

			auto Wnd = HookWnd();
			if (Wnd && !Registered)
			{
				Registered = true;
				Wnd->RegisterHook(this, LKeyEvents);
			}

			#ifdef WINNATIVE
				Edit->Focus(true);
			#else
				Lst->Focus(true);
			#endif
		}
	}
	
	void OnChange()
	{
		auto Str = Edit->Name();
		bool Has = ValidStr(Str) && Lst->Length();
		bool Vis = Visible();
		// printf("%s:%i - PopupLst, Str=%s, Len=%i, has=%i vis=%i\n", _FL, Str, (int)Lst->Length(), Has, Vis);
		if (Has ^ Vis)
			Visible(Has);
	}

	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		if (Lst &&
			Ctrl == Edit &&
			(n.Type == LNotifyValueChanged || n.Type == LNotifyDocChanged))
		{
			OnChange();
		}
		else if (Ctrl == Lst)
		{
			if (n.Type == LNotifyReturnKey ||
				n.Type == LNotifyItemClick)
			{
				auto s = Lst->GetSelected();
				Item *Sel = dynamic_cast<Item*>(s);
				if (Sel)
					OnSelect(Sel->Value);
				else
					OnSelect(s);
				
				Visible(false);
			}
		}
		
		return 0;
	}

	bool OnViewKey(LView *v, LKey &k)
	{
		if (!Visible())
			return false;

		#if 0
		LString s;
		s.Printf("%s:%i - OnViewKey vis=%i", _FL, Visible());
		k.Trace(s);
		#endif
		
		switch (k.vkey)
		{
			case LK_TAB:
			{
				Visible(false);
				return false;
			}
			case LK_ESCAPE:
			{
				if (!k.Down())
				{
					Visible(false);
				}
				return true;
				break;
			}
			case LK_RETURN:
			{
				if (Lst)
					Lst->OnKey(k);
				return true;
				break;
			}
			case LK_UP:
			case LK_DOWN:
			case LK_PAGEDOWN:
			case LK_PAGEUP:
			{
				if (!k.IsChar)
				{
					if (Lst)
						Lst->OnKey(k);
					
					return true;
				}
				break;
			}
		}

		#if defined(MAC) && !defined(__GTK_H__)
		return Edit->OnKey(k);
		#else
		return false;
		#endif
	}
};

#endif
