#pragma once

//
// Compiler specific macros:
//
//		Microsoft Visual C++: _MSC_VER
//
//		GCC: __GNUC__
//
#include "lgi/common/DocView.h"
#include "lgi/common/OptionsFile.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/List.h"
#include "lgi/common/Tree.h"
#include "lgi/common/SubProcess.h"

#include "resdefs.h"
#include "FindSymbol.h"
#include "Debugger.h"
#ifdef WIN32
#include "resource.h"
#endif

#define APP_VER					"1.0"
#define APP_URL					"http://www.memecode.com/lgi/ide/"

#define DEBUG_FIND_DEFN			0

#define OptFileSeparator		"\n"

enum IdeMessages
{
	M_APPEND_TEXT = M_USER+200, // A=(char*)heapStr, B=(int)tabIndex
	M_APPEND_STR,
	M_START_BUILD,
	M_BUILD_ERR,
	M_BUILD_DONE,
	M_DEBUG_ON_STATE,
	M_MAKEFILES_CREATED,
	M_LAST_MAKEFILE_CREATED,
	M_SELECT_TAB, // A=(int)tabIndex
	
	/// Find symbol results message:
	/// LAutoPtr<FindSymRequest> Req((FindSymRequest*)Msg->A());
	M_FIND_SYM_REQUEST,
	
	/// Send a file to the worker thread...
	/// FindSymbolSystem::SymFileParams *Params = (FindSymbolSystem::SymFileParams*)Msg->A();
	M_FIND_SYM_FILE,

	/// Send a file to the worker thread...
	/// LAutoPtr<LString::Array> Paths((LString::Array*)Msg->A());
	M_FIND_SYM_INC_PATHS,

	/// Styling is finished
	M_STYLING_DONE,
};

#define ICON_PROJECT			0
#define ICON_DEPENDANCY			1
#define ICON_FOLDER				2
#define ICON_SOURCE				3
#define ICON_HEADER				4
#define ICON_RESOURCE			5
#define ICON_GRAPHIC			6
#define ICON_WEB				7

enum IdeIcon
{
	CMD_NEW,
	CMD_OPEN,
	CMD_SAVE,
	CMD_CUT,
	CMD_COPY,
	CMD_PASTE,
	CMD_COMPILE,
	CMD_BUILD,
	CMD_STOP_BUILD,
	CMD_EXECUTE,
	CMD_DEBUG,
	CMD_PAUSE,
	CMD_KILL,
	CMD_RESTART,
	CMD_RUN_TO,
	CMD_STEP_INTO,
	CMD_STEP_OVER,
	CMD_STEP_OUT,
	CMD_FIND_IN_FILES,
	CMD_SEARCH,
	CMD_SAVE_ALL,
};

enum IdeControls
{
	IDC_STATIC = -1,
	IDC_WATCH_LIST = 700,
	IDC_LOCALS_LIST,
	IDC_DEBUG_EDIT,
	IDC_DEBUGGER_LOG,
	IDC_BUILD_LOG,
	IDC_OUTPUT_LOG,
	IDC_FIND_LOG,
	IDC_CALL_STACK,
	IDC_DEBUG_TAB,
	IDC_OBJECT_DUMP,
	IDC_MEMORY_DUMP,
	IDC_MEMORY_TABLE,
	IDC_MEM_ADDR,
	IDC_MEM_SIZE,
	IDC_MEM_ROW_LEN,
	IDC_MEM_HEX,
	IDC_REGISTERS,
	IDC_THREADS,
	IDC_EDIT,
	IDC_FILE_SEARCH,
	IDC_METHOD_SEARCH,
	IDC_SYMBOL_SEARCH,
	IDC_PROJECT_TREE,
};

enum IdeMenuCmds
{
	IDM_CONTINUE = 900
};

enum BuildConfig
{
	BuildDebug,
	BuildRelease,
	BuildMax
};
inline const char *toString(BuildConfig c)
{
	switch (c)
	{
		default:
		case BuildDebug: return "Debug";
		case BuildRelease: return "Release";
	}
	return "#err";
}

#define OPT_EditorFont			"EdFont"
#define OPT_Jobs				"Jobs"
#define OPT_SearchSysInc		"SearchSysInc"

//////////////////////////////////////////////////////////////////////
// Platform stuff
enum IdePlatform
{
	PlatformCurrent = -1,
	PlatformWin = 0,		// 0x1
	PlatformLinux,			// 0x2
	PlatformMac,			// 0x4
	PlatformHaiku,			// 0x8
	PlatformMax,
};
#define PLATFORM_WIN32			(1 << PlatformWin)
#define PLATFORM_LINUX			(1 << PlatformLinux)
#define PLATFORM_MAC			(1 << PlatformMac)
#define PLATFORM_HAIKU			(1 << PlatformHaiku)
#define PLATFORM_ALL			(PLATFORM_WIN32|PLATFORM_LINUX|PLATFORM_MAC|PLATFORM_HAIKU)
extern IdePlatform PlatformFlagsToEnum(int flags);

#if defined(_WIN32)
#define PLATFORM_CURRENT		PLATFORM_WIN32
#elif defined(MAC)
#define PLATFORM_CURRENT		PLATFORM_MAC
#elif defined(LINUX)
#define PLATFORM_CURRENT		PLATFORM_LINUX
#elif defined(HAIKU)
#define PLATFORM_CURRENT		PLATFORM_HAIKU
#endif

extern const char *PlatformNames[];
extern const char sCurrentPlatform[];
extern const char *Untitled;
extern const char SourcePatterns[];

//////////////////////////////////////////////////////////////////////
class IdeDoc;
class IdeProject;

