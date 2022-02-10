#include "lgi/common/Lgi.h"
#include "lgi/common/ProgressView.h"
#include "lgi/common/Css.h"

LColour LProgressView::cNormal(50, 150, 255);
LColour LProgressView::cPaused(222, 160, 0);
LColour LProgressView::cError(255, 0, 0);

LProgressView::LProgressView(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Progress)
{
	SetId(id);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	if (name) LControl::Name(name);

	c = cNormal;
}

LProgressView::~LProgressView()
{
}

bool LProgressView::Colour(LColour Col)
{
	c = Col;
	Invalidate();
	return true;
}

LColour LProgressView::Colour()
{
	return c;
}

bool LProgressView::Pour(LRegion &r)
{
	LRect *l = FindLargest(r);
	if (l)
	{
		l->y2 = l->y1 + 10;
		SetPos(*l);
		return true;
	}
	return false;
}

bool LProgressView::OnLayout(LViewLayoutInfo &Inf)
{
	if (!Inf.Width.Max)
	{
		Inf.Width.Max = -1;
		Inf.Width.Min = 32;
	}
	else if (!Inf.Height.Max)
	{
		Inf.Height.Max =
			Inf.Height.Min = 10;
	}
	else return false;
	return true;
}

bool LProgressView::SetRange(const LRange &r)
{
	Low = r.Start;
	High = r.End();
	Invalidate();
	return true;
}

void LProgressView::Value(int64 v)
{
	if (Val != v)
	{
		Val = v;
		Invalidate();
	}
}

int64 LProgressView::Value()
{
	return Val;
}

LMessage::Result LProgressView::OnEvent(LMessage *Msg)
{
	return LView::OnEvent(Msg);
}

void LProgressView::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	LThinBorder(pDC, r, DefaultSunkenEdge);

	if (High > Low)
	{
		double v = ((double)Val - Low) / ((double)High - Low);
		int Pos = (int) (v * r.X());
		// printf("Prog Paint v=%f val=%i range=%i-%i pos=%i\n", v, (int)Val, (int)Low, (int)High, Pos);
		if (Pos > 0)
		{
			LColour High = c.Mix(LColour::White);
			LColour Low = c.Mix(LColour::Black);
			LRect p = r;

			p.x2 = p.x1 + Pos - 1;
			r.x1 = p.x2 + 1;

			pDC->Colour(Low);
			pDC->Line(p.x2, p.y2, p.x2, p.y1);
			pDC->Line(p.x2, p.y2, p.x1, p.y2);

			pDC->Colour(High);
			pDC->Line(p.x1, p.y1, p.x2, p.y1);
			pDC->Line(p.x1, p.y1, p.x1, p.y2);

			p.Inset(1, 1);

			LCss::ColorDef f;
			if (GetCss())
				f = GetCss()->BackgroundColor();
			if (f.Type == LCss::ColorRgb)
				pDC->Colour(f.Rgb32, 32);
			else
				pDC->Colour(c);
			pDC->Rectangle(&p);
		}
	}

	if (r.Valid())
	{
		pDC->Colour(L_MED);
		pDC->Rectangle(&r);
	}
}
