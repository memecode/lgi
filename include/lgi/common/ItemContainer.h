#ifndef _ITEM_CONTAINER_H_
#define _ITEM_CONTAINER_H_

#include "lgi/common/Popup.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Css.h"
#include "lgi/common/ImageList.h"

class LItemContainer;
#define DragColumnColour				LColour(L_LOW)

/// Base class for items in widget containers
class LgiClass LItem : virtual public LEventsI
{
protected:
    LAutoPtr<LCss> Css;
	int SelectionStart, SelectionEnd;
    
public:
	/// Painting context
	struct ItemPaintCtx : public LRect
	{
		/// The surface to draw on
		LSurface *pDC;

		/// Current foreground colour
		LColour Fore;
		/// Current background colour
		LColour TxtBack, Back;
		
		// Horizontal alignment of content
		LCss::Len Align;
		
		/// Number of columns
		int Columns;
		/// Width of each column
		int *ColPx;
	};
	
	LItem();
	~LItem();

    LItem &operator =(LItem &i)
    {
		if (i.GetCss())
		{
			LCss *c = GetCss(true);
			if (c)
			{
				*c = *i.GetCss();
			}
		}
        return *this;
    }

	virtual const char *GetClass() { return "LItem"; }
	virtual LItemContainer *GetContainer() = 0;

	// Events
	
	/// Called when the item is selected
	virtual void OnSelect() {}
	/// Called when the item is clicked
	virtual void OnMouseClick(LMouse &m) override {}
	/// Called when the item needs painting
	virtual void OnPaint(ItemPaintCtx &Ctx) = 0;
	/// Called when the item is dragged
	virtual bool OnBeginDrag(LMouse &m) { return false; }
	/// Called when the owning container needs to know the size of the item
	virtual void OnMeasure(LPoint *Info) {}
	/// Called when the item is inserted into a new container
	virtual void OnInsert() {}
	/// Called when the item is removed from it's container
	virtual void OnRemove() {}

	// Methods

	/// Call to tell the container that the data displayed by the item has changed
	virtual void Update() {}
	/// Moves the item on screen
	virtual void ScrollTo() {}
	/// Shows a editable label above the item allowing the user to change the value associated with the column 'Col'
	virtual LView *EditLabel(int Col = -1);
	/// Event called when the edit label ends
	virtual void OnEditLabelEnd();
	/// Sets the default selection of text when editing a label
	void SetEditLabelSelection(int SelStart, int SelEnd); // call before 'EditLabel'

	// Data

	/// True if the item is selected
	virtual bool Select() { return false; }
	/// Select/Deselect the item
	virtual void Select(bool b) {}
	/// Gets the text associated with the column 'Col'
	virtual const char *GetText(int Col = 0) { return 0; }
	/// Sets the text associated with the column 'Col'
	virtual bool SetText(const char *s, int Col=0) { return false; }
	/// Gets the icon index
	virtual int GetImage(int Flags = 0) { return -1; }
	/// Sets the icon index
	virtual void SetImage(int Col) {}
	/// Gets the position
	virtual LRect *GetPos(int Col = -1) { return 0; }
	/// Gets the font for the item
	virtual LFont *GetFont() { return 0; }
	
	/// Reads / writes list item to XML
	virtual bool XmlIo(class LXmlTag *Tag, bool Write) { return false; }

	bool OnScriptEvent(LViewI *Ctrl) { return false; }
	LMessage::Result OnEvent(LMessage *Msg) override { return 0; }
	void OnMouseEnter(LMouse &m) override {}
	void OnMouseExit(LMouse &m) override {}
	void OnMouseMove(LMouse &m) override {}
	bool OnMouseWheel(double Lines) override { return false; }
	bool OnKey(LKey &k) override { return false; }
	void OnAttach() override {}
	void OnCreate() override {}
	void OnDestroy() override {}
	void OnFocus(bool f) override {}
	void OnPulse() override {}
	void OnPosChange() override {}
	bool OnRequestClose(bool OsShuttingDown) override { return false; }
	int OnHitTest(int x, int y) override { return 0; }
	void OnChildrenChanged(LViewI *Wnd, bool Attaching) override {}
	int OnNotify(LViewI *Ctrl, const LNotification &n) override { return 0; }
	int OnCommand(int Cmd, int Event, OsView Wnd) override { return 0; }
	void OnPaint(LSurface *pDC) override { LAssert(0); }

	// Style access
	LCss *GetCss(bool Create = false)
	{
		if (!Css && Create) Css.Reset(new LCss);
		return Css;
	}
};

