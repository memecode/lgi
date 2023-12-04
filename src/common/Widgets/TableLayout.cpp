#include "lgi/common/Lgi.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Variant.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Combo.h"
#include "lgi/common/List.h"
#include "lgi/common/Tree.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/Bitmap.h"
#include "lgi/common/TabView.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Css.h"

#undef _FL
#define _FL LGetLeaf(__FILE__), __LINE__

enum CellFlag
{
	// A null value when the call flag is not known yet
	SizeUnknown,
	// The cell contains fixed size objects
	SizeFixed,
	// The cell contains objects that have variable size
	SizeGrow,
	// The cell contains objects that will use all available space
	SizeFill,
};

#define Izza(c)				dynamic_cast<c*>(v)
// #define DEBUG_LAYOUT		723
#define DEBUG_PROFILE		0
#define DEBUG_DRAW_CELLS	0
// #define DEBUG_CTRL_ID		105

#ifdef DEBUG_CTRL_ID
static LString Indent(int Depth)
{
	return LString(" ") * (Depth << 2);
}
#endif

struct LPrintfLogger : public LStream
{
public:
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0)
	{
		if (!Ptr)
			return 0;
	
		#ifdef WINNATIVE
		LgiTrace("%.*s", (int)Size, (const char*)Ptr);
		return Size;
		#else
		return printf("%.*s", (int)Size, (const char*)Ptr);
		#endif	
	}
}	PrintfLogger;

int LTableLayout::CellSpacing = 4;

const char *FlagToString(CellFlag f)
{
	switch (f)
	{
		case SizeUnknown: return "Unknown";
		case SizeFixed:   return "Fixed";
		case SizeGrow:    return "Grow";
		case SizeFill:    return "Fill";
	}
	return "error";
}

template <class T>
T CountRange(LArray<T> &a, ssize_t Start, ssize_t End)
{
	T c = 0;
	for (ssize_t i=Start; i<=End; i++)
	{
		c += a[i];
	}
	return c;
}

struct UnderInfo
{
	int Priority;
	int Col;		// index of column
	int Grow;		// in px
};

static void
DistributeUnusedSpace(	LArray<int> &Min,
						LArray<int> &Max,
						LArray<CellFlag> &Flags,
						int Total,
						int CellSpacing,
						LStream *Debug = NULL)
{
	// Now allocate unused space
	int Borders = (int)Min.Length() - 1;
	int Sum = CountRange<int>(Min, 0, Borders) + (Borders * CellSpacing);
	if (Sum >= Total)
		return;

	int i, Avail = Total - Sum;

	// Do growable ones first
	LArray<UnderInfo> Unders;
	int UnknownGrow = 0;
	for (i=0; i<Min.Length(); i++)
	{
		CellFlag f = Flags[i];
		if (/* f == SizeGrow ||*/ f == SizeFill)
		{
			UnderInfo &u = Unders.New();
			u.Col = i;
			u.Grow = 0;
			u.Priority = f == SizeGrow ? 2 : 3;
			UnknownGrow++;

			/*			
			if (Debug)
				Debug->Print("\t\tAdding[%i] fill, pri=%i\n", i, u.Priority);
			*/
		}
		else if (Max[i] > Min[i])
		{
			UnderInfo &u = Unders.New();
			u.Col = i;
			u.Grow = Max[i] - Min[i];
			if (u.Grow > Avail)
			{
				u.Grow = 0;
				u.Priority = 2;
				UnknownGrow++;
			}
			else
			{
				u.Priority = u.Grow < Avail >> 1 ? 0 : 1;
			}

			/*
			if (Debug)
				Debug->Print("\t\tAdding[%i] grow, pri=%i\n", i, u.Priority);
			*/
		}
	}
	
	Unders.Sort([](auto a, auto b)
	{
		if (a->Priority != b->Priority)
			return a->Priority - b->Priority;
		return b->Grow - a->Grow;
	});

	int UnknownSplit = 0;
	for (i=0; Avail>0 && i<Unders.Length(); i++)
	{
		UnderInfo &u = Unders[i];
		if (u.Grow)
		{
			/*
			if (Debug)
				Debug->Print("\t\tGrow[%i] %i += %i\n", i, Min[u.Col], u.Grow);
			*/

			Min[u.Col] += u.Grow;
			Avail -= u.Grow;
		}
		else
		{
			if (!UnknownSplit)
				UnknownSplit = Avail / UnknownGrow;
			if (!UnknownSplit)
				UnknownSplit = Avail;
			
			/*    
			if (Debug)
				Debug->Print("\t\tFill[%i] %i += %i\n", i, Min[u.Col], UnknownSplit);
			*/

			Min[u.Col] += UnknownSplit;
			Avail -= UnknownSplit;
		}
	}
}

static void
DistributeSize(	LArray<int> &a,
				LArray<CellFlag> &Flags,
				int Start, int Span,
				int Size,
				int Border,
				LStream *Debug = NULL)
{
	// Calculate the current size of the cells
	int Cur = -Border;
	for (int i=0; i<Span; i++)
	{
		Cur += a[Start+i] + Border;
	}

	// Is there more to allocate?
	if (Cur >= Size)
		return;

	// Get list of growable cells
	LArray<int> Grow, Fill, Fixed, Unknown;
	int ExistingGrowPx = 0;
	int ExistingFillPx = 0;
	int UnknownPx = 0;

	for (int i=Start; i<Start+Span; i++)
	{
		switch (Flags[i])
		{
			default:
				break;
			case SizeUnknown:
			{
				Unknown.Add(i);
				UnknownPx += a[i];
				break;
			}
			case SizeFixed:
			{
				Fixed.Add(i);
				break;
			}
			case SizeGrow:
			{
				Grow.Add(i);
				ExistingGrowPx += a[i];
				break;
			}
			case SizeFill:
			{
				Fill.Add(i);
				ExistingFillPx += a[i];
				break;
			}
		}
	}

	int AdditionalSize = Size - Cur;
	if (Fill.Length())
	{
		// Distribute size amongst the growable cells
		int PerFillSize = AdditionalSize / (int)Fill.Length();
		if (PerFillSize <= 0)
			PerFillSize = 1;
			
		for (int i=0; i<Fill.Length(); i++)
		{
			int Cell = Fill[i];
			int Add = i == Fill.Length() - 1 ? AdditionalSize : PerFillSize;
			a[Cell] = a[Cell] + Add;
			AdditionalSize -= Add;
		}
	}
	else if (Grow.Length())
	{
		// Distribute size amongst the growable cells
		int AdditionalSize = Size - Cur;
		for (int i=0; i<Grow.Length(); i++)
		{
			int Cell = Grow[i];
			int Add;
			if (a[Cell] && ExistingGrowPx)
				Add = a[Cell] * AdditionalSize / ExistingGrowPx;
			else
				Add = (int)MAX(1, AdditionalSize / Grow.Length());
			a[Cell] = a[Cell] + Add;
		}
	}
	else if (Fixed.Length() > 0 && Unknown.Length() > 0)
	{
		// Distribute size amongst the unknown cells
		int AdditionalSize = Size - Cur;
		for (int i=0; i<Unknown.Length(); i++)
		{
			int Cell = Unknown[i];
			int Add;
			if (a[Cell] && UnknownPx)
				Add = a[Cell] * AdditionalSize / UnknownPx;
			else
				Add = (int)MAX(1, AdditionalSize / Unknown.Length());
			a[Cell] = a[Cell] + Add;
		}
	}
	else
	{
		// Distribute size amongst the cells
		int Needs = Size - Cur;
		int Part = Needs / Span;
		for (int k=0; k<Span; k++)
		{
			int Add = k < Span - 1 ? Part : Needs;
			a[Start+k] = a[Start+k] + Add;
			Needs -= Add;
		}
	}
}

LCss::LengthType ConvertAlign(char *s, bool x_axis)
{
	if (s)
	{
		if (stricmp(s, "Center") == 0)
			return x_axis ? LCss::AlignCenter : LCss::VerticalMiddle;
		if (stricmp(s, "Max") == 0)
			return x_axis ? LCss::AlignRight : LCss::VerticalBottom;
	}

	return x_axis ? LCss::AlignLeft : LCss::VerticalTop;
}

class TableCell : public LLayoutCell
{
	LStream *TopLevelLog = NULL;

public:
	struct Child
	{
		LViewLayoutInfo Inf;
		LView *View;
		LRect r;
		bool IsLayout;
		bool Debug;
	};

	LTableLayout *Table;
	LRect Cell;		// Cell position
	LRect Pos;		// Pixel position
	LRect Padding;	// Cell padding from CSS styles
	LArray<Child> Children;
	LCss::DisplayType Disp;
	LString ClassName;

