#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <ctype.h>

#include "Lgi.h"
#include "LSubProcess.h"
#include "SkinEngine.h"
#include "LArray.h"
#include "LVariant.h"
#if defined(WIN32)
#include "../win32/LSymLookup.h"
#elif defined(LINUX) && defined(LGI_SDL)
#include "../linux/LSymLookup.h"
#else
#include "LSymLookup.h"
#endif
#include "LToken.h"
#include "GViewPriv.h"
#include "LRegionClipDC.h"
#include "LFontCache.h"

#define DEBUG_MSG_TYPES				0
#define DEBUG_HND_WARNINGS			0
#define MOUSE_CAPTURE_POLL			100

extern LHashTbl<PtrKey<LView*>,bool> ViewMap;

////////////////////////////////////////////////////////////////
struct OsAppArgumentsPriv
{
	LArray<char*> Ptr;
	
	~OsAppArgumentsPriv()
	{
		Ptr.DeleteArrays();
	}
};

OsAppArguments::OsAppArguments(int args, const char **arg)
{
	d = new OsAppArgumentsPriv;

	for (int i=0; i<args; i++)
	{
		d->Ptr.Add(NewStr(arg[i]));
	}
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
}

OsAppArguments::~OsAppArguments()
{
	DeleteObj(d);
}

bool OsAppArguments::Get(const char *Name, LString *Value)
{
	if (!Name)
		return false;
		
	for (unsigned i = 1; i < Args; i++)
	{
		if (Arg[i][0] == '-' &&
			!_stricmp(Arg[i]+1, Name))
		{
			if (Value && i < Args - 1)
				*Value = Arg[i+1];
			
			return true;
		}
	}
	
	return false;
}

void OsAppArguments::Set(const char *CmdLine)
{
	d->Ptr.DeleteArrays();
	
	if (!CmdLine)
		return;
	
	for (const char *s = CmdLine; *s; )
	{
		while (*s && strchr(WhiteSpace, *s)) s++;
		if (*s == '\'' || *s == '\"')
		{
			char delim = *s++;
			char *e = strchr(s, delim);
			if (e)
				d->Ptr.Add(NewStr(s, e - s));
			else
				break;
			
			s = e + 1;
		}
		else
		{
			const char *e = s;
			while (*e && !strchr(WhiteSpace, *e))
				e++;
			d->Ptr.Add(NewStr(s, e-s));
			s = e;
		}
	}	

	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];
}

OsAppArguments &OsAppArguments::operator =(OsAppArguments &a)
{
	d->Ptr.DeleteArrays();

	for (int i=0; i<a.Args; i++)
	{
		if (a.Arg[i])
			d->Ptr.Add(NewStr(a.Arg[i]));
	}
	
	Args = d->Ptr.Length();
	Arg = &d->Ptr[0];

	return *this;
}

/////////////////////////////////////////////////////////////////////////////
//
// Attempts to cleanup and call drkonqi to process the crash
//
void LgiCrashHandler(int Sig)
{
	// Don't get into an infinite loop
   	signal(SIGSEGV, SIG_DFL);

	#ifndef _MSC_VER
	// Our pid
	int MyPid = getpid();	
	printf("LgiCrashHandler trigger MyPid=%i\n", MyPid);
	#endif

	exit(-1);
}

/////////////////////////////////////////////////////////////////////////////
typedef LArray<LAppInfo*> AppArray;

#ifndef XK_Num_Lock
#define XK_Num_Lock                      0xff7f
#endif
#ifndef XK_Shift_Lock
#define XK_Shift_Lock                    0xffe6
#endif
#ifndef XK_Caps_Lock
#define XK_Caps_Lock                     0xffe5
#endif

class LAppPrivate : public LSymLookup, public LMutex
{
public:
	// Common
	LXmlTag *Config;
	LFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	LLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	#if HAS_SHARED_MIME
	LSharedMime *Sm;
	#endif
	LHashTbl<IntKey<int>, LView*> Handles;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	int MessageLoopDepth;
	int CurEvent;
	#if DEBUG_MSG_TYPES
	LArray<int> Types;
	#endif
	LArray<LViewI*> DeleteLater;
	LMouse LastMove;
	LAutoString Name;
	LAutoPtr<LFontCache> FontCache;

