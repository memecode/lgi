#include "Lvc.h"
#include "../Resources/resdefs.h"
#include "GCombo.h"

#ifndef CALL_MEMBER_FN
#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))
#endif

#define PROFILE_FN 0
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

VcFolder::VcFolder(AppPriv *priv, const char *p)
{
	Init(priv);
	Path = p;
}

VcFolder::VcFolder(AppPriv *priv, GXmlTag *t)
{
	Init(priv);
	Serialize(t, false);
}

VersionCtrl VcFolder::GetType()
{
	if (Type == VcNone)
		Type = DetectVcs(Path);
	return Type;
}

const char *VcFolder::GetText(int Col)
{
	switch (Col)
	{
		case 0:
		{
			if (Cmds.Length())
			{
				Cache = Path;
				Cache += " (...)";
				return Cache;
			}
			return Path;
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
		t->SetContent(Path);
	else
		Path = t->GetContent();
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

	GSubProcess Process(Exe, Args);
	Process.SetInitFolder(Path);
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

	GAutoPtr<GSubProcess> Process(new GSubProcess(Exe, Args));
	if (!Process)
		return false;

	Process->SetInitFolder(Params && Params->AltInitPath ? Params->AltInitPath : Path);

	GString::Array Ctx;
	Ctx.SetFixedLength(false);
	Ctx.Add(Path);
	Ctx.Add(Exe);
	Ctx.Add(Args);
	GAutoPtr<Cmd> c(new Cmd(Ctx, Logging, d->Log));
	if (!c)
		return false;

	c->PostOp = Parser;
	c->Params.Reset(Params);
	c->Rd.Reset(new ReaderThread(GetType(), Process.Release(), c));
	Cmds.Add(c.Release());

	Update();

	// LgiTrace("Cmd: %s %s\n", Exe, Args);

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
			for (GString *l = NULL; a.Iterate(l); )
			{
				GString n = l->Strip();
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
		if (!DirExists(Path))
			return;

		PROF("DefaultFields");
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

		PROF("Type Change");
		if (GetType() != d->PrevType)
		{
			d->PrevType = GetType();
			d->Lst->EmptyColumns();

			for (auto c: Fields)
			{
				switch (c)
				{
					case LGraph: d->Lst->AddColumn("---", 60); break;
					case LIndex: d->Lst->AddColumn("Index", 60); break;
					case LBranch: d->Lst->AddColumn("Branch", 60); break;
					case LRevision: d->Lst->AddColumn("Revision", 60); break;
					case LAuthor: d->Lst->AddColumn("Author", 240); break;
					case LTimeStamp: d->Lst->AddColumn("Date", 130); break;
					case LMessage: d->Lst->AddColumn("Message", 400); break;
					default: LgiAssert(0); break;
				}
			}
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
					if (CommitListDirty)
					{
						IsLogging = StartCmd("up", &VcFolder::ParsePull, new ParseParams("log"));
						break;
					}
					// else fall through
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

		char *Ctrl = d->Lst->GetWindow()->GetCtrlName(IDC_FILTER);
		GString Filter = ValidStr(Ctrl) ? Ctrl : NULL;

		if (d->CurFolder != this)
		{
			PROF("RemoveAll");
			d->CurFolder = this;
			d->Lst->RemoveAll();
		}

		PROF("Uncommit");
		if (!Uncommit)
			Uncommit.Reset(new UncommitedItem(d));
		d->Lst->Insert(Uncommit, 0);

		PROF("Log Loop");
		int64 CurRev = Atoi(CurrentCommit.Get());
		List<LListItem> Ls;
		for (unsigned i=0; i<Log.Length(); i++)
		{
			if (CurrentCommit &&
				Log[i]->GetRev())
			{
				switch (GetType())
				{
					case VcSvn:
					{
						int64 LogRev = Atoi(Log[i]->GetRev());
						if (CurRev >= 0 && CurRev >= LogRev)
						{
							CurRev = -1;
							Log[i]->SetCurrent(true);
						}
						else
						{
							Log[i]->SetCurrent(false);
						}
						break;
					}
					default:
						Log[i]->SetCurrent(!_stricmp(CurrentCommit, Log[i]->GetRev()));
						break;
				}
			}

			bool Add = !Filter;
			if (Filter)
			{
				const char *s = Log[i]->GetRev();
				if (s && strstr(s, Filter) != NULL)
					Add = true;

				s = Log[i]->GetAuthor();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
				s = Log[i]->GetMsg();
				if (s && stristr(s, Filter) != NULL)
					Add = true;
				
			}

			LList *CurOwner = Log[i]->GetList();
			if (Add ^ (CurOwner != NULL))
			{
				if (Add)
					Ls.Insert(Log[i]);
				else
					d->Lst->Remove(Log[i]);
			}
		}

		PROF("Ls Ins");
		d->Lst->Insert(Ls);
		if (d->Resort >= 0)
		{
			PROF("Resort");
			d->Lst->Sort(LstCmp, d->Resort);
			d->Resort = -1;
		}

		PROF("ColSizing");
		if (GetType() == VcHg)
		{
			if (d->Lst->GetColumns() >= 7)
			{
				int i = 0;
				d->Lst->ColumnAt(i++)->Width(60); // LGraph
				d->Lst->ColumnAt(i++)->Width(40); // LIndex
				d->Lst->ColumnAt(i++)->Width(100); // LRevision
				d->Lst->ColumnAt(i++)->Width(60); // LBranch
				d->Lst->ColumnAt(i++)->Width(240); // LAuthor
				d->Lst->ColumnAt(i++)->Width(130); // LTimeStamp
				d->Lst->ColumnAt(i++)->Width(400); // LMessage
			}
		}
		else
		{
			if (d->Lst->GetColumns() >= 5)
			{
				// This is too slow, over 2 seconds for Lgi
				// d->Lst->ResizeColumnsToContent();
				d->Lst->ColumnAt(0)->Width(40); // LGraph
				d->Lst->ColumnAt(1)->Width(270); // LRevision
				d->Lst->ColumnAt(2)->Width(240); // LAuthor
				d->Lst->ColumnAt(3)->Width(130); // LTimeStamp
				d->Lst->ColumnAt(4)->Width(400); // LMessage
			}
		}

		PROF("UpdateAll");
		d->Lst->UpdateAllItems();

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

bool VcFolder::ParseLog(int Result, GString s, ParseParams *Params)
{
	LHashTbl<StrKey<char>, VcCommit*> Map;
	for (VcCommit **pc = NULL; Log.Iterate(pc); )
		Map.Add((*pc)->GetRev(), *pc);

	int Skipped = 0, Errors = 0;
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
						Log.Add(Rev.Release());
					else
						Skipped++;
				}
				else
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, c[i].Get());
					Errors++;
				}
			}
			Log.Sort(CommitDateCmp);
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
					if (!Map.Find(Rev->GetRev()))
						Log.Add(Rev.Release());
					else
						Skipped++;
				}
				else if (Raw)
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, Raw.Get());
					Errors++;
				}
			}
			Log.Sort(CommitRevCmp);
			break;
		}
		case VcHg:
		{
			GString::Array c = s.Split("\n\n");
			LHashTbl<IntKey<int64_t>, VcCommit*> Idx;
			for (GString *Commit = NULL; c.Iterate(Commit); )
			{
				GAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				if (Rev->HgParse(*Commit))
				{
					auto Existing = Map.Find(Rev->GetRev());
					if (!Existing)
						Log.Add(Existing = Rev.Release());
					if (Existing->GetIndex() >= 0)
						Idx.Add(Existing->GetIndex(), Existing);
				}
			}

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

			Log.Sort(CommitIndexCmp);
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
			for (GString *Commit = NULL; c.Iterate(Commit);)
			{
				if (Commit->Strip().Length())
				{
					GString Head, File;
					GString::Array Versions = Commit->Split("----------------------------");
					GString::Array Lines = Versions[0].SplitDelimit("\r\n");
					for (GString *Line = NULL; Lines.Iterate(Line);)
					{
						GString::Array p = Line->Split(":", 1);
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
										Log.Add(Cc);
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
			else
				return;
				// LgiAssert(0);
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
			{
				return f;
			}
		}
	}

	return NULL;
}

void VcFolder::OnCmdError(GString Output, const char *Msg)
{
	if (!CmdErrors)
	{
		d->Log->Write(Output, Output.Length());

		GString::Array a = GetProgramsInPath(GetVcName());
		d->Log->Print("'%s' executables in the path:\n", GetVcName());
		for (auto Bin : a)
			d->Log->Print("    %s\n", Bin.Get());
	}
				
	CmdErrors++;
	d->Tabs->Value(1);
	Color(GColour::Red);
}

void VcFolder::ClearError()
{
	Color(GCss::ColorInherit);
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

bool VcFolder::ParseDiff(int Result, GString s, ParseParams *Params)
{
	ParseDiffs(s, NULL, true);
	return false;
}

void VcFolder::Diff(VcFile *file)
{
	switch (GetType())
	{
		case VcSvn:
		case VcGit:
		case VcHg:
		{
			GString a;
			a.Printf("diff \"%s\"", file->GetFileName());
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
	GXmlTag *t = d->Opts.LockTag(NULL, _FL);
	if (t)
	{
		Uncommit.Reset();
		if (GTreeItem::Select())
		{
			d->Files->Empty();
			d->Lst->Empty();
		}

		for (auto c: t->Children)
		{
			if (c->IsTag(OPT_Folder) &&
				c->GetContent() &&
				!_stricmp(c->GetContent(), Path))
			{
				c->RemoveTag();
				delete c;
				break;
			}
		}
		d->Opts.Unlock();
	}
}

void VcFolder::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		LSubMenu s;
		s.AppendItem("Browse To", IDM_BROWSE_FOLDER);
		s.AppendItem(
			#ifdef WINDOWS
			"Command Prompt At",
			#else
			"Terminal At",
			#endif
			IDM_TERMINAL);
		s.AppendItem("Clean", IDM_CLEAN);
		s.AppendSeparator();
		s.AppendItem("Remove", IDM_REMOVE);
		int Cmd = s.Float(GetTree(), m);
		switch (Cmd)
		{
			case IDM_BROWSE_FOLDER:
			{
				LgiBrowseToFile(Path);
				break;
			}
			case IDM_TERMINAL:
			{
				TerminalAt(Path);
				break;
			}
			case IDM_CLEAN:
			{
				Clean();
				break;
			}
			case IDM_REMOVE:
			{
				OnRemove();
				delete this;
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

void VcFolder::ReadDir(GTreeItem *Parent, const char *Path)
{
	// Read child items
	GDirectory Dir;
	for (int b = Dir.First(Path); b; b = Dir.Next())
	{
		if (Dir.IsDir())
		{
			if (Dir.GetName()[0] != '.')
			{
				new VcLeaf(this, Parent, Path, Dir.GetName(), true);
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
				new VcLeaf(this, Parent, Path, Dir.GetName(), false);
			}
		}
	}

	Parent->SortChildren(FolderCompare);
}

void VcFolder::OnExpand(bool b)
{
	if (Tmp && b)
	{
		Tmp->Remove();
		DeleteObj(Tmp);
		ReadDir(this, Path);
	}
}

void VcFolder::OnPaint(ItemPaintCtx &Ctx)
{
	auto c = Color();
	if (c.IsValid())
		Ctx.Fore = c;
	c = BackgroundColor();
	if (c.IsValid() && !GTreeItem::Select())
		Ctx.Back = c;

	GTreeItem::OnPaint(Ctx);
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

	Unpushed = Ins.Length() > 0;
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

void VcFolder::FolderStatus(const char *Path, VcLeaf *Notify)
{
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
	if (Path && Notify)
	{
		p->AltInitPath = Path;
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
				Color(GColour::Green);
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
					Color(GColour::Green);
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
		Color(GColour::Green);
		Update();
		Status = true;
	}

	GetTree()->SendNotify(LvcCommandEnd);
	return Status; // no reselect
}

void VcFolder::Pull(bool AndUpdate, LoggingType Logging)
{
	bool Status = false;
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
				Color(GColour::Green);
			break;
		}
		case VcSvn:
		{
			// Svn also does a merge by default and can update our current position...
			CurrentCommit.Empty();

			GString::Array a = s.SplitDelimit("\r\n");
			for (GString *Ln = NULL; a.Iterate(Ln); )
			{
				if (Ln->Find("At revision") >= 0)
				{
					GString::Array p = Ln->SplitDelimit(" .");
					CurrentCommit = p.Last();
					break;
				}
				else if (Ln->Find("svn cleanup") >= 0)
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
				Color(ColorInherit);
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
	ListWorkingFolder();
	return false;
}

bool VcFolder::Revert(const char *Path, const char *Revision)
{
	if (!Path)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			GString a;
			a.Printf("checkout \"%s\"", Path);
			return StartCmd(a, &VcFolder::ParseRevert);
			break;
		}
		case VcHg:
		case VcSvn:
		{
			GString a;
			a.Printf("revert \"%s\"", Path);
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
VcLeaf::VcLeaf(VcFolder *parent, GTreeItem *Item, GString path, GString leaf, bool folder)
{
	Parent = parent;
	d = Parent->GetPriv();
	Path = path;
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

GString VcLeaf::Full()
{
	GFile::Path p(Path);
	p += Leaf;
	return p.GetFull();
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

		VcFile *f = new VcFile(d, Parent, NULL);
		if (f)
		{
			f->SetPath(full);
			f->SetText(Dir.GetName(), COL_FILENAME);
			Files->Insert(f);
		}
	}
	Files->ResizeColumnsToContent();
	Parent->FolderStatus(full, this);
}

void VcLeaf::AfterBrowse()
{
}

void VcLeaf::OnExpand(bool b)
{
	if (Tmp && b)
	{
		Tmp->Remove();
		DeleteObj(Tmp);
			
		GFile::Path p(Path);
		p += Leaf;
		Parent->ReadDir(this, p);
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
	if (b) OnBrowse();
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
		
		int Cmd = s.Float(GetTree(), m);
		switch (Cmd)
		{
			case IDM_LOG:
			{
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

