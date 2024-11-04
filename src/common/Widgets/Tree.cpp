#include <math.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Tree.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Palette.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/CssTools.h"

#define TREE_BLOCK          16
#define DRAG_THRESHOLD		4
#define DRAG_SCROLL_EDGE	20
#define DRAG_SCROLL_X		8
#define DRAG_SCROLL_Y		1

#define TreeUpdateNow		false
#define ForAll(Items)		for (auto c : Items)

struct LTreeLocker
{
	LTree *t = NULL;
	bool status = false;
	LTreeLocker(LTree *tree, const char *file, int line) : t(tree)
	{
		if (t)
			status = t->Lock(file, line);
	}
	~LTreeLocker()
	{
		if (status && t)
			t->Unlock();
	}
};

#define TREELOCK(ptr) LTreeLocker _lock(ptr, _FL);

//////////////////////////////////////////////////////////////////////////////
// Private class definitions for binary compatibility
class LTreePrivate
{
public:
	// Private data
	int				LineFlags[4] = {};
	bool			LayoutDirty = true;
	LPoint			Limit;
	LPoint			LastClick;
	LPoint			DragStart;
	int				DragData = 0;
	LAutoPtr<LMemDC> IconCache;
	bool			InPour = false;
	int64			DropSelectTime = 0;
    int8            IconTextGap = 0;
    int				LastLayoutPx = -1;
	LMouse			*CurrentClick = NULL;
	LTreeItem		*ScrollTo = NULL;
    
    // Visual style
	LTree::ThumbStyle Btns = LTree::TreeTriangle;
	bool			JoiningLines = false;

	// Pointers into items... be careful to clear when deleting items...
	LTreeItem		*LastHit = NULL;
	List<LTreeItem>	Selection;
	LTreeItem		*DropTarget = NULL;
};

class LTreeItemPrivate
{
	LArray<LDisplayString*> Ds;
	LArray<uint32_t> ColPx;

public:
	LTreeItem *Item;
	LRect Pos;
	LRect Thumb;
	LRect Text;
	LRect Icon;
	bool Open = false;
	bool Selected = false;
	bool Visible = false;
	bool Last = false;
	int Depth = 0;
	
	LTreeItemPrivate(LTreeItem *it)
	{
		Item = it;
		Pos.ZOff(-1, -1);
		Text.ZOff(-1, -1);
		Icon.ZOff(-1, -1);
	}

	~LTreeItemPrivate()
	{
		Ds.DeleteObjects();
	}

	LDisplayString *GetDs(int Col, int FixPx)
	{
		if (!Ds[Col])
		{
			LFont *f = Item->GetTree() ? Item->GetTree()->GetFont() : LSysFont;
			auto txt = Item->GetText(Col);
			if (txt)
			{			
				Ds[Col] = new LDisplayString(f, Item->GetText(Col));
				if (Ds[Col])
				{
					ColPx[Col] = Ds[Col]->X();
					if (FixPx > 0)
					{
						Ds[Col]->TruncateWithDots(FixPx);
					}
				}
			}
		}
		
		return Ds[Col];
	}
	
	void ClearDs(int Col = -1)
	{
		if (Col >= 0)
		{
			delete Ds[Col];
			Ds[Col] = NULL;
		}
		else
		{
			Ds.DeleteObjects();
		}
	}

	int GetColumnPx(int Col)
	{
		int BasePx = 0;
		
		GetDs(Col, 0);
		if (Col == 0)
		{
			BasePx = (Depth + 1) * TREE_BLOCK;
		}
		
		return ColPx[Col] + BasePx;
	}
};

//////////////////////////////////////////////////////////////////////////////
LTreeNode::LTreeNode()
{
}

LTreeNode::~LTreeNode()
{
}

bool LTreeNode::ReorderPos(LItemContainer::ContainerItemDrop &drop, LPoint &pt, int depth)
{
	ssize_t i = 0;
	
	for (auto item: Items)
	{
		auto p = item->GetPos();
		if (p->Overlap(pt))
		{
			drop.pos.x1 = p->x1;
			drop.pos.x2 = p->x2;
			if (pt.y >= p->Center().y)
			{
				// Bottom half...
				drop.pos.y1 = p->y2 - 1;
				drop.pos.y2 = p->y2 + 2;
				drop.prev = item;
				drop.next = Items.ItemAt(i + 1);
				
				return true;
			}
			else
			{
				// Top half...
				drop.pos.y1 = p->y1 - 2;
				drop.pos.y2 = p->y1 + 1;
				drop.next = item;
				drop.prev = Items.ItemAt(i - 1);
				
				return true;
			}
		}
		
		if (item->ReorderPos(drop, pt, depth + 1))
			return true;
		
		i++;
	}

	#if 0
	LgiTrace("reorder %s %s %s\n",
		inf.prev ? inf.prev->GetText() : "null",
		inf.next ? inf.next->GetText() : "null",
		inf.pos.GetStr());
	#endif

	return false;
}

void LTreeNode::SetLayoutDirty()
{
	Tree->d->LayoutDirty = true;
}

void LTreeNode::_Visible(bool v)
{
	for (LTreeItem *i=GetChild(); i; i=i->GetNext())
	{
		LAssert(i != this);
		i->OnVisible(v);
		i->_Visible(v);
	}
}

void LTreeNode::_ClearDs(int Col)
{
	List<LTreeItem>::I it = Items.begin();
	for (LTreeItem *c = *it; c; c = *++it)
	{
		c->_ClearDs(Col);
	}
}

LItemContainer *LTreeItem::GetContainer()
{
	return Tree;
}

LTreeItem *LTreeNode::Insert(LTreeItem *Obj, ssize_t Idx)
{
	LAssert(Obj != this);

	if (Obj && Obj->Tree)
		Obj->Remove();
	
	auto NewObj = Obj ? Obj : new LTreeItem;
	if (NewObj)
	{
		NewObj->Parent = this;
		NewObj->_SetTreePtr(Tree);

		Items.Delete(NewObj);
		Items.Insert(NewObj, Idx);

		if (Tree)
		{
			Tree->d->LayoutDirty = true;
			if (Pos() && Pos()->Y() > 0)
				Tree->_UpdateBelow(Pos()->y1);
			else
				Tree->Invalidate();
		}
	}

	return NewObj;
}

void LTreeNode::Detach()
{
	auto item = IsItem();
	if (Parent)
	{
		if (item)
		{
			LAssert(Parent->Items.HasItem(item));
			Parent->Items.Delete(item);
		}
		else LAssert(0);
		Parent = NULL;
	}
	if (Tree)
	{
		Tree->d->LayoutDirty = true;
		Tree->Invalidate();
	}
	if (item)
		item->_SetTreePtr(NULL);
}

void LTreeNode::Remove()
{
	int y = 0;
	if (Parent)
	{
		if (auto p = Parent->IsItem())
		{
			// Parent is a tree item..
			y = p->d->Pos.y1;
		}
		else if (auto item = IsItem())
		{
			// Parent is the top level tree view..
			y = item->d->Pos.y1;
		}
		else LAssert(!"Shouldn't happen");
	}

	LTree *t = Tree;

	if (IsItem())
		IsItem()->_Remove();

	if (t)
	{
		t->_UpdateBelow(y);
	}
}

