// Linux Implementation of General LGI functions
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#ifndef _POSIX_TIMERS
#define _POSIX_TIMERS
#endif
#include <time.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"
#include "lgi/common/Uri.h"
#ifndef LGI_SDL
#include "LgiWinManGlue.h"
#endif
#include "lgi/common/Token.h"

#include <pwd.h>

#if 1
#define MT_APPS_LOG(...)		LgiTrace(__VA_ARGS__)
#else
#define MT_APPS_LOG(...)
#endif

////////////////////////////////////////////////////////////////
// Local helper functions
bool _lgi_check_file(char *Path)
{
	if (Path)
	{
		if (LFileExists(Path))
		{
			// file is there
			return true;
		}
		else
		{
			// shortcut?
			char *e = Path + strlen(Path);
			strcpy(e, ".lnk");
			if (LFileExists(Path))
			{
				// resolve shortcut
				char Link[256];
				if (LResolveShortcut(Path, Link, sizeof(Link)))
				{
					// check destination of link
					if (LFileExists(Link))
					{
						strcpy(Path, Link);
						return true;
					}
				}
			}
			*e = 0;
		}
	}

	return false;
}

LString LCurrentUserName()
{
	struct passwd *pw = getpwuid(geteuid());
	if (pw)
		return pw->pw_name;

  	return "";
}


void LSleep(uint32_t i)
{
	struct timespec request, remain;

	ZeroObj(request);
	ZeroObj(remain);
	
	request.tv_sec = i / 1000;
	request.tv_nsec = (i % 1000) * 1000000;

	//printf("%i LSleep(%i)\n", LCurrentThreadHnd(), i);
	while (nanosleep(&request, &remain) == -1)
	{
		request = remain;
		//printf("\t%i Resleeping=%i\n", LCurrentThreadHnd(), request.tv_sec*1000 + request.tv_nsec/1000);
	}
}

enum AssertBtn {
	AB_NONE,
	AB_BREAK,
	AB_QUIT,
	AB_IGNORE,
	AB_WAITING_RESPONSE,
};

AssertBtn GtkAssertDlg(const char *File, int Line, const char *Msg)
{
	Gtk::GtkWidget *dialog = 
		Gtk::gtk_message_dialog_new 
		(
			NULL,
			Gtk::GTK_DIALOG_DESTROY_WITH_PARENT,
			Gtk::GTK_MESSAGE_ERROR,
			Gtk::GTK_BUTTONS_NONE,
			"%s:%i - Assert failed:\n%s",
			File, Line,
			Msg
		);
	Gtk::GtkDialog *dlg = GtkCast(dialog, gtk_dialog, GtkDialog);		
	Gtk::gtk_dialog_add_buttons(dlg,
		"Break", AB_BREAK,
		"Quit", AB_QUIT,
		"Ignore", AB_IGNORE
		, NULL);
	int Result = Gtk::gtk_dialog_run(dlg);
	Gtk::gtk_widget_destroy(dialog);
	return (AssertBtn)Result;
}

void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	static bool Asserting = false;

	if (!b && !Asserting)
	{
		Asserting = true;

		printf("%s:%i - Assert failed:\n%s\n", file, line, test);

		#ifdef LGI_SDL
		exit(-1);
		#else
		AssertBtn Result = AB_NONE;
		
		#if 1
		if (LAppInst->InThread())
		{
			Result = GtkAssertDlg(file, line, test);
		}
		else
		{
			// This may or may not work, depending on whether the GUI thread is
			// actually running. If it's deadlocked, this will fail.
			LAppInst->RunCallback([pResult = &Result, file, line, test]()
				{
					// Tell the calling thread we've got the callback and we're
					// waiting on the user.
					*pResult = AB_WAITING_RESPONSE;

					// Actually ASK the user for input:
					*pResult = GtkAssertDlg(file, line, test);
				},
				_FL);

			auto startTs = LCurrentTime();
			while (Result == AB_NONE)
			{
				if (LCurrentTime() - startTs >= 5000 &&
					Result == AB_NONE)
				{
					// GUI thread is deadlocked!
					// Assume the user wants 'Ignore'
					Result = AB_IGNORE;
					break;
				}
				LSleep(10);
			}
		}
		#endif

		switch (Result)
		{
			case AB_BREAK:
			{
				// Try and bring the debugger up:
				int *i = nullptr;
				*i = 0;
				break;
			}
			case AB_QUIT:
			{
				// Hard exit:
				exit(-1);
				break;
			}
			default:
			case AB_IGNORE:
			{
				printf("%s:%i - assert '%s' ignored.\n", _FL, test);
				break;
			}
		}
		#endif

		Asserting = false;
	}
}

