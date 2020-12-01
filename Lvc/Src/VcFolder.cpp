#include "Lvc.h"
#include "../Resources/resdefs.h"
#include "GCombo.h"
#include "GClipBoard.h"

#ifndef CALL_MEMBER_FN
#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))
#endif

#define MAX_AUTO_RESIZE_ITEMS	2000
#define PROFILE_FN				0
#if PROFILE_FN
#define PROF(s) Prof.Add(s)
#else
#define PROF(s)
#endif

bool TerminalAt(GString Path)
{
	#if defined(MAC)
		return LgiExecute("/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal", Path);
	#elif defined(WINDOWS)
		TCHAR w[MAX_PATH];
		auto r = GetWindowsDirectory(w, CountOf(w));
		if (r > 0)
		{
			GFile::Path p = GString(w);
			p += "system32\\cmd.exe";
			FileDev->SetCurrentFolder(Path);
			return LgiExecute(p);
		}
	#elif defined(LINUX)
		// #error "Impl me."
	#endif

	return false;
}

int Ver2Int(GString v)
{
	auto p = v.Split(".");
	int i = 0;
	for (auto s : p)
	{
		auto Int = s.Int();
		if (Int < 256)
		{
			i <<= 8;
			i |= (uint8_t)Int;
		}
		else
		{
			LgiAssert(0);
			return 0;
		}
	}
	return i;
}

int ToolVersion[VcMax] = {0};

ReaderThread::ReaderThread(VersionCtrl vcs, GSubProcess *p, GStream *out) : LThread("ReaderThread")
{
	Vcs = vcs;
	Process = p;
	Out = out;
	Result = -1;
	FilterCount = 0;
	
	Run();
}

ReaderThread::~ReaderThread()
{
	Out = NULL;
	while (!IsExited())
		LgiSleep(1);
}

const char *HgFilter = "We\'re removing Mercurial support";
const char *CvsKill = "No such file or directory";

int ReaderThread::OnLine(char *s, ssize_t len)
{
	switch (Vcs)
	{
		case VcHg:
		{
			if (strnistr(s, HgFilter, len))
				FilterCount = 4;
			if (FilterCount > 0)
			{
				FilterCount--;
				return 0;
			}
			else if (GString(s, len).Strip().Equals("remote:"))
			{
				return 0;
			}
			break;
		}
		case VcCvs:
		{
			if (strnistr(s, CvsKill, len))
				return -1;
			break;
		}
		default:
			break;
	}

	return 1;
}

bool ReaderThread::OnData(char *Buf, ssize_t &r)
{
	#if 1
	char *Start = Buf;
	for (char *c = Buf; c < Buf + r;)
	{
		bool nl = *c == '\n';
		c++;
		if (nl)
		{
			int Result = OnLine(Start, c - Start);
			if (Result < 0)
			{
				// Kill process and exit thread.
				Process->Kill();
				return false;
			}
			if (Result == 0)
			{
				ssize_t LineLen = c - Start;
				ssize_t NextLine = c - Buf;
				ssize_t Remain = r - NextLine;
				if (Remain > 0)
					memmove(Start, Buf + NextLine, Remain);
				r -= LineLen;
				c = Start;
			}
			else Start = c;
		}
	}
	#endif

	Out->Write(Buf, r);
	return true;
}

int ReaderThread::Main()
{
	bool b = Process->Start(true, false);
	if (!b)
	{
		GString s("Process->Start failed.\n");
		Out->Write(s.Get(), s.Length(), ErrSubProcessFailed);
		return ErrSubProcessFailed;
	}

	char Buf[1024];
	ssize_t r;
	// printf("%s:%i - starting reader loop.\n", _FL);
	while (Process->IsRunning())
	{
		if (Out)
		{
			// printf("%s:%i - starting read.\n", _FL);
			r = Process->Read(Buf, sizeof(Buf));
			// printf("%s:%i - read=%i.\n", _FL, (int)r);
			if (r > 0)
			{
				if (!OnData(Buf, r))
					return -1;
			}
		}
		else
		{
			Process->Kill();
			return -1;
			break;
		}
	}

	// printf("%s:%i - process loop done.\n", _FL);
	if (Out)
	{
		while ((r = Process->Read(Buf, sizeof(Buf))) > 0)
			OnData(Buf, r);
	}

	// printf("%s:%i - loop done.\n", _FL);
	Result = (int) Process->GetExitValue();
	#if _DEBUG
	if (Result)
		printf("%s:%i - Process err: %i 0x%x\n", _FL, Result, Result);
	#endif
	
	return Result;
}

/////////////////////////////////////////////////////////////////////////////////////////////
void VcFolder::Init(AppPriv *priv)
{
	d = priv;

	IsCommit = false;
	IsLogging = false;
	IsUpdate = false;
	IsFilesCmd = false;
	IsWorkingFld = false;
	CommitListDirty = false;
	IsUpdatingCounts = false;
	IsBranches = StatusNone;
	IsIdent = StatusNone;

	Unpushed = Unpulled = -1;
	Type = VcNone;
	CmdErrors = 0;
	CurrentCommitIdx = -1;

	Expanded(false);
	Insert(Tmp = new GTreeItem);
	Tmp->SetText("Loading...");

	LgiAssert(d != NULL);
}

VcFolder::VcFolder(AppPriv *priv, const char *uri)
{
	Init(priv);
	Uri.Set(uri);
	GetType();
}

VcFolder::VcFolder(AppPriv *priv, GXmlTag *t)
{
	Init(priv);
	Serialize(t, false);
}

VcFolder::~VcFolder()
{
	Log.DeleteObjects();
}

VersionCtrl VcFolder::GetType()
{
	if (Type == VcNone)
		Type = d->DetectVcs(this);
	return Type;
}

const char *VcFolder::LocalPath()
{
	if (!Uri.IsProtocol("file") || Uri.sPath.IsEmpty())
	{
		LgiAssert(!"Shouldn't call this if not a file path.");
		return NULL;
	}
	auto c = Uri.sPath.Get();
	#ifdef WINDOWS
	if (*c == '/')
		c++;
	#endif
	return c;
}

const char *VcFolder::GetText(int Col)
{
	switch (Col)
	{
		case 0:
		{
			if (Uri.IsFile())
				Cache = LocalPath();
			else
				Cache.Printf("%s%s", Uri.sHost.Get(), Uri.sPath.Get());

			if (Cmds.Length())
				Cache += " (...)";

			return Cache;
		}
		case 1:
		{
			CountCache.Printf("%i/%i", Unpulled, Unpushed);
			CountCache = CountCache.Replace("-1", "--");
			return CountCache;
		}
	}

	return NULL;
}

bool VcFolder::Serialize(GXmlTag *t, bool Write)
{
	if (Write)
		t->SetContent(Uri.ToString());
	else
	{
		GString s = t->GetContent();
		bool isUri = s.Find("://") >= 0;
		if (isUri)
			Uri.Set(s);
		else
			Uri.SetFile(s);
	}
	return true;
}

GXmlTag *VcFolder::Save()
{
	GXmlTag *t = new GXmlTag(OPT_Folder);
	if (t)
		Serialize(t, true);
	return t;
}

const char *VcFolder::GetVcName()
{
	const char *Def = NULL;
	switch (GetType())
	{
		case VcGit:
			Def = "git";
			break;
		case VcSvn:
			Def = "svn";
			break;
		case VcHg:
			Def = "hg";
			break;
		case VcCvs:
			Def = "cvs";
			break;
		default:
			break;
	}
	
	if (!VcCmd)
	{
		GString Opt;
		Opt.Printf("%s-path", Def);
		GVariant v;
		if (d->Opts.GetValue(Opt, v))
			VcCmd = v.Str();
	}
	
	if (!VcCmd)
		VcCmd = Def;
	
	return VcCmd;
}

Result VcFolder::RunCmd(const char *Args, LoggingType Logging)
{
	Result Ret;
	Ret.Code = -1;

	const char *Exe = GetVcName();
	if (!Exe || CmdErrors > 2)
		return Ret;

	if (Uri.IsFile())
	{
		GSubProcess Process(Exe, Args);
		Process.SetInitFolder(LocalPath());
		if (!Process.Start())
		{
			Ret.Out.Printf("Process failed with %i", Process.GetErrorCode());
			return Ret;
		}

		if (Logging == LogNormal)
			d->Log->Print("%s %s\n", Exe, Args);

		while (Process.IsRunning())
		{
			auto Rd = Process.Read();
			if (Rd.Length())
			{
				Ret.Out += Rd;
				if (Logging == LogNormal)
					d->Log->Write(Rd.Get(), Rd.Length());
			}

			LgiYield();
		}

		auto Rd = Process.Read();
		if (Rd.Length())
		{
			Ret.Out += Rd;
			if (Logging == LogNormal)
				d->Log->Write(Rd.Get(), Rd.Length());
		}

		Ret.Code = Process.GetExitValue();
	}
	else
	{
		LgiAssert(!"Impl me.");
	}

	return Ret;
}

bool VcFolder::StartCmd(const char *Args, ParseFn Parser, ParseParams *Params, LoggingType Logging)
{
	const char *Exe = GetVcName();
	if (!Exe)
		return false;
	if (CmdErrors > 2)
		return false;

	if (d->Log && Logging != LogSilo)
		d->Log->Print("%s %s\n", Exe, Args);

	if (Uri.IsFile())
	{
		GAutoPtr<GSubProcess> Process(new GSubProcess(Exe, Args));
		if (!Process)
			return false;

		Process->SetInitFolder(Params && Params->AltInitPath ? Params->AltInitPath.Get() : LocalPath());
		#ifdef MAC
		// Mac GUI apps don't share the terminal path, so this overrides that and make it work
		auto Path = LGetPath();
		GString Tmp = GString(LGI_PATH_SEPARATOR).Join(Path);
		Process->SetEnvironment("PATH", Tmp);
		#endif

		GString::Array Ctx;
		Ctx.SetFixedLength(false);
		Ctx.Add(LocalPath());
		Ctx.Add(Exe);
		Ctx.Add(Args);
		GAutoPtr<Cmd> c(new Cmd(Ctx, Logging, d->Log));
		if (!c)
			return false;

		c->PostOp = Parser;
		c->Params.Reset(Params);
		c->Rd.Reset(new ReaderThread(GetType(), Process.Release(), c));
		Cmds.Add(c.Release());
	}
	else
	{
		auto c = d->GetConnection(Uri.ToString());
		if (!c)
			return false;
		
		if (!c->Command(this, Exe, Args, Parser, Params))
			return false;
	}

	Update();
	return true;
}

int LogDateCmp(LListItem *a, LListItem *b, NativeInt Data)
{
	VcCommit *A = dynamic_cast<VcCommit*>(a);
	VcCommit *B = dynamic_cast<VcCommit*>(b);

	if ((A != NULL) ^ (B != NULL))
	{
		// This handles keeping the "working folder" list item at the top
		return (A != NULL) - (B != NULL);
	}

	// Sort the by date from most recent to least
	return -A->GetTs().Compare(&B->GetTs());
}

bool VcFolder::ParseBranches(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		{
			GString::Array a = s.SplitDelimit("\r\n");
			for (auto &l: a)
			{
				GString n = l.Strip();
				if (n(0) == '*')
				{
					GString::Array c = n.SplitDelimit(" \t", 1);
					if (c.Length() > 1)
					{
						CurrentBranch = c[1];
						Branches.Add(CurrentBranch, new VcBranch(CurrentBranch));
					}
					else
						Branches.Add(n, new VcBranch(n));
				}
				else Branches.Add(n, new VcBranch(n));
			}
			break;
		}
		case VcHg:
		{
			auto a = s.SplitDelimit("\r\n");
			Branches.DeleteObjects();
			for (auto b: a)
			{
				if (!CurrentBranch)
					 CurrentBranch = b;
				Branches.Add(b, new VcBranch(b));
			}

			if (Params && Params->Str.Equals("CountToTip"))
				CountToTip();
			break;
		}
		default:
		{
			break;
		}
	}

	IsBranches = Result ? StatusError : StatusNone;
	OnBranchesChange();
	return false;
}

