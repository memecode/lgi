/*hdr
**      FILE:           LgiRes.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           12/3/2000
**      DESCRIPTION:    Xp resource interfaces
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/List.h"
#include "lgi/common/TableLayout.h"
#if defined(LINUX) && !defined(LGI_SDL)
#include "LgiWinManGlue.h"
#endif
#include "lgi/common/Variant.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ProgressView.h"

// If it is defined it will use the cross platform 
// "res" library distributed with the LGI library.

#define DEBUG_RES_FILE						0
#define CastToGWnd(RObj)					((RObj != 0) ? dynamic_cast<LView*>(RObj) : 0)

class TagHash : public LHashTbl<StrKey<char>,bool>, public ResReadCtx
{
	GToken Toks;

public:
	TagHash(const char *TagList) : Toks(TagList)
	{
		for (int i=0; i<Toks.Length(); i++)
			Add(Toks[i], true);
	}

	bool Check(const char *Tags)
	{
		bool Result = true;
		if (Tags)
		{
			GToken t(Tags);
			for (int i=0; i<t.Length(); i++)
			{
				char *Tag = t[i];
				if (*Tag == '!')
				{
					if (Find(Tag + 1))
						return false;
				}
				else if (!Find(Tag))
				{
					Result = false;
					break;
				}
			}
		}
		return Result;
	}

	bool Check(LXmlTag *t)
	{
		if (!t)
			return false;

		char *Tag = t->GetAttr("Tag");
		if (Tag)
			return Check(Tag);

		return true;
	}
};

const char *LStringRes::CodePage = 0;
LLanguage *LStringRes::CurLang = 0;

LStringRes::LStringRes(LResources *res)
{
	Res = res;
	Ref = 0;
	Id = 0;
	Str = 0;
	Tag = 0;
	// IsString = false;
}

LStringRes::~LStringRes()
{
	DeleteArray(Str);
	DeleteArray(Tag);
}

bool LStringRes::Read(LXmlTag *t, ResFileFormat Format)
{
	if (LStringRes::CurLang &&
		t &&
		stricmp(t->GetTag(), "string") == 0)
	{
		char *n = 0;
		if ((n = t->GetAttr("Cid")) ||
			(n = t->GetAttr("Id")) )
		{
			Id = atoi(n);
		}
		if ((n = t->GetAttr("Ref")))
		{
			Ref = atoi(n);
		}
		if ((n = t->GetAttr("Define")))
		{
			if (strcmp(n, "IDOK") == 0)
			{
				Id = IDOK;
			}
			else if (strcmp(n, "IDCANCEL") == 0)
			{
				Id = IDCANCEL;
			}
			else if (strcmp(n, "IDC_STATIC") == 0)
			{
				Id = -1;
			}
		}
		if ((n = t->GetAttr("Tag")))
		{
			Tag = NewStr(n);
		}

		const char *Cp = LStringRes::CodePage;
		char Name[256];
		strcpy_s(Name, sizeof(Name), LStringRes::CurLang->Id);
		n = 0;
		
		if ((n = t->GetAttr(Name)) &&
			strlen(n) > 0)
		{
			// Language string ok
			// Lang = LangFind(0, CurLang, 0);
		}
		else if (LStringRes::CurLang->OldId &&
				sprintf_s(Name, sizeof(Name), "Text(%i)", LStringRes::CurLang->OldId) &&
				(n = t->GetAttr(Name)) &&
				strlen(n) > 0)
		{
			// Old style language string ok
		}
		else if (!LStringRes::CurLang->IsEnglish())
		{
			// no string, try english
			n = t->GetAttr("en");
			LLanguage *Lang = GFindLang("en");
			if (Lang)
			{
				Cp = Lang->Charset;
			}
		}

		if (n)
		{
			DeleteArray(Str);

			if (Cp)
			{
				Str = (char*)LNewConvertCp("utf-8", n, Cp);
			}
			else
			{
				Str = NewStr(n);
			}

			if (Str)
			{
				char *d = Str;
				for (char *s = Str; s && *s;)
				{
					if (*s == '\\')
					{
						if (*(s+1) == 'n')
						{
							*d++ = '\n';
						}
						else if (*(s+1) == 't')
						{
							*d++ = '\t';
						}
						s += 2;
					}
					else 
					{
						*d++ = *s++;
					}
				}
				*d++ = 0;
			}
		}

		if (Res)
		{
			for (int a=0; a<t->Attr.Length(); a++)
			{
				LXmlAttr *v = &t->Attr[a];
				char *Name = v->GetName();

				if (GFindLang(Name))
				{
					Res->AddLang(Name);
				}
				/*
				else if (	Name[0] == 'T' &&
							Name[1] == 'e' && 
							Name[2] == 'x' && 
							Name[3] == 't' &&
							Name[4] == '(')
				{
					int Old = atoi(Name + 5);
					if (Old)
					{
						LLanguage *OldLang = GFindOldLang(Old);
						if (OldLang)
						{
							LAssert(OldLang->OldId == Old);
							Res->AddLang(OldLang->Id);
						}
					}
				}
				*/
			}
		}

		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////
