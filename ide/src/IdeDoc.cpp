#include <stdio.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/Net.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Edit.h"
#include "lgi/common/List.h"
#include "lgi/common/PopupList.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/EventTargetThread.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/Http.h"
#include "lgi/common/Menu.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/PopupNotification.h"

#include "LgiIde.h"
#include "ProjectNode.h"
#include "SpaceTabConv.h"
#include "DocEdit.h"
#include "IdeDocPrivate.h"
#include "ProjectBackend.h"
#include "resdefs.h"

const char *Untitled = "[untitled]";
// static const char *White = " \r\t\n";

#define USE_OLD_FIND_DEFN	1
#define POPUP_WIDTH			700 // px
#define POPUP_HEIGHT		350 // px

enum
{
	IDM_COPY_FILE = 1100,
	IDM_COPY_PATH,
	IDM_BROWSE
};

int FileNameSorter(LString *a, LString *b)
{
	char *A = strrchr(a->Get(), DIR_CHAR);
	char *B = strrchr(b->Get(), DIR_CHAR);
	return stricmp(A?A:a->Get(), B?B:b->Get());
}

EditTray::EditTray(LTextView3 *ctrl, IdeDoc *doc)
{
	Ctrl = ctrl;
	Doc = doc;
	Line = Col = 0;
	FuncBtn.ZOff(-1, -1);
	SymBtn.ZOff(-1, -1);
		
	int Ht = LSysFont->GetHeight() + 6;
	AddView(FileSearch   = new LEdit(IDC_FILE_SEARCH,   0, 0, EDIT_CTRL_WIDTH, Ht));
	AddView(FuncSearch   = new LEdit(IDC_METHOD_SEARCH, 0, 0, EDIT_CTRL_WIDTH, Ht));
	AddView(SymSearch    = new LEdit(IDC_SYMBOL_SEARCH, 0, 0, EDIT_CTRL_WIDTH, Ht));
}
	
EditTray::~EditTray()
{
}
	
void EditTray::GotoSearch(int CtrlId, char *InitialText)
{
	if (FileSearch && FileSearch->GetId() == CtrlId)
	{
		FileSearch->Name(InitialText);
		FileSearch->Focus(true);
		if (InitialText)
			FileSearch->SendNotify(LNotifyDocChanged);
	}
		
	if (FuncSearch && FuncSearch->GetId() == CtrlId)
	{
		FuncSearch->Name(InitialText);
		FuncSearch->Focus(true);
		if (InitialText)
			FuncSearch->SendNotify(LNotifyDocChanged);
	}

	if (SymSearch && SymSearch->GetId() == CtrlId)
	{
		SymSearch->Name(InitialText);
		SymSearch->Focus(true);
		if (InitialText)
			SymSearch->SendNotify(LNotifyDocChanged);
	}
}
	
void EditTray::OnCreate()
{
	AttachChildren();
}

int MeasureText(const char *s)
{
	LDisplayString Ds(LSysFont, s);
	return Ds.X();
}

#define HEADER_BTN_LABEL	"h"
#define FUNCTION_BTN_LABEL	"{ }"
#define SYMBOL_BTN_LABEL	"s"
	
void EditTray::OnPosChange()
{
	LLayoutRect c(this, 2);

	c.Left(FileBtn, MeasureText(HEADER_BTN_LABEL)+10);
	if (FileSearch)
		c.Left(FileSearch, EDIT_CTRL_WIDTH);
	c.x1 += 8;

	c.Left(FuncBtn, MeasureText(FUNCTION_BTN_LABEL)+10);
	if (FuncSearch)
		c.Left(FuncSearch, EDIT_CTRL_WIDTH);
	c.x1 += 8;

	c.Left(SymBtn, MeasureText(SYMBOL_BTN_LABEL)+10);
	if (SymSearch)
		c.Left(SymSearch, EDIT_CTRL_WIDTH);
	c.x1 += 8;

	c.Remaining(TextMsg);
}
	
void EditTray::OnPaint(LSurface *pDC)
{
	LRect c = GetClient();
	pDC->Colour(L_MED);
	pDC->Rectangle();
	LSysFont->Colour(L_TEXT, L_MED);
	LSysFont->Transparent(true);
		
	LString s;
	s.Printf("Cursor: %i,%i", Col, Line + 1);
	{
		LDisplayString ds(LSysFont, s);
		ds.Draw(pDC, TextMsg.x1, TextMsg.y1 + ((c.Y()-TextMsg.Y())/2), &TextMsg);
	}

	LRect f = FileBtn;
	LThinBorder(pDC, f, DefaultRaisedEdge);
	{
		LDisplayString ds(LSysFont, HEADER_BTN_LABEL);
		ds.Draw(pDC, f.x1 + 4, f.y1);
	}

	f = FuncBtn;
	LThinBorder(pDC, f, DefaultRaisedEdge);
	{
		LDisplayString ds(LSysFont, FUNCTION_BTN_LABEL);
		ds.Draw(pDC, f.x1 + 4, f.y1);
	}

	f = SymBtn;
	LThinBorder(pDC, f, DefaultRaisedEdge);
	{
		LDisplayString ds(LSysFont, SYMBOL_BTN_LABEL);
		ds.Draw(pDC, f.x1 + 4, f.y1);
	}
}

bool EditTray::Pour(LRegion &r)
{
	LRect *c = FindLargest(r);
	if (c)
	{
		LRect n = *c;
		SetPos(n);		
		return true;
	}
	return false;
}

