/*
**	FILE:			LgiRes_String.h
**	AUTHOR:			Matthew Allen
**	DATE:			10/9/1999
**	DESCRIPTION:	Dialog Resource Editor
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#pragma once

#include "lgi/common/Res.h"
#include "lgi/common/List.h"
#include "lgi/common/Combo.h"
#include "lgi/common/Box.h"

////////////////////////////////////////////////////////////////
class ResString;
class ResStringGroup;
class ResStringUi;
class ResDialogCtrl;

////////////////////////////////////////////////////////////////
class StrLang
{
	LLanguageId Lang;
	char *Str;

public:
	StrLang();
	~StrLang();

	LLanguageId GetLang();
	void SetLang(LLanguageId i);
	char *&GetStr();
	void SetStr(const char *s);
	bool IsEnglish();

	bool operator ==(LLanguageId LangId);
	bool operator !=(LLanguageId LangId);
};

class ResString :
	public LListItem,
	public FieldSource,
	public ResourceItem
{
	friend class ResStringGroup;

protected:
	ResStringGroup *Group = NULL;

	char RefStr[16];
	char IdStr[16];
	char **GetIndex(int i);
	StrLang *GetLang(LLanguageId i);
	LString Define;			// #define used in code to reference
	int Ref = 0;			// globally unique
	int Id = 0;				// numerical value used in code to reference

public:
	char *Tag = NULL;			// Optional component tag, for turning off features.
	List<StrLang> Items;

	ResString(ResStringGroup *group, int init_ref = -1);
	~ResString();
	ResString &operator =(ResString &s);
	ResStringGroup *GetGroup() { return Group; }

	// Data
	int GetRef() { return Ref; }
	int SetRef(int r);
	int GetId() { return Id; }
	int SetId(int id);
	int NewId(); // Creates a new Id based on #define
	char *Get(LLanguageId Lang = 0);
	void Set(const char *s, LLanguageId Lang = 0);
	void UnDuplicate();
	int Compare(LListItem *To, ssize_t Field);
	void CopyText();
	void PasteText();
	char *GetDefine() { return Define; }
	void SetDefine(const char *s);
	
	// Item
	int GetCols();
	const char *GetText(int i);
	void OnMouseClick(LMouse &m);

	// Fields
	bool GetFields(FieldTree &Fields);
	bool Serialize(FieldTree &Fields);

	// Persistence
	bool Test(ErrorCollection *e);
	bool Read(LXmlTag *t, SerialiseContext &Ctx);
	bool Write(LXmlTag *t, SerialiseContext &Ctx);
};

#define RESIZE_X1			0x0001
#define RESIZE_Y1			0x0002
#define RESIZE_X2			0x0004
#define RESIZE_Y2			0x0008

class StringGroupList;
class ResStringGroup : public Resource
{
	friend class ResString;
	friend class ResStringUi;
	friend class StringGroupList;

protected:
	ResStringUi *Ui = NULL;
	StringGroupList *Lst = NULL;
	List<ResString> Strs;
	LArray<LLanguage*> Lang;
	LArray<LLanguage*> Visible;
	int SortCol = 0;
	bool SortAscend = true;
	LString GrpName;
	
	void FillList();

public:
	ResStringGroup(AppWnd *w, int type = TYPE_STRING);
	~ResStringGroup();

	const char *Name();
	bool Name(const char *n);
	size_t Length() { return Strs.Length(); }
	LView *Wnd();
	ResStringGroup *GetStringGroup() { return this; }

	// Methods
	void Create(LXmlTag *load, SerialiseContext *ctx);
	ResString *CreateStr(bool User = false);
	void DeleteStr(ResString *Str);
	ResString *FindName(char *Name);
	ResString *FindRef(int Ref);
	int FindId(int Id, List<ResString*> &Strs);
	int UniqueRef();
	int UniqueId(char *Define = 0);
	int OnCommand(int Cmd, int Event, OsView hWnd);
	void SetLanguages();
	int GetLanguages() { return (int)Lang.Length(); }
	LLanguage *GetLanguage(int i) { return i < Lang.Length() ? Lang[i] : 0; }
	LLanguage *GetLanguage(LLanguageId Id);
	int GetLangIdx(LLanguageId Id);
	void AppendLanguage(LLanguageId Id);
	void DeleteLanguage(LLanguageId Id);
	void Sort();
	void RemoveUnReferenced();
	List<ResString> *GetStrs() { return &Strs; }
	void OnShowLanguages();

	// Clipboard access
	void DeleteSel();
	void Copy(bool Delete = false);
	void Paste();

	// Resource
	LView *CreateUI();
	void OnRightClick(LSubMenu *RClick);
	void OnCommand(int Cmd);
	bool Test(ErrorCollection *e);
	bool Read(LXmlTag *t, SerialiseContext &Ctx);
	bool Write(LXmlTag *t, SerialiseContext &Ctx);
	ResStringGroup *IsStringGroup() { return this; }
};

class StringGroupList : public LList
{
	ResStringGroup *grp = NULL;
	
public:
	StringGroupList(ResStringGroup *g);
	~StringGroupList();

	void UpdateColumns();
	void OnColumnClick(int Col, LMouse &m);
	void OnItemSelect(LArray<LListItem*> &Items);
};

class ResStringUi : public LBox
{
	LToolBar *Tools = NULL;
	ResStringGroup *StringGrp = NULL;
	LStatusBar *Status = NULL;
	LStatusPane *StatusInfo = NULL;

public:
	ResStringUi(ResStringGroup *Res);
	~ResStringUi();

	void OnCreate();
	LMessage::Result OnEvent(LMessage *Msg);
};

class LangDlg : public LDialog
{
	LCombo *Sel;
	List<LLanguage> Langs;

public:
	LLanguage *Lang;

	LangDlg(LView *parent, List<LLanguage> &l, int Init = -1);
	int OnNotify(LViewI *Ctrl, LNotification n);
};