	TableCell(LTableLayout *t, int Cx, int Cy);
	LTableLayout *GetTable() override { return Table; }
	bool Add(LView *v) override;
	bool Remove(LView *v) override;
	LArray<LView*> GetChildren() override;
	bool RemoveAll();
	Child *HasView(LView *v);
	LStream &Log();

	bool IsSpanned();
	bool GetVariant(const char *Name, LVariant &Value, const char *Array) override;
	bool SetVariant(const char *Name, LVariant &Value, const char *Array) override;
	int MaxCellWidth();

	/// Calculates the minimum and maximum widths this cell can occupy.
	void LayoutWidth(int Depth, int &MinX, int &MaxX, CellFlag &Flag);
	/// Calculate the height of the cell based on the given width
	void LayoutHeight(int Depth, int Width, int &MinY, int &MaxY, CellFlag &Flags);
	/// Called after the layout has been done to move the controls into place
	void LayoutPost(int Depth);
	
	void OnPaint(LSurface *pDC);
	void OnChange(PropType Prop) override;
};

class LTableLayoutPrivate
{
	friend class TableCell;

	bool InLayout;
	bool DebugLayout;

public:
	LPoint PrevSize, Dpi;
	bool LayoutDirty;
	LArray<double> Rows, Cols;
	LArray<TableCell*> Cells;
	int BorderSpacing;
	LRect LayoutBounds;
	int LayoutMinX, LayoutMaxX;
	LTableLayout *Ctrl;

	LStream &Log() { return PrintfLogger; }
	bool GetDebugLayout() { return DebugLayout; }

	// Object
	LTableLayoutPrivate(LTableLayout *ctrl);
	~LTableLayoutPrivate();
	
	// Utils
	bool IsInLayout() { return InLayout; }
	TableCell *GetCellAt(int cx, int cy);
	void Empty(LRect *Range = NULL);
	bool CollectRadioButtons(LArray<LRadioButton*> &Btns);
	void InitBorderSpacing();
	double Scale()
	{
		if (!Dpi.x)
		{
			auto Wnd = Ctrl->GetWindow();
			if (Wnd)
				Dpi = Wnd->GetDpi();
			else
				Dpi.x = Dpi.y = 96;
		}

		LAssert(Dpi.x > 0);

		return Dpi.x / 96.0;
	}

	// Layout temporary values
	LArray<int> MinCol, MaxCol;
	LArray<int> MinRow, MaxRow;
	LArray<CellFlag> ColFlags, RowFlags;

	// Layout staged methods, call in order to complete the layout
	void LayoutHorizontal(const LRect Client, int Depth, int *MinX = NULL, int *MaxX = NULL, CellFlag *Flag = NULL);
	void LayoutVertical(const LRect Client, int Depth, int *MinY = NULL, int *MaxY = NULL, CellFlag *Flag = NULL);
	void LayoutPost(const LRect Client, int Depth);
	
	// This does the whole layout, basically calling all the stages for you
	void Layout(const LRect Client, int Depth);
};

///////////////////////////////////////////////////////////////////////////////////////////
TableCell::TableCell(LTableLayout *t, int Cx, int Cy)
{
	TextAlign(AlignLeft);
	VerticalAlign(VerticalTop);
	Table = t;
	Cell.ZOff(0, 0);
	Cell.Offset(Cx, Cy);
	Padding.ZOff(0, 0);
	Pos.ZOff(-1, -1);
	Disp = LCss::DispBlock;

	Children.SetFixedLength(true);
}

void TableCell::OnChange(PropType Prop)
{
	if (Prop == PropDisplay)
	{
		bool Vis = Display() != LCss::DispNone;
		for (auto c: Children)
			c.View->Visible(Vis);
	}
}

LStream &TableCell::Log()
{
	/*
	if (!TopLevelLog)
	{
		// Find the top most table layout
		LTableLayout *Top = Table;
		for (LViewI* t = Top; t; t = t->GetParent())
		{
			auto tbl = dynamic_cast<LTableLayout*>(t);
			if (tbl)
				Top = tbl;
			if (t->GetId() == 24)
				break;
		}

		TopLevelLog = &Top->d->Dbg;
	}

	LAssert(TopLevelLog != NULL);
	return *TopLevelLog;
	*/

	return PrintfLogger;
}

TableCell::Child *TableCell::HasView(LView *v)
{
	size_t Len = Children.Length();
	if (!Len)
		return NULL;

	Child *s = &Children[0];
	Child *e = s + Len;
	while (s < e)
	{
		if (s->View == v)
			return s;
		s++;
	}
	return NULL;
}

LArray<LView*> TableCell::GetChildren()
{
	LArray<LView*> a;
	for (auto &c: Children)
		a.Add(c.View);
	return a;
}

bool TableCell::Add(LView *v)
{
	if (!v || HasView(v))
		return false;

	if (Table->IsAttached())
		v->Attach(Table);
	else
		Table->AddView(v);

	Children.SetFixedLength(false);
	Children.New().View = v;
	Children.SetFixedLength(true);

	return true;
}

bool TableCell::Remove(LView *v)
{
	Child *c = HasView(v);
	if (!c)
		return false;
		
	Table->DelView(v);

	if (Children.Length())
	{
		int Idx = (int) (c - &Children.First());
		Children.DeleteAt(Idx, true);
	}
	return true;
}

bool TableCell::RemoveAll()
{
	Child *c = &Children[0];
	for (unsigned i=0; i<Children.Length(); i++)
	{
		DeleteObj(c[i].View);
	}
	return Children.Length(0);
}

bool TableCell::IsSpanned()
{
	return Cell.X() > 1 || Cell.Y() > 1;
}

bool TableCell::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty Fld = LStringToDomProp(Name);
	switch (Fld)
	{
		case ContainerChildren: // Type: LView[]
		{
			if (!Value.SetList())
				return false;

			for (unsigned i=0; i<Children.Length(); i++)
			{
				Child &c = Children[i];
				LVariant *v = new LVariant;
				if (v)
				{
					*v = c.View;
					Value.Value.Lst->Insert(v);
				}
			}
			break;
		}
		case ContainerSpan: // Type: LRect
		{
			Value = Cell.GetStr();
			break;
		}
		case ContainerAlign: // Type: String
		{
			LStringPipe p(128);
			if (TextAlign().ToString(p))
				Value.OwnStr(p.NewStr());
			else
				return false;
			break;
		}
		case ContainerVAlign: // Type: String
		{
			LStringPipe p(128);
			if (VerticalAlign().ToString(p))
				Value.OwnStr(p.NewStr());
			else
				return false;
			break;
		}
		case ObjClass: // Type: String
		{
			Value = ClassName;
			break;
		}
		case ObjStyle: // Type: String
		{
			Value.OwnStr(ToAutoString().Release());
			break;
		}
		case ObjDebug: // Type: Boolean
		{
			Value = Debug;
			break;
		}
		default:
			return false;
	}

	return true;
}

bool TableCell::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty Fld = LStringToDomProp(Name);
	switch (Fld)
	{
		case ContainerChildren: // Type: LView[]
		{
			if (Value.Type != GV_LIST)
			{
				LAssert(!"Should be a list.");
				return false;
			}
			
			// LProfile p("ContainerChildren");
			for (auto v: *Value.Value.Lst)
			{
				if (v->Type != GV_VOID_PTR)
					continue;

				ResObject *o = (ResObject*)v->Value.Ptr;
				if (!o)
					continue;

				LView *gv = dynamic_cast<LView*>(o);
				if (!gv)
					continue;

				// p.Add("c1");
				Children.SetFixedLength(false);
				Children.New().View = gv;
				Children.SetFixedLength(true);

				// p.Add("c2");
				Table->AddView(gv);

				// p.Add("c3");
				gv->SetParent(Table);

				// p.Add("c4");
				LTextLabel *t = dynamic_cast<LTextLabel*>(gv);
				if (t)
					t->SetWrap(true);
			}			
			break;
		}
		case ContainerSpan:
		{
			LRect r;
			if (r.SetStr(Value.Str()))
				Cell = r;
			else
				return false;
			break;
		}
		case ContainerAlign:
		{
			TextAlign
			(
				Len
				(
					ConvertAlign(Value.Str(), true)
				)
			);
			break;
		}
		case ContainerVAlign:
		{
			VerticalAlign
			(
				Len
				(
					ConvertAlign(Value.Str(), false)
				)
			);
			break;
		}
		case ObjClass:
		{
			ClassName = Value.Str();
			
			LResources *r = LgiGetResObj();
			if (r)
			{	
				LCss::SelArray *a = r->CssStore.ClassMap.Find(ClassName);
				if (a)
				{
					for (int i=0; i<a->Length(); i++)
					{
						LCss::Selector *s = (*a)[i];
						
						// This is not exactly a smart matching algorithm.
						if (s && s->Parts.Length() == 1)
						{
							const char *style = s->Style;
							Parse(style, ParseRelaxed);
						}
					}
				}
			}
			break;
		}
		case ObjStyle:
		{
			const char *style = Value.Str();
			if (style)
				Parse(style, ParseRelaxed);
			break;
		}
		case ObjDebug:
		{
			Debug = Value.CastInt32() != 0;
			break;
		}
		default:
			return false;
	}

	return true;
}

