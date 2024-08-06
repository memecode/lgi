#ifndef _GDEBUGGER_H_
#define _GDEBUGGER_H_

#include "lgi/common/Variant.h"
#include "lgi/common/StringClass.h"

#define DEBUG_SESSION_LOGGING		0

class LDebugEvents : public LStream
{
public:
	virtual ~LDebugEvents() {}
	
	virtual void OnState(bool Debugging, bool Running) = 0;
	virtual void OnFileLine(const char *File, int Line, bool CurrentIp) = 0;
	virtual void OnError(int Code, const char *Str) = 0;
	virtual void OnCrash(int Code) = 0;
	virtual void Ungrab() = 0;
	virtual void OnWarning(LString str) = 0;
};

class LDebugger
{
public:
	struct BreakPoint
	{
		int Index = 0;
		bool Added = false;

		// Use File:Line
		LString File;
		ssize_t Line = 0;
		// -or-
		// A symbol reference
		LString Symbol;
		
		BreakPoint()
		{
		}
		
		BreakPoint(const char *file, ssize_t line)
		{
			File = file;
			Line = line;
		}
		
		BreakPoint &operator =(const BreakPoint &b)
		{
			Index = b.Index;
			File = b.File;
			Line = b.Line;
			Symbol = b.Symbol;
			return *this;
		}
		
		bool operator ==(const BreakPoint &b)
		{
			if (File == b.File &&
				Line == b.Line &&
				Symbol == b.Symbol)
				return true;
			
			return false;
		}
		
		LString Save()
		{
			if (File && Line > 0)
				return LString::Fmt("file://%s:" LPrintfSSizeT, File.Get(), Line);
			else if (Symbol)
				return LString::Fmt("symbol://%s", Symbol.Get());
			
			LAssert(!"Invalid breakpoint");
			return LString();
		}
		
		bool Load(LString s)
		{
			auto sep = s.Find("://");
			if (sep < 0)
				return false;
			auto var = s(0, sep);
			auto value = s(sep+3, -1);
			if (var.Equals("file"))
			{
				auto p = value.SplitDelimit(":");
				LAssert(p.Length() == 2);
				File = p[0];
				Line = p[1].Int();
			}
			else if (var.Equals("symbol"))
			{
				Symbol = value;
			}
			else
			{
				LAssert(!"Invalid type");
				return false;
			}
			
			return true;			
		}
	};

	struct Variable
	{
		enum ScopeType
		{
			Local,
			Arg,
			Global
		}	Scope;
		
		LString Name;
		LString Type;
		LVariant Value;
		LString Detail;
	};
	
	virtual ~LDebugger() {}
	
	virtual bool Load(LDebugEvents *EventHandler, const char *Exe, const char *Args, bool RunAsAdmin, const char *InitDir, const char *Env) = 0;
	virtual bool AttachTo(LDebugEvents *EventHandler, int Pid) = 0;
	virtual bool Restart() = 0;
	virtual bool Unload() = 0;
	
	virtual bool GetCallStack(LArray<LAutoString> &Stack) = 0;
	virtual bool GetThreads(LArray<LString> &Threads, int *CurrentThread) = 0;
	virtual bool SetCurrentThread(int ThreadId) = 0;
	virtual bool GetFrame(int &Frame, LAutoString &File, int &Line) = 0;
	virtual bool SetFrame(int Frame) = 0;

	virtual bool SetBreakPoint(BreakPoint *bp) = 0;
	virtual bool RemoveBreakPoint(BreakPoint *bp) = 0;
	virtual bool GetBreakPoints(LArray<BreakPoint> &bps) = 0;

	virtual bool GetVariables(bool Locals, LArray<Variable> &vars, bool Detailed) = 0;
	virtual bool PrintObject(const char *Var, LStream *Output) = 0;
	virtual bool ReadMemory(LString &BaseAddr, int Length, LArray<uint8_t> &OutBuf, LString *ErrorMsg = NULL) = 0;
	virtual bool GetRegisters(LStream *Out) = 0;

	virtual bool GetLocation(LAutoString &File, int &Line) = 0;
	virtual bool SetLocation(const char *File, int Line) = 0;

	virtual bool GetRunning() = 0;
	virtual bool SetRunning(bool Run) = 0;
	virtual bool StepInto() = 0;
	virtual bool StepOver() = 0;
	virtual bool StepOut() = 0;
	virtual bool Break(bool SuppressFileLine = false) = 0;
	
	virtual bool UserCommand(const char *Cmd) = 0;
};

#ifndef HAIKU
extern LDebugger *CreateGdbDebugger(LStream *Log);
#endif

#endif
