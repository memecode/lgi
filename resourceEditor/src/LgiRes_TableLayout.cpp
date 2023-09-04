#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_Dialog.h"
#include "lgi/common/Button.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Token.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Menu.h"

#define DRAW_CELL_INDEX			0
#define DRAW_TABLE_SIZE			0

enum Cmds
{
	IDM_ALIGN_X_MIN				= 100,
	IDM_ALIGN_X_CTR,
	IDM_ALIGN_X_MAX,
	IDM_ALIGN_Y_MIN,
	IDM_ALIGN_Y_CTR,
	IDM_ALIGN_Y_MAX,
	IDM_UNMERGE,
	IDM_FIX_TABLE,
	IDM_INSERT_ROW,
	IDM_INSERT_COL,
};

static LColour Blue(0, 30, 222);

/////////////////////////////////////////////////////////////////////
struct Pair { int Pos, Size; };
void CalcCell(LArray<Pair> &p, LArray<double> &s, int Total)
{
	int CurI = 0;
	for (int i=0; i<s.Length(); i++)
	{
		p[i].Size = s[i] * Total;
		p[i].Pos = CurI;
		CurI += p[i].Size;
	}
	p[s.Length()].Pos = Total;
	p[s.Length()].Size = 0;
}

/////////////////////////////////////////////////////////////////////
// Table layout
template<class T>
T SumElements(LArray<T> &a, ssize_t start, ssize_t end)
{
	T Sum = 0;
	for (auto i=start; i<=end; i++)
	{
		Sum += a[i];
	}
	return Sum;
}

void MakeSumUnity(LArray<double> &a)
{
	double s = SumElements<double>(a, 0, a.Length()-1);
	double Diff = s - 1.0;
	if (Diff < 0) Diff *= -1.0;
	if (Diff > 0.001)
	{
		for (int i=0; i<a.Length(); i++)
		{
			a[i] = a[i] / s;
		}
	}
}

enum TableAlign
{
	AlignMin = 0,
	AlignCenter = 1,
	AlignMax = 2
};

const char *TableAlignNames[] =
{
	"Min",
	"Center",
	"Max"
};

class ResTableCell : public LDom
{
public:
	CtrlTable *Table;
	LRect Cell;	// Cell location
	LRect Pos;	// Pixels
	bool Selected;
	LArray<ResDialogCtrl*> Ctrls;
	TableAlign AlignX;
	TableAlign AlignY;
	LAutoString Class; // CSS class for styling
	LAutoString Style; // CSS styles

	ResTableCell(CtrlTable *table, ssize_t cx, ssize_t cy)
	{
		AlignX = AlignY = AlignMin;
		Table = table;
		Selected = false;
		Cell.ZOff(0, 0);
		Cell.Offset((int)cx, (int)cy);
		Pos.ZOff(-1, -1);
	}

	ResTableCell()
	{
		Ctrls.DeleteObjects();
	}
	
	const char *GetClass() override { return "ResTableCell"; }

	void MoveCtrls()
	{
		for (int i=0; i<Ctrls.Length(); i++)
		{
			ResDialogCtrl *c = Ctrls[i];
			if (c)
			{
				LRect r = c->View()->GetPos();

				if (r.X() > Pos.X())
				{
					r.x2 = r.x1 + Pos.X() - 1;
				}
				if (r.Y() > Pos.Y())
				{
					r.y2 = r.y1 + Pos.Y() - 1;
				}
				if (r.x2 > Pos.x2)
				{
					r.Offset(Pos.x2 - r.x2, 0);
				}
				if (r.y2 > Pos.y2)
				{
					r.Offset(0, Pos.y2 - r.y2);
				}

				c->SetPos(r);
			}
		}
	}

	void SetPos(LRect &p)
	{
		int Dx = p.x1 - Pos.x1;
		int Dy = p.y1 - Pos.y1;
		for (int i=0; i<Ctrls.Length(); i++)
		{
			ResDialogCtrl *c = Ctrls[i];
			if (c)
			{
				LRect r = c->View()->GetPos();
				r.Offset(Dx, Dy);
				c->SetPos(r);
			}
		}
		Pos = p;
		MoveCtrls();
	}

	bool GetVariant(const char *Name, LVariant &Value, const char *Array)
	{
		if (stricmp(Name, VAL_Span) == 0)
		{
			Value = Cell.Describe();
		}
		else if (stricmp(Name, VAL_Children) == 0)
		{
			List<LVariant> c;
			for (int i=0; i<Ctrls.Length(); i++)
			{
				c.Insert(new LVariant((ResObject*)Ctrls[i]));
			}
			Value.SetList(&c);
			c.DeleteObjects();
		}
		else if (stricmp(Name, VAL_HorizontalAlign) == 0)
		{
			Value = TableAlignNames[AlignX];
		}
		else if (stricmp(Name, VAL_VerticalAlign) == 0)
		{
			Value = TableAlignNames[AlignY];
		}
		else if (stricmp(Name, VAL_Class) == 0)
		{
			Value = Class.Get();
		}
		else if (stricmp(Name, VAL_Style) == 0)
		{
			Value = Style.Get();
		}
		else return false;

		return true;
	}