void EditTray::OnHeaderList(LMouse &m)
{
	// Header list button
	LString::Array Paths;
	if (Doc->BuildIncludePaths(Paths, PlatformCurrent, false))
	{
		LString::Array Headers;
		if (Doc->BuildHeaderList(Ctrl->NameW(), Headers, Paths))
		{
			// Sort them..
			Headers.Sort(FileNameSorter);
			
			LSubMenu *s = new LSubMenu;
			if (s)
			{
				// Construct the menu
				LHashTbl<StrKey<char>, int> Map;
				int DisplayLines = GdcD->Y() / LSysFont->GetHeight();
				if (Headers.Length() > (0.9 * DisplayLines))
				{
					LArray<char*> Letters[26];
					LArray<char*> Other;
					
					for (int i=0; i<Headers.Length(); i++)
					{
						char *h = Headers[i];
						char *f = LGetLeaf(h);
						
						Map.Add(h, i + 1);
						
						if (IsAlpha(*f))
						{
							int Idx = tolower(*f) - 'a';
							Letters[Idx].Add(h);
						}
						else
						{
							Other.Add(h);
						}
					}
					
					for (int i=0; i<CountOf(Letters); i++)
					{
						if (Letters[i].Length() > 1)
						{
							char *First = LGetLeaf(Letters[i][0]);
							char *Last = LGetLeaf(Letters[i].Last());

							char Title[256];
							sprintf_s(Title, sizeof(Title), "%s - %s", First, Last);
							LSubMenu *sub = s->AppendSub(Title);
							if (sub)
							{
								for (int n=0; n<Letters[i].Length(); n++)
								{
									char *h = Letters[i][n];
									int Id = Map.Find(h);
									LAssert(Id > 0);
									sub->AppendItem(LGetLeaf(h), Id, true);
								}
							}
						}
						else if (Letters[i].Length() == 1)
						{
							char *h = Letters[i][0];
							int Id = Map.Find(h);
							LAssert(Id > 0);
							s->AppendItem(LGetLeaf(h), Id, true);
						}
					}

					if (Other.Length() > 0)
					{
						for (int n=0; n<Other.Length(); n++)
						{
							char *h = Other[n];
							int Id = Map.Find(h);
							LAssert(Id > 0);
							s->AppendItem(LGetLeaf(h), Id, true);
						}
					}
				}
				else
				{
					for (int i=0; i<Headers.Length(); i++)
					{
						char *h = Headers[i];
						// char *f = LGetLeaf(h);
						
						Map.Add(h, i + 1);
					}

					for (int i=0; i<Headers.Length(); i++)
					{
						char *h = Headers[i];
						int Id = Map.Find(h);
						if (Id > 0)
							s->AppendItem(LGetLeaf(h), Id, true);
						else
							LgiTrace("%s:%i - Failed to get id for '%s' (map.len=%i)\n", _FL, h, Map.Length());
					}

					if (!Headers.Length())
					{
						s->AppendItem("(none)", 0, false);
					}
				}
				
				// Show the menu
				LPoint p(m.x, m.y);
				PointToScreen(p);
				int Goto = s->Float(this, p.x, p.y, true);
				if (Goto > 0)
				{
					char *File = Headers[Goto-1];
					if (File)
					{
						// Open the selected file
						Doc->GetProject()->GetApp()->OpenFile(File, NULL, NULL);
					}
				}
				
				DeleteObj(s);
			}
		}
		
		// Clean up memory
		Headers.DeleteArrays();
	}
	else
	{
		LgiTrace("%s:%i - No include paths set.\n", _FL);
	}
}

void EditTray::OnFunctionList(LMouse &m)
{
	LArray<DefnInfo> Funcs;

	if (BuildDefnList(Doc->GetFileName(), (char16*)Ctrl->NameW(), Funcs, DefnNone /*DefnFunc | DefnClass*/))
	{
		LSubMenu s;
		LArray<DefnInfo*> a;					
		
		int ScreenHt = GdcD->Y();
		int ScreenLines = ScreenHt / LSysFont->GetHeight();
		float Ratio = ScreenHt ? (float)(LSysFont->GetHeight() * Funcs.Length()) / ScreenHt : 0.0f;
		bool UseSubMenus = Ratio > 0.9f;
		int Buckets = UseSubMenus ? (int)(ScreenLines * 0.9) : 1;
		int BucketSize = MAX(2, (int)Funcs.Length() / Buckets);
		LSubMenu *Cur = NULL;
		
		for (unsigned n=0; n<Funcs.Length(); n++)
		{
			DefnInfo *i = &Funcs[n];
			char Buf[256], *o = Buf;
			
			if (i->Type != DefnEnumValue)
			{
				for (char *k = i->Name; *k && o < Buf+sizeof(Buf)-8; k++)
				{
					if (*k == '&')
					{
						*o++ = '&';
						*o++ = '&';
					}
					else if (*k == '\t')
					{
						*o++ = ' ';
					}
					else
					{
						*o++ = *k;
					}
				}
				*o++ = 0;
				
				a[n] = i;

				if (UseSubMenus)
				{
					if (!Cur || n % BucketSize == 0)
					{
						LString SubMsg;
						SubMsg.Printf("%s...", Buf);
						Cur = s.AppendSub(SubMsg);
					}
					if (Cur)
						Cur->AppendItem(Buf, n+1, true);
				}
				else
				{
					s.AppendItem(Buf, n+1, true);
				}
			}
		}
		
		LPoint p(m.x, m.y);
		PointToScreen(p);
		auto Goto = s.Float(this, p.x, p.y, true);
		if (Goto)
		{
			if (auto Info = a[Goto-1])
				Ctrl->SetLine(Info->Line);
		}
	}
	else
	{
		LgiTrace("%s:%i - No functions in input.\n", _FL);
	}
}

void EditTray::OnSymbolList(LMouse &m)
{
	LAutoString s(Ctrl->GetSelection());
	if (s)
	{
		LAutoWString sw(Utf8ToWide(s));
		if (sw)
		{
			#if USE_OLD_FIND_DEFN
			List<DefnInfo> Matches;
			Doc->FindDefn(sw, Ctrl->NameW(), Matches);
			#else
			LArray<FindSymResult> Matches;
			Doc->GetApp()->FindSymbol(s, Matches);
			#endif

			LSubMenu *s = new LSubMenu;
			if (s)
			{
				// Construct the menu
				int n=1;
				
				#if USE_OLD_FIND_DEFN
				for (auto Def: Matches)
				{
					char m[512];
					char *d = strrchr(Def->File, DIR_CHAR);
					snprintf(m, sizeof(m), "%s (%s:%i)", Def->Name.Get(), d ? d + 1 : Def->File.Get(), Def->Line);
					s->AppendItem(m, n++, true);
				}
				#else
				for (int i=0; i<Matches.Length(); i++)
				{
					FindSymResult &Res = Matches[i];
					char m[512];
					char *d = strrchr(Res.File, DIR_CHAR);
					sprintf(m, "%s (%s:%i)", Res.Symbol.Get(), d ? d + 1 : Res.File.Get(), Res.Line);
					s->AppendItem(m, n++, true);
				}
				#endif

				if (!Matches.Length())
				{
					s->AppendItem("(none)", 0, false);
				}
				
				// Show the menu
				LPoint p(m.x, m.y);
				PointToScreen(p);
				int Goto = s->Float(this, p.x, p.y, true);
				if (Goto)
				{
					#if USE_OLD_FIND_DEFN
					DefnInfo *Def = Matches[Goto-1];
					#else
					FindSymResult *Def = &Matches[Goto-1];
					#endif
					{
						// Open the selected symbol
						if (Doc->GetProject() &&
							Doc->GetProject()->GetApp())
						{
							AppWnd *App = Doc->GetProject()->GetApp();
							App->OpenFile(Def->File, NULL, [this, File=LString(Def->File), Line=Def->Line](auto Doc)
							{
								if (Doc)
									Doc->SetLine(Line, false);
								else
									LgiTrace("%s:%i - Couldn't open doc '%s'\n", _FL, File.Get());
							});
						}
						else
						{
							LgiTrace("%s:%i - No project / app ptr.\n", _FL);
						}
					}
				}
				
				DeleteObj(s);
			}
		}
	}
	else
	{
		LSubMenu *s = new LSubMenu;
		if (s)
		{
			s->AppendItem("(No symbol currently selected)", 0, false);
			LPoint p(m.x, m.y);
			PointToScreen(p);
			s->Float(this, p.x, p.y, true);
			DeleteObj(s);
		}
	}
}

void EditTray::OnMouseClick(LMouse &m)
{
	if (m.Left() && m.Down())
	{
		if (FileBtn.Overlap(m.x, m.y))
		{
			OnHeaderList(m);
		}
		else if (FuncBtn.Overlap(m.x, m.y))
		{
			OnFunctionList(m);
		}
		else if (SymBtn.Overlap(m.x, m.y))
		{
			OnSymbolList(m);
		}
	}
}