	// Update handling (lock before using)
	LRegion Dirty;
	
	// Mouse capture
	SDL_TimerID CaptureId;

	// Window stack
	LArray<LWindow*> Stack;
	
	// Clipboard handling
	int Clipboard, Utf8, Utf8String;
	LVariant ClipData;
	LHashTbl<IntKey<int>, LVariant*> ClipNotify;

	// Mouse click info
	uint64 LastClickTime;
	OsView LastClickWnd;
	int LastClickX;
	int LastClickY;

	LAppPrivate() : Args(0, 0)
	{
		CaptureId = 0;
		CurEvent = 0;
		GuiThread = LCurrentThreadHnd();
		GuiThreadId = LCurrentThreadId();
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
		MessageLoopDepth = 0;
		#if HAS_SHARED_MIME
		Sm = 0;
		#endif

		LastClickTime = 0;
		LastClickWnd = 0;
		LastClickX = 0;
		LastClickY = 0;
	}

	~LAppPrivate()
	{
		DeleteObj(SkinLib);
		
		#if HAS_SHARED_MIME
		DeleteObj(Sm);
		#endif
		
		for (auto a : MimeToApp)
		{
			a.value->DeleteObjects();
		}
	}
};

/////////////////////////////////////////////////////////////////////////////
LSkinEngine *LApp::SkinEngine = 0;
LApp *TheApp = 0;
LMouseHook *LApp::MouseHook = 0;

#ifdef LINUX
void sighandler(int signum)
{
	SDL_Quit();
	signal(signum, SIG_DFL);
	printf("sighandler called.\n");
    kill(getpid(), signum);
}
#endif

LApp::LApp(OsAppArguments &AppArgs, const char *name, LAppArguments *Args) :
	OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	d = new LAppPrivate;
	Name(name);

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
		LgiTrace("%s:%i - Couldn't initialize SDL: %s\n", _FL, SDL_GetError());
	SDL_EnableUNICODE(true);

	#ifdef LINUX
	signal(SIGSEGV, sighandler);
	signal(SIGINT, sighandler);
	#endif
		
	// We want our printf's NOW!
	setvbuf(stdout,(char *)NULL,_IONBF,0); // print mesgs immediately.

	// Save the args somewhere
	SetAppArgs(AppArgs);

	// Setup the file and graphics sub-systems
	d->FileSystem = new LFileSystem;
	d->GdcSystem = new GdcDevice;

	srand(LCurrentTime());
	LgiInitColours();
	AppWnd = 0;

	MouseHook = new LMouseHook;

	// System font setup
	LFontType SysFontType;

	if (SysFontType.GetSystemFont("System"))
	{
		SystemNormal = SysFontType.Create();
		if (SystemNormal)
			SystemNormal->Transparent(true);
		
		SystemBold = SysFontType.Create();
		if (SystemBold)
		{
			SystemBold->Bold(true);
			SystemBold->Transparent(true);
			SystemBold->Create();
		}
	}
	
	if (!SystemNormal)
		SystemNormal = new LFont;
	if (!SystemBold)
		SystemBold = new LFont;

	if (!GetOption("noskin"))
	{
		extern LSkinEngine *CreateSkinEngine(LApp *App);
		SkinEngine = CreateSkinEngine(this);
	}
}

LApp::~LApp()
{
	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(d->Config);
	DeleteObj(LFontSystem::Me);
	DeleteObj(d);
	TheApp = 0;

	SDL_Quit();
}

LApp *LApp::ObjInstance()
{
	return TheApp;
}

bool LApp::IsOk()
{
	bool Status = 	(this != NULL) &&
					(d != NULL)/* &&
					(SystemNormal != NULL)*/;
					
	LAssert(Status);
	return Status;
}

