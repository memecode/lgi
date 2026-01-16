#ifndef _IDE_PROJECT_H_
#define _IDE_PROJECT_H_

#include "lgi/common/XmlTree.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Tree.h"
#include "lgi/common/OptionsFile.h"
#include "Debugger.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/List.h"

#if MAC
#define NODE_DROP_FORMAT			"com.memecode.ProjectNode"
#else
#define NODE_DROP_FORMAT			"application/x-lgiide-projectNode"
#endif

#define OPT_Ftp						"ftp"
#define OPT_Www						"www"
#define OPT_NodeFlags				"NodeFlags"
#define OPT_Breakpoint				"Breakpoint"

enum TExeAction
{
	ExeRun,
	ExeDebug,
	ExeValgrind
};

enum TAppCommands
{
	IDM_INSERT = 100,
	IDM_NEW_FOLDER,
	IDM_RENAME,
	IDM_DELETE,
	IDM_SETTINGS,
	IDM_INSERT_DEP,
	IDM_SORT_CHILDREN,
	IDM_PROPERTIES,
	IDM_BROWSE_FOLDER,
	IDM_OPEN_TERM,
	IDM_IMPORT_FOLDER,
	IDM_WEB_FOLDER,
	IDM_SHOW_IN_PROJECT,

	// IDM_BUILD,
	IDM_CLEAN_PROJECT,
	IDM_CLEAN_ALL,
	IDM_REBUILD_PROJECT,
	IDM_REBUILD_ALL
};

enum TProjectStatus
{
	OpenError,
	OpenOk,
	OpenCancel,
};

extern int PlatformCtrlId[];

class AddFilesProgress : public LDialog
{
	uint64 Ts;
	uint64 v;
	LView *Msg;

public:
	bool Cancel;
	static const char *DefaultExt;
	LHashTbl<ConstStrKey<char,false>, bool> Exts;

	AddFilesProgress(LViewI *par);

	int64 Value() override;
	void Value(int64 val) override;
	int OnNotify(LViewI *c, const LNotification &n) override;
};

extern SysPlatform GetCurrentPlatform();

class AppWnd;
class IdeProject;
class IdeCommon : public LTreeItem, public LXmlTag
{
	friend class IdeProject;

protected:
	IdeProject *Project;

public:
	IdeCommon(IdeProject *p);
	~IdeCommon();

	IdeProject *GetProject() { return Project; }
	LStream *GetLog();
	bool OnOpen(LProgressDlg *Prog, LXmlTag *Src);	
	void CollectAllSubProjects(LArray<IdeProject*> &c);
	void CollectAllSource(LArray<LString> &c, SysPlatform Platform);
	void SortChildren();
	void InsertTag(LXmlTag *t) override;
	bool RemoveTag() override;
	virtual int GetPlatforms() = 0;
	bool AddFiles(AddFilesProgress *Prog, const char *Path);
	IdeCommon *GetSubFolder(IdeProject *Project, char *Name, bool Create = false);
};

class WatchItem : public LTreeItem
{
	class IdeOutput *Out;
	LTreeItem *PlaceHolder;

public:
	WatchItem(IdeOutput *out, const char *Init = NULL);
	~WatchItem();
	
	bool SetText(const char *s, int i = 0) override;
	void OnExpand(bool b) override;
	bool SetValue(LVariant &v);
};

#include "IdeProjectSettings.h"
#include "DebugContext.h"

class IdeProject : public LXmlFactory, public IdeCommon
{
	friend class ProjectNode;
	friend class BuildThread;
	class IdeProjectPrivate *d;
	int bpStoreCb = 0;

	bool OnNode(const char *Path, class ProjectNode *Node, bool Add);

public:
	IdeProject(AppWnd *App, ProjectNode *DepParent = NULL);
	~IdeProject();

	int GetPlatforms();

	const char *GetFileName(); // Can be a relative path
	LAutoString GetFullPath(); // Always a complete path
	LAutoString GetBasePath(); // A non-relative path to the folder containing the project