void VcFolder::OnBranchesChange()
{
	GWindow *w = d->Tree->GetWindow();
	if (!w || !GTreeItem::Select())
		return;

	if (Branches.Length())
	{
		// Set the colours up
		GString Default;
		for (auto b: Branches)
		{
			if (!stricmp(b.key, "default") ||
				!stricmp(b.key, "trunk"))
				Default = b.key;
			/*
			else
				printf("Other=%s\n", b.key);
			*/
		}
		int Idx = 1;
		for (auto b: Branches)
		{
			if (!b.value->Colour.IsValid())
			{
				if (Default && !stricmp(b.key, Default))
					b.value->Colour = GetPaletteColour(0);
				else
					b.value->Colour = GetPaletteColour(Idx++);
			}
		}
	}

	DropDownBtn *dd;
	if (w->GetViewById(IDC_BRANCH_DROPDOWN, dd))
	{
		GString::Array a;
		for (auto b: Branches)
		{
			a.Add(b.key);
		}
		dd->SetList(IDC_BRANCH, a);
	}

	if (Branches.Length() > 0)
	{
		GViewI *b;
		if (w->GetViewById(IDC_BRANCH, b))
		{
			if (!ValidStr(b->Name()))
			{
				if (CurrentBranch)
					b->Name(CurrentBranch);
				else
				{
					auto it = Branches.begin();
					b->Name((*it).key);
				}
			}
		}
	}
}

void VcFolder::DefaultFields()
{
	if (Fields.Length() == 0)
	{
		switch (GetType())
		{
			case VcHg:
			{
				Fields.Add(LGraph);
				Fields.Add(LIndex);
				Fields.Add(LRevision);
				Fields.Add(LBranch);
				Fields.Add(LAuthor);
				Fields.Add(LTimeStamp);
				Fields.Add(LMessage);
				break;
			}
			default:
			{
				Fields.Add(LGraph);
				Fields.Add(LRevision);
				Fields.Add(LAuthor);
				Fields.Add(LTimeStamp);
				Fields.Add(LMessage);
				break;
			}
		}
	}
}

void VcFolder::UpdateColumns()
{
	d->Commits->EmptyColumns();

	for (auto c: Fields)
	{
		switch (c)
		{
			case LGraph: d->Commits->AddColumn("---", 60); break;
			case LIndex: d->Commits->AddColumn("Index", 60); break;
			case LBranch: d->Commits->AddColumn("Branch", 60); break;
			case LRevision: d->Commits->AddColumn("Revision", 60); break;
			case LAuthor: d->Commits->AddColumn("Author", 240); break;
			case LTimeStamp: d->Commits->AddColumn("Date", 130); break;
			case LMessage: d->Commits->AddColumn("Message", 400); break;
			default: LgiAssert(0); break;
		}
	}
}

void VcFolder::Select(bool b)
{
	#if PROFILE_FN
	GProfile Prof("Select");
	#endif
	if (!b)
	{
		GWindow *w = d->Tree->GetWindow();
		w->SetCtrlName(IDC_BRANCH, NULL);
	}

	PROF("Parent.Select");
	GTreeItem::Select(b);
	
	if (b)
	{
		if (Uri.IsFile() && !DirExists(LocalPath()))
			return;

		PROF("DefaultFields");
		DefaultFields();

		PROF("Type Change");
		if (GetType() != d->PrevType)
		{
			d->PrevType = GetType();			
			UpdateColumns();
		}

		PROF("UpdateCommitList");
		if ((Log.Length() == 0 || CommitListDirty) && !IsLogging)
		{
			switch (GetType())
			{
				case VcGit:
				{
					IsLogging = StartCmd("rev-list --all --header --timestamp --author-date-order", &VcFolder::ParseRevList);
					break;
				}
				case VcSvn:
				{
					GVariant Limit;
					d->Opts.GetValue("svn-limit", Limit);

					if (CommitListDirty)
					{
						IsLogging = StartCmd("up", &VcFolder::ParsePull, new ParseParams("log"));
						break;
					}
					
					GString s;
					if (Limit.CastInt32())
						s.Printf("log --limit %i", Limit.CastInt32());
					else
						s = "log";
					IsLogging = StartCmd(s, &VcFolder::ParseLog);
					break;
				}
				default:
				{
					IsLogging = StartCmd("log", &VcFolder::ParseLog);
					break;
				}
			}

			CommitListDirty = false;
		}

		PROF("GetBranches");
		if (GetBranches())
			OnBranchesChange();

		auto Ctrl = d->Commits->GetWindow()->GetCtrlName(IDC_FILTER);
		GString Filter = ValidStr(Ctrl) ? Ctrl : NULL;

		if (d->CurFolder != this)
		{
			PROF("RemoveAll");
			d->CurFolder = this;
			d->Commits->RemoveAll();
		}

		PROF("Uncommit");
		if (!Uncommit)
			Uncommit.Reset(new UncommitedItem(d));
		d->Commits->Insert(Uncommit, 0);

		PROF("Log Loop");
		int64 CurRev = Atoi(CurrentCommit.Get());
		List<LListItem> Ls;
		for (auto l: Log)
		{
			if (CurrentCommit &&
				l->GetRev())
			{
				switch (GetType())
				{
					case VcSvn:
					{
						int64 LogRev = Atoi(l->GetRev());
						if (CurRev >= 0 && CurRev >= LogRev)
						{
							CurRev = -1;
							l->SetCurrent(true);
						}
						else
						{
							l->SetCurrent(false);
						}
						break;
					}
					default:
						l->SetCurrent(!_stricmp(CurrentCommit, l->GetRev()));
						break;
				}
			}

			bool Add = !Filter;
			if (Filter)
			{
				const char *s = l->GetRev();
				if (s && strstr(s, Filter) != NULL)
					Add = true;

				s = l->GetAuthor();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
				s = l->GetMsg();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
			}

			LList *CurOwner = l->GetList();
			if (Add ^ (CurOwner != NULL))
			{
				if (Add)
					Ls.Insert(l);
				else
					d->Commits->Remove(l);
			}
		}

		PROF("Ls Ins");
		d->Commits->Insert(Ls);
		if (d->Resort >= 0)
		{
			PROF("Resort");
			d->Commits->Sort(LstCmp, d->Resort);
			d->Resort = -1;
		}

		PROF("ColSizing");
		if (d->Commits->Length() > MAX_AUTO_RESIZE_ITEMS)
		{
			int i = 0;
			if (GetType() == VcHg && d->Commits->GetColumns() >= 7)
			{
				d->Commits->ColumnAt(i++)->Width(60); // LGraph
				d->Commits->ColumnAt(i++)->Width(40); // LIndex
				d->Commits->ColumnAt(i++)->Width(100); // LRevision
				d->Commits->ColumnAt(i++)->Width(60); // LBranch
				d->Commits->ColumnAt(i++)->Width(240); // LAuthor
				d->Commits->ColumnAt(i++)->Width(130); // LTimeStamp
				d->Commits->ColumnAt(i++)->Width(400); // LMessage
			}
			else if (d->Commits->GetColumns() >= 5)
			{
				d->Commits->ColumnAt(i++)->Width(40); // LGraph
				d->Commits->ColumnAt(i++)->Width(270); // LRevision
				d->Commits->ColumnAt(i++)->Width(240); // LAuthor
				d->Commits->ColumnAt(i++)->Width(130); // LTimeStamp
				d->Commits->ColumnAt(i++)->Width(400); // LMessage
			}
		}
		else d->Commits->ResizeColumnsToContent();

		PROF("UpdateAll");
		d->Commits->UpdateAllItems();

		PROF("GetCur");
		GetCurrentRevision();
	}
}


int CommitRevCmp(VcCommit **a, VcCommit **b)
{
	int64 arev = Atoi((*a)->GetRev());
	int64 brev = Atoi((*b)->GetRev());
	int64 diff = (int64)brev - arev;
	if (diff < 0) return -1;
	return (diff > 0) ? 1 : 0;
}

int CommitIndexCmp(VcCommit **a, VcCommit **b)
{
	auto ai = (*a)->GetIndex();
	auto bi = (*b)->GetIndex();
	auto diff = (int64)bi - ai;
	if (diff < 0) return -1;
	return (diff > 0) ? 1 : 0;
}

int CommitDateCmp(VcCommit **a, VcCommit **b)
{
	uint64 ats, bts;
	(*a)->GetTs().Get(ats);
	(*b)->GetTs().Get(bts);
	int64 diff = (int64)bts - ats;
	if (diff < 0) return -1;
	return (diff > 0) ? 1 : 0;
}

void VcFolder::GetCurrentRevision(ParseParams *Params)
{
	if (CurrentCommit || IsIdent != StatusNone)
		return;
	
	switch (GetType())
	{
		case VcGit:
			if (StartCmd("rev-parse HEAD", &VcFolder::ParseInfo, Params))
				IsIdent = StatusActive;
			break;
		case VcSvn:
			if (StartCmd("info", &VcFolder::ParseInfo, Params))
				IsIdent = StatusActive;
			break;
		case VcHg:
			if (StartCmd("id -i -n", &VcFolder::ParseInfo, Params))
				IsIdent = StatusActive;
			break;
		case VcCvs:
			break;
		default:
			break;
	}
}

bool VcFolder::GetBranches(ParseParams *Params)
{
	if (Branches.Length() > 0 || IsBranches != StatusNone)
		return true;

	switch (GetType())
	{
		case VcGit:
			if (StartCmd("branch -a", &VcFolder::ParseBranches, Params))
				IsBranches = StatusActive;
			break;
		case VcSvn:
			Branches.Add("trunk", new VcBranch("trunk"));
			OnBranchesChange();
			break;
		case VcHg:
			if (StartCmd("branch", &VcFolder::ParseBranches, Params))
				IsBranches = StatusActive;
			break;
		case VcCvs:
			break;
		default:
			break;
	}

	return false;
}

bool VcFolder::ParseRevList(int Result, GString s, ParseParams *Params)
{
	/*
	LHashTbl<StrKey<char>, int> Map(0, -1);
	for (unsigned i=0; i<Log.Length(); i++)
		Map.Add(Log[i]->GetRev(), i);
	*/
	Log.DeleteObjects();

	int Errors = 0;
	switch (GetType())
	{
		case VcGit:
		{
			GString::Array Commits;
			Commits.SetFixedLength(false);
			
			// Split on the NULL chars...
			char *c = s.Get();
			char *e = c + s.Length();
			while (c < e)
			{
				char *nul = c;
				while (nul < e && *nul) nul++;
				if (nul <= c) break;
				Commits.New().Set(c, nul-c);
				if (nul >= e) break;
				c = nul + 1;
			}

			for (auto Commit: Commits)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				if (Rev->GitParse(Commit, true))
				{
					Log.Add(Rev.Release());
				}
				else
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, Commit.Get());
					Errors++;
				}
			}

			// Log.Sort(CommitDateCmp);
			LinkParents();
			break;
		}
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	IsLogging = false;
	return true;
}

GString VcFolder::GetFilePart(const char *uri)
{
	GUri u(uri);
	GString File = u.IsFile() ? GString(u.LocalPath()) : u.sPath(Uri.sPath.Length(), -1).LStrip("/");
	return File;
}

