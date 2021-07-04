// Lgi.cpp
#include "lgi/common/Lgi.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/TableLayout.h"

//////////////////////////////////////////////////////////////////////////////
#define CMD_BASE		10000
#ifdef LGI_SDL
#define BTN_SCALE		2.0f
#else
#define BTN_SCALE		1.0f
#endif

LAlert::LAlert(	LViewI *parent,
				const char *Title,
				const char *Text,
				const char *Btn1,
				const char *Btn2,
				const char *Btn3)
{
	GArray<const char*> Names;
	if (Btn1) Names.Add(Btn1);
	if (Btn2) Names.Add(Btn2);
	if (Btn3) Names.Add(Btn3);

	// Setup dialog
	SetParent(parent);
	Name((char*)Title);

	LTableLayout *Tbl = new LTableLayout(100);
	Tbl->GetCss(true)->Padding("10px");
	AddView(Tbl);

	GLayoutCell *c = Tbl->GetCell(0, 0, true);
	c->Add(new LTextLabel(-1, 8, 8, -1, -1, Text));
	c->PaddingBottom(LCss::Len("10px"));

	c = Tbl->GetCell(0, 1, true);
	for (unsigned i=0; i<Names.Length(); i++)
	{
		c->TextAlign(LCss::Len(LCss::AlignCenter));
		c->Add(new LButton(CMD_BASE + i,
							0, 0, -1, -1,
							Names[i]));
	}
		
	LRect r(0, 0, 1000, 1000);
	Tbl->SetPos(r);
	r = Tbl->GetUsedArea();
	r.Size(-10, -10);
	int x = LgiApp->GetMetric(LGI_MET_DECOR_X);
	int y = LgiApp->GetMetric(LGI_MET_DECOR_Y) +
			LgiApp->GetMetric(LGI_MET_DECOR_CAPTION);
	r.x2 += x + 20;
	r.y2 += y;

	SetPos(r);
	if (parent && parent->GetPos().Valid())
		MoveSameScreen(parent);
	else
		MoveToCenter();
}

void LAlert::SetAppModal()
{
    #if WINNATIVE
    SetExStyle(GetExStyle() | WS_EX_TOPMOST);
	#elif defined(LGI_CARBON)
	if (Handle())
	{
		OSStatus e = HIWindowChangeClass(WindowHandle(), kMovableModalWindowClass);
		if (e)
			printf("%s:%i - Error: HIWindowChangeClass=%i\n", _FL, (int)e);
	}
    #endif
}

int LAlert::OnNotify(LViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case CMD_BASE+0:
		case CMD_BASE+1:
		case CMD_BASE+2:
		{
			if (Flags != GNotifyTableLayout_LayoutChanged)
			{
				EndModal(Ctrl->GetId() - CMD_BASE + 1);
			}
			break;
		}
	}

	return 0;
}
