#include "lgi/common/Lgi.h"
#include "lgi/common/DataGrid.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Combo.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Menu.h"

enum LDataGridControls
{
	IDC_DELETE = 2000,
	IDC_COPY,
	IDC_EDIT,
};
#define M_DELETE_LATER		(M_USER+2000)
#define CELL_EDGE			2

struct LDataGridPriv
{
	LDataGrid *This;
	int Col;
	LView *e;
	LView *DeleteLater;
	LListItem *Cur;
	bool Dirty, PosDirty;
	LArray<LDataGrid::LDataGridFlags> Flags;
	LArray<LVariant> ColumnArgs;
	LListItem *NewRecord;
	LDataGrid::ItemFactory Factory;
	void *UserData;
	LDataGrid::ItemArray Dropped;
	LDataGrid::IndexArray Deleted;
	char *SrcFmt;
	char *AcceptFmt;

	LDataGridPriv(LDataGrid *t);
	void Save();
	void UpdatePos();
	void Create(int NewCol = -1);
	void MoveCell(int dx);
	void Invalidate();

	LListItem *NewItem()
	{
		if (Factory)
			return Factory(UserData);
		
		return new LListItem;
	}
};

class LDataGridEdit : public LEdit
{
	LDataGridPriv *d;

public:
	LDataGridEdit(LDataGridPriv *data, int id, int x, int y, int cx, int cy, const char *txt) :
		LEdit(id, x, y, cx, cy, txt)
	{
		d = data;
	}

	bool OnKey(LKey &k)
	{
		if (!LEdit::OnKey(k) && k.Down())
		{
			if (k.IsChar)
			{
			}
			else
			{
				switch (k.vkey)
				{
					case LK_RETURN:
					{
						LListItem *s = d->This->GetSelected();
						int Idx = s ? d->This->IndexOf(s) : -1;
						d->Save();
						if (Idx >= 0)
						{
							s->Select(false);
							if ((s = d->This->ItemAt(Idx + 1)))
								s->Select(true);

						}
						break;
					}
					case LK_LEFT:
					{
						LRange r = GetSelectionRange();
						if (r.Len && GetCaret() ==  0)
						{
							d->MoveCell(-1);
							return true;
						}
						break;
					}
					case LK_RIGHT:
					{
						LRange r = GetSelectionRange();
						if (r.Len == 0)
							break;

						const char16 *Txt = NameW();
						r.Len = Txt ? StrlenW(Txt) : 0;
						auto Cursor = GetCaret();
						if (Cursor >= r.Len)
						{
							d->MoveCell(1);
							return true;
						}
						break;
					}
					case LK_PAGEUP:
					case LK_PAGEDOWN:
					case LK_UP:
					case LK_DOWN:
					{
						bool b = d->This->OnKey(k);
						d->Invalidate();
						return b;
						break;
					}
				}
			}
		}

		return false;
	}
};

class LDataGridCombo : public LCombo
{
	LDataGridPriv *d;

public:
	LDataGridCombo(LDataGridPriv *priv, int id, LRect &rc) : LCombo(id, rc.x1, rc.y1, rc.X(), rc.Y(), 0)
	{
		d = priv;
	}
	