/// The popup label for LItem's
class LItemEdit : public LPopup
{
	class LItemEditPrivate *d;

public:
	LItemEdit(LView *parent, LItem *item, int index, int selstart, int selend);
	~LItemEdit();
	
	LItem *GetItem();
	void OnPaint(LSurface *pDC) override;
	int OnNotify(LViewI *v, const LNotification &n) override;
	void Visible(bool i) override;
	LMessage::Result OnEvent(LMessage *Msg) override;

	bool OnKey(LKey &k) override;
	void OnFocus(bool f) override;
};

/// Item container column
class LgiClass LItemColumn :
	public ResObject,
	public LCss
{
	class LItemColumnPrivate *d;
	friend class LDragColumn;
	friend class LListItem;
	friend class LItemContainer;

public:
	LItemColumn(LItemContainer *parent, const char *name, int width);
	virtual ~LItemColumn();

	// properties
	bool InDrag();
	LRect GetPos();
	void SetPos(LRect &r);
	
	/// Sets the text
	void Name(const char *n);
	/// Gets the text
	char *Name();

	/// Sets the width
	void Width(int i);
	/// Gets the width
	int Width();

	//////////////////////////////////////////////////////
	#define COLUMN_FLAGS() \
		_(HasText)  \
		_(HasImage) \
		_(UpArrow)  \
		_(DownArrow)
	#define _(name) \
		bool name(); \
		void name(bool b);
	COLUMN_FLAGS()
	#undef _
	bool HasSort()   { return UpArrow() | DownArrow(); }

	//////////////////////////////////////////////////////

	/// Sets the icon to display
	void Icon(LSurface *i, bool Own = true);
	/// Gets the icon displayed
	LSurface *Icon();

	/// True if clicked
	bool Value();
	/// Set clicked
	void Value(bool i);
	
	/// Sets the index into the parent containers LImageList
	void Image(int i);
	/// Gets the index into the parent containers LImageList
	int Image();

	/// true if resizable
	bool Resizable();
	/// Sets whether the user can resize the column
	void Resizable(bool i);

	/// Returns the index of the column if attached to a list
	int GetIndex();
	/// Returns the size of the content in this column
	int GetContentSize();
	/// Returns the list
	LItemContainer *GetList();

	/// Paint the column header.
	void OnPaint(LSurface *pDC, LRect &r);
	/// Draws the just the icon, text and mark.
	void OnPaint_Content(LSurface *pDC, LRect &r, bool FillBackground); 
};

#define DEFAULT_COLUMN_SPACING		12

class LgiClass LSortable
{
public:
    struct SortParam
	{
		constexpr static int INVALID = -1;

        int Col = INVALID;
        bool Ascend = true;

		operator bool() const
		{
			return Col != INVALID;
		}

		bool operator ==(const SortParam &sp) const
		{
			return Col == sp.Col && Ascend == sp.Ascend;
		}

		bool operator !=(const SortParam &sp) const
		{
			return Col != sp.Col || Ascend != sp.Ascend;
		}

		void Empty()
		{
			Col = INVALID;
		}
    };

protected:
	SortParam sortParam, sortMark;

public:
	/// Sets/clears the sorting mark
	virtual void SetSortingMark(
		/// Index of the column, or -1 to unset
		int ColIdx = -1,
		/// Which mark to set
		bool Up = true)
	{
		SetSortingMark({ ColIdx, !Up });
	}
	
	virtual void SetSortingMark(SortParam sort)
	{
		sortMark = sort;
	}

	/// \returns the current sorting params:
	virtual SortParam GetSort() { return sortParam; }

	/// Set the sorting params, and optionally reorder the items
	virtual bool SetSort(SortParam sort, bool reorderItems = true, bool setMark = true)
	{
		sortParam = sort;

		if (setMark)
			SetSortingMark(sort);
		if (reorderItems)
			Sort();

		return true;
	}
		
	/// Sorts items via the specified column
	virtual void Sort(int column) = 0;
		
	/// Sorts items via the LItem::Compare function
	virtual void Sort() = 0;
};

