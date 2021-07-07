#ifndef _PROJECT_NODE_H_
#define _PROJECT_NODE_H_

#include "FtpThread.h"

// Linked to 'TypeNames' global variable, update that too.
enum NodeType
{
	NodeNone,
	NodeDir,
	NodeSrc,
	NodeHeader,
	NodeDependancy,
	NodeResources,
	NodeGraphic,
	NodeWeb,
	NodeText,
	NodeMakeFile,

	NodeMax, // Always last
};

extern int NodeSort(LTreeItem *a, LTreeItem *b, NativeInt d);
extern DeclGArrayCompare(XmlSort, LXmlTag*, NativeInt);

class ProjectNode : public IdeCommon, public LDragDropSource, public FtpCallback, public NodeSource
{
	NodeType Type;
	int NodeId;
	int Platforms;
	LString sFile;
	LString sLocalCache;
	LString sName;
	LString Charset;
	IdeProject *Dep;
	bool IgnoreExpand;
	int64 ChildCount;
	LString Label;

	void OpenLocalCache(IdeDoc *&Doc);
	void OnCmdComplete(FtpCmd *Cmd) override;
	int64 CountNodes();

public:
	ProjectNode(IdeProject *p);
	~ProjectNode();

	// Actions
	IdeDoc *Open();
	void Delete();
	void SetClean();
	void AddNodes(LArray<ProjectNode*> &Nodes);
	bool HasNode(ProjectNode *Node);
	
	// Props
	int GetId();
	bool IsWeb() override;
	IdeProject *GetDep();
	IdeProject *GetProject() override;
	const char *GetFileName() override;
	void SetFileName(const char *f);
	char *GetName();
	void SetName(const char *f);
	NodeType GetType();
	void SetType(NodeType t);
	int GetImage(int f) override;
	const char *GetText(int c) override;
	LString GetFullPath() override;
	ProjectNode *FindFile(const char *In, char **Full);
	/// \sa Some combination of PLATFORM_WIN32, PLATFORM_LINUX, PLATFORM_MAC, PLATFORM_HAIKU or PLATFORM_ALL
	int GetPlatforms() override;
	const char *GetLocalCache() override;
	void AutoDetectType();
	const char *GetCharset() override { return Charset; }
	
	// Dnd
	bool GetFormats(LDragFormats &Formats) override;
	bool GetData(LArray<LDragData> &Data) override;
	
	// Ui events
	bool OnBeginDrag(LMouse &m) override;
	bool OnKey(LKey &k) override;
	void OnExpand(bool b) override;
	void OnMouseClick(LMouse &m) override;
	void OnProperties();

	// Serialization
	bool Load(GDocView *Edit, NodeView *Callback) override;
	bool Save(GDocView *Edit, NodeView *Callback) override;
	bool Serialize(bool Write) override;
};

#endif