	bool OnKey(LKey &k)
	{
		if (!k.IsChar && k.Down())
		{
			if (k.c16 == LK_LEFT)
			{
				d->MoveCell(-1);
			}
			else if (k.c16 == LK_RIGHT)
			{
				d->MoveCell(1);
			}
		}
		
		return LCombo::OnKey(k);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
LDataGridPriv::LDataGridPriv(LDataGrid *t)
{
	Factory = 0;
	UserData = 0;
	SrcFmt = AcceptFmt = 0;
	DeleteLater = NULL;

	NewRecord = 0;
	Dirty = false;
	PosDirty = false;
	Cur = 0;
	This = t;
	Col = 0;
	e = 0;
}

void LDataGridPriv::Save()
{
	if (Dirty && Cur && e)
	{
		if (Flags[Col] & LDataGrid::GDG_INTEGER)
		{
			int64 OldVal = e->Value();
			if (OldVal < 0 && Cur == NewRecord)
				return;

			char t[64];
			sprintf_s(t, sizeof(t), LPrintfInt64, OldVal);
			Cur->SetText(t, Col);
		}
		else
		{
			const char *OldTxt = e->Name();
			if (!OldTxt && Cur == NewRecord)
				return;

			Cur->SetText(OldTxt, Col);
		}

		if (Cur == NewRecord)
		{
			This->Insert(NewRecord = NewItem());
		}
	}
}

void LDataGridPriv::UpdatePos()
{
	if (e && Cur)
	{
		LRect *r = Cur->GetPos(Col);
		if (r)
		{
			LRect rc = *r;

			if (Flags[Col] & LDataGrid::GDG_INTEGER)
			{
				rc.Offset(2, 0);
			}
			else
			{
				rc.x2--;
				rc.y2--;
			}

			e->SetPos(rc);
			e->Focus(true);
			
			
			PosDirty = false;
		}
	}
}

void LDataGridPriv::Invalidate()
{
	if (e)
	{
		LRect p = e->GetPos();
		p.Size(-CELL_EDGE, -CELL_EDGE);
		This->Invalidate(&p);
	}
}

void LDataGridPriv::MoveCell(int Dx)
{
	for (int i=Col+Dx; i>=0 && i<This->GetColumns(); i+=Dx)
	{
		if (!(Flags[i] & LDataGrid::GDG_READONLY))
		{
			Create(i);
			break;
		}
	}
}

void LDataGridPriv::Create(int NewCol)
{
	if (!This->IsAttached())
		return;

	LListItem *i = This->GetSelected();
	if (!i)
		return;

	if (!Cur || i != Cur || (NewCol >= 0 && NewCol != Col))
	{
		int OldCtrl = Flags[Col] & LDataGrid::GDG_INTEGER;

		Save();
		Cur = i;

		if (NewCol >= 0)
			Col = NewCol;

		if (Flags[Col] & LDataGrid::GDG_READONLY)
		{
			// Pick a valid column
			int NewCol = -1;
			for (int i=0; i<This->GetColumns(); i++)
			{
				if (Col != i && !(Flags[i] & LDataGrid::GDG_READONLY))
				{
					NewCol = i;
					break;
				}
			}

			if (NewCol < 0)
				return;

			Col = NewCol;
		}

		int NewCtrl = Flags[Col] & LDataGrid::GDG_INTEGER;

		const char *CurText = i->GetText(Col);
		LRect *r = i->GetPos(Col);
		if (r)
		{
			LDataGridEdit *Edit = 0;
			LCombo *Combo = 0;
			LRect rc = r;
			
			if (!e || (NewCtrl ^ OldCtrl))
			{
				if (e)
				{
					e->Detach();
					Invalidate();
					DeleteLater = e;
					e = NULL;					
					This->PostEvent(M_DELETE_LATER);
				}

				if (Flags[Col] & LDataGrid::GDG_INTEGER)
				{
                    e = Combo = new LDataGridCombo(this, IDC_EDIT, rc);
					if (e)
					{
						LVariant &e = ColumnArgs[Col];
						if (e.Type == GV_LIST)
						{
							for (auto v: *e.Value.Lst)
							{
								char *s = v->Str();
								Combo->Insert(s);
							}
						}

						if (CurText)
							Combo->Name(CurText);
						else
							Combo->Value(-1);
					}
				}
				else
				{
					rc.x2 -= 1;
					rc.y2 -= 1;

					// Create edit control with the correct info for the cell
                    e = Edit = new LDataGridEdit(this, IDC_EDIT, rc.x1, rc.y1, rc.X(), rc.Y(), CurText);
					if (e)
					{
						e->Sunken(false);
					}
				}

				e->SetPos(rc);
				e->Attach(This);
				Invalidate();
			}
			else
			{
				Invalidate();
				e->SetPos(rc);
				e->Name(CurText);
				Invalidate();
			}

			if (Edit)
				Edit->Select(0);
			e->Focus(true);
		}			
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LDataGrid::LDataGrid(int CtrlId, ItemFactory Func, void *userdata) :
	LList(CtrlId, 0, 0, 1000, 1000)
{
	_ObjName = Res_Custom;

	DrawGridLines(true);
	SetPourLargest(true);

	d = new LDataGridPriv(this);
	SetFactory(Func, userdata);
}

LDataGrid::~LDataGrid()
{
	DeleteObj(d);
}

bool LDataGrid::Remove(LListItem *Obj)
{
	if (Obj == d->Cur)
	{
		DeleteObj(d->e);
		d->Cur = 0;
	}
	return LList::Remove(Obj);
}

void LDataGrid::Empty()
{
	d->NewRecord = 0;
	d->Cur = 0;
	DeleteObj(d->e);
	
	LList::Empty();
}

void LDataGrid::OnItemSelect(LArray<LListItem*> &Items)
{
	if (Items.Length() == 1)
	{
		d->Create();
	}
	else if (d->e)
	{
		d->Cur = 0;
		d->DeleteLater = d->e;
		d->e = NULL;
		PostEvent(M_DELETE_LATER);
	}

	LList::OnItemSelect(Items);
}

void LDataGrid::OnItemClick(LListItem *Item, LMouse &m)
{
	if (m.IsContextMenu())
	{
		LSubMenu s;
		s.AppendItem("Copy", IDC_COPY);
		s.AppendItem("Delete", IDC_DELETE);
		m.ToScreen();
		switch (s.Float(this, m.x, m.y, m.Left()))
		{
			case IDC_COPY:
			{
				List<LListItem> Sel;
				GetSelection(Sel);
				LStringPipe p(256);
				int Cols = GetColumns();
				for (auto i: Sel)
				{
					for (int c=0; c<Cols; c++)
					{
						p.Print("%s%s", c ? ", " : "", i->GetText(c));
					}
					p.Print("\n");
				}
				LClipBoard cb(this);
				LAutoString a(p.NewStr());
				cb.Text(a);
				break;
			}
			case IDC_DELETE:
			{
				List<LListItem> Sel;
				GetSelection(Sel);
				d->Deleted.Length(0);
				for (auto i: Sel)
				{
					d->Deleted.Add(IndexOf(i));
				}
				SendNotify(LNotifyItemDelete);
				d->Deleted.Length(0);

				Sel.Delete(d->NewRecord);
				Sel.DeleteObjects();
				break;
			}
		}
	}
	else
	{
		int ClickCol = ColumnAtX(m.x);
		if (ClickCol >= 0 && ClickCol != d->Col)
		{
			d->Create(ClickCol);
		}
	}
}

void LDataGrid::OnCreate()
{
	d->Create(0);
	SetWindow(this);
}

LMessage::Result LDataGrid::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_DELETE_LATER:
		{
			DeleteObj(d->DeleteLater);
			break;
		}
	}

	return LList::OnEvent(Msg);
}

int LDataGrid::OnNotify(LViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_EDIT:
		{
			d->Dirty = true;
			break;
		}
		case IDC_VSCROLL:
		{
			d->PosDirty = true;
			break;
		}
	}

	return LList::OnNotify(c, f);
}

void LDataGrid::SetColFlag(int Col, LDataGridFlags Flags, LVariant *Arg)
{
	d->Flags[Col] = Flags;
	if (Arg)
		d->ColumnArgs[Col] = *Arg;
}

bool LDataGrid::OnMouseWheel(double Lines)
{
	LList::OnMouseWheel(Lines);
	d->UpdatePos();
	return true;
}

void LDataGrid::OnPaint(LSurface *pDC)
{
	LList::OnPaint(pDC);

	d->UpdatePos();
	if (d->e && !(d->Flags[d->Col] & GDG_INTEGER))
	{
		pDC->Colour(L_BLACK);
		LRect p = d->e->GetPos();
		p.Size(-CELL_EDGE, -CELL_EDGE);
		pDC->Box(&p);
	}
}

bool LDataGrid::OnLayout(LViewLayoutInfo &Inf)
{
	Inf.Width.Min = 16;
	for (int i=0; i<GetColumns(); i++)
	{
		Inf.Width.Min += ColumnAt(i)->Width();
	}
	Inf.Width.Max = Inf.Width.Min + 20;
	
	Inf.Height.Min = 50;
	Inf.Height.Max = 2000;
	
	return true;
}

bool LDataGrid::CanAddRecord()
{
	return d->NewRecord != 0;
}

void LDataGrid::CanAddRecord(bool b)
{
	if (b)
	{
		if (!d->NewRecord)
			Insert(d->NewRecord = d->NewItem());
	}
	else
	{
		DeleteObj(d->NewRecord);
	}
}

LListItem *LDataGrid::NewItem()
{
	return d->NewItem();
}

void LDataGrid::SetFactory(ItemFactory Func, void *userdata)
{
	d->Factory = Func;
	d->UserData = userdata;
}

void LDataGrid::SetDndFormats(char *SrcFmt, char *AcceptFmt)
{
	d->SrcFmt = SrcFmt;
	d->AcceptFmt = AcceptFmt;
}

int LDataGrid::WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState)
{
	Formats.Supports(d->AcceptFmt);
	return Formats.GetSupported().Length() ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

LDataGrid::ItemArray *LDataGrid::GetDroppedItems()
{
	return &d->Dropped;
}

LDataGrid::IndexArray *LDataGrid::GetDeletedItems()
{
	return &d->Deleted;
}

int LDataGrid::OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState)
{
	if (!d->AcceptFmt)
		return DROPEFFECT_NONE;
	
	for (unsigned n=0; n<Data.Length(); n++)
	{
		LDragData &dd = Data[n];
		if (dd.IsFormat(d->AcceptFmt))
		{
			LVariant *Data = &dd.Data.First();
			if (Data->Type == GV_BINARY)
			{
				LListItem **Item = (LListItem**)Data->Value.Binary.Data;
				auto Items = Data->Value.Binary.Length / sizeof(LListItem*);
				d->Dropped.Length(0);
				for (int i=0; i<Items; i++)
				{
					d->Dropped.Add(Item[i]);
				}
				SendNotify(LNotifyItemItemsDropped);
				return DROPEFFECT_COPY;
			}
		}
	}

	return DROPEFFECT_NONE;
}

void LDataGrid::OnItemBeginDrag(LListItem *Item, LMouse &m)
{
	Drag(this, m.Event, DROPEFFECT_COPY);
}

bool LDataGrid::GetFormats(LDragFormats &Formats)
{
	if (!d->SrcFmt)
		return false;

	Formats.Supports(d->SrcFmt);
	return true;
}

bool LDataGrid::GetData(LArray<LDragData> &Data)
{
	for (unsigned i=0; i<Data.Length(); i++)
	{
		if (Data[i].IsFormat(d->SrcFmt))
		{
			List<LListItem> s;
			if (GetSelection(s))
			{
				LArray<LListItem*> a;
				for (auto it: s)
				{
					a.Add(it);
				}
				Data[i].Data.New().SetBinary(sizeof(LListItem*)*a.Length(), &a[0]);
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
class GDataGridFactory : public LViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (Class &&
			stricmp(Class, "LDataGrid") == 0)
		{
			return new LDataGrid(0);
		}

		return 0;
	}
};

static GDataGridFactory _Factory;
