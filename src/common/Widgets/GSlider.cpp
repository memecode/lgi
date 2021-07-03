#include "Lgi.h"
#include "GSlider.h"

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	ResObject(Res_Slider)
{
	SetId(id);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	Vertical = vert;
	Min = Max = 0;
	Val = 0;
	SetTabStop(true);
}

GSlider::~GSlider()
{
}

void GSlider::Value(int64 i)
{
	if (i > Max) i = Max;
	if (i < Min) i = Min;
	
	if (i != Val)
	{
		Val = i;

		GViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
		{
			n->OnNotify(this, Val);
		}
		
		Invalidate();
	}
}

int64 GSlider::Value()
{
	return Val;
}

GRange GSlider::GetRange()
{
	return GRange(Min, Max-Min+1);
}

bool GSlider::SetRange(const GRange &r)
{
	Min = r.Start;
	Max = r.End();
}

void GSlider::GetLimits(int64 &min, int64 &max)
{
	min = Min;
	max = Max;
}

void GSlider::SetLimits(int64 min, int64 max)
{
	Min = min;
	Max = max;
}

bool GSlider::OnLayout(GViewLayoutInfo &Inf)
{
	if (Inf.Width.Min)
	{
		// Height
		Inf.Height.Min = GetFont()->GetHeight();
		Inf.Height.Max = Inf.Height.Min;
	}
	else
	{
		// Width
		Inf.Width.Min = GetFont()->GetHeight();
		Inf.Width.Max = -1; // Fill
	}

	return true;
}

GMessage::Param GSlider::OnEvent(GMessage *Msg)
{
	return GControl::OnEvent(Msg);
}

void GSlider::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
	
	LRect r = GetClient();
	int y = r.Y() >> 1;
	r.y1 = y - 2;
	r.y2 = r.y1 + 3;
	r.x1 += 3;
	r.x2 -= 3;
	LWideBorder(pDC, r, DefaultSunkenEdge);
	
	if (Min < Max)
	{
		int x = Val * r.X() / (Max-Min);
		Thumb.ZOff(5, 9);
		Thumb.Offset(r.x1 + x - 3, y - 5);
		LRect b = Thumb;
		LWideBorder(pDC, b, DefaultRaisedEdge);
		pDC->Rectangle(&b);		
	}
}

void GSlider::OnMouseClick(LMouse &m)
{
	Capture(m.Down());
	if (Thumb.Overlap(m.x, m.y))
	{
		Tx = m.x - Thumb.x1;
		Ty = m.y - Thumb.y1;
	}
}

void GSlider::OnMouseMove(LMouse &m)
{
	if (IsCapturing())
	{
		int Rx = X() - 6;
		if (Rx > 0 && Max >= Min)
		{
			int x = m.x - Tx;
			int v = x * (Max-Min) / Rx;
			Value(v);
		}
	}
}

class GSlider_Factory : public GViewFactory
{
	GView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (stricmp(Class, "GSlider") == 0)
		{
			return new GSlider(-1, 0, 0, 100, 20, 0, 0);
		}

		return 0;
	}

} GSliderFactory;

