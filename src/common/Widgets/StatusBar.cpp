#include "lgi/common/Lgi.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/StatusBar.h"
#include "lgi/common/CssTools.h"

//////////////////////////////////////////////////////////////////////////////////////
LStatusBar::LStatusBar(int id)
{
	Name("LGI_StatusBar");
	Raised(true);
	GetBorder().Set(1, 1, 1, 1);
	if (id > 0)
		SetId(id);
	LResources::StyleElement(this);
}

LStatusBar::~LStatusBar()
{
}

bool LStatusBar::OnLayout(LViewLayoutInfo &Inf)
{
	if (!Inf.Width.Min)
	{
		// Width is always 'fill'
		Inf.Width.Min = Inf.Width.Max = LViewLayoutInfo::FILL;
	}
	else
	{
		auto fnt = GetFont();
		Inf.Height.Min = Inf.Height.Max = fnt ? fnt->GetHeight() + 8 : 32;
	}
	
	return true;
}

bool LStatusBar::Pour(LRegion &r)
{
	if (auto Best = FindLargestEdge(r, GV_EDGE_BOTTOM))
	{
		auto fnt = GetFont();
		auto ht = fnt ? fnt->GetHeight() + 8 : 32;

		LRect Take = *Best;
		if (Take.Y() > ht)
		{
			Take.y1 = MAX((Take.y2 - ht), Take.y1);
			SetPos(Take);
			return true;
		}
	}

	return false;
}

void LStatusBar::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
}

int LStatusBar::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	switch (n.Type)
	{
		case LNotifyTableLayoutRefresh:
		{
			OnPosChange();
			Invalidate();
			return true;
		}
		default:
			break;
	}

	return LLayout::OnNotify(Ctrl, n);
}

void LStatusBar::OnPosChange()
{
	auto boxPx = X();
	auto fnt = GetFont();
	auto c = GetClient();
	c.Inset(1, 1);

	LArray<LViewLayoutInfo> layout;
	int remainingPx = c.X();
	int adjustable = 0;

	auto children = IterateViews();

	// Filter out non pane children...
	for (unsigned i=0; i<children.Length();)
	{
		if (!dynamic_cast<LStatusPane*>(children[i])) children.DeleteAt(i, true);
		else i++;
	}

	// First pass over the panes to see what has known sizes
	for (unsigned i=0; i<children.Length(); i++)
	{
		auto p = dynamic_cast<LStatusPane*>(children[i]);
		auto css = p->GetCss();
		LCss::Len wid;
		int px = 0;		
		if (css)
			wid = css->Width();
		
		auto &l = layout[i];
		if (wid)
		{
			px = wid.ToPx(boxPx, fnt);
			l.Width.Min = px;
			l.Width.Max = px;
		}
		else
		{
			LDisplayString ds(fnt, p->Name());
			l.Width.Min = ds.X() + (SEPARATOR_PX * 2);
			adjustable++;
		}
		remainingPx -= l.Width.Min + SEPARATOR_PX;
	}

	if (remainingPx > 0)
	{
		// Allocate remaining space...
		int perPane = adjustable ? remainingPx / adjustable : remainingPx;
		for (unsigned i=0; i<children.Length(); i++)
		{
			auto &l = layout[i];
			if (l.Width.Max == 0)
			{
				if (adjustable > 1)
					l.Width.Max = l.Width.Min + perPane;
				else
					l.Width.Max = l.Width.Min + remainingPx;
			}
		}
	}

	// Final layout
	for (unsigned i=0; i<children.Length(); i++)
	{
		auto p = dynamic_cast<LStatusPane*>(children[i]);
		auto &l = layout[i];
		auto px = l.Width.Max;
		auto css = p->GetCss();
		LCss::Len align;
		if (css)
			align = css->TextAlign();

		#ifndef WIN32
		if (!p->IsAttached())
			p->Attach(this);
		#endif

		LRect pos;
		pos.ZOff(px-1, c.Y()-1);
		switch (align.Type)
		{
			case LCss::AlignRight:
			{
				pos.Offset(c.x2 - pos.X(), c.y1);
				c.x2 = pos.x1 - SEPARATOR_PX;
				break;
			}
			default: // Left?
			{
				pos.Offset(c.x1, c.y1);
				c.x1 = pos.x2 + SEPARATOR_PX;
				break;
			}
		}

		p->SetPos(pos);
	}
}


