#include "lgi/common/Lgi.h"
#include "lgi/common/Browser.h"
#include "lgi/common/Html.h"
#include "lgi/common/Net.h"
#include "lgi/common/Http.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Button.h"

#define M_LOADED		(M_USER+3000)
#define M_BUSY			(M_USER+3001)

enum {
	IDC_HTML = 100,
	IDC_URI,
	IDC_BACK,
	IDC_FORWARD,
	IDC_REFRESH_STOP,
	IDC_SEARCH_TXT,
	IDC_SEARCH,
};

class LBrowserPriv;
class LBrowserThread : public LThread, public LMutex
{
	LBrowserPriv *d;
	LArray<LAutoString> Work;
	bool Loop;

public:
	LBrowserThread(LBrowserPriv *priv);
	~LBrowserThread();

	bool Add(char *Uri);
	void Stop();
	int Main();
};

class LBrowserPriv : public LDocumentEnv
{
public:
	typedef LHashTbl<StrKey<char,false>,LStream*> Collection;

	struct FileLock : public LMutex::Auto
	{
		Collection *Files;

		FileLock(LMutex *s, const char *file, int line) : LMutex::Auto(s, file, line)
		{
			Files = 0;
		}
	};

	typedef LAutoPtr<FileLock> FilePtr;

private:
	Collection Files; // requires locking to access

public:
	LBrowser *Wnd = NULL;
	Html1::LHtml *Html = NULL;
	LAutoPtr<LBrowserThread> Thread;
	LEdit *UriEdit = NULL;
	LEdit *SearchEdit = NULL;
	LButton *Back = NULL;
	LButton *Forward = NULL;
	LButton *Stop = NULL;
	LButton *Search = NULL;
	LArray<LString> History;
	ssize_t CurHistory = 0;
	bool Loading = false;
	LBrowser::LBrowserEvents *Events = NULL;
	LHttp Http;
	LString::Array ResourcePaths;

	LBrowserPriv(LBrowser *wnd)
	{
		Wnd = wnd;
		ResourcePaths.SetFixedLength(false);
	}

	FilePtr Lock()
	{
		FilePtr p(new FileLock(this, _FL));
		p->Files = &Files;
		return p;
	}

	void LoadStream(LStream *s)
	{
		int len = (int) s->GetSize();
		LArray<char> txt;
		txt.Length(len);
		s->Read(&txt[0], len);
		txt.Add(0);
		Html->Name(&txt[0]);
	}

	bool LoadCurrent()
	{
		char *Uri = History[CurHistory];
		if (!Uri)
			return false;

		LUri u(Uri);
		#ifdef WIN32
		if (u.sPath)
			u.sPath = u.sPath.Replace("/", DIR_STR);
		#endif
		bool IsFile = LFileExists(u.sPath);
		bool IsHttp = false;

		if (IsFile)
			u.sProtocol = "file";
		else if (u.Set(Uri))
		{
			if (u.sProtocol)
			{
				if (!_stricmp(u.sProtocol, "file"))
				{
					if (!u.sPath)
						return false;

					IsFile = true;
				}
				else if (!_stricmp(u.sProtocol, "http"))
				{
					IsHttp = true;
				}
				else
					return false; // Protocol not support
			}
			else
			{
				// Probably...?
				IsHttp = true;
				u.sProtocol = "http";
				History[CurHistory] = u.ToString();
				Uri = History[CurHistory];
			}
		}
		
		if (IsFile)
		{
			LAutoString txt(LReadTextFile(u.sPath));
			if (txt)
			{
				Html->Name(txt);
			}
			else return false;
		}
		else if (IsHttp)
		{
			if (!Thread)
				Thread.Reset(new LBrowserThread(this));
			Thread->Add(Uri);
		}

		if (u.sAnchor)
		{
			Html->GotoAnchor(u.sAnchor);
		}

		LString a = u.ToString();
		UriEdit->Name(a);

		Wnd->SetCtrlEnabled(IDC_BACK, CurHistory > 0);
		Wnd->SetCtrlEnabled(IDC_FORWARD, CurHistory < (ssize_t)History.Length() - 1);

		return true;
	}