int TableCell::MaxCellWidth()
{
	// Table size minus padding
	LCssTools t(Table->GetCss(), Table->GetFont());
	LRect cli = Table->GetClient();
	cli = t.ApplyPadding(cli);
	
	// Work out any borders on spanned cells...
	int BorderPx = Cell.X() > 1 ? (Cell.X() - 1) * Table->d->BorderSpacing : 0;
	
	// Return client - padding - border size.
	return cli.X() - BorderPx;
}

/// Calculates the minimum and maximum widths this cell can occupy.
void TableCell::LayoutWidth(int Depth, int &MinX, int &MaxX, CellFlag &Flag)
{
	int MaxBtnX = 0;
	int TotalBtnX = 0;
	int Min = 0, Max = 0;

	Pos = Pos.ZeroTranslate();

	// Calculate CSS padding
	#define CalcCssPadding(Prop, Axis, Edge) \
	{ \
		Len l = Prop(); \
		if (l.Type == LCss::LenInherit) l = LCss::Padding(); \
		if (l.Type) \
			Padding.Edge = l.ToPx(Table->Axis(), Table->GetFont()); \
		else \
			Padding.Edge = 0; \
	}
	
	Disp = Display();
	if (Disp == DispNone)
		return;
	
	CalcCssPadding(PaddingLeft, X, x1)
	CalcCssPadding(PaddingRight, X, x2)
	CalcCssPadding(PaddingTop, Y, y1)
	CalcCssPadding(PaddingBottom, Y, y2)

	// Cell CSS size:
	auto Wid = Width();
	auto MinWid = MinWidth();
	auto MaxWid = MaxWidth();
	auto Fnt = Table->GetFont();
	int Tx = Table->X();

	if (MinWid.IsValid())
		Min = MAX(Min, MinWid.ToPx(Tx, Fnt));

	if (Wid.IsValid())
	{		
		if (Wid.Type == LCss::LenAuto)
			Flag = SizeFill;
		else
		{
			// If we set Min here too the console app breaks because the percentage 
			// is over-subscribe (95%) to force one cell to grow to fill available space.
			Max = Wid.ToPx(Tx, Fnt) - Padding.x1 - Padding.x2;
			
			if (!Wid.IsDynamic())
			{
				Flag = SizeFixed;
				Min = Max;

				/*	This is breaking normal usage, where 'Min' == 0.
					Need to redesign for edge case.

				if (Padding.x1 + Padding.x2 > Min)
				{
					// Remove padding as it's going to oversize the cell
					Padding.x1 = Padding.x2 = 0;
				}
				*/
			}
			else
			{
				Flag = SizeGrow;
			}
		}
	}

	if (!Wid.IsValid() || Flag != SizeFixed)
	{
		Child *c = Children.AddressOf();
		for (int i=0; i<Children.Length(); i++, c++)
		{
			LView *v = c->View;
			if (!v)
				continue;

			ZeroObj(c->Inf);
			c->r = v->GetPos().ZeroTranslate();
			
			// Child view CSS size:
			LCss *Css = v->GetCss();
			LCss::Len ChildWid;
			// LCss::Len ChildMin, ChildMax;
			if (Css)
			{
				ChildWid = Css->Width();
				// ChildMin = Css->MinWidth();
				// ChildMax = Css->MaxWidth();
			}

			#ifdef DEBUG_CTRL_ID
			if (v->GetId() == DEBUG_CTRL_ID)
				Log().Print("\t\tdbgCtrl=%i pos=%s c->r=%s (%s:%i)\n",
					v->GetId(), Pos.GetStr(), c->r.GetStr(), _FL);
			#endif
				
			if (ChildWid.IsValid())
			{
				int MaxPx = MaxCellWidth();
				c->Inf.Width.Min = c->Inf.Width.Max = ChildWid.ToPx(MaxPx, v->GetFont());
				Min = MAX(Min, c->Inf.Width.Min);
				Max = MAX(Max, c->Inf.Width.Min);
				
				c->r = v->GetPos();
				c->r.x2 = c->r.x1 + c->Inf.Width.Min - 1;
			}
			else if (v->OnLayout(c->Inf))
			{
				if (c->Inf.Width.Max < 0)
				{
					if (Flag != SizeFixed)
						Flag = SizeFill;
				}
				else if (Max)
					Max += c->Inf.Width.Max + LTableLayout::CellSpacing;
				else
					Max = c->Inf.Width.Max;

				if (c->Inf.Width.Min)
				{
					Min = MAX(Min, c->Inf.Width.Min);
					if (c->Inf.Width.Max > c->Inf.Width.Min &&
						Flag == SizeUnknown)
						Flag = SizeGrow;
				}
			}
			else if (Izza(LButton))
			{
				LDisplayString ds(v->GetFont(), v->Name());
				auto Scale = Table->d->Scale();
				LPoint Pad((int)(Scale * LButton::Overhead.x + 0.5), (int)(Scale * LButton::Overhead.y + 0.5));

				c->Inf.Width.Min = c->Inf.Width.Max = ds.X() + Pad.x;
				c->Inf.Height.Min = c->Inf.Height.Max = ds.Y() + Pad.y;

				MaxBtnX = MAX(MaxBtnX, c->Inf.Width.Min);
				TotalBtnX = TotalBtnX ?
							TotalBtnX + LTableLayout::CellSpacing + c->Inf.Width.Min :
							c->Inf.Width.Min;
				
				if (Flag < SizeFixed)
					Flag = SizeFixed;
			}
			else if (Izza(LEdit) ||
					 Izza(LScrollBar))
			{
				Min = MAX(Min, 40);
				if (Flag != SizeFixed)
					Flag = SizeFill;
			}
			else if (Izza(LCombo))
			{
				int PadX = LCombo::Pad.x1 + LCombo::Pad.x2;
				LCombo *Cbo = Izza(LCombo);
				LFont *f = Cbo->GetFont();
				int min_x = -1, max_x = 0;
				char *t;

				for (int i=0; i < Cbo->Length() && (t = (*Cbo)[i]); i++)
				{
					LDisplayString ds(f, t);
					int x = ds.X();
					min_x = min_x < 0 ? x : MIN(min_x, x);
					max_x = MAX(x + 4, max_x);
				}				
				
				Min = MAX(Min, min_x + PadX);
				Max = MAX(Max, max_x + PadX);
				if (Flag < SizeGrow)
					Flag = SizeGrow;
			}
			else if (Izza(LBitmap))
			{
				LBitmap *b = Izza(LBitmap);
				LSurface *Dc = b->GetSurface();
				if (Dc)
				{
					Min = MAX(Min, Dc->X() + 4);
					Max = MAX(Max, Dc->X() + 4);
				}
				else
				{
					Min = MAX(Min, 16);
					Max = MAX(Max, 16);
					if (Flag < SizeFill)
						Flag = SizeFill;
				}
			}
			else if (Izza(LList))
			{
				LList *Lst = Izza(LList);
				int m = 0;
				for (int i=0; i<Lst->GetColumns(); i++)
				{
					m += Lst->ColumnAt(i)->Width();
				}
				m = MAX(m, 40);
				Min = MAX(Min, 40);
				Flag = SizeFill;
			}
			else if (Izza(LTree) ||
					 Izza(LTabView))
			{
				Min = MAX(Min, 40);
				Flag = SizeFill;
			}
			else
			{
				LTableLayout *Tbl = Izza(LTableLayout);
				if (Tbl)
				{
					Tbl->d->InitBorderSpacing();
					Tbl->d->LayoutHorizontal(Table->GetClient(), Depth+1, &Min, &Max, &Flag);
					
					if (Wid.IsDynamic())
					{
						if (Min > Max)
							Min = Max;
					}
				}
				else
				{
					Min = MAX(Min, v->X());
					Max = MAX(Max, v->X());
				}
			}
		}
	}

	if (MaxBtnX)
	{
		Min = MAX(Min, MaxBtnX);
		Max = MAX(Max, TotalBtnX);
	}

	if (MaxWid.IsValid())
	{
		int Tx = Table->X();
		int Px = MaxWid.ToPx(Tx, Table->GetFont()) - Padding.x1 - Padding.x2;
		if (Min > Px)
			Min = Px;
		if (Max > Px)
			Max = Px;
	}

	MinX = MAX(MinX, Min + Padding.x1 + Padding.x2);
	MaxX = MAX(MaxX, Max + Padding.x1 + Padding.x2);

}

