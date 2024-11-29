/*
**	FILE:			FtpApp.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			7/9/99
**	DESCRIPTION:	i.Ftp application
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

// Debug defines

// Includes
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/FileTransferProgress.h"
#include "lgi/common/Slider.h"
#include "lgi/common/Variant.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"

//////////////////////////////////////////////////////////////////
#define THROTTLE_TEXT_WIDTH		80
#define SLIDER_ID				100

enum ProgressMessages
{
    IDM_SET_LIMITS   = M_USER + 100,
    IDM_SET_START_VAL,
};

int PipeSize[] =
{
    29491,
    56 << 10,
    64 << 10,
    128 << 10,
    256 << 10,
    512 << 10,
    1500 << 10,
    8 << 20,
    22 << 20,
    0
};

class LPaneThrottle : public LStatusPane
{
	LSlider *Slider;
	LDom *App;
	int Pipe;

	void Set()
	{
		LRect r(0, 0, THROTTLE_TEXT_WIDTH, Y()-1);
		LView::Invalidate(&r);

		if (App)
		{
			/*
			App->SetDataRate(	(Value() == 100) ? -1 :
								(Value() * (PipeSize[Pipe]/8)) / 100 );
			*/

			LVariant v;
			App->SetValue(OPT_Throttle, v = Value());
			App->SetValue(OPT_PipeSize, v = Pipe);
		}
	}

public:
	LPaneThrottle(LDom *app);

	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
	void OnPaint(LSurface *pDC);
	int64 Value();
	void OnMouseClick(LMouse &m);
};

LPaneThrottle::LPaneThrottle(LDom *app)
{
	Pipe = -1;
	App = app;
	SetWidth(80+THROTTLE_TEXT_WIDTH);
	Slider = new LSlider(SLIDER_ID, 0, 0, 100, 20, "Throttle", false);
}

int LPaneThrottle::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	if (Ctrl && Ctrl->GetId() == SLIDER_ID)
	{
		Set();
	}

	return 0;
}

void LPaneThrottle::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);

	int x, y;
	pDC->GetOrigin(x, y);

	LThinBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(L_MED);
	pDC->Rectangle(r.x1, r.y1, r.x1+THROTTLE_TEXT_WIDTH, r.y2);

	if (Slider)
	{
		LRect Sr = r;
		#ifdef WIN32
		Sr.Offset(GetPos().x1, GetPos().y1);
		#endif
		Sr.x1 += THROTTLE_TEXT_WIDTH+1;
		Slider->SetPos(Sr);

		if (!Slider->IsAttached())
		{
			#ifdef WIN32
			Slider->Attach(GetParent());
			#else
			Slider->Attach(this);
			#endif
			Slider->SetNotify(this);
			Slider->SetRange(100);
			Slider->Value(100);

			if (Pipe < 0)
			{
				LVariant v;
				if (App->GetValue(OPT_PipeSize, v))
					Pipe = v.CastInt32();
				if (Slider)
				{
					v = 100;
					if (App->GetValue(OPT_Throttle, v))
						Slider->Value(v.CastInt32());
				}
			}
			
			// DumpHnd(HIViewGetRoot(GetWindow()->WindowHandle()));
		}
		
		char Str[256];
		int64 v = Value();
		if (v < 100)
		{
			if (PipeSize[Pipe] < (1 << 20))
			{
				sprintf_s(Str, sizeof(Str), "%i%% of %i Kbps", (int)v, PipeSize[Pipe]>>10);
			}
			else
			{
				sprintf_s(Str, sizeof(Str), "%i%% of %.1f Mbps", (int)v, (double)(PipeSize[Pipe]>>20));
			}
		}
		else
		{
			strcpy_s(Str, sizeof(Str), "No limit");
		}
		
		LSysFont->Colour(LColour(L_TEXT), LColour(L_MED));
		LDisplayString ds(LSysFont, Str);
		ds.Draw(pDC, r.x1+2, r.y1);
	}
}

int64 LPaneThrottle::Value()
{
	return (Slider) ? Slider->Value() : 100;
}

