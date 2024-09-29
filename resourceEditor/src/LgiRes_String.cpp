/*
**	FILE:			LgiRes_String.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			10/9/1999
**	DESCRIPTION:	String Resource Editor
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_String.h"
#include "lgi/common/Token.h"
#include "lgi/common/Variant.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Combo.h"
#include "lgi/common/Button.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/ToolBar.h"
#include "lgi/common/Menu.h"
#include "lgi/common/StatusBar.h"
#include "resdefs.h"

////////////////////////////////////////////////////////////////////////////
LangDlg::LangDlg(LView *parent, List<LLanguage> &l, int Init)
{
	Lang = 0;
	SetParent((parent) ? parent : MainWnd);
	LRect r(0, 0, 260, 90);
	SetPos(r);
	MoveToCenter();
	Name("Language");

	Children.Insert(new LTextLabel(-1, 10, 10, -1, -1, "Select language:"));
	Children.Insert(Sel = new LCombo(-1, 20, 30, 150, 20, ""));
	if (Sel)
	{
		if (l.Length() > 40)
		{
			Sel->Sub(GV_STRING);
		}

		for (auto li: l)
		{
			Langs.Insert(li);
			Sel->Insert(li->Name);
		}

		if (Init >= 0)
		{
			Sel->Value(Init);
		}
	}

	Children.Insert(new LButton(IDOK, 180, 5, 60, 20, "Ok"));
	Children.Insert(new LButton(IDCANCEL, 180, 30, 60, 20, "Cancel"));
}

int LangDlg::OnNotify(LViewI *Ctrl, LNotification n)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		{
			Lang = Langs.ItemAt(Sel->Value());
			EndModal(1);
			break;
		}
		case IDCANCEL:
		{
			EndModal(0);
			break;
		}
	}

	return 0;
}	

////////////////////////////////////////////////////////////////////
StrLang::StrLang()
{
	Lang = "en";
	Str = 0;
}

StrLang::~StrLang()
{
	DeleteArray(Str);
}

LLanguageId StrLang::GetLang()
{
	return Lang;
}

void StrLang::SetLang(LLanguageId i)
{
	LAssert(i);
	Lang = i;
}

char *&StrLang::GetStr()
{
	return Str;
}

void StrLang::SetStr(const char *s)
{
	char *n = NewStr(s);
	DeleteArray(Str);
	Str = n;
}

bool StrLang::IsEnglish()
{
	return stricmp(Lang, "en") == 0;
}

bool StrLang::operator ==(LLanguageId LangId)
{
	return stricmp(Lang, LangId) == 0;
}

bool StrLang::operator !=(LLanguageId LangId)
{
	return !(*this == LangId);
}

////////////////////////////////////////////////////////////////////
ResString::ResString(ResStringGroup *grp, int init_ref)
{
	Ref = init_ref;
	Group = grp;
	LAssert(Group);
	IdStr[0] = 0;
	RefStr[0] = 0;

	if (Group)
	{
		LAssert(!Group->Strs.HasItem(this));
		Group->Strs.Insert(this);
	}
	else
	{
		LAssert(0);
	}

	// LStackTrace("%p::ResString\n", this);
}

ResString::~ResString()
{
	if (Group)
	{
		Group->App()->OnObjDelete(this);

		if (!Group->Strs.Delete(this))
			LAssert(0);
	}

	for (auto s: Items)
	{
		DeleteObj(s);
	}

	DeleteArray(Tag);
}

int ResString::SetRef(int r)
{
	LAssert(r != 0 && r != -1);

	if (r != Ref)
	{
		Ref = r;

		Update();
		UpdateAllViews();
	}	

	return Ref;
}

int ResString::SetId(int id)
{
	LAssert(id != 0);
	if (Id != id)
	{
		Id = id;

		Update();
		UpdateAllViews();
	}
	return Id;
}

void ResString::SetDefine(const char *s)
{
	Define = s;
}

ResString &ResString::operator =(ResString &s)
{
	memcpy(RefStr, s.RefStr, sizeof(RefStr));
	memcpy(IdStr, s.IdStr, sizeof(IdStr));
	
	SetRef(s.GetRef());
	SetId(s.GetId());
	
	SetDefine(s.GetDefine());
	DeleteObj(Tag);
	Tag = NewStr(s.Tag);
	
	for (auto l: s.Items)
	{
		Set(l->GetStr(), l->GetLang());
	}

	return *this;
}

char *ResString::Get(LLanguageId Lang)
{
	if (!Lang)
	{
		LLanguage *L = Group->App()->GetCurLang();
		if (L)
		{
			Lang = L->Id;
		}
		else
		{
			LAssert(0);
			return 0;
		}
	}

	StrLang *s = GetLang(Lang);
	if (s)
	{
		return s->GetStr();
	}

	return 0;
}

void ResString::Set(const char *p, LLanguageId Lang)
{
	if (!Lang)
	{
		LLanguage *L = Group->App()->GetCurLang();
		if (L)
		{
			Lang = L->Id;
		}
		else
		{
			LAssert(0);
			return;
		}
	}

	StrLang *s = GetLang(Lang);
	if (!s)
	{
		Items.Insert(s = new StrLang);
		if (s)
		{
			s->SetLang(Lang);
		}
	}
	if (s)
	{
		s->SetStr(p);

		Update();
		UpdateAllViews();
	}
	else
	{
		LAssert(0);
	}
}

void ResString::UnDuplicate()
{
	// reset Ref if it's a duplicate
	int OldStrRef = Ref;
	Ref = 0x80000000;
	ResString *Dupe = Group->App()->GetStrFromRef(OldStrRef);
	// now we return the Id to the string
	
	SetRef(OldStrRef);

	if (Dupe ||	Ref < 0)
	{
		// there is a duplicate string Id
		// so assign a new Id
		SetRef(Group->App()->GetUniqueStrRef());
	}				
}

char **ResString::GetIndex(int i)
{
	StrLang *s = Items.ItemAt(i);
	return (s) ? &s->GetStr() : 0;
}


bool ResString::Test(ErrorCollection *e)
{
	bool Status = true;

	if (ValidStr(Define) && Id == 0)
	{
		ErrorInfo *Err = &e->StrErr.New();
		Err->Str = this;
		Err->Msg.Reset(NewStr("No ctrl id."));
		Status = false;
	}

	if (Ref == 0)
	{
		ErrorInfo *Err = &e->StrErr.New();
		Err->Str = this;
		Err->Msg.Reset(NewStr("No resource ref."));
		Status = false;
	}

	#if 0
	if (!ValidStr(Define))
	{
		ErrorInfo *Err = &e->StrErr.New();
		Err->Str = this;
		Err->Msg.Reset(NewStr("No define name."));
		Status = false;
	}
	#endif

	for (auto s: Items)
	{
		if (!LIsUtf8(s->GetStr()))
		{
			ErrorInfo *Err = &e->StrErr.New();
			Err->Str = this;
			char m[256];
			snprintf(m, sizeof(m), "Utf-8 error in translation '%s'", s->GetLang());
			Err->Msg.Reset(NewStr(m));
			Status = false;
		}
	}

	return Status;
}

bool ResString::Read(LXmlTag *t, SerialiseContext &Ctx)
{
	if (!t || !t->IsTag("string"))
	{
		Ctx.Log.Print("%s:%i - Not a string tag (Tag='%s').\n", _FL, t->GetTag());
		return false;
	}

	char *n = 0;

	int Ref = t->GetAsInt("Ref");
	if (!Ref)
	{
		Ctx.Log.Print("%s:%i - String has no Ref number.\n", _FL);
		return false;
	}
	
	SetRef(Ref);
	if ((n = t->GetAttr("Define")) &&
		ValidStr(n))
	{
		SetDefine(n);

		char *c;
		if ((c = t->GetAttr("Cid")) ||
			(c = t->GetAttr("Id")))
		{
			int Cid = atoi(c);
			if (!Cid)
			{
				Ctx.Log.Print("%s:%i - String missing id (ref=%i)\n", _FL, Ref);
				Ctx.FixId.Add(this);
			}
			else
			{
				SetId(Cid);
			}
		}
	}
	if ((n = t->GetAttr("Tag")))
	{
		DeleteArray(Tag);
		Tag = NewStr(n);
	}

	for (int i=0; i<t->Attr.Length(); i++)
	{
		LXmlAttr *v = &t->Attr[i];
		if (v->GetName())
		{
			LLanguage *Lang = LFindLang(v->GetName());
			if (Lang)
			{
				if (Ctx.Format == Lr8File)
				{
					LAssert(LIsUtf8(v->GetName()));
					Set(v->GetValue(), Lang->Id);
				}
				else if (Ctx.Format == CodepageFile)
				{
					char *Utf8 = (char*)LNewConvertCp("utf-8", v->GetValue(), Lang->Charset);
					Set(Utf8 ? Utf8 : v->GetValue(), Lang->Id);
					if (Utf8)
					{
						DeleteArray(Utf8);
					}
				}
				else if (Ctx.Format == XmlFile)
				{
					char *x = DecodeXml(v->GetValue());
					if (x)
					{
						Set(x, Lang->Id);
						DeleteArray(x);
					}
				}
				else
					LAssert(0);
			}
		}
	}

	return true;
}

bool ResString::Write(LXmlTag *t, SerialiseContext &Ctx)
{
	t->SetTag("String");
	t->SetAttr("Ref", Ref);
	if (ValidStr(Define))
	{
		t->SetAttr("Cid", Id);
		t->SetAttr("Define", Define);
	}
	if (Tag) t->SetAttr("Tag", Tag);

	char *English = 0;
	for (auto s: Items)
	{
		if (s->IsEnglish())
			English = s->GetStr();
	}

	for (auto s: Items)
	{
		if (ValidStr(s->GetStr()))
		{
			char *Mem = 0;
			char *String = 0;

			if (Ctx.Format == Lr8File)
			{
				// Don't save the string if it's the same as the English
				if (English && !s->IsEnglish())
				{
					if (strcmp(English, s->GetStr()) == 0)
					{
						continue;
					}
				}

				// Save this string
				String = s->GetStr();
			}
			else if (Ctx.Format == CodepageFile)
			{
				LLanguage *Li = LFindLang(s->GetLang());
				if (Li)
				{
					Mem = String = (char*)LNewConvertCp(Li->Charset, s->GetStr(), "utf-8");
				}

				if (!String) String = s->GetStr();
			}
			else if (Ctx.Format == XmlFile)
			{
				// Don't save the string if it's the same as the English
				if (English && !s->IsEnglish())
				{
					if (strcmp(English, s->GetStr()) == 0)
					{
						continue;
					}
				}

				// Save this string
				String = Mem = EncodeXml(s->GetStr());
			}
			else
				LAssert(0);

			if (ValidStr(String))
			{
				t->SetAttr(s->GetLang(), String);
			}

			DeleteArray(Mem);
		}
	}

	return true;
}

StrLang *ResString::GetLang(LLanguageId i)
{
	for (auto s: Items)
	{
		if (stricmp(s->GetLang(), i) == 0)
		{
			return s;
		}
	}

	return 0;
}

bool ResString::GetFields(FieldTree &Fields)
{
	// string view [literal]
	Fields.Insert(this, DATA_INT, 199, VAL_Ref, "Ref");
	Fields.Insert(this, DATA_INT, 200, VAL_Id, "Id");

	// else dialog view [embeded]
	Fields.Insert(this, DATA_STR, 201, VAL_Define, "#define");
	Fields.Insert(this, DATA_STR, 198, VAL_Tag, "Tag");

	if (Group)
	{
		for (int i=0; i<Group->GetLanguages(); i++)
		{
			if (Group->App()->ShowLang(Group->Lang[i]->Id))
			{
				Fields.Insert(this, DATA_STR, 202+i, Group->Lang[i]->Name, Group->Lang[i]->Name);
			}
		}
	}

	return true;
}

bool ResString::Serialize(FieldTree &Fields)
{
	Fields.Serialize(this, VAL_Ref, Ref);
	Fields.Serialize(this, VAL_Id, Id);
	Fields.Serialize(this, VAL_Define, Define);
	Fields.Serialize(this, VAL_Tag, Tag);

	if (Group)
	{
		for (int i=0; i<Group->GetLanguages(); i++)
		{
			LLanguageId Id = Group->Lang[i]->Id;
			StrLang *s = GetLang(Id);
			if (Fields.GetMode() == FieldTree::UiToObj && !s)
			{
				s = new StrLang;
				if (s)
				{
					Items.Insert(s);
					s->SetLang(Id);
				}
			}
			if (s)
			{
				Fields.Serialize(this, Group->Lang[i]->Name, s->GetStr());
			}
		}

		Update();
	}

	return true;
}

int ResString::NewId()
{
	List<ResString> sl;

	if (Group && Group->AppWindow)
	{
		Group->AppWindow->FindStrings(sl, Define);
		int NewId = Group->AppWindow->GetUniqueCtrlId();
		for (auto s: sl)
		{
			s->SetId(NewId);
			s->Update();
		}
	}
	else LAssert(!"Invalid ptrs.");

	return Id;
}

int ResString::GetCols()
{
	return 3 + Group->GetLanguages();
}

const char *ResString::GetText(int i)
{
	switch (i)
	{
		case 0:
		{
			return Define ? Define : "";
		}
		case 1:
		{
			snprintf(RefStr, sizeof(RefStr), "%i", Ref);
			return RefStr;
		}
		case 2:
		{
			snprintf(IdStr, sizeof(IdStr), "%i", Id);
			return IdStr;
		}
		default:
		{
			int LangIdx = i - 3;
			if (LangIdx < Group->Visible.Length())
			{
				LLanguage *Info = Group->Visible[LangIdx];
				if (Info)
				{
					for (auto s: Items)
					{
						if (*s == Info->Id)
						{
							return (s->GetStr()) ? s->GetStr() : (char*)"";
						}
					}
				}
			}
			break;
		}
	}

	return NULL;
}

void ResString::OnMouseClick(LMouse &m)
{
	if (m.IsContextMenu())
	{
		LSubMenu *RClick = new LSubMenu;
		if (RClick)
		{
			bool PasteTranslations = false;
			char *Clip;

			{
				LClipBoard c(Parent);
				Clip = c.Text();
				if (Clip)
					PasteTranslations = strstr(Clip, TranslationStrMagic);
			}

			RClick->AppendItem("Copy Text", ID_COPY_TEXT, true);
			RClick->AppendItem("Paste Text", ID_PASTE_TEXT, PasteTranslations);
			RClick->AppendSeparator();
			RClick->AppendItem("New Id", ID_NEW_ID, true);
			RClick->AppendItem("Make Ref=Id", ID_REF_EQ_ID, true);
			RClick->AppendItem("Make Id=Ref", ID_ID_EQ_REF, true);

			if (Parent->GetMouse(m, true))
			{
				int Cmd = 0;
				switch (Cmd = RClick->Float(Parent, m.x, m.y))
				{
					case ID_COPY_TEXT:
					{
						CopyText();
						break;
					}
					case ID_PASTE_TEXT:
					{
						PasteText();
						break;
					}
					case ID_NEW_ID:
					{
						List<ResString> Sel;
						if (!GetList()->GetSelection(Sel))
							break;

						auto App = Group->AppWindow;
						for (auto s : Sel)
						{
							List<ResString> sl;
							App->FindStrings(sl, s->Define);
								
							int NewId = App->GetUniqueCtrlId();
							for (auto i : sl)
							{
								i->SetId(NewId);
							}
						}
						break;
					}
					case ID_REF_EQ_ID:
					{
						List<ResString> a;
						if (GetList()->GetSelection(a))
						{
							bool Dirty = false;
							for (auto s: a)
							{
								if (s->GetId() != s->GetRef())
								{
									ResString *Existing = Group->App()->GetStrFromRef(s->GetId());
									if (!Existing)
									{
										s->SetRef(s->GetId());
										Dirty = true;
									}
								}
							}
							
							if (Dirty)
							{
								Group->App()->SetDirty(true, NULL);
							}
						}
						break;
					}
					case ID_ID_EQ_REF:
					{
						List<ResString> a;
						if (GetList()->GetSelection(a))
						{
							bool Dirty = false;
							for (auto s: a)
							{
								if (s->GetId() != s->GetRef())
								{
									List<ResString> Existing;
									int CtrlId = s->GetRef();
									Group->App()->FindStrings(Existing, 0, &CtrlId);
									if (Existing.Length() == 0)
									{
										s->SetId(s->GetRef());
										Dirty = true;
									}
								}
							}
							
							if (Dirty)
							{
								Group->App()->SetDirty(true, NULL);
							}
						}
						break;
					}
				}
			}

			DeleteObj(RClick);
		}
	}
}

void ResString::CopyText()
{
	if (Items.Length() > 0)
	{
		LStringPipe p;

		p.Push(TranslationStrMagic);
		p.Push(EOL_SEQUENCE);

		for (auto s: Items)
		{
			char Str[256];
			snprintf(Str, sizeof(Str), "%s,%s", s->GetLang(), s->GetStr());
			p.Push(Str);
			p.Push(EOL_SEQUENCE);
		}

		char *All = p.NewStr();
		if (All)
		{
			LClipBoard Clip(Parent);

			Clip.Text(All);

			char16 *w = Utf8ToWide(All);
			Clip.TextW(w, false);
			DeleteArray(w);

			DeleteArray(All);
		}
	}
	else
	{
		LgiMsg(Parent, "No translations to copy.", "ResDialogCtrl::CopyText", MB_OK);
	}
}

void ResString::PasteText()
{
	LClipBoard c(Parent);
	
	char *Clip = 0;
	char16 *w = c.TextW();
	if (w)
	{
		Clip = WideToUtf8(w);
	}
	else
	{
		Clip = c.Text();
	}

	if (Clip)
	{
		LToken Lines(Clip, "\r\n");
		if (Lines.Length() > 0 &&
			strcmp(Lines[0], TranslationStrMagic) == 0)
		{
			Items.DeleteObjects();

			for (int i=1; i<Lines.Length(); i++)
			{
				char *Translation = strchr(Lines[i], ',');
				if (Translation)
				{
					*Translation++ = 0;

					LLanguage *l = LFindLang(Lines[i]);
					if (l)
					{
						Set(Translation, l->Id);
					}
				}
			}

			if (Group && Group->App())
			{
				Group->App()->OnObjSelect(this);
			}
		}
	}

	if (w)
	{
		DeleteArray(Clip);
	}
}

////////////////////////////////////////////////////////////////////
int ResStringCompareFunc(LListItem *a, LListItem *b, ssize_t Data)
{
	ResString *A = dynamic_cast<ResString*>(a);
	if (A)
	{
		return A->Compare(dynamic_cast<ResString*>(b), Data);
	}
	return -1;
}

StringGroupList::StringGroupList(ResStringGroup *g) :
	LList(0, 0, 200, 200),
	grp(g)
{
	Sunken(false);
}

StringGroupList::~StringGroupList()
{
	RemoveAll(); // This list doesn't own the strings...
	grp->Lst = NULL;
}

void StringGroupList::OnColumnClick(int Col, LMouse &m)
{
	if (grp->SortCol == Col)
	{
		grp->SortAscend = !grp->SortAscend;
	}
	else
	{
		grp->SortCol = Col;
		grp->SortAscend = true;
	}

	LList::Sort<ssize_t>(ResStringCompareFunc, (grp->SortCol + 1) * ((grp->SortAscend) ? 1 : -1));

	LListItem *Sel = GetSelected();
	if (Sel)
	{
		Sel->ScrollTo();
	}
}

void StringGroupList::OnItemSelect(LArray<LListItem*> &Items)
{
	if (IsAttached())
	{
		ResString *s = dynamic_cast<ResString*>(Items[0]);
		if (s && grp->AppWindow)
		{
			grp->AppWindow->OnObjSelect(s);
		}
	}
}

void StringGroupList::UpdateColumns()
{
	EmptyColumns();
	AddColumn("#define", 150);
	AddColumn("Ref", 70);
	AddColumn("Id", 70);

	grp->Visible.Length(0);
	for (int i=0; i<grp->GetLanguages(); i++)
	{
		if (grp->App()->ShowLang(grp->Lang[i]->Id))
		{
			grp->Visible.Add(grp->Lang[i]);
			AddColumn(grp->Lang[i]->Name, 100);
		}
	}
}

////////////////////////////////////////////////////////////////////
ResStringGroup::ResStringGroup(AppWnd *w, int type) : 
	Resource(w, type)
{
	Name("<untitled>");

	AppendLanguage("en");
	App()->OnLanguagesChange("en", true);
}

ResStringGroup::~ResStringGroup()
{
	while (Strs.Length())
		delete Strs[0];
	DeleteObj(Ui);
}

void ResStringGroup::FillList()
{
	if (Lst)
	{
		auto n = Name();
		Lst->Name(n);
		Lst->UpdateColumns();
		
		LArray<LListItem*> a;
		for (auto i: Strs)
			a.Add(i);
		Lst->Insert(a);
	}
}

LView *ResStringGroup::Wnd()
{
	if (!Lst)
	{
		Lst = new StringGroupList(this);
		FillList();
	}
	return Lst;
}

LLanguage *ResStringGroup::GetLanguage(LLanguageId Id)
{
	for (int i=0; i<Lang.Length(); i++)
	{
		if (Id == Lang[i]->Id)
			return Lang[i];
	}
	return 0;
}

int ResStringGroup::GetLangIdx(LLanguageId Id)
{
	for (int i=0; i<Lang.Length(); i++)
	{
		if (stricmp(Id, Lang[i]->Id) == 0)
			return i;
	}
	return -1;
}

int ResString::Compare(LListItem *li, ssize_t Column)
{
	ResString *r = dynamic_cast<ResString*>(li);
	static const char *Empty = "";
	bool Mod = (Column < 0) ? -1 : 1;
	Column = abs(Column) - 1;

	if (r)
	{
		switch (Column)
		{
			case 0:
			{
				return stricmp(Define ? Define.Get() : Empty, r->Define ? r->Define.Get() : Empty) * Mod;
				break;
			}
			case 1:
			{
				return (Ref - r->Ref) * Mod;
				break;
			}
			case 2:
			{
				return (Id - r->Id) * Mod;
				break;
			}
			default:
			{
				auto Col = Column - 3;
				if (Group && Col >= 0 && Col < Group->GetLanguages())
				{
					LLanguageId Lang = Group->Visible[Column - 3]->Id;
					char *a = Get(Lang);
					char *b = r->Get(Lang);
					const char *Empty = "";
					return stricmp(a?a:Empty, b?b:Empty) * Mod;
				}
				break;
			}
		}
	}

	return -1;
}

void ResStringGroup::OnShowLanguages()
{
	if (Lst)
	{
		Lst->UpdateColumns();
		Lst->UpdateAllItems();
	}
}

void ResStringGroup::DeleteLanguage(LLanguageId Id)
{
	// Check for presence
	int Index = -1;
	for (int i=0; i<GetLanguages(); i++)
	{
		if (Lang[i]->Id == Id)
		{
			Index = i;
			break;
		}
	}

	if (Index >= 0)
	{
		// Remove lang
		Lang.DeleteAt(Index);
		if (Lst)
			Lst->UpdateColumns();
	}
}

void ResStringGroup::AppendLanguage(LLanguageId Id)
{
	// Check for presence
	int Index = -1;
	for (int i=0; i<Lang.Length(); i++)
	{
		if (Lang[i]->Id == Id)
		{
			Index = i;
			break;
		}
	}

	if (Index < 0)
	{
		LLanguage *n = LFindLang(Id);
		if (n)
		{
			Lang[Lang.Length()] = n;
			if (Lst)
				Lst->UpdateColumns();
		}
	}
}

int RefCmp(ResString *a, ResString *b, NativeInt d)
{
	return a->GetRef() - b->GetRef();
}

void ResStringGroup::Sort()
{
	Strs.Sort(RefCmp);
}

void ResStringGroup::RemoveUnReferenced()
{
	for (auto It = Strs.begin(); It != Strs.end(); )
	{
		auto s = *It;
		if (!s->HasViews())
		{
			s->Group = NULL;
			Strs.Delete(It);
			DeleteStr(s);
		}
		else It++;
	}
}

int ResStringGroup::OnCommand(int Cmd, int Event, OsView hWnd)
{
	switch (Cmd)
	{
		case IDM_NEW:
		{
			CreateStr(true);
			break;
		}
		case ID_DELETE:
		{
			List<ResString> l;
			if (Lst &&
				Lst->GetSelection(l))
			{
				for (auto s: l)
				{
					DeleteObj(s);
				}
			}
			break;
		}
		case ID_NEW_LANG:
		{
			// List all the languages minus the ones already
			// being edited
			List<LLanguage> l;
			for (LLanguage *li = LFindLang(NULL); li->Id; li++)
			{
				bool Present = false;
				for (int i=0; i<Lang.Length(); i++)
				{
					if (Lang[i]->Id == li->Id)
					{
						Present = true;
						break;
					}
				}

				if (!Present)
				{
					l.Insert(li);
				}
			}

			// Display the list
			auto Dlg = new LangDlg(Lst, l);
			Dlg->DoModal([this, Dlg](auto dlg, auto id)
			{
				if (id && Dlg->Lang)
				{
					AppendLanguage(Dlg->Lang->Id);

					// Update the global language list
					AppWindow->ShowLang(Dlg->Lang->Id, true);
				}
			});
			break;
		}
		case ID_DELETE_LANG:
		{
			// List all the languages being edited
			List<LLanguage> l;
			for (int i=0; i<Lang.Length(); i++)
			{
				l.Insert(Lang[i]);
			}

			// Display the list
			auto Dlg = new LangDlg(Lst, l);
			Dlg->DoModal([this, Dlg](auto dlg, auto id)
			{
				if (id && Dlg->Lang)
				{
					DeleteLanguage(Dlg->Lang->Id);
					// CurrentLang = limit(CurrentLang, 0, Lang.Length()-1);
				}
			});
			break;
		}
	}
	return 0;
}

ResString *ResStringGroup::CreateStr(bool User)
{
    int NextRef = App()->GetUniqueStrRef();
	ResString *s = new ResString(this, NextRef);
	if (s)
	{
		s->SetId(s->SetRef(NextRef));
		if (Lst)
			Lst->Insert(s);

		if (User)
		{
			char Str[256];
			snprintf(Str, sizeof(Str), "IDS_%i", s->Ref);
			s->SetDefine(Str);

			if (Lst)
			{
				s->ScrollTo();
				s->Select(true);
			}
		}
	}
	
	return s;
}

void ResStringGroup::DeleteStr(ResString *Str)
{
	if (Str->Items.Length() == 0)
		DeleteObj(Str);
}

ResString *ResStringGroup::FindName(char *Name)
{
	if (Name)
	{
		for (auto s: Strs)
		{
			if (stricmp(s->Define, Name) == 0)
			{
				return s;
			}
		}
	}
	return NULL;
}

ResString *ResStringGroup::FindRef(int Ref)
{
	for (auto s: Strs)
	{
		if (s->GetRef() == Ref)
		{
			return s;
		}
	}

	return NULL;
}

int ResStringGroup::UniqueRef()
{
	int n = 1;
	for (auto s: Strs)
	{
		n = MAX(n, s->Ref);
	}

	return n + 1;
}

int ResStringGroup::UniqueId(char *Define)
{
	int n = 1;

	for (auto i: Strs)
	{
		if (i->Id &&
			i->Define &&
			Define &&
			stricmp(Define, i->Define) == 0)
		{
			return i->Id;
		}

		n = MAX(n, i->Id);
	}

	return n + 1;
}

int ResStringGroup::FindId(int Id, List<ResString*> &Strs)
{
	return 0;
}

void ResStringGroup::SetLanguages()
{
	List<LLanguage> l;
	bool EnglishFound = false;

	for (auto s: Strs)
	{
		for (auto sl: s->Items)
		{
			LLanguage *li = 0;
			for (auto i: l)
				if (*sl == i->Id)
				{
					li = i;
					break;
				}
			
			if (!li)
			{
				LLanguage *NewLang = LFindLang(sl->GetLang());
				if (NewLang) l.Insert(NewLang);
				
				if (*sl == (LLanguageId)"en")
					EnglishFound = true;
			}
		}
	}

	// CurrentLang = 0;
	// LAssert(CurrentLang < Lang.Length());

	Lang.Length(l.Length() + ((EnglishFound) ? 0 : 1));

	int n = 0;
	Lang[n++] = LFindLang("en");
	for (auto li: l)
	{
		if (stricmp(li->Id, "en") != 0) // !English
		{
			Lang[n] = LFindLang(li->Id);
			LAssert(Lang[n]);
			n++;
		}
	}

	if (Lst)
		Lst->UpdateColumns();
}

void ResStringGroup::DeleteSel()
{
}

void ResStringGroup::Copy(bool Delete)
{
}

void ResStringGroup::Paste()
{
}

const char *ResStringGroup::Name()
{
	return GrpName;
}

bool ResStringGroup::Name(const char *n)
{
	GrpName = n;
	if (Lst)
		Lst->Name(n);
	return true;
}

LView *ResStringGroup::CreateUI()
{
	return Ui = new ResStringUi(this);
}

bool ResStringGroup::Test(ErrorCollection *e)
{
	bool Status = true;

	for (auto s: Strs)
	{
		if (!s->Test(e))
		{
			Status = false;
		}
	}

	return Status;
}

bool ResStringGroup::Read(LXmlTag *t, SerialiseContext &Ctx)
{
	bool Status = false;

	char *p = NULL;
	if ((p = t->GetAttr("Name")))
	{
		if (Ctx.Format == XmlFile)
		{
			if (auto x = DecodeXml(p))
			{
				Name(x);
				DeleteArray(x);
			}
		}
		else
		{
			Name(p);
		}
		
		if (Lst)
			Lst->Empty();

		LHashTbl<ConstStrKey<char,false>, bool> L;
		L.Add("en", true);
		Status = true;
		for (auto c: t->Children)
		{
			ResString *s = new ResString(this);
			if (s && s->Read(c, Ctx))
			{
				for (auto i: s->Items)
				{
					if (!L.Find(i->GetLang()))
					{
						L.Add(i->GetLang(), true);
						AppendLanguage(i->GetLang());
					}
				}
			}
			else
			{
				// Lets not fail everything here... just keep trying to read
				DeleteObj(s);
			}
		}
	}

	if (Item)
	{
		Item->Update();
	}

	return Status;
}

bool ResStringGroup::Write(LXmlTag *t, SerialiseContext &Ctx)
{
	bool Status = true;

	t->SetTag("string-group");
	auto n = Ctx.Format == XmlFile ? EncodeXml(Name()) : NewStr(Name());
	t->SetAttr("Name", n);
	DeleteArray(n);

	for (auto i: Strs)
	{
		ResString *s = dynamic_cast<ResString*>(i);
		if (s && (s->Define || s->Items.Length() > 0))
		{
			LXmlTag *c = new LXmlTag;
			if (c && s->Write(c, Ctx))
			{
				t->InsertTag(c);
			}
			else
			{
				Status = false;
				DeleteObj(c);
			}
		}
	}

	return Status;
}

void ResStringGroup::OnRightClick(LSubMenu *RClick)
{
	if (!RClick || (Lst && !Lst->Enabled()))
		return;

	RClick->AppendSeparator();
	if (Type() > 0)
	{
		auto Export = RClick->AppendSub("Export to...");
		if (Export)
		{
			Export->AppendItem("Lgi File", ID_EXPORT, true);
			Export->AppendItem("Win32 Resource Script", ID_EXPORT_WIN32, false);
		}
	}
}

void ResStringGroup::OnCommand(int Cmd)
{
	switch (Cmd)
	{
		case ID_IMPORT:
		{
			auto Select = new LFileSelect(AppWindow);
			Select->Type("Text", "*.txt");
			Select->Open([this, Select](auto dlg, auto status)
			{
				if (status)
				{
					LFile F;
					if (F.Open(dlg->Name(), O_READ))
					{
						SerialiseContext Ctx;
						Resource *Res = AppWindow->NewObject(Ctx, 0, -Type());
						if (Res)
						{
							// TODO
							// Res->Read();
						}
					}
					else
					{
						LgiMsg(AppWindow, "Couldn't open file for reading.");
					}
				}
			});
			break;
		}
		case ID_EXPORT:
		{
			auto Select = new LFileSelect(AppWindow);
			Select->Type("Text", "*.txt");
			Select->Save([Select,this](auto dlg, auto status)
			{
				if (status)
				{
					LFile F;
					if (F.Open(dlg->Name(), O_WRITE))
					{
						F.SetSize(0);
						// Serialize(F, true);
					}
					else
					{
						LgiMsg(AppWindow, "Couldn't open file for writing.");
					}
				}
			});
			break;
		}
	}
}

void ResStringGroup::Create(LXmlTag *load, SerialiseContext *ctx)
{
	if (load)
	{
		if (ctx)
			Read(load, *ctx);
		else
			LAssert(0);
	}
}

////////////////////////////////////////////////////////////////////
ResStringUi::ResStringUi(ResStringGroup *Res) :
	LBox(ID_STR_UI, true)
{
	StringGrp = Res;
}

ResStringUi::~ResStringUi()
{
	if (StringGrp)
	{
		delete StringGrp->Wnd();
		StringGrp->Ui = NULL;
	}
}

void ResStringUi::OnCreate()
{
	if ((Tools = new LToolBar))
	{
		auto FileName = LFindFile("_StringIcons.gif");
		if (FileName && Tools->SetBitmap(FileName, 16, 16))
		{
			Tools->Attach(this);

			Tools->AppendButton("New String", IDM_NEW, TBT_PUSH, !StringGrp->SystemObject());
			Tools->AppendButton("Delete String", ID_DELETE, TBT_PUSH, !StringGrp->SystemObject());
			Tools->AppendSeparator();

			Tools->AppendButton("New Language", ID_NEW_LANG, TBT_PUSH);
			Tools->AppendButton("Delete Language", ID_DELETE_LANG, TBT_PUSH);
		}
		else
		{
			DeleteObj(Tools);
		}
	}

	if (auto w = StringGrp->Wnd())
	{
		StringGrp->FillList();
		w->Attach(this);
	}

	if ((Status = new LStatusBar))
	{
		Status->Attach(this);
		StatusInfo = Status->AppendPane("", 2);
	}
}

LMessage::Result ResStringUi::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_COMMAND:
		{
			StringGrp->OnCommand(Msg->A()&0xffff, (int)(Msg->A()>>16), (OsView) Msg->B());
			break;
		}
		case M_DESCRIBE:
		{
			char *Text = (char*) Msg->B();
			if (Text)
			{
				StatusInfo->Name(Text);
			}
			break;
		}
	}

	return LView::OnEvent(Msg);
}