// Lgi resources
class LResourcesPrivate
{
public:
	bool Ok;
	ResFileFormat Format;
	LString File;
	LString ThemeFolder;
	LHashTbl<IntKey<int>, LStringRes*> StrRef;
	LHashTbl<IntKey<int>, LStringRes*> Strings;
	LHashTbl<IntKey<int>, LStringRes*> DlgStrings;

	LResourcesPrivate()
	{
		Ok = false;
		Format = Lr8File;
	}

	~LResourcesPrivate()
	{
		StrRef.DeleteObjects();
	}
};

/// A collection of resources
/// \ingroup Resources
class LResourceContainer : public LArray<LResources*>
{
public:
	~LResourceContainer()
	{
		DeleteObjects();
	}
};

static LResourceContainer ResourceOwner;
bool LResources::LoadStyles = false;

LResources::LResources(const char *FileName, bool Warn, const char *ThemeFolder)
{
	d = new LResourcesPrivate;
	ScriptEngine = 0;

	// global pointer list
	ResourceOwner.Add(this);

	LString File;
	LString FullPath;

#if DEBUG_RES_FILE
LgiTrace("%s:%i - Filename='%s'\n", _FL, FileName);
#endif
	
	if (LFileExists(FileName))
	{
		if (Load(FileName))
			return;
	}

	if (FileName)
	{
		// We're given the file name, and we should find the path.
		const char *f = strrchr(FileName, DIR_CHAR);

#if DEBUG_RES_FILE
LgiTrace("%s:%i - f='%s'\n", _FL, f);
#endif

		File = f ? f + 1 : FileName;

#if DEBUG_RES_FILE
LgiTrace("%s:%i - File='%s'\n", _FL, File.Get());
#endif
	}
	else
	{
		// Need to look up the file associated by name with the current exe
		auto Exe = LGetExeFile();
		if (Exe)
		{
			#if DEBUG_RES_FILE
			LgiTrace("%s:%i - Str='%s'\n", _FL, Exe.Get());
			#endif
			auto p = Exe.SplitDelimit(DIR_STR);
			File = p.Last();

			#if defined WINDOWS
				auto DotExe = File.RFind(".exe");
				if (DotExe >= 0)
					File.Length(DotExe);
			#elif defined MAC
				auto DotApp = File.RFind(".app");
				if (DotApp >= 0)
					File.Length(DotApp);
			#endif

			#if DEBUG_RES_FILE
			LgiTrace("%s:%i - File='%s'\n", _FL, File.Get());
			#endif
		}
		else
		{
			LgiMsg(0, LLoadString(L_ERROR_RES_NO_EXE_PATH,
									"Fatal error: Couldn't get the path of the running\nexecutable. Can't find resource file."),
									"LResources::LResources");
			LgiTrace("%s:%i - Fatal error: Couldn't get the path of the running\nexecutable. Can't find resource file.", _FL);
			LExitApp();
		}
	}

	#if DEBUG_RES_FILE
	LgiTrace("%s:%i - File='%s'\n", _FL, File.Get());
	#endif

	LString BaseFile = File;
	LString AltFile = File.Replace(".");
	BaseFile += ".lr8";
	AltFile += ".lr8";

	#if DEBUG_RES_FILE
	LgiTrace("%s:%i - File='%s'\n", _FL, BaseFile.Get());
	#endif

	FullPath = LFindFile(BaseFile);
	if (!FullPath)
		FullPath = LFindFile(AltFile);

	#if DEBUG_RES_FILE
	LgiTrace("%s:%i - FullPath='%s'\n", _FL, FullPath.Get());
	#endif

	if (FullPath)
	{
		Load(FullPath);

		if (ThemeFolder)
			SetThemeFolder(ThemeFolder);
	}
	else
	{
		char Msg[256];
		auto Exe = LGetExeFile();

		// Prepare data
		sprintf_s(Msg, sizeof(Msg),
				LLoadString(L_ERROR_RES_NO_LR8_FILE,
								"Couldn't find the file '%s' required to run this application\n(Exe='%s')"),
				BaseFile.Get(),
				Exe.Get());

		// Dialog
		printf("%s\n", Msg);
		if (Warn)
		{
			LgiMsg(0, Msg, "LResources::LResources");

			// Exit
			LExitApp();
		}
	}
}

LResources::~LResources()
{
	ResourceOwner.Delete(this);
	LanguageNames.DeleteArrays();
	
	Dialogs.DeleteObjects();
	Menus.DeleteObjects();
	DeleteObj(d);
}

const char *LResources::GetThemeFolder()
{
	return d->ThemeFolder;
}