	bool SetVariant(const char *Name, LVariant &Value, const char *Array)
	{
		if (stricmp(Name, VAL_Span) == 0)
		{
			LRect r;
			if (r.SetStr(Value.Str()))
			{
				Cell = r;
			}
			else return false;
		}
		else if (stricmp(Name, VAL_Children) == 0)
		{
			for (auto v: *Value.Value.Lst)
			{
				LAssert(v->Type == GV_VOID_PTR);
				ResDialogCtrl *Obj = dynamic_cast<ResDialogCtrl*>((ResObject*) v->Value.Ptr);
				if (Obj)
				{
					Table->SetAttachCell(this);
					LRect r = Obj->View()->GetPos();
					Table->AttachCtrl(Obj, &r);
					Table->SetAttachCell(0);
				}
			}
		}
		else if (stricmp(Name, VAL_HorizontalAlign) == 0)
		{
			if (Value.Str())
			{
				for (int i=0; i<CountOf(TableAlignNames); i++)
				{
					if (stricmp(TableAlignNames[i], Value.Str()) == 0)
					{
						AlignX = (TableAlign)i;
						break;
					}
				}
			}
		}
		else if (stricmp(Name, VAL_VerticalAlign) == 0)
		{
			if (Value.Str())
			{
				for (int i=0; i<CountOf(TableAlignNames); i++)
				{
					if (stricmp(TableAlignNames[i], Value.Str()) == 0)
					{
						AlignY = (TableAlign)i;
						break;
					}
				}
			}
		}
		else if (stricmp(Name, VAL_Class) == 0)
		{
			Class.Reset(Value.ReleaseStr());
		}
		else if (stricmp(Name, VAL_Style) == 0)
		{
			Style.Reset(Value.ReleaseStr());
		}
		else
		{
			return false;
		}

		return true;
	}
	
	void OnMouseClick(LMouse &m)
	{
		if (m.Down() && m.Right())
		{
			LSubMenu *RClick = new LSubMenu;
			if (RClick)
			{
				LSubMenu *s = RClick->AppendSub("Horizontal Align");
				if (s)
				{
					s->AppendItem("Left", IDM_ALIGN_X_MIN, AlignX != AlignMin);
					s->AppendItem("Center", IDM_ALIGN_X_CTR, AlignX != AlignCenter);
					s->AppendItem("Right", IDM_ALIGN_X_MAX, AlignX != AlignMax);
				}
				s = RClick->AppendSub("Vertical Align");
				if (s)
				{
					s->AppendItem("Top", IDM_ALIGN_Y_MIN, AlignY != AlignMin);
					s->AppendItem("Center", IDM_ALIGN_Y_CTR, AlignY != AlignCenter);
					s->AppendItem("Bottom", IDM_ALIGN_Y_MAX, AlignY != AlignMax);
				}
				RClick->AppendItem("Unmerge", IDM_UNMERGE, Cell.X() > 1 || Cell.Y() > 1);
				RClick->AppendItem("Fix Missing Cells", IDM_FIX_TABLE, true);
				RClick->AppendItem("Insert Row", IDM_INSERT_ROW, true);
				RClick->AppendItem("Insert Column", IDM_INSERT_COL, true);

				m.ToScreen();
				switch (RClick->Float(Table, m.x, m.y))
				{
					case IDM_ALIGN_X_MIN:
						AlignX = AlignMin;
						break;
					case IDM_ALIGN_X_CTR:
						AlignX = AlignCenter;
						break;
					case IDM_ALIGN_X_MAX:
						AlignX = AlignMax;
						break;
					case IDM_ALIGN_Y_MIN:
						AlignY = AlignMin;
						break;
					case IDM_ALIGN_Y_CTR:
						AlignY = AlignCenter;
						break;
					case IDM_ALIGN_Y_MAX:
						AlignY = AlignMax;
						break;
					case IDM_UNMERGE:
						Table->UnMerge(this);
						break;
					case IDM_FIX_TABLE:
						Table->Fix();
						break;
					case IDM_INSERT_ROW:
						Table->InsertRow(Cell.y1);
						break;
					case IDM_INSERT_COL:
						Table->InsertCol(Cell.x1);
						break;
				}

				DeleteObj(RClick);
			}
		}
	}
};

enum HandleTypes
{
	LAddCol,
	LAddRow,
	LDeleteCol,
	LDeleteRow,
	LSizeCol,
	LSizeRow,
	LJoinCells,
};

struct OpHandle : public LRect
{
	bool Over;
	HandleTypes Type;
	int Index;
	ResTableCell *a, *b;

	OpHandle &Set(HandleTypes type, int x = -1, int y = -1)
	{
		Type = type;
		Over = false;
		Index = -1;
		a = b = NULL;
		if (x > 0 && y > 0)
			ZOff(x, y);
		return *this;
	}

	void OnPaint(LSurface *pDC)
	{
		#define OFF			1
		int cx = X() >> 1;
		int cy = Y() >> 1;

		pDC->Colour(Over ? LColour::Red : Blue);
		pDC->Rectangle(this);

		pDC->Colour(LColour::White);
		switch (Type)
		{
			case LSizeRow:
			{
				pDC->Line(x1 + cx, y1 + OFF, x1 + cx, y2 - OFF);
				pDC->Line(x1 + cx - 1, y1 + OFF + 1, x1 + cx + 1, y1 + OFF + 1);
				pDC->Line(x1 + cx - 1, y2 - OFF - 1, x1 + cx + 1, y2 - OFF - 1);
				break;
			}
			case LSizeCol:
			{
				pDC->Line(x1 + OFF, y1 + cy, x2 - OFF, y1 + cy);
				pDC->Line(x1 + OFF + 1, y1 + cy - 1, x1 + OFF + 1, y1 + cy + 1);
				pDC->Line(x2 - OFF - 1, y1 + cy - 1, x2 - OFF - 1, y1 + cy + 1);
				break;
			}
			case LAddCol:
			case LAddRow:
			case LJoinCells:
			{
				pDC->Line(x1 + cx, y1 + OFF, x1 + cx, y2 - OFF);
				pDC->Line(x1 + OFF, y1 + cy, x2 - OFF, y1 + cy);
				break;
			}
			case LDeleteCol:
			case LDeleteRow:
			{
				pDC->Line(x1 + OFF, y1 + cy, x2 - OFF, y1 + cy);
				break;
			}
			default:
				LAssert(!"Impl me.");
		}
	}
};

