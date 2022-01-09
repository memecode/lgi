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

#include "LgiIde.h"
#include "resdefs.h"
#include "FtpThread.h"
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

const char *PlatformNames[] =
{
	"Windows",
	"Linux",
	"Mac",
	"Haiku",
	0
};

int PlatformCtrlId[] =
{
	IDC_WIN32,
	IDC_LINUX,
	IDC_MAC,
	IDC_HAIKU,
	0
};

const char *PlatformDynamicLibraryExt(IdePlatform Platform)
{
	if (Platform == PlatformWin)
		return "dll";
	if (Platform == PlatformMac)
		return "dylib";
	
	return "so";
}

const char *PlatformSharedLibraryExt(IdePlatform Platform)
{
	if (Platform == PlatformWin)
		return "lib";

	return "a";
}

const char *PlatformExecutableExt(IdePlatform Platform)
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
		char p[MAX_PATH];
		LMakePath(p, sizeof(p), Path[i], Exe);
		if (LFileExists(p))
		{
			Exe = p;
			return true;
		}
	}
	
	return false;
}

LAutoString ToNativePath(const char *s)
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

class BuildThread : public LThread, public LStream
{
	IdeProject *Proj;
	LString Makefile, CygwinPath;
	bool Clean, Release, All;
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

public:
	BuildThread(IdeProject *proj, char *makefile, bool clean, bool release, bool all, int wordsize);
	~BuildThread();
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override;
	LString FindExe();
	LAutoString WinToMingWPath(const char *path);
	int Main() override;
};

class IdeProjectPrivate
{
public:
	AppWnd *App;
	IdeProject *Project;
	bool Dirty, UserFileDirty;
	LString FileName;
	IdeProject *ParentProject;
	IdeProjectSettings Settings;
	LAutoPtr<BuildThread> Thread;
	LHashTbl<ConstStrKey<char,false>, ProjectNode*> Nodes;
	int NextNodeId;

	// Threads
	LAutoPtr<class MakefileThread> CreateMakefile;

	// User info file
	LString UserFile;
	LHashTbl<IntKey<int>,int> UserNodeFlags;

	IdeProjectPrivate(AppWnd *a, IdeProject *project) :
		Project(project),
		Settings(project)
	{
		App = a;
		Dirty = false;
		UserFileDirty = false;
		ParentProject = 0;
		NextNodeId = 1;
	}

	void CollectAllFiles(LTreeNode *Base, LArray<ProjectNode*> &Files, bool SubProjects, int Platform);
};

LString ToPlatformPath(const char *s, IdePlatform platform)
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
	IdePlatform Platform;
	LStream *Log;
	bool BuildAfterwards;
	bool HasError;
	
