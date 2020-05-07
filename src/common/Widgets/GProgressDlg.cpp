/*hdr
**      FILE:           GProgressDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           11/11/98
**      DESCRIPTION:    Progress stuff
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <time.h>
#include "Lgi.h"
#include "GProgressDlg.h"
#include "GTextLabel.h"
#include "GButton.h"
#include "GTableLayout.h"
#include "LgiRes.h"

/////////////////////////////////////////////////////////////////////////////////////////
Progress::Progress() : LMutex("ProgressObj")
{
	Start = 0;
	Val = Low = 0;
	High = 0;
	Type = 0;
	Scale = 1.0;
	UserData = 0;
}

Progress::Progress(char *desc, int64 l, int64 h, char *type, double scale)
{
	Description = desc;
	Start = 0;
	Val = Low = l;
	High = h;
	Type = type;
	Scale = scale;
}

void Progress::SetLimits(int64 l, int64 h)
{
	Low = l;
	High = h;
}

void Progress::GetLimits(int64 *l, int64 *h)
{
	if (l) *l = Low;
	if (h) *h = High;
}

GString Progress::GetDescription()
{
	GString r;

	LMutex::Auto lck(this, _FL);
	if (!lck)
		LgiAssert(0);
	else
		r = Description.Get();

	return r;
}

void Progress::SetDescription(const char *d)
{
	LMutex::Auto lck(this, _FL);
	if (!lck)
	{
		LgiAssert(0);
		return;
	}
	
    if (d != Description)
   		Description = d;
}

Progress &Progress::operator =(Progress &p)
{
	SetDescription(p.GetDescription());

	int64 h, l;
	p.GetLimits(&h, &l);
	SetLimits(h, l);

	SetScale(p.GetScale());
	SetType(p.GetType());
	Value(p.Value());

	return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////
#define IDC_DESCRIPTION			100
#define IDC_VALUE				101
#define IDC_RATE				102
#define IDC_PROGRESS			103
#define IDC_BUTTON				104
#define IDC_TABLE				105
#define IDC_PANE				106

#define PANE_X					300
#define PANE_Y					100

GProgressPane::GProgressPane(GProgressDlg *dlg) : Dlg(dlg)
{
	t = NULL;
	UiDirty = false;
	GRect r(0, 0, PANE_X-1, PANE_Y-1);
	SetPos(r);
	Name(LgiLoadString(L_PROGRESSDLG_PROGRESS, "Progress"));
	SetId(IDC_PANE);
	Ref = 0;

	if (AddView(t = new GTableLayout(IDC_TABLE)))
	{
		OnPosChange();

		#define PAD 1
		
		int Row = 0;
		GLayoutCell *c = t->GetCell(0, Row++, true, 2, 1);
		#ifdef PAD
		c->Padding(GCss::Len(GCss::LenPx, PAD));
		#endif
		c->Height("1.1em"); // This stops the layout flickering
		c->Add(Desc = new GTextLabel(IDC_DESCRIPTION, 0, 0, -1, -1, "##"));

		c = t->GetCell(0, Row);
		#ifdef PAD
		c->Padding(GCss::Len(GCss::LenPx, PAD));
		#endif
		c->Add(ValText = new GTextLabel(IDC_VALUE, 0, 0, -1, -1, "##"));

		c = t->GetCell(1, Row++);
		#ifdef PAD
		c->Padding(GCss::Len(GCss::LenPx, PAD));
		#endif
		c->Add(Rate = new GTextLabel(IDC_RATE, 0, 0, -1, -1, "##"));

		c = t->GetCell(0, Row++, true, 2, 1);
		#ifdef PAD
		c->Padding(GCss::Len(GCss::LenPx, PAD));
		#endif
		c->Add(Bar = new GProgress(IDC_PROGRESS, 0, 0, PANE_X - 14, 10, "Progress"));

		c = t->GetCell(0, Row++, true, 2, 1);
		#ifdef PAD
		c->Padding(GCss::Len(GCss::LenPx, PAD));
		#endif
		c->TextAlign(GCss::Len(GCss::AlignCenter));
		c->Add(But = new GButton(IDC_BUTTON, 0, 0, -1, -1, LgiLoadString(L_PROGRESSDLG_REQ_ABORT, "Request Abort")));
	}
}

GProgressPane::~GProgressPane()
{
}

void GProgressPane::SetLimits(int64 l, int64 h)
{
	Progress::SetLimits(l, h);

	if (Bar)
	{
		#ifdef ALT_SCALE
		Bar->SetLimits(0, ALT_SCALE);
		#else
		Bar->SetLimits(l, h);
		#endif
	}

	if (InThread())
	{
		GProgressDlg *Pd = dynamic_cast<GProgressDlg*>(GetParent());
		if (Pd && But)
			But->Enabled(Pd->CanCancel);
	}
}

void GProgressPane::UpdateUI()
{
	if (!UiDirty)
		return;

	char Str[256];

	bool Update = false;
	UiDirty = false;

	uint64 Now = LgiCurrentTime();
	if (Start == 0)
	{
		// initialize the clock
		Start = Now;
	}
	else if (Rate)
	{
		// calc rate
		double Secs = ((double)(int64)(Now - Start)) / 1000.0;
		double PerSec;
		
		if (Secs != 0.0)
			PerSec = ((double) Val - Low) / Secs;
		else
			PerSec = 0;
		
		sprintf_s(Str, sizeof(Str), LgiLoadString(L_PROGRESSDLG_RATE_FMT, "@ %.2f %s / sec"), PerSec * Scale, (Type) ? Type : "");
		Update |= Rate->Name(Str);
	}

	if (ValText)
	{
		auto ValFmt = LgiLoadString(L_PROGRESSDLG_VALUE_FMT, "%.1f of %.1f %s");
		sprintf_s(Str, sizeof(Str), ValFmt,
			(double)Val * Scale,
			(double)(High - Low) * Scale,
			(Type) ? Type : "");
		
		Update |= ValText->Name(Str);
	}

	if (Bar)
	{
		if (High != Low)
		{
			#ifdef ALT_SCALE
			double Raw = ((double) v - Low) / ((double) High - Low);
			Bar->Value((int)(Raw * ALT_SCALE));
			#else
			Bar->Value(Value());
			#endif

			Bar->Invalidate();
		}
		else
		{
			Bar->Value(0);
		}
	}
	
	if (ValText)
		ValText->SendNotify(GNotifyTableLayout_Refresh);
}

void GProgressPane::Value(int64 v)
{
	Progress::Value(v);
	
	UiDirty = true;
	if (Dlg)
		Dlg->TimeCheck();
}

void GProgressPane::OnCreate()
{
	AttachChildren();
}

GProgressPane &GProgressPane::operator++(int)
{
	Value(Progress::Value() + 1);
	return *this;
}

GProgressPane &GProgressPane::operator--(int)
{
	Value(Progress::Value() - 1);
	return *this;
}


int GProgressPane::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_TABLE:
		{
			if (Flags == GNotifyTableLayout_LayoutChanged)
			{
				GRect p = GetPos();
				GRect tbl_pos = t->GetPos();
				GRect tbl_req = t->GetUsedArea();
				if (tbl_req.Valid() &&
					tbl_req.Y() > tbl_pos.Y())
				{
					p.y2 = p.y1 + (tbl_req.Y() + GTableLayout::CellSpacing * 2);
					SetPos(p);
					
					SendNotify(GNotifyTableLayout_LayoutChanged);
				}
			}
			break;
		}
		case IDC_BUTTON:
		{
			Cancel(true);

			if (But)
			{
				But->Name("Waiting...");
			}
			break;
		}
	}

	return 0;
}

void GProgressPane::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();
	LgiThinBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(L_MED);
	pDC->Rectangle(&r);
}

void GProgressPane::OnPosChange()
{
	if (t)
	{
		GRect cr = GetClient();
		cr.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
		t->SetPos(cr);
	}
}

GFont *GProgressPane::GetFont()
{
	// GdcBeTtf *Fnt = SysFont;
	// return (Fnt) ? Fnt->Handle() : 0;
	return 0;
}

void GProgressPane::SetDescription(const char *d)
{
	Progress::SetDescription(d);
	if (Desc)
	{
		Desc->Name(d);
		Desc->SendNotify(GNotifyTableLayout_Refresh);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// #ifdef WIN32
// #define DefX		((GetSystemMetrics(SM_CXDLGFRAME) * 2))
// #define DefY		((GetSystemMetrics(SM_CYDLGFRAME) * 2) + GetSystemMetrics(SM_CYCAPTION))
// #else
#define DefX		LgiApp->GetMetric(LGI_MET_DECOR_X)
#define DefY		LgiApp->GetMetric(LGI_MET_DECOR_Y)
// #endif

GProgressDlg::GProgressDlg(GView *parent, uint64 timeout)
{
	Ts = LgiCurrentTime();
	YieldTs = 0;
	Timeout = timeout;
	CanCancel = true;
	SetParent(parent);
	Resize();
	MoveToCenter();
	Name(LgiLoadString(L_PROGRESSDLG_PROGRESS, "Progress"));
	#ifdef BEOS
	WindowHandle()->SetFeel(B_NORMAL_WINDOW_FEEL);
	#endif
	if (Timeout == 0)
		DoModeless();
	else
		Push();
}

GProgressDlg::~GProgressDlg()
{
	if (Visible())
		EndModeless(true);
}

bool GProgressDlg::OnRequestClose(bool OsClose)
{
	for (GProgressPane **p = NULL; Panes.Iterate(p); )
	{
		(*p)->Cancel(true);
	}
		
	return false;
}

void GProgressDlg::Resize()
{
	GRect r, c = GetPos();
	int DecorX = LgiApp->GetMetric(LGI_MET_DECOR_X);
	int DecorY = LgiApp->GetMetric(LGI_MET_DECOR_Y);

	size_t Items = MAX(1, Panes.Length());
	int Width = DecorX + PANE_X;
	int Height = (int) (DecorY + (PANE_Y * Items));

	r.ZOff(Width - 1, Height - 1);
	r.Offset(c.x1, c.y1);
	SetPos(r);

	// Layout all the panes...
	int y = 0;
	for (GProgressPane **p = NULL; Panes.Iterate(p); )
	{
		GRect r(0, y, PANE_X - 1, y + PANE_Y - 1);
		(*p)->SetPos(r);
		(*p)->Visible(true);
		y = r.y2 + 1;
	}
}

void GProgressDlg::OnCreate()
{
	if (Panes.Length() == 0)
		Push();
	SetPulse(500);
}

void GProgressDlg::OnPulse()
{
	for (GProgressPane **p = NULL; Panes.Iterate(p); )
	{
		(*p)->UpdateUI();
	}
}

void GProgressDlg::OnPosChange()
{
	GRect c = GetClient();
	
	// Layout all the panes...
	int y = 0;
	for (GProgressPane **p = NULL; Panes.Iterate(p); )
	{
		GRect r = (*p)->GetPos();
		r.Offset(0, y-r.y1);
		r.x2 = c.x2;
			
		(*p)->SetPos(r);
		(*p)->Visible(true);
			
		y = r.y2 + 1;
	}
}

int GProgressDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_PANE &&
		Flags == GNotifyTableLayout_LayoutChanged)
	{
		// This code recalculates the size needed by all the progress panes
		// and then resizes the window to contain them all.
		GRect u(0, 0, -1, -1);
		for (GProgressPane **p = NULL; Panes.Iterate(p); )
		{
			GRect r = (*p)->GetPos();
			if (u.Valid()) u.Union(&r);
			else u = r;
		}

		if (u.Valid())
		{		
			int x = u.X();
			int y = u.Y();
			GRect p = GetPos();
			p.Dimension(x + LgiApp->GetMetric(LGI_MET_DECOR_X),
						y + LgiApp->GetMetric(LGI_MET_DECOR_Y));
			SetPos(p);
		}
	}
	
	return 0;
}

GMessage::Result GProgressDlg::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		#ifdef WIN32
		case WM_CLOSE:
		{
			return 0;
		}
		#endif
	}

	return GDialog::OnEvent(Msg);
}

GProgressPane *GProgressDlg::ItemAt(int i)
{
	return Panes.ItemAt(i);
}

GProgressPane *GProgressDlg::Push()
{
	GProgressPane *Pane = new GProgressPane(this);
	if (Pane)
	{
		// Attach the new pane..
		if (Visible())
			Pane->Attach(this);
		else
			AddView(Pane);
		Panes.Add(Pane);
		Resize();
	}

	return Pane;
}

void GProgressDlg::Pop(GProgressPane *p)
{
	GProgressPane *Pane = (p) ? p : Panes.Last();
	if (Pane)
	{
		Pane->Detach();
		Panes.Delete(Pane);
		GView::Invalidate();
		DeleteObj(Pane);
		Resize();
	}
}

void GProgressDlg::SetCanCancel(bool cc)
{
	CanCancel = cc;
}

GString GProgressDlg::GetDescription()
{
	GString s;
	if (Panes.Length())
		s = Panes.First()->GetDescription();
	return s;
}

void GProgressDlg::SetDescription(const char *d)
{
	if (Panes.Length())
		Panes.First()->SetDescription(d);
}

void GProgressDlg::GetLimits(int64 *l, int64 *h)
{
	if (Panes.Length())
		Panes.First()->GetLimits(l, h);
	else
	{
		if (l) *l = 0;
		if (h) *h = 0;
	}
}

void GProgressDlg::SetLimits(int64 l, int64 h)
{
	if (Panes.Length())
		Panes.First()->SetLimits(l, h);
}

GProgressDlg &GProgressDlg::operator++(int)
{
	if (Panes.Length())
	{
		auto p = Panes.First();
		(*p)++;
	}
	return *this;
}

GProgressDlg &GProgressDlg::operator--(int)
{
	if (Panes.Length())
	{
		auto p = Panes.First();
		(*p)--;
	}
	return *this;
}

void GProgressDlg::TimeCheck()
{
	if (!InThread())
		return;
	uint64 Now = LgiCurrentTime();
	if (Timeout)
	{
		if (Now - Ts >= Timeout)
		{
			DoModeless();
			Timeout = 0;
		}
	}
	else if (YieldTs)
	{
		if (Now - Ts >= YieldTs)
		{
			Ts = Now;
			LgiYield();
		}
	}
}

int64 GProgressDlg::Value()
{
	return Panes.Length() ? Panes.First()->Value() : -1;
}

void GProgressDlg::Value(int64 v)
{
	if (Panes.Length())
		Panes.First()->Value(v);
}

double GProgressDlg::GetScale()
{
	return Panes.Length() ? Panes.First()->GetScale() : 0.0;
}

void GProgressDlg::SetScale(double s)
{
	if (Panes.Length())
		Panes.First()->SetScale(s);
}

const char *GProgressDlg::GetType()
{
	return Panes.Length() ? Panes.First()->GetType() : NULL;
}

void GProgressDlg::SetType(const char *t)
{
	if (Panes.Length())
		Panes.First()->SetType(t);
}

bool GProgressDlg::IsCancelled()
{
	for (GProgressPane **p = NULL; Panes.Iterate(p); )
	{
		if ((*p)->IsCancelled())
			return true;
	}
	return false;
}

void GProgressDlg::OnPaint(GSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
}