class ProjMethodPopup : public LPopupList<DefnInfo>
{
	AppWnd *App;

public:
	LArray<DefnInfo> All;

	ProjMethodPopup(AppWnd *app, LViewI *target) : LPopupList(target, PopupAbove, POPUP_WIDTH)
	{
		App = app;
	}

	LString ToString(DefnInfo *Obj)
	{
		return Obj->Name;
	}
	
	void OnSelect(DefnInfo *Obj)
	{
		App->GotoReference(Obj->File, Obj->Line, false, true, NULL);
	}
	
	bool Name(const char *s)
	{
		LString InputStr = s;
		LString::Array p = InputStr.SplitDelimit(" \t");
		
		LArray<DefnInfo*> Matching;
		for (unsigned i=0; i<All.Length(); i++)
		{
			DefnInfo *Def = &All[i];
			
			bool Match = true;
			for (unsigned n=0; n<p.Length(); n++)
			{
				if (!stristr(Def->Name, p[n]))
				{
					Match = false;
					break;
				}
			}
			
			if (Match)
				Matching.Add(Def);
		}
		
		return SetItems(Matching);
	}
	
	int OnNotify(LViewI *Ctrl, LNotification &n) override
	{
		if (Lst &&
			Ctrl == Edit &&
			(n.Type == LNotifyValueChanged || n.Type == LNotifyDocChanged))
		{
			Name(Edit->Name());
		}
		
		return LPopupList<DefnInfo>::OnNotify(Ctrl, n);
	}
};

class ProjSymPopup : public LPopupList<FindSymResult>
{
	AppWnd *App;
	IdeDoc *Doc;
	int CommonPathLen;

public:
	LArray<FindSymResult*> All;

	ProjSymPopup(AppWnd *app, IdeDoc *doc, LViewI *target) : LPopupList(target, PopupAbove, POPUP_WIDTH)
	{
		App = app;
		Doc = doc;
		CommonPathLen = 0;
	}
	
	void FindCommonPathLength()
	{
		LString s;
		
		for (unsigned i=0; i<All.Length(); i++)
		{
			if (i)
			{
				char *a = s.Get();
				char *a_end = strrchr(a, DIR_CHAR);

				char *b = All[i]->File.Get();
				char *b_end = strrchr(b, DIR_CHAR);

				int Common = 0;
				while (	*a && a <= a_end
						&& 
						*b && b <= b_end
						&&
						ToLower(*a) == ToLower(*b))
				{
					Common++;
					a++;
					b++;
				}
				if (i == 1)
					CommonPathLen = Common;
				else
					CommonPathLen = MIN(CommonPathLen, Common);
			}
			else s = All[i]->File;
		}
	}

	LString ToString(FindSymResult *Obj)
	{
		LString s;
		s.Printf("%s:%i - %s",
				CommonPathLen < Obj->File.Length()
				?
				Obj->File.Get() + CommonPathLen
				:
				Obj->File.Get(),
				Obj->Line,
				Obj->Symbol.Get());
		return s;
	}
	
	void OnSelect(FindSymResult *Obj)
	{
		App->GotoReference(Obj->File, Obj->Line, false, true, NULL);
	}
	
	int OnNotify(LViewI *Ctrl, LNotification &n) override
	{
		if (Lst &&
			Ctrl == Edit &&
			(n.Type == LNotifyValueChanged || n.Type == LNotifyDocChanged))
		{
			// Kick off search...
			LString s = Ctrl->Name();
			s = s.Strip();
			if (s.Length() > 2)
				App->FindSymbol(Doc->AddDispatch(), s);
		}
		
		return LPopupList<FindSymResult>::OnNotify(Ctrl, n);
	}
};

#if 0
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

void FilterFiles(LArray<ProjectNode*> &Perfect, LArray<ProjectNode*> &Nodes, LString InputStr, int Platforms)
{
	LString::Array p = InputStr.SplitDelimit(" \t");
		
	auto InputLen = InputStr.RFind(".");
	if (InputLen < 0)
		InputLen = InputStr.Length();

	LOG("%s:%i - InputStr='%s'\n", _FL, InputStr.Get());

	LArray<ProjectNode*> Partial;
	auto Start = LCurrentTime();
	for (unsigned i=0; i<Nodes.Length(); i++)
	{
		if (i % 100 == 0 && LCurrentTime() - Start > 400)
		{
			break;
		}

		ProjectNode *Pn = Nodes[i];
		if (! (Pn->GetPlatforms() & Platforms) )
			continue;

		LString Fn = Pn->GetFileName();
		if (Fn)
			Fn = Fn.Replace("\\", "/"); // Normalize the path to unix slashes.		
		
		bool debug = Stristr(Fn.Get(), "File.cpp") != NULL;
		if (!Fn)
		{
			// Mostly folders... ignore.
		}
		else
		{
			const char *Dir = strrchr(Fn, '/');
			auto Leaf = Dir ? Dir : Fn.Get();

			bool Match = true;
			for (unsigned n=0; n<p.Length(); n++)
			{
				auto s = stristr(Leaf, p[n]);
				
				if (debug)
				{
					LOG("....'%s' '%s' = %p\n", Leaf, p[n].Get(), s);
				}
			
				if (!s)
				{
					Match = false;
					break;
				}
			}
			if (Match)
			{
				bool PerfectMatch = false;
				auto Leaf = LGetLeaf(Fn);
				if (Leaf)
				{
					auto Dot = strrchr(Leaf, '.');
					if (Dot)
					{
						auto Len = Dot - Leaf;
						PerfectMatch =	Len == InputLen &&
										strncmp(InputStr, Leaf, Len) == 0;
					}
					else
					{
						PerfectMatch = stricmp(InputStr, Leaf);
					}
				}

				if (PerfectMatch)
					Perfect.Add(Pn);
				else
					Partial.Add(Pn);
			}
		}
	}

	Perfect.Add(Partial);
}

class ProjFilePopup : public LPopupList<ProjectNode>
{
	AppWnd *App;
	LString::Array SysInc;
	LString::Array SysHeaders;
	LAutoPtr<SysIncThread> Thread;

public:
	LArray<ProjectNode*> Nodes;

	ProjFilePopup(AppWnd *app, LViewI *target) : LPopupList(target, PopupAbove, POPUP_WIDTH)
	{
		App = app;
	}

	LString ToString(ProjectNode *Obj)
	{
		LString s(Obj->GetFileName());
		#ifdef WINDOWS
		return s.Replace("/", "\\");
		#else
		return s.Replace("\\", "/");
		#endif
	}
	
	void SetSysInc(LString::Array sysInc)
	{
		SysInc = sysInc;
		if (SysHeaders.Length() != 0)
			return;

		Thread.Reset(new SysIncThread(App, App->RootProject(), [this](auto hdrs)
		{
			SysHeaders.Swap(*hdrs);

			auto s = Edit->Name();
			if (ValidStr(s))
				Update(s);
		}));
	}
	
