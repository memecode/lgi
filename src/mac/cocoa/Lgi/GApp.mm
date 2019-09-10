#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#import <AppKit/AppKit.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GSkinEngine.h"
#include "GArray.h"
#include "GToken.h"
#include "LThread.h"
#include "GXmlTree.h"
#include "GSymLookup.h"
#include "GFontCache.h"

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#import "LCocoaView.h"
#include "GEventTargetThread.h"

extern int hndstate(int hnd);

struct OsAppArgumentsPriv
{
	GAutoString Str;
	GArray<char*> Ptr;
};

OsAppArguments::OsAppArguments(int args, const char **arg)
{
	d = new OsAppArgumentsPriv;
	Args = args;
	Arg = arg;
}

OsAppArguments::~OsAppArguments()
{
	DeleteObj(d);
}

void OsAppArguments::Set(char *CmdLine)
{
	d->Str.Reset();
	d->Ptr.Length(0);
	
	GArray<char> Raw;
	GArray<size_t> Offsets;
	
	auto Exe = LGetExeFile();
	Offsets.Add(0);
	if (Exe)
	{
		Raw.Length(Exe.Length() + 1);
		strcpy(Raw.AddressOf(), Exe);
	}
	else
	{
		Raw.Add(0);
	}
	
	if (CmdLine)
	{
		for (char *s = CmdLine; *s; )
		{
			while (*s && strchr(WhiteSpace, *s)) s++;
			char *e;
			if (*s == '\'' || *s == '\"')
			{
				char delim = *s++;
				Offsets.Add(Raw.Length());
				e = s;
				while (*e && *e != delim)
				{
					Raw.Add(*e++);
				}
				Raw.Add(0);
			}
			else
			{
				Offsets.Add(Raw.Length());
				for (e = s; *e && !strchr(WhiteSpace, *e); e++)
				{
					Raw.Add(*e);
				}
				Raw.Add(0);
			}
			s = *e ? e + 1 : e;
		}
	}
	
	d->Str.Reset(Raw.Release());
	for (int n=0; n<Offsets.Length(); n++)
	{
		d->Ptr[n] = d->Str + Offsets[n];
	}
	
	Args = (int)d->Ptr.Length();
	Arg = (const char**) &d->Ptr[0];
}

OsAppArguments &OsAppArguments::operator =(OsAppArguments &a)
{
	GArray<char> Raw;
	GArray<size_t> Offsets;
	for (int i=0; i<a.Args; i++)
	{
		size_t Len = strlen(a.Arg[i]) + 1;
		Offsets[i] = Raw.Length();
		Raw.Length(Offsets[i] + Len);
		memcpy(&Raw[Offsets[i]], a.Arg[i], Len);
	}
	
	d->Str.Reset(new char[Raw.Length()]);
	memcpy(d->Str, &Raw[0], Raw.Length());
	for (int n=0; n<a.Args; n++)
	{
		d->Ptr[n] = d->Str + Offsets[n];
	}
	
	Args = (int)d->Ptr.Length();
	Arg = (const char**) &d->Ptr[0];
	
	return *this;
}

////////////////////////////////////////////////////////////////
#if defined(MAC_OS_X_VERSION_10_12) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
	#define SDK_10_12(newSym, oldSym) newSym
#else
	#define SDK_10_12(newSym, oldSym) oldSym
#endif

