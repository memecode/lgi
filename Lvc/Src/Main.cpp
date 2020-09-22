#include "Lgi.h"
#include "../Resources/resdefs.h"
#include "Lvc.h"
#include "GTableLayout.h"
#ifdef WINDOWS
#include "../Resources/resource.h"
#endif
#include "GTextLog.h"
#include "GButton.h"
#include "GXmlTreeUi.h"
#include "GTree.h"

//////////////////////////////////////////////////////////////////
enum SshMsgs
{
	M_DETECT_VCS = M_USER + 100,
	M_RUN_CMD,
	M_RESPONSE,
};

SshConnection::SshConnection(GTextLog *log, const char *uri, const char *prompt) : LSsh(log), GEventTargetThread("SshConnection")
{
	GuiHnd = log->GetWindow()->AddDispatch();
	Prompt = prompt;
	Host.Set(Uri = uri);
}

bool SshConnection::DetectVcs(VcFolder *Fld)
{
	GAutoPtr<GString> p(new GString(Fld->GetUri().sPath(1, -1)));
	TypeNotify.Add(Fld);
	return PostObject(GetHandle(), M_DETECT_VCS, p);
}

bool SshConnection::Command(VcFolder *Fld, GString Exe, GString Args, ParseFn Parser, ParseParams *Params)
{
	if (!Fld || !Exe || !Parser)
		return false;

	GAutoPtr<SshParams> p(new SshParams(this));
	p->f = Fld;
	p->Exe = Exe;
	p->Args = Args;
	p->Parser = Parser;
	p->Params = Params;
	p->Path = Fld->GetUri().sPath(1, -1);

	return PostObject(GetHandle(), M_RUN_CMD, p);
}

GStream *SshConnection::GetConsole()
{
	if (!Connected)
	{
		auto r = Open(Host.sHost, Host.sUser, Host.sPass, true);
		Log->Print("Ssh: %s open: %i\n", Host.sHost.Get(), r);
	}
	if (Connected && !c)
	{
		c = CreateConsole();
		WaitPrompt(c);
	}
	return c;
}

bool SshConnection::WaitPrompt(GStream *con, GString *Data)
{
	char buf[512];
	GString out;
	while (!IsCancelled())
	{
		auto rd = con->Read(buf, sizeof(buf));
		if (rd < 0)
			return false;

		if (rd == 0)
		{
			LgiSleep(10);
			continue;
		}
				
		out += GString(buf, rd);
		DeEscape(out);
		auto lines = out.SplitDelimit("\n");
		auto last = lines.Last();
		if (last.Find(Prompt) >= 0)
		{
			if (Data)
			{
				lines.DeleteAt(0, true);
				lines.PopLast();
				*Data = GString("\n").Join(lines);
			}
			break;
		}
	}

	return true;
}

bool SshConnection::HandleMsg(GMessage *m)
{
	if (m->Msg() != M_RESPONSE)
		return false;

	GAutoPtr<SshParams> u((SshParams*)m->A());
	if (!u || !u->c)
		return false;

	SshConnection &c = *u->c;
	if (u->Vcs) // Check the VCS type..
	{
		c.Types.Add(u->Path, u->Vcs);
		for (auto f: c.TypeNotify)
			f->OnVcsType();
	}
	else if (u->Output) // Cmd output
	{
		LgiAssert(u->f != NULL);
		u->f->OnSshCmd(u);
	}
	else return false; // ?

	return true;
}

