#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/DateTime.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Scripting.h"
#include "lgi/common/Box.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/Net.h"
#include "lgi/common/Filter.h"
#include "lgi/common/ImageComparison.h"
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/Net.h"
#include "lgi/common/EmojiFont.h"
#include "lgi/common/Menu.h"
#include "lgi/common/PopupNotification.h"
#include "lgi/common/TabView.h"
#include "lgi/common/Tree.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/Uri.h"
#include "lgi/common/Http.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/ImageThreadManager.h"

#include "lgi/common/Html.h"
#include "../src/common/Text/HtmlPriv.h"

#include "lgi/common/Html2.h"
#include "../src/common/Text/HtmlPriv2.h"

#include "resdefs.h"

#define HAS_LOG_VIEW			0
#define HAS_IMAGE_LOADER		1

enum Controls
{
	ID_HTML = 100,
	ID_LOG,
	ID_LIST,
	ID_TABS,
	ID_DEBUG_BOX,
	ID_TAG_TREE,
	ID_TAG_DETAIL
};

#if 1
using THtmlClass = Html2::LHtml;
using THtmlTag = Html2::LTag;
#else
using THtmlClass = Html1::LHtml;
using THtmlTag = Html1::LTag;
#endif

class HtmlScriptContext;
struct AppContext
{
	LTabView *tabs = nullptr;
	LTree *tagTree = nullptr;
	LTextLog *tagDetail = nullptr;
    HtmlScriptContext *Html = nullptr;

	bool ValidTag(THtmlTag *t);

	struct TagItem : public LTreeItem
	{
		THtmlTag *tag = nullptr;
		THtmlClass *html = nullptr;
		AppContext *context = nullptr;

		TagItem(AppContext *ctx, THtmlTag *t) :
			tag(t), context(ctx)
		{
			html = tag->Html;
		}

		const char *GetText(int i = 0) override
		{
			if (!context->ValidTag(tag))
			{
				tag = nullptr;
				return "##deleted##";
			}

			if (tag->Tag)
				return tag->Tag;

			switch (tag->TagId)
			{
				case HtmlTag::CONTENT:
					return "CONTENT";
				case HtmlTag::CONDITIONAL:
					return "CONDITIONAL";
				case HtmlTag::ROOT:
					return "ROOT";
				default:
					return "NULL";
			}
		}

		void OnSelect() override
		{
			if (!Expanded())
				Expanded(true);

			// Update details view:
			if (tag)
			{
				LStringPipe p;
				if (tag->Class.Length())
					p.Print("class: %s\n", LString(",").Join(tag->Class).Get());
				if (tag->Condition)
					p.Print("condition: %s\n", tag->Condition.Get());
				if (tag->GetText())
				{
					LAutoString utf(WideToUtf8(tag->GetText()));
					if (LIsUtf8(utf))
						p.Print("text: '%s'\n", utf.Get());
				}
				tag->AllAttr([&](auto var, auto val)
					{
						if (Stricmp(var, "class"))
							p.Print("attr: %s='%s'\n", var, val);
					});

				if (auto css = tag->ToLString())
				{
					p.Print("css:\n");
					for (auto ln: css.SplitDelimit("\n"))
						p.Print("\t%s\n", ln.Get());
				}
				
				if (tag->HtmlId)
					p.Print("id='%s'\n", tag->HtmlId);
				p.Print("isTable: %i\n", tag->IsTable());
				p.Print("isTableRow: %i\n", tag->IsTableRow());
				p.Print("isTableCell: %i\n", tag->IsTableCell());
				p.Print("pos: %i, %i\n", tag->Pos.x, tag->Pos.y);
				p.Print("size: %i, %i\n", tag->Size.x, tag->Size.y);

				auto detail = p.NewLStr();
				context->tagDetail->Name(detail);

				// Repaint the html control to show the active selection:
				html->Invalidate();
			}
			else context->tagDetail->Name("null tag");
		}
	};

