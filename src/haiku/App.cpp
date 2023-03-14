#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Array.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Token.h"
#include "lgi/common/FontCache.h"
#include "AppPriv.h"

#include "MimeType.h"

#define DEBUG_MSG_TYPES				0
#define DEBUG_HND_WARNINGS			0
#define IDLE_ALWAYS					0

LString LgiArgsAppPath;

////////////////////////////////////////////////////////////////
struct OsAppArgumentsPriv
{
	::LArray<const char*> Ptr;
	
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

class LSharedMime : public LLibrary
{
public:
	LSharedMime() :
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
#define XK_Num_Lock						 0xff7f
#endif
#ifndef XK_Shift_Lock
#define XK_Shift_Lock					 0xffe6
#endif
#ifndef XK_Caps_Lock
#define XK_Caps_Lock					 0xffe5
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
LApp *TheApp = NULL;
LSkinEngine *LApp::SkinEngine;
LMouseHook *LApp::MouseHook;

LApp::LApp(OsAppArguments &AppArgs, const char *name, LAppArguments *Args) :
	OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	LgiArgsAppPath = AppArgs.Arg[0];
	Name(name);
	d = new LAppPrivate(this);
	
	if (LIsRelativePath(LgiArgsAppPath))
	{
		char Cwd[MAX_PATH_LEN];
		getcwd(Cwd, sizeof(Cwd));
		LMakePath(Cwd, sizeof(Cwd), Cwd, LgiArgsAppPath);
		LgiArgsAppPath = Cwd;
	}
	
	char AppPathLnk[MAX_PATH_LEN];
	if (LResolveShortcut(LgiArgsAppPath, AppPathLnk, sizeof(AppPathLnk)))
		LgiArgsAppPath = AppPathLnk;

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

	MouseHook = new LMouseHook;

	d->GetConfig();

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
	CommonCleanup();

	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	DeleteObj(SkinEngine);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(LFontSystem::Me);
	DeleteObj(d);
	
	TheApp = NULL;
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

bool LApp::Run(OnIdleProc IdleCallback, void *IdleParam)
{
	if (!InThread())
	{
		printf("%s:%i - Error: Out of thread.\n", _FL);
		return false;
	}

	printf("Running main loop...\n");
	d->Run();
	printf("Main loop finished.\n");

	return true;
}

bool LApp::Yield()
{
	return false;
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
		printf("Quitting main loop...\n");
		if (d->Lock())
		{
			d->Quit();
			d->Unlock();
		}
	}
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
			auto *a = d->Args.Arg[i];
			if (!a)
				continue;

			if (strchr("-/\\", a[0]))
			{
				if (strnicmp(a+1, Option, OptLen) == 0)
				{
					const char *Arg = NULL;
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
		auto a = GetAppArgs()->Arg[i];
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

LString LApp::GetFileMimeType(const char *File)
{
	BMimeType mt;
	auto r = BMimeType::GuessMimeType(File, &mt);
	if (r != B_OK)
	{
		LgiTrace("%s:%i - GuessMimeType(%s) failed: %i\n", _FL, File, r);
		return LString();
	}
	
	return mt.Type();
}

bool LApp::GetAppsForMimeType(const char *Mime, LArray<LAppInfo> &Apps)
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
	return NULL;
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

OsApplication::OsApplication(int Args, const char **Arg)
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
int LMessage::Msg()
{
	return what;
}

LMessage::Param LMessage::A()
{
	int64 a = 0;
	if (FindInt64(PropA, &a) != B_OK)
	{
		int32 c = CountNames(B_ANY_TYPE);
		printf("%s:%i - Failed to find PropA (%i names)\n", _FL, c);
		for (int32 i=0; i<c; i++)
		{
			char *name = NULL;
			type_code type;
			int32 count = 0;
			status_t r = GetInfo(B_ANY_TYPE, i, &name, &type, &count);
			if (r == B_OK)
				printf("\t%x %s[%i]\n", type, name, count);
		}
	}
	return a;
}

LMessage::Param LMessage::B()
{
	int64 b = 0;
	if (FindInt64(PropB, &b) != B_OK)
		printf("%s:%i - Failed to find PropB.\n", _FL);
	return b;
}

void LMessage::Set(int Msg, Param a, Param b)
{
	what = Msg;
	AddInt64(PropA, a);
	AddInt64(PropA, b);
}

bool LMessage::Send(LViewI *View)
{
	if (!View)
	{
		LAssert(!"No view");
		return false;
	}
	
	return View->PostEvent(what, A(), B());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
LLocker::LLocker(BHandler *h, const char *File, int Line)
{
	hnd = h;
	file = File;
	line = Line;
}

LLocker::~LLocker()
{
	Unlock();
}

bool LLocker::Lock(bool debug)
{
	if (locked)
	{
		printf("%s:%i - Locker already locked.\n", file, line);
		LAssert(!"Locker already locked.");
		return false;
	}
	
	if (!hnd)
	{
		if (debug)
			printf("%s:%i - Locker hnd is NULL.\n", file, line);
		return false;
	}
	
	auto looper = hnd->Looper();
	if (!looper)
	{
		if (debug)
			printf("%s:%i - Locker looper is NULL.\n", file, line);
		return false;
	}
	
	thread_id threadId = looper->Thread();
	if (threadId <= 0)
	{
		if (debug)
			printf("%s:%i - Looper has no thread?!?!\n", file, line);
		noThread = true;
		return locked = true;
	}

	while (!locked)
	{
		status_t result = hnd->LockLooperWithTimeout(1000 * 1000);
		if (result == B_OK)
		{
			locked = true;
			break;
		}
		else if (result == B_TIMED_OUT)
		{
			// Warn about failure to lock...
			auto cur = GetCurrentThreadId();
			auto locking = hnd->Looper()->LockingThread();
			
			printf("%s:%i - Warning: can't lock. cur=%i locking=%i\n",
				_FL,
				cur,
				locking);
		}
		else if (result == B_BAD_VALUE)
		{
			break;
		}
		else
		{
			// Warn about error
			printf("%s:%i - Error from LockLooperWithTimeout = 0x%x\n", _FL, result);
		}
	}

	return locked;
}

status_t LLocker::LockWithTimeout(int64 time)
{
	LAssert(!locked);
	
	if (!hnd)
	{
		// printf("%s:%i - Locker hnd is NULL.\n", file, line);
		return false;
	}

	auto looper = hnd->Looper();
	if (!looper)
	{
		// printf("%s:%i - Locker looper is NULL %i.\n", file, line, count);
		return false;
	}
	
	thread_id threadId = looper->Thread();
	if (threadId <= 0)
	{
		// printf("%s:%i - Looper has no thread?!?!\n", file, line);
		noThread = true;
		return locked = true;
	}

	status_t result = hnd->LockLooperWithTimeout(time);
	if (result == B_OK)
		locked = true;
	
	return result;
}

void LLocker::Unlock()
{
	if (noThread)
	{
		locked = false;
		return;
	}

	if (locked)
	{
		hnd->UnlockLooper();
		locked = false;
	}
}
