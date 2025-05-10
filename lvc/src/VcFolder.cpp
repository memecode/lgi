#include "Lvc.h"

#include "lgi/common/Combo.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Json.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/IniFile.h"
#include "lgi/common/PopupNotification.h"

#include "resdefs.h"

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

class TmpFile : public LFile
{
	int Status;
	LString Hint;
	
public:
	TmpFile(const char *hint = NULL)
	{
		Status = 0;
		if (hint)
			Hint = hint;
		else
			Hint = "_lvc";
	}
	
	LFile &Create()
	{
		LFile::Path p(LSP_TEMP);
		p += Hint;
		
		do
		{
			char s[256];
			sprintf_s(s, sizeof(s), "../%s%i.tmp", Hint.Get(), LRand());
			p += s;
		}
		while (p.Exists());
	
		Status = LFile::Open(p.GetFull(), O_READWRITE);
		return *this;
	}
};

bool TerminalAt(LString Path)
{
	#if defined(MAC)
		const char *Locations[] = {
			"/System/Applications/Utilities/Terminal.app",
			"/Applications/Utilities/Terminal.app",
			NULL
		};
		for (size_t i=0; Locations[i]; i++)
		{
			if (LFileExists(Locations[i]))
			{
				LString term;
				term.Printf("%s/Contents/MacOS/Terminal", Locations[i]);
				return LExecute(term, Path);
			}
		}
	#elif defined(WINDOWS)
		TCHAR w[MAX_PATH_LEN];
		auto r = GetWindowsDirectory(w, CountOf(w));
		if (r > 0)
		{
			LFile::Path p = LString(w);
			p += "system32\\cmd.exe";
			FileDev->SetCurrentFolder(Path);
			return LExecute(p);
		}
	#elif defined(LINUX)
		LExecute("gnome-terminal", NULL, Path);
	#endif

	return false;
}

int Ver2Int(LString v)
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
			LAssert(0);
			return 0;
		}
	}
	return i;
}

int ToolVersion[VcMax] = {0};

#define DEBUG_READER_THREAD		0
#if DEBUG_READER_THREAD
	#define LOG_READER(...) printf(__VA_ARGS__)
#else
	#define LOG_READER(...)
#endif

ReaderThread::ReaderThread(VersionCtrl vcs, LAutoPtr<LSubProcess> p, LStream *out) : LThread("ReaderThread")
{
	Vcs = vcs;
	Process = p;
	Out = out;
	Result = -1;
	FilterCount = 0;

	// We don't start this thread immediately... because the number of threads is scaled to the system
	// resources, particularly CPU cores.
}