GMessage::Result SshConnection::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_DETECT_VCS:
		{
			GAutoPtr<GString> p;
			if (!ReceiveA(p, Msg))
				break;

			GStream *con = GetConsole();
			if (!con)
				break;

			GString ls, out;
			ls.Printf("find \"%s\" -maxdepth 1 -printf \"%%f\n\"\n", p->Get());
			auto wr = con->Write(ls, ls.Length());
			auto pr = WaitPrompt(con, &out);
			auto lines = out.SplitDelimit("\r\n");

			VersionCtrl Vcs = VcNone;
			for (auto ln: lines)
			{
				if (ln.Equals(".svn"))
					Vcs = VcSvn;
				else if (ln.Equals("CVS"))
					Vcs = VcCvs;
				else if (ln.Equals(".hg"))
					Vcs = VcHg;
				else if (ln.Equals(".git"))
					Vcs = VcGit;
			}
			if (Vcs)
			{
				GAutoPtr<SshParams> r(new SshParams(this));
				r->Path = *p;
				r->Vcs = Vcs;
				PostObject(GuiHnd, M_RESPONSE, r);
			}
			break;
		}
		case M_RUN_CMD:
		{
			GAutoPtr<SshParams> p;
			if (!ReceiveA(p, Msg))
				break;

			GStream *con = GetConsole();
			if (!con)
				break;

			GString cmd;
			cmd.Printf("( cd \"%s\" && %s %s )\n", p->Path.Get(), p->Exe.Get(), p->Args.Get());
			auto wr = con->Write(cmd, cmd.Length());
			auto pr = WaitPrompt(con, &p->Output);

			GString result;
			cmd = "echo $?\n";
			wr = con->Write(cmd, cmd.Length());
			pr = WaitPrompt(con, &result);
			if (pr)
				p->ExitCode = (int)result.Int();

			PostObject(GuiHnd, M_RESPONSE, p);
			break;
		}
		default:
		{
			LgiAssert(!"Unhandled msg.");
			break;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////
const char *AppName =			"Lvc";
#define DEFAULT_BUILD_FIX_MSG	"Build fix."
#define OPT_Hosts				"Hosts"
#define OPT_Host				"Host"

VersionCtrl AppPriv::DetectVcs(VcFolder *Fld)
{
	char p[MAX_PATH];
	GUri u = Fld->GetUri();

	if (!u.IsFile() || !u.sPath)
	{
		auto c = GetConnection(u.ToString());
		if (!c)
			return VcNone;
		
		auto type = c->Types.Find(u.sPath);
		if (type)
			return type;

		c->DetectVcs(Fld);
		return VcPending;
	}

	auto Path = u.sPath.Get();
	if (*Path == '/')
		Path++;

	if (LgiMakePath(p, sizeof(p), Path, ".git") &&
		DirExists(p))
		return VcGit;

	if (LgiMakePath(p, sizeof(p), Path, ".svn") &&
		DirExists(p))
		return VcSvn;

	if (LgiMakePath(p, sizeof(p), Path, ".hg") &&
		DirExists(p))
		return VcHg;

	if (LgiMakePath(p, sizeof(p), Path, "CVS") &&
		DirExists(p))
		return VcCvs;

	return VcNone;
}


class DiffView : public GTextLog
{
public:
	DiffView(int id) : GTextLog(id)
	{
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		for (auto ln : GTextView3::Line)
		{
			if (!ln->c.IsValid())
			{
				char16 *t = Text + ln->Start;
				
				if (*t == '+')
				{
					ln->c = GColour::Green;
					ln->Back.Rgb(245, 255, 245);
				}
				else if (*t == '-')
				{
					ln->c = GColour::Red;
					ln->Back.Rgb(255, 245, 245);
				}
				else if (*t == '@')
				{
					ln->c.Rgb(128, 128, 128);
					ln->Back.Rgb(235, 235, 235);
				}
				else
					ln->c = LColour(L_TEXT);
			}
		}
	}
};

class ToolBar : public GLayout, public GLgiRes
{
public:
	ToolBar()
	{
		GAutoString Name;
		GRect Pos;
		if (LoadFromResource(IDD_TOOLBAR, this, &Pos, &Name))
		{
			OnPosChange();
		}
		else LgiAssert(!"Missing toolbar resource");
	}

	void OnCreate()
	{
		AttachChildren();
	}

	void OnPosChange()
	{
		GRect Cli = GetClient();

		GTableLayout *v;
		if (GetViewById(IDC_TABLE, v))
		{
			v->SetPos(Cli);
				
			v->OnPosChange();
			auto r = v->GetUsedArea();
			if (r.Y() <= 1)
				r.Set(0, 0, 30, 30);
			
			// printf("Used = %s\n", r.GetStr());
			GCss::Len NewSz(GCss::LenPx, (float)r.Y()+3);
			auto OldSz = GetCss(true)->Height();
			if (OldSz != NewSz)
			{
				GetCss(true)->Height(NewSz);
				SendNotify(GNotifyTableLayout_Refresh);
			}
		}
		else LgiAssert(!"Missing table ctrl");
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LColour(L_MED));
		pDC->Rectangle();
	}
};

class CommitCtrls : public GLayout, public GLgiRes
{
public:
	CommitCtrls()
	{
		GAutoString Name;
		GRect Pos;
		if (LoadFromResource(IDD_COMMIT, this, &Pos, &Name))
		{
			GTableLayout *v;
			if (GetViewById(IDC_COMMIT_TABLE, v))
			{
				v->GetCss(true)->PaddingRight("8px");
				GRect r = v->GetPos();
				r.Offset(-r.x1, -r.y1);
				r.x2++;
				v->SetPos(r);
				
				v->OnPosChange();
				r = v->GetUsedArea();
				if (r.Y() <= 1)
					r.Set(0, 0, 30, 30);
				GetCss(true)->Height(GCss::Len(GCss::LenPx, (float)r.Y()));
			}
			else LgiAssert(!"Missing table ctrl");
		}
		else LgiAssert(!"Missing toolbar resource");
	}

