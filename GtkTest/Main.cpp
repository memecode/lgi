#include "lgi/common/Lgi.h"
#include "lgi/common/Button.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Combo.h"

#define IDM_EXIT 4

class Test : public LDialog
{
public:
    Test(LViewI *p)
    {
        SetParent(p);
        SetPos(LRect(0, 0, 300, 300));
        MoveToCenter();
        
		GCombo *c;
		AddView(c = new GCombo(100, 10, 40, 100, 20, ""));
		c->Insert("One");
		c->Insert("Two");
		c->Insert("Three");

		AddView(new LButton(50, 10, 10, -1, -1, "Open"));

        /*
		AddView(c = new GCombo(200, 10, 140, 100, 20, ""));
		c->Insert("One");
		c->Insert("Two");
		c->Insert("Three");
		*/

        DoModal();
    }
    
	int OnNotify(LViewI *c, int f)
	{
		switch (c->GetId())
		{
		    case 50:
		    {
		        Test Dlg(this);
		        break;
		    }
		}
		return 0;
	}
};

class App : public LWindow
{
    LImageList *i;
    
public:
    App()
    {
        i = LLoadImageList("cmds.png", 16, 16);
        Name("Gtk Test");
        SetQuitOnClose(true);
        LRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        if (Attach(0))
        {
            /*
            #if 1
            Menu = new GMenu();
            Menu->Attach(this);
            GSubMenu *s = Menu->AppendSub("File");
            s->AppendItem("Open", 1, true);
            s->AppendItem("Save", 2, true);
            s->AppendItem("Close", 3, true);
            s->AppendSeparator();
            s->AppendItem("Exit", IDM_EXIT, true);
            s = Menu->AppendSub("Edit");
            s->AppendItem("Copy", 11, true);
            s->AppendItem("Paste", 12, true);
            s = Menu->AppendSub("Tools");
            s->AppendItem("Options", 21, true);
            s = Menu->AppendSub("Help");
            s->AppendItem("Help", 31, true);
            s->AppendItem("About", 31, true);
            #endif

			LToolBar *t = new LToolBar;
            t->SetImageList(i, 16, 16);
            t->AppendButton("New", 1);
            t->AppendButton("Open", 2);
            t->AppendButton("Save", 3);
            AddView(t);

            #if 1
            LSplitter *split = new LSplitter();
            split->Raised(true);
            split->IsVertical(true);
            split->Border(true);
            split->Value(200);
            AddView(split);
            #endif
			*/
			
			AddView(new LButton(50, 10, 10, -1, -1, "Open"));

		    GCombo *c;
		    AddView(c = new GCombo(100, 10, 40, 100, 20, ""));
		    c->Insert("One");
		    c->Insert("Two");
		    c->Insert("Three");


            AttachChildren();
            // LgiMsg(this, "Test", "App");
            Visible(true);
        }
    }

	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDM_EXIT:
				LgiCloseApp();
				break;
		}
		return 0;
	}
	
	int OnNotify(LViewI *c, int f)
	{
		switch (c->GetId())
		{
		    case 50:
		    {
		        Test Dlg(this);
		        break;
		    }
		}
		return 0;
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
    LApp a("application/x-gtk-test", AppArgs);
    if (a.IsOk())
    {
        // a.SkinEngine = CreateSkinEngine(&a);

        a.AppWnd = new App;
        a.Run();
    }    
    return 0;
}