	void OnSelect(ProjectNode *Obj)
	{
		auto Fn = Obj->GetFileName();
		if (LIsRelativePath(Fn))
		{
			auto Proj = Obj->GetProject();
			auto Base = Proj->GetBasePath();
			LFile::Path p(Base);
			p += Fn;
			App->GotoReference(p, 1, false, true, NULL);
		}
		else
		{
			App->GotoReference(Fn, 1, false, true, NULL);
		}
	}
	
	void OnSelect(LListItem *Item)
	{
		App->GotoReference(Item->GetText(), 1, false, true, NULL);
	}
	
	void Update(LString InputStr)
	{
		auto Start = LCurrentTime();
		auto Proj = App->RootProject();

		if (auto backend = Proj ? Proj->GetBackend() : NULL)
		{
			if (InputStr.Length() < 3)
				return;

			backend->SearchFileNames(InputStr, [this, hnd=Lst->AddDispatch()](auto &results)
				{
					// This prevents the callback crashing if the list has been deleted 
					// between the SearchFileNames and the results coming back.
					if (!LEventSinkMap::Dispatch.IsSink(hnd))
						return;

					List<LListItem> Items;
					for (auto r: results)
					{
						Items.Insert(new LListItem(r));
						if (Items.Length() > 200)
							break;
					}
					Lst->Empty();
					Lst->Insert(Items);
					OnChange();
				});
		}
		else
		{
			// Local only project....
			LArray<ProjectNode*> Matches;
			FilterFiles(Matches, Nodes, InputStr, App->GetPlatform());
			SetItems(Matches);		
		
			List<LListItem> Items;
			for (auto &hdr: SysHeaders)
			{
				if (Lst && hdr.Find(InputStr) >= 0)
					Items.Insert(new LListItem(hdr));

				if (Items.Length() % 50 == 0 &&
					LCurrentTime() - Start > 500)
					break;
			}		
			Lst->Insert(Items);
		}
	}
	
	int OnNotify(LViewI *Ctrl, LNotification &n) override
	{
		if (Lst &&
			Ctrl == Edit &&
			(n.Type == LNotifyValueChanged || n.Type == LNotifyDocChanged))
		{
			auto s = Ctrl->Name();
			if (ValidStr(s))
				Update(s);
		}
		
		return LPopupList<ProjectNode>::OnNotify(Ctrl, n);
	}
};

class LStyleThread : public LEventTargetThread
{
public:
	LStyleThread() : LEventTargetThread("StyleThread")
	{
	}
	
	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->Msg())
		{
		}
		
		return 0;
	}
	
}	StyleThread;

////////////////////////////////////////////////////////////////////////////////////////////
IdeDocPrivate::IdeDocPrivate(IdeDoc *d, AppWnd *a, NodeSource *src, const char *file) : NodeView(src), LMutex("IdeDocPrivate.Lock")
{
	App = a;
	Doc = d;
	FileName = file;
	
	LFontType Font, *Use = NULL;
	if (Font.Serialize(App->GetOptions(), OPT_EditorFont, false))
	{
		Use = &Font;
	}		
	
	Doc->AddView(Edit = new DocEdit(Doc, Use));
	Doc->AddView(Tray = new EditTray(Edit, Doc));
}

void IdeDocPrivate::OnDelete()
{
	IdeDoc *Temp = Doc;
	DeleteObj(Temp);
}

void IdeDocPrivate::UpdateName()
{
	char n[MAX_PATH_LEN+30];
	
	LString Dsp = GetDisplayName();
	char *File = Dsp;
	#if MDI_TAB_STYLE
	char *Dir = File ? strrchr(File, DIR_CHAR) : NULL;
	if (Dir) File = Dir + 1;
	#endif
	
	strcpy_s(n, sizeof(n), File ? LGetLeaf(File) : Untitled);
	if (Edit->IsDirty())
	{
		strcat(n, " (changed)");
	}	
	Doc->Name(n);
}

LString IdeDocPrivate::GetDisplayName()
{
	if (nSrc)
	{
		auto Fn = nSrc->GetFileName();
		if (Fn)
		{
			if (stristr(Fn, "://"))
			{
				LUri u(nSrc->GetFileName());
				if (u.sPass)
				{
					u.sPass = "******";
				}
				
				return u.ToString();
			}
			else if (*Fn == '.')
			{
				return nSrc->GetFullPath();
			}
		}

		return LString(Fn);
	}

	return LString(FileName);
}

bool IdeDocPrivate::IsFile(const char *File)
{
	LString Mem;
	char *f = NULL;
	
	if (nSrc)
	{
		Mem = nSrc->GetFullPath();
		f = Mem;
	}
	else
	{
		f = FileName;
	}

	if (!f)
		return false;

	LToken doc(f, DIR_STR);
	LToken in(File, DIR_STR);
	ssize_t in_pos = (ssize_t)in.Length() - 1;
	ssize_t doc_pos = (ssize_t)doc.Length() - 1;
	while (in_pos >= 0 && doc_pos >= 0)
	{
		char *i = in[in_pos--];
		char *d = doc[doc_pos--];
		if (!i || !d)
		{
			return false;
		}
		
		if (!strcmp(i, ".") ||
			!strcmp(i, ".."))
		{
			continue;
		}
		
		if (stricmp(i, d))
		{
			return false;
		}
	}
	
	return true;
}

const char *IdeDocPrivate::GetLocalFile()
{
	if (nSrc)
	{
		if (nSrc->IsWeb())
			return nSrc->GetLocalCache();
		
		auto fp = nSrc->GetFullPath();
		if (_stricmp(fp.Get()?fp.Get():"", Buffer.Get()?Buffer.Get():""))
			Buffer = fp;
		return Buffer;
	}
	
	return FileName;
}

void IdeDocPrivate::SetFileName(const char *f)
{
	nSrc = NULL;
	FileName = f;
	Edit->IsDirty(true);
}

bool IdeDocPrivate::Load()
{
	bool Status = false;
	
	if (nSrc)
	{
		Status = nSrc->Load(Edit, this);
	}
	else if (FileName)
	{
		if (LFileExists(FileName))
		{
			Status = Edit->Open(FileName);
		}
		else LgiTrace("%s:%i - '%s' doesn't exist.\n", _FL, FileName.Get());
	}

	if (Status)
		ModTs = GetModTime();	

	return Status;
}

bool IdeDocPrivate::Save()
{
	bool Status = false;
	
	if (nSrc)
	{
		Status = nSrc->Save(Edit, this);
	}
	else if (FileName)
	{
		Status = Edit->Save(FileName);
		if (!Status)
		{
			const char *Err = Edit->GetLastError();
			LgiMsg(App, "%s", AppName, MB_OK, Err ? Err : "$unknown_error");
		}
		
		OnSaveComplete(Status);
	}
	else
	{
		Edit->IsDirty(false);
	}

	if (Status)
		ModTs = GetModTime();

	// LPopupNotification::Message(Doc->GetWindow(), "Saved");

	return Status;
}

void IdeDocPrivate::OnSaveComplete(bool Status)
{
	if (Status)
		Edit->IsDirty(false);
	UpdateName();

	ProjectNode *Node = dynamic_cast<ProjectNode*>(nSrc);
	if (Node)
	{
		auto Full = nSrc->GetFullPath();
		App->OnNode(Full, Node, FindSymbolSystem::FileReparse);
	}
}