ReaderThread::~ReaderThread()
{
	Out = NULL;
	while (!IsExited())
		LSleep(1);
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
			else if (LString(s, len).Strip().Equals("remote:"))
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
	LOG_READER("OnData %i\n", (int)r);
	
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
		LString s("Process->Start failed.\n");
		Out->Write(s.Get(), s.Length(), ErrSubProcessFailed);
		return ErrSubProcessFailed;
	}

	char Buf[1024];
	ssize_t r;
	
	LOG_READER("%s:%i - starting reader loop, pid=%i\n", _FL, Process->Handle());
	while (Process->IsRunning())
	{
		if (Out)
		{
			LOG_READER("%s:%i - starting read.\n", _FL);
			r = Process->Read(Buf, sizeof(Buf));
			LOG_READER("%s:%i - read=%i.\n", _FL, (int)r);
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

	LOG_READER("%s:%i - process loop done.\n", _FL);
	if (Out)
	{
		while ((r = Process->Read(Buf, sizeof(Buf))) > 0)
			OnData(Buf, r);
	}

	LOG_READER("%s:%i - loop done.\n", _FL);
	Result = (int) Process->GetExitValue();
	#if _DEBUG
	if (Result)
		printf("%s:%i - Process err: %i 0x%x\n", _FL, Result, Result);
	#endif
	
	return Result;
}

/////////////////////////////////////////////////////////////////////////////////////////////
int VcFolder::CmdMaxThreads = 0;
int VcFolder::CmdActiveThreads = 0;

void VcFolder::Init(AppPriv *priv)
{
	if (!CmdMaxThreads)
		CmdMaxThreads = LAppInst->GetCpuCount();

	d = priv;

	Expanded(false);
	Insert(Tmp = new LTreeItem);
	Tmp->SetText("Loading...");

	LAssert(d != NULL);
}

VcFolder::VcFolder(AppPriv *priv, const char *uri)
{
	Init(priv);
	Uri.Set(uri);
	GetType();
}

VcFolder::VcFolder(AppPriv *priv, LXmlTag *t)
{
	Init(priv);
	Serialize(t, false);
}

VcFolder::~VcFolder()
{
	if (d->CurFolder == this)
		d->CurFolder = NULL;

	Log.DeleteObjects();
}

VersionCtrl VcFolder::GetType()
{
	if (Type == VcNone)
		Type = d->DetectVcs(this);
	return Type;
}

LUri VcFolder::GetUri()
{
	// Check the path ends in a dir separator, other adding paths to the URI 
	// won't work correctly.
	if (Uri.sPath.Length() > 0)
		LAssert(Uri.sPath(-1) == '/');

	return Uri;
}

bool VcFolder::IsLocal()
{
	return Uri.IsProtocol("file");
}

LString VcFolder::LocalPath()
{
	if (!Uri.IsProtocol("file") || Uri.sPath.IsEmpty())
	{
		LAssert(!"Shouldn't call this if not a file path.");
		return LString();
	}

	return Uri.LocalPath();
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

bool VcFolder::Serialize(LXmlTag *t, bool Write)
{
	if (Write)
		t->SetContent(Uri.ToString());
	else
	{
		LString s = t->GetContent();
		bool isUri = s.Find("://") >= 0;
		if (isUri)
			Uri.Set(s);
		else
			Uri.SetFile(s);

		if (Uri.sPath.Length() > 0)
		{
			if (Uri.sPath(-1) != '/')
				Uri.sPath += "/";
		}
	}
	return true;
}

LXmlTag *VcFolder::Save()
{
	auto t = new LXmlTag(OPT_Folder);
	if (t)
		Serialize(t, true);
	return t;
}

const char *VcFolder::GetVcName()
{
	if (!VcCmd)
		VcCmd = d->GetVcName(GetType());
	return VcCmd;
}

char VcFolder::GetPathSep()
{
	if (Uri.IsFile())
		return DIR_CHAR;

	return '/'; // FIXME: Assumption is that the remote system is unix based.
}

bool VcFolder::RunCmd(const char *Args, LoggingType Logging, std::function<void(Result)> Callback)
{
	Result Ret;
	Ret.Code = -1;

	const char *Exe = GetVcName();
	if (!Exe || CmdErrors > 2)
		return false;

	if (Uri.IsFile())
	{
		new ProcessCallback(Exe,
							Args,
							LocalPath(),
							Logging == LogNone ? d->Log : NULL, 
							GetTree()->GetWindow(),
							Callback);
	}
	else
	{
		LAssert(!"Impl me.");
		return false;
	}

	return true;
}

#if HAS_LIBSSH
SshConnection::LoggingType Convert(LoggingType t)
{
	switch (t)
	{
		case LogNormal:
		case LogSilo:
			return SshConnection::LogInfo;
		case LogDebug:
			return SshConnection::LogDebug;
	}
	return SshConnection::LogNone;
}
#endif

bool VcFolder::StartCmd(const char *RawArgs, ParseFn Parser, ParseParams *Params, LoggingType Logging)
{
	const char *Exe = GetVcName();
	if (!Exe)
		return false;
	if (CmdErrors > 2)
		return false;

	LString Args;
	if (auto NoPipe = NoPipeOpt())
		Args.Printf("%s%s", NoPipe, RawArgs);
	else
		Args = RawArgs;

	if (Uri.IsFile())
	{
		if (d->Log && Logging != LogSilo)
			d->Log->Print("%s %s\n", Exe, Args.Get());

		LAutoPtr<LSubProcess> Process(new LSubProcess(Exe, Args));
		if (!Process)
			return false;

		Process->SetInitFolder(Params && Params->AltInitPath ? Params->AltInitPath : LocalPath());
		#if 0//def MAC
			// Mac GUI apps don't share the terminal path, so this overrides that and make it work
			auto Path = LGetPath();
			if (Path.Length())
			{
				LString Tmp = LString(LGI_PATH_SEPARATOR).Join(Path);
				printf("Tmp='%s'\n", Tmp.Get());
				Process->SetEnvironment("PATH", Tmp);
			}
		#endif

		LString::Array Ctx;
		Ctx.SetFixedLength(false);
		Ctx.Add(LocalPath());
		Ctx.Add(Exe);
		Ctx.Add(Args);
		LAutoPtr<Cmd> c(new Cmd(Ctx, Logging, d->Log));
		if (!c)
			return false;

		c->PostOp = Parser;
		c->Params.Reset(Params);
		c->Rd.Reset(new ReaderThread(GetType(), Process, c));
		Cmds.Add(c.Release());
	}
	else
	{
		#if HAS_LIBSSH
		auto c = d->GetConnection(Uri.ToString());
		if (!c)
			return false;
		
		LString exeLeaf = LGetLeaf(Exe);
		if (!c->Command(this, exeLeaf, Args, Parser, Params, Convert(Logging)))
			return false;
		#endif
	}

	Update();
	return true;
}

int LogDateCmp(LListItem *a, LListItem *b, NativeInt Data)
{
	auto A = dynamic_cast<VcCommit*>(a);
	auto B = dynamic_cast<VcCommit*>(b);

	if ((A != NULL) ^ (B != NULL))
	{
		// This handles keeping the "working folder" list item at the top
		return (A != NULL) - (B != NULL);
	}

	// Sort the by date from most recent to least
	return -A->GetTs().Compare(&B->GetTs());
}

void VcFolder::AddGitName(LString Hash, LString Name)
{
	if (!Hash || !Name)
	{
		LAssert(!"Param error");
		return;
	}

	LString Existing = GitNames.Find(Hash);
	if (Existing)
		GitNames.Add(Hash, Existing + "," + Name);
	else
		GitNames.Add(Hash, Name);		
}

LString VcFolder::GetGitNames(LString Hash)
{
	LString Short = Hash(0, 11);
	return GitNames.Find(Short);
}

bool VcFolder::ParseBranches(int Result, LString s, ParseParams *Params)
{
	Branches.DeleteObjects();

	switch (GetType())
	{
		case VcGit:
		{
			auto a = s.SplitDelimit("\r\n");
			for (auto &l: a)
			{
				LString::Array c;
				auto s = l.Get();
				while (*s && IsWhite(*s))
					s++;
				bool IsCur = *s == '*';
				if (IsCur)
					s++;
				while (*s && IsWhite(*s))
					s++;
				if (*s == '(')
				{
					s++;
					auto e = strchr(s, ')');
					if (e)
					{
						c.New().Set(s, e - s);
						e++;
						c += LString(e).SplitDelimit(" \t");
					}
				}
				else
				{
					c = LString(s).SplitDelimit(" \t");
				}
				
				if (c.Length() < 1)
				{
					d->Log->Print("%s:%i - Too few parts in line '%s'\n", _FL, l.Get());
					continue;
				}

				if (IsCur)
					SetCurrentBranch(c[0]);

				AddGitName(c[1], c[0]);
				Branches.Add(c[0], new VcBranch(c[0], c[1]));
			}
			break;
		}
		case VcHg:
		{
			auto a = s.SplitDelimit("\r\n");
			for (auto b: a)
			{
				if (b.Find("inactive") > 0)
					continue;
			
				auto name = b(0, 28).Strip();
				auto refs = b(28, -1).SplitDelimit()[0].SplitDelimit(":");

				auto branch = Branches.Find(name);
				if (branch)
					branch->Hash = refs.Last();
				else
					Branches.Add(name, new VcBranch(name, refs.Last()));
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

void VcFolder::GetRemoteUrl(ParseParams::TCallback Callback)
{
	LAutoPtr<ParseParams> p(new ParseParams(std::move(Callback)));

	switch (GetType())
	{
		case VcGit:
		{
			StartCmd("config --get remote.origin.url", NULL, p.Release());
			break;
		}
		case VcSvn:
		{
			StartCmd("info --show-item=url", NULL, p.Release());
			break;
		}
		case VcHg:
		{
			StartCmd("paths default", NULL, p.Release());
			break;
		}
		default:
			break;
	}
}

void VcFolder::SelectCommit(LWindow *Parent, LString Commit, LString Path)
{
	bool requireFullMatch = true;
	if (GetType() == VcGit)
		requireFullMatch = false;

	// This function find the given commit and selects it such that the diffs are displayed in the file list
	VcCommit *ExistingMatch = NULL;
	for (auto c: Log)
	{
		char *rev = c->GetRev();
		bool match = requireFullMatch ? Commit.Equals(rev) : Strstr(rev, Commit.Get()) != NULL;
		if (match)
		{
			ExistingMatch = c;
			break;
		}
	}
	
	FileToSelect = Path;
	if (ExistingMatch)
	{
		ExistingMatch->Select(true);
	}
	else
	{
		// If the commit isn't there, it's likely that the log item limit was reached before the commit was
		// found. In which case we should go get just that commit and add it:
		d->Files->Empty();

		// Diff just that ref:
		LString a;
		switch (GetType())
		{
			case VcGit:
			{
				a.Printf("diff %s~ %s", Commit.Get(), Commit.Get());
				StartCmd(a, &VcFolder::ParseSelectCommit);
				break;
			}
			case VcHg:
			{
				a.Printf("log -p -r %s", Commit.Get());
				StartCmd(a, &VcFolder::ParseSelectCommit);
				break;
			}
			default:
			{
				NoImplementation(_FL);
				break;
			}
		}
		
		// if (Parent) LgiMsg(Parent, "The commit '%s' wasn't found", AppName, MB_OK, Commit.Get());
	}
}

bool VcFolder::ParseSelectCommit(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		case VcHg:
		case VcSvn:
		case VcCvs:
		{
			ParseDiff(Result, s, Params);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
	
	return false;
}

void VcFolder::OnBranchesChange()
{
	auto *w = d->Tree->GetWindow();
	if (!w || !LTreeItem::Select())
		return;

	if (Branches.Length())
	{
		// Set the colours up
		LString Default;
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

	UpdateBranchUi();
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
				Fields.Add(LTime);
				Fields.Add(LMessageTxt);
				break;
			}
			case VcGit:
			{
				Fields.Add(LGraph);
				Fields.Add(LRevision);
				Fields.Add(LBranch);
				Fields.Add(LAuthor);
				Fields.Add(LTime);
				Fields.Add(LMessageTxt);
				break;
			}
			default:
			{
				Fields.Add(LGraph);
				Fields.Add(LRevision);
				Fields.Add(LAuthor);
				Fields.Add(LTime);
				Fields.Add(LMessageTxt);
				break;
			}
		}
	}
}

int VcFolder::IndexOfCommitField(CommitField fld)
{
	return (int)Fields.IndexOf(fld);
}

void VcFolder::UpdateColumns(LList *lst)
{
	if (!lst)
		lst = d->Commits;
	
	lst->EmptyColumns();

	for (auto c: Fields)
	{
		switch (c)
		{
			case LGraph:      lst->AddColumn("---",      60); break;
			case LIndex:      lst->AddColumn("Index",    60); break;
			case LBranch:     lst->AddColumn("Branch",   60); break;
			case LRevision:   lst->AddColumn("Revision", 60); break;
			case LAuthor:     lst->AddColumn("Author",   240); break;
			case LTime:       lst->AddColumn("Date",     130); break;
			case LMessageTxt: lst->AddColumn("Message",  700); break;
			default: LAssert(0); break;
		}
	}
}

void VcFolder::FilterCurrentFiles()
{
	LArray<LListItem*> All;
	d->Files->GetAll(All);

	// Update the display property
	for (auto i: All)
	{
		auto fn = i->GetText(COL_FILENAME);
		bool vis = !d->FileFilter || Stristr(fn, d->FileFilter.Get());
		i->GetCss(true)->Display(vis ? LCss::DispBlock : LCss::DispNone);

		// LgiTrace("Filter '%s' by '%s' = %i\n", fn, d->FileFilter.Get(), vis);
	}

	d->Files->Sort(0);
	d->Files->UpdateAllItems();
	d->Files->ResizeColumnsToContent();
}

void VcFolder::ShowAuthor()
{
	if (!AuthorLocal)
		GetAuthor(true, [this](auto name, auto email)
		{
			UpdateAuthorUi();
		});
	if (!AuthorGlobal)
		GetAuthor(false, [this](auto name, auto email)
		{
			UpdateAuthorUi();
		});
	UpdateAuthorUi();
}

void VcFolder::UpdateAuthorUi()
{
	if (AuthorLocal)
		d->Wnd()->SetCtrlName(IDC_AUTHOR, AuthorLocal.ToString());
	else if (AuthorGlobal)
		d->Wnd()->SetCtrlName(IDC_AUTHOR, AuthorGlobal.ToString());
}

LString VcFolder::GetConfigFile(bool local)
{
	switch (GetType())
	{
		case VcHg:
		{
			if (Uri.IsFile())
			{
				LFile::Path p;
				if (local)
				{
					p = LFile::Path(LocalPath()) / ".hg" / "hgrc";
				}
				else
				{
					p = LFile::Path(LSP_HOME) / ".hgrc";
					if (!p.Exists())
						p = LFile::Path(LSP_HOME) / "mercurial.ini";
				}

				d->Log->Print("%s: %i\n", p.GetFull().Get(), p.Exists());
				if (p.Exists())
					return p.GetFull();
			}
			break;
		}
		default:
		{
			NoImplementation(_FL);
			return false;
		}
	}

	return LString();
}

bool VcFolder::GetAuthor(bool local, std::function<void(LString name,LString email)> callback)
{
	auto scope = local ? "--local" : "--global";
	auto target = local ? &AuthorLocal : &AuthorGlobal;

	switch (GetType())
	{
		case VcGit:
		{
			if (target->InProgress)
				return true;

			auto params = new ParseParams;
			params->Callback = [this, callback, target](auto code, auto s)
			{
				for (auto ln: s.Strip().SplitDelimit("\r\n"))
				{
					auto parts = ln.SplitDelimit("=", 1);
					if (parts.Length() == 2)
					{
						if (parts[0].Equals("user.email"))
							target->email = parts[1];
						else if (parts[0].Equals("user.name"))
							target->name = parts[1];
					}
				}

				target->InProgress = false;
				if (callback)
					callback(target->name, target->email);
			};

			auto args = LString::Fmt("config -l %s", scope);
			target->InProgress = StartCmd(args, NULL, params);
			break;
		}
		case VcHg:
		{
			auto config = GetConfigFile(local);
			if (!config)
				return false;

			LIniFile data(config);
			auto author = data.Get("ui", "username");

			auto start = author.Find("<");
			auto end = author.Find(">", start);
			if (start >= 0 &&
				end >= start)
			{
				target->name = author(0, start).Strip();
				target->email = author(start + 1, end).Strip();
			}

			IsGettingAuthor = false;
			callback(target->name, target->email);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			return false;
		}
	}

	return true;
}

bool VcFolder::SetAuthor(bool local, LString name, LString email)
{
	if (!name || !email)
	{
		d->Log->Print("%s:%i - No user/email given.\n", _FL);
		return false;
	}

	auto scope = local ? "--local" : "--global";
	auto target = local ? &AuthorLocal : &AuthorGlobal;

	target->name = name;
	target->email = email;

	switch (GetType())
	{
		case VcGit:
		{
			auto args = LString::Fmt("config %s user.name \"%s\"", scope, name.Get());
			StartCmd(args);

			args = LString::Fmt("config %s user.email \"%s\"", scope, email.Get());
			StartCmd(args);
			break;
		}
		case VcHg:
		{
			auto config = GetConfigFile(local);
			if (!config)
				return false;

			LString author;
			author.Printf("%s <%s>", name.Get(), email.Get());

			LIniFile data(config);
			data.Set("ui", "username", author);
			return data.Write();
		}
		default:
		{
			NoImplementation(_FL);
			return false;
		}
	}

	return true;
}

void VcFolder::OnSelectWithType()
{
	DefaultFields();
	ShowAuthor();
	auto curType = GetType();
	if (curType != d->PrevType)
	{
		d->PrevType = curType;
		UpdateColumns();
	}
}

void VcFolder::OnSelectUpdateItems()
{
	if (d->Commits->Length() > MAX_AUTO_RESIZE_ITEMS)
	{
		int i = 0;
		if (GetType() == VcHg && d->Commits->GetColumns() >= 7)
		{
			d->Commits->ColumnAt(i++)->Width(60);  // LGraph
			d->Commits->ColumnAt(i++)->Width(40);  // LIndex
			d->Commits->ColumnAt(i++)->Width(100); // LRevision
			d->Commits->ColumnAt(i++)->Width(60);  // LBranch
			d->Commits->ColumnAt(i++)->Width(240); // LAuthor
			d->Commits->ColumnAt(i++)->Width(130); // LTimeStamp
			d->Commits->ColumnAt(i++)->Width(400); // LMessage
		}
		else if (d->Commits->GetColumns() >= 5)
		{
			d->Commits->ColumnAt(i++)->Width(40);  // LGraph
			d->Commits->ColumnAt(i++)->Width(270); // LRevision
			d->Commits->ColumnAt(i++)->Width(240); // LAuthor
			d->Commits->ColumnAt(i++)->Width(130); // LTimeStamp
			d->Commits->ColumnAt(i++)->Width(400); // LMessage
		}
	}
	else d->Commits->ResizeColumnsToContent();

	d->Commits->UpdateAllItems();
}

void VcFolder::Select(bool b)
{
	#if PROFILE_FN
	LProfile Prof("Select");
	#endif
	if (!b)
	{
		auto *w = d->Tree->GetWindow();
		w->SetCtrlName(IDC_BRANCH, NULL);
	}

	PROF("Parent.Select");
	LTreeItem::Select(b);
	
	if (b)
	{
		if (Uri.IsFile() && !LDirExists(LocalPath()))
			return;

		if (GetType() == VcPending)
			OnVcsTypeEvents.Add([this]()
				{
					OnSelectWithType();
					OnSelectUpdateItems();
				});
		else
			OnSelectWithType();

		PROF("UpdateCommitList");
		if ((Log.Length() == 0 || CommitListDirty) && !IsLogging)
		{
			switch (GetType())
			{
				case VcGit:
				{
					LVariant Limit;
					d->Opts.GetValue(OPT_GitLimit, Limit);

					LString cmd = "rev-list --all --header --timestamp --author-date-order", s;
					if (Limit.CastInt32() > 0)
					{
						s.Printf(" -n %i", Limit.CastInt32());
						cmd += s;
					}
					
					IsLogging = StartCmd(cmd, &VcFolder::ParseRevList);
					break;
				}
				case VcSvn:
				{
					LVariant Limit;
					d->Opts.GetValue(OPT_SvnLimit, Limit);

					if (CommitListDirty)
					{
						IsLogging = StartCmd("up", &VcFolder::ParsePull, new ParseParams("log"));
						break;
					}
					
					LString s;
					if (Limit.CastInt32() > 0)
						s.Printf("log --limit %i", Limit.CastInt32());
					else
						s = "log";
					IsLogging = StartCmd(s, &VcFolder::ParseLog);
					break;
				}
				case VcHg:
				{
					IsLogging = StartCmd("log", &VcFolder::ParseLog);
					// StartCmd("resolve -l", &VcFolder::ParseResolveList);
					break;
				}
				case VcPending:
				{
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
		if (GetBranches(false))
			OnBranchesChange();

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

			LList *CurOwner = l->GetList();
			if (!CurOwner)
				Ls.Insert(l);
		}

		PROF("Ls Ins");
		d->Commits->Insert(Ls);
		if (d->Resort >= 0)
		{
			PROF("Resort");
			d->SortCommits(d->Resort);
			d->Resort = -1;
		}

		if (GetType() != VcPending)
			OnSelectUpdateItems();

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
	LTimeStamp ats, bts;
	(*a)->GetTs().Get(ats);
	(*b)->GetTs().Get(bts);
	int64 diff = (int64)bts.Get() - ats.Get();
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

bool VcFolder::GetBranches(bool refresh, ParseParams *Params)
{
	if ((!refresh && Branches.Length() > 0) || IsBranches != StatusNone)
		return true;

	switch (GetType())
	{
		case VcGit:
			if (StartCmd("branch -v", &VcFolder::ParseBranches, Params))
				IsBranches = StatusActive;
			break;
		case VcSvn:
			Branches.Add("trunk", new VcBranch("trunk"));
			OnBranchesChange();
			break;
		case VcHg:
		{
			if (StartCmd("branches", &VcFolder::ParseBranches, Params))
				IsBranches = StatusActive;
			
			auto p = new ParseParams([this](auto code, auto str)
				{
					SetCurrentBranch(str.Strip());
				});
			StartCmd("branch", NULL, p);
			break;
		}
		case VcCvs:
			break;
		default:
			break;
	}

	return false;
}

bool VcFolder::ParseRevList(int Result, LString s, ParseParams *Params)
{
	Log.DeleteObjects();

	int Errors = 0;
	switch (GetType())
	{
		case VcGit:
		{
			LString::Array Commits;
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
				LAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				if (Rev->GitParse(Commit, true))
				{
					Log.Add(Rev.Release());
				}
				else
				{
					// LAssert(!"Parse failed.");
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, Commit.Get());
					Errors++;
				}
			}

			LinkParents();
			break;
		}
		default:
			LAssert(!"Impl me.");
			break;
	}

	IsLogging = false;
	return Errors == 0;
}

LString VcFolder::GetFilePart(const char *uri)
{
	LUri u(uri);
	LString File =	u.IsFile() ?
					u.DecodeStr(u.LocalPath()) :
					u.sPath(Uri.sPath.Length(), -1).LStrip("/");
	return File;
}


void VcFolder::ClearLog()
{
	Uncommit.Reset();
	Log.DeleteObjects();
}

void VcFolder::LogFilter(const char *Filter)
{
	if (!Filter)
	{
		LAssert(!"No filter.");
		return;
	}

	switch (GetType())
	{
		case VcGit:
		{
			// See if 'Filter' is a commit id?
			LString args;
			args.Printf("show %s", Filter);
			ParseParams *params = new ParseParams;
			params->Callback = [this, Filter=LString(Filter)](auto code, auto str)
			{
				ClearLog();

				if (code == 0 && str.Find(Filter) >= 0)
				{
					// Found the commit...
					d->Commits->Empty();
					CurrentCommit.Empty();

					ParseLog(code, str, NULL);
					d->Commits->Insert(Log);
				}
				else
				{
					// Not a commit ref...?
					LVariant Limit;
					if (!d->Opts.GetValue(OPT_GitLimit, Limit) || Limit.CastInt32() <= 0)
						Limit = 1000;

					LString args;
					if (Filter.Find("@") == 0)
						args.Printf("log -n %i --author \"%s\"", Limit.CastInt32(), LString(Filter).LStrip("@").Get());
					else
						args.Printf("log -n %i --grep \"%s\"", Limit.CastInt32(), Filter.Get());
					IsLogging = StartCmd(args, &VcFolder::ParseLog);
				}
			};
			StartCmd(args, NULL, params);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
}

void VcFolder::LogFile(const char *uri)
{
	LString Args;
	
	if (IsLogging)
	{
		d->Log->Print("%s:%i - already logging.\n", _FL);
		return;
	}

	switch (GetType())
	{
		case VcGit:
		case VcSvn:
		case VcHg:
		{
			FileToSelect = GetFilePart(uri);
			if (IsLocal() && !LFileExists(FileToSelect))
			{
				LFile::Path Abs(LocalPath());
				Abs += FileToSelect;
				if (Abs.Exists())
					FileToSelect = Abs;
			}

			ParseParams *Params = new ParseParams(uri);
			Args.Printf("log \"%s\"", FileToSelect.Get());
			IsLogging = StartCmd(Args, &VcFolder::ParseLog, Params, LogNormal);
			break;
		}
		default:
			NoImplementation(_FL);
			break;
	}
}

VcLeaf *VcFolder::FindLeaf(const char *Path, bool OpenTree)
{
	VcLeaf *r = NULL;	
	
	if (OpenTree)
		DoExpand();
	
	for (auto n = GetChild(); !r && n; n = n->GetNext())
	{
		auto l = dynamic_cast<VcLeaf*>(n);
		if (l)
			r = l->FindLeaf(Path, OpenTree);
	}
	
	return r;
}

bool VcFolder::ParseLog(int Result, LString s, ParseParams *Params)
{
	int Skipped = 0, Errors = 0;
	bool LoggingFile = Params ? Params->Str != NULL : false;
	VcLeaf *File = LoggingFile ? FindLeaf(Params->Str, true) : NULL; // This may be NULL even if we are logging a file...
	
	LArray<VcCommit*> *Out, BrowseLog;
	if (File)
		Out = &File->Log;
	else if (LoggingFile)
		Out = &BrowseLog;
	else
		Out = &Log;

	LHashTbl<StrKey<char>, VcCommit*> Map;
	for (auto pc: *Out)
		Map.Add(pc->GetRev(), pc);

	if (File)
	{
		for (auto Leaf = File; Leaf; Leaf = dynamic_cast<VcLeaf*>(Leaf->GetParent()))
			Leaf->OnExpand(true);
		File->Select(true);
		File->ScrollTo();
	}	

	switch (GetType())
	{
		case VcGit:
		{
			LString::Array c;
			c.SetFixedLength(false);
			char *prev = s.Get();
			
			#if 0
			LFile::Path outPath("~/code/dump.txt");
			LFile out(outPath.Absolute(), O_WRITE);
			out.Write(s);
			#endif
			
			if (!s)
			{
				OnCmdError(s, "No output from command.");
				return false;
			}

			char *i = s.Get();
			while (*i)
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
			if (prev && i > prev)
			{
				// Last one...
				c.New().Set(prev, i - prev);
			}

			for (auto txt: c)
			{
				LAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				if (Rev->GitParse(txt, false))
				{
					if (!Map.Find(Rev->GetRev()))
						Out->Add(Rev.Release());
					else
						Skipped++;
				}
				else
				{
					LgiTrace("%s:%i - Failed:\n%s\n\n", _FL, txt.Get());
					Errors++;
				}
			}
			
			Out->Sort(CommitDateCmp);
			break;
		}
		case VcSvn:
		{
			LString::Array c = s.Split("------------------------------------------------------------------------");
			for (unsigned i=0; i<c.Length(); i++)
			{
				LAutoPtr<VcCommit> Rev(new VcCommit(d, this));
				LString Raw = c[i].Strip();
				if (Rev->SvnParse(Raw))
				{
					if (File || !Map.Find(Rev->GetRev()))
						Out->Add(Rev.Release());
					else
						Skipped++;
				}
				else if (Raw)
				{
					OnCmdError(Raw, "ParseLog Failed");
					Errors++;
				}
			}
			
			Out->Sort(CommitRevCmp);
			break;
		}
		case VcHg:
		{
			LString::Array c = s.Split("\n\n");
			LHashTbl<IntKey<int64_t>, VcCommit*> Idx;
			for (auto &Commit: c)
			{
				LAutoPtr<VcCommit> Rev(new VcCommit(d, this));
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
			LString::Array c = s.Split("=============================================================================");
			for (auto &Commit: c)
			{
				if (Commit.Strip().Length())
				{
					LString Head, File;
					LString::Array Versions = Commit.Split("----------------------------");
					LString::Array Lines = Versions[0].SplitDelimit("\r\n");
					for (auto &Line: Lines)
					{
						LString::Array p = Line.Split(":", 1);
						if (p.Length() == 2)
						{
							// LgiTrace("Line: %s\n", Line->Get());

							LString Var = p[0].Strip().Lower();
							LString Val = p[1].Strip();
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
								LString::Array f = Val.SplitDelimit(",");
								File = f.First();
							}
						}
					}

					// LgiTrace("%s\n", Commit->Get());

					for (unsigned i=1; i<Versions.Length(); i++)
					{
						LString::Array Lines = Versions[i].SplitDelimit("\r\n");
						if (Lines.Length() >= 3)
						{
							LString Ver = Lines[0].Split(" ").Last();
							LString::Array a = Lines[1].SplitDelimit(";");
							LString Date = a[0].Split(":", 1).Last().Strip();
							LString Author = a[1].Split(":", 1).Last().Strip();
							LString Id = a[2].Split(":", 1).Last().Strip();
							LString Msg = Lines[2];
							LDateTime Dt;
							
							if (Dt.Parse(Date))
							{
								LTimeStamp Ts;
								if (Dt.Get(Ts))
								{
									VcCommit *Cc = Map.Find(Ts.Get());
									if (!Cc)
									{
										Map.Add(Ts.Get(), Cc = new VcCommit(d, this));
										Out->Add(Cc);
										Cc->CvsParse(Dt, Author, Msg);
									}
									Cc->Files.Add(File.Get());
								}
								else LAssert(!"NO ts for date.");
							}
							else LAssert(!"Date parsing failed.");
						}
					}
				}
			}
			break;
		}
		default:
			LAssert(!"Impl me.");
			break;
	}

	if (File)
	{
		File->ShowLog();
	}
	else if (LoggingFile)
	{
		if (auto ui = new BrowseUi(BrowseUi::TLog, d, this, Params->Str))
			ui->ParseLog(BrowseLog, s);
	}

	// LgiTrace("%s:%i - ParseLog: Skip=%i, Error=%i\n", _FL, Skipped, Errors);
	IsLogging = false;

	return !Result;
}

void VcFolder::LinkParents()
{
	#if PROFILE_FN
	LProfile Prof("LinkParents");
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
	typedef LArray<VcEdge*> EdgeArr;
	LArray<EdgeArr> Active;
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
			EdgeArr *Edges = &Active[i];
			for (unsigned n=0; n<Edges->Length(); n++)
			{
				LAssert(Active.PtrCheck(Edges));
				VcEdge *e = (*Edges)[n];

				if (c == e->Child || c == e->Parent)
				{
					LAssert(c->NodeIdx >= 0);
					c->Pos.Add(e, c->NodeIdx);
				}
				else
				{
					// May need to untangle edges with different parents here
					bool Diff = false;
					for (auto edge: *Edges)
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
							for (auto ee: Active[ii])
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

						Edges->Delete(e);
						
						auto &NewEdges = Active[NewIndex];
						NewEdges.Add(e);
						Edges = &Active[i]; // The 'Add' above can invalidate the object 'Edges' refers to

						e->Idx = NewIndex;
						c->Pos.Add(e, NewIndex);
						n--;
					}
					else
					{
						LAssert(e->Idx == i);
						c->Pos.Add(e, i);
					}
				}
			}
		}
		
		// Process terminating edges
		for (auto e: c->Edges)
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

				if (Active[i].HasItem(e))
					Active[i].Delete(e);
				else
					LgiTrace("%s:%i - Warning: Active doesn't have 'e'.\n", _FL);
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
						LAssert(edge->Idx > 0);
						edge->Idx--;
						c->Pos.Add(edge, edge->Idx);
					}
				}
				i--;
			}
		}
	}
}

void VcFolder::UpdateBranchUi()
{
	auto w = d->Wnd();
	DropDownBtn *dd;
	if (w->GetViewById(IDC_BRANCH_DROPDOWN, dd))
	{
		LString::Array a;
		for (auto b: Branches)
			a.Add(b.key);

		dd->SetList(IDC_BRANCH, a);
	}

	LViewI *b;
	if (Branches.Length() > 0 &&
		w->GetViewById(IDC_BRANCH, b))
	{
		if (CurrentBranch)
		{
			b->Name(CurrentBranch);
		}
		else
		{
			auto it = Branches.begin();
			if (it != Branches.end())
				b->Name((*it).key);
		}
	}

	LCombo *Cbo;
	if (w->GetViewById(ID_BRANCHES, Cbo))
	{
		Cbo->Empty();

		int64 select = -1;
		for (auto b: Branches)
		{
			if (CurrentBranch && CurrentBranch == b.key)
				select = Cbo->Length();
			Cbo->Insert(b.key);
		}
		if (select >= 0)
			Cbo->Value(select);

		Cbo->SendNotify(LNotifyTableLayoutRefresh);
		// LgiTrace("%s:%i - Branches len=%i->%i\n", _FL, (int)Branches.Length(), (int)Cbo->Length());
	}
}

VcFile *AppPriv::FindFile(const char *Path)
{
	if (!Path)
		return NULL;

	LArray<VcFile*> files;
	if (Files->GetAll(files))
	{
		LString p = Path;
		p = p.Replace(DIR_STR, "/");
		for (auto f : files)
		{
			auto Fn = f->GetFileName();
			if (p.Equals(Fn))
				return f;
		}
	}

	return NULL;
}

VcFile *VcFolder::FindFile(const char *Path)
{
	return d->FindFile(Path);
}

void VcFolder::NoImplementation(const char* file, int line)
{
	LString s;
	s.Printf("%s, uri=%s, type=%s (%s:%i)",
		LLoadString(IDS_ERR_NO_IMPL_FOR_TYPE),
		Uri.Sanitize().ToString().Get(),
		toString(GetType()),
		file, line);
	OnCmdError(LString(), s);
}

void VcFolder::OnCmdError(LString Output, const char *Msg)
{
	if (!CmdErrors)
	{
		if (Output)
			d->Log->Write(Output);

		auto vc_name = GetVcName();
		if (vc_name)
		{
			LString::Array a = GetProgramsInPath(GetVcName());
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
	GetCss(true)->Color(LColour::Red);
	Update();
}

void VcFolder::ClearError()
{
	GetCss(true)->Color(LCss::ColorInherit);
}

bool VcFolder::ParseInfo(int Result, LString s, ParseParams *Params)
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
			
			LString::Array c = s.Split("\n");
			for (unsigned i=0; i<c.Length(); i++)
			{
				LString::Array a = c[i].SplitDelimit(":", 1);
				if (a.First().Strip().Equals("Revision"))
					CurrentCommit = a[1].Strip();
				else if (a.First().Strip().Equals("URL"))
					RepoUrl = a[1].Strip();
			}
			break;
		}			
		default:
			LAssert(!"Impl me.");
			break;
	}

	IsIdent = Result ? StatusError : StatusNone;

	return true;
}

bool VcFolder::ParseCheckout(int Result, LString s, ParseParams *Params)
{
	if (Result == 0)
	{
		if (Params && Params->Str.Equals("Branch"))
			SetCurrentBranch(NewRev);
		else
			CurrentCommit = NewRev;
	}	

	NewRev.Empty();		
	IsUpdate = false;
	return true;
}

bool VcFolder::ParseWorking(int Result, LString s, ParseParams *Params)
{
	IsListingWorking = false;

	switch (GetType())
	{
		case VcSvn:
		case VcHg:
		{
			ParseParams Local;
			if (!Params) Params = &Local;
			Params->IsWorking = true;
			ParseStatus(Result, s, Params);
			
			#if 1
			if (GetType() == VcHg)
				ConflistCheck();
			#endif
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
						auto f = new VcFile(d, this, LString(), true);
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
			ParseDiffs(s, LString(), true);
			break;
		}
	}
	
	FilterCurrentFiles();
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
			p->Str = LString(FromRev) + ":" + ToRev;

			LString a;
			a.Printf("diff -r%s:%s", FromRev, ToRev);
			StartCmd(a, &VcFolder::ParseDiff, p);
			break;
		}
		case VcGit:
		{
			ParseParams *p = new ParseParams;
			p->IsWorking = false;
			p->Str = LString(FromRev) + ":" + ToRev;

			LString a;
			a.Printf("diff %s..%s", FromRev, ToRev);
			StartCmd(a, &VcFolder::ParseDiff, p);
			break;
		}
		case VcCvs:
		case VcHg:
		default:
		{
			auto sType = toString(GetType());
			d->Log->Print("%s:%i - 'DiffRange' for %s not implemented.\n", _FL, sType);
			break;
		}
	}
}

