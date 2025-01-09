#if defined(WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Token.h"
#include "lgi/common/Combo.h"
#include "lgi/common/Net.h"
#include "lgi/common/ListItemCheckBox.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/DropFiles.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/Css.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"
#include "lgi/common/RegKey.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/Menu.h"
#include "lgi/common/PopupNotification.h"
#include "lgi/common/RemoveAnsi.h"
#include "lgi/common/Uri.h"

#include "LgiIde.h"
#include "resdefs.h"
#include "ProjectNode.h"
#include "WebFldDlg.h"

extern const char *Untitled;
const char SourcePatterns[] = "*.c;*.h;*.cpp;*.cc;*.java;*.d;*.php;*.html;*.css;*.js";
const char *AddFilesProgress::DefaultExt = "c,cpp,cc,cxx,h,hpp,hxx,html,css,json,js,jsx,txt,png,jpg,jpeg,rc,xml,mk,paths,makefile,py,java,php";
const char *VsBinaries[] = {"devenv.com", "WDExpress.exe"};

#define USE_OPEN_PROGRESS			1

#define STOP_BUILD_TIMEOUT			2000
#ifdef WINDOWS
#define LGI_STATIC_LIBRARY_EXT		"lib"
#else
#define LGI_STATIC_LIBRARY_EXT		"a"
#endif

#if 1
#define LOG(...)					log.Print(__VA_ARGS__)
#else
#define LOG(...)
#endif

int PlatformCtrlId[] =
{
	IDC_WIN32,
	IDC_LINUX,
	IDC_MAC,
	IDC_HAIKU,
	0
};

const char *PlatformDynamicLibraryExt(SysPlatform Platform)
{
	if (Platform == PlatformWin)
		return "dll";
	if (Platform == PlatformMac)
		return "dylib";
	
	return "so";
}

const char *PlatformSharedLibraryExt(SysPlatform Platform)
{
	if (Platform == PlatformWin)
		return "lib";

	return "a";
}

const char *PlatformExecutableExt(SysPlatform Platform)
{
	if (Platform == PlatformWin)
		return ".exe";
	return "";
}

char *ToUnixPath(char *s)
{
	if (s)
	{
		char *c;
		while ((c = strchr(s, '\\')))
			*c = '/';
	}
	return s;
}

const char *CastEmpty(char *s)
{
	return s ? s : "";
}

bool FindInPath(LString &Exe)
{
	LString::Array Path = LString(getenv("PATH")).Split(LGI_PATH_SEPARATOR);
	for (unsigned i=0; i<Path.Length(); i++)
	{
		char p[MAX_PATH_LEN];
		LMakePath(p, sizeof(p), Path[i], Exe);
		if (LFileExists(p))
		{
			Exe = p;
			return true;
		}
	}
	
	return false;
}

LAutoString ToNativeStr(const char *s)
{
	LAutoString a(NewStr(s));
	if (a)
	{
		if (strnicmp(a, "ftp://", 6))
		{
			for (char *c = a; *c; c++)
			{
				#ifdef WIN32
				if (*c == '/')
					*c = '\\';
				#else
				if (*c == '\\')
					*c = '/';
				#endif
			}
		}
	}
	return a;
}

int StrCmp(char *a, char *b, NativeInt d)
{
	return stricmp(a, b);
}

int StrSort(char **a, char **b)
{
	return stricmp(*a, *b);
}

//////////////////////////////////////////////////////////////////////////////////
class ProjectNode;

class BuildThread : public LThread, public LStream, public LCancel
{
	IdeProject *Proj = NULL;
	LString Makefile, CygwinPath;
	bool Clean, All;
	BuildConfig Config;
	SysPlatform Platform;
	int WordSize;
	LAutoPtr<LSubProcess> SubProc;
	LString::Array BuildConfigs;
	LString::Array PostBuild;
	int AppHnd;

	enum CompilerType
	{
		DefaultCompiler,
		VisualStudio,
		MingW,
		Gcc,
		CrossCompiler,
		PythonScript,
		IAR,
		Nmake,
		Cygwin,
		Xcode,
	}
		Compiler;

	enum ArchType
	{
		DefaultArch,
		ArchX32,
		ArchX64,
		ArchArm6,
		ArchArm7,
	}
		Arch;

	// Convert a stream to log messages, minus the ANSI stuff...
	struct StreamToLog :
		public LStream,
		public SystemIntf::ProcessIo
	{
		LViewI *target;
	
	public:
		StreamToLog(LViewI *t)
		{
			target = t;
			output = this;
		}

		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
		{
			// Remove ansi
			LAutoString s(NewStr((char*)Ptr, Size));
			auto newSize = RemoveAnsi(s.Get(), Size);
			if (newSize >= 0 && newSize < Size)
				s.Get()[newSize] = 0; // null terminate the string...

			// Send the string to the log...
			target->PostEvent(M_APPEND_TEXT, (LMessage::Param)s.Release(), AppWnd::BuildTab);
			return Size;
		}
	};

	// Remote backend state
	struct CallRef
	{
		const char *file = NULL;
		int line = 0;
		operator bool() const { return line != 0; }
		CallRef(int i = 0) { file = NULL; line = 0; }
		CallRef(const char *f, int ln) { file = f; line = ln; }
	};
	LMutex backendLock;
	LHashTbl<IntKey<int>,CallRef> backendCalls;
	LArray<std::function<void()>> backendWork;
	void AddWork(std::function<void()> cb)
	{
		LMutex::Auto lck(&backendLock, _FL);
		backendWork.Add(std::move(cb));
	}
	int AddCall(const char *file, int line)
	{
		LMutex::Auto lck(&backendLock, _FL);
		CallRef ref(file, line);
		int id;
		while (backendCalls.Find(id = LRand(10000)))
			;
		backendCalls.Add(id, ref);
		#if 0
			StreamToLog log(Proj->GetApp());
			log.Print("AddCall %u=%s:%i\n", id, file, line);
		#endif
		return id;
	}
	void RemoveCall(int id)
	{
		LMutex::Auto lck(&backendLock, _FL);
		LAssert(backendCalls.Find(id));
		backendCalls.Delete(id);
	}

	// Step 1, look through parent folders for a project file..
	LString::Array backendPaths;
	void Step1();

	// Step 2, once found, rewrite the initDir and makefile path
	LString backendProjectFolder;
	void Step2();

	// Step 3, run the make process with the given details...
	LString backendArgs;
	LString backendInitFolder;		
	LString backendMakeFile; 
	LAutoPtr<int> backendExitCode;
	LAutoPtr<StreamToLog> buildLogger;
	void Step3();

public:
	BuildThread(IdeProject *proj, char *makefile, bool clean, BuildConfig config, SysPlatform platform, bool all, int wordsize);
	~BuildThread();
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override;
	LString FindExe();
	LAutoString WinToMingWPath(const char *path);
	int Main() override;
	void OnAfterMain() override;
};

class IdeProjectPrivate
{
public:
	AppWnd *App = NULL;
	IdeProject *Project = NULL;
	LAutoPtr<SystemIntf> Backend;
	bool Dirty = false, UserFileDirty = false;
	LString FileName;
	IdeProject *ParentProject = NULL;
	IdeProjectSettings Settings;
	LAutoPtr<BuildThread> Thread;
	LHashTbl<ConstStrKey<char,false>, ProjectNode*> Nodes;
	int NextNodeId = 1;
	ProjectNode *DepParent = NULL;

	// The build root folder:
	LString BuildFolder;

	// Threads
	LAutoPtr<class MakefileThread> CreateMakefile;

	// User info file
	LString UserFile;
	LHashTbl<IntKey<int>,int> UserNodeFlags;
	LArray<int> UserBreakpoints;

	void EmptyUserData()
	{
		UserNodeFlags.Empty();
		UserBreakpoints.Empty();
	}

	// Constructor
	IdeProjectPrivate(AppWnd *a, IdeProject *project, ProjectNode *depParent) :
		Project(project),
		Settings(project),
		DepParent(depParent)
	{
		App = a;
	}

	void CollectAllFiles(LTreeNode *Base, LArray<ProjectNode*> &Files, bool SubProjects, int Platform);
};

LString ToPlatformPath(const char *s, SysPlatform platform)
{
	LString p = s;
	if (platform == PlatformWin)
		return p.Replace("/", "\\");
	else
		return p.Replace("\\", "/");
}

class MakefileThread : public LThread, public LCancel
{
	IdeProjectPrivate *d;
	IdeProject *Proj;
	SysPlatform Platform;
	LStream *Log;
	bool BuildAfterwards;
	bool HasError;
	
	void ToNativePath(char *in)
	{
		for (auto s = in; *s; s++)
		{
			if (Platform == PlatformWin)
			{
				if (*s == '/') *s = '\\';
			}
			else
			{
				if (*s == '\\') *s = '/';
			}
		}
	}

public:
	static int Instances;

	MakefileThread(IdeProjectPrivate *priv, SysPlatform platform, bool Build) : LThread("MakefileThread")
	{
		Instances++;

		d = priv;
		Proj = d->Project;
		Platform = platform;
		BuildAfterwards = Build;
		HasError = false;
		Log = d->App->GetBuildLog();
		
		Run();
	}

	~MakefileThread()
	{
		Cancel();
		while (!IsExited())
			LSleep(1);
		Instances--;
	}
	
	void OnError(const char *Fmt, ...)
	{
		va_list Arg;
		va_start(Arg, Fmt);
		LStreamPrintf(Log, 0, Fmt, Arg);
		va_end(Arg);
		HasError = true;
	}
	
	void EmbedAppIcon(LStream &m)
	{
		// Is there an application icon configured?
		auto AppIcon = d->Settings.GetStr(ProjApplicationIcon, NULL, Platform);
		if (!AppIcon)
			return;

		if (Platform == PlatformHaiku)
		{
			bool showDebugLogging = false;
			
			// Find hvif_to_rdef.py
			LFile::Path scriptPath(LSP_APP_INSTALL);
			scriptPath += "../src/haiku/hvif_to_rdef.py";
			auto script = scriptPath.GetFull();
			if (!scriptPath.Exists())
			{
				Log->Print("%s:%i - Failed to find '%s'\n", _FL, script.Get());
				return;
			}
			
			// Create a path for the rdef file
			auto Exe = Proj->GetExecutable(Platform);
			auto appName = LGetLeaf(Exe);
			LString rdefPath;
			rdefPath.Printf("$(Build)/%s.rdef", appName);
			
			LString rsrcPath = rdefPath.Replace(".rdef", ".rsrc");
				
			auto Ext = LGetExtension(AppIcon);
			if (!Stricmp(Ext, "hvif"))
			{
				// Convert 'hvif' to 'rdef'
				m.Print("	%s %s %s %s\n",
					script.Get(),
					AppIcon,
					rdefPath.Get(),
					appName);
				
				// Convert from .rdef to .rsrc using 'rc'
				m.Print("	rc %s\n", rdefPath.Get());
					
				if (showDebugLogging)
					m.Print("	ls -l %s\n", rsrcPath.Get());
			
				// Embed in binary using 'resattr'
				m.Print("	resattr -o $(Target) %s\n"
						"	xres -o $(Target) %s\n",
					rsrcPath.Get(),
					rsrcPath.Get());
					
				if (showDebugLogging)
					// Print result:
					m.Print("	xres -l $(Target)\n");
			}
			else
			{
				Log->Print("%s:%i - No handler '%s' file type.\n", _FL, Ext);
			}
		}
		else
		{
			Log->Print("%s:%i - No handler for app icon embedding.\n", _FL);
		}
	}
	
