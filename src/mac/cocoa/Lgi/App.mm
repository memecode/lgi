#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#import <AppKit/AppKit.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Process.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Array.h"
#include "lgi/common/Token.h"
#include "lgi/common/Thread.h"
#include "lgi/common/EventTargetThread.h"
#include "lgi/common/Menu.h"

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#import "LCocoaView.h"
#include "GAppPriv.h"

extern int hndstate(int hnd);

struct OsAppArgumentsPriv
{
	LAutoString Str;
	LArray<char*> Ptr;
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
	
	LArray<char> Raw;
	LArray<size_t> Offsets;
	
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
	LArray<char> Raw;
	LArray<size_t> Offsets;
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

void LMouse::SetFromEvent(NSEvent *ev, NSView *view)
{
	auto r = view.frame;
	auto pt = ev.locationInWindow;

	x = (int)pt.x;
	y = (int)(r.size.height - pt.y);
	SetModifer((uint32)ev.modifierFlags);
	Event = ev;

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
			LAssert(!"Unknown event.");
			break;
	}

	Double(ev.clickCount == 2 && Down());
}

void LUiEvent::SetModifer(uint32_t modifierKeys)
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

////////////////////////////////////////////////////////////////////////////
@implementation LNsApplication

- (id)init
{
	if ((self = [super init]) != nil)
	{
		self.d = NULL;
	}
	return self;
}

- (void)setPriv:(nonnull LAppPrivate*)priv
{
	self.d = priv;
}

- (void)terminate:(nullable id)sender
{
	[super terminate:sender];
}

- (void)dealloc
{
	[super dealloc];
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

- (void)onUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)reply
{
	LString s = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
	if (self.d && self.d->Owner)
    	self.d->Owner->OnUrl(s);
}

@end

/////////////////////////////////////////////////////////////////////////////
LSkinEngine *LApp::SkinEngine = 0;
LApp *TheApp = 0;
LMouseHook *LApp::MouseHook = 0;

LApp::LApp(OsAppArguments &AppArgs, const char *AppName, LAppArguments *ObjArgs) :
OsApplication(AppArgs.Args, AppArgs.Arg)
{
	TheApp = this;
	d = new LAppPrivate(this);
	d->Name.Reset(NewStr(AppName));
	AppWnd = 0;
	Name(AppName);
	if (LIsRelativePath(AppArgs.Arg[0]))
	{
		char wd[MAX_PATH];
		char exe[MAX_PATH];
		if (LMakePath(exe, sizeof(exe), getcwd(wd, sizeof(wd)), AppArgs.Arg[0]))
			LgiArgsAppPath = exe;
		else
			printf("%s:%i - LMakePath for Exe failed.\n", _FL);
	}
	else
		LgiArgsAppPath = AppArgs.Arg[0];
	
	// printf("%s:%i - LgiArgsAppPath='%s'\n", _FL, LgiArgsAppPath.Get());
	
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
	[d->NsApp setPriv:d];
	
	// Register to get apple events
	NSAppleEventManager *em = [NSAppleEventManager sharedAppleEventManager];
	[em setEventHandler:d->NsApp andSelector:@selector(onUrl:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
	
	// Setup the file and graphics sub-systems
	d->FileSystem = new LFileSystem;
	d->GdcSystem = new GdcDevice;
	
	srand((unsigned)LCurrentTime());
	LColour::OnChange();
	
	SetAppArgs(AppArgs);
	MouseHook = new LMouseHook;
	
	// System font setup
	SystemNormal = 0;
	LFontType SysFontType;
	
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
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error: LApp::LApp", MB_OK);
		LExitApp();
	}
	
	if (!GetOption("noskin"))
	{
		extern LSkinEngine *CreateSkinEngine(LApp *App);
		SkinEngine = CreateSkinEngine(this);
	}
	
	Default.Reset(new LMenu);
}

LApp::~LApp()
{
	DeleteObj(AppWnd);
	DeleteObj(SystemNormal);
	DeleteObj(SystemBold);
	
	DeleteObj(MouseHook);
	DeleteObj(d->FileSystem);
	DeleteObj(d->GdcSystem);
	DeleteObj(LFontSystem::Me);
	DeleteObj(d);
	TheApp = 0;
}

OsApp &LApp::Handle()
{
	return d->NsApp;
}

bool LApp::PostEvent(LViewI *View, int Msg, GMessage::Param A, GMessage::Param B)
{
	if (!View)
	{
		printf("%s:%i - No view.\n", _FL);
		return false;
	}
	
	bool Exists = LView::LockHandler(View, LView::LockOp::OpExists);
	if (!Exists)
	{
		printf("%s:%i - View deleted.\n", _FL);
		return false;
	}

	LWindow *w = View->GetWindow();
	if (!w)
	{
		// printf("%s:%i - No window.\n", _FL);
		return false;
	}
	
	auto v = w->Handle();
	if (!v)
	{
		// printf("%s:%i - No handle.\n", _FL);
		return false;
	}

	auto m = [[LCocoaMsg alloc] init:View msg:Msg a:A b:B];
	[v performSelectorOnMainThread:@selector(userEvent:) withObject:m waitUntilDone:false];
	return true;
}

LApp *LApp::ObjInstance()
{
	return TheApp;
}

bool LApp::IsOk()
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

LViewI *LApp::GetFocus()
{
	auto kw = d->NsApp.p.keyWindow;
	if (!kw)
		return NULL;

	LNsWindow *w = objc_dynamic_cast(LNsWindow, kw);
	LWindow *gw = w ? [w getWindow] : nil;
	if (!gw)
		return NULL;

	return gw->GetFocus();
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
	return GetCurrentThreadId() == d->GuiThreadId;
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

struct IdleGluePtrs
{
	LApp::OnIdleProc Callback;
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

bool LApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (!d->NsApp)
	{
		LAssert(!"No d->NsApp");
		return false;
	}

	if (Loop)
	{
		#if CUSTOM_LOOP
	
			// This impl allows for us to exit gracefully.
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
	
		#else
		
			OnCommandLine();
			NSApplicationMain(GetArgs(), GetArg());
		
		#endif
		return true;
	}
	else
	{
		// NSWindow nextEventMatchingMask may be useful here.
	}
		
	return 0;
}

void LApp::Exit(int Code)
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

void LApp::OnUrl(const char *Url)
{
	if (AppWnd)
		AppWnd->OnUrl(Url);
	else
		d->UrlArg.Reset(NewStr(Url));
}

void LApp::OnReceiveFiles(LArray<const char*> &Files)
{
	if (AppWnd)
		AppWnd->OnReceiveFiles(Files);
}

const char *LApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? d->Args.Arg[n] : 0;
}

bool LApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	LString Buf;
	if (GetOption(Option, Buf))
	{
		if (Dest)
			strcpy_s(Dest, DestLen, Buf);
		return true;
	}
	return false;
}

