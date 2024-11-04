#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/About.h"
#include "lgi/common/DocView.h"
#include "lgi/common/Bitmap.h"
#include "lgi/common/Button.h"
#include "lgi/common/Css.h"
#include "lgi/common/TableLayout.h"

//////////////////////////////////////////////////////////////////////////////
enum Ctrls
{
	IDC_WEB = 100,
	IDC_TABLE,
	IDC_BMP,
	IDC_MESSAGE,
};

LAbout::LAbout(	LView *parent,
				const char *AppName,
				const char *Ver,
				const char *Text,
				const char *AboutGraphic,
				const char *Url,
				const char *Email)
{
	SetParent(parent);
	if (!AppName) AppName = "Application";

	LString n;
	n.Printf("About %s", AppName);
	Name(n);

	#ifdef _DEBUG
	const char *Build = "Debug";
	#else
	const char *Build = "Release";
	#endif

	LStringPipe p;
	const char *OsName = LGetOsName();
	#if defined(_WIN64)
	OsName = "Win64";
	#elif defined(WIN32)
	OsName = "Win32";
	#endif	
	
	p.Print("%s v%s (%s %s)\n", AppName, Ver, OsName, Build);
	p.Print("Build: %s, %s\n", __DATE__, __TIME__);
	#ifdef __GTK_H__
	p.Print("GTK v%i.%i.%i\n", GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
	#endif
	p.Print("\n");

	if (Url) p.Print("Homepage:\n\t%s\n", Url);
	if (Email) p.Print("Email:\n\t%s\n", Email);
	if (Text) p.Write((char*)Text, strlen(Text));

	LColour cBack(L_MED);
	LTableLayout *Tbl = new LTableLayout(IDC_TABLE);
	AddView(Tbl);
	Tbl->GetCss(true)->Padding("0.5em");
	int x = 0;

	if (AboutGraphic)
	{
		LString FileName = LFindFile(AboutGraphic);
		if (FileName)
		{
			auto c = Tbl->GetCell(x++, 0, true);
			auto Img = new LBitmap(IDC_BMP, 0, 0, FileName, true);
			c->Add(Img);
			Img->GetCss(true)->BackgroundColor(cBack);
		}
	}

	LView *Ctrl = LViewFactory::Create("LTextView3");
	if (Ctrl)
	{
		auto c = Tbl->GetCell(x++, 0, true);
		c->Add(Ctrl);

		Ctrl->SetId(IDC_MESSAGE);
		Ctrl->Name(p.NewLStr());
		Ctrl->GetCss(true)->BackgroundColor(cBack);
		Ctrl->SetFont(LSysFont);
	}

	auto c = Tbl->GetCell(0, 1, true, x);
	c->TextAlign(LCss::AlignRight);
	c->Add(new LButton(IDOK, 0, 0, -1, -1, "Ok"));

	LRect r(0, 0, 400, 260);
	SetPos(r);
	MoveSameScreen(parent);
}

int LAbout::OnNotify(LViewI *Ctrl, LNotification &n)
{
	if (!Ctrl) return 0;
	
	switch (Ctrl->GetId())
	{
		case IDC_BMP:
		{
			break;
		}
		case IDOK:
		{
			EndModal(0);
			break;
		}
	}
	return 0;
}

