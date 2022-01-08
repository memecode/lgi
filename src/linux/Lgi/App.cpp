#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Array.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Token.h"
#include "lgi/common/FontCache.h"
#include "AppPriv.h"

#define DEBUG_MSG_TYPES				0
#define DEBUG_HND_WARNINGS			0
#define IDLE_ALWAYS					0


bool GlibWidgetSearch(GtkWidget *p, GtkWidget *w, bool Debug, int depth = 0);

////////////////////////////////////////////////////////////////
struct OsAppArgumentsPriv
{
	::LArray<char*> Ptr;
	
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

bool OsAppArguments::Get(const char *Name, const char **Val)
{
	if (!Name)
		return false;
		
	for (int i=0; i<Args; i++)
	{
		auto a = Arg[i];
		if (a[0] == '-' || a[0] == '/')
		{
			if (Stricmp(a+1, Name) == 0)
			{
				if (i < Args - 1 && Val)
					*Val = Arg[i + 1];
				return true;
			}
		}
	}
	
	return false;
}

void OsAppArguments::Set(char *CmdLine)
{
	d->Ptr.DeleteArrays();
	
	if (!CmdLine)
		return;
	
	for (char *s = CmdLine; *s; )
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
			char *e = s;
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

////////////////////////////////////////////////////////////////
#if HAS_SHARED_MIME

#include "GFilterUtils.h"
#include "mime-types.h"

class GSharedMime : public LLibrary
{
public:
	GSharedMime() :
		#ifdef _DEBUG
		LLibrary("libsharedmime1d")
		#else
		LLibrary("libsharedmime1")
		#endif
	{
	}
	
	DynFunc0(int, mimetypes_init);
	DynFunc1(const char*, mimetypes_set_default_type, const char *, default_type);	
	DynFunc2(const char*, mimetypes_get_file_type, const char*, pathname, mimetypes_flags, flags);
	DynFunc2(const char*, mimetypes_get_data_type, const void*, data, int, length);
	DynFunc3(bool, mimetypes_decode, const char *, type, char **, media_type, char **, sub_type);
	DynFunc2(char *, mimetypes_convert_filename, const char *, pathname, const char *, mime_type);
	DynFunc3(bool, mimetypes_add_mime_dir, const char *, path, bool, high_priority, bool, rescan);
	DynFunc2(const char *, mimetypes_get_type_info, const char *, mime_type, const char *, lang);
};

#endif

#if 1
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
#endif

/////////////////////////////////////////////////////////////////////////////
#ifndef XK_Num_Lock
#define XK_Num_Lock                      0xff7f
#endif
#ifndef XK_Shift_Lock
#define XK_Shift_Lock                    0xffe6
#endif
#ifndef XK_Caps_Lock
#define XK_Caps_Lock                     0xffe5
#endif

struct Msg
{
	LViewI *v;
	int m;
	LMessage::Param a, b;
	
	void Set(LViewI *V, int M, LMessage::Param A, LMessage::Param B)
	{
		v = V;
		m = M;
		a = A;
		b = B;
	}
};

// Out of thread messages... must lock before access.
class LMessageQue : public LMutex
{
public:
	typedef ::LArray<Msg> MsgArray;

	LMessageQue() : LMutex("LMessageQue")
	{
	}

	MsgArray *Lock(const char *file, int line)
	{
		if (!LMutex::Lock(file, line))
			return NULL;
		return &q;
	}
	
	operator bool()
	{
		return q.Length() > 0;
	}

private:
	MsgArray q;

}	MsgQue;

/////////////////////////////////////////////////////////////////////////////
LSkinEngine *LApp::SkinEngine = 0;
LApp *TheApp = 0;
LMouseHook *LApp::MouseHook = 0;

LApp::LApp(OsAppArguments &AppArgs, const char *name, LAppArguments *Args) :
	OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	d = new LAppPrivate(this);
	Name(name);
	LgiArgsAppPath = AppArgs.Arg[0];
	if (LIsRelativePath(LgiArgsAppPath))
	{
		char Cwd[MAX_PATH];
		getcwd(Cwd, sizeof(Cwd));
		LMakePath(Cwd, sizeof(Cwd), Cwd, LgiArgsAppPath);
		LgiArgsAppPath = Cwd;
	}

	int WCharSz = sizeof(wchar_t);
	#if defined(_MSC_VER)
	LAssert(WCharSz == 2);
	::LFile::Path Dlls(LgiArgsAppPath);
	Dlls--;
	SetDllDirectoryA(Dlls);
	#else
	LAssert(WCharSz == 4);
	#endif

	#ifdef _MSC_VER
	SetEnvironmentVariable(_T("GTK_CSD"), _T("0"));
	#else
	setenv("GTK_CSD", "0", true);
	#endif
  	
	// We want our printf's NOW!
	setvbuf(stdout,(char *)NULL,_IONBF,0); // print mesgs immediately.

	// Setup the file and graphics sub-systems
	d->FileSystem = new LFileSystem;
	d->GdcSystem = new GdcDevice;

	SetAppArgs(AppArgs);

	srand(LCurrentTime());
	LColour::OnChange();
	AppWnd = NULL;

	Gtk::gchar id[256];
	sprintf_s(id, sizeof(id), "com.memecode.%s", name);
	d->App = gtk_application_new(id, G_APPLICATION_FLAGS_NONE); 
	LAssert(d->App != NULL);

	MouseHook = new LMouseHook;

	#if 0
	// Setup the SIGSEGV signal to call the KDE crash handler
	if (!GetOption("nch") &&
		LGetWindowManager() == WM_Kde)
	{
		signal(SIGSEGV, LgiCrashHandler);
	}
	#endif

	d->GetConfig();

	// System font setup
	LFontType SysFontType;

	Gtk::PangoFontMap *fm = Gtk::pango_cairo_font_map_get_default();
	if (fm)
	{
		using namespace Gtk;
		auto cfm = PANGO_CAIRO_FONT_MAP(fm);
		#ifdef MAC
		double Dpi = 80.0;
		#else
		double Dpi = 96.0;
		#endif

		::LFile::Path p(LSP_APP_ROOT);
		p += "lgi-conf.json";
		if (p.IsFile())
		{
			::LFile f(p, O_READ);
			LJson j(f.Read());
			auto sDpi = j.Get("DPI");
			if (sDpi)
				Dpi = sDpi.Float();
		}

		pango_cairo_font_map_set_resolution(cfm, Dpi);
	}
	
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
	else
	{
		printf("%s:%i - Couldn't get system font setting.\n", __FILE__, __LINE__);
	}

	if (!SystemNormal)
	{
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error: LApp::LApp", MB_OK);
		LExitApp();
	}
	
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
	DeleteObj(SkinEngine);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(LFontSystem::Me);
	DeleteObj(d);
	TheApp = 0;
}

LApp *LApp::ObjInstance()
{
	return TheApp;
}

bool LApp::IsOk()
{
	bool Status =
		#ifndef __clang__
		(this != 0) &&
		#endif
		(d != 0);
					
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
			return 8;
		case LGI_MET_DECOR_Y:
			return 8 + 19;
		case LGI_MET_DECOR_CAPTION:
			return 19;
		default:
			break;
	}