LStatusPane *LStatusBar::AppendPane(const char *Text, int WidthPx)
{
	if (!Text)
		return NULL;

	if (auto Pane = new LStatusPane)
	{
		AddView(Pane);
		Pane->Name(Text);
		if (WidthPx)
			Pane->SetWidth(WidthPx);
		return Pane;
	}
	return NULL;
}

bool LStatusBar::AppendPane(LStatusPane *Pane)
{
	if (!Pane)
		return false;

	return AddView(Pane);
}

// Pane
LStatusPane::LStatusPane()
{
	SetParent(0);
	LRect r(0, 0, GetWidth()-1, 20);
	SetPos(r);
	GetBorder().Set(1, 1, 1, 1);
}

bool LStatusPane::Name(const char *n)
{
	bool Status = false;

	GetWindow(); // This searchs for a parent before we lock...

	if (Lock(_FL))
	{
		auto l = Name();
		if (!n ||
			!l ||
			strcmp(l, n) != 0)
		{
			Status = LBase::Name(n);
			LRect p(0, 0, X()-1, Y()-1);
			p.Inset(1, 1);
			Invalidate(&p);
		}
		Unlock();
	}

	SendNotify(LNotifyTableLayoutRefresh);
	return Status;
}

int LStatusPane::GetWidth()
{
	auto css = GetCss();
	if (!css)
	{
		auto nm = Name();
		if (!nm)
			return 32;

		LDisplayString ds(GetFont(), nm);
		return ds.X() + (LStatusBar::SEPARATOR_PX * 2);
	}

	auto wid = css->Width();
	return wid.ToPx(X(), GetFont());
}

void LStatusPane::SetWidth(int px)
{
	LCss::Len w(LCss::LenPx, px);
	GetCss(true)->Width(w);
	SendNotify(LNotifyTableLayoutRefresh);
}

bool LStatusPane::Sunken()
{
	auto css = GetCss();
	if (!css)
		return false; // default sunken

	auto b = css->Border();
	return b.Style == LCss::BorderInset;
}

void LStatusPane::Sunken(bool inset)
{
	LCss::BorderDef b;
	b.Style = inset ? LCss::BorderInset : LCss::BorderNone;
	GetCss(true)->Border(b);
	SendNotify(LNotifyTableLayoutRefresh);
}

LSurface *LStatusPane::Bitmap()
{
	return Icon;
}

void LStatusPane::Bitmap(LAutoPtr<LSurface> pdc)
{
	Icon = pdc;
	SendNotify(LNotifyTableLayoutRefresh);
}

void LStatusPane::OnPaint(LSurface *pDC)
{
	LString t;
	if (Lock(_FL))
	{
		t = Name();
		Unlock();
	}

	LRect r = GetClient();
	auto css = GetCss();
	auto fnt = GetFont();
	if (css)
	{
		LCssTools tools(css, fnt);
		// LView::OnNcPaint paints the border for us
		// r = tools.PaintBorder(pDC, r);
		tools.PaintContent(pDC, r, t, Icon);
	}
	else if (ValidStr(t))
	{
		auto TabSize = fnt->TabSize();

		fnt->TabSize(0);
		fnt->Colour(L_TEXT, L_MED);
		fnt->Transparent(false);

		LDisplayString ds(LSysFont, t);
		ds.Draw(pDC, LStatusBar::SEPARATOR_PX, (r.Y()-ds.Y())/2, &r);
		
		fnt->TabSize(TabSize);
	}
	else
	{
		pDC->Colour(L_MED);
		pDC->Rectangle(&r);
	}

}

///////////////////////////////////////////////////////////////////////////////////////////
int64 LProgressStatus::Value()
{
	return Progress::Value();
}

void LProgressStatus::Value(int64 v)
{
	Progress::Value(v);
	Invalidate();
}

void LProgressStatus::OnPaint(LSurface *pDC)
{
	LRect r = GetClient();

	LThinBorder(pDC, r, EdgeWin7Sunken);

	auto rng = GetRange();
	auto val = Progress::Value();
	auto px = rng.Len ? val * r.X() / rng.Len : 0;
	
	LRect r1 = r;
	r1.x2 = r1.x1 + (int)px - 1;
	pDC->Colour(IsCancelled() ? LColour::Red : L_FOCUS_SEL_BACK);
	pDC->Rectangle(&r1);

	LRect r2 = r;
	r2.x1 = r1.x2 + 1;
	pDC->Colour(L_MED);
	pDC->Rectangle(&r2);
}