	void OnPosChange()
	{
		GTableLayout *v;
		if (GetViewById(IDC_COMMIT_TABLE, v))
			v->SetPos(GetClient());
	}

	void OnCreate()
	{
		AttachChildren();
	}
};

GString::Array GetProgramsInPath(const char *Program)
{
	GString::Array Bin;
	GString Prog = Program;
	#ifdef WINDOWS
	Prog += LGI_EXECUTABLE_EXT;
	#endif

	GString::Array a = LGetPath();
	for (auto p : a)
	{
		GFile::Path c(p, Prog);
		if (c.Exists())
			Bin.New() = c.GetFull();
	}

	return Bin;
}

class OptionsDlg : public GDialog, public GXmlTreeUi
{
	GOptionsFile &Opts;

public:
	OptionsDlg(GViewI *Parent, GOptionsFile &opts) : Opts(opts)
	{
		SetParent(Parent);
		Map("svn-path", IDC_SVN, GV_STRING);
		Map("git-path", IDC_GIT, GV_STRING);
		Map("hg-path", IDC_HG, GV_STRING);
		Map("cvs-path", IDC_CVS, GV_STRING);

		if (LoadFromResource(IDD_OPTIONS))
		{
			MoveSameScreen(Parent);
			Convert(&Opts, this, true);
		}
	}

	void Browse(int EditId)
	{
		GFileSelect s;
		s.Parent(this);
		if (s.Open())
		{
			SetCtrlName(EditId, s.Name());
		}
	}

	void BrowseFiles(GViewI *Ctrl, const char *Bin, int EditId)
	{
		GRect Pos = Ctrl->GetPos();
		GdcPt2 Pt(Pos.x1, Pos.y2 + 1);
		PointToScreen(Pt);
		
		LSubMenu s;

		GString::Array Bins = GetProgramsInPath(Bin);
		for (unsigned i=0; i<Bins.Length(); i++)
		{
			s.AppendItem(Bins[i], 1000+i);
		}

		if (Bins.Length() == 0)
		{
			Browse(EditId);
		}
		else
		{
			s.AppendSeparator();
			s.AppendItem("Browse...", 1);
			int Cmd = s.Float(this, Pt.x, Pt.y, LSubMenu::BtnLeft);
			switch (Cmd)
			{
				case 1:
					Browse(EditId);
					break;
				default:
					if (Cmd >= 1000)
					{
						GString Bin = Bins[Cmd - 1000];
						if (Bin)
							SetCtrlName(EditId, Bin);
					}
					break;
			}
		}
	}

	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDC_SVN_BROWSE:
				BrowseFiles(Ctrl, "svn", IDC_SVN);
				break;
			case IDC_GIT_BROWSE:
				BrowseFiles(Ctrl, "git", IDC_GIT);
				break;
			case IDC_HG_BROWSE:
				BrowseFiles(Ctrl, "hg", IDC_HG);
				break;
			case IDC_CVS_BROWSE:
				BrowseFiles(Ctrl, "cvs", IDC_CVS);
				break;
			case IDOK:
				Convert(&Opts, this, false);
				// fall
			case IDCANCEL:
			{
				EndModal(Ctrl->GetId() == IDOK);
				break;
			}
		}

		return GDialog::OnNotify(Ctrl, Flags);
	}
};

int CommitDataCmp(VcCommit **_a, VcCommit **_b)
{
	auto a = *_a;
	auto b = *_b;
	return a->GetTs().Compare(&b->GetTs());
}

GString::Array AppPriv::GetCommitRange()
{
	GString::Array r;

	if (Commits)
	{
		GArray<VcCommit*> Sel;
		Commits->GetSelection(Sel);
		if (Sel.Length() > 1)
		{
			Sel.Sort(CommitDataCmp);
			r.Add(Sel[0]->GetRev());
			r.Add(Sel.Last()->GetRev());
		}
		else
		{
			r.Add(Sel[0]->GetRev());
		}
	}
	else LgiAssert(!"No commit list ptr");

	return r;
}

