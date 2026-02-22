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

class App :
	public LApp,
	public LScriptContext,
	public LVmCallback
{
	LScriptEngine *Engine;
	LString SrcFile;
	ConsoleLog Log;

	LStream *GetLog()
	{
		return &Log;
	}

	bool CallCallback(LVirtualMachine &Vm, LString CallbackName, LScriptArguments &Args)
	{
		Args.Throw(_FL, "Not implemented.");
		return false;
	}

	LVmDebugger *AttachVm(LVirtualMachine *Vm, LCompiledCode *Code, const char *Assembly)
	{
		LAutoPtr<LVirtualMachine> vm(new LVirtualMachine(Vm));
		LAutoPtr<LCompiledCode> code(new LCompiledCode(*Code));
		return new LVmDebuggerWnd(NULL, this, vm, code, Assembly);
	}

	bool CompileScript(LAutoPtr<LCompiledCode> &Output, const char *FileName, const char *Source)
	{
		return false;
	}
	
public:
	int Status;

	const char* GetClass() override { return "App"; }

	App(OsAppArguments &AppArgs) : LApp(AppArgs, "LgiScript")
	{
		LFile::Path p(LSP_APP_INSTALL);
		p += "Resources";
		p += "LgiScript.lr8";
		LgiGetResObj(true, p);	
	
		Engine = NULL;
		Status = 0;
	}

	LHostFunc *GetCommands() { return NULL; }
	void SetEngine(LScriptEngine *Eng) { Engine = Eng; }

	void OnReceiveFiles(LArray<const char*> &Files)
	{
		for (unsigned i=0; i<Files.Length(); i++)
		{
			if (!RunScript(Files[i]))
			{
				Status = -1;
			}
		}
	}
	
	LString GetIncludeFile(const char *FileName) override
	{
		char p[MAX_PATH_LEN];
		LMakePath(p, sizeof(p), SrcFile, "..");
		LMakePath(p, sizeof(p), p, FileName);
		if (LFileExists(p))
		{
			LFile f(p);
			if (f)
				return f.Read();
		}
		
		return NULL;
	}
	
	bool RunScript(const char *File)
	{
		auto Disassemble = LAppInst->GetOption("disassemble");
		if (!LFileExists(File))
		{
			printf("Error: '%s' not found.\n", File);
			return false;
		}
		
		SrcFile = File;		
		LScriptEngine Eng(NULL, this, this);
		Eng.SetConsole(&Log);

		auto Src = LReadFile(SrcFile);
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
		auto s = Eng.Run(Obj, &Ret);
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
				auto a = LReadFile(f);
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