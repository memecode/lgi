/// \file
/// \author Matthew Allen
#ifndef _GFILTER_UI_H_
#define _GFILTER_UI_H_

#include "lgi/common/Tree.h"

class LFilterViewPrivate;

enum LFilterNode
{
	LNODE_NULL,
	LNODE_AND,
	LNODE_OR,
	LNODE_COND,
	LNODE_NEW
};

enum LFilterMenu
{
	FMENU_FIELD,
	FMENU_OP,
	FMENU_VALUE
};

class LFilterView;
class LFilterItem;

typedef int (*FilterUi_Menu)(LFilterView *View,
							 LFilterItem *Item,
							 LFilterMenu Menu,
							 LRect &r,
							 LArray<char*> *GetList,
							 void *UserData);

class LFilterItem : public LTreeItem, public LDragDropSource
{
	class LFilterItemPrivate *d;

protected:
	void _PourText(LPoint &Size);
	void _PaintText(LItem::ItemPaintCtx &Ctx);
	void ShowControls(bool s);

public:
	LFilterItem(LFilterViewPrivate *Data, LFilterNode Node = LNODE_NEW);
	~LFilterItem();

	bool GetNot();
	const char *GetField();
	int GetOp();
	const char *GetValue();

	void SetNot(bool b);
	void SetField(const char *s);
	void SetOp(int i);
	void SetValue(char *s);

	LFilterNode GetNode();
	void SetNode(LFilterNode n);
	bool OnKey(LKey &k);
	void OnMouseClick(LMouse &m);
	void OnExpand(bool b);
	void OptionsMenu();

	// Dnd
	bool OnBeginDrag(LMouse &m);
	bool GetFormats(LDragFormats &Formats);
	bool GetData(LArray<LDragData> &Data);
};

/// Filter user interface using a tree structure.
class LFilterView : public LLayout
{
	class LFilterViewPrivate *d;

public:
	LFilterView(FilterUi_Menu Callback = NULL, void *UserData = NULL);
	~LFilterView();

	void SetDefault();
	bool ShowLegend();
	void ShowLegend(bool b);
	void Empty();
	LFilterItem *Create(LFilterNode Node = LNODE_NEW);
	LTreeNode *GetRootNode();

	void OnPaint(LSurface *pDC);
	void OnPosChange();
	void OnCreate();
};

#endif