	return 0;
}

LViewI *LApp::GetFocus()
{
	// GtkWidget *w = gtk_window_get_focus(GtkWindow *window);
	return NULL;
}

OsThread LApp::_GetGuiThread()
{
	return d->GuiThread;
}

OsThreadId LApp::GetGuiThreadId()
{
	return d->GuiThreadId;
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

bool LApp::InThread()
{
	OsThreadId Me = GetCurrentThreadId();
	OsThreadId Gui = GetGuiThreadId();
	// printf("Me=%i Gui=%i\n", Me, Gui);
	return Gui == Me;
}

struct GtkIdle
{
	LAppPrivate *d;
	LAppI::OnIdleProc cb;
	void *param;
};

Gtk::gboolean IdleWrapper(Gtk::gpointer data)
{
	#if 0
	
		static int64 ts = LCurrentTime();
		static int count = 0;
		int64 now = LCurrentTime();
		if (now - ts > 300)
		{
			printf("IdleWrapper = %i\n", count);
			count = 0;
			ts = now;
		}
		else count++;
		
	#endif

	GtkIdle *i = (GtkIdle*) data;
	if (i->cb)
		i->cb(i->param);
	
	LMessageQue::MsgArray *Msgs;
	if (MsgQue &&
		(Msgs = MsgQue.Lock(_FL)))
	{
		// printf("IdleWrapper start %i\n", (int)Msgs->Length());

		// Copy the messages out of the locked structure..
		// This allows new messages to arrive independent
		// of us processing them here...
		LMessageQue::MsgArray q = *Msgs;
		Msgs->Empty();
		MsgQue.Unlock();
		
		for (auto m : q)
		{
			if (!LView::LockHandler(m.v, LView::OpExists))
			{
				// LgiTrace("%s:%i - Invalid view: %p.\n", _FL, m.v);
			}
			else
			{
				LMessage Msg(m.m, m.a, m.b);
				// LgiTrace("%s::OnEvent %i,%i,%i\n", m.v->GetClass(), m.m, m.a, m.b);
				m.v->OnEvent(&Msg);
			}
		}
	}
	else
	{
		// printf("IdleWrapper start no lock\n");
	}
	
	// printf("IdleWrapper end\n");
	return i->cb != NULL;
	// return false;
}

static GtkIdle idle = {0};
bool LApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (!InThread())
	{
		printf("%s:%i - Error: Out of thread.\n", _FL);
		return false;
	}

	if (Loop)
	{
		if (!idle.d)
		{
			idle.d = d;
			idle.cb = IdleCallback;
			idle.param = IdleParam;
		}

		static bool CmdLineDone = false;
		if (!CmdLineDone)
		{
			CmdLineDone = true;
			OnCommandLine();
		}
		
	    Gtk::gtk_main();
	}
	else
	{
	    Gtk::gtk_main_iteration_do(false);
	}

	return true;
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
		Gtk::gtk_main_quit();
		if (d->IdleId.Length() > 0)
		{
			size_t Last = d->IdleId.Length() - 1;
			g_source_remove(d->IdleId[Last]);
			d->IdleId.Length(Last);
		}
	}
}