	bool OnNavigate(LDocView *Parent, const char *Uri)
	{
		if (!Uri)
			return false;

		LUri u(Uri);
		char Sep, Buf[MAX_PATH_LEN];
		if (!u.sProtocol)
		{
			// Relative link?
			char *Cur = History[CurHistory];
			if (strchr(Cur, '\\'))
				Sep = '\\';
			else
				Sep = '/';
			
			// Trim off filename...
			char *Last = strrchr(Cur, Sep);
			auto t = LString(Uri).SplitDelimit("\\/");
			if (*Uri != '#' && Last)
				sprintf_s(Buf, sizeof(Buf), "%.*s", (int) (Last - Cur), Cur);
			else
				strcpy_s(Buf, sizeof(Buf), Cur);

			char *End = Buf + strlen(Buf) - 1;
			if (*End == Sep)
				*End = 0;

			for (unsigned i=0; i<t.Length(); i++)
			{
				if (!_stricmp(t[i], "."))
					;
				else if (!_stricmp(t[i], ".."))
				{
					if ((End = strrchr(Buf, Sep)))
						*End = 0;
				}
				else if (t[i][0] == '#')
				{
					size_t len = strlen(Buf);
					sprintf_s(Buf+len, sizeof(Buf)-len, "%s", t[i].Get());
				}
				else
				{
					size_t len = strlen(Buf);
					sprintf_s(Buf+len, sizeof(Buf)-len, "%c%s", Sep, t[i].Get());
				}
			}

			Uri = Buf;
		}
		else if (!_stricmp(u.sProtocol, "mailto") ||
				 !_stricmp(u.sProtocol, "http"))
		{
			LExecute(Uri);
			return true;
		}

		return Wnd->SetUri(Uri);
	}

	LoadType GetContent(LoadJob *&j)
	{
		if (!j || !j->Uri)
			return LoadError;

		auto Uri = History[CurHistory];
		LUri BaseUri(Uri);
		
		LUri u(j->Uri);
		const char *LoadFileName = NULL;
		if (u.sProtocol)
		{
			
		}
		else LoadFileName = j->Uri;		
		
		if (LoadFileName)
		{
			auto SearchPaths = ResourcePaths;
			if (!BaseUri.sPath.IsEmpty())
			{
				LFile::Path p(BaseUri.sPath);
				p--;
				SearchPaths.AddAt(0, p.GetFull());
			}

			LFile::Path p;
			for (auto Path: SearchPaths)
			{
				p = Path;
				p += LoadFileName;
				if (p.Exists())
				{
					auto mt = LGetFileMimeType(p).SplitDelimit("/");
					if (mt[0].Equals("image"))
					{
						j->pDC.Reset(GdcD->Load(p));
						return LoadImmediate;
					}
					else
					{
						LFile *f;
						if (j->Stream.Reset(f = new LFile))
						{
							if (!f->Open(p, O_READ))
							{
								j->Stream.Reset();
								j->Error.Printf("Can't open file '%s' for reading", p.GetFull().Get());
								return LoadError;
							}

							return LoadImmediate;
						}
					}
					break;
				}
			}
		}

		return LoadError;
	}
};

LBrowserThread::LBrowserThread(LBrowserPriv *priv) :
	LThread("LBrowserThread.Thread"),
	LMutex("LBrowserThread.Mutex")
{
	Loop = true;
	d = priv;
	Run();
}

LBrowserThread::~LBrowserThread()
{
	Loop = false;
	while (!IsExited())
		LSleep(10);
}

void LBrowserThread::Stop()
{
	d->Http.Close();
}

bool LBrowserThread::Add(char *Uri)
{
	if (Lock(_FL))
	{
		Work.New().Reset(NewStr(Uri));
		Unlock();
		return true;
	}
	return false;
}

