/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A tree/heirarchy control

#ifndef __GTREE2_H
#define __GTREE2_H

#include "lgi/common/ItemContainer.h"
#include <functional>

enum LTreeItemRect
{
	TreeItemPos,
	TreeItemThumb,
	TreeItemText,
	TreeItemIcon
};

class LTree;
class LTreeItem;

class LgiClass LTreeNode
{
	friend class LTree;
	friend class LTreeItem;
	
protected:
	LTree *Tree = NULL;
	LTreeNode *Parent = NULL;
	List<LTreeItem> Items;

	virtual LRect *Pos() { return NULL; }
	virtual void _ClearDs(int Col);
	void _Visible(bool v);
	void SetLayoutDirty();
	bool ReorderPos(LItemContainer::ContainerItemDrop &drop, LPoint &pt, int depth);

public:
	class LgiClass Locker {
		LTree *tree = nullptr;
		bool locked = false;
	public:
		Locker(LTree *t, const char *file, int line);
		~Locker();
		operator bool() const { return locked || !tree; }
		void Unlock();
	};
	Locker ScopedLock(const char *file, int line)
	{
		return Locker(Tree, file, line);
	}

	LTreeNode();
	virtual ~LTreeNode();

	/// Inserts a tree item as a child at 'Pos'
	LTreeItem *Insert(LTreeItem *Obj = NULL, ssize_t Pos = -1);
	/// Removes this node from it's parent, for permanent separation.
	void Remove();
	/// Detachs the item from the tree so it can be re-inserted else where.
	void Detach();
	/// Gets the node after this one at the same level.
	LTreeItem *GetNext();
	/// Gets the node before this one at the same level.
	LTreeItem *GetPrev();
	/// Gets the first child node.
	LTreeItem *GetChild();
	/// Gets the parent of this node.
	LTreeNode *GetParent() { return Parent; }
	/// Gets the owning tree. May be NULL if not attached to a tree.
	LTree *GetTree() const { return Tree; }
	/// Returns true if this is the root node.	
	bool IsRoot();
	/// Returns the index of this node in the list of item owned by it's parent.
	ssize_t IndexOf();
	/// \returns number of child.
	size_t Length();
	/// \returns if the object is in the tree
	bool HasItem(LTreeItem *obj, bool recurse = true);

	List<LTreeItem>::I begin()
	{
		return Items.begin();
	}
	
	List<LTreeItem>::I end()
	{
		return Items.end();
	}

	/// Calls a function for each item in the tree.
	/// \returns true if the whole tree was visited
	bool ForEach(
		/// Can return false to stop propagation
		std::function<bool(LTreeItem*)> Fn,
		/// [Optional] count of items visited
		int *Count = nullptr);

	/// Returns a valid pointer if this is a child tree item.
	/// NULL for the root Tree.
	virtual LTreeItem *IsItem() { return NULL; }

	/// Gets the expanded state of the tree item
	virtual bool Expanded() { return false; }

	/// Sets the expanded state of the tree item
	virtual void Expanded(bool b) {}

	/// Called to set the visible state of the node
	virtual void OnVisible(bool v) {}
};

/// The item class for a tree. This defines a node in the heirarchy.
class LgiClass LTreeItem :
	public LItem,
	public LTreeNode,
	virtual public LSortable
{
	friend class LTree;
	friend class LTreeNode;

protected:
	class LTreeItemPrivate *d;

	// Private methods
	void _RePour();
	void _Pour(LPoint *Limit, int ColumnPx, int Depth, bool Visible);
	void _Remove();
	void _MouseClick(LMouse &m);
	void _SetTreePtr(LTree *t);
	LTreeItem *_HitTest(int x, int y, bool Debug = false);
	LRect *_GetRect(LTreeItemRect Which);
	LPoint _ScrollPos();
	LTreeItem *IsItem() override { return this; }
	LRect *Pos() override;

	virtual void _PourText(LPoint &Size);
	virtual void _PaintText(LItem::ItemPaintCtx &Ctx);
	void _ClearDs(int Col) override;
	virtual void OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c);
	int GetColumnSize(int Col);

protected:
	LString::Array Str;
	int Sys_Image = -1;

public:
	LTreeItem(const char *initStr = NULL);
	virtual ~LTreeItem();

	LItemContainer *GetContainer() override;
	
	/// \brief Get the text for the node
	///
	/// You can either return a string stored internally to your
	/// object by implementing this function in your item class 
	/// or use the SetText function to store the string in this 
	/// class.
	const char *GetText(int i = 0) override;
	/// \brief Sets the text for the node.
	///
	/// This will allocate and store the string in this class.
	bool SetText(const char *s, int i=0) override;
	/// Returns the icon index into the parent tree's LImageList.
	int GetImage(int Flags = 0) override;
	/// Sets the icon index into the parent tree's LImageList.
	void SetImage(int i) override;
	/// Tells the item to update itself on the screen when the
	/// LTreeItem::GetText data has changed.
	void Update() override;
	/// Returns true if the tree item is currently selected.
	bool Select() override;
	/// Selects or de-selects the tree item.
	void Select(bool b) override;
	/// Returns true if the node has children and is open.
	bool Expanded() override;
	/// Opens or closes the node to show or hide the children.
	void Expanded(bool b) override;
	/// Scrolls the tree view so this node is visible.
	void ScrollTo() override;
	/// Gets the bounding box of the item.
	LRect *GetPos(int Col = -1) override;
	/// True if the node is the drop target
	bool IsDropTarget();
	/// \returns true if visible
	bool Visible();

	/// Called when the node expands/contracts to show or hide it's children.
	virtual void OnExpand(bool b);

	/// Paints the item
	void OnPaint(ItemPaintCtx &Ctx) override;
	void OnPaint(LSurface *pDC) override { LAssert(0); }

	// Sorting:
		virtual int Compare(LTreeItem *To, ssize_t Field = 0)
		{
			LAssert(!"need to override this");
			return 0;
		}
		bool SetSort(SortParam sort, bool reorderItems = true, bool setMark = true) override;

		template<typename T>
		[[deprecated]]
		bool Sort(int (*Compare)(LTreeItem*, LTreeItem*, T user_param), T user_param = 0)
		{
			// FIXME: needs locking... but tree is not defined...
			// auto t = GetTree();
			if (!Compare)
				return false;
			// if (!t->Lock(_FL)) return false;

			Items.Sort([Compare, user_param](auto a, auto b)
				{
					return Compare(a, b, user_param);
				});
			SetLayoutDirty();

			// t->Unlock();
			return true;
		}

		void Sort(std::function<int(LTreeNode*, LTreeNode*)> compare);
		void Sort(int Column) override;
		void Sort() override;
};

