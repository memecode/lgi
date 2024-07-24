#ifndef _VcFolder_h_
#define _VcFolder_h_

#include <functional>

#include "lgi/common/SubProcess.h"
#include "lgi/common/Uri.h"

#include "SshConnection.h"

class VcLeaf;

enum LoggingType
{
	LogNone,	// No output from cmd
	LogNormal,	// Output appears as it's available
	LogSilo,	// Output appears after cmd finished (keeps it non-interleaved with other log msgs)
	LogDebug,	// Debug log level
};

enum LvcError
{
	ErrNone,
	ErrSubProcessFailed = LSUBPROCESS_ERROR,
};

enum LvcStatus
{
	StatusNone,
	StatusActive,
	StatusError,
};

enum LvcResolve
{
	ResolveNone,
	//---
	ResolveMark,
	ResolveUnmark,
	//---
	ResolveLocal,
	ResolveIncoming,
	ResolveTool,
};

class ReaderThread : public LThread
{
	VersionCtrl Vcs;
	LStream *Out;
	LAutoPtr<LSubProcess> Process;
	int FilterCount;

	int OnLine(char *s, ssize_t len);
	bool OnData(char *Buf, ssize_t &r);

public:
	int Result;

	ReaderThread(VersionCtrl vcs, LAutoPtr<LSubProcess> p, LStream *out);
	~ReaderThread();

	int Main();
};

extern int Ver2Int(LString v);
extern int ToolVersion[VcMax];
extern int LstCmp(LListItem *a, LListItem *b, int Col);
const char *GetVcName(VersionCtrl type);

struct Result
{
	int Code;
	LString Out;
};

struct ProcessCallback : public LThread, public LSubProcess
{
	Result Ret;
	LView *View = NULL;
	LTextLog *Log = NULL;
	std::function<void(Result)> Callback;

public:
	ProcessCallback(LString exe,
					LString args,
					LString localPath,
					LTextLog *log,
					LView *view,
					std::function<void(Result)> callback);
	int Main();
	void OnComplete();
};

struct VcBranch : public LString
{
	bool Default;
	LColour Colour;
	LString Hash;

	VcBranch(LString name, LString hash = LString())
	{
		Default =	name.Equals("default") ||
					name.Equals("trunk") ||
					name.Equals("main");
		
		Set(name);
		if (hash)
			Hash = hash;
	}
};

#if HAS_LIBSSH
struct SshParams
{
	SshConnection *c = NULL;
	SshConnection::LoggingType LogType = SshConnection::LogNone;
	VcFolder *f = NULL;
	LString Exe, Args, Path, Output;
	VersionCtrl Vcs = VcNone;
	ParseFn Parser = NULL;
	ParseParams *Params = NULL;
	int ExitCode = -1;

	SshParams(SshConnection *con) : c(con)
	{
	}
};
#endif

class VcFolder : public LTreeItem
{
	friend class VcCommit;

	struct Author
	{
		bool InProgress = false;
		LString name, email;
		operator bool() const { return !name.IsEmpty() && !email.IsEmpty(); }
		LString ToString() { return LString::Fmt("%s <%s>", name.Get(), email.Get()); }
	};

	class Cmd : public LStream
	{
		LString::Array Context;
		LStringPipe Buf;

	public:
		LoggingType Logging;
		LStream *Log;
		LAutoPtr<LThread> Rd;
		ParseFn PostOp;
		LAutoPtr<ParseParams> Params;
		LvcError Err;

		Cmd(LString::Array &context, LoggingType logging, LStream *log)
		{
			Context = context;
			Logging = logging;
			Log = log;
			Err = ErrNone;
		}
		
		~Cmd()
		{
		}

