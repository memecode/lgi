#ifndef _VcFolder_h_
#define _VcFolder_h_

#include "lgi/common/SubProcess.h"

class VcLeaf;

enum LoggingType
{
	LogNone,	// No output from cmd
	LogNormal,	// Output appears as it's available
	LogSilo,	// Output appears after cmd finished (keeps it non-interleaved with other log msgs)
};

enum LvcError
{
	ErrNone,
	ErrSubProcessFailed = GSUBPROCESS_ERROR,
};

enum LvcStatus
{
	StatusNone,
	StatusActive,
	StatusError,
};

class ReaderThread : public LThread
{
	VersionCtrl Vcs;
	LStream *Out;
	LSubProcess *Process;
	int FilterCount;

	int OnLine(char *s, ssize_t len);
	bool OnData(char *Buf, ssize_t &r);

public:
	int Result;

	ReaderThread(VersionCtrl vcs, LSubProcess *p, LStream *out);
	~ReaderThread();

	int Main();
};

extern int Ver2Int(LString v);
extern int ToolVersion[VcMax];
extern int LstCmp(LListItem *a, LListItem *b, int Col);

struct Result
{
	int Code;
	LString Out;
};

struct VcBranch
{
	bool Default;
	LColour Colour;

	VcBranch(const char *n)
	{
		Default =	!stricmp(n, "default") ||
					!stricmp(n, "trunk");
	}
};

struct SshParams
{
	SshConnection *c;
	VcFolder *f;
	LString Exe, Args, Path, Output;
	VersionCtrl Vcs;
	ParseFn Parser;
	ParseParams *Params;
	int ExitCode;

	SshParams(SshConnection *con) : c(con)
	{
		f = NULL;
		Parser = NULL;
		Params = NULL;
		Vcs = VcNone;
		ExitCode = -1;
	}
};

class VcFolder : public LTreeItem
{
	friend class VcCommit;

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
			LString s = Buf.NewGStr();
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

		void OnPaint(GItem::ItemPaintCtx &Ctx);
		void Select(bool b);
	};

	AppPriv *d;
	VersionCtrl Type;
	LUri Uri;
	LString CurrentCommit, RepoUrl, VcCmd;
	int64 CurrentCommitIdx;
	LArray<VcCommit*> Log;
	LString CurrentBranch;
	LHashTbl<ConstStrKey<char>,VcBranch*> Branches;
	LAutoPtr<UncommitedItem> Uncommit;
	LString Cache, NewRev;
	bool CommitListDirty;
	int Unpushed, Unpulled;
	LString CountCache;
	LTreeItem *Tmp;
	int CmdErrors;
	LArray<CommitField> Fields;
	
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
	bool IsLogging, IsUpdate, IsFilesCmd, IsWorkingFld, IsCommit, IsUpdatingCounts;
	LvcStatus IsBranches, IsIdent;

	void Init(AppPriv *priv);
	const char *GetVcName();
	bool StartCmd(const char *Args, ParseFn Parser = NULL, ParseParams *Params = NULL, LoggingType Logging = LogNone);
	Result RunCmd(const char *Args, LoggingType Logging = LogNone);
	void OnBranchesChange();
	void OnCmdError(LString Output, const char *Msg);
	void ClearError();
	VcFile *FindFile(const char *Path);
	void LinkParents();
	LString CurrentRev();
	LColour BranchColour(const char *Name);
	void Empty();

	bool ParseDiffs(LString s, LString Rev, bool IsWorking);
	
	bool ParseRevList(int Result, LString s, ParseParams *Params);
	bool ParseLog(int Result, LString s, ParseParams *Params);
	bool ParseInfo(int Result, LString s, ParseParams *Params);
	bool ParseFiles(int Result, LString s, ParseParams *Params);
	bool ParseWorking(int Result, LString s, ParseParams *Params);
	bool ParseUpdate(int Result, LString s, ParseParams *Params);
	bool ParseCommit(int Result, LString s, ParseParams *Params);
	bool ParseGitAdd(int Result, LString s, ParseParams *Params);
	bool ParsePush(int Result, LString s, ParseParams *Params);
	bool ParsePull(int Result, LString s, ParseParams *Params);
	bool ParseCounts(int Result, LString s, ParseParams *Params);
	bool ParseRevert(int Result, LString s, ParseParams *Params);
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
	void DoExpand();
	
public:
	VcFolder(AppPriv *priv, const char *uri);
	VcFolder(AppPriv *priv, LXmlTag *t);
	~VcFolder();

	VersionCtrl GetType();
	AppPriv *GetPriv() { return d; }
	const char *LocalPath();
	LUri GetUri() { return Uri; }
	VcLeaf *FindLeaf(const char *Path, bool OpenTree);
	void DefaultFields();
	void UpdateColumns();
	const char *GetText(int Col);
	LArray<CommitField> &GetFields() { return Fields; }
	bool Serialize(LXmlTag *t, bool Write);
	LXmlTag *Save();
	void Select(bool b);
	void ListCommit(VcCommit *c);
	void ListWorkingFolder();
	void FolderStatus(const char *Path = NULL, VcLeaf *Notify = NULL);
	void Commit(const char *Msg, const char *Branch, bool AndPush);
	void StartBranch(const char *BranchName, const char *OnCreated = NULL);
	void Push(bool NewBranchOk = false);
	void Pull(int AndUpdate = -1, LoggingType Logging = LogNormal);
	void Clean();
	bool Revert(LString::Array &uris, const char *Revision = NULL);
	bool Resolve(const char *Path);
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
	bool UpdateSubs(); // Clone/checkout any sub-repositries.
	void LogFile(const char *Path);
	LString GetFilePart(const char *uri);

	void OnPulse();
	void OnUpdate(const char *Rev);
	void OnMouseClick(LMouse &m);
	void OnRemove();
	void OnExpand(bool b);
	void OnVcsType();
	void OnSshCmd(SshParams *p);
};

class VcLeaf : public LTreeItem
{
	AppPriv *d;
	VcFolder *Parent;
	bool Folder;
	LUri Uri;
	LString Leaf;
	LTreeItem *Tmp;

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