////////////////////////////////////////////////////////////////////////
// Implementations
LMessage CreateMsg(int m, int a, int b)
{
	static LMessage Msg(0);
	Msg.Set(m, a, b);
	return Msg;
}

bool LGetMimeTypeExtensions(const char *Mime, LArray<LString> &Ext)
{
	int Start = Ext.Length();
	char *e;

	#define HardCodeExtention(Mime, Ext1, Ext2) \
		else if (!stricmp(Mime, Mime)) \
		{	if (Ext1) Ext.Add(Ext1); \
			if (Ext2) Ext.Add(Ext2); }

	const char *Null = NULL;
	if (!Mime);
	HardCodeExtention("text/calendar", "ics", Null)
	HardCodeExtention("text/x-vcard", "vcf", Null)
	HardCodeExtention("text/mbox", "mbx", "mbox");

	return Ext.Length() > Start;
}

LString LGetFileMimeType(const char *File)
{
	return LAppInst->GetFileMimeType(File);
}

bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize)
{
	bool Status = false;

	#ifndef LGI_SDL
	LLibrary *WmLib = LAppInst->GetWindowManagerLib();
	if (WmLib)
	{
		Proc_LgiWmGetSysFont GetSysFont = (Proc_LgiWmGetSysFont) WmLib->GetAddress("LgiWmGetSysFont");
		if (GetSysFont)
		{
			Status = GetSysFont(FontType, Font, FontBufSize, PointSize);
			if (!Status)
			{
				printf("%s:%i - GetSysFont failed\n", _FL);
			}
		}
		else
		{
			printf("%s:%i - Entry point doesn't exist\n", _FL);
		}
	}
	else
	#endif
	{
		static bool Warn = true;
		if (Warn)
		{
			printf("%s:%i - GetWindowManagerLib failed\n", _FL);
			Warn = false;
		}
	}
	
	return Status;
}

static const char *desktopFolders[] =
{
	"/usr/share/applications",
	"/var/lib/snapd/desktop/applications",
	nullptr
};

