#include <Path.h>
#include "Lgi.h"
#include "SkinEngine.h"
#include "LFontCache.h"

class LAppPrivate
{
public:
	// Common
	OsAppArguments Args;
	LXmlTag *Config;
	LFileSystem *FileSystem;
	GLibrary *SkinLib;
	GdcDevice *GdcSystem;
	LAutoPtr<LFontCache> FontCache;
	OsThreadId GuiThread;
	bool FirstRun;

	LAppPrivate() : Args(0, 0)
	{
		FileSystem = 0;
		GdcSystem = 0;
		Config = 0;
		SkinLib = 0;
		GuiThread = LGetCurrentThread();
		FirstRun = true;
	}

	~LAppPrivate()
	{
		DeleteObj(Config);
		DeleteObj(FileSystem);
		DeleteObj(GdcSystem);
		DeleteObj(SkinLib)
	}
};

/////////////////////////////////////////////////////////////////////////////
LApp *TheApp = 0;
class LSkinEngine *LApp::SkinEngine = 0;

LApp *LApp::ObjInstance()
{
	return TheApp;
}

char *MimeFromName(char *Name)
{
	static char m[256];
	sprintf_s(m, sizeof(m), "application/x-%s", Name);
	return m;
}

LApp::LApp(OsAppArguments &OsArgs, const char *Name, LAppArguments *AppArgs) : BApplication(MimeFromName(Name))
{
	// Sanity Checks
	LAssert(sizeof(int8) == 1);
	LAssert(sizeof(uint8) == 1);

	LAssert(sizeof(int16) == 2);
	LAssert(sizeof(uint16) == 2);

	LAssert(sizeof(int32) == 4);
	LAssert(sizeof(uint32) == 4);

	LAssert(sizeof(int64) == 8);
	LAssert(sizeof(uint64) == 8);
	
	LAssert(sizeof(char16) == 4);

	// Private data
	d = new LAppPrivate;
	TheApp = this;
	d->Args = OsArgs;
	LBase::Name(Name);

	// Setup LGI Sub-systems
	LFontSystem::Inst();
	d->FileSystem = new LFileSystem;
	d->GdcSystem = new GdcDevice;

	srand(system_time());
	LgiInitColours();
	// MouseHook = new LMouseHook;
	MouseHook = 0;
	AppWnd = 0;
	SetAppArgs(OsArgs);

	// System font
	SystemNormal = 0;
	LFontType SysFontType;
	if (SysFontType.GetSystemFont("System"))
	{
		SystemNormal = SysFontType.Create();
		if (SystemNormal)
		{
			SystemNormal->Transparent(true);
			
			SystemBold = new LFont;
			if (SystemBold)
			{
				*SystemBold = *SystemNormal;
				SystemBold->Bold(true);
				SystemBold->Create();
				LAssert(SystemBold->Handle());	
			}
		}
	}

	if (!SystemNormal)
	{
		LgiMsg(0, "Error: Couldn't create system font.", "Lgi Error: LApp::LApp", MB_OK);
		LExitApp();
	}

	#if 1
	if (!GetOption("noskin"))
	{
		extern LSkinEngine *CreateSkinEngine(LApp *App);
		SkinEngine = CreateSkinEngine(this);
	}
	#endif
}

LApp::~LApp()
{
	TheApp = 0;
	DeleteObj(d);
}

LSymLookup *LApp::GetSymLookup()
{
	return NULL;
}

void LApp::OnUrl(const char *Url)
{
}

OsThreadId LApp::GetGuiThread()
{
	return d->GuiThread;
}

OsProcessId LApp::GetProcessId()
{
	return getpid();
}

bool LApp::IsOk()
{
	return true;
}

LViewI *LApp::GetFocus()
{
	for (int i=0; i<CountWindows(); i++)
	{
		BWindow *w = WindowAt(i);
		if (w && w->IsActive())
		{
			BView *f = w->CurrentFocus();
			if (f)
			{
				BViewRedir *r = dynamic_cast<BViewRedir*>(f);
				if (r)
				{
					return r->WindowHandle();
				}
			}
		}
	}
	
	return 0;
}

int32 LApp::GetMetric(LSystemMetric Which)
{
	return 0;
}

bool LApp::Run(bool Loop, OnIdleProc IdleCallback, void *IdleParam)
{
	if (Loop)
	{
		if (d->FirstRun)
		{
			d->FirstRun = false;
			OnCommandLine();
		}
		BApplication::Run();
	}
	else
	{
		// ???
	}
}

