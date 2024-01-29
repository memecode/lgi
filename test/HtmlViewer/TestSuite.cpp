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
#include "lgi/common/Http.h"
#include "lgi/common/Filter.h"
#include "lgi/common/ImageComparison.h"
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/Net.h"
#include "lgi/common/EmojiFont.h"
#include "lgi/common/Http.h"
#include "lgi/common/Menu.h"
#include "resdefs.h"

#define HAS_LOG_VIEW			0
#define HAS_IMAGE_LOADER		1

enum Controls
{
	IDC_HTML = 100,
	IDC_LOG,
	IDC_LIST
};

class FileInf
{
public:
	char *File;
	LDateTime Date;

	FileInf()
	{
		File = 0;
	}

	~FileInf()
	{
		DeleteArray(File);
	}
};

int InfCmp(FileInf *a, FileInf *b, NativeInt d)
{
	return -a->Date.Compare(&b->Date);
}

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
	#ifdef _GHTML2_H
	public Html2::LHtml2,
	#else
	public Html1::LHtml,
	#endif
	public LScriptContext
{
	LScriptEngine *Eng;
	
public:
	static LHostFunc Methods[];

	HtmlScriptContext(int Id, LDocumentEnv *Env) :
		#ifdef _GHTML2_H
		Html2::LHtml2
		#else
		Html1::LHtml
		#endif
		(Id, 0, 0, 100, 100, Env)
	{
		Eng = NULL;
		
		LFont *f = new LFont;
		if (f)
		{
			f->Face("Times New Roman");
			f->PointSize(12); // Size '16' is about 12 pt. Maybe they mean 16px?
			SetFont(f, true);
		}
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
			In.Add(j);
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
					LString Err;
					auto r = LgiGetUri(this, &p, &Err, Job->Uri);
					if (r)
					{
						uchar Hint[16];
						p.Peek(Hint, sizeof(Hint));
						auto Filter = LFilterFactory::New(u.sPath, FILTER_CAP_READ, Hint);
						if (Filter)
						{
							LAutoPtr<LSurface> Img(new LMemDC);
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

class AppWnd : public LWindow, public LDefaultDocumentEnv
{
	LList *Lst;
    HtmlScriptContext *Html;
    LTextView3 *Text;
	char Base[256];
	LAutoPtr<LScriptEngine> Script;
	LAutoPtr<HtmlImageLoader> Worker;
	LAutoPtr<LEmojiFont> Emoji;

	LoadType GetContent(LAutoPtr<LoadJob> &j)
	{
		LUri u(j->Uri);
		if (!u.sProtocol)
		{
			char p[MAX_PATH_LEN];
			if (LMakePath(p, sizeof(p), Base, j->Uri) &&
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

public:
	AppWnd()
	{
		Html = 0;
		Lst = 0;
		Text = NULL;
		Base[0] = 0;
		Emoji.Reset(new LEmojiFont());
		
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
			
			LBox *s = new LBox;
			if (s)
			{
				AddView(s);
				s->AddView(Lst = new LList(IDC_LIST, 0, 0, 100, 100));
				Lst->Sunken(false);
				Lst->AddColumn("File", 400);
				Lst->CssStyles("width: 200px;");
				s->Value(200);

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
				s->AddView(Html = new HtmlScriptContext(IDC_HTML, this));
				#endif

				Script.Reset(new LScriptEngine(this, Html, NULL));

				if (Html)
					Html->SetEnv(this);
				#if HAS_IMAGE_LOADER
				Html->SetLoadImages(true);
				#endif
				
				if (sprintf_s(Base, sizeof(Base), "%s", LGetExePath().Get()) > 0)
				{
					#if defined(WIN32)
					if (stristr(Base, "Release") || stristr(Base, "Debug"))
						LTrimDir(Base);
					#endif
					#if defined(MAC) && defined(__GTK_H__)
					LMakePath(Base, sizeof(Base), Base, "../../../..");
					#endif

					List<FileInf> Files;
					LDirectory *d = new LDirectory;
					if (d)
					{
						LMakePath(Base, sizeof(Base), Base, "Files");
						for (bool b = d->First(Base); b; b = d->Next())
						{
							if (!d->IsDir() && MatchStr("*.html", d->GetName()))
							{
								char p[256];
								if (d->Path(p, sizeof(p)))
								{
									FileInf *f = new FileInf;
									if (f)
									{
										f->File = NewStr(p);
										f->Date.Set(d->GetLastWriteTime());
										Files.Insert(f);
									}
								}
							}
						}
						DeleteObj(d);
						Files.Sort(InfCmp);

						for (auto f: Files)
						{
							char *d = strrchr(f->File, DIR_CHAR);
							if (d)
							{
								HtmlItem *i = new HtmlItem(Base, d + 1);
								if (i)
									Lst->Insert(i);
							}
						}

						Files.DeleteObjects();
					}
				}
			}
			
			
			LRect r(0, 0, 1200, 800);
			SetPos(r);
			MoveToCenter();
			AttachChildren();
			
			Visible(true);
			OnNotify(FindControl(IDC_LIST), LNotifyItemSelect);
		}
		else LExitApp();
	}

	~AppWnd()
	{
	}
	
	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDM_SAVE_IMGS:
			{
				LPoint PageSize(1000, 2000);
				
				LProgressDlg Prog(this, true);
				Prog.SetDescription("Scanning for HTML...");
				
				char p[MAX_PATH_LEN];
				LGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
				LArray<const char*> Ext;
				LArray<char*> Files;
				Ext.Add("*.html");
				Ext.Add("*.htm");				
				if (LRecursiveFileSearch(p, &Ext, &Files))
				{
					LDateTime Now;
					Now.SetNow();
					char OutPath[MAX_PATH_LEN], NowStr[32];
					sprintf_s(	NowStr, sizeof(NowStr),
								"%.4i-%.2i-%.2i_%.2i-%.2i-%.2i",
								Now.Year(), Now.Month(), Now.Day(),
								Now.Hours(), Now.Minutes(), Now.Seconds());
					LMakePath(OutPath, sizeof(OutPath), p, "Output");
					if (!LDirExists(OutPath))
						FileDev->CreateFolder(OutPath);
					LMakePath(OutPath, sizeof(OutPath), OutPath, NowStr);
					if (!LDirExists(OutPath))
						FileDev->CreateFolder(OutPath);
					
					Prog.SetDescription("Saving renders...");
					Prog.SetRange(Files.Length());
					for (int i=0; i<Files.Length() && !Prog.IsCancelled(); i++)
					{
						char *File = Files[i];
						auto Content = LReadFile(File);
						if (!Content)
						{
							LAssert(0);
							continue;
						}

						char *Leaf = strrchr(File, DIR_CHAR);
						if (!Leaf)
						{
							LAssert(0);
							continue;
						}
						Leaf++;
						char Png[256];
						strcpy_s(Png, sizeof(Png), Leaf);
						char *Ext = strrchr(Png, '.');
						if (Ext) *Ext = 0;
						strcat_s(Png, sizeof(Png), ".png");
						LMakePath(p, sizeof(p), OutPath, Png);

						Html1::LHtml Html(100, 0, 0, PageSize.x, PageSize.y, this);
						Html.Name(Content);
						LMemDC Screen(PageSize.x, PageSize.y, System24BitColourSpace);
						Screen.Colour(LColour(255, 255, 255));
						Screen.Rectangle();
						Html.OnPaint(&Screen);
						if (!GdcD->Save(p, &Screen))
							LAssert(0);
						
						Prog.Value(i);
					}
					Files.DeleteArrays();
				}
				break;
			}
			case IDM_COMPARE_IMAGES:
			{
				char p[MAX_PATH_LEN];
				LGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
				LArray<const char*> Ext;
				LArray<char*> Files;
				Ext.Add("*.png");
				// if (LRecursiveFileSearch(p, &Ext, &Files))

				char OutPath[MAX_PATH_LEN];
				LMakePath(OutPath, sizeof(OutPath), p, "Output");
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
	
	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDC_LIST:
			{
				if (n.Type == LNotifyItemSelect)
				{
					LListItem *s = Lst->GetSelected();
					if (s)
					{
						char p[256];

						LMakePath(p, sizeof(p), Base, s->GetText(0));
						if (LFileExists(p))
						{
							auto h = LReadFile(p);
							if (h)
							{
								if (Html)
									Html->Name(h);
							}
						}
					}
				}
				break;
			}
			case IDC_HTML:
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
