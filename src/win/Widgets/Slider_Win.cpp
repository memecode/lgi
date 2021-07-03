#include "lgi/common/Lgi.h"
#include "lgi/common/Slider.h"
#include "lgi/common/Css.h"
#include <COMMCTRL.H>
#include "lgi/common/Widgets.h"

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	GControl(LGI_SLIDER),
	ResObject(Res_Slider)
{
	SetId(id);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	Vertical = vert;
	Min = Max = 0;
	Val = 0;

	SetStyle(GetStyle() | WS_VISIBLE | WS_CHILD | WS_TABSTOP | ((Vertical) ? TBS_VERT : TBS_HORZ));
	if (SubClass && !SubClass->SubClass(TRACKBAR_CLASSA))
	{
		LgiAssert(0);
	}
}

GSlider::~GSlider()
{
}

void GSlider::Value(int64 i)
{
	Val = i;
	if (Handle())
	{
		SendMessage(Handle(), TBM_SETPOS, TRUE, Val);
	}
}

int64 GSlider::Value()
{
	if (Handle())
	{
		Val = SendMessage(Handle(), TBM_GETPOS, 0, 0);
	}
	return Val;
}

GRange GSlider::GetRange()
{
	if (Handle())
	{
		Min = SendMessage(Handle(), TBM_GETRANGEMIN, 0, 0);
		Max = SendMessage(Handle(), TBM_GETRANGEMAX, 0, 0);
	}
	return GRange(Min, Max-Min+1);
}

bool GSlider::SetRange(const GRange &r)
{
	Min = r.Start;
	Max = r.End();
	if (Handle())
	{
		SendMessage(Handle(), TBM_SETRANGEMIN, false, (LPARAM)Min);
		SendMessage(Handle(), TBM_SETRANGEMAX, true, (LPARAM)Max);
	}
	return true;
}

void GSlider::GetLimits(int64 &min, int64 &max)
{
	auto r = GetRange();
	min = r.Start;
	max = r.End();
}

void GSlider::SetLimits(int64 min, int64 max)
{
	SetRange(GRange(Min, max-min+1));
}

GMessage::Result GSlider::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_HSCROLL:
		case WM_VSCROLL:
		{
			SendNotify();
			break;
		}
		case WM_ERASEBKGND:
		{
			if (GetCss())
			{
				LCss::ColorDef f = GetCss()->BackgroundColor();
				if (f.Type == LCss::ColorRgb)
				{
					HDC hDC = (HDC)Msg->A();
					LScreenDC dc(hDC, Handle());
					dc.Colour(f.Rgb32, 32);
					dc.Rectangle();
					return 1;
				}
			}
			break;
		}
		case WM_PAINT:
		{
			if (GetCss())
			{
				LCss::ColorDef f = GetCss()->BackgroundColor();
				if (f.Type == LCss::ColorRgb)
				{
					LScreenDC dc(Handle());
					dc.Colour(f.Rgb32, 32);
					dc.Rectangle();
					
					RECT rc;
					SendMessage(Handle(), TBM_GETCHANNELRECT, 0, (LPARAM)&rc);
					LRect r = rc;
					LWideBorder(&dc, r, DefaultSunkenEdge);

					SendMessage(Handle(), TBM_GETTHUMBRECT, 0, (LPARAM)&rc);
					LRect t = rc;
					LWideBorder(&dc, t, DefaultRaisedEdge);
					dc.Colour(L_MED);
					dc.Rectangle(&t);

					if (GetFocus() == Handle())
					{
						RECT rc = GetClient();
						DrawFocusRect(dc.Handle(), &rc);
					}
					return 0;
				}
			}
			break;
		}
	}

	GMessage::Result Status = GControl::OnEvent(Msg);

	switch (Msg->Msg())
	{
		case WM_CREATE:
		{
			SetRange(GRange(Min, Max-Min+1));
			Value(Val);
			break;
		}
	}

	return Status;
}

bool GSlider::OnLayout(LViewLayoutInfo &Inf)
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

////////////////////////////////////////////////////////////////////////
class GSlider_Factory : public GViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (stricmp(Class, "GSlider") == 0)
		{
			return new GSlider(-1, 0, 0, 100, 20, 0, 0);
		}

		return 0;
	}

} GSliderFactory;