class CtrlTablePrivate
{
public:
	bool InLayout, LayoutDirty;

	// The cell container
	CtrlTable *Table;
	ssize_t CellX, CellY;
	LArray<ResTableCell*> Cells;
	ResTableCell *AttachTo;

	// Column + Row sizes
	LArray<double> ColSize;
	LArray<double> RowSize;

	// Goobers for editing the cell layout
	LArray<OpHandle> Handles;

	int DragRowSize;
	int DragColSize;

	// Methods
	CtrlTablePrivate(CtrlTable *t)
	{
		InLayout = false;
		LayoutDirty = false;
		Table = t;
		CellX = CellY = 2;
		AttachTo = 0;
		Cells[0] = new ResTableCell(t, 0, 0);
		Cells[1] = new ResTableCell(t, 1, 0);
		Cells[2] = new ResTableCell(t, 0, 1);
		Cells[3] = new ResTableCell(t, 1, 1);
		ColSize[0] = 0.5;
		ColSize[1] = 0.5;
		RowSize[0] = 0.5;
		RowSize[1] = 0.5;
		DragRowSize = DragColSize = -1;
	}

	~CtrlTablePrivate()
	{
		Cells.DeleteObjects();
	}

	bool GetSelected(LArray<ResTableCell*> &s)
	{
		for (int i=0; i<Cells.Length(); i++)
		{
			if (Cells[i]->Selected)
				s.Add(Cells[i]);
		}
		return s.Length();
	}

	ResTableCell *GetCellAt(int cx, int cy)
	{
		for (int i=0; i<Cells.Length(); i++)
		{
			if (Cells[i]->Cell.Overlap(cx, cy))
			{
				return Cells[i];
			}
		}

		return 0;
	}

	void Layout(LRect c)
	{
		LAssert(LAppInst->AppWnd);
		auto scale = LAppInst->AppWnd->GetDpiScale();
		int ADD_BORDER = (int) (10.0 * scale.x + 0.5);
		#define CELL_SPACING	1

		if (InLayout)
			return;
		InLayout = true;
		LayoutDirty = false;
		Handles.Length(0);

		int x, y;
		int BtnSize = ADD_BORDER * 2 / 3;
		auto &AddX = Handles.New().Set(LAddCol, BtnSize, BtnSize);
		auto &AddY = Handles.New().Set(LAddRow, BtnSize, BtnSize);
		int BtnX = c.X() - AddX.X() - 2;
		int BtnY = c.Y() - AddY.Y() - 2;
		AddX.Offset(BtnX, 0);
		AddY.Offset(0, BtnY);
		c.x2 = AddX.x2 - ADD_BORDER;
		c.y2 = AddY.y2 - ADD_BORDER;

		if (c.Valid())
		{
			int AvailX = c.X();
			int AvailY = c.Y();

			LArray<Pair> XPair, YPair;
			CalcCell(XPair, ColSize, AvailX);
			CalcCell(YPair, RowSize, AvailY);
			XPair[CellX].Pos = AvailX;

			for (x=0; x<CellX; x++)
			{
				auto &DelCol = Handles.New().Set(LDeleteCol, BtnSize, BtnSize);
				DelCol.Index = x;
				DelCol.Offset(XPair[x].Pos + (XPair[x].Size / 2), BtnY);

				if (x < CellX - 1)
				{
					auto &SizeCol = Handles.New().Set(LSizeCol, BtnSize+4, BtnSize-2);
					SizeCol.Index = x;
					SizeCol.Offset(XPair[x+1].Pos - (SizeCol.X() / 2) - 1, BtnY);
				}
			}

			for (y=0; y<CellY; y++)
			{
				auto &DelRow = Handles.New().Set(LDeleteRow, BtnSize, BtnSize);
				DelRow.Index = y;
				DelRow.Offset(BtnX, YPair[y].Pos + (YPair[y].Size / 2));

				if (y < CellY - 1)
				{
					auto &SizeRow = Handles.New().Set(LSizeRow, BtnSize-2, BtnSize+4);
					SizeRow.Index = y;
					SizeRow.Offset(BtnX, YPair[y+1].Pos - (SizeRow.Y() / 2) - 1);
				}
			}

			for (y=0; y<CellY; y++)
			{
				int Py = YPair[y].Pos;

				for (x=0; x<CellX;)
				{
					int Px = XPair[x].Pos;

					ResTableCell *Cell = GetCellAt(x, y);
					// LgiTrace("Layout[%i,%i] %p\n", x, y, Cell);
					if (Cell)
					{
						if (Cell->Cell.x1 == x &&
							Cell->Cell.y1 == y)
						{
							LRect Pos = Cell->Pos;
							
							// Set cells pixel position, with spanning
							int Ex = XPair[Cell->Cell.x2 + 1].Pos;
							int Ey = YPair[Cell->Cell.y2 + 1].Pos;
							Pos.ZOff(Ex - Px - 1 - CELL_SPACING, Ey - Py - 1 - CELL_SPACING);

							// Offset the cell into place
							Pos.Offset(Px, Py);

							Cell->SetPos(Pos);

							if (Cell->Selected)
							{
								// Add cell joining goobers...
								ResTableCell *c = GetCellAt(x + Cell->Cell.X(), y);
								if (c && c->Selected)
								{
									auto &j = Handles.New().Set(LJoinCells, BtnSize, BtnSize);
									j.a = Cell;
									j.b = c;
									j.Offset(Cell->Pos.x2 - (j.X()>>1), Cell->Pos.y1 + ((Cell->Pos.Y()-j.Y()) >> 1));
								}

								c = GetCellAt(x, y + Cell->Cell.Y());
								// LgiTrace("%s %i,%i+%i = %p\n", Cell->Cell.GetStr(), x, y, Cell->Cell.Y(), c);
								if (c && c->Selected)
								{
									auto &j = Handles.New().Set(LJoinCells, BtnSize, BtnSize);
									j.a = Cell;
									j.b = c;
									j.Offset(Cell->Pos.x1 + ((Cell->Pos.X()-j.X()) >> 1), Cell->Pos.y2 - (j.Y()>>1));
								}
							}
						}

						LAssert(Cell->Cell.X());
						x += Cell->Cell.X();

						Px += Cell->Pos.X() + CELL_SPACING;
					}
					else break;
				}
			}
		}

		InLayout = false;
	}

