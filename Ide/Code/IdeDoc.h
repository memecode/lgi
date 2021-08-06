#ifndef _IDE_DOC_H_
#define _IDE_DOC_H_

#include "lgi/common/Mdi.h"
#include "lgi/common/TextView3.h"
#include "ParserCommon.h"

extern void FilterFiles(LArray<ProjectNode*> &Perfect, LArray<ProjectNode*> &Nodes, LString InputStr);

class IdeDoc : public GMdiChild, public LStream
{
	friend class DocEdit;
	class IdeDocPrivate *d;

	static LString CurIpDoc;
	static int CurIpLine;

public:
	IdeDoc(class AppWnd *a, NodeSource *src, const char *file);
	~IdeDoc();

	AppWnd *GetApp();
		
	void SetProject(IdeProject *p);	
	IdeProject *GetProject();
	const char *GetFileName();
	void SetFileName(const char *f, bool Write);
	void Focus(bool f) override;
	bool SetClean();
	void SetDirty();
	bool OnRequestClose(bool OsShuttingDown) override;
	void OnPosChange() override;
	void OnPaint(LSurface *pDC) override;
	bool IsFile(const char *File);
	bool AddBreakPoint(ssize_t Line, bool Add);
	
	bool OpenFile(const char *File);
	void SetEditorParams(int IndentSize, int TabSize, bool HardTabs, bool ShowWhiteSpace);
	bool HasFocus(int Set = -1);
	void ConvertWhiteSpace(bool ToTabs);
	void EscapeSelection(bool ToEscaped);
	void SplitSelection(LString s);
	void JoinSelection(LString s);
	void SetCrLf(bool CrLf);
	ssize_t GetLine();
	void SetLine(int Line, bool CurIp);
	static void ClearCurrentIp();
	bool IsCurrentIp();
	void GotoSearch(int CtrlId, char *InitialText = NULL);
	void SearchSymbol();
	void SearchFile();
	void UpdateControl();
	bool Build();

	// Source tools
	bool BuildIncludePaths(LArray<LString> &Paths, IdePlatform Platform, bool IncludeSysPaths);
	bool BuildHeaderList(const char16 *Cpp, LArray<char*> &Headers, LArray<LString> &IncPaths);
	bool FindDefn(char16 *Def, const char16 *Source, List<DefnInfo> &Matches);

	// Events
	void OnLineChange(int Line);
	void OnMarginClick(int Line);
	void OnProjectChange();
	
	// Impl
	void OnTitleClick(LMouse &m) override;
	LMessage::Result OnEvent(LMessage *Msg) override;
	int OnNotify(LViewI *v, LNotification n) override;
	void OnPulse() override;
	LString Read();
	ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override { return 0; }
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override;
};

#endif