	AppWnd *GetApp();
	LString GetExecutable(SysPlatform Platform);
	const char *GetExeArgs();
	const char *GetIncludePaths();
	const char *GetPreDefinedValues();
	const char *GetCompileOptions();

	LXmlTag *Create(char *Tag);
	void Empty();
	LString GetMakefile(SysPlatform Platform);
	[[deprecated]] bool GetExePath(char *Path, int Len);
	void GetExePath(std::function<void(LString,SysPlatform)> cb);
	bool RelativePath(LString &Out, const char *In, bool Debug = false);
	void Build(bool All, BuildConfig Config);
	void BuildForPlatform(bool All, BuildConfig Config, SysPlatform Platform);
	void StopBuild();
	void BuildThreadFinished();
	void Clean(bool All, BuildConfig Config);
	void CleanForPlatform(bool All, BuildConfig Config, SysPlatform Platform);
	void Execute(TExeAction Act = ExeRun, std::function<void(LError&, LDebugContext*)> contextCb = NULL);
	bool FixMissingFiles();
	bool FindDuplicateSymbols();
	bool InProject(bool FuzzyMatch, const char *Path, bool Open, class IdeDoc **Doc = 0);
	const char *GetFileComment();
	const char *GetFunctionComment();
	bool IsMakefileUpToDate();
	bool IsMakefileAScript();
	bool CreateMakefile(SysPlatform Platform, bool BuildAfterwards);
	LString GetTargetName(SysPlatform Platform);
	LString GetTargetFile(SysPlatform Platform);
	bool BuildIncludePaths(LString::Array &Paths, LString::Array *SysPaths, bool Recurse, bool IncludeSystem, SysPlatform Platform);
	void ShowFileProperties(const char *File);
	int AllocateId();
	bool CheckExists(LString &p, bool Debug = false);
	bool CheckExists(LAutoString &p, bool Debug = false);
	void OnMakefileCreated();
	class SystemIntf *GetBackend();
	LString GetBuildFolder() const;
	void SetBuildFolder(LString folder);
	void Refresh();

	// User file settings
	bool GetExpanded(int Id);
	void SetExpanded(int Id, bool Exp);
	void AddBreakpoint(BreakPoint &bp);
	bool DeleteBreakpoint(int id);
	bool HasBreakpoint(int id);
	bool LoadBreakPoints(IdeDoc *doc);
	
	// Nodes
	char *FindFullPath(const char *File, class ProjectNode **Node = NULL);
	bool GetAllNodes(LArray<ProjectNode*> &Nodes);
	bool HasNode(ProjectNode *Node);

	// Project hierarchy
	IdeProject *GetParentProject();
	LArray<IdeProject*> GetAllProjects();
	bool GetChildProjects(LArray<IdeProject*> &c);
	void SetParentProject(IdeProject *p);

	// File
	void CreateProject();
	TProjectStatus OpenFile(const char *FileName);
	bool SaveFile();
	bool GetClean();
	void SetClean(std::function<void(bool)> OnDone);
	void SetDirty();
	void SetUserFileDirty();
	bool Serialize(bool Write);
	void ImportDsp(const char *File);

	// Dependency calculation
	bool GetAllDependencies(LArray<char*> &Files, SysPlatform Platform);
	bool GetDependencies(const char *SourceFile, LString::Array &IncPaths, LArray<char*> &Files, SysPlatform Platform);
	
	// Settings
	IdeProjectSettings *GetSettings();
	void EditSettings(int platformFlags);

	// Impl
	const char *GetText(int Col);
	int GetImage(int Flags);
	void OnMouseClick(LMouse &m);
	void OnBackendReady();
};

class IdeTree : public LTree
{
	LTreeItem *Hit = NULL;

public:
	const char *GetClass() override { return "IdeTree"; }

	IdeTree();
	
	void OnCreate() override;
	void OnDragExit() override;
	int WillAccept(LDragFormats &Formats, LPoint p, int KeyState) override;
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState) override;
};

extern const char TagSettings[];
extern void FixMissingFilesDlg(IdeProject *Proj);

#endif