/// Calculate the height of the cell based on the given width
void TableCell::LayoutHeight(int Depth, int Width, int &MinY, int &MaxY, CellFlag &Flags)
{
	Pos.ZOff(Width-1, 0);
	if (Disp == DispNone)
		return;
	
	Len Ht = Height();
	if (Ht.Type != LenInherit)
	{
		if (Ht.IsDynamic())
		{
			MaxY = Ht.ToPx(Table->Y(), Table->GetFont());
			if (Flags < SizeGrow)
				Flags = SizeGrow;
		}
		else
		{
			MinY = MaxY = Ht.ToPx(Table->Y(), Table->GetFont());
			Pos.y2 = MinY - 1;
			if (Flags < SizeFixed)
				Flags = SizeFixed;
			return;
		}
	}
	
	LPoint Cur(Pos.x1, Pos.y1);
	int NextY = Pos.y1;
	
	LCss::Len CssWidth = LCss::Width();
	Width -= Padding.x1 + Padding.x2;
	LAssert(Width >= 0);
	
	Child *c = Children.AddressOf();
	for (int i=0; i<Children.Length(); i++, c++)
	{
		LView *v = c->View;
		if (!v)
			continue;

		#ifdef DEBUG_CTRL_ID
		if (v->GetId() == DEBUG_CTRL_ID)
			Log().Print("\t\tdbgCtrl=%i pos=%s c->r=%s (%s:%i)\n",
				v->GetId(), Pos.GetStr(), c->r.GetStr(), _FL);
		#endif

		if (CssWidth.Type != LenInherit)
		{
			// In the case where the CSS width is set, we need to default the
			// OnLayout info with valid widths otherwise the OnLayout calls will
			// not fill out the height, but initialize the width instead.

			// Without this !zero check the Bayesian Filter setting dialog in Scribe
			// has the wrong Help button size. ie if the PreLayout step gave a valid
			// layout size, don't override it here.
			if (!c->Inf.Width.Min)
				c->Inf.Width.Min = Width;
			if (!c->Inf.Width.Max)
				c->Inf.Width.Max = Width;
		}
		
		LTableLayout *Tbl = NULL;

		LCss *Css = v->GetCss();
		LCss::Len Ht;
		if (Css)
			Ht = Css->Height();

		if (!Izza(LButton) && i)
			Pos.y2 += Table->d->BorderSpacing;

		if (c->Inf.Width.Min > Width)
			c->Inf.Width.Min = Width;
		if (c->Inf.Width.Max > Width)
			c->Inf.Width.Max = Width;

		if (Ht.IsValid())
		{
			int CtrlHeight = Ht.ToPx(Table->Y(), v->GetFont());
			c->Inf.Height.Min = c->Inf.Height.Max = CtrlHeight;
			if (MaxY < CtrlHeight)
				MaxY = CtrlHeight;
			if (!Ht.IsDynamic() && MinY < CtrlHeight)
				MinY = CtrlHeight;
			
			c->r = v->GetPos().ZeroTranslate();
			c->r.y2 = c->r.y1 + CtrlHeight - 1;			
			Pos.y2 = MAX(Pos.y2, c->r.Y()-1);
		}
		else if (v->OnLayout(c->Inf))
		{
			// Supports layout info
			c->r = v->GetPos();

			// Process height
			if (c->Inf.Height.Max < 0)
				Flags = SizeFill;
			else
				c->r.y2 = c->r.y1 + c->Inf.Height.Max - 1;
			
			// Process width
			if (c->Inf.Width.Max < 0)
				c->r.x2 = c->r.x1 + Width - 1;
			else
				c->r.x2 = c->r.x1 + c->Inf.Width.Max - 1;

			if (Cur.x > Pos.x1 && Cur.x + c->r.X() > Pos.x2)
			{
				// Wrap
				Cur.x = Pos.x1;
				Cur.y = NextY + LTableLayout::CellSpacing;
			}

			c->r.Offset(Cur.x - c->r.x1, Cur.y - c->r.y1);
			Cur.x = c->r.x2 + 1;
			NextY = MAX(NextY, c->r.y2 + 1);
			Pos.y2 = MAX(Pos.y2, c->r.y2);

			c->IsLayout = true;
		}
		else if (Izza(LScrollBar))
		{
			Pos.y2 += 15;
		}
		else if (Izza(LButton))
		{
			c->r.ZOff(c->Inf.Width.Min-1, c->Inf.Height.Min-1);
			
			if (Cur.x + c->r.X() > Width)
			{
				// Wrap
				Cur.x = Pos.x1;
				Cur.y = NextY + LTableLayout::CellSpacing;
			}
			
			c->r.Offset(Cur.x, Cur.y);
			Cur.x = c->r.x2 + 1;
			NextY = MAX(NextY, c->r.y2 + 1);
			Pos.y2 = MAX(Pos.y2, c->r.y2);
		}
		else if (Izza(LEdit) || Izza(LCombo))
		{
			LFont *f = v->GetFont();
			int y = (f ? f : LSysFont)->GetHeight() + 8;
			
			c->r = v->GetPos();
			c->r.y2 = c->r.y1 + y - 1;
			Pos.y2 += y;

			if (Izza(LEdit) &&
				Izza(LEdit)->MultiLine())
			{
				Flags = SizeFill;
				// MaxY = MAX(MaxY, 1000);
			}
		}
		else if (Izza(LRadioButton))
		{
			int y = v->GetFont()->GetHeight() + 2;
			
			c->r = v->GetPos();
			c->r.y2 = c->r.y1 + y - 1;

			Pos.y2 += y;
		}
		else if (Izza(LList) ||
				 Izza(LTree) ||
				 Izza(LTabView))
		{
			Pos.y2 += v->GetFont()->GetHeight() + 8;
			// MaxY = MAX(MaxY, 1000);
			Flags = SizeFill;
		}
		else if (Izza(LBitmap))
		{
			LBitmap *b = Izza(LBitmap);
			LSurface *Dc = b->GetSurface();
			if (Dc)
			{
				MaxY = MAX(MaxY, Dc->Y() + 4);
			}
			else
			{
				MaxY = MAX(MaxY, 1000);
			}
		}
		else if ((Tbl = Izza(LTableLayout)))
		{
			auto Children = Tbl->IterateViews();
			
			c->r.ZOff(Width-1, Table->Y()-1);

			LCssTools tools(Tbl->GetCss(), Tbl->GetFont());
			auto client = tools.ApplyBorder(c->r);
			Tbl->d->InitBorderSpacing();
			Tbl->d->LayoutHorizontal(client, Depth+1);
			Tbl->d->LayoutVertical(client, Depth+1, &MinY, &MaxY, &Flags);

			Tbl->d->LayoutPost(client, Depth+1);
			Pos.y2 += MinY;
			
			c->Inf.Height.Min = MinY;
			c->Inf.Height.Max = MaxY;
		}
		else
		{
			// Doesn't support layout info
			Pos.y2 += v->Y();
		}

		#ifdef DEBUG_CTRL_ID
		if (v->GetId() == DEBUG_CTRL_ID)
		{
			Log().Print("\t\tdbgCtrl=%i pos=%s c->r=%s (%s:%i)\n",
				v->GetId(), Pos.GetStr(), c->r.GetStr(), _FL);
		}
		#endif
	}
	
	// Fix: This if statement is needed to stop LFileSelect dialogs only growing in size, and
	// the Ok/Cancel shifting off the bottom of the dialog if you shrink the window.
	if (Flags != SizeFill)
	{
		MinY = MAX(MinY, Pos.Y() + Padding.y1 + Padding.y2);
		MaxY = MAX(MaxY, Pos.Y() + Padding.y1 + Padding.y2);
	}
}

