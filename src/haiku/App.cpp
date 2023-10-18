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
	LArray<const char*> Ptr;
	
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
LApp *TheApp = NULL;
LSkinEngine *LApp::SkinEngine = NULL;
LMouseHook *LApp::MouseHook = NULL;

LApp::LApp(OsAppArguments &AppArgs, const char *name, LAppArguments *Args) :
	OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	LThread::RegisterThread(LCurrentThreadId(), LString("LApp.")+name);
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
	LAssert(WCharSz == 4);

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
	DeleteObj(SkinEngine);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(LFontSystem::Me);
	DeleteObj(d);
	
	TheApp = NULL;
}

LFont *LApp::GetFont(bool bold)
{
	auto cur = LCurrentThreadId();

	LHashTbl<IntKey<OsThreadId>,LFont*> &store = bold ? d->BoldFont : d->SystemFont;
	auto sysFont = store.Find(cur);
	if (sysFont)
		return sysFont;

	LFontType SysFontType;
	if (SysFontType.GetSystemFont("System"))
	{
		sysFont = SysFontType.Create();
		if (sysFont)
			sysFont->Transparent(true);
		
		if (bold)
			sysFont->Bold(true);

		sysFont->Create();		
		store.Add(cur, sysFont);
	}
	else
	{
		printf("%s:%i - Couldn't get system font setting.\n", __FILE__, __LINE__);
	}

	return sysFont;
}

LApp *LApp::ObjInstance()
{
	return TheApp;
}

bool LApp::IsOk() const
{
	bool Status =
		#ifndef __clang__
		(this != NULL) &&
		#endif
		(d != NULL);
					
	LAssert(Status);
	return Status;
}

LMouseHook *LApp::GetMouseHook()
{
	return MouseHook;
}

int32 LApp::GetMetric(LSystemMetric Metric)
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
	return getpid();
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
	OsThreadId Me = LCurrentThreadId();
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

	if (!d->CmdLineProcessed)
	{
		d->CmdLineProcessed = true;
		OnCommandLine();
	}

	d->Run();
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

void LApp::OnReceiveFiles(LArray<const char*> &Files)
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
	LArray<const char*> Files;

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
		OnReceiveFiles(Files);

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

LSymLookup *LApp::GetSymLookup()
{
	return NULL;
}

bool LApp::IsElevated()
{
	return geteuid() == 0;
}

int LApp::GetCpuCount()
{
	system_info info;
	auto result = get_system_info(&info);
	return result == B_OK ? info.cpu_count : 1;
}

LFontCache *LApp::GetFontCache()
{
	auto cur = LCurrentThreadId();
	auto cache = d->FontCache.Find(cur);
	if (!cache)
	{
		cache = new LFontCache(GetFont(false));
		d->FontCache.Add(cur, cache);
	}
	return cache;
}

////////////////////////////////////////////////////////////////
OsApplication *OsApplication::Inst = NULL;

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
	AddInt64(PropB, b);
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
struct LWatchList : public LMutex
{
	LArray<BHandler*> Watches;
	LWatchList() : LMutex("LWatchList") {}
};
static LWatchList Watch;

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

void LLocker::AddWatch(BHandler *h)
{
	LAssert(h);
	Watch.Watches.Add(h);
	printf("%s:%i - add watch %p\n", _FL, h);
}

LLocker::operator bool()
{
	if (!locked && !attempted)
		printf("%s:%i - No attempt to call LLocker::Lock() before checking state?\n", _FL);

	return locked;
}

