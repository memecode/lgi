#pragma once

#include "lgi/common/List.h"
#include "lgi/common/DragAndDrop.h"

class LDataGrid : public LList, public LDragDropSource, public LDragDropTarget
{
	struct LDataGridPriv *d;

public:
	enum LDataGridFlags
	{
		GDG_READONLY	= 0x1,
		GDG_INTEGER		= 0x2,
	};

	typedef LListItem *(*ItemFactory)(void *userdata);
	typedef LArray<LListItem*> ItemArray;
	typedef LArray<int> IndexArray;

	LDataGrid(int CtrlId, ItemFactory Func = 0, void *userdata = 0);
	~LDataGrid();

	// Methods
	bool CanAddRecord();
	void CanAddRecord(bool b);
	void SetFactory(ItemFactory Func = 0, void *userdata = 0);
	LListItem *NewItem();
	void SetColFlag(int Col, LDataGridFlags Flags, LVariant *Arg = 0);
	IndexArray *GetDeletedItems();

	// Impl
	void OnItemSelect(LArray<LListItem*> &Items);
	void OnItemClick(LListItem *Item, LMouse &m);
	void OnCreate();
	LMessage::Result OnEvent(LMessage *Msg);
	int OnNotify(LViewI *c, int f);
	bool OnMouseWheel(double Lines);
	void OnPaint(LSurface *pDC);
	bool OnLayout(LViewLayoutInfo &Inf);
	bool Remove(LListItem *Obj);
	void Empty();
	
	// D'n'd
	void SetDndFormats(char *SrcFmt, char *AcceptFmt);
	void OnItemBeginDrag(LListItem *Item, LMouse &m);
	bool GetData(LArray<LDragData> &Data);
	bool GetFormats(LDragFormats &Formats);
	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState);
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState);
	ItemArray *GetDroppedItems();
};