GArray<VcCommit*> AppPriv::GetRevs(GString::Array &Revs)
{
	GArray<VcCommit*> a;
	
	for (auto i = Commits->begin(); i != Commits->end(); i++)
	{
		VcCommit *c = dynamic_cast<VcCommit*>(*i);
		if (c)
		{
			for (auto r: Revs)
			{
				if (r.Equals(c->GetRev()))
				{
					a.Add(c);
					break;
				}
			}
		}
	}

	return a;
}

class CommitList : public LList
{
public:
	CommitList(int id) : LList(id, 0, 0, 200, 200)
	{
	}

	void SelectRevisions(GString::Array &Revs, const char *BranchHint = NULL)
	{
		VcCommit *Scroll = NULL;
		GArray<VcCommit*> Matches;
		for (auto i: *this)
		{
			VcCommit *item = dynamic_cast<VcCommit*>(i);
			if (item)
			{
				for (auto r: Revs)
				{
					if (item->IsRev(r))
						Matches.Add(item);
				}
			}
		}

		for (auto item: Matches)
		{
			auto b = item->GetBranch();
			if (BranchHint)
			{
				if (!b || Stricmp(b, BranchHint))
					continue;
			}
			else if (b)
			{
				continue;
			}

			if (!Scroll)
				Scroll = item;
			item->Select(true);
		}
		if (!Scroll && Matches.Length() > 0)
		{
			Scroll = Matches[0];
			Scroll->Select(true);
		}

		if (Scroll)
			Scroll->ScrollTo();
	}

	bool OnKey(GKey &k)
	{
		switch (k.c16)
		{
			case 'p':
			case 'P':
			{
				if (k.Down())
				{
					GArray<VcCommit*> Sel;
					GetSelection(Sel);
					if (Sel.Length())
					{
						auto first = Sel[0];
						auto branch = first->GetBranch();
						auto p = first->GetParents();
						if (p->Length() == 0)
							break;

						for (auto c:Sel)
							c->Select(false);

						SelectRevisions(*p, branch);
					}
				}
				return true;
			}
			case 'c':
			case 'C':
			{
				if (k.Down())
				{
					GArray<VcCommit*> Sel;
					GetSelection(Sel);
					if (Sel.Length())
					{
						LHashTbl<StrKey<char>,VcCommit*> Map;
						for (auto s:Sel)
							Map.Add(s->GetRev(), s);
						
						GString::Array n;
						for (auto it = begin(); it != end(); it++)
						{
							VcCommit *c = dynamic_cast<VcCommit*>(*it);
							if (c)
							{
								for (auto r:*c->GetParents())
								{
									if (Map.Find(r))
									{
										n.Add(c->GetRev());
										break;
									}
								}
							}
						}

						for (auto c:Sel)
							c->Select(false);

						SelectRevisions(n, Sel[0]->GetBranch());
					}
				}
				return true;
			}
		}

		return LList::OnKey(k);
	}
};

int LstCmp(LListItem *a, LListItem *b, int Col)
{
	VcCommit *A = dynamic_cast<VcCommit*>(a);
	VcCommit *B = dynamic_cast<VcCommit*>(b);

	if (A == NULL || B == NULL)
	{
		return (A ? 1 : -1) - (B ? 1 : -1);
	}

	auto f = A->GetFolder();
	auto flds = f->GetFields();
	if (!flds.Length())
	{
		LgiTrace("%s:%i - No fields?\n", _FL);
		return 0;
	}
		
	auto fld = flds[Col];
	switch (fld)
	{
		case LGraph:
		case LIndex:
		case LParents:
		case LRevision:
		default:
			return (int) (B->GetIndex() - A->GetIndex());

		case LBranch:
		case LAuthor:
		case LMessage:
			return Stricmp(A->GetFieldText(fld), B->GetFieldText(fld));

		case LTimeStamp:
			return B->GetTs().Compare(&A->GetTs());
	}

	return 0;
}

struct TestThread : public LThread
{
public:
    TestThread() : LThread("test")
    {
        Run();
    }
    
    int Main()
    {
        auto Path = LGetPath();
        GSubProcess p("python", "/Users/matthew/CodeLib/test.py");
        
        auto t = GString(LGI_PATH_SEPARATOR).Join(Path);
        for (auto s: Path)
            printf("s: %s\n", s.Get());
        p.SetEnvironment("PATH", t);
        if (p.Start())
        {
            GStringPipe s;
            p.Communicate(&s);
            printf("Test: %s\n", s.NewGStr().Get());
        }
        return 0;
    }
};

class RemoteFolderDlg : public GDialog
{
	class App *app;
	GTree *tree;
	struct SshHost *root, *newhost;
	GXmlTreeUi Ui;

public:
	GString Uri;