bool VcFolder::ParseDiff(int Result, LString s, ParseParams *Params)
{
	if (Params)
		ParseDiffs(s, Params->Str, Params->IsWorking);
	else
		ParseDiffs(s, LString(), true);
	return false;
}

const char *VcFolder::NoPipeOpt()
{
	switch (GetType())
	{
		case VcGit:
			return "-P ";
		case VcHg:
			return "--pager none ";
		default:
			return "";
	}
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
			LString a;

			auto rev = file->GetRevision();
			if (rev)
				a.Printf("diff %s \"%s\"", rev, Fn);
			else
				a.Printf("diff \"%s\"", Fn);
			
			StartCmd(a, &VcFolder::ParseDiff);
			break;
		}
		case VcSvn:
		{
			LString a;
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
			LAssert(!"Impl me.");
			break;
	}
}

void VcFolder::InsertFiles(List<LListItem> &files)
{
	d->Files->Insert(files);
	if (FileToSelect)
	{
		LListItem *scroll = NULL;
		for (auto f: files)
		{
			// Convert to an absolute path:
			bool match = false;
			auto relPath = f->GetText(COL_FILENAME);
			if (IsLocal())
			{
				LFile::Path p(LocalPath());
				p += relPath;
				match = p.GetFull().Equals(FileToSelect);
			}
			else
			{
				match = !Stricmp(FileToSelect.Get(), relPath);
			}
			f->Select(match);
			if (match)
				scroll = f;
		}
		if (scroll)
			scroll->ScrollTo();
	}
}