LMouseHook *LApp::GetMouseHook()
{
	return MouseHook;
}

int LApp::GetMetric(LSystemMetric Metric)
{
	switch (Metric)
	{
		case LGI_MET_DECOR_X:
		{
			return 8;
		}
		case LGI_MET_DECOR_Y:
		{
			return 8 + 19;
		}
		case LGI_MET_DECOR_CAPTION:
		{
			return 19;
		}
		default:
		{
			LAssert(0);
			break;
		}
	}

	return 0;
}

LViewI *LApp::GetFocus()
{
	return 0;
}

OsThread LApp::_GetGuiThread()
{
	return d->GuiThread;
}

OsThreadId LApp::GetGuiThreadId()
{
	return d->GuiThreadId;
}

bool LApp::InThread()
{
	return LCurrentThreadId() == d->GuiThreadId;
}

OsProcessId LApp::GetProcessId()
{
    #ifdef WIN32
	return GetCurrentProcessId();
    #else
	return getpid();
	#endif
}

OsAppArguments *LApp::GetAppArgs()
{
	return IsOk() ? &d->Args : 0;
}

void LApp::SetAppArgs(OsAppArguments &AppArgs)
{
	if (IsOk())
	{
		d->Args = AppArgs;
	}
}

struct GtkIdle
{
	LAppI::OnIdleProc cb;
	void *param;
};

template<typename T>
void SDL_to_Mouse(LMouse &ms, T &ev)
{
	ms.x = ev.x;
	ms.y = ev.y;
	ms.Down(ev.state == SDL_PRESSED);

	/*
	LViewI *Over = ms.Target->WindowFromPoint(ms.x, ms.y);
	if (Over != ms.Target)
	{
		LPoint p;
		Over->WindowVirtualOffset(&p);
		ms.Target = Over;
		ms.x -= p.x;
		ms.y -= p.y;
	}
	*/
}