	bool DeleteCol(int x)
	{
		bool Status = false;
		auto OldCellX = CellX;

		// Delete column 'x'
		for (int y=0; y<CellY; )
		{
			ResTableCell *c = GetCellAt(x, y);
			if (c)
			{
				y = c->Cell.y2 + 1;
				if (c->Cell.X() == 1)
				{
					Cells.Delete(c);
					DeleteObj(c);
				}
				else
				{
					c->Cell.x2--;
				}
				Status = true;
			}
			else break;
		}
		CellX--;

		double Last = ColSize[x];
		ColSize.DeleteAt(x, true);
		for (int i=0; i<ColSize.Length(); i++)
		{
			ColSize[i] = ColSize[i] / (1.0 - Last);
		}

		// Move down all the columns after 'x'
		for (x++; x<OldCellX; x++)
		{
			for (int y=0; y<CellY; y++)
			{
				ResTableCell *c = GetCellAt(x, y);
				if (c)
				{
					if (c->Cell.x1 == x &&
						c->Cell.y1 == y)
					{
						c->Cell.Offset(-1, 0);
					}
				}
			}
		}

		Table->Layout();
		return Status;
	}

	bool DeleteRow(int y)
	{
		bool Status = false;
		int x;
		auto OldCellY = CellY;

		// Delete row 'y'
		for (x=0; x<CellX; )
		{
			ResTableCell *c = GetCellAt(x, y);
			if (c)
			{
				x = c->Cell.x2 + 1;

				if (c->Cell.Y() == 1)
				{
					Cells.Delete(c);
					DeleteObj(c);
				}
				else
				{
					c->Cell.y2--;
				}
				Status = true;
			}
			else break;
		}
		CellY--;

		double Last = RowSize[y];
		RowSize.DeleteAt(y, true);
		for (int i=0; i<RowSize.Length(); i++)
		{
			RowSize[i] = RowSize[i] / (1.0 - Last);
		}

		// Move down all the rows after 'y'
		for (y++; y<OldCellY; y++)
		{
			for (int x=0; x<CellX; x++)
			{
				ResTableCell *c = GetCellAt(x, y);
				if (c)
				{
					if (c->Cell.x1 == x &&
						c->Cell.y1 == y)
					{
						c->Cell.Offset(0, -1);
					}
				}
			}
		}

		Table->Layout();
		return Status;
	}

	ResTableCell *GetCell(ResDialogCtrl *Ctrl)
	{
		for (int i=0; i<Cells.Length(); i++)
		{
			if (Cells[i]->Ctrls.HasItem(Ctrl))
			{
				return Cells[i];
			}
		}

		return 0;
	}
};

CtrlTable::CtrlTable(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Table, load)
{
	d = new CtrlTablePrivate(this);
	AcceptChildren = true;
}

CtrlTable::~CtrlTable()
{
	DeleteObj(d);
}

bool CtrlTable::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	
	LArray<ResTableCell*> s;
	if (d->GetSelected(s) == 1)
	{
		int Id = 150;
		Fields.Insert(this, DATA_STR, Id++, VAL_CellClass, "Cell Class");
		Fields.Insert(this, DATA_STR, Id++, VAL_CellStyle, "Cell Style", -1, true);
	}
	
	return Status;
}

bool CtrlTable::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	
	LArray<ResTableCell*> s;
	ResTableCell *c;
	if (d->GetSelected(s) == 1 && ((c = s[0])) != NULL)
	{
		Fields.Serialize(this, VAL_CellClass, c->Class);
		Fields.Serialize(this, VAL_CellStyle, c->Style);
	}
	
	return Status;
}

void CtrlTable::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	// LgiTrace("Tbl Ref=%i\n", Str->GetRef());
	for (int i=0; i<d->Cells.Length(); i++)
	{
		ResTableCell *c = d->Cells[i];
		for (int n=0; n<c->Ctrls.Length(); n++)
		{
			// LgiTrace("	Ref=%i\n", c->Ctrls[n]->Str->GetRef());
			c->Ctrls[n]->EnumCtrls(Ctrls);
		}
	}
}

bool CtrlTable::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case TableLayoutCols:
		{
			LStringPipe p;
			for (int i=0; i<d->ColSize.Length(); i++)
			{
				if (i) p.Push(",");
				p.Print("%.3f", d->ColSize[i]);
			}
			Value.OwnStr(p.NewStr());
			break;
		}
		case TableLayoutRows:
		{
			LStringPipe p;
			for (int i=0; i<d->RowSize.Length(); i++)
			{
				if (i) p.Push(",");
				p.Print("%.3f", d->RowSize[i]);
			}
			Value.OwnStr(p.NewStr());
			break;
		}
		case TableLayoutCell:
		{
			auto Coords = LString(Array).SplitDelimit(",");
			if (Coords.Length() != 2)
				return false;

			Value = d->GetCellAt((int)Coords[0].Int(), (int)Coords[1].Int());
			break;
		}
		default:
		{
			LAssert(!"Invalid property.");
			return false;
		}
	}

	return true;
}