int LBrowserThread::Main()
{
	bool IsBusy = false;

	while (Loop)
	{
		LAutoString Uri;
		if (Lock(_FL))
		{
			if (Work.Length())
			{
				Uri = Work[0];
				Work.DeleteAt(0, true);

				if (!IsBusy)
					d->Wnd->PostEvent(M_BUSY, IsBusy = true);
			}
			else if (IsBusy)
				d->Wnd->PostEvent(M_BUSY, IsBusy = false);

			Unlock();
		}

		if (Uri)
		{
			LUri u(Uri);
			if (!u.Port)
				u.Port = HTTP_PORT;

			int Status = 0;
			LAutoPtr<LStream> p(new LStringPipe);
			if (p)
			{
				LProxyUri Proxy;
				if (Proxy.sHost)
					d->Http.SetProxy(Proxy.sHost, Proxy.Port);

				LAutoPtr<LSocketI> Sock(new LSocket);
				if (d->Http.Open(Sock, u.sHost, u.Port))
				{
					LHttp::ContentEncoding Enc;
					bool b = d->Http.Get(Uri, "Cache-Control:no-cache", &Status, p, &Enc);
					LBrowserPriv::FilePtr f = d->Lock();
					if (!b)
					{
						p->Print("<h1>HTTP Error</h1>\nCode: %i<br>", Status);
					}
					f->Files->Add(Uri, p.Release());
					d->Wnd->PostEvent(M_LOADED);
				}
			}
		}
		else LSleep(50);
	}

	return false;
}

LBrowser::LBrowser(LViewI *owner, const char *Title, char *Uri)
{
	d = new LBrowserPriv(this);
	Name(Title?Title:(char*)"Browser");

	LRect r(0, 0, 1000, 800);
	SetPos(r);
	MoveSameScreen(owner);

	if (!Attach(0))
		return;

	#ifdef MAC
	#define BTN_X 50
	#else
	#define BTN_X 30
	#endif
		
	AddView(d->Back = new LButton(IDC_BACK, 0, 0, BTN_X, 20, "<-"));
	AddView(d->Forward = new LButton(IDC_FORWARD, 0, 0, BTN_X, 20, "->"));
	AddView(d->Stop = new LButton(IDC_REFRESH_STOP, 0, 0, -1, 20, "Refresh"));
	AddView(d->UriEdit = new LEdit(IDC_URI, 0, 0, 100, 20, 0));
	AddView(d->SearchEdit = new LEdit(IDC_SEARCH_TXT, 0, 0, 100, 20, ""));
	AddView(d->Search = new LButton(IDC_SEARCH, 0, 0, -1, 20, "Search"));
	AddView(d->Html = new Html1::LHtml(IDC_HTML, 0, 0, 100, 100));

	AttachChildren();
	OnPosChange();
	Visible(true);

	d->Html->SetEnv(d);
	d->Html->SetLinkDoubleClick(false);
	d->Back->Enabled(false);
	d->Forward->Enabled(false);
	if (Uri)
		SetUri(Uri);

	RegisterHook(this, LKeyEvents);
}

LBrowser::~LBrowser()
{
	DeleteObj(d);
}

bool LBrowser::OnViewKey(LView *v, LKey &k)
{
	if (k.Ctrl() && ToUpper(k.c16) == 'W')
	{
		Quit();
		return true;
	}

	return false;
}

bool LBrowser::SetUri(const char *Uri)
{
	if (Uri)
	{
		char s[MAX_PATH_LEN];
		if (LDirExists(Uri))
		{
			sprintf_s(s, sizeof(s), "%s%cindex.html", Uri, DIR_CHAR);
			Uri = s;
		}

		// Delete history past the current point
		while ((ssize_t)d->History.Length() > d->CurHistory + 1)
		{
			d->History.DeleteAt(d->CurHistory + 1);
		}

		// Add a new URI to the history
		d->History.New() = Uri;
		d->CurHistory = d->History.Length() - 1;

		// Enabled the controls accordingly
		d->Back->Enabled(d->CurHistory > 0);
		d->Forward->Enabled(d->CurHistory < (ssize_t)d->History.Length() - 1);

		// Show the content
		d->LoadCurrent();
	}
	else
	{
		d->Html->Name("");
	}

	return true;
}