bool LTreeNode::IsRoot()
{
	return Parent == 0 || (LTreeNode*)Parent == (LTreeNode*)Tree;
}

size_t LTreeNode::Length()
{
	return Items.Length();
}

bool LTreeNode::HasItem(LTreeItem *obj, bool recurse)
{
	if (!obj)
		return false;
	if (this == (LTreeNode*)obj)
		return true;

	for (auto i: Items)
	{
		if (i == obj)
			return true;
		if (recurse && i->HasItem(obj, recurse))
			return true;
	}

	return false;
}

int LTreeNode::ForEach(std::function<void(LTreeItem*)> Fn)
{
	int Count = 0;
	
	for (auto t : Items)
	{
		Fn(t);
		Count += t->ForEach(Fn);
	}
	
	return Count + 1;
}

ssize_t LTreeNode::IndexOf()
{
	if (Parent)
		return Parent->Items.IndexOf(IsItem());

	return -1;
}

LTreeItem *LTreeNode::GetChild()
{
	return Items.Length() ? Items[0] : NULL;
}

LTreeItem *LTreeNode::GetPrev()
{
	if (Parent)
	{
		ssize_t Index = Parent->Items.IndexOf(IsItem());
		if (Index >= 0)
		{
			return Parent->Items.ItemAt(Index - 1);
		}
	}

	return NULL;
}