bool CtrlTable::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case TableLayoutCols:
		{
			d->Cells.DeleteObjects();

			LToken t(Value.Str(), ",");
			d->ColSize.Length(0);
			for (int i=0; i<t.Length(); i++)
			{
				d->ColSize.Add(atof(t[i]));
			}

			MakeSumUnity(d->ColSize);

			d->CellX = d->ColSize.Length();
			break;
		}
		case TableLayoutRows:
		{
			d->Cells.DeleteObjects();

			LToken t(Value.Str(), ",");
			d->RowSize.Length(0);
			for (int i=0; i<t.Length(); i++)
			{
				d->RowSize.Add(atof(t[i]));
			}

			MakeSumUnity(d->RowSize);

			d->CellY = d->RowSize.Length();
			break;
		}
		case TableLayoutCell:
		{
			auto Coords = LString(Array).SplitDelimit(",");
			if (Coords.Length() != 2)
				return false;

			auto Cx = Coords[0].Int();
			auto Cy = Coords[1].Int();
			ResTableCell *c = new ResTableCell(this, Cx, Cy);
			if (!c)
				return false;

			d->Cells.Add(c);
			LDom **Ptr = (LDom**)Value.Value.Ptr;
			if (!Ptr)
				return false;
		
			*Ptr = c;
			// LgiTrace("Create cell %i,%i = %p\n", (int)Cx, (int)Cy, c);
			d->LayoutDirty = true;
			break;
		}
		case ObjStyle:
		{
			const char *s = Value.Str();
			GetCss(true)->Parse(s);
			break;
		}
		default:
		{
			LAssert(!"Invalid property.");
			return false;
		}
	}

	return true;
}

LRect *CtrlTable::GetPasteArea()
{
	for (int i=0; i<d->Cells.Length(); i++)
	{
		if (d->Cells[i]->Selected)
			return &d->Cells[i]->Pos;
	}

	return 0;
}

LRect *CtrlTable::GetChildArea(ResDialogCtrl *Ctrl)
{
	ResTableCell *c = d->GetCell(Ctrl);
	if (c)
		return &c->Pos;
	return NULL;
}

void CtrlTable::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	if (Attaching)
		return;

	ResDialogCtrl *Rc = dynamic_cast<ResDialogCtrl*>(Wnd);
	if (Rc)
	{
		ResTableCell *c = d->GetCell(Rc);
		if (c)
			c->Ctrls.Delete(Rc);
	}
}

void CtrlTable::SetAttachCell(ResTableCell *c)
{
	d->AttachTo = c;
}

bool CtrlTable::AttachCtrl(ResDialogCtrl *Ctrl, LRect *r)
{
	if (d->AttachTo)
	{
		Layout();

		if (!d->AttachTo->Ctrls.HasItem(Ctrl))
		{
			d->AttachTo->Ctrls.Add(Ctrl);
		}

	
		LRect b = d->AttachTo->Pos;
		if (r->X() > b.X())
		{
			r->x1 = b.x1;
			r->x2 = b.x2;
		}
		else if (r->x2 > b.x2)
		{
			r->Offset(b.x2 - r->x2, 0);
		}
		if (r->Y() > b.Y())
		{
			r->y1 = b.y1;
			r->y2 = b.y2;
		}
		else if (r->y2 > b.y2)
		{
			r->Offset(b.y2 - r->y2, 0);
		}

		Ctrl->SetPos(*r);
	}
	else
	{
		for (int i=0; i<d->Cells.Length(); i++)
		{
			ResTableCell *c = d->Cells[i];
			if (c)
			{
				if (c->Pos.Overlap(r->x1, r->y1))
				{
					if (!c->Ctrls.HasItem(Ctrl))
					{
						c->Ctrls.Add(Ctrl);
					}
					Ctrl->SetPos(*r);
					c->MoveCtrls();
					*r = Ctrl->View()->GetPos();
					break;
				}
			}
		}
	}

	return ResDialogCtrl::AttachCtrl(Ctrl, r);
}

void CtrlTable::Layout()
{
	
	d->Layout(GetClient());
}

void CtrlTable::OnPosChange()
{
	Layout();
}

void CtrlTable::OnPaint(LSurface *pDC)
{
	int i;
	Client.Set(0, 0, X()-1, Y()-1);

	if (d->LayoutDirty || d->Handles.Length() == 0)
		d->Layout(GetClient());
	
	pDC->Colour(Blue);
	for (i=0; i<d->Cells.Length(); i++)
	{
		ResTableCell *c = d->Cells[i];
		if (c)
		{
			pDC->Box(&c->Pos);
			if (c->Selected)
			{
				int Op = pDC->Op(GDC_ALPHA);
				if (pDC->Applicator())
				{
					pDC->Colour(Blue);
					LVariant v;
					pDC->Applicator()->SetValue(LDomPropToString(AppAlpha), v = 0x20);
				}
				else
					pDC->Colour(LColour(Blue.r(), Blue.g(), Blue.b(), 0x20));
				pDC->Rectangle(c->Pos.x1 + 1, c->Pos.y1 + 1, c->Pos.x2 - 1, c->Pos.y2 - 1);
				pDC->Op(Op);
				pDC->Colour(Blue);
			}

			#if DRAW_CELL_INDEX
			LSysFont->Colour(Blue, 24);
			LSysFont->Transparent(true);
			char s[256];
			sprintf(s, "%i,%i-%i,%i", c->Cell.x1, c->Cell.y1, c->Cell.X(), c->Cell.Y());
			LSysFont->Text(pDC, c->Pos.x1 + 3, c->Pos.y1 + 1, s);
			#endif

			// LgiTrace("Drawing %i,%i = %p @ %s\n", c->Cell.x1, c->Cell.y1, c, c->Pos.GetStr());
		}
	}

	for (auto &h: d->Handles)
		h.OnPaint(pDC);

	#if DRAW_TABLE_SIZE
	LSysFont->Colour(Blue, 24);
	LSysFont->Transparent(true);
	char s[256];
	sprintf(s, "Cells: %i,%i", d->CellX, d->CellY);
	LSysFont->Text(pDC, 3, 24, s);
	#endif

	ResDialogCtrl::OnPaint(pDC);
}