bool LResources::DefaultColours = true;
void LResources::SetThemeFolder(const char *f)
{
	d->ThemeFolder = f;

	LFile::Path Colours(f, "colours.json");
	if (Colours.Exists())
		DefaultColours = !LColourLoad(Colours.GetFull());

	LFile::Path Css(f, "styles.css");
	if (Css.Exists())
	{
		LFile in(Css, O_READ);
		if (in.IsOpen())
		{
			auto s = in.Read();
			const char *css = s;
			CssStore.Parse(css);
		}
	}		
}

bool LResources::StyleElement(LViewI *v)
{
	if (!v) return false;
	if (!LoadStyles) return true;

	LCss::SelArray Selectors;
	for (auto r: ResourceOwner)
	{
		LViewCssCb Ctx;
		r->CssStore.Match(Selectors, &Ctx, v);
	}
	
	LCss *Css = v->GetCss(true);
	for (auto *Sel: Selectors)
	{
		const char *Defs = Sel->Style;
		if (Css && Defs)
			Css->Parse(Defs, LCss::ParseRelaxed);
	}

	auto ElemStyles = v->CssStyles();
	if (ElemStyles)
	{
		const char *Defs = ElemStyles;
		Css->Parse(Defs, LCss::ParseRelaxed);
	}
	
	return true;
}

ResFileFormat LResources::GetFormat()
{
	return d->Format;
}

bool LResources::IsOk()
{
	return d->Ok;
}

void LResources::AddLang(LLanguageId id)
{
	// search through id's...
	for (int i=0; i<Languages.Length(); i++)
	{
		if (stricmp(Languages[i], id) == 0)
			// already in the list
			return;
	}

	Languages.Add(id);
}

char *LResources::GetFileName()
{
	return d->File;
}

bool LResources::Load(const char *FileName)
{
	if (!FileName)
	{
		LgiTrace("%s:%i - No filename.\n", _FL);
		LAssert(0);
		return false;
	}

	LFile F;
	if (!F.Open(FileName, O_READ))
	{
		LgiTrace("%s:%i - Couldn't open '%s'.\n", _FL, FileName);
		LAssert(0);
		return false;
	}

	d->File = FileName;
	d->Format = Lr8File;
	char *Ext = LGetExtension(FileName);
	if (Ext && stricmp(Ext, "lr") == 0)
	{
		d->Format = CodepageFile;
	}
	else if (Ext && stricmp(Ext, "xml") == 0)
	{
		d->Format = XmlFile;
	}

	LStringRes::CurLang = LGetLanguageId();
	if (d->Format != CodepageFile)
	{
		LStringRes::CodePage = 0;
	}
	else if (LStringRes::CurLang)
	{
		LStringRes::CodePage = LStringRes::CurLang->Charset;
	}

	LXmlTree x(0);
	LAutoPtr<LXmlTag> Root(new LXmlTag);
	if (!x.Read(Root, &F, 0))
	{
		LgiTrace("%s:%i - ParseXmlFile failed: %s\n", _FL, x.GetErrorMsg());
		// LAssert(0);
		return false;
	}
	
	if (Root->Children.Length() == 0)
		return false;

	for (auto It = Root->Children.begin(); It != Root->Children.end(); )
	{
		auto t = *It;
		if (t->IsTag("string-group"))
		{
			bool IsString = true;
			char *Name = 0;
			if ((Name = t->GetAttr("Name")))
			{
				IsString = stricmp("_Dialog Symbols_", Name) != 0;
			}

			for (auto c: t->Children)
			{
				LStringRes *s = new LStringRes(this);
				if (s && s->Read(c, d->Format))
				{
					// This code saves the names of the languages if specified in the LR8 files.
					char *Def = c->GetAttr("define");
					if (Def && !stricmp(Def, "IDS_LANGUAGE"))
					{
						for (int i=0; i<c->Attr.Length(); i++)
						{
							LXmlAttr &a = c->Attr[i];
							LanguageNames.Add(a.GetName(), NewStr(a.GetValue()));
						}
					}

					// Save the string for later.
					d->StrRef.Add(s->Ref, s);
					if (s->Id != -1)
					{
					    if (IsString)
						    d->Strings.Add(s->Id, s);
					    else
						    d->DlgStrings.Add(s->Id, s);
					}
					d->Ok = true;
				}
				else
				{
					LgiTrace("%s:%i - string read failed\n", _FL);
					DeleteObj(s);
				}
			}
		}
		else if (t->IsTag("dialog"))
		{
			LDialogRes *n = new LDialogRes(this);
			if (n && n->Read(t, d->Format))
			{
				Dialogs.Insert(n);
				d->Ok = true;

				// This allows the menu to take ownership of the XML tag
				t->RemoveTag();
				t = NULL;
			}
			else
			{
				LgiTrace("%s:%i - dialog read failed\n", _FL);
				DeleteObj(n);
			}
		}
		else if (t->IsTag("menu"))
		{
			LMenuRes *m = new LMenuRes(this);
			if (m && m->Read(t, d->Format))
			{
				Menus.Insert(m);
				d->Ok = true;

				// This allows the menu to take ownership of the XML tag
				t->RemoveTag();
				t = NULL;
			}
			else
			{
				LgiTrace("%s:%i - menu read failed\n", _FL);
				DeleteObj(m);
			}
		}
		else if (t->IsTag("style"))
		{
			const char *c = t->GetContent();
			CssStore.Parse(c);
		}

		if (t)
			It++;
	}

	return true;
}