/// Called after the layout has been done to move the controls into place
void TableCell::LayoutPost(int Depth)
{
	int Cx = Padding.x1;
	int Cy = Padding.y1;
	int MaxY = Padding.y1;
	int RowStart = 0;
	LArray<LRect> New;
	int WidthPx = Pos.X() - Padding.x1 - Padding.x2;
	int HeightPx = Pos.Y() - Padding.y1 - Padding.y2;

	#ifdef DEBUG_CTRL_ID
	bool HasDebugCtrl = false;
	#endif

	Child *c = Children.AddressOf();
	for (int i=0; i<Children.Length(); i++, c++)
	{
		LView *v = c->View;
		if (!v)
			continue;
			
		if (Disp == DispNone)
		{
			v->Visible(false);
			continue;
		}
		
		#ifdef DEBUG_CTRL_ID
		if (v->GetId() == DEBUG_CTRL_ID)
		{
			HasDebugCtrl = true;
			Log().Print("\t\tdbgCtrl=%i c->r=%s padding=%s cur=%i,%i (%s:%i)\n",
				v->GetId(), c->r.GetStr(), Padding.GetStr(), Cx, Cy, _FL);
		}
		#endif

		LTableLayout *Tbl = Izza(LTableLayout);
		
		if (i > 0 && Cx + c->r.X() > Pos.X())
		{
			// Do wrapping
			int Wid = Cx - Table->d->BorderSpacing;
			int OffsetX = 0;
			if (TextAlign().Type == AlignCenter)
			{
				OffsetX = (Pos.X() - Wid) / 2;
			}
			else if (TextAlign().Type == AlignRight)
			{
				OffsetX = Pos.X() - Wid;
			}

			for (int n=RowStart; n<=i; n++)
			{
				New[n].Offset(OffsetX, 0);
			}

			RowStart = i + 1;
			Cx = Padding.x1;
			Cy = MaxY + Table->d->BorderSpacing;
		}

		#ifdef DEBUG_CTRL_ID
		if (v->GetId() == DEBUG_CTRL_ID)
		{
			Log().Print("\t\tdbgCtrl=%i offset %i %i %i, %i %i %i %s:%i\n",
				v->GetId(), 
				Pos.x1, c->r.x1, Cx,
				Pos.y1, c->r.y1, Cy,
				_FL);
		}
		#endif

		c->r.Offset(Pos.x1 - c->r.x1 + Cx, Pos.y1 - c->r.y1 + Cy);

		if (c->Inf.Width.Max >= WidthPx)
			c->Inf.Width.Max = WidthPx;
		if (c->Inf.Height.Max >= HeightPx)
			c->Inf.Height.Max = HeightPx;

		if (c->r.Y() > HeightPx)
		{
			c->r.y2 = c->r.y1 + HeightPx - 1;
		}

		if (Tbl)
		{
			c->r.SetSize(Pos.X(), MIN(Pos.Y(), c->Inf.Height.Min));
		}
		else if
		(
			Izza(LList) ||
			Izza(LTree) ||
			Izza(LTabView) ||
			(Izza(LEdit) && Izza(LEdit)->MultiLine())
		)
		{
			c->r.y2 = Pos.y2;

			if (c->Inf.Width.Max <= 0 ||
				c->Inf.Width.Max >= WidthPx)
				c->r.x2 = c->r.x1 + WidthPx - 1;
			else if (c->Inf.Width.Max)
				c->r.x2 = c->r.x1 + c->Inf.Width.Max - 1;
		}
		else if (c->IsLayout)
		{
			if (c->Inf.Height.Max < 0)
				c->r.y2 = Pos.y2;
			else
				c->r.y2 = c->r.y1 + c->Inf.Height.Max - 1;
		}
		else
		{
			if (c->Inf.Width.Max <= 0 ||
				c->Inf.Width.Max >= WidthPx)
				c->r.x2 = c->r.x1 + WidthPx - 1;
			else if (c->Inf.Width.Max)
				c->r.x2 = c->r.x1 + c->Inf.Width.Max - 1;
		}

		New[i] = c->r;

		#ifdef DEBUG_CTRL_ID
		if (v->GetId() == DEBUG_CTRL_ID)
		{
			HasDebugCtrl = true;
			Log().Print("\t\tdbgCtrl=%i c->r=%s (%s:%i)\n",
				v->GetId(), c->r.GetStr(), _FL);
		}
		#endif

		MaxY = MAX(MaxY, c->r.y2 - Pos.y1);
		Cx += c->r.X() + Table->d->BorderSpacing;
	}

	if (Disp == DispNone)
	{
		return;
	}
	
	int n;
	int Wid = Cx - Table->d->BorderSpacing;
	int OffsetX = 0;
	if (TextAlign().Type == AlignCenter)
	{
		OffsetX = (Pos.X() - Wid) / 2;
	}
	else if (TextAlign().Type == AlignRight)
	{
		OffsetX = Pos.X() - Wid;
	}
	if (OffsetX)
	{
		for (n=RowStart; n<Children.Length(); n++)
		{
			New[n].Offset(OffsetX, 0);
		}
	}

	int OffsetY = 0;
	LCss::Len VAlign = VerticalAlign();

	#ifdef DEBUG_CTRL_ID
	if (HasDebugCtrl)
		Log().Print("\t\tdbgCtrl=%i VAlign.Type=%i (%s:%i)\n",
			DEBUG_CTRL_ID, VAlign.Type, _FL);
	#endif

	if (VAlign.Type == VerticalMiddle)
	{
		int Py = Pos.Y();
		OffsetY = (Py - MaxY) / 2;
		#ifdef DEBUG_CTRL_ID
		if (HasDebugCtrl)
			Log().Print("\t\tdbgCtrl=%i Py=%i MaxY=%i OffsetY=%i\n",
				DEBUG_CTRL_ID, Py, MaxY, OffsetY);
		#endif
	}
	else if (VAlign.Type == VerticalBottom)
	{
		int Py = Pos.Y();
		OffsetY = Py - MaxY;
	}

	#if DEBUG_LAYOUT
	if (Table->d->DebugLayout)
	{
		Log().Print("\tCell[%i,%i]=%s (%ix%i)\n", Cell.x1, Cell.y1, Pos.GetStr(), Pos.X(), Pos.Y());
	}
	#endif
	for (n=0; n<Children.Length(); n++)
	{
		auto &Child = Children[n];
		auto v = Child.View;
		if (!v)
			break;

		New[n].Offset(0, OffsetY);

		#ifdef DEBUG_CTRL_ID
		if (v->GetId() == DEBUG_CTRL_ID)
		{
			Log().Print("\t\tdbgCtrl=%i %s[%i]=%s, %ix%i, Offy=%i %s (%s:%i)\n",
				v->GetId(), 
				v->GetClass(), n,
				New[n].GetStr(),
				New[n].X(), New[n].Y(),
				OffsetY,
				v->Name(),
				_FL);
		}
		#endif

		v->SetPos(New[n]);	
	}
}

void TableCell::OnPaint(LSurface *pDC)
{
	LCssTools t(this, Table->GetFont());
	LRect r = Pos;	
	t.PaintBorder(pDC, r);

	LColour Trans;
	auto bk = t.GetBack(&Trans);
	if (bk.IsValid())
	{
		pDC->Colour(bk);
		pDC->Rectangle(&r);
	}
}

////////////////////////////////////////////////////////////////////////////
LTableLayoutPrivate::LTableLayoutPrivate(LTableLayout *ctrl)
{
	PrevSize.Set(-1, -1);
	LayoutDirty = true;
	InLayout = false;
	DebugLayout = false;
	Ctrl = ctrl;
	BorderSpacing = LTableLayout::CellSpacing;
	LayoutBounds.ZOff(-1, -1);
	LayoutMinX = LayoutMaxX = 0;
}

LTableLayoutPrivate::~LTableLayoutPrivate()
{
	Empty();
}

bool LTableLayoutPrivate::CollectRadioButtons(LArray<LRadioButton*> &Btns)
{
	for (LViewI *i: Ctrl->IterateViews())
	{
		LRadioButton *b = dynamic_cast<LRadioButton*>(i);
		if (b) Btns.Add(b);
	}
	return Btns.Length() > 0;
}

void LTableLayoutPrivate::Empty(LRect *Range)
{
	if (Range)
	{
		// Clear a range of cells..
		for (int i=0; i<Cells.Length(); i++)
		{
			TableCell *c = Cells[i];
			if (Range->Overlap(&c->Cell))
			{
				c->RemoveAll();
				Cells.DeleteAt(i--, true);
				DeleteObj(c);
			}
		}
	}
	else
	{
		// Clear all the cells
		Ctrl->IterateViews().DeleteObjects();
		Cells.DeleteObjects();
		Rows.Length(0);
		Cols.Length(0);
	}

	PrevSize.Set(-1, -1);
	LayoutBounds.ZOff(-1, -1);
	LayoutMinX = LayoutMaxX = 0;
}

TableCell *LTableLayoutPrivate::GetCellAt(int cx, int cy)
{
	for (int i=0; i<Cells.Length(); i++)
		if (Cells[i]->Cell.Overlap(cx, cy))
			return Cells[i];

	return NULL;
}