void VcFolder::LogFile(const char *uri)
{
	GString Args;
	
	switch (GetType())
	{
		case VcSvn:
		case VcHg:
		{
			GString File = GetFilePart(uri);
			ParseParams *Params = new ParseParams(uri);
			Args.Printf("log \"%s\"", File.Get());
			IsLogging = StartCmd(Args, &VcFolder::ParseLog, Params, LogNormal);
			break;
		}
		/*
		case VcGit:
			break;
		case VcCvs:
			break;
		*/
		default:
			LgiAssert(!"Impl me.");
			break;
	}
}

VcLeaf *VcFolder::Find(const char *Path)
{
	VcLeaf *r = NULL;	
	for (auto n = GetChild(); !r && n; n = n->GetNext())
	{
		auto l = dynamic_cast<VcLeaf*>(n);
		if (l)
			r = l->Find(Path);
	}
	
	return r;
}

bool VcFolder::ParseLog(int Result, GString s, ParseParams *Params)
{
	LHashTbl<StrKey<char>, VcCommit*> Map;
	for (auto pc: Log)
		Map.Add(pc->GetRev(), pc);

	int Skipped = 0, Errors = 0;
	VcLeaf *File = Params ? Find(Params->Str) : NULL;
	GArray<VcCommit*> *Out = File ? &File->Log : &Log;

	switch (GetType())
	{
		case VcGit:
		{
			GString::Array c;
			c.SetFixedLength(false);
			char *prev = s.Get();

			for (char *i = s.Get(); *i; )
			{
				if (!strnicmp(i, "commit ", 7))
				{
					if (i > prev)
					{
						c.New().Set(prev, i - prev);
						// LgiTrace("commit=%i\n", (int)(i - prev));
					}
					prev = i;
				}

				while (*i)
				{
					if (*i++ == '\n')
						break;
				}
			}

			for (unsigned i=0; i<c.Length(); i++)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				if (Rev->GitParse(c[i], false))
				{
					if (!Map.Find(Rev->GetRev()))
						Out->Add(Rev.Release());
					else
						Skipped++;
				}
				else
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, c[i].Get());
					Errors++;
				}
			}
			
			Out->Sort(CommitDateCmp);
			break;
		}
		case VcSvn:
		{
			GString::Array c = s.Split("------------------------------------------------------------------------");
			for (unsigned i=0; i<c.Length(); i++)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				GString Raw = c[i].Strip();
				if (Rev->SvnParse(Raw))
				{
					if (File || !Map.Find(Rev->GetRev()))
						Out->Add(Rev.Release());
					else
						Skipped++;
				}
				else if (Raw)
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, Raw.Get());
					Errors++;
				}
			}
			
			Out->Sort(CommitRevCmp);
			break;
		}
		case VcHg:
		{
			GString::Array c = s.Split("\n\n");
			LHashTbl<IntKey<int64_t>, VcCommit*> Idx;
			for (auto &Commit: c)
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				if (Rev->HgParse(Commit))
				{
					auto Existing = File ? NULL : Map.Find(Rev->GetRev());
					if (!Existing)
						Out->Add(Existing = Rev.Release());
					if (Existing->GetIndex() >= 0)
						Idx.Add(Existing->GetIndex(), Existing);
				}
			}

			if (!File)
			{
				// Patch all the trivial parents...
				for (auto c: Log)
				{
					if (c->GetParents()->Length() > 0)
						continue;

					auto CIdx = c->GetIndex();
					if (CIdx <= 0)
						continue;

					auto Par = Idx.Find(CIdx - 1);
					if (Par)
						c->GetParents()->Add(Par->GetRev());
				}
			}

			Out->Sort(CommitIndexCmp);
			
			if (!File)
				LinkParents();

			d->Resort = 1;
			break;
		}
		case VcCvs:
		{
			if (Result)
			{
				OnCmdError(s, "Cvs command failed.");
				break;
			}

			LHashTbl<IntKey<uint64>, VcCommit*> Map;
			GString::Array c = s.Split("=============================================================================");
			for (auto &Commit: c)
			{
				if (Commit.Strip().Length())
				{
					GString Head, File;
					GString::Array Versions = Commit.Split("----------------------------");
					GString::Array Lines = Versions[0].SplitDelimit("\r\n");
					for (auto &Line: Lines)
					{
						GString::Array p = Line.Split(":", 1);
						if (p.Length() == 2)
						{
							// LgiTrace("Line: %s\n", Line->Get());

							GString Var = p[0].Strip().Lower();
							GString Val = p[1].Strip();
							if (Var.Equals("branch"))
							{
								if (Val.Length())
									Branches.Add(Val, new VcBranch(Val));
							}
							else if (Var.Equals("head"))
							{
								Head = Val;
							}
							else if (Var.Equals("rcs file"))
							{
								GString::Array f = Val.SplitDelimit(",");
								File = f.First();
							}
						}
					}

					// LgiTrace("%s\n", Commit->Get());

					for (unsigned i=1; i<Versions.Length(); i++)
					{
						GString::Array Lines = Versions[i].SplitDelimit("\r\n");
						if (Lines.Length() >= 3)
						{
							GString Ver = Lines[0].Split(" ").Last();
							GString::Array a = Lines[1].SplitDelimit(";");
							GString Date = a[0].Split(":", 1).Last().Strip();
							GString Author = a[1].Split(":", 1).Last().Strip();
							GString Id = a[2].Split(":", 1).Last().Strip();
							GString Msg = Lines[2];
							LDateTime Dt;
							
							if (Dt.Parse(Date))
							{
								uint64 Ts;
								if (Dt.Get(Ts))
								{
									VcCommit *Cc = Map.Find(Ts);
									if (!Cc)
									{
										Map.Add(Ts, Cc = new VcCommit(d, this));
										Out->Add(Cc);
										Cc->CvsParse(Dt, Author, Msg);
									}
									Cc->Files.Add(File.Get());
								}
								else LgiAssert(!"NO ts for date.");
							}
							else LgiAssert(!"Date parsing failed.");
						}
					}
				}
			}
			break;
		}
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	if (File)
		File->ShowLog();

	// LgiTrace("%s:%i - ParseLog: Skip=%i, Error=%i\n", _FL, Skipped, Errors);
	IsLogging = false;

	return !Result;
}

void VcFolder::LinkParents()
{
	#if PROFILE_FN
	GProfile Prof("LinkParents");
	#endif	
	LHashTbl<StrKey<char>,VcCommit*> Map;
	
	// Index all the commits
	int i = 0;
	for (auto c:Log)
	{
		c->Idx = i++;
		c->NodeIdx = -1;
		Map.Add(c->GetRev(), c);
	}

	// Create all the edges...
	PROF("Create edges.");
	for (auto c:Log)
	{
		auto *Par = c->GetParents();
		for (auto &pRev : *Par)
		{
			auto *p = Map.Find(pRev);
			if (p)
				new VcEdge(p, c);
			#if 0
			else
				return;
			#endif
		}
	}

	// Map the edges to positions
	PROF("Map edges.");
	typedef GArray<VcEdge*> EdgeArr;
	GArray<EdgeArr> Active;
	for (auto c:Log)
	{
		for (unsigned i=0; c->NodeIdx<0 && i<Active.Length(); i++)
		{
			for (auto e:Active[i])
			{
				if (e->Parent == c)
				{
					c->NodeIdx = i;
					break;
				}
			}
		}

		// Add starting edges to active set
		for (auto e:c->Edges)
		{
			if (e->Child == c)
			{
				if (c->NodeIdx < 0)
					c->NodeIdx = (int)Active.Length();

				e->Idx = c->NodeIdx;
				c->Pos.Add(e, e->Idx);
				Active[e->Idx].Add(e);
			}
		}

		// Now for all active edges... assign positions
		for (unsigned i=0; i<Active.Length(); i++)
		{
			EdgeArr &Edges = Active[i];
			for (unsigned n=0; n<Edges.Length(); n++)
			{
				auto e = Edges[n];

				if (c == e->Child || c == e->Parent)
				{
					LgiAssert(c->NodeIdx >= 0);
					c->Pos.Add(e, c->NodeIdx);
				}
				else
				{
					// May need to untangle edges with different parents here
					bool Diff = false;
					for (auto edge: Edges)
					{
						if (edge != e &&
							edge->Child != c &&
							edge->Parent != e->Parent)
						{
							Diff = true;
							break;
						}
					}
					if (Diff)
					{
						int NewIndex = -1;
						
						// Look through existing indexes for a parent match
						for (unsigned ii=0; ii<Active.Length(); ii++)
						{
							if (ii == i) continue;
							// Match e->Parent?
							bool Match = true;
							for (auto ee:Active[ii])
							{
								if (ee->Parent != e->Parent)
								{
									Match = false;
									break;
								}
							}
							if (Match)
								NewIndex = ii;
						}

						if (NewIndex < 0)
							// Create new index for this parent
							NewIndex = (int)Active.Length();

						Edges.Delete(e);
						Active[NewIndex].Add(e);
						e->Idx = NewIndex;
						c->Pos.Add(e, NewIndex);
						n--;
					}
					else
					{
						LgiAssert(e->Idx == i);
						c->Pos.Add(e, i);
					}
				}
			}
		}
		
		// Process terminating edges
		for (auto e:c->Edges)
		{
			if (e->Parent == c)
			{
				if (e->Idx < 0)
				{
					// This happens with out of order commits..?
					continue;
				}

				int i = e->Idx;
				if (c->NodeIdx < 0)
					c->NodeIdx = i;

				LgiAssert(Active[i].HasItem(e));
				Active[i].Delete(e);
			}
		}

		// Collapse any empty active columns
		for (unsigned i=0; i<Active.Length(); i++)
		{
			if (Active[i].Length() == 0)
			{
				// No more edges using this index, bump any higher ones down
				Active.DeleteAt(i, true);
				for (int n=i; n<Active.Length(); n++)
				{
					for (auto edge:Active[n])
					{
						LgiAssert(edge->Idx > 0);
						edge->Idx--;
						c->Pos.Add(edge, edge->Idx);
					}
				}
				i--;
			}
		}
	}

	// Find all the "heads", i.e. a commit without any children
	PROF("Find heads.");
	GCombo *Heads;
	if (d->Files->GetWindow()->GetViewById(IDC_HEADS, Heads))
	{
		Heads->Empty();
		for (auto c:Log)
		{
			bool Has = false;
			for (auto e:c->Edges)
			{
				if (e->Parent == c)
				{
					Has = true;
					break;
				}
			}
			if (!Has)
				Heads->Insert(c->GetRev());
		}
		Heads->SendNotify(GNotifyTableLayout_Refresh);
	}
}

VcFile *VcFolder::FindFile(const char *Path)
{
	if (!Path)
		return NULL;

	GArray<VcFile*> Files;
	if (d->Files->GetAll(Files))
	{
		GString p = Path;
		p = p.Replace(DIR_STR, "/");
		for (auto f : Files)
		{
			auto Fn = f->GetFileName();
			if (p.Equals(Fn))
				return f;
		}
	}

	return NULL;
}

void VcFolder::OnCmdError(GString Output, const char *Msg)
{
	if (!CmdErrors)
	{
		if (Output.Length())
			d->Log->Write(Output, Output.Length());

		auto vc_name = GetVcName();
		if (vc_name)
		{
			GString::Array a = GetProgramsInPath(GetVcName());
			d->Log->Print("'%s' executables in the path:\n", GetVcName());
			for (auto Bin : a)
				d->Log->Print("    %s\n", Bin.Get());
		}
		else if (Msg)
		{
			d->Log->Print("%s\n", Msg);
		}
	}
				
	CmdErrors++;
	d->Tabs->Value(1);
	GetCss(true)->Color(GColour::Red);
}