	RemoteFolderDlg(App *application);
	~RemoteFolderDlg();

	int OnNotify(GViewI *Ctrl, int Flags);
};

class App : public GWindow, public AppPriv
{
	GAutoPtr<GImageList> ImgLst;
	GBox *FoldersBox;

public:
	App()
	{
		FoldersBox = NULL;
		
		GString AppRev;
		AppRev.Printf("%s v%s", AppName, APP_VERSION);
		Name(AppRev);

		GRect r(0, 0, 1400, 800);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);

		Opts.SerializeFile(false);
		SerializeState(&Opts, "WndPos", true);

		#ifdef WINDOWS
		SetIcon(MAKEINTRESOURCEA(IDI_ICON1));
		#else
		SetIcon("icon32.png");
		#endif

		ImgLst.Reset(LgiLoadImageList("image-list.png", 16, 16));

		if (Attach(0))
		{
			if ((Menu = new LMenu))
			{
				Menu->SetPrefAndAboutItems(IDM_OPTIONS, IDM_ABOUT);
				Menu->Attach(this);
				Menu->Load(this, "IDM_MENU");
			}

			GBox *ToolsBox = new GBox(IDC_TOOLS_BOX, true, "ToolsBox");
			FoldersBox = new GBox(IDC_FOLDERS_BOX, false, "FoldersBox");
			GBox *CommitsBox = new GBox(IDC_COMMITS_BOX, true, "CommitsBox");

			ToolBar *Tools = new ToolBar;

			ToolsBox->Attach(this);
			Tools->Attach(ToolsBox);
			FoldersBox->Attach(ToolsBox);

			Tree = new GTree(IDC_TREE, 0, 0, 320, 200);
			// Tree->GetCss(true)->Width(GCss::Len("320px"));
			Tree->ShowColumnHeader(true);
			Tree->AddColumn("Folder", 250);
			Tree->AddColumn("Counts", 50);
			Tree->Attach(FoldersBox);
			Tree->SetImageList(ImgLst, false);
			CommitsBox->Attach(FoldersBox);

			Commits = new CommitList(IDC_LIST);
			Commits->Attach(CommitsBox);
			Commits->GetCss(true)->Height("40%");

			GBox *FilesBox = new GBox(IDC_FILES_BOX, false);
			FilesBox->Attach(CommitsBox);

			Files = new LList(IDC_FILES, 0, 0, 200, 200);
			Files->GetCss(true)->Width("35%");
			Files->Attach(FilesBox);
			Files->AddColumn("[ ]", 30);
			Files->AddColumn("State", 100);
			Files->AddColumn("Name", 400);

			GBox *MsgBox = new GBox(IDC_MSG_BOX, true);
			MsgBox->Attach(FilesBox);
			
			CommitCtrls *Commit = new CommitCtrls;
			Commit->Attach(MsgBox);
			Commit->GetCss(true)->Height("25%");
			if (Commit->GetViewById(IDC_MSG, Msg))
			{
				GTextView3 *Tv = dynamic_cast<GTextView3*>(Msg);
				if (Tv)
				{
					Tv->Sunken(true);
					Tv->SetWrapType(TEXTED_WRAP_NONE);
				}
			}
			else LgiAssert(!"No ctrl?");

			Tabs = new GTabView(IDC_TAB_VIEW);
			Tabs->Attach(MsgBox);
			const char *Style = "Padding: 0px 8px 8px 0px";
			Tabs->GetCss(true)->Parse(Style);

			GTabPage *p = Tabs->Append("Diff");
			p->Append(Diff = new DiffView(IDC_TXT));
			// Diff->Sunken(true);
			Diff->SetWrapType(TEXTED_WRAP_NONE);

			p = Tabs->Append("Log");
			p->Append(Log = new GTextLog(IDC_LOG));
			// Log->Sunken(true);
			Log->SetWrapType(TEXTED_WRAP_NONE);
			
			SetCtrlValue(IDC_UPDATE, true);

			AttachChildren();
			Visible(true);
		}

		GXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (!f)
		{
			Opts.CreateTag(OPT_Folders);
			f = Opts.LockTag(OPT_Folders, _FL);
		}
		if (f)
		{
			bool Req[VcMax] = {0};
			
			for (auto c: f->Children)
			{
				if (c->IsTag(OPT_Folder))
				{
					auto f = new VcFolder(this, c);
				
					Tree->Insert(f);
					
					if (!Req[f->GetType()])
					{
						Req[f->GetType()] = true;
						f->GetVersion();
					}
				}
			}
			Opts.Unlock();
			
			GRect Large(0, 0, 2000, 200);
			Tree->SetPos(Large);
			Tree->ResizeColumnsToContent();
			
			GItemColumn *c;
			int i = 0, px = 0;
			while ((c = Tree->ColumnAt(i++)))
			{
				px += c->Width();
			}
			
			FoldersBox->Value(MAX(320, px + 20));
            
            // new TestThread();
		}
		
		SetPulse(200);
		DropTarget(true);
	}

	~App()
	{
		SerializeState(&Opts, "WndPos", false);

		GXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (f)
		{
			f->EmptyChildren();

			for (auto i:*Tree)
			{
				VcFolder *vcf = dynamic_cast<VcFolder*>(i);
				if (vcf)
					f->InsertTag(vcf->Save());
			}
			Opts.Unlock();
		}
		Opts.SerializeFile(true);
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		if (Msg->Msg() == M_RESPONSE)
			SshConnection::HandleMsg(Msg);
		return GWindow::OnEvent(Msg);
	}

	void OnReceiveFiles(GArray<const char*> &Files)
	{
		for (auto f : Files)
		{
			if (DirExists(f))
				OpenLocalFolder(f);
		}
	}

	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDM_OPEN_LOCAL:
			{
				OpenLocalFolder();
				break;
			}
			case IDM_OPEN_REMOTE:
			{
				OpenRemoteFolder();
				break;
			}
			case IDM_OPTIONS:
			{
				OptionsDlg Dlg(this, Opts);
				Dlg.DoModal();
				break;
			}
			case IDM_FIND:
			{
				GInput i(this, "", "Search string:");
				if (i.DoModal())
				{
					GString::Array Revs;
					Revs.Add(i.GetStr());

					CommitList *cl;
					if (GetViewById(IDC_LIST, cl))
						cl->SelectRevisions(Revs);
				}
				break;
			}
			case IDM_UNTRACKED:
			{
				auto mi = GetMenu()->FindItem(IDM_UNTRACKED);
				if (!mi)
					break;

				mi->Checked(!mi->Checked());

				GArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f : Flds)
				{
					f->Refresh();
				}
				break;
			}
			case IDM_REFRESH:
			{
				GArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->Refresh();
				break;
			}
			case IDM_PULL:
			{
				GArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->Pull();
				break;
			}
			case IDM_PUSH:
			{
				GArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->Push();
				break;
			}
			case IDM_STATUS:
			{
				GArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->FolderStatus();
				break;
			}
			case IDM_UPDATE_SUBS:
			{
				GArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->UpdateSubs();
				break;
				break;
			}
			case IDM_EXIT:
			{
				LgiCloseApp();
				break;
			}
		}

		return 0;
	}

	void OnPulse()
	{
		for (auto i:*Tree)
		{
			VcFolder *vcf = dynamic_cast<VcFolder*>(i);
			if (vcf)
				vcf->OnPulse();
		}
	}

	void OpenLocalFolder(const char *Fld = NULL)
	{
		GFileSelect s;

		if (!Fld)
		{
			s.Parent(this);
			if (s.OpenFolder())
				Fld = s.Name();
		}

		if (Fld)
		{
			// Check the folder isn't already loaded...
			bool Has = false;
			GArray<VcFolder*> Folders;
			Tree->GetAll(Folders);
			for (auto f: Folders)
			{
				if (!Stricmp(f->LocalPath(), Fld))
				{
					Has = true;
					break;
				}
			}

			if (!Has)
			{
				GUri u;
				u.SetFile(Fld);
				Tree->Insert(new VcFolder(this, u.ToString()));
			}
		}
	}

	void OpenRemoteFolder()
	{
		RemoteFolderDlg dlg(this);
		if (!dlg.DoModal())
			return;

		Tree->Insert(new VcFolder(this, dlg.Uri));
	}

	int OnNotify(GViewI *c, int flag)
	{
		switch (c->GetId())
		{
			case IDC_FILES:
			{
				switch (flag)
				{
					case GNotifyItem_ColumnClicked:
					{
						int Col = -1;
						GMouse m;
						if (Files->GetColumnClickInfo(Col, m))
						{
							if (Col == 0)
							{
								// Select / deselect all checkboxes..
								List<VcFile> n;
								if (Files->GetAll(n))
								{
									bool Checked = false;
									for (auto f: n)
										Checked |= f->Checked() > 0;
									for (auto f: n)
										f->Checked(Checked ? 0 : 1);
								}
							}
						}
						break;
					}
				}
				break;
			}
			case IDC_OPEN:
			{
				OpenLocalFolder();
				break;
			}
			case IDC_TREE:
			{
				switch (flag)
				{
					case GNotifyContainer_Click:
					{
						GMouse m;
						c->GetMouse(m);
						if (m.Right())
						{
							LSubMenu s;
							s.AppendItem("Add Local", IDM_ADD_LOCAL);
							s.AppendItem("Add Remote", IDM_ADD_REMOTE);
							int Cmd = s.Float(c->GetGView(), m);
							switch (Cmd)
							{
								case IDM_ADD_LOCAL:
								{
									OpenLocalFolder();
									break;
								}
								case IDM_ADD_REMOTE:
								{
									OpenRemoteFolder();
									break;
								}
							}
						}
						break;
					}
					case LvcCommandStart:
					{
						SetCtrlEnabled(IDC_PUSH, false);
						SetCtrlEnabled(IDC_PULL, false);
						SetCtrlEnabled(IDC_PULL_ALL, false);
						break;
					}
					case LvcCommandEnd:
					{
						SetCtrlEnabled(IDC_PUSH, true);
						SetCtrlEnabled(IDC_PULL, true);
						SetCtrlEnabled(IDC_PULL_ALL, true);
						break;
					}
				}
				break;
			}
			case IDC_CLEAR_FILTER:
			{
				SetCtrlName(IDC_FILTER, NULL);
				// Fall through
			}
			case IDC_FILTER:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Select(true);
				break;
			}
			case IDC_COMMIT_AND_PUSH:
			case IDC_COMMIT:
			{
				auto BuildFix = GetCtrlValue(IDC_BUILD_FIX);
				const char *Msg = GetCtrlName(IDC_MSG);
				if (BuildFix || ValidStr(Msg))
				{
					auto Sel = Tree->Selection();
					if (Sel)
					{
						VcFolder *f = dynamic_cast<VcFolder*>(Sel);
						if (!f)
						{
							for (auto p = Sel->GetParent(); p; p = p->GetParent())
							{
								f = dynamic_cast<VcFolder*>(p);
								if (f)
									break;
							}
						}
						if (f)
						{
							auto Branch = GetCtrlName(IDC_BRANCH);
							bool AndPush = c->GetId() == IDC_COMMIT_AND_PUSH;
							f->Commit(BuildFix ? DEFAULT_BUILD_FIX_MSG : Msg, ValidStr(Branch) ? Branch : NULL, AndPush);
						}
					}
				}
				else LgiMsg(this, "No message for commit.", AppName);
				break;
			}
			case IDC_PUSH:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Push();
				break;
			}
			case IDC_PULL:
			{
				VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
				if (f)
					f->Pull();
				break;
			}
			case IDC_PULL_ALL:
			{
				GArray<VcFolder*> Folders;
				Tree->GetAll(Folders);
				bool AndUpdate = GetCtrlValue(IDC_UPDATE) != 0;
				
				for (auto f : Folders)
				{
					f->Pull(AndUpdate, LogSilo);
				}
				break;
			}
			case IDC_STATUS:
			{
				GArray<VcFolder*> Folders;
				Tree->GetAll(Folders);
				for (auto f : Folders)
				{
					f->FolderStatus();
				}
				break;
			}
			case IDC_HEADS:
			{
				if (flag == GNotifyValueChanged)
				{
					auto Revs = GString(c->Name()).SplitDelimit();

					CommitList *cl;
					if (GetViewById(IDC_LIST, cl))
						cl->SelectRevisions(Revs);
				}
				break;
			}
			case IDC_LIST:
			{
				switch (flag)
				{
					case GNotifyItem_ColumnClicked:
					{
						int Col = -1;
						GMouse Ms;
						Commits->GetColumnClickInfo(Col, Ms);
						Commits->Sort(LstCmp, Col);
						break;
					}
					case GNotifyItem_DoubleClick:
					{
						VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
						if (!f)
							break;

						GArray<VcCommit*> s;
						if (Commits->GetSelection(s) && s.Length() == 1)
							f->OnUpdate(s[0]->GetRev());
						break;
					}
				}
				break;
			}
		}

		return 0;
	}
};

