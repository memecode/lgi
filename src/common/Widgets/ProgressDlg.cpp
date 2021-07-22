/*hdr
**      FILE:           LProgressDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           11/11/98
**      DESCRIPTION:    Progress stuff
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <time.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/LgiRes.h"

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

LString Progress::GetDescription()
{
	LString r;

	LMutex::Auto lck(this, _FL);
	if (!lck)
		LAssert(0);
	else
		r = Description.Get();

	return r;
}

void Progress::SetDescription(const char *d)
{
	LMutex::Auto lck(this, _FL);
	if (!lck)
	{
		LAssert(0);
		return;
	}
	
    if (d != Description)
   		Description = d;
}

LString Progress::GetType()
{
	LString s;
	{
		LMutex::Auto lck(this, _FL);
		if (lck) s = Type.Get();
	}
	return s;
}

void Progress::SetType(const char *t)
{
	LMutex::Auto lck(this, _FL);
	Type = t;
}

Progress &Progress::operator =(Progress &p)
{
	SetDescription(p.GetDescription());

	SetRange(p.GetRange());
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

LProgressPane::LProgressPane(LProgressDlg *dlg) : Dlg(dlg)
{
	t = NULL;
	UiDirty = false;
	LRect r(0, 0, PANE_X-1, PANE_Y-1);
	SetPos(r);
	Name(LgiLoadString(L_PROGRESSDLG_PROGRESS, "Progress"));
	SetId(IDC_PANE);
	Ref = 0;

	if (AddView(t = new LTableLayout(IDC_TABLE)))
	{
		OnPosChange();

		#define PAD 1
		
		int Row = 0;
		GLayoutCell *c = t->GetCell(0, Row++, true, 2, 1);
		#ifdef PAD
		c->Padding(LCss::Len(LCss::LenPx, PAD));
		#endif
		c->Height("1.1em"); // This stops the layout flickering
		c->Add(Desc = new LTextLabel(IDC_DESCRIPTION, 0, 0, -1, -1, "##"));

		c = t->GetCell(0, Row);
		#ifdef PAD
		c->Padding(LCss::Len(LCss::LenPx, PAD));
		#endif
		c->Add(ValText = new LTextLabel(IDC_VALUE, 0, 0, -1, -1, "##"));

		c = t->GetCell(1, Row++);
		#ifdef PAD
		c->Padding(LCss::Len(LCss::LenPx, PAD));
		#endif
		c->Add(Rate = new LTextLabel(IDC_RATE, 0, 0, -1, -1, "##"));

		c = t->GetCell(0, Row++, true, 2, 1);
		#ifdef PAD
		c->Padding(LCss::Len(LCss::LenPx, PAD));
		#endif
		c->Add(Bar = new LProgressView(IDC_PROGRESS, 0, 0, PANE_X - 14, 10, "Progress"));

		c = t->GetCell(0, Row++, true, 2, 1);
		#ifdef PAD
		c->Padding(LCss::Len(LCss::LenPx, PAD));
		#endif
		c->TextAlign(LCss::Len(LCss::AlignCenter));
		c->Add(But = new LButton(IDC_BUTTON, 0, 0, -1, -1, LgiLoadString(L_PROGRESSDLG_REQ_ABORT, "Request Abort")));
	}
}

LProgressPane::~LProgressPane()
{
}

bool LProgressPane::SetRange(const LRange &r)
{
	UiDirty = true;
	Progress::SetRange(r);

	if (Bar)
		Bar->SetRange(r);

	if (InThread())
	{
		LProgressDlg *Pd = dynamic_cast<LProgressDlg*>(GetParent());
		if (Pd && But)
			But->Enabled(Pd->CanCancel);
	}

	return true;
}

void LProgressPane::UpdateUI()
{
	if (!UiDirty)
		return;

	char Str[256];

	bool Update = false;
	UiDirty = false;

	uint64 Now = LCurrentTime();
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
		
		sprintf_s(Str, sizeof(Str), LgiLoadString(L_PROGRESSDLG_RATE_FMT, "@ %.2f %s / sec"), PerSec * Scale, (Type) ? Type.Get() : "");
		Update |= Rate->Name(Str);
	}

	if (ValText)
	{
		auto ValFmt = LgiLoadString(L_PROGRESSDLG_VALUE_FMT, "%.1f of %.1f %s");
		sprintf_s(Str, sizeof(Str), ValFmt,
			(double)Val * Scale,
			(double)(High - Low) * Scale,
			(Type) ? Type.Get() : "");
		
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

void LProgressPane::Value(int64 v)
{
	Progress::Value(v);
	
	UiDirty = true;
	if (Dlg)
		Dlg->TimeCheck();
}

void LProgressPane::OnCreate()
{
	AttachChildren();
}

LProgressPane &LProgressPane::operator++(int)
{
	Value(Progress::Value() + 1);
	return *this;
}

LProgressPane &LProgressPane::operator--(int)
{
	Value(Progress::Value() - 1);
	return *this;
}

int LProgressPane::OnNotify(LViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_TABLE:
		{
			if (Flags == GNotifyTableLayout_LayoutChanged)
			{
				LRect p = GetPos();
				LRect tbl_pos = t->GetPos();
				LRect tbl_req = t->GetUsedArea();
				if (tbl_req.Valid() &&
					tbl_req.Y() > tbl_pos.Y())
				{
					p.y2 = p.y1 + (tbl_req.Y() + LTableLayout::CellSpacing * 2);
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

void LProgressPane::OnPaint(LSurface *pDC)
{
	LRect r = GetClient();
	LThinBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(L_MED);
	pDC->Rectangle(&r);
}

void LProgressPane::OnPosChange()
{
	if (t)
	{
		LRect cr = GetClient();
		cr.Size(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
		t->SetPos(cr);
	}
}

LFont *LProgressPane::GetFont()
{
	// GdcBeTtf *Fnt = LSysFont;
	// return (Fnt) ? Fnt->Handle() : 0;
	return 0;
}

void LProgressPane::SetDescription(const char *d)
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
#define DefX		LAppInst->GetMetric(LGI_MET_DECOR_X)
#define DefY		LAppInst->GetMetric(LGI_MET_DECOR_Y)
// #endif

LProgressDlg::LProgressDlg(LView *parent, uint64 timeout)
{
	Ts = LCurrentTime();
	YieldTs = 0;
	Timeout = timeout;
	CanCancel = true;
	SetParent(parent);
	Resize();
	
	if (parent)
		MoveSameScreen(parent);
	else
		MoveToCenter();	
	
	Name(LgiLoadString(L_PROGRESSDLG_PROGRESS, "Progress"));
	if (Timeout == 0)
		DoModeless();
	else
		Push();
}

LProgressDlg::~LProgressDlg()
{
	if (Visible())
		EndModeless(true);
}

bool LProgressDlg::OnRequestClose(bool OsClose)
{
	for (auto p: Panes)
		p->Cancel(true);
		
	return false;
}

void LProgressDlg::Resize()
{
	LRect r, c = GetPos();
	int DecorX = LAppInst->GetMetric(LGI_MET_DECOR_X);
	int DecorY = LAppInst->GetMetric(LGI_MET_DECOR_Y);

	size_t Items = MAX(1, Panes.Length());
	int Width = DecorX + PANE_X;
	int Height = (int) (DecorY + (PANE_Y * Items));

	r.ZOff(Width - 1, Height - 1);
	r.Offset(c.x1, c.y1);
	SetPos(r);

	// Layout all the panes...
	int y = 0;
	for (auto p: Panes)
	{
		LRect r(0, y, PANE_X - 1, y + PANE_Y - 1);
		p->SetPos(r);
		p->Visible(true);
		y = r.y2 + 1;
	}
}

void LProgressDlg::OnCreate()
{
	if (Panes.Length() == 0)
		Push();
	SetPulse(500);
}

void LProgressDlg::OnPulse()
{
	for (auto p: Panes)
		p->UpdateUI();
}

void LProgressDlg::OnPosChange()
{
	LRect c = GetClient();
	
	// Layout all the panes...
	int y = 0;
	for (auto p: Panes)
	{
		LRect r = p->GetPos();
		r.Offset(0, y-r.y1);
		r.x2 = c.x2;
			
		p->SetPos(r);
		p->Visible(true);
			
		y = r.y2 + 1;
	}
}

int LProgressDlg::OnNotify(LViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_PANE &&
		Flags == GNotifyTableLayout_LayoutChanged)
	{
		// This code recalculates the size needed by all the progress panes
		// and then resizes the window to contain them all.
		LRect u(0, 0, -1, -1);
		for (auto p: Panes)
		{
			LRect r = p->GetPos();
			if (u.Valid()) u.Union(&r);
			else u = r;
		}

		if (u.Valid())
		{		
			int x = u.X();
			int y = u.Y();
			LRect p = GetPos();
			p.Dimension(x + LAppInst->GetMetric(LGI_MET_DECOR_X),
						y + LAppInst->GetMetric(LGI_MET_DECOR_Y));
			SetPos(p);
		}
	}
	
	return 0;
}

GMessage::Result LProgressDlg::OnEvent(GMessage *Msg)
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

	return LDialog::OnEvent(Msg);
}

LProgressPane *LProgressDlg::ItemAt(int i)
{
	return Panes.ItemAt(i);
}

LProgressPane *LProgressDlg::Push()
{
	LProgressPane *Pane = new LProgressPane(this);
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

void LProgressDlg::Pop(LProgressPane *p)
{
	LProgressPane *Pane = (p) ? p : Panes.Last();
	if (Pane)
	{
		Pane->Detach();
		Panes.Delete(Pane);
		LView::Invalidate();
		DeleteObj(Pane);
		Resize();
	}
}

void LProgressDlg::SetCanCancel(bool cc)
{
	CanCancel = cc;
}

LString LProgressDlg::GetDescription()
{
	LString s;
	if (Panes.Length())
		s = Panes.First()->GetDescription();
	return s;
}

void LProgressDlg::SetDescription(const char *d)
{
	if (Panes.Length())
		Panes.First()->SetDescription(d);
}

LRange LProgressDlg::GetRange()
{
	if (Panes.Length())
		return Panes.First()->GetRange();
	return LRange();
}

bool LProgressDlg::SetRange(const LRange &r)
{
	if (!Panes.Length())
		return false;
	Panes.First()->SetRange(r);
	return true;
}

LProgressDlg &LProgressDlg::operator++(int)
{
	if (Panes.Length())
	{
		auto p = Panes.First();
		(*p)++;
	}
	return *this;
}

LProgressDlg &LProgressDlg::operator--(int)
{
	if (Panes.Length())
	{
		auto p = Panes.First();
		(*p)--;
	}
	return *this;
}

void LProgressDlg::TimeCheck()
{
	if (!InThread())
		return;
	uint64 Now = LCurrentTime();
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

int64 LProgressDlg::Value()
{
	return Panes.Length() ? Panes.First()->Value() : -1;
}

void LProgressDlg::Value(int64 v)
{
	if (Panes.Length())
		Panes.First()->Value(v);
}

double LProgressDlg::GetScale()
{
	return Panes.Length() ? Panes.First()->GetScale() : 0.0;
}

void LProgressDlg::SetScale(double s)
{
	if (Panes.Length())
		Panes.First()->SetScale(s);
}

LString LProgressDlg::GetType()
{
	return Panes.Length() ? Panes.First()->GetType() : NULL;
}

void LProgressDlg::SetType(const char *t)
{
	if (Panes.Length())
		Panes.First()->SetType(t);
}

bool LProgressDlg::IsCancelled()
{
	for (auto p: Panes)
	{
		if (p->IsCancelled())
			return true;
	}
	return false;
}

void LProgressDlg::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
}