void LApp::OnSDLEvent(LMessage *m)
{
	SDL_EventType t = (SDL_EventType) m->Event.type;
	switch (m->Event.type)
	{
		case SDL_VIDEOEXPOSE:
		{
			if (AppWnd)
			{
				LScreenDC Dc(AppWnd, GdcD->Handle());
				AppWnd->_Paint(&Dc);
			}
			break;
		}
		case SDL_MOUSEMOTION:
		{
			if (AppWnd)
			{
				LMouse ms;
				ms.Target = AppWnd;
				SDL_to_Mouse(ms, m->Event.motion);
				AppWnd->_Mouse(ms, true);
			}
			break;
		}
		
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
		{
			if (AppWnd)
			{
				LMouse ms;
				SDL_to_Mouse(ms, m->Event.button);

				LViewI *v = AppWnd->WindowFromPoint(ms.x, ms.y);
				if (v)
				{
					LPoint p(ms.x, ms.y);
					v->PointToView(p);
					ms.Target = v;
					ms.x = p.x;
					ms.y = p.y;
				}
				else
				{
					ms.Target = AppWnd;
				}
				
				ms.Left(m->Event.button.button == SDL_BUTTON_LEFT);
				ms.Right(m->Event.button.button == SDL_BUTTON_RIGHT);
				ms.Middle(m->Event.button.button == SDL_BUTTON_MIDDLE);
				ms.Down(m->Event.type == SDL_MOUSEBUTTONDOWN);

				LView *gv = ms.Target->GetGView();
				/*
				if (gv)
					printf("AppWnd=%s/%p Target=%s/%p\n", AppWnd->GetClass(), AppWnd, ms.Target->GetClass(), ms.Target);
				*/
				if (gv)
					gv->_Mouse(ms, false);
			}
			break;
		}
		case SDL_KEYDOWN:
		{
			if
			(
				m->Event.key.keysym.sym == SDLK_F4 &&
				(
					m->Event.key.keysym.mod == KMOD_LALT ||
					m->Event.key.keysym.mod == KMOD_RALT
				)
			)
				LCloseApp();
			
			// fall through
		}
		case SDL_KEYUP:
		{
			LKey k;
			k.vkey = m->Event.key.keysym.sym;
			k.c16 = m->Event.key.keysym.unicode;
			if (m->Event.key.state & (KMOD_LSHIFT|KMOD_RSHIFT))
				k.Shift(true);
			if (m->Event.key.state & (KMOD_LCTRL|KMOD_RCTRL))
				k.Ctrl(true);
			if (m->Event.key.state & (KMOD_LALT|KMOD_RALT))
				k.Alt(true);
			k.Down(m->Event.type == SDL_KEYDOWN);
				
			if (AppWnd)
			{
				LViewI *f = AppWnd->GetFocus();
				AppWnd->HandleViewKey(f ? f->GetGView() : NULL, k);
			}
			break;
		}
		case SDL_VIDEORESIZE:
		{
			int asd=0;
			break;
		}
		case SDL_ACTIVEEVENT:
		{
			if (AppWnd)
			{
				AppWnd->OnFrontSwitch(m->Event.active.gain);
			}
			break;
		}
		case SDL_USEREVENT:
		{
			switch (m->Event.user.code)
			{
				case M_PULSE:
				{
					LView *v = (LView*)m->Event.user.data1;
					if (v && ViewMap.Find(v))
					{
						v->OnPulse();
					}
					break;
				}
				case M_INVALIDATE:
				{
					SDL_Surface *Screen = GdcD->Handle();
					if (AppWnd != NULL &&
						Screen != NULL)
					{
						LScreenDC Dc(AppWnd, Screen);
						if (d->Lock(_FL))
						{
							LRegion r = d->Dirty;
							d->Dirty.Empty();
							d->Unlock();

							AppWnd->_Paint(&Dc, NULL, &r);
							for (LRect *i=r.First(); i; i=r.Next())
							{
								SDL_UpdateRect(Screen, i->x1, i->y1, i->X(), i->Y());
							}
						}
						else LgiTrace("%s:%i - failed to lock mutex.\n", _FL);
					}
					else LgiTrace("%s:%i - parameter error.\n", _FL);
					break;
				}
				case M_MOUSE_CAPTURE_POLL:
				{
					// Is there any way to get the position of the mouse outside the
					// main window in SDL v1?
					break;
				}
				case M_CHANGE:
				{
					LView *v = (LView*)m->Event.user.data1;
					// LAutoPtr<LMessage::EventParams> p((LMessage::EventParams*)m->Event.user.data2);
					// FIXME: 'B' data type is changing...
					LAssert(!"Fixme");
					LAutoPtr<LNotification> note((LNotification*)m->B());
					
					if (!ViewMap.Find(v))
						break;
					
					if (p && AppWnd && v)
					{
						LViewI *Ctrl;
						if (AppWnd->GetViewById((int)p->a, Ctrl))
							v->OnNotify(Ctrl, (int)p->b);
						else
							printf("%s:%i - Can't find ctrl %i\n", _FL, (int)p->a);
					}
					else printf("%s:%i - Param error\n", _FL);
					break;
				}
				default:
				{
					LView *v = (LView*)m->Event.user.data1;
					LAutoPtr<LMessage::EventParams> p((LMessage::EventParams*)m->Event.user.data2);

					if (!ViewMap.Find(v))
						break;
					
					#if 0
					printf("SDL_USEREVENT cmd=%i v=%p/%s data2=%p\n",
						m->Event.user.code,
						v, v ? v->GetClass() : 0,
						m->Event.user.data2);
					#endif
					
					if (v)
					{
						v->OnEvent(m);
					}
					break;
				}
			}
			break;
		}
		default:
		{
			LgiTrace("%s:%i - Unhandled SDL event: %i\n", _FL , m->Event.type);
			break;
		}
	}
}

