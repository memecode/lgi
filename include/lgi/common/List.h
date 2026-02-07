/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A list control

#ifndef __LList_H
#define __LList_H

// Includes
#include "lgi/common/Mem.h"
#include "lgi/common/Array.h"
#include "lgi/common/Popup.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Css.h"
#include "lgi/common/ItemContainer.h"
#include "lgi/common/UnrolledList.h"

class LList;
class LListItem;

// Messages
#define WM_END_EDIT_LABEL			(WM_USER+0x556)

//////////////////////////////////////////////////////////////////////////////

/// View modes for the list control
enum LListMode
{
	LListDetails,
	LListColumns,
	LListSpacial,
};

#if 1
typedef LUnrolledList<LListItem*> LListT;
#else
typedef List<LListItem> LListT;
#endif

class LgiClass LListItemPainter
{
public:
	// Overridable
	virtual void OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c) = 0;
};

class LgiClass LListItemColumn : public LBase, public LItem, public LListItemPainter
{
	LListItem *_Item;
	int _Column;
	int64 _Value = 0;

	void OnPaint(ItemPaintCtx &Ctx) {}

protected:
	LListT *GetAllItems();
	LListItemColumn *GetItemCol(LListItem *i, int Col);

public:
	LListItemColumn(LListItem *item, int col);

	// Other objects
	LListItem *GetItem() { return _Item; }
	LList *GetList();
	LItemContainer *GetContainer();

	// Props
	int GetColumn() { return _Column; }
	void SetColumn(int i) { _Column = i; }
	virtual int64 Value() { return _Value; }
	virtual void Value(int64 i);
};

/// LItem for populating a LList
class LgiClass LListItem : public LItem, public LListItemPainter
{
	friend class LList;
	friend class LListItemColumn;
	friend class LItemColumn;

protected:
	// Data
	class LListItemPrivate *d = NULL;
	LRect Pos;
	LList *Parent = NULL;

	// Methods
	bool GridLines();
	LDisplayString *GetDs(int Col, int FitTo = -1);
	void ClearDs(int Col);

public:
	// Application defined, defaults to 0
	union {
		void *Ptr;
		NativeInt Int;
	}	User = {};

	// Object
	LListItem(const char *initStr = NULL);
	virtual ~LListItem();

	LItemContainer *GetContainer() override;
	/// Get the owning list
	LList *GetList() { return Parent; }
	/// Gets the LListItemColumn's.
	List<LListItemColumn> *GetItemCols();

	// Properties
	
	/// Set the text for a given column
	bool SetText(const char *s, int i=0) override;

	/// \brief Get the text for a given column.
	///
	/// Override this in your LListItem based class to
	/// return the text for a column. Otherwise call SetText to store the text in the
	/// control.
	/// 
	/// \returns A string
	const char *GetText
	(
		/// The index of the column.
		int i = 0
	)	override;

	/// Get the icon index to display in the '0th' column. The image list is stored in
	/// the parent LList.
	int GetImage(int Flags = 0) override;
	/// Sets the icon index
	void SetImage(int i) override;
	/// Returns true if selected
	bool Select() override;
	/// Changes the selection start of the control.
	void Select(bool b) override;
	/// Gets the on screen position of the field at column 'col'
	LRect *GetPos(int Col = -1) override;
	/// True if the item is visible
	bool OnScreen() { return Pos.y1 < Pos.y2; }

	// Methods
	
	/// Update the text cache and display the updated data
	void Update() override;
	/// Moves the item on screen if not visible
	void ScrollTo() override;
	
	// Events;
	void OnMouseClick(LMouse &m) override;
	void OnMeasure(LPoint *Info) override;
	void OnPaint(LSurface *pDC) override { LAssert(0); }
	void OnPaint(LItem::ItemPaintCtx &Ctx) override;
	void OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c) override;

	// Over-ridable
	virtual int Compare(LListItem *To, ssize_t Field = 0)
	{
		LAssert(!"need to override this");
		return 0;
	}
	virtual void OnColumnNotify(int Col, int64 Data) { Update(); }
};

class LListItems
{
protected:
	LListT Items;

public:
	LListItems() : Items(nullptr)
	{
	}

	template<class T>
	bool GetSelection(List<T> &n)
	{
		n.Empty();
		for (auto i : Items)
		{
			if (i->Select())
			{
				T *ptr = dynamic_cast<T*>(i);
				if (ptr)
					n.Insert(ptr);
			}
		}
		return n.Length() > 0;
	}

	template<class T>
	bool GetSelection(LArray<T*> &n)
	{
		n.Empty();
		for (auto i : Items)
		{
			if (i->Select())
			{
				T *ptr = dynamic_cast<T*>(i);
				if (ptr)
					n.Add(ptr);
			}
		}
		return n.Length() > 0;
	}

	template<class T>
	bool GetAll(List<T> &n)
	{
		n.Empty();
		for (auto i : Items)
		{
			T *ptr = dynamic_cast<T*>(i);
			if (ptr)
				n.Insert(ptr);
		}
		return n.Length() > 0;
	}

