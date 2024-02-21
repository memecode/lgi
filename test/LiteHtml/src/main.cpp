#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/Edit.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Layout.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Menu.h"
#include "lgi/common/LiteHtmlView.h"
#include "lgi/common/Uri.h"

const char *AppName = "LgiLiteHtml";

enum Ctrls
{
	IDC_BOX = 100,
	IDC_LOCATION,
	IDC_BROWSER,
};

class AppLiteHtmlView : public LiteHtmlView
{
public:
	AppLiteHtmlView(int id) : LiteHtmlView(id)
	{

	}

	void OnNavigate(LString url) override
	{
		GetWindow()->SetCtrlName(IDC_LOCATION, url);
		LiteHtmlView::OnNavigate(url);
	}
};

class App : public LWindow
{
	LBox *box = NULL;
	LEdit *location = NULL;
	LiteHtmlView *browser = NULL;

public:
	App()
	{
		LRect r(200, 200, 1400, 1000);
		SetPos(r);
		Name(AppName);
		MoveToCenter();
		SetQuitOnClose(true);

		if (Attach(0))
		{
			AddView(box = new LBox(IDC_BOX, true));
			box->AddView(location = new LEdit(IDC_LOCATION, 0, 0, 100, 20));
			box->AddView(browser = new AppLiteHtmlView(IDC_BROWSER));
			box->Value(LSysFont->GetHeight() + 8);
			AttachChildren();
			location->Focus(true);
			Visible(true);
		}
	}

	void SetUrl(LString s)
	{
		if (location)
			location->Name(s);
		if (browser)
			browser->SetUrl(s);
	}

	void OnReceiveFiles(LArray<const char *> &Files)
	{
		SetUrl(Files[0]);
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	LApp app(AppArgs, "application/x-lgi-litehtml");
	if (app.IsOk())
	{
		auto result = LUri::UnitTests();

		App *a = new App;
		app.AppWnd = a;

		#ifdef WINDOWS
		LString cmdLine(AppArgs.lpCmdLine);
		#else
		LString cmdLine;
		if (AppArgs.Args > 1)
			cmdLine = AppArgs.Arg[1];
		#endif
		LUri u(cmdLine);
		if (u.IsProtocol("http") ||
			u.IsProtocol("https"))
			a->SetUrl(cmdLine);

		app.Run();
	}
	return 0;
}