	int Main()
	{
		const char *PlatformName = ToString(Platform);
		const char *PlatformLibraryExt = NULL;
		const char *PlatformStaticLibExt = NULL;
		const char *PlatformExeExt = "";
		const char *CCompilerFlags = "-MMD -MP -fPIC -fno-inline";
		const char *CppCompilerFlags = "$(CFlags) -fpermissive -std=c++14";
		const char *TargetType = d->Settings.GetStr(ProjTargetType, NULL, Platform);
		const char *CompilerName = d->Settings.GetStr(ProjCompiler);
		LString LinkerFlags;
		LString CCompilerBinary = "gcc";
		LString CppCompilerBinary = "g++";
		LStream *Log = d->App->GetBuildLog();
		bool IsExecutableTarget = TargetType && !stricmp(TargetType, "Executable");
		bool IsDynamicLibrary	= TargetType && !stricmp(TargetType, "DynamicLibrary");
		
		LAssert(Log);
		if (!Log)
			return false;
		
		Log->Print("CreateMakefile for '%s'...\n", PlatformName);
		
		if (Platform == PlatformWin)
		{
			LinkerFlags = ",--enable-auto-import";
		}
		else
		{
			if (IsDynamicLibrary)
			{
				LinkerFlags = ",-soname,$(TargetFile)";
			}
			LinkerFlags += ",-export-dynamic,-R.";
		}

		auto Base = Proj->GetBasePath();
		auto MakeFile = Proj->GetMakefile(Platform);
		LString MakeFilePath;
		Proj->CheckExists(MakeFile);
		if (!MakeFile)
		{
			MakeFilePath = Base.Get();
			LFile::Path p(MakeFilePath, "makefile");
			MakeFile = p.GetFull();
		}
		else if (LIsRelativePath(MakeFile))
		{
			LFile::Path p(Base);
			p += MakeFile;
			MakeFile = p.GetFull();
			MakeFilePath = (p / "..").GetFull();
		}
		else
		{
			LFile::Path p(MakeFile);
			MakeFilePath = (p / "..").GetFull();
		}
		
		// LGI_LIBRARY_EXT
		switch (Platform)
		{
			case PlatformWin:
				PlatformLibraryExt = "dll";
				PlatformStaticLibExt = "lib";
				PlatformExeExt = ".exe";
				break;
			case PlatformLinux:
			case PlatformHaiku:
				PlatformLibraryExt = "so";
				PlatformStaticLibExt = "a";
				break;
			case PlatformMac:
				PlatformLibraryExt = "dylib";
				PlatformStaticLibExt = "a";
				break;
			default:
				LAssert(0);
				break;
		}

		if (CompilerName)
		{
			if (!stricmp(CompilerName, "cross"))
			{
				LString CBin = d->Settings.GetStr(ProjCCrossCompiler, NULL, Platform);
				if (CBin && !LFileExists(CBin))
					FindInPath(CBin);
				if (CBin && LFileExists(CBin))
					CCompilerBinary = CBin;
				else
					Log->Print("%s:%i - Error: C cross compiler '%s' not found.\n", _FL, CBin.Get());
				
				LString CppBin = d->Settings.GetStr(ProjCppCrossCompiler, NULL, Platform);
				if (CppBin && !LFileExists(CppBin))
					FindInPath(CppBin);
				if (CppBin && LFileExists(CppBin))
					CppCompilerBinary = CppBin;
				else
					Log->Print("%s:%i - Error: C++ cross compiler '%s' not found.\n", _FL, CppBin.Get());
			}
		}
		
		LFile m;
		if (!m.Open(MakeFile, O_WRITE))
		{
			Log->Print("Error: Failed to open '%s' for writing.\n", MakeFile.Get());
			return false;
		}
		
		m.SetSize(0);
		
		m.Print("#!/usr/bin/make\n"
				"#\n"
				"# This makefile generated by LgiIde\n"
				"# http://www.memecode.com/lgi.php\n"
				"#\n"
				"\n"
				".SILENT :\n"
				"\n"
				"CC = %s\n"
				"CPP = %s\n",
				CCompilerBinary.Get(),
				CppCompilerBinary.Get());

		// Collect all files that require building
		LArray<ProjectNode*> Files;
		d->CollectAllFiles
		(
			Proj,
			Files,
			false,
			1 << Platform
		);
		
		if (IsExecutableTarget)
		{
			auto Exe = Proj->GetExecutable(Platform);
			if (Exe)
			{
				if (LIsRelativePath(Exe))
					m.Print("Target = %s\n", ToPlatformPath(Exe, Platform).Get());
				else
				{
					auto RelExe = LMakeRelativePath(Base, Exe);
					if (Base && RelExe)
					{
						m.Print("Target = %s\n", ToPlatformPath(RelExe, Platform).Get());
					}
					else
					{
						Log->Print("%s:%i - Error: Missing path (%s, %s).\n", _FL, Base.Get(), RelExe.Get());
						return false;
					}
				}
			}
			else
			{
				Log->Print("%s:%i - Error: No executable name specified (%s, %s).\n", _FL, TargetType, d->FileName.Get());
				return false;
			}
		}
		else
		{
			LString Target = Proj->GetTargetName(Platform);
			if (Target)
				m.Print("Target = %s\n", ToPlatformPath(Target, Platform).Get());
			else
			{
				Log->Print("%s:%i - Error: No target name specified.\n", _FL);
				return false;
			}
		}

		// Output the build mode, flags and some paths
		auto BuildMode = d->App->GetBuildMode();
		auto BuildModeName = toString(BuildMode);
		m.Print("ifndef Build\n"
				"	Build = %s\n"
				"endif\n",
				BuildModeName);
		m.Print("MakeDir := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))\n");
		
		LString sDefines[BuildMax];
		LString sLibs[BuildMax];
		LString sIncludes[BuildMax];
		LString sCompilerOpts[BuildMax];
		const char *ExtraLinkFlags = NULL;
		const char *ExeFlags = NULL;
		if (Platform == PlatformWin)
		{
			ExtraLinkFlags = "";
			ExeFlags = " -mwindows";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"CFlags = %s\n"
					"CppFlags = %s\n",
					CCompilerFlags,
					CppCompilerFlags);
			
			const char *DefDefs = "-DWIN32 -D_REENTRANT";
			sDefines[BuildDebug] = DefDefs;
			sDefines[BuildRelease] = DefDefs;
		}
		else
		{
			LString PlatformCap = PlatformName;
		
			ExtraLinkFlags = "";
			ExeFlags = "";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"CFlags = %s\n"
					"CppFlags = %s\n",
					CCompilerFlags,
					CppCompilerFlags);
			sDefines[0].Printf("-D%s -D_REENTRANT", PlatformCap.Upper().Get());
			#ifdef LINUX
			sDefines[BuildDebug] += " -D_FILE_OFFSET_BITS=64"; // >:-(
			sDefines[BuildDebug] += " -DPOSIX";
			#endif
			sDefines[BuildRelease] = sDefines[BuildDebug];
		}

		LArray<IdeProject*> Deps;
		Proj->GetChildProjects(Deps);
		for (auto d: Deps)
		{
			printf("Dep is: %p\n", d);
			if (!d)
			{
				LAssert(!"Dep is NULL!");
				return -1;
			}
		}

		for (int Cfg = BuildDebug; Cfg < BuildMax; Cfg++)
		{
			// Set the config
			auto cfgName = toString((BuildConfig)Cfg);
			d->Settings.SetCurrentConfig(cfgName);
		
			// Get the defines setup
			auto PDefs = d->Settings.GetStr(ProjDefines, NULL, Platform);
			if (ValidStr(PDefs))
			{
				LToken Defs(PDefs, " ;,\r\n");
				for (int i=0; i<Defs.Length(); i++)
				{
					LString s;
					s.Printf(" -D%s", Defs[i]);
					sDefines[Cfg] += s;
				}
			}
			
			// Get the compile options:
			auto compOpts = d->Settings.GetStr(ProjCompileOptions, NULL, Platform);
			if (ValidStr(compOpts))
			{
				sCompilerOpts[Cfg].Printf(" %s", compOpts);
			}
		
			// Collect all dependencies, output their lib names and paths
			LString PLibPaths = d->Settings.GetStr(ProjLibraryPaths, NULL, Platform);
			if (ValidStr(PLibPaths))
			{
				LString::Array LibPaths = PLibPaths.Split("\n");
				for (auto i: LibPaths)
				{
					LString s, in = i.Strip();
					if (!in.Length())
						continue;
					
					if (strchr("`-", in(0)))
					{
						s.Printf(" \\\n\t\t%s", in.Get());
					}
					else
					{
						LString Rel;
						if (!LIsRelativePath(in))
							Rel = LMakeRelativePath(Base, in);
							
						LString Final = Rel ? Rel.Get() : in.Get();
						if (!Proj->CheckExists(Final))
							OnError("%s:%i - Library path '%s' doesn't exist (from %s).\n",
								_FL, Final.Get(),
								Proj->GetFileName());

						s.Printf(" \\\n\t\t-L%s", ToUnixPath(Final));
					}
					
					sLibs[Cfg] += s;
				}
			}

			const char *PLibs = d->Settings.GetStr(ProjLibraries, NULL, Platform);
			if (ValidStr(PLibs))
			{
				LToken Libs(PLibs, "\r\n");
				for (int i=0; i<Libs.Length(); i++)
				{
					LString l = Libs[i];

					LString s;
					if (l(0) == '`' || l(0) == '-')
						s.Printf(" \\\n\t\t%s", l.Get());
					else
					{
						Proj->CheckExists(l);
						s.Printf(" \\\n\t\t-l%s", ToUnixPath(l));
					}
					sLibs[Cfg] += s;
				}
			}

			for (auto dep: Deps)
			{
				printf("Dep2 is: %p\n", d);
				if (!dep)
					continue;
				LString Target = dep->GetTargetName(Platform);
				if (Target)
				{
					char t[MAX_PATH_LEN];
					strcpy_s(t, sizeof(t), Target);
					if (!strnicmp(t, "lib", 3))
						memmove(t, t + 3, strlen(t + 3) + 1);
					char *dot = strrchr(t, '.');
					if (dot)
						*dot = 0;
													
					LString s, sTarget = t;
					Proj->CheckExists(sTarget);
					s.Printf(" \\\n\t\t-l%s$(Tag)", ToUnixPath(sTarget));
					sLibs[Cfg] += s;

					auto DepBase = dep->GetBasePath();
					if (DepBase)
					{
						LString DepPath = DepBase.Get();
						
						auto Rel = LMakeRelativePath(Base, DepPath);

						LString Final = Rel ? Rel.Get() : DepPath.Get();
						Proj->CheckExists(Final);
						s.Printf(" \\\n\t\t-L%s/$(BuildDir)", ToUnixPath(Final.RStrip("/\\")));
						sLibs[Cfg] += s;
					}
				}
			}
			
			// Includes

			// Do include paths
			LHashTbl<StrKey<char>,bool> Inc;
			auto ProjIncludes = d->Settings.GetStr(ProjIncludePaths, NULL, Platform);
			if (ValidStr(ProjIncludes))
			{
				// Add settings include paths.
				LToken Paths(ProjIncludes, "\r\n");
				for (int i=0; i<Paths.Length(); i++)
				{
					auto p = Paths[i];
					auto pn = ToNativeStr(p);					
					if (pn.Get()[0] != '`' && !Proj->CheckExists(pn))
						OnError("%s:%i - Include path '%s' doesn't exist.\n", _FL, pn.Get());
					else if (!Inc.Find(pn))
						Inc.Add(pn, true);
				}
			}
			const char *SysIncludes = d->Settings.GetStr(ProjSystemIncludes, NULL, Platform);
			if (ValidStr(SysIncludes))
			{
				// Add settings include paths.
				LToken Paths(SysIncludes, "\r\n");
				for (int i=0; i<Paths.Length(); i++)
				{
					auto p = Paths[i];
					auto pn = ToNativeStr(p);
					if (pn.Get()[0] != '`' && !Proj->CheckExists(pn))
						OnError("%s:%i - System include path '%s' doesn't exist (from %s).\n",
							_FL, pn.Get(),
							Proj->GetFileName());
					else if (!Inc.Find(pn))
						Inc.Add(pn, true);
				}
			}

			LString::Array Incs;
			for (auto i: Inc)
				Incs.New() = i.key;
			Incs.Sort();
			
			for (auto i: Incs)
			{
				LString s;
				if (*i == '`')
				{
					// Pass though shell cmd
					s.Printf(" \\\n\t\t%s", i.Get());
				}
				else
				{
					LFile::Path p;
					if (LIsRelativePath(i))
					{
						p = Base.Get();
						p += i;
					}
					else p = i;
					auto rel = LMakeRelativePath(Base, p.GetFull());
					s.Printf(" \\\n\t\t-I%s", ToUnixPath(rel ? rel : i));
				}

				sIncludes[Cfg] += s;
			}
		}

		// Output the defs section for Debug and Release

		// Debug specific
		m.Print("\n"
				"ifeq ($(Build),Debug)\n"
				"	CFlags += -g%s\n"
				"	CppFlags += -g%s\n"
				"	Tag = d\n"
				"	Defs = -D_DEBUG %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n",
			CastEmpty(sCompilerOpts[BuildDebug].Get()),
			CastEmpty(sCompilerOpts[BuildDebug].Get()),
			CastEmpty(sDefines     [BuildDebug].Get()),
			CastEmpty(sLibs        [BuildDebug].Get()),
			CastEmpty(sIncludes    [BuildDebug].Get()));
		
		// Release specific
		m.Print("else\n"
				"	CFlags += -s -Os%s\n"
				"	CppFlags += -s -Os%s\n"
				"	Defs = %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n"
				"endif\n"
				"\n",
			CastEmpty(sCompilerOpts[BuildRelease].Get()),
			CastEmpty(sCompilerOpts[BuildRelease].Get()),
			CastEmpty(sDefines     [BuildRelease].Get()),
			CastEmpty(sLibs        [BuildRelease].Get()),
			CastEmpty(sIncludes    [BuildRelease].Get()));
		
		if (Files.Length())
		{
			LArray<LString> VPath;
			// Proj->BuildIncludePaths(VPath, false, false, Platform);

			auto AddVPath = [&VPath](LString p)
			{
				for (auto s: VPath)
					if (p == s)
						return;
				VPath.Add(p);
			};

			// Do source code list...
			m.Print("# Dependencies\n"
					"Source =\t");
				
			LString::Array SourceFiles;
			for (auto &n: Files)
			{
				if (n->GetType() == NodeSrc)
				{
					auto f = n->GetFileName();

					LFile::Path p;
					if (LIsRelativePath(f))
					{
						p = Base.Get();
						p += f;
					}
					else
					{
						p = f;
					}

					auto rel = LMakeRelativePath(Base, p.GetFull());
					if (rel)
					{
						if (rel.Find("./") == 0)
							rel = rel(2, -1);
					}

					LString path = rel ? rel.Get() : f;
					ToNativePath(path);
					SourceFiles.Add(path);

					if (true)
					{
						// Also add path of file to VPath.
						AddVPath((p / "..").GetFull());
					}
				}
			}
			SourceFiles.Sort();
			int c = 0;
			for (auto &src: SourceFiles)
			{
				if (c++) m.Print(" \\\n\t\t\t");
				m.Print("%s", src.Get());
			}

			const char *TargetPattern = Platform == PlatformHaiku ? "$(abspath $<)" : "$<";

			m.Print("\n"
					"\n"
					"SourceC    := $(filter %%.c,$(Source))\n"
					"ObjectsC   := $(SourceC:.c=.o)\n"
					"SourceCpp  := $(filter %%.cpp,$(Source))\n"
					"ObjectsCpp := $(SourceCpp:.cpp=.o)\n"
					"Objects    := $(notdir $(ObjectsC) $(ObjectsCpp))\n"
					"Objects    := $(addprefix $(BuildDir)/,$(Objects))\n"
					"Deps       := $(patsubst %%.o,%%.d,$(Objects))\n"
					"\n"
					"$(BuildDir)/%%.o: %%.c\n"
					"	mkdir -p $(@D)\n"
					"	echo $(notdir $<) [$(Build)]\n"
					"	$(CC) $(Inc) $(CFlags) $(Defs) -c %s -o $@\n"
					"\n"
					"$(BuildDir)/%%.o: %%.cpp\n"
					"	mkdir -p $(@D)\n"
					"	echo $(notdir $<) [$(Build)]\n"
					"	$(CPP) $(Inc) $(CppFlags) $(Defs) -c %s -o $@\n"
					"\n",
					TargetPattern, TargetPattern);

			// Write out the target stuff
			m.Print("# Target\n");

			LHashTbl<StrKey<char,false>,bool> DepFiles;

			if (TargetType)
			{
				if (IsExecutableTarget)
				{
					m.Print("# Executable target\n"
							"$(Target) :");
							
					LStringPipe Rules;
					IdeProject *Dep;
					
					uint64 Last = LCurrentTime();
					int Count = 0;
					auto It = Deps.begin();
					for (Dep=*It; Dep && !IsCancelled(); Dep=*(++It), Count++)
					{
						// Get dependency to create it's own makefile...
						Dep->CreateMakefile(Platform, false);
					
						// Build a rule to make the dependency if any of the source changes...
						auto DepBase = Dep->GetBasePath();
						auto TargetFile = Dep->GetTargetFile(Platform);
						
						if (DepBase && Base && TargetFile)
						{
							LString Rel = LMakeRelativePath(Base, DepBase);
							
							// Add tag to target name
							auto Parts = TargetFile.SplitDelimit(".");
							if (Parts.Length() == 2)
								TargetFile.Printf("lib%s$(Tag).%s", Parts[0].Get(), Parts[1].Get());

							LFile::Path Buf(Rel);
							Buf += "$(BuildDir)";
							Buf += TargetFile;
							auto FullBuf = Buf.GetFull();
							ToNativePath(FullBuf);
							m.Print(" %s", FullBuf.Get());
							
							LArray<char*> AllDeps;
							Dep->GetAllDependencies(AllDeps, Platform);
							LAssert(AllDeps.Length() > 0);
							AllDeps.Sort(StrSort);

							Rules.Print("%s : ", FullBuf.Get());
							for (int i=0; i<AllDeps.Length(); i++)
							{
								if (i)
									Rules.Print(" \\\n\t");
								
								LString DepRel = LMakeRelativePath(Base, AllDeps[i]);
								auto f = DepRel ? DepRel.Get() : AllDeps[i];
								ToNativePath(f);
								Rules.Print("%s", f);
								
								// Add these dependencies to this makefiles dep list
								if (!DepFiles.Find(f))
									DepFiles.Add(f, true);
							}

							AllDeps.DeleteArrays();

							ToNativePath(Rel);
							Rules.Print("\n\texport Build=$(Build); \\\n"
										"\t$(MAKE) -C %s",
										Rel.Get());

							auto Mk = Dep->GetMakefile(Platform);
							if (Mk)
							{
								LString MakefileRel = LMakeRelativePath(DepBase, Mk);
								if (MakefileRel)
								{
									ToNativePath(MakefileRel);	
									Rules.Print(" -f %s", MakefileRel.Get());
								}
								else if (auto DepMakefile = strrchr(Mk, DIR_CHAR))
								{
									Rules.Print(" -f %s", DepMakefile + 1);
								}
							}
							else
							{
								Mk = Dep->GetMakefile(Platform);
								OnError("%s:%i - No makefile for '%s'\n", _FL, Dep->GetFullPath().Get());
							}

							Rules.Print("\n\n");
						}
						
						uint64 Now = LCurrentTime();
						if (Now - Last > 1000)
						{
							Last = Now;
							Log->Print("Building deps %i%%...\n", (int) (((int64)Count+1)*100/Deps.Length()));
						}
					}

					m.Print(" $(Objects)\n"
							"	mkdir -p $(BuildDir)\n"
							"	@echo Linking $(Target) [$(Build)]...\n"
							"	$(CPP)%s%s %s%s -o \\\n"
							"		$(Target) $(Objects) $(Libs)\n",
							ExtraLinkFlags,
							ExeFlags,
							ValidStr(LinkerFlags) ? "-Wl" : "", LinkerFlags.Get());
					
					EmbedAppIcon(m);
					
					LString PostBuildCmds = d->Settings.GetStr(ProjPostBuildCommands, NULL, Platform);
					if (ValidStr(PostBuildCmds))
					{
						LString::Array a = PostBuildCmds.Split("\n");
						for (unsigned i=0; i<a.Length(); i++)
							m.Print("\t%s\n", a[i].Strip().Get());
					}

					m.Print("	@echo Done.\n"
							"\n");

					auto r = Rules.NewLStr();
					if (r)
						m.Write(r);

					// Various fairly global rules
					m.Print("-include $(Objects:.o=.d)\n"
							"\n"						
							"# Clean just this target\n"
							"clean :\n"
							"	rm -rf $(BuildDir) $(Target)%s\n"
							"	@echo Cleaned $(BuildDir) $(Target)\n"
							"\n",
							PlatformExecutableExt(Platform));
					
					m.Print("# Clean all targets\n"
							"cleanall :\n"
							"	rm -rf $(BuildDir) $(Target)%s\n"
							"	@echo Cleaned $(BuildDir) $(Target)\n",
							PlatformExecutableExt(Platform));					
					for (auto Dep: Deps)
					{
						if (!Dep)
						{
							Log->Print("%s:%i - NULL dep?\n", _FL);
							continue;
						}
						auto Mk = Dep->GetMakefile(Platform);
						if (Mk)
						{
							auto DepBase = Dep->GetBasePath();
							Dep->CheckExists(DepBase);
							
							LString DepFolderPath = LMakeRelativePath(Base, DepBase);
							if (!DepFolderPath)
								DepFolderPath = DepBase;
							ToNativePath(DepFolderPath);
							
							m.Print("	+make -C \"%s\"", DepFolderPath.Get());
							
							LString MakefileRel = LMakeRelativePath(DepBase, Mk);
							if (MakefileRel)
							{
								ToNativePath(MakefileRel);	
								m.Print(" -f %s clean\n", MakefileRel.Get());
							}
							else if (auto DepMakefile = strrchr(Mk, DIR_CHAR))
							{
								m.Print(" -f %s clean\n", DepMakefile + 1);
							}
						}
					}
					
					m.Print("\n");
				}
				// Shared library
				else if (!stricmp(TargetType, "DynamicLibrary"))
				{
					m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
							"$(TargetFile) : $(Objects)\n"
							"	mkdir -p $(BuildDir)\n"
							"	@echo Linking $(TargetFile) [$(Build)]...\n"
							"	$(CPP)$s -shared \\\n"
							"		%s%s \\\n"
							"		-o $(BuildDir)/$(TargetFile) \\\n"
							"		$(Objects) \\\n"
							"		$(Libs)\n",
							PlatformLibraryExt,
							ValidStr(ExtraLinkFlags) ? "-Wl" : "", ExtraLinkFlags,
							LinkerFlags.Get());

					LString PostBuildCmds = d->Settings.GetStr(ProjPostBuildCommands, NULL, Platform);
					if (ValidStr(PostBuildCmds))
					{
						LString::Array a = PostBuildCmds.Split("\n");
						for (unsigned i=0; i<a.Length(); i++)
							m.Print("\t%s\n", a[i].Strip().Get());
					}

					m.Print("	@echo Done.\n"
							"\n");

					// Other rules
					m.Print("-include $(Objects:.o=.d)\n"
							"\n"
							"# Clean out targets\n"
							"clean :\n"
							"	rm -rf $(BuildDir)\n"
							"	@echo Cleaned $(BuildDir)\n"
							"\n",
							PlatformLibraryExt);
				}
				// Static library
				else if (!stricmp(TargetType, "StaticLibrary"))
				{
					m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
							"$(TargetFile) : $(Objects)\n"
							"	mkdir -p $(BuildDir)\n"
							"	@echo Linking $(TargetFile) [$(Build)]...\n"
							"	ar rcs $(BuildDir)/$(TargetFile) $(Objects)\n",
							PlatformStaticLibExt);

					LString PostBuildCmds = d->Settings.GetStr(ProjPostBuildCommands, NULL, Platform);
					if (ValidStr(PostBuildCmds))
					{
						LString::Array a = PostBuildCmds.Split("\n");
						for (unsigned i=0; i<a.Length(); i++)
							m.Print("\t%s\n", a[i].Strip().Get());
					}

					m.Print("	@echo Done.\n"
							"\n");

					// Other rules
					m.Print("-include $(Objects:.o=.d)\n"
							"\n"
							"# Clean out targets\n"
							"clean :\n"
							"	rm -rf $(BuildDir)\n"
							"	@echo Cleaned $(BuildDir)\n"
							"\n",
							PlatformStaticLibExt);
				}

				// Output VPATH
				m.Print("VPATH=$(BuildDir)");
				VPath.Sort();
				for (int i=0; i<VPath.Length() && !IsCancelled(); i++)
				{
					LString p = VPath[i];
					Proj->CheckExists(p);
					if (p && !strchr(p, '`'))
					{
						if (!LIsRelativePath(p))
						{
							auto a = LMakeRelativePath(Base, p);
							m.Print(" \\\n\t%s", ToPlatformPath(a ? a.Get() : p.Get(), Platform).Get());
						}
						else
						{
							m.Print(" \\\n\t%s", ToPlatformPath(p, Platform).Get());
						}
					}
				}
				m.Print("\n");

				const char *OtherMakefileRules = d->Settings.GetStr(ProjMakefileRules, NULL, Platform);
				if (ValidStr(OtherMakefileRules))
				{
					m.Print("\n%s\n", OtherMakefileRules);
				}
			}
		}
		else
		{
			m.Print("# No files require building.\n");
		}

		Log->Print("...Done: '%s'\n", MakeFile.Get());

		if (BuildAfterwards)
		{
			if (!Proj->GetApp()->PostEvent(M_START_BUILD))
				printf("%s:%i - PostEvent(M_START_BUILD) failed.\n", _FL);
		}
		
		return HasError;
	}

	void OnAfterMain()
	{
		Proj->GetApp()->PostEvent(M_MAKEFILES_CREATED, (LMessage::Param)Proj);
	}
};


