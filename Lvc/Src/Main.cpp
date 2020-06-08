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


//////////////////////////////////////////////////////////////////
const char *AppName =			"Lvc";
#define DEFAULT_BUILD_FIX_MSG	"Build fix."

VersionCtrl DetectVcs(const char *Path)
{
	char p[MAX_PATH];

	if (!Path)
		return VcNone;

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

GArray<VcCommit*> AppPriv::GetRevs(GString::Array &Revs)
{
	GArray<VcCommit*> a;
	
	for (auto i = Lst->begin(); i != Lst->end(); i++)
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

			Lst = new CommitList(IDC_LIST);
			Lst->Attach(CommitsBox);
			Lst->GetCss(true)->Height("40%");

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

	void OnReceiveFiles(GArray<char*> &Files)
	{
		for (auto f : Files)
		{
			if (DirExists(f))
				OpenFolder(f);
		}
	}

	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
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

	void OpenFolder(const char *Fld = NULL)
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
			if (DetectVcs(Fld))
			{
				// Check the folder isn't already loaded...
				bool Has = false;
				GArray<VcFolder*> Folders;
				Tree->GetAll(Folders);
				for (auto f: Folders)
				{
					if (!Stricmp(f->GetPath(), Fld))
					{
						Has = true;
						break;
					}
				}

				if (!Has)
					Tree->Insert(new VcFolder(this, Fld));
			}
			else
				LgiMsg(this, "Folder not under version control.", AppName);
		}
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
				OpenFolder();
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
							s.AppendItem("Add", IDM_ADD);
							int Cmd = s.Float(c->GetGView(), m);
							switch (Cmd)
							{
								case IDM_ADD:
								{
									OpenFolder();
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
						Lst->GetColumnClickInfo(Col, Ms);
						Lst->Sort(LstCmp, Col);
						break;
					}
					case GNotifyItem_DoubleClick:
					{
						VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
						if (!f)
							break;

						GArray<VcCommit*> s;
						if (Lst->GetSelection(s) && s.Length() == 1)
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

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