void GMouse::SetFromEvent(NSEvent *ev, NSView *view)
{
	auto r = view.frame;
	auto pt = ev.locationInWindow;

	GMouse m;
	x = (int)pt.x;
	y = (int)(r.size.height - pt.y);
	SetModifer((uint32)ev.modifierFlags);

	switch (ev.type)
	{
		case SDK_10_12(NSEventTypeLeftMouseDown,  NX_LMOUSEDOWN):
			Down(true); Left(true);
			break;
		case SDK_10_12(NSEventTypeLeftMouseUp,    NX_LMOUSEUP):
			Down(false); Left(true);
			break;
		case SDK_10_12(NSEventTypeRightMouseDown, NX_RMOUSEDOWN):
			Down(true); Right(true);
			break;
		case SDK_10_12(NSEventTypeRightMouseUp,   NX_RMOUSEUP):
			Down(false); Right(true);
			break;
		case SDK_10_12(NSEventTypeOtherMouseDown, NX_OMOUSEDOWN):
			Down(true); Middle(true);
			break;
		case SDK_10_12(NSEventTypeOtherMouseUp,   NX_OMOUSEUP):
			Down(false); Middle(true);
			break;
		case SDK_10_12(NSEventTypeMouseMoved,     NX_MOUSEMOVED):
			IsMove(true);
			break;
		case SDK_10_12(NSEventTypeLeftMouseDragged, NX_LMOUSEDRAGGED):
			Down(true); IsMove(true); Left(true);
			break;
		case SDK_10_12(NSEventTypeRightMouseDragged, NX_RMOUSEDRAGGED):
			Down(true); IsMove(true); Right(true);
			break;
		default:
			LgiAssert(!"Unknown event.");
			break;
	}

	Double(ev.clickCount == 2 && Down());
}

void GUiEvent::SetModifer(uint32_t modifierKeys)
{
	System(modifierKeys & SDK_10_12(NSEventModifierFlagCommand, NSCommandKeyMask));
	Shift (modifierKeys & SDK_10_12(NSEventModifierFlagShift,   NSShiftKeyMask));
	Alt   (modifierKeys & SDK_10_12(NSEventModifierFlagOption,  NSAlternateKeyMask));
	Ctrl  (modifierKeys & SDK_10_12(NSEventModifierFlagControl, NSControlKeyMask));
}

void GMessage::Set(int msg, Param A, Param B)
{
	m = msg;
	a = A;
	b = B;
}

////////////////////////////////////////////////////////////////
typedef GArray<GAppInfo*> AppArray;

void OnSigPipe(int i)
{
}

void OnCrash(int i)
{
	printf("%s:%i - on crash.\n", __FILE__, __LINE__);
	signal(SIGBUS, 0);
	signal(SIGSEGV, 0);
	
	struct Pipe
	{
		int Read;
		int Write;
		
		Pipe()
		{
			Read = -1;
			Write = -1;
		}
	};
	
	
	Pipe Read;
	Pipe Write;
	Pipe Error;
	int Pid;
	
	pipe((int*)&Read);
	pipe((int*)&Error);
	
	auto Exe = LGetExeFile();
	
	// Has stdin pipe
	pipe((int*)&Write);
	if (!(Pid = fork()))
	{
		// stdin -> Write
		close(0);			// close stdin
		dup(Write.Read);
		close(Write.Write);
		
		// stdout -> Read
		close(1);			// close stdout
		dup(Read.Write);
		close(Read.Read);
		
		// stderr -> Error
		close(2);			// close stderr
		dup(Error.Write);
		close(Error.Read);
		
		// setup read & write handles
		char sPid[32];
		sprintf(sPid, "--pid=%i", getpid());
		char *Args[] = {sPid, Exe, 0};
		// printf("Calling: execv('gdb', '%s', '%s');\n", Exe, sPid);
		execv("/usr/bin/gdb", Args);
		
		// We should never get here
		printf("%s:%i - execv(gdb) failed.\n", __FILE__, __LINE__);
		exit(-1);
	}
	
	ssize_t r, Used = 0;
	char Buf[1025];
	bool Capture = false;
	while ((r = read(Read.Read, Buf + Used, sizeof(Buf) - Used - 1)) > 0)
	{
		printf("Got %i bytes\n", (int)r);
		Used += r;
		Buf[Used] = 0;
		
		printf("Capt=%i Buf='%s'\n", Capture, Buf);
		if (!Capture)
		{
			if (stristr(Buf, "(gdb)"))
			{
				char c[] = "info pid\n";
				ssize_t w = write(Write.Write, c, strlen(c));
				printf("Writing cmd %i bytes\n", (int)w);
				Capture = true;
				Used = 0;
				Buf[0] = 0;
			}
		}
		
		char *Eol;
		while ((Eol = strstr(Buf, "\n")))
		{
			*Eol = 0;
			
			if (Capture)
			{
				printf("Capture '%s'\n", Buf);
			}
			
			Eol += 1;
			ptrdiff_t Len = Eol - Buf;
			memmove(Buf, Eol, Used + 1 - Len);
			Used -= Len;
		}
	}
	
	exit(-1);
}