LTreeItem *LTreeNode::GetNext()
{
	if (Parent)
	{
		ssize_t Index = Parent->Items.IndexOf(IsItem());
		if (Index >= 0)
		{
			return Parent->Items.ItemAt(Index + 1);
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
LTreeItem::LTreeItem(const char *initStr)
{
	d = new LTreeItemPrivate(this);
	if (initStr)
		SetText(initStr);
}

LTreeItem::~LTreeItem()
{
	if (Tree)
	{
		if (Tree->d->DropTarget == this)
			Tree->d->DropTarget = NULL;

		if (Tree->d->LastHit == this)
			Tree->d->LastHit = NULL;
		
		if (Tree->IsCapturing())
			Tree->Capture(false);
	}

	int y = 0;
	LTree *t = NULL;
	if (Parent && Parent->IsItem())
	{
		t = Tree;
		y = Parent->IsItem()->d->Pos.y1;
	}
	else if ((LTreeNode*)this != (LTreeNode*)Tree)
	{
		t = Tree;
		LTreeItem *p = GetPrev();
		if (p)
			y = p->d->Pos.y1;
		else
			y = d->Pos.y1;
	}

	_Remove();
	while (Items.Length())
	{
		auto It = Items.begin();
		delete *It;
	}
	DeleteObj(d);

	if (t)
		t->_UpdateBelow(y);
}

bool LTreeItem::Visible()
{
	return d->Visible;
}

int LTreeItem::GetColumnSize(int Col)
{
	int Px = d->GetColumnPx(Col);
	if (Expanded())
	{
		ForAll(Items)
		{
			int ChildPx = c->GetColumnSize(Col);
			Px = MAX(ChildPx, Px);
		}
	}
	return Px;
}

LRect *LTreeItem::Pos()
{
	return &d->Pos;
}

LPoint LTreeItem::_ScrollPos()
{
	LPoint p;
	if (Tree) p = Tree->ScrollPxPos();
	return p;
}

LRect *LTreeItem::_GetRect(LTreeItemRect Which)
{
	switch (Which)
	{
		case TreeItemPos:   return &d->Pos;
		case TreeItemThumb: return &d->Thumb;
		case TreeItemText:  return &d->Text;
		case TreeItemIcon:  return &d->Icon;
	}
	
	return NULL;
}

bool LTreeItem::IsDropTarget()
{
	LTree *t = GetTree();
	if (t && t->d && t->d->DropTarget == this)
		return true;
	
	return false;
}

LRect *LTreeItem::GetPos(int Col)
{
	if (!d->Pos.Valid() && Tree)
		Tree->_Pour();

	static LRect r;

	r = d->Pos;

	if (Col >= 0)
	{
		LItemColumn *Column = 0;

		int Cx = Tree->GetImageList() ? 16 : 0;
		for (int c=0; c<Col; c++)
		{
			Column = Tree->ColumnAt(c);
			if (Column)
			{
				Cx += Column->Width();
			}
		}
		Column = Tree->ColumnAt(Col);

		if (Column)
		{
			r.x1 = Cx;
			r.x2 = Cx + Column->Width() - 1;
		}
	}

	return &r;
}

void LTreeItem::_RePour()
{
	if (Tree)
		Tree->_Pour();
}

void LTreeItem::ScrollTo()
{
	if (!Tree)
		return;

	if (Tree->VScroll)
	{
		LRect c = Tree->GetClient();
		LRect p = d->Pos;
		int y = d->Pos.Y() ? d->Pos.Y() : 16;
		p.Offset(0, (int) (-Tree->VScroll->Value() * y));

		if (p.y1 < c.y1)
		{
			int Lines = (c.y1 - p.y1 + y - 1) / y;
			Tree->VScroll->Value(Tree->VScroll->Value() - Lines);
		}
		else if (p.y2 > c.y2)
		{
			int Lines = (p.y2 - c.y2 + y - 1) / y;
			Tree->VScroll->Value(Tree->VScroll->Value() + Lines);
		}
	}
	else
	{
		Tree->d->ScrollTo = this;
		if (Tree->IsAttached())
			Tree->PostEvent(M_SCROLL_TO);
	}
}

void LTreeItem::_SetTreePtr(LTree *t)
{
	if (Tree && !t)
	{
		// Clearing tree pointer, must remove all references to this item that
		// the tree might still have.
		if (d->Selected)
		{
			Tree->d->Selection.Delete(this);
			d->Selected = false;
		}
		if (Tree->d->LastHit == this)
			Tree->d->LastHit = NULL;
		if (Tree->d->DropTarget == this)
			Tree->d->DropTarget = NULL;
	}
	Tree = t;

	for (auto i: Items)
		i->_SetTreePtr(t);
}

void LTreeItem::_Remove()
{
	if (Parent)
	{
		LAssert(Parent->Items.HasItem(this));
		Parent->Items.Delete(this);
		Parent = NULL;
	}

	if (Tree)
	{
		LAssert(Tree->d != NULL);
		Tree->d->LayoutDirty = true;
		
		if (Tree->IsCapturing())
			Tree->Capture(false);
	}

	_SetTreePtr(NULL);
}

void LTreeItem::_PourText(LPoint &Size)
{
	auto f = Tree ? Tree->GetFont() : LSysFont;
	auto Txt = GetText();
	
	#if defined(_WIN64) && defined(_DEBUG)
	if ((void*)Txt == (void*)0xfeeefeeefeeefeee ||
		(void*)Txt == (void*)0xcdcdcdcdcdcdcdcd)
	{
		LAssert(!"Yeah nah...");
		return;
	}
	#endif
	
	LDisplayString ds(f, Txt);
	Size.x = ds.X() + 4;
	Size.y = 0;
}

void LTreeItem::_PaintText(LItem::ItemPaintCtx &Ctx)
{
	auto Text = GetText();
	if (Text)
	{
		auto Ds = d->GetDs(0, d->Text.X());
		auto f = Tree ? Tree->GetFont() : LSysFont;

		auto Tab = f->TabSize();
		f->TabSize(0);
		f->Transparent(false);
		f->Colour(Ctx.Fore, Ctx.TxtBack);
		
		if (Ds)
		{
			Ds->Draw(Ctx.pDC, d->Text.x1 + 2, d->Text.y1 + 1, &d->Text);
			if (Ctx.x2 > d->Text.x2)
			{
				LRect r = Ctx;
				r.x1 = d->Text.x2 + 1;
				Ctx.pDC->Colour(Ctx.Back);
				Ctx.pDC->Rectangle(&r);
			}
		}
		
		f->TabSize(Tab);
	}
	else
	{
		Ctx.pDC->Colour(Ctx.Back);
		Ctx.pDC->Rectangle(&Ctx);
	}
}

void LTreeItem::_Pour(LPoint *Limit, int ColumnPx, int Depth, bool Visible)
{
	auto css = GetCss(false);
	auto display = css ? css->Display() != LCss::DispNone : true;

	d->Visible = display && Visible;
	d->Depth = Depth;

	if (d->Visible)
	{
		LPoint TextSize;
		_PourText(TextSize);
		
		auto ImgLst = Tree->GetImageList();
		int IconY = (ImgLst && GetImage() >= 0) ? ImgLst->TileY() : 0;
		int Height = MAX(TextSize.y, IconY);
		if (!Height)
		    Height = 16;

		auto Ds = d->GetDs(0, 0);
		d->Pos.ZOff(ColumnPx - 1, (Ds ? MAX(Height, Ds->Y()) : Height) - 1);
		d->Pos.Offset(0, Limit->y);
		if (!d->Pos.Valid())
		{
			LgiTrace("%s:%i - Invalid pos: %s, ColumnPx=%i\n", _FL, d->Pos.GetStr(), ColumnPx);
		}

		Limit->x = MAX(Limit->x, d->Pos.x2 + 1);
		Limit->y = MAX(Limit->y, d->Pos.y2 + 1);
	}
	else
	{
		d->Pos.ZOff(-1, -1);
	}

	LTreeItem *n;
	List<LTreeItem>::I it = Items.begin();
	for (LTreeItem *i=*it; i; i=n)
	{
		n = *++it;
		i->d->Last = n == 0;
		i->_Pour(Limit, ColumnPx, Depth+1, d->Open && d->Visible);
	}
}

void LTreeItem::_ClearDs(int Col)
{
	d->ClearDs(Col);	
	LTreeNode::_ClearDs(Col);
}

const char *LTreeItem::GetText(int i)
{
	return Str[i];
}

bool LTreeItem::SetText(const char *s, int i)
{
	TREELOCK(Tree);

	Str[i] = s;
	if (Tree)
		Update();

	return true;
}

int LTreeItem::GetImage(int Flags)
{
	return Sys_Image;
}

void LTreeItem::SetImage(int i)
{
	Sys_Image = i;
}

void LTreeItem::Update()
{
	if (Tree)
	{
		LRect p = d->Pos;
		p.x2 = 10000;
		d->ClearDs();
		Tree->_Update(&p, TreeUpdateNow);
	}
}

bool LTreeItem::Select()
{
	return d->Selected;
}

void LTreeItem::Select(bool b)
{
	if (d->Selected != b)
	{
		d->Selected = b;
		if (b)
		{
			LTreeNode *p = this;
			while ((p = p->GetParent()))
			{
				p->Expanded(true);
			}
		}

		Update();

		if (b && Tree)
		{
			Tree->_OnSelect(this);
			Tree->OnItemSelect(this);
		}
	}
}

bool LTreeItem::Expanded()
{
	return d->Open;
}

void LTreeItem::Expanded(bool b)
{
	if (d->Open != b)
	{
		d->Open = b;

		if (Items.Length() > 0)
		{
			if (Tree)
			{
				Tree->d->LayoutDirty = true;
				Tree->_UpdateBelow(d->Pos.y1);
			}
			OnExpand(b);
		}
	}
}

void LTreeItem::OnExpand(bool b)
{
	_Visible(b);
}

LTreeItem *LTreeItem::_HitTest(int x, int y, bool Debug)
{
	LTreeItem *Status = 0;

	if (d->Pos.Overlap(x, y) &&
		x > (d->Depth*TREE_BLOCK))
	{
		Status = this;
	}

	if (d->Open)
	{
		List<LTreeItem>::I it = Items.begin();
		for (LTreeItem *i=*it; i && !Status; i=*++it)
		{
			Status = i->_HitTest(x, y, Debug);
		}
	}

	return Status;
}

void LTreeItem::_MouseClick(LMouse &m)
{
	if (m.Down())
	{
		if ((Items.Length() > 0 &&
			d->Thumb.Overlap(m.x, m.y)) ||
			m.Double())
		{
			Expanded(!Expanded());
		}

		LRect rText = d->Text;
		if (Tree && Tree->Columns.Length() > 0)
			rText.x2 = Tree->X();

		if (rText.Overlap(m.x, m.y) ||
			d->Icon.Overlap(m.x, m.y))
		{
			Select(true);

			if (Tree)
				Tree->OnItemClick(this, m);
		}
	}
}

void LTreeItem::OnPaint(ItemPaintCtx &Ctx)
{
	LAssert(Tree != NULL);
	if (!d->Visible)
		return;

	// background up to text
	LSurface *&pDC = Ctx.pDC;
	pDC->Colour(Ctx.Back);
	pDC->Rectangle(0, d->Pos.y1, (d->Depth*TREE_BLOCK)+TREE_BLOCK, d->Pos.y2);

	// draw trunk
	LRect Pos = d->Pos;
	Pos.x2 = Pos.x1 + Ctx.ColPx[0] - 1;

	int x = 0;
	LColour Ws(L_WORKSPACE);
	LColour Lines = Ws.Invert().Mix(Ws);

	pDC->Colour(Lines);
	if (Tree->d->JoiningLines)
	{
		for (int i=0; i<d->Depth; i++)
		{
			if (Tree->d->LineFlags[0] & (1 << i))
				pDC->Line(x + 8, Pos.y1, x + 8, Pos.y2);
			x += TREE_BLOCK;
		}
	}
	else
	{
		x += TREE_BLOCK * d->Depth;
	}

	// draw node
	int cy = Pos.y1 + (Pos.Y() >> 1);
	if (Items.Length() > 0)
	{
		d->Thumb.ZOff(8, 8);
		d->Thumb.Offset(x + 4, cy - 4);

		switch (Tree->d->Btns)
		{
			case LTree::TreePlus:
			{
				// plus/minus symbol
				pDC->Colour(L_LOW);
				pDC->Box(&d->Thumb);
				pDC->Colour(L_WHITE);
				pDC->Rectangle(d->Thumb.x1+1, d->Thumb.y1+1, d->Thumb.x2-1, d->Thumb.y2-1);
				pDC->Colour(L_SHADOW);
				pDC->Line(	d->Thumb.x1+2,
							d->Thumb.y1+4,
							d->Thumb.x1+6,
							d->Thumb.y1+4);

				if (!d->Open)
				{
					// not open, so draw the cross bar making the '-' into a '+'
					pDC->Colour(L_SHADOW);
					pDC->Line(	d->Thumb.x1+4,
								d->Thumb.y1+2,
								d->Thumb.x1+4,
								d->Thumb.y1+6);
				}
				break;
			}
			case LTree::TreeTriangle:
			{
				// Triangle style expander
				pDC->Colour(Lines);

				int Off = 2;
				if (d->Open)
				{
					for (int y=0; y<d->Thumb.Y(); y++)
					{
						int x1 = d->Thumb.x1 + y;
						int x2 = d->Thumb.x2 - y;
						if (x2 < x1)
							break;
						pDC->HLine(x1, x2, d->Thumb.y1 + y + Off);
					}
				}
				else
				{
					for (int x=0; x<d->Thumb.X(); x++)
					{
						int y1 = d->Thumb.y1 + x;
						int y2 = d->Thumb.y2 - x;
						if (y2 < y1)
							break;
						pDC->VLine(d->Thumb.x1 + x + Off, y1, y2);
					}
				}
				break;
			}
		}

		pDC->Colour(Lines);

		if (Tree->d->JoiningLines)
		{
			if (Parent || IndexOf() > 0)
				// draw line to item above
				pDC->Line(x + 8, Pos.y1, x + 8, d->Thumb.y1-1);

			// draw line to leaf beside
			pDC->Line(d->Thumb.x2+1, cy, x + (TREE_BLOCK-1), cy);

			if (!d->Last)
				// draw line to item below
				pDC->Line(x + 8, d->Thumb.y2+1, x + 8, Pos.y2);
		}
	}
	else if (Tree->d->JoiningLines)
	{
		// leaf node
		pDC->Colour(L_MED);
		if (d->Last)
			pDC->Rectangle(x + 8, Pos.y1, x + 8, cy);
		else
			pDC->Rectangle(x + 8, Pos.y1, x + 8, Pos.y2);

		pDC->Rectangle(x + 8, cy, x + (TREE_BLOCK-1), cy);
	}
	x += TREE_BLOCK;

	// draw icon
	int Image = GetImage(Select());
	LImageList *Lst = Tree->GetImageList();
	if (Image >= 0 && Lst)
	{
		d->Icon.ZOff(Lst->TileX() + Tree->d->IconTextGap - 1, Pos.Y() - 1);
		d->Icon.Offset(x, Pos.y1);

		pDC->Colour(Ctx.Back);

		if (Tree->d->IconCache)
		{
			// no flicker
			LRect From;

			From.ZOff(Lst->TileX()-1, Tree->d->IconCache->Y()-1);
			From.Offset(Lst->TileX()*Image, 0);

			pDC->Blt(d->Icon.x1, d->Icon.y1, Tree->d->IconCache, &From);

			pDC->Rectangle(d->Icon.x1 + Lst->TileX(), d->Icon.y1, d->Icon.x2, d->Icon.y2);
		}
		else
		{
			// flickers...
			int Px = d->Icon.y1 + ((Lst->TileY()-Pos.Y()) >> 1);
			pDC->Rectangle(&d->Icon);
			Tree->GetImageList()->Draw(pDC, d->Icon.x1, Px, Image, Ctx.Back);
		}

		x += d->Icon.X();
	}

	LColour SelFore(Tree->Focus() ? L_FOCUS_SEL_FORE : L_NON_FOCUS_SEL_FORE);
	LColour SelBack(Tree->Focus() ? L_FOCUS_SEL_BACK : L_NON_FOCUS_SEL_BACK);

	bool IsSelected = (Tree->d->DropTarget == this) || (Tree->d->DropTarget == NULL && Select());
	LColour Fore = Ctx.Fore;
	LColour TxtBack = Ctx.TxtBack;
	auto Css = GetCss();
	LCss::ColorDef f, b;
	if (Css)
	{
		f = Css->Color();
		b = Css->BackgroundColor();
	}

	// text: first column
	Ctx.Fore = f.Type == LCss::ColorRgb ? (LColour)f : (IsSelected ? SelFore : Fore);
	Ctx.TxtBack = b.Type == LCss::ColorRgb ? (LColour)b : (IsSelected ? SelBack : Ctx.Back);

	auto ColourDiff = abs(Ctx.Fore.GetGray() - Ctx.TxtBack.GetGray());
	if (ColourDiff < 32) // Check if the colours are too similar and then disambiguate...
	{
		// LgiTrace("%s %s are too similar %i\n", Ctx.Fore.GetStr(), Ctx.TxtBack.GetStr(), (int)ColourDiff);
		Ctx.TxtBack = Ctx.TxtBack.Mix(L_WORKSPACE);
	}

	LPoint TextSize;
	_PourText(TextSize);
	d->Text.ZOff(TextSize.x-1, Pos.Y()-1);
	d->Text.Offset(x, Pos.y1);
	(LRect&)Ctx = d->Text;
	Ctx.x2 = Ctx.ColPx[0] - 1;
	_PaintText(Ctx);
	x = Pos.x2 + 1;

	// text: other columns
	Ctx.Fore = f.Type == LCss::ColorRgb ? (LColour)f : Fore;
	Ctx.TxtBack = b.Type == LCss::ColorRgb ? (LColour)b : Ctx.Back;
	for (int i=1; i<Ctx.Columns; i++)
	{
		Ctx.Set(x, Pos.y1, x + Ctx.ColPx[i] - 1, Pos.y2);
		OnPaintColumn(Ctx, i, Tree->Columns[i]);
		x = Ctx.x2 + 1;
	}
	
	Ctx.Fore = Fore;
	Ctx.TxtBack = TxtBack;

	// background after text
	pDC->Colour(Ctx.Back);
	pDC->Rectangle(x, Pos.y1, MAX(Tree->X(), Tree->d->Limit.x), Pos.y2);

	// children
	if (d->Open)
	{
		if (!d->Last)
			Tree->d->LineFlags[0] |= 1 << d->Depth;

		List<LTreeItem>::I it = Items.begin();
		for (LTreeItem *i=*it; i; i=*++it)
			i->OnPaint(Ctx);

		Tree->d->LineFlags[0] &= ~(1 << d->Depth);
	}
}

void LTreeItem::OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c)
{
	LDisplayString *ds = d->GetDs(i, Ctx.ColPx[i]);
	if (ds)
	{
		// Draw the text in the context area:
		LFont *f = ds->GetFont();
		f->Colour(Ctx.Fore, Ctx.TxtBack);
		ds->Draw(Ctx.pDC, Ctx.x1 + 2, Ctx.y1 + 1, &Ctx);
	}
	else
	{
		// No string, fill the space with background
		Ctx.pDC->Colour(Ctx.Back);
		Ctx.pDC->Rectangle(&Ctx);
	}
}		

//////////////////////////////////////////////////////////////////////////////
LTree::LTree(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_TreeView)
{
	d = new LTreePrivate;
	SetId(id);
	LRect e(x, y, x+cx, y+cy);
	SetPos(e);
	if (name) Name(name);
	else Name("LGI.LTree");
	Sunken(true);

	Tree = this;
	Lines = true;
	Buttons = true;
	LinesAtRoot = true;
	EditLabels = false;
	ColumnHeaders(false);
	rItems.ZOff(-1, -1);

	#if WINNATIVE
	SetStyle(GetStyle() | WS_CHILD | WS_VISIBLE | WS_TABSTOP);
	#endif
	SetTabStop(true);
	LResources::StyleElement(this);
}

LTree::~LTree()
{
	Empty();
	DeleteObj(d);
}

// Internal tree methods
List<LTreeItem>	*LTree::GetSelLst()
{
	return &d->Selection;
}

void LTree::_Update(LRect *r, bool Now)
{
	TREELOCK(this)

	if (r)
	{
		LRect u = *r;
		LPoint s = ScrollPxPos();
		LRect c = GetClient();
		u.Offset(c.x1-s.x, c.y1-s.y);
		Invalidate(&u, Now && !d->InPour);
	}
	else
	{
		Invalidate((LRect*)0, Now && !d->InPour);
	}
}

void LTree::_UpdateBelow(int y, bool Now)
{
	TREELOCK(this)

	LPoint s = ScrollPxPos();
	LRect c = GetClient();
	LRect u(c.x1, y - s.y + c.y1, X()-1, Y()-1);
	Invalidate(&u, Now);
}

void LTree::ClearDs(int Col)
{
	TREELOCK(this)

	List<LTreeItem>::I it = Items.begin();
	for (LTreeItem *i=*it; i; i=*++it)
		i->_ClearDs(Col);
}

LPoint LTree::ScrollPxPos()
{
	TREELOCK(this)

	LPoint Status;
	Status.x = (HScroll) ? (int)HScroll->Value() : 0;
	Status.y = (VScroll) ? (int)VScroll->Value() * TREE_BLOCK : 0;

	return Status;
}

void LTree::_UpdateScrollBars()
{
	static bool Processing = false;
	if (!Processing)
	{
		Processing = true;
		
		{
			TREELOCK(this)
			LPoint Old = ScrollPxPos();
			
			LRect Client = GetClient();
			bool x = d->Limit.x > Client.X();
			bool y = d->Limit.y > Client.Y();
			SetScrollBars(x, y);
			Client = GetClient();

			// x scroll... in pixels
			if (HScroll)
			{
				HScroll->SetRange(d->Limit.x);
				HScroll->SetPage(Client.X());

				int Max = d->Limit.x - Client.X();
				if (HScroll->Value() > Max)
				{
					HScroll->Value(Max+1);
				}
			}

			// y scroll... in items
			if (VScroll)
			{
				int All = (d->Limit.y + TREE_BLOCK - 1) / TREE_BLOCK;
				int Visible = Client.Y() / TREE_BLOCK;

				VScroll->SetRange(All);
				VScroll->SetPage(Visible);

				/* Why is this commented out? -fret Dec2018
				int Max = All - Visible + 1;
				if (VScroll->Value() > Max)
					VScroll->Value(Max);
				*/
			}

			LPoint New = ScrollPxPos();
			if (Old.x != New.x ||
				Old.y != New.y)
			{
				Invalidate();
			}
		}

		Processing = false;
	}
}

void LTree::_OnSelect(LTreeItem *Item)
{
	TREELOCK(this)

	if
	(
		!MultiSelect()
		||
		!d->CurrentClick
		||
		(
			d->CurrentClick 
			&&
			!d->CurrentClick->Ctrl()
		)
	)
	{
		for (auto i: d->Selection)
		{
			if (i != Item)
				i->Select(false);
		}

		d->Selection.Empty();
	}
	else
	{
		d->Selection.Delete(Item);
	}

	d->Selection.Insert(Item);
}

void LTree::_Pour()
{
	TREELOCK(this)

	d->InPour = true;
	d->Limit.x = rItems.x1;
	d->Limit.y = rItems.y1;

	int ColumnPx = 0;
	if (Columns.Length())
	{
		for (int i=0; i<Columns.Length(); i++)
		{
			LItemColumn *c = Columns[i];
			ColumnPx += c->Width();
		}
	}
	else
	{
		ColumnPx = d->LastLayoutPx = GetClient().X();
		if (ColumnPx < 16)
			ColumnPx = 16;
	}

	LTreeItem *n;
	List<LTreeItem>::I it = Items.begin();
	for (LTreeItem *i=*it; i; i=n)
	{
		n = *++it;
		i->d->Last = n == 0;
		i->_Pour(&d->Limit, ColumnPx, 0, true);
	}

	_UpdateScrollBars();
	d->LayoutDirty = false;
	d->InPour = false;
}

// External methods and events
void LTree::OnItemSelect(LTreeItem *Item)
{
	if (!Item)
		return;

	TREELOCK(this)

	Item->OnSelect();
	SendNotify(LNotifyItemSelect);
}

void LTree::OnItemExpand(LTreeItem *Item, bool Expand)
{
	TREELOCK(this)

	if (Item)
		Item->OnExpand(Expand);
}

LTreeItem *LTree::GetAdjacent(LTreeItem *i, bool Down)
{
	TREELOCK(this)

	LTreeItem *Ret = NULL;
	if (i)
	{
		if (Down)
		{
			LTreeItem *n = i->GetChild();
			
			if (!n ||
				!n->d->Visible)
			{
				for (n = i; n; )
				{
					auto p = n->GetParent();
					if (p)
					{
						ssize_t Index = n->IndexOf();
						if (Index < (ssize_t)p->Items.Length()-1)
						{
							n = n->GetNext();
							break;
						}
						else
						{
							n = p->IsItem();
						}
					}
					else
					{
						n = n->GetNext();
						break;
					}
				}
			}

			Ret = n ? n->IsItem() : nullptr;
		}
		else if (auto p = i->GetParent())
		{			
			ssize_t Index = i->IndexOf();
			if (p)
			{
				LTreeNode *n = p;
				if (Index > 0)
				{
					n = i->GetPrev();
					while (	n->GetChild() &&
							n->GetChild()->d->Visible)
					{
						n = n->Items.ItemAt(n->Items.Length()-1);
					}
				}

				Ret = n ? n->IsItem() : nullptr;
			}
			else if (Index > 0)
			{
				p = i->GetTree()->ItemAt(Index - 1);
				while (p->GetChild() &&
						p->GetChild()->d->Visible)
				{
					if (p->Items.Length())
					{
						p = p->Items.ItemAt(p->Items.Length()-1);
					}
					else break;
				}
				
				Ret = p ? p->IsItem() : nullptr;
			}
		}
	}
	
	return Ret;
}

bool LTree::OnKey(LKey &k)
{
	if (!Lock(_FL))
		return false;
	
	bool Status = false;
	LTreeItem *i = d->Selection[0];
	if (!i)
	{
		i = Items[0];
		if (i)
			i->Select();
	}

	if (k.Down())
	{
		switch (k.vkey)
		{
			case LK_PAGEUP:
			case LK_PAGEDOWN:
			{
				if (i && i->d->Pos.Y() > 0)
				{
					int Page = GetClient().Y() / i->d->Pos.Y();
					for (int j=0; j<Page; j++)
					{
						LTreeItem *n = GetAdjacent(i, k.c16 == LK_PAGEDOWN);
						if (n)
						{
							i = n;
						}
						else break;
					}
					if (i)
					{
						i->Select(true);
						i->ScrollTo();
					}
				}
				Status = true;
				break;
			}
			case LK_HOME:
			{
				LTreeItem *i;
				if ((i = Items[0]))
				{
					i->Select(true);
					i->ScrollTo();
				}
				Status = true;
				break;
			}
			case LK_END:
			{
				LTreeItem *n = i, *p = NULL;
				while ((n = GetAdjacent(n, true)))
				{
					p = n;
				}
				if (p)
				{
					p->Select(true);
					p->ScrollTo();
				}
				Status = true;
				break;
			}
			case LK_LEFT:
			{
				if (i)
				{
					if (i->Items.Length() && i->Expanded())
					{
						i->Expanded(false);
						break;
					}
					else if (auto p = i->GetParent())
					{
						if (auto item = p->IsItem())
							item->Select(true);
						p->Expanded(false);
						_Pour();
						break;
					}
				}
				// fall thru
			}
			case LK_UP:
			{
				LTreeItem *n = GetAdjacent(i, false);
				if (n)
				{
					n->Select(true);
					n->ScrollTo();
				}
				Status = true;
				break;
			}
			case LK_RIGHT:
			{
				if (i)
				{
					i->Expanded(true);
					if (d->LayoutDirty)
					{
						_Pour();
						break;
					}
				}
				// fall thru
			}
			case LK_DOWN:
			{
				LTreeItem *n = GetAdjacent(i, true);
				if (n)
				{
					n->Select(true);
					n->ScrollTo();
				}
				Status = true;
				break;
			}
			case LK_DELETE:
			{
				if (k.Down())
				{
					Unlock(); // before potentially being deleted...?
					SendNotify(LNotification(k));
					// This might delete the item... so just return here.
					return true;
				}
				break;
			}
			#ifdef VK_APPS
			case VK_APPS:
			{
				LTreeItem *s = Selection();
				if (s)
				{
					LRect *r = &s->d->Text;
					if (r)
					{
						LMouse m;
						m.x = r->x1 + (r->X() >> 1);
						m.y = r->y1 + (r->Y() >> 1);
						m.Target = this;
						m.ViewCoords = true;
						m.Down(true);
						m.Right(true);
						
						s->OnMouseClick(m);
					}
				}
				break;
			}
			#endif
			default:
			{
				switch (k.c16)
				{
					case 'F':
					case 'f':
					{
						if (k.Ctrl())
							SendNotify(LNotifyContainerFind);
						break;
					}
				}
				break;
			}
		}
	}

	if (i && i != (LTreeItem*)this)
	{
		i->OnKey(k);
	}

	Unlock();
	return Status;
}

LTreeItem *LTree::ItemAtPoint(int x, int y, bool Debug)
{
	TREELOCK(this)

	LPoint s = ScrollPxPos();

	List<LTreeItem>::I it = Items.begin();
	LTreeItem *Hit = NULL;
	for (LTreeItem *i = *it; i; i=*++it)
	{
		Hit = i->_HitTest(s.x + x, s.y + y, Debug);
		if (Hit)
			break;
	}

	return Hit;
}

bool LTree::OnMouseWheel(double Lines)
{
	TREELOCK(this)

	if (VScroll)
		VScroll->Value(VScroll->Value() + (int)Lines);
	
	return true;
}

void LTree::OnMouseClick(LMouse &m)
{
	TREELOCK(this)

	d->CurrentClick = &m;

	if (m.Down())
	{
		DragMode = DRAG_NONE;

		if (ColumnHeaders() &&
			ColumnHeader.Overlap(m.x, m.y))
		{
			d->DragStart.x = m.x;
			d->DragStart.y = m.y;

			// Clicked on a column heading
			LItemColumn *Resize;
			LItemColumn *Over = NULL;
			HitColumn(m.x, m.y, Resize, Over);

			if (Resize)
			{
				if (m.Double())
				{
					Resize->Width(Resize->GetContentSize() + DEFAULT_COLUMN_SPACING);
					Invalidate();
				}
				else
				{
					DragMode = RESIZE_COLUMN;
					d->DragData = (int)Columns.IndexOf(Resize);
					Capture(true);
				}
			}
			/*
			else
			{
				DragMode = CLICK_COLUMN;
				d->DragData = Columns.IndexOf(Over);
				if (Over)
				{
					Over->Value(true);
					LRect r = Over->GetPos();
					Invalidate(&r);
					Capture(true);
				}
			}
			*/
		}
		else if (rItems.Overlap(m.x, m.y))
		{
			Focus(true);
			Capture(true);
			d->LastClick.x = m.x;
			d->LastClick.y = m.y;

			d->LastHit = ItemAtPoint(m.x, m.y, true);
			if (d->LastHit)
			{
				LPoint c = ScrollPxPos();
				m.x += c.x;
				m.y += c.y;
				d->LastHit->_MouseClick(m);
			}
			else
			{
				SendNotify(LNotification(m, LNotifyContainerClick));
			}
		}
	}
	else if (IsCapturing())
	{
		Capture(false);

		if (rItems.Overlap(m.x, m.y))
		{
			d->LastClick.x = m.x;
			d->LastClick.y = m.y;

			d->LastHit = ItemAtPoint(m.x, m.y);
			if (d->LastHit)
			{
				LPoint c = ScrollPxPos();
				m.x += c.x;
				m.y += c.y;
				d->LastHit->_MouseClick(m);
			}
		}
	}

	d->CurrentClick = NULL;	
}

void LTree::OnMouseMove(LMouse &m)
{
	if (!IsCapturing())
		return;

	TREELOCK(this)

	switch (DragMode)
	{
		/*
		case DRAG_COLUMN:
		{
			if (DragCol)
			{
				LPoint p;
				PointToScreen(p);

				LRect r = DragCol->GetPos();
				r.Offset(-p.x, -p.y); // to view co-ord

				r.Offset(m.x - DragCol->GetOffset() - r.x1, 0);
				if (r.x1 < 0) r.Offset(-r.x1, 0);
				if (r.x2 > X()-1) r.Offset((X()-1)-r.x2, 0);

				r.Offset(p.x, p.y); // back to screen co-ord
				DragCol->SetPos(r, true);
				r = DragCol->GetPos();
			}
			break;
		}
		*/
		case RESIZE_COLUMN:
		{
			LItemColumn *c = Columns[d->DragData];
			if (c)
			{
				// int OldWidth = c->Width();
				int NewWidth = m.x - c->GetPos().x1;

				c->Width(MAX(NewWidth, 4));
				_ClearDs(d->DragData);
				Invalidate();
			}
			break;
		}
		default:
		{
			if (rItems.Overlap(m.x, m.y))
			{
				if (abs(d->LastClick.x - m.x) > DRAG_THRESHOLD ||
					abs(d->LastClick.y - m.y) > DRAG_THRESHOLD)
				{
					OnItemBeginDrag(d->LastHit, m);
					Capture(false);
				}
			}
			break;
		}
	}
}

void LTree::OnPosChange()
{
	TREELOCK(this)
		
	if (Columns.Length() == 0 &&
		d->LastLayoutPx != GetClient().X())
		d->LayoutDirty = true;
	LLayout::OnPosChange();
	_UpdateScrollBars();
}

void LTree::OnPaint(LSurface *pDC)
{
	TREELOCK(this)
	LCssTools Tools(this);

	#if 0 // coverage testing...
	pDC->Colour(LColour(255, 0, 255));
	pDC->Rectangle();
	#endif

	rItems = GetClient();
	LFont *f = GetFont();
	if (ColumnHeaders())
	{
		ColumnHeader.ZOff(rItems.X()-1, f->GetHeight() + 4);
		PaintColumnHeadings(pDC);
		rItems.y1 = ColumnHeader.y2 + 1;
	}
	else
	{
		ColumnHeader.ZOff(-1, -1);
	}

	d->IconTextGap = GetFont()->GetHeight() / 6;
	auto cText = LColour(L_TEXT);
	auto cWs   = LColour(L_WORKSPACE);
	LColour Fore = Tools.GetFore(&cText);
	LColour Background = Tools.GetBack(&cWs, 0);

	// icon cache
	if (GetImageList() &&
		!d->IconCache)
	{
		int CacheHeight = MAX(LSysFont->GetHeight(), GetImageList()->Y());
		
		if (d->IconCache.Reset(new LMemDC) &&
			d->IconCache->Create(GetImageList()->X(), CacheHeight, GdcD->GetColourSpace()))
		{
			if (d->IconCache->GetColourSpace() == CsIndex8)
			{
				d->IconCache->Palette(new LPalette(GdcD->GetGlobalColour()->GetPalette()));
			}

			d->IconCache->Colour(Background);
			d->IconCache->Rectangle();
			d->IconCache->Op(GDC_ALPHA);

			GetImageList()->Lock();
			int DrawY = (CacheHeight - GetImageList()->TileY()) >> 1;
			LAssert(DrawY >= 0);
			for (int i=0; i<GetImageList()->GetItems(); i++)
			{
				GetImageList()->Draw(d->IconCache, i * GetImageList()->TileX(), DrawY, i, Background);
			}
			GetImageList()->Unlock();
			d->IconCache->Unlock();
		}
	}

	// scroll
	LPoint s = ScrollPxPos();
	int Ox, Oy;
	pDC->GetOrigin(Ox, Oy);
	pDC->SetOrigin(Ox + s.x, Oy + s.y);

	// selection colour
	LArray<int> ColPx;
	LItem::ItemPaintCtx Ctx;
	Ctx.pDC = pDC;
	if (Columns.Length() > 0)
	{
		Ctx.Columns = (int)Columns.Length();
		for (int i=0; i<Columns.Length(); i++)
			ColPx[i] = Columns[i]->Width();
	}
	else
	{
		Ctx.Columns = 1;
		ColPx[0] = rItems.X();
	}
	Ctx.ColPx = &ColPx[0];
	Ctx.Fore = Fore;
	Ctx.Back = Background;
	Ctx.TxtBack = Background;
	LColour SelFore(Focus() ? L_FOCUS_SEL_FORE : L_NON_FOCUS_SEL_FORE);
	LColour SelBack(Focus() ? L_FOCUS_SEL_BACK : L_NON_FOCUS_SEL_BACK);

	// layout items
	if (d->LayoutDirty)
	{
		_Pour();
	}

	// paint items
	ZeroObj(d->LineFlags);
	List<LTreeItem>::I it = Items.begin();
	for (LTreeItem *i = *it; i; i=*++it)
		i->OnPaint(Ctx);

	pDC->SetOrigin(Ox, Oy);
	if (d->Limit.y-s.y < rItems.Y())
	{
		// paint after items
		pDC->Colour(Background);
		pDC->Rectangle(rItems.x1, d->Limit.y - s.y, rItems.x2, rItems.y2);
	}

	if (DropStatus)
	{
		auto r = DropStatus->pos;
		pDC->Colour(LColour(L_WORKSPACE).Mix(LColour(L_FOCUS_SEL_BACK)));
		pDC->Rectangle(&r);
	}
}

int LTree::OnNotify(LViewI *Ctrl, LNotification &n)
{
	switch (Ctrl->GetId())
	{
		case IDC_HSCROLL:
		case IDC_VSCROLL:
		{
			TREELOCK(this)
			if (n.Type == LNotifyScrollBarCreate)
			{
				_UpdateScrollBars();
				if (VScroll)
				{
					if (HasItem(d->ScrollTo))
						d->ScrollTo->ScrollTo();
					d->ScrollTo = NULL;
				}
			}

			Invalidate();
			break;
		}
	}

	return LLayout::OnNotify(Ctrl, n);
}

LMessage::Result LTree::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_SCROLL_TO:
		{
			LTreeItem *Item = (LTreeItem*)Msg->A();
			if (!HasItem(Item))
				break;
			
			if (VScroll)
				Item->ScrollTo();
			break;
		}
	}

	return LItemContainer::OnEvent(Msg);
}