void LTableLayoutPrivate::LayoutHorizontal(const LRect Client, int Depth, int *MinX, int *MaxX, CellFlag *Flag)
{
	// This only gets called when you nest LTableLayout controls. It's 
	// responsible for doing pre layout stuff for an entire control of cells.
	int Cx, Cy, i;

	LString::Array Ps;
	Ps.SetFixedLength(false);
	LAutoPtr<LProfile> Prof(/*Debug ? new LProfile("Layout") :*/ NULL);

	// Zero everything to start with
	MinCol.Length(0);
	MaxCol.Length(0);
	MinRow.Length(0);
	MaxRow.Length(0);
	ColFlags.Length(0);
	RowFlags.Length(0);

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		int asd=0;
	}
	#endif
		
	// Do pre-layout to determine minimum and maximum column widths
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Non-spanned cells
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.X() == 1)
				{
					if (Prof)
					{
						LString &s = Ps.New();
						s.Printf("pre layout %i,%i", c->Cell.x1, c->Cell.y1);
						Prof->Add(s);
					}

					int &MinC = MinCol[Cx];
					int &MaxC = MaxCol[Cx];
					CellFlag &ColF = ColFlags[Cx];
					c->LayoutWidth(Depth, MinC, MaxC, ColF);
				}

				Cx += c->Cell.X();
			}
			else
				Cx++;
		}
	}

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		Log().Print("\tLayout Id=%i, Size=%i,%i (%s:%i)\n", Ctrl->GetId(), Client.X(), Client.Y(), _FL);
		for (i=0; i<Cols.Length(); i++)
		{
			Log().Print(	"\tLayoutHorizontal.AfterSingle[%i]: min=%i max=%i (%s)\n",
						i,
						MinCol[i], MaxCol[i],
						FlagToString(ColFlags[i]));
		}
	}
	#endif

	if (Prof) Prof->Add("Pre layout spanned");
	// Pre-layout column width for spanned cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.X() > 1)
				{
					int Min = 0, Max = 0;
					CellFlag Flag = SizeUnknown;
					
					if (Prof)
					{
						LString &s = Ps.New();
						s.Printf("spanned %i,%i", c->Cell.x1, c->Cell.y1);
						Prof->Add(s);
					}

					if (c->Width().IsValid())
					{
						LCss::Len l = c->Width();
						if (l.Type == LCss::LenAuto)
						{
							for (int i=c->Cell.x1; i<=c->Cell.x2; i++)
							{
								ColFlags[i] = SizeFill;
							}
						}
						else
						{
							int Px = l.ToPx(Client.X(), Ctrl->GetFont());;
							if (l.IsDynamic())
							{
								c->LayoutWidth(Depth, Min, Max, Flag);
							}
							else
							{
								Min = Max = Px;
							}
						}
					}
					else
					{
						c->LayoutWidth(Depth, Min, Max, Flag);
					}

					// Log().Print("Spanned cell: %i,%i\n", Min, Max);

					if (Max > Client.X())
						Max = Client.X();
					if (Flag)
					{
						bool HasFlag = false;
						bool HasUnknown = false;
						for (i=c->Cell.x1; i<=c->Cell.x2; i++)
						{
							if (ColFlags[i] == Flag)
							{
								HasFlag = true;
							}
							else if (ColFlags[i] == SizeUnknown)
							{
								HasUnknown = true;
							}
						}
						
						if (!HasFlag)
						{
							for (i=c->Cell.x1; i<=c->Cell.x2; i++)
							{
								if (HasUnknown)
								{
									if (ColFlags[i] == SizeUnknown)
										ColFlags[i] = Flag;
								}
								else
								{
									if (ColFlags[i] < Flag)
										ColFlags[i] = Flag;
								}
							}
						}
					}
					
					// This is the total size of all the px currently allocated
					int AllPx = CountRange(MinCol, 0, Cols.Length()-1) + (((int)Cols.Length() - 1) * BorderSpacing);
					
					// This is the minimum size of this cell's cols
					int MyPx = CountRange(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1) * BorderSpacing);
					
					int Remaining = Client.X() - AllPx;
					
					// Log().Print("AllPx=%i MyPx=%i, Remaining=%i\n", AllPx, MyPx, Remaining);
			
					// This is the total remaining px we could add...
					if (Remaining > 0)
					{
						// Limit the max size of this cell to the existing + remaining px
						Max = MIN(Max, MyPx + Remaining);
						
						// Distribute the max px across the cell's columns.
						DistributeSize(MinCol, ColFlags, c->Cell.x1, c->Cell.X(), Min, BorderSpacing);
						DistributeSize(MaxCol, ColFlags, c->Cell.x1, c->Cell.X(), Max, BorderSpacing);
					}
				}

				Cx += c->Cell.X();
			}
			else Cx++;
		}
	}

	LayoutMinX = -BorderSpacing;
	LayoutMaxX = -BorderSpacing;
	for (i=0; i<Cols.Length(); i++)
	{
		LayoutMinX += MinCol[i] + BorderSpacing;
		LayoutMaxX += MaxCol[i] + BorderSpacing;
	}

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Cols.Length(); i++)
		{
			Log().Print("\tLayoutHorizontal.AfterSpanned[%i]: min=%i max=%i (%s)\n",
						i,
						MinCol[i], MaxCol[i],
						FlagToString(ColFlags[i]));
		}
	}
	#endif

	// Now allocate unused space
	if (Prof) Prof->Add("DistributeUnusedSpace");
	DistributeUnusedSpace(MinCol, MaxCol, ColFlags, Client.X(), BorderSpacing, DebugLayout?&Log():NULL);

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Cols.Length(); i++)
		{
			Log().Print("\tLayoutHorizontal.AfterDist[%i]: min=%i max=%i (%s)\n",
						i,
						MinCol[i], MaxCol[i],
						FlagToString(ColFlags[i]));
		}
	}
	#endif
	
	// Collect together our sizes
	if (Prof) Prof->Add("Collect together our sizes");
	int Spacing = BorderSpacing * ((int)MinCol.Length() - 1);
	auto Css = Ctrl->GetCss();
	if (MinX)
	{
		int x = CountRange<int>(MinCol, 0, MinCol.Length()-1) + Spacing;
		*MinX = MAX(*MinX, x);

		if (Css)
		{
			auto MinWid = Css->MinWidth();
			if (MinWid.IsValid())
			{
				int px = MinWid.ToPx(Ctrl->X(), Ctrl->GetFont());
				*MinX = MAX(*MinX, px);
			}
		}
	}
	if (MaxX)
	{
		int x = CountRange<int>(MaxCol, 0, MinCol.Length()-1) + Spacing;
		*MaxX = MAX(*MaxX, x);

		if (Css)
		{
			auto MaxWid = Css->MaxWidth();
			if (MaxWid.IsValid())
			{
				int px = MaxWid.ToPx(Ctrl->X(), Ctrl->GetFont());
				*MaxX = MIN(*MaxX, px);
			}
		}
	}

	if (Flag)
	{	
		for (i=0; i<ColFlags.Length(); i++)
		{
			*Flag = MAX(*Flag, ColFlags[i]);
		}
	}

	Prof.Reset();
}

