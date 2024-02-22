#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/Edit.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Layout.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Menu.h"
#include "lgi/common/LiteHtmlView.h"
#include "lgi/common/Uri.h"
#include "lgi/common/Button.h"

const char *AppName = "LgiLiteHtml";

enum Ctrls
{
	IDC_BOX = 100,
	IDC_CTRLS,
	IDC_LOCATION,
	IDC_BROWSER,
	IDC_BACK,
	IDC_FORWARD,
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

	void OnHistory(bool hasBack, bool hasForward)
	{
		auto wnd = GetWindow();
		wnd->SetCtrlEnabled(IDC_BACK, hasBack);
		wnd->SetCtrlEnabled(IDC_FORWARD, hasForward);
	}
};

class App : public LWindow
{
	LBox *box = NULL;
	LBox *ctrls = NULL;
	LEdit *location = NULL;
	LiteHtmlView *browser = NULL;
	LButton *back = NULL;
	LButton *forward = NULL;

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
			box->AddView(ctrls = new LBox(IDC_CTRLS, false));
			ctrls->AddView(back = new LButton(IDC_BACK, 0, 0, -1, -1, "<"));
			back->GetCss(true)->Width("2em");
			back->Enabled(false);
			ctrls->AddView(forward = new LButton(IDC_FORWARD, 0, 0, -1, -1, ">"));
			forward->GetCss(true)->Width("2em");
			forward->Enabled(false);
			ctrls->AddView(location = new LEdit(IDC_LOCATION, 0, 0, 100, 20));
			
			box->AddView(browser = new AppLiteHtmlView(IDC_BROWSER));
			box->Value(LSysFont->GetHeight() + 8);
			
			AttachChildren();
			location->Focus(true);
			Visible(true);
		}
	}

	void SetUrl(LString s)
	{
		if (browser)
			browser->SetUrl(s);
	}

	void OnReceiveFiles(LArray<const char *> &Files)
	{
		SetUrl(Files[0]);
	}

	int OnNotify(LViewI *Ctrl, LNotification n)
	{
		switch (Ctrl->GetId())
		{
			case IDC_BACK:
				browser->HistoryBack();
				break;
			case IDC_FORWARD:
				browser->HistoryForward();
				break;
			case IDC_LOCATION:
				if (n.Type == LNotifyReturnKey)
					browser->SetUrl(Ctrl->Name());
				break;
		}

		return LWindow::OnNotify(Ctrl, n);
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
			a->SetUrl(cmdLine.Strip());

		app.Run();
	}
	return 0;
}