LTreeItem *LTree::Insert(LTreeItem *Obj, ssize_t Pos)
{
	TREELOCK(this)
		
	LTreeItem *NewObj = LTreeNode::Insert(Obj, Pos);
	if (NewObj)
		NewObj->_SetTreePtr(this);
	
	return NewObj;
}

bool LTree::HasItem(LTreeItem *Obj, bool Recurse)
{
	TREELOCK(this)
	if (!Obj)
		return false;
	return LTreeNode::HasItem(Obj, Recurse);
}

bool LTree::Remove(LTreeItem *Obj)
{
	TREELOCK(this)
		
	bool Status = false;
	if (Obj && Obj->Tree == this)
	{
		Obj->Remove();
		Status = true;
	}
	
	return Status;
}

void LTree::RemoveAll()
{
	TREELOCK(this)
		
	List<LTreeItem>::I it = Items.begin();
	for (LTreeItem *i=*it; i; i=*++it)
		i->_Remove();

	Invalidate();
}

void LTree::Empty()
{
	TREELOCK(this)
		
	LTreeItem *i;
	while ((i = Items[0]))
		Delete(i);	
}

bool LTree::Delete(LTreeItem *Obj)
{
	bool Status = false;
	
	TREELOCK(this)
		
	if (Obj)
	{
		LTreeItem *i;
		while ((i = Obj->Items[0]))
		{
			Delete(i);
		}
		
		Obj->Remove();
		DeleteObj(Obj);
		Status = true;
	}
	
	return Status;
}