bool LApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	ThreadCheck();

	LMessage Msg;
	int r;
	
	if (AppWnd)
	{
		LRect r(0, 0, GdcD->X()-1, GdcD->Y()-1);
		AppWnd->SetPos(r);
	}

	do
	{
		if (Loop)
			r = SDL_WaitEvent(&Msg.Event);
		else
			r = SDL_PollEvent(&Msg.Event);
		
		if (Msg.Event.type == SDL_QUIT)
		{
			if (!AppWnd)
				break;
			
			bool Close = AppWnd->OnRequestClose(false);
			if (Close)
				break;
			continue;
		}

		OnSDLEvent(&Msg);
	}
	while (Loop || r > 0);

	return false;
}

void LApp::OnUrl(const char *Url)
{
	if (AppWnd)
		AppWnd->OnUrl(Url);
}

void LApp::OnReceiveFiles(LArray<char*> &Files)
{
	if (AppWnd)
		AppWnd->OnReceiveFiles(Files);
}

void LApp::SetConfig(LXmlTag *Tag)
{
	if (IsOk() && Tag)
	{
		LXmlTag *Old = GetConfig(Tag->GetTag());
		if (Old)
		{
			Old->RemoveTag();
			DeleteObj(Old);
		}

		if (!d->Config)
		{
			d->Config = new LXmlTag("Config");
		}
		if (d->Config)
		{
			d->Config->InsertTag(Tag);
		}
	}
}

LXmlTag *LApp::GetConfig(const char *Tag)
{
	if (IsOk() && !d->Config)
	{
		char File[] = "lgi.conf";
		char Path[256];
		if (LgiGetExePath(Path, sizeof(Path)))
		{
			if (Path[strlen(Path)-1] != DIR_CHAR) strcat(Path, DIR_STR);
			strcat(Path, File);

			if (!FileExists(Path))
			{
				char *f = LgiFindFile(File);
				if (f)
				{
					strcpy(Path, f);
					DeleteArray(f);
				}
			}

			if (FileExists(Path))
			{
				d->Config = new LXmlTag("Config");
				if (d->Config)
				{
					LFile f;
					if (f.Open(Path, O_READ))
					{
						LXmlTree t;
						t.Read(d->Config, &f, 0);
					}
				}
			}
		}
	}

	if (Tag && d->Config)
	{
		return d->Config->GetChildTag(Tag);
	}

	return 0;
}

const char *LApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? NewStr(d->Args.Arg[n]) : 0;
}

bool LApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	LString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
		{
			if (DestLen > 0)
			{
				strcpy_s(Dest, DestLen, Buf);
			}
			else return false;
		}
		return true;
	}
	return false;
}

bool LApp::GetOption(const char *Option, LString &Buf)
{
	if (IsOk() && Option)
	{
		int OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			char *a = d->Args.Arg[i];
			if (!a)
				continue;

			if (strchr("-/\\", a[0]))
			{
				if (strnicmp(a+1, Option, OptLen) == 0)
				{
					char *Arg = 0;
					if (strlen(a+1+OptLen) > 0)
					{
						Arg = a + 1 + OptLen;
					}
					else if (i < d->Args.Args - 1)
					{
						Arg = d->Args.Arg[i + 1];
					}
					
					if (Arg)
					{
						if (strchr("\'\"", *Arg))
						{
							char Delim = *Arg++;
							char *End = strchr(Arg, Delim);
							if (End)
							{
								int Len = (int)End-(int)Arg;
								if (Len > 0)
								    Buf.Set(Arg, Len);
								else
								    return false;
							}
							else return false;
						}
						else
						{
						    Buf = Arg;
						}
					}

					return true;
				}
			}
		}
	}

	return false;
}

void LApp::OnCommandLine()
{
	char WhiteSpace[] = " \r\n\t";

	LArray<char*> Files;

	for (int i=1; i<GetAppArgs()->Args; i++)
	{
		char *a = GetAppArgs()->Arg[i];
		if (FileExists(a))
		{
			Files.Add(NewStr(a));
		}
	}

	// call app
	if (Files.Length() > 0)
	{
		OnReceiveFiles(Files);
	}

	// clear up
	Files.DeleteArrays();
}

