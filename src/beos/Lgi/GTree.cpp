//////////////////////////////////////////////////////////////////////////////
LTreeItem::LTreeItem()
{
	Str = 0;
	Tree = 0;
	Parent = 0;
}

LTreeItem::~LTreeItem()
{
	if (Tree)
	{
		Tree->Remove(this);
	}

	DeleteArray(Str);
}

LTreeItem *LTreeItem::GetChild()
{
	if (Tree)
	{
		BListItem *Bi = Tree->ItemUnderAt(this, TRUE, 0);
		LTreeItem *Ti = dynamic_cast<LTreeItem*>(Bi);
		return Ti;
	}
	return 0;
}

LTreeItem *LTreeItem::GetNext()
{
	if (Tree)
	{
		int MyIndex = Tree->IndexOf(this);
		BListItem *Super = Tree->Superitem(this);
		BListItem *Bi = 0;
		if (Super)
		{
			Bi = Tree->ItemUnderAt(Super, TRUE, MyIndex + 1);
		}
		else
		{
			Tree->ItemAt(MyIndex + 1);
		}
		LTreeItem *Ti = dynamic_cast<LTreeItem*>(Bi);
		return Ti;
	}
	return 0;
}

char *LTreeItem::GetText(int i)
{
	return Str;
}

bool LTreeItem::SetText(const char *s, int i)
{
	DeleteArray(Str);
	Str = NewStr(s);
	return Str != 0;
}

int LTreeItem::GetImage(int Flags)
{
	return Sys_Image;
}

void LTreeItem::SetImage(int i)
{
	Sys_Image = i;
}

int GTreeCompareFunc(const BListItem *a, const BListItem *b)
{
	return ((LTreeItem*)a)->Index - ((LTreeItem*)b)->Index;
}

#define ITEM_HEIGHT 16.0

LTreeItem *LTreeItem::Insert(LTreeItem *Obj, int Pos)
{
	LTreeItem *NewObj = (Obj) ? Obj : new LTreeItem;
	if (NewObj AND Tree)
	{
		bool Lock = Tree->LockLooper();
		Obj->Index = Tree->CountItemsUnder(this, true);
		Obj->Tree = Tree;
		Obj->Parent = this;
		Tree->AddUnder(Obj, this);
		Tree->SortItemsUnder(this, true, GTreeCompareFunc);
		if (Lock) Tree->UnlockLooper();
	}
	return NewObj;
}

void LTreeItem::Update()
{
	if (Parent)
	{
		if (Tree->LockLooper())
		{
			int32 i = Tree->IndexOf(this); // FullList
			BOutlineListView *t = dynamic_cast<BOutlineListView*>(Tree->GetBView());
			if (t)
			{
				BRect r = t->ItemFrame(i);
				t->Invalidate(r);
			}
			
			Tree->UnlockLooper();
		}
	}
}

bool LTreeItem::Selected()
{
	return IsSelected();
}

void LTreeItem::Selected(bool b)
{
	if (Tree)
	{
		Tree->BOutlineListView::Select(Tree->FullListIndexOf(this));
	}
}

bool LTreeItem::Expanded()
{
	return IsExpanded();
}

void LTreeItem::Expanded(bool b)
{
	SetExpanded(b);
}

void LTreeItem::DrawItem(BView *owner, BRect frame, bool complete)
{
	rgb_color color;
	if (IsSelected())
	{
		color.red = 192;
		color.green = 192;
		color.blue = 192;
	}
	else
	{
		color = owner->ViewColor();
	}

	owner->SetHighColor(color);
	owner->SetLowColor(color);
	owner->FillRect(frame);
	
	int Image = -1;
	int x = 4;
	if (Tree->ImageList)
	{
		Image = GetImage(Selected());
		x += 16;
	}

	char *Text = GetText(0);
	if (!Text)
	{
		Text = Str;
	}
	if (Text)
	{
		owner->MovePenTo(frame.left+x, frame.bottom-2);
		owner->SetHighColor(IsSelected() ? BeRGB(255, 255, 255) : BeRGB(0, 0, 0));
		owner->DrawString(Text);
	}

	if (Image >= 0)
	{
		int x = Image * 16;
		BBitmap *Bmp = Tree->ImageList->Handle();
		BRect S(x, 0, x+15, 15);
		BRect D(frame.left, frame.top, frame.left+15, frame.top+15);
		drawing_mode Mode = owner->DrawingMode();
		owner->SetDrawingMode(B_OP_OVER);
		owner->DrawBitmapAsync(Bmp, S, D);
		owner->SetDrawingMode(Mode);
	}
}

void LTree::OnItemSelect(LTreeItem *Item)
{
	if (Item)
	{
		Item->OnSelect();
	}
}