void LTree::OnPulse()
{
	TREELOCK(this)
		
	if (d->DropTarget)
	{
		int64 p = LCurrentTime() - d->DropSelectTime;
		if (p >= 1000)
		{
			SetPulse();

			if (!d->DropTarget->Expanded() &&
				d->DropTarget->GetChild())
			{
				d->DropTarget->Expanded(true);
			}
		}
	}	

	if (InsideDragOp())
	{
		LMouse m;
		if (GetMouse(m))
		{
			if (!m.Left() && !m.Right() && !m.Middle())
			{
				// Be robust against missing drag exit events (Mac specific?)
				InsideDragOp(false);
			}
			else
			{
				LRect c = GetClient();
				if (VScroll)
				{
					if (m.y < DRAG_SCROLL_EDGE)
					{
						// Scroll up...
						VScroll->Value(VScroll->Value() - DRAG_SCROLL_Y);
					}
					else if (m.y > c.Y() - DRAG_SCROLL_EDGE)
					{
						// Scroll down...
						VScroll->Value(VScroll->Value() + DRAG_SCROLL_Y);
					}
				}

				if (HScroll)
				{
					if (m.x < DRAG_SCROLL_EDGE)
					{
						// Scroll left...
						HScroll->Value(HScroll->Value() - DRAG_SCROLL_X);
					}
					else if (m.x > c.X() - DRAG_SCROLL_EDGE)
					{
						// Scroll right...
						HScroll->Value(HScroll->Value() + DRAG_SCROLL_X);
					}
				}
			}
		}
	}
}