bool LApp::PostEvent(LViewI *View, int Msg, LMessage::Param a, LMessage::Param b)
{
	LMessageQue::MsgArray *q = MsgQue.Lock(_FL);
	if (!q)
	{
		printf("%s:%i - Couldn't lock app.\n", _FL);
		return false;
	}
	
	q->New().Set(View, Msg, a, b);
	
	#if 1 // defined(_DEBUG)
	if (q->Length() > 200 && (q->Length()%20)==0)
	{
		static uint64 prev = 0;
		auto now = LCurrentTime();
		if (now - prev >= 500)
		{
			prev = now;
			#if defined(WIN32)
				char s[256];
				sprintf_s(s, sizeof(s), 
			#else
				printf(
			#endif
				"PostEvent Que=" LPrintfSizeT " (msg=%i)\n", q->Length(), Msg);
			#if defined(WIN32)
				OutputDebugStringA(s);
			#endif
		}
	}
	#endif

	MsgQue.Unlock();
	
	// g_idle_add((GSourceFunc)IdleWrapper, &idle);
	g_idle_add_full(G_PRIORITY_HIGH_IDLE, (GSourceFunc)IdleWrapper, &idle, NULL);
	return true;
}

void LApp::OnUrl(const char *Url)
{
	if (AppWnd)
		AppWnd->OnUrl(Url);
}

void LApp::OnReceiveFiles(::LArray<const char*> &Files)
{
	if (AppWnd)
		AppWnd->OnReceiveFiles(Files);
	else
		LAssert(!"You probably want to set 'AppWnd' before calling LApp::Run... maybe.");
}

const char *LApp::KeyModFlags::FlagName(int Flag)
{
	#define CHECK(f) if (Flag & f) return #f;
	CHECK(GDK_SHIFT_MASK)
	CHECK(GDK_LOCK_MASK)
	CHECK(GDK_CONTROL_MASK)
	CHECK(GDK_MOD1_MASK)
	CHECK(GDK_MOD2_MASK)
	CHECK(GDK_MOD3_MASK)
	CHECK(GDK_MOD4_MASK)
	CHECK(GDK_MOD5_MASK)
	CHECK(GDK_BUTTON1_MASK)
	CHECK(GDK_BUTTON2_MASK)
	CHECK(GDK_BUTTON3_MASK)
	CHECK(GDK_BUTTON4_MASK)
	CHECK(GDK_BUTTON5_MASK)
	CHECK(GDK_SUPER_MASK)
	CHECK(GDK_HYPER_MASK)
	CHECK(GDK_META_MASK)
	CHECK(GDK_RELEASE_MASK)
	#undef CHECK
	return NULL;
}

int LApp::KeyModFlags::FlagValue(const char *Name)
{
	#define CHECK(f) if (!Stricmp(Name, #f)) return f;
	CHECK(GDK_SHIFT_MASK)
	CHECK(GDK_LOCK_MASK)
	CHECK(GDK_CONTROL_MASK)
	CHECK(GDK_MOD1_MASK)
	CHECK(GDK_MOD2_MASK)
	CHECK(GDK_MOD3_MASK)
	CHECK(GDK_MOD4_MASK)
	CHECK(GDK_MOD5_MASK)
	CHECK(GDK_BUTTON1_MASK)
	CHECK(GDK_BUTTON2_MASK)
	CHECK(GDK_BUTTON3_MASK)
	CHECK(GDK_BUTTON4_MASK)
	CHECK(GDK_BUTTON5_MASK)
	CHECK(GDK_SUPER_MASK)
	CHECK(GDK_HYPER_MASK)
	CHECK(GDK_META_MASK)
	CHECK(GDK_RELEASE_MASK)
	#undef CHECK
	return 0;
}

::LString LApp::KeyModFlags::FlagsToString(int s)
{
	::LString::Array a;
	for (int i=0; i<32; i++)
	{
		if (((1 << i) & s) != 0)
			a.New() = FlagName(1 << i);
	}
	return ::LString(",").Join(a);
}

LApp::KeyModFlags *LApp::GetKeyModFlags()
{
	return d->GetModFlags();
}

const char *LApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? NewStr(d->Args.Arg[n]) : 0;
}