void VcFolder::ClearError()
{
	GetCss(true)->Color(GCss::ColorInherit);
}

bool VcFolder::ParseInfo(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		case VcHg:
		{
			auto p = s.Strip().SplitDelimit();
			CurrentCommit = p[0].Strip(" \t\r\n+");
			if (p.Length() > 1)
				CurrentCommitIdx = p[1].Int();
			else
				CurrentCommitIdx = -1;

			if (Params && Params->Str.Equals("CountToTip"))
				CountToTip();
			break;
		}
		case VcSvn:
		{
			if (s.Find("client is too old") >= 0)
			{
				OnCmdError(s, "Client too old");
				break;
			}
			
			GString::Array c = s.Split("\n");
			for (unsigned i=0; i<c.Length(); i++)
			{
				GString::Array a = c[i].SplitDelimit(":", 1);
				if (a.First().Strip().Equals("Revision"))
					CurrentCommit = a[1].Strip();
				else if (a.First().Strip().Equals("URL"))
					RepoUrl = a[1].Strip();
			}
			break;
		}			
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	IsIdent = Result ? StatusError : StatusNone;

	return true;
}

bool VcFolder::ParseUpdate(int Result, GString s, ParseParams *Params)
{
	if (Result == 0)
	{
		CurrentCommit = NewRev;
	}
	
	IsUpdate = false;
	return true;
}

bool VcFolder::ParseWorking(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcSvn:
		case VcHg:
		{
			ParseParams Local;
			if (!Params) Params = &Local;
			Params->IsWorking = true;
			ParseStatus(Result, s, Params);
			break;
		}
		case VcCvs:
		{
			bool Untracked = d->IsMenuChecked(IDM_UNTRACKED);
			if (Untracked)
			{
				auto Lines = s.SplitDelimit("\n");
				for (auto Ln: Lines)
				{
					auto p = Ln.SplitDelimit(" \t", 1);
					if (p.Length() > 1)
					{
						auto f = new VcFile(d, this, NULL, true);
						f->SetText(p[0], COL_STATE);
						f->SetText(p[1], COL_FILENAME);
						f->GetStatus();
						d->Files->Insert(f);
					}
				}
			}
			// else fall thru
		}
		default:
		{
			ParseDiffs(s, NULL, true);
			break;
		}
	}
	
	IsWorkingFld = false;
	d->Files->ResizeColumnsToContent();

	if (GetType() == VcSvn)
	{
		Unpushed = d->Files->Length() > 0 ? 1 : 0;
		Update();
	}

	return false;
}

void VcFolder::DiffRange(const char *FromRev, const char *ToRev)
{
	if (!FromRev || !ToRev)
		return;

	switch (GetType())
	{
		case VcSvn:
		{
			ParseParams *p = new ParseParams;
			p->IsWorking = false;
			p->Str = GString(FromRev) + ":" + ToRev;

			GString a;
			a.Printf("diff -r%s:%s", FromRev, ToRev);
			StartCmd(a, &VcFolder::ParseDiff, p);
			break;
		}
		case VcCvs:
		case VcGit:
		case VcHg:
		default:
			LgiAssert(!"Impl me.");
			break;
	}
}

bool VcFolder::ParseDiff(int Result, GString s, ParseParams *Params)
{
	if (Params)
		ParseDiffs(s, Params->Str, Params->IsWorking);
	else
		ParseDiffs(s, NULL, true);
	return false;
}

void VcFolder::Diff(VcFile *file)
{
	auto Fn = file->GetFileName();
	if (!Fn ||
		!Stricmp(Fn, ".") ||
		!Stricmp(Fn, ".."))
		return;

	switch (GetType())
	{
		case VcGit:
		case VcHg:
		{
			GString a;

			if (file->GetRevision())
				LgiAssert(!"impl the revision cmd line arg.");

			a.Printf("diff \"%s\"", Fn);
			StartCmd(a, &VcFolder::ParseDiff);
			break;
		}
		case VcSvn:
		{
			GString a;
			if (file->GetRevision())
				a.Printf("diff -r %s \"%s\"", file->GetRevision(), Fn);
			else
				a.Printf("diff \"%s\"", Fn);
			StartCmd(a, &VcFolder::ParseDiff);
			break;
		}
		case VcCvs:
			break;
		default:
			LgiAssert(!"Impl me.");
			break;
	}
}

bool VcFolder::ParseDiffs(GString s, GString Rev, bool IsWorking)
{
	LgiAssert(IsWorking || Rev.Get() != NULL);

	switch (GetType())
	{
		case VcGit:
		{
			GString::Array a = s.Split("\n");
			GString Diff;
			VcFile *f = NULL;
			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "diff", 4))
				{
					if (f)
						f->SetDiff(Diff);
					Diff.Empty();

					auto Bits = a[i].SplitDelimit();
					GString Fn, State = "M";
					if (Bits[1].Equals("--cc"))
					{
						Fn = Bits.Last();
						State = "C";
					}
					else
						Fn = Bits.Last()(2,-1);

					// LgiTrace("%s\n", a[i].Get());

					f = FindFile(Fn);
					if (!f)
						f = new VcFile(d, this, Rev, IsWorking);

					f->SetText(State, COL_STATE);
					f->SetText(Fn.Replace("\\","/"), COL_FILENAME);
					f->GetStatus();
					d->Files->Insert(f);
				}
				else if (!_strnicmp(Ln, "new file", 8))
				{
					if (f)
						f->SetText("A", COL_STATE);
				}
				else if (!_strnicmp(Ln, "index", 5) ||
						 !_strnicmp(Ln, "commit", 6)   ||
						 !_strnicmp(Ln, "Author:", 7)   ||
						 !_strnicmp(Ln, "Date:", 5)   ||
						 !_strnicmp(Ln, "+++", 3)   ||
						 !_strnicmp(Ln, "---", 3))
				{
					// Ignore
				}
				else
				{
					if (Diff) Diff += "\n";
					Diff += a[i];
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			break;
		}
		case VcHg:
		{
			GString::Array a = s.Split("\n");
			GString Diff;
			VcFile *f = NULL;
			List<LListItem> Files;

			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "diff", 4))
				{
					if (f)
						f->SetDiff(Diff);
					Diff.Empty();

					auto MainParts = a[i].Split(" -r ");
					auto FileParts = MainParts.Last().Split(" ",1);
					GString Fn = FileParts.Last();
					// GString Status = "M";

					f = FindFile(Fn);
					if (!f)
						f = new VcFile(d, this, Rev, IsWorking);

					// printf("a='%s'\n", a[i].Get());

					f->SetText(Fn.Replace("\\","/"), COL_FILENAME);
					// f->SetText(Status, COL_STATE);
					Files.Insert(f);
				}
				else if (!_strnicmp(Ln, "index", 5) ||
						 !_strnicmp(Ln, "commit", 6)   ||
						 !_strnicmp(Ln, "Author:", 7)   ||
						 !_strnicmp(Ln, "Date:", 5)   ||
						 !_strnicmp(Ln, "+++", 3)   ||
						 !_strnicmp(Ln, "---", 3))
				{
					// Ignore
				}
				else
				{
					if (Diff) Diff += "\n";
					Diff += a[i];
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			d->Files->Insert(Files);
			break;
		}
		case VcSvn:
		{
			GString::Array a = s.Replace("\r").Split("\n");
			GString Diff;
			VcFile *f = NULL;
			bool InPreamble = false;
			bool InDiff = false;
			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "Index:", 6))
				{
					if (f)
					{
						f->SetDiff(Diff);
						f->Select(false);
					}
					Diff.Empty();
					InDiff = false;
					InPreamble = false;

					GString Fn = a[i].Split(":", 1).Last().Strip();

					f = FindFile(Fn);
					if (!f)
						f = new VcFile(d, this, Rev, IsWorking);

					f->SetText(Fn.Replace("\\","/"), COL_FILENAME);
					f->SetText("M", COL_STATE);
					f->GetStatus();
					d->Files->Insert(f);
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
					if (!strncmp(Ln, "--- ", 4) ||
						!strncmp(Ln, "+++ ", 4))
					{
					}
					else
					{
						if (Diff) Diff += "\n";
						Diff += a[i];
					}
				}
			}
			if (f && Diff)
			{
				f->SetDiff(Diff);
				Diff.Empty();
			}
			break;
		}
		case VcCvs:
		{
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

bool VcFolder::ParseFiles(int Result, GString s, ParseParams *Params)
{
	d->ClearFiles();
	ParseDiffs(s, Params->Str, false);
	IsFilesCmd = false;
	d->Files->ResizeColumnsToContent();

	return false;
}

void VcFolder::OnSshCmd(SshParams *p)
{
	if (!p || !p->f)
	{
		LgiAssert(!"Param error.");
		return;
	}

	GString s = p->Output;
	int Result = p->ExitCode;
	if (Result == ErrSubProcessFailed)
	{
		CmdErrors++;
	}
	else if (p->Parser)
	{
		bool Reselect = CALL_MEMBER_FN(*this, p->Parser)(Result, s, p->Params);
		if (Reselect)
		{
			if (GTreeItem::Select())
				Select(true);
		}
	}
}

void VcFolder::OnPulse()
{
	bool Reselect = false, CmdsChanged = false;
	static bool Processing = false;
	
	// printf("%s:%i - OnPulse\n", _FL);
	if (!Processing)
	{	
		Processing = true; // Lock out processing, if it puts up a dialog or something...
		// bad things happen if we try and re-process something.	
		for (unsigned i=0; i<Cmds.Length(); i++)
		{
			Cmd *c = Cmds[i];
			if (c)
			{
				bool Ex = c->Rd->IsExited();
				// printf("%s:%i - Ex=%i, Cmds=%i\n", _FL, (int)Cmds.Length());
				if (Ex)
				{
					GString s = c->GetBuf();
					int Result = c->Rd->ExitCode();
					if (Result == ErrSubProcessFailed)
					{
						if (!CmdErrors)
							d->Log->Print("Error: Can't run '%s'\n", GetVcName());
						CmdErrors++;
					}
					else if (c->PostOp)
					{
						Reselect |= CALL_MEMBER_FN(*this, c->PostOp)(Result, s, c->Params);
					}
					
					Cmds.DeleteAt(i--, true);
					delete c;
					CmdsChanged = true;
				}
			}
		}
		Processing = false;
	}

	if (Reselect)
	{
		if (GTreeItem::Select())
			Select(true);
	}
	if (CmdsChanged)
		Update();
	if (CmdErrors)
	{
		d->Tabs->Value(1);
		CmdErrors = false;
	}
}

void VcFolder::OnRemove()
{
	GXmlTag *t = d->Opts.LockTag(OPT_Folders, _FL);
	if (t)
	{
		Uncommit.Reset();
		if (GTreeItem::Select())
		{
			d->Files->Empty();
			d->Commits->RemoveAll();
		}

		bool Found = false;
		auto u = Uri.ToString();
		for (auto c: t->Children)
		{
			if (c->IsTag(OPT_Folder) &&
				c->GetContent())
			{
				auto Content = c->GetContent();				
				if (!_stricmp(Content, u))
				{
					c->RemoveTag();
					delete c;
					Found = true;
					break;
				}
			}
		}
		LgiAssert(Found);

		d->Opts.Unlock();
	}
}

void VcFolder::Empty()
{
	Type = VcNone;
	
	IsCommit = false;
	IsLogging = false;
	IsUpdate = false;
	IsFilesCmd = false;
	IsWorkingFld = false;
	CommitListDirty = false;
	IsUpdatingCounts = false;
	IsBranches = StatusNone;
	IsIdent = StatusNone;

	Unpushed = Unpulled = -1;
	CmdErrors = 0;
	CurrentCommitIdx = -1;

	CurrentCommit.Empty();
	RepoUrl.Empty();
	VcCmd.Empty();
	Uncommit.Reset();
	Log.DeleteObjects();

	d->Commits->Empty();
	d->Files->Empty();

	if (!Uri.IsFile())	
		GetCss(true)->Color(GColour::Blue);
}

void VcFolder::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		LSubMenu s;
		s.AppendItem("Browse To", IDM_BROWSE_FOLDER, Uri.IsFile());
		s.AppendItem(
			#ifdef WINDOWS
			"Command Prompt At",
			#else
			"Terminal At",
			#endif
			IDM_TERMINAL, Uri.IsFile());
		s.AppendItem("Clean", IDM_CLEAN);
		s.AppendSeparator();
		s.AppendItem("Pull", IDM_PULL);
		s.AppendItem("Status", IDM_STATUS);
		s.AppendItem("Push", IDM_PUSH);
		s.AppendItem("Update Subs", IDM_UPDATE_SUBS, GetType() == VcGit);
		s.AppendSeparator();
		s.AppendItem("Remove", IDM_REMOVE);
		if (!Uri.IsFile())
		{
			s.AppendSeparator();
			s.AppendItem("Edit Location", IDM_EDIT);
		}
		int Cmd = s.Float(GetTree(), m);
		switch (Cmd)
		{
			case IDM_BROWSE_FOLDER:
			{
				LgiBrowseToFile(LocalPath());
				break;
			}
			case IDM_TERMINAL:
			{
				TerminalAt(LocalPath());
				break;
			}
			case IDM_CLEAN:
			{
				Clean();
				break;
			}
			case IDM_PULL:
			{
				Pull();
				break;
			}
			case IDM_STATUS:
			{
				FolderStatus();
				break;
			}
			case IDM_PUSH:
			{
				Push();
				break;
			}
			case IDM_UPDATE_SUBS:
			{
				UpdateSubs();
				break;
			}
			case IDM_REMOVE:
			{
				OnRemove();
				delete this;
				break;
			}
			case IDM_EDIT:
			{
				GInput Dlg(GetTree(), Uri.ToString(), "URI:", "Remote Folder Location");
				if (Dlg.DoModal())
				{
					Uri.Set(Dlg.GetStr());
					Empty();
					Select(true);
				}
				break;
			}
			default:
				break;
		}
	}
}