bool VcFolder::ParseDiffs(LString s, LString Rev, bool IsWorking)
{
	LAssert(IsWorking || Rev.Get() != NULL);

	switch (GetType())
	{
		case VcGit:
		{
			List<LListItem> Files;
			LString::Array a = s.Split("\n");
			LString Diff;
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
					LString Fn, State = "M";
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
					Files.Insert(f);
				}
				else if (!_strnicmp(Ln, "new file", 8))
				{
					if (f)
						f->SetText("A", COL_STATE);
				}
				else if (!_strnicmp(Ln, "deleted file", 12))
				{
					if (f)
						f->SetText("D", COL_STATE);
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

			InsertFiles(Files);
			break;
		}
		case VcHg:
		{
			LString Sep("\n");
			LString::Array a = s.Split(Sep);
			LString::Array Diffs;
			VcFile *f = NULL;
			List<LListItem> Files;
			LProgressDlg Prog(GetTree(), 1000);

			Prog.SetDescription("Reading diff lines...");
			Prog.SetRange(a.Length());
			// Prog.SetYieldTime(300);

			for (unsigned i=0; i<a.Length(); i++)
			{
				const char *Ln = a[i];
				if (!_strnicmp(Ln, "diff", 4))
				{
					if (f)
						f->SetDiff(Sep.Join(Diffs));
					Diffs.Empty();

					auto MainParts = a[i].Split(" -r ");
					auto FileParts = MainParts.Last().Split(" ",1);
					LString Fn = FileParts.Last();

					f = FindFile(Fn);
					if (!f)
						f = new VcFile(d, this, Rev, IsWorking);

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
					Diffs.Add(a[i]);
				}

				Prog.Value(i);
				if (Prog.IsCancelled())
					break;
			}
			if (f && Diffs.Length())
			{
				f->SetDiff(Sep.Join(Diffs));
				Diffs.Empty();
			}
			InsertFiles(Files);
			break;
		}
		case VcSvn:
		{
			List<LListItem> Files;
			LString::Array a = s.Replace("\r").Split("\n");
			LString Diff;
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

					LString Fn = a[i].Split(":", 1).Last().Strip();

					f = FindFile(Fn);
					if (!f)
						f = new VcFile(d, this, Rev, IsWorking);

					f->SetText(Fn.Replace("\\","/"), COL_FILENAME);
					f->SetText("M", COL_STATE);
					f->GetStatus();
					Files.Insert(f);
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
			InsertFiles(Files);
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
			LAssert(!"Impl me.");
			break;
		}
	}

	FilterCurrentFiles();
	return true;
}

bool VcFolder::ParseFiles(int Result, LString s, ParseParams *Params)
{
	d->ClearFiles();
	ParseDiffs(s, Params->Str, false);
	IsFilesCmd = false;
	FilterCurrentFiles();

	return false;
}

#if HAS_LIBSSH
void VcFolder::OnSshCmd(SshParams *p)
{
	if (!p || !p->f)
	{
		LAssert(!"Param error.");
		return;
	}

	LString s = p->Output;
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
			if (LTreeItem::Select())
				Select(true);
		}
	}
	
	if (p->Params &&
		p->Params->Callback)
	{
		p->Params->Callback(Result, s);
	}
}
#endif

void VcFolder::OnPulse()
{
	bool Reselect = false, CmdsChanged = false;
	static bool Processing = false;
	
	if (!Processing)
	{	
		Processing = true;	// Lock out processing, if it puts up a dialog or something...
							// bad things happen if we try and re-process something.	

		for (unsigned i=0; i<Cmds.Length(); i++)
		{
			auto c = Cmds[i];
			if (!c)
			{
				Cmds.DeleteAt(i--, true);
				continue;
			}

			if (c->Rd->GetState() == LThread::THREAD_INIT)
			{
				if (CmdActiveThreads < CmdMaxThreads)
				{
					// Start ready thread...
					c->Rd->Run();
					CmdActiveThreads++;
				}
			}
			else if (c->Rd->IsExited())
			{
				// Thread is finished...
				CmdActiveThreads--;

				auto s = c->GetBuf();
				auto Result = c->Rd->ExitCode();

				if (Result == ErrSubProcessFailed)
				{
					if (!CmdErrors)
						d->Log->Print("Error: Can't run '%s'\n", GetVcName());
					CmdErrors++;
				}
				else if (c->PostOp)
				{
					if (s.Length() == 18 &&
						s.Equals("LSUBPROCESS_ERROR\n"))
					{
						OnCmdError(s, "Sub process failed.");
					}
					else
					{
						Reselect |= CALL_MEMBER_FN(*this, c->PostOp)(Result, s, c->Params);
					}
				}
				
				if (c->Params &&
					c->Params->Callback)
				{
					c->Params->Callback(Result, s);
				}
					
				Cmds.DeleteAt(i--, true);
				delete c;
				CmdsChanged = true;
			}
		}

		Processing = false;
	}

	if (Reselect)
	{
		if (LTreeItem::Select())
			Select(true);
	}
	
	if (CmdsChanged)
	{
		Update();
	}

	if (CmdErrors)
	{
		d->Tabs->Value(1);
		CmdErrors = false;
	}
}

