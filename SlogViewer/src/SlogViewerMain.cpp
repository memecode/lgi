#include "lgi/common/Lgi.h"
#include "lgi/common/OptionsFile.h"
#include "lgi/common/DocApp.h"
#include "lgi/common/Box.h"
#include "lgi/common/List.h"
#include "lgi/common/TextView3.h"
#include "resdefs.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "SlogViewer";

enum Ctrls 
{
	IDC_BOX = 100,
	IDC_LOG,
	IDC_DETAIL,
};

class App : public LDocApp<LOptionsFile>
{
	LBox *box = NULL;
	LList *log = NULL;
	LTextView3 *detail = NULL;

public:
    App() : LDocApp<LOptionsFile>(AppName)
    {
        Name(AppName);
        LRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (_Create())
        {
			_LoadMenu();

			AddView(box = new LBox(IDC_BOX));
			box->AddView(log = new LList(IDC_LOG, 0, 0, 200, 200));
			log->GetCss(true)->Width("40%");
			log->ShowColumnHeader(false);
			log->AddColumn("Items", 1000);
			box->AddView(detail = new LTextView3(IDC_DETAIL));
			detail->Sunken(true);

			AttachChildren();
            Visible(true);
        }
    }

	void OnReceiveFiles(LArray<const char*> &Files)
	{
		if (Files.Length())
			OpenFile(Files[0]);
	}	

	bool OpenFile(const char *FileName, bool ReadOnly = false)
	{
		return false;
	}

	bool SaveFile(const char *FileName)
	{
		return false;
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

