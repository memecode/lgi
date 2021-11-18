/// \file
#ifndef _LGI_SCRIPTING_H_
#define _LGI_SCRIPTING_H_

#include "lgi/common/Variant.h"
#include "lgi/common/List.h"

class LScriptContext;
class LScriptEnginePrivate;
class LVmDebuggerCallback;
class LVirtualMachine;

class LScriptArguments : public LArray<LVariant*>
{
	friend class LScriptEngine;
	friend class LVirtualMachine;

	LVirtualMachine *Vm;
	LStream *Console;
	LVariant _Return;
	LVariant *PtrRet;

public:
	static LStream NullConsole;

	LScriptArguments(LVirtualMachine *vm, LVariant *ret = NULL, LStream *console = NULL)
	{
		Vm = vm;
		if (ret)
			PtrRet = ret;
		else
			PtrRet = &_Return;
		
		if (console)
			Console = console;
		else
			Console = &NullConsole;
	}

	LVirtualMachine *GetVm() { return Vm; }
	LVariant *GetReturn() { return PtrRet; }
	LStream *GetConsole() { return Console; }
	bool Throw(const char *File, int Line, const char *Msg, ...);
};

typedef bool (LScriptContext::*ScriptCmd)(LScriptArguments &Args);

#define SCOPE_REGISTER		0
#define SCOPE_LOCAL			1
#define SCOPE_GLOBAL		2
#define SCOPE_OBJECT		3
#define SCOPE_RETURN		4
#define SCOPE_MAX			5

/// Execution status
enum LExecutionStatus
{
	ScriptError,
	ScriptWarning,
	ScriptSuccess,
};

/// Various type of methods
enum LFuncType
{
	/// No method type.
	NullFunc,
	/// This method is provided by the hosting application.
	HostFunc,
	/// This method is defined in the script itself.
	ScriptFunc,
	/// This method is defined in an external library.
	ExternFunc
};

struct LFunc
{
	LFuncType Type;
	LString Method;
	bool InUse;

	LFunc(const char *m = 0, LFuncType t = NullFunc)
	{
		Type = t;
		Method = m;
		InUse = false;

		// LStackTrace("%p alloc\n", this);
	}

	virtual ~LFunc()
	{
		// LAssert(!InUse);
	}

	virtual LExecutionStatus Call(LScriptContext *Ctx, LScriptArguments &Args) = 0;
};

struct GHostFunc : public LFunc
{
	LScriptContext *Context;
	LString Args;
	ScriptCmd Func;
	
	GHostFunc(const GHostFunc &f)
	{
		Context = f.Context;
		Args = f.Args;
		Method = f.Method;
		Func = f.Func;
	}
	
	GHostFunc(const char *method, const char *args, ScriptCmd proc) : LFunc(method, HostFunc)
	{
		Args = args;
		Func = proc;
	}

	LExecutionStatus Call(LScriptContext *Ctx, LScriptArguments &Args) override;
};

struct LExternFunc : public LFunc
{
	struct ExternType
	{
		int Ptr;
		bool Unsigned;
		bool Out;
		int ArrayLen;
		LVariantType Base;
	};

	LAutoString Lib;
	ExternType ReturnType;
	LArray<ExternType> ArgType;

	LExecutionStatus Call(LScriptContext *Ctx, LScriptArguments &Args) override;
};

class LFunctionInfo : public GRefCount
{
	static int _Infos;

	int32 StartAddr = INVALID_ADDR;
	LString Name;

	// The reason why this is a pointer is because during the function compilation the frame
	// size is actually unknown. If the function calls itself then it won't know what
	// frame size to insert in the assembly (this is NULL).
	// In which case it has to insert a post compilation fix-up for the frame size.
	LAutoPtr<uint16> FrameSize;

	// The number and names of the parameters to the function.
	LArray<LString> Params;

public:
	static constexpr int32 INVALID_ADDR = -1;

	LFunctionInfo(const char *name)
	{
		if (name)
			Name = name;
	}

	~LFunctionInfo()
	{
	}
	
	char *GetName()
	{
		return Name;
	}

	LArray<LString> &GetParams()
	{
		return Params;
	}

	bool ValidFrameSize()
	{
		return FrameSize.Get() != NULL;
	}
	