static bool XdgMimeLookup(const char *Mime, LArray<LAppInfo> &Apps, int Limit)
{
	static bool NoXdgMime = false;
	if (NoXdgMime)
	{
		MT_APPS_LOG("%s:%i %s - NoXdgMime.\n", _FL, __func__);
		return false;
	}

	char Args[MAX_PATH_LEN];
	sprintf(Args, "query default %s", Mime);
	LStringPipe Output;

	auto CurLang = LGetLanguageId();	
	char LangName[64];
	sprintf_s(LangName, sizeof(LangName), "Name[%s]", CurLang ? CurLang->Id : "en");

	MT_APPS_LOG("LGetAppsForMimeType('%s', ..., %i)\nRunning 'xdg-mime %s'\n", Mime, Limit, Args);

	LSubProcess proc("xdg-mime", Args);
	if (!proc.Start())
	{
		NoXdgMime = true;
		MT_APPS_LOG("%s:%i %s - Failed to execute xdg-mime %s\n", _FL, __func__, Args);
		return false;
	}

	proc.Communicate(&Output);
	auto o = Output.NewLStr().Strip();

	if (!o)
	{
		MT_APPS_LOG("%s:%i - No output from xdg-mime\n", _FL);
		return false;
	}
	MT_APPS_LOG("Output:\n%s\n", o.Get());

	char p[MAX_PATH_LEN];
	bool exists = false;	
	for (int i=0; desktopFolders[i]; i++)
	{
		if (!LMakePath(p, sizeof(p), desktopFolders[i], o.Strip()))
		{
			MT_APPS_LOG("%s:%i - Failed to create path.\n", _FL);
			return false;
		}
		
		exists = LFileExists(p);
		MT_APPS_LOG("DesktopExists: '%s' = %i\n", p, exists);
		if (exists)
			break;
	}
	if (!exists)
	{
		MT_APPS_LOG("%s:%i - '%s' doesn't exist.\n", _FL, p);
		return false;
	}

	auto txt = LReadFile(p);
	LString Section;

	MT_APPS_LOG("Reading '%s', got %i bytes\n", p, (int)strlen(txt));

	if (!txt)
	{
		MT_APPS_LOG("%s:%i - Can't read from '%s'\n", _FL, p);
		return false;
	}

	auto &info = Apps.New();
	bool Status = false;
	
	for (auto line: txt.SplitDelimit("\r\n"))
	{
		// MT_APPS_LOG("	'%s'\n", Var);
		
		if (line(0) == '[')
		{
			auto end = line.Find("]");
			if (end > 0)
			{
				Section = line(1, end);
				MT_APPS_LOG("	Section='%s'\n", Section.Get());
			}
		}
		else if (Section.Equals("Desktop Entry"))
		{
			auto var = line.SplitDelimit("=", 1);
			if (var.Length() == 2)
			{
				if (var[0].Equals("Exec"))
				{
					info.Path = var[1];
					Status = true;
				}
				else if (var[0].Equals("Name") ||
						 var[0].Equals(LangName))
				{
					info.Name = var[1];
				}
			}
		}
	}

	MT_APPS_LOG("	info='%s' '%s'\n", info.Name.Get(), info.Path.Get());
	
	return Status;
}

static bool DesktopToAppInfo(const char *desktop, LAppInfo &info)
{
	LString fullPath;
	for (int i=0; desktopFolders[i]; i++)
	{
		LFile::Path path(desktopFolders[i]);
		path += desktop;
		if (path.Exists())
		{
			fullPath = path.GetFull();
			break;
		}
	}
	if (!fullPath)
		return false;
		
	LFile f(fullPath);
	if (!f)
		return false;
	
	auto lines = f.Read().SplitDelimit("\n");
	for (auto line: lines)
	{
		auto p = line.SplitDelimit("=", 1);
		if (p.Length() == 2)
		{
			if (p[0].Equals("Name"))
			{
				info.Name = p[1];
			}
			else if (p[0].Equals("Exec"))
			{
				auto e = p[1].SplitDelimit(" ", 1);
				info.Path = e[0];
				if (e.Length() > 1)
					info.Params = e[1];
			}
				
			// info.Path
			// info.Params
			// info.Icon
		}
	}
	
	return info.Name != NULL && info.Path != NULL;
}

static bool MimeAppsLookup(const char *Mime, LArray<LAppInfo> &Apps, int Limit)
{
	LString::Array files, desktops;
	
	LFile::Path p(LSP_HOME);
	p += ".config/mimeapps.list";
	if (p.Exists())
		files.Add(p.GetFull());
	
	auto GnomeDefs = "/etc/gnome/defaults.list";
	if (LFileExists(GnomeDefs))
		files.Add(GnomeDefs);
	
	for (auto file: files)
	{
		LFile f(file);
		auto lines = f.Read().Split("\n");
		for (auto ln: lines)
		{
			if (ln(0) == '[')
				continue;
				
			auto p = ln.SplitDelimit("=", 1);
			if (p.Length() == 2)
			{
				if (p[0].Equals(Mime))
					desktops.Add(p[1]);
			}
		}
	}
	
	for (auto desktop: desktops)
	{
		if (!DesktopToAppInfo(desktop, Apps.New()))
			Apps.PopLast();
		if (Limit >= 0 && Apps.Length() >= Limit)
			break;
	}
	
	return Apps.Length() > 0;
}