bool LApp::GetOption(const char *Option, LString &Buf)
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

void LApp::OnCommandLine()
{
	LArray<const char*> Files;
	
	for (int i=1; i<GetAppArgs()->Args; i++)
	{
		const char *a = GetAppArgs()->Arg[i];
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

LString MimeFromData(const char *File)
{
	LString Ret;
	LFile f;
	if (!f.Open(File, O_READ))
		return Ret;
	LArray<uint8_t> b;
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

LString LApp::GetFileMimeType(const char *File)
{
	LString Ret;
	
	if (!LFileExists(File))
	{
		// Look in the path
		GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
		for (int i=0; i<p.Length(); i++)
		{
			char Full[MAX_PATH];
			LMakePath(Full, sizeof(Full), p[i], File);
			if (LFileExists(Full))
			{
				break;
			}
		}
	}
	
	auto filePath = LString(File).NsStr();
	CFStringRef fileExtension = (__bridge CFStringRef)[filePath pathExtension];
	CFStringRef UTI = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, fileExtension, NULL);
	CFStringRef MIMEType = UTTypeCopyPreferredTagWithClass(UTI, kUTTagClassMIMEType);
	Ret = MIMEType;
	if (MIMEType) CFRelease(MIMEType);
	if (UTI) CFRelease(UTI);
	[filePath release];
	
	if (!Ret)
	{
		Ret = MimeFromData(File);
		if (!Ret)
			Ret = "application/octet-stream";
	}
	
	return Ret;
}

bool LApp::GetAppsForMimeType(char *Mime, LArray<LAppInfo*> &Apps)
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

LSymLookup *LApp::GetSymLookup()
{
	return &d->SymLookup;
}

bool LApp::IsElevated()
{
	return geteuid() == 0;
}

int LApp::GetCpuCount()
{
	return 1;
}

GFontCache *LApp::GetFontCache()
{
	if (!d->FontCache)
		d->FontCache.Reset(new GFontCache(SystemNormal));
	return d->FontCache;
}