		LString GetBuf()
		{
			LString s = Buf.NewLStr();
			if (Log && Logging == LogSilo)
			{
				LString m;
				m.Printf("=== %s ===\n\t%s %s\n",
						Context[0].Get(),
						Context[1].Get(), Context[2].Get());
				Log->Write(m.Get(), m.Length());
				auto Lines = s.Split("\n");
				for (auto Ln : Lines)
					Log->Print("\t%s\n", Ln.Get());
			}
			return s;
		}

		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0)
		{
			ssize_t Wr = Buf.Write(Ptr, Size, Flags);

			if (Log && Logging == LogNormal)
				Log->Write(Ptr, Size, Flags);

			if (Flags)
				Err = (LvcError) Flags;

			return Wr;
		}
	};

	class UncommitedItem : public LListItem
	{
		AppPriv *d;

	public:
		UncommitedItem(AppPriv *priv)
		{
			d = priv;
		}

		void OnPaint(LItem::ItemPaintCtx &Ctx);
		void Select(bool b);
	};

	AppPriv *d;
	VersionCtrl Type = VcNone;
	LUri Uri;
	LString CurrentCommit, RepoUrl, VcCmd;
	int64 CurrentCommitIdx = -1;
	LArray<VcCommit*> Log;
	LString CurrentBranch;
	LHashTbl<ConstStrKey<char>,VcBranch*> Branches;
	LAutoPtr<UncommitedItem> Uncommit;
	LString Cache, NewRev;
	bool CommitListDirty = false;
	int Unpushed = -1, Unpulled = -1;
	LString CountCache;
	LTreeItem *Tmp = NULL;
	int CmdErrors = 0;
	LArray<CommitField> Fields;
	LArray<std::function<void()>> OnVcsTypeEvents;

	// Author name/email
	Author AuthorLocal, AuthorGlobal;
	bool IsGettingAuthor = false;

	// This is set when a blame or log is looking at a particular file, 
	// and wants it selected after the file list is populated
	LString FileToSelect;
	void InsertFiles(List<LListItem> &files);

	// Git specific
	LHashTbl<ConstStrKey<char>,LString> GitNames;
	void AddGitName(LString Hash, LString Name);
	LString GetGitNames(LString Hash);

	static int CmdMaxThreads;
	static int CmdActiveThreads;
	
	struct GitCommit
	{
		LString::Array Files;
		LString Msg, Branch;
		ParseParams *Param;
		
		GitCommit()
		{
			Param = NULL;
		}
	};
	LAutoPtr<GitCommit> PostAdd;
	void GitAdd();

	LArray<Cmd*> Cmds;
	bool IsLogging = false, IsUpdate = false, IsFilesCmd = false;
	bool IsCommit = false, IsUpdatingCounts = false;
	bool IsListingWorking = false;
	LvcStatus IsBranches = StatusNone, IsIdent = StatusNone;

	void Init(AppPriv *priv);
	const char *GetVcName();
	char GetPathSep();
	bool StartCmd(const char *Args, ParseFn Parser = NULL, ParseParams *Params = NULL, LoggingType Logging = LogNone);
	bool RunCmd(const char *Args, LoggingType Logging, std::function<void(Result)> Callback);
	void OnBranchesChange();
	void NoImplementation(const char *file, int line);
	void OnCmdError(LString Output, const char *Msg);
	void ClearError();
	VcFile *FindFile(const char *Path);
	void LinkParents();
	void CurrentRev(std::function<void(LString)> Callback);
	LColour BranchColour(const char *Name);
	void UpdateBranchUi();

	bool ParseDiffs(LString s, LString Rev, bool IsWorking);
	
	bool ParseRevList(int Result, LString s, ParseParams *Params);
	bool ParseLog(int Result, LString s, ParseParams *Params);
	bool ParseInfo(int Result, LString s, ParseParams *Params);
	bool ParseFiles(int Result, LString s, ParseParams *Params);
	bool ParseWorking(int Result, LString s, ParseParams *Params);
	bool ParseCheckout(int Result, LString s, ParseParams *Params);
	bool ParseCommit(int Result, LString s, ParseParams *Params);
	bool ParseGitAdd(int Result, LString s, ParseParams *Params);
	bool ParsePush(int Result, LString s, ParseParams *Params);
	bool ParsePull(int Result, LString s, ParseParams *Params);
	bool ParseCounts(int Result, LString s, ParseParams *Params);
	bool ParseRevert(int Result, LString s, ParseParams *Params);
	bool ParseResolveList(int Result, LString s, ParseParams *Params);
	bool ParseResolve(int Result, LString s, ParseParams *Params);
	bool ParseBlame(int Result, LString s, ParseParams *Params);
	bool ParseSaveAs(int Result, LString s, ParseParams *Params);
	bool ParseBranches(int Result, LString s, ParseParams *Params);
	bool ParseStatus(int Result, LString s, ParseParams *Params);
	bool ParseAddFile(int Result, LString s, ParseParams *Params);
	bool ParseVersion(int Result, LString s, ParseParams *Params);
	bool ParseClean(int Result, LString s, ParseParams *Params);
	bool ParseDiff(int Result, LString s, ParseParams *Params);
	bool ParseMerge(int Result, LString s, ParseParams *Params);
	bool ParseCountToTip(int Result, LString s, ParseParams *Params);
	bool ParseUpdateSubs(int Result, LString s, ParseParams *Params);
	bool ParseRemoteFind(int Result, LString s, ParseParams *Params);
	bool ParseStartBranch(int Result, LString s, ParseParams *Params);
	bool ParseSelectCommit(int Result, LString s, ParseParams *Params);
	bool ParseDelete(int Result, LString s, ParseParams *Params);
	bool ParseConflicts(int Result, LString s, ParseParams *Params);
	void DoExpand();
	