void LTree::OnItemExpand(LTreeItem *Item, bool Expand)
{
	if (Item)
	{
		Item->OnExpand(Expand);
	}
}

//////////////////////////////////////////////////////////////////////////////
LTree::LTree(int id, int x, int y, int cx, int cy, char *name) :
	LControl(this),
	BOutlineListView(	BRect(x, y, x+cx-14, y+cy-14),
						"List",
						B_SINGLE_SELECTION_LIST),
	ResObject(Resources, Res_TreeView)
{
	SetId(id);
	Pos.Set(x, y, x+cx, y+cy);
	if (name) LControl::Name(name);

	Lines = true;
	Buttons = true;
	LinesAtRoot = true;
	EditLabels = false;
	ScrollView = 0;
	SetViewColor(255, 255, 255);
}

LTree::~LTree()
{
	Lines = false;
}

bool LTree::SetPos(LRect &p, bool Repaint)
{
	ScrollView->MoveTo(p.x1, p.y1);
	ScrollView->ResizeTo(p.X()-1, p.Y()-1);
	ResizeTo(p.X()-16, p.Y()-16);
	Pos = p;
	return TRUE;
}

bool LTree::Attach(LViewI *parent)
{
	SetParent(parent);
	if (GetParent())
	{
		ScrollView = new BScrollView(	"scroll_cities",
										this,
										B_FOLLOW_LEFT | B_FOLLOW_TOP,
										0,
										TRUE,
										TRUE,
										B_NO_BORDER);
		if (ScrollView)
		{
			GetParent()->GetBView()->AddChild(ScrollView);
			ScrollView->MoveTo(Pos.x1, Pos.y1);
			ScrollView->ResizeTo(Pos.X() + 3, Pos.Y() + 3);

			// standard white background where we don't draw
			ScrollView->SetViewColor(255, 255, 255);

			// set font size
			BFont f;
			GetBView()->GetFont(&f);
			f.SetSize(11.0);
			GetBView()->SetFont(&f);

			return TRUE;
		}
	}
	return FALSE;
}

void LTree::MouseDown(BPoint point)
{	
	MouseClickEvent(true);
	BOutlineListView::MouseDown(point);
	MouseClickEvent(false);
}

void LTree::SelectionChanged()
{
	int32 Index = FullListCurrentSelection();
	if (Index >= 0)
	{
		LTreeItem *Item = (LTreeItem*) FullListItemAt(Index);
		if (Item)
		{
			OnItemSelect(Item);
		}
	}
}

void LTree::OnMouseClick(LMouse &m)
{
	if (LockLooper())
	{
		int32 Index = FullListCurrentSelection();
		if (Index >= 0)
		{
			LTreeItem *Item = (LTreeItem*) FullListItemAt(Index);
			if (Item)
			{
				OnItemClick(Item, m);
			}
		}
		
		UnlockLooper();
	}
}

int LTree::OnEvent(BMessage *Msg)
{
	return LView::OnEvent(Msg);
}

LTreeItem *LTree::Insert(LTreeItem *Obj, int Pos)
{
	LTreeItem *NewObj = (Obj) ? Obj : new LTreeItem;
	if (NewObj)
	{
		NewObj->Tree = this;
		NewObj->Parent = NULL;
		
		bool Lock = LockLooper();
		AddItem(NewObj, (Pos < 0) ? 100000000 : Pos);
		if (Lock) UnlockLooper();
	}
	return NewObj;
}

bool LTree::Remove(LTreeItem *Obj)
{
	if (Obj)
	{
		RemoveItem(Obj);
		return TRUE;
	}
	return FALSE;
}

void LTree::RemoveAll()
{
	MakeEmpty();
}

bool LTree::Delete(LTreeItem *Obj)
{
	bool Status = FALSE;
	if (Obj)
	{
		RemoveItem(Obj);
	}
	return Status;
}

bool LTree::Select(LTreeItem *Obj)
{
	bool Status = FALSE;
	if (Obj)
	{
		int32 Index = IndexOf(Obj);
		BListView::Select(Index);
	}
	return Status;
}

LTreeItem *LTree::Selection()
{
	LTreeItem *Item = 0;
	int Index = CurrentSelection(0);
	if (Index >= 0)
	{
		Item = (LTreeItem*) ItemAt(Index);
	}
	return Item;
}

void LTree::OnItemClick(LTreeItem *Item, LMouse &m)
{
	if (Item)
	{
		Item->OnMouseClick(m);
	}
}

void LTree::OnItemBeginDrag(LTreeItem *Item, int Flags)
{
	if (Item)
	{
		LMouse m;
		ZeroObj(m);
		m.Flags = Flags;
		Item->OnBeginDrag(m);
	}
}