struct SshHost : public GTreeItem
{
	GXmlTag *t;
	GString Host, User, Pass;

	SshHost(GXmlTag *tag = NULL)
	{
		t = tag;
		if (t)
		{
			Serialize(false);
			SetText(Host);
		}
	}

	void Serialize(bool WriteToTag)
	{
		if (WriteToTag)
		{
			GUri u;
			u.sProtocol = "ssh";
			u.sHost = Host;
			u.sUser = User;
			u.sPass = Pass;
			t->SetContent(u.ToString());
		}
		else
		{
			GUri u(t->GetContent());
			if (!Stricmp(u.sProtocol.Get(), "ssh"))
			{
				Host = u.sHost;
				User = u.sUser;
				Pass = u.sPass;
			}
		}
	}
};

RemoteFolderDlg::RemoteFolderDlg(App *application) : app(application), root(NULL), newhost(NULL)
{
	SetParent(app);
	LoadFromResource(IDD_REMOTE_FOLDER);

	if (GetViewById(IDC_HOSTS, tree))
	{
		tree->Insert(root = new SshHost());
		root->SetText("Ssh Hosts");
	}
	else return;

	GViewI *v;
	if (GetViewById(IDC_HOSTNAME, v))
		v->Focus(true);

	Ui.Map("Host", IDC_HOSTNAME);
	Ui.Map("User", IDC_USER);
	Ui.Map("Password", IDC_PASS);

	GXmlTag *hosts = app->Opts.LockTag(OPT_Hosts, _FL);
	if (hosts)
	{
		SshHost *h;
		for (auto c: hosts->Children)
			if (c->IsTag(OPT_Host) && (h = new SshHost(c)))
				root->Insert(h);
		app->Opts.Unlock();
	}

	root->Insert(newhost = new SshHost());
	newhost->SetText("New Host");
	root->Expanded(true);
	newhost->Select(true);
}