void VcFolder::OnRemove()
{
	LXmlTag *t = d->Opts.LockTag(OPT_Folders, _FL);
	if (t)
	{
		Uncommit.Reset();
		if (LTreeItem::Select())
		{
			d->Files->Empty();
			d->Commits->RemoveAll();
		}

		bool Found = false;
		auto u = Uri.ToString();
		for (auto c: t->Children)
		{
			if (!c->IsTag(OPT_Folder))
				printf("%s:%i - Wrong tag: %s, %s\n", _FL, c->GetTag(), OPT_Folder);
			else if (!c->GetContent())
				printf("%s:%i - No content.\n", _FL);
			else
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
		LAssert(Found);

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
		GetCss(true)->Color(LColour::Blue);
}

void VcFolder::OnMouseClick(LMouse &m)
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
		s.AppendItem("Goto Item", ID_GOTO_ITEM);
		s.AppendSeparator();
		s.AppendItem("Remove", IDM_REMOVE);
		s.AppendItem("Remote URL", IDM_REMOTE_URL);
		if (!Uri.IsFile())
		{
			s.AppendSeparator();
			s.AppendItem("Edit Location", IDM_EDIT);
		}
		
		m -= GetTree()->ScrollPxPos();
		auto Cmd = s.Float(GetTree(), m);
		switch (Cmd)
		{
			case IDM_BROWSE_FOLDER:
			{
				LBrowseToFile(LocalPath());
				break;
			}
			case IDM_TERMINAL:
			{
				auto p = LocalPath();
				TerminalAt(p);
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
				auto Dlg = new LInput(GetTree(), Uri.ToString(), "URI:", "Remote Folder Location");
				Dlg->DoModal([this, Dlg](auto dlg, auto ctrlId)
				{
					if (ctrlId)
					{
						Uri.Set(Dlg->GetStr());
						Empty();
						Select(true);
					}
				});
				break;
			}
			case IDM_REMOTE_URL:
			{
				GetRemoteUrl([this](auto code, auto str)
				{
					LString Url = str.Strip();
					if (Url)
					{
						auto a = new LAlert(GetTree(), "Remote Url", Url, "Copy", "Ok");
						a->DoModal([this, Url](auto dlg, auto code)
						{
							if (code == 1)
							{
								LClipBoard c(GetTree());
								c.Text(Url);
							}
						});
					}
				});
				break;
			}
			case ID_GOTO_ITEM:
			{
				LClipBoard clip(GetTree());
				auto txt = clip.Text();

				if (auto inp = new LInput(GetTree(), txt, "Item path:", "Goto Item"))
					inp->DoModal([this, inp](auto dlg, auto status)
					{
						if (!status)
							return;

						if (auto path = inp->GetStr())
						{
							pathParts = path.SplitDelimit("\\/");
							PathSeek();
						}
					});
				break;
			}
			default:
				break;
		}
	}
}

void VcFolder::PathSeek()
{
	LTreeItem *item = this;
	for (size_t i=0; i<pathParts.Length(); i++)
	{
		auto p = pathParts[i];
		if (!item->Expanded())
			item->Expanded(true);

		LTreeItem *match = nullptr;
		for (auto c = item->GetChild(); c; c = c->GetNext())
		{
			auto txt = c->GetText();
			if (p.Equals(txt))
			{
				match = c;
				break;
			}
		}

		if (match)
			item = match;
		else
			break;
	}

	if (item && item != (LTreeItem*)this)
	{
		if (!item->Select())
			item->Select(true);
		item->ScrollTo();
	}
}

LString &VcFolder::GetCurrentBranch()
{
	return CurrentBranch;
}

void VcFolder::SetCurrentBranch(LString name)
{
	if (CurrentBranch != name)
	{
		CurrentBranch = name;
		UpdateBranchUi();
	}
}