void LApp::Exit(int Code)
{
	if (Code)
	{
		// hard exit
		::exit(Code);
	}
	else
	{
		// soft exit
		ThreadCheck();
		
		SDL_Event e;
		e.type = SDL_QUIT;
		SDL_PushEvent(&e);
	}
}

LAutoString LApp::GetFileMimeType(const char *File)
{
	LAutoString Status;
	char Full[300] = "";

	if (!FileExists(File))
	{
		// Look in the path
		LToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
		for (int i=0; i<p.Length(); i++)
		{
			LMakePath(Full, sizeof(Full), p[i], File);
			if (FileExists(Full))
			{
				File = Full;
				break;
			}
		}		
	}

	#if HAS_SHARED_MIME
	// freedesktop.org rocks...
	if (!d->Sm)
	{
		// Not loaded, go and try to load it...
		d->Sm = new LSharedMime;
		if (d->Sm && d->Sm->IsLoaded())
		{
			d->Sm->mimetypes_init();
		}
	}
	if (d->Sm && d->Sm->IsLoaded())
	{
		// Loaded...
		char *m = (char*)d->Sm->mimetypes_get_file_type(File, MIMETYPES_CHECK_ALL);
		if (m)
		{
			#if HAS_FILE_CMD
			if (stricmp(m, "application/x-not-recognised") != 0)
			#endif
			{
				strcpy(Mime, m);
				return true;
			}
		}
		else
		{
			printf("%s:%i - mimetypes_get_file_type failed for '%s'\n", __FILE__, __LINE__, File);
		}
	}
	else
	{
		printf("%s:%i - Shared Mime not loaded!!!\n", __FILE__, __LINE__);
	}
	#endif

	#if HAS_FILE_CMD
	// doh! not installed... :(
	LStringPipe Output;
	char Args[256];
	sprintf(Args, "-i \"%s\"", File);
	LProcess p;
	if (p.Run("file", Args, 0, true, 0, &Output))
	{
		char *Out = Output.NewStr();
		if (Out)
		{
			char *s = strchr(Out, ':');
			if (s && strchr(Out, '/'))
			{
				s += 2;
				
				char *e = s;
				while (*e && (isalpha(*e) || strchr("-_/", *e))) e++;
				*e = 0;
				
				Status.Reset(NewStr(s));
			}
			DeleteArray(Out);
		}
	}
	#endif
	
	return Status;
}

bool LApp::GetAppsForMimeType(char *Mime, LArray<LAppInfo*> &Apps)
{
	// Find alternative version of the MIME type (e.g. x-type and type).
	char AltMime[256];
	strcpy(AltMime, Mime);
	char *s = strchr(AltMime, '/');
	if (s)
	{
		s++;
		
		int Len = strlen(s) + 1;
		if (strnicmp(s, "x-", 2) == 0)
		{
			memmove(s, s+2, Len - 2);
		}
		else
		{
			memmove(s+2, s, Len);
			s[0] = 'x';
			s[1] = '-';
		}
	}

	LGetAppsForMimeType(Mime, Apps);
	LGetAppsForMimeType(AltMime, Apps);
	
	return Apps.Length() > 0;
}

LSymLookup *LApp::GetSymLookup()
{
	return d;
}

bool LApp::IsElevated()
{
	#ifdef WIN32
	LAssert(!"What API works here?");
	return false;
	#else
	return geteuid() == 0;
	#endif
}

LFontCache *LApp::GetFontCache()
{
	if (!d->FontCache)
		d->FontCache.Reset(new LFontCache(SystemNormal));
	return d->FontCache;
}

int LApp::GetCpuCount()
{
	return 1;
}