RemoteFolderDlg::~RemoteFolderDlg()
{
}

int RemoteFolderDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	SshHost *cur = dynamic_cast<SshHost*>(tree->Selection());

	#define CHECK_SPECIAL() \
		if (cur == newhost) \
		{ \
			root->Insert(cur = new SshHost()); \
			cur->Select(true); \
		} \
		if (cur == root) \
			break;

	switch (Ctrl->GetId())
	{
		case IDC_HOSTS:
		{
			switch (Flags)
			{
				case GNotifyItem_Select:
				{
					bool isRoot = cur == root;
					SetCtrlEnabled(IDC_HOSTNAME, !isRoot);
					SetCtrlEnabled(IDC_USER, !isRoot);
					SetCtrlEnabled(IDC_PASS, !isRoot);
					SetCtrlEnabled(IDC_DELETE, !isRoot && !(cur == newhost));
					
					SetCtrlName(IDC_HOSTNAME, cur ? cur->Host : NULL);
					SetCtrlName(IDC_USER, cur ? cur->User : NULL);
					SetCtrlName(IDC_PASS, cur ? cur->Pass : NULL);
					break;
				}
			}			
			break;
		}
		case IDC_HOSTNAME:
		{
			CHECK_SPECIAL()
			if (cur)
			{
				cur->Host = Ctrl->Name();
				cur->SetText(cur->Host ? cur->Host : "<empty>");
			}
			break;
		}
		case IDC_USER:
		{
			CHECK_SPECIAL()
			if (cur)
				cur->User = Ctrl->Name();
			break;
		}
		case IDC_PASS:
		{
			CHECK_SPECIAL()
			if (cur)
				cur->Pass = Ctrl->Name();
			break;
		}
		case IDOK:
		{
			GXmlTag *hosts;
			if (!(hosts = app->Opts.LockTag(OPT_Hosts, _FL)))
			{
				if (!(app->Opts.CreateTag(OPT_Hosts) && (hosts = app->Opts.LockTag(OPT_Hosts, _FL))))
					break;
			}
			LgiAssert(hosts != NULL);
			
			for (auto i = root->GetChild(); i; i = i->GetNext())
			{
				SshHost *h = dynamic_cast<SshHost*>(i);
				if (!h || h == newhost)
					continue;

				if (h->t)
					;
				else if ((h->t = new GXmlTag(OPT_Host)))
					hosts->InsertTag(cur->t);
				else
					return false;

				h->Serialize(true);
			}

			app->Opts.Unlock();

			GUri u;
			u.sProtocol = "ssh";
			u.sHost = GetCtrlName(IDC_HOSTNAME);
			u.sUser = GetCtrlName(IDC_USER);
			u.sPass = GetCtrlName(IDC_PASS);
			u.sPath = GetCtrlName(IDC_REMOTE_PATH);
			Uri = u.ToString();

			// Fall through
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId() == IDOK);
			break;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	LgiAssert(VcCommit::Instances == 0);
	return 0;
}