LStringRes *LResources::StrFromRef(int Ref)
{
	return d->StrRef.Find(Ref);
}

char *LResources::StringFromId(int Id)
{
	LStringRes *NotStr = 0;
	LStringRes *sr;

	if ((sr = d->Strings.Find(Id)))
		return sr->Str;

	if ((sr = d->DlgStrings.Find(Id)))
		return sr->Str;

	return NotStr ? NotStr->Str : 0;
}

char *LResources::StringFromRef(int Ref)
{
	LStringRes *s = d->StrRef.Find(Ref);
	return s ? s->Str : 0;
}

#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/Button.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/TabView.h"
#include "lgi/common/Combo.h"
#include "lgi/common/Bitmap.h"
#include "lgi/common/Slider.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Tree.h"

class GMissingCtrl : public LLayout, public ResObject
{
    LAutoString n;

public:
    GMissingCtrl(char *name) : ResObject(Res_Custom)
    {
        n.Reset(NewStr(name));
    }    

    void OnPaint(LSurface *pDC)
    {
        LRect c = GetClient();
        pDC->Colour(LColour(0xcc, 0xcc, 0xcc));
        pDC->Rectangle();
        pDC->Colour(LColour(0xff, 0, 0));
        pDC->Line(c.x1, c.y1, c.x2, c.y2);
        pDC->Line(c.x2, c.y1, c.x1, c.y2);
        LDisplayString ds(LSysFont, n);
        LSysFont->Transparent(true);
        LSysFont->Fore(LColour(L_TEXT));
        ds.Draw(pDC, 3, 0);
    }
};

ResObject *LResources::CreateObject(LXmlTag *t, ResObject *Parent)
{
	ResObject *Wnd = 0;
	if (t && t->GetTag())
	{
		char *Control = 0;
		
		if (stricmp(t->GetTag(), Res_StaticText) == 0)
		{
			Wnd = new LTextLabel(0, 0, 0, -1, -1, NULL);
		}
		else if (stricmp(t->GetTag(), Res_EditBox) == 0)
		{
			Wnd = new LEdit(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_CheckBox) == 0)
		{
			Wnd = new LCheckBox(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_Button) == 0)
		{
			Wnd = new LButton(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_Group) == 0)
		{
			Wnd = new LRadioGroup(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_RadioBox) == 0)
		{
			Wnd = new LRadioButton(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_TabView) == 0)
		{
			LTabView *Tv = new LTabView(0, 10, 10, 100, 100, "LTabView");
			Wnd = Tv;
			if (Tv)
			{
				Tv->SetPourLargest(false);
				Tv->SetPourChildren(false);
			}
		}
		else if (stricmp(t->GetTag(), Res_Tab) == 0)
		{
			Wnd = new LTabPage(0);
		}
		else if (stricmp(t->GetTag(), Res_ListView) == 0)
		{
			LList *w;
			Wnd = w = new LList(0, 0, 0, -1, -1, "");
			if (w)
			{
				w->Sunken(true);
			}
		}
		else if (stricmp(t->GetTag(), Res_Column) == 0)
		{
			if (Parent)
			{
				LList *Lst = dynamic_cast<LList*>(Parent);

				LAssert(Lst != NULL);

				if (Lst)
				{
					Wnd = Lst->AddColumn("");
				}
			}
		}
		else if (stricmp(t->GetTag(), Res_ComboBox) == 0)
		{
			Wnd = new LCombo(0, 0, 0, 100, 20, "");
		}
		else if (stricmp(t->GetTag(), Res_Bitmap) == 0)
		{
			Wnd = new LBitmap(0, 0, 0, 0);
		}
		else if (stricmp(t->GetTag(), Res_Progress) == 0)
		{
			Wnd = new LProgressView(0, 0, 0, -1, -1, "");
		}
		else if (stricmp(t->GetTag(), Res_Slider) == 0)
		{
			Wnd = new LSlider(0, 0, 0, -1, -1, "", false);
		}
		else if (stricmp(t->GetTag(), Res_ScrollBar) == 0)
		{
			Wnd = new LScrollBar(0, 0, 0, 20, 100, "");
		}
		else if (stricmp(t->GetTag(), Res_Progress) == 0)
		{
			Wnd = new LProgressView(0, 0, 0, 1, 1, "");
		}
		else if (stricmp(t->GetTag(), Res_TreeView) == 0)
		{
			Wnd = new LTree(0, 0, 0, 1, 1, "");
		}
		else if (stricmp(t->GetTag(), Res_ControlTree) == 0)
		{
			LView *v = LViewFactory::Create("LControlTree");
			if (!(Wnd = dynamic_cast<ResObject*>(v)))
			{
				DeleteObj(v);
			}
		}
		else if (stricmp(t->GetTag(), Res_Custom) == 0)
		{
			Control = t->GetAttr("ctrl");
			LView *v = LViewFactory::Create(Control);

			if (!v)
			    v = new GMissingCtrl(Control);

			if (v)
			{
				Wnd = dynamic_cast<ResObject*>(v);
				if (!Wnd)
				{
					// Not a "ResObject"
					LAssert(!"Not a ResObject");
					DeleteObj(v);
				}
			}
		}
		else if (stricmp(t->GetTag(), Res_Table) == 0)
		{
			Wnd = new LTableLayout;
		}
	
		if (!Wnd)
		{
			printf(LLoadString(L_ERROR_RES_CREATE_OBJECT_FAILED,
								"LResources::CreateObject(%s) failed. (Ctrl=%s)\n"),
					t->GetTag(),
					Control);
		}
	}
	
	LAssert(Wnd != NULL);
	return Wnd;
}

