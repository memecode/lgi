#include "lgi/common/Lgi.h"
#include "lgi/common/Html.h"
#include "lgi/common/List.h"
#include "lgi/common/DateTime.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Scripting.h"
#include "lgi/common/Box.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/Combo.h"
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
#include "../src/common/Text/HtmlPriv.h"
#include "lgi/common/Http.h"

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

#ifdef _LHTML2_H
using THtmlClass = Html2::LHtml2;
using THtmlTag = Html2::LTag;
#else
using THtmlClass = Html1::LHtml;
using THtmlTag = Html1::LTag;
#endif

class HtmlScriptContext;
struct AppContext
{
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
			}

			return "NULL";
		}

		void OnSelect()
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
						p.Print("text: %S\n", utf.Get());
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

				auto detail = p.NewLStr();
				context->tagDetail->Name(detail);

				// Repaint the html control to show the active selection:
				html->Invalidate();
			}
			else context->tagDetail->Name("null tag");
		}
	};
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

	LHostFunc *GetCommands()
	{
		return Methods;
	}
	
	void SetEngine(LScriptEngine *Eng)
	{		
	}
	
	LString GetIncludeFile(const char *FileName)
	{
		return LString();
	}
	
	LAutoString GetDataFolder()
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
};

LHostFunc HtmlScriptContext::Methods[] =
{
	LHostFunc(0, 0, 0),
};

class HtmlImageLoader : public LThread, public LMutex, public LCancel
{
	LArray<LDocumentEnv::LoadJob*> In;

public:
	HtmlImageLoader() :
		LThread("HtmlImageLoader.Thread"),
		LMutex("HtmlImageLoader.Mutex")
	{
		Run();
	}
	
	~HtmlImageLoader()
	{
		Cancel(true);
		while (!IsExited())
			LSleep(1);
	}
	
	void Add(LAutoPtr<LDocumentEnv::LoadJob> j)
	{
		if (Lock(_FL))
		{
			In.Add(j.Release());
			Unlock();
		}
	}
	
	LAutoPtr<LSocketI> CreateSock(const char *Proto)
	{
		LAutoPtr<LSocketI> s;
		if (Proto && !_stricmp(Proto, "https"))
		{
			SslSocket *ss;
			s.Reset(ss = new SslSocket);
			ss->SetSslOnConnect(false);
		}
		else
			s.Reset(new LSocket);
		
		return s;
	}
	
	int Main()
	{
		while (!IsCancelled())
		{
			LAutoPtr<LThreadJob> j;
			if (Lock(_FL))
			{
				if (In.Length())
				{
					j.Reset(In[0]);
					In.DeleteAt(0, true);
				}
				Unlock();
			}
			
			LDocumentEnv::LoadJob *Job = dynamic_cast<LDocumentEnv::LoadJob*>(j.Get());
			if (Job)
			{
				LUri u(Job->Uri);
				if (u.IsFile())
				{
					// Local document?
					if (Job->pDC.Reset(GdcD->Load(Job->Uri)))
					{
						LDocumentEnv *e = Job->Env;
						if (e)
						{
							// LgiTrace("Loaded '%s' as image %ix%i\n", u.Path, j->pDC->X(), j->pDC->Y());
							e->OnDone(j);
						}
					}
				}
				else
				{
					LMemQueue p(1024);
					LError Err;
					auto r = LGetUri(this, &p, &Err, Job->Uri);
					if (r)
					{
						auto Hint = p.Peek(16);
						auto Filter = LFilterFactory::New(u.sPath, FILTER_CAP_READ, (uchar*)Hint.Get());
						if (Filter)
						{
							LAutoPtr<LSurface> Img(new LMemDC(_FL));
							LFilter::IoStatus Rd = Filter->ReadImage(Img, &p);
							if (Rd == LFilter::IoSuccess)
							{
								Job->pDC = Img;
								if (Job->Env)
								{
									// LgiTrace("Loaded '%s' as image %ix%i\n", u.Path, j->pDC->X(), j->pDC->Y());
									Job->Env->OnDone(j);
								}
								else
								{
									LgiTrace("%s:%i - No env for '%s'\n", _FL, u.sPath.Get());
									LAssert(0);
								}
							}
							else LgiTrace("%s:%i - Failed to read '%s'\n", _FL, u.sPath.Get());
						}
						else LgiTrace("%s:%i - Failed to find filter for '%s'\n", _FL, u.sPath.Get());
					}
					else LgiTrace("%s:%i - Failed to get '%s'\n", _FL, Job->Uri.Get());
				}
			}
			else LSleep(10);
		}
	
		return 0;
	}
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
	LTabView *tabs = nullptr;
	LBox *debugBox = nullptr;
    LTextView3 *Text = nullptr;
	LString FilesFolder;
	LAutoPtr<LScriptEngine> Script;
	LAutoPtr<HtmlImageLoader> Worker;
	LAutoPtr<LEmojiFont> Emoji;

	LoadType GetContent(LAutoPtr<LoadJob> &j)
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
			Worker.Reset(new HtmlImageLoader);
		Worker->Add(j);
		return LoadDeferred;
		#else
		return LoadNotImpl; // GDefaultDocumentEnv::GetContent(j);
		#endif
	}

	bool NeedsCapability(const char *Name, const char *Param) override
	{
		RunCallback([this, Name=LString::Fmt("Needs capability '%s'", Name)]()
		{
			LPopupNotification::Message(this, Name);
		});
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

				Script.Reset(new LScriptEngine(this, Html, NULL));

				if (Html)
					Html->SetEnv(this);
				#if HAS_IMAGE_LOADER
					Html->SetLoadImages(true);
				#endif
				
				LFile::Path files(LSP_APP_INSTALL);
				files += "../files";
				if (files.Exists())
				{
					FilesFolder = files.GetFull();

					List<FileInf> Files;
					LDirectory d;
					for (auto b = d.First(FilesFolder); b; b = d.Next())
					{
						if (!d.IsDir() && MatchStr("*.html", d.GetName()))
						{
							char p[256];
							if (d.Path(p, sizeof(p)))
							{
								FileInf *f = new FileInf;
								if (f)
								{
									f->File = p;
									f->Date.Set(d.GetLastWriteTime());
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
						char *d = strrchr(f->File, DIR_CHAR);
						if (d)
						{
							HtmlItem *i = new HtmlItem(FilesFolder, d + 1);
							if (i)
								Lst->Insert(i);
						}
					}

					Files.DeleteObjects();
				}
			}
			
			
			LRect r(0, 0, 1200, 800);
			SetPos(r);
			MoveToCenter();
			AttachChildren();
			
			Visible(true);
			OnNotify(FindControl(ID_LIST), LNotifyItemSelect);
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
			LThread("SaveImagesState", AddDispatch()),
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

	int OnCommand(int Cmd, int Event, OsView Wnd)
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

	bool OnCompileScript(LDocView *Parent, char *Script, const char *Language, const char *MimeType)
	{
		// return Script->Compile(Code, true);
		return false;
	}

	bool OnExecuteScript(LDocView *Parent, char *Script)
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

int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, "HtmlTestSuite");
	if (a.IsOk())
	{
		//FontSz();

		a.AppWnd = new AppWnd;
		a.Run();
	}
	return 0;
}