bool LApp::InvalidateRect(LRect &r)
{
	if (d->Lock(_FL))
	{
		int Len = d->Dirty.Length();
		d->Dirty.Union(&r);
		// LgiTrace("Invalidate %s\n", r.GetStr());
		
		if (Len == 0)
		{
			SDL_Event e;
			e.type = SDL_USEREVENT;
			e.user.code = M_INVALIDATE;
			e.user.data1 = AppWnd;
			e.user.data2 = NULL;
			SDL_PushEvent(&e);
		}

		d->Unlock();
	}
	
	return true;
}

bool LApp::PushWindow(LWindow *w)
{
	if (!w)
	{
		printf("%s:%i - No window to push.\n", _FL);
		return false;
	}

	if (AppWnd)
	{
		d->Stack.Add(AppWnd);
		// printf("Pushing %s, new AppWnd=%s %p\n", AppWnd?AppWnd->GetClass():0, w?w->GetClass():0, w);
	}
	else
	{
		// printf("Pushing AppWnd=%s %p\n", w?w->GetClass():0, w);
	}
	
	AppWnd = w;
	AppWnd->Invalidate();

	return true;
}

LWindow *LApp::PopWindow()
{
	if (d->Stack.Length() == 0)
	{
		printf("%s:%i - No window to pop.\n", _FL);
		LAssert(0);
		return NULL;
	}
	
	AppWnd = d->Stack.Last();
	d->Stack.Length(d->Stack.Length()-1);
	AppWnd->Invalidate();

	printf("Popping AppWnd=%s\n", AppWnd?AppWnd->GetClass():0);

	return AppWnd;
}

Uint32 SDL_MouseCapture(Uint32 interval, LView *v)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = M_MOUSE_CAPTURE_POLL;
	e.user.data1 = NULL;
	e.user.data2 = NULL;
	SDL_PushEvent(&e);
	return MOUSE_CAPTURE_POLL;
}

void LApp::CaptureMouse(bool capture)
{
	if (capture)
	{
		if (d->CaptureId == 0)
			d->CaptureId = SDL_AddTimer(MOUSE_CAPTURE_POLL, (SDL_NewTimerCallback)SDL_MouseCapture, this);
	}
	else if (d->CaptureId)
	{
		SDL_RemoveTimer(d->CaptureId);
	}
}

extern LString GetFreetypeLibraryVersion();
LString LApp::GetFreetypeVersion()
{
	return GetFreetypeLibraryVersion();
}

////////////////////////////////////////////////////////////////
OsApplication *OsApplication::Inst = 0;

class OsApplicationPriv
{
public:
	OsApplicationPriv()
	{
	}
};

OsApplication::OsApplication(int Args, char **Arg)
{
    Inst = this;
    d = new OsApplicationPriv;	
}

OsApplication::~OsApplication()
{
	DeleteObj(d);
	Inst = 0;
}

////////////////////////////////////////////////////////////////
void LMessage::Set(int m, Param pa, Param pb)
{
	Event.type = SDL_USEREVENT;
	Event.user.code = m;
	Event.user.data1 = NULL;
	if (pa || pb)
		Event.user.data2 = new EventParams(pa, pb);
	else
		Event.user.data2 = NULL;
}

bool LMessage::Send(LViewI *Wnd)
{
	bool Status = false;
	LAssert(!"Not impl.");
	return Status;
}

int LMessage::Msg()
{
	if (Event.type >= SDL_USEREVENT && Event.type <= SDL_NUMEVENTS)
		return Event.user.code;
	else
		printf("%s:%i - Error: Event.type=%i (%i - %i)\n", _FL, Event.type, SDL_USEREVENT, SDL_NUMEVENTS);
	return 0;
}

LMessage::Param LMessage::A()
{
	if (Event.type >= SDL_USEREVENT &&
		Event.type <= SDL_NUMEVENTS &&
		Event.user.data2 != NULL)
		return ((EventParams*)Event.user.data2)->a;
	return 0;
}

LMessage::Param LMessage::B()
{
	if (Event.type >= SDL_USEREVENT &&
		Event.type <= SDL_NUMEVENTS &&
		Event.user.data2 != NULL)
		return ((EventParams*)Event.user.data2)->b;
	return 0;
}