void CtrlTable::OnMouseMove(LMouse &m)
{
	bool Change = false;
	for (auto &h: d->Handles)
	{
		bool Over = h.Overlap(m.x, m.y);
		if (Over != h.Over)
		{
			h.Over = Over;
			Change = true;
		}
	}
	if (Change)
		Invalidate();

	if (IsCapturing())
	{
		LArray<Pair> p;

		int Fudge = 6;
		if (d->DragRowSize >= 0)
		{
			// Adjust RowSize[d->DragRowSize] and RowSize[d->DragRowSize+1] to
			// center on the mouse y location
			int AvailY = GetClient().Y();
			CalcCell(p, d->RowSize, AvailY);
			
			int BothPx = p[d->DragRowSize].Size + p[d->DragRowSize+1].Size;
			int PxOffset = m.y - p[d->DragRowSize].Pos + Fudge;
			PxOffset = limit(PxOffset, 2, BothPx - 2);

			double Frac = (double) PxOffset / BothPx;
			double Total = d->RowSize[d->DragRowSize] + d->RowSize[d->DragRowSize+1];

			d->RowSize[d->DragRowSize] = Frac * Total;
			d->RowSize[d->DragRowSize+1] = (1.0 - Frac) * Total;

			// LgiTrace("Int: %i/%i, Frac: %f,%f\n", PxOffset, BothPx, d->RowSize[d->DragRowSize], d->RowSize[d->DragRowSize+1]);

			Layout();
			Invalidate();
			return;
		}
		else if (d->DragColSize >= 0)
		{
			// Adjust ColSize[d->DragColSize] and ColSize[d->DragColSize+1] to
			// center on the mouse x location
			int AvailX = GetClient().X();
			CalcCell(p, d->ColSize, AvailX);

			int BothPx = p[d->DragColSize].Size + p[d->DragColSize+1].Size;
			int PxOffset = m.x - p[d->DragColSize].Pos + Fudge;
			PxOffset = limit(PxOffset, 2, BothPx - 2);

			double Frac = (double) PxOffset / BothPx;
			double Total = d->ColSize[d->DragColSize] + d->ColSize[d->DragColSize+1];

			d->ColSize[d->DragColSize] = Frac * Total;
			d->ColSize[d->DragColSize+1] = (1.0 - Frac) * Total;

			Layout();
			Invalidate();
			return;
		}
	}

	ResDialogCtrl::OnMouseMove(m);
}

void CtrlTable::Fix()
{
	// Fix missing cells
	for (int y=0; y<d->CellY; y++)
	{
		for (int x=0; x<d->CellX; )
		{
			ResTableCell *c = d->GetCellAt(x, y);
			// LgiTrace("[%i][%i] = %p (%ix%i, %s)\n", x, y, c, c ? c->Cell.X() : -1, c ? c->Cell.Y() : -1, c ? c->Pos.GetStr() : NULL);
			if (c)
			{
				x += c->Cell.X();
			}
			else
			{
				c = new ResTableCell(this, x, y);
				if (c)
				{
					d->Cells.Add(c);
					x++;
				}
				else break;
			}
		}
	}

	// Fix rows/cols size
	#define absf(i) ((i) < 0.0 ? -(i) : (i))
	double Sum = 0.0;
	int n;
	for (n=0; n<d->RowSize.Length(); n++)
	{
		Sum += d->RowSize[n];
	}
	if (absf(Sum - 1.0) > 0.001)
	{
		for (n=0; n<d->RowSize.Length(); n++)
		{
			double k = d->RowSize[n];
			k /= Sum;
			d->RowSize[n] = k;
		}
	}
	Sum = 0.0;
	for (n=0; n<d->RowSize.Length(); n++)
	{
		Sum += d->RowSize[n];
	}
	if (absf(Sum - 1.0) > 0.001)
	{
		for (n=0; n<d->RowSize.Length(); n++)
		{
			double k = d->RowSize[n];
			k /= Sum;
			d->RowSize[n] = k;
		}
	}

	Layout();
	Invalidate();
}

void CtrlTable::InsertRow(int y)
{
	// Shift existing cells down
	int i;
	for (i=0; i<d->Cells.Length(); i++)
	{
		ResTableCell *c = d->Cells[i];
		if (c)
		{
			if (c->Cell.y1 >= y)
				c->Cell.Offset(0, 1);
			else if (c->Cell.y2 >= y)
				// Make spanned cells taller...
				c->Cell.y2++;				
		}
	}

	// Add new row of 1x1 cells
	for (i=0; i<d->CellX; i++)
	{
		d->Cells.Add(new ResTableCell(this, i, y));
	}
	d->CellY++;

	// Update rows
	double Last = d->RowSize[d->RowSize.Length()-1];
	d->RowSize.Add(Last);
	for (i=0; i<d->RowSize.Length(); i++)
	{
		d->RowSize[i] = d->RowSize[i] / (1.0 + Last);
	}

	// Refresh the screen
	Layout();
	Invalidate();
}

void CtrlTable::InsertCol(int x)
{
	// Shift existing cells down
	int i;
	for (i=0; i<d->Cells.Length(); i++)
	{
		ResTableCell *c = d->Cells[i];
		if (c)
		{
			if (c->Cell.x1 >= x)
				c->Cell.Offset(1, 0);
			else if (c->Cell.x2 >= x)
				// Make spanned cells wider...
				c->Cell.x2++;
		}
	}

	// Add new row of 1x1 cells
	for (i=0; i<d->CellY; i++)
	{
		d->Cells.Add(new ResTableCell(this, x, i));
	}
	d->CellX++;

	// Update rows
	double Last = d->ColSize.Last();
	d->ColSize.Add(Last);
	for (i=0; i<d->ColSize.Length(); i++)
	{
		d->ColSize[i] = d->ColSize[i] / (1.0 + Last);
	}

	// Refresh the screen
	Layout();
	Invalidate();
}