void VcFolder::Checkout(const char *Rev, bool isBranch)
{
	if (!Rev || IsUpdate)
		return;
	
	LString Args;

	LAutoPtr<ParseParams> params(new ParseParams(isBranch ? "Branch" : "Rev"));

	NewRev = Rev;
	switch (GetType())
	{
		case VcGit:
			Args.Printf("checkout %s", Rev);
			IsUpdate = StartCmd(Args, &VcFolder::ParseCheckout, params.Release(), LogNormal);
			break;
		case VcSvn:
			Args.Printf("up -r %s", Rev);
			IsUpdate = StartCmd(Args, &VcFolder::ParseCheckout, params.Release(), LogNormal);
			break;
		case VcHg:
			Args.Printf("update -r %s", Rev);
			IsUpdate = StartCmd(Args, &VcFolder::ParseCheckout, params.Release(), LogNormal);
			break;
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
}

void VcFolder::Delete(const char *Path, bool KeepLocal)
{
	switch (GetType())
	{
		case VcHg:
		{
			LString args;
			if (KeepLocal)
				args.Printf("forget \"%s\"", Path);
			else
				args.Printf("remove \"%s\"", Path);

			StartCmd(args, &VcFolder::ParseDelete);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
}

bool VcFolder::ParseDelete(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcHg:
		{			
			break;
		}
	}

	return true;
}

void VcFolder::DeleteBranch(const char *Name, ParseParams::TCallback cb)
{
	if (!Name)
		return OnCmdError(LString(), "No name specified.");

	switch (GetType())
	{
		case VcGit:
		{
			LString args;
			args.Printf("branch -D \"%s\"", Name);

			ParseParams *params = nullptr;
			if (cb)
				params = new ParseParams(std::move(cb));
			StartCmd(args, nullptr, params);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
}

void VcFolder::MergeBranch(const char *Name, ParseParams::TCallback cb)
{
	if (!Name)
		return OnCmdError(LString(), "No name specified.");

	switch (GetType())
	{
		case VcGit:
		{
			LString args;
			args.Printf("merge \"%s\"", Name);

			ParseParams *params = nullptr;
			if (cb)
				params = new ParseParams(std::move(cb));
			StartCmd(args, nullptr, params);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
}

void VcFolder::CreateBranch(const char *Name, ParseParams::TCallback cb)
{
	if (!Name)
		return OnCmdError(LString(), "No name specified.");

	switch (GetType())
	{
		case VcGit:
		{
			LString args;
			args.Printf("checkout -b \"%s\"", Name);

			ParseParams *params = nullptr;
			if (cb)
				params = new ParseParams(std::move(cb));
			StartCmd(args, nullptr, params);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
int FolderCompare(LTreeItem *a, LTreeItem *b, NativeInt UserData)
{
	VcLeaf *A = dynamic_cast<VcLeaf*>(a);
	VcLeaf *B = dynamic_cast<VcLeaf*>(b);
	if (!A || !B)
		return 0;
	return A->Compare(B);
}

struct SshFindEntry
{
	LString Flags, Name, User, Group;
	uint64_t Size;
	LDateTime Modified, Access;

	SshFindEntry &operator =(const LString &s)
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

bool VcFolder::ParseRemoteFind(int Result, LString s, ParseParams *Params)
{
	if (!Params || !s)
		return false;

	auto Lines = s.SplitDelimit("\r\n");
	if (Result)
	{
		d->Log->Print("%s:%i - error: %s\n", _FL, s.Get());
		LPopupNotification::Message(GetTree()->GetWindow(), LString::Fmt("Error: %s", Lines.Last().Get()));
		return false;
	}

	auto Parent = Params->Leaf ? static_cast<LTreeItem*>(Params->Leaf) : static_cast<LTreeItem*>(this);
	LUri u(Params->Str);

	// Parse the output
	LArray<SshFindEntry> Entries;
	for (size_t i=1; i<Lines.Length(); i++)
	{
		auto &e = Entries.New();
		e = Lines[i];
		if (!e.Name)
			Entries.PopLast();
	}
	Entries.Sort(SshFindEntry::Compare);

	// Get a map of existing entries:
	LHashTbl<ConstStrKey<char,false>, LTreeItem*> existing;
	for (auto i = Parent->GetChild(); i; i = i->GetNext())
	{
		auto txt = i->GetText();
		existing.Add(txt, i);
	}

	for (auto &Dir: Entries)
	{
		auto name = Dir.GetName();
		if (existing.Find(name))
			continue;

		if (Dir.IsDir())
		{
			if (Dir.GetName()[0] != '.')
			{
				new VcLeaf(this, Parent, Params->Str, Dir.GetName(), true);
			}
		}
		else if (!Dir.IsHidden())
		{
			auto Ext = LGetExtension(Dir.GetName());
			if (!Ext) continue;
			if (!stricmp(Ext, "c") ||
				!stricmp(Ext, "cpp") ||
				!stricmp(Ext, "h"))
			{
				LUri Path = u;
				Path += Dir.GetName();
				new VcLeaf(this, Parent, Params->Str, Dir.GetName(), false);
			}
		}
	}

	PathSeek();
	return false;
}

void VcFolder::ReadDir(LTreeItem *Parent, const char *ReadUri)
{
	LUri u(ReadUri);

	if (u.IsFile())
	{
		// Read child items
		LDirectory Dir;
		for (int b = Dir.First(u.LocalPath()); b; b = Dir.Next())
		{
			auto name = Dir.GetName();
			if (Dir.IsHidden())
				continue;
				
			LUri Path = u;
			Path += name;
			new VcLeaf(this, Parent, u.ToString(), name, Dir.IsDir());
		}

		PathSeek();
	}
	#if HAS_LIBSSH
	else
	{
		auto c = d->GetConnection(ReadUri);
		if (!c)
			return;
		
		LString uri = ReadUri;
		if (uri(-1) != '/')
			uri += "/";

		LString Path = u.sPath(Uri.sPath.Length(), -1).LStrip("/");
		LString Args;
		Args.Printf("\"%s\" -maxdepth 1 -printf \"%%M/%%g/%%u/%%A@/%%T@/%%s/%%P\n\"", Path ? Path.Get() : ".");

		auto *Params = new ParseParams(uri);
		Params->Leaf = dynamic_cast<VcLeaf*>(Parent);

		c->Command(this, "find", Args, &VcFolder::ParseRemoteFind, Params, SshConnection::LogNone);
		return;
	}
	#endif

	Parent->Sort(FolderCompare);
}

void VcFolder::OnVcsType(LString errorMsg)
{
	if (!d)
	{
		LAssert(!"No priv instance");
		return;
	}

	#if HAS_LIBSSH
	auto c = d->GetConnection(Uri.ToString(), false);
	if (c)
	{
		auto NewType = c->Types.Find(Uri.sPath);
		if (NewType && NewType != Type)
		{
			if (NewType == VcError)
			{
				OnCmdError(LString(), errorMsg);
			}
			else
			{
				Type = NewType;
				ClearError();
				Update();

				if (LTreeItem::Select())
					Select(true);

				for (auto &e: OnVcsTypeEvents)
					e();
				OnVcsTypeEvents.Empty();
			}
		}
	}
	#endif
}

void VcFolder::DoExpand()
{
	if (Tmp)
	{
		Tmp->Remove();
		DeleteObj(Tmp);
		ReadDir(this, Uri.ToString());
	}
}

void VcFolder::OnExpand(bool b)
{
	if (b)
		DoExpand();
}

void VcFolder::ListCommit(VcCommit *c)
{
	if (!IsFilesCmd)
	{
		LString Args;
		switch (GetType())
		{
			case VcGit:
				Args.Printf("show %s^..%s", c->GetRev(), c->GetRev());
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
				FilterCurrentFiles();
				break;
			}
			case VcHg:
			{
				Args.Printf("diff --change %s", c->GetRev());
				IsFilesCmd = StartCmd(Args, &VcFolder::ParseFiles, new ParseParams(c->GetRev()));
				break;
			}
			default:
				LAssert(!"Impl me.");
				break;
		}

		if (IsFilesCmd)
			d->ClearFiles();
	}
}

LString ConvertUPlus(LString s)
{
	LArray<uint32_t> c;
	LUtf8Ptr p(s);
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
	return LString((LString::Char32*)c.AddressOf());
}

bool VcFolder::ParseStatus(int Result, LString s, ParseParams *Params)
{
	bool ShowUntracked = d->Wnd()->GetCtrlValue(ID_UNTRACKED) != 0;
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
			LFile Tmp("C:\\tmp\\output.txt", O_WRITE);
			Tmp.Write(s);
			Tmp.Close();
			#endif

			auto a = s.Split("===================================================================");
			for (auto i : a)
			{
				auto Lines = i.SplitDelimit("\r\n");
				if (Lines.Length() == 0)
					continue;
				auto f = Lines[0].Strip();
				if (f.Find("File:") == 0)
				{
					auto Parts = f.SplitDelimit("\t");
					auto File = Parts[0].Split(": ").Last().Strip();
					auto Status = Parts[1].Split(": ").Last();
					LString WorkingRev;

					for (auto l : Lines)
					{
						auto p = l.Strip().Split(":", 1);
						if (p.Length() > 1 &&
							p[0].Strip().Equals("Working revision"))
						{
							WorkingRev = p[1].Strip();
						}
					}

					auto f = Map.Find(File);
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
					LString File = f(2, -1);

					VcFile *f = Map.Find(File);
					if (!f)
					{
						if ((f = new VcFile(d, this, LString(), IsWorking)))
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
			auto Lines = s.SplitDelimit("\r\n");
			int Fmt = ToolVersion[VcGit] >= Ver2Int("2.8.0") ? 2 : 1;
			for (auto Ln : Lines)
			{
				auto Type = Ln(0);
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
						LString::Array p = Ln.SplitDelimit(" ", 8);
						if (p.Length() < 7)
							d->Log->Print("%s:%i - Error: not enough tokens: '%s'\n", _FL, Ln.Get());
						else
						{
							auto path = p[6];
							f = new VcFile(d, this, path, IsWorking);
							auto state = p[1].Strip(".");
							auto pos = p[1].Find(state);
							
							d->Log->Print("%s state='%s' pos=%i\n", path.Get(), state.Get(), (int)pos);
							
							f->SetText(state, COL_STATE);
							f->SetText(p.Last(), COL_FILENAME);
							f->SetStaged(pos == 0);
						}
					}
					else if (Fmt == 1)
					{
						LString::Array p = Ln.SplitDelimit(" ");
						f = new VcFile(d, this, LString(), IsWorking);
						f->SetText(p[0], COL_STATE);
						f->SetText(p.Last(), COL_FILENAME);
					}
					
					if (f)
						Ins.Insert(f);
				}
				else if (ShowUntracked)
				{
					VcFile *f = new VcFile(d, this, LString(), IsWorking);
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
			
			LString::Array Lines = s.SplitDelimit("\r\n");
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
					LString::Array p = Ln.SplitDelimit(" ", 1);
					if (p.Length() == 2)
					{
						LString File;
						if (GetType() == VcSvn)
							File = ConvertUPlus(p.Last());
						else
							File = p.Last();

						if (GetType() == VcSvn &&
							File.Find("+    ") == 0)
						{
							File = File(5, -1);
						}

						VcFile *f = new VcFile(d, this, LString(), IsWorking);
						f->SetText(p[0], COL_STATE);
						f->SetText(File.Replace("\\","/"), COL_FILENAME);
						f->GetStatus();
						Ins.Insert(f);
					}
					else LAssert(!"What happen?");
				}
				else if (ShowUntracked)
				{
					VcFile *f = new VcFile(d, this, LString(), IsWorking);
					f->SetText("?", COL_STATE);
					f->SetText(Ln(2,-1), COL_FILENAME);
					Ins.Insert(f);
				}
			}
			break;
		}
		default:
		{
			LAssert(!"Impl me.");
			break;
		}
	}

	if ((Unpushed = Ins.Length() > 0))
	{
		if (CmdErrors == 0)
			GetCss(true)->Color(LColour(255, 128, 0));
	}
	else if (Unpulled == 0)
	{
		GetCss(true)->Color(LCss::ColorInherit);
	}

	Update();
	if (LTreeItem::Select())
	{
		d->Files->Insert(Ins);
		FilterCurrentFiles();
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
	LString Arg;
	switch (GetType())
	{
		default:
		case VcSvn:
		case VcHg:
		case VcCvs:
			return false;
		case VcGit:
			Arg = "submodule update --init --recursive";
			break;
	}

	return StartCmd(Arg, &VcFolder::ParseUpdateSubs, NULL, LogNormal);
}

bool VcFolder::ParseUpdateSubs(int Result, LString s, ParseParams *Params)
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
	LUri Uri(uri);
	if (Uri.IsFile() && Uri.sPath)
	{
		LFile::Path p(Uri.sPath(1,-1));
		if (!p.IsFolder())
		{
			LAssert(!"Needs to be a folder.");
			return;
		}
	}

	if (LTreeItem::Select())
		d->ClearFiles();

	LString Arg;
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
				LAssert(!"Where is the version?");
			
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
			LString Arg;
			Arg.Printf("id -n -r %s", CurrentBranch.Get());
			StartCmd(Arg, &VcFolder::ParseCountToTip);
		}
	}
}

bool VcFolder::ParseCountToTip(int Result, LString s, ParseParams *Params)
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
	if (IsListingWorking)
		return;

	d->ClearFiles();
		
	bool Untracked = d->IsMenuChecked(IDM_UNTRACKED);

	LString Arg;
	switch (GetType())
	{
		case VcPending:
			OnVcsTypeEvents.Add([this]()
			{
				ListWorkingFolder();
			});
			break;
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
			#if 1
				Arg = "status -vv";
			#else
				Arg = "diff --diff-filter=CMRTU --cached";
			#endif
			break;
		case VcHg:
			Arg = "status -mard";
			break;
		default:
			return;
	}

	IsListingWorking = StartCmd(Arg, &VcFolder::ParseWorking);
}

bool VcFolder::ParseConflicts(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcHg:
		{
			if (Result)
				break;
			auto lines = s.SplitDelimit("\n");
			for (auto line: lines)
			{
				auto parts = line.SplitDelimit(" \t", 1);
				if (parts[0].Equals("U"))
				{
					auto f = FindFile(parts.Last());
					if (!f)
					{
						f = new VcFile(d, this, LString(), true);
						f->SetText(parts[0], COL_STATE);
						f->SetText(parts[1], COL_FILENAME);
						f->GetStatus();
						d->Files->Insert(f);
					}
					else
					{
						f->SetText(parts[0], COL_STATE);
						f->SetStatus(VcFile::SConflicted);
						f->Update();
					}
				}
			}
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
	
	return false;
}

void VcFolder::ConflistCheck()
{
	switch (GetType())
	{
		case VcHg:
		{
			StartCmd("resolve -l", &VcFolder::ParseConflicts);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}
}

void VcFolder::GitAdd()
{
	if (!PostAdd)
		return;

	LString Args;
	if (PostAdd->Files.Length() == 0)
	{
		LString m(PostAdd->Msg);
		m = m.Replace("\"", "\\\"");
		Args.Printf("commit -m \"%s\"", m.Get());
		IsCommit = StartCmd(Args, &VcFolder::ParseCommit, PostAdd->Param, LogNormal);
		PostAdd.Reset();
	}
	else
	{
		char NativeSep[] = {GetPathSep(), 0};
		LString Last = PostAdd->Files.Last();
		Args.Printf("add \"%s\"", Last.Replace("\"", "\\\"").Replace("/", NativeSep).Get());
		PostAdd->Files.PopLast();	
		StartCmd(Args, &VcFolder::ParseGitAdd, NULL, LogNormal);
	}
}

bool VcFolder::ParseGitAdd(int Result, LString s, ParseParams *Params)
{
	if (Result)
	{
		OnCmdError(s, "add failed.");
	}
	else
	{
		GitAdd();
	}

	return false;
}

bool VcFolder::ParseCommit(int Result, LString s, ParseParams *Params)
{
	if (LTreeItem::Select())
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
					auto i = new LInput(GetTree(), "", "Git user name:", AppName);
					i->DoModal([this, i](auto dlg, auto ctrlId)
					{
						if (ctrlId)
						{
							LString Args;
							Args.Printf("config --global user.name \"%s\"", i->GetStr().Get());
							StartCmd(Args);

							auto inp = new LInput(GetTree(), "", "Git user email:", AppName);
							i->DoModal([this, inp](auto dlg, auto ctrlId)
							{
								if (ctrlId)
								{
									LString Args;
									Args.Printf("config --global user.email \"%s\"", inp->GetStr().Get());
									StartCmd(Args);
								}
							});
						}
					});
				}
				break;
			}
			default:
				break;
		}
		
		return false;
	}

	if (Result == 0 && LTreeItem::Select())
	{
		d->ClearFiles();

		auto *w = d->Diff ? d->Diff->GetWindow() : NULL;
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

			GetTree()->SendNotify((LNotifyType)LvcCommandEnd);
			if (!Result)
			{
				Unpushed = 0;
				Update();
				GetCss(true)->Color(LColour::Green);
			}
			break;
		}
		case VcHg:
		{
			CurrentCommit.Empty();
			CommitListDirty = true;
			GetTree()->SendNotify((LNotifyType)LvcCommandEnd);
			if (!Result)
			{
				Unpushed = 0;
				Update();

				if (Params && Params->Str.Find("Push") >= 0)
					Push();
				else
					GetCss(true)->Color(LColour::Green);
			}
			break;
		}
		case VcCvs:
		{
			CurrentCommit.Empty();
			CommitListDirty = true;
			GetTree()->SendNotify((LNotifyType)LvcCommandEnd);
			if (!Result)
			{
				Unpushed = 0;
				Update();
				GetCss(true)->Color(LColour::Green);
			}
			break;
		}
		default:
		{
			LAssert(!"Impl me.");
			break;
		}
	}

	return true;
}

void VcFolder::Commit(const char *Msg, const char *Branch, bool AndPush)
{
	LArray<VcFile*> Add;
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

		LJson j;
		j.Set("Command", "commit");
		j.Set("Msg", Msg);
		j.Set("AndPush", (int64_t)AndPush);
		StartBranch(Branch, j.GetJson());
		return;
	}

	if (!IsCommit)
	{
		LString Args;
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
					LString m(Msg);
					m = m.Replace("\"", "\\\"");
					Args.Printf("commit -am \"%s\"", m.Get());
					IsCommit = StartCmd(Args, &VcFolder::ParseCommit, Param, LogNormal);
				}
				break;
			}
			case VcSvn:
			{
				LString::Array a;
				a.New().Printf("commit -m \"%s\"", Msg);
				for (auto pf: Add)
				{
					LString s = pf->GetFileName();
					if (s.Find(" ") >= 0)
						a.New().Printf("\"%s\"", s.Get());
					else
						a.New() = s;
				}

				Args = LString(" ").Join(a);
				IsCommit = StartCmd(Args, &VcFolder::ParseCommit, Param, LogNormal);

				if (d->Tabs && IsCommit)
				{
					d->Tabs->Value(1);
					GetTree()->SendNotify((LNotifyType)LvcCommandStart);
				}
				break;
			}
			case VcHg:
			{
				LString::Array a;
				LString CommitMsg = Msg;
				TmpFile Tmp;
				if (CommitMsg.Find("\n") >= 0)
				{
					Tmp.Create().Write(Msg);
					a.New().Printf("commit -l \"%s\"", Tmp.GetName());
				}
				else
				{				
					a.New().Printf("commit -m \"%s\"", Msg);
				}
				if (Partial)
				{
					for (auto pf: Add)
					{
						LString s = pf->GetFileName();
						if (s.Find(" ") >= 0)
							a.New().Printf("\"%s\"", s.Get());
						else
							a.New() = s;
					}
				}

				Args = LString(" ").Join(a);
				IsCommit = StartCmd(Args, &VcFolder::ParseCommit, Param, LogNormal);

				if (d->Tabs && IsCommit)
				{
					d->Tabs->Value(1);
					GetTree()->SendNotify((LNotifyType)LvcCommandStart);
				}
				break;
			}
			case VcCvs:
			{
				LString a;
				a.Printf("commit -m \"%s\"", Msg);
				IsCommit = StartCmd(a, &VcFolder::ParseCommit, NULL, LogNormal);
				break;
			}
			default:
			{
				OnCmdError(LString(), "No commit impl for type.");
				break;
			}
		}
	}
}

bool VcFolder::ParseStartBranch(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcHg:
		{
			if (Result == 0 && Params && Params->Str)
			{
				LJson j(Params->Str);
				auto cmd = j.Get("Command");
				if (cmd.Equals("commit"))
				{
					auto Msg = j.Get("Msg");
					auto AndPush = j.Get("AndPush").Int();
					if (Msg)
					{
						Commit(Msg, NULL, AndPush > 0);
					}
				}
			}
			break;
		}
		default:
		{
			OnCmdError(LString(), "No commit impl for type.");
			break;
		}
	}
	
	return true;
}