/////////////////////////////////////////////////////////////////////////////////////
NodeSource::~NodeSource()
{
	if (nView)
	{
		nView->nSrc = 0;
	}
}

NodeView::~NodeView()
{
	if (nSrc)
	{
		nSrc->nView = 0;
	}
}


//////////////////////////////////////////////////////////////////////////////////
int NodeSort(LTreeItem *a, LTreeItem *b, NativeInt d)
{
	ProjectNode *A = dynamic_cast<ProjectNode*>(a);
	ProjectNode *B = dynamic_cast<ProjectNode*>(b);
	if (A && B)
	{
		if
		(
			(A->GetType() == NodeDir)
			^
			(B->GetType() == NodeDir)
		)
		{
			return A->GetType() == NodeDir ? -1 : 1;
		}
		else
		{
			return Stricmp(a->GetText(0), b->GetText(0));
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ReadVsProjFile(LString File, LString &Ver, LString::Array &Configs)
{
	const char *Ext = LGetExtension(File);
	if (!Ext || _stricmp(Ext, "vcxproj"))
		return false;

	LFile f;
	if (!f.Open(File, O_READ))
		return false;

	LXmlTree Io;
	LXmlTag r;
	if (Io.Read(&r, &f) &&
		r.IsTag("Project"))
	{
		Ver = r.GetAttr("ToolsVersion");

		LXmlTag *ItemGroup = r.GetChildTag("ItemGroup");
		if (ItemGroup)
			for (auto c: ItemGroup->Children)
			{
				if (c->IsTag("ProjectConfiguration"))
				{
					char *s = c->GetAttr("Include");
					if (s)
						Configs.New() = s;
				}
			}
	}
	else return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BuildThread::BuildThread(IdeProject *proj,
						char *makefile,
						bool clean,
						BuildConfig config,
						SysPlatform platform,
						bool all,
						int wordsize)
	: LThread("BuildThread"), backendLock("BuildThread.Lock")
{
	Proj = proj;
	Makefile = makefile;
	Clean = clean;
	Config = config;
	Platform = platform == PlatformCurrent ? GetCurrentPlatform() : platform;
	All = all;
	WordSize = wordsize;
	Arch = DefaultArch;
	Compiler = DefaultCompiler;
	AppHnd = Proj->GetApp()->AddDispatch();
	
	LString Cmds = proj->d->Settings.GetStr(ProjPostBuildCommands, NULL);
	if (ValidStr(Cmds))
		PostBuild = Cmds.SplitDelimit("\r\n");

	auto Ext = LGetExtension(Makefile);
	if (Ext && !_stricmp(Ext, "py"))
		Compiler = PythonScript;
	else if (Ext && !_stricmp(Ext, "sln"))
		Compiler = VisualStudio;
	else if (Ext && !Stricmp(Ext, "xcodeproj"))
		Compiler = Xcode;
	else
	{
		auto Comp = Proj->GetSettings()->GetStr(ProjCompiler, NULL, Platform);
		if (Comp)
		{
			// Use the specified compiler...
			if (!stricmp(Comp, "VisualStudio"))
				Compiler = VisualStudio;
			else if (!stricmp(Comp, "MingW"))
				Compiler = MingW;
			else if (!stricmp(Comp, "gcc"))
				Compiler = Gcc;
			else if (!stricmp(Comp, "cross"))
				Compiler = CrossCompiler;
			else if (!stricmp(Comp, "IAR"))
				Compiler = IAR;
			else if (!stricmp(Comp, "Cygwin"))
				Compiler = Cygwin;
			else
			{
				LgiTrace("%s:%i - Unknown compiler '%s' for makefile '%s'\n", _FL, Comp, makefile);
				LAssert(!"Unknown compiler.");
			}
		}
	}

	if (Compiler == DefaultCompiler)
	{
		// Use default compiler for platform...
		if (Platform == PlatformWin)
			Compiler = VisualStudio;
		else if (Platform == PlatformMac)
			Compiler = Xcode;
		else
			Compiler = Gcc;
	}

	Run();
}

void BuildThread::OnAfterMain()
{
	auto p = Proj;
	p->GetApp()->RunCallback([p]()
	{
		p->BuildThreadFinished();
	});
}

BuildThread::~BuildThread()
{
	Cancel();
}

ssize_t BuildThread::Write(const void *Buffer, ssize_t Size, int Flags)
{
	if (Proj->GetApp())
	{
		Proj->GetApp()->PostEvent(M_APPEND_TEXT, (LMessage::Param)NewStr((char*)Buffer, Size), AppWnd::BuildTab);
	}
	return Size;
}

#pragma comment(lib, "version.lib")

struct ProjInfo
{
	LString Guid, Name, File;
	LHashTbl<StrKey<char>,ssize_t> Configs;
};

LString BuildThread::FindExe()
{
	LString::Array p = LGetPath();

	if (Compiler == PythonScript)
	{
		#if defined(WINDOWS)
		uint32_t BestVer = 0;
		#else
		LString BestVer;
		#endif
		static LString Best;

		if (!Best)
		{
			const char *binName[] = {"python3", "python"};
			for (int i=0; i<p.Length(); i++)
			{
				char Path[MAX_PATH_LEN];
				for (unsigned n=0; n<CountOf(binName); n++)
				{
					LMakePath(Path, sizeof(Path), p[i], LString(binName[n]) + LGI_EXECUTABLE_EXT);
					if (LFileExists(Path))
					{			
						// Check version
						#if defined(WINDOWS)
							DWORD Sz = GetFileVersionInfoSizeA(Path, NULL);
							void *Buf = malloc(Sz);
							if (GetFileVersionInfoA(Path, NULL, Sz, Buf))
							{
								LPVOID Ptr = NULL;
								UINT Bytes;
								if (VerQueryValueA(Buf, "\\", &Ptr, &Bytes))
								{
									VS_FIXEDFILEINFO *v = (VS_FIXEDFILEINFO *)Ptr;
									if (v->dwProductVersionMS > BestVer)
									{
										BestVer = v->dwProductVersionMS;
										Best = Path;
									}
								}
							}
							else if (!Best)
							{
								Best = Path;
							}
							free(Buf);
						#else
							LSubProcess p(Path, "--version");
							LStringPipe o;
							if (p.Start())
								p.Communicate(&o);
							auto Out = o.NewLStr();
							auto Ver = Out.SplitDelimit().Last();
							printf("Ver=%s\n", Ver.Get());

							if (!BestVer || Stricmp(Ver.Get(), BestVer.Get()) > 0)
							{
								Best = Path;
								BestVer = Ver;
							}
							break;
						#endif
					}
				}
			}
		}

		return Best;
	}
	else if (Compiler == VisualStudio)
	{
		// Find the version we need:
		double fVer = 0.0;

		ProjInfo *First = NULL;
		LHashTbl<StrKey<char>, ProjInfo*> Projects;
		const char *Ext = LGetExtension(Makefile);
		if (Ext && !_stricmp(Ext, "sln"))
		{
			LFile f;
			if (f.Open(Makefile, O_READ))
			{
				LString ProjKey = "Project(";
				LString StartSection = "GlobalSection(";
				LString EndSection = "EndGlobalSection";
				LString Section;
				ssize_t Pos;

				LString::Array Ln = f.Read().SplitDelimit("\r\n");
				for (size_t i = 0; i < Ln.Length(); i++)
				{
					LString s = Ln[i].Strip();
					if ((Pos = s.Find(ProjKey)) >= 0)
					{
						LString::Array p = s.SplitDelimit("(),=");
						if (p.Length() > 5)
						{
							ProjInfo *i = new ProjInfo;
							i->Name = p[3].Strip(" \t\"");
							i->File = p[4].Strip(" \t\'\"");
							i->Guid = p[5].Strip(" \t\"");
							if (LIsRelativePath(i->File))
							{
								char f[MAX_PATH_LEN];
								LMakePath(f, sizeof(f), Makefile, "..");
								LMakePath(f, sizeof(f), f, i->File);
								if (LFileExists(f))
									i->File = f;
								/*
								else
									LAssert(0);
								*/
							}

							if (!First)
								First = i;

							Projects.Add(i->Guid, i);
						}
					}
					else if (s.Find(StartSection) >= 0)
					{
						auto p = s.SplitDelimit("() \t");
						Section = p[1];
					}
					else if (s.Find(EndSection) >= 0)
					{
						Section.Empty();
					}
					else if (Section == "ProjectConfigurationPlatforms")
					{
						auto p = s.SplitDelimit(". \t");
						auto i = Projects.Find(p[0]);
						if (i)
						{
							if (!i->Configs.Find(p[1]))
							{
								auto Idx = i->Configs.Length() + 1;
								i->Configs.Add(p[1], Idx);
							}
						}
					}
					else if (Section == "SolutionConfigurationPlatforms")
					{
						auto p = s.SplitDelimit();
						auto config = p[0];
						for (auto &it: Projects)
						{							
							auto proj = it.value;
							if (!proj->Configs.Find(config))
								proj->Configs.Add(config, proj->Configs.Length()+1);
						}
					}
				}
			}
		}
		else if (Ext && !_stricmp(Ext, "vcxproj"))
		{
			// ProjFile = Makefile;
		}
		else
		{
			if (Arch == DefaultArch)
			{
				if (sizeof(size_t) == 4)
					Arch = ArchX32;
				else
					Arch = ArchX64;
			}

			#ifdef _MSC_VER
			// Nmake file..
			LString NmakePath;
			switch (_MSC_VER)
			{
				#ifdef _MSC_VER_VS2013
					case _MSC_VER_VS2013:
					{
						if (Arch == ArchX32)
							NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\bin\\nmake.exe";
						else
							NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\bin\\amd64\\nmake.exe";
						break;
					}
				#endif
				#ifdef _MSC_VER_VS2015
					case _MSC_VER_VS2015:
					{
						if (Arch == ArchX32)
							NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\nmake.exe";
						else
							NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\amd64\\nmake.exe";
						break;
					}
				#endif
				default:
				{
					#ifdef _MSC_VER_VS2022
						if (_MSC_VER >= _MSC_VER_VS2022)
						{
							if (Arch == ArchX32)
								NmakePath = "c:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.39.33519\\bin\\Hostx64\\x86\\nmake.exe";
							else
								NmakePath = "c:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.39.33519\\bin\\Hostx64\\x64\\nmake.exe";
							break;
						}
					#endif
					#ifdef _MSC_VER_VS2019
						if (_MSC_VER >= _MSC_VER_VS2019)
						{
							if (Arch == ArchX32)
								NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.29.30133\\bin\\Hostx64\\x86\\nmake.exe";
							else
								NmakePath = "c:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.29.30133\\bin\\Hostx64\\x64\\nmake.exe";
							break;
						}
					#endif

					LgiTrace("%s:%i - _MSC_VER=%i\n", _FL, _MSC_VER);
					LAssert(!"Impl me.");
					break;
				}
			}

			if (LFileExists(NmakePath))
			{
				Compiler = Nmake;
				return NmakePath;
			}
			#endif
		}

		/*
		if (ProjFile && LFileExists(ProjFile))
		{
			LString sVer;
			if (ReadVsProjFile(ProjFile, sVer, BuildConfigs))
			{
				fVer = sVer.Float();
			}
		}
		*/
		if (First)
		{
			for (auto i: First->Configs)
			{
				BuildConfigs[i.value - 1] = i.key;

				LFile f(First->File, O_READ);
				LXmlTree t;
				LXmlTag r;
				if (t.Read(&r, &f))
				{
					if (r.IsTag("Project"))
					{
						auto ToolsVersion = r.GetAttr("ToolsVersion");
						if (ToolsVersion)
						{
							fVer = atof(ToolsVersion);
						}
					}
				}
			}
		}

		if (fVer > 0.0)
		{
			for (int i=0; i<CountOf(VsBinaries); i++)
			{
				LString p;
				p.Printf("C:\\Program Files (x86)\\Microsoft Visual Studio %.1f\\Common7\\IDE\\%s", fVer, VsBinaries[i]);
				if (LFileExists(p))
				{
					return p;
				}
			}
		}
	}
	else if (Compiler == IAR)
	{
		// const char *Def = "c:\\Program Files (x86)\\IAR Systems\\Embedded Workbench 7.0\\common\\bin\\IarBuild.exe";
		
		LString ProgFiles = LGetSystemPath(LSP_USER_APPS, 32);
		char p[MAX_PATH_LEN];
		if (!LMakePath(p, sizeof(p), ProgFiles, "IAR Systems"))
			return LString();
		LDirectory d;
		double LatestVer = 0.0;
		LString Latest;
		for (int b = d.First(p); b; b = d.Next())
		{
			if (d.IsDir())
			{
				LString n(d.GetName());
				LString::Array p = n.Split(" ");
				if (p.Length() == 3 &&
					p[2].Float() > LatestVer)
				{
					LatestVer = p[2].Float();
					Latest = n;
				}
			}
		}
		if (Latest &&
			LMakePath(p, sizeof(p), p, Latest) &&
			LMakePath(p, sizeof(p), p, "common\\bin\\IarBuild.exe"))
		{
			if (LFileExists(p))
				return p;
		}
	}
	else if (Compiler == Xcode)
	{
		return "/usr/bin/xcodebuild";
	}
	else if (Compiler == Cygwin)
	{
		#ifdef WINDOWS
		LRegKey k(false, "HKEY_CURRENT_USER\\Software\\Cygwin\\Installations");
		List<char> n;
		k.GetValueNames(n);
		LString s;
		for (auto i:n)
		{
			s = k.GetStr(i);
			if (s.Find("\\??\\") == 0)
				s = s(4,-1);
			if (LDirExists(s))
			{
				CygwinPath = s;
				break;
			}
		}
		n.DeleteArrays();

		LFile::Path p(s, "bin\\make.exe");
		if (p.Exists())
			return p.GetFull();
		#endif
	}
	else if (auto backend = Proj->GetBackend())
	{
		// Assume the remote system has make...
		return "make";
	}
	else
	{
		// Look up local make to build with gcc
		if (Compiler == MingW)
		{
			// Have a look in the default spot first...
			const char *Def = "C:\\MinGW\\msys\\1.0\\bin\\make.exe";
			if (LFileExists(Def))
			{
				return Def;
			}				
		}
		
		for (size_t i=0; i<p.Length(); i++)
		{
			char Exe[MAX_PATH_LEN];
			LMakePath
			(
				Exe,
				sizeof(Exe),
				p[i],
				"make"
				LGI_EXECUTABLE_EXT
			);
			if (LFileExists(Exe))
			{
				return Exe;
			}
		}
	}
	
	return LString();
}

LAutoString BuildThread::WinToMingWPath(const char *path)
{
	LToken t(path, "\\");
	LStringPipe a(256);
	for (int i=0; i<t.Length(); i++)
	{
		char *p = t[i];
		char *colon = strchr(p, ':');
		if (colon)
		{
			*colon = 0;
			StrLwr(p);
		}
		a.Print("/%s", p);				
	}
	return LAutoString(a.NewStr());
}

void BuildThread::Step1()
{
	auto backend = Proj->GetBackend();
	if (!backend || backendPaths.Length() == 0)
		return; // nothing to do...

	// Step 1, check the path for a project file...
	auto p = backendPaths.PopLast();

	StreamToLog log(Proj->GetApp());
	// log.Print("%s: readdir '%s'\n", __FUNCTION__, p.Get());
	auto callId = AddCall(_FL);
	LOG("Step1 reading folder..\n");
	backend->ReadFolder(SystemIntf::TForeground,
		p,
		[this, backend, callId](auto d)
		{
			StreamToLog log(Proj->GetApp());
			LOG("Step1 got folder..\n");
			for (int i=true; i; i=d->Next())
			{
				if (d->IsDir())
					continue;
				auto nm = d->GetName();
				auto ext = LGetExtension(nm);
				if (!Stricmp(ext, "xml"))
				{
					// log.Print("%s: got xml '%s'\n", __FUNCTION__, nm);

					// Could be a project file?
					auto readId = AddCall(_FL);
					backend->Read(SystemIntf::TForeground,
						d->FullPath(),
						[this, full=LString(d->FullPath()), readId](auto err, auto data)
						{
							StreamToLog log(Proj->GetApp());
							// log.Print("%s: got content '%s'\n", __FUNCTION__, full.Get());

							// If has project data..
							if (data.Find("<Project") > 0)
							{
								if (!backendProjectFolder)
								{
									backendProjectFolder = full;
									backendPaths.Empty();

									LString cpy = backendProjectFolder.Get();
									LTrimDir(cpy);
									Proj->SetBuildFolder(cpy.Get());

									AddWork([this]()
										{
											Step2();
										}); // Go to step2...
								}
							}
							
							RemoveCall(readId);
						});
				}
			}

			if (backendPaths.Length() > 0)
				AddWork([this]() { Step1(); }); // Look at the next path...
			RemoveCall(callId);
		});
}

void BuildThread::Step2()
{
	auto backend = Proj->GetBackend();
	if (!backend || !backendProjectFolder)
		return; // nothing to do...

	// Step 2, rewrite the init dir and makefile path
	backendInitFolder = backendProjectFolder.Get();
	LTrimDir(backendInitFolder);

	LArray<char*> parts;
	const char *s = backendArgs;
	while (auto part = LTokStr(s))
		parts.Add(part);

	auto &last = parts.Last();
	backendMakeFile = LString(last).Strip("\"\'");
	auto pos = backendMakeFile.Find(backendInitFolder);
	if (pos == 0)
	{
		// Remove the leading path...
		backendMakeFile = backendMakeFile.Replace(backendInitFolder).LStrip("\\/");
	}

	delete [] last;
	last = NewStr(backendMakeFile);

	LStringPipe p;
	for (auto part: parts)
		p.Print("%s%s", p.GetSize() ? " " : "", part);

	backendArgs = p.NewLStr();
	parts.DeleteArrays();

	StreamToLog log(Proj->GetApp());
	LOG("Step2 add 3..\n");
	AddWork([this]() { Step3(); }); // Move onto the build itself...
}

void BuildThread::Step3()
{
	auto backend = Proj->GetBackend();
	if (!backend || !backendMakeFile)
		return; // nothing to do...

	// Step 3, run the build...
	StreamToLog log(Proj->GetApp());
	if (buildLogger.Reset(new StreamToLog(Proj->GetApp()))) // Create a persistent logger, to outlive the build process...
	{
		// buildLogger->Print("%s: building in '%s'\n", __FUNCTION__, backendInitFolder.Get());
		auto buildId = AddCall(_FL);
		LOG("Step3 run process..\n");
		backend->RunProcess(backendInitFolder, LString("make ") + backendArgs, buildLogger, this, [this, buildId](auto val)
			{
				backendExitCode.Reset(new int(val));
				RemoveCall(buildId);
			},
			Proj->GetApp()->GetBuildLog());
	}
	else Cancel();
}

int BuildThread::Main()
{
	const char *Err = 0;
	char ErrBuf[256];

	auto Exe = FindExe();
	if (Exe)
	{
		bool Status = false;
		LString MakePath = Makefile.Get();
		LString InitDir;
		if (Proj->GetBackend())
			InitDir = Proj->GetBackend()->GetBasePath();
		else if (auto path = Proj->GetBasePath())
			InitDir = path.Get();

		LVariant Jobs;
		if (!Proj->GetApp()->GetOptions()->GetValue(OPT_Jobs, Jobs) || Jobs.CastInt32() < 1)
			Jobs = 2;

		LString TmpArgs, Include, Lib, LibPath, Path;
		
		if (Compiler == VisualStudio)
		{
			// TmpArgs.Printf("\"%s\" /make \"All - Win32 Debug\"", Makefile.Get());
			LString BuildConf = "All - Win32 Debug";
			if (BuildConfigs.Length())
			{
				auto Key = toString(Config);
				for (size_t i=0; i<BuildConfigs.Length(); i++)
				{
					LString c = BuildConfigs[i];
					if (c.Find(Key) >= 0)
					{
						if (!BuildConf || (c.Find("x64") >= 0 && BuildConf.Find("x64") < 0))
							BuildConf = c;
					}
				}
			}
			TmpArgs.Printf("\"%s\" %s \"%s\"", Makefile.Get(), Clean ? "/Clean" : "/Build", BuildConf.Get());
		}
		else if (Compiler == Nmake)
		{
			const char *DefInc[] = {
				"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\INCLUDE",
				"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\INCLUDE",
				"C:\\Program Files (x86)\\Windows Kits\\8.1\\include\\shared",
				"C:\\Program Files (x86)\\Windows Kits\\8.1\\include\\um",
				"C:\\Program Files (x86)\\Windows Kits\\8.1\\include\\winrt"
			};
			LString f;

			#define ADD_PATHS(out, in) \
				for (unsigned i=0; i<CountOf(in); i++) \
				{ \
					f.Printf("%s%s", i ? LGI_PATH_SEPARATOR : "", in[i]); \
					out += f; \
				}
			if (Arch == ArchX32)
			{
				const char *DefLib[] = {
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\lib\\winv6.3\\um\\x32"
				};
				const char *DefLibPath[] = {
					"C:\\WINDOWS\\Microsoft.NET\\Framework64\\v4.0.30319",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\References\\CommonConfiguration\\Neutral",
					"C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v8.1\\ExtensionSDKs\\Microsoft.VCLibs\\12.0\\References\\CommonConfiguration\\neutral"
				};
				const char *DefPath[] = {
					"c:\\Program Files (x86)\\Windows Kits\\8.1\\bin\\x86"
				};
			
				ADD_PATHS(Include, DefInc);
				ADD_PATHS(Lib, DefLib);
				ADD_PATHS(LibPath, DefLibPath);
				ADD_PATHS(Path, DefPath);
			}
			else if (Arch == ArchX64)
			{
				const char *DefLib[] = {
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\lib\\winv6.3\\um\\x64"
				};
				const char *DefLibPath[] = {
					"C:\\WINDOWS\\Microsoft.NET\\Framework64\\v4.0.30319",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\ATLMFC\\LIB\\amd64",
					"C:\\Program Files (x86)\\Windows Kits\\8.1\\References\\CommonConfiguration\\Neutral",
					"C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v8.1\\ExtensionSDKs\\Microsoft.VCLibs\\12.0\\References\\CommonConfiguration\\neutral"
				};
				const char *DefPath[] = {
					"c:\\Program Files (x86)\\Windows Kits\\8.1\\bin\\x64"
				};
							
				ADD_PATHS(Include, DefInc);
				ADD_PATHS(Lib, DefLib);
				ADD_PATHS(LibPath, DefLibPath);
				ADD_PATHS(Path, DefPath);
			}
			else LAssert(!"Invalid Arch");

			TmpArgs.Printf(" -f \"%s\"", Makefile.Get());

			if (Clean)
				TmpArgs += " clean";
		}
		else if (Compiler == PythonScript)
		{
			TmpArgs = MakePath;
		}
		else if (Compiler == IAR)
		{
			LString Conf;

			LFile f;
			if (f.Open(Makefile, O_READ))
			{
				LXmlTree t;
				LXmlTag r;
				if (t.Read(&r, &f))
				{
					LXmlTag *c = r.GetChildTag("configuration");
					c = c ? c->GetChildTag("name") : NULL;
					if (c)
						Conf = c->GetContent();
				}				
			}

			TmpArgs.Printf("\"%s\" %s %s -log warnings", Makefile.Get(), Clean ? "-clean" : "-make", Conf.Get());
		}
		else if (Compiler == Xcode)
		{
			LString::Array Configs;
			Configs.SetFixedLength(false);
			LString a;
			a.Printf("-list -project \"%s\"", Makefile.Get());
			LSubProcess Ls(Exe, a);
			if (Ls.Start())
			{
				LStringPipe o;
				Ls.Communicate(&o);
				auto key = "Build Configurations:";
				auto lines = o.NewLStr().SplitDelimit("\n");
				int inKey = -1;
				for (auto l: lines)
				{
					int ws = 0;
					for (int i=0; i<l.Length(); i++)
						if (IsWhite(l(i))) ws++;
						else break;
					if (l.Find(key)>=0)
						inKey = ws;
					else if (inKey>=0)
					{
						if (ws>inKey) { Configs.New() = l.Strip(); }
						else { inKey = -1; }
					}
					// printf("%s\n", l.Get());
				}
			}
			
			auto Config = Configs.Length() > 0 ? Configs[0] : LString("Debug");
			TmpArgs.Printf("-project \"%s\" -configuration %s", Makefile.Get(), Config.Get());
		}
		else
		{
			if (Compiler == Cygwin)
			{
				LFile::Path p(CygwinPath, "bin");
				Path = p.GetFull();
			}

			if (Compiler == MingW)
			{
				LString a;
				char *Dir = strrchr(MakePath, DIR_CHAR);
				#if 1
				TmpArgs.Printf("/C \"%s\"", Exe.Get());

				
				/*	As of MSYS v1.0.18 the support for multiple jobs causes make to hang:
					http://sourceforge.net/p/mingw/bugs/1950/
					http://mingw-users.1079350.n2.nabble.com/MSYS-make-freezes-td7579038.html
					
					Apparently it'll be "fixed" in v1.0.19. We'll see. >:-(
				
				if (Jobs.CastInt32() > 1)
					a.Print(" -j %i", Jobs.CastInt32());
				
				*/
				
				a.Printf(" -f \"%s\"", Dir ? Dir + 1 : MakePath.Get());
				TmpArgs += a;
				#else
				TmpArgs.Printf("/C set");
				#endif
				Exe = "C:\\Windows\\System32\\cmd.exe";
			}
			else
			{
				if (Jobs.CastInt32())
					TmpArgs.Printf("-j %i -f \"%s\"", Jobs.CastInt32(), MakePath.Get());
				else
					TmpArgs.Printf("-f \"%s\"", MakePath.Get());
			}

			if (Clean)
            {
				if (All)
					TmpArgs += " cleanall";
				else
					TmpArgs += " clean";
            }
			if (Config == BuildRelease)
				TmpArgs += " Build=Release";
		}

		PostThreadEvent(AppHnd, M_SELECT_TAB, AppWnd::BuildTab);

		LString Msg;
		Proj->GetApp()->PostEvent(M_APPEND_TEXT, (LMessage::Param)NewStr(Msg), AppWnd::BuildTab);

		Print("Making: %s (%s)\n", MakePath.Get(), TmpArgs.Get());
		if (auto backend = Proj->GetBackend())
		{
			LString sep = MakePath.Find("\\") >= 0 ? "\\" : "/";
			auto parts = MakePath.SplitDelimit(sep);
			backendPaths.SetFixedLength(false);
			for (int i = 2; i < parts.Length(); i++)
				backendPaths.New() = sep.Join(parts.Slice(0, i));
			backendArgs = TmpArgs;
			AddWork([this]() { Step1(); });
			
			StreamToLog log(Proj->GetApp());
			uint64_t msgTs = LCurrentTime();
			int calls = 0;
			while
			(
				(!backendExitCode && !IsCancelled())
				||
				calls > 0 // Don't exit till the backend has finished it's callbacks
			)
			{
				if (backendLock.LockWithTimeout(100, _FL))
				{
					std::function<void()> cb;
					calls = backendCalls.Length();
					if (backendWork.Length() > 0)
					{
						cb.swap(backendWork[0]);
						backendWork.DeleteAt(0);
					}
					backendLock.Unlock();
					if (cb)
						cb();
				}

				LSleep(10);

				uint64_t now = LCurrentTime();
				if (now - msgTs > 1000)
				{
					msgTs = now;

					if (calls > 0 && backendExitCode)
					{
						// This state shouldn't persist for long right?
						LMutex::Auto lck(&backendLock, _FL);
						log.Print("Have exit code but still outstanding calls:\n");
						for (auto p: backendCalls)
							log.Print("\tcall: %s:%i\n", p.value.file, p.value.line);
					}

					// log.Print("%s: backendCalls=%i\n", __FUNCTION__, backendCalls);
				}
			}

			log.Print("%s: build exit code: %i\n", __FUNCTION__, backendExitCode ? *backendExitCode : -1);
		}
		else 
		{
			// local build...
			if (SubProc.Reset(new LSubProcess(Exe, TmpArgs)))
			{
				SubProc->SetNewGroup(false);
				SubProc->SetInitFolder(InitDir);
				if (Include)
					SubProc->SetEnvironment("INCLUDE", Include);
				if (Lib)
					SubProc->SetEnvironment("LIB", Lib);
				if (LibPath)
					SubProc->SetEnvironment("LIBPATHS", LibPath);
				if (Path)
				{
					LString Cur = getenv("PATH");
					LString New = Cur + LGI_PATH_SEPARATOR + Path;
					SubProc->SetEnvironment("PATH", New);
				}
				// SubProc->SetEnvironment("DLL", "1");

				if (Compiler == MingW)
					SubProc->SetEnvironment("PATH", "c:\\MingW\\bin;C:\\MinGW\\msys\\1.0\\bin;%PATH%");
				
				if ((Status = SubProc->Start(true, false)))
				{
					// Read all the output					
					char Buf[256];
					ssize_t rd;

					while ((rd = SubProc->Read(Buf, sizeof(Buf))) > 0 )
					{
						if (IsCancelled())
						{
							SubProc->Interrupt();
							break;
						}

						Write(Buf, rd);
					}
					
					uint32_t ex = SubProc->Wait();
					Print("Make exited with %i (0x%x)\n", ex, ex);

					if (Compiler == IAR &&
						ex == 0 &&
						PostBuild.Length())
					{
						for (auto Cmd : PostBuild)
						{
							auto p = Cmd.Split(" ", 1);
							if (p[0].Equals("cd"))
							{
								if (p.Length() > 1)
									FileDev->SetCurrentFolder(p[1]);
								else
									LAssert(!"No folder for cd?");
							}
							else
							{
								LSubProcess PostCmd(p[0], p.Length() > 1 ? p[1] : LString());
								if (PostCmd.Start(true, false))
								{
									char Buf[256];
									ssize_t rd;
									while ( (rd = PostCmd.Read(Buf, sizeof(Buf))) > 0 )
									{
										Write(Buf, rd);
									}
								}
							}
						}
					}
				}
				else
				{
					// Create a nice error message.
					LString ErrStr = LErrorCodeToString(SubProc->GetErrorCode());
					if (ErrStr)
					{
						char *e = ErrStr.Get() + ErrStr.Length();
						while (e > ErrStr && strchr(" \t\r\n.", e[-1]))
							*(--e) = 0;
					}
					
					sprintf_s(ErrBuf, sizeof(ErrBuf), "Running make failed with %i (%s)\n",
						SubProc->GetErrorCode(),
						ErrStr.Get());
					Err = ErrBuf;
				}
			}
		}
	}
	else
	{
		Err = "Couldn't find program to build makefile.";
		LgiTrace("%s,%i - %s.\n", _FL, Err);
	}

	AppWnd *w = Proj->GetApp();
	if (w)
	{
		w->PostEvent(M_BUILD_DONE);
		if (Err)
			Proj->GetApp()->PostEvent(M_BUILD_ERR, 0, (LMessage::Param)NewStr(Err));
	}
	else LAssert(0);
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////
IdeProject::IdeProject(AppWnd *App, ProjectNode *DepParent) : IdeCommon(NULL)
{
	Project = this;
	d = new IdeProjectPrivate(App, this, DepParent);
	Tag = NewStr("Project");

	bpStoreCb = App->GetBreakPointStore()->AddCallback([this](auto event, auto id)
		{
			SetUserFileDirty();
		});
}

IdeProject::~IdeProject()
{
	d->App->GetBreakPointStore()->DeleteCallback(bpStoreCb);

	d->App->OnProjectDestroy(this);
	LXmlTag::Empty(true);
	while (Items.Length())
		delete Items[0];
	DeleteObj(d);
}

bool IdeProject::OnNode(const char *Path, ProjectNode *Node, bool Add)
{
	if (!Path || !Node)
	{
		LAssert(0);
		return false;
	}

	char Full[MAX_PATH_LEN] = "";
	LString absPath;
	if (LIsRelativePath(Path))
	{
		if (d->Backend)
		{
		}
		else
		{
			auto Base = GetBasePath();
			if (LMakePath(Full, sizeof(Full), Base, Path))
				Path = Full;
		}
	}

	bool Status = false;
	if (Add)
		Status = d->Nodes.Add(Path, Node);
	else
		Status = d->Nodes.Delete(Path);
	
	LString p = Path;
	if (Status && CheckExists(p))
		d->App->OnNode(p, Node, Add ? FindSymbolSystem::FileAdd : FindSymbolSystem::FileRemove);

	return Status;
}

void IdeProject::ShowFileProperties(const char *File)
{
	ProjectNode *Node = NULL;
	auto full = FindFullPath(File, &Node);
	if (Node)
		Node->OnProperties();
}

const char *IdeProject::GetFileComment()
{
	return d->Settings.GetStr(ProjCommentFile);
}

const char *IdeProject::GetFunctionComment()
{
	return d->Settings.GetStr(ProjCommentFunction);
}

IdeProject *IdeProject::GetParentProject()
{
	return d->ParentProject;
}

void IdeProject::SetParentProject(IdeProject *p)
{
	d->ParentProject = p;
}

LArray<IdeProject*> IdeProject::GetAllProjects()
{
	LArray<IdeProject*> a;
	a.Add(this);
	GetChildProjects(a);
	return a;	
}

bool IdeProject::GetChildProjects(LArray<IdeProject*> &c)
{
	CollectAllSubProjects(c);
	return c.Length() > 0;
}

bool IdeProject::RelativePath(LString &Out, const char *In, bool Debug)
{
	if (!In)
		return false;

	auto Base = GetBasePath();
	if (!Base)
		return false;

	CheckExists(Base);
if (Debug) LgiTrace("XmlBase='%s'\n		In='%s'\n", Base.Get(), In);
		
	LToken b(Base, DIR_STR);
	LToken i(In, DIR_STR);
	char out[MAX_PATH_LEN] = "";
	int outCh = 0;
	
if (Debug) LgiTrace("Len %i-%i\n", b.Length(), i.Length());
	
	auto ILen = i.Length() + (LDirExists(In) ? 0 : 1);
	auto Max = MIN(b.Length(), ILen);
	int Common = 0;
	for (; Common < Max; Common++)
	{
		#ifdef WIN32
		#define StrCompare stricmp
		#else
		#define StrCompare strcmp
		#endif
		
if (Debug) LgiTrace("Cmd '%s'-'%s'\n", b[Common], i[Common]);
		if (StrCompare(b[Common], i[Common]) != 0)
		{
			break;
		}
	}
	
if (Debug) LgiTrace("Common=%i\n", Common);
	if (Common > 0)
	{
		if (Common < b.Length())
		{
			out[outCh = 0] = 0;
			auto Back = b.Length() - Common;
if (Debug) LgiTrace("Back=%i\n", (int)Back);
			for (int n=0; n<Back; n++)
			{
				if (outCh)
					outCh += snprintf(out+outCh, sizeof(out)-outCh, DIR_STR);
				outCh += snprintf(out+outCh, sizeof(out)-outCh, "..");
			}
		}
		else
		{
			outCh += snprintf(out+outCh, sizeof(out)-outCh, ".");
		}
		for (int n=Common; n<i.Length(); n++)
		{
			outCh += snprintf(out+outCh, sizeof(out)-outCh, "%s%s", DIR_STR, i[n]);
		}
		
		Out = out;
	}	
	else
	{
		Out = In;
	}		

	CheckExists(Out);
	return true;
}

void IdeProject::GetExePath(std::function<void(LString,SysPlatform)> cb)
{
	if (!cb)
		return;
	if (d->Backend)
	{
		d->Backend->GetSysType(
			[this, cb](auto Platform)
			{
				LString exe = d->Settings.GetStr(ProjExe, NULL, Platform);

				if (LIsRelativePath(exe))
				{
					auto base = d->Backend->GetBasePath();
					exe = d->Backend->JoinPath(base, exe);
				}

				if (cb)
					cb(exe, Platform);
			});
		return;
	}

	auto platform = GetCurrentPlatform();
	auto PExe = d->Settings.GetStr(ProjExe, NULL, platform);
	if (!PExe)
	{
		// Use the default exe name?
		if (auto Exe = GetExecutable(platform))
			PExe = Exe;
	}	
	if (!PExe)
		return;

	if (LIsRelativePath(PExe))
	{
		auto Base = GetBasePath();
		if (!Base)
			return;

		LFile::Path p(Base);
		cb((p / PExe).GetFull(), platform);
	}
	else
	{
		cb(PExe, platform);
	}
}

bool IdeProject::GetExePath(char *Path, int Len)
{
	auto PExe = d->Settings.GetStr(ProjExe);
	LString Exe;
	if (!PExe)
	{
		// Use the default exe name?
		Exe = GetExecutable(GetCurrentPlatform());
		if (Exe)
		{
			printf("Exe='%s'\n", Exe.Get());
			PExe = Exe;
		}
	}
	
	if (!PExe)
		return false;	
	
	if (LIsRelativePath(PExe))
	{
		auto Base = GetBasePath();
		if (Base)
			LMakePath(Path, Len, Base, PExe);
		else
			return false;
	}
	else
	{
		strcpy_s(Path, Len, PExe);
	}
	
	return true;
}

LString IdeProject::GetMakefile(SysPlatform Platform)
{
	const char *PMakefile = d->Settings.GetStr(ProjMakefile, NULL, Platform);
	if (!PMakefile)
		return LString();
	
	LString Path;
	if (d->Backend)
	{
		auto base = d->Backend->GetBasePath();
		if (base)
		{
			auto sep = strchr(base, '\\') ? "\\" : "/";
			auto joinParts = base(-1) == sep[0] ? "" : sep;
			Path = base + joinParts + PMakefile;

			auto cur = LString(sep) + "." + sep;
			Path = Path.Replace(cur, sep);
		}
		else
		{
			Path = PMakefile;
		}
	}
	else
	{
		if (LIsRelativePath(PMakefile))
		{
			auto Base = GetBasePath();
			if (Base)
			{
				char p[MAX_PATH_LEN];
				LMakePath(p, sizeof(p), Base, PMakefile);
				Path = p;
			}
		}
		else
		{
			Path = PMakefile;
		}
	}
	
	return Path;
}

void IdeProject::Clean(bool All, BuildConfig Config)
{
	if (d->Backend)
	{
		d->Backend->GetSysType([this, All, Config](auto Platform)
		{
			CleanForPlatform(All, Config, Platform);
		});
	}
	else CleanForPlatform(All, Config, PlatformCurrent);
}

void IdeProject::CleanForPlatform(bool All, BuildConfig Config, SysPlatform Platform)
{
	if (!d->Thread &&
		d->Settings.GetStr(ProjMakefile, NULL, Platform))
	{
		if (auto m = GetMakefile(Platform))
		{
			CheckExists(m);
			d->Thread.Reset(new BuildThread(this, m, true, Config, Platform, All, sizeof(ssize_t)*8));
		}
	}
}

char *QuoteStr(char *s)
{
	LStringPipe p(256);
	while (s && *s)
	{
		if (*s == ' ')
		{
			p.Push("\\ ", 2);
		}
		else p.Push(s, 1);
		s++;
	}
	return p.NewStr();
}

class ExecuteThread : public LThread, public LStream, public LCancel
{
	IdeProject *Proj = NULL;
	LString Exe, Args, Path;
	int Len = 32 << 10;
	ExeAction Act;
	int AppHnd;

public:
	ExecuteThread(IdeProject *proj, const char *exe, const char *args, char *path, ExeAction act) :
		LThread("ExecuteThread")
	{
		Proj = proj;
		Act = act;
		Exe = exe;
		Args = args;
		Path = path;
		DeleteOnExit = true;
		AppHnd = proj->GetApp()->AddDispatch();
		
		Run();
	}
	
	~ExecuteThread()
	{
		Cancel();
		while (!IsExited())
			LSleep(1);
	}
	
	int Main() override
	{
		PostThreadEvent(AppHnd, M_SELECT_TAB, AppWnd::OutputTab);
		PostThreadEvent(AppHnd, M_APPEND_TEXT, 0, AppWnd::OutputTab);
		
		if (Exe)
		{
			if (Act == ExeDebug)
			{
				LSubProcess sub("kdbg", Exe);
				if (Path)
					sub.SetInitFolder(Path);
				if (sub.Start())
					sub.Communicate(this, NULL, this);
			}
			else if (Act == ExeValgrind)
			{
				#ifdef LINUX
				LString ExePath = Proj->GetExecutable(GetCurrentPlatform());
				if (ExePath)
				{
					char Path[MAX_PATH_LEN];
					char *ExeLeaf = LGetLeaf(Exe);
					strcpy_s(Path, sizeof(Path), ExeLeaf ? ExeLeaf : Exe.Get());
					LTrimDir(Path);
									
					const char *Term = NULL;
					const char *WorkDir = NULL;
					const char *Execute = NULL;
					switch (LGetWindowManager())
					{
						case WM_Kde:
							Term = "konsole";
							WorkDir = "--workdir ";
							Execute = "-e";
							break;
						case WM_Gnome:
							Term = "gnome-terminal";
							WorkDir = "--working-directory=";
							Execute = "-x";
							break;
					}
					
					if (Term && WorkDir && Execute)
					{					
						char *e = QuoteStr(ExePath);
						char *p = QuoteStr(Path);
						const char *a = Proj->GetExeArgs() ? Proj->GetExeArgs() : "";
						char Args[512];
						sprintf(Args,
								"%s%s "
								"--noclose "
								"%s valgrind --tool=memcheck --num-callers=20 %s %s",
								WorkDir,
								p,
								Execute,
								e, a);
								
						LExecute(Term, Args);
					}
				}
				#endif
			}
			else
			{
				LSubProcess sub(Exe, Args);
				if (Path)
					sub.SetInitFolder(Path);
				if (sub.Start())
					sub.Communicate(this, NULL, this);
			}
		}
		
		return 0;
	}

	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override
	{
		if (Len <= 0)
			return 0;

		PostThreadEvent(AppHnd, M_APPEND_TEXT, (LMessage::Param)NewStr((char*)Buffer, Size), AppWnd::OutputTab);
		Len -= Size;
		return Size;
	}
};

void IdeProject::Execute(ExeAction Act, std::function<void(LError&, LDebugContext*)> cb)
{
	LString Base;
	if (d->Backend)
		Base = d->Backend->GetBasePath();
	else if (auto s = GetBasePath())
		Base = s.Get();
	if (!Base)
 	{
		if (cb)
		{
			LError err(LErrorPathNotFound, "No base path for project.");
			cb(err, NULL);
		}
		return;
	}
	
	GetExePath([this, Act, Base, cb](auto e, auto platform)
	{
		LAssert(d->App->InThread());

		if (!d->Backend && !LFileExists(e))
		{
			if (cb)
			{
				LError err(LErrorPathNotFound, LString::Fmt("Executable '%s' doesn't exist.\n", e.Get()));
				cb(err, NULL);
			}
			return;
		}

		auto Args = d->Settings.GetStr(ProjArgs, NULL, platform);
		auto Env = d->Settings.GetStr(ProjEnv, NULL, platform);
		LString InitDir = d->Settings.GetStr(ProjInitDir, NULL, platform);
		auto RunAsAdmin = d->Settings.GetInt(ProjDebugAdmin, 0, platform);
	
		if (Act == ExeDebug)
		{
			if (InitDir && LIsRelativePath(InitDir))
			{
				LFile::Path p(Base);
				p += InitDir;
				InitDir = p.GetFull();
			}
			
			if (cb)
			{
				LError err(LErrorNone);
				cb(err, new LDebugContext(d->App, this, platform, e, Args, RunAsAdmin != 0, Env, InitDir));
			}
		}
		else
		{
			new ExecuteThread(	this,
								e,
								Args,
								Base,
								#if defined(HAIKU) || defined(WINDOWS)
									ExeRun // No gdb or valgrind
								#else
									Act
								#endif
								);
		}
	});
}

bool IdeProject::IsMakefileAScript()
{
	auto m = GetMakefile(PlatformCurrent);
	if (m)
	{
		auto Ext = LGetExtension(m);
		if (!Stricmp(Ext, "py"))
		{
			// Not a makefile but a build script... can't update.
			return true;
		}
	}

	return false;
}

bool IdeProject::IsMakefileUpToDate()
{
	if (IsMakefileAScript())
		return true; // We can't know if it's up to date...

	auto Proj = GetAllProjects();
	for (auto p: Proj)
	{
		// Is the project file modified after the makefile?
		auto Proj = p->GetFullPath();
		uint64 ProjModTime = 0, MakeModTime = 0;
		LDirectory dir;
		if (dir.First(Proj))
		{
			ProjModTime = dir.GetLastWriteTime();
			dir.Close();
		}

		auto m = p->GetMakefile(PlatformCurrent);
		if (!m)
		{		
			d->App->GetBuildLog()->Print("Error: no makefile? (%s:%i)\n", _FL);
			break;
		}
		
		auto Ext = LGetExtension(m);
		if (!Stricmp(Ext, "py"))
		{
			// Not a makefile but a build script... can't update.
			return true;
		}

		if (dir.First(m))
		{
			MakeModTime = dir.GetLastWriteTime();
			dir.Close();
		}

		#if 0
		printf("IsMakefileUpToDate: Proj=%s - Timestamps " LPrintfInt64 " - " LPrintfInt64 "\n",
			Proj.Get(),
			ProjModTime,
			MakeModTime);
		#endif
			
		if (ProjModTime != 0 &&
			MakeModTime != 0 &&
			ProjModTime > MakeModTime)
		{
			// Need to rebuild the makefile...
			printf("Out of date.\n");
			return false;
		}
	}
	
	return true;
}

bool IdeProject::FindDuplicateSymbols()
{
	LStream *Log = d->App->GetBuildLog();	
	Log->Print("FindDuplicateSymbols starting...\n");

	LArray<IdeProject*> Proj;
	CollectAllSubProjects(Proj);
	Proj.Add(this);

	int Lines = 0;
	
	LHashTbl<StrKey<char,false>,int64> Map(200000);
	int Found = 0;
	for (auto p: Proj)
	{
		LString s = p->GetExecutable(GetCurrentPlatform());
		if (s)
		{
			LString Args;
			Args.Printf("--print-size --defined-only -C %s", s.Get());
			LSubProcess Nm("nm", Args);
			if (Nm.Start(true, false))
			{
				char Buf[256];
				LStringPipe q;
				for (ssize_t Rd = 0; (Rd = Nm.Read(Buf, sizeof(Buf))); )
					q.Write(Buf, Rd);
				LString::Array a = q.NewLStr().SplitDelimit("\r\n");
				LHashTbl<StrKey<char,false>,bool> Local(200000);
				for (auto &Ln: a)
				{
					LString::Array p = Ln.SplitDelimit(" \t", 3);
					if (!Local.Find(p.Last()))
					{
						Local.Add(p.Last(), true);
					
						// const char *Sz = p[1];
						int64 Ours = p[1].Int(16);
						int64 Theirs = Map.Find(p.Last());
						if (Theirs >= 0)
						{
							if (Ours != Theirs)
							{
								if (Found++ < 100)
									Log->Print("    %s (" LPrintfInt64 " -> " LPrintfInt64 ")\n",
										p.Last().Get(),
										Ours, Theirs);
							}
						}
						else if (Ours >= 0)
						{						
							Map.Add(p.Last(), Ours);
						}
						else
						{
							printf("Bad line: %s\n", Ln.Get());
						}
					}
					Lines++;
				}
			}
		}
		else printf("%s:%i - GetExecutable failed.\n", _FL);
	}

	/*
	char *Sym;
	for (int Count = Map.First(&Sym); Count; Count = Map.Next(&Sym))
	{
		if (Count > 1)
			Log->Print("    %i: %s\n", Count, Sym);
	}
	*/

	Log->Print("FindDuplicateSymbols finished (%i lines)\n", Lines);
	return false;
}

bool IdeProject::FixMissingFiles()
{
	FixMissingFilesDlg(this);
	return true;
}

void IdeProject::Build(bool All, BuildConfig Config)
{
	if (d->Thread)
	{
		d->App->GetBuildLog()->Print("Error: Already building (%s:%i)\n", _FL);
		return;
	}

	if (d->Backend)
	{
		d->Backend->GetSysType([this, All, Config](auto platform)
		{
			BuildForPlatform(All, Config, platform);
		});
	}
	else BuildForPlatform(All, Config, PlatformCurrent);
}

void IdeProject::BuildForPlatform(bool All, BuildConfig Config, SysPlatform Platform)
{
	auto m = GetMakefile(Platform);
	CheckExists(m);
	if (!m)
	{
		d->App->GetBuildLog()->Print("Error: no makefile? (%s:%i)\n", _FL);
		return;
	}

	// Clear the build tab log...
	if (GetApp())
		GetApp()->PostEvent(M_APPEND_TEXT, 0, AppWnd::BuildTab);

	SetClean([this, m, All, Config, Platform](bool ok)
	{
		if (!ok)
			return;

		if (!IsMakefileUpToDate())
		{
			CreateMakefile(Platform, true);
		}
		else
		{
			// Start the build thread...
			d->Thread.Reset
			(
				new BuildThread
				(
					this,
					m,
					false,
					Config,
					Platform,
					All,
					sizeof(size_t)*8
				)
			);
		}
	});
}

void IdeProject::BuildThreadFinished()
{
	// Now clean up the thread..
	d->Thread.Reset();
}

void IdeProject::StopBuild()
{
	if (d->Thread && !d->Thread->IsCancelled())
	{
		// When canceling the build thread, we should keep the message loop 
		// running at all times... and not block
		//
		// Tell it we want to finish:
		d->Thread->Cancel();
	}
}

bool IdeProject::Serialize(bool Write)
{
	return true;
}

AppWnd *IdeProject::GetApp()
{
	return d->App;
}

const char *IdeProject::GetIncludePaths()
{
	return d->Settings.GetStr(ProjIncludePaths);
}

const char *IdeProject::GetPreDefinedValues()
{
	return d->Settings.GetStr(ProjDefines);
}

const char *IdeProject::GetCompileOptions()
{
	return d->Settings.GetStr(ProjCompileOptions);
}

const char *IdeProject::GetExeArgs()
{
	return d->Settings.GetStr(ProjArgs);
}

LString IdeProject::GetExecutable(SysPlatform Platform)
{
	LString Bin = d->Settings.GetStr(ProjExe, NULL, Platform);
	auto TargetType = d->Settings.GetStr(ProjTargetType, NULL, Platform);
	auto Base = GetBasePath();

	if (Bin)
	{
		if (LIsRelativePath(Bin) && Base)
		{
			char p[MAX_PATH_LEN];
			if (LMakePath(p, sizeof(p), Base, Bin))
				Bin = p;
		}
		
		return Bin;
	}

	// Create binary name from target:	
	auto Target = GetTargetName(Platform);
	if (Target)
	{
		bool IsLibrary = Stristr(TargetType, "library");
		int BuildMode = d->App->GetBuildMode();
		const char *Name = BuildMode ? "Release" : "Debug";
		const char *Postfix = BuildMode ? "" : "d";
		
		switch (Platform)
		{			
			case PlatformWin:
			{
				if (IsLibrary)
					Bin.Printf("%s%s.dll", Target.Get(), Postfix);
				else
					Bin = Target;
				break;
			}
			case PlatformMac:
			{
				if (IsLibrary)
					Bin.Printf("lib%s%s.dylib", Target.Get(), Postfix);
				else
					Bin = Target;
				break;
			}
			case PlatformLinux:
			case PlatformHaiku:
			{
				if (IsLibrary)
					Bin.Printf("lib%s%s.so", Target.Get(), Postfix);
				else
					Bin = Target;
				break;
			}
			default:
			{
				LAssert(0);
				printf("%s:%i - Unknown platform.\n", _FL);
				return LString();
			}
		}
		
		// Find the actual file...
		if (!Base)
		{
			printf("%s:%i - GetBasePath failed.\n", _FL);
			return LString();
		}
			
		char Path[MAX_PATH_LEN];
		LMakePath(Path, sizeof(Path), Base, Name);
		LMakePath(Path, sizeof(Path), Path, Bin);
		Bin = Path; // This doesn't need to exist
		
		return Bin;
	}
	
	return LString();
}

const char *IdeProject::GetFileName()
{
	return d->FileName;
}

int IdeProject::GetPlatforms()
{
	return PLATFORM_ALL;
}
	
LAutoString IdeProject::GetFullPath()
{
	LAutoString Status;
	if (!d->FileName)
	{
		// LAssert(!"No path.");
		return Status;
	}

	LArray<const char*> sections;
	IdeProject *proj = this;
	while (	proj &&
			proj->GetFileName() &&
			LIsRelativePath(proj->GetFileName()))
	{
		sections.AddAt(0, proj->GetFileName());
		proj = proj->GetParentProject();
	}

	if (!proj)
	{
		// LAssert(!"All projects have a relative path?");
		return Status; // No absolute path in the parent projects?
	}

	char p[MAX_PATH_LEN];
	strcpy_s(p, sizeof(p), proj->GetFileName()); // Copy the base path
	if (sections.Length() > 0)
		LTrimDir(p); // Trim off the filename
	for (int i=0; i<sections.Length(); i++) // For each relative section
	{
		LMakePath(p, sizeof(p), p, sections[i]); // Append relative path
		if (i < sections.Length() - 1)
			LTrimDir(p); // Trim filename off
	}
	
	Status.Reset(NewStr(p));
	return Status;
}

LAutoString IdeProject::GetBasePath()
{
	auto a = GetFullPath();
	LTrimDir(a);
	return a;
}

void IdeProject::CreateProject()
{
	Empty();
	
	d->FileName.Empty();
	d->UserFile.Empty();
	d->App->GetTree()->Insert(this);
	
	ProjectNode *f = new ProjectNode(this);
	if (f)
	{
		f->SetName("Source");
		f->SetType(NodeDir);
		InsertTag(f);
	}
	
	f = new ProjectNode(this);
	if (f)
	{
		f->SetName("Headers");
		f->SetType(NodeDir);
		InsertTag(f);
	}
	
	d->Settings.Set(ProjEditorTabSize, 4);
	d->Settings.Set(ProjEditorIndentSize, 4);
	d->Settings.Set(ProjEditorUseHardTabs, true);
	
	d->Dirty = true;
	Expanded(true);	
}

ProjectStatus IdeProject::OpenFile(const char *FileName)
{
	auto Log = d->App->GetBuildLog();

	LProfile Prof("IdeProject::OpenFile");
	Prof.HideResultsIfBelow(1000);

	Empty();

	Prof.Add("Init");

	d->EmptyUserData();
	if (LIsRelativePath(FileName))
	{
		char p[MAX_PATH_LEN];
		getcwd(p, sizeof(p));
		LMakePath(p, sizeof(p), p, FileName);
		d->FileName = p;
	}
	else
	{
		d->FileName = FileName;
	}
	d->UserFile = d->FileName + "." + LCurrentUserName();

	if (!d->FileName)
	{
		Log->Print("%s:%i - No filename.\n", _FL);
		return OpenError;
	}
	
	Prof.Add("FileOpen");

	LFile f;
	LString FullPath = d->FileName.Get();
	for (unsigned attempt = 0; attempt < 5; attempt++)
	{
		if (!CheckExists(FullPath) ||
			!f.Open(FullPath, O_READWRITE))
		{
			Log->Print("%s:%i - Retrying '%s', attempt %i.\n", _FL, FullPath.Get(), attempt+1);
			LSleep(10);
			continue;
		}
		else break;
	}
	if (!f.IsOpen())
	{
		Log->Print("%s:%i - Error: Can't open '%s'.\n", _FL, FullPath.Get());
		return OpenError;
	}

	Prof.Add("Xml");

	LXmlTree x;
	LXmlTag r;
	if (!x.Read(&r, &f))
	{
		Log->Print("%s:%i - Error: Can't read XML: %s\n", _FL, x.GetErrorMsg());
		return OpenError;
	}

	Prof.Add("Progress Setup");

	#if DEBUG_OPEN_PROGRESS
	int64 Nodes = r.CountTags();
	LProgressDlg Prog(d->App, 1000);
	Prog.SetDescription("Loading project...");
	Prog.SetLimits(0, Nodes);
	Prog.SetYieldTime(1000);
	Prog.SetAlwaysOnTop(true);
	#endif

	Prof.Add("UserFile");

	if (LFileExists(d->UserFile))
	{
		LFile Uf;
		if (!Uf.Open(d->UserFile, O_READ))
			LgiTrace("%s:%i failed to open '%s' for reading.\n", _FL, d->UserFile.Get());
		else
		{
			LString::Array lines = Uf.Read().SplitDelimit("\n");
			for (auto &ln: lines)
			{
				LString var, value;
				auto hasColon = ln.Find(":");
				if (hasColon > 0)
				{
					auto p = ln.SplitDelimit(":", 1);
					var = p[0];
					value = p[1];
				}
				else value = ln;
				
				auto p = value.SplitDelimit(",");
				if (!var || var.Equals(OPT_NodeFlags))
				{
					if (p.Length() == 2)
						d->UserNodeFlags.Add((int)p[0].Int(), (int)p[1].Int(16));
					else
						LAssert(!"Wrong length");
				}
				else if (var.Equals(OPT_Breakpoint))
				{
					BreakPoint bp;
					if (bp.Load(value))
					{
						if (auto store = d->App->GetBreakPointStore())
						{
							if (auto id = store->Add(bp))
							{
								d->UserBreakpoints.Add(id);
							}
							else LAssert(0);
						}
						else LAssert(0);
					}
					else LAssert(0);
				}
				else LgiTrace("%s:%i - Unknown user file line: '%s'\n", _FL, ln.Get());
			}

			#if 0
			LgiTrace("%s:%i Loaded %i,%i user project lines.\n",
				_FL, (int)d->UserNodeFlags.Length(), (int)d->UserBreakpoints.Length());
			#endif
		}
	}
	
	if (!r.IsTag("Project"))
	{
		Log->Print("%s:%i - No 'Project' tag.\n", _FL);
		return OpenError;
	}

	Prof.Add("Serialize");

	d->Settings.Serialize(&r, false /* read */);

	Prof.Add("IncludePathProcessing");

	LString::Array Inc, Sys;
	auto plat = PlatformFlagsToEnum(d->App->GetPlatform());
	BuildIncludePaths(Inc, &Sys, true, true, plat);
	d->App->GetFindSym()->SetIncludePaths(Inc, Sys, d->App->GetPlatform());

	Prof.Add("OnOpen");

	bool Ok = OnOpen(
		#if DEBUG_OPEN_PROGRESS
		&Prog,
		#else
		NULL,
		#endif
		&r);
	#if DEBUG_OPEN_PROGRESS
	if (Prog.IsCancelled())
		return OpenCancel;
	else 
	#endif
	if (!Ok)
		return OpenError;

	Prof.Add("Insert");

	d->App->GetTree()->Insert(this);
	Expanded(true);

	auto Uri = d->Settings.GetStr(ProjRemoteUri);
	auto Pass = d->Settings.GetStr(ProjRemotePass);
	if (Uri && !d->Backend)
	{
		LString cache;
		if (Pass)
		{
			// Rewrite the URI to include the password
			LUri u(Uri);
			u.sPass = Pass;
			Uri = cache = u.ToString();
		}

		d->Backend = CreateSystemInterface(d->App, Uri, d->App->GetNetworkLog());
		if (d->Backend)
		{
			auto path = d->Backend->GetBasePath();
			d->Backend->ReadFolder(SystemIntf::TForeground, path, [this](auto d)
				{
					for (auto b = d->First(NULL); b; b = d->Next())
					{
						if (d->IsHidden())
							continue;

						/*
						LgiTrace("%s: %s, %i bytes\n",	
							d->IsDir() ? "dir" : "file",
							d->GetName(), (int)d->GetSize());
						*/

						if (auto n = new ProjectNode(this))
						{
							if (n->Serialize(d))
								Insert(n);
							else
								delete n;
						}
					}
				});
		}
	}

	return OpenOk;
}

SystemIntf *IdeProject::GetBackend()
{
	return d->Backend;
}

LString IdeProject::GetBuildFolder() const
{
	return d->BuildFolder;
}

void IdeProject::SetBuildFolder(LString folder)
{
	d->BuildFolder = folder;
}

bool IdeProject::SaveFile()
{
	auto Full = GetFullPath();

	if (ValidStr(Full) && d->Dirty)
	{
		LFile f;
		if (f.Open(Full, O_WRITE))
		{
			f.SetSize(0);

			LXmlTree x;
			d->Settings.Serialize(this, true /* write */);

			if (x.Write(this, &f))
				d->Dirty = false;
			else
				LgiTrace("%s:%i - Failed to write XML.\n", _FL);
		}
		else LgiTrace("%s:%i - Couldn't open '%s' for writing.\n", _FL, Full.Get());
	}

	if (d->UserFileDirty)
	{
		LFile f;
		LAssert(d->UserFile.Get());
		if (f.Open(d->UserFile, O_WRITE))
		{
			f.SetSize(0);

			// Save user file details..
			for (auto i: d->UserNodeFlags)
				f.Print("%s:%i,%x\n", OPT_NodeFlags, i.key, i.value);
			
			if (auto store = d->App->GetBreakPointStore())
			{
				for (auto id: d->UserBreakpoints)
				{
					auto bp = store->Get(id);
					if (bp)
						f.Print("%s:%s\n", OPT_Breakpoint, bp.Save().Get());
				}
			}
			else LAssert(0);

			d->UserFileDirty = false;
			#if 0
			LgiTrace("%s:%i Saved %i,%i user project lines.\n",
				_FL, (int)d->UserNodeFlags.Length(), (int)d->UserBreakpoints.Length());
			#endif
		}
	}

	return	!d->Dirty &&
			!d->UserFileDirty;
}

bool IdeProject::LoadBreakPoints(IdeDoc *doc)
{
	if (!doc)
		return false;

	LString fn = doc->GetFileName();
	if (d->Backend && (!LIsRelativePath(fn) || fn(0) == '~'))
	{
		// Convert the path to a relative one
		auto base = d->Backend->GetBasePath();
		if (auto rel = d->Backend->MakeRelative(fn))
			fn = rel.Replace("\\", "/");
	}

	auto fnLeaf = LGetLeaf(fn);
	LString::Array msg;
	msg.SetFixedLength(false);

	if (auto store = d->App->GetBreakPointStore())
	{
		for (auto id: d->UserBreakpoints)
		{
			auto bp = store->Get(id);
			bool sameLeaf = !Stricmp(LGetLeaf(bp.File), fnLeaf);
			LString normalized = bp.File;

			if (d->Backend)
			{
				if (auto rel = d->Backend->MakeRelative(normalized))
					normalized = rel;
			}

			if (normalized.Equals(fn))
			{
				doc->OnBreakPoint(bp, true);
				return true;
			}

			if (sameLeaf)
				msg.New().Printf("LoadBreakPoints: matching leaf: bp.File=%s, fn=%s\n", normalized.Get(), fn.Get());
		}
	}

	if (msg.Length())
	{
		for (auto m: msg)
			LgiTrace("%s", m.Get());
		LAssert(!"Failed to add break point with matching leaf?");
	}
	return false;
}

void IdeProject::SetDirty()
{
	d->Dirty = true;
	d->App->OnProjectChange();
}

void IdeProject::SetUserFileDirty()
{
	if (!d->UserFileDirty)
	{
		d->UserFileDirty = true;
		LgiTrace("%s:%i UserFileDirty is dirty.\n", _FL);
	}
}

bool IdeProject::GetExpanded(int Id)
{
	int f = d->UserNodeFlags.Find(Id);
	return f >= 0 ? f : false;
}

void IdeProject::SetExpanded(int Id, bool Exp)
{
	if (d->UserNodeFlags.Find(Id) != (int)Exp)
	{
		d->UserNodeFlags.Add(Id, Exp);
		SetUserFileDirty();
	}
}

int IdeProject::AllocateId()
{
	return d->NextNodeId++;
}

void IdeProject::AddBreakpoint(BreakPoint &bp)
{
	auto idx = d->UserBreakpoints.IndexOf(bp);
	if (idx >= 0)
	{
		// LgiTrace("%s:%i bp already exists: %s\n", _FL, bp.Save().Get());
		return;
	}

	d->UserBreakpoints.Add(bp);
	SetUserFileDirty();
}

bool IdeProject::DeleteBreakpoint(int id)
{
	auto idx = d->UserBreakpoints.IndexOf(id);
	if (idx < 0)
	{
		LgiTrace("%s:%i bp doesn't exist: %i\n", _FL, id);
		return false;
	}
	
	SetUserFileDirty();
	return d->UserBreakpoints.DeleteAt(idx);
}

bool IdeProject::HasBreakpoint(int id)
{
	return d->UserBreakpoints.HasItem(id);
}

template<typename T, typename Fn>
bool CheckExists(LAutoString Base, T &p, Fn Setter, bool Debug)
{
	LFile::Path Full;
	bool WasRel = LIsRelativePath(p);
	if (WasRel)
	{
		Full = Base.Get();
		Full += p.Get();
	}
	else Full = p.Get();
	
	bool Ret = Full.Exists();
	if (!Ret)
	{
		// Is the case wrong?
		for (int i=1; i<Full.Length(); i++)
		{
			LFile::Path t;
			t = Full.Slice(0, i);
			if (!t.Exists())
			{
				LDirectory dir;
				bool Matched = false;
				auto &Leaf = Full[i-1];
				auto Parent = t.GetParent();
				
				if (Debug)
					printf("Searching '%s' for missing '%s'\n", Parent.GetFull().Get(), Leaf.Get());
				
				for (auto b=dir.First(Parent); b; b = dir.Next())
				{
					if (!Stricmp(dir.GetName(), t.Last().Get()))
					{
						if (Debug)
							printf("t[%i]: %s->%s\n", i, Leaf.Get(), dir.GetName());
						Leaf = dir.GetName();
						Matched = true;
						break;
					}
				}
				if (!Matched)					
					break;
			}
		}
		
		if ((Ret = Full.Exists()))
		{
			LString Old = p.Get();
			if (WasRel)
			{
				auto r = LMakeRelativePath(Base, Full);
				Setter(p, r);
			}
			else
			{
				Setter(p, Full.GetFull());
			}
			
			if (Debug)
				printf("%s -> %s\n", Old.Get(), p.Get());
		}
	}

	if (Debug)
		printf("CheckExists '%s' = %i\n", Full.GetFull().Get(), Ret);		

	return Ret;
}

bool IdeProject::CheckExists(LString &p, bool Debug)
{
	if (auto backend = GetBackend())
	{
		// Fixme...
		return true;
	}
	else
	{
		return ::CheckExists(GetBasePath(), p, [](LString &o, const char *i) {
				o = i;
			}, Debug);
	}
}

bool IdeProject::CheckExists(LAutoString &p, bool Debug)
{
	if (auto backend = GetBackend())
	{
		// Fixme...
		return true;
	}
	else
	{
		return ::CheckExists(GetBasePath(), p, [](LAutoString &o, const char *i) {
				o.Reset(NewStr(i));
			}, Debug);
	}
}

bool IdeProject::GetClean()
{
	for (auto i: *this)
	{
		ProjectNode *p = dynamic_cast<ProjectNode*>(i);
		if (p && !p->GetClean())
			return false;
	}
	
	return !d->Dirty && !d->UserFileDirty;
}

void IdeProject::SetClean(std::function<void(bool)> OnDone)
{
	auto CleanNodes = [this, OnDone]()
	{
		// printf("IdeProject.SetClean.CleanNodes\n");
		for (auto i: *this)
		{
			ProjectNode *p = dynamic_cast<ProjectNode*>(i);
			if (!p) break;
			p->SetClean();
		}

		if (OnDone)
			OnDone(true);
	};

	// printf("IdeProject.SetClean dirty=%i,%i validfile=%i\n", d->Dirty, d->UserFileDirty, ValidStr(d->FileName));
	if (d->Dirty || d->UserFileDirty)
	{
		if (ValidStr(d->FileName))
			SaveFile();
		else
		{
			auto s = new LFileSelect;
			s->Parent(Tree);
			s->Name("Project.xml");
			s->Save([this, OnDone, CleanNodes](auto s, auto ok)
			{
				// printf("IdeProject.SetClean.FileSelect ok=%i\n", ok);
				if (ok)
				{
					d->FileName = s->Name();
					d->UserFile = d->FileName + "." + LCurrentUserName();
					d->App->OnFile(d->FileName, true);
					Update();

					CleanNodes();
				}
				else
				{
					if (OnDone)
						OnDone(false);
				}
			});
			return;
		}
	}
	
	CleanNodes();
}


const char *IdeProject::GetText(int Col)
{
	if (d->FileName)
	{
		char *s = strrchr(d->FileName, DIR_CHAR);
		return s ? s + 1 : d->FileName.Get();
	}

	return Untitled;
}

int IdeProject::GetImage(int Flags)
{
	return 0;
}

void IdeProject::Empty()
{
	LXmlTag *t;
	while (Children.Length() > 0 &&
		(t = Children[0]))
	{
		ProjectNode *n = dynamic_cast<ProjectNode*>(t);
		if (n)
		{
			n->Remove();
		}
		DeleteObj(t);
	}
}

LXmlTag *IdeProject::Create(char *Tag)
{
	if (!stricmp(Tag, TagSettings))
		return NULL;

	return new ProjectNode(this);
}

void IdeProject::OnMouseClick(LMouse &m)
{
	if (m.IsContextMenu())
	{
		LSubMenu Sub;
		Sub.AppendItem("New Folder", IDM_NEW_FOLDER);
		Sub.AppendItem("New Web Folder", IDM_WEB_FOLDER);
		Sub.AppendItem("Delete", IDM_DELETE, GetParentProject() != NULL);
		Sub.AppendSeparator();
		Sub.AppendItem("Build", IDM_BUILD);
		Sub.AppendItem("Clean Project", IDM_CLEAN_PROJECT);
		Sub.AppendItem("Clean All", IDM_CLEAN_ALL);
		Sub.AppendItem("Rebuild Project", IDM_REBUILD_PROJECT);
		Sub.AppendItem("Rebuild All", IDM_REBUILD_ALL);
		Sub.AppendSeparator();
		Sub.AppendItem("Sort Children", IDM_SORT_CHILDREN);
		Sub.AppendSeparator();
		Sub.AppendItem("Settings", IDM_SETTINGS, true);
		Sub.AppendItem("Insert Dependency", IDM_INSERT_DEP);

		m.ToScreen();
		auto c = _ScrollPos();
		m.x -= c.x;
		m.y -= c.y;
		switch (Sub.Float(Tree, m.x, m.y))
		{
			case IDM_NEW_FOLDER:
			{
				auto Name = new LInput(Tree, "", "Name:", AppName);
				Name->DoModal([this, Name](auto d, auto code)
				{
					if (code)
						GetSubFolder(this, Name->GetStr(), true);
				});
				break;
			}
			case IDM_DELETE:
			{
				if (d->DepParent)
				{
					d->DepParent->Delete(); // will delete 'this'
					return;
				}
				else LAssert(!"Should be a project node for this dependancy");
				break;
			}
			case IDM_WEB_FOLDER:
			{
				WebFldDlg *Dlg = new WebFldDlg(Tree, 0, 0, 0);
				Dlg->DoModal([Dlg, this](auto d, auto code)
				{
					if (Dlg->Ftp && Dlg->Www)
					{
						IdeCommon *f = GetSubFolder(this, Dlg->Name, true);
						if (f)
						{
							f->SetAttr(OPT_Ftp, Dlg->Ftp);
							f->SetAttr(OPT_Www, Dlg->Www);
						}
					}
				});
				break;
			}
			case IDM_BUILD:
			{
				StopBuild();
				Build(true, d->App->GetBuildMode());
				break;
			}
			case IDM_CLEAN_PROJECT:
			{
				Clean(false, d->App->GetBuildMode());
				break;
			}
			case IDM_CLEAN_ALL:
			{
				Clean(true, d->App->GetBuildMode());
				break;
			}
			case IDM_REBUILD_PROJECT:
			{
				StopBuild();
				Clean(false, d->App->GetBuildMode());
				Build(false, d->App->GetBuildMode());
				break;
			}
			case IDM_REBUILD_ALL:
			{
				StopBuild();
				Clean(true, d->App->GetBuildMode());
				Build(true, d->App->GetBuildMode());
				break;
			}
			case IDM_SORT_CHILDREN:
			{
				SortChildren();
				Project->SetDirty();
				break;
			}
			case IDM_SETTINGS:
			{
				EditSettings(d->App->GetPlatform());
				break;
			}
			case IDM_INSERT_DEP:
			{
				LFileSelect *s = new LFileSelect;
				s->Parent(Tree);
				s->Type("Project", "*.xml");
				s->Open([this](auto s, bool ok)
				{
					if (ok)
					{
						ProjectNode *New = new ProjectNode(this);
						if (New)
						{
							New->SetFileName(s->Name());
							New->SetType(NodeDependancy);
							InsertTag(New);
							SetDirty();
						}
					}
				});
				break;
			}
		}
	}
}

char *IdeProject::FindFullPath(const char *File, ProjectNode **Node)
{
	char *Full = 0;
	
	for (auto i:*this)
	{
		ProjectNode *c = dynamic_cast<ProjectNode*>(i);
		if (!c) break;

		ProjectNode *n = c->FindFile(File, &Full);
		if (n)
		{
			if (Node)
				*Node = n;
			break;
		}
	}
	
	return Full;
}

bool IdeProject::HasNode(ProjectNode *Node)
{
	for (auto i:*this)
	{
		ProjectNode *c = dynamic_cast<ProjectNode*>(i);
		if (!c) break;

		if (c->HasNode(Node))
			return true;
	}
	return false;
}

bool IdeProject::GetAllNodes(LArray<ProjectNode*> &Nodes)
{
	for (auto i:*this)
	{
		ProjectNode *c = dynamic_cast<ProjectNode*>(i);
		if (!c) break;

		c->AddNodes(Nodes);
	}
	return true;
}

bool IdeProject::InProject(bool FuzzyMatch, const char *Path, bool Open, IdeDoc **Doc)
{
	if (!Path)
		return false;

	// Search complete path first...
	auto PlatformFlags = d->App->GetPlatform();
	auto n = d->Nodes.Find(Path);	
	if (!n && FuzzyMatch)
	{
		// No match, do partial matching.
		const char *Leaf = LGetLeaf(Path);
		auto PathLen = strlen(Path);
		auto LeafLen = strlen(Leaf);
		uint32_t MatchingScore = 0;

		// Traverse all nodes and try and find the best fit.
		for (auto Cur: d->Nodes)
		{
			int NodePlatforms = Cur.value->GetPlatforms();
			uint32_t Score = 0;

			LgiTrace("node:%s\n", Cur.key);
			if (stristr(Cur.key, Path))
			{
				Score += PathLen;
			}
			else if (stristr(Cur.key, Leaf))
			{
				Score += LeafLen;
			}

			const char *pLeaf = LGetLeaf(Cur.key);
			if (pLeaf && !stricmp(pLeaf, Leaf))
			{
				Score |= 0x40000000;
			}
			if (Score && (NodePlatforms & PlatformFlags) != 0)
			{
				Score |= 0x80000000;
			}

			if (Score > MatchingScore)
			{
				MatchingScore = Score;
				n = Cur.value;
			}
		}
	}
	
	if (n && Doc)
	{
		*Doc = n->Open();
	}
	
	return n != 0;
}

char *GetQuotedStr(char *Str)
{
	char *s = strchr(Str, '\"');
	if (s)
	{
		s++;
		char *e = strchr(s, '\"');
		if (e)
		{
			return NewStr(s, e - s);
		}
	}

	return 0;
}

void IdeProject::ImportDsp(const char *File)
{
	if (File && LFileExists(File))
	{
		char Base[256];
		strcpy(Base, File);
		LTrimDir(Base);
		
		auto Dsp = LReadFile(File);
		if (Dsp)
		{
			auto Lines = Dsp.SplitDelimit("\r\n");
			IdeCommon *Current = this;
			bool IsSource = false;
			for (int i=0; i<Lines.Length(); i++)
			{
				char *L = Lines[i];
				if (strnicmp(L, "# Begin Group", 13) == 0)
				{
					char *Folder = GetQuotedStr(L);
					if (Folder)
					{
						IdeCommon *Sub = Current->GetSubFolder(this, Folder, true);
						if (Sub)
						{
							Current = Sub;
						}
						DeleteArray(Folder);
					}
				}
				else if (strnicmp(L, "# End Group", 11) == 0)
				{
					IdeCommon *Parent = dynamic_cast<IdeCommon*>(Current->GetParent());
					if (Parent)
					{
						Current = Parent;
					}
				}
				else if (strnicmp(L, "# Begin Source", 14) == 0)
				{
					IsSource = true;
				}
				else if (strnicmp(L, "# End Source", 12) == 0)
				{
					IsSource = false;
				}
				else if (Current && IsSource && strncmp(L, "SOURCE=", 7) == 0)
				{
					ProjectNode *New = new ProjectNode(this);
					if (New)
					{
						char *Src = 0;
						if (strchr(L, '\"'))
						{
							Src = GetQuotedStr(L);
						}
						else
						{
							Src = NewStr(L + 7);
						}						
						if (Src)
						{
							// Make absolute path
							char Abs[256];
							LMakePath(Abs, sizeof(Abs), Base, ToUnixPath(Src));
							
							// Make relitive path
							New->SetFileName(Src);
							DeleteArray(Src);
						}

						Current->InsertTag(New);
						SetDirty();
					}
				}
			}
		}
	}
}

void IdeProject::EditSettings(int platformFlags)
{
	d->Settings.Edit(Tree, platformFlags, [this]()
	{
		SetDirty();
	});
}

IdeProjectSettings *IdeProject::GetSettings()
{
	return &d->Settings;
}

void AddAllSubFolders(LString::Array &out, LString in)
{
	out.SetFixedLength(false);
	
	LDirectory dir;
	for (int b=dir.First(in); b; b=dir.Next())
	{
		if (dir.IsDir() &&
			Strncmp(dir.GetName(), "Qt", 2)
			// && Strcmp(dir.GetName(), "private")
			)
		{
			out.Add(dir.FullPath());
			AddAllSubFolders(out, dir.FullPath());
		}
	}
}

bool PkgConfigPaths(LString::Array &out, LString in)
{
	if (in(0) == '`')
	{
		// Run config app to get the full path list...
		in = in.Strip("`");
		auto a = in.Split(" ", 1);
		LSubProcess Proc(a[0], a.Length() > 1 ? a[1].Get() : NULL);
		if (Proc.Start())
		{
			LStringPipe Buf;
			Proc.Communicate(&Buf);
			auto lines = Buf.NewLStr().SplitDelimit(" \t\r\n");
			for (auto line: lines)
			{
				char *inc = line;
				if (inc[0] == '-' && inc[1] == 'I')
				{
					auto path = line(2,-1);
					// printf("   inc='%s'\n", path.Get());
					out.New() = path;
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - Error: failed to run process for '%s'\n", _FL, in.Get());
			return false;
		}
	}
	else
	{
		out.New() = in;
	}
	
	return true;
}

bool IdeProject::BuildIncludePaths(LString::Array &Paths, LString::Array *SysPaths, bool Recurse, bool IncludeSystem, SysPlatform Platform)
{
	LArray<IdeProject*> Projects;
	if (Recurse)
		GetChildProjects(Projects);
	Projects.AddAt(0, this);

	LHashTbl<StrKey<char>, bool> MapProj, MapSys;
	
	for (auto p: Projects)
	{
		const auto Base = p->GetBasePath();
		LAssert(Base);
		
		const char *Delim = ",;\r\n";
		LString::Array InProj, InSys, OutProj, OutSys;
		InProj.SetFixedLength(false);
		OutProj.SetFixedLength(false);
		InSys.SetFixedLength(false);
		OutSys.SetFixedLength(false);

		LString PlatformInc = d->Settings.GetStr(ProjIncludePaths, NULL, Platform);
		InProj += PlatformInc.SplitDelimit(Delim);

		if (IncludeSystem)
		{
			auto sys = LString(d->Settings.GetStr(ProjSystemIncludes, NULL, Platform)).SplitDelimit(Delim);
			for (auto s: sys)
				PkgConfigPaths(InSys, s);

			bool recurseSystem = true;
			if (recurseSystem)
			{
				auto sz = InSys.Length();
				for (unsigned i=0; i<sz; i++)
				{
					auto &path = InSys[i];					
					auto rel = LIsRelativePath(path);
					if (rel)
					{
						LFile::Path full(Base.Get());
						full += path;
						InSys[i] = full.GetFull();
					}

					AddAllSubFolders(InSys, InSys[i]);
				}				
			}
		}

		auto PreProcessPath = [Platform](LString::Array &out, LString in)
		{
			auto p = ToPlatformPath(in, Platform);
			PkgConfigPaths(out, p);
		};

		auto RelativeToFull = [&Base](LHashTbl<StrKey<char>, bool> &Map, LString &p)
		{
			char *Path = p;
			char *Full = 0, Buf[MAX_PATH_LEN];
			if
				(
					*Path != '/'
					&&
					!(
						IsAlpha(*Path)
						&&
						Path[1] == ':'
					)
				)
			{
				if (LMakePath(Buf, sizeof(Buf), Base, Path))
				{
					Full = Buf;
				}
				else
				{
					LAssert(!"Make path err.");
					return;
				}
			}
			else
			{
				Full = Path;
			}

			if (!Map.Find(Full))
				Map.Add(Full, true);
		};
		
		for (auto &p: InProj)
			PreProcessPath(OutProj, p);
		for (auto &p: InSys)
			PreProcessPath(OutSys, p);

		for (auto &p: OutProj)
			RelativeToFull(MapProj, p);
		for (auto &p: OutSys)
			RelativeToFull(MapSys, p);

		// Add paths for the headers in the project... bit of a hack but it'll
		// help it find them if the user doesn't specify the paths in the project.
		LArray<ProjectNode*> Nodes;
		if (p->GetAllNodes(Nodes))
		{
			auto NodeBase = p->GetFullPath();
			if (NodeBase)
			{
				LTrimDir(NodeBase);

				for (auto &n: Nodes)
				{
					if (n->GetType() == NodeHeader &&				// Only look at headers.
						(n->GetPlatforms() & (1 << Platform)) != 0)	// Exclude files not on this platform.
					{
						auto f = n->GetFileName();
						char p[MAX_PATH_LEN];
						if (f &&
							LMakePath(p, sizeof(p), NodeBase, f))
						{
							char *l = strrchr(p, DIR_CHAR);
							if (l)
								*l = 0;
							if (!MapProj.Find(p))
								MapProj.Add(p, true);
						}
					}
				}
			}
		}
	}

	for (auto p: MapProj)
	{
		Paths.Add(p.key);
		// LgiTrace("Proj: %s\n", p.key);
	}

	auto SysTarget = SysPaths ? SysPaths : &Paths;
	for (auto p: MapSys)
	{
		SysTarget->Add(p.key);
		// LgiTrace("Sys: %s\n", p.key);
	}

	return true;
}

void IdeProjectPrivate::CollectAllFiles(LTreeNode *Base, LArray<ProjectNode*> &Files, bool SubProjects, int Platform)
{
	for (auto i: *Base)
	{
		IdeProject *Proj = dynamic_cast<IdeProject*>(i);
		if (Proj)
		{
			if (Proj->GetParentProject() && !SubProjects)
			{
				continue;
			}
		}
		else
		{
			ProjectNode *p = dynamic_cast<ProjectNode*>(i);
			if (p)
			{
				if (p->GetType() == NodeSrc ||
					p->GetType() == NodeHeader)
				{
					if (p->GetPlatforms() & Platform)
					{
						Files.Add(p);
					}
				}
			}
		}

		CollectAllFiles(i, Files, SubProjects, Platform);
	}
}

LString IdeProject::GetTargetName(SysPlatform Platform)
{
	LString Status;
	const char *t = d->Settings.GetStr(ProjTargetName, NULL, Platform);
	if (ValidStr(t))
	{
		// Take target name from the settings
		Status = t;
	}
	else
	{
		char *s = strrchr(d->FileName, DIR_CHAR);
		if (s)
		{
			// Generate the target executable name
			char Target[MAX_PATH_LEN];
			strcpy_s(Target, sizeof(Target), s + 1);
			s = strrchr(Target, '.');
			if (s) *s = 0;
			strlwr(Target);
			s = Target;
			for (char *i = Target; *i; i++)
			{
				if (*i != '.' && *i != ' ')
				{
					*s++ = *i;
				}
			}
			*s = 0;
			Status = Target;
		}
	}
	
	return Status;
}

LString IdeProject::GetTargetFile(SysPlatform Platform)
{
	LString Target = GetTargetName(PlatformCurrent);
	if (!Target)
	{
		LAssert(!"No target?");
		return LString();
	}

	const char *TargetType = d->Settings.GetStr(ProjTargetType);
	if (!TargetType)
	{
		LAssert(!"Needs a target type.");
		return LString();
	}

	if (!stricmp(TargetType, "Executable"))
	{
		return Target;
	}
	else if (!stricmp(TargetType, "DynamicLibrary"))
	{
		char t[MAX_PATH_LEN];
		auto DefExt = PlatformDynamicLibraryExt(Platform);
		strcpy_s(t, sizeof(t), Target);

		auto tCh = strlen(t);
		auto ext = LGetExtension(t);
		if (!ext)
			snprintf(t+tCh, sizeof(t)-tCh, ".%s", DefExt);
		else if (stricmp(ext, DefExt))
			strcpy((char*)ext, DefExt);
		return t;
	}
	else if (!stricmp(TargetType, "StaticLibrary"))
	{
		LString Ret;
		Ret.Printf("%s.%s", Target.Get(), PlatformSharedLibraryExt(Platform));
		return Ret;
	}
	
	return LString();
}

struct ProjDependency
{
	bool Scanned;
	LAutoString File;
	
	ProjDependency(const char *f)
	{
		Scanned = false;
		File.Reset(NewStr(f));
	}
};

bool IdeProject::GetAllDependencies(LArray<char*> &Files, SysPlatform Platform)
{
	if (!GetTree()->Lock(_FL))
		return false;
		
	LHashTbl<StrKey<char>, ProjDependency*> Deps;
	auto Base = GetBasePath();
	
	// Build list of all the source files...
	LArray<LString> Src;
	CollectAllSource(Src, Platform);
	
	// Get all include paths
	LString::Array IncPaths;
	BuildIncludePaths(IncPaths, NULL, false, false, Platform);
	
	// Add all source to dependencies
	for (int i=0; i<Src.Length(); i++)
	{
		char *f = Src[i];
		ProjDependency *dep = Deps.Find(f);
		if (!dep)
			Deps.Add(f, new ProjDependency(f));
	}
	
	// Scan all dependencies for includes
	LArray<ProjDependency*> Unscanned;
	do
	{
		// Find all the unscanned dependencies
		Unscanned.Length(0);

		for (auto d : Deps)
		{
			if (!d.value->Scanned)
				Unscanned.Add(d.value);
		}		

		for (int i=0; i<Unscanned.Length(); i++)
		{
			// Then scan source for includes...
			ProjDependency *d = Unscanned[i];
			d->Scanned = true;
			
			char *Src = d->File;
			char Full[MAX_PATH_LEN];
			if (LIsRelativePath(d->File))
			{
				LMakePath(Full, sizeof(Full), Base, d->File);
				Src = Full;
			}

			LArray<char*> SrcDeps;
			if (GetDependencies(Src, IncPaths, SrcDeps, Platform))
			{
				for (auto File: SrcDeps)
				{
					// Add include to dependencies...
					if (LIsRelativePath(File))
					{
						LMakePath(Full, sizeof(Full), Base, File);
						File = Full;
					}

					if (!Deps.Find(File))
					{						
						Deps.Add(File, new ProjDependency(File));
					}
				}
				SrcDeps.DeleteArrays();
			}
		}
	}
	while (Unscanned.Length() > 0);
	
	for (auto d : Deps)
	{
		Files.Add(d.value->File.Release());
	}
	
	Deps.DeleteObjects();
	GetTree()->Unlock();
	
	return true;
}

bool IdeProject::GetDependencies(const char *InSourceFile, LString::Array &IncPaths, LArray<char*> &Files, SysPlatform Platform)
{
	LString SourceFile = InSourceFile;
	if (!CheckExists(SourceFile))
	{
		LgiTrace("%s:%i - can't read '%s'\n", _FL, SourceFile.Get());
		return false;
	}

	auto c8 = LReadFile(SourceFile);
	if (!c8)
		return false;

	LString::Array Headers;
	LArray<LString::Array*> AllInc;
	AllInc.Add(&IncPaths);
	if (!BuildHeaderList(c8,
						Headers,
						false,
						[&AllInc](auto Name)
						{
							return FindHeader(Name, AllInc);
						}))
		return false;
	
	for (int n=0; n<Headers.Length(); n++)
	{
		char *i = Headers[n];
		LString p;
		if (!RelativePath(p, i))
			p = i;
		ToUnixPath(p);
		Files.Add(NewStr(p));
	}
	
	return true;
}

int MakefileThread::Instances = 0;

bool IdeProject::CreateMakefile(SysPlatform Platform, bool BuildAfterwards)
{
	if (d->CreateMakefile)
	{
		if (d->CreateMakefile->IsExited())
			d->CreateMakefile.Reset();
		else
		{
			d->App->GetBuildLog()->Print("%s:%i - Makefile thread still running.\n", _FL);
			return false;
		}
	}
	
	if (Platform == PlatformCurrent)
		Platform = GetCurrentPlatform();

	return d->CreateMakefile.Reset(new MakefileThread(d, Platform, BuildAfterwards));
}

void IdeProject::OnMakefileCreated()
{
	if (d->CreateMakefile)
	{
		d->CreateMakefile.Reset();
		if (MakefileThread::Instances == 0)
			GetApp()->PostEvent(M_LAST_MAKEFILE_CREATED);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
IdeTree::IdeTree() : LTree(IDC_PROJECT_TREE, 0, 0, 100, 100)
{
	MultiSelect(true);
}

void IdeTree::OnCreate()
{
	SetWindow(this);
}

void IdeTree::OnDragExit()
{
	SelectDropTarget(NULL);
}

int IdeTree::WillAccept(LDragFormats &Formats, LPoint p, int KeyState)
{
	static bool First = true;
	
	Formats.SupportsFileDrops();
	Formats.Supports(NODE_DROP_FORMAT);
	First = false;
		
	if (Formats.Length() == 0)
	{
		LgiTrace("%s:%i - No valid drop formats.\n", _FL);
		return DROPEFFECT_NONE;
	}

	Hit = ItemAtPoint(p.x, p.y);
	if (!Hit)
	{
		SelectDropTarget(NULL);
		return DROPEFFECT_NONE;
	}

	if (Formats.HasFormat(LGI_FileDropFormat))
	{
		SelectDropTarget(Hit);
		return DROPEFFECT_LINK;
	}

	IdeCommon *Src = dynamic_cast<IdeCommon*>(Selection());
	IdeCommon *Dst = dynamic_cast<IdeCommon*>(Hit);
	if (Src && Dst)
	{
		// Check this folder is not a child of the src
		for (IdeCommon *n=Dst; n; n=dynamic_cast<IdeCommon*>(n->GetParent()))
		{
			if (n == Src)
				return DROPEFFECT_NONE;
		}
	}

	// Valid target
	SelectDropTarget(Hit);
	return DROPEFFECT_MOVE;
}

int IdeTree::OnDrop(LArray<LDragData> &Data, LPoint p, int KeyState)
{
	int Ret = DROPEFFECT_NONE;
	SelectDropTarget(NULL);
	if (!(Hit = ItemAtPoint(p.x, p.y)))
		return Ret;
	
	for (unsigned n=0; n<Data.Length(); n++)
	{
		LDragData &dd = Data[n];
		LVariant *Data = &dd.Data[0];
		if (dd.IsFormat(NODE_DROP_FORMAT))
		{
			if (Data->Type == GV_BINARY && Data->Value.Binary.Length == sizeof(ProjectNode*))
			{
				ProjectNode *Src = ((ProjectNode**)Data->Value.Binary.Data)[0];
				if (Src)
				{
					ProjectNode *Folder = dynamic_cast<ProjectNode*>(Hit);
					while (Folder && Folder->GetType() != NodeDir)
						Folder = dynamic_cast<ProjectNode*>(Folder->GetParent());

					IdeCommon *Dst = dynamic_cast<IdeCommon*>(Folder?Folder:Hit);
					if (Dst)
					{
						// Check this folder is not a child of the src
						for (IdeCommon *n=Dst; n; n=dynamic_cast<IdeCommon*>(n->GetParent()))
						{
							if (n == Src)
								return DROPEFFECT_NONE;
						}

						// Detach
						LTreeItem *i = dynamic_cast<LTreeItem*>(Src);
						i->Detach();
						if (Src->LXmlTag::Parent)
						{
							LAssert(Src->LXmlTag::Parent->Children.HasItem(Src));
							Src->LXmlTag::Parent->Children.Delete(Src);
						}
						
						// Attach
						Src->LXmlTag::Parent = Dst;
						Dst->Children.SetFixedLength(false);
						Dst->Children.Add(Src);
						Dst->Children.SetFixedLength(true);
						Dst->Insert(Src);
						
						// Dirty
						Src->GetProject()->SetDirty();
					}
				
					Ret = DROPEFFECT_MOVE;
				}
			}
		}
		else if (dd.IsFileDrop())
		{
			ProjectNode *Folder = dynamic_cast<ProjectNode*>(Hit);
			while (Folder && Folder->GetType() > NodeDir)
				Folder = dynamic_cast<ProjectNode*>(Folder->GetParent());
			
			IdeCommon *Dst = dynamic_cast<IdeCommon*>(Folder?Folder:Hit);
			if (Dst)
			{
				AddFilesProgress Prog(this);
				LDropFiles Df(dd);
				for (int i=0; i<Df.Length() && !Prog.Cancel; i++)
				{
					if (Dst->AddFiles(&Prog, Df[i]))
						Ret = DROPEFFECT_LINK;
				}
			}
		}
		else LgiTrace("%s:%i - Unknown drop format: %s.\n", _FL, dd.Format.Get());
	}

	return Ret;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
AddFilesProgress::AddFilesProgress(LViewI *par)
{
	v = 0;
	Cancel = false;
	Msg = NULL;
	SetParent(par);
	Ts = LCurrentTime();
	LRect r(0, 0, 140, 100);
	SetPos(r);
	MoveSameScreen(par);
	Name("Importing files...");

	LString::Array a = LString(DefaultExt).SplitDelimit(",");
	for (unsigned i=0; i<a.Length(); i++)
	{
		Exts.Add(a[i], true);
	}

	LTableLayout *t = new LTableLayout(100);
	AddView(t);
		
	auto *c = t->GetCell(0, 0);
	c->Add(new LTextLabel(-1, 0, 0, -1, -1, "Loaded:"));
		
	c = t->GetCell(1, 0);
	c->Add(Msg = new LTextLabel(-1, 0, 0, -1, -1, "..."));
		
	c = t->GetCell(0, 1, true, 2);
	c->TextAlign(LCss::Len(LCss::AlignRight));
	c->Add(new LButton(IDCANCEL, 0, 0, -1, -1, "Cancel"));
}

int64 AddFilesProgress::Value()
{
	return v;
}

void AddFilesProgress::Value(int64 val)
{
	v = val;

	uint64 Now = LCurrentTime();
	if (Visible())
	{
		if (Now - Ts > 200)
		{
			if (Msg)
			{
				Msg->Value(v);
				Msg->SendNotify(LNotifyTableLayoutRefresh);
			}
			Ts = Now;
		}
	}
	else if (Now - Ts > 1000)
	{
		DoModeless();
		SetAlwaysOnTop(true);
	}
}

int AddFilesProgress::OnNotify(LViewI *c, const LNotification &n)
{
	if (c->GetId() == IDCANCEL)
		Cancel = true;
	return 0;
}
