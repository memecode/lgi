#ifndef _IDE_PROJECT_H_
#define _IDE_PROJECT_H_

#include "lgi/common/XmlTree.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Tree.h"
#include "lgi/common/OptionsFile.h"
#include "GDebugger.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/List.h"

#define NODE_DROP_FORMAT			"com.memecode.ProjectNode"

#define OPT_Ftp						"ftp"
#define OPT_Www						"www"

enum ExeAction
{
	ExeRun,
	ExeDebug,
	ExeValgrind
};

enum AppCommands
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
	IDM_INSERT_FTP,
	IDM_SHOW_IN_PROJECT,

	// IDM_BUILD,
	IDM_CLEAN_PROJECT,
	IDM_CLEAN_ALL,
	IDM_REBUILD_PROJECT,
	IDM_REBUILD_ALL
};

enum ProjectStatus
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

	int64 Value();
	void Value(int64 val);
	int OnNotify(LViewI *c, LNotification n);
};

extern IdePlatform GetCurrentPlatform();

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
	bool OnOpen(LProgressDlg *Prog, LXmlTag *Src);	
	void CollectAllSubProjects(List<IdeProject> &c);
	void CollectAllSource(LArray<LString> &c, IdePlatform Platform);
	void SortChildren();
	void InsertTag(LXmlTag *t) override;
	bool RemoveTag() override;
	virtual bool IsWeb() = 0;	
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
#include "GDebugContext.h"

class IdeProject : public LXmlFactory, public IdeCommon
{
	friend class ProjectNode;
	friend class BuildThread;
	class IdeProjectPrivate *d;

	bool OnNode(const char *Path, class ProjectNode *Node, bool Add);

public:
	IdeProject(AppWnd *App);
	~IdeProject();

	bool IsWeb() { return false; }
	int GetPlatforms();

	const char *GetFileName(); // Can be a relative path
	LAutoString GetFullPath(); // Always a complete path
	LAutoString GetBasePath(); // A non-relative path to the folder containing the project

	AppWnd *GetApp();
	LString GetExecutable(IdePlatform Platform);
	const char *GetExeArgs();
	const char *GetIncludePaths();
	const char *GetPreDefinedValues();

	LXmlTag *Create(char *Tag);
	void Empty();
	LString GetMakefile(IdePlatform Platform);
	bool GetExePath(char *Path, int Len);
	bool RelativePath(LString &Out, const char *In, bool Debug = false);
	void Build(bool All, bool Release);
	void StopBuild();
	void Clean(bool All, bool Release);
	GDebugContext *Execute(ExeAction Act = ExeRun);
	bool FixMissingFiles();
	bool FindDuplicateSymbols();
	bool InProject(bool FuzzyMatch, const char *Path, bool Open, class IdeDoc **Doc = 0);
	const char *GetFileComment();
	const char *GetFunctionComment();
	bool IsMakefileUpToDate();
	bool IsMakefileAScript();
	bool CreateMakefile(IdePlatform Platform, bool BuildAfterwards);
	LString GetTargetName(IdePlatform Platform);
	LString GetTargetFile(IdePlatform Platform);
	bool BuildIncludePaths(LArray<LString> &Paths, bool Recurse, bool IncludeSystem, IdePlatform Platform);
	void ShowFileProperties(const char *File);
	bool GetExpanded(int Id);
	void SetExpanded(int Id, bool Exp);
	int AllocateId();
	bool CheckExists(LString &p, bool Debug = false);
	bool CheckExists(LAutoString &p, bool Debug = false);
	void OnMakefileCreated();
	
	// Nodes
	char *FindFullPath(const char *File, class ProjectNode **Node = NULL);
	bool GetAllNodes(LArray<ProjectNode*> &Nodes);
	bool HasNode(ProjectNode *Node);

	// Project heirarchy
	IdeProject *GetParentProject();
	bool GetChildProjects(List<IdeProject> &c);
	void SetParentProject(IdeProject *p);

	// File
	void CreateProject();
	ProjectStatus OpenFile(const char *FileName);
	bool SaveFile();
	void SetClean(std::function<void(bool)> OnDone);
	void SetDirty();
	void SetUserFileDirty();
	bool Serialize(bool Write);
	void ImportDsp(const char *File);

	// Dependency calculation
	bool GetAllDependencies(LArray<char*> &Files, IdePlatform Platform);
	bool GetDependencies(const char *SourceFile, LArray<LString> &IncPaths, LArray<char*> &Files, IdePlatform Platform);
	
	// Settings
	IdeProjectSettings *GetSettings();

	// Impl
	const char *GetText(int Col);
	int GetImage(int Flags);
	void OnMouseClick(LMouse &m);
};

class IdeTree : public LTree, public LDragDropTarget
{
	LTreeItem *Hit;

public:
	IdeTree();
	
	void OnCreate();
	void OnDragExit();
	int WillAccept(LDragFormats &Formats, LPoint p, int KeyState);
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState);
};

extern const char TagSettings[];
extern void FixMissingFilesDlg(IdeProject *Proj);

#endif