public:
	VcFolder(AppPriv *priv, const char *uri);
	VcFolder(AppPriv *priv, LXmlTag *t);
	~VcFolder();

	VersionCtrl GetType();
	AppPriv *GetPriv() { return d; }
	bool IsLocal();
	LString LocalPath();
	const char *NoPipeOpt();
	LUri GetUri();
	VcLeaf *FindLeaf(const char *Path, bool OpenTree);
	void DefaultFields();
	void UpdateColumns(LList *lst = NULL);
	int IndexOfCommitField(CommitField fld);
	const char *GetText(int Col);
	LArray<CommitField> &GetFields() { return Fields; }
	bool Serialize(LXmlTag *t, bool Write);
	LString &GetCurrentBranch();
	void SetCurrentBranch(LString name);
	LXmlTag *Save();
	LString GetConfigFile(bool local);
	bool GetAuthor(bool local, std::function<void(LString name,LString email)> callback);
	bool SetAuthor(bool local, LString name, LString email);
	void ShowAuthor();
	void UpdateAuthorUi();
	void Empty();
	void Select(bool b);
	void ListCommit(VcCommit *c);
	void ListWorkingFolder();
	void ConflistCheck();
	void FolderStatus(const char *Path = NULL, VcLeaf *Notify = NULL);
	void Commit(const char *Msg, const char *Branch, bool AndPush);
	void StartBranch(const char *BranchName, const char *OnCreated = NULL);
	void Push(bool NewBranchOk = false);
	void Pull(int AndUpdate = -1, LoggingType Logging = LogNormal);
	void Clean();
	bool Revert(LString::Array &uris, const char *Revision = NULL);
	bool Resolve(const char *Path, LvcResolve Type);
	bool AddFile(const char *Path, bool AsBinary = true);
	bool Blame(const char *Path);
	bool SaveFileAs(const char *Path, const char *Revision);
	void ReadDir(LTreeItem *Parent, const char *Uri);
	void SetEol(const char *Path, int Type);
	void GetVersion();
	void Diff(VcFile *file);
	void DiffRange(const char *FromRev, const char *ToRev);
	void MergeToLocal(LString Rev);
	bool RenameBranch(LString NewName, LArray<VcCommit*> &Revs);
	void Refresh();
	bool GetBranches(ParseParams *Params = NULL);
	void GetCurrentRevision(ParseParams *Params = NULL);
	void CountToTip();
	bool UpdateSubs(); // Clone/checkout any sub-repositories.
	void LogFilter(const char *Filter);
	void LogFile(const char *Path);
	void ClearLog();
	LString GetFilePart(const char *uri);
	void FilterCurrentFiles();
	void GetRemoteUrl(std::function<void(int32_t, LString)> Callback);
	void SelectCommit(LWindow *Parent, LString Commit, LString Path);
	void Checkout(const char *Rev, bool isBranch);
	void Delete(const char *Path, bool KeepLocal = true);

	void OnPulse();
	void OnMouseClick(LMouse &m);
	void OnRemove();
	void OnExpand(bool b);
	void OnVcsType(LString errorMsg);
	#if HAS_LIBSSH
	void OnSshCmd(SshParams *p);
	#endif
};

class VcLeaf : public LTreeItem
{
	AppPriv *d = NULL;
	VcFolder *Parent = NULL;
	bool Folder = false;
	LUri Uri;
	LString Leaf;
	LTreeItem *Tmp = NULL;

	void DoExpand();

public:
	LArray<VcCommit*> Log;

	VcLeaf(VcFolder *parent, LTreeItem *Item, LString uri, LString leaf, bool folder);
	~VcLeaf();

	LString Full();
	VcLeaf *FindLeaf(const char *Path, bool OpenTree);
	void OnBrowse();
	void AfterBrowse();
	void OnExpand(bool b);
	const char *GetText(int Col);
	int GetImage(int Flags);
	int Compare(VcLeaf *b);
	bool Select();
	void Select(bool b);
	void OnMouseClick(LMouse &m);
	void ShowLog();
};

#endif