int LTree::GetContentSize(int ColumnIdx)
{
	TREELOCK(this)
		
	int MaxPx = 0;
	
	List<LTreeItem>::I it = Items.begin();
	for (LTreeItem *i = *it; i; i=*++it)
	{
		int ItemPx = i->GetColumnSize(ColumnIdx);
		MaxPx = MAX(ItemPx, MaxPx);
	}

	return MaxPx;
}

LCursor LTree::GetCursor(int x, int y)
{
	TREELOCK(this)
		
	LItemColumn *Resize = NULL, *Over = NULL;
	HitColumn(x, y, Resize, Over);

	return (Resize) ? LCUR_SizeHor : LCUR_Normal;
}

void LTree::OnDragEnter()
{
	TREELOCK(this)
		
	InsideDragOp(true);
	SetPulse(120);
}

void LTree::OnDragExit()
{
	TREELOCK(this)
		
	InsideDragOp(false);
	SetPulse();
	SelectDropTarget(0);

	LItemContainer::OnDragExit();
}

void LTree::SelectDropTarget(LTreeItem *Item)
{
	TREELOCK(this)
		
	if (Item != d->DropTarget)
	{
		bool Update = (d->DropTarget != 0) ^ (Item != 0);
		LTreeItem *Old = d->DropTarget;
		
		d->DropTarget = Item;
		if (Old)
		{
			Old->Update();
		}

		if (d->DropTarget)
		{
			d->DropTarget->Update();
			d->DropSelectTime = LCurrentTime();
		}

		if (Update)
		{
			OnFocus(true);
		}
	}
}

