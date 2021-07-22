#include "lgi/common/Lgi.h"
#include "lgi/common/ItemContainer.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Edit.h"
#include "lgi/common/CssTools.h"

// Colours
#if defined(__GTK_H__)
	#define DOUBLE_BUFFER_COLUMN_DRAWING	1
#else
	#define DOUBLE_BUFFER_COLUMN_DRAWING	0
#endif

#if defined(WIN32)
	#if !defined(WS_EX_LAYERED)
		#define WS_EX_LAYERED				0x80000
	#endif
	#if !defined(LWA_ALPHA)
		#define LWA_ALPHA					2
	#endif
	typedef BOOL (__stdcall *_SetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
#endif

class LItemColumnPrivate
{
public:
	LRect Pos;
	bool Down;
	bool Drag;

	LItemContainer *Parent;
	char *cName;
	LDisplayString *Txt;
	int cWidth;
	int cType;
	LSurface *cIcon;
	int cImage;
	int cMark;
	bool OwnIcon;
	bool CanResize;

	LItemColumnPrivate(LItemContainer *parent)
	{
		Parent = parent;
		Txt = 0;
		cName = 0;
		cWidth = 0;
		cIcon = 0;
		cType = GIC_ASK_TEXT;
		cImage = -1;
		cMark = GLI_MARK_NONE;
		Down = false;
		OwnIcon = false;
		CanResize = true;
		Drag = false;
	}

	~LItemColumnPrivate()
	{
		DeleteArray(cName);
		if (OwnIcon)
		{
			DeleteObj(cIcon);
		}
		DeleteObj(Txt);
	}
};

static LColour cActiveCol(0x86, 0xba, 0xe9);
static void FillStops(LArray<GColourStop> &Stops, LRect &r, bool Active)
{
	if (Active)
	{
		Stops[0].Set(0.0f, LColour(0xd0, 0xe2, 0xf5));
		Stops[1].Set(2.0f / (r.Y() - 2), LColour(0x98, 0xc1, 0xe9));
		Stops[2].Set(0.5f, LColour(0x86, 0xba, 0xe9));
		Stops[3].Set(0.51f, LColour(0x68, 0xaf, 0xea));
		Stops[4].Set(1.0f, LColour(0xbb, 0xfc, 0xff));
	}
	else
	{
		LColour cMed(L_MED), cWs(L_WORKSPACE);
		if (cWs.GetGray() < 96)
		{
			cMed = cMed.Mix(LColour::White, 0.25f);
			cWs = cWs.Mix(LColour::White, 0.25f);
		}
		Stops[0].Set(0.0f, cWs);
		Stops[1].Set(0.5f, cMed.Mix(cWs));
		Stops[2].Set(0.51f, cMed);
		Stops[3].Set(1.0f, cWs);
	}
}


//////////////////////////////////////////////////////////////////////////////////////
LItemContainer::LItemContainer()
{
	Flags = 0;
	DragMode = 0;
	DragCol = NULL;
	ColClick = -1;
	ColumnHeaders = true;
	ColumnHeader.ZOff(-1, -1);
	Columns.SetFixedLength(true);
	ItemEdit = NULL;
}

LItemContainer::~LItemContainer()
{
	DeleteObj(ItemEdit);
	DeleteObj(DragCol);
	Columns.DeleteObjects();
}

void LItemContainer::PaintColumnHeadings(LSurface *pDC)
{
	// Draw column headings
	if (!ColumnHeaders || !ColumnHeader.Valid())
		return;

	LSurface *ColDC = pDC;
	LRect cr;

	#if DOUBLE_BUFFER_COLUMN_DRAWING
	LMemDC Bmp;
	if (!pDC->SupportsAlphaCompositing() &&
		Bmp.Create(ColumnHeader.X(), ColumnHeader.Y(), System32BitColourSpace))
	{
		ColDC = &Bmp;
		Bmp.Op(GDC_ALPHA);
		cr = ColumnHeader.ZeroTranslate();
	}
	else
	#endif
	{
		cr = ColumnHeader;
		pDC->ClipRgn(&cr);
	}

	// Draw columns
	int cx = cr.x1;
	if (IconCol)
	{
		cr.x1 = cx;
		cr.x2 = cr.x1 + IconCol->Width() - 1;
		IconCol->SetPos(cr);
		IconCol->OnPaint(ColDC, cr);
		cx += IconCol->Width();
	}

	// Draw other columns
	for (int i=0; i<Columns.Length(); i++)
	{
		LItemColumn *c = Columns[i];
		if (c)
		{
			cr.x1 = cx;
			cr.x2 = cr.x1 + c->Width() - 1;
			c->SetPos(cr);
			c->OnPaint(ColDC, cr);
			cx += c->Width();
		}
		else LAssert(0);
	}

	// Draw ending piece
	cr.x1 = cx;
	cr.x2 = ColumnHeader.x2 + 2;

	if (cr.Valid())
	{
		// Draw end section where there are no columns
		#ifdef MAC
			LArray<GColourStop> Stops;
			LRect j(cr.x1, cr.y1, cr.x2-1, cr.y2-1);
		
			FillStops(Stops, j, false);
			LgiFillGradient(ColDC, j, true, Stops);
		
			ColDC->Colour(L_LOW);
			ColDC->Line(cr.x1, cr.y2, cr.x2, cr.y2);
		#else
			if (LApp::SkinEngine)
			{
				LSkinState State;
				State.pScreen = ColDC;
				State.Rect = cr;
				State.Enabled = Enabled();
				State.View = this;
				LApp::SkinEngine->OnPaint_ListColumn(0, 0, &State);
			}
			else
			{
				LWideBorder(ColDC, cr, DefaultRaisedEdge);
				ColDC->Colour(LColour(L_MED));
				ColDC->Rectangle(&cr);
			}
		#endif
	}

	#if DOUBLE_BUFFER_COLUMN_DRAWING
	if (!pDC->SupportsAlphaCompositing())
		pDC->Blt(ColumnHeader.x1, ColumnHeader.y1, &Bmp);
	else
	#endif
		pDC->ClipRgn(0);
}

LItemColumn *LItemContainer::AddColumn(const char *Name, int Width, int Where)
{
	LItemColumn *c = 0;

	if (Lock(_FL))
	{
		c = new LItemColumn(this, Name, Width);
		if (c)
		{
			Columns.SetFixedLength(false);
			Columns.AddAt(Where, c);
			Columns.SetFixedLength(true);

			UpdateAllItems();
			SendNotify(GNotifyItem_ColumnsChanged);
		}
		Unlock();
	}

	return c;
}

bool LItemContainer::AddColumn(LItemColumn *Col, int Where)
{
	bool Status = false;

	if (Col && Lock(_FL))
	{
		Columns.SetFixedLength(false);
		Status = Columns.AddAt(Where, Col);
		Columns.SetFixedLength(true);

		if (Status)
		{
			UpdateAllItems();
			SendNotify(GNotifyItem_ColumnsChanged);
		}

		Unlock();
	}

	return Status;
}

void LItemContainer::DragColumn(int Index)
{
	DeleteObj(DragCol);

	if (Index >= 0)
	{
		DragCol = new LDragColumn(this, Index);
		if (DragCol)
		{
			Capture(true);
			DragMode = DRAG_COLUMN;
		}
	}
}

int LItemContainer::ColumnAtX(int x, LItemColumn **Col, int *Offset)
{
	LItemColumn *Column = 0;
	if (!Col) Col = &Column;

	int Cx = GetImageList() ? 16 : 0;
	int c;
	for (c=0; c<Columns.Length(); c++)
	{
		*Col = Columns[c];
		if (x >= Cx && x < Cx + (*Col)->Width())
		{
			if (Offset)
				*Offset = Cx;
			return c;
		}
		Cx += (*Col)->Width();
	}

	return -1;
}

void LItemContainer::EmptyColumns()
{
	Columns.DeleteObjects();
	Invalidate(&ColumnHeader);
	SendNotify(GNotifyItem_ColumnsChanged);
}

int LItemContainer::HitColumn(int x, int y, LItemColumn *&Resize, LItemColumn *&Over)
{
	int Index = -1;

	Resize = 0;
	Over = 0;

	if (ColumnHeaders &&
		ColumnHeader.Overlap(x, y))
	{
		// Clicked on a column heading
		int cx = ColumnHeader.x1 + ((IconCol) ? IconCol->Width() : 0);
		
		for (int n = 0; n < Columns.Length(); n++)
		{
			LItemColumn *c = Columns[n];
			cx += c->Width();
			if (abs(x-cx) < 5)
			{
				if (c->Resizable())
				{
					Resize = c;
					Index = n;
					break;
				}
			}
			else if (c->d->Pos.Overlap(x, y))
			{
				Over = c;
				Index = n;
				break;
			}
		}
	}

	return Index;
}

DeclGArrayCompare(ColInfoCmp, LItemContainer::ColInfo, void)
{
	int AGrowPx = a->GrowPx();
	int BGrowPx = b->GrowPx();
	return AGrowPx - BGrowPx;
}

void LItemContainer::OnColumnClick(int Col, LMouse &m)
{
	ColClick = Col;
	ColMouse = m;
	SendNotify(GNotifyItem_ColumnClicked);
}

bool LItemContainer::GetColumnClickInfo(int &Col, LMouse &m)
{
	if (ColClick >= 0)
	{
		Col = ColClick;
		m = ColMouse;
		return true;
	}
	
	return false;	
}

void LItemContainer::GetColumnSizes(ColSizes &cs)
{
	// Read in the current sizes
	cs.FixedPx = 0;
	cs.ResizePx = 0;
	for (int i=0; i<Columns.Length(); i++)
	{
		LItemColumn *c = Columns[i];
		if (c->Resizable())
		{
			ColInfo &Inf = cs.Info.New();
			Inf.Col = c;
			Inf.Idx = i;
			Inf.ContentPx = c->GetContentSize();
			Inf.WidthPx = c->Width();

			cs.ResizePx += Inf.ContentPx;
		}
		else
		{
			cs.FixedPx += c->Width();
		}
	}
}

GMessage::Result LItemContainer::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_RESIZE_TO_CONTENT:
		{
			ResizeColumnsToContent((int)Msg->A());
			break;
		}
		default:
			break;
	}

	return LLayout::OnEvent(Msg);
}

