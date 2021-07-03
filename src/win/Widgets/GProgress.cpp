#include "lgi/common/Lgi.h"
#include "lgi/common/Progress.h"
#include <COMMCTRL.H>
#include "lgi/common/Css.h"

///////////////////////////////////////////////////////////////////////////////////////////
GProgress::GProgress(int id, int x, int y, int cx, int cy, const char *name) :
	GControl(LGI_PROGRESS),
	ResObject(Res_Progress)
{
	Shift = 0;
	SetClassW32(LGI_PROGRESS);
	SetId(id);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	if (name) GControl::Name(name);

	SetStyle(GetStyle() | WS_CHILD | WS_VISIBLE | PBS_SMOOTH);
	if (SubClass)
	{
		SubClass->SubClass("msctls_progress32");
	}
}

GProgress::~GProgress()
{
}

GColour GProgress::cNormal(0, 222, 0);
GColour GProgress::cPaused(255, 222, 0);
GColour GProgress::cError(255, 0, 0);

bool GProgress::Colour(GColour Col)
{
	c = Col;
	if (!_View)
		return false;

	int msg = 0;
	if (Col == cNormal)
		msg = PBST_NORMAL;
	else if (Col == cPaused)
		msg = PBST_PAUSED;
	else if (Col == cError)
		msg = PBST_ERROR;
	if (!msg)
		return false;
	
	SendMessage(_View, PBM_SETSTATE, msg, 0);
	return true;
}

GColour GProgress::Colour()
{
	return c;
}

bool GProgress::SetRange(const GRange &r)
{
	Low = r.Start;
	High = r.End();
	Shift = 0;

	if (High > 0x7fffffff)
	{
		int64 i = High;
		while (i > 0x7fffffff)
		{
			i >>= 1;
			Shift++;
		}
	}

	if (Handle())
		PostMessage(Handle(), PBM_SETRANGE32, Low>>Shift, High>>Shift);
	return true;
}

int64 GProgress::Value()
{
	return Val;
}

void GProgress::Value(int64 v)
{
	Val = v;
	if (Handle())
	{
		PostMessage(Handle(), PBM_SETPOS, (WPARAM) (Val>>Shift), 0);
	}
}

bool GProgress::Pour(LRegion &r)
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
	    Inf.Width.Min = 64;
	}
	else
	{
		Inf.Height.Max = Inf.Height.Min = 10;
	}

	return true;
}

GMessage::Result GProgress::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_CREATE:
		{
			PostMessage(Handle(), PBM_SETRANGE32, Low>>Shift, High>>Shift);
			PostMessage(Handle(), PBM_SETPOS, (WPARAM) Val>>Shift, 0);

			if (GetCss())
			{
				LCss::ColorDef f = GetCss()->Color();
				if (f.Type == LCss::ColorRgb)
				{
					PostMessage(_View, PBM_SETBARCOLOR, 0, Rgb32To24(f.Rgb32));
				}
				
				f = GetCss()->BackgroundColor();
				if (f.Type == LCss::ColorRgb)
				{
					PostMessage(_View, PBM_SETBKCOLOR, 0, Rgb32To24(f.Rgb32));
				}
			}
			break;
		}
	}

	return GControl::OnEvent(Msg);
}

GString GProgress::CssStyles(const char *CssStyle)
{
	GString Status = GControl::CssStyles(CssStyle);
	if (Status && GetCss())
	{
		LCss::ColorDef f = GetCss()->Color();
		if (f.Type == LCss::ColorRgb)
			SendMessage(_View, PBM_SETBARCOLOR, 0, Rgb32To24(f.Rgb32));
		else
			SendMessage(_View, PBM_SETBARCOLOR, 0, CLR_DEFAULT);
		
		f = GetCss()->BackgroundColor();
		if (f.Type == LCss::ColorRgb)
			SendMessage(_View, PBM_SETBKCOLOR, 0, Rgb32To24(f.Rgb32));
		else
			SendMessage(_View, PBM_SETBKCOLOR, 0, CLR_DEFAULT);
	}
	else
	{
		SendMessage(_View, PBM_SETBARCOLOR, 0, CLR_DEFAULT);
		SendMessage(_View, PBM_SETBKCOLOR, 0, CLR_DEFAULT);
	}
	
	return Status;
}