void LPaneThrottle::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		auto RClick = new LSubMenu;
		if (RClick)
		{
			for (int i=0; PipeSize[i]; i++)
			{
				char Str[256];
				if (PipeSize[i] < (1 << 20))
				{
					sprintf_s(Str, sizeof(Str), "%i Kbps", PipeSize[i]>>10);
				}
				else
				{
					sprintf_s(Str, sizeof(Str), "%.1f Mbps", (double)PipeSize[i] / 1024.0 / 1024.0);
				}
				auto Item = RClick->AppendItem(Str, 100+i, true);
				if (Item && i == Pipe)
				{
					Item->Checked(true);
				}
			}

			if (GetMouse(m, true))
			{
				int Cmd = RClick->Float(this, m.x, m.y, m.Left());
				if (Cmd >= 100)
				{
					Pipe = Cmd - 100;
					Set();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define HISTORY_TEXT_WIDTH		70
#define MAX_SAMPLE				70
#define HISTORY_DIV				1000

class LPaneHistory : public LStatusPane
{
	LDom *App;
	int64 Cur; // Current value
	int64 Max; // Max value
	uint64 Last; // Last time value changed
	int64 Bytes[MAX_SAMPLE];

	LSurface *pMemDC;

	void Push()
	{
		for (int i=MAX_SAMPLE-1; i>0; i--)
		{
			Bytes[i] = Bytes[i-1];
		}
		Bytes[0] = 0;
	}

	void Clear()
	{
		for (int i=0; i<MAX_SAMPLE; i++)
		{
			Bytes[i] = 0;
		}
	}

public:
	LPaneHistory(LDom *app);
	~LPaneHistory();

	void OnPaint(LSurface *pDC);
	void Value(int64 i);
};

LPaneHistory::LPaneHistory(LDom *app)
{
	SetWidth(HISTORY_TEXT_WIDTH + MAX_SAMPLE + 2);
	App = app;
	pMemDC = 0;
	Cur = 0;
	Max = 0;
	Last = 0;
	Clear();
}

LPaneHistory::~LPaneHistory()
{
	DeleteObj(pMemDC);
}

void LPaneHistory::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);

	LThinBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(L_MED);
	pDC->Rectangle(r.x1, r.y1, r.x1+HISTORY_TEXT_WIDTH, r.y2);

	char Str[256];
	sprintf_s(Str, sizeof(Str), "%.1f K/s", Max / 1024.0);
	LSysFont->Colour(LColour(L_TEXT), LColour(L_MED));
	LDisplayString ds(LSysFont, Str);
	ds.Draw(pDC, r.x1+2, r.y1);

	r.x1 += HISTORY_TEXT_WIDTH+1;

	if (!pMemDC ||
		(r.X() != pMemDC->X()))
	{
		DeleteObj(pMemDC);
		pMemDC = new LMemDC(_FL);
		if (pMemDC)
		{
			if (pMemDC->Create(r.X(), r.Y(), GdcD->GetColourSpace()))
			{
				pMemDC->Colour(Rgb24(0, 0, 0), 24);
				pMemDC->Rectangle();
			}
		}
	}

	if (pMemDC)
	{
		pDC->Blt(r.x1, r.y1, pMemDC);
	}
	else
	{
		pDC->Colour(L_LOW);
		pDC->Rectangle(&r);
	}
}

void LPaneHistory::Value(int64 i)
{
	if (i <= 0)
	{
		Max = 0;
		Last = LCurrentTime();
		Cur = -i;
		Clear();
	}
	else
	{
		uint64 Now = LCurrentTime();
		int64 NewData = i - Cur;
		int64 Diff = Now - Last;

		if (Last > 0)
		{
			double Sec = (double)Diff / 1000.0;
			double Rate = (double) NewData / Sec;
			
			// do any fractional second from Last -> next second
			if ((Last / 1000) != (Now / 1000))
			{
				int Ms = 1000 - (Last % 1000);
				double LastSec = (double)Ms / 1000.0;
				int Len = (int)(Rate * LastSec);

				Bytes[0] += Len;
				NewData -= Len;
				Diff -= Ms;
				
				Max = MAX(Bytes[0], Max);
				Push();
			}

			// do any whole seconds...
			while (Diff > 1000)
			{
				Bytes[0] = (int)Rate;
				NewData -= Bytes[0];
				Diff -= 1000;

				Max = MAX(Bytes[0], Max);
				Push();
			}

			// Any last partial second left to process.
			Bytes[0] += NewData;
			Max = MAX(Bytes[0], Max);
			Last = Now;
		}
		else
		{
			Bytes[0] += NewData;
		}
		Cur = i;

		// draw
		if (pMemDC && Max > 0)
		{
			pMemDC->Colour(Rgb24(0, 0, 0), 24);
			pMemDC->Rectangle();
			pMemDC->Colour(Rgb24(0, 255, 0), 24);

			int64 m=0;
			for (int x=0; x<pMemDC->X() && x<MAX_SAMPLE; x++)
			{
				m = MAX(Bytes[x], m);
				int64 Sample = (Bytes[x] * pMemDC->Y()) / Max;
				pMemDC->Line(x, pMemDC->Y()-(int)Sample, x, pMemDC->Y());
			}

			if (m < Max)
			{
				Max = m;
			}
		}
	}

	LView::Invalidate();
}

//////////////////////////////////////////////////////////////////////////////////////
FileTransferProgress::FileTransferProgress(	LDom *App,
											LStatusBar *Status,
											bool Limit)
{
	StartTime = StartPos = 0;
	ProgressPane = 0;
	DspVal = 0;
	SetWidth(70);

	// Download throttle
	StatusInfo[_STATUS_THROTTLE]	= (Limit) ? new LPaneThrottle(App) : 0;
	if (StatusInfo[_STATUS_THROTTLE]) Status->AppendPane(StatusInfo[_STATUS_THROTTLE]);
	
	// Historical graph
	StatusInfo[_STATUS_HISTORY]		= new LPaneHistory(App);
	if (StatusInfo[_STATUS_HISTORY]) Status->AppendPane(StatusInfo[_STATUS_HISTORY]);

	// current download position
	StatusInfo[_STATUS_POSITION]	= Status->AppendPane("", 130);
	if (StatusInfo[_STATUS_POSITION]) StatusInfo[_STATUS_POSITION]->Sunken(true);
	
	// progress meter
	StatusInfo[_STATUS_PROGRESS]	= ProgressPane = new LProgressStatusPane;
	if (StatusInfo[_STATUS_PROGRESS]) Status->AppendPane(ProgressPane);
	
	// download rate ('this' will be the rate, as we need a window handle to post events to)
	Status->AppendPane(this);
	StatusInfo[_STATUS_RATE]		= this;
	if (StatusInfo[_STATUS_RATE]) StatusInfo[_STATUS_RATE]->Sunken(true);
	
	// estimated time left
	StatusInfo[_STATUS_TIME_LEFT]	= Status->AppendPane("", 70);
	if (StatusInfo[_STATUS_TIME_LEFT]) StatusInfo[_STATUS_TIME_LEFT]->Sunken(true);
	
	Status->AttachChildren();
}

void FileTransferProgress::OnCreate()
{
	SetPulse(500);
}

void FileTransferProgress::OnPulse()
{
	if (DspVal != Val)
		UpdateUi();
}

LMessage::Result FileTransferProgress::OnEvent(LMessage *m)
{
    switch (m->Msg())
    {
        case IDM_SET_LIMITS:
        {			
            SetRange(LRange(m->A(), m->B()));
            break;
        }
        case IDM_SET_START_VAL:
        {
			LVariant v = m->A();
			SetVariant(sStartValue, v);
            break;
        }
    }

    return LStatusPane::OnEvent(m);
}

bool FileTransferProgress::SetRange(const LRange &r)
{
    if (!InThread())
    {
        bool Status = PostEvent(IDM_SET_LIMITS, (LMessage::Param)r.Start, (LMessage::Param)r.Len);
        LAssert(Status);
    }
    else
    {
    	Progress::SetRange(r);
    	if (ProgressPane)
    		ProgressPane->SetRange(r);
    }

	return true;
}

void FileTransferProgress::Value(int64 v)
{
	if (High <= 0)
		return;

	if (Val == 0)
		StartTime = LCurrentTime();

	Progress::Value(v);
}

void FileTransferProgress::UpdateUi()
{
	if (DspVal == Val)
		return;

	uint64 Now = LCurrentTime();
	// LgiTrace("Update UI %i, %i  InThread()=%i\n", (int)Val, (int)DspVal, InThread());
	bool Start = Val == 0;
	
	// Tell everyone about the new value
	if (ProgressPane) ProgressPane->Value(Val);
	if (StatusInfo[_STATUS_HISTORY]) StatusInfo[_STATUS_HISTORY]->Value((Start) ? -Val : Val);
		
	if (Val == 0)
	{
		StatusInfo[_STATUS_POSITION]->Name("");
		StatusInfo[_STATUS_TIME_LEFT]->Name("");
		StatusInfo[_STATUS_RATE]->Name("");
	}
	else if (High > 0)
	{
		if (StatusInfo[_STATUS_POSITION])
		{
			char a[64], b[64], Str[128];
			LFormatSize(a, sizeof(a), Val);
			LFormatSize(b, sizeof(b), High);
			sprintf_s(Str, sizeof(Str), "%s of %s", a, b);
			StatusInfo[_STATUS_POSITION]->Name(Str);
		}

		StatusInfo[_STATUS_PROGRESS]->Value(Val);

		double Rate = 0.0;
		double Seconds = ((double)Now-(double)StartTime)/1000;
		if (StatusInfo[_STATUS_RATE] && Seconds > 0.0)
		{
			char Str[256];
			Rate = ((double)(Val-StartPos))/Seconds;

			sprintf_s(Str, sizeof(Str), "%.2f K/s", Rate/1024.0);
			StatusInfo[_STATUS_RATE]->Name(Str);
		}

		if (StatusInfo[_STATUS_TIME_LEFT] && Rate > 0.0)
		{
			char Str[256];
			double Time = ((double) (High-StartPos) / Rate) - Seconds + 0.5;
			sprintf_s(Str, sizeof(Str), "%i:%2.2i:%2.2i", (int)(Time/3600), ((int)(Time/60))%60, ((int)Time)%60);
			StatusInfo[_STATUS_TIME_LEFT]->Name(Str);
		}
	}
	else
	{
		if (StatusInfo[_STATUS_POSITION])
		{
			char Str[256];
			sprintf_s(Str, sizeof(Str), LPrintfInt64 " K", Val>>10);
			StatusInfo[_STATUS_POSITION]->Name(Str);
		}
	}

	DspVal = Val;
}

bool FileTransferProgress::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (Stricmp(Name, sStartValue))
		return false;

	if (!InThread())
	{
		bool Status = PostEvent(IDM_SET_START_VAL, (LMessage::Param)Value.CastInt32());
		LAssert(Status);
	}
	else
	{
		Progress::Value(StartPos = Value.CastInt32());
		if (StatusInfo[_STATUS_HISTORY]) StatusInfo[_STATUS_HISTORY]->Value(-Value.CastInt32());
	}

	return true;
}