void VcFolder::OnUpdate(const char *Rev)
{
	if (!Rev)
		return;
	
	if (!IsUpdate)
	{
		GString Args;

		NewRev = Rev;
		switch (GetType())
		{
			case VcGit:
				Args.Printf("checkout %s", Rev);
				IsUpdate = StartCmd(Args, &VcFolder::ParseUpdate, NULL, LogNormal);
				break;
			case VcSvn:
				Args.Printf("up -r %s", Rev);
				IsUpdate = StartCmd(Args, &VcFolder::ParseUpdate, NULL, LogNormal);
				break;
			case VcHg:
				Args.Printf("update -r %s", Rev);
				IsUpdate = StartCmd(Args, &VcFolder::ParseUpdate, NULL, LogNormal);
				break;
			default:
			{
				LgiAssert(!"Impl me.");
				break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
int FolderCompare(GTreeItem *a, GTreeItem *b, NativeInt UserData)
{
	VcLeaf *A = dynamic_cast<VcLeaf*>(a);
	VcLeaf *B = dynamic_cast<VcLeaf*>(b);
	if (!A || !B)
		return 0;
	return A->Compare(B);
}

struct SshFindEntry
{
	GString Flags, Name, User, Group;
	uint64_t Size;
	LDateTime Modified, Access;

	SshFindEntry &operator =(const GString &s)
	{
		auto p = s.SplitDelimit("/");
		if (p.Length() == 7)
		{
			Flags = p[0];
			Group = p[1];
			User = p[2];
			Access.Set((uint64_t) p[3].Int());
			Modified.Set((uint64_t) p[4].Int());
			Size = p[5].Int();
			Name = p[6];
		}

		return *this;
	}

	bool IsDir() { return Flags(0) == 'd'; }
	bool IsHidden() { return Name(0) == '.'; }
	const char *GetName() { return Name; }
	static int Compare(SshFindEntry *a, SshFindEntry *b) { return Stricmp(a->Name.Get(), b->Name.Get()); }
};

bool VcFolder::ParseRemoteFind(int Result, GString s, ParseParams *Params)
{
	if (!Params || !s)
		return false;

	auto Parent = Params->Leaf ? static_cast<GTreeItem*>(Params->Leaf) : static_cast<GTreeItem*>(this);
	GUri u(Params->Str);

	auto Lines = s.SplitDelimit("\r\n");
	GArray<SshFindEntry> Entries;
	for (size_t i=1; i<Lines.Length(); i++)
	{
		auto &e = Entries.New();
		e = Lines[i];
		if (!e.Name)
			Entries.PopLast();
	}
	Entries.Sort(SshFindEntry::Compare);

	for (auto &Dir: Entries)
	{
		if (Dir.IsDir())
		{
			if (Dir.GetName()[0] != '.')
			{
				new VcLeaf(this, Parent, Params->Str, Dir.GetName(), true);
			}
		}
		else if (!Dir.IsHidden())
		{
			char *Ext = LgiGetExtension(Dir.GetName());
			if (!Ext) continue;
			if (!stricmp(Ext, "c") ||
				!stricmp(Ext, "cpp") ||
				!stricmp(Ext, "h"))
			{
				GUri Path = u;
				Path += Dir.GetName();
				new VcLeaf(this, Parent, Params->Str, Dir.GetName(), false);
			}
		}
	}

	return false;
}

void VcFolder::ReadDir(GTreeItem *Parent, const char *ReadUri)
{
	GUri u(ReadUri);

	if (u.IsFile())
	{
		// Read child items
		GDirectory Dir;
		for (int b = Dir.First(u.LocalPath()); b; b = Dir.Next())
		{
			if (Dir.IsDir())
			{
				if (Dir.GetName()[0] != '.')
				{
					new VcLeaf(this, Parent, u.ToString(), Dir.GetName(), true);
				}
			}
			else if (!Dir.IsHidden())
			{
				char *Ext = LgiGetExtension(Dir.GetName());
				if (!Ext) continue;
				if (!stricmp(Ext, "c") ||
					!stricmp(Ext, "cpp") ||
					!stricmp(Ext, "h"))
				{
					GUri Path = u;
					Path += Dir.GetName();
					new VcLeaf(this, Parent, u.ToString(), Dir.GetName(), false);
				}
			}
		}
	}
	else
	{
		auto c = d->GetConnection(ReadUri);
		if (!c)
			return;
		
		GString Path = u.sPath(Uri.sPath.Length(), -1).LStrip("/");
		GString Args;
		Args.Printf("\"%s\" -maxdepth 1 -printf \"%%M/%%g/%%u/%%A@/%%T@/%%s/%%P\n\"", Path ? Path.Get() : ".");

		auto *Params = new ParseParams(ReadUri);
		Params->Leaf = dynamic_cast<VcLeaf*>(Parent);

		c->Command(this, "find", Args, &VcFolder::ParseRemoteFind, Params);
		return;
	}

	Parent->SortChildren(FolderCompare);
}

void VcFolder::OnVcsType()
{
	if (!d)
	{
		LgiAssert(!"No priv instance");
		return;
	}
	auto c = d->GetConnection(Uri.ToString(), false);
	if (c)
	{
		auto NewType = c->Types.Find(Uri.sPath(1, -1));
		if (NewType && NewType != Type)
		{
			Type = NewType;
			ClearError();
			Update();

			if (GTreeItem::Select())
				Select(true);
		}
	}
}

void VcFolder::OnExpand(bool b)
{
	if (Tmp && b)
	{
		Tmp->Remove();
		DeleteObj(Tmp);
		ReadDir(this, Uri.ToString());
	}
}

void VcFolder::ListCommit(VcCommit *c)
{
	if (!IsFilesCmd)
	{
		GString Args;
		switch (GetType())
		{
			case VcGit:
				// Args.Printf("show --oneline --name-only %s", Rev);
				Args.Printf("show %s", c->GetRev());
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles, new ParseParams(c->GetRev()));
				break;
			case VcSvn:
				Args.Printf("log --verbose --diff -r %s", c->GetRev());
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles, new ParseParams(c->GetRev()));
				break;
			case VcCvs:
			{
				d->ClearFiles();
				for (unsigned i=0; i<c->Files.Length(); i++)
				{				
					VcFile *f = new VcFile(d, this, c->GetRev(), false);
					if (f)
					{
						f->SetText(c->Files[i], COL_FILENAME);
						d->Files->Insert(f);
					}
				}
				d->Files->ResizeColumnsToContent();
				break;
			}
			case VcHg:
			{
				Args.Printf("diff --change %s", c->GetRev());
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles, new ParseParams(c->GetRev()));
				break;
			}
			default:
				LgiAssert(!"Impl me.");
				break;
		}

		if (IsFilesCmd)
			d->ClearFiles();
	}
}

GString ConvertUPlus(GString s)
{
	GArray<uint32_t> c;
	GUtf8Ptr p(s);
	int32 ch;
	while ((ch = p))
	{
		if (ch == '{')
		{
			auto n = p.GetPtr();
			if (n[1] == 'U' &&
				n[2] == '+')
			{
				// Convert unicode code point
				p += 3;
				ch = (int32)htoi(p.GetPtr());
				c.Add(ch);
				while ((ch = p) != '}')
					p++;
			}
			else c.Add(ch);
		}
		else c.Add(ch);
		p++;
	}
	
	c.Add(0);
	#ifdef LINUX
	return GString((char16*)c.AddressOf());
	#else
	return GString(c.AddressOf());
	#endif
}

bool VcFolder::ParseStatus(int Result, GString s, ParseParams *Params)
{
	bool ShowUntracked = d->Files->GetWindow()->GetCtrlValue(IDC_UNTRACKED) != 0;
	bool IsWorking = Params ? Params->IsWorking : false;
	List<LListItem> Ins;

	switch (GetType())
	{
		case VcCvs:
		{
			LHashTbl<ConstStrKey<char>,VcFile*> Map;
			for (auto i: *d->Files)
			{
				VcFile *f = dynamic_cast<VcFile*>(i);
				if (f)
					Map.Add(f->GetText(COL_FILENAME), f);
			}

			#if 0
			GFile Tmp("C:\\tmp\\output.txt", O_WRITE);
			Tmp.Write(s);
			Tmp.Close();
			#endif

			GString::Array a = s.Split("===================================================================");
			for (auto i : a)
			{
				GString::Array Lines = i.SplitDelimit("\r\n");
				if (Lines.Length() == 0)
					continue;
				GString f = Lines[0].Strip();
				if (f.Find("File:") == 0)
				{
					GString::Array Parts = f.SplitDelimit("\t");
					GString File = Parts[0].Split(": ").Last().Strip();
					GString Status = Parts[1].Split(": ").Last();
					GString WorkingRev;

					for (auto l : Lines)
					{
						GString::Array p = l.Strip().Split(":", 1);
						if (p.Length() > 1 &&
							p[0].Strip().Equals("Working revision"))
						{
							WorkingRev = p[1].Strip();
						}
					}

					VcFile *f = Map.Find(File);
					if (!f)
					{
						if ((f = new VcFile(d, this, WorkingRev, IsWorking)))
							Ins.Insert(f);
					}
					if (f)
					{
						f->SetText(Status, COL_STATE);
						f->SetText(File, COL_FILENAME);
						f->Update();
					}
				}
				else if (f(0) == '?' &&
						ShowUntracked)
				{
					GString File = f(2, -1);

					VcFile *f = Map.Find(File);
					if (!f)
					{
						if ((f = new VcFile(d, this, NULL, IsWorking)))
							Ins.Insert(f);
					}
					if (f)
					{
						f->SetText("?", COL_STATE);
						f->SetText(File, COL_FILENAME);
						f->Update();
					}
				}
			}

			for (auto i: *d->Files)
			{
				VcFile *f = dynamic_cast<VcFile*>(i);
				if (f)
				{
					if (f->GetStatus() == VcFile::SUnknown)
						f->SetStatus(VcFile::SUntracked);
				}
			}
			break;
		}
		case VcGit:
		{
			GString::Array Lines = s.SplitDelimit("\r\n");
			int Fmt = ToolVersion[VcGit] >= Ver2Int("2.8.0") ? 2 : 1;
			for (auto Ln : Lines)
			{
				char Type = Ln(0);
				if (Ln.Lower().Find("error:") >= 0)
				{
				}
				else if (Ln.Find("usage: git") >= 0)
				{
					// It's probably complaining about the --porcelain=2 parameter
					OnCmdError(s, "Args error");
				}
				else if (Type != '?')
				{
					VcFile *f = NULL;
					
					if (Fmt == 2)
					{
						GString::Array p = Ln.SplitDelimit(" ", 8);
						if (p.Length() < 7)
							d->Log->Print("%s:%i - Error: not enough tokens: '%s'\n", _FL, Ln.Get());
						else
						{
							f = new VcFile(d, this, p[6], IsWorking);
							f->SetText(p[1].Strip("."), COL_STATE);
							f->SetText(p.Last(), COL_FILENAME);
						}
					}
					else if (Fmt == 1)
					{
						GString::Array p = Ln.SplitDelimit(" ");
						f = new VcFile(d, this, NULL, IsWorking);
						f->SetText(p[0], COL_STATE);
						f->SetText(p.Last(), COL_FILENAME);
					}
					
					if (f)
						Ins.Insert(f);
				}
				else if (ShowUntracked)
				{
					VcFile *f = new VcFile(d, this, NULL, IsWorking);
					f->SetText("?", COL_STATE);
					f->SetText(Ln(2,-1), COL_FILENAME);
					Ins.Insert(f);
				}
			}
			break;
		}
		case VcHg:
		case VcSvn:
		{
			if (s.Find("failed to import") >= 0)
			{
				OnCmdError(s, "Tool error.");
				return false;
			}
			
			GString::Array Lines = s.SplitDelimit("\r\n");
			for (auto Ln : Lines)
			{
				char Type = Ln(0);
				if (Ln.Lower().Find("error:") >= 0)
				{
				}
				else if (Ln.Find("client is too old") >= 0)
				{
					OnCmdError(s, "Client too old.");
					return false;
				}
				else if (Strchr(" \t", Type) ||
						Ln.Find("Summary of conflicts") >= 0)
				{
					// Ignore
				}
				else if (Type != '?')
				{
					GString::Array p = Ln.SplitDelimit(" ", 1);
					if (p.Length() == 2)
					{
						GString File;
						if (GetType() == VcSvn)
							File = ConvertUPlus(p.Last());
						else
							File = p.Last();

						VcFile *f = new VcFile(d, this, NULL, IsWorking);
						f->SetText(p[0], COL_STATE);
						f->SetText(File.Replace("\\","/"), COL_FILENAME);
						f->GetStatus();
						Ins.Insert(f);
					}
					else LgiAssert(!"What happen?");
				}
				else if (ShowUntracked)
				{
					VcFile *f = new VcFile(d, this, NULL, IsWorking);
					f->SetText("?", COL_STATE);
					f->SetText(Ln(2,-1), COL_FILENAME);
					Ins.Insert(f);
				}
			}
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	if ((Unpushed = Ins.Length() > 0))
	{
		if (CmdErrors == 0)
			GetCss(true)->Color(GColour(255, 128, 0));
	}
	else if (Unpulled == 0)
	{
		GetCss(true)->Color(GCss::ColorInherit);
	}

	Update();
	if (GTreeItem::Select())
	{
		d->Files->Insert(Ins);
		d->Files->ResizeColumnsToContent();
	}
	else
	{
		Ins.DeleteObjects();
	}

	if (Params && Params->Leaf)
		Params->Leaf->AfterBrowse();

	return false; // Don't refresh list
}

// Clone/checkout any sub-repositries.
bool VcFolder::UpdateSubs()
{
	GString Arg;
	switch (GetType())
	{
		default:
		case VcSvn:
		case VcHg:
		case VcCvs:
			return false;
		case VcGit:
			Arg = "submodule update --init";
			break;
	}

	return StartCmd(Arg, &VcFolder::ParseUpdateSubs, NULL, LogNormal);
}

bool VcFolder::ParseUpdateSubs(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		default:
		case VcSvn:
		case VcHg:
		case VcCvs:
			return false;
		case VcGit:
			break;
	}

	return false;
}

void VcFolder::FolderStatus(const char *uri, VcLeaf *Notify)
{
	GUri Uri(uri);
	if (Uri.IsFile() && Uri.sPath)
	{
		GFile::Path p(Uri.sPath(1,-1));
		if (!p.IsFolder())
		{
			LgiAssert(!"Needs to be a folder.");
			return;
		}
	}

	if (GTreeItem::Select())
		d->ClearFiles();

	GString Arg;
	switch (GetType())
	{
		case VcSvn:
		case VcHg:
			Arg = "status";
			break;
		case VcCvs:
			Arg = "status -l";
			break;
		case VcGit:
			if (!ToolVersion[VcGit])
				LgiAssert(!"Where is the version?");
			
			// What version did =2 become available? It's definitely not in v2.5.4
			// Not in v2.7.4 either...
			if (ToolVersion[VcGit] >= Ver2Int("2.8.0"))
				Arg = "status --porcelain=2";
			else
				Arg = "status --porcelain";
			break;
		default:
			return;
	}

	ParseParams *p = new ParseParams;
	if (uri && Notify)
	{
		p->AltInitPath = uri;
		p->Leaf = Notify;
	}
	else
	{
		p->IsWorking = true;
	}
	StartCmd(Arg, &VcFolder::ParseStatus, p);

	switch (GetType())
	{
		case VcHg:
			CountToTip();
			break;
		default:
			break;
	}
}

void VcFolder::CountToTip()
{
	// if (Path.Equals("C:\\Users\\matthew\\Code\\Lgi\\trunk"))
	{
		// LgiTrace("%s: CountToTip, br=%s, idx=%i\n", Path.Get(), CurrentBranch.Get(), (int)CurrentCommitIdx);

		if (!CurrentBranch)
			GetBranches(new ParseParams("CountToTip"));
		else if (CurrentCommitIdx < 0)
			GetCurrentRevision(new ParseParams("CountToTip"));
		else
		{
			GString Arg;
			Arg.Printf("id -n -r %s", CurrentBranch.Get());
			StartCmd(Arg, &VcFolder::ParseCountToTip);
		}
	}
}

bool VcFolder::ParseCountToTip(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcHg:
			if (CurrentCommitIdx >= 0)
			{
				auto p = s.Strip();
				auto idx = p.Int();
				if (idx >= CurrentCommitIdx)
				{
					Unpulled = (int) (idx - CurrentCommitIdx);
					Update();
				}
			}
			break;
		default:
			break;
	}

	return false;
}

void VcFolder::ListWorkingFolder()
{
	if (!IsWorkingFld)
	{
		d->ClearFiles();
		
		bool Untracked = d->IsMenuChecked(IDM_UNTRACKED);

		GString Arg;
		switch (GetType())
		{
			case VcCvs:
				if (Untracked)
					Arg = "-qn update";
				else
					Arg = "-q diff --brief";
				break;
			case VcSvn:
				Arg = "status";
				break;
			case VcGit:
				StartCmd("diff --staged", &VcFolder::ParseWorking);
				Arg = "diff --diff-filter=ACDMRTU";
				break;
			case VcHg:
				Arg = "status -mard";
				break;
			default:
				Arg ="diff";
				break;
		}

		IsWorkingFld = StartCmd(Arg, &VcFolder::ParseWorking);
	}
}

void VcFolder::GitAdd()
{
	if (!PostAdd)
		return;

	GString Args;
	if (PostAdd->Files.Length() == 0)
	{
		GString m(PostAdd->Msg);
		m = m.Replace("\"", "\\\"");
		Args.Printf("commit -m \"%s\"", m.Get());
		IsCommit = StartCmd(Args, &VcFolder::ParseCommit, PostAdd->Param, LogNormal);
		PostAdd.Reset();
	}
	else
	{
		GString Last = PostAdd->Files.Last();
		Args.Printf("add \"%s\"", Last.Replace("\"", "\\\"").Replace("/", DIR_STR).Get());
		PostAdd->Files.PopLast();	
		StartCmd(Args, &VcFolder::ParseGitAdd, NULL, LogNormal);
	}
}

bool VcFolder::ParseGitAdd(int Result, GString s, ParseParams *Params)
{
	GitAdd();
	return false;
}

bool VcFolder::ParseCommit(int Result, GString s, ParseParams *Params)
{
	if (GTreeItem::Select())
		Select(true);

	CommitListDirty = Result == 0;
	CurrentCommit.Empty();
	IsCommit = false;

	if (Result)
	{
		switch (GetType())
		{
			case VcGit:
			{
				if (s.Find("Please tell me who you are") >= 0)
				{
					{
						GInput i(GetTree(), "", "Git user name:", AppName);
						if (i.DoModal())
						{
							GString Args;
							Args.Printf("config --global user.name \"%s\"", i.GetStr().Get());
							StartCmd(Args);
						}
					}
					{
						GInput i(GetTree(), "", "Git user email:", AppName);
						if (i.DoModal())
						{
							GString Args;
							Args.Printf("config --global user.email \"%s\"", i.GetStr().Get());
							StartCmd(Args);
						}
					}
				}
				break;
			}
			default:
				break;
		}
		
		return false;
	}

	if (Result == 0 && GTreeItem::Select())
	{
		d->ClearFiles();

		GWindow *w = d->Diff ? d->Diff->GetWindow() : NULL;
		if (w)
			w->SetCtrlName(IDC_MSG, NULL);
	}

	switch (GetType())
	{
		case VcGit:
		{
			Unpushed++;
			CommitListDirty = true;
			Update();

			if (Params && Params->Str.Find("Push") >= 0)
				Push();
			break;
		}
		case VcSvn:
		{
			CurrentCommit.Empty();
			CommitListDirty = true;
			GetTree()->SendNotify(LvcCommandEnd);
			if (!Result)
			{
				Unpushed = 0;
				Update();
				GetCss(true)->Color(GColour::Green);
			}
			break;
		}
		case VcHg:
		{
			CurrentCommit.Empty();
			CommitListDirty = true;
			GetTree()->SendNotify(LvcCommandEnd);
			if (!Result)
			{
				Unpushed = 0;
				Update();

				if (Params && Params->Str.Find("Push") >= 0)
					Push();
				else
					GetCss(true)->Color(GColour::Green);
			}
			break;
		}
		case VcCvs:
		{
			CurrentCommit.Empty();
			CommitListDirty = true;
			GetTree()->SendNotify(LvcCommandEnd);
			if (!Result)
			{
				Unpushed = 0;
				Update();
				GetCss(true)->Color(GColour::Green);
			}
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

void VcFolder::Commit(const char *Msg, const char *Branch, bool AndPush)
{
	GArray<VcFile*> Add;
	bool Partial = false;
	for (auto fp: *d->Files)
	{
		VcFile *f = dynamic_cast<VcFile*>(fp);
		if (f)
		{
			int c = f->Checked();
			if (c > 0)
				Add.Add(f);
			else
				Partial = true;
		}
	}

	if (CurrentBranch && Branch &&
		!CurrentBranch.Equals(Branch))
	{
		int Response = LgiMsg(GetTree(), "Do you want to start a new branch?", AppName, MB_YESNO);
		if (Response != IDYES)
			return;
	}

	if (!IsCommit)
	{
		GString Args;
		ParseParams *Param = AndPush ? new ParseParams("Push") : NULL;

		switch (GetType())
		{
			case VcGit:
			{
				if (Add.Length() == 0)
				{
					break;
				}
				else if (Partial)
				{
					if (PostAdd.Reset(new GitCommit))
					{
						PostAdd->Files.SetFixedLength(false);
						for (auto f : Add)
							PostAdd->Files.Add(f->GetFileName());
						PostAdd->Msg = Msg;
						PostAdd->Branch = Branch;
						PostAdd->Param = Param;
						GitAdd();
					}
				}
				else
				{
					GString m(Msg);
					m = m.Replace("\"", "\\\"");
					Args.Printf("commit -am \"%s\"", m.Get());
					IsCommit = StartCmd(Args, &VcFolder::ParseCommit, Param, LogNormal);
				}
				break;
			}
			case VcSvn:
			{
				GString::Array a;
				a.New().Printf("commit -m \"%s\"", Msg);
				for (auto pf: Add)
				{
					GString s = pf->GetFileName();
					if (s.Find(" ") >= 0)
						a.New().Printf("\"%s\"", s.Get());
					else
						a.New() = s;
				}

				Args = GString(" ").Join(a);
				IsCommit = StartCmd(Args, &VcFolder::ParseCommit, Param, LogNormal);

				if (d->Tabs && IsCommit)
				{
					d->Tabs->Value(1);
					GetTree()->SendNotify(LvcCommandStart);
				}
				break;
			}
			case VcHg:
			{
				GString::Array a;
				a.New().Printf("commit -m \"%s\"", GString(Msg).Replace("\n", "\\n").Get());
				for (auto pf: Add)
				{
					GString s = pf->GetFileName();
					if (s.Find(" ") >= 0)
						a.New().Printf("\"%s\"", s.Get());
					else
						a.New() = s;
				}

				Args = GString(" ").Join(a);
				IsCommit = StartCmd(Args, &VcFolder::ParseCommit, Param, LogNormal);

				if (d->Tabs && IsCommit)
				{
					d->Tabs->Value(1);
					GetTree()->SendNotify(LvcCommandStart);
				}
				break;
			}
			case VcCvs:
			{
				GString a;
				a.Printf("commit -m \"%s\"", Msg);
				IsCommit = StartCmd(a, &VcFolder::ParseCommit, NULL, LogNormal);
				break;
			}
			default:
			{
				OnCmdError(NULL, "No commit impl for type.");
				break;
			}
		}
	}
}

void VcFolder::Push()
{
	GString Args;
	bool Working = false;
	switch (GetType())
	{
		case VcHg:
		case VcGit:
			Working = StartCmd("push", &VcFolder::ParsePush, NULL, LogNormal);
			break;
		case VcSvn:
			// Nothing to do here.. the commit pushed the data already
			break;
		default:
			OnCmdError(NULL, "No push impl for type.");
			break;
	}

	if (d->Tabs && Working)
	{
		d->Tabs->Value(1);
		GetTree()->SendNotify(LvcCommandStart);
	}
}

bool VcFolder::ParsePush(int Result, GString s, ParseParams *Params)
{
	bool Status = false;
	
	if (Result)
	{
		OnCmdError(s, "Push failed.");
	}
	else
	{
		switch (GetType())
		{
			case VcGit:
				break;
			case VcSvn:
				break;
			default:
				break;
		}

		Unpushed = 0;
		GetCss(true)->Color(GColour::Green);
		Update();
		Status = true;
	}

	GetTree()->SendNotify(LvcCommandEnd);
	return Status; // no reselect
}

void VcFolder::Pull(int AndUpdate, LoggingType Logging)
{
	bool Status = false;

	if (AndUpdate < 0)
		AndUpdate = GetTree()->GetWindow()->GetCtrlValue(IDC_UPDATE) != 0;

	switch (GetType())
	{
		case VcNone:
			return;
		case VcHg:
			Status = StartCmd(AndUpdate ? "pull -u" : "pull", &VcFolder::ParsePull, NULL, Logging);
			break;
		case VcGit:
			Status = StartCmd(AndUpdate ? "pull" : "fetch", &VcFolder::ParsePull, NULL, Logging);
			break;
		case VcSvn:
			Status = StartCmd("up", &VcFolder::ParsePull, NULL, Logging);
			break;
		default:
			OnCmdError(NULL, "No pull impl for type.");
			break;
	}

	if (d->Tabs && Status)
	{
		d->Tabs->Value(1);
		GetTree()->SendNotify(LvcCommandStart);
	}
}

bool VcFolder::ParsePull(int Result, GString s, ParseParams *Params)
{
	GetTree()->SendNotify(LvcCommandEnd);
	if (Result)
	{
		OnCmdError(s, "Pull failed.");
		return false;
	}
	else ClearError();

	switch (GetType())
	{
		case VcGit:
		{
			// Git does a merge by default, so the current commit changes...
			CurrentCommit.Empty();
			break;
		}
		case VcHg:
		{
			CurrentCommit.Empty();
			
			auto Lines = s.SplitDelimit("\n");
			bool HasUpdates = false;
			for (auto Ln: Lines)
			{
				if (Ln.Find("files updated") < 0)
					continue;

				auto Parts = Ln.Split(",");
				for (auto p: Parts)
				{
					auto n = p.Strip().Split(" ", 1);
					if (n.Length() == 2)
					{
						if (n[0].Int() > 0)
							HasUpdates = true;
					}
				}
			}
			if (HasUpdates)
				GetCss(true)->Color(GColour::Green);
			else
				GetCss(true)->Color(GCss::ColorInherit);
			break;
		}
		case VcSvn:
		{
			// Svn also does a merge by default and can update our current position...
			CurrentCommit.Empty();

			GString::Array a = s.SplitDelimit("\r\n");
			for (auto &Ln: a)
			{
				if (Ln.Find("At revision") >= 0)
				{
					GString::Array p = Ln.SplitDelimit(" .");
					CurrentCommit = p.Last();
					break;
				}
				else if (Ln.Find("svn cleanup") >= 0)
				{
					OnCmdError(s, "Needs cleanup");
					break;
				}
			}

			if (Params && Params->Str.Equals("log"))
			{
				IsLogging = StartCmd("log", &VcFolder::ParseLog);
				return false;
			}
			break;
		}
		default:
			break;
	}

	CommitListDirty = true;
	return true; // Yes - reselect and update
}

void VcFolder::MergeToLocal(GString Rev)
{
	switch (GetType())
	{
		case VcGit:
		{
			GString Args;
			Args.Printf("merge -m \"Merge with %s\" %s", Rev.Get(), Rev.Get());
			StartCmd(Args, &VcFolder::ParseMerge, NULL, LogNormal);
			break;
		}
		case VcHg:
		{
			GString Args;
			Args.Printf("merge -r %s", Rev.Get());
			StartCmd(Args, &VcFolder::ParseMerge, NULL, LogNormal);
			break;
		}
		default:
			LgiMsg(GetTree(), "Not implemented.", AppName);
			break;
	}
}

bool VcFolder::ParseMerge(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		case VcHg:
			if (Result == 0)
				CommitListDirty = true;
			else
				OnCmdError(s, "Merge failed.");
			break;
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	return true;
}

void VcFolder::Refresh()
{
	CommitListDirty = true;
	CurrentCommit.Empty();
	if (Uncommit && Uncommit->LListItem::Select())
		Uncommit->Select(true);
	Select(true);
}

void VcFolder::Clean()
{
	switch (GetType())
	{
		case VcSvn:
			StartCmd("cleanup", &VcFolder::ParseClean, NULL, LogNormal);
			break;
		default:
			LgiMsg(GetTree(), "Not implemented.", AppName);
			break;
	}
}

bool VcFolder::ParseClean(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcSvn:
			if (Result == 0)
				GetCss(true)->Color(GCss::ColorInherit);
			break;
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	return false;
}

GColour VcFolder::BranchColour(const char *Name)
{
	if (!Name)
		return GetPaletteColour(0);

	auto b = Branches.Find(Name);
	if (!b) // Must be a new one?
	{

		int i = 1;
		for (auto b: Branches)
		{
			auto &v = b.value;
			if (!v->Colour.IsValid())
			{
				if (v->Default)
					v->Colour = GetPaletteColour(0);
				else
					v->Colour = GetPaletteColour(i++);
			}
		}
		Branches.Add(Name, b = new VcBranch(Name));
		b->Colour = GetPaletteColour((int)Branches.Length());
	}

	return b ? b->Colour : GetPaletteColour(0);
}

GString VcFolder::CurrentRev()
{
	GString Cmd, Id;
	Cmd.Printf("id -i");
	Result r = RunCmd(Cmd);
	if (r.Code == 0)
		Id = r.Out.Strip();	
	return Id;
}

bool VcFolder::RenameBranch(GString NewName, GArray<VcCommit*> &Revs)
{
	switch (GetType())
	{
		case VcHg:
		{
			// Update to the ancestor of the commits
			LHashTbl<StrKey<char>,int> Refs(0, -1);
			for (auto c:Revs)
			{
				for (auto p:*c->GetParents())
					if (Refs.Find(p) < 0)
						Refs.Add(p, 0);
				if (Refs.Find(c->GetRev()) >= 0)
					Refs.Add(c->GetRev(), 1);
			}
			GString::Array Ans;
			for (auto i:Refs)
			{
				if (i.value == 0)
					Ans.Add(i.key);
			}

			GArray<VcCommit*> Ancestors = d->GetRevs(Ans);
			if (Ans.Length() != 1)
			{
				// We should only have one ancestor
				GString s, m;
				s.Printf("Wrong number of ancestors: " LPrintfInt64 ".\n", Ans.Length());
				for (auto i: Ancestors)
				{
					m.Printf("\t%s\n", i->GetRev());
					s += m;
				}
				LgiMsg(GetTree(), s, AppName, MB_OK);
				break;
			}

			GArray<VcCommit*> Top;
			for (auto c:Revs)
			{
				for (auto p:*c->GetParents())
					if (Refs.Find(p) == 0)
						Top.Add(c);
			}
			if (Top.Length() != 1)
			{
				d->Log->Print("Error: Can't find top most commit. (%s:%i)\n", _FL);
				return false;
			}

			// Create the new branch...
			auto First = Ancestors.First();
			GString Cmd;
			Cmd.Printf("update -r " LPrintfInt64, First->GetIndex());
			Result r = RunCmd(Cmd, LogNormal);
			if (r.Code)
			{
				d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
				return false;
			}

			Cmd.Printf("branch \"%s\"", NewName.Get());
			r = RunCmd(Cmd, LogNormal);
			if (r.Code)
			{
				d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
				return false;
			}

			// Commit it to get a revision point to rebase to
			Cmd.Printf("commit -m \"Branch: %s\"", NewName.Get());
			r = RunCmd(Cmd, LogNormal);
			if (r.Code)
			{
				d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
				return false;
			}

			auto BranchNode = CurrentRev();

			// Rebase the old tree to this point
			Cmd.Printf("rebase -s %s -d %s", Top.First()->GetRev(), BranchNode.Get());
			r = RunCmd(Cmd, LogNormal);
			if (r.Code)
			{
				d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
				return false;
			}

			CommitListDirty = true;
			d->Log->Print("Finished rename.\n", _FL);
			break;
		}
		default:
		{
			LgiMsg(GetTree(), "Not impl for this VCS.", AppName);
			break;
		}
	}

	return true;
}

void VcFolder::GetVersion()
{
	auto t = GetType();
	switch (t)
	{
		case VcGit:
		case VcSvn:
		case VcHg:
		case VcCvs:
			StartCmd("--version", &VcFolder::ParseVersion, NULL, LogNormal);
			break;
		case VcPending:
			break;
		default:
			OnCmdError(NULL, "No version control found.");
			break;
	}
}

bool VcFolder::ParseVersion(int Result, GString s, ParseParams *Params)
{
	if (Result)
		return false;
	
	auto p = s.SplitDelimit();
	switch (GetType())
	{
		case VcGit:
		{
			if (p.Length() > 2)
			{
				ToolVersion[GetType()] = Ver2Int(p[2]);
				printf("Git version: %s\n", p[2].Get());
			}
			else
				LgiAssert(0);
			break;
		}
		case VcSvn:
		{
			if (p.Length() > 2)
			{
				ToolVersion[GetType()] = Ver2Int(p[2]);
				printf("Svn version: %s\n", p[2].Get());
			}
			else
				LgiAssert(0);
			break;
		}
		case VcHg:
		{
			if (p.Length() >= 5)
			{
				auto Ver = p[4].Strip("()");
				ToolVersion[GetType()] = Ver2Int(Ver);
				printf("Hg version: %s\n", Ver.Get());
			}
			break;
		}
		case VcCvs:
		{
			#ifdef _DEBUG
			for (auto i : p)
				printf("i='%s'\n", i.Get());
			#endif

			if (p.Length() > 1)
			{
				auto Ver = p[2];
				ToolVersion[GetType()] = Ver2Int(Ver);
				printf("Cvs version: %s\n", Ver.Get());
			}
			break;
		}
		default:
			break;
	}

	return false;
}

bool VcFolder::ParseAddFile(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcCvs:
		{
			if (Result)
			{
				d->Tabs->Value(1);
				OnCmdError(s, "Add file failed");
			}
			else ClearError();

			break;
		}
		default:
			break;
	}

	return false;
}

bool VcFolder::AddFile(const char *Path, bool AsBinary)
{
	if (!Path)
		return false;

	switch (GetType())
	{
		case VcCvs:
		{
			GString p = Path;
			auto dir = strrchr(p, DIR_CHAR);
			ParseParams *params = NULL;
			if (dir)
			{
				*dir++ = 0;
				if ((params = new ParseParams))
					params->AltInitPath = p;
			}

			GString a;
			a.Printf("add%s \"%s\"", AsBinary ? " -kb" : "", dir ? dir : Path);
			return StartCmd(a, &VcFolder::ParseAddFile, params);
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return false;
}

bool VcFolder::ParseRevert(int Result, GString s, ParseParams *Params)
{
	if (GetType() == VcSvn)
	{
		if (s.Find("Skipped ") >= 0)
			Result = 1; // Stupid svn... *sigh*
	}

	if (Result)
	{
		OnCmdError(s, "Error reverting changes.");
	}

	ListWorkingFolder();
	return false;
}

bool VcFolder::Revert(const char *uri, const char *Revision)
{
	if (!uri)
		return false;

	auto Path = GetFilePart(uri);
	switch (GetType())
	{
		case VcGit:
		{
			GString a;
			a.Printf("checkout \"%s\"", Path.Get());
			return StartCmd(a, &VcFolder::ParseRevert);
			break;
		}
		case VcHg:
		case VcSvn:
		{
			GString a;
			a.Printf("revert \"%s\"", Path.Get());
			return StartCmd(a, &VcFolder::ParseRevert);
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return false;
}

bool VcFolder::ParseResolve(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		{
			break;
		}
		case VcSvn:
		case VcHg:
		case VcCvs:
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

bool VcFolder::Resolve(const char *Path)
{
	if (!Path)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			GString a;
			a.Printf("add \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseResolve, new ParseParams(Path));
		}
		case VcSvn:
		case VcHg:
		case VcCvs:
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return false;
}

bool VcFolder::ParseBlame(int Result, GString s, ParseParams *Params)
{
	new BlameUi(d, GetType(), s);
	return false;
}

bool VcFolder::Blame(const char *Path)
{
	if (!Path)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			GString a;
			a.Printf("blame \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseBlame);
			break;
		}
		case VcHg:
		{
			GString a;
			a.Printf("annotate -un \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseBlame);
			break;
		}
		case VcSvn:
		{
			GString a;
			a.Printf("blame \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseBlame);
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

bool VcFolder::SaveFileAs(const char *Path, const char *Revision)
{
	if (!Path || !Revision)
		return false;

	return true;
}

bool VcFolder::ParseSaveAs(int Result, GString s, ParseParams *Params)
{
	return false;
}

bool VcFolder::ParseCounts(int Result, GString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		{
			Unpushed = (int) s.Strip().Split("\n").Length();
			break;
		}
		case VcSvn:
		{
			int64 ServerRev = 0;
			bool HasUpdate = false;

			GString::Array c = s.Split("\n");
			for (unsigned i=0; i<c.Length(); i++)
			{
				GString::Array a = c[i].SplitDelimit();
				if (a.Length() > 1 && a[0].Equals("Status"))
					ServerRev = a.Last().Int();
				else if (a[0].Equals("*"))
					HasUpdate = true;
			}

			if (ServerRev > 0 && HasUpdate)
			{
				int64 CurRev = CurrentCommit.Int();
				Unpulled = (int) (ServerRev - CurRev);
			}
			else Unpulled = 0;
			Update();
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	IsUpdatingCounts = false;
	Update();
	return false; // No re-select
}

void VcFolder::SetEol(const char *Path, int Type)
{
	if (!Path) return;

	switch (Type)
	{
		case IDM_EOL_LF:
		{
			ConvertEol(Path, false);
			break;
		}
		case IDM_EOL_CRLF:
		{
			ConvertEol(Path, true);
			break;
		}
		case IDM_EOL_AUTO:
		{
			#ifdef WINDOWS
			ConvertEol(Path, true);
			#else
			ConvertEol(Path, false);
			#endif
			break;
		}
	}
}

void VcFolder::UncommitedItem::Select(bool b)
{
	LListItem::Select(b);
	if (b)
	{
		GTreeItem *i = d->Tree->Selection();
		VcFolder *f = dynamic_cast<VcFolder*>(i);
		if (f)
			f->ListWorkingFolder();

		if (d->Msg)
		{
			d->Msg->Name(NULL);

			GWindow *w = d->Msg->GetWindow();
			if (w)
			{
				w->SetCtrlEnabled(IDC_COMMIT, true);
				w->SetCtrlEnabled(IDC_COMMIT_AND_PUSH, true);
			}
		}
	}
}

void VcFolder::UncommitedItem::OnPaint(GItem::ItemPaintCtx &Ctx)
{
	GFont *f = GetList()->GetFont();
	f->Transparent(false);
	f->Colour(Ctx.Fore, Ctx.Back);
	
	GDisplayString ds(f, "(working folder)");
	ds.Draw(Ctx.pDC, Ctx.x1 + ((Ctx.X() - ds.X()) / 2), Ctx.y1 + ((Ctx.Y() - ds.Y()) / 2), &Ctx);
}

//////////////////////////////////////////////////////////////////////////////////////////
VcLeaf::VcLeaf(VcFolder *parent, GTreeItem *Item, GString uri, GString leaf, bool folder)
{
	Parent = parent;
	d = Parent->GetPriv();
	LgiAssert(uri.Find("://") >= 0); // Is URI
	Uri.Set(uri);
	LgiAssert(Uri);
	Leaf = leaf;
	Folder = folder;
	Tmp = NULL;
	Item->Insert(this);

	if (Folder)
	{
		Insert(Tmp = new GTreeItem);
		Tmp->SetText("Loading...");
	}
}

VcLeaf::~VcLeaf()
{
	Log.DeleteObjects();
}

GString VcLeaf::Full()
{
	GUri u = Uri;
	u += Leaf;
	return u.ToString();
}

void VcLeaf::OnBrowse()
{
	auto full = Full();

	LList *Files = d->Files;
	Files->Empty();
	GDirectory Dir;
	for (int b = Dir.First(full); b; b = Dir.Next())
	{
		if (Dir.IsDir())
			continue;

		VcFile *f = new VcFile(d, Parent, NULL, true);
		if (f)
		{
			f->SetUri(GString("file://") + full);
			f->SetText(Dir.GetName(), COL_FILENAME);
			Files->Insert(f);
		}
	}
	Files->ResizeColumnsToContent();
	if (Folder)
		Parent->FolderStatus(full, this);
}

void VcLeaf::AfterBrowse()
{
}

VcLeaf *VcLeaf::Find(const char *Path)
{
	if (!Stricmp(Path, Full().Get()))
		return this;

	VcLeaf *r = NULL;	
	for (auto n = GetChild(); !r && n; n = n->GetNext())
	{
		auto l = dynamic_cast<VcLeaf*>(n);
		if (l)
			r = l->Find(Path);
	}
	
	return r;
}

void VcLeaf::OnExpand(bool b)
{
	if (Tmp && b)
	{
		Tmp->Remove();
		DeleteObj(Tmp);
		Parent->ReadDir(this, Full());
	}
}

const char *VcLeaf::GetText(int Col)
{
	if (Col == 0)
		return Leaf;

	return NULL;
}

int VcLeaf::GetImage(int Flags)
{
	return Folder ? IcoFolder : IcoFile;
}

int VcLeaf::Compare(VcLeaf *b)
{
	// Sort folders to the top...
	if (Folder ^ b->Folder)
		return (int)b->Folder - (int)Folder;
	
	// Then alphabetical
	return Stricmp(Leaf.Get(), b->Leaf.Get());
}

bool VcLeaf::Select()
{
	return GTreeItem::Select();
}

void VcLeaf::Select(bool b)
{
	GTreeItem::Select(b);
	if (b)
	{
		d->Commits->RemoveAll();
		OnBrowse();
		ShowLog();
	}
}

void VcLeaf::ShowLog()
{
	if (Log.Length())
	{	
		d->Commits->RemoveAll();
		Parent->DefaultFields();
		Parent->UpdateColumns();
		for (auto i: Log)
			d->Commits->Insert(i);
	}
}

void VcLeaf::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		LSubMenu s;
		s.AppendItem("Log", IDM_LOG);
		s.AppendItem("Blame", IDM_BLAME, !Folder);
		s.AppendSeparator();
		s.AppendItem("Browse To", IDM_BROWSE_FOLDER);
		s.AppendItem("Terminal At", IDM_TERMINAL);
		
		int Cmd = s.Float(GetTree(), m - _ScrollPos());
		switch (Cmd)
		{
			case IDM_LOG:
			{
				Parent->LogFile(Full());
				break;
			}
			case IDM_BLAME:
			{
				Parent->Blame(Full());
				break;
			}
			case IDM_BROWSE_FOLDER:
			{
				LgiBrowseToFile(Full());
				break;
			}
			case IDM_TERMINAL:
			{
				TerminalAt(Full());
				break;
			}
		}
	}
}

