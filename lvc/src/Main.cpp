#include "lgi/common/Lgi.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/Button.h"
#include "lgi/common/XmlTreeUi.h"
#include "lgi/common/Tree.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/StructuredLog.h"
#include "lgi/common/PopupNotification.h"

#include "Lvc.h"
#include "resdefs.h"
#ifdef WINDOWS
#include "resource.h"
#endif

//////////////////////////////////////////////////////////////////
const char *AppName =			"Lvc";
#define DEFAULT_BUILD_FIX_MSG	"Build fix."
#define OPT_Hosts				"Hosts"
#define OPT_Host				"Host"

AppPriv::~AppPriv()
{
	if (CurFolder)
		CurFolder->Empty();
}
	
#if HAS_LIBSSH
SshConnection *AppPriv::GetConnection(const char *Uri, bool Create)
{
	LUri u(Uri);
	u.sPath.Empty();
	auto s = u.ToString();
	auto Conn = Connections.Find(s);
	if (!Conn && Create)
		Connections.Add(s, Conn = new SshConnection(Log, s, "matthew@*$ "));
	return Conn;
}
#endif

VersionCtrl AppPriv::DetectVcs(VcFolder *Fld)
{
	char p[MAX_PATH_LEN];
	LUri u = Fld->GetUri();

	if (!u.IsFile() || !u.sPath)
	{
		#if HAS_LIBSSH
			auto c = GetConnection(u.ToString());
			if (!c)
				return VcError;
			
			auto type = c->Types.Find(u.sPath);
			if (type)
				return type;

			c->DetectVcs(Fld);
			Fld->GetCss(true)->Color(LColour::Blue);
			Fld->Update();
			return VcPending;
		#else
			return VcError;
		#endif
	}

	auto Path = u.sPath.Get();
	#ifdef WINDOWS
		if (*Path == '/')
			Path++;
	#endif

	if (LMakePath(p, sizeof(p), Path, ".git") &&
		LDirExists(p))
		return VcGit;

	if (LMakePath(p, sizeof(p), Path, ".svn") &&
		LDirExists(p))
		return VcSvn;

	if (LMakePath(p, sizeof(p), Path, ".hg") &&
		LDirExists(p))
		return VcHg;

	if (LMakePath(p, sizeof(p), Path, "CVS") &&
		LDirExists(p))
		return VcCvs;

	return VcNone;
}


class DiffView : public LTextLog
{
public:
	DiffView(int id) : LTextLog(id)
	{
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		for (auto ln : LTextView3::Line)
		{
			if (!ln->c.IsValid())
			{
				char16 *t = Text + ln->Start;
				
				if (*t == '+')
				{
					ln->c = LColour::Green;
					ln->Back.Rgb(245, 255, 245);
				}
				else if (*t == '-')
				{
					ln->c = LColour::Red;
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

class ToolBar : public LLayout, public LResourceLoad
{
public:
	ToolBar()
	{
		LString Name;
		LRect Pos;
		if (LoadFromResource(IDD_TOOLBAR, this, &Pos, &Name))
		{
			OnPosChange();
		}
		else LAssert(!"Missing toolbar resource");
	}

	void OnCreate()
	{
		AttachChildren();
	}

	void OnPosChange()
	{
		LRect Cli = GetClient();

		LTableLayout *v;
		if (GetViewById(IDC_TABLE, v))
		{
			v->SetPos(Cli);
				
			v->OnPosChange();
			auto r = v->GetUsedArea();
			if (r.Y() <= 1)
				r.Set(0, 0, 30, 30);
			
			// printf("Used = %s\n", r.GetStr());
			LCss::Len NewSz(LCss::LenPx, (float)r.Y()+3);
			auto OldSz = GetCss(true)->Height();
			if (OldSz != NewSz)
			{
				GetCss(true)->Height(NewSz);
				SendNotify(LNotifyTableLayoutRefresh);
			}
		}
		else LAssert(!"Missing table ctrl");
	}

	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(LColour(L_MED));
		pDC->Rectangle();
	}
};

class CommitCtrls : public LLayout, public LResourceLoad
{
public:
	CommitCtrls()
	{
		LString Name;
		LRect Pos;
		if (LoadFromResource(IDD_COMMIT, this, &Pos, &Name))
		{
			LTableLayout *v;
			if (GetViewById(IDC_COMMIT_TABLE, v))
			{
				v->GetCss(true)->PaddingRight("8px");
				LRect r = v->GetPos();
				r.Offset(-r.x1, -r.y1);
				r.x2++;
				v->SetPos(r);
				
				v->OnPosChange();
				r = v->GetUsedArea();
				if (r.Y() <= 1)
					r.Set(0, 0, 30, 30);
				GetCss(true)->Height(LCss::Len(LCss::LenPx, (float)r.Y()));
			}
			else LAssert(!"Missing table ctrl");
		}
		else LAssert(!"Missing toolbar resource");
	}

	void OnPosChange()
	{
		LTableLayout *v;
		if (GetViewById(IDC_COMMIT_TABLE, v))
			v->SetPos(GetClient());
	}

	void OnCreate()
	{
		AttachChildren();
	}
};

LString::Array GetProgramsInPath(const char *Program)
{
	LString::Array Bin;
	LString Prog = Program;
	#ifdef WINDOWS
	Prog += LGI_EXECUTABLE_EXT;
	#endif

	LString::Array a = LGetPath();
	for (auto p : a)
	{
		LFile::Path c(p, Prog);
		if (c.Exists())
			Bin.New() = c.GetFull();
	}

	return Bin;
}

class OptionsDlg : public LDialog, public LXmlTreeUi
{
	LOptionsFile &Opts;

public:
	OptionsDlg(LViewI *Parent, LOptionsFile &opts) : Opts(opts)
	{
		SetParent(Parent);

		Map("svn-path", IDC_SVN, GV_STRING);
		Map("svn-limit", IDC_SVN_LIMIT);

		Map("git-path", IDC_GIT, GV_STRING);
		Map("git-limit", IDC_GIT_LIMIT);

		Map("hg-path", IDC_HG, GV_STRING);
		Map("hg-limit", IDC_HG_LIMIT);

		Map("cvs-path", IDC_CVS, GV_STRING);
		Map("cvs-limit", IDC_CVS_LIMIT);

		if (LoadFromResource(IDD_OPTIONS))
		{
			MoveSameScreen(Parent);
			Convert(&Opts, this, true);
		}
	}

	void Browse(int EditId)
	{
		auto s = new LFileSelect;
		s->Parent(this);
		s->Open([this, EditId](auto s, auto status)
		{
			if (status)
				SetCtrlName(EditId, s->Name());
			delete s;
		});
	}

	void BrowseFiles(LViewI *Ctrl, const char *Bin, int EditId)
	{
		LRect Pos = Ctrl->GetPos();
		LPoint Pt(Pos.x1, Pos.y2 + 1);
		PointToScreen(Pt);
		
		LSubMenu s;

		LString::Array Bins = GetProgramsInPath(Bin);
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
						LString Bin = Bins[Cmd - 1000];
						if (Bin)
							SetCtrlName(EditId, Bin);
					}
					break;
			}
		}
	}

	int OnNotify(LViewI *Ctrl, LNotification n)
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

		return LDialog::OnNotify(Ctrl, n);
	}
};