void LTableLayoutPrivate::LayoutVertical(const LRect Client, int Depth, int *MinY, int *MaxY, CellFlag *Flag)
{
	int Cx, Cy, i;

	// Do row height layout for single cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Single cell
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.Y() == 1)
				{
					int x = CountRange(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1) * BorderSpacing);
					int &Min = MinRow[Cy];
					int &Max = MaxRow[Cy];
					CellFlag &Flags = RowFlags[Cy];
					c->LayoutHeight(Depth, x, Min, Max, Flags);
				}

				Cx += c->Cell.X();
			}
			else
				Cx++;
		}
	}

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Rows.Length(); i++)
			Log().Print("\tLayoutVertical.AfterSingle[%i]: min=%i max=%i (%s)\n", i, MinRow[i], MaxRow[i], FlagToString(RowFlags[i]));
	}
	#endif

	// Row height for spanned cells
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				// Spanned cell
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy &&
					c->Cell.Y() > 1)
				{
					int WidthPx      = CountRange(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1)   * BorderSpacing);					
					int InitMinY     = CountRange(MinRow, c->Cell.y1, c->Cell.y2) + ((c->Cell.Y() - 1)   * BorderSpacing);
					int InitMaxY     = CountRange(MaxRow, c->Cell.y1, c->Cell.y2) + ((c->Cell.Y() - 1)   * BorderSpacing);
					//int AllocY       = CountRange(MinRow, 0,     Rows.Length()-1) + ((Rows.Length() - 1) * BorderSpacing);
					int MinY         = InitMinY;
					int MaxY         = InitMaxY;
					// int RemainingY   = Client.Y() - AllocY;
					CellFlag RowFlag = SizeUnknown;

					c->LayoutHeight(Depth, WidthPx, MinY, MaxY, RowFlag);

					// This code stops the max being set on spanned cells.
					LArray<int> AddTo;
					for (int y=c->Cell.y1; y<=c->Cell.y2; y++)
					{
						if
						(
							RowFlags[y] != SizeFixed
							&&
							(
								RowFlags[y] != SizeGrow
								||
								MaxRow[y] > MinRow[y]
							)
						)
							AddTo.Add(y);
					}
					if (AddTo.Length() == 0)
					{
						for (int y=c->Cell.y1; y<=c->Cell.y2; y++)
						{
							if (!AddTo.HasItem(y))
							{
								if (RowFlags[y] != SizeFixed)
									AddTo.Add(y);
							}
						}
					}

					if (AddTo.Length())
					{
						if (MinY > InitMinY)
						{
							// Allocate any extra min px somewhere..
							int Amt = (MinY - InitMinY) / (int)AddTo.Length();
							for (int i=0; i<AddTo.Length(); i++)
							{
								int Idx = AddTo[i];
								MinRow[Idx] += Amt;
							}
						}
						
						if (MaxY > InitMaxY)
						{
							// Allocate any extra max px somewhere..
							int Amt = (MaxY - InitMaxY) / (int)AddTo.Length();
							for (int i=0; i<AddTo.Length(); i++)
							{
								int Idx = AddTo[i];
								MaxRow[Idx] += Amt;
							}
						}						
						
						if (RowFlag > SizeUnknown)
						{
							// Apply the size flag somewhere...
							for (int y=c->Cell.y2; y>=c->Cell.y1; y++)
							{
								if (RowFlags[y] == SizeUnknown)
								{
									RowFlags[y] = SizeFill;
									break;
								}
							}
						}
					}
					else
					{
						// Last chance... stuff extra px in last cell...
						MaxRow[c->Cell.y2] = MAX(MaxY, MaxRow[c->Cell.y2]);
					}
				}

				Cx += c->Cell.X();
			}
			else Cx++;
		}
	}

	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Rows.Length(); i++)
			Log().Print("\tLayoutVertical.AfterSpanned[%i]: min=%i max=%i (%s)\n", i, MinRow[i], MaxRow[i], FlagToString(RowFlags[i]));
	}
	#endif

	// Allocate remaining vertical space
	if (Depth == 0)
		DistributeUnusedSpace(MinRow, MaxRow, RowFlags, Client.Y(), BorderSpacing);
	
	#if DEBUG_LAYOUT
	if (DebugLayout)
	{
		for (i=0; i<Rows.Length(); i++)
		{
			Log().Print("\tLayoutVertical.AfterDist[%i]: min=%i max=%i (%s)\n", i, MinRow[i], MaxRow[i], FlagToString(RowFlags[i]));
		}
		for (i=0; i<Cols.Length(); i++)
		{
			Log().Print("\tLayoutHorizontal.Final[%i]=%i\n", i, MinCol[i]);
		}
	}
	#endif

	// Collect together our sizes
	auto Css = Ctrl->GetCss();
	if (MinY)
	{
		int y = CountRange(MinRow, 0, MinRow.Length()-1) + (((int)MinRow.Length()-1) * BorderSpacing);
		*MinY = MAX(*MinY, y);

		if (Css)
		{
			auto MinHt = Css->MinHeight();
			if (MinHt.IsValid())
			{
				auto px = MinHt.ToPx(Ctrl->Y(), Ctrl->GetFont());
				*MinY = MAX(*MinY, px);
			}
		}
	}
	if (MaxY)
	{
		int y = CountRange(MaxRow, 0, MinRow.Length()-1) + (((int)MaxRow.Length()-1) * BorderSpacing);
		*MaxY = MAX(*MaxY, y);

		if (Css)
		{
			auto MaxHt = Css->MaxHeight();
			if (MaxHt.IsValid())
			{
				auto px = MaxHt.ToPx(Ctrl->Y(), Ctrl->GetFont());
				*MaxY = MIN(*MaxY, px);
			}
		}
	}
	if (Flag)
	{	
		for (i=0; i<RowFlags.Length(); i++)
		{
			*Flag = MAX(*Flag, RowFlags[i]);
		}
	}
}

void LTableLayoutPrivate::LayoutPost(const LRect Client, int Depth)
{
	int Px = 0, Py = 0, Cx, Cy;
	LFont *Fnt = Ctrl->GetFont();

	// Move cells into their final positions
	#if DEBUG_LAYOUT
	if (DebugLayout && Cols.Length() == 7)
		Log().Print("\tLayoutPost %ix%i\n", Cols.Length(), Rows.Length());
	#endif
	for (Cy=0; Cy<Rows.Length(); Cy++)
	{
		int MaxY = 0;
		Px = 0;
		for (Cx=0; Cx<Cols.Length(); )
		{
			TableCell *c = GetCellAt(Cx, Cy);
			if (c)
			{
				if (c->Cell.x1 == Cx &&
					c->Cell.y1 == Cy)
				{
					LCss::PositionType PosType = c->Position();
					int x = CountRange<int>(MinCol, c->Cell.x1, c->Cell.x2) + ((c->Cell.X() - 1) * BorderSpacing);
					int y = CountRange<int>(MinRow, c->Cell.y1, c->Cell.y2) + ((c->Cell.Y() - 1) * BorderSpacing);

					// Set the height of the cell
					c->Pos.x2 = c->Pos.x1 + x - 1;
					c->Pos.y2 = c->Pos.y1 + y - 1;
					
					if (PosType == LCss::PosAbsolute)
					{
						// Hmm this is a bit of a hack... we'll see
						LCss::Len Left = c->Left();
						LCss::Len Top = c->Top();
						
						int LeftPx = Left.IsValid() ? Left.ToPx(Client.X(), Fnt) : Px;
						int TopPx = Top.IsValid() ? Top.ToPx(Client.Y(), Fnt) : Py;
						
						c->Pos.Offset(Client.x1 + LeftPx, Client.y1 + TopPx);
					}
					else
					{
						c->Pos.Offset(Client.x1 + Px, Client.y1 + Py);
						#if 0//def DEBUG_CTRL_ID
						Log().Print("\t\tdbgCtrl=%i Client=%s pos=%s p=%i,%i (%s:%i)\n",
							DEBUG_CTRL_ID, Client.GetStr(), c->Pos.GetStr(), Px, Py, _FL);
						#endif
					}
					c->LayoutPost(Depth);

					MaxY = MAX(MaxY, c->Pos.y2);

					#if DEBUG_LAYOUT
					if (DebugLayout)
					{
						c->Log().Print("\tCell[%i][%i]: %s %s\n", Cx, Cy, c->Pos.GetStr(), Client.GetStr());
					}
					#endif
				}

				Px = c->Pos.x2 + BorderSpacing - Client.x1 + 1;
				Cx += c->Cell.X();
			}
			else
			{
				Px = MinCol[Cx] + BorderSpacing;
				Cx++;
			}
		}

		Py += MinRow[Cy] + BorderSpacing;
	}

	LayoutBounds.ZOff(Px-1, Py-1);
	#if DEBUG_LAYOUT
	if (DebugLayout)
		Log().Print("\tLayoutBounds: %s\n", LayoutBounds.GetStr());
	#endif
}

void LTableLayoutPrivate::InitBorderSpacing()
{
	BorderSpacing = LTableLayout::CellSpacing;
	if (Ctrl->GetCss())
	{
		LCss::Len bs = Ctrl->GetCss()->BorderSpacing();
		if (bs.Type != LCss::LenInherit)
			BorderSpacing = bs.ToPx(Ctrl->X(), Ctrl->GetFont());
	}
}

void LTableLayoutPrivate::Layout(const LRect Client, int Depth)
{
	if (InLayout)
	{    
		LAssert(!"In layout, no recursion should happen.");
		return;
	}

	InLayout = true;
	InitBorderSpacing();
	
	#if DEBUG_LAYOUT
	int CtrlId = Ctrl->GetId();
	// auto CtrlChildren = Ctrl->IterateViews();
	DebugLayout = CtrlId == DEBUG_LAYOUT && Ctrl->IterateViews().Length() > 0;
	if (DebugLayout)
	{
		int asd=0;
	}
	#endif

	#if DEBUG_PROFILE
	int64 Start = LCurrentTime();
	#endif

	LString s;
	s.Printf("Layout id %i: %i x %i", Ctrl->GetId(), Client.X(), Client.Y());
	LAutoPtr<LProfile> Prof(/*Debug ? new LProfile(s) :*/ NULL);

	#if DEBUG_LAYOUT
	if (DebugLayout)
		Log().Print("%s\n", s.Get());
	#endif
	if (Prof) Prof->Add("Horz");

	LayoutHorizontal(Client, Depth);

	if (Prof) Prof->Add("Vert");

	LayoutVertical(Client, Depth);

	if (Prof) Prof->Add("Post");

	LayoutPost(Client, Depth);

	if (Prof) Prof->Add("Notify");

	#if DEBUG_PROFILE
	Log().Print("LTableLayout::Layout(%i) = %i ms\n", Ctrl->GetId(), (int)(LCurrentTime()-Start));
	#endif

	InLayout = false;

	Ctrl->SendNotify(LNotifyTableLayoutChanged);
}