bool LGetAppsForMimeType(const char *Mime, LArray<LAppInfo> &Apps, int Limit)
{
	bool Status = XdgMimeLookup(Mime, Apps, Limit);
	MT_APPS_LOG("%s: XdgMimeLookup=%i apps=%i\n", __func__, Status, (int)Apps.Length());
	if (!Status)
	{
		Status = MimeAppsLookup(Mime, Apps, Limit);
		MT_APPS_LOG("%s: MimeAppsLookup=%i apps=%i\n", __func__, Status, (int)Apps.Length());
	}
	
	return Status;
}

LString LGetAppForMimeType(const char *Mime)
{
	LString App;
	LArray<LAppInfo> Apps;
	if (LGetAppsForMimeType(Mime, Apps, 1))
		App = Apps[0].Path.Get();
	return App;
}

int LRand(int Limit)
{
	return rand() % Limit;
}

LString LErrorCodeToString(uint32_t ErrorCode)
{
	LString e = strerror(ErrorCode);
	if (!e)
		e.Printf("UnknownError(%i)", ErrorCode);
	return e;
}

bool LExecute(const char *File, const char *Args, const char *Dir, LError *error)
{
	if (!File)
	{
		LgiTrace("%s:%i - no file.\n", _FL);
		if (error)
			error->Set(LErrorInvalidParam, "Missing file parameter");
		return false;
	}
	
	bool IsUrl = false;

	LString App;
	auto uriSep = Stristr(File, "://");
	if (uriSep != nullptr && uriSep - File < 32)
	{
		IsUrl = true;
		LUri u(File);
		if (u.IsProtocol("http") ||
			u.IsProtocol("https"))
			App = LGetAppForMimeType("text/html");
	}
	else
	{
		struct stat f;
		char Path[MAX_PATH_LEN];
		
		// see if the file is executable
		bool InPath = false;
		bool Ok = stat(File, &f) == 0;
		if (Ok)
		{
			strcpy_s(Path, sizeof(Path), File);
		}
		else
		{
			// look in the path
			InPath = true;
			LToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
			for (int i=0; i<p.Length() && !Ok; i++)
			{
				LMakePath(Path, sizeof(Path), p[i], File);
				Ok = stat(Path, &f) == 0;
			}
		}
		
		if (Ok)			
		{
			if (S_ISDIR(f.st_mode))
			{
				// open the directory in the current browser
				App = LGetAppForMimeType("inode/directory");
			}
			else
			{
				// look up the type...
				auto Mime = LGetFileMimeType(File);
				if (Mime)
				{
					// printf("LGetFileMimeType(%s)=%s\n", File, Mime);
					
					if (stricmp(Mime, "application/x-executable") == 0 ||
						stricmp(Mime, "application/x-shellscript") == 0 ||
						stricmp(Mime, "application/x-pie-executable") == 0 ||
						stricmp(Mime, "text/x-python") == 0)
					{
						TreatAsExe:
						char f[512];
						sprintf_s(f, sizeof(f), "\"%s\" %s &", File, Args ? Args : (char*)"");
						if (Dir)
							chdir(Dir);						
						
						auto result = system(f);
						printf("%s:%i - system(%s) = %i\n", _FL, f, result);
						if (result)
						{
							if (error)
								error->Set(LErrorFuncFailed, LString::Fmt("system(%s) failed", f));
							return false;
						}
						
						return true;
					}
					else
					{
						// got the mime type
						if (!(App = LGetAppForMimeType(Mime)))
						{
							// printf("%s:%i: LExecute - LgiGetAppForMimeType failed to return the app for '%s'\n", _FL, File);
							goto TreatAsExe;
						}
					}
				}
				else
				{
					// printf("LExecute: LGetFileMimeType failed to return a mime type for '%s'\n", File);
					
					// If a file can't be typed it's most likely an executable.
					// goto TreatAsExe;
				}
			}
		}
		else if (error)
			error->Set(LErrorPathNotFound, LString::Fmt("'%s' doesn't exist.\n", File));
	}

	if (!App)
	{
		if (error)
			error->Set(LErrorPathNotFound, "No app registered to open file type.");
		return false;
	}

	bool FileAdded = false;
	LString AppPath;
	LString EscFile = LString::Escape(File, -1, " &");

	LString a = App;
	int Pos;
	while ((Pos = a.Find("%")) >= 0)
	{
		char *s = a.Get() + Pos;
		printf("a='%s'\n", a.Get());
		switch (s[1])
		{			
			case 'f':
			case 'F':
			{
				a = a(0,Pos) + EscFile + a(Pos+2,-1);
				FileAdded = true;
				break;
			}
			case 'u':
			case 'U':
			{
				if (IsUrl)
					a = a(0,Pos) + EscFile + a(Pos+2,-1);
				else
					a = a(0,Pos) + LString("file:") + EscFile + a(Pos+2,-1);
				FileAdded = true;
				break;
			}
			default:
			{
				// we don't understand this command
				a = a(0,Pos) + a(Pos+2,-1);
				break;
			}
		}
		printf("a='%s'\n", a.Get());
	}

	if (!FileAdded)
	{
		a += " ";
		a += EscFile;
	}

	a += " > /dev/null 2> /dev/null &";

	int e;
	if (Dir)
		chdir(Dir);
	printf("a=\n%s\n", a.Get());
	if (e = system(a))
	{
		if (error)
			error->Set(errno);
		return false;
	}

	return true;
} 

