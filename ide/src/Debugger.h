#ifndef _GDEBUGGER_H_
#define _GDEBUGGER_H_

#include "lgi/common/Variant.h"
#include "lgi/common/StringClass.h"

#include "IdePlatform.h"

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
	enum TScope
	{
		Local,
		Arg,
		Global
	};

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

		BreakPoint(const char *sym)
		{
			Symbol = sym;
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
		TScope Scope;
		LString Name;
		LString Type;
		LVariant Value;
		LString Detail;
	};

	using TStatusCb = std::function<void(bool)>;
	using TStringCb = std::function<void(LString)>;
	using TStringsCb = std::function<void(LString::Array&)>;
	using TVarArray = LArray<Variable>;
	using TVarsCb = std::function<void(LError&,TVarArray&)>;
	
	virtual ~LDebugger() {}
	
	virtual bool Load(LDebugEvents *EventHandler, const char *Exe, const char *Args, bool RunAsAdmin, const char *InitDir, const char *Env) = 0;
	virtual bool AttachTo(LDebugEvents *EventHandler, int Pid) = 0;
	virtual void Restart(TStatusCb cb) = 0;
	virtual void Unload(TStatusCb cb) = 0;
	
	virtual void GetCallStack(TStringsCb cb) = 0;
	virtual void GetThreads(std::function<void(LArray<LString>&, int)> cb) = 0;
	virtual void SetCurrentThread(int ThreadId, TStatusCb cb) = 0;
	virtual void SetFrame(int Frame, TStatusCb cb) = 0;

	virtual void SetBreakPoint(BreakPoint *bp, TStatusCb cb) = 0;
	virtual void RemoveBreakPoint(BreakPoint *bp, TStatusCb cb) = 0;
	virtual bool GetBreakPoints(LArray<BreakPoint> &bps) = 0;

	virtual void GetVariables(bool Locals, bool Detailed, TVarArray *init, TVarsCb cb) = 0;
	virtual bool PrintObject(const char *Var, LStream *Output) = 0;
	virtual bool ReadMemory(LString &BaseAddr, int Length, LArray<uint8_t> &OutBuf, LString *ErrorMsg = NULL) = 0;
	virtual void GetRegisters(TStringsCb cb) = 0;

	virtual bool GetLocation(LString &File, int &Line) = 0;
	virtual bool SetLocation(const char *File, int Line) = 0;

	virtual bool GetRunning() = 0;
	virtual void SetRunning(bool Run, TStatusCb cb) = 0;
	virtual void StepInto(TStatusCb cb) = 0;
	virtual void StepOver(TStatusCb cb) = 0;
	virtual void StepOut(TStatusCb cb) = 0;
	virtual void Break(TStatusCb cb, bool SuppressFileLine = false) = 0;
	
	virtual void UserCommand(const char *Cmd, TStatusCb cb) = 0;
};

extern LDebugger *CreateGdbDebugger(LStream *Log,
									class ProjectBackend *Backend,
									IdePlatform platform,
									LStream *networkLog);

#endif