	uint16 GetFrameSize()
	{
		if (!FrameSize)
		{
			LAssert(!"Invalid frame size");
			return 0;
		}
		return *FrameSize;
	}

	template<typename T>
	bool SetFrameSize(T size)
	{
		if (size >= 0xffff)
		{
			LAssert(!"Invalid frame size.");
			return false;
		}

		return FrameSize.Reset(new uint16(size));
	}

	bool ValidStartAddr()
	{
		return StartAddr != INVALID_ADDR;
	}

	int32 GetStartAddr()
	{
		LAssert(ValidStartAddr());
		return StartAddr;
	}

	bool SetStartAddr(int32_t addr)
	{
		if (addr < 0)
		{
			LAssert(!"Invalid start address");
			return false;
		}

		StartAddr = addr;
		return true;
	}
	
	LFunctionInfo &operator =(LFunctionInfo &f)
	{
		StartAddr = f.StartAddr;
		FrameSize = f.FrameSize;
		Name = f.Name;
		for (unsigned i=0; i<f.Params.Length(); i++)
		{
			Params[i] = f.Params[i];
		}
		return *this;
	}
};

class LScriptUtils
{
public:
	virtual ~LScriptUtils() {}

	int atoi(char16 *s);
	int64 atoi64(char16 *s);
	double atof(char16 *s);
	int htoi(char16 *s);
};

/// The base context class for scripting engines
class LScriptContext : public LScriptUtils
{
public:
	virtual ~LScriptContext() {}
	
	virtual GHostFunc *GetCommands() = 0;
	virtual char *GetIncludeFile(char *FileName) = 0;
	virtual LAutoString GetDataFolder() { return LAutoString(); }
	virtual LStream *GetLog() { return NULL; }
	virtual bool SetLog(LStream *Log) { return false; }

	// AddPrimitive: Add your primitive's functions in a derived class using the format:
	//
	//		bool MyPrim(LVariant *Ret, ArgumentArray &Args) { ... }
	//
	// Where you return true if successful or false on error.
};

class LVariables : public LArray<LVariant>
{
	friend class LVirtualMachinePriv;

	LHashTbl<ConstStrKey<char>,int> Lut;
	
public:
	int Scope;
	int NullIndex;
	LCustomType *Obj;
	
	LVariables(int scope)
	{
		Scope = scope;
		NullIndex = -1;
		Obj = NULL;
	}
	
	LVariables(LCustomType *obj)
	{
		Scope = SCOPE_OBJECT;
		NullIndex = -1;
		Obj = obj;
	}

	int Var(const char *n, bool create = false)
	{
		if (Obj)
		{
			return Obj->IndexOf(n);
		}
		else
		{
			int p = Lut.Find(n);
			if (p)
			{
				return p - 1;
			}

			if (create)
			{
				int Len = (int)Length();

				Lut.Add(n, Len + 1);
				Length(Len + 1);

				return Len;
			}
		}

		return -1;
	}
};

/// A block of compile byte code
class LCompiledCode
{
	friend class LCompilerPriv;
	friend class LVirtualMachinePriv;
	friend class GCompiler;
	friend class LVmDebuggerWnd;

	/// The global variables
	LVariables Globals;
	
	/// The byte code of all the instructions
	LArray<uint8_t> ByteCode;
	
	/// All the methods defined in the byte code and their arguments.
	LArray< GAutoRefPtr<LFunctionInfo> > Methods;
	
	/// All the externs defined in the code.
	LArray<LExternFunc*> Externs;
	
	/// All the user types defined
	LHashTbl<StrKey<char16>, class LCustomType*> Types;
	
	/// The original script details
	LString FileName;
	LString Source;
	
	/// The system context (all the system functions)
	LScriptContext *SysContext;
	
	/// Any user context (application functions)
	LScriptContext *UserContext;

	/// Debug info to map instruction address back to source line numbers
	LHashTbl<IntKey<NativeInt>, int> Debug;

public:
	LCompiledCode();
	LCompiledCode(LCompiledCode &copy);
	~LCompiledCode();