//////////////////////////////////////////////////////////////////////////
WindowManager LGetWindowManager()
{
	static WindowManager Status = WM_Unknown;

	if (Status == WM_Unknown)
	{
		LDirectory d;

		for (bool b=d.First("/proc"); b && Status == WM_Unknown; b=d.Next())
		{
			if (d.IsDir() && isdigit(d.GetName()[0]))
			{
				char Path[256];
				d.Path(Path, sizeof(Path));
				LMakePath(Path, sizeof(Path), Path, "status");
				
				LFile s;
				if (s.Open(Path, O_READ))
				{
					char Buf[256];
					Buf[sizeof(Buf)-1] = 0;
					s.Read(Buf, sizeof(Buf)-1);
					
					char *n = strchr(Buf, '\n');
					if (n)
					{
						*n = 0;
						
						if (stristr(Buf, "gnome-settings") != 0 ||
							stristr(Buf, "gnome-session") != 0 ||
							stristr(Buf, "gnome-panel") != 0)
						{
							Status = WM_Gnome;
						}
						else if (stristr(Buf, "startkde") != 0 ||
								 stristr(Buf, "kdesktop") != 0)
						{
							Status = WM_Kde;
						}
					}
				}
				else printf("%s:%i - error\n", __FILE__, __LINE__);
			}
		}	
	}
	
	return Status;
}