	template<class T>
	bool GetAll(LArray<T*> &n)
	{
		n.Empty();
		for (auto i : Items)
		{
			T *ptr = dynamic_cast<T*>(i);
			if (ptr)
				n.Add(ptr);
		}
		return n.Length() == Items.Length();
	}

	LListT::I begin()
	{
		return Items.begin();
	}

	LListT::I end()
	{
		return Items.end();
	}
};

/// List widget
class LgiClass LList :
	public LItemContainer,
	public ResObject,
	public LListItems
{
	friend class LListItem;
	friend class LItemColumn;
	friend class LListItemColumn;

	#ifdef WIN32
	HCURSOR Cursor;
	#endif
	
protected:
	class LListPrivate *d;

	// Contents
	ssize_t Keyboard; // index of the item with keyboard focus

	// Flags
	bool EditLabels;
	bool GridLines;

	// Double buffered
	LAutoPtr<LSurface> Buf;

	// Drawing locations
	LRect ItemsPos;
	LRect ScrollX, ScrollY;
	int FirstVisible;
	int LastVisible;
	int CompletelyVisible;

	// Misc
	bool GetUpdateRegion(LListItem *i, LRegion &r);
	LListItem *HitItem(int x, int y, int *Index = 0);
	LRect &GetClientRect();
	void PourAll();
	void UpdateScrollBars();
	void KeyScroll(ssize_t iTo, ssize_t iFrom, bool SelectItems);
	void ClearDs(int Col) override;
	ContainerItemDrop GetItemReorderPos(LPoint ms) override;

public:
	using TItem = LListItem;

	/// Constructor
	LList
	(
		/// The control's ID
		int id,
		/// Left edge position
		int x = 0,
		/// Top edge position
		int y = 0,
		/// The width
		int cx = 100,
		/// The height
		int cy = 100,
		/// An unseen descriptor of the control
		const char *name = NULL
	);
	~LList();

	const char *GetClass() override { return "LList"; }

	// Overridables
	
	/// Called when an item is clicked
	virtual void OnItemClick
	(
		/// The item clicked
		LListItem *Item,
		/// The mouse parameters for the click
		LMouse &m
	);
	/// Called when the user selects an item and starts to drag it
	virtual void OnItemBeginDrag
	(
		/// The item being dragged
		LListItem *Item,
		/// The mouse parameters at the time
		LMouse &m
	);
	/// Called when the user selects an item. If multiple items are selected
	/// in one hit this is only called for the first item. Use GetSelection to
	/// get the extent of the selected items.
	virtual void OnItemSelect
	(
		/// The item selected
		LArray<LListItem*> &Items
	);
	/// Called when a column is dragged somewhere
	virtual void OnColumnDrag
	(
		/// The column index
		int Col,
		/// The mouse parameters at the time
		LMouse &m
	) {}
	/// Called when the column is dropped to a new location
	/// /return true to have the columns reindexed for you
	virtual bool OnColumnReindex
	(
		/// The column dropped
		LItemColumn *Col,
		/// The old index
		int OldIndex,
		/// The new index
		int NewIndex
	)
	{
		return false;
	}
	
	// Events
	
	void OnPaint(LSurface *pDC) override;
	LMessage::Result OnEvent(LMessage *Msg) override;
	// int OnHitTest(int x, int y);
	LCursor GetCursor(int x, int y) override;
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
	void OnPosChange() override;
	bool OnKey(LKey &k) override;
	bool OnMouseWheel(double Lines) override;
	void OnFocus(bool b) override;
	void OnPulse() override;
	bool OnReorderDrop(ContainerItemDrop &dest, ContainerItemsDrag &source) override;

	// Properties
	
	/// Returns whether the user can edit labels
	bool AllowEditLabels() { return EditLabels; }
	/// Sets whether the user can edit labels
	void AllowEditLabels(bool b) { EditLabels = b; }
	/// Returns whether grid lines are drawn
	bool DrawGridLines() { return GridLines; }
	/// Sets whether grid lines are drawn
	void DrawGridLines(bool b) { GridLines = b; }

	// Methods
	
	/// Get the display mode.
	/// \sa LListMode
	LListMode GetMode();
	/// Set the display mode.
	/// \sa LListMode
	void SetMode(LListMode m);	

	/// Returns the index of the first selected item
	int64 Value() override;
	/// Selects the item at index 'i'
	void Value(int64 i) override;
	/// Selects 'obj'
	bool Select(LListItem *Obj);
	/// Gets the first selected object
	LListItem *GetSelected();
	/// Scrolls the view to the first selected item if not in view
	void ScrollToSelection();
	/// Clears the text cache for all the items and repaints the screen.
	void UpdateAllItems() override;
	/// Select or deselect all items..
	bool SelectAll(bool select = true) override;
	/// Gets the number of items.
	size_t Length() { return Items.Length(); }

	/// Returns true if the list is empty
	bool IsEmpty() { return Items.Length() == 0; }
	/// Deletes the item at index 'Index'
	bool Delete(ssize_t Index);
	/// Deletes the item 'p'
	virtual bool Delete(LListItem *p);
	/// Remove the item 'Obj' but don't delete it
	virtual bool Remove(LListItem *Obj);
	/// Inserts the item 'p' at index 'Index'.
	bool Insert
	(
		/// The item to insert
		LListItem *p,
		/// The index to insert at or -1 for append
		int Index = -1,
		/// True if you want the list to update immediately. If you are inserting a lot of items quickly
		/// then you should pass false here and then update just once at the end of the insertions.
		bool Update = true
	);
	template<class T>
	bool Insert(LArray<T*> &items, int Index = -1, bool Update = true)
	{
		List<LListItem> lst(items);
		return Insert(lst, Index, Update);
	}
	/// Insert a list of item
	virtual bool Insert
	(
		/// The items to insert
		List<LListItem> &l,
		/// The starting index to insert at
		int Index = -1,
		/// True if you want the list to update immediately. If you are inserting a lot of item list quickly
		/// then you should pass false here and then update just once at the end of the insertions.
		bool Update = true
	);
	/// Return true if the item 'Obj' is in the list
	bool HasItem(LListItem *Obj);
	/// Return the index of the item 'Obj' or -1 if not present
	int IndexOf(LListItem *Obj);
	/// Returns the item at index 'Index'
	LListItem *ItemAt(size_t Index);

	/*
	/// Sort the list
	void Sort
	(
		/// The comparison function. Should return a integer greater then > 0 if the first item item is greater in value.
		LListCompareFunc Compare,
		/// User defined 32-bit value passed through to the 'Compare' function
		NativeInt Data = 0
	);
	*/

	/// Sort the list
	template<typename User>
	[[deprecated]]
	void Sort
	(
		/// The comparison function. Should return a integer greater then > 0 if the first item item is greater in value.
		int (*Compare)(LListItem *a, LListItem *b, User data),
		/// User defined value passed through to the 'Compare' function
		User Data = 0
	)
	{
		if (!Compare || !Lock(_FL))
			return;

		LListItem *Kb = Keyboard >= 0 && Keyboard < (ssize_t)Items.Length() ? Items[Keyboard] : NULL;
		Items.Sort([Compare, Data](auto a, auto b)
		{
			return Compare(a, b, Data);
		});
		Keyboard = Kb ? (int)Items.IndexOf(Kb) : -1;
		Unlock();
		Invalidate(&ItemsPos);
	}

	bool SetSort(SortParam sort, bool reorderItems = true, bool setMark = true) override;

	/// Sort the list
	void Sort
	(
		/// The comparison function. Should return a integer greater then > 0 if the first item item is greater in value.
		std::function<int(LListItem*, LListItem*)> compare
	)
	{
		if (!compare || !Lock(_FL))
			return;

		auto Kb = Keyboard >= 0 && Keyboard < (ssize_t)Items.Length() ? Items[Keyboard] : NULL;
		Items.Sort(compare);
		Keyboard = Kb ? Items.IndexOf(Kb) : -1;
		Unlock();
		Invalidate(&ItemsPos);
	}
	
    /// Sort by a textual comparison on the given column
	void Sort(int Column) override
	{
		if (Items.Length() == 0)
			return;
		if (!Lock(_FL))
			return;

		auto Kb = Items[Keyboard];
		Items.Sort
		(
			[Column](auto *a, auto *b)
			{
				auto aTxt = a->GetText(Column);
				auto bTxt = b->GetText(Column);
				auto cmp = Stricmp(aTxt, bTxt);
				// LgiTrace("item.cmp: '%s' '%s' = %i, ptr=%i\n", aTxt, bTxt, cmp, (int)(b - a));
				return cmp ? cmp : b - a;
			}
		);
		Keyboard = Kb ? (int)Items.IndexOf(Kb) : -1;
		Unlock();
		Invalidate(&ItemsPos);
	}
	
	/// Sort by the compare function of the items:
	void Sort() override
	{
		if (Items.Length() == 0)
			return;
		if (!Lock(_FL))
			return;

		auto Kb = Items[Keyboard];
		Items.Sort
		(
			[this](auto *a, auto *b) -> int
			{
				return a->Compare(b, sortParam.Col);
			}
		);
		Keyboard = Kb ? (int)Items.IndexOf(Kb) : -1;
		Unlock();
		Invalidate(&ItemsPos);
	}

	/// Removes all items from list and delete the objects.
	virtual void Empty();
	/// Removes all references to externally owned items. Doesn't delete objects.
	virtual void RemoveAll();

	// Impl
	int GetContentSize(int ColumnIdx) override;
	bool GetItems(LArray<LItem*> &arr, bool selectedOnly = false) override
	{
		if (selectedOnly)
			GetSelection(arr);
		else
			GetAll(arr);
		return arr.Length() > 0;
	}
};

#endif