	TagItem *FindTagItem(THtmlTag *tag)
	{
		TagItem *out = nullptr;
		tagTree->ForAllItems([&](auto item)
			{
				if (auto ti = dynamic_cast<TagItem*>(item))
				{
					if (ti->tag == tag)
					{
						out = ti;
						return false;
					}
				}
				return true;
			});
		return out;
	}
};

class FileInf
{
public:
	LString File;
	LDateTime Date;
};

class HtmlItem : public LListItem
{
    char *Base;
    
public:
    HtmlItem(char *b, char *n)
    {
        Base = b;
        SetText(n);
    }
    
	void OnMouseClick(LMouse &m)
    {
        if (m.IsContextMenu())
        {
            LSubMenu s;
            s.AppendItem("Copy Path", 100);
            m.ToScreen();
            if (s.Float(GetList(), m.x, m.y, m.Left()) == 100)
            {
                LClipBoard c(GetList());
                char Path[MAX_PATH_LEN];
                LMakePath(Path, sizeof(Path), Base, GetText(0));
                c.Text(Path);
            }
        }
    }
};

class HtmlScriptContext :
	public THtmlClass,
	public LScriptContext
{
	LScriptEngine *Eng = nullptr;
	AppContext *appContext = nullptr;
	
public:
	static LHostFunc Methods[];

	HtmlScriptContext(int Id, LDocumentEnv *Env, AppContext *context) :
		THtmlClass(Id, 0, 0, 100, 100, Env),
		appContext(context)
	{		
		if (auto f = new LFont)
		{
			f->Face("Times New Roman");
			f->PointSize(12); // Size '16' is about 12 pt. Maybe they mean 16px?
			SetFont(f, true);
		}
	}

	bool Find(THtmlTag *tag, THtmlTag *search)
	{
		if (tag == search)
			return true;
		for (auto c: tag->Children)
			if (auto child = dynamic_cast<THtmlTag*>(c))
				if (Find(child, search))
					return true;
		return false;
	}

	bool ValidTag(THtmlTag *t)
	{
		return Find(Tag, t);
	}

	LHostFunc *GetCommands() override
	{
		return Methods;
	}
	
	void SetEngine(LScriptEngine *Eng)
	{		
	}
	
	LString GetIncludeFile(const char *FileName) override
	{
		return LString();
	}
	
	LAutoString GetDataFolder() override
	{
		return LAutoString();
	}

	void createTree(LTreeNode *out, THtmlTag *in)
	{
		if (auto item = new AppContext::TagItem(appContext, in))
		{
			out->Insert(item);

			for (auto e: in->Children)
			{
				if (auto child = dynamic_cast<THtmlTag*>(e))
				{
					createTree(item, child);
				}
			}
		}
	}

	using TTagMap = LHashTbl<PtrKey<THtmlTag*>,bool>;
	void MakeMap(TTagMap &map, THtmlTag *t)
	{
		map.Add(t, true);
		for (auto c: t->Children)
			if (auto cTag = dynamic_cast<THtmlTag*>(c))
				MakeMap(map, cTag);
	}

	void cleanTree(TTagMap &map, LTreeNode *n)
	{
		// remove any deleted nodes from the tree...
		for (auto c = n->GetChild(); c; )
		{
			auto next = c->GetNext();
			cleanTree(map, c);
			c = next;
		}

		if (auto i = dynamic_cast<AppContext::TagItem*>(n))
			if (!i->tag || !map.Find(i->tag))
			{
				i->Remove();
				delete i;
			}
	}

	void cleanTree()
	{
		if (!Tag)
			return;

		LHashTbl<PtrKey<THtmlTag*>,bool> map;
		MakeMap(map, Tag);
		cleanTree(map, appContext->tagTree);
	}

	void OnLoad() override
	{
		// call parent class
		THtmlClass::OnLoad();

		// build tag tree:
		if (appContext->tagTree)
		{			
			cleanTree();
			createTree(appContext->tagTree, Tag);
		}
	}

	void OnPaintFinished(LSurface *pDC) override
	{
		cleanTree();

		// If there is a selection tag for debug... show it now...
		if (auto sel = dynamic_cast<AppContext::TagItem*>(appContext->tagTree->Selection()))
		{
			auto tag = sel->tag;

			// Figure out global position of tag:
			LRect r;
			r.ZOff(tag->Size.x, tag->Size.y);
			for (auto t = tag; t; t = dynamic_cast<THtmlTag*>(t->Parent))
				r.Offset(t->Pos.x, t->Pos.y);

			pDC->Colour(LColour::Red);
			pDC->LineStyle(LSurface::LineAlternate);
			pDC->Box(&r);
		}
	}

	void DebugTagInfo(THtmlTag *tag) override
	{
		appContext->tabs->Value(1);

		// Find the selected tag's item:
		if (auto item = appContext->FindTagItem(tag))
		{
			item->Select(true);
			item->ScrollTo();
		}
	}
};