void LFinishXWindowsStartup(LViewI *Wnd)
{
	// Get the startup ID
	const char *EnvStartId = "DESKTOP_STARTUP_ID";
	char *DesktopStartupId = getenv(EnvStartId);
	if (ValidStr(DesktopStartupId))
	{
		LStringPipe oss;

		// Create remove string
		oss.Push("remove: ID=");

		// Quote the id string
		for (char *c = DesktopStartupId; *c; c++)
		{
			if (*c == ' ' || *c == '"' || *c == '\\')
			{
				oss.Write((char*)"\\", 1);
			}
			oss.Write(c, 1);
		}

		char *Str = oss.NewStr();

		// Get the window and display
		/*
		XWidget *View = Wnd->Handle();
		if (!View)
			return;
			
		Display *display = View->XDisplay();
		Window xroot_window = DefaultRootWindow(display);

		XSetWindowAttributes attrs;
		attrs.override_redirect = True;
		attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

		// Get the atoms
		Atom type_atom = XInternAtom(display, "_NET_STARTUP_INFO", false);
		Atom type_atom_begin = XInternAtom(display, "_NET_STARTUP_INFO_BEGIN", false);

		// Create the event we will send
		XEvent xevent;
		xevent.xclient.type = ClientMessage;
		xevent.xclient.message_type = type_atom_begin;
		xevent.xclient.display = display;
		xevent.xclient.window = View->handle();
		xevent.xclient.format = 8;

		const char* src = Str;
		const char* src_end = src + strlen(Str) + 1; // Include trailing NUL.

		// Loop over the string and send it.
		while (src != src_end)
		{
			char* dest = &xevent.xclient.data.b[0];
			char* dest_end = dest + 20;		   
			while (dest != dest_end && src != src_end)
			{
				*dest++ = *src++;
			}
			while (dest != dest_end)
			{
				*dest++ = 0;
			}
			printf("%s:%i - XSendEvent\n", __FILE__, __LINE__);
			XSendEvent(display, xroot_window, False, PropertyChangeMask, &xevent);
			xevent.xclient.message_type = type_atom;
		}

		// Clear the event ID so it's not inherited by child processes.
		unsetenv(EnvStartId);
		*/
	}
}

#if HAS_GSTREAMER
	// sudo apt-get install libgstreamer1.0-dev
	using namespace Gtk;
	#include <gst/gst.h>
	static bool GstHasInit = false;
#endif

bool LPlaySound(const char *FileName, int ASync)
{
	#if HAS_GSTREAMER

		setenv("GST_DEBUG", "4", true);

		LFile::Path path(FileName);		

		if (!GstHasInit)
		{
			/* Initialize GStreamer */
			auto args = LAppInst ? LAppInst->GetAppArgs() : NULL;
			if (args)
			{
				GstHasInit = true;
				gst_init(&args->Args, (char***)&args->Arg);
			}
			else
			{
				LAssert(!"No app?");
				return false;
			}
		}

		/* Create the elements */
		auto source		   = gst_element_factory_make("filesrc",       NULL);
		auto decode		   = gst_element_factory_make("uridecodebin",  NULL);
		auto audioconvert  = gst_element_factory_make("audioconvert",  NULL);
		auto sink		   = gst_element_factory_make("autoaudiosink", NULL);
		
		// Set parameters for some elements
		g_object_set(G_OBJECT(source),	"location", path.Absolute().GetFull().Get(), NULL);
		
		/* Create the empty pipeline */
		auto pipeline = gst_pipeline_new("pipeline");

		/* Build the pipeline */
		gst_bin_add_many(GST_BIN(pipeline), source, decode, audioconvert, sink, NULL);

		#if 1
			auto r = gst_element_link(source, decode);
			printf("src->decode=%i\n", r);
			if (!r)
				return false;

			r = gst_element_link(decode, audioconvert);
			printf("decode->audioconvert=%i\n", r);
			if (!r)
				return false;

			r = gst_element_link(audioconvert, sink);
			printf("audioconvert->sink=%i\n", r);
			if (!r)
				return false;
		#else
			if (!gst_element_link_many(source, decode, audioconvert, sink, NULL))
			{
				LgiTrace("%s:%i - Failed to link elements!\n", _FL);
				gst_object_unref (pipeline);
				return false;
			}
		#endif

		/* Start playing */
		gst_element_set_state(pipeline, GST_STATE_PLAYING);

		auto bus = gst_element_get_bus(pipeline);
		gst_object_unref(bus);

		// Wait for the playback to complete:
		GstState state = GST_STATE_PLAYING;
		do
		{
			auto res = gst_element_get_state(pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
			printf("gst_element_get_state=%i, state=%i GST_STATE_PLAYING=%i\n", res, state, GST_STATE_PLAYING);
			LSleep(10);
		}
		while (state == GST_STATE_PLAYING);

		/* Free pipeline */
		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(pipeline));

		return true;

	#else

		return LExecute(FileName);

	#endif
}
