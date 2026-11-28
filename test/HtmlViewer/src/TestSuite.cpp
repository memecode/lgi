#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/DateTime.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Scripting.h"
#include "lgi/common/Box.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/ImageComparison.h"
#include "lgi/common/OpenSSLSocket.h"
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
#include "lgi/common/Stat.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/ZoomView.h"

#include "lgi/common/Html.h"
#include "../src/common/Text/HtmlPriv.h"

#include "lgi/common/Html2.h"
#include "../src/common/Text/HtmlPriv2.h"

#include "lgi/common/DocView.h"
#include "lgi/common/Stream.h"
#include "lgi/common/Uri.h"

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

#if defined(__GTK_H__)
#include "../../../src/linux/Gtk/PrintGeometry.h"
namespace
{
	int PrintHtmlContentToPdf(const LString &content, const char *output, bool testBitmap = false);
}
#endif

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
    LString Base;
    
public:
    HtmlItem(char *folder, char *leaf)
    {
        Base = folder;
        SetText(leaf);
    }

    LString GetFolder() const { return Base; }

    LString Path()
    {
        LFile::Path p(Base, GetText());
        return p.GetFull();
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

	void DumpPositions(THtmlTag *tag, int depth)
	{
		if (!tag)
			tag = Tag;
		if (!tag)
			return;
		if (tag->TagId != HtmlTag::CONTENT)
		{
			LPoint a = tag->AbsolutePos();
			printf("%*s%s pos=%d abs=%d,%d size=%d,%d\n",
				depth * 2, "",
				tag->Tag ? tag->Tag : "?",
				(int)tag->Position(),
				a.x, a.y, tag->Size.x, tag->Size.y);
		}
		for (auto c: tag->Children)
			if (auto child = dynamic_cast<THtmlTag*>(c))
				DumpPositions(child, depth + 1);
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
	LAutoPtr<LScriptEngine> Script;
	LAutoPtr<LImageThreadManager> Worker;
    LAutoPtr<LEmojiFont> Emoji;
	bool renderPng = false;
	bool dumpLayout = false;

    LString CurFolder()
    {
		if (HtmlItem *item = dynamic_cast<HtmlItem*>(Lst->GetSelected()))
			return item->GetFolder();
		return LString();
	}

	LoadType GetContent(LAutoPtr<LoadJob> &j) override
	{
		LUri u(j->Uri);
		if (!u.sProtocol)
		{
			char p[MAX_PATH_LEN] = "";
			LMakePath(p, sizeof(p), CurFolder(), j->Uri);
			printf("%s:%i - p='%s'\n", _FL, p);
			if (LFileExists(p))
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
		if (!Attach(0))
		{
			LCloseApp();
			return;
		}

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
					// Lst->CssStyles("width: 200px;");
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
				LCss::Len wid(LCss::LenPx, 1000 + LScrollBar::GetScrollSize());
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
			
			List<FileInf> Files;
			LString path;
			renderPng = LAppInst->GetOption("render", path);
		    printf("renderPng=%i '%s'\n", renderPng, path.Get());
			dumpLayout = LAppInst->GetOption("dump");
			if (renderPng)
			{
				if (auto f = new FileInf)
				{
				    auto stat = LStat(path);
					f->File = path;
					f->Date.Set(TIME_TO_DATE(stat.GetLastWriteTime()));
					Files.Insert(f);
				}
			}
			else
			{			
    			LFile::Path files(LSP_APP_INSTALL);
    			#if defined(MAC) && defined(_DEBUG)
    				files += "../../../../..";
    			#elif defined(WINDOWS)
    				files += "..";
    			#endif
    			files += "files";
    			if (!files.Exists())
    				LgiTrace("%s:%i - files folder '%s' doesn't exist\n", _FL, files.GetFull().Get());
    			else
    			{
    				LDirectory d;
    				for (auto b = d.First(files.GetFull()); b; b = d.Next())
    				{
    					if (!d.IsDir() &&
    						MatchStr("*.html", d.GetName()))
    					{
    						char p[MAX_PATH_LEN];
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
    			}
    			Files.Sort([](auto a, auto b)
    				{
    					return -a->Date.Compare(&b->Date);
    				});
			}

			for (auto f: Files)
			{
			    auto parts = f->File.RSplit(DIR_STR, 1);
				if (parts.Length() == 2)
				{
					printf("%s:%i - parts '%s', '%s'\n", _FL, parts[0].Get(), parts[1].Get());
					if (auto i = new HtmlItem(parts[0], parts[1]))
						Lst->Insert(i);
				}
				else printf("%s:%i - wrong parts %i\n", _FL, (int)parts.Length());
			}

			Files.DeleteObjects();
		}
			
			
		LRect r(0, 0, 1400, 800);
		SetPos(r);
		MoveToCenter();
		AttachChildren();
			
		Visible(true);
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
					FileDev->CreateFolder(path.c_str());
				path += NowStr;
				if (!path.Exists())
					FileDev->CreateFolder(path.c_str());
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
			case ID_PRINT:
			{
				#if defined(__GTK_H__)
				if (auto fs = new LFileSelect(this))
				{
					fs->Type("PDF", "*.pdf");
					if (auto item = dynamic_cast<HtmlItem*>(Lst->GetSelected()))
					{
						LString fileName = item->GetText();
						ssize_t extension = fileName.RFind(".");
						if (extension > 0)
							fileName.Length(extension);
						fileName += ".pdf";
						fs->Name(fileName);
					}

					fs->Save([this](auto fs, auto status)
					{
						if (!status)
							return;

						LString content;
						if (!Html || !Html->GetFormattedContent("text/html", content))
						{
							LgiMsg(this, "Failed to get the current HTML document.", "Html Test Suite");
							return;
						}

						if (PrintHtmlContentToPdf(content, fs->Name()) != 0)
							LgiMsg(this, "Failed to export the document to PDF.", "Html Test Suite");
					});
				}
				#else
					LgiMsg(this, "PDF export is only supported by the GTK build.", "Html Test Suite");
				#endif
				break;
			}
		}
		
		return 0;
	}
	
	void RenderPng(LString outFile)
	{
		LPoint PageSize = LPoint(800, 3000);

		LMemDC Screen(_FL, PageSize.x, PageSize.y, System24BitColourSpace);
		Screen.Colour(LColour::White);
		Screen.Rectangle();
		auto pos = Html->GetPos();
		pos.SetSize(PageSize.x, PageSize.y);
		Html->SetPos(pos);
								Html->OnPaint(&Screen);
								if (dumpLayout)
									Html->DumpPositions(nullptr, 0);
		if (!GdcD->Save(outFile, &Screen))
		{
			LPopupNotification::Message(this,
				LString::Fmt("Failed to save '%s'", outFile.Get()));
		}
		else printf("Render: %s\n", outFile.Get());
	}

	int OnNotify(LViewI *c, const LNotification &n) override
	{
		switch (c->GetId())
		{
			case ID_LIST:
			{
				if (n.Type == LNotifyItemSelect)
				{
					if (!Html)
						break;
					if (HtmlItem *item = dynamic_cast<HtmlItem*>(Lst->GetSelected()))
					{
						LFile::Path p(item->Path());
						if (p.Exists())
						{
							if (auto h = LReadFile(p.c_str()))
							{
								Html->Name(h);
								if (renderPng)
								{
									LFile::Path out(LSP_APP_INSTALL);
									out = out / "render.png";
									RenderPng(out.GetFull());
								}
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

bool TestIframeBodyBackground()
{
	Html2::LHtml html(0, 0, 0, 80, 60);
	LColour root(255, 255, 255);
	LColour frame(240, 240, 255);
	LCss::ColorDef rootBk(root);
	html.GetCss(true)->BackgroundColor(rootBk);

	Html2::LTag iframe(&html, nullptr);
	iframe.TagId = TAG_IFRAME;

	Html2::LTag body(&html, nullptr);
	body.Parent = &iframe;
	body.TagId = TAG_BODY;
	LCss::ColorDef frameBk(frame);
	body.BackgroundColor(frameBk);
	body.SetStyle();

	if (html.GetCss()->BackgroundColor() != rootBk)
	{
		printf("Iframe body background: iframe body changed the control background\n");
		return false;
	}

	Html2::LTag rootBody(&html, nullptr);
	rootBody.TagId = TAG_BODY;
	LColour alt(0, 200, 100);
	LCss::ColorDef altBk(alt);
	rootBody.BackgroundColor(altBk);
	rootBody.SetStyle();

	if (html.GetCss()->BackgroundColor() != altBk)
	{
		printf("Iframe body background: root body did not set the control background\n");
		return false;
	}

	return true;
}

static LHtmlElement *FindTag(LHtmlElement *e, HtmlTag id)
{
	if (!e)
		return nullptr;
	if (e->TagId == id)
		return e;
	for (ssize_t i = 0; i < e->Children.Length(); i++)
	{
		if (auto r = FindTag(e->Children[i], id))
			return r;
	}
	return nullptr;
}

static LHtmlElement *FindText(LHtmlElement *e)
{
	if (!e)
		return nullptr;
	if (e->GetText() && e->GetText()[0])
		return e;
	for (ssize_t i = 0; i < e->Children.Length(); i++)
	{
		if (auto r = FindText(e->Children[i]))
			return r;
	}
	return nullptr;
}

static LHtmlElement *FindTextOutside(LHtmlElement *e, LHtmlElement *exclude)
{
	if (!e || e == exclude)
		return nullptr;
	if (e->GetText() && e->GetText()[0])
		return e;
	for (ssize_t i = 0; i < e->Children.Length(); i++)
	{
		if (e->Children[i] == exclude)
			continue;
		if (auto r = FindTextOutside(e->Children[i], exclude))
			return r;
	}
	return nullptr;
}

class TestIframeEnv : public LDocumentEnv {
public:
	LoadType GetContent(LAutoPtr<LoadJob> &j) override {
		if (!j)
			return LoadError;

		const char *Content =
			"<!DOCTYPE html><html><head></head><body class='menu'>"
			"<table class='menu'><tr><td>Help Index<tr><td>Index"
			"<tr><td>Introduction</table></body></html>";
		LMemStream *Stream = new LMemStream(Content, strlen(Content));
		if (!Stream)
			return LoadError;

		Stream->SetPos(0);
		if (!j->Stream.Reset(Stream)) {
			DeleteObj(Stream);
			return LoadError;
		}
		return LoadImmediate;
	}
};

class TestIframeHtml : public Html2::LHtml {
public:
	using Html2::LHtml::LHtml;

	Html2::LTag *RootTag() { return Tag; }
};

bool TestIframeLoadedContent()
{
	TestIframeEnv env;
	TestIframeHtml html(0, 0, 0, 80, 60);
	html.SetEnv(&env);

	char doc[] = "<html><body><div style='position:fixed;height:100%;width:190px;'>"
		"<iframe style='width:190px;height:100%;' src='menu.html'>No iframe support.</iframe>"
		"</div><table><tr><td>Outside</td></tr></table></body></html>";
	if (!html.Name(doc))
		return false;
	html.Layout(true);

	LHtmlElement *root = html.RootTag();
	LHtmlElement *frame = FindTag(root, TAG_IFRAME);
	if (!frame) {
		printf("Iframe loaded content: no iframe element\n");
		return false;
	}

	if (frame->GetText()) {
		printf("Iframe loaded content: fallback text was parsed as content\n");
		return false;
	}

	if (!frame->Children.Length()) {
		printf("Iframe loaded content: loaded iframe content was not attached\n");
		return false;
	}

	LHtmlElement *loaded = FindText(frame);
	if (!loaded) {
		printf("Iframe loaded content: loaded content text was not parsed\n");
		return false;
	}

	auto frameTag = dynamic_cast<Html2::LTag*>(frame);
	auto loadedTag = dynamic_cast<Html2::LTag*>(loaded);
	if (!frameTag || frameTag->Size.y <= 0 ||
		!loadedTag || !loadedTag->TextPos.Length())
	{
		printf("Iframe loaded content: loaded content was not laid out\n");
		return false;
	}

	LMemDC screen(_FL, 80, 60, CsBgra32, LSurface::SurfaceRequireExactCs);
	if (!screen[0])
		return false;
	screen.Colour(LColour::White);
	screen.Rectangle();
	html.OnPaint(&screen);

	bool paintedText = false;
	for (auto run: loadedTag->TextPos)
	{
		int x1 = MAX(loadedTag->AbsX() + run->x1, 0);
		int y1 = MAX(loadedTag->AbsY() + run->y1, 0);
		int x2 = MIN(loadedTag->AbsX() + run->x2, 79);
		int y2 = MIN(loadedTag->AbsY() + run->y2, 59);
		for (int y = y1; y <= y2 && !paintedText; y++)
		{
			auto pixels = reinterpret_cast<const LBgra32*>(screen[y]);
			for (int x = x1; x <= x2; x++)
			{
				if (pixels[x].r < 100 && pixels[x].g < 100 && pixels[x].b < 100)
				{
					paintedText = true;
					break;
				}
			}
		}
	}
	if (!paintedText)
	{
		printf("Iframe loaded content: text was laid out but not painted\n");
		return false;
	}

	LHtmlElement *outside = FindTextOutside(root, frame);
	if (!outside) {
		printf("Iframe loaded content: text outside the iframe was not parsed\n");
		return false;
	}

	html.SetEnv(nullptr);
	return true;
}

bool TestIframeFallbackContent()
{
	TestIframeHtml html(0, 0, 0, 80, 60);
	char doc[] = "<html><body><div style='position:fixed;height:100%;width:190px;'>"
		"<iframe style='width:190px;height:100%;' src='menu.html'>No iframe support.</iframe>"
		"</div><table><tr><td>Outside</td></tr></table></body></html>";
	if (!html.Name(doc))
		return false;
	html.Layout(true);

	LHtmlElement *root = html.RootTag();
	LHtmlElement *frame = FindTag(root, TAG_IFRAME);
	if (!frame)
	{
		printf("Iframe fallback content: no iframe element\n");
		return false;
	}

	if (!FindText(frame))
	{
		printf("Iframe fallback content: fallback text was not parsed when no content was loaded\n");
		return false;
	}

	auto frameTag = dynamic_cast<Html2::LTag*>(frame);
	auto fallbackTag = dynamic_cast<Html2::LTag*>(FindText(frame));
	if (!frameTag || frameTag->Size.y <= 0 ||
		!fallbackTag || !fallbackTag->TextPos.Length())
	{
		printf("Iframe fallback content: fallback text was not laid out\n");
		return false;
	}

	LHtmlElement *outside = FindTextOutside(root, frame);
	if (!outside)
	{
		printf("Iframe fallback content: text outside the iframe was not parsed\n");
		return false;
	}

	return true;
}

static bool TestFragmentScroll()
{
	LUri section("/help/intro.html#section");
	LUri details("#more%20details");
	if (!section.sPath.Equals("/help/intro.html") ||
		!section.sAnchor.Equals("section") ||
		!section.ToString().Equals("/help/intro.html#section") ||
		!details.sAnchor.Equals("more details") ||
		!details.ToString().Equals("#more%20details"))
	{
		printf("Fragment scroll: URL did not round-trip\n");
		return false;
	}

	TestIframeHtml html(0, 0, 0, 320, 100);
	LMemDC screen(_FL, 320, 100, CsBgra32, LSurface::SurfaceRequireExactCs);
	if (!screen[0])
		return false;

	auto jump = [&](const char *doc, LUri &uri, HtmlTag tag)
	{
		if (!html.Name(doc) || !html.GotoAnchor(uri.sAnchor))
			return false;
		html.OnPaint(&screen);
		#if defined(__GTK_H__) || defined(HAIKU)
		// Deliver the scrollbar creation queued by layout in an offscreen view.
		auto scrollbar = CreateMsg(M_SET_SCROLL, 0, 1);
		html.OnEvent(&scrollbar);
		html.OnPaint(&screen);
		#endif

		auto target = static_cast<Html2::LTag*>(FindTag(html.RootTag(), tag));
		int lineHeight = html.GetFont()->GetHeight();
		int top = html.GetScrollPos().y * lineHeight;
		if (!target || top <= 0 || target->AbsY() < top || target->AbsY() - top >= lineHeight)
		{
			printf("Fragment scroll: viewport top %i does not match target %i\n",
				top, target ? target->AbsY() : -1);
			return false;
		}
		return true;
	};

	// Navigate twice without delay, so the second document inherits a scrollbar.
	return jump("<html><body><div style='height:350px'>Before</div>"
				"<div><a name='section'>Section</a></div>"
				"<div style='height:250px'>After</div></body></html>", section, TAG_A) &&
		   jump("<html><body><div style='height:900px'>Before</div>"
				"<h2 id='more details'>Details</h2>"
				"<div style='height:250px'>After</div></body></html>", details, TAG_H2);
}

static bool TestFormattedContent()
{
	Html2::LHtml html(0, 0, 0, 80, 60);
	const char *document = "<html><body>formatted content</body></html>";
	if (!html.Name(document))
		return false;

	LString content;
	if (!html.GetFormattedContent("text/html", content) ||
		content.Find("formatted content") < 0)
	{
		printf("Formatted content: HTML export failed\n");
		return false;
	}

	if (!html.GetFormattedContent("text/plain", content) ||
		content.Find("formatted content") < 0)
	{
		printf("Formatted content: text export failed\n");
		return false;
	}

	if (html.GetFormattedContent("application/json", content))
	{
		printf("Formatted content: unsupported MIME type was accepted\n");
		return false;
	}

	return true;
}

#if defined(__GTK_H__)
template<typename Pixel>
static bool TestZoomOpaque32(LColourSpace space, bool exact)
{
	LMemDC source(_FL);
	if (!source.Create(70, 66, space, exact ? LSurface::SurfaceRequireExactCs : 0))
		return false;
	if (source.GetColourSpace() != (exact ? space : CsBgrx32))
		return false;
	for (int y = 0; y < source.Y(); y++)
	{
		auto row = (Pixel*)source[y];
		for (int x = 0; x < source.X(); x++)
		{
			row[x].r = (x * 3 + 17) % 256;
			row[x].g = (y * 5 + 29) % 256;
			row[x].b = (x + y + 71) % 256;
			row[x].pad = 0;
		}
	}

	LZoomView zoom(nullptr);
	LRect bounds(0, 0, 149, 149);
	zoom.SetPos(bounds);
	zoom.SetSurface(&source, false);
	for (int level : {1, 2})
	{
		zoom.SetZoom(level);
		LMemDC output(_FL, 150, 150, System32BitColourSpace);
		zoom.OnPaint(&output);
		int factor = level + 1;
		auto client = zoom.GetClient();
		for (int y : {0, 1, 42, 49, 63, 65})
			for (int x : {0, 1, 42, 49, 63, 64, 69})
			{
				if (x * factor + factor - 1 > client.x2 ||
					y * factor + factor - 1 > client.y2)
					continue;
				for (int dy : {0, factor - 1})
					for (int dx : {0, factor - 1})
					{
						LColour actual(output.Get(x * factor + dx, y * factor + dy), output.GetBits());
						if (actual.r() != (x * 3 + 17) % 256 ||
							actual.g() != (y * 5 + 29) % 256 ||
							actual.b() != (x + y + 71) % 256 ||
							actual.a() != 255)
							return false;
					}
			}
	}
	return true;
}

static bool TestZoomOpaque32()
{
	return TestZoomOpaque32<LBgrx32>(System24BitColourSpace, false) &&
		TestZoomOpaque32<LBgrx32>(CsBgrx32, true) &&
		TestZoomOpaque32<LRgbx32>(CsRgbx32, true) &&
		TestZoomOpaque32<LXbgr32>(CsXbgr32, true) &&
		TestZoomOpaque32<LXrgb32>(CsXrgb32, true);
}
#endif

#if defined(__GTK_H__)
namespace
{
	using namespace Gtk;

	class HtmlPdfView : public Html2::LHtml
	{
	public:
		using Html2::LHtml::LHtml;

		bool HasPrintFonts(LSurface *surface)
		{
			if (!FontCache)
				return false;
			bool found = false;
			for (int i = 0; auto font = FontCache->FontAt(i); i++)
			{
				if (font->GetSurface() != surface)
					return false;
				found = true;
			}
			return found;
		}

	};

	struct HtmlPdfPrint
	{
		HtmlPdfView *Html = nullptr;
		LString Content;
		LAutoPtr<LPrintDC> PrintDC;
		LRect Printable;
		int PageHeight = 0;
		int LineHeight = 0;
		int PageStep = 0;
		double Scale = 1.0;
		bool Failed = false;
		bool TestBitmap = false;
		LString BitmapOutput;
	};

	bool TestPrintGeometry()
	{
		auto setup = gtk_page_setup_new();
		gtk_page_setup_set_top_margin(setup, 10, GTK_UNIT_POINTS);
		gtk_page_setup_set_bottom_margin(setup, 20, GTK_UNIT_POINTS);
		gtk_page_setup_set_left_margin(setup, 30, GTK_UNIT_POINTS);
		gtk_page_setup_set_right_margin(setup, 40, GTK_UNIT_POINTS);
		bool passed = true;
		struct Case
		{
			GtkPageOrientation orientation;
			bool fullPage;
			LRect expected;
		};
		Case cases[] = {
			{GTK_PAGE_ORIENTATION_PORTRAIT, false, LRect(5, 2, 488, 592)},
			{GTK_PAGE_ORIENTATION_LANDSCAPE, false, LRect(7, 5, 497, 588)},
			{GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT, false, LRect(11, 7, 494, 597)},
			{GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE, false, LRect(2, 11, 492, 594)},
			{GTK_PAGE_ORIENTATION_PORTRAIT, true, LRect(35, 12, 448, 572)},
		};
		for (auto &c : cases)
		{
			gtk_page_setup_set_orientation(setup, c.orientation);
			auto rect = LGtkPrintableRect(setup, 500.5, 600.5, 12, 27, 35, 51, c.fullPage);
			if (rect != c.expected)
			{
				fprintf(stderr, "Printable rect mismatch for orientation %d, full page %d: %d,%d-%d,%d\n",
					c.orientation, c.fullPage, rect.x1, rect.y1, rect.x2, rect.y2);
				passed = false;
			}
		}
		g_object_unref(setup);
		return passed;
	}

	bool TestPrintFontDpi(HtmlPdfPrint *print)
	{
		auto context = print->PrintDC->GetPrintContext();
		double originalDpiX = gtk_print_context_get_dpi_x(context);
		double originalDpiY = gtk_print_context_get_dpi_y(context);
		auto cr = cairo_reference(print->PrintDC->Handle());
		cairo_save(cr);
		int firstHeight = 0, firstWidth = 0, firstInkWidth = 0, firstInkHeight = 0;
		bool passed = true;
		for (int dpi : {72, 300, 600})
		{
			gtk_print_context_set_cairo_context(context, cr, dpi, dpi);
			LFont font;
			if (!font.Create("DejaVu Sans", LCss::Len(LCss::LenPt, 12), print->PrintDC))
			{
				passed = false;
				break;
			}
			LDisplayString text(&font, "Font size");
			int height = font.GetHeight();
			int width = text.X();
			if (!firstHeight)
			{
				firstHeight = height;
				firstWidth = width;
			}
			if (pango_font_description_get_size(font.Handle()) != 12 * PANGO_SCALE ||
				height < 12 || height > 18 || width < 40 || width > 75 ||
				std::abs(height - firstHeight) > 1 || std::abs(width - firstWidth) > 1)
			{
				fprintf(stderr, "Print font metrics at %d DPI: height %d, width %d (expected %d, %d)\n",
					dpi, height, width, firstHeight, firstWidth);
				passed = false;
			}

			font.Transparent(true);
			font.Colour(L_TEXT, L_WORKSPACE);
			LAutoPtr<LMemDC> image(print->PrintDC->RenderPageToBitmap(LPoint(300, 300),
				[&text](LPrintDC *dc)
				{
					text.Draw(dc, 20, 20);
					return true;
				}));
			if (!image)
			{
				passed = false;
				continue;
			}
			int x1 = image->X(), y1 = image->Y(), x2 = -1, y2 = -1;
			for (int y = 0; y < 300; y++)
				for (int x = 0; x < 600; x++)
				{
					LColour c(image->Get(x, y), image->GetBits());
					if (c.r() < 128 && c.g() < 128 && c.b() < 128)
					{
						x1 = MIN(x1, x);
						y1 = MIN(y1, y);
						x2 = MAX(x2, x);
						y2 = MAX(y2, y);
					}
				}
			int inkWidth = x2 - x1 + 1, inkHeight = y2 - y1 + 1;
			if (x2 < x1 || y2 < y1)
				passed = false;
			else if (!firstInkWidth)
			{
				firstInkWidth = inkWidth;
				firstInkHeight = inkHeight;
			}
			else if (std::abs(inkWidth - firstInkWidth) > 1 ||
				std::abs(inkHeight - firstInkHeight) > 1)
			{
				fprintf(stderr, "Print font ink at %d DPI: %d x %d (expected %d x %d)\n",
					dpi, inkWidth, inkHeight, firstInkWidth, firstInkHeight);
				passed = false;
			}

			LFont pixelFont;
			if (!pixelFont.Create("DejaVu Sans", LCss::Len(LCss::LenPx, 16), print->PrintDC) ||
				pango_font_description_get_size(pixelFont.Handle()) != 12 * PANGO_SCALE)
				passed = false;
			else
			{
				LDisplayString pixelText(&pixelFont, "Font size");
				if (std::abs(pixelFont.GetHeight() - firstHeight) > 1 ||
					std::abs(pixelText.X() - firstWidth) > 1)
					passed = false;
			}
		}
		gtk_print_context_set_cairo_context(context, cr, originalDpiX, originalDpiY);
		cairo_restore(cr);
		cairo_destroy(cr);
		return passed;
	}

	bool TestPrintImages(HtmlPdfPrint *print)
	{
		LMemDC opaque(_FL, 8, 8, CsBgrx32);
		LMemDC rgba(_FL, 8, 8, CsRgba32, LSurface::SurfaceRequireExactCs);
		LMemDC transparent(_FL, 8, 8, CsBgra32);
		LMemDC empty(_FL);
		if (!opaque[0] || !rgba[0] || !transparent[0])
			return false;
		opaque.Colour(LColour::Blue);
		opaque.Rectangle();
		opaque.Colour(LColour::Red);
		opaque.Rectangle(4, 4, 7, 7);
		rgba.Colour(LColour(0, 255, 0));
		rgba.Rectangle();
		transparent.ConvertPreMulAlpha(false);
		for (int y = 0; y < 8; y++)
			for (int x = 0; x < 8; x++)
			{
				auto &pixel = reinterpret_cast<LBgra32*>(transparent[y])[x];
				pixel.r = 200; pixel.g = 0; pixel.b = 0; pixel.a = 128;
			}
		LAutoPtr<LMemDC> image(print->PrintDC->RenderPageToBitmap(LPoint(72, 72),
			[&](LPrintDC *dc)
			{
				LRect source(4, 4, 7, 7), dest(20, 20, 35, 35);
				dc->StretchBlt(&dest, &opaque, &source);
				dc->Blt(40, 20, &rgba);
				dc->Blt(60, 20, &transparent);
				// Failed image allocation must not assert or poison the Cairo target.
				dc->Blt(80, 20, &empty);
				dc->SetOrigin(-100, -100);
				dc->SetDefaultClip(LRect(100, 100, 103, 103));
				dc->Blt(0, 0, &opaque);
				return cairo_status(dc->Handle()) == CAIRO_STATUS_SUCCESS;
			}));
		if (!image)
			return false;
		auto colour = [&](int x, int y) { return LColour(image->Get(x, y), image->GetBits()); };
		auto red = colour(28, 28), green = colour(44, 24), alpha = colour(64, 24);
		auto blue = colour(101, 101), outside = colour(105, 105);
		if (red.r() < 240 || red.g() > 15 || red.b() > 15 ||
			green.g() < 240 || green.r() > 15 || green.b() > 15 ||
			std::abs(alpha.r() - 227) > 2 || std::abs(alpha.g() - 127) > 2 ||
			std::abs(alpha.b() - 127) > 2 ||
			blue.b() < 240 || blue.r() > 15 || blue.g() > 15 ||
			outside.r() < 240 || outside.g() < 240 || outside.b() < 240)
		{
			fprintf(stderr, "Print image pixels failed: red=%d,%d,%d green=%d,%d,%d alpha=%d,%d,%d blue=%d,%d,%d outside=%d,%d,%d\n",
				red.r(), red.g(), red.b(), green.r(), green.g(), green.b(),
				alpha.r(), alpha.g(), alpha.b(), blue.r(), blue.g(), blue.b(),
				outside.r(), outside.g(), outside.b());
			return false;
		}
		LDefaultDocumentEnv env;
		TestIframeHtml html(0, 0, 0, 100, 100, &env);
		if (!html.SetFontSurface(print->PrintDC) ||
			!html.Name("<html><body style='margin:0'><img width='8' height='8'></body></html>"))
			return false;
		auto tag = static_cast<Html2::LTag*>(FindTag(html.RootTag(), TAG_IMG));
		if (!tag)
			return false;
		tag->SetImage("test-empty-image", LAutoPtr<LSurface>(new LMemDC(_FL)));
		if (tag->Image)
		{
			fprintf(stderr, "HTML retained an unreadable image\n");
			return false;
		}
		LAutoPtr<LSurface> loaded(new LMemDC(_FL, 4, 4, CsBgrx32));
		if (!loaded || !(*loaded)[0])
			return false;
		loaded->Colour(LColour::Blue);
		loaded->Rectangle();
		tag->SetImage("test-loaded-image", loaded);
		html.Layout(true);
		LAutoPtr<LMemDC> htmlImage(print->PrintDC->RenderPageToBitmap(LPoint(72, 72),
			[&](LPrintDC *dc)
			{
				html.OnPaint(dc);
				return !html.GetMaxPaintTimeout();
			}));
		if (!htmlImage || !tag->Image || tag->Image->X() != 8 || tag->Image->Y() != 8)
			return false;
		LColour htmlPixel(htmlImage->Get(4, 4), htmlImage->GetBits());
		if (htmlPixel.b() < 240 || htmlPixel.r() > 15 || htmlPixel.g() > 15)
		{
			fprintf(stderr, "HTML image did not reach print bitmap\n");
			return false;
		}
		return true;
	}

	bool TestPrintBitmap(HtmlPdfPrint *print)
	{
		LRect printable;
		if (!TestPrintGeometry() || !TestPrintImages(print) || !print->PrintDC->GetPrintableRect(printable) ||
			printable != print->PrintDC->Bounds() ||
			!print->Html->HasPrintFonts(print->PrintDC) || !TestPrintFontDpi(print))
			return false;
		auto originalHandle = print->PrintDC->Handle();
		auto originalOrigin = print->PrintDC->GetOrigin();
		for (int dpi : {300, 600})
		{
			LAutoPtr<LMemDC> image(print->PrintDC->RenderPageToBitmap(LPoint(dpi, dpi),
				[print](LPrintDC *dc)
				{
					print->Html->SetVScroll(0);
					print->Html->OnPaint(dc);
					if (print->Html->GetMaxPaintTimeout())
						return false;
					dc->SetOrigin(10, 10);
					dc->Colour(LColour::Red);
					dc->Rectangle(200, 200, 209, 209);
					return true;
				}));
			if (!image ||
				image->X() != (int)ceil(print->PrintDC->X() * dpi / 72.0) ||
				image->Y() != (int)ceil(print->PrintDC->Y() * dpi / 72.0))
				return false;
			if (dpi == 300 && !GdcD->Save(print->BitmapOutput, image))
				return false;

			int marker = (int)(195.0 * dpi / 72.0);
			LColour red(image->Get(marker, marker), image->GetBits());
			if (red.r() < 240 || red.g() > 15 || red.b() > 15)
				return false;

			int darkPixels = 0;
			int greenPixels = 0;
			int bluePixels = 0;
			for (int y = 0; y < (int)(80.0 * dpi / 72.0); y++)
				for (int x = 0; x < (int)(300.0 * dpi / 72.0); x++)
				{
					LColour c(image->Get(x, y), image->GetBits());
					if (c.r() < 80 && c.g() < 80 && c.b() < 80)
						darkPixels++;
					if (c.r() < 30 && c.g() > 225 && c.b() < 30)
						greenPixels++;
					if (c.r() < 30 && c.g() < 30 && c.b() > 225)
						bluePixels++;
				}
			if (darkPixels < 20 || greenPixels < 20 || bluePixels < 20)
				return false;
		}

		LAutoPtr<LMemDC> invalid(print->PrintDC->RenderPageToBitmap(LPoint(0, 300),
			[](LPrintDC*) { return true; }));
		LAutoPtr<LMemDC> rejected(print->PrintDC->RenderPageToBitmap(LPoint(300, 300),
			[](LPrintDC*) { return false; }));
		LAutoPtr<LMemDC> transformed(print->PrintDC->RenderPageToBitmap(LPoint(72, 72),
			[](LPrintDC *dc)
			{
				auto cr = dc->Handle();
				cairo_translate(cr, 30, 40);
				dc->SetHandle(cr);
				dc->SetOrigin(10, 10);
				dc->Colour(LColour::Red);
				dc->Rectangle(200, 200, 209, 209);
				return true;
			}));
		if (!transformed)
			return false;
		LColour marker(transformed->Get(225, 235), transformed->GetBits());
		LAutoPtr<LMemDC> clipped(print->PrintDC->RenderPageToBitmap(LPoint(72, 72),
			[](LPrintDC *dc)
			{
				auto cr = dc->Handle();
				cairo_save(cr);
				cairo_rectangle(cr, 50, 80, 100, 100);
				cairo_clip(cr);
				cairo_translate(cr, 50, 80);
				cairo_scale(cr, 0.5, 0.5);
				dc->SetHandle(cr);
				dc->SetDefaultClip(LRect(0, 0, 199, 199));
				dc->SetOrigin(0, 40);
				dc->Colour(LColour::Red);
				dc->Rectangle(40, 100, 59, 119);
				dc->Rectangle(200, 200, 240, 240);
				cairo_restore(cr);
				return true;
			}));
		if (!clipped)
			return false;
		LColour inside(clipped->Get(75, 115), clipped->GetBits());
		LColour outside(clipped->Get(155, 165), clipped->GetBits());
		return marker.r() > 240 && marker.g() < 15 && marker.b() < 15 &&
			inside.r() > 240 && inside.g() < 15 && inside.b() < 15 &&
			outside.r() > 240 && outside.g() > 240 && outside.b() > 240 &&
			!invalid && !rejected &&
			print->PrintDC->Handle() == originalHandle &&
			print->PrintDC->GetOrigin() == originalOrigin;
	}

	void HtmlPdfBeginPrint(GtkPrintOperation *operation, GtkPrintContext *context, gpointer data)
	{
		auto print = (HtmlPdfPrint*)data;
		print->PrintDC.Reset(new LPrintDC(context, "HtmlTestSuite"));
		gboolean fullPage = false;
		g_object_get(G_OBJECT(operation), "use-full-page", &fullPage, nullptr);
		print->PrintDC->SetFullPage(fullPage);
		if (!print->PrintDC || !print->PrintDC->GetPrintableRect(print->Printable))
		{
			print->Failed = true;
			gtk_print_operation_cancel(operation);
			return;
		}

		if (!print->Html->SetFontSurface(print->PrintDC) ||
			!print->Html->Name(print->Content))
		{
			print->Failed = true;
			gtk_print_operation_cancel(operation);
			return;
		}

		LRect pos = print->Html->GetPos();
		pos.SetSize(print->Printable.X(), print->Printable.Y());
		print->Html->SetPos(pos);
		print->PageHeight = print->Printable.Y();

		LPoint content = print->Html->Layout(true);
		if (print->TestBitmap)
		{
			if (!print->Html->HasPrintFonts(print->PrintDC) ||
				!print->Html->Name(print->Content))
			{
				fprintf(stderr, "Print font cache did not survive document loading\n");
				print->Failed = true;
				gtk_print_operation_cancel(operation);
				return;
			}
			content = print->Html->Layout(true);
		}
		print->LineHeight = print->Html->GetFont()->GetHeight();
		if (print->LineHeight <= 0)
		{
			print->Failed = true;
			gtk_print_operation_cancel(operation);
			return;
		}
		print->Scale = content.x > print->Printable.X() ?
			(double)print->Printable.X() / content.x : 1.0;
		int available = (int)(print->PageHeight / print->Scale);
		print->PageStep = MAX((available / print->LineHeight) * print->LineHeight, print->LineHeight);
		int pages = MAX((content.y + print->PageStep - 1) / print->PageStep, 1);
		gtk_print_operation_set_n_pages(operation, pages);
		if (print->TestBitmap && !TestPrintBitmap(print))
		{
			fprintf(stderr, "Print bitmap test failed\n");
			print->Failed = true;
			gtk_print_operation_cancel(operation);
		}
	}

	void HtmlPdfDrawPage(GtkPrintOperation*, GtkPrintContext *context, gint page, gpointer data)
	{
		auto print = (HtmlPdfPrint*)data;
		print->PrintDC->SetHandle(gtk_print_context_get_cairo_context(context));
		print->PrintDC->Colour(LColour::White);
		print->PrintDC->Rectangle();
		auto cr = print->PrintDC->Handle();
		cairo_save(cr);
		cairo_rectangle(cr, print->Printable.x1, print->Printable.y1,
			print->Printable.X(), print->Printable.Y());
		cairo_clip(cr);
		cairo_translate(cr, print->Printable.x1, print->Printable.y1);
		cairo_scale(cr, print->Scale, print->Scale);
		print->PrintDC->SetHandle(cr);
		LRect htmlClip(0, 0, (int)ceil(print->Printable.X() / print->Scale) - 1,
			(int)ceil(print->Printable.Y() / print->Scale) - 1);
		print->PrintDC->SetDefaultClip(htmlClip);
		int scrollLines = page * print->PageStep / print->LineHeight;
		print->Html->SetVScroll(0);
		print->PrintDC->SetOrigin(0, scrollLines * print->LineHeight);
		print->Html->OnPaint(print->PrintDC);
		cairo_restore(cr);
		print->PrintDC->SetHandle(cr);
		print->PrintDC->SetDefaultClip(print->Printable);
		if (print->TestBitmap)
		{
			LMemDC source(_FL, 8, 8, CsRgba32, LSurface::SurfaceRequireExactCs);
			if (!source[0])
			{
				print->Failed = true;
				return;
			}
			source.Colour(LColour::Blue);
			source.Rectangle();
			print->PrintDC->Blt(300, 200, &source);
		}
	}

	int PrintHtmlContentToPdf(const LString &content, const char *output, bool testBitmap)
	{
		if (!content || !ValidStr(output))
		{
			fprintf(stderr, "Missing HTML content or PDF output path\n");
			return 1;
		}

		LDefaultDocumentEnv env;
		HtmlPdfView html(0, 0, 0, 800, 600, &env);

		HtmlPdfPrint print;
		print.Html = &html;
		print.Content = content;
		print.TestBitmap = testBitmap;
		if (testBitmap)
			print.BitmapOutput = LString::Fmt("%s.preview.png", output);
		auto operation = gtk_print_operation_new();
		if (!operation)
		{
			fprintf(stderr, "Failed to create PDF print operation\n");
			return 1;
		}

		g_signal_connect(G_OBJECT(operation), "begin-print", G_CALLBACK(HtmlPdfBeginPrint), &print);
		g_signal_connect(G_OBJECT(operation), "draw-page", G_CALLBACK(HtmlPdfDrawPage), &print);
		gtk_print_operation_set_export_filename(operation, output);
		gtk_print_operation_set_unit(operation, GTK_UNIT_POINTS);
		if (testBitmap)
		{
			auto setup = gtk_page_setup_new();
			gtk_page_setup_set_left_margin(setup, 18, GTK_UNIT_POINTS);
			gtk_page_setup_set_right_margin(setup, 27, GTK_UNIT_POINTS);
			gtk_page_setup_set_top_margin(setup, 36, GTK_UNIT_POINTS);
			gtk_page_setup_set_bottom_margin(setup, 45, GTK_UNIT_POINTS);
			gtk_print_operation_set_default_page_setup(operation, setup);
			g_object_unref(setup);
		}
		GError *error = nullptr;
		auto result = gtk_print_operation_run(
			operation,
			GTK_PRINT_OPERATION_ACTION_EXPORT,
			nullptr,
			&error);
		g_object_unref(operation);

		if (error)
		{
			fprintf(stderr, "Failed to export PDF: %s\n", error->message);
			g_error_free(error);
		}
		if (print.Failed || result != GTK_PRINT_OPERATION_RESULT_APPLY)
		{
			fprintf(stderr, "PDF export failed for: %s\n", output);
			return 1;
		}

		printf("PDF: %s\n", output);
		return 0;
	}

	int PrintHtmlToPdf(const char *fileName)
	{
		if (!LFileExists(fileName))
		{
			fprintf(stderr, "HTML file does not exist: %s\n", fileName);
			return 1;
		}

		auto content = LReadFile(fileName);
		if (!content)
		{
			fprintf(stderr, "Failed to read HTML file: %s\n", fileName);
			return 1;
		}

		LString output = LString::Fmt("%s.pdf", fileName);
		return PrintHtmlContentToPdf(content, output);
	}
}
#endif

int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, "HtmlTestSuite");
	if (a.IsOk())
	{
		#if defined(__GTK_H__)
		if (a.GetOption("test-zoom-opaque32"))
		{
			bool passed = TestZoomOpaque32();
			printf("Opaque 32-bit zoom: %s\n", passed ? "PASS" : "FAIL");
			return passed ? 0 : 1;
		}
		#endif
		LString printFile;
		#if defined(__GTK_H__)
		if (a.GetOption("test-print-bitmap", printFile))
			return PrintHtmlContentToPdf(
				"<html><body style='background-color:white;color:black'>"
				"<div style='font-size:12pt;background-color:#00ff00;border:2px solid #0000ff;width:150px;height:40px'>"
				"Preview text sample</div></body></html>",
				printFile, true);
		#endif
		if (a.GetOption("print", printFile))
		{
			#if defined(__GTK_H__)
				return PrintHtmlToPdf(printFile);
			#else
				fprintf(stderr, "PDF export is only supported by the GTK build\n");
				return 1;
			#endif
		}

		if (a.GetOption("test-fragment-scroll"))
		{
			bool passed = TestFragmentScroll();
			printf("Fragment scroll: %s\n", passed ? "PASS" : "FAIL");
			return passed ? 0 : 1;
		}

		if (a.GetOption("test-formatted-content"))
		{
			bool passed = TestFormattedContent();
			printf("Formatted content: %s\n", passed ? "PASS" : "FAIL");
			return passed ? 0 : 1;
		}

		if (a.GetOption("test-iframe-background"))
		{
			bool passed = TestIframeBodyBackground();
			printf("Iframe body background: %s\n", passed ? "PASS" : "FAIL");
			return passed ? 0 : 1;
		}

		if (a.GetOption("test-iframe-fallback"))
		{
			bool passed = TestIframeFallbackContent();
			printf("Iframe fallback content: %s\n", passed ? "PASS" : "FAIL");
			return passed ? 0 : 1;
		}

		if (a.GetOption("test-iframe-loaded"))
		{
			bool passed = TestIframeLoadedContent();
			printf("Iframe loaded content: %s\n", passed ? "PASS" : "FAIL");
			return passed ? 0 : 1;
		}

	    printf("Usage: %s [args]\n"
	        "\n"
	        "   -test-rounded-borders   Run the rounded borders test\n"
	        "   -test-iframe-background Run the iframe body background test\n"
	        "   -test-iframe-fallback   Run the iframe fallback content test\n"
	        "   -test-iframe-loaded     Run the iframe loaded content test\n"
	        "   -test-fragment-scroll   Run the fragment navigation test\n"
	        "   -test-formatted-content Check HTML and text export results\n"
	        "   -test-zoom-opaque32     Check zooming opaque 32-bit images (GTK only)\n"
	        "   -test-print-bitmap <file> Test page capture and export PDF (GTK only)\n"
	        "   -print <fileName>       Export the HTML file to <fileName>.pdf (GTK only)\n"
	        "   -render <fileName>      Render the file to the 'render.png'\n",
	        AppArgs.Arg[0]);
	
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