void LApp::RefsReceived(BMessage *Msg)
{
	LArray<char*> Files;
	int32 Count = 0;
	type_code Type = 0;
	
	if (Msg->GetInfo("refs", &Type, &Count) == B_OK)
	{
		for (int i=0; i<Count; i++)
		{
			entry_ref Ref;
			if (Msg->FindRef("refs", i, &Ref) == B_OK)
			{
				BPath Path;
				BEntry Entry(&Ref, true);
				Entry.GetPath(&Path);
				char *p = (char*) Path.Path();
				if (p)
				{
					Files.Add(NewStr(p));
				}
			}
		}
	}
	
	if (AppWnd && Files.Length() > 0)
	{
		AppWnd->OnReceiveFiles(Files);
	}
	
	Files.DeleteArrays();
}

void LApp::OnReceiveFiles(LArray<char*> &Files)
{
	if (AppWnd)
	{
		AppWnd->OnReceiveFiles(Files);
	}
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
			GetConfig(0);
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

			/*
			if (!LFileExists(Path))
			{
				char *f = LgiFindFile(File);
				if (f)
				{
					strcpy(Path, f);
					DeleteArray(f);
				}
			}
			*/

			if (LFileExists(Path))
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

		if (!d->Config)
		{
			d->Config = new LXmlTag("Options");
		}
	}

	if (Tag && d->Config)
	{
		return d->Config->GetChildTag(Tag);
	}

	return 0;
}

bool LApp::GetOption(const char *Option, char *Dest, int DestLen)
{
	if (Option)
	{
		int OptLen = strlen(Option);
		
		for (int i=1; i<d->Args.Args; i++)
		{
			char *c = d->Args.Arg[i];
			if (*c == '/' || *c == '\\' || *c == '-')
			{
				c++;
				if (strncmp(c, Option, OptLen) == 0)
				{
					c += OptLen;
					
					if (!*c && i < d->Args.Args - 1)
					{
						c = d->Args.Arg[++i];
					}
					
					if (c)
					{
						while (*c && strchr(" \t\r\n", *c))
						{
							c++;
						}
		
						if (Dest && DestLen > 0)
						{
							for (char *i = c; *i && *i != ' ' && DestLen > 1;)
							{
								*Dest++ = *i++;
								DestLen--;
							}
							*Dest++ = 0;
						}
						
						return true;
					}
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
	for (int i = 1; i<d->Args.Args; i++)
	{
		const char *a = d->Args.Arg[i];
		if (a)
		{
			if (LIsRelativePath(a))
			{
				char p[MAX_PATH];
				LgiGetExePath(p, sizeof(p));
				LMakePath(p, sizeof(p), p, a);
				if (FileExists(a))
				{
					Files.Add(NewStr(a));
				}
			}
			else if (FileExists(a))
			{
				Files.Add(NewStr(a));
			}
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

char *LApp::GetArgumentAt(int n)
{
	return n >= 0 && n < d->Args.Args ? NewStr(d->Args.Arg[n]) : 0;
}

OsAppArguments *LApp::GetAppArgs()
{
	return &d->Args;
}

bool LApp::GetOption(const char *Option, LAutoString &Buf)
{
	if (IsOk() && Option)
	{
		int OptLen = strlen(Option);
		for (int i=1; i<d->Args.Args; i++)
		{
			char *a = d->Args.Arg[i];

			if (strchr("-/\\", a[0]))
			{
				if (strncmp(a+1, Option, OptLen) == 0)
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
								{
									Buf.Reset(NewStr(Arg, End - Arg));
								}
								else return false;
							}
							else return false;
						}
						else
						{
							Buf.Reset(NewStr(Arg));
						}
					}

					return true;
				}
			}
		}
	}

	return false;
}

void LApp::SetAppArgs(OsAppArguments &AppArgs)
{
	d->Args = AppArgs;
}

void LApp::Exit(int Code)
{
	if (Code)
	{
		// hard exit
		exit(-1);
	}
	else
	{
		// normal exit
		BMessenger m(0, be_app);
		m.SendMessage(B_QUIT_REQUESTED);
	}
}

LAutoString LApp::GetFileMimeType(const char *File)
{
	LAssert(0);
	return LAutoString();
}

LMouseHook *LApp::MouseHook = 0;
LMouseHook *LApp::GetMouseHook()
{
	return MouseHook;
}

int LApp::GetCpuCount()
{
	return 4;	
}

LFontCache *LApp::GetFontCache()
{
	if (!d->FontCache)
		d->FontCache.Reset(new LFontCache(SystemNormal));
	return d->FontCache;
}

bool LApp::IsElevated()
{
	return true;
}

////////////////////////////////////////////////////////////////////////////
struct OsAppArgumentsPriv
{
	LArray<char*> Ptr;
	
	~OsAppArgumentsPriv()
	{
		Ptr.DeleteArrays();
	}
};

OsAppArguments::OsAppArguments(int args, char **arg)
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