void LResources::Res_SetPos(ResObject *Obj, int x1, int y1, int x2, int y2)
{
	LItemColumn *Col = dynamic_cast<LItemColumn*>(Obj);
	if (Col)
	{
		Col->Width(x2-x1);
	}
	else
	{
		LView *w = CastToGWnd(Obj);
		if (w)
		{
			LRect n(x1, y1, x2, y2);
			w->SetPos(n);
		}
	}
}

void LResources::Res_SetPos(ResObject *Obj, char *s)
{
	if (Obj && s)
	{
		GToken T(s, ",");
		if (T.Length() == 4)
		{
			Res_SetPos(Obj, atoi(T[0]), atoi(T[1]), atoi(T[2]), atoi(T[3]));
		}
	}
}

bool LResources::Res_GetProperties(ResObject *Obj, GDom *Props)
{
	// this is a read-only system...
	return false;
}

struct ResObjectCallback : public LCss::ElementCallback<ResObject>
{
	GDom *Props;
	LVariant v;
	
	ResObjectCallback(GDom *props)
	{
		Props = props;
	}

	const char *GetElement(ResObject *obj)
	{
		return obj->GetObjectName();
	}
	
	const char *GetAttr(ResObject *obj, const char *Attr)
	{
		if (!Props->GetValue(Attr, v))
			v.Empty();		
		return v.Str();
	}
	
	bool GetClasses(LString::Array &Classes, ResObject *obj)
	{
		if (Props->GetValue("class", v))
			Classes.New() = v.Str();
		return Classes.Length() > 0;
	}
	
	ResObject *GetParent(ResObject *obj)
	{
		LAssert(0);
		return NULL;
	}
	
	LArray<ResObject*> GetChildren(ResObject *obj)
	{
		LArray<ResObject*> a;
		return a;
	}
};

bool LResources::Res_SetProperties(ResObject *Obj, GDom *Props)
{
	LView *v = dynamic_cast<LView*>(Obj);
	if (!v || !Props)
		return false;

	LVariant i;
	if (Props->GetValue("enabled", i))
		v->Enabled(i.CastInt32() != 0);

	if (Props->GetValue("visible", i))
		v->Visible(i.CastInt32() != 0);

	if (Props->GetValue("style", i))
		v->CssStyles(i.Str());

	if (Props->GetValue("class", i))
	{
		LString::Array *a = v->CssClasses();
		if (a)
			(*a) = LString(i.Str()).SplitDelimit(" \t");
	}

	if (Props->GetValue("image", i))
		v->GetCss(true)->BackgroundImage(LCss::ImageDef(i.Str()));

	auto e = dynamic_cast<LEdit*>(v);
	if (e)
	{
		if (Props->GetValue("pw", i))
			e->Password(i.CastInt32() != 0);

		if (Props->GetValue("multiline", i))
			e->MultiLine(i.CastInt32() != 0);
	}

	auto b = dynamic_cast<LButton*>(v);
	if (b)
	{
		if (Props->GetValue("image", i))
			b->GetCss(true)->BackgroundImage(LCss::ImageDef(i.Str()));

		if (Props->GetValue("toggle", i))
			b->SetIsToggle(i.CastInt32()!=0);
	}

	return true;
}

LRect LResources::Res_GetPos(ResObject *Obj)
{
	LView *w = CastToGWnd(Obj);
	if (w)
	{
		return w->GetPos();
	}

	return LRect(0, 0, 0, 0);
}

int LResources::Res_GetStrRef(ResObject *Obj)
{
	return 0;
}

