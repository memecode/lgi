#include "lgi/common/Lgi.h"
#include "lgi/common/Scripting.h"
#include "../src/common/Coding/ScriptingPriv.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/LgiRes.h"

struct ConsoleLog : public LStream
{
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags)
	{
		return printf("%.*s", (int)Size, (char*)Ptr);
	}
};

class App : public LApp, public LScriptContext, public LVmDebuggerCallback
{
	LScriptEngine *Engine;
	LAutoString SrcFile;
	ConsoleLog Log;

	LVmDebugger *AttachVm(LVirtualMachine *Vm, LCompiledCode *Code, const char *Assembly)
	{
		return new LVmDebuggerWnd(NULL, this, Vm, Code, Assembly);
	}

	bool CompileScript(LAutoPtr<LCompiledCode> &Output, const char *FileName, const char *Source)
	{
		return false;
	}
	
public:
	int Status;

	App(OsAppArguments &AppArgs) : LApp(AppArgs, "LgiScript")
	{
		LFile::Path p(LSP_APP_INSTALL);
		p += "Resources";
		p += "LgiScript.lr8";
		LgiGetResObj(true, p);	
	
		Engine = NULL;
		Status = 0;
	}

	GHostFunc *GetCommands() { return NULL; }
	void SetEngine(LScriptEngine *Eng) { Engine = Eng; }

	void OnReceiveFiles(LArray<const char*> &Files)
	{
		for (int i=0; i<Files.Length(); i++)
		{
			if (!RunScript(Files[i]))
			{
				Status = -1;
			}
		}
	}
	
	char *GetIncludeFile(char *FileName)
	{
		char p[MAX_PATH_LEN];
		LMakePath(p, sizeof(p), SrcFile, "..");
		LMakePath(p, sizeof(p), p, FileName);
		if (LFileExists(p))
		{
			return ::LReadTextFile(p);
		}
		
		return NULL;
	}
	
	bool RunScript(const char *File)
	{
		bool Disassemble = LAppInst->GetOption("disassemble");
		if (!LFileExists(File))
		{
			printf("Error: '%s' not found.\n", File);
			return false;
		}
		
		if (!SrcFile.Reset(NewStr(File)))
		{
			printf("Error: Mem alloc failed.\n");
			return false;
		}
		
		LScriptEngine Eng(NULL, NULL, this);
		Eng.SetConsole(&Log);

		LAutoString Src(::LReadTextFile(SrcFile));
		if (!Src)
		{
			printf("Error: Failed to read '%s'.\n", SrcFile.Get());
			return false;
		}
		
		LAutoPtr<LCompiledCode> Obj;
		if (!Eng.Compile(Obj, NULL, Src, File))
		{
			printf("Error: Compilation failed '%s'.\n", SrcFile.Get());
			return false;
		}
		
		LVariant Ret;
		LExecutionStatus s = Eng.Run(Obj, &Ret);
		if (s == ScriptError)
		{
			printf("Error: Execution failed '%s'.\n", SrcFile.Get());
			return false;
		}
		else if (s == ScriptWarning)
		{
			printf("Warning: Execution succeeded with warnings '%s'.\n", SrcFile.Get());
			return false;
		}
		
		if (Ret.CastInt32())
			printf("Success: %s\n", File);
		else
		{
			printf("Failed: %s\n", File);
			s = ScriptError;
		}
		
		if (Disassemble)
		{
			LString f = File;
			int Idx = f.RFind(".");
			if (Idx > 0)
			{
				f = f(0, Idx) + ".asm";
				LAutoString a(LReadTextFile(f));
				if (a)
				{
					printf("%s\n", a.Get());
				}
			}
		}
		
		return s == ScriptSuccess;
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	App a(AppArgs);
	if (a.IsOk())
	{
		a.OnCommandLine();
	}
	return a.Status;
}