void CtrlTable::UnMerge(ResTableCell *Cell)
{
	if (Cell && (Cell->Cell.X() > 1 || Cell->Cell.Y() > 1))
	{
		for (int y=Cell->Cell.x1; y<=Cell->Cell.y2; y++)
		{
			for (int x=Cell->Cell.x1; x<=Cell->Cell.x2; x++)
			{
				if (x > Cell->Cell.x1 ||
					y > Cell->Cell.y1)
				{
					ResTableCell *n = new ResTableCell(this, x, y);
					if (n)
					{
						d->Cells.Add(n);
					}
				}
			}
		}

		Cell->Cell.x2 = Cell->Cell.x1;
		Cell->Cell.y2 = Cell->Cell.y1;

		Layout();
		Invalidate();
	}
}

void CtrlTable::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		if (m.Left())
		{
			OpHandle *h = NULL;
			for (auto &i: d->Handles)
			{
				if (i.Overlap(m.x, m.y))
				{
					h = &i;
					break;
				}
			}

			if (h && h->Type == LAddCol)
			{
				for (int i=0; i<d->CellY; i++)
					d->Cells.Add(new ResTableCell(this, d->CellX, i));
				d->CellX++;

				double Last = d->ColSize[d->ColSize.Length()-1];
				d->ColSize.Add(Last);
				for (int i=0; i<d->ColSize.Length(); i++)
					d->ColSize[i] = d->ColSize[i] / (1.0 + Last);

				Layout();
				Invalidate();
				return;
			}
			else if (h && h->Type == LAddRow)
			{
				for (int i=0; i<d->CellX; i++)
					d->Cells.Add(new ResTableCell(this, i, d->CellY));
				d->CellY++;

				double Total = 0;
				double Last = d->RowSize[d->RowSize.Length()-1];
				d->RowSize.Add(Last);
				for (int i=0; i<d->RowSize.Length(); i++)
				{
					d->RowSize[i] = d->RowSize[i] / (1.0 + Last);
					Total += d->RowSize[i];
				}

				Layout();
				Invalidate();
				return;
			}
			else
			{
				bool Dirty = false;
				bool EatClick = true;
				int i;
				ResTableCell *Over = 0;

				// Look at cell joins
				if (h && h->Type == LJoinCells)
				{
					// Do a cell merge
					LRect u = h->a->Cell;
					u.Union(&h->b->Cell);

					d->Cells.Delete(h->a);
					for (int y=u.y1; y<=u.y2; y++)
					{
						for (int x=u.x1; x<=u.x2; x++)
						{
							ResTableCell *c = d->GetCellAt(x, y);
							if (c)
							{
								d->Cells.Delete(c);
								DeleteObj(c);
							}
						}
					}
					h->a->Cell = u;
					d->Cells.Add(h->a);
					Dirty = true;
				}

				if (!Dirty)
				{
					// Select a cell?
					for (i=0; i<d->Cells.Length(); i++)
					{
						ResTableCell *c = d->Cells[i];
						if (c->Pos.Overlap(m.x, m.y))
						{
							Over = c;
							if (!c->Selected || m.Ctrl())
							{
								Dirty = true;
								EatClick = false;
								c->Selected = !c->Selected;
							}
						}
						else if (!m.Ctrl())
						{
							if (c->Selected)
							{
								Dirty = true;
								EatClick = false;
								c->Selected = false;
							}
						}
					}
				}

				// Delete column goobers
				if (h && h->Type == LDeleteCol)
					Dirty = d->DeleteCol(h->Index);

				// Delete row goobers
				if (!Dirty && h && h->Type == LDeleteRow)
					Dirty = d->DeleteRow(h->Index);

				// Size row goobs
				if (!Dirty && h && h->Type == LSizeRow)
				{
					Dirty = true;
					d->DragRowSize = h->Index;
					Capture(true);
				}

				// Size col goobs
				if (!Dirty && h && h->Type == LSizeCol)
				{
					Dirty = true;
					d->DragColSize = h->Index;
					Capture(true);
				}

				if (Dirty)
				{
					Layout();
					Invalidate();
					if (EatClick)
						return;
				}
			}
		}
		else if (m.IsContextMenu())
		{
			for (int i=0; i<d->Cells.Length(); i++)
			{
				ResTableCell *c = d->Cells[i];
				if (c->Pos.Overlap(m.x, m.y))
				{
					c->OnMouseClick(m);
					return;
				}
			}
		}
	}
	else
	{
		Capture(false);
		d->DragRowSize = -1;
		d->DragColSize = -1;
	}

	ResDialogCtrl::OnMouseClick(m);
}

////////////////////////////////////////////////////////////////////////////////////////////////
enum {
    M_FINISHED = M_USER + 1000,
};

class TableLayoutTest : public LDialog
{
	LTableLayout *Tbl;
	LView *Msg;
	LTree *Tree;
	class DlgContainer *View;
	LAutoPtr<LThread> Worker;
	LAutoString Base;
	
public:
	TableLayoutTest(LViewI *par);
	~TableLayoutTest();
	
	void OnDialog(LDialogRes *Dlg);
	int OnNotify(LViewI *Ctrl, LNotification n);
    LMessage::Param OnEvent(LMessage *m);
};