bool LResources::Res_SetStrRef(ResObject *Obj, int Ref, ResReadCtx *Ctx)
{
	LStringRes *s = d->StrRef.Find(Ref);
	if (!s)
		return false;

	if (Ctx && !Ctx->Check(s->Tag))
		return false;

	LView *w = CastToGWnd(Obj);
	if (w)
	{
		w->SetId(s->Id);
		if (ValidStr(s->Str))
		{
			w->Name(s->Str);
		}
	}
	else if (Obj)
	{
		LItemColumn *Col = dynamic_cast<LItemColumn*>(Obj);
		if (Col)
		{
			Col->Name(s->Str);
		}
		else
		{
			LTabPage *Page = dynamic_cast<LTabPage*>(Obj);
			if (Page)
			{
				Page->Name(s->Str);
			}
		}
	}

	return true;
}

void LResources::Res_Attach(ResObject *Obj, ResObject *Parent)
{
	LView *o = CastToGWnd(Obj);
	LView *p = CastToGWnd(Parent);
	LTabPage *Tab = dynamic_cast<LTabPage*>(Parent);
	if (o)
	{
		if (Tab)
		{
			if (Tab)
			{
				LRect r = o->GetPos();
				r.Offset(-4, -24);
				o->SetPos(r);

				Tab->Append(o);
			}
		}
		else if (p)
		{
			if (!p->IsAttached() ||
				!o->Attach(p))
			{
				p->AddView(o);
				o->SetParent(p);
			}
		}
		else
		{
			LAssert(p != NULL);
		}
	}
}

bool LResources::Res_GetChildren(ResObject *Obj, List<ResObject> *l, bool Deep)
{
	LView *o = CastToGWnd(Obj);
	if (o && l)
	{
		for (LViewI *w: o->IterateViews())
		{
			ResObject *n = dynamic_cast<ResObject*>(w);
			if (n)
				l->Insert(n);
		}
		return true;
	}
	return false;
}

void LResources::Res_Append(ResObject *Obj, ResObject *Parent)
{
	if (Obj && Parent)
	{
		LItemColumn *Col = dynamic_cast<LItemColumn*>(Obj);
		LList *Lst = dynamic_cast<LList*>(Parent);
		if (Lst && Col)
		{
			Lst->AddColumn(Col);
		}

		LTabView *Tab = dynamic_cast<LTabView*>(Obj);
		LTabPage *Page = dynamic_cast<LTabPage*>(Parent);
		if (Tab && Page)
		{
			Tab->Append(Page);
		}
	}
}

bool LResources::Res_GetItems(ResObject *Obj, List<ResObject> *l)
{
	if (Obj && l)
	{
		LList *Lst = dynamic_cast<LList*>(Obj);
		if (Lst)
		{
			for (int i=0; i<Lst->GetColumns(); i++)
			{
				l->Insert(Lst->ColumnAt(i));
			}
			return true;
		}

		LTabView *Tabs = dynamic_cast<LTabView*>(Obj);
		if (Tabs)
		{
			for (int i=0; i<Tabs->GetTabs(); i++)
			{
				l->Insert(Tabs->TabAt(i));
			}
			return true;
		}
	}
	return false;
}

GDom *LResources::Res_GetDom(ResObject *Obj)
{
	return dynamic_cast<GDom*>(Obj);
}

///////////////////////////////////////////////////////
LDialogRes::LDialogRes(LResources *res)
{
	Res = res;
	Dialog = 0;
	Str = 0;
}

LDialogRes::~LDialogRes()
{
	DeleteObj(Dialog);
}