/// A tree control.
class LgiClass LTree :
	public LItemContainer,
	public ResObject,
	public LTreeNode
{
	friend class LTreeItem;
	friend class LTreeNode;

	class LTreePrivate *d = nullptr;

	// Private methods
	void _Pour();
	void _OnSelect(LTreeItem *Item);
	void _Update(LRect *r = 0, bool Now = false);
	void _UpdateBelow(int y, bool Now = false);
	void _UpdateScrollBars();
	List<LTreeItem>	*GetSelLst();

protected:
	// Options
	bool Lines;
	bool Buttons;
	bool LinesAtRoot;
	bool EditLabels;
	
	LRect rItems;
	
	LTreeItem *GetAdjacent(LTreeItem *From, bool Down);
	void OnDragEnter() override;
	void ClearDs(int Col) override;
	bool OnReorderDrop(ContainerItemDrop &dest, ContainerItemsDrag &source) override;
	ContainerItemDrop GetItemReorderPos(LPoint ms) override;
	void OnDragExit() override;
	
public:
	using TItem = LTreeItem;

	LTree(int id, int x = 0, int y = 0, int cx = 100, int cy = 100, const char *name = NULL);
	~LTree();

	const char *GetClass() override { return "LTree"; }

	/// Called when an item is clicked
	virtual void OnItemClick(LTreeItem *Item, LMouse &m);
	/// Called when an item is dragged from it's position
	virtual void OnItemBeginDrag(LTreeItem *Item, LMouse &m);
	/// Called when an item is expanded/contracted to show or hide it's children
	virtual void OnItemExpand(LTreeItem *Item, bool Expand);
	/// Called when an item is selected
	virtual void OnItemSelect(LTreeItem *Item);
	
	// Implementation
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	bool OnMouseWheel(double Lines) override;
	void OnPaint(LSurface *pDC) override;
	void OnFocus(bool b) override;
	void OnPosChange() override;
	bool OnKey(LKey &k) override;
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
	LMessage::Result OnEvent(LMessage *Msg) override;
	void OnPulse() override;
	int GetContentSize(int ColumnIdx) override;
	LCursor GetCursor(int x, int y) override;
	LPoint ScrollPxPos();

	/// Add a item to the tree
	LTreeItem *Insert(LTreeItem *Obj = 0, ssize_t Pos = -1);
	/// Remove and delete an item
	bool Delete(LTreeItem *Obj);
	/// Remove but don't delete an item
	bool Remove(LTreeItem *Obj);
	/// Gets the item at an index
	LTreeItem *ItemAt(size_t Pos) { return Items[Pos]; }
	/// \returns if the object is in the tree
	bool HasItem(LTreeItem *obj, bool recurse = true);

	/// Select the item 'Obj'
	bool Select(LTreeItem *Obj);
	/// Returns the first selected item
	LTreeItem *Selection();
	/// Get items as an array
	bool GetItems(LArray<LItem*> &arr, bool selectedOnly = false) override;

	/// Gets the whole selection and puts it in 'n'
	template<class T>
	bool GetSelection(LArray<T*> &n)
	{
		n.Empty();
		auto s = GetSelLst();
		for (auto i : *s)
		{
			T *ptr = dynamic_cast<T*>(i);
			if (ptr)
				n.Add(ptr);
		}
		return n.Length() > 0;
	}
	
	/// Gets an array of all items
	template<class T>
	bool GetAll(LArray<T*> &n)
	{
		n.Empty();
		return ForAllItems([&n](LTreeItem *item)
			{
				if (auto t = dynamic_cast<T*>(item))
					n.Add(t);
				return true;
			});
	}
	
	/// Call a function for every item
	/// Callback can should return true to continue iteration.
	/// Or false to stop.
	bool ForAllItems(std::function<bool(LTreeItem*)> Callback);

	/// Returns the item at an x,y location
	LTreeItem *ItemAtPoint(int x, int y, bool Debug = false);
	/// Temporarily selects one of the items as the drop target during a
	/// drag and drop operation. Call SelectDropTarget(0) when done.
	void SelectDropTarget(LTreeItem *Item);

	/// Delete all items (frees the items)
	void Empty();
	/// Remove reference to items (doesn't free the items)
	void RemoveAll();
	/// Call 'Update' on all tree items
	void UpdateAllItems() override;
	/// This only works if MultiSelect is enabled
	bool SelectAll(bool select = true) override;
	
	// Sorting:
	void Sort(std::function<int(LTreeNode*, LTreeNode*)> compare);
	void Sort(int Column) override;
	void Sort() override;

	// Visual style
	enum ThumbStyle
	{
		TreePlus,
		TreeTriangle
	};

	void SetVisualStyle(ThumbStyle Btns, bool JoiningLines);
};

#endif