bool LTree::Select(LTreeItem *Obj)
{
	TREELOCK(this)
		
	bool Status = false;
	if (Obj && IsAttached())
	{
		Obj->Select(true);
		Status = true;
	}
	else if (d->Selection.Length())
	{		
		d->Selection.Empty();
		OnItemSelect(0);
		Status = true;
	}

	return Status;
}

LTreeItem *LTree::Selection()
{
	TREELOCK(this)
	return d->Selection[0];
}

bool LTree::GetItems(LArray<LItem*> &arr, bool selectedOnly)
{
	if (selectedOnly)
	{
		for (auto s: d->Selection)
			arr.Add(s);
	}
	else
	{
		ForEach([arr](auto i) mutable
		{
			arr.Add(i);
		});
	}

	return arr.Length() > 0;
}

bool LTree::ForAllItems(std::function<void(LTreeItem*)> Callback)
{
	TREELOCK(this)
	return ForEach(Callback) > 0;
}

void LTree::OnItemClick(LTreeItem *Item, LMouse &m)
{
	if (!Item)
		return;

	TREELOCK(this)

	Item->OnMouseClick(m);
	if (!m.Ctrl() && !m.Shift())
		SendNotify(LNotification(m));
}

void LTree::OnFocus(bool b)
{
	TREELOCK(this)

	// errors during deletion of the control can cause 
	// this to be called after the destructor
	if (d)
	{
		List<LTreeItem>::I it = d->Selection.begin();
		for (LTreeItem *i=*it; i; i=*++it)
			i->Update();
	}
}