bool LDialogRes::Read(LXmlTag *t, ResFileFormat Format)
{
	if ((Dialog = t))
	{
		char *n = 0;
		if ((n = Dialog->GetAttr("ref")))
		{
			int Ref = atoi(n);
			Str = Res->StrFromRef(Ref);
		}
		if ((n = Dialog->GetAttr("pos")))
		{
			GToken T(n, ",");
			if (T.Length() == 4)
			{
				Pos.x1 = atoi(T[0]);
				Pos.y1 = atoi(T[1]);
				Pos.x2 = atoi(T[2]);
				Pos.y2 = atoi(T[3]);
			}
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
LMenuRes::LMenuRes(LResources *res)
{
	Res = res;
	Tag = 0;
}

LMenuRes::~LMenuRes()
{
	Strings.DeleteObjects();
	DeleteObj(Tag);
}

bool LMenuRes::Read(LXmlTag *t, ResFileFormat Format)
{
	Tag = t;
	if (t && stricmp(t->GetTag(), "menu") == 0)
	{
		char *n;
		if ((n = t->GetAttr("name")))
		{
			LBase::Name(n);
		}

		for (auto c: t->Children)
		{
			if (stricmp(c->GetTag(), "string-group") == 0)
			{
				for (auto i: c->Children)
				{
					LStringRes *s = new LStringRes(Res);
					if (s && s->Read(i, Format))
					{
					    LAssert(!Strings.Find(s->Ref)); // If this fires the string has a dupe ref
						Strings.Add(s->Ref, s);
					}
					else
					{
						DeleteObj(s);
					}
				}
				break;
			}
		}

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Dialog
bool LResourceLoad::LoadFromResource(int Resource, LViewI *Parent, LRect *Pos, LAutoString *Name, char *TagList)
{
	LgiGetResObj();

	for (int i=0; i<ResourceOwner.Length(); i++)
	{		
		if (ResourceOwner[i]->LoadDialog(Resource, Parent, Pos, Name, 0, TagList))
			return true;
	}


	return false;
}

////////////////////////////////////////////////////////////////////////
// Get language
LLanguage *LGetLanguageId()
{
	// Check for command line override
	char Buf[64];
	if (LAppInst->GetOption("i", Buf))
	{
		LLanguage *i = GFindLang(Buf);
		if (i)
			return i;
	}

	// Check for config setting
	auto LangId = LAppInst->GetConfig("Language");
	if (LangId)
	{
		LLanguage *l = GFindLang(LangId);
		if (l)
			return l;
	}

	// Use a system setting
	#if defined WIN32

	int Lang = GetSystemDefaultLCID();

	TCHAR b[256];
	if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTLANGUAGE, b, CountOf(b)) > 0)
	{
		Lang = htoi(b);
	}

	int Primary = PRIMARYLANGID(Lang);
	int Sub = SUBLANGID(Lang);
	LLanguage *i, *English = 0;

	// Search for exact match
	for (i = GFindLang(NULL); i->Id; i++)
	{
		if (i->Win32Id == Lang)
			return i;
	}

	// Search for PRIMARYLANGID match
	for (i = GFindLang(NULL); i->Id; i++)
	{
		if (PRIMARYLANGID(i->Win32Id) == PRIMARYLANGID(Lang))
			return i;
	}

	#elif defined(LINUX) && !defined(LGI_SDL)

	GLibrary *WmLib = LAppInst->GetWindowManagerLib();
	if (WmLib)
	{
		Proc_LgiWmGetLanguage GetLanguage = (Proc_LgiWmGetLanguage) WmLib->GetAddress("LgiWmGetLanguage");
		
		char Lang[256];
		if (GetLanguage &&
			GetLanguage(Lang))
		{
			LLanguage *l = GFindLang(Lang);
			if (l)
				return l;
		}
	}

	return GFindLang("en");

	/*
	// Read in KDE language setting
	char Path[256];
	if (LgiGetSystemPath(LSP_HOME, Path, sizeof(Path)))
	{
		LMakePath(Path, sizeof(Path), Path, ".kde/share/config/kdeglobals");
		char *Txt = LReadTextFile(Path);

		if (Txt)
		{
			extern bool _GetIniField(char *Grp, char *Field, char *In, char *Out, int OutSize);
			char Lang[256] = "";
			LLanguage *Ret = 0;
			if (_GetIniField("Locale", "Language", Txt, Lang, sizeof(Lang)))
			{
				GToken Langs(Lang, ":,; \t");
				for (int i=0; i<Langs.Length(); i++)
				{
					if (Ret = GFindLang(Langs[i]))
					{
						break;
					}
				}
			}
			DeleteArray(Txt);
			if (Ret)
			{
				return Ret;
			}
		}
	}
	*/
	
	#endif

	// Return a default of English
	return GFindLang("en");
}

///////////////////////////////////////////////////////////////////////////////////////
// Load string
const char *LLoadString(int Res, const char *Default)
{
	char *s = 0;
	LResources *r = LgiGetResObj();
	if (r)
	{
		// use to test for non-resource strings
		// return "@@@@";
		s = r->StringFromId(Res);
	}
	
	if (s)
		return s;

	return Default;
}

bool LResources::LoadDialog(int Resource, LViewI *Parent, LRect *Pos, LAutoString *Name, LEventsI *Engine, char *TagList)
{
	bool Status = false;

	if (Resource)
	{
		ScriptEngine = Engine;
		TagHash Tags(TagList);

		for (auto Dlg: Dialogs)
		{
			if (Dlg->Id() == ((int) Resource))
			{
				// LProfile p("LoadDialog");
			
				// found the dialog to load, set properties
				if (Dlg->Name())
				{
					if (Name)
						Name->Reset(NewStr(Dlg->Name()));
					else if (Parent)
						Parent->Name(Dlg->Name());
				}
				
				// p.Add("1");
				int x = Dlg->X();
				int y = Dlg->Y();
				x += LAppInst->GetMetric(LGI_MET_DECOR_X) - 4;
				y += LAppInst->GetMetric(LGI_MET_DECOR_Y) - 18;
				if (Pos)
				{
					Pos->ZOff(x, y);

					// Do some scaling for the monitor's DPI
					LPoint Dpi;
					auto *Wnd = Parent ? Parent->GetWindow() : NULL;
					if (Wnd)
						Dpi = Wnd->GetDpi();
					else
					{
						LArray<GDisplayInfo*> Displays;
						if (LGetDisplays(Displays))
						{
							for (auto d: Displays)
							{
								if (d->r.Overlap(Pos) && d->Dpi.x)
									Dpi = d->Dpi;
							}
							Displays.DeleteObjects();
						}
					}
					if (Dpi.x)
					{
						LPointF s((double)Dpi.x / 96.0, (double)Dpi.y / 96.0);
						Pos->Dimension(	(int) (s.x * Pos->X()),
										(int) (s.y * Pos->Y()));
					}
				}
				else if (Parent && stricmp(Parent->GetClass(), "LTabPage"))
				{
					LRect r = Parent->GetPos();
					r.Dimension(x, y);
					Parent->SetPos(r);
				}

				// instantiate control list
				// p.Add("2");
				for (auto t: Dlg->Dialog->Children)
				{
					ResObject *Obj = CreateObject(t, 0);
					if (Obj)
					{
						// p.Add("3");
						if (Tags.Check(t))
						{
							if (Res_Read(Obj, t, Tags))
							{
								LView *w = dynamic_cast<LView*>(Obj);
								if (w)
									Parent->AddView(w);
							}
							else
							{
								LgiMsg(	NULL,
										LLoadString(	L_ERROR_RES_RESOURCE_READ_FAILED,
														"Resource read error, tag: %s"),
										"LResources::LoadDialog",
										MB_OK,
										t->GetTag());
								break;
							}
						}
					}
					else
					{
						LAssert(0);
					}
				}

				Status = true;
				break;
			}
		}
	}

	return Status;	
}

//////////////////////////////////////////////////////////////////////
// Menus
LStringRes *LMenuRes::GetString(LXmlTag *Tag)
{
	if (Tag)
	{
		char *n = Tag->GetAttr("ref");
		if (n)
		{
			int Ref = atoi(n);
			LStringRes *s = Strings.Find(Ref);
			if (s)
				return s;
		}
	}

	return 0;
}

bool LMenuLoader::Load(LMenuRes *MenuRes, LXmlTag *Tag, ResFileFormat Format, TagHash *TagList)
{
	bool Status = false;

	if (!Tag)
	{
		LgiTrace("%s:%i - No tag?", _FL);
		return false;
	}
	
	if (Tag->IsTag("menu"))
	{
		#if WINNATIVE
		if (!Info)
		{
			Info = ::CreateMenu();
		}
		#endif
	}

	#if WINNATIVE
	if (Info)
	#endif
	{
		Status = true;
		
		for (auto t: Tag->Children)
		{
			if (!Status)
				break;
			if (t->IsTag("submenu"))
			{
				LStringRes *Str = MenuRes->GetString(t);
				if (Str && Str->Str)
				{
					bool Add = !TagList || TagList->Check(Str->Tag);
					LSubMenu *Sub = AppendSub(Str->Str);
					if (Sub)
					{
						auto p = Sub->GetParent();
						if (p)
							p->Id(Str->Id);
						else
							LgiTrace("%s:%i - No Parent to set menu item ID.\n", _FL);
						
						Status = Sub->Load(MenuRes, t, Format, TagList);

						if (!Add)
						{
							// printf("Sub->GetParent()=%p this=%p\n", Sub->GetParent(), this);
							Sub->GetParent()->Remove();
							delete Sub->GetParent();
						}
					}
				}
				else
				{
					LAssert(0);
				}
			}
			else if (t->IsTag("menuitem"))
			{
				if (t->GetAsInt("sep") > 0)
				{
					AppendSeparator();
				}
				else
				{
					LStringRes *Str = MenuRes->GetString(t);
					if (Str && Str->Str)
					{
						if (!TagList || TagList->Check(Str->Tag))
						{
							int Enabled = t->GetAsInt("enabled");
							char *Shortcut = t->GetAttr("shortcut");
							Status = AppendItem(Str->Str, Str->Id, Enabled != 0, -1, Shortcut) != 0;
						}
						else Status = true;
					}
					else
					{
						LAssert(0);
					}
				}
			}
		}
	}

	return Status;
}

bool LMenu::Load(LView *w, const char *Res, const char *TagList)
{
	bool Status = false;

	LResources *r = LgiGetResObj();
	if (r)
	{
		TagHash Tags(TagList);
		for (auto m: r->Menus)
		{
			if (stricmp(m->Name(), Res) == 0)
			{
				#if WINNATIVE
				Status = LSubMenu::Load(m, m->Tag, r->GetFormat(), &Tags);
				#else
				Status = LMenuLoader::Load(m, m->Tag, r->GetFormat(), &Tags);
				#endif
				break;
			}
		}
	}

	return Status;
}

LResources *LgiGetResObj(bool Warn, const char *filename, bool LoadOnDemand)
{
	// Look for existing file?
	if (filename && LoadOnDemand)
	{
		for (auto r: ResourceOwner)
			if (!Stricmp(filename, r->GetFileName()))
				return r;

		// Load the new file...
		return new LResources(filename, Warn);
	}

	if (ResourceOwner.Length())
		return ResourceOwner[0];
	if (!LoadOnDemand)
		return NULL;

	return new LResources(filename, Warn);
}
