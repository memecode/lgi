#ifndef _Lvc_h_
#define _Lvc_h_

#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Box.h"
#include "lgi/common/Tree.h"
#include "lgi/common/OptionsFile.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/TabView.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Menu.h"
#include "lgi/common/Ssh.h"
#include "lgi/common/EventTargetThread.h"
#include "lgi/common/StructuredLog.h"

#define OPT_Folders			"Folders"
#define OPT_Folder			"Folder"

#define METHOD_GetContext	"GetContext"

#define APP_VERSION			"0.8"
extern const char *AppName;
extern void OpenPatchViewer(LViewI *Parent, LOptionsFile *Opts);

enum LvcIcon
{
	IcoFolder,
	IcoCleanFolder,
	IcoDirtyFolder,
	IcoFile,
	IcoCleanFile,
	IcoDirtyFile,
};

enum LvcNotify
{
	 LvcBase = LNotifyUserApp,
	 LvcCommandStart,
	 LvcCommandEnd
};

enum FileColumns
{
	COL_CHECKBOX,
	COL_STATE,
	COL_FILENAME,
};

enum AppIds
{
	IDC_TOOLS_BOX = 100,
	IDC_FOLDERS_BOX,
	IDC_COMMITS_BOX,
	IDC_FILES_BOX,
	IDC_TXT,
	IDC_LOG,
	IDC_MSG_BOX,
	IDC_TAB_VIEW,
	IDC_FOLDER_TBL, // Contains the following controls:
		IDC_TREE,
		IDC_FILTER_FOLDERS,
		IDC_CLEAR_FILTER_FOLDERS,
	IDC_COMMITS_TBL, // Contains the following controls:
		IDC_LIST,
		IDC_FILTER_COMMITS,
		IDC_CLEAR_FILTER_COMMITS,
	IDC_FILES_TBL, // Contains the following controls:
		IDC_FILES,
		IDC_FILTER_FILES,
		IDC_CLEAR_FILTER_FILES,

	IDM_ADD_LOCAL = 200,
	IDM_ADD_REMOTE,
	IDM_ADD_DIFF_FILE,
	IDM_UPDATE,
	IDM_COPY_REV,
	IDM_COPY_INDEX,
	IDM_RENAME_BRANCH,
	IDM_REMOVE,
	IDM_CLEAN,
	IDM_REVERT,
	IDM_REVERT_TO_REV,
	IDM_BLAME,
	IDM_SAVE_AS,
	IDM_BROWSE,
	IDM_LOG,
	IDM_EOL_LF,
	IDM_EOL_CRLF,
	IDM_EOL_AUTO,
	IDM_ADD_FILE,
	IDM_ADD_BINARY_FILE,
	IDM_BROWSE_FOLDER,
	IDM_TERMINAL,
	IDM_RESOLVE_MARK,
	IDM_RESOLVE_UNMARK,
	IDM_RESOLVE_LOCAL,
	IDM_RESOLVE_INCOMING,
	IDM_RESOLVE_TOOL,
	IDM_MERGE,
	IDM_LOG_FILE,
	IDM_COPY_PATH,
	IDM_COPY_LEAF,
	IDM_EDIT,
	IDM_REMOTE_URL,
};

enum AppMessages
{
	M_DETECT_VCS = M_USER + 100,
	M_RUN_CMD,
	M_RESPONSE,
	M_HANDLE_CALLBACK,
};

enum VersionCtrl
{
	VcNone,
	
	VcCvs,
	VcSvn,
	VcGit,
	VcHg,
	
	VcPending,
	VcError,
	VcMax,
};

class VcFolder;
struct ParseParams
{
	LString Str;
	LString AltInitPath;
	class VcLeaf *Leaf = NULL;
	bool IsWorking = false;;
	bool Debug = false;
	std::function<void(LString)> Callback;

	ParseParams(const char *str = NULL)
	{
		Str = str;
	}
};

typedef bool (VcFolder::*ParseFn)(int, LString, ParseParams*);
class SshConnection;

struct AppPriv
{
	VcFolder		*CurFolder	= NULL;
	LTree			*Tree		= NULL;
	LList			*Commits	= NULL;
	LList			*Files		= NULL;
	LEdit			*Msg		= NULL;
	LTextLog		*Diff		= NULL;
	LTextLog		*Log		= NULL;
	LTabView		*Tabs		= NULL;
	VersionCtrl		PrevType	= VcNone;
	LOptionsFile	Opts;
	LStructuredLog	sLog;
	int				Resort = -1;

	// Filtering
	LString			FolderFilter, CommitFilter, FileFilter;

	LHashTbl<StrKey<char,false>,SshConnection*> Connections;
	
	AppPriv() :
		Opts(LOptionsFile::DesktopMode, AppName),
		sLog("Lvc.slog")
	{		
	}	
	~AppPriv();

	SshConnection *GetConnection(const char *Uri, bool Create = true);
	auto Wnd() { return Commits ? Commits->GetWindow() : LAppInst->AppWnd; }
	
	void ClearFiles()
	{
		if (Files)
			Files->Empty();
		if (Diff)
			Diff->Name(NULL);		
	}

	LArray<class VcCommit*> GetRevs(LString::Array &Revs);
	LString::Array GetCommitRange();
	
	bool IsMenuChecked(int Item)
	{
		LMenu *m = Tree->GetWindow()->GetMenu();
		LMenuItem *i = m ? m->FindItem(Item) : NULL;
		return i ? i->Checked() : false;
	}

	VersionCtrl DetectVcs(VcFolder *Fld);
	class VcFile *FindFile(const char *Path);
};

#include "SshConnection.h"

class BlameUi : public LWindow
{
	struct BlameUiPriv *d;

public:
	BlameUi(AppPriv *priv, VersionCtrl Vc, LString Output);
	~BlameUi();
};

class DropDownBtn : public LDropDown, public ResObject
{
	struct DropDownBtnPriv *d;
	class DropLst *Pu;

public:
	DropDownBtn();
	~DropDownBtn();

	LString::Array GetList();
	bool SetList(int EditCtrl, LString::Array a);
	bool OnLayout(LViewLayoutInfo &Inf);
};

extern bool ConvertEol(const char *Path, bool Cr);
extern int GetEol(const char *Path);
extern LString::Array GetProgramsInPath(const char *Program);
extern LColour GetPaletteColour(int i);

#include "VcFile.h"
#include "VcCommit.h"
#include "VcFolder.h"

#endif