static void LTreeItemUpdateAll(LTreeNode *n)
{
	for (LTreeItem *i=n->GetChild(); i; i=i->GetNext())
	{
		i->Update();
		LTreeItemUpdateAll(i);
	}
}

void LTree::UpdateAllItems()
{
	TREELOCK(this)

	d->LayoutDirty = true;
	LTreeItemUpdateAll(this);
}

void LTree::SetVisualStyle(ThumbStyle Btns, bool JoiningLines)
{
	TREELOCK(this)

	d->Btns = Btns;
	d->JoiningLines = JoiningLines;
	Invalidate();
}

void LTree::OnItemBeginDrag(LTreeItem *Item, LMouse &m)
{
	if (!Item)
		return;

	TREELOCK(this)

	auto clientHandled = Item->OnBeginDrag(m);
	if (DragItem && !clientHandled)
	{
		// Do internal handling of item drag...
		Drag(this, NULL, DROPEFFECT_MOVE);
	}
}

LItemContainer::ContainerItemDrop LTree::GetItemReorderPos(LPoint pt)
{
	LItemContainer::ContainerItemDrop inf;
	ReorderPos(inf, pt, 0);
	return inf;
}

bool LTree::OnReorderDrop(ContainerItemDrop &dest, ContainerItemsDrag &source)
{
	bool screenDirty = false;

	if (DragItem & ITEM_DRAG_REORDER)
	{
		bool allowDepthChange = TestFlag(DragItem, ITEM_DEPTH_CHANGE);
		
		for (size_t i=0; i<source.items; i++)
		{
			auto moveItem = dynamic_cast<LTreeItem*>(source.item[i]);
			if (!moveItem)
				continue;
			LTreeNode *parent = moveItem->GetParent() ? moveItem->GetParent() : moveItem->GetTree();
			auto prev = dynamic_cast<LTreeItem*>(dest.prev);
			auto next = dynamic_cast<LTreeItem*>(dest.next);
			if (!prev && !next)
				continue;

			#define DEPTH_CHECK(ptr) \
				if (ptr->GetTree() != moveItem->GetTree() && \
					!allowDepthChange) \
					continue;

			if (!parent)
			{
				LAssert(!"Should always have some parent?");
				continue;
			}

			if (prev)
			{
				DEPTH_CHECK(prev);
				
				// Insert AFTER the prev item...
				parent->Items.Delete(moveItem);
				auto destIdx = parent->Items.IndexOf(prev);
				if (destIdx >= 0)
				{
					parent->Items.Insert(moveItem, destIdx + 1);
					screenDirty = true;
				}
			}
			else if (next)
			{
				DEPTH_CHECK(next);
				
				// Insert BEFORE the next item...
				parent->Items.Delete(moveItem);
				auto destIdx = parent->Items.IndexOf(next);
				if (destIdx >= 0)
				{
					parent->Items.Insert(moveItem, destIdx);
					screenDirty = true;
				}
			}
		}

		if (screenDirty)
			SendNotify(LNotifyContainerReorder);
	}

	return screenDirty;
}