class GAppPrivate
{
public:
	OsApp NsApp;
	int RunDepth;

	// Common
	GXmlTag *Config;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	GSymLookup SymLookup;
	GAutoString Mime;
	GAutoString Name;
	GAutoString UrlArg;
	
	/// Any fonts needed for styling the elements
	GAutoPtr<GFontCache> FontCache;
	
	GAppPrivate()
	{
		NsApp = NULL;
		RunDepth = 0;
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
		GuiThread = LgiGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
	}
	
	~GAppPrivate()
	{
		DeleteObj(SkinLib);
		
		for (auto p : MimeToApp)
		{
			p.value->DeleteObjects();
			DeleteObj(p.value);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////
@implementation LNsApplication

- (id)init:(GAppPrivate*)priv
{
	if ((self = [super init]) != nil)
	{
		self.d = priv;
		printf("LNsApplication.init\n");
	}
	return self;
}

- (void)terminate:(nullable id)sender
{
	[super terminate:sender];
}

- (void)dealloc
{
	[super dealloc];
	printf("LNsApplication.dealloc\n");
}

- (void)assert:(LCocoaAssert*)ca
{
	NSAlert *a = [[NSAlert alloc] init];
	a.messageText = ca.msg.NsStr();
	a.alertStyle = NSAlertStyleCritical;
	[a addButtonWithTitle:@"Debug"];
	[a addButtonWithTitle:@"Ignore"];
	[a addButtonWithTitle:@"Abort"];
	
	ca.result = [a runModal];
	
	[a release];
}

@end
/////////////////////////////////////////////////////////////////////////////
GSkinEngine *GApp::SkinEngine = 0;
GApp *TheApp = 0;
GMouseHook *GApp::MouseHook = 0;

GApp::GApp(OsAppArguments &AppArgs, const char *AppName, GAppArguments *ObjArgs) :
OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	d = new GAppPrivate;
	d->Name.Reset(NewStr(AppName));
	AppWnd = 0;
	Name(AppName);
	if (LgiIsRelativePath(AppArgs.Arg[0]))
	{
		char wd[MAX_PATH];
		char exe[MAX_PATH];
		if (LgiMakePath(exe, sizeof(exe), getcwd(wd, sizeof(wd)), AppArgs.Arg[0]))
			LgiArgsAppPath = exe;
		else
			printf("%s:%i - LgiMakePath for Exe failed.\n", _FL);
	}
	else
		LgiArgsAppPath = AppArgs.Arg[0];
	
	printf("%s:%i - LgiArgsAppPath='%s'\n", _FL, LgiArgsAppPath.Get());
	
	// Catch and ignore SIGPIPE
	signal(SIGPIPE, OnSigPipe);
	
#if 0
	// Crash handler...
	signal(SIGBUS, OnCrash);
	signal(SIGSEGV, OnCrash);
#endif
	
	// We want our printf's NOW!
	setvbuf(stdout,(char *)NULL,_IONBF,0); // print mesgs immediately.
	
	// Connect to the server
	d->NsApp = [LNsApplication sharedApplication];

	#if 0
	auto mainMenu = [d->NsApp mainMenu];
	if (!mainMenu)
	{
		mainMenu = [[NSMenu alloc] initWithTitle:@"App"];
	}
	NSMenuItem *appItem = [mainMenu itemAtIndex:0];
	if (!appItem)
	{
		appItem = [[NSMenuItem alloc] initWithTitle:@"appItem" action:NULL keyEquivalent:@""];
		[mainMenu addItem:appItem];
	}
	NSMenu *appMenu = [appItem submenu];
	if (!appMenu)
	{
		appMenu = [[NSMenu alloc] initWithTitle:@"AppMenu"];
		[appItem setSubmenu:appMenu];
	}
	if (appMenu)
	{
		auto myItem = [[NSMenuItem alloc] initWithTitle:@"Quit" action:NULL keyEquivalent:@"Q"];
		[myItem setKeyEquivalentModifierMask: NSCommandKeyMask];
		[appMenu addItem:myItem];
	}
	
	[d->NsApp setMainMenu:mainMenu];
	#endif
	
	// Setup the file and graphics sub-systems
	d->FileSystem = new GFileSystem;
	d->GdcSystem = new GdcDevice;
	
	srand((unsigned)LgiCurrentTime());
	GColour::OnChange();
	
	SetAppArgs(AppArgs);
	MouseHook = new GMouseHook;
	
	// System font setup
	SystemNormal = 0;
	GFontType SysFontType;
	
	if (SysFontType.GetSystemFont("System"))
	{
		SystemNormal = SysFontType.Create();
		if (SystemNormal)
		{
			SystemNormal->Transparent(true);
		}
		else
		{
			printf("%s:%i - Error creating system font.\n", __FILE__, __LINE__);
		}
		
		SystemBold = SysFontType.Create();
		if (SystemBold)
		{
			SystemBold->Bold(true);
			SystemBold->Transparent(true);
			SystemBold->Create();
		}
		else
		{
			printf("%s:%i - Error creating bold version of the system font.\n", __FILE__, __LINE__);
		}
	}
	else
	{
		printf("%s:%i - Couldn't get system font setting.\n", __FILE__, __LINE__);
	}
	
	if (!SystemNormal)
	{
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error: GApp::GApp", MB_OK);
		LgiExitApp();
	}
	
	if (!GetOption("noskin"))
	{
		extern GSkinEngine *CreateSkinEngine(GApp *App);
		SkinEngine = CreateSkinEngine(this);
	}
	
	Default.Reset(new LMenu);
}

GApp::~GApp()
{
	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(d->Config);
	DeleteObj(GFontSystem::Me);
	DeleteObj(d);
	TheApp = 0;
}

OsApp &GApp::Handle()
{
	return d->NsApp;
}

bool GApp::PostEvent(GViewI *View, int Msg, GMessage::Param A, GMessage::Param B)
{
	if (!View)
	{
		printf("%s:%i - No view.\n", _FL);
		return false;
	}
	
	bool Exists = GView::LockHandler(View, GView::LockOp::OpExists);
	if (!Exists)
	{
		printf("%s:%i - View deleted.\n", _FL);
		return false;
	}

	GWindow *w = View->GetWindow();
	if (!w)
	{
		// printf("%s:%i - No window.\n", _FL);
		return false;
	}
	
	auto v = w->Handle();
	if (!v)
	{
		printf("%s:%i - No handle.\n", _FL);
		return false;
	}

	auto m = [[LCocoaMsg alloc] init:View msg:Msg a:A b:B];
	[v performSelectorOnMainThread:@selector(userEvent:) withObject:m waitUntilDone:false];
	return true;
}

GApp *GApp::ObjInstance()
{
	return TheApp;
}

bool GApp::IsOk()
{
	bool Status =
					#if !defined(__clang__)
					(this != 0) &&
					#endif
					(d != 0)
	/*
	 #ifdef XWIN
	 && (XDisplay() != 0)
	 #endif
	 */
	;
	
	LgiAssert(Status);
	return Status;
}

GMouseHook *GApp::GetMouseHook()
{
	return MouseHook;
}

int GApp::GetMetric(LgiSystemMetric Metric)
{
	switch (Metric)
	{
		default:
			break;
		case LGI_MET_DECOR_X:
		{
			return 0;
		}
		case LGI_MET_DECOR_Y:
		case LGI_MET_DECOR_CAPTION:
		{
			if (AppWnd)
			{
				#if 0
				Rect r;
				OSStatus e = GetWindowBounds(AppWnd->WindowHandle(), kWindowTitleBarRgn, &r);
				if (e) printf("%s:%i - GetWindowBounds failed with %i\n", _FL, (int)e);
				else
				{
					int y = r.bottom - r.top;
					return y;
				}
				#endif
			}
			
			return 22;
		}
	}
	
	return 0;
}

GViewI *GApp::GetFocus()
{
	auto kw = d->NsApp.p.keyWindow;
	if (!kw)
		return NULL;

	LNsWindow *w = objc_dynamic_cast(LNsWindow, kw);
	GWindow *gw = w ? [w getWindow] : nil;
	if (!gw)
		return NULL;

	return gw->GetFocus();
}

OsThread GApp::_GetGuiThread()
{
	return d->GuiThread;
}

OsThreadId GApp::GetGuiThreadId()
{
	return d->GuiThreadId;
}

bool GApp::InThread()
{
	return GetCurrentThreadId() == d->GuiThreadId;
}

OsProcessId GApp::GetProcessId()
{
	return getpid();
}

OsAppArguments *GApp::GetAppArgs()
{
	return IsOk() ? &d->Args : 0;
}

void GApp::SetAppArgs(OsAppArguments &AppArgs)
{
	if (IsOk())
	{
		d->Args = AppArgs;
	}
}

struct IdleGluePtrs
{
	GApp::OnIdleProc Callback;
	void *Param;
};

#define CUSTOM_LOOP		0

#if 0
void IdleGlue(EventLoopTimerRef inTimer, void *inUserData)
{
	IdleGluePtrs *p = (IdleGluePtrs*)inUserData;
	p->Callback(p->Param);
}
#endif

bool GApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (!d->NsApp)
	{
		LgiAssert(!"No d->NsApp");
		return false;
	}

	if (Loop)
	{
		#if CUSTOM_LOOP
		// This impl allows for us to exit gracefully.
		// NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		int Depth = ++d->RunDepth;
		
		do
		{
			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

			NSEvent *event =
				[	d->NsApp
					nextEventMatchingMask:NSAnyEventMask
					untilDate:[NSDate distantFuture]
					inMode:NSDefaultRunLoopMode
					dequeue:YES];

			[d->NsApp sendEvent:event];
			[d->NsApp updateWindows];

			[pool release];
		}
		while (d->RunDepth >= Depth);
		
		// [pool release];
		#else
		
			#if 1
			NSApplicationMain(GetArgs(), GetArg());
			#else
			[d->NsApp run];
			#endif
			
		#endif
		return true;
	}
	else
	{
	}
		
	return 0;
}

void GApp::Exit(int Code)
{
	#if CUSTOM_LOOP
	if (!Code)
	{
		if (d->RunDepth > 0)
			d->RunDepth--;
	}
	#else
	if (!Code)
	{
		if (AppWnd)
			AppWnd->Quit();
		
		[d->NsApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
	}
	else
	#endif
	{
		// hard exit
		::exit(Code);
	}
}

void GApp::OnUrl(const char *Url)
{
	if (AppWnd)
		AppWnd->OnUrl(Url);
	else
		d->UrlArg.Reset(NewStr(Url));
}

void GApp::OnReceiveFiles(GArray<char*> &Files)
{
	if (AppWnd)
		AppWnd->OnReceiveFiles(Files);
}

GXmlTag *GApp::GetConfig(const char *Tag)
{
	if (IsOk() && !d->Config)
	{
		char File[] = "lgi.conf";
		char Path[MAX_PATH];
		LgiMakePath(Path, sizeof(Path), LGetExePath(), File);
		if (FileExists(Path))
		{
			d->Config = new GXmlTag("Config");
			if (d->Config)
			{
				GFile f;
				if (f.Open(Path, O_READ))
				{
					GXmlTree t;
					t.Read(d->Config, &f, 0);
				}
			}
		}
		
		if (!d->Config)
		{
			d->Config = new GXmlTag("Options");
		}
	}
	
	if (Tag && d->Config)
	{
		return d->Config->GetChildTag(Tag);
	}
	
	return 0;
}

void GApp::SetConfig(GXmlTag *Tag)
{
	if (IsOk() && Tag)
	{
		GXmlTag *Old = GetConfig(Tag->GetTag());
		if (Old)
		{
			Old->RemoveTag();
			DeleteObj(Old);
		}
		
		if (!d->Config)
		{
			GetConfig(0);
		}
		if (d->Config)
		{
			d->Config->InsertTag(Tag);
		}
	}
}

const char *GApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? d->Args.Arg[n] : 0;
}

bool GApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	GString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
			strcpy_s(Dest, DestLen, Buf);
		return true;
	}
	return false;
}

