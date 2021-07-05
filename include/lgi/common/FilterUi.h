/// \file
/// \author Matthew Allen
#ifndef _GFILTER_UI_H_
#define _GFILTER_UI_H_

#include "lgi/common/Tree.h"

class LFilterViewPrivate;

enum GFilterNode
{
	LNODE_NULL,
	LNODE_AND,
	LNODE_OR,
	LNODE_COND,
	LNODE_NEW
};

enum GFilterMenu
{
	FMENU_FIELD,
	FMENU_OP,
	FMENU_VALUE
};

class LFilterView;
class LFilterItem;

typedef int (*FilterUi_Menu)(LFilterView *View,
							 LFilterItem *Item,
							 GFilterMenu Menu,
							 LRect &r,
							 LArray<char*> *GetList,
							 void *UserData);

class LFilterItem : public LTreeItem, public GDragDropSource
{
	class LFilterItemPrivate *d;

protected:
	void _PourText(LPoint &Size);
	void _PaintText(GItem::ItemPaintCtx &Ctx);
	void ShowControls(bool s);

public:
	LFilterItem(LFilterViewPrivate *Data, GFilterNode Node = LNODE_NEW);
	~LFilterItem();

	bool GetNot();
	const char *GetField();
	int GetOp();
	const char *GetValue();

	void SetNot(bool b);
	void SetField(const char *s);
	void SetOp(int i);
	void SetValue(char *s);

	GFilterNode GetNode();
	void SetNode(GFilterNode n);
	bool OnKey(LKey &k);
	void OnMouseClick(LMouse &m);
	void OnExpand(bool b);
	void OptionsMenu();

	// Dnd
	bool OnBeginDrag(LMouse &m);
	bool GetFormats(GDragFormats &Formats);
	bool GetData(LArray<GDragData> &Data);
};

/// Filter user interface using a tree structure.
class LFilterView : public LLayout
{
	class LFilterViewPrivate *d;

public:
	LFilterView(FilterUi_Menu Callback = 0, void *UserData = 0);
	~LFilterView();

	void SetDefault();
	bool ShowLegend();
	void ShowLegend(bool b);
	void Empty();
	LFilterItem *Create(GFilterNode Node = LNODE_NEW);
	LTreeNode *GetRootNode();

	void OnPaint(LSurface *pDC);
	void OnPosChange();
	void OnCreate();
};

#endif