bool LApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	::LString Buf;
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

bool LApp::GetOption(const char *Option, ::LString &Buf)
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
								auto Len = End-Arg;
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
	::LArray<const char*> Files;

	for (int i=1; i<GetAppArgs()->Args; i++)
	{
		char *a = GetAppArgs()->Arg[i];
		if (LFileExists(a))
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

::LString LApp::GetFileMimeType(const char *File)
{
	::LString Status;
	char Full[MAX_PATH] = "";

	if (!LFileExists(File))
	{
		// Look in the path
		GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
		for (int i=0; i<p.Length(); i++)
		{
			LMakePath(Full, sizeof(Full), p[i], File);
			if (LFileExists(Full))
			{
				File = Full;
				break;
			}
		}		
	}

	#ifdef __GTK_H__
	
	gboolean result_uncertain;
	gchar *gt = g_content_type_guess(File, NULL, 0, &result_uncertain);
	if (gt)
	{
		gchar *mt = g_content_type_get_mime_type(gt);
		g_free(gt);
		if (mt)
		{
			Status = mt;
			g_free(mt);
			
			if (!Status.Equals("application/octet-stream"))
				return Status;
				
			// So gnome has no clue... lets try 'file'
			::LString args;
			args.Printf("--mime-type \"%s\"", File);
			LSubProcess sub("file", args);
			if (sub.Start())
			{
				LStringPipe p;
				sub.Communicate(&p);
				auto s = p.NewGStr();
				if (s && s.Find(":") > 0)
					Status = s.SplitDelimit(":").Last().Strip();
			}

			return Status;
		}
	}
	
	#endif

	#if HAS_LIB_MAGIC
	
	static bool MagicError = false;
	if (d->MagicLock.Lock(_FL))
	{
		if (!d->hMagic && !MagicError)
		{
			d->hMagic = magic_open(MAGIC_MIME_TYPE);
			if (d->hMagic)
			{
				if (magic_load(d->hMagic, NULL) != 0)
				{
		        	printf("%s:%i - magic_load failed - %s\n", _FL, magic_error(d->hMagic));
		        	magic_close(d->hMagic);
		        	d->hMagic = NULL;
		        	MagicError = true;
				}
			}
			else
			{
				printf("%s:%i - magic_open failed.\n", _FL);
				MagicError = true;
			}
		}
		
		if (d->hMagic && !MagicError)
		{
			const char *mt = magic_file(d->hMagic, File);
			if (mt)
			{
				Status = mt;
			}
		}
		
		d->MagicLock.Unlock();

		if (Status)
			return Status;
	}

	#endif

	#if HAS_SHARED_MIME
	// freedesktop.org rocks...
	if (!d->Sm)
	{
		// Not loaded, go and try to load it...
		d->Sm = new GSharedMime;
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
	LSubProcess p("file", Args);
	if (p.Start())
	{
		p.Communicate(&Output);
		char *Out = Output.NewStr();
		if (Out)
		{
			char *s = strchr(Out, ':');
			if (s && strchr(Out, '/'))
			{
				s += 2;
				
				char *e = s;
				while
				(
					*e &&
					(
						IsAlpha(*e) ||
						IsDigit(*e) ||
						strchr(".-_/", *e)
					)
				)
					e++;
				*e = 0;
				
				Status.Reset(NewStr(s));
			}
			DeleteArray(Out);
		}
	}
	#endif
	
	return Status;
}

bool LApp::GetAppsForMimeType(char *Mime, ::LArray<::LAppInfo*> &Apps)
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

#if defined(LINUX)
LLibrary *LApp::GetWindowManagerLib()
{
	if (this != NULL && !d->WmLib)
	{
		char Lib[32];
		WindowManager Wm = LGetWindowManager();
		switch (Wm)
		{
			case WM_Kde:
				strcpy(Lib, "liblgikde3");
				break;
			case WM_Gnome:
				strcpy(Lib, "liblgignome2");
				break;
			default:
				strcpy(Lib, "liblgiother");
				break;
		}
		#ifdef _DEBUG
		strcat(Lib, "d");
		#endif
		
		d->WmLib = new LLibrary(Lib, true);
		if (d->WmLib)
		{
			if (d->WmLib->IsLoaded())
			{
				Proc_LgiWmInit WmInit = (Proc_LgiWmInit) d->WmLib->GetAddress("LgiWmInit");
				if (WmInit)
				{
					WmInitParams Params;
					// Params.Dsp = XObject::XDisplay();
					Params.Args = d->Args.Args;
					Params.Arg = d->Args.Arg;
					
					WmInit(&Params);
				}
				// else printf("%s:%i - Failed to find method 'LgiWmInit' in WmLib.\n", __FILE__, __LINE__);
			}
			// else printf("%s:%i - couldn't load '%s.so'\n", __FILE__, __LINE__, Lib);
		}
		// else printf("%s:%i - alloc error\n", __FILE__, __LINE__);
	}
	
	return d->WmLib && d->WmLib->IsLoaded() ? d->WmLib : 0;
}

void LApp::DeleteMeLater(LViewI *v)
{
	d->DeleteLater.Add(v);
}

void LApp::SetClipBoardContent(OsView Hnd, ::LVariant &v)
{
	// Store the clipboard data we will serve
	d->ClipData = v;
}

bool LApp::GetClipBoardContent(OsView Hnd, ::LVariant &v, ::LArray<char*> &Types)
{
	return false;
}
#endif

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

int LApp::GetCpuCount()
{
	return 1;
}

LFontCache *LApp::GetFontCache()
{
	if (!d->FontCache)
		d->FontCache.Reset(new LFontCache(SystemNormal));
	return d->FontCache;
}

#ifdef LINUX
LApp::DesktopInfo::DesktopInfo(const char *file)
{
	File = file;
	Dirty = false;
	if (File)
		Serialize(false);
}

bool LApp::DesktopInfo::Serialize(bool Write)
{
	::LFile f;
	
	if (Write)
	{
		::LFile::Path p(File);
		p--;
		if (!p.Exists())
			return false;
	}
	else if (!LFileExists(File))	
		return false;
	
	if (!f.Open(File, Write?O_WRITE:O_READ))
	{
		LgiTrace("%s:%i - Failed to open '%s'\n", _FL, File.Get());
		return false;
	}

	if (Write)
	{
		f.SetSize(0);
		for (unsigned i=0; i<Data.Length(); i++)
		{
			Section &s = Data[i];
			if (s.Name)
				f.Print("[%s]\n", s.Name.Get());
			for (unsigned n=0; n<s.Values.Length(); n++)
			{
				KeyPair &kp = s.Values[n];
				f.Print("%s=%s\n", kp.Key.Get(), kp.Value.Get());
			}
			f.Print("\n");
		}
		Dirty = false;
	}
	else
	{
		::LString::Array Lines = f.Read().Split("\n");
		Section *Cur = NULL;
		for (unsigned i=0; i<Lines.Length(); i++)
		{
			::LString l = Lines[i].Strip();
			if (l.Length() < 1)
				continue;
			int s = l.Find("[");
			if (s >= 0)
			{
				int e = l.Find("]", ++s);
				if (e >= 0)
				{
					Cur = &Data.New();
					Cur->Name = l(s, e - s + 1);
				}
			}
			else if ((s = l.Find("=")) >= 0)
			{
				if (!Cur)
					Cur = &Data.New();
				KeyPair &kp = Cur->Values.New();
				kp.Key = l(0, s).Strip();
				kp.Value = l(++s, -1).Strip();
				
				// printf("Read '%s': '%s'='%s'\n", Cur->Name.Get(), kp.Key.Get(), kp.Value.Get());
			}
		}
	}

	return true;
}

LApp::DesktopInfo::Section *LApp::DesktopInfo::GetSection(const char *Name, bool Create)
{
	for (unsigned i=0; i<Data.Length(); i++)
	{
		Section &s = Data[i];
		if (s.Name && s.Name == Name)
			return &s;
		if (!s.Name && !Name)
			return &s;
	}
	if (Create)
	{
		Section &s = Data.New();
		s.Name = Name;
		Dirty = true;
		return &s;
	}
	
	return NULL;
}

static const char *DefaultSection = "Desktop Entry";

::LString LApp::DesktopInfo::Get(const char *Field, const char *Sect)
{
	if (Field)
	{
		Section *s = GetSection(Sect ? Sect : DefaultSection, false);
		if (s)
		{
			KeyPair *kp = s->Get(Field, false, Dirty);
			if (kp)
			{
				return kp->Value;
			}
		}
	}
	
	return ::LString();
}

bool LApp::DesktopInfo::Set(const char *Field, const char *Value, const char *Sect)
{
	if (!Field)
		return false;

	Section *s = GetSection(Sect ? Sect : DefaultSection, true);
	if (!s)
		return false;

	KeyPair *kp = s->Get(Field, true, Dirty);
	if (!kp)
		return false;

	if (kp->Value != Value)
	{
		kp->Value = Value;
		Dirty = true;
	}
	return true;
}

LApp::DesktopInfo *LApp::GetDesktopInfo()
{
	auto sExe = LGetExeFile();
	::LFile::Path Exe(sExe);
	::LFile::Path Desktop(LSP_HOME);
	::LString Leaf;
	Leaf.Printf("%s.desktop", Exe.Last().Get());
	
	Desktop += ".local/share/applications";
	Desktop += Leaf;
	
	const char *Ex = Exe;
	const char *Fn = Desktop;

	if (d->DesktopInfo.Reset(new DesktopInfo(Desktop)))
	{
		// Do a sanity check...
		::LString s = d->DesktopInfo->Get("Name");
		if (!s && Name())
			d->DesktopInfo->Set("Name", Name());
		
		s = d->DesktopInfo->Get("Exec");
		if (!s || s != (const char*)sExe)
			d->DesktopInfo->Set("Exec", sExe);
		
		s = d->DesktopInfo->Get("Type");
		if (!s) d->DesktopInfo->Set("Type", "Application");

		s = d->DesktopInfo->Get("Categories");
		if (!s) d->DesktopInfo->Set("Categories", "Application;");

		s = d->DesktopInfo->Get("Terminal");
		if (!s) d->DesktopInfo->Set("Terminal", "false");
		
		d->DesktopInfo->Update();
	}
	
	return d->DesktopInfo;
}

bool LApp::SetApplicationIcon(const char *FileName)
{
	DesktopInfo *di = GetDesktopInfo();
	if (!di)
		return false;
	
	::LString IcoPath = di->Get("Icon");
	if (IcoPath == FileName)
		return true;
	
	di->Set("Icon", FileName);
	return di->Update();
}
#endif

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
void LMessage::Set(int Msg, Param ParamA, Param ParamB)
{
	m = Msg;
	a = ParamA;
	b = ParamB;
}

struct GlibEventParams : public LMessage
{
    GtkWidget *w;
};

bool GlibWidgetSearch(GtkWidget *p, GtkWidget *w, bool Debug, int depth)
{
	char indent[256] = "";

	if (Debug)
	{	
		int ch = depth * 2;
		memset(indent, ' ', ch);
		indent[ch] = 0;

		printf("%sGlibWidgetSearch: %p, %p\n", indent, p, w);
	}
	
	if (p == w)
		return true;

	if (GTK_IS_CONTAINER(p))
	{
		int n = 0;
		Gtk::GList *top = gtk_container_get_children(GTK_CONTAINER(p));
		Gtk::GList *i = top;
		while (i)
		{
			if (Debug)
				printf("%s[%i]=%s\n", indent, n, gtk_widget_get_name((GtkWidget*)i->data));

			if (i->data == w)
				return true;
			else if (GlibWidgetSearch((GtkWidget*)i->data, w, Debug, depth + 1))
				return true;

			i = i->next;
			n++;
		}
		g_list_free(top);
	}
	else if (GTK_IS_BIN(p))
	{
	    GtkWidget *child = gtk_bin_get_child(GTK_BIN(p));
	    if (child)
	    {
	    	if (Debug)
				printf("%schild=%s\n", indent, gtk_widget_get_name(child));
			if (child == w)
				return true;
			else if (GlibWidgetSearch(child, w, Debug, depth + 1))
				return true;
		}
    }
    else if (Debug)
    {
		printf("%sUnknown=%s\n", indent, gtk_widget_get_name(p));
	}

	return false;
}

void LApp::OnDetach(LViewI *View)
{
	LMessageQue::MsgArray *q = MsgQue.Lock(_FL);
	if (!q)
	{
		printf("%s:%i - Couldn't lock app.\n", _FL);
		return;
	}

	MsgQue.Unlock();
}

bool LMessage::Send(LViewI *View)
{
	if (!View)
	{
		LAssert(!"No view");
		return false;
	}
	
	return LAppInst->PostEvent(View, m, a, b);
}