LHostFunc HtmlScriptContext::Methods[] =
{
	LHostFunc(0, 0, 0),
};

bool AppContext::ValidTag(THtmlTag *t)
{
	return Html->ValidTag(t);
}

class AppWnd :
	public LWindow,
	public LDefaultDocumentEnv,
	public LCapabilityTarget,
	public AppContext
{
	LList *Lst = nullptr;
	LBox *debugBox = nullptr;
    LTextView3 *Text = nullptr;
	LString FilesFolder;
	LAutoPtr<LScriptEngine> Script;
	LAutoPtr<LImageThreadManager> Worker;
	LAutoPtr<LEmojiFont> Emoji;

	LoadType GetContent(LAutoPtr<LoadJob> &j) override
	{
		LUri u(j->Uri);
		if (!u.sProtocol)
		{
			char p[MAX_PATH_LEN];
			if (LMakePath(p, sizeof(p), FilesFolder, j->Uri) &&
				LFileExists(p))
			{
				LString Ext = LGetExtension(p);
				if (Ext.Equals("css") ||
					Ext.Equals("html"))
				{
					LAutoPtr<LFile> f(new LFile);
					if (f && f->Open(p, O_READ))
					{
						j->Stream.Reset(f.Release());
						return LoadImmediate;
					}
				}
				else
				{
					j->pDC.Reset(GdcD->Load(p));
					if (j->pDC)
						return LoadImmediate;
				}

				return LoadError;
			}
		}		
	
		#if HAS_IMAGE_LOADER
			if (!Worker)
				Worker.Reset(new LImageThreadManager);
			Worker->Add(j);
			return LoadDeferred;
		#else
			return LoadNotImpl;
		#endif
	}

	bool NeedsCapability(const char *Name, const char *Param) override
	{
		RunCallback(
			[this, Name=LString::Fmt("Needs capability '%s'", Name)]()
			{
				LPopupNotification::Message(this, Name);
			},
			_FL);
		return true;
	}
	
	void OnInstall(CapsHash *Caps, bool Status) override { }
	void OnCloseInstaller() override { }

public:
	AppWnd()
	{
		Emoji.Reset(new LEmojiFont());

		LFontSystem::Inst()->Register(this);
		
		Name("Html Test Suite");
		SetQuitOnClose(true);
		if (Attach(0))
		{
			Menu = new LMenu();
			if (Menu)
			{
				Menu->Attach(this);
				Menu->Load(this, "IDM_MENU");
			}
			
			if (auto box = new LBox)
			{
				AddView(box);

				box->AddView(tabs = new LTabView(ID_TABS));
				tabs->SetPourChildren(true);
				
				auto tab = tabs->Append("Files");
					tab->Append(Lst = new LList(ID_LIST, 0, 0, 100, 100));
						Lst->Sunken(false);
						Lst->SetPourLargest(true);
						Lst->AddColumn("File", 400);
						Lst->CssStyles("width: 200px;");
				tab = tabs->Append("Debug");
					tab->Append(debugBox = new LBox(ID_DEBUG_BOX, true));
						debugBox->AddView(tagTree = new LTree(ID_TAG_TREE));
						debugBox->AddView(tagDetail = new LTextLog(ID_TAG_DETAIL));

				box->Value(350);

				#if HAS_LOG_VIEW
					LBox *vert = new LBox;
					vert->SetVertical(true);
					s->AddView(vert);

					vert->AddView(Html = new HtmlScriptContext(IDC_HTML, this));
					Html->SetCssStyle("height: 70%;");

					vert->AddView(Text = new LTextView3(IDC_LOG, 0, 0, 100, 100));
				
					LBox::Spacer *sp = s->GetSpacer(0);
					if (sp)
					{
						sp->Colour.Rgb(64, 64, 64);
						sp->SizePx = 2;
					}
					sp = vert->GetSpacer(0);
					if (sp)
					{
						sp->Colour.Rgb(64, 64, 64);
						sp->SizePx = 2;
					}
				#else
					box->AddView(Html = new HtmlScriptContext(ID_HTML, this, this));
				#endif

				#if 1
				if (auto css = Html->GetCss(true))
				{
					LCss::Len wid(LCss::LenPx, 1000 + LScrollBar::SCROLL_BAR_SIZE);
					css->Width(wid);
					css->MinWidth(wid);
					css->MaxWidth(wid);
				}
				#endif

				Script.Reset(new LScriptEngine(this, Html, NULL));

				if (Html)
					Html->SetEnv(this);
				#if HAS_IMAGE_LOADER
					Html->SetLoadImages(true);
				#endif
				
				#ifdef MAC
				    #define TIME_TO_DATE(ts) ((ts) / 1000)
				#else
				    #define TIME_TO_DATE(ts) (ts)
				#endif
				
				LFile::Path files(LSP_APP_INSTALL);
				#if defined(MAC) && defined(_DEBUG)
				    files += "../../../../..";
				#endif
				files += "files";
				if (!files.Exists())
					LgiTrace("%s:%i - files folder '%s' doesn't exist", _FL, files.GetFull().Get());
				else
				{
					FilesFolder = files.GetFull();

					List<FileInf> Files;
					LDirectory d;
					for (auto b = d.First(FilesFolder); b; b = d.Next())
					{
						if (!d.IsDir() &&
							MatchStr("*.html", d.GetName()))
						{
							char p[256];
							if (d.Path(p, sizeof(p)))
							{
								if (auto f = new FileInf)
								{
									f->File = p;
									f->Date.Set(TIME_TO_DATE(d.GetLastWriteTime()));
									Files.Insert(f);
								}
							}
						}
					}
					Files.Sort([](auto a, auto b)
						{
							return -a->Date.Compare(&b->Date);
						});

					for (auto f: Files)
					{
						if (auto d = strrchr(f->File, DIR_CHAR))
						{
							if (auto i = new HtmlItem(FilesFolder, d + 1))
								Lst->Insert(i);
						}
					}

					Files.DeleteObjects();
				}
			}
			
			
			LRect r(0, 0, 1400, 800);
			SetPos(r);
			MoveToCenter();
			AttachChildren();
			
			Visible(true);
		}
		else LExitApp();
	}

	~AppWnd()
	{
	}

	LString SaveImagePath()
	{
		LFile::Path p(LSP_APP_INSTALL);
		return (p / ".." / "Output").GetFull();
	}
	
	struct SaveImagesState :
		public LProgressDlg,
		public LThread
	{
		AppWnd *App = NULL;
		LArray<char*> Files;
		LString OutPath;
		LPoint PageSize = LPoint(1000, 2000);

	public:
		SaveImagesState(AppWnd *app) :
			LProgressDlg(app),
			LThread("SaveImagesState"),
			App(app)
		{
			SetDescription("Scanning for HTML...");
			
			char p[MAX_PATH_LEN];
			LGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
			LTrimDir(p);

			Visible(true);
			DeleteOnExit = true;

			LArray<const char*> Ext;
			Ext.Add("*.html");
			Ext.Add("*.htm");				
			if (LRecursiveFileSearch(p, &Ext, &Files))
			{
				LDateTime Now;
				Now.SetNow();
				char NowStr[32];
				sprintf_s(	NowStr, sizeof(NowStr),
							"%.4i-%.2i-%.2i_%.2i-%.2i-%.2i",
							Now.Year(), Now.Month(), Now.Day(),
							Now.Hours(), Now.Minutes(), Now.Seconds());
				
				LFile::Path path(App->SaveImagePath());
				if (!path.Exists())
					FileDev->CreateFolder(path);
				path += NowStr;
				if (!path.Exists())
					FileDev->CreateFolder(path);
				OutPath = path.GetFull();
					
				SetDescription("Saving renders...");
				SetRange(Files.Length());
			}

			Run();
		}

		~SaveImagesState()
		{
			Cancel();
			WaitForExit();
			Files.DeleteArrays();
		}

		int Main()
		{
			for (auto file: Files)
			{
				LString Content;
				char Png[MAX_PATH_LEN] = "", p[MAX_PATH_LEN] = "";

				if (IsCancelled())
					break;

				Content = LReadFile(file);
				if (!Content)
				{
					LAssert(0);
					break;
				}

				auto Leaf = strrchr(file, DIR_CHAR);
				if (!Leaf)
				{
					LAssert(0);
					break;
				}
			
				Leaf++;
				strcpy_s(Png, sizeof(Png), Leaf);
				char *Ext = strrchr(Png, '.');
				if (Ext) *Ext = 0;
				strcat_s(Png, sizeof(Png), ".png");
				LMakePath(p, sizeof(p), OutPath, Png);

				Html1::LHtml Html(100, 0, 0, PageSize.x, PageSize.y, App);
				Html.Name(Content);

				LMemDC Screen(_FL, PageSize.x, PageSize.y, System24BitColourSpace);
				Screen.Colour(LColour(255, 255, 255));
				Screen.Rectangle();
				Html.OnPaint(&Screen);
				if (!GdcD->Save(p, &Screen))
				{
					LAssert(0);
					break;
				}

				(*this)++;
			}
						
			return 0;
		}
	};

	int OnCommand(int Cmd, int Event, OsView Wnd) override
	{
		switch (Cmd)
		{
			case IDM_SAVE_IMGS:
			{
				new SaveImagesState(this);
				break;
			}
			case IDM_COMPARE_IMAGES:
			{
				auto OutPath = SaveImagePath();
				if (!LDirExists(OutPath))
				{
					LgiMsg(this, "No output render folder.", "Html Test Suite");
					break;
				}
				
				new ImageCompareDlg(this, OutPath);
				break;
			}
		}
		
		return 0;
	}
	
	int OnNotify(LViewI *c, const LNotification &n) override
	{
		switch (c->GetId())
		{
			case ID_LIST:
			{
				if (n.Type == LNotifyItemSelect)
				{
					if (auto s = Lst->GetSelected())
					{
						LFile::Path p(FilesFolder, s->GetText(0));
						if (p.Exists())
						{
							if (auto h = LReadFile(p))
							{
								if (Html)
									Html->Name(h);
							}
							else LPopupNotification::Message(this,
								LString::Fmt("Failed to read '%s'", p.GetFull().Get()));
						}
						else LPopupNotification::Message(this,
							LString::Fmt("The path '%s' doesn't exist", p.GetFull().Get()));
					}
				}
				break;
			}
			case ID_HTML:
			{
				switch (n.Type)
				{
					case LNotifySelectionChanged:
					{
						if (Text)
						{
							LAutoString s(Html->GetSelection());
							if (s)
								Text->Name(s);
							else
								Text->Name("(null)");
						}
						break;
					}
					default:
						break;
				}
				break;
			}
		}
		
		return 0;
	}

	bool OnCompileScript(LDocView *Parent, char *Script, const char *Language, const char *MimeType) override
	{
		// return Script->Compile(Code, true);
		return false;
	}

	bool OnExecuteScript(LDocView *Parent, char *Script) override
	{
		return false; // Script->RunTemporary(Code);
	}
};