class LgiClass LItemContainer :
	public LLayout,
	public LImageListOwner,
	public LDragDropSource,
	public LDragDropTarget,
	virtual public LSortable
{
	friend class LItemColumn;
	friend class LItem;
	friend class LItemEdit;

public:
	struct ColInfo
	{
		int Idx, ContentPx, WidthPx;
		LItemColumn *Col;
		int GrowPx() { return WidthPx < ContentPx ? ContentPx - WidthPx : 0; }
	};
	
	struct ColSizes
	{
		LArray<ColInfo> Info;
		int FixedPx;
		int ResizePx;
	};
	
	enum ContainerDragMode
	{
		DRAG_NONE,
		SELECT_ITEMS,
		RESIZE_COLUMN,
		DRAG_COLUMN,
		CLICK_COLUMN,
		TOGGLE_ITEMS,
		CLICK_ITEM,
	};

	/* This defines the default handling of dragging an item.
	   ITEM_DRAG_USER - Means the application has to define what
						happens when the user drags and item. Ie
						there is no default.
	   ITEM_DRAG_REORDER - The drag handler will allow the user to
						reorder the items in the container.
	   ITEM_DRAG_OVER - The drag handler allows the user to
						drop the item onto existing items. This is
						more for tree controls where the item is
						added as a child of the target item.
	   ITEM_DEPTH_CHANGE - Allow changes of depth in the tree control.
	   					If this is on items can change their parent
	   					during a reorder drop operation.

	   If this flag is enabled the view will manage the drag of items
	   using `ContainerItemsFormat` and `ContainerItemsDrag`. And
	   finally the drop will be handled via `OnReorderDrop`.
	*/
	enum ItemDragFlags
	{
		ITEM_DRAG_USER		= 0,
		ITEM_DRAG_REORDER	= 1 << 0,
		ITEM_DRAG_OVER		= 1 << 1,
		ITEM_DEPTH_CHANGE	= 1 << 2,
	};
	
	/// This is the drag and drop format that uses `ContainerItemsDrag`
	/// \sa SetDragItem
	constexpr static const char *ContainerItemsFormat = "com.memecode.lgi-item";

	/// The data format for `ContainerItemsFormat`
	/// A list of items being dragged over a container view
	/// \sa SetDragItem
	struct ContainerItemsDrag
	{
		// Owner view
		LItemContainer *view;
		// Count of the items in `item`.
		uint32_t items;
		// Array of size `items`
		LItem *item[1];

		size_t Sizeof() { return Sizeof(items); }
		static size_t Sizeof(uint32_t items)
		{
			return sizeof(ContainerItemsDrag) + (items > 1 ? sizeof(LItem*) * (items - 1) : 0);
		}
		
		typedef LAutoPtr<ContainerItemsDrag, false, true> Auto;
		static Auto New(uint32_t items)
		{
			Auto obj((ContainerItemsDrag*) calloc(Sizeof(items), 1));
			if (obj)
				obj->items = items;
			return obj;
		}
	};

	/// This struct is the drop format for a reorder operation
	/// Describes where a `ContainerItemsFormat` is dropped on the view
	struct ContainerItemDrop
	{
		// One or both of these cases will be true:
	
			// 1) Is the pointer is roughly between two other items, `prev` and `next` will be set.
			// The `ITEM_DRAG_REORDER` flag needs to be set to see this case.
			LItem *prev = NULL, *next = NULL;
		
			// 2) If the point is over an item, and not near the edge, then `over` will be set.
			// The `ITEM_DRAG_OVER` flag needs to be set to see this case.
			LItem *over = NULL;
		
		// The paint region for case 1
		LRect pos = {0, 0, -1, -1};

		operator bool() const
		{
			return (prev != nullptr || next != nullptr) && pos.Valid();
		}
		bool operator==(const ContainerItemDrop &i)
		{
			return prev == i.prev &&
				next == i.next &&
				pos == i.pos;
		}
		bool operator!=(const ContainerItemDrop &i)
		{
			return !(*this == i);
		}
	};

protected:
	/// The handler for re-ordering drops.
	/// \returns true if the screen should be updated
	/// \sa SetDragItem
	virtual bool OnReorderDrop(ContainerItemDrop &dest, ContainerItemsDrag &source) { return false; }

	/// \returns the item drop details for a given point
	/// \sa SetDragItem
	virtual ContainerItemDrop GetItemReorderPos(LPoint pt) { return ContainerItemDrop(); }

	ContainerDragMode DragMode = DRAG_NONE;
	ItemDragFlags DragItem = ITEM_DRAG_USER;
	LAutoPtr<ContainerItemDrop> DropStatus;
	
	LRect ColumnHeader;
	int ColClick = -1;
	LMouse ColMouse;
	LItemEdit *ItemEdit = NULL;

	LArray<LItemColumn*> Columns;
	LAutoPtr<LItemColumn> IconCol;
	class LDragColumn *DragCol = NULL;

	/// Returns size information for columns
	void GetColumnSizes(ColSizes &cs);
	
	/// Paints all the column headings
	void PaintColumnHeadings(LSurface *pDC);
	
	/// This clears all the display string cache for a given column
	virtual void ClearDs(int Col) = 0;

public:
	LItemContainer();
	virtual ~LItemContainer();

	#define DEFINE_FLAG(name, default, ...) \
		private: \
			bool b##name = default; \
		public: \
			bool name() const { return b##name; } \
			void name(bool b) __VA_ARGS__ { b##name = b; }

	// Props
	DEFINE_FLAG(AskText, false)
	DEFINE_FLAG(AskImage, false, override)
	DEFINE_FLAG(OwnList, false)
	DEFINE_FLAG(InDragOp, false)
	DEFINE_FLAG(MultiSelect, false)
	DEFINE_FLAG(InsideDragOp, false)
	DEFINE_FLAG(DropOnItem, false)
	DEFINE_FLAG(DropBetweenItems, false)
	DEFINE_FLAG(ColumnHeaders, true)

	/// Adds a column to the list
	LItemColumn *AddColumn
	(
		/// The text for the column or NULL for no text
		const char *Name,
		/// The width of the column
		int Width = 50,
		/// The index to insert at, or -1 to append to the end
		int Where = -1
	);
	
	/// Adds a preexisting column to the control
	bool AddColumn
	(
		/// The column object. The object once added is owned by the LList
		LItemColumn *Col,
		/// The location to insert or -1 to append
		int Where = -1
	);
	
	/// Deletes a column from the LList
	bool DeleteColumn(LItemColumn *Col);
	
	/// Deletes all the columns of the LList
	void EmptyColumns();
	
	/// Returns the column at index 'Index'
	LItemColumn *ColumnAt(unsigned i) { return i < Columns.Length() ? Columns[i] : NULL; }
	
	/// Returns the column at horizontal offset 'x', or -1 if none matches.
	int ColumnAtX(int X, LItemColumn **Col = 0, int *Offset = 0);
	
	/// Returns the number of columns
	int GetColumns() { return (int)Columns.Length(); }
	
	/// Starts a column d'n'd operation with the column at index 'Index'
	/// \sa OnColumnReindex is called when the user drops the column
	void DragColumn(int Index);
	
	/// Returns the last column click info
	bool GetColumnClickInfo(int &Col, LMouse &m);

	int HitColumn(int x, int y, LItemColumn *&Resize, LItemColumn *&Over);

	/// Called when a column is clicked
	virtual void OnColumnClick
	(
		/// The index of the column
		int Col,
		/// The mouse parameters at the time
		LMouse &m
	);

	virtual void UpdateAllItems() = 0;
	virtual int GetContentSize(int ColumnIdx) = 0;
	virtual bool GetItems(LArray<LItem*> &arr, bool selectedOnly = false) = 0;

	/// Resizes all the columns to their content, allowing a little extra space for visual effect
	virtual void ResizeColumnsToContent(int Border = DEFAULT_COLUMN_SPACING);

	LMessage::Result OnEvent(LMessage *Msg) override;

	LItemContainer &operator =(const LItemContainer &i)
	{
		LAssert(!"Not impl..");
		return *this;
	}

	// Sorting:
	void SetSortingMark(SortParam sort) override;

	// Drag and drop support:
	
	/// Gets the current drag item flags
	ItemDragFlags GetDragItem() { return DragItem; }

	/// Sets the drag item flags. This enables the user to organize the items in the container
	/// via drag and drop. Either just reordering or also changing the heirarchy of the items
	/// in a tree control.
	/// 
	/// The format for the drag and drop is `ContainerItemsFormat` and the data in that
	/// format is described by `ContainerItemsDrag`. Finally when the user drops the items
	/// the control handles the drop via `OnReorderDrop`.
	void SetDragItem(ItemDragFlags flags) { DragItem = flags; }

	bool GetFormats(LDragFormats &Formats) override;
	bool GetData(LArray<LDragData> &Data) override;
	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState) override;
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState) override;
	void OnDragExit() override;
	void OnCreate() override;
	virtual int OnDragError(const char *Msg, const char *file, int line);
};

////////////////////////////////////////////////////////////////////////////////////////////
#define DRAG_COL_ALPHA					0xc0
#define LINUX_TRANS_COL					0

class LDragColumn : public LWindow
{
	LItemContainer *List;
	LItemColumn *Col;
	int Index;
	int Offset;
	LPoint ListScrPos;
	
	#ifdef LINUX
	LSurface *Back;
	#endif

public:
	int GetOffset() { return Offset; }
	int GetIndex() { return Index; }
	LItemColumn *GetColumn() { return Col; }

	LDragColumn(LItemContainer *list, int col);
	~LDragColumn();

	void OnPaint(LSurface *pScreen);
};


#endif
