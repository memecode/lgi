#ifndef _IdeDocPrivate_h_
#define _IdeDocPrivate_h_

#include "lgi/common/CheckBox.h"

#define EDIT_CTRL_WIDTH		200

class EditTray : public LLayout
{
	LRect FileBtn;
	LEdit *FileSearch;

	LRect FuncBtn;
	LEdit *FuncSearch;

	LRect SymBtn;
	LEdit *SymSearch;

	LCheckBox *AllPlatforms;

	LRect TextMsg;

	LTextView3 *Ctrl;
	IdeDoc *Doc;

public:
	int Line, Col;

	EditTray(LTextView3 *ctrl, IdeDoc *doc);
	~EditTray();
	
	const char *GetClass() override { return "EditTray"; }
	void GotoSearch(int CtrlId, char *InitialText = NULL);	
	void OnCreate() override;
	void OnPosChange() override;
	void OnPaint(LSurface *pDC) override;
	bool Pour(LRegion &r) override;
	void OnMouseClick(LMouse &m) override;
	void OnHeaderList(LMouse &m);
	void OnFunctionList(LMouse &m);
	void OnSymbolList(LMouse &m);
};

class IdeDocPrivate : public NodeView, public LMutex
{
	LString FileName;
	LString Buffer;

public:
	IdeDoc *Doc;
	AppWnd *App;
	IdeProject *Project;
	LDateTime ModTs;
	class DocEdit *Edit;
	EditTray *Tray;
	LHashTbl<IntKey<ssize_t>, bool> BreakPoints;
	class ProjFilePopup *FilePopup;
	class ProjMethodPopup *MethodPopup;
	class ProjSymPopup *SymPopup;
	LString::Array WriteBuf;
	LAutoPtr<LThread> Build;
	
	IdeDocPrivate(IdeDoc *d, AppWnd *a, NodeSource *src, const char *file);
	void OnDelete();
	void UpdateName();
	LString GetDisplayName();
	bool IsFile(const char *File);
	const char *GetLocalFile();
	void SetFileName(const char *f);
	bool Load();
	bool Save();
	void OnSaveComplete(bool Status);

	LDateTime GetModTime()
	{
		LDateTime Ts;

		auto Full = nSrc ? nSrc->GetFullPath() : FileName;
		if (Full)
		{
			LDirectory Dir;
			if (Dir.First(Full, NULL))
				Ts.Set(Dir.GetLastWriteTime());
		}

		return Ts;
	}

	void CheckModTime();
};

#endif