void FontSz()
{
	for (int i=6; i<32; i++)
	{
		LFont f;
		if (f.Create("verdana", LCss::Len(LCss::LenPx, i)))
		{
			double a = (double) f.GetHeight() / f.Ascent();
			LgiTrace("%i: %i, ascent=%f, a=%f\n", i, f.GetHeight(), f.Ascent(), a);
		}
	}
}

bool TestRoundedBorders()
{
	Html2::LHtml html(0, 0, 0, 80, 60);
	Html2::LTag tag(&html, nullptr);
	tag.Font = LSysFont;
	tag.Display(LCss::DispBlock);
	tag.Size = LPoint(80, 60);
	tag.BorderRadius(LCss::Len(LCss::LenPx, 16));
	LCss::BorderDef left, top, right, bottom;
	auto edge = [](LCss::BorderDef &border, int width, LColour colour)
	{
		border.Type = LCss::LenPx;
		border.Value = width;
		border.Style = LCss::BorderSolid;
		border.Color = LCss::ColorDef(colour);
	};
	edge(left, 6, LColour(255, 0, 0));
	edge(top, 6, LColour(0, 255, 0));
	edge(right, 6, LColour(0, 0, 255));
	edge(bottom, 6, LColour(255, 165, 0));
	auto borders = [&]()
	{
		tag.BorderLeft(left);
		tag.BorderTop(top);
		tag.BorderRight(right);
		tag.BorderBottom(bottom);
	};
	borders();
	LMemDC dc(_FL, 80, 60, CsBgra32, LSurface::SurfaceRequireExactCs);
	if (!dc[0])
		return false;
	auto pixel = [&](int x, int y) { return reinterpret_cast<const LBgra32*>(dc[y])[x]; };
	auto reset = [&]()
	{
		dc.ClipRgn(nullptr);
		dc.Op(GDC_SET);
		dc.Colour(LColour(11, 23, 37));
		dc.Rectangle();
	};
	auto untouched = [&](int x, int y)
	{
		auto p = pixel(x, y);
		return p.r == 11 && p.g == 23 && p.b == 37 && p.a == 255;
	};
	auto fail = [&](const char *message, int x, int y)
	{
		auto p = pixel(x, y);
		printf("Rounded borders: %s (%i,%i = %i,%i,%i,%i)\n",
			message, x, y, p.r, p.g, p.b, p.a);
		return false;
	};
	LColour transparent(0, 0, 0, 0);
	reset();
	dc.Op(GDC_XOR);
	tag.PaintBorderAndBackground(&dc, transparent);
	auto corner = pixel(6, 6);
	if (dc.Op() != GDC_XOR || !untouched(0, 0) || !untouched(79, 59) ||
		!untouched(40, 30) || pixel(0, 30).r != 255 || pixel(40, 0).g != 255 ||
		pixel(79, 30).b != 255 || pixel(40, 59).r != 255 ||
		corner.r < 110 || corner.r > 145 || corner.g < 110 || corner.g > 145 ||
		corner.b || corner.a != 255)
		return fail("transparent centre, corner gradient, or state restoration", 6, 6);

	reset();
	LColour background(200, 100, 50, 128);
	tag.PaintBorderAndBackground(&dc, background);
	auto center = pixel(40, 30);
	if (center.r != 105 || center.g != 61 || center.b != 43 || !untouched(0, 0))
		return fail("translucent background must be composited once", 40, 30);

	edge(left, 8, LColour(255, 0, 0));
	edge(top, 4, LColour(0, 255, 0));
	edge(right, 2, LColour(0, 0, 255));
	edge(bottom, 10, LColour(255, 165, 0));
	borders();
	reset();
	tag.PaintBorderAndBackground(&dc, transparent);
	if (pixel(7, 30).r != 255 || !untouched(8, 30) ||
		pixel(40, 3).g != 255 || !untouched(40, 4) ||
		pixel(78, 30).b != 255 || !untouched(77, 30) ||
		pixel(40, 50).r != 255 || !untouched(40, 49) || !untouched(40, 30))
		return fail("unequal border widths", 7, 30);

	left.Value = 20;
	top.Style = LCss::BorderNone;
	borders();
	reset();
	tag.PaintBorderAndBackground(&dc, transparent);
	if (pixel(6, 6).r != 255 || pixel(18, 4).r != 255 ||
		!untouched(40, 0) || !untouched(40, 30))
		return fail("thick borders and a missing adjacent edge", 6, 6);

	left.Value = 6;
	top.Style = LCss::BorderSolid;
	top.Value = 20;
	borders();
	reset();
	tag.PaintBorderAndBackground(&dc, transparent);
	if (untouched(10, 18) || !untouched(40, 30))
		return fail("thick corner cells must join the straight borders", 10, 18);

	tag.BorderRadius(LCss::Len(LCss::LenPx, 1000));
	tag.Size = LPoint(20, 40);
	edge(left, 2, LColour(255, 0, 0));
	edge(top, 2, LColour(0, 255, 0));
	borders();
	reset();
	tag.PaintBorderAndBackground(&dc, transparent);
	if (!untouched(0, 0) || !untouched(20, 20) || !untouched(10, 20) ||
		pixel(0, 20).r != 255)
		return fail("radius must be clamped to both dimensions", 10, 20);

	tag.Size = LPoint(80, 60);
	tag.BorderRadius(LCss::Len(LCss::LenPx, 16));
	left.Type = LCss::LenPt;
	left.Value = 3;
	borders();
	reset();
	LRect borderPixels;
	tag.PaintBorderAndBackground(&dc, transparent, &borderPixels);
	int expectedWidth = left.ToPx(tag.Size.x, LSysFont);
	if (borderPixels.x1 != expectedWidth || pixel(expectedWidth - 1, 30).r != 255 ||
		!untouched(expectedWidth, 30))
		return fail("straight and curved borders must use converted pixel widths", expectedWidth - 1, 30);
	left.Type = LCss::LenPx;
	left.Value = 2;
	borders();
	reset();
	LRect clip(10, 10, 60, 40);
	dc.ClipRgn(&clip);
	tag.PaintBorderAndBackground(&dc, background);
	if (dc.ClipRgn() != clip || !untouched(0, 30) || !untouched(40, 0) ||
		pixel(40, 30).r != 105)
		return fail("clip preservation", 40, 30);

	left.Style = LCss::BorderDotted;
	borders();
	reset();
	tag.PaintBorderAndBackground(&dc, transparent);
	int painted = 0, gaps = 0;
	for (int y = 20; y < 40; y++)
	{
		if (pixel(0, y).r == 255)
			painted++;
		if (untouched(0, y))
			gaps++;
	}
	if (!painted || !gaps)
		return fail("dotted straight edges must retain their pattern", 0, 30);

	left.Style = LCss::BorderSolid;
	borders();
	tag.BorderRadius(LCss::Len(LCss::LenPx, 0));
	reset();
	tag.PaintBorderAndBackground(&dc, transparent);
	if (pixel(0, 0).g != 255 || !untouched(40, 30))
		return fail("zero radius must retain square borders", 0, 0);

	Html2::LTag inlineTag(&html, nullptr);
	inlineTag.Font = LSysFont;
	inlineTag.Display(LCss::DispInline);
	inlineTag.BorderRadius(LCss::Len(LCss::LenPx, 6));
	for (auto rect : {LRect(2, 2, 31, 21), LRect(42, 32, 71, 51)})
	{
		auto fragment = new Html2::LFlowRect;
		fragment->Set(rect.x1, rect.y1, rect.x2, rect.y2);
		inlineTag.TextPos.Add(fragment);
	}
	reset();
	LColour solid(100, 150, 200);
	inlineTag.PaintBorderAndBackground(&dc, solid);
	if (!untouched(2, 2) || !untouched(42, 32) ||
		pixel(16, 12).r != 100 || pixel(56, 42).r != 100 || !untouched(38, 25))
		return fail("inline fragments must have separate rounded backgrounds", 16, 12);

	left.Style = top.Style = right.Style = bottom.Style = LCss::BorderNone;
	borders();
	tag.Size = LPoint(1, 1);
	tag.BorderRadius(LCss::Len(LCss::LenPx, 1000));
	reset();
	tag.PaintBorderAndBackground(&dc, solid);
	if (pixel(0, 0).r != 100 || !untouched(1, 0) || !untouched(0, 1))
		return fail("one-pixel boxes must not create empty corner images", 0, 0);

	#if defined(__GTK_H__)
	tag.Size = LPoint(80, 60);
	tag.BorderRadius(LCss::Len(LCss::LenPx, 16));
	edge(left, 6, LColour(255, 0, 0));
	edge(top, 6, LColour(0, 255, 0));
	edge(right, 6, LColour(0, 0, 255));
	edge(bottom, 6, LColour(255, 165, 0));
	borders();
	reset();
	tag.PaintBorderAndBackground(&dc, transparent);
	auto surface = Gtk::cairo_image_surface_create(Gtk::CAIRO_FORMAT_ARGB32, 80, 60);
	auto context = Gtk::cairo_create(surface);
	Gtk::cairo_set_source_rgb(context, 11 / 255.0, 23 / 255.0, 37 / 255.0);
	Gtk::cairo_paint(context);
	bool matches = true;
	{
		LScreenDC screen(context, 80, 60);
		tag.PaintBorderAndBackground(&screen, transparent);
		Gtk::cairo_surface_flush(surface);
		auto data = Gtk::cairo_image_surface_get_data(surface);
		int stride = Gtk::cairo_image_surface_get_stride(surface);
		const LPoint samples[] = {{0, 0}, {40, 30}, {0, 30}, {40, 0},
			{79, 30}, {40, 59}, {6, 6}, {73, 6}, {73, 53}, {6, 53}};
		for (auto &point : samples)
		{
			auto actual = reinterpret_cast<const LBgra32*>(data + point.y * stride)[point.x];
			auto expected = pixel(point.x, point.y);
			if (abs(int(actual.r) - expected.r) > 1 || abs(int(actual.g) - expected.g) > 1 ||
				abs(int(actual.b) - expected.b) > 1 || actual.a != expected.a)
				matches = false;
		}
	}
	Gtk::cairo_destroy(context);
	Gtk::cairo_surface_destroy(surface);
	if (!matches)
		return fail("native and memory rendering must agree", 6, 6);
	#endif
	return true;
}

int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, "HtmlTestSuite");
	if (a.IsOk())
	{
		if (a.GetOption("test-rounded-borders"))
		{
			bool passed = TestRoundedBorders();
			printf("Rounded borders: %s\n", passed ? "PASS" : "FAIL");
			return passed ? 0 : 1;
		}
		//FontSz();

		a.AppWnd = new AppWnd;
		a.Run();
	}
	return 0;
}