extern const char *AppName;
extern LString FindHeader(char *Short, LArray<LString::Array*> &Paths);
extern bool BuildHeaderList(const char *Cpp, LString::Array &Headers, LArray<LString::Array*> &IncPaths, bool Recurse);

class NodeView;
class NodeSource
{
	friend class NodeView;

protected:
	NodeView *nView;

public:
	NodeSource()
	{
		nView = 0;
	}
	virtual ~NodeSource();

	virtual LString GetFullPath() = 0;
	virtual bool IsWeb() = 0;
	virtual const char *GetFileName() = 0;
	virtual const char *GetLocalCache() = 0;
	virtual const char *GetCharset() = 0;
	virtual bool Load(LDocView *Edit, NodeView *Callback) = 0;
	virtual bool Save(LDocView *Edit, NodeView *Callback) = 0;
	virtual IdeProject *GetProject() = 0;
};

class NodeView
{
	friend class NodeSource;

protected:
	NodeSource *nSrc;

public:
	NodeView(NodeSource *s)
	{
		if ((nSrc = s))
		{
			nSrc->nView = this;
		}
	}

	virtual ~NodeView();
	virtual void OnDelete() = 0;
	virtual void OnSaveComplete(bool Status) = 0;

	NodeSource *GetSrc() { return nSrc; }
};

class AppWnd : public LWindow
{
	class AppWndPrivate *d;
	friend class AppWndPrivate;

	void UpdateMemoryDump();
	void DumpHistory();

public:
	enum Channels
	{
		BuildTab,
		OutputTab,
		FindTab,
		FtpTab,
		DebugTab,
		
		ChannelMax,
	};

	enum DebugTabs
	{
		LocalsTab,
		ObjectTab,
		WatchTab,
		MemoryTab,
		ThreadsTab,
		CallStackTab,
		RegistersTab
	};
	
	LArray<class ProjectNode*> NeedsPulse;

	AppWnd();
	~AppWnd();
	
	const char *GetClass() override { return "AppWnd"; }

	int GetPlatform();
	bool SetPlatform(int p);
	bool IsClean();
	void SaveAll(std::function<void(bool)> Callback, bool CloseDirty = false);
	void CloseAll();
	IdeDoc *OpenFile(const char *FileName, NodeSource *Src = 0);
	IdeDoc *NewDocWnd(const char *FileName, NodeSource *Src);
	IdeDoc *GetCurrentDoc();
	IdeProject *OpenProject(const char *FileName, IdeProject *ParentProj, bool Create = false, bool Dep = false);
	IdeProject *RootProject();
	IdeDoc *TopDoc();
	IdeDoc *FocusDoc();
	LTextView3 *FocusEdit();
	void AppendOutput(char *Txt, Channels Channel);
	int OnFixBuildErrors();
	void OnBuildStateChanged(bool NewState);
	void UpdateState(int Debugging = -1, int Building = -1);
	void OnReceiveFiles(LArray<const char*> &Files) override;
	BuildConfig GetBuildMode();
	LTree *GetTree();
	LOptionsFile *GetOptions();
	LList *GetFtpLog();
	LStream *GetOutputLog();
	LStream *GetBuildLog();
	LStream *GetDebugLog();
	IdeDoc *FindOpenFile(char *FileName);
	IdeDoc *GotoReference(const char *File, int Line, bool CurIp, bool WithHistory = true);
	void FindSymbol(int ResultsSinkHnd, const char *Sym);
	bool GetSystemIncludePaths(LArray<LString> &Paths);
	bool ShowInProject(const char *Fn);
	bool Build();
	void SeekHistory(int Offset);
	
	// Events
	void OnLocationChange(const char *File, int Line);
	int OnCommand(int Cmd, int Event, OsView Wnd) override;
	void OnDocDestroy(IdeDoc *Doc);
	void OnProjectDestroy(IdeProject *Proj);
	void OnProjectChange();
	void OnFile(char *File, bool IsProject = false);
	bool OnRequestClose(bool IsClose) override;
	int OnNotify(LViewI *Ctrl, LNotification n) override;
	LMessage::Result OnEvent(LMessage *m) override;
	bool OnNode(const char *Path, class ProjectNode *Node, FindSymbolSystem::SymAction Action);
	void OnPulse() override;

	// Debugging support
	class LDebugContext *GetDebugContext();
	bool ToggleBreakpoint(const char *File, ssize_t Line);
	bool OnBreakPoint(LDebugger::BreakPoint &b, bool Add);
	bool LoadBreakPoints(IdeDoc *doc);
	bool LoadBreakPoints(LDebugger *db);
	void OnDebugState(bool Debugging, bool Running);
};

#include "IdeDoc.h"
#include "IdeProject.h"

extern void NewMemDumpViewer(AppWnd *App, const char *file = 0);
extern void NewProjectFromTemplate(LViewI *parent);

class SysCharSupport : public LWindow
{
	class SysCharSupportPriv *d;

public:
	SysCharSupport(AppWnd *app);
	~SysCharSupport();

	int OnNotify(LViewI *v, LNotification n);
	void OnPosChange();
};

////////////////////////////////////////////////////////////////////////
struct SysIncThread : public LThread, public LCancel
{
	AppWnd *App = NULL;
	LVariant SearchSysInc = false;

	// Output
	std::function<void(LString::Array*)> Callback;

	// Internal
	LString::Array Paths;
	LString::Array Headers;
	LHashTbl<ConstStrKey<char, true>, bool> Map;

	SysIncThread(AppWnd *app,
				IdeProject *proj,
				std::function<void(LString::Array*)> callback);
	~SysIncThread();

	void Scan(LString p);
	int Main() override;
};