void LItemContainer::ResizeColumnsToContent(int Border)
{
	if (!InThread())
	{
		PostEvent(M_RESIZE_TO_CONTENT, Border);
		return;
	}
	
	if (Lock(_FL))
	{
		// Read in the current sizes
		ColSizes Sizes;
		GetColumnSizes(Sizes);
		
		// Allocate space
		int AvailablePx = GetClient().X() - 5;
		if (VScroll)
			AvailablePx -= VScroll->X();

		int ExpandPx = AvailablePx - Sizes.FixedPx;
		Sizes.Info.Sort(ColInfoCmp, (void*)NULL);
		
		for (int i=0; i<Sizes.Info.Length(); i++)
		{
			ColInfo &Inf = Sizes.Info[i];
			if (Inf.Col && Inf.Col->Resizable())
			{
				if (ExpandPx > Sizes.ResizePx)
				{
					// Everything fits...
					Inf.Col->Width(Inf.ContentPx + Border);
				}
				else
				{
					int Cx = GetClient().X();
					double Ratio = Cx ? (double)Inf.ContentPx / Cx : 1.0;
					if (Ratio < 0.25)
					{
						Inf.Col->Width(Inf.ContentPx + Border);
					}
					else
					{					
						// Need to scale to fit...
						int Px = Inf.ContentPx * ExpandPx / Sizes.ResizePx;
						Inf.Col->Width(Px + Border);
					}
				}

				ClearDs(Inf.Idx);
			}
		}

		Unlock();
	}

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////////
LDragColumn::LDragColumn(LItemContainer *list, int col)
{
	List = list;
	Index = col;
	Offset = 0;
	#ifdef LINUX
	Back = 0;
	#endif
	Col = List->ColumnAt(Index);
	if (Col)
	{
		Col->d->Down = false;
		Col->d->Drag = true;

		LRect r = Col->d->Pos;
		r.y1 = 0;
		r.y2 = List->Y()-1;
		List->Invalidate(&r, true);

		#if WINNATIVE
		
		LArray<int> Ver;
		bool Layered = (
							LGetOs(&Ver) == LGI_OS_WIN32 ||
							LGetOs(&Ver) == LGI_OS_WIN64
						) &&
						Ver[0] >= 5;
		SetStyle(WS_POPUP);
		SetExStyle(GetExStyle() | WS_EX_TOOLWINDOW);
		if (Layered)
		{
			SetExStyle(GetExStyle() | WS_EX_LAYERED | WS_EX_TRANSPARENT);
		}
		
		#endif

		Attach(0);

		#if WINNATIVE
		
		if (Layered)
		{
			SetWindowLong(Handle(), GWL_EXSTYLE, GetWindowLong(Handle(), GWL_EXSTYLE) | WS_EX_LAYERED);

			GLibrary User32("User32");
			_SetLayeredWindowAttributes SetLayeredWindowAttributes = (_SetLayeredWindowAttributes)User32.GetAddress("SetLayeredWindowAttributes");
			if (SetLayeredWindowAttributes)
			{
				if (!SetLayeredWindowAttributes(Handle(), 0, DRAG_COL_ALPHA, LWA_ALPHA))
				{
					DWORD Err = GetLastError();
				}
			}
		}
		
		#elif defined(__GTK_H__)

		Gtk::GtkWindow *w = WindowHandle();
		if (w)
		{
			gtk_window_set_decorated(w, FALSE);
			gtk_widget_set_opacity(GtkCast(w, gtk_widget, GtkWidget), DRAG_COL_ALPHA / 255.0);
		}
		
		#endif

		LMouse m;
		List->GetMouse(m);
		Offset = m.x - r.x1;

		List->PointToScreen(ListScrPos);
		r.Offset(ListScrPos.x, ListScrPos.y);

		SetPos(r);
		Visible(true);
	}
}

LDragColumn::~LDragColumn()
{
	Visible(false);

	if (Col)
	{
		Col->d->Drag = false;
	}

	List->Invalidate();
}

#if LINUX_TRANS_COL
void LDragColumn::OnPosChange()
{
	Invalidate();
}
#endif

void LDragColumn::OnPaint(LSurface *pScreen)
{
	#if LINUX_TRANS_COL
	LSurface *Buf = new LMemDC(X(), Y(), GdcD->GetBits());
	LSurface *pDC = new LMemDC(X(), Y(), GdcD->GetBits());
	#else
	LSurface *pDC = pScreen;
	#endif
	
	pDC->SetOrigin(Col->d->Pos.x1, 0);
	if (Col) Col->d->Drag = false;
	List->OnPaint(pDC);
	if (Col) Col->d->Drag = true;
	pDC->SetOrigin(0, 0);
	
	#if LINUX_TRANS_COL
	if (Buf && pDC)
	{
		LRect p = GetPos();
		
		// Fill the buffer with the background
		Buf->Blt(ListScrPos.x - p.x1, 0, Back);

		// Draw painted column over the back with alpha
		Buf->Op(GDC_ALPHA);
		LApplicator *App = Buf->Applicator();
		if (App)
		{
			App->SetVar(GAPP_ALPHA_A, DRAG_COL_ALPHA);
		}
		Buf->Blt(0, 0, pDC);

		// Put result on the screen
		pScreen->Blt(0, 0, Buf);
	}
	DeleteObj(Buf);
	DeleteObj(pDC);
	#endif
}

///////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// List column
LItemColumn::LItemColumn(LItemContainer *parent, const char *name, int width)
	: ResObject(Res_Column)
{
	d = new LItemColumnPrivate(parent);
	d->cWidth = width;
	if (name)
		Name(name);
}

LItemColumn::~LItemColumn()
{
	if (d->Drag)
	{
		d->Parent->DragColumn(-1);
	}

	DeleteObj(d);
}

LItemContainer *LItemColumn::GetList()
{
	return d->Parent;
}

void LItemColumn::Image(int i)
{
	d->cImage = i;
}

int LItemColumn::Image()
{
	return d->cImage;
}

bool LItemColumn::Resizable()
{
	return d->CanResize;
}

void LItemColumn::Resizable(bool i)
{
	d->CanResize = i;
}

bool LItemColumn::InDrag()
{
	return d->Drag;
}

LRect LItemColumn::GetPos()
{
	return d->Pos;
}

void LItemColumn::SetPos(LRect &r)
{
	d->Pos = r;
}

void LItemColumn::Name(const char *n)
{
	DeleteArray(d->cName);
	DeleteObj(d->Txt);
	d->cName = NewStr(n);
	
	LFont *f =	d->Parent &&
				d->Parent->GetFont() &&
				d->Parent->GetFont()->Handle() ?
				d->Parent->GetFont() :
				LSysFont;
	
	d->Txt = new LDisplayString(f, (char*)n);
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

char *LItemColumn::Name()
{
	return d->cName;
}

int LItemColumn::GetIndex()
{
	if (d->Parent)
	{
		return (int)d->Parent->Columns.IndexOf(this);
	}

	return -1;
}

int LItemColumn::GetContentSize()
{
	return d->Parent->GetContentSize(GetIndex());
}

void LItemColumn::Width(int i)
{
	if (d->cWidth != i)
	{
		d->cWidth = i;
		
		// If we are attached to a list...
		if (d->Parent)
		{
			/* FIXME
			 int MyIndex = GetIndex();
			// Clear all the cached strings for this column
			for (List<LListItem>::I it=d->Parent->Items.Start(); it.In(); it++)
			{
				DeleteObj((*it)->d->Display[MyIndex]);
			}

			if (d->Parent->IsAttached())
			{
				// Update the screen from this column across
				LRect Up = d->Parent->GetClient();
				Up.x1 = d->Pos.x1;
				d->Parent->Invalidate(&Up);
			}
			*/
		}

		// Notify listener
		auto p = d->Parent;
		if (p)
			p->SendNotify(GNotifyItem_ColumnsResized);
	}
}

int LItemColumn::Width()
{
	return d->cWidth;
}

void LItemColumn::Mark(int i)
{
	d->cMark = i;
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

int LItemColumn::Mark()
{
	return d->cMark;
}

void LItemColumn::Type(int i)
{
	d->cType = i;
	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

int LItemColumn::Type()
{
	return d->cType;
}

void LItemColumn::Icon(LSurface *i, bool Own)
{
	if (d->OwnIcon)
	{
		DeleteObj(d->cIcon);
	}
	d->cIcon = i;
	d->OwnIcon = Own;

	if (d->Parent)
	{
		d->Parent->Invalidate(&d->Parent->ColumnHeader);
	}
}

LSurface *LItemColumn::Icon()
{
	return d->cIcon;
}

bool LItemColumn::Value()
{
	return d->Down;
}

void LItemColumn::Value(bool i)
{
	d->Down = i;
}

void LItemColumn::OnPaint_Content(LSurface *pDC, LRect &r, bool FillBackground)
{
	if (!d->Drag)
	{
		LCssTools Tools(d->Parent);
		auto Fore = Tools.GetFore();
		auto cMed = LColour(L_MED);
		int Off = d->Down ? 1 : 0;
		int Mx = r.x1 + 8, My = r.y1 + ((r.Y() - 8) / 2);
		if (d->cIcon)
		{
			if (FillBackground)
			{
				pDC->Colour(cMed);
				pDC->Rectangle(&r);
			}

			int x = (r.X()-d->cIcon->X()) / 2;
			
			pDC->Blt(	r.x1 + x + Off,
						r.y1 + ((r.Y()-d->cIcon->Y())/2) + Off,
						d->cIcon);

			if (d->cMark)
			{
				Mx += x + d->cIcon->X() + 4;
			}
		}
		else if (d->cImage >= 0 && d->Parent)
		{
			LColour Background = cMed;
			if (FillBackground)
			{
				pDC->Colour(Background);
				pDC->Rectangle(&r);
			}
			
			if (d->Parent->GetImageList())
			{
				LRect *b = d->Parent->GetImageList()->GetBounds();
				int x = r.x1;
				int y = r.y1;
				if (b)
				{
					b += d->cImage;
					x = r.x1 + ((r.X()-b->X()) / 2) - b->x1;
					y = r.y1 + ((r.Y()-b->Y()) / 2) - b->y1;
				}				
				
				d->Parent->GetImageList()->Draw(pDC,
												x + Off,
												y + Off,
												d->cImage,
												Background);
			}

			if (d->cMark)
			{
				Mx += d->Parent->GetImageList()->TileX() + 4;
			}
		}
		else if (ValidStr(d->cName) && d->Txt)
		{
			LFont *f = d->Txt->GetFont();
			if (!f)
			{
				LAssert(0);
				return;
			}

			LColour cText = Fore;
			#ifdef MAC
			// Contrast check
			if (d->cMark && (cText - cActiveCol) < 64)
				cText = cText.Invert();
			#endif

			f->Transparent(!FillBackground);
			f->Colour(cText, cMed);
			int ty = d->Txt->Y();
			int ry = r.Y();
			int y = r.y1 + ((ry - ty) >> 1);
			d->Txt->Draw(pDC, r.x1 + Off + 3, y + Off, &r);

			if (d->cMark)
			{
				Mx += d->Txt->X();
			}
		}
		else
		{
			if (FillBackground)
			{
				pDC->Colour(cMed);
				pDC->Rectangle(&r);
			}
		}

		#define ARROW_SIZE	9
		pDC->Colour(Fore);
		Mx += Off;
		My += Off - 1;

		switch (d->cMark)
		{
			case GLI_MARK_UP_ARROW:
			{
				pDC->Line(Mx + 2, My, Mx + 2, My + ARROW_SIZE - 1);
				pDC->Line(Mx, My + 2, Mx + 2, My);
				pDC->Line(Mx + 2, My, Mx + 4, My + 2);
				break;
			}
			case GLI_MARK_DOWN_ARROW:
			{
				pDC->Line(Mx + 2, My, Mx + 2, My + ARROW_SIZE - 1);
				pDC->Line(	Mx,
							My + ARROW_SIZE - 3,
							Mx + 2,
							My + ARROW_SIZE - 1);
				pDC->Line(	Mx + 2,
							My + ARROW_SIZE - 1,
							Mx + 4,
							My + ARROW_SIZE - 3);
				break;
			}
		}
	}
}

void ColumnPaint(void *UserData, LSurface *pDC, LRect &r, bool FillBackground)
{
	((LItemColumn*)UserData)->OnPaint_Content(pDC, r, FillBackground);
}

void LItemColumn::OnPaint(LSurface *pDC, LRect &Rgn)
{
	LRect r = Rgn;

	if (d->Drag)
	{
		pDC->Colour(DragColumnColour);
		pDC->Rectangle(&r);
	}
	else
	{
		#ifdef MAC

			LArray<GColourStop> Stops;
			LRect j(r.x1, r.y1, r.x2-1, r.y2-1);
			FillStops(Stops, j, d->cMark != 0);
			LgiFillGradient(pDC, j, true, Stops);
			if (d->cMark)
				pDC->Colour(Rgb24(0x66, 0x93, 0xc0), 24);
			else
				pDC->Colour(Rgb24(178, 178, 178), 24);
			pDC->Line(r.x1, r.y2, r.x2, r.y2);
			pDC->Line(r.x2, r.y1, r.x2, r.y2);

			LRect n = r;
			n.Size(2, 2);
			OnPaint_Content(pDC, n, false);

		#else
		
			if (LApp::SkinEngine)
			{
				LSkinState State;
				
				State.pScreen = pDC;
				State.ptrText = &d->Txt;
				State.Rect = Rgn;
				State.Value = Value();
				State.Enabled = GetList()->Enabled();
				State.View = d->Parent;

				LApp::SkinEngine->OnPaint_ListColumn(ColumnPaint, this, &State);
			}
			else
			{
				if (d->Down)
				{
					LThinBorder(pDC, r, DefaultSunkenEdge);
					LFlatBorder(pDC, r, 1);
				}
				else
				{
					LWideBorder(pDC, r, DefaultRaisedEdge);
				}
				
				OnPaint_Content(pDC, r, true);
			}
		
		#endif	
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
GItem::GItem()
{
	SelectionStart = SelectionEnd = -1;
}
	
GItem::~GItem()
{
}

LView *GItem::EditLabel(int Col)
{
	LItemContainer *c = GetContainer();
	if (!c)
		return NULL;
	
	c->Capture(false);
	if (!c->ItemEdit)
	{
		c->ItemEdit = new GItemEdit(c, this, Col, SelectionStart, SelectionEnd);
		SelectionStart = SelectionEnd = -1;
	}

	return c->ItemEdit;
}

void GItem::OnEditLabelEnd()
{
	LItemContainer *c = GetContainer();
	if (c)
		c->ItemEdit = NULL;
}

void GItem::SetEditLabelSelection(int SelStart, int SelEnd)
{
	SelectionStart = SelStart;
	SelectionEnd = SelEnd;
}

////////////////////////////////////////////////////////////////////////////////////////////
#define M_END_POPUP			(M_USER+0x1500)
#define M_LOSING_FOCUS		(M_USER+0x1501)

class GItemEditBox : public LEdit
{
	GItemEdit *ItemEdit;

public:
	GItemEditBox(GItemEdit *i, int x, int y, const char *s) : LEdit(100, 1, 1, x-3, y-3, s)
	{
		ItemEdit = i;
		Sunken(false);
		MultiLine(false);
		
		#ifndef LINUX
		SetPos(GetPos());
		#endif
	}
	
	const char *GetClass() { return "GItemEditBox"; }

	void OnCreate()
	{
		LEdit::OnCreate();
		Focus(true);
	}

	void OnFocus(bool f)
	{
		if (!f && GetParent())
		{
			#if DEBUG_EDIT_LABEL
			LgiTrace("%s:%i - GItemEditBox posting M_LOSING_FOCUS\n", _FL);
			#endif
			GetParent()->PostEvent(M_LOSING_FOCUS);
		}
		
		LEdit::OnFocus(f);
	}

	bool OnKey(LKey &k)
	{
		/*	This should be handled by LEdit::OnKey now.
			Which will send a GNotify_EscapeKey or GNotify_ReturnKey
			up to the ItemEdit OnNotify handler.

		switch (k.vkey)
		{
			case LK_RETURN:
			case LK_ESCAPE:
			{
				if (k.Down())
					ItemEdit->OnNotify(this, k.c16);			
				return true;
			}
		}
		*/
		
		return LEdit::OnKey(k);
	}

	bool SetScrollBars(bool x, bool y)
	{
		return false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
class GItemEditPrivate
{
public:
	GItem *Item;
	LEdit *Edit;
	int Index;
	bool Esc;

	GItemEditPrivate()
	{
		Esc = false;
		Item = 0;
		Index = 0;
	}
};

GItemEdit::GItemEdit(LView *parent, GItem *item, int index, int SelStart, int SelEnd)
	: LPopup(parent)
{
	d = new GItemEditPrivate;
	d->Item = item;
	d->Index = index;

	_BorderSize = 0;
	Sunken(false);
	Raised(false);
	
	#if DEBUG_EDIT_LABEL
	LgiTrace("%s:%i - GItemEdit(%p/%s, %i, %i, %i)\n",
		_FL,
		parent, parent?parent->GetClass():0,
		index,
		SelStart, SelEnd);
	#endif
	
	LPoint p;
	SetParent(parent);
	GetParent()->PointToScreen(p);

	LRect r = d->Item->GetPos(d->Index);
	int MinY = 6 + LSysFont->GetHeight();
	if (r.Y() < MinY)
		r.y2 = r.y1 + MinY - 1;
	r.Offset(p.x, p.y);	
	SetPos(r);

	if (Attach(parent))
	{
		d->Edit = new GItemEditBox(this, r.X(), r.Y(), d->Item->GetText(d->Index));
		if (d->Edit)
		{
			d->Edit->Attach(this);
			d->Edit->Focus(true);
			
			if (SelStart >= 0)
			{
				d->Edit->Select(SelStart, SelEnd-SelStart+1);
			}
		}

		Visible(true);
	}
}

GItemEdit::~GItemEdit()
{
	if (d->Item)
	{
		if (d->Edit && !d->Esc)
		{
			auto Str = d->Edit->Name();
			#if DEBUG_EDIT_LABEL
			LgiTrace("%s:%i - ~GItemEdit, updating item(%i) with '%s'\n", _FL,
				d->Index, Str);
			#endif

			LItemContainer *c = d->Item->GetContainer();
			if (d->Item->SetText(Str, d->Index))
			{
				d->Item->Update();
			}
			else
			{
				// Item is deleting itself...
				
				// Make sure there is no dangling ptr on the container..
				if (c) c->ItemEdit = NULL;
				// And we don't touch the no longer existant item..
				d->Item = NULL;
			}
		}
		#if DEBUG_EDIT_LABEL
		else LgiTrace("%s:%i - Edit=%p Esc=%i\n", _FL, d->Edit, d->Esc);
		#endif

		if (d->Item)
			d->Item->OnEditLabelEnd();
	}
	#if DEBUG_EDIT_LABEL
	else LgiTrace("%s:%i - Error: No item?\n", _FL);
	#endif
		
	DeleteObj(d);
}

GItem *GItemEdit::GetItem()
{
	return d->Item;
}

void GItemEdit::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_BLACK);
	pDC->Rectangle();
}

int GItemEdit::OnNotify(LViewI *v, int f)
{
	switch (v->GetId())
	{
		case 100:
		{
			if (f == GNotify_EscapeKey)
			{
				d->Esc = true;
				#if DEBUG_EDIT_LABEL
				LgiTrace("%s:%i - GItemEdit got escape\n", _FL);
				#endif
			}

			if (f == GNotify_EscapeKey || f == GNotify_ReturnKey)
			{
				#if DEBUG_EDIT_LABEL
				LgiTrace("%s:%i - GItemEdit hiding on esc/enter\n", _FL);
				#endif
				d->Edit->KeyProcessed();
				Visible(false);
			}

			break;
		}
	}

	return 0;
}

void GItemEdit::Visible(bool i)
{
	LPopup::Visible(i);
	if (!i)
	{
		#if DEBUG_EDIT_LABEL
		LgiTrace("%s:%i - GItemEdit posting M_END_POPUP\n", _FL);
		#endif
		PostEvent(M_END_POPUP);
	}
}

bool GItemEdit::OnKey(LKey &k)
{
	if (d->Edit)
		return d->Edit->OnKey(k);
	return false;
}

void GItemEdit::OnFocus(bool f)
{
	if (f && d->Edit)
		d->Edit->Focus(true);
}

GMessage::Result GItemEdit::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_LOSING_FOCUS:
		{
			#if DEBUG_EDIT_LABEL
			LgiTrace("%s:%i - GItemEdit get M_LOSING_FOCUS\n", _FL);
			#endif

			// One of us has to retain focus... don't care which control.
			if (Focus() || d->Edit->Focus())
				break;

			// else fall thru to end the popup
			#if DEBUG_EDIT_LABEL
			LgiTrace("%s:%i - GItemEdit falling thru to M_END_POPUP\n", _FL);
			#endif
		}
		case M_END_POPUP:
		{
			#if DEBUG_EDIT_LABEL
			LgiTrace("%s:%i - GItemEdit got M_END_POPUP, quiting\n", _FL);
			#endif
			
			if (d->Item &&
				d->Item->GetContainer())
			{
				d->Item->GetContainer()->Focus(true);
			}
			
			Quit();
			return 0;
		}
	}

	return LPopup::OnEvent(Msg);
}