bool LLocker::Lock(bool debug)
{
	if (!debug && Watch.Watches.HasItem(hnd))
		debug = true;

	if (locked)
	{
		printf("%s:%i - Locker already locked.\n", file, line);
		LAssert(!"Locker already locked.");
		return false;
	}

	attempted = true;	
	if (!hnd)
	{
		if (debug)
			printf("%s:%i - Locker hnd is NULL.\n", file, line);
		return false;
	}
	
	auto looper = hnd->Looper();
	if (!looper)
	{
		// if (debug)
		// 	printf("%s:%i - Locker looper is NULL.\n", file, line);
		return false;
	}
	
	thread_id threadId = looper->Thread();
	if (threadId < 0)
	{
		if (debug)
			printf("%s:%i - Looper has no thread?!?!\n", file, line);
		noThread = true;
		locked = true;
		startTs = LCurrentTime();
		lockingThread = LCurrentThreadId();
		return true;
	}

	while (!locked)
	{
		status_t result = hnd->LockLooperWithTimeout(1000 * 1000);
		if (result == B_OK)
		{
			locked = true;
			startTs = LCurrentTime();
			lockingThread = LCurrentThreadId();
			
			if (debug)
				printf("%s:%i - %s:%i locked.\n", _FL, file, line);
			break;
		}
		else if (result == B_TIMED_OUT)
		{
			// Warn about failure to lock...
			auto cur = LCurrentThreadId();
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

status_t LLocker::LockWithTimeout(int64 time, bool debug)
{
	LAssert(!locked);

	if (!debug && Watch.Watches.HasItem(hnd))
		debug = true;
	
	if (!hnd)
	{
		if (debug)
			printf("%s:%i - Locker hnd is NULL.\n", file, line);
		return B_ERROR;
	}

	auto looper = hnd->Looper();
	if (!looper)
	{
		if (debug)
			printf("%s:%i - Locker looper is NULL.\n", file, line);
		return B_ERROR;
	}
	
	thread_id threadId = looper->Thread();
	if (threadId <= 0)
	{
		if (debug)
			printf("%s:%i - Looper has no thread?!?!\n", file, line);
		noThread = true;
		locked = true;
		lockingThread = LCurrentThreadId();
		startTs = LCurrentTime();

		if (debug)
			printf("%s:%i - %s:%i locked.\n", _FL, file, line);
		return B_OK;
	}

	status_t result = hnd->LockLooperWithTimeout(time);
	if (result == B_OK)
	{
		locked = true;
		lockingThread = LCurrentThreadId();
		startTs = LCurrentTime();
	}
	
	return result;
}

bool LLocker::WaitForLock(int timeout)
{
	auto start = LCurrentTime();
	bool debug = Watch.Watches.HasItem(hnd);
	
	while (true)
	{
		auto result = LockWithTimeout(1, debug);
		if (result == B_OK)
			return true;
		
		if (result == B_TIMED_OUT)
		{
			auto now = LCurrentTime();
			if (now - start >= timeout)
			{
				auto looper = hnd ? hnd->Looper() : NULL;
				LgiTrace("%s:%i WaitForLock timing out: cur=%i:%s, locker=%i:%s (%s:%i)\n",
					_FL,
					LCurrentThreadId(), LThread::GetThreadName(LCurrentThreadId()),
					looper ? looper->LockingThread() : -1, looper ? LThread::GetThreadName(looper->LockingThread()) : "none",
					file, line);
				return false;
			}
			
			LSleep(5);
		}
		else break; // B_ERROR etc...
	}
	
	return false;
}

void LLocker::Unlock()
{
	bool debug = Watch.Watches.HasItem(hnd);

	if (noThread)
	{
		locked = false;
		lockingThread = -1;
		return;
	}

	if (locked)
	{
		if (lockingThread != LCurrentThreadId())
		{
			printf("%s:%i - Lock/Unlock thread mismatch: locking=%i/%s != unlocking=%i/%s\n",
				_FL,
				lockingThread, LThread::GetThreadName(lockingThread),
				LCurrentThreadId(), LThread::GetThreadName(LCurrentThreadId()));
		}
		
		hnd->UnlockLooper();
		locked = false;
		auto now = LCurrentTime();
		if (now - startTs >= 1000)
			printf("%s:%i held lock for more than a second.\n", file, line);
			
		if (debug)
			printf("%s:%i unlock %s:%i\n", _FL, file, line);
	}
}