class DlgItem : public LTreeItem
{
    TableLayoutTest *Wnd;
	LDialogRes *Dlg;
	
public:
	DlgItem(TableLayoutTest *w, LDialogRes *dlg)
	{
	    Wnd = w;
		Dlg = dlg;
	}
	
	const char *GetText(int i)
	{
		return Dlg->Str->Str;
	}
	
	void OnSelect()
	{
	    Wnd->OnDialog(Dlg);
	}
};

static bool HasTableLayout(LXmlTag *t)
{
	if (t->IsTag("TableLayout"))
		return true;
	for (auto c: t->Children)
	{
		if (HasTableLayout(c))
			return true;
	}
	return false;
}

class Lr8Item : public LTreeItem
{
    TableLayoutTest *Wnd;
	LString File;
	LAutoPtr<LResources> Res;

public:
	Lr8Item(TableLayoutTest *w, LAutoPtr<LResources> res, char *file)
	{
	    Wnd = w;
		Res = res;
		File = file;

		List<LDialogRes>::I d = Res->GetDialogs();
		for (LDialogRes *dlg = *d; dlg; dlg = *++d)
		{
			if (dlg->Str && HasTableLayout(dlg->Dialog))
			{
				Insert(new DlgItem(Wnd, dlg));
			}
		}
		
		if (stristr(File, "Scribe-Branches\\v2.00"))
		{
		    Expanded(true);
		}
	}
	
	const char *GetText(int i)
	{
		return File ? File.Get() : "#error";
	}
};

class Lr8Search : public LThread
{
    TableLayoutTest *Wnd;
	char *Base;
	LTree *Tree;
	
public:
	Lr8Search(TableLayoutTest *w, char *base, LTree *tree) : LThread("Lr8Search")
	{
	    Wnd = w;
		Base = base;
		Tree = tree;
		Run();
	}
	
	~Lr8Search()
	{
		while (!IsExited())
		{
			LSleep(1);
		}
	}
	
	int Main()
	{
		LArray<const char*> Ext;
		LArray<char*> Files;
		Ext.Add("*.lr8");
		if (LRecursiveFileSearch(Base, &Ext, &Files))
		{
			for (int i=0; i<Files.Length(); i++)
			{
            	LAutoPtr<LResources> Res;
    			if (Res.Reset(new LResources(Files[i])))
    			{
				    List<LDialogRes>::I d = Res->GetDialogs();
				    bool HasTl = false;
				    for (LDialogRes *dlg = *d; dlg; dlg = *++d)
				    {
					    if (dlg->Str && HasTableLayout(dlg->Dialog))
					    {
					        HasTl = true;
					        break;
					    }
				    }
				    if (HasTl)
				    {
			            auto r = LMakeRelativePath(Base, Files[i]);
			            Tree->Insert(new Lr8Item(Wnd, Res, r));
				    }
				}
			}
		}
		
		Files.DeleteArrays();
		Wnd->PostEvent(M_FINISHED);
		return 0;
	}
};

class DlgContainer : public LLayout
{
    LDialogRes *Dlg;
    LRect Size;

public:
    DlgContainer(int id)
    {
        Dlg = 0;
        Sunken(true);
        SetId(id);
        
        Size.Set(0, 0, 100, 100);
        SetPos(Size);
    }
    
    void OnPaint(LSurface *pDC)
    {
        pDC->Colour(L_WORKSPACE);
        pDC->Rectangle();
    }
    
    void OnDialog(LDialogRes *d)
    {
        while (Children.Length())
            delete Children[0];
    
        if ((Dlg = d))
        {
            if (Dlg->GetRes()->LoadDialog(Dlg->Str->Id, this, &Size))
            {
                LRect r = GetPos();
                r.SetSize(Size.X(), Size.Y());
                SetPos(r);
                AttachChildren();
                
                SendNotify(LNotifyTableLayoutRefresh);
            }
        }        
    }
};

TableLayoutTest::TableLayoutTest(LViewI *par)
{
	LRect r(0, 0, 1000, 800);
	SetPos(r);
	SetParent(par);
	
	AddView(Tbl = new LTableLayout);

	auto *c = Tbl->GetCell(0, 0, true, 2);
	c->Add(Msg = new LTextLabel(100, 0, 0, -1, -1, "Searching for files..."));

	c = Tbl->GetCell(0, 1);
	c->Add(Tree = new LTree(101, 0, 0, 100, 100));
	c = Tbl->GetCell(1, 1);
	c->Add(View = new DlgContainer(102));

	c = Tbl->GetCell(0, 2, true, 2);
	c->TextAlign(LCss::AlignRight);
	c->Add(new LButton(IDOK, 0, 0, -1, -1, "Close"));

	char e[MAX_PATH_LEN];
	LGetSystemPath(LSP_APP_INSTALL, e, sizeof(e));
	LMakePath(e, sizeof(e), e, "../../..");
	Base.Reset(NewStr(e));
	
	Worker.Reset(new Lr8Search(this, Base, Tree));
}

TableLayoutTest::~TableLayoutTest()
{
	Worker.Reset();
}

void TableLayoutTest::OnDialog(LDialogRes *Dlg)
{
    if (View)
        View->OnDialog(Dlg);
}

int TableLayoutTest::OnNotify(LViewI *Ctrl, LNotification n)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
			EndModal(1);
			break;
	}
	
	return LDialog::OnNotify(Ctrl, n);
}

LMessage::Param TableLayoutTest::OnEvent(LMessage *m)
{
    if (m->Msg() == M_FINISHED)
    {
        Tree->Invalidate();
        Msg->Name("Finished.");
        Worker.Reset();
    }
    return LDialog::OnEvent(m);
}

void OpenTableLayoutTest(LViewI *p)
{
	auto Dlg = new TableLayoutTest(p);
	Dlg->DoModal([](auto dlg, auto id)
	{
		delete dlg;
	});
}
