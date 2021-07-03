#include "Lgi.h"
#include "GProgress.h"
#include "GCss.h"

GColour GProgress::cNormal(50, 150, 255);
GColour GProgress::cPaused(222, 160, 0);
GColour GProgress::cError(255, 0, 0);

GProgress::GProgress(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_Progress)
{
	SetId(id);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	if (name) GControl::Name(name);

	c = cNormal;
}

GProgress::~GProgress()
{
}

bool GProgress::Colour(GColour Col)
{
	c = Col;
	Invalidate();
	return true;
}

GColour GProgress::Colour()
{
	return c;
}

bool GProgress::Pour(GRegion &r)
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

bool GProgress::OnLayout(GViewLayoutInfo &Inf)
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

bool GProgress::SetRange(const GRange &r)
{
	Low = r.Start;
	High = r.End();
	Invalidate();
	return true;
}

void GProgress::Value(int64 v)
{
	if (Val != v)
	{
		Val = v;
		Invalidate();
	}
}

int64 GProgress::Value()
{
	return Val;
}

GMessage::Result GProgress::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GProgress::OnPaint(GSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	LgiThinBorder(pDC, r, DefaultSunkenEdge);

	if (High > Low)
	{
		double v = ((double)Val - Low) / ((double)High - Low);
		int Pos = (int) (v * r.X());
		// printf("Prog Paint v=%f val=%i range=%i-%i pos=%i\n", v, (int)Val, (int)Low, (int)High, Pos);
		if (Pos > 0)
		{
			GColour High = c.Mix(GColour::White);
			GColour Low = c.Mix(GColour::Black);
			LRect p = r;

			p.x2 = p.x1 + Pos - 1;
			r.x1 = p.x2 + 1;

			pDC->Colour(Low);
			pDC->Line(p.x2, p.y2, p.x2, p.y1);
			pDC->Line(p.x2, p.y2, p.x1, p.y2);

			pDC->Colour(High);
			pDC->Line(p.x1, p.y1, p.x2, p.y1);
			pDC->Line(p.x1, p.y1, p.x1, p.y2);

			p.Size(1, 1);

			GCss::ColorDef f;
			if (GetCss())
				f = GetCss()->BackgroundColor();
			if (f.Type == GCss::ColorRgb)
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