void LBrowser::OnPosChange()
{
	LRect c = GetClient();
	LRect e = c;
	e.y2 = e.y1 + LSysFont->GetHeight() + 8;
	LRect back = e;
	LRect forward = e;
	LRect stop = e;
	LRect search_txt = e;
	LRect search_btn = e;
	LRect uri = e;
	back.x2 = back.x1 + (d->Back ? d->Back->X() - 1 : 0);
	forward.x1 = back.x2 + 1;
	forward.x2 = forward.x1 + (d->Forward ? d->Forward->X() - 1 : 0);
	stop.x1 = forward.x2 + 1;
	stop.x2 = stop.x1 + (d->Stop ? d->Stop->X() - 1 : 0);
	uri.x1 = stop.x2 + 1;
	search_btn.x1 = search_btn.x2 - (d->Search ? d->Search->X() - 1 : 0);
	search_txt.x2 = search_btn.x1 - 1;
	search_txt.x1 = search_txt.x2 - 99;
	uri.x2 = search_txt.x1 - 1;

	LRect html = c;
	html.y1 = e.y2 + 1;

	if (d->Back)
		d->Back->SetPos(back);
	if (d->Forward)
		d->Forward->SetPos(forward);
	if (d->Stop)
		d->Stop->SetPos(stop);
	if (d->UriEdit)
		d->UriEdit->SetPos(uri);
	if (d->SearchEdit)
		d->SearchEdit->SetPos(search_txt);
	if (d->Search)
		d->Search->SetPos(search_btn);
	if (d->Html)
		d->Html->SetPos(html);
}

void LBrowser::SetEvents(LBrowserEvents *Events)
{
	d->Events = Events;
}

void LBrowser::AddPath(const char *Path)
{
	d->ResourcePaths.Add(Path);
}

bool LBrowser::SetHtml(const char *Html)
{
	d->History.Length(++d->CurHistory);
	d->Html->Name(Html);
	d->UriEdit->Name(0);
	d->Back->Enabled(d->CurHistory > 0);
	d->Forward->Enabled(d->CurHistory < (ssize_t)d->History.Length() - 1);

	return true;
}

int LBrowser::OnNotify(LViewI *c, LNotification n)
{
	switch (c->GetId())
	{
		case IDC_SEARCH_TXT:
		{
			if (n.Type != LNotifyReturnKey)
				break;
			// else fall through
		}
		case IDC_SEARCH:
		{
			if (d->Events && d->SearchEdit)
			{
				const char *Search = d->SearchEdit->Name();
				if (Search)
					d->Events->OnSearch(this, Search);
			}
			break;
		}
		case IDC_URI:
		{
			if (n.Type == LNotifyReturnKey)
			{
				const char *u = c->Name();
				SetUri(u);
				break;
			}
			break;
		}
		case IDC_BACK:
		{
			if (d->CurHistory > 0)
			{
				d->CurHistory--;
				d->LoadCurrent();
			}
			break;
		}
		case IDC_FORWARD:
		{
			if (d->CurHistory < (ssize_t)d->History.Length() - 1)
			{
				d->CurHistory++;
				d->LoadCurrent();
			}
			break;
		}
		case IDC_REFRESH_STOP:
		{
			if (d->Loading)
			{
				d->Thread->Stop();
			}
			else // refresh
			{
				d->LoadCurrent();
			}
			break;
		}
	}

	return 0;
}

LMessage::Result LBrowser::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case M_LOADED:
		{
			LBrowserPriv::FilePtr f = d->Lock();

			char *Uri = d->History[d->CurHistory];
			LStream *s = f->Files->Find(Uri);
			if (s)
			{
				f->Files->Delete(Uri);
				d->LoadStream(s);
				DeleteObj(s);
			}			
			break;
		}
		case M_BUSY:
		{
			d->Loading = m->A() != 0;
			if (d->Loading)
				SetCtrlName(IDC_REFRESH_STOP, "Stop");
			else
				SetCtrlName(IDC_REFRESH_STOP, "Refresh");
			break;
		}
	}
	return LWindow::OnEvent(m);
}