void IdeDocPrivate::CheckModTime()
{
	if (!ModTs.IsValid())
		return;

	LDateTime Ts = GetModTime();
	if (Ts.IsValid() && Ts > ModTs)
	{
		static bool InCheckModTime = false;
		if (!InCheckModTime)
		{
			InCheckModTime = true;
			if (!Edit->IsDirty() ||
				LgiMsg(Doc, "Do you want to reload modified file from\ndisk and lose your changes?", AppName, MB_YESNO) == IDYES)
			{
				auto Ln = Edit->GetLine();
				Load();
				Edit->IsDirty(false);
				UpdateName();
				Edit->SetLine((int)Ln);
			}
			InCheckModTime = false;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
LString IdeDoc::CurIpDoc;
int IdeDoc::CurIpLine = -1;

IdeDoc::IdeDoc(AppWnd *a, NodeSource *src, const char *file)
{
	d = new IdeDocPrivate(this, a, src, file);
	d->UpdateName();
}

IdeDoc::~IdeDoc()
{
	d->App->OnDocDestroy(this);
	DeleteObj(d);
}

class WebBuild : public LThread
{
	IdeDocPrivate *d;
	LString Uri;
	int64 SleepMs;
	LStream *Log;
	LCancel Cancel;

public:
	WebBuild(IdeDocPrivate *priv, LString uri, int64 sleepMs) :
		LThread("WebBuild"), d(priv), Uri(uri), SleepMs(sleepMs)
	{
		Log = d->App->GetBuildLog();
		Run();
	}

	~WebBuild()
	{
		Cancel.Cancel();
		while (!IsExited())
		{
			LSleep(1);
		}
	}

	int Main()
	{
		if (SleepMs > 0)
		{
			// Sleep for a number of milliseconds to allow the file to upload/save to the website
			uint64 Ts = LCurrentTime();
			while (!Cancel.IsCancelled() && (LCurrentTime()-Ts) < SleepMs)
				LSleep(1);
		}

		// Download the file...
		LStringPipe Out;
		LString Error;
		bool r = LgiGetUri(&Cancel, &Out, &Error, Uri, NULL/*InHdrs*/, NULL/*Proxy*/);
		if (r)
		{
			// Parse through it and extract any errors...
		}
		else
		{
			// Show the download error in the build log...
			Log->Print("%s:%i - Web build download failed: %s\n", _FL, Error.Get());
		}

		return 0;
	}
};

bool IdeDoc::Build()
{
	if (!d->Edit)
		return false;

	int64 SleepMs = -1;
	LString s = d->Edit->Name(), Uri;
	LString::Array Lines = s.Split("\n");
	for (auto Ln : Lines)
	{
		s = Ln.Strip();
		if (s.Find("//") == 0)
		{
			LString::Array p = s(2,-1).Strip().Split(":", 1);
			if (p.Length() == 2)
			{
				if (p[0].Equals("build-sleep"))
				{
					SleepMs = p[1].Strip().Int();
				}
				else if (p[0].Equals("build-uri"))
				{
					Uri = p[1].Strip();
					break;
				}
			}
		}
	}

	if (Uri)
	{
		if (d->Build &&
			!d->Build->IsExited())
		{
			// Already building...
			LStream *Log = d->App->GetBuildLog();
			if (Log)
				Log->Print("%s:%i - Already building...\n");
			return false;
		}
		
		return d->Build.Reset(new WebBuild(d, Uri, SleepMs));
	}

	return false;
}

void IdeDoc::OnLineChange(int Line)
{
	d->App->OnLocationChange(d->GetLocalFile(), Line);
}

void IdeDoc::OnMarginClick(int Line)
{
	LString Dn = d->GetDisplayName();
	d->App->ToggleBreakpoint(Dn, Line);
}

void IdeDoc::OnTitleClick(LMouse &m)
{
	LMdiChild::OnTitleClick(m);
	
	if (m.IsContextMenu())
	{
		char Full[MAX_PATH_LEN] = "", sFile[MAX_PATH_LEN] = "", sFull[MAX_PATH_LEN] = "", sBrowse[MAX_PATH_LEN] = "";
		const char *Fn = GetFileName(), *Dir = NULL;
		IdeProject *p = GetProject();
		if (Fn)
		{
			strcpy_s(Full, sizeof(Full), Fn);
			if (LIsRelativePath(Fn) && p)
			{
				LAutoString Base = p->GetBasePath();
				if (Base)
					LMakePath(Full, sizeof(Full), Base, Fn);
			}
			
			auto leaf = LGetLeaf(Full);
			if (leaf != Full)
				sprintf_s(sFile, sizeof(sFile), "Copy '%s'", Dir = leaf);
			sprintf_s(sFull, sizeof(sFull), "Copy '%.500s'", Full);
			sprintf_s(sBrowse, sizeof(sBrowse), "Browse to '%s'", Dir ? Dir + 1 : Full);			
		}
		
		LSubMenu s;
		s.AppendItem("Save", IDM_SAVE, d->Edit->IsDirty());
		s.AppendItem("Close", IDM_CLOSE, true);
		if (Fn)
		{
			s.AppendSeparator();
			if (Dir)
				s.AppendItem(sFile, IDM_COPY_FILE, true);
			s.AppendItem(sFull, IDM_COPY_PATH, true);
			s.AppendItem(sBrowse, IDM_BROWSE, true);
			s.AppendItem("Show In Project", IDM_SHOW_IN_PROJECT, true);
		}
		if (p)
		{
			s.AppendSeparator();
			s.AppendItem("Properties", IDM_PROPERTIES, true);
		}
		
		m.ToScreen();
		int Cmd = s.Float(this, m.x, m.y, m.Left());
		switch (Cmd)
		{
			case IDM_SAVE:
			{
				SetClean(NULL);
				break;
			}
			case IDM_CLOSE:
			{
				if (OnRequestClose(false))
					Quit();
				break;
			}
			case IDM_COPY_FILE:
			{
				if (Dir)
				{
					LClipBoard c(this);
					c.Text(Dir + 1);
				}
				break;
			}
			case IDM_COPY_PATH:
			{
				LClipBoard c(this);
				c.Text(Full);
				break;
			}
			case IDM_BROWSE:
			{
				LBrowseToFile(Full);
				break;
			}
			case IDM_SHOW_IN_PROJECT:
			{
				d->App->ShowInProject(Fn);
				break;
			}
			case IDM_PROPERTIES:
			{
				p->ShowFileProperties(Full);
				break;
			}
		}
	}
}

AppWnd *IdeDoc::GetApp()
{
	return d->App;
}

bool IdeDoc::IsFile(const char *File)
{
	return File ? d->IsFile(File) : false;
}

bool IdeDoc::AddBreakPoint(BreakPoint &bp, bool Add)
{
	if (Add)
		d->BreakPoints.Add(bp.Line, true);
	else
		d->BreakPoints.Delete(bp.Line);
	
	if (d->Edit)
		d->Edit->Invalidate();

	if (auto proj = GetProject())
	{
		if (Add)
			proj->AddBreakpoint(bp);
		
		// else proj->DeleteBreakpoint(bp);
	}
	else LgiTrace("%s:%i no project for this doc?\n", _FL);

	return true;
}


void IdeDoc::GotoSearch(int CtrlId, char *InitialText)
{
	LString File;

	if (CtrlId == IDC_SYMBOL_SEARCH)
	{
		// Check if the cursor is on a #include line... in which case we
		// should look up the header and go to that instead of looking for
		// a symbol in the code.
		if (d->Edit)
		{
			// Get current line
			LString Ln = (*d->Edit)[d->Edit->GetLine()];
			if (Ln.Find("#include") >= 0)
			{
				LString::Array a = Ln.SplitDelimit(" \t", 1);
				if (a.Length() == 2)
				{
					File = a[1].Strip("\'\"<>");
					InitialText = File;
					CtrlId = IDC_FILE_SEARCH;
				}
			}			
		}
	}

	if (d->Tray)
		d->Tray->GotoSearch(CtrlId, InitialText);
}

#define IsVariableChar(ch) \
	( \
		IsAlpha(ch) \
		|| \
		IsDigit(ch) \
		|| \
		strchr("-_~", ch) != NULL \
	)

void IdeDoc::SearchSymbol()
{
	if (!d->Edit || !d->Tray)
	{
		LAssert(0);
		return;
	}
	
	ssize_t Cur = d->Edit->GetCaret();
	auto Txt = d->Edit->NameW();
	if (Cur >= 0 &&
		Txt != NULL)
	{
		ssize_t Start = Cur;
		while (	Start > 0 &&
				IsVariableChar(Txt[Start-1]))
			Start--;

		ssize_t End = Cur;
		while (	Txt[End] &&
				IsVariableChar(Txt[End]))
			End++;

		LString Word(Txt + Start, End - Start);
		GotoSearch(IDC_SYMBOL_SEARCH, Word);
	}
}

void IdeDoc::UpdateControl()
{
	if (d->Edit)
		d->Edit->Invalidate();
	else
		LgiTrace("%s:%i - Error: no edit control?\n", _FL);
}

void IdeDoc::SearchFile()
{
	GotoSearch(IDC_FILE_SEARCH, NULL);
}

bool IdeDoc::IsCurrentIp()
{
	return !Stricmp(GetFileName(), CurIpDoc.Get());
}

void IdeDoc::ClearCurrentIp()
{
	CurIpDoc.Empty();
	CurIpLine = -1;
}

void IdeDoc::SetCrLf(bool CrLf)
{
	if (d->Edit)
		d->Edit->SetCrLf(CrLf);
}

bool IdeDoc::OpenFile(const char *File)
{
	if (!d->Edit)
		return false;

	auto Cs = d->GetSrc() ? d->GetSrc()->GetCharset() : NULL;

	return d->Edit->Open(File, Cs);
}

bool IdeDoc::OpenData(LString Data)
{
	if (!d->Edit)
		return false;

	auto Cs = d->GetSrc() ? d->GetSrc()->GetCharset() : NULL;
	if (Cs)
	{
		LAssert(!"Impl me");
	}

	return d->Edit->Name(Data);
}

void IdeDoc::SetEditorParams(int IndentSize, int TabSize, bool HardTabs, bool ShowWhiteSpace)
{
	if (d->Edit)
	{
		d->Edit->SetIndentSize(IndentSize > 0 ? IndentSize : 4);
		d->Edit->SetTabSize(TabSize > 0 ? TabSize : 4);
		d->Edit->SetHardTabs(HardTabs);
		d->Edit->SetShowWhiteSpace(ShowWhiteSpace);
		d->Edit->Invalidate();
	}
}

bool IdeDoc::HasFocus(int Set)
{
	if (!d->Edit)
		return false;

	if (Set)
		d->Edit->Focus(Set);

	return d->Edit->Focus();
}

void IdeDoc::SplitSelection(LString Sep)
{
	if (!d->Edit)
		return;

	auto r = d->Edit->GetSelectionRange();
	LString s = d->Edit->GetSelection();
	if (!s)
		return;

	auto parts = s.SplitDelimit(Sep);
	auto joined = LString("\n").Join(parts);

	LAutoWString w(Utf8ToWide(joined));
	d->Edit->DeleteSelection();
	d->Edit->Insert(r.Start, w, StrlenW(w));
}

void IdeDoc::JoinSelection(LString Sep)
{
	if (!d->Edit)
		return;

	auto r = d->Edit->GetSelectionRange();
	LString s = d->Edit->GetSelection();
	if (!s)
		return;

	LAutoWString w(Utf8ToWide(s.Replace("\n", Sep)));
	d->Edit->DeleteSelection();
	d->Edit->Insert(r.Start, w, StrlenW(w));
}

void IdeDoc::EscapeSelection(bool ToEscaped)
{
	if (!d->Edit)
		return;

	LString s = d->Edit->GetSelection();
	if (!s)
		return;

	auto ReplaceSelection = [this](LString s)
	{
		auto r = d->Edit->GetSelectionRange();
		LAutoWString w(Utf8ToWide(s));
		d->Edit->DeleteSelection();
		d->Edit->Insert(r.Start, w, StrlenW(w));
	};

	if (ToEscaped)
	{
		LMouse m;
		GetMouse(m);
		LString Delim = "\r\n\\";
		if (m.Ctrl())
		{
			auto Inp = new LInput(this, LString::Escape(Delim, -1, "\\"), "Delimiter chars:", "Escape");
			Inp->DoModal([this, ReplaceSelection, s, Inp](auto d, auto code)
			{
				if (code)
				{
					auto Delim = LString::UnEscape(Inp->GetStr().Get(), -1);
					auto str = LString::Escape(s, -1, Delim);
					ReplaceSelection(str);
				}
			});
			return;
		}

		s = LString::Escape(s, -1, Delim);
	}
	else
	{
		s = LString::UnEscape(s.Get(), -1);
	}

	ReplaceSelection(s);
}

void IdeDoc::ConvertWhiteSpace(bool ToTabs)
{
	if (!d->Edit)
		return;

	LAutoString Sp(
		ToTabs ?
		SpacesToTabs(d->Edit->Name(), d->Edit->GetTabSize()) :
		TabsToSpaces(d->Edit->Name(), d->Edit->GetTabSize())
		);
	if (Sp)
	{
		d->Edit->Name(Sp);
		SetDirty();
	}
}

ssize_t IdeDoc::GetLine()
{
	return d->Edit ? d->Edit->GetLine() : -1;
}

void IdeDoc::SetLine(int Line, bool CurIp)
{
	if (CurIp)
	{
		LString CurDoc = GetFileName();
		
		if (ValidStr(CurIpDoc) ^ ValidStr(CurDoc)
			||
			(CurIpDoc && CurDoc && strcmp(CurDoc, CurIpDoc) != 0)
			||
			Line != CurIpLine)
		{
			bool Cur = IsCurrentIp();
			if (d->Edit && Cur && CurIpLine >= 0)
			{
				// Invalidate the old IP location
				d->Edit->InvalidateLine(CurIpLine - 1);
			}
			
			CurIpLine = Line;
			CurIpDoc = CurDoc;
			
			d->Edit->InvalidateLine(CurIpLine - 1);
		}
	}
	
	if (d->Edit)
	{
		d->Edit->SetLine(Line);
	}
}

IdeProject *IdeDoc::GetProject()
{
	return d->Project;
}

void IdeDoc::SetProject(IdeProject *p)
{
	d->Project = p;
	
	if (d->Project->GetApp() &&
		d->BreakPoints.Length() == 0)
		d->Project->LoadBreakPoints(this);
}

const char *IdeDoc::GetFileName()
{
	return d->GetLocalFile();
}

void IdeDoc::SetFileName(const char *f, bool Write)
{
	d->SetFileName(f);
	if (Write)
		d->Edit->Save(d->GetLocalFile());
}

void IdeDoc::Focus(bool f)
{
	d->Edit->Focus(f);
}

LMessage::Result IdeDoc::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_FIND_SYM_REQUEST:
		{
			auto Resp = Msg->AutoA<FindSymRequest>();
			if (!Resp || !d->SymPopup)
				break;

			LViewI *SymEd;
			if (!GetViewById(IDC_SYMBOL_SEARCH, SymEd))
				break;

			LString Input = SymEd->Name();
			if (Input != Resp->Str) // Is the input string still the same?
				break;

			d->SymPopup->All.Swap(Resp->Results);
			d->SymPopup->FindCommonPathLength();
			d->SymPopup->SetItems(d->SymPopup->All);
			d->SymPopup->Visible(true);
			break;
		}
	}
	
	return LMdiChild::OnEvent(Msg);
}

void IdeDoc::OnPulse()
{
	d->CheckModTime();

	if (d->Lock(_FL))
	{
		if (d->WriteBuf.Length())
		{
			bool Pour = d->Edit->SetPourEnabled(false);
			for (auto s: d->WriteBuf)
			{
				LAutoWString w(Utf8ToWide(s, s.Length()));
				d->Edit->Insert(d->Edit->Length(), w, Strlen(w.Get()));
			}
			d->Edit->SetPourEnabled(Pour);

			d->WriteBuf.Empty();
			d->Edit->Invalidate();
		}
		d->Unlock();
	}
}

LString IdeDoc::Read()
{
	return d->Edit->Name();
}

ssize_t IdeDoc::Write(const void *Ptr, ssize_t Size, int Flags)
{
	if (d->Lock(_FL))
	{
		d->WriteBuf.New().Set((char*)Ptr, Size);
		d->Unlock();
	}
	return 0;
}

void IdeDoc::OnProjectChange()
{
	DeleteObj(d->FilePopup);
	DeleteObj(d->MethodPopup);
	DeleteObj(d->SymPopup);
}

int IdeDoc::OnNotify(LViewI *v, LNotification &n)
{
	// printf("IdeDoc::OnNotify(%i, %i)\n", v->GetId(), f);
	
	switch (v->GetId())
	{
		case IDC_EDIT:
		{
			switch (n.Type)
			{
				case LNotifyDocChanged:
				{
					d->UpdateName();
					break;
				}
				case LNotifyCursorChanged:
				{
					if (d->Tray)
					{
						LPoint Pt;
						if (d->Edit->GetLineColumnAtIndex(Pt, d->Edit->GetCaret()))
						{
							d->Tray->Col = Pt.x;
							d->Tray->Line = Pt.y;
							d->Tray->Invalidate();
						}
					}
					break;
				}
				default:
					break;
			}
			break;
		}
		case IDC_FILE_SEARCH:
		{
			if (n.Type == LNotifyEscapeKey)
			{
				d->Edit->Focus(true);
				break;
			}
			
			auto SearchStr = v->Name();
			if (ValidStr(SearchStr))
			{
				auto Platform = PlatformFlagsToEnum(d->App->GetPlatform());
				LVariant searchSysInc = false;
				d->App->GetOptions()->GetValue(OPT_SearchSysInc, searchSysInc);
				LString::Array SysInc;
			
				if (!d->FilePopup)
				{
					if ((d->FilePopup = new ProjFilePopup(d->App, v)))
					{
						// Populate with files from the project...
						
						// Find the root project...
						IdeProject *p = d->Project;
						while (p && p->GetParentProject())
							p = p->GetParentProject();
						
						if (p)
						{
							// Get all the nodes
							auto All = p->GetAllProjects();
							for (auto p: All)
							{
								p->GetAllNodes(d->FilePopup->Nodes);
								
								if (searchSysInc.CastInt32())
								{
									if (LString s = p->GetSettings()->GetStr(ProjSystemIncludes, NULL, Platform))
										SysInc += s.Strip().SplitDelimit("\r\n");
								}
							}
						}
					}
				}
				if (d->FilePopup)
				{
					if (SysInc.Length())
						d->FilePopup->SetSysInc(SysInc);
					
					// Update list elements...
					d->FilePopup->OnNotify(v, n);
				}
			}
			else if (d->FilePopup)
			{
				DeleteObj(d->FilePopup);
			}
			break;
		}
		case IDC_METHOD_SEARCH:
		{
			if (n.Type == LNotifyEscapeKey)
			{
				printf("%s:%i Got LNotifyEscapeKey\n", _FL);
				d->Edit->Focus(true);
				break;
			}
			
			auto SearchStr = v->Name();
			if (ValidStr(SearchStr))
			{
				if (!d->MethodPopup)
					d->MethodPopup = new ProjMethodPopup(d->App, v);
				if (d->MethodPopup)
				{
					// Populate with symbols
					d->MethodPopup->All.Length(0);
					BuildDefnList(GetFileName(), (char16*)d->Edit->NameW(), d->MethodPopup->All, DefnFunc);

					// Update list elements...
					d->MethodPopup->OnNotify(v, n);
				}
			}
			else if (d->MethodPopup)
			{
				DeleteObj(d->MethodPopup);
			}
			break;
		}
		case IDC_SYMBOL_SEARCH:
		{
			if (n.Type == LNotifyEscapeKey)
			{
				printf("%s:%i Got LNotifyEscapeKey\n", _FL);
				d->Edit->Focus(true);
				break;
			}
			
			auto SearchStr = v->Name();
			if (ValidStr(SearchStr))
			{
				if (!d->SymPopup)
					d->SymPopup = new ProjSymPopup(d->App, this, v);
				if (d->SymPopup)
					d->SymPopup->OnNotify(v, n);
			}
			else if (d->SymPopup)
			{
				DeleteObj(d->SymPopup);
			}
			break;
		}
	}
	
	return 0;
}

void IdeDoc::SetDirty()
{
	d->Edit->IsDirty(true);
	d->UpdateName();
}

bool IdeDoc::GetClean()
{
	return !d->Edit->IsDirty();
}

LString IdeDoc::GetFullPath()
{
	LAutoString Base;
	if (GetProject())
		Base = GetProject()->GetBasePath();
	
	LString LocalPath;
	if (d->GetLocalFile() &&
		LIsRelativePath(LocalPath) &&
		Base)
	{
		char p[MAX_PATH_LEN];
		LMakePath(p, sizeof(p), Base, d->GetLocalFile());
		LocalPath = p;
	}
	else
	{
		LocalPath = d->GetLocalFile();
	}
	
	if (d->Project)
		d->Project->CheckExists(LocalPath);

	return LocalPath;
}

void IdeDoc::SetClean(std::function<void(bool)> Callback)
{
	static bool Processing = false;

	if (!Processing)
	{
		Processing = true;

		auto proj = GetProject();
		LAutoString Base;
		if (proj)
			Base = proj->GetBasePath();
		LString LocalPath = GetFullPath();

		auto OnSave = [this, Callback](bool ok)
		{
			if (ok)
				d->Save();
			if (Callback)
				Callback(ok);
		};		
		
		if (!d->Edit->IsDirty())
		{
			if (Callback)
				Callback(true);
		}
		else if (auto backend = proj ? proj->GetBackend() : NULL)
		{
			LString Content = d->Edit->Name();
			backend->Write(LocalPath,
				Content,
				[this, Callback](auto err)
				{
					d->OnSaveComplete(!err);
					if (Callback)
						Callback(!err);
				});
		}
		else
		{
			if (!LFileExists(LocalPath))
			{
				// We need a valid filename to save to...			
				auto s = new LFileSelect;
				s->Parent(this);
				if (Base)
					s->InitialDir(Base);			
				s->Save([this, OnSave=std::move(OnSave)](auto fileSel, auto ok)
				{
					if (ok)
						d->SetFileName(fileSel->Name());
					OnSave(ok);
				});
			}
			else OnSave(true);
		}
		
		Processing = false;
	}
}

void IdeDoc::OnPaint(LSurface *pDC)
{
	LMdiChild::OnPaint(pDC);
	
	#if !MDI_TAB_STYLE
	LRect c = GetClient();
	LWideBorder(pDC, c, SUNKEN);
	#endif
}

void IdeDoc::OnPosChange()
{
	LMdiChild::OnPosChange();
}

bool IdeDoc::OnRequestClose(bool OsShuttingDown)
{
	if (d->Edit->IsDirty())
	{
		LString Dsp = d->GetDisplayName();
		int a = LgiMsg(this, "Save '%s'?", AppName, MB_YESNOCANCEL, Dsp ? Dsp.Get() : Untitled);
		switch (a)
		{
			case IDYES:
			{
				SetClean([this](bool ok)
				{
					if (ok)
						delete this;
				});

				// This is returned immediately... before the user has decided to save or not
				return false;
			}
			case IDNO:
			{
				break;
			}
			default:
			case IDCANCEL:
			{
				return false;
			}
		}
	}
	
	return true;
}

/*
LTextView3 *IdeDoc::GetEdit()
{
	return d->Edit;
}
*/

bool IdeDoc::BuildIncludePaths(LString::Array &Paths, IdePlatform Platform, bool IncludeSysPaths)
{
	if (!GetProject())
	{
		LgiTrace("%s:%i - GetProject failed.\n", _FL);
		return false;
	}

	bool Status = GetProject()->BuildIncludePaths(Paths, NULL, true, IncludeSysPaths, Platform);
	if (Status)
	{
		if (IncludeSysPaths)
			GetApp()->GetSystemIncludePaths(Paths);
	}
	else
	{
		LgiTrace("%s:%i - GetProject()->BuildIncludePaths failed.\n", _FL);
	}
	
	return Status;
}

bool IdeDoc::BuildHeaderList(const char16 *Cpp, LString::Array &Headers, LString::Array &IncPaths)
{
	LAutoString c8(WideToUtf8(Cpp));
	if (!c8)
		return false;

	LArray<LString::Array*> All;
	All.Add(&IncPaths);
	return ::BuildHeaderList(c8,
							Headers,
							true,
							[&All](auto Name)
							{
								return FindHeader(Name, All);
							});
}

bool MatchSymbol(DefnInfo *Def, char16 *Symbol)
{
	static char16 Dots[] = {':', ':', 0};

	LBase o;
	o.Name(Def->Name);
	auto Name = o.NameW();

	auto Sep = StristrW((char16*)Name, Dots);
	auto Start = Sep ? Sep : Name;
	// char16 *End = StrchrW(Start, '(');
	ssize_t Len = StrlenW(Symbol);
	char16 *Match = StristrW((char16*)Start, Symbol);

	if (Match) // && Match + Len <= End)
	{
		if (Match > Start && isword(Match[-1]))
		{
			return false;
		}
		
		char16 *e = Match + Len;
		if (*e && isword(*e))
		{
			return false;
		}
		
		return true;
	}
	
	return false;
}

bool IdeDoc::FindDefn(char16 *Symbol, const char16 *Source, List<DefnInfo> &Matches)
{
	if (!Symbol || !Source)
	{
		LgiTrace("%s:%i - Arg error.\n", _FL);
		return false;
	}

	#if DEBUG_FIND_DEFN
	LStringPipe Dbg;
	LgiTrace("FindDefn(%S)\n", Symbol);
	#endif

	LString::Array Paths;
	LString::Array Headers;

	if (!BuildIncludePaths(Paths, PlatformCurrent, true))
	{
		LgiTrace("%s:%i - BuildIncludePaths failed.\n", _FL);
		// return false;
	}

	char Local[MAX_PATH_LEN];
	strcpy_s(Local, sizeof(Local), GetFileName());
	LTrimDir(Local);
	Paths.New() = Local;

	if (!BuildHeaderList(Source, Headers, Paths))
	{
		LgiTrace("%s:%i - BuildHeaderList failed.\n", _FL);
		// return false;
	}

	{
		LArray<DefnInfo> Defns;
		
		for (int i=0; i<Headers.Length(); i++)
		{
			char *h = Headers[i];
			auto c8 = LReadFile(h);
			if (c8)
			{
				LAutoWString c16(Utf8ToWide(c8));
				if (c16)
				{
					LArray<DefnInfo> Defns;
					if (BuildDefnList(h, c16, Defns, DefnNone, false ))
					{
						bool Found = false;
						for (unsigned n=0; n<Defns.Length(); n++)
						{
							DefnInfo &Def = Defns[n];
							if (MatchSymbol(&Def, Symbol))
							{
								Matches.Insert(new DefnInfo(Def));
								Found = true;
							}
						}
						
						#if DEBUG_FIND_DEFN
						if (!Found)
							Dbg.Print("Not in '%s'\n", h);
						#endif
					}
				}
			}
		}

		auto FileName = GetFileName();
		if (BuildDefnList(FileName, (char16*)Source, Defns, DefnNone))
		{
			#if DEBUG_FIND_DEFN
			bool Found = false;
			#endif
			
			for (unsigned i=0; i<Defns.Length(); i++)
			{
				DefnInfo &Def = Defns[i];
				if (MatchSymbol(&Def, Symbol))
					Matches.Insert(new DefnInfo(Def));
			}

			#if DEBUG_FIND_DEFN
			if (!Found)
				Dbg.Print("Not in current file.\n");
			#endif
		}
	}
	
	Headers.DeleteArrays();

	#if DEBUG_FIND_DEFN
	{
		LAutoString a(Dbg.NewStr());
		if (a) LgiTrace("%s", a.Get());
	}
	#endif
	
	return Matches.Length() > 0;
}
