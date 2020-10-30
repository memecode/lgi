#ifndef _Lvc_h_
#define _Lvc_h_

#include "Lgi.h"
#include "LList.h"
#include "GBox.h"
#include "GTree.h"
#include "GOptionsFile.h"
#include "GTextView3.h"
#include "GTextLog.h"
#include "GTabView.h"
#include "GEdit.h"
#include "LMenu.h"
#include "LSsh.h"
#include "GEventTargetThread.h"

#define OPT_Folders			"Folders"
#define OPT_Folder			"Folder"

#define METHOD_GetContext	"GetContext"

#define APP_VERSION		"0.7"
extern const char *AppName;

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
	 LvcBase = GNotifyUserApp,
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
	IDC_LIST = 100,
	IDC_TOOLS_BOX,
	IDC_FOLDERS_BOX,
	IDC_COMMITS_BOX,
	IDC_TREE,
	IDC_FILES,
	IDC_FILES_BOX,
	IDC_TXT,
	IDC_LOG,
	IDC_MSG_BOX,
	IDC_TAB_VIEW,

	IDM_ADD_LOCAL = 200,
	IDM_ADD_REMOTE,
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
	IDM_RESOLVE,
	IDM_MERGE,
	IDM_LOG_FILE,
	IDM_COPY_PATH,
	IDM_COPY_LEAF,
	IDM_EDIT,
};

enum VersionCtrl
{
	VcNone,
	
	VcCvs,
	VcSvn,
	VcGit,
	VcHg,
	
	VcPending,
	VcMax,
};

class VcFolder;
struct ParseParams
{
	GString Str;
	GString AltInitPath;
	class VcLeaf *Leaf;
	bool IsWorking;

	ParseParams(const char *str = NULL)
	{
		Str = str;
		Leaf = NULL;
		IsWorking = false;
	}
};

typedef bool (VcFolder::*ParseFn)(int, GString, ParseParams*);
class SshConnection;

struct AppPriv
{
	GTree *Tree;
	VcFolder *CurFolder;
	LList *Commits;
	LList *Files;
	GOptionsFile Opts;
	GEdit *Msg;
	GTextLog *Diff;
	GTextLog *Log;
	GTabView *Tabs;
	VersionCtrl PrevType;
	int Resort;

	LHashTbl<StrKey<char,false>,SshConnection*> Connections;
	
	AppPriv()  : Opts(GOptionsFile::DesktopMode, AppName)
	{
		Commits = NULL;
		PrevType = VcNone;
		Tree = NULL;
		Files = NULL;
		Diff = NULL;
		Log = NULL;
		Msg = NULL;
		Tabs = NULL;
		CurFolder = NULL;
		Resort = -1;
	}

	SshConnection *GetConnection(const char *Uri, bool Create = true);
	
	void ClearFiles()
	{
		if (Files)
			Files->Empty();
		if (Diff)
			Diff->Name(NULL);		
	}

	GArray<class VcCommit*> GetRevs(GString::Array &Revs);
	GString::Array GetCommitRange();
	
	bool IsMenuChecked(int Item)
	{
		LMenu *m = Tree->GetWindow()->GetMenu();
		LMenuItem *i = m ? m->FindItem(Item) : NULL;
		return i ? i->Checked() : false;
	}

	VersionCtrl DetectVcs(VcFolder *Fld);
};

class SshConnection : public LSsh, public GEventTargetThread
{
	int GuiHnd;
	GUri Host;
	GAutoPtr<GStream> c;
	GString Uri, Prompt;
	AppPriv *d;

	GMessage::Result OnEvent(GMessage *Msg);
	GStream *GetConsole();
	bool WaitPrompt(GStream *c, GString *Data = NULL);

public:
	LHashTbl<StrKey<char,false>,VersionCtrl> Types;
	GArray<VcFolder*> TypeNotify;
	
	SshConnection(GTextLog *log, const char *uri, const char *prompt);
	bool DetectVcs(VcFolder *Fld);
	bool Command(VcFolder *Fld, GString Exe, GString Args, ParseFn Parser, ParseParams *Params);
	
	// This is the GUI thread message handler
	static bool HandleMsg(GMessage *m);
};

class BlameUi : public GWindow
{
	struct BlameUiPriv *d;

public:
	BlameUi(AppPriv *priv, VersionCtrl Vc, GString Output);
	~BlameUi();
};

class DropDownBtn : public GDropDown, public ResObject
{
	struct DropDownBtnPriv *d;
	class DropLst *Pu;

public:
	DropDownBtn();
	~DropDownBtn();

	GString::Array GetList();
	bool SetList(int EditCtrl, GString::Array a);
	bool OnLayout(GViewLayoutInfo &Inf);
};

extern bool ConvertEol(const char *Path, bool Cr);
extern int GetEol(const char *Path);
extern GString::Array GetProgramsInPath(const char *Program);
extern GColour GetPaletteColour(int i);

#include "VcFile.h"
#include "VcCommit.h"
#include "VcFolder.h"

#endif