bool GApp::GetOption(const char *Option, GString &Buf)
{
	if (IsOk() && Option)
	{
		size_t OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			const char *a = d->Args.Arg[i];
			
			if (strchr("-/\\", a[0]))
			{
				if (strncmp(a+1, Option, OptLen) == 0)
				{
					const char *Arg = 0;
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
								size_t Len = End-Arg;
								if (Len > 0)
								{
									Buf.Set(Arg, Len);
								}
								else return false;
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

void GApp::OnCommandLine()
{
	GArray<char*> Files;
	
	for (int i=1; i<GetAppArgs()->Args; i++)
	{
		const char *a = GetAppArgs()->Arg[i];
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

GString MimeFromData(const char *File)
{
	GString Ret;
	GFile f;
	if (!f.Open(File, O_READ))
		return Ret;
	GArray<uint8_t> b;
	b.Length(1024);
	auto r = f.Read(b.AddressOf(), b.Length());
	if (r <= 0)
		return Ret;
	
	if (b.Length() >= 8)
	{
		if (memcmp(b.AddressOf(), "GIF89a\x01", 7) == 0)
			Ret = "image/gif";
	}
	
	return Ret;
}

GString GApp::GetFileMimeType(const char *File)
{
	GString Ret;
	
	if (!FileExists(File))
	{
		// Look in the path
		GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
		for (int i=0; i<p.Length(); i++)
		{
			char Full[MAX_PATH];
			LgiMakePath(Full, sizeof(Full), p[i], File);
			if (FileExists(Full))
			{
				break;
			}
		}
	}
	
	auto filePath = GString(File).NsStr();
	CFStringRef fileExtension = (__bridge CFStringRef)[filePath pathExtension];
	CFStringRef UTI = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, fileExtension, NULL);
	CFStringRef MIMEType = UTTypeCopyPreferredTagWithClass(UTI, kUTTagClassMIMEType);
	Ret = MIMEType;
	if (MIMEType) CFRelease(MIMEType);
	CFRelease(UTI);
	[filePath release];
	
	if (!Ret)
	{
		Ret = MimeFromData(File);
		if (!Ret)
			Ret = "application/octet-stream";
	}
	
	return Ret;
}

bool GApp::GetAppsForMimeType(char *Mime, GArray<GAppInfo*> &Apps)
{
	// Use LSCopyApplicationForMIMEType?
	
	// Find alternative version of the MIME type (e.g. x-type and type).
	char AltMime[256];
	strcpy(AltMime, Mime);
	char *s = strchr(AltMime, '/');
	if (s)
	{
		s++;
		
		size_t Len = strlen(s) + 1;
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
	
	if (!d->MimeToApp.Length())
	{
		// printf("%s:%i - Building MimeToApp.\n", __FILE__, __LINE__);
		
	}
	
	AppArray *p = (AppArray*)d->MimeToApp.Find(Mime);
	if (p)
	{
		for (int i=0; i<p->Length(); i++)
		{
			Apps[i] = (*p)[i];
		}
		
		return true;
	}
	
	return false;
}

GSymLookup *GApp::GetSymLookup()
{
	return &d->SymLookup;
}

bool GApp::IsElevated()
{
	return geteuid() == 0;
}

int GApp::GetCpuCount()
{
	return 1;
}

GFontCache *GApp::GetFontCache()
{
	if (!d->FontCache)
		d->FontCache.Reset(new GFontCache(SystemNormal));
	return d->FontCache;
}