LTableLayout::LTableLayout(int id) : ResObject(Res_Table)
{
	d = new LTableLayoutPrivate(this);
	SetPourLargest(true);
	Name("LTableLayout");
	SetId(id);
}

LTableLayout::~LTableLayout()
{
	DeleteObj(d);
}

void LTableLayout::OnFocus(bool b)
{
	if (b)
	{
		LViewI *v = GetNextTabStop(this, false);
		if (v)
			v->Focus(true);
	}
}

void LTableLayout::OnCreate()
{
	LResources::StyleElement(this);
	AttachChildren();
}

int LTableLayout::CellX()
{
	return (int)d->Cols.Length();
}

int LTableLayout::CellY()
{
	return (int)d->Rows.Length();
}

LLayoutCell *LTableLayout::CellAt(int x, int y)
{
	return d->GetCellAt(x, y);
}

bool LTableLayout::SizeChanged()
{
	LRect r = GetClient();
	return	r.X() != d->PrevSize.x ||
			r.Y() != d->PrevSize.y;
}

void LTableLayout::OnPosChange()
{
	LRect r = GetClient();
	bool Up = SizeChanged() || d->LayoutDirty;	
	
	#ifdef DEBUG_LAYOUT
	// if (GetId() == DEBUG_LAYOUT) LgiTrace("%s:%i - Up=%i for Id=%i\n", _FL, Up, GetId());
	#endif

	if (Up)
	{
		d->PrevSize.x = r.X();
		d->PrevSize.y = r.Y();

		if (d->PrevSize.x > 0)
		{		
			if (GetCss())
			{
				LCssTools t(GetCss(), GetFont());
				r = t.ApplyBorder(r);
				r = t.ApplyPadding(r);
			}
			
			d->LayoutDirty = false;
			d->Layout(r, 0);
		}
	}
}

LRect LTableLayout::GetUsedArea()
{
	if (SizeChanged())
	{
		OnPosChange();
	}

	LRect r(0, 0, -1, -1);
	for (int i=0; i<d->Cells.Length(); i++)
	{
		TableCell *c = d->Cells[i];
		if (i)
			r.Union(&c->Pos);
		else
			r = c->Pos;
	}
	
	return r;
}

void LTableLayout::InvalidateLayout()
{
	if (!d->LayoutDirty)
	{
		d->LayoutDirty = true;

		for (auto p = GetParent(); p; p = p->GetParent())
		{
			LTableLayout *t = dynamic_cast<LTableLayout*>(p);
			if (t)
				t->InvalidateLayout();
		}

		if (IsAttached())
			PostEvent(M_TABLE_LAYOUT);
	}

	Invalidate();
}

LMessage::Result LTableLayout::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case M_TABLE_LAYOUT:
		{
			OnPosChange();
			Invalidate();
			return 0;
		}
	}
	
	return LLayout::OnEvent(m);
}

void LTableLayout::OnPaint(LSurface *pDC)
{
	if (SizeChanged() || d->LayoutDirty)
	{
		if (GetId() == 20)
			Log().Print("20 : clearing layout dirty LGI_VIEW_HANDLE=%i\n", LGI_VIEW_HANDLE);

		#if LGI_VIEW_HANDLE
		if (!Handle())
		#endif
			OnPosChange();
		#if LGI_VIEW_HANDLE
		else
			if (PostEvent(M_TABLE_LAYOUT))
				return;
			else
				LAssert(!"Post event failed.");
		#endif

		if (GetId() == 20)
			Log().Print("20 : painting\n");
	}

	d->Dpi = GetWindow()->GetDpi();
	LCssTools Tools(this);
	LRect Client = GetClient();

	Client = Tools.PaintBorder(pDC, Client);
	Tools.PaintContent(pDC, Client);

	for (int i=0; i<d->Cells.Length(); i++)
	{
		TableCell *c = d->Cells[i];
		c->OnPaint(pDC);
	}

	#if 0 // DEBUG_DRAW_CELLS
	pDC->Colour(LColour(255, 0, 0));
	pDC->Box();
	#endif

	#if DEBUG_DRAW_CELLS
	#if defined(DEBUG_LAYOUT)
	if (GetId() == DEBUG_LAYOUT)
	#endif
	{
		pDC->LineStyle(LSurface::LineDot);
		for (int i=0; i<d->Cells.Length(); i++)
		{
			TableCell *c = d->Cells[i];
			LRect r = c->Pos;
			pDC->Colour(c->Debug ? Rgb24(255, 222, 0) : Rgb24(192, 192, 222), 24);
			pDC->Box(&r);
			pDC->Line(r.x1, r.y1, r.x2, r.y2);
			pDC->Line(r.x2, r.y1, r.x1, r.y2);
		}
		pDC->LineStyle(LSurface::LineSolid);
	}
	#endif
}

bool LTableLayout::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	return false;
}

bool ConvertNumbers(LArray<double> &a, char *s)
{
	for (auto &i: LString(s).SplitDelimit(","))
		a.Add(i.Float());
	return a.Length() > 0;
}

bool LTableLayout::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case TableLayoutCols:
			return ConvertNumbers(d->Cols, Value.Str());
		case TableLayoutRows:
			return ConvertNumbers(d->Rows, Value.Str());
		case ObjStyle:
		{
			const char *Defs = Value.Str();
			if (Defs)
				GetCss(true)->Parse(Defs, LCss::ParseRelaxed);
			break;
		}
		case TableLayoutCell:
		{
			auto Coords = LString(Array).SplitDelimit(",");
			if (Coords.Length() != 2)
				return false;

			auto Cx = Coords[0].Int();
			auto Cy = Coords[1].Int();
			TableCell *c = new TableCell(this, (int)Cx, (int)Cy);
			if (!c)
				return false;

			d->Cells.Add(c);
			
			if (Value.Type == GV_VOID_PTR)
			{
				LDom **d = (LDom**)Value.Value.Ptr;
				if (d)
					*d = c;
			}
			break;
		}
		default:
		{
			LAssert(!"Unsupported property.");
			return false;
		}
	}

	return true;
}

void LTableLayout::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	InvalidateLayout();
	if (Attaching)
		return;

	for (int i=0; i<d->Cells.Length(); i++)
	{
		TableCell *c = d->Cells[i];
		for (int n=0; n<c->Children.Length(); n++)
		{
			if (c->Children[n].View == Wnd)
			{
				c->Children.DeleteAt(n);
				return;
			}
		}
	}
}

int LTableLayout::OnNotify(LViewI *c, LNotification n)
{
	if (n.Type == LNotifyTableLayoutRefresh)
	{
		bool hasTableParent = false;
		for (LViewI *p = this; p; p = p->GetParent())
		{
			auto tbl = dynamic_cast<LTableLayout*>(p);
			if (tbl)
			{
				hasTableParent = true;
				tbl->d->LayoutDirty = true;
				tbl->Invalidate();
				break;
			}
		}

		if (hasTableParent)
		{
			// One of the parent controls is a table layout, which when it receives this
			// notification will lay this control out too... so don't do it twice.
			// LgiTrace("%s:%i - Ignoring LNotifyTableLayoutRefresh because hasTableParent.\n", _FL);
		}

		SendNotify(LNotifyTableLayoutRefresh);
		return 0;
	}

	return LLayout::OnNotify(c, n);
}

int64 LTableLayout::Value()
{
	LArray<LRadioButton*> Btns;
	if (d->CollectRadioButtons(Btns))
	{
		for (int i=0; i<Btns.Length(); i++)
		{
			if (Btns[i]->Value())
				return i;
		}
	}
	return -1;
}

void LTableLayout::Value(int64 v)
{
	LArray<LRadioButton*> Btns;
	if (d->CollectRadioButtons(Btns))
	{
		for (int i=0; i<Btns.Length(); i++)
			Btns[i]->Value(i == v);
	}
}

void LTableLayout::Empty(LRect *Range)
{
	d->Empty(Range);
}

LLayoutCell *LTableLayout::GetCell(int cx, int cy, bool create, int colspan, int rowspan)
{
	TableCell *c = d->GetCellAt(cx, cy);
	if (!c && create)
	{
		c = new TableCell(this, cx, cy);
		if (c)
		{
			d->LayoutDirty = true;
			if (colspan > 1)
				c->Cell.x2 += colspan - 1;
			if (rowspan > 1)
				c->Cell.y2 += rowspan - 1;
			d->Cells.Add(c);
			
			while (d->Cols.Length() <= c->Cell.x2)
				d->Cols.Add(1);
			while (d->Rows.Length() <= c->Cell.y2)
				d->Rows.Add(1);
		}
	}
	
	return c;
}

LStream &LTableLayout::Log()
{
	return PrintfLogger;
}