void VcFolder::StartBranch(const char *BranchName, const char *OnCreated)
{
	if (!BranchName)
		return;
	
	switch (GetType())
	{
		case VcHg:
		{
			LString a;
			a.Printf("branch \"%s\"", BranchName);

			StartCmd(a, &VcFolder::ParseStartBranch, OnCreated ? new ParseParams(OnCreated) : NULL);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}	
}

void VcFolder::Push(bool NewBranchOk)
{
	LString Args;
	bool Working = false;
	switch (GetType())
	{
		case VcHg:
		{
			auto args = NewBranchOk ? "push --new-branch" : "push";
			Working = StartCmd(args, &VcFolder::ParsePush, NULL, LogNormal);
			break;
		}
		case VcGit:
		{
			LString args;
			if (NewBranchOk)
			{
				if (CurrentBranch)
				{
					args.Printf("push --set-upstream origin %s", CurrentBranch.Get());
				}
				else
				{
					OnCmdError(LString(), "Don't have the current branch?");
					return;
				}
			}
			else
			{
				args = "push";
			}

			Working = StartCmd(args, &VcFolder::ParsePush, NULL, LogNormal);
			break;
		}
		case VcSvn:
		{
			// Nothing to do here.. the commit pushed the data already
			break;
		}
		default:
		{
			OnCmdError(LString(), "No push impl for type.");
			break;
		}
	}

	if (d->Tabs && Working)
	{
		d->Tabs->Value(1);
		GetTree()->SendNotify((LNotifyType)LvcCommandStart);
	}
}

bool VcFolder::ParsePush(int Result, LString s, ParseParams *Params)
{
	bool Status = false;
	
	if (Result)
	{
		bool needsNewBranchPerm = false;

		switch (GetType())
		{
			case VcHg:
			{
				needsNewBranchPerm = s.Find("push creates new remote branches") >= 0;
				break;
			}
			case VcGit:
			{
				needsNewBranchPerm = s.Find("The current branch") >= 0 &&
									 s.Find("has no upstream branch") >= 0;
				break;
			}
		}

		if (needsNewBranchPerm &&
			LgiMsg(GetTree(), LLoadString(IDS_CREATE_NEW_BRANCH), AppName, MB_YESNO) == IDYES)
		{
			Push(true);
			return false;
		}
		
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
		GetCss(true)->Color(LColour::Green);
		Update();
		Status = true;
	}

	GetTree()->SendNotify((LNotifyType)LvcCommandEnd);
	return Status; // no reselect
}

void VcFolder::Pull(int AndUpdate, LoggingType Logging)
{
	bool Status = false;

	if (AndUpdate < 0)
		AndUpdate = GetTree()->GetWindow()->GetCtrlValue(ID_UPDATE) != 0;

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
		case VcPending:
			OnVcsTypeEvents.New() = [this, AndUpdate, Logging]()
			{
				Pull(AndUpdate, Logging);
			};
			break;
		default:
			NoImplementation(_FL);
			break;
	}

	if (d->Tabs && Status)
	{
		d->Tabs->Value(1);
		GetTree()->SendNotify((LNotifyType)LvcCommandStart);
	}
}

bool VcFolder::ParsePull(int Result, LString s, ParseParams *Params)
{
	GetTree()->SendNotify((LNotifyType)LvcCommandEnd);
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
				GetCss(true)->Color(LColour::Green);
			else
				GetCss(true)->Color(LCss::ColorInherit);
			break;
		}
		case VcSvn:
		{
			// Svn also does a merge by default and can update our current position...
			CurrentCommit.Empty();

			LString::Array a = s.SplitDelimit("\r\n");
			for (auto &Ln: a)
			{
				if (Ln.Find("At revision") >= 0)
				{
					LString::Array p = Ln.SplitDelimit(" .");
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
				LVariant Limit;
				d->Opts.GetValue(OPT_SvnLimit, Limit);
				
				LString Args;
				if (Limit.CastInt32() > 0)
					Args.Printf("log --limit %i", Limit.CastInt32());
				else
					Args = "log";				
				
				IsLogging = StartCmd(Args, &VcFolder::ParseLog);
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

void VcFolder::MergeToLocal(LString Rev)
{
	switch (GetType())
	{
		case VcGit:
		{
			LString Args;
			Args.Printf("merge -m \"Merge with %s\" %s", Rev.Get(), Rev.Get());
			StartCmd(Args, &VcFolder::ParseMerge, NULL, LogNormal);
			break;
		}
		case VcHg:
		{
			LString Args;
			Args.Printf("merge -r %s", Rev.Get());
			StartCmd(Args, &VcFolder::ParseMerge, NULL, LogNormal);
			break;
		}
		default:
			LgiMsg(GetTree(), LLoadString(IDS_ERR_NO_IMPL_FOR_TYPE), AppName);
			break;
	}
}

bool VcFolder::ParseMerge(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		case VcHg:
			if (Result == 0)
				CommitListDirty = true;
			else
				OnCmdError(s, LLoadString(IDS_ERR_MERGE_FAILED));
			break;
		default:
			LAssert(!"Impl me.");
			break;
	}

	return true;
}

void VcFolder::Refresh()
{
	CommitListDirty = true;
	
	CurrentCommit.Empty();
	GitNames.Empty();
	Branches.DeleteObjects();
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
			LgiMsg(GetTree(), LLoadString(IDS_ERR_NO_IMPL_FOR_TYPE), AppName);
			break;
	}
}

bool VcFolder::ParseClean(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcSvn:
			if (Result == 0)
				GetCss(true)->Color(LCss::ColorInherit);
			break;
		default:
			LAssert(!"Impl me.");
			break;
	}

	return false;
}

LColour VcFolder::BranchColour(const char *Name)
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

void VcFolder::CurrentRev(std::function<void(LString)> Callback)
{
	LString Cmd;
	Cmd.Printf("id -i");
	RunCmd(Cmd, LogNormal, [Callback](auto r)
	{
		if (r.Code == 0)
			Callback(r.Out.Strip());
	});
}

bool VcFolder::RenameBranch(LString NewName, LArray<VcCommit*> &Revs)
{
	switch (GetType())
	{
		case VcHg:
		{
			// Update to the ancestor of the commits
			LHashTbl<StrKey<char>,int> Refs(0, -1);
			for (auto c: Revs)
			{
				for (auto p:*c->GetParents())
					if (Refs.Find(p) < 0)
						Refs.Add(p, 0);
				if (Refs.Find(c->GetRev()) >= 0)
					Refs.Add(c->GetRev(), 1);
			}
			LString::Array Ans;
			for (auto i:Refs)
			{
				if (i.value == 0)
					Ans.Add(i.key);
			}

			LArray<VcCommit*> Ancestors = d->GetRevs(Ans);
			if (Ans.Length() != 1)
			{
				// We should only have one ancestor
				LString s, m;
				s.Printf("Wrong number of ancestors: " LPrintfInt64 ".\n", Ans.Length());
				for (auto i: Ancestors)
				{
					m.Printf("\t%s\n", i->GetRev());
					s += m;
				}
				LgiMsg(GetTree(), s, AppName, MB_OK);
				break;
			}

			LArray<VcCommit*> Top;
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
			LString Cmd;
			Cmd.Printf("update -r " LPrintfInt64, First->GetIndex());
			RunCmd(Cmd, LogNormal, [this, &Cmd, NewName, &Top](auto r)
			{
				if (r.Code)
				{
					d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
					return;
				}

				Cmd.Printf("branch \"%s\"", NewName.Get());
				RunCmd(Cmd, LogNormal, [this, &Cmd, NewName, &Top](auto r)
				{
					if (r.Code)
					{
						d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
						return;
					}

					// Commit it to get a revision point to rebase to
					Cmd.Printf("commit -m \"Branch: %s\"", NewName.Get());
					RunCmd(Cmd, LogNormal, [this, &Cmd, NewName, &Top](auto r)
					{
						if (r.Code)
						{
							d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
							return;
						}

						CurrentRev([this, &Cmd, NewName, &Top](auto BranchNode)
						{
							// Rebase the old tree to this point
							Cmd.Printf("rebase -s %s -d %s", Top.First()->GetRev(), BranchNode.Get());
							RunCmd(Cmd, LogNormal, [this, &Cmd, NewName, Top](auto r)
							{
								if (r.Code)
								{
									d->Log->Print("Error: Cmd '%s' failed. (%s:%i)\n", Cmd.Get(), _FL);
									return;
								}

								CommitListDirty = true;
								d->Log->Print("Finished rename.\n", _FL);
							});
						});
					});
				});
			});
			break;
		}
		default:
		{
			LgiMsg(GetTree(), LLoadString(IDS_ERR_NO_IMPL_FOR_TYPE), AppName);
			break;
		}
	}

	return true;
}

bool VcFolder::ParseAddFile(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcCvs:
		{
			if (Result)
			{
				d->Tabs->Value(1);
				OnCmdError(s, LLoadString(IDS_ERR_ADD_FAILED));
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
			auto p = LString(Path).RSplit(DIR_STR, 1);
			ParseParams *params = NULL;
			if (p.Length() >= 2)
			{
				if ((params = new ParseParams))
					params->AltInitPath = p[0];
			}

			LString a;
			a.Printf("add%s \"%s\"", AsBinary ? " -kb" : "", p.Length() > 1 ? p.Last().Get() : Path);
			return StartCmd(a, &VcFolder::ParseAddFile, params);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}

	return false;
}

bool VcFolder::ParseRevert(int Result, LString s, ParseParams *Params)
{
	if (GetType() == VcSvn)
	{
		if (s.Find("Skipped ") >= 0)
			Result = 1; // Stupid svn... *sigh*
	}

	if (Result)
	{
		OnCmdError(s, LLoadString(IDS_ERR_REVERT_FAILED));
	}

	ListWorkingFolder();
	return false;
}

bool VcFolder::Revert(LString::Array &Uris, const char *Revision, bool RevertToBefore)
{
	if (Uris.Length() == 0)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			LStringPipe cmd, paths;
			LAutoPtr<ParseParams> params;
			if (Revision)
			{
				cmd.Print("checkout %s%s", Revision, RevertToBefore ? "^" : "");
			}
			else
			{
				// Unstage the file...
				cmd.Print("reset");
			}
			
			for (auto u: Uris)
			{
				auto Path = GetFilePart(u);
				paths.Print(" \"%s\"", Path.Get());
			}
			auto p = paths.NewLStr();
			cmd.Write(p);
			
			if (!Revision)
			{
				if (params.Reset(new ParseParams))
				{
					params->Callback = [this, p, RevertToBefore](auto code, auto str)
					{
						LString c;
						c.Printf("checkout %s%s", p.Get(), RevertToBefore ? "^" : "");
						StartCmd(c, &VcFolder::ParseRevert);
					};
				}
			}
			
			return StartCmd(cmd.NewLStr(), &VcFolder::ParseRevert, params.Release());
			break;
		}
		case VcHg:
		case VcSvn:
		{
			LAssert(!RevertToBefore); // Impl me!!

			LStringPipe p;
			if (Revision)
				p.Print("revert -r %s", Revision);
			else
				p.Print("revert");
			for (auto u: Uris)
			{
				auto Path = GetFilePart(u);
				p.Print(" \"%s\"", Path.Get());
			}

			auto a = p.NewLStr();			
			return StartCmd(a, &VcFolder::ParseRevert);
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}

	return false;
}

bool VcFolder::ParseResolveList(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcHg:
		{
			auto lines = s.Replace("\r").Split("\n");
			for (auto &ln: lines)
			{
				auto p = ln.Split(" ", 1);
				if (p.Length() == 2)
				{
					if (p[0].Equals("U"))
					{
						auto f = FindFile(p[1]);
						if (!f)
							f = new VcFile(d, this, LString(), true);
						f->SetText(p[0], COL_STATE);
						f->SetText(p[1], COL_FILENAME);
						f->GetStatus();
						if (f->GetList() != d->Files)
							d->Files->Insert(f);
					}
				}
			}
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}

	return true;
}