int CommitDataCmp(VcCommit **_a, VcCommit **_b)
{
	auto a = *_a;
	auto b = *_b;
	return a->GetTs().Compare(&b->GetTs());
}

LString::Array AppPriv::GetCommitRange()
{
	LString::Array r;

	if (Commits)
	{
		LArray<VcCommit*> Sel;
		Commits->GetSelection(Sel);
		if (Sel.Length() > 1)
		{
			Sel.Sort(CommitDataCmp);
			r.Add(Sel[0]->GetRev());
			r.Add(Sel.Last()->GetRev());
		}
		else if (Sel.Length() == 1)
		{
			r.Add(Sel[0]->GetRev());
		}
	}
	else LAssert(!"No commit list ptr");

	return r;
}

LArray<VcCommit*> AppPriv::GetRevs(LString::Array &Revs)
{
	LArray<VcCommit*> a;
	
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

	void SelectRevisions(LString::Array &Revs, const char *BranchHint = NULL)
	{
		VcCommit *Scroll = NULL;
		LArray<VcCommit*> Matches;
		for (auto i: *this)
		{
			VcCommit *item = dynamic_cast<VcCommit*>(i);
			if (!item)
				continue;

			bool IsMatch = false;
			for (auto r: Revs)
			{
				if (item->IsRev(r))
				{
					IsMatch = true;
					break;
				}
			}
			if (IsMatch)
				Matches.Add(item);
			else if (i->Select())
				i->Select(false);
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

	bool OnKey(LKey &k)
	{
		switch (k.c16)
		{
			case 'p':
			case 'P':
			{
				if (k.Down())
				{
					LArray<VcCommit*> Sel;
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
					LArray<VcCommit*> Sel;
					GetSelection(Sel);
					if (Sel.Length())
					{
						LHashTbl<StrKey<char>,VcCommit*> Map;
						for (auto s:Sel)
							Map.Add(s->GetRev(), s);
						
						LString::Array n;
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
		case LMessageTxt:
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
        LSubProcess p("python", "/Users/matthew/CodeLib/test.py");
        
        auto t = LString(LGI_PATH_SEPARATOR).Join(Path);
        for (auto s: Path)
            printf("s: %s\n", s.Get());
        p.SetEnvironment("PATH", t);
        if (p.Start())
        {
            LStringPipe s;
            p.Communicate(&s);
            printf("Test: %s\n", s.NewLStr().Get());
        }
        return 0;
    }
};

class RemoteFolderDlg : public LDialog
{
	class App *app;
	LTree *tree;
	struct SshHost *root, *newhost;
	LXmlTreeUi Ui;

public:
	LString Uri;

	RemoteFolderDlg(App *application);
	~RemoteFolderDlg();

	int OnNotify(LViewI *Ctrl, LNotification n);
};

class VcDiffFile : public LTreeItem
{
	AppPriv *d;
	LString File;
	uint64 modTime = 0;
	
public:
	VcDiffFile(AppPriv *priv, LString file) : d(priv), File(file)
	{
	}
	
	const char *GetText(int i = 0) override
	{
		return i ? NULL : File.Get();
	}
	
	LString StripFirst(LString s)
	{
		return s.Replace("\\","/").SplitDelimit("/", 1).Last();
	}
	
	void Reload()
	{
		Select(true);
	}
	
	void OnPulse()
	{
		LDirectory dir;
		if (dir.First(File))
		{
			if (modTime)
			{
				if (modTime != dir.GetLastWriteTime())
				{
					modTime = dir.GetLastWriteTime();
					Reload();
				}
			}
			else
			{
				modTime = dir.GetLastWriteTime();
			}
		}
		// else LgiTrace("%s:%i couldn't get stat for '%s'\n", _FL, File.Get());
	}
	
	void OnMouseClick(LMouse &m)
	{
		if (m.IsContextMenu())
		{
			LSubMenu s;
			s.AppendItem("Remove", IDM_DELETE);
			switch (s.Float(GetTree(), m))
			{
				case IDM_DELETE:
				{
					if (LTreeItem::Select())
						d->Files->Empty();
					delete this;
					return;
				}
			}
		}
	}

	void Select(bool selected) override
	{
		LTreeItem::Select(selected);
		if (!selected)
			return;

		d->Files->Empty();
		d->Diff->Name(NULL);
			
		LFile in(File, O_READ);
		LString s = in.Read();
		if (!s)
			return;
			
		LString NewLine("\n");
		LString::Array a = s.Replace("\r").Split("\n");
		LArray<LString> index;
		LString oldName, newName;
		LString::Array Diff;
		VcFile *f = NULL;
		bool InPreamble = false;
		bool InDiff = false;
		for (unsigned i=0; i<a.Length(); i++)
		{
			const char *Ln = a[i];
			if (!Strnicmp(Ln, "diff ", 5))
			{
				if (f)
				{
					f->SetDiff(NewLine.Join(Diff));
					f->Select(false);
				}

				Diff.Empty();
				oldName.Empty();
				newName.Empty();
				InDiff = false;
				InPreamble = true;
			}
			else if (!Strnicmp(Ln, "Index", 5))
			{
				if (InPreamble)
					index = a[i].SplitDelimit(": ", 1).Slice(1);
			}
			else if (!strncmp(Ln, "--- ", 4))
			{
				auto p = a[i].SplitDelimit(" \t", 1);
				if (p.Length() > 1)
					oldName = p[1];
			}
			else if (!strncmp(Ln, "+++ ", 4))
			{
				auto p = a[i].SplitDelimit(" \t", 1);
				if (p.Length() > 1)
					newName = p[1];

				if (oldName && newName)
				{
					InDiff = true;
					InPreamble = false;

					f = d->FindFile(newName);
					if (!f)
						f = new VcFile(d, NULL, LString(), false);

					const char *nullFn = "dev/null";
					auto Path = StripFirst(oldName);
					f->SetUri(LString("file:///") + Path);
					if (newName.Find(nullFn) >= 0)
					{
						// Delete
						f->SetText(Path, COL_FILENAME);
						f->SetText("D", COL_STATE);
					}
					else
					{
						f->SetText(Path, COL_FILENAME);
						if (oldName.Find(nullFn) >= 0)
							// Add
							f->SetText("A", COL_STATE);
						else
							// Modify
							f->SetText("M", COL_STATE);
					}

					f->GetStatus();
					d->Files->Insert(f);
				}
			}
			else if (!_strnicmp(Ln, "------", 6))
			{
				InPreamble = !InPreamble;
			}
			else if (!_strnicmp(Ln, "======", 6))
			{
				InPreamble = false;
				InDiff = true;
			}
			else if (InDiff)
			{
				Diff.Add(a[i]);
			}
		}
		if (f && Diff.Length())
		{
			f->SetDiff(NewLine.Join(Diff));
			Diff.Empty();
		}
	}
};

#ifdef HAIKU
enum PipeIndexes
{
	READ_END,
	WRITE_END,
};

#include <sys/ioctl.h>

int peekPipe(int fd, char *buf, size_t sz)
{
	int bytesAvailable = 0;
	int r = ioctl(fd, FIONREAD, &bytesAvailable);
	// printf("ioctl=%i %i\n", r, bytesAvailable);
	if (r)
		return 0;
		
	// printf("starting read\n");
	auto rd = read(fd, buf, MIN(bytesAvailable, sz));
	// printf("read=%i\n", (int)rd);
	return rd;
}

LString RunProcess(const char *exe, const char *args)
{
	LStringPipe r;
	
	int fd[2];
	pipe(fd);

	printf("Running %s...\n", exe);
	auto pid = fork();
	if (pid == 0)
	{
		// Child...
	    dup2(fd[WRITE_END], STDOUT_FILENO);
	    close(fd[READ_END]);
	    close(fd[WRITE_END]);
	    execlp(exe, exe, args, (char*) NULL);
	    fprintf(stderr, "Failed to execute '%s'\n", exe);
	    exit(1);
	}
	else
	{
		// Parent...
        int status;
        pid_t result;

		#if 0
			// More basic...
	        close(fd[READ_END]);
	        close(fd[WRITE_END]);
	        result = waitpid(pid, &status, 0);
			printf("waitpid=%i\n", result);
		#else
			// Read the output
			do
			{
				result = waitpid(pid, &status, WNOHANG);
				
				char buf[256];
				auto rd = peekPipe(fd[READ_END], buf, sizeof(buf));
				if (rd > 0)
					r.Write(buf, rd);
				//printf("waitpid=%i rd=%i\n", result, rd);
			}
			while (result == 0);

	        close(fd[READ_END]);
	        close(fd[WRITE_END]);
        #endif
	}

	printf("RunProcess done.\n");
	return r.NewLStr();
}
#endif

class GetVcsVersions : public LThread
{
	AppPriv *d = NULL;

public:
	GetVcsVersions(AppPriv *priv) : LThread("GetVcsVersions")
	{
		d = priv;
		Run();
	}

	bool ParseVersion(int Result, VersionCtrl type, LString s)
	{
		if (Result)
			return false;

		// printf("s=%s\n", s.Get());
		auto p = s.SplitDelimit();
		switch (type)
		{
			case VcGit:
			{
				if (p.Length() > 2)
				{
					ToolVersion[type] = Ver2Int(p[2]);
					d->Log->Print("Git version: %s\n", p[2].Get());
				}
				break;
			}
			case VcSvn:
			{
				if (p.Length() > 2)
				{
					ToolVersion[type] = Ver2Int(p[2]);
					d->Log->Print("Svn version: %s\n", p[2].Get());
				}
				break;
			}
			case VcHg:
			{
				if (p.Length() >= 5)
				{
					auto Ver = p[4].Strip("()");
					ToolVersion[type] = Ver2Int(Ver);
					d->Log->Print("Hg version: %s\n", Ver.Get());
				}
				break;
			}
			case VcCvs:
			{
				if (p.Length() > 1)
				{
					auto Ver = p[2];
					ToolVersion[type] = Ver2Int(Ver);
					d->Log->Print("Cvs version: %s\n", Ver.Get());
				}
				break;
			}
			default:
				break;
		}

		return false;
	}


	int Main()
	{
		VersionCtrl types[] = {
			#ifndef HAIKU
				// Enabling these causes lock error in the parent process...
				VcCvs,
				VcSvn,
			#endif
			VcHg,
			VcGit
		};

		for (int i=0; i<CountOf(types); i++)
		{
			auto Exe = d->GetVcName(types[i]);
			
			#ifdef HAIKU
			
			// Something funky is going on with launching subprocesses on Haiku...
			// So lets do an absolute minimal example of fork/exec to test whether it's
			// something in the LSubProcess classes?
			
			auto result = RunProcess(Exe, "--version");
			ParseVersion(0, types[i], result);
			
			#else
			
			LSubProcess sub(Exe, "--version");
			if (sub.Start())
			{
				LStringPipe p;
				auto result = sub.Communicate(&p);
				ParseVersion(result, types[i], p.NewLStr());
			}
			
			#endif
		}
		
		printf("GetVcsVersions finished.\n");

		return 0;
	}
};

class App : public LWindow, public AppPriv
{
	LAutoPtr<LImageList> ImgLst;
	LBox *FoldersBox = NULL;

	bool CallMethod(const char *MethodName, LScriptArguments &Args)
	{
		if (!Stricmp(MethodName, METHOD_GetContext))
		{
			*Args.GetReturn() = (AppPriv*)this;
			return true;
		}

		return false;
	}

public:
	App()
	{
		LString AppRev;
		AppRev.Printf("%s v%s", AppName, APP_VERSION);
		Name(AppRev);

		LRect r(0, 0, 1400, 800);
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

		ImgLst.Reset(LLoadImageList("image-list.png", 16, 16));

		if (Attach(0))
		{
			SetPulse(200);
			DropTarget(true);
			Visible(true);
		}
	}

	~App()
	{
		SerializeState(&Opts, "WndPos", false);
		SaveFolders();
	}
	
	void OnCreate()
	{
		if ((Menu = new LMenu))
		{
			Menu->SetPrefAndAboutItems(IDM_OPTIONS, IDM_ABOUT);
			Menu->Attach(this);
			Menu->Load(this, "IDM_MENU");
		}

		LBox *ToolsBox = new LBox(IDC_TOOLS_BOX, true, "ToolsBox");
		FoldersBox = new LBox(IDC_FOLDERS_BOX, false, "FoldersBox");
		LBox *CommitsBox = new LBox(IDC_COMMITS_BOX, true, "CommitsBox");

		ToolBar *Tools = new ToolBar;

		ToolsBox->Attach(this);
		Tools->Attach(ToolsBox);
		FoldersBox->Attach(ToolsBox);

		auto FolderLayout = new LTableLayout(IDC_FOLDER_TBL);
		auto c = FolderLayout->GetCell(0, 0, true, 2);
			Tree = new LTree(IDC_TREE, 0, 0, 320, 200);
			Tree->SetImageList(ImgLst, false);
			Tree->ShowColumnHeader(true);
			Tree->AddColumn("Folder", 250);
			Tree->AddColumn("Counts", 50);
			c->Add(Tree);
		c = FolderLayout->GetCell(0, 1);
			c->Add(new LEdit(IDC_FILTER_FOLDERS, 0, 0, -1, -1));
		c = FolderLayout->GetCell(1, 1);
			c->Add(new LButton(IDC_CLEAR_FILTER_FOLDERS, 0, 0, -1, -1, "x"));
		FolderLayout->Attach(FoldersBox);
		CommitsBox->Attach(FoldersBox);

		auto CommitsLayout = new LTableLayout(IDC_COMMITS_TBL);
		c = CommitsLayout->GetCell(0, 0, true, 2);
			Commits = new CommitList(IDC_LIST);
			c->Add(Commits);
		c = CommitsLayout->GetCell(0, 1);
			c->Add(new LEdit(IDC_FILTER_COMMITS, 0, 0, -1, -1));
		c = CommitsLayout->GetCell(1, 1);
			c->Add(new LButton(IDC_CLEAR_FILTER_COMMITS, 0, 0, -1, -1, "x"));
		CommitsLayout->Attach(CommitsBox);
		CommitsLayout->GetCss(true)->Height("40%");

		LBox *FilesBox = new LBox(IDC_FILES_BOX, false);
		FilesBox->Attach(CommitsBox);

		auto FilesLayout = new LTableLayout(IDC_FILES_TBL);
		c = FilesLayout->GetCell(0, 0, true, 2);
			Files = new LList(IDC_FILES, 0, 0, 200, 200);
			Files->AddColumn("[ ]", 30);
			Files->AddColumn("State", 100);
			Files->AddColumn("Name", 400);
			c->Add(Files);
		c = FilesLayout->GetCell(0, 1);
			c->Add(new LEdit(IDC_FILTER_FILES, 0, 0, -1, -1));
		c = FilesLayout->GetCell(1, 1);
			c->Add(new LButton(IDC_CLEAR_FILTER_FILES, 0, 0, -1, -1, "x"));
		FilesLayout->GetCss(true)->Width("35%");
		FilesLayout->Attach(FilesBox);

		LBox *MsgBox = new LBox(IDC_MSG_BOX, true);
		MsgBox->Attach(FilesBox);
		
		CommitCtrls *Commit = new CommitCtrls;
		Commit->Attach(MsgBox);
		Commit->GetCss(true)->Height("25%");
		if (Commit->GetViewById(IDC_MSG, Msg))
		{
			LTextView3 *Tv = dynamic_cast<LTextView3*>(Msg);
			if (Tv)
			{
				Tv->Sunken(true);
				Tv->SetWrapType(L_WRAP_NONE);
			}
		}
		else LAssert(!"No ctrl?");

		Tabs = new LTabView(IDC_TAB_VIEW);
		Tabs->Attach(MsgBox);
		const char *Style = "Padding: 0px 8px 8px 0px";
		Tabs->GetCss(true)->Parse(Style);

		LTabPage *p = Tabs->Append("Diff");
		p->Append(Diff = new DiffView(IDC_TXT));
		// Diff->Sunken(true);
		Diff->SetWrapType(L_WRAP_NONE);

		p = Tabs->Append("Log");
		p->Append(Log = new LTextLog(IDC_LOG));
		// Log->Sunken(true);
		Log->SetWrapType(L_WRAP_NONE);
		
		SetCtrlValue(IDC_UPDATE, true);

		AttachChildren();

		LXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (!f)
		{
			Opts.CreateTag(OPT_Folders);
			f = Opts.LockTag(OPT_Folders, _FL);
		}
		if (f)
		{
			new GetVcsVersions(this);

			for (auto c: f->Children)
			{
				if (c->IsTag(OPT_Folder))
				{
					auto f = new VcFolder(this, c);
					Tree->Insert(f);
				}
			}
			Opts.Unlock();
			
			LRect Large(0, 0, 2000, 200);
			Tree->SetPos(Large);
			Tree->ResizeColumnsToContent();
			
			LItemColumn *c;
			int i = 0, px = 0;
			while ((c = Tree->ColumnAt(i++)))
			{
				px += c->Width();
			}
			
			FoldersBox->Value(MAX(320, px + 20));
            
            // new TestThread();
		}
	}
	
	void SaveFolders()
	{
		LXmlTag *f = Opts.LockTag(OPT_Folders, _FL);
		if (!f)
			return;

		f->EmptyChildren();

		for (auto i: *Tree)
		{
			VcFolder *vcf = dynamic_cast<VcFolder*>(i);
			if (vcf)
				f->InsertTag(vcf->Save());
		}

		Opts.Unlock();
		Opts.SerializeFile(true);
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->Msg())
		{
			#if HAS_LIBSSH
			case M_RESPONSE:
			{
				SshConnection::HandleMsg(Msg);
				break;
			}
			#endif
			case M_HANDLE_CALLBACK:
			{
				LAutoPtr<ProcessCallback> Pc((ProcessCallback*)Msg->A());
				if (Pc)
					Pc->OnComplete();
				break;
			}
		}

		return LWindow::OnEvent(Msg);
	}

	void OnReceiveFiles(LArray<const char*> &Files)
	{
		for (auto f : Files)
		{
			if (LDirExists(f))
				OpenLocalFolder(f);
		}
	}

	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDM_PATCH_VIEWER:
			{
				OpenPatchViewer(this, &Opts);
				break;
			}
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
			case IDM_OPEN_DIFF:
			{
				auto s = new LFileSelect;
				s->Parent(this);
				s->Open([this](auto dlg, auto status)
				{
					if (status)
						OpenDiff(dlg->Name());
					delete dlg;
				});
				break;
			}
			case IDM_OPTIONS:
			{
				auto Dlg = new OptionsDlg(this, Opts);
				Dlg->DoModal([](auto dlg, auto ctrlId)
				{
					delete dlg;
				});
				break;
			}
			case IDM_FIND:
			{
				auto i = new LInput(this, "", "Search string:");
				i->DoModal([this, i](auto dlg, auto ctrlId)
				{
					if (ctrlId == IDOK)
					{
						LString::Array Revs;
						Revs.Add(i->GetStr());

						CommitList *cl;
						if (GetViewById(IDC_LIST, cl))
							cl->SelectRevisions(Revs);
					}
					delete dlg;
				});
				break;
			}
			case IDM_UNTRACKED:
			{
				auto mi = GetMenu()->FindItem(IDM_UNTRACKED);
				if (!mi)
					break;

				mi->Checked(!mi->Checked());

				LArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f : Flds)
				{
					f->Refresh();
				}
				break;
			}
			case IDM_REFRESH:
			{
				LArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->Refresh();
				break;
			}
			case IDM_PULL:
			{
				LArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->Pull();
				break;
			}
			case IDM_PUSH:
			{
				LArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->Push();
				break;
			}
			case IDM_STATUS:
			{
				LArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->FolderStatus();
				break;
			}
			case IDM_UPDATE_SUBS:
			{
				LArray<VcFolder*> Flds;
				Tree->GetSelection(Flds);
				for (auto f: Flds)
					f->UpdateSubs();
				break;
				break;
			}
			case IDM_EXIT:
			{
				LCloseApp();
				break;
			}
		}

		return 0;
	}

	void OnPulse()
	{
		for (auto i: *Tree)
			i->OnPulse();
	}

	void OpenLocalFolder(const char *Fld = NULL)
	{
		auto Load = [this](const char *Fld)
		{
			// Check the folder isn't already loaded...
			VcFolder *Has = NULL;
			LArray<VcFolder*> Folders;
			Tree->GetAll(Folders);
			for (auto f: Folders)
			{
				if (f->GetUri().IsFile() &&
					!Stricmp(f->LocalPath(), Fld))
				{
					Has = f;
					break;
				}
			}

			if (Has)
			{
				Has->Select(true);
				LPopupNotification::Message(this, LString::Fmt("'%s' is already open", Has->LocalPath()));
			}
			else
			{
				LUri u;
				u.SetFile(Fld);
				
				auto f = new VcFolder(this, u.ToString());
				if (f)
				{
					Tree->Insert(f);
					SaveFolders();
				}
			}
		};

		if (!Fld)
		{
			auto s = new LFileSelect;
			s->Parent(this);
			s->OpenFolder([this, Load](auto s, auto status)
			{
				if (status)
					Load(s->Name());
				delete s;
			});
		}
		else Load(Fld);
	}

	void OpenRemoteFolder()
	{
		auto Dlg = new RemoteFolderDlg(this);
		Dlg->DoModal([this, Dlg](auto dlg, auto status)
		{
			if (status)
			{
				Tree->Insert(new VcFolder(this, Dlg->Uri));
				SaveFolders();
			}
			delete dlg;
		});
	}
	
	void OpenDiff(const char *File)
	{
		Tree->Insert(new VcDiffFile(this, File));
	}

	void OnFilterFolders()
	{
		if (!Tree)
			return;

		for (auto i = Tree->GetChild(); i; i = i->GetNext())
		{
			auto n = i->GetText();
			bool vis = !FolderFilter || Stristr(n, FolderFilter.Get()) != NULL;
			i->GetCss(true)->Display(vis ? LCss::DispBlock : LCss::DispNone);
		}

		Tree->UpdateAllItems();
		Tree->Invalidate();
	}

	void OnFilterCommits()
	{
		if (!Commits || !CommitFilter)
			return;

		CurFolder->LogFilter(CommitFilter);

		/*			
		LArray<LListItem*> a;
		if (!Commits->GetAll(a))
			return;
		
		auto cols = Commits->GetColumns();	
		for (auto i: a)
		{
			bool vis = !CommitFilter;
			for (int c=1; !vis && c<cols; c++)
			{
				auto txt = i->GetText(c);
				if (Stristr(txt, CommitFilter.Get()))
					vis = true;
			}
			i->GetCss(true)->Display(vis ? LCss::DispBlock : LCss::DispNone);
		}

		Commits->UpdateAllItems();
		Commits->Invalidate();
		*/
	}

	void OnFilterFiles()
	{
		VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
		if (f)
			f->FilterCurrentFiles();
	}

	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDC_CLEAR_FILTER_FOLDERS:
			{
				SetCtrlName(IDC_FILTER_FOLDERS, NULL);
				// Fall through
			}
			case IDC_FILTER_FOLDERS:
			{
				if (n.Type == LNotifyEscapeKey)
					SetCtrlName(IDC_FILTER_FOLDERS, NULL);
					
				LString n = GetCtrlName(IDC_FILTER_FOLDERS);
				if (n != FolderFilter)
				{
					FolderFilter = n;
					OnFilterFolders();
				}
				break;
			}
			case IDC_CLEAR_FILTER_COMMITS:
			{
				SetCtrlName(IDC_FILTER_COMMITS, NULL);
				// Fall through
			}
			case IDC_FILTER_COMMITS:
			{
				if (n.Type == LNotifyEscapeKey)
					SetCtrlName(IDC_FILTER_COMMITS, NULL);

				LString n = GetCtrlName(IDC_FILTER_COMMITS);
				if (n != CommitFilter)
				{
					CommitFilter = n;
					OnFilterCommits();
				}
				break;
			}
			case IDC_CLEAR_FILTER_FILES:
			{
				SetCtrlName(IDC_FILTER_FILES, NULL);
				// Fall through
			}
			case IDC_FILTER_FILES:
			{
				if (n.Type == LNotifyEscapeKey)
					SetCtrlName(IDC_FILTER_FILES, NULL);

				LString n = GetCtrlName(IDC_FILTER_FILES);
				if (n != FileFilter)
				{
					FileFilter = n;
					OnFilterFiles();
				}
				break;
			}
			case IDC_FILES:
			{
				switch (n.Type)
				{
					case LNotifyItemColumnClicked:
					{
						int Col = -1;
						LMouse m;
						if (Files->GetColumnClickInfo(Col, m))
						{
							if (Col == 0)
							{
								// Select / deselect all check boxes..
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
					default:
						break;
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
				switch (n.Type)
				{
					case LNotifyContainerClick:
					{
						LMouse m;
						c->GetMouse(m);
						if (m.Right())
						{
							LSubMenu s;
							s.AppendItem("Add Local", IDM_ADD_LOCAL);
							s.AppendItem("Add Remote", IDM_ADD_REMOTE);
							s.AppendItem("Add Diff File", IDM_ADD_DIFF_FILE);
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
								case IDM_ADD_DIFF_FILE:
								{
									auto s = new LFileSelect;
									s->Parent(this);
									s->Open([this](auto dlg, auto status)
									{
										if (status)
											OpenDiff(dlg->Name());
										delete dlg;
									});
									break;
								}
							}
						}
						break;
					}
					case (LNotifyType)LvcCommandStart:
					{
						SetCtrlEnabled(IDC_PUSH, false);
						SetCtrlEnabled(IDC_PULL, false);
						SetCtrlEnabled(IDC_PULL_ALL, false);
						break;
					}
					case (LNotifyType)LvcCommandEnd:
					{
						SetCtrlEnabled(IDC_PUSH, true);
						SetCtrlEnabled(IDC_PULL, true);
						SetCtrlEnabled(IDC_PULL_ALL, true);
						break;
					}
					default:
						break;
				}
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
				LArray<VcFolder*> Folders;
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
				LArray<VcFolder*> Folders;
				Tree->GetAll(Folders);
				for (auto f : Folders)
				{
					f->FolderStatus();
				}
				break;
			}
			case IDC_BRANCHES:
			{
				if (n.Type == LNotifyValueChanged)
				{
					VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
					auto branch = c->Name();
					if (!f || !branch)
					{
						Log->Print("%s:%i - Missing param: %p %p\n", _FL, f, branch);
						break;
					}
					
					f->OnUpdate(branch);
				}
				break;
			}
			case IDC_LIST:
			{
				switch (n.Type)
				{
					case LNotifyItemColumnClicked:
					{
						int Col = -1;
						LMouse Ms;
						Commits->GetColumnClickInfo(Col, Ms);
						Commits->Sort(LstCmp, Col);
						break;
					}
					case LNotifyItemDoubleClick:
					{
						VcFolder *f = dynamic_cast<VcFolder*>(Tree->Selection());
						if (!f)
							break;

						LArray<VcCommit*> s;
						if (Commits->GetSelection(s) && s.Length() == 1)
							f->OnUpdate(s[0]->GetRev());
						break;
					}
					default:
						break;
				}
				break;
			}
		}

		return 0;
	}
};

struct SshHost : public LTreeItem
{
	LXmlTag *t;
	LString Host, User, Pass;

	SshHost(LXmlTag *tag = NULL)
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
			LUri u;
			u.sProtocol = "ssh";
			u.sHost = Host;
			u.sUser = User;
			u.sPass = Pass;
			t->SetContent(u.ToString());
		}
		else
		{
			LUri u(t->GetContent());
			if (!Stricmp(u.sProtocol.Get(), "ssh"))
			{
				Host = u.sHost;
				User = u.sUser;
				Pass = u.sPass;
			}
		}
	}
};

RemoteFolderDlg::RemoteFolderDlg(App *application) : app(application), root(NULL), newhost(NULL), tree(NULL)
{
	SetParent(app);
	LoadFromResource(IDD_REMOTE_FOLDER);

	if (GetViewById(IDC_HOSTS, tree))
	{
		printf("tree=%p\n", tree);

		tree->Insert(root = new SshHost());
		root->SetText("Ssh Hosts");
	}
	else return;
	
	LViewI *v;
	if (GetViewById(IDC_HOSTNAME, v))
		v->Focus(true);

	Ui.Map("Host", IDC_HOSTNAME);
	Ui.Map("User", IDC_USER);
	Ui.Map("Password", IDC_PASS);

	LXmlTag *hosts = app->Opts.LockTag(OPT_Hosts, _FL);
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

int RemoteFolderDlg::OnNotify(LViewI *Ctrl, LNotification n)
{
	SshHost *cur = tree ? dynamic_cast<SshHost*>(tree->Selection()) : NULL;

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
			switch (n.Type)
			{
				case LNotifyItemSelect:
				{
					bool isRoot = cur == root;
					SetCtrlEnabled(IDC_HOSTNAME, !isRoot);
					SetCtrlEnabled(IDC_USER, !isRoot);
					SetCtrlEnabled(IDC_PASS, !isRoot);
					SetCtrlEnabled(IDC_DELETE, !isRoot && !(cur == newhost));
					
					SetCtrlName(IDC_HOSTNAME, cur ? cur->Host.Get() : NULL);
					SetCtrlName(IDC_USER, cur ? cur->User.Get() : NULL);
					SetCtrlName(IDC_PASS, cur ? cur->Pass.Get() : NULL);
					break;
				}
				default:
					break;
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
		case IDC_DELETE:
		{
			auto sel = tree ? dynamic_cast<SshHost*>(tree->Selection()) : NULL;
			if (!sel)
				break;

			LXmlTag *hosts = app->Opts.LockTag(OPT_Hosts, _FL);
			if (!hosts)
			{
				LAssert(!"Couldn't lock tag.");
				break;
			}

			if (hosts->Children.HasItem(sel->t))
			{
				sel->t->RemoveTag();
				DeleteObj(sel->t);
				delete sel;
			}
			
			app->Opts.Unlock();
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
			LXmlTag *hosts;
			if (!(hosts = app->Opts.LockTag(OPT_Hosts, _FL)))
			{
				if (!(app->Opts.CreateTag(OPT_Hosts) && (hosts = app->Opts.LockTag(OPT_Hosts, _FL))))
					break;
			}
			LAssert(hosts != NULL);
			
			for (auto i = root->GetChild(); i; i = i->GetNext())
			{
				SshHost *h = dynamic_cast<SshHost*>(i);
				if (!h || h == newhost)
					continue;

				if (h->t)
					;
				else if ((h->t = new LXmlTag(OPT_Host)))
					hosts->InsertTag(cur->t);
				else
					return false;

				h->Serialize(true);
			}

			app->Opts.Unlock();

			LUri u;
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

const char* toString(VersionCtrl v)
{
	switch (v)
	{
		case VcCvs: return "VcCvs";
		case VcSvn: return "VcSvn";
		case VcGit: return "VcGit";
		case VcHg:  return "VcHg";

		case VcPending: return "VcPending";
		case VcError: return "VcError";
	}

	return "VcNone";
}

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		// LStructuredLog::UnitTest();

		a.AppWnd = new App;
		a.Run();
		
		DeleteObj(a.AppWnd);
	}

	LAssert(VcCommit::Instances == 0);
	return 0;
}