	/// Size of the byte code
	size_t Length() { return ByteCode.Length(); }
	/// Assignment operator
	LCompiledCode &operator =(const LCompiledCode &c);
	/// Gets a method defined in the code
	LFunctionInfo *GetMethod(const char *Name, bool Create = false);
	/// Sets a global variable
	LVariant *Set(const char *Name, LVariant &v);
	/// Gets the definition of a struct or custom type
	LCustomType *GetType(char16 *Name) { return Types.Find(Name); }
	/// Gets the file name this code was compiled from
	const char *GetFileName() { return FileName; }
	/// Gets the source
	const char *GetSource() { return Source; }
	/// Gets the source line number associated with an address
	int ObjectToSourceAddress(size_t ObjAddr);
	/// Turns an object address into a FileName:LineNumber string.
	const char *AddrToSourceRef(size_t ObjAddr);

	/// Sets the file name this code was compiled from
	void SetSource(const char *file, const char *src)
	{
		if (file != FileName.Get())
			FileName = file;
		Source = src;
	}
};

/// New compiler/byte code/VM scripting engine
class LScriptEngine
{
	class LScriptEnginePrivate *d;

public:
	LScriptEngine(LViewI *parent, LScriptContext *UserContext, LVmDebuggerCallback *Callback);
	~LScriptEngine();

	LStream *GetConsole();
	bool SetConsole(LStream *t);

	LCompiledCode *GetCurrentCode();
	bool Compile(	LAutoPtr<LCompiledCode> &Obj,
					LScriptContext *UserContext,
					const char *Script,
					const char *FileName = NULL,
					LDom *Args = NULL);
	LExecutionStatus Run(LCompiledCode *Obj, LVariant *Ret = NULL, const char *TempPath = NULL);
	LExecutionStatus RunTemporary(LCompiledCode *Obj, char *Script, LVariant *Ret = NULL);
	bool EvaluateExpression(LVariant *Result, LDom *VariableSource, char *Expression);
	bool CallMethod(LCompiledCode *Obj, const char *Method, LScriptArguments &Args);
	LScriptContext *GetSystemContext();
};

class LVirtualMachine;

class LVmDebugger : public LDom
{
public:
	/// Set the VM ownership flag.
	virtual void OwnVm(bool Own) = 0;
	/// Makes the debugger the owner of the compiled code
	virtual void OwnCompiledCode(LAutoPtr<LCompiledCode> Cc) = 0;
	/// Gets the code owned by the debugger
	virtual LCompiledCode *GetCode() = 0;
	/// Set the source and asm
	virtual void SetSource(const char *Mixed) = 0;
	/// Show UI and wait for user response
	virtual void Run() = 0;
	
	// Events
		/// Called to update the debugger UI to the new current execution position
		virtual void OnAddress(size_t Addr) = 0;
		/// Called when an error occurs executing the script
		virtual void OnError(const char *Msg) = 0;
		/// Called when execution starts or ends
		virtual void OnRun(bool Running) = 0;
};

class LVmDebuggerCallback : public LDom
{
public:
	/// Start a debugger instance to handle the execution in 'Vm'
	virtual LVmDebugger *AttachVm(LVirtualMachine *Vm, LCompiledCode *Code, const char *Assembly) = 0;
	/// Compile a new script
	virtual bool CompileScript(LAutoPtr<LCompiledCode> &Output, const char *FileName, const char *Source) = 0;
};

/// Debugger for vm script
class LVmDebuggerWnd : public LWindow, public LVmDebugger
{
	struct LScriptVmDebuggerPriv *d;

	void UpdateVariables(LList *Lst, LVariant *Arr, ssize_t Len, char Prefix);

public:
	LVmDebuggerWnd(LView *Parent, LVmDebuggerCallback *Callback, LVirtualMachine *Vm, LCompiledCode *Code, const char *Assembly);
	~LVmDebuggerWnd();

	void OwnVm(bool Own);
	void OnAddress(size_t Addr);
	void OnError(const char *Msg);
	void OnRun(bool Running);
	void SetSource(const char *Mixed);
	int OnNotify(LViewI *Ctrl, LNotification n);
	int OnCommand(int Cmd, int Event, OsView Wnd);
	bool OnRequestClose(bool OsShuttingDown);
	LMessage::Param OnEvent(LMessage *Msg);
	void LoadFile(const char *File);
	LStream *GetLog();
	void OwnCompiledCode(LAutoPtr<LCompiledCode> Cc);
	LCompiledCode *GetCode();
	
	void Run();
};

#endif