bool VcFolder::ParseResolve(int Result, LString s, ParseParams *Params)
{
	switch (GetType())
	{
		case VcGit:
		{
			break;
		}
		case VcHg:
		{
			d->Log->Print("Resolve: %s\n", s.Get());
			break;
		}
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}

	return true;
}

bool VcFolder::Resolve(const char *Path, LvcResolve Type)
{
	if (!Path)
		return false;

	switch (GetType())
	{
		case VcGit:
		{
			LString a;
			auto local = GetFilePart(Path);
			LAutoPtr<ParseParams> params(new ParseParams(Path));

			switch (Type)
			{
				case ResolveIncoming:
					a.Printf("checkout --theirs \"%s\"", local.Get());
					break;
				case ResolveLocal:
					a.Printf("checkout --ours \"%s\"", local.Get());
					break;
				case ResolveMark:
					a.Printf("add \"%s\"", local.Get());
					break;
				default:
					OnCmdError(Path, "No resolve type implemented.");
					return false;
			}

			if (Type == ResolveIncoming ||
				Type == ResolveLocal)
			{
				// Add the file after the resolution:
				params->Callback = [this, local](auto code, auto str)
				{
					LString a;
					a.Printf("add \"%s\"", local.Get());
					StartCmd(a, &VcFolder::ParseAddFile);
					Refresh();
				};
			}

			return StartCmd(a, &VcFolder::ParseResolve, params.Release());
		}
		case VcHg:
		{
			LString a;
			auto local = GetFilePart(Path);
			switch (Type)
			{
				case ResolveMark:
					a.Printf("resolve -m \"%s\"", local.Get());
					break;
				case ResolveUnmark:
					a.Printf("resolve -u \"%s\"", local.Get());
					break;
				case ResolveLocal:
					a.Printf("resolve -t internal:local \"%s\"", local.Get());
					break;
				case ResolveIncoming:
					a.Printf("resolve -t internal:other \"%s\"", local.Get());
					break;
				case ResolveTool:
				{
					#if defined(MAC)
						a.Printf("resolve -t vscode \"%s\"", local.Get());
					#elif defined(WINDOWS)
						a.Printf("resolve -t winmerge \"%s\"", local.Get());
					#else
						a.Printf("resolve -t kdiff3 \"%s\"", local.Get());
					#endif
					break;
				}
				default:
					LAssert(!"Impl me");
					break;
			}
			if (a)
				return StartCmd(a, &VcFolder::ParseResolve, new ParseParams(Path));
			break;
		}
		case VcSvn:
		case VcCvs:
		default:
		{
			NoImplementation(_FL);
			break;
		}
	}

	return false;
}

bool BlameLine::Parse(VersionCtrl type, LArray<BlameLine> &out, LString in)
{
	auto lines = in.SplitDelimit("\n", -1, false);

	switch (type)
	{
		case VcGit:
		{
			for (auto &ln: lines)
			{
				auto s = ln.Get();
				auto open = ln.Find("(");
				auto close = ln.Find(")", open);
				if (open > 0 && close > open)
				{
					auto eRef = ln(0, open-1);
					auto fields = ln(open + 1, close);
					auto parts = fields.SplitDelimit();
					
					auto &o = out.New();
					o.ref = eRef;
					o.line = parts.PopLast();

					LString::Array name;
					LDateTime dt;
					for (auto p: parts)
					{
						auto first = p(0);
						if (IsDigit(first))
						{
							if (p.Find("-") > 0)
								dt.SetDate(p);
							else if (p.Find(":") > 0)
								dt.SetTime(p);
						}
						else if (first == '+')
							dt.SetTimeZone((int)p.Int(), false);
						else
							name.Add(p);
					}
					o.user = LString(" ").Join(name);
					o.date = dt.Get();
					o.src = ln(close + 1, -1);
				}
				else if (ln.Length() > 0)
				{
					int asd=0;
				}
			}
			break;
		}
		case VcHg:
		{
			for (auto &ln: lines)
			{
				auto s = ln.Get();
				auto eUser = strchr(s, ' ');
				if (!eUser)
					continue;
				auto eRef = strchr(eUser, ':');
				if (!eRef)
					continue;
				auto &o = out.New();
				o.user.Set(s, eUser++ - s);
				o.ref.Set(eUser, eRef - eUser);
				o.src = eRef + 1;
			}
			break;
		}
		/*
		case VcSvn:
		{
			break;
		}
		*/
		default:
		{
			LAssert(0);
			return false;
		}
	}

	return true;
}

bool VcFolder::ParseBlame(int Result, LString s, ParseParams *Params)
{
	if (!Params)
	{
		LAssert(!"Need the path in the params.");
		return false;
	}

	LArray<BlameLine> lines;
	if (BlameLine::Parse(GetType(), lines, s))
	{
		if (auto ui = new BrowseUi(BrowseUi::TBlame, d, this, Params->Str))
			ui->ParseBlame(lines, s);
	}
	else NoImplementation(_FL);

	return false;
}

bool VcFolder::Blame(const char *Path)
{
	if (!Path)
		return false;

	auto file = GetFilePart(Path);
	LAutoPtr<ParseParams> Params(new ParseParams(file));

	LUri u(Path);
	switch (GetType())
	{
		case VcGit:
		{
			LString a;
			a.Printf("blame \"%s\"", file.Get());
			return StartCmd(a, &VcFolder::ParseBlame, Params.Release());
			break;
		}
		case VcHg:
		{
			LString a;
			a.Printf("annotate -un \"%s\"", file.Get());
			return StartCmd(a, &VcFolder::ParseBlame, Params.Release());
			break;
		}
		case VcSvn:
		{
			LString a;
			a.Printf("blame \"%s\"", file.Get());
			return StartCmd(a, &VcFolder::ParseBlame, Params.Release());
			break;
		}
		default:
		{
			NoImplementation(_FL);
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

bool VcFolder::ParseSaveAs(int Result, LString s, ParseParams *Params)
{
	return false;
}

bool VcFolder::ParseCounts(int Result, LString s, ParseParams *Params)
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

			LString::Array c = s.Split("\n");
			for (unsigned i=0; i<c.Length(); i++)
			{
				LString::Array a = c[i].SplitDelimit();
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
			LAssert(!"Impl me.");
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
		LTreeItem *i = d->Tree->Selection();
		VcFolder *f = dynamic_cast<VcFolder*>(i);
		if (f)
			f->ListWorkingFolder();

		if (d->Msg)
		{
			d->Msg->Name(NULL);

			auto *w = d->Msg->GetWindow();
			if (w)
			{
				w->SetCtrlEnabled(IDC_COMMIT, true);
				w->SetCtrlEnabled(IDC_COMMIT_AND_PUSH, true);
			}
		}
	}
}

void VcFolder::UncommitedItem::OnPaint(LItem::ItemPaintCtx &Ctx)
{
	LFont *f = GetList()->GetFont();
	f->Transparent(false);
	f->Colour(Ctx.Fore, Ctx.Back);
	
	LDisplayString ds(f, "(working folder)");
	ds.Draw(Ctx.pDC, Ctx.x1 + ((Ctx.X() - ds.X()) / 2), Ctx.y1 + ((Ctx.Y() - ds.Y()) / 2), &Ctx);
}

//////////////////////////////////////////////////////////////////////////////////////////
VcLeaf::VcLeaf(VcFolder *parent, LTreeItem *Item, LString uri, LString leaf, bool folder)
{
	Parent = parent;
	d = Parent->GetPriv();
	LAssert(uri.Find("://") >= 0); // Is URI
	LAssert(uri(-1) == '/'); // has trailing folder sep
	Uri.Set(uri);
	LAssert(Uri);
	Leaf = leaf;
	Folder = folder;
	Item->Insert(this);

	if (Folder)
	{
		Insert(Tmp = new LTreeItem);
		Tmp->SetText("Loading...");
	}
}

VcLeaf::~VcLeaf()
{
	for (auto l: Log)
	{
		if (!l->GetList())
			delete l;
	}
}

LString VcLeaf::Full()
{
	LUri u = Uri;
	u += Leaf;
	return u.ToString();
}

void VcLeaf::OnBrowse()
{
	auto sFull = Full();
	LUri full(sFull);

	LList *Files = d->Files;
	Files->Empty();

	if (full.IsFile())
	{
		LDirectory Dir;
		for (int b = Dir.First(full.LocalPath()); b; b = Dir.Next())
		{
			if (Dir.IsDir())
				continue;

			VcFile *f = new VcFile(d, Parent, LString(), true);
			if (f)
			{
				f->SetUri(LString("file://") + full);
				f->SetText(Dir.GetName(), COL_FILENAME);
				Files->Insert(f);
			}
		}
		Files->ResizeColumnsToContent();
		if (Folder)
			Parent->FolderStatus(full.ToString(), this);
	}
	else
	{
		// FIXME: add ssh support:
		Parent->ReadDir(this, sFull);
	}
}

void VcLeaf::AfterBrowse()
{
}

VcLeaf *VcLeaf::FindLeaf(const char *Path, bool OpenTree)
{
	if (!Stricmp(Path, Full().Get()))
		return this;

	if (OpenTree)
		DoExpand();

	VcLeaf *r = NULL;	
	for (auto n = GetChild(); !r && n; n = n->GetNext())
	{
		auto l = dynamic_cast<VcLeaf*>(n);
		if (l)
			r = l->FindLeaf(Path, OpenTree);
	}
	
	return r;
}

void VcLeaf::DoExpand()
{
	if (Tmp)
	{
		Tmp->Remove();
		DeleteObj(Tmp);
		Parent->ReadDir(this, Full());
	}
}

void VcLeaf::OnExpand(bool b)
{
	if (b)
		DoExpand();
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
	return LTreeItem::Select();
}

void VcLeaf::Select(bool b)
{
	LTreeItem::Select(b);
	if (b)
	{
		d->Commits->RemoveAll();
		OnBrowse();
		ShowLog();
	}
}

void VcLeaf::ShowLog()
{
	if (!Log.Length())
		return;

	d->Commits->RemoveAll();

	Parent->DefaultFields();
	Parent->UpdateColumns();

	for (auto i: Log)
		// We make a copy of the commit here so that the LList owns the copied object,
		// and this object still owns 'i'.
		d->Commits->Insert(new VcCommit(*i), -1, false);
	d->Commits->UpdateAllItems();
	d->Commits->ResizeColumnsToContent();
}

void VcLeaf::OnMouseClick(LMouse &m)
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
				LBrowseToFile(Full());
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

/////////////////////////////////////////////////////////////////////////////////////////
ProcessCallback::ProcessCallback(LString exe,
								LString args,
								LString localPath,
								LTextLog *log,
								LView *view,
								std::function<void(Result)> callback) :
	Log(log),
	View(view),
	Callback(callback),
	LThread("ProcessCallback.Thread"),
	LSubProcess(exe, args)
{
	SetInitFolder(localPath);

	if (Log)
		Log->Print("%s %s\n", exe.Get(), args.Get());

	Run();
}

int ProcessCallback::Main()
{
	if (!Start())
	{
		Ret.Out.Printf("Process failed with %i", GetErrorCode());
		Callback(Ret);
	}
	else
	{

		while (IsRunning())
		{
			auto Rd = Read();
			if (Rd.Length())
			{
				Ret.Out += Rd;
				if (Log)
					Log->Write(Rd.Get(), Rd.Length());
			}
		}

		auto Rd = Read();
		if (Rd.Length())
		{
			Ret.Out += Rd;
			if (Log)
				Log->Write(Rd.Get(), Rd.Length());
		}

		Ret.Code = GetExitValue();
	}

	View->PostEvent(M_HANDLE_CALLBACK, (LMessage::Param)this);
	return 0;
}

void ProcessCallback::OnComplete() // Called in the GUI thread...
{
	Callback(Ret);
}