public:
	static int Instances;

	MakefileThread(IdeProjectPrivate *priv, IdePlatform platform, bool Build) : LThread("MakefileThread")
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
	
	int Main()
	{
		const char *PlatformName = PlatformNames[Platform];
		const char *PlatformLibraryExt = NULL;
		const char *PlatformStaticLibExt = NULL;
		const char *PlatformExeExt = "";
		LString LinkerFlags;
		const char *TargetType = d->Settings.GetStr(ProjTargetType, NULL, Platform);
		const char *CompilerName = d->Settings.GetStr(ProjCompiler);
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

		char Buf[256];
		auto MakeFile = Proj->GetMakefile(Platform);
		Proj->CheckExists(MakeFile);
		if (!MakeFile)
			MakeFile = "../Makefile";
		
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
		
		LAutoString Base = Proj->GetBasePath();
		if (IsExecutableTarget)
		{
			LString Exe = Proj->GetExecutable(Platform);
			if (Exe)
			{
				if (LIsRelativePath(Exe))
					m.Print("Target = %s\n", ToPlatformPath(Exe, Platform).Get());
				else
				{
					LAutoString RelExe = LMakeRelativePath(Base, Exe);
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
		int BuildMode = d->App->GetBuildMode();
		auto BuildModeName = BuildMode ? "Release" : "Debug";
		m.Print("ifndef Build\n"
				"	Build = %s\n"
				"endif\n",
				BuildModeName);
		
		LString sDefines[2];
		LString sLibs[2];
		LString sIncludes[2];
		const char *ExtraLinkFlags = NULL;
		const char *ExeFlags = NULL;
		if (Platform == PlatformWin)
		{
			ExtraLinkFlags = "";
			ExeFlags = " -mwindows";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"Flags = -fPIC -w -fno-inline -fpermissive\n");
			
			const char *DefDefs = "-DWIN32 -D_REENTRANT";
			sDefines[0] = DefDefs;
			sDefines[1] = DefDefs;
		}
		else
		{
			LString PlatformCap = PlatformName;
		
			ExtraLinkFlags = "";
			ExeFlags = "";
			m.Print("BuildDir = $(Build)\n"
					"\n"
					"Flags = -fPIC -w -fno-inline -fpermissive\n" // -fexceptions
					);
			sDefines[0].Printf("-D%s -D_REENTRANT", PlatformCap.Upper().Get());
			#ifdef LINUX
			sDefines[0] += " -D_FILE_OFFSET_BITS=64"; // >:-(
			sDefines[0] += " -DPOSIX";
			#endif
			sDefines[1] = sDefines[0];
		}

		List<IdeProject> Deps;
		Proj->GetChildProjects(Deps);

		const char *sConfig[] = {"Debug", "Release"};
		for (int Cfg = 0; Cfg < CountOf(sConfig); Cfg++)
		{
			// Set the config
			d->Settings.SetCurrentConfig(sConfig[Cfg]);
		
			// Get the defines setup
			const char *PDefs = d->Settings.GetStr(ProjDefines, NULL, Platform);
			if (ValidStr(PDefs))
			{
				GToken Defs(PDefs, " ;,\r\n");
				for (int i=0; i<Defs.Length(); i++)
				{
					LString s;
					s.Printf(" -D%s", Defs[i]);
					sDefines[Cfg] += s;
				}
			}
		
			// Collect all dependencies, output their lib names and paths
			LString PLibPaths = d->Settings.GetStr(ProjLibraryPaths, NULL, Platform);
			if (ValidStr(PLibPaths))
			{
				LString::Array LibPaths = PLibPaths.Split("\n");
				for (unsigned i=0; i<LibPaths.Length(); i++)
				{
					LString s, in = LibPaths[i].Strip();
					if (!in.Length())
						continue;
					
					if (in(0) == '-')
					{
						s.Printf(" \\\n\t\t%s", in.Get());
					}
					else
					{
						LAutoString Rel;
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
				GToken Libs(PLibs, "\r\n");
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
				LString Target = dep->GetTargetName(Platform);
				if (Target)
				{
					char t[MAX_PATH];
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
						
						LAutoString Rel;
						Rel = LMakeRelativePath(Base, DepPath);

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
				GToken Paths(ProjIncludes, "\r\n");
				for (int i=0; i<Paths.Length(); i++)
				{
					char *p = Paths[i];
					LAutoString pn = ToNativePath(p);					
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
				GToken Paths(SysIncludes, "\r\n");
				for (int i=0; i<Paths.Length(); i++)
				{
					char *p = Paths[i];
					LAutoString pn = ToNativePath(p);
					if (pn.Get()[0] != '`' && !Proj->CheckExists(pn))
						OnError("%s:%i - System include path '%s' doesn't exist (from %s).\n",
							_FL, pn.Get(),
							Proj->GetFileName());
					else if (!Inc.Find(pn))
						Inc.Add(pn, true);
				}
			}

			#if 0
			/*
			Currently this code is adding extra paths that are covered by the official 'IncludePaths'
			in addition to relative paths in the actual #include parameter. e.g.
				#include "lgi/common/SomeHeader.h"
			Hence disabling it for the time being.
			*/
			// Add paths of headers
			for (int i=0; i<Files.Length(); i++)
			{
				ProjectNode *n = Files[i];
				
				if (n->GetFileName())
				{
					char *e = LGetExtension(n->GetFileName());
					if (e && stricmp(e, "h") == 0)
					{
						LString Fn = n->GetFileName();
						for (char *Dir = Fn; *Dir; Dir++)
						{
							if (*Dir == '/' || *Dir == '\\')
							{
								*Dir = DIR_CHAR;
							}
						}						

						char Path[MAX_PATH];
						strcpy_s(Path, sizeof(Path), Fn);

						LTrimDir(Path);
					
						LString Rel;
						if (!Proj->RelativePath(Rel, Path))
							Rel = Path;
						
						if (stricmp(Rel, ".") != 0)
						{
							LAutoString RelN = ToNativePath(Rel);
							if (!Proj->CheckExists(RelN))
								OnError("Header include path '%s' doesn't exist.\n", RelN.Get());
							else if (!Inc.Find(RelN))
								Inc.Add(RelN, true);
						}
					}
				}
			}
			#endif

			LString::Array Incs;
			for (auto i: Inc)
				Incs.New() = i.key;
			Incs.Sort();
			
			for (auto i: Incs)
			{
				LString s;
				if (*i == '`')
					s.Printf(" \\\n\t\t%s", i.Get());
				else
					s.Printf(" \\\n\t\t-I%s", ToUnixPath(i));
				sIncludes[Cfg] += s;
			}
		}

		// Output the defs section for Debug and Release		

		// Debug specific
		m.Print("\n"
				"ifeq ($(Build),Debug)\n"
				"	Flags += -g -std=c++14\n"
				"	Tag = d\n"
				"	Defs = -D_DEBUG %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n",
					CastEmpty(sDefines[0].Get()),
					CastEmpty(sLibs[0].Get()),
					CastEmpty(sIncludes[0].Get()));
		
		// Release specific
		m.Print("else\n"
				"	Flags += -s -Os -std=c++14\n"
				"	Defs = %s\n"
				"	Libs = %s\n"
				"	Inc = %s\n"
				"endif\n"
				"\n",
					CastEmpty(sDefines[1].Get()),
					CastEmpty(sLibs[1].Get()),
					CastEmpty(sIncludes[1].Get()));
		
		if (Files.Length())
		{
			LArray<LString> IncPaths;
			if (Proj->BuildIncludePaths(IncPaths, false, false, Platform))
			{
				// Do source code list...
				m.Print("# Dependencies\n"
						"Source =\t");
					
				LString::Array SourceFiles;
				for (auto &n: Files)
				{
					if (n->GetType() == NodeSrc)
					{
						auto f = n->GetFileName();
						auto path = ToPlatformPath(f, Platform);
						if (path.Find("./") == 0)
							path = path(2,-1);
						SourceFiles.Add(path);
					}
				}
				SourceFiles.Sort();
				int c = 0;
				for (auto &src: SourceFiles)
				{
					if (c++) m.Print(" \\\n\t\t\t");
					m.Print("%s", src.Get());
				}
				m.Print("\n"
						"\n"
						"SourceLst := $(patsubst %%.c,%%.o,$(patsubst %%.cpp,%%.o,$(Sources)))\n"
						"\n"
						"Objects := $(addprefix $(BuildDir)/,$(SourceLst))\n"
						"\n");

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
							auto Base = Proj->GetBasePath();
							auto TargetFile = Dep->GetTargetFile(Platform);
							
							if (DepBase && Base && TargetFile)
							{
								LString Rel;
								if (!Proj->RelativePath(Rel, DepBase))
									Rel = DepBase;
								ToUnixPath(Rel);
								
								// Add tag to target name
								GToken Parts(TargetFile, ".");
								if (Parts.Length() == 2)
									TargetFile.Printf("lib%s$(Tag).%s", Parts[0], Parts[1]);

								sprintf(Buf, "%s/$(BuildDir)/%s", Rel.Get(), TargetFile.Get());
								m.Print(" %s", Buf);
								
								LArray<char*> AllDeps;
								Dep->GetAllDependencies(AllDeps, Platform);
								LAssert(AllDeps.Length() > 0);
								AllDeps.Sort(StrSort);

								Rules.Print("%s : ", Buf);
								for (int i=0; i<AllDeps.Length(); i++)
								{
									if (i)
										Rules.Print(" \\\n\t");
									
									LString DepRel;
									char *f = Proj->RelativePath(DepRel, AllDeps[i]) ? DepRel.Get() : AllDeps[i];
									ToUnixPath(f);
									Rules.Print("%s", f);
									
									// Add these dependencies to this makefiles dep list
									if (!DepFiles.Find(f))
										DepFiles.Add(f, true);
								}

								AllDeps.DeleteArrays();

								Rules.Print("\n\texport Build=$(Build); \\\n"
											"\t$(MAKE) -C %s",
											Rel.Get());

								auto Mk = Dep->GetMakefile(Platform);
								// RenameMakefileForPlatform(Mk, Platform);
								if (Mk)
								{
									char *DepMakefile = strrchr(Mk, DIR_CHAR);
									if (DepMakefile)
										Rules.Print(" -f %s", DepMakefile + 1);
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

						m.Print(" $(Depends)\n"
								"	@echo Linking $(Target) [$(Build)]...\n"
								"	$(CPP)%s%s %s%s -o \\\n"
								"		$(Target) $(addprefix $(BuildDir)/,$(Depends)) $(Libs)\n",
								ExtraLinkFlags,
								ExeFlags,
								ValidStr(LinkerFlags) ? "-Wl" : "", LinkerFlags.Get());

						if (Platform == PlatformHaiku)
						{
							// Is there an application icon configured?
							const char *AppIcon = d->Settings.GetStr(ProjApplicationIcon, NULL, Platform);
							if (AppIcon)
							{
								m.Print("	addattr -f %s -t \"'VICN'\" \"BEOS:ICON\" $(Target)\n", AppIcon);
							}							
						}
						
						LString PostBuildCmds = d->Settings.GetStr(ProjPostBuildCommands, NULL, Platform);
						if (ValidStr(PostBuildCmds))
						{
							LString::Array a = PostBuildCmds.Split("\n");
							for (unsigned i=0; i<a.Length(); i++)
								m.Print("\t%s\n", a[i].Strip().Get());
						}

						m.Print("	@echo Done.\n"
								"\n");

						LAutoString r(Rules.NewStr());
						if (r)
							m.Write(r, strlen(r));

						// Various fairly global rules
						m.Print(".SECONDEXPANSION:\n"
								"$(Objects): $(BuildDir)/%%.o: $$(wildcard %%.c*)\n"
								"	mkdir -p $(@D)\n"
								"	@echo $(<F) [$(Build)]\n"
								"ifeq \"$(suffix $<)\" \".cpp\"\n"
								"	$(CPP) -MMD -MP $(Inc) $(Flags) $(Defs) -c $< -o $@\n"
								"else\n"
								"	$(CC) -MMD -MP $(Inc) $(Flags) $(Defs) -c $< -o $@\n"
								"endif\n"
								"\n"
								"-include $(Objects:.o=.d)\n"
								"\n"						
								"# Clean just this target\n"
								"clean :\n"
								"	rm -rf $(BuildDir)/* $(Target)%s\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								PlatformExecutableExt(Platform));
						
						m.Print("# Clean all targets\n"
								"cleanall :\n"
								"	rm -rf $(BuildDir)/* $(Target)%s\n"
								"	@echo Cleaned $(BuildDir)\n",
								PlatformExecutableExt(Platform));
						
						for (auto d: Deps)
						{
							auto mk = d->GetMakefile(Platform);
							if (mk)
							{
								LAutoString my_base = Proj->GetBasePath();
								LAutoString dep_base = d->GetBasePath();
								d->CheckExists(dep_base);

								LAutoString rel_dir = LMakeRelativePath(my_base, dep_base);
								d->CheckExists(rel_dir);
								
								char *mk_leaf = strrchr(mk, DIR_CHAR);
								m.Print("	+make -C \"%s\" -f \"%s\" clean\n",
									ToUnixPath(rel_dir ? rel_dir.Get() : dep_base.Get()),
									ToUnixPath(mk_leaf ? mk_leaf + 1 : mk.Get()));
							}
						}
						m.Print("\n");
					}
					// Shared library
					else if (!stricmp(TargetType, "DynamicLibrary"))
					{
						m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
								"$(TargetFile) : $(Objects)\n"
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
						m.Print(".SECONDEXPANSION:\n"
								"$(Objects): $(BuildDir)/%%.o: $$(wildcard %%.c*)\n"
								"	mkdir -p $(@D)\n"
								"	@echo $(<F) [$(Build)]\n"
								"ifeq \"$(suffix $<)\" \".cpp\"\n"
								"	$(CPP) -MMD -MP $(Inc) $(Flags) $(Defs) -c $< -o $@\n"
								"else\n"
								"	$(CC) -MMD -MP $(Inc) $(Flags) $(Defs) -c $< -o $@\n"
								"endif\n"
								"\n"
								"-include $(Objects:.o=.d)\n"
								"\n"
								"# Clean out targets\n"
								"clean :\n"
								"	rm -rf $(BuildDir)/* $(BuildDir)/$(TargetFile)\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								PlatformLibraryExt);
					}
					// Static library
					else if (!stricmp(TargetType, "StaticLibrary"))
					{
						m.Print("TargetFile = lib$(Target)$(Tag).%s\n"
								"$(TargetFile) : outputfolder $(Depends)\n"
								"	@echo Linking $(TargetFile) [$(Build)]...\n"
								"	ar rcs $(BuildDir)/$(TargetFile) $(addprefix $(BuildDir)/,$(Depends))\n",
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
						m.Print(".SECONDEXPANSION:\n"
								"$(Objects): $(BuildDir)/%%.o: $$(wildcard %%.c*)\n"
								"	mkdir -p $(@D)\n"
								"	@echo $(<F) [$(Build)]\n"
								"ifeq \"$(suffix $<)\" \".cpp\"\n"
								"	$(CPP) -MMD -MP $(Inc) $(Flags) $(Defs) -c $< -o $@\n"
								"else\n"
								"	$(CC) -MMD -MP $(Inc) $(Flags) $(Defs) -c $< -o $@\n"
								"endif\n"
								"\n"
								"-include $(Objects:.o=.d)\n"
								"\n"
								"# Clean out targets\n"
								"clean :\n"
								"	rm -rf $(BuildDir)/* $(BuildDir)/$(TargetFile)\n"
								"	@echo Cleaned $(BuildDir)\n"
								"\n",
								PlatformStaticLibExt);
					}
				}

				// Output VPATH
				m.Print("VPATH=%%.cpp \\\n");
				IncPaths.Sort();
				for (int i=0; i<IncPaths.Length() && !IsCancelled(); i++)
				{
					LString p = IncPaths[i];
					Proj->CheckExists(p);
					if (p && !strchr(p, '`'))
					{
						if (!LIsRelativePath(p))
						{
							LAutoString a = LMakeRelativePath(Base, p);
							m.Print("\t%s \\\n", ToPlatformPath(a ? a.Get() : p.Get(), Platform).Get());
						}
						else
						{
							m.Print("\t%s \\\n", ToPlatformPath(p, Platform).Get());
						}
					}
				}
				m.Print("\t$(BuildDir)\n\n");

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
BuildThread::BuildThread(IdeProject *proj, char *makefile, bool clean, bool release, bool all, int wordsize) : LThread("BuildThread")
{
	Proj = proj;
	Makefile = makefile;
	Clean = clean;
	Release = release;
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
		auto Comp = Proj->GetSettings()->GetStr(ProjCompiler);
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
				LAssert(!"Unknown compiler.");
		}
	}

	if (Compiler == DefaultCompiler)
	{
		// Use default compiler for platform...
		#ifdef WIN32
		Compiler = VisualStudio;
		#else
		Compiler = Gcc;
		#endif
	}

	Run();
}

BuildThread::~BuildThread()
{
	if (SubProc)
	{
		bool b = SubProc->Interrupt();
		LgiTrace("%s:%i - Sub process interrupt = %i.\n", _FL, b);
	}
	else LgiTrace("%s:%i - No sub process to interrupt.\n", _FL);

	uint64 Start = LCurrentTime();
	bool Killed = false;
	while (!IsExited())
	{
		LSleep(10);

		if (LCurrentTime() - Start > STOP_BUILD_TIMEOUT &&
			SubProc)
		{
			if (Killed)
			{
				// Thread is stuck as well... ok kill that too!!! Argh - kill all the things!!!!
				Terminate();
				LgiTrace("%s:%i - Thread killed.\n", _FL);
				Proj->GetApp()->PostEvent(M_BUILD_DONE);
				break;
			}
			else
			{
				// Kill the sub-process...
				bool b = SubProc->Kill();
				Killed = true;
				LgiTrace("%s:%i - Sub process killed.\n", _FL, b);
				Start = LCurrentTime();
			}
		}
	}
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
				char Path[MAX_PATH];
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
							auto Out = o.NewGStr();
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
								char f[MAX_PATH];
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
		char p[MAX_PATH];
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
		GRegKey k(false, "HKEY_CURRENT_USER\\Software\\Cygwin\\Installations");
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
	else
	{
		if (Compiler == MingW)
		{
			// Have a look in the default spot first...
			const char *Def = "C:\\MinGW\\msys\\1.0\\bin\\make.exe";
			if (LFileExists(Def))
			{
				return Def;
			}				
		}
		
		for (int i=0; i<p.Length(); i++)
		{
			char Exe[MAX_PATH];
			LMakePath
			(
				Exe,
				sizeof(Exe),
				p[i],
				"make"
				#ifdef WIN32
				".exe"
				#endif
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
	GToken t(path, "\\");
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

int BuildThread::Main()
{
	const char *Err = 0;
	char ErrBuf[256];

	auto Exe = FindExe();
	if (Exe)
	{
		bool Status = false;
		LString MakePath = Makefile.Get();
		LString InitDir = Makefile.Get();
		LVariant Jobs;
		if (!Proj->GetApp()->GetOptions()->GetValue(OPT_Jobs, Jobs) || Jobs.CastInt32() < 1)
			Jobs = 2;

		auto Pos = InitDir.RFind(DIR_STR);
		if (Pos)
			InitDir.Length(Pos);

		LString TmpArgs, Include, Lib, LibPath, Path;
		
		if (Compiler == VisualStudio)
		{
			// TmpArgs.Printf("\"%s\" /make \"All - Win32 Debug\"", Makefile.Get());
			LString BuildConf = "All - Win32 Debug";
			if (BuildConfigs.Length())
			{
				const char *Key = Release ? "Release" : "Debug";
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
				auto lines = o.NewGStr().SplitDelimit("\n");
				int inKey = -1;
				for (auto l: lines)
				{
					int ws = 0;
					for (int i=0; i<l.Length(); i++)
						if (IsWhiteSpace(l(i))) ws++;
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
			if (Release)
				TmpArgs += " Build=Release";
		}

		PostThreadEvent(AppHnd, M_SELECT_TAB, AppWnd::BuildTab);

		LString Msg;
		Msg.Printf("Making: %s\n", MakePath.Get());
		Proj->GetApp()->PostEvent(M_APPEND_TEXT, (LMessage::Param)NewStr(Msg), AppWnd::BuildTab);

		LgiTrace("%s %s\n", Exe.Get(), TmpArgs.Get());
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
				LFile log("c:\\tmp\\build.txt", O_WRITE);
				log.SetSize(0);

				while ( (rd = SubProc->Read(Buf, sizeof(Buf))) > 0 )
				{
					log.Write(Buf, rd);
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
							LSubProcess PostCmd(p[0], p.Length() > 1 ? p[1] : NULL);
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
IdeProject::IdeProject(AppWnd *App) : IdeCommon(NULL)
{
	Project = this;
	d = new IdeProjectPrivate(App, this);
	Tag = NewStr("Project");
}

IdeProject::~IdeProject()
{
	d->App->OnProjectDestroy(this);
	LXmlTag::Empty(true);
	DeleteObj(d);
}

bool IdeProject::OnNode(const char *Path, ProjectNode *Node, bool Add)
{
	if (!Path || !Node)
	{
		LAssert(0);
		return false;
	}

	char Full[MAX_PATH];
	if (LIsRelativePath(Path))
	{
		LAutoString Base = GetBasePath();
		if (LMakePath(Full, sizeof(Full), Base, Path))
		{
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
	// char *fp = FindFullPath(File, &Node);
	if (Node)
	{
		Node->OnProperties();
	}
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

bool IdeProject::GetChildProjects(List<IdeProject> &c)
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
		
	GToken b(Base, DIR_STR);
	GToken i(In, DIR_STR);
	char out[MAX_PATH] = "";
	
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
			out[0] = 0;
			auto Back = b.Length() - Common;
if (Debug) LgiTrace("Back=%i\n", (int)Back);
			for (int n=0; n<Back; n++)
			{
				strcat(out, "..");
				if (n < Back - 1) strcat(out, DIR_STR);
			}
		}
		else
		{
			strcpy(out, ".");
		}
		for (int n=Common; n<i.Length(); n++)
		{
			sprintf(out+strlen(out), "%s%s", DIR_STR, i[n]);
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

bool IdeProject::GetExePath(char *Path, int Len)
{
	const char *PExe = d->Settings.GetStr(ProjExe);
	if (PExe)
	{
		if (LIsRelativePath(PExe))
		{
			LAutoString Base = GetBasePath();
			if (Base)
			{
				LMakePath(Path, Len, Base, PExe);
			}
			else return false;
		}
		else
		{
			strcpy_s(Path, Len, PExe);
		}
		
		return true;
	}
	else return false;
}

LString IdeProject::GetMakefile(IdePlatform Platform)
{
	LString Path;
	const char *PMakefile = d->Settings.GetStr(ProjMakefile, NULL, Platform);
	if (PMakefile)
	{
		if (LIsRelativePath(PMakefile))
		{
			LAutoString Base = GetBasePath();
			if (Base)
			{
				char p[MAX_PATH];
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

void IdeProject::Clean(bool All, bool Release)
{
	if (!d->Thread &&
		d->Settings.GetStr(ProjMakefile))
	{
		auto m = GetMakefile(PlatformCurrent);
		if (m)		
		{
			CheckExists(m);
			d->Thread.Reset(new BuildThread(this, m, true, Release, All, sizeof(ssize_t)*8));
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
	IdeProject *Proj;
	LString Exe, Args, Path;
	int Len;
	ExeAction Act;
	int AppHnd;

public:
	ExecuteThread(IdeProject *proj, const char *exe, const char *args, char *path, ExeAction act) : LThread("ExecuteThread")
	{
		Len = 32 << 10;
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
					char Path[MAX_PATH];
					char *ExeLeaf = LGetLeaf(Exe);
					strcpy_s(Path, sizeof(Path), ExeLeaf ? ExeLeaf : Exe.Get());
					LTrimDir(Path);
									
					char *Term = 0;
					char *WorkDir = 0;
					char *Execute = 0;
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
						char *a = Proj->GetExeArgs() ? Proj->GetExeArgs() : (char*)"";
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

GDebugContext *IdeProject::Execute(ExeAction Act)
{
	LAutoString Base = GetBasePath();
	if (d->Settings.GetStr(ProjExe) &&
		Base)
	{
		char e[MAX_PATH];
		if (GetExePath(e, sizeof(e)))
		{
			if (LFileExists(e))
			{
				const char *Args = d->Settings.GetStr(ProjArgs);
				const char *Env = d->Settings.GetStr(ProjEnv);
				LString InitDir = d->Settings.GetStr(ProjInitDir);
				int RunAsAdmin = d->Settings.GetInt(ProjDebugAdmin);
				if (Act == ExeDebug)
				{
					if (InitDir && LIsRelativePath(InitDir))
					{
						LFile::Path p(Base);
						p += InitDir;
						InitDir = p.GetFull();
					}
						
					return new GDebugContext(d->App, this, e, Args, RunAsAdmin != 0, Env, InitDir);
				}
				else
				{
					new ExecuteThread(this, e, Args, Base, Act);
				}
			}
			else
			{
				LgiMsg(Tree, "Executable '%s' doesn't exist.\n", AppName, MB_OK, e);
			}
		}
	}
	
	return NULL;
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

	List<IdeProject> Proj;
	GetChildProjects(Proj);

	Proj.Insert(this);
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

		// printf("Proj=%s - Timestamps " LGI_PrintfInt64 " - " LGI_PrintfInt64 "\n", Proj.Get(), ProjModTime, MakeModTime);
		if (ProjModTime != 0 &&
			MakeModTime != 0 &&
			ProjModTime > MakeModTime)
		{
			// Need to rebuild the makefile...
			return false;
		}
	}
	
	return true;
}

bool IdeProject::FindDuplicateSymbols()
{
	LStream *Log = d->App->GetBuildLog();	
	Log->Print("FindDuplicateSymbols starting...\n");

	List<IdeProject> Proj;
	CollectAllSubProjects(Proj);
	Proj.Insert(this);

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
				LString::Array a = q.NewGStr().SplitDelimit("\r\n");
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

void IdeProject::Build(bool All, bool Release)
{
	if (d->Thread)
	{
		d->App->GetBuildLog()->Print("Error: Already building (%s:%i)\n", _FL);
		return;
	}

	auto m = GetMakefile(PlatformCurrent);
	CheckExists(m);
	if (!m)
	{		
		d->App->GetBuildLog()->Print("Error: no makefile? (%s:%i)\n", _FL);
		return;
	}

	if (GetApp())
		GetApp()->PostEvent(M_APPEND_TEXT, 0, 0);

	SetClean();

	if (!IsMakefileUpToDate())
		CreateMakefile(GetCurrentPlatform(), true);
	else
		// Start the build thread...
		d->Thread.Reset
		(
			new BuildThread
			(
				this,
				m,
				false,
				Release,
				All,
				sizeof(size_t)*8
			)
		);
}

void IdeProject::StopBuild()
{
	d->Thread.Reset();
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

const char *IdeProject::GetExeArgs()
{
	return d->Settings.GetStr(ProjArgs);
}

LString IdeProject::GetExecutable(IdePlatform Platform)
{
	LString Bin = d->Settings.GetStr(ProjExe, NULL, Platform);
	auto TargetType = d->Settings.GetStr(ProjTargetType, NULL, Platform);
	auto Base = GetBasePath();

	if (Bin)
	{
		if (LIsRelativePath(Bin) && Base)
		{
			char p[MAX_PATH];
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
			
		char Path[MAX_PATH];
		LMakePath(Path, sizeof(Path), Base, Name);
		LMakePath(Path, sizeof(Path), Path, Bin);
		if (LFileExists(Path))
			Bin = Path;
		else
			printf("%s:%i - '%s' doesn't exist.\n", _FL, Path);
		
		
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

	char p[MAX_PATH];
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
	LProfile Prof("IdeProject::OpenFile");
	Prof.HideResultsIfBelow(1000);

	Empty();

	Prof.Add("Init");

	d->UserNodeFlags.Empty();
	if (LIsRelativePath(FileName))
	{
		char p[MAX_PATH];
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
		LgiTrace("%s:%i - No filename.\n", _FL);
		return OpenError;
	}
	
	Prof.Add("FileOpen");

	LFile f;
	LString FullPath = d->FileName.Get();
	if (CheckExists(FullPath) &&
		!f.Open(FullPath, O_READWRITE))
	{
		LgiTrace("%s:%i - Error: Can't open '%s'.\n", _FL, FullPath.Get());
		return OpenError;
	}

	Prof.Add("Xml");

	LXmlTree x;
	LXmlTag r;
	if (!x.Read(&r, &f))
	{
		LgiTrace("%s:%i - Error: Can't read XML: %s\n", _FL, x.GetErrorMsg());
		LgiMsg(Tree, x.GetErrorMsg(), AppName);
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
		if (Uf.Open(d->UserFile, O_READ))
		{
			LString::Array Ln = Uf.Read().SplitDelimit("\n");
			for (unsigned i=0; i<Ln.Length(); i++)
			{
				LString::Array p = Ln[i].SplitDelimit(",");
				if (p.Length() == 2)
					d->UserNodeFlags.Add((int)p[0].Int(), (int)p[1].Int(16));
			}
		}
	}
	
	if (!r.IsTag("Project"))
		return OpenError;

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

	Prof.Add("Serialize");
	
	d->Settings.Serialize(&r, false /* read */);
	return OpenOk;
}

bool IdeProject::SaveFile()
{
	auto Full = GetFullPath();

	printf("IdeProject::SaveFile %s %i\n", Full.Get(), d->Dirty);
	if (ValidStr(Full) && d->Dirty)
	{
		LFile f;
		if (f.Open(Full, O_WRITE))
		{
			f.SetSize(0);

			LXmlTree x;
			LProgressDlg Prog(d->App, 1000);
			Prog.SetAlwaysOnTop(true);
			Prog.SetDescription("Serializing project XML...");
			Prog.SetYieldTime(200);
			Prog.SetCanCancel(false);

			d->Settings.Serialize(this, true /* write */);
			
			LStringPipe Buf(4096);			
			if (x.Write(this, &Buf, &Prog))
			{
				GCopyStreamer Cp;
				Prog.SetDescription("Writing XML...");
				LYield();
				if (Cp.Copy(&Buf, &f))
					d->Dirty = false;
			}
			else LgiTrace("%s:%i - Failed to write XML.\n", _FL);
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
			// int Id;
			// for (int Flags = d->UserNodeFlags.First(&Id); Flags >= 0; Flags = d->UserNodeFlags.Next(&Id))
			for (auto i : d->UserNodeFlags)
			{
				f.Print("%i,%x\n", i.key, i.value);
			}

			d->UserFileDirty = false;
		}
	}

	printf("\tIdeProject::SaveFile %i %i\n", d->Dirty, d->UserFileDirty);

	return	!d->Dirty &&
			!d->UserFileDirty;
}

void IdeProject::SetDirty()
{
	d->Dirty = true;
	d->App->OnProjectChange();
}

void IdeProject::SetUserFileDirty()
{
	d->UserFileDirty = true;
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
	return ::CheckExists(GetBasePath(), p, [](LString &o, const char *i) {
			o = i;
		}, Debug);
}

bool IdeProject::CheckExists(LAutoString &p, bool Debug)
{
	return ::CheckExists(GetBasePath(), p, [](LAutoString &o, const char *i) {
			o.Reset(NewStr(i));
		}, Debug);
}

bool IdeProject::SetClean()
{
	// printf("IdeProject::SetClean %i %i\n", d->Dirty, d->UserFileDirty);
	if (d->Dirty || d->UserFileDirty)
	{
		if (!ValidStr(d->FileName))
		{
			LFileSelect s;
			s.Parent(Tree);
			s.Name("Project.xml");
			if (s.Save())
			{
				d->FileName = s.Name();
				d->UserFile = d->FileName + "." + LCurrentUserName();
				d->App->OnFile(d->FileName, true);
				Update();
			}
			else return false;
		}

		SaveFile();
	}

	for (auto i:*this)
	{
		ProjectNode *p = dynamic_cast<ProjectNode*>(i);
		if (!p) break;

		p->SetClean();
	}

	return true;
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
				LInput Name(Tree, "", "Name:", AppName);
				if (Name.DoModal())
				{
					GetSubFolder(this, Name.GetStr(), true);
				}
				break;
			}
			case IDM_WEB_FOLDER:
			{
				WebFldDlg Dlg(Tree, 0, 0, 0);
				if (Dlg.DoModal())
				{
					if (Dlg.Ftp && Dlg.Www)
					{
						IdeCommon *f = GetSubFolder(this, Dlg.Name, true);
						if (f)
						{
							f->SetAttr(OPT_Ftp, Dlg.Ftp);
							f->SetAttr(OPT_Www, Dlg.Www);
						}
					}
				}
				break;
			}
			case IDM_BUILD:
			{
				StopBuild();
				Build(true, d->App->IsReleaseMode());
				break;
			}
			case IDM_CLEAN_PROJECT:
			{
				Clean(false, d->App->IsReleaseMode());
				break;
			}
			case IDM_CLEAN_ALL:
			{
				Clean(true, d->App->IsReleaseMode());
				break;
			}
			case IDM_REBUILD_PROJECT:
			{
				StopBuild();
				Clean(false, d->App->IsReleaseMode());
				Build(false, d->App->IsReleaseMode());
				break;
			}
			case IDM_REBUILD_ALL:
			{
				StopBuild();
				Clean(true, d->App->IsReleaseMode());
				Build(true, d->App->IsReleaseMode());
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
				if (d->Settings.Edit(Tree))
				{
					SetDirty();
				}
				break;
			}
			case IDM_INSERT_DEP:
			{
				LFileSelect s;
				s.Parent(Tree);
				s.Type("Project", "*.xml");
				if (s.Open())
				{
					ProjectNode *New = new ProjectNode(this);
					if (New)
					{
						New->SetFileName(s.Name());
						New->SetType(NodeDependancy);
						InsertTag(New);
						SetDirty();
					}
				}
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
	ProjectNode *n = d->Nodes.Find(Path);	
	if (!n && FuzzyMatch)
	{
		// No match, do partial matching.
		const char *Leaf = LGetLeaf(Path);
		auto PathLen = strlen(Path);
		auto LeafLen = strlen(Leaf);
		uint32_t MatchingScore = 0;

		// Traverse all nodes and try and find the best fit.
		// const char *p;
		// for (ProjectNode *Cur = d->Nodes.First(&p); Cur; Cur = d->Nodes.Next(&p))
		for (auto Cur : d->Nodes)
		{
			int CurPlatform = Cur.value->GetPlatforms();
			uint32_t Score = 0;

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
			if (Score && (CurPlatform & PLATFORM_CURRENT) != 0)
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
		
		char *Dsp = LReadTextFile(File);
		if (Dsp)
		{
			GToken Lines(Dsp, "\r\n");
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
			
			DeleteArray(Dsp);
		}
	}
}

IdeProjectSettings *IdeProject::GetSettings()
{
	return &d->Settings;
}

bool IdeProject::BuildIncludePaths(LArray<LString> &Paths, bool Recurse, bool IncludeSystem, IdePlatform Platform)
{
	List<IdeProject> Projects;
	if (Recurse)
		GetChildProjects(Projects);
	Projects.Insert(this, 0);

	LHashTbl<StrKey<char>, bool> Map;
	
	for (auto p: Projects)
	{
		LString ProjInclude = d->Settings.GetStr(ProjIncludePaths, NULL, Platform);
		LAutoString Base = p->GetBasePath();
		
		const char *Delim = ",;\r\n";
		LString::Array In, Out;
		LString::Array a = ProjInclude.SplitDelimit(Delim);
		In = a;
		
		if (IncludeSystem)
		{
			LString SysInclude = d->Settings.GetStr(ProjSystemIncludes, NULL, Platform);
			a = SysInclude.SplitDelimit(Delim);
			In.SetFixedLength(false);
			In.Add(a);
		}
		
		for (unsigned i=0; i<In.Length(); i++)
		{
			auto p = ToPlatformPath(In[i], Platform);

			char *Path = p;
			if (*Path == '`')
			{
				// Run config app to get the full path list...
				p = p.Strip("`");
				LString::Array a = p.Split(" ", 1);
				LSubProcess Proc(a[0], a.Length() > 1 ? a[1].Get() : NULL);
				LStringPipe Buf;
				if (Proc.Start())
				{
					Proc.Communicate(&Buf);

					LString result = Buf.NewGStr();
					a = result.Split(" \t\r\n");
					for (int i=0; i<a.Length(); i++)
					{
						char *inc = a[i];
						if (inc[0] == '-' &&
							inc[1] == 'I')
						{
							Out.New() = a[i](2,-1);
						}
					}
				}
				else LgiTrace("%s:%i - Error: failed to run process for '%s'\n", _FL, p.Get());
			}
			else
			{
				// Add path literal
				Out.New() = Path;
			}
		}
			
		for (int i=0; i<Out.Length(); i++)
		{
			char *Path = Out[i];
			char *Full = 0, Buf[MAX_PATH];
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
				LMakePath(Buf, sizeof(Buf), Base, Path);
				Full = Buf;
			}
			else
			{
				Full = Out[i];
			}
			
			if (!Map.Find(Full))
				Map.Add(Full, true);
		}

		// Add paths for the headers in the project... bit of a hack but it'll
		// help it find them if the user doesn't specify the paths in the project.
		LArray<ProjectNode*> Nodes;
		if (p->GetAllNodes(Nodes))
		{
			auto Base = p->GetFullPath();
			if (Base)
			{
				LTrimDir(Base);

				for (auto &n: Nodes)
				{
					if (n->GetType() == NodeHeader &&				// Only look at headers.
						(n->GetPlatforms() & (1 << Platform)) != 0)	// Exclude files not on this platform.
					{
						auto f = n->GetFileName();
						if (Stristr(f, "linux"))
						{
							int as=0;
						}

						char p[MAX_PATH];
						if (f &&
							LMakePath(p, sizeof(p), Base, f))
						{
							char *l = strrchr(p, DIR_CHAR);
							if (l)
								*l = 0;
							if (!Map.Find(p))
							{
								Map.Add(p, true);
							}
						}
					}
				}
			}
		}
	}

	// char *p;
	// for (bool b = Map.First(&p); b; b = Map.Next(&p))
	for (auto p : Map)
		Paths.Add(p.key);

	return true;
}

void IdeProjectPrivate::CollectAllFiles(LTreeNode *Base, LArray<ProjectNode*> &Files, bool SubProjects, int Platform)
{
	for (auto i:*Base)
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

LString IdeProject::GetTargetName(IdePlatform Platform)
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
			char Target[MAX_PATH];
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

LString IdeProject::GetTargetFile(IdePlatform Platform)
{
	LString Ret;
	LString Target = GetTargetName(PlatformCurrent);
	if (Target)
	{
		const char *TargetType = d->Settings.GetStr(ProjTargetType);
		if (TargetType)
		{
			if (!stricmp(TargetType, "Executable"))
			{
				Ret = Target;
			}
			else if (!stricmp(TargetType, "DynamicLibrary"))
			{
				char t[MAX_PATH];
				auto DefExt = PlatformDynamicLibraryExt(Platform);
				strcpy_s(t, sizeof(t), Target);
				char *ext = LGetExtension(t);
				if (!ext)
					sprintf(t + strlen(t), ".%s", DefExt);
				else if (stricmp(ext, DefExt))
					strcpy(ext, DefExt);				
				Ret = t;
			}
			else if (!stricmp(TargetType, "StaticLibrary"))
			{
				Ret.Printf("lib%s.%s", Target.Get(), PlatformSharedLibraryExt(Platform));
			}
		}
	}
	
	return Ret;
}

struct Dependency
{
	bool Scanned;
	LAutoString File;
	
	Dependency(const char *f)
	{
		Scanned = false;
		File.Reset(NewStr(f));
	}
};

bool IdeProject::GetAllDependencies(LArray<char*> &Files, IdePlatform Platform)
{
	if (!GetTree()->Lock(_FL))
		return false;
		
	LHashTbl<StrKey<char>, Dependency*> Deps;
	LAutoString Base = GetBasePath();
	
	// Build list of all the source files...
	LArray<LString> Src;
	CollectAllSource(Src, Platform);
	
	// Get all include paths
	LArray<LString> IncPaths;
	BuildIncludePaths(IncPaths, false, false, Platform);
	
	// Add all source to dependencies
	for (int i=0; i<Src.Length(); i++)
	{
		char *f = Src[i];
		Dependency *dep = Deps.Find(f);
		if (!dep)
			Deps.Add(f, new Dependency(f));
	}
	
	// Scan all dependencies for includes
	LArray<Dependency*> Unscanned;
	do
	{
		// Find all the unscanned dependencies
		Unscanned.Length(0);
		// for (Dependency *d = Deps.First(); d; d = Deps.Next())
		for (auto d : Deps)
		{
			if (!d.value->Scanned)
				Unscanned.Add(d.value);
		}		

		for (int i=0; i<Unscanned.Length(); i++)
		{
			// Then scan source for includes...
			Dependency *d = Unscanned[i];
			d->Scanned = true;
			
			char *Src = d->File;
			char Full[MAX_PATH];
			if (LIsRelativePath(d->File))
			{
				LMakePath(Full, sizeof(Full), Base, d->File);
				Src = Full;
			}

			LArray<char*> SrcDeps;
			if (GetDependencies(Src, IncPaths, SrcDeps, Platform))
			{
				for (int n=0; n<SrcDeps.Length(); n++)
				{
					// Add include to dependencies...
					char *File = SrcDeps[n];

					if (LIsRelativePath(File))
					{
						LMakePath(Full, sizeof(Full), Base, File);
						File = Full;
					}

					if (!Deps.Find(File))
					{						
						Deps.Add(File, new Dependency(File));
					}
				}
				SrcDeps.DeleteArrays();
			}
		}
	}
	while (Unscanned.Length() > 0);
	
	// for (Dependency *d = Deps.First(); d; d = Deps.Next())
	for (auto d : Deps)
	{
		Files.Add(d.value->File.Release());
	}
	
	Deps.DeleteObjects();
	GetTree()->Unlock();
	
	return true;
}

bool IdeProject::GetDependencies(const char *InSourceFile, LArray<LString> &IncPaths, LArray<char*> &Files, IdePlatform Platform)
{
	LString SourceFile = InSourceFile;
	if (!CheckExists(SourceFile))
	{
		LgiTrace("%s:%i - can't read '%s'\n", _FL, SourceFile.Get());
		return false;
	}

	LAutoString c8(LReadTextFile(SourceFile));
	if (!c8)
		return false;

	LArray<char*> Headers;
	if (!BuildHeaderList(c8, Headers, IncPaths, false))
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
	Headers.DeleteArrays();
	
	return true;
}

int MakefileThread::Instances = 0;

bool IdeProject::CreateMakefile(IdePlatform Platform, bool BuildAfterwards)
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
	Hit = 0;
	MultiSelect(true);
}

void IdeTree::OnCreate()
{
	SetWindow(this);
}

void IdeTree::OnDragExit()
{
	SelectDropTarget(0);
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
			LYield();
			Ts = Now;
		}
	}
	else if (Now - Ts > 1000)
	{
		DoModeless();
		SetAlwaysOnTop(true);
	}
}

int AddFilesProgress::OnNotify(LViewI *c, LNotification n)
{
	if (c->GetId() == IDCANCEL)
		Cancel = true;
	return 0;
}
