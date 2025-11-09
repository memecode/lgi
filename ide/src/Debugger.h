#ifndef _GDEBUGGER_H_
#define _GDEBUGGER_H_

#include "lgi/common/Variant.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/SystemIntf.h"

#include "BreakPointStore.h"

#define DEBUG_SESSION_LOGGING		1
#if DEBUG_SESSION_LOGGING
	#define DBG_LOG(...)			LgiTrace("--- " __VA_ARGS__)
#else
	#define DBG_LOG(...)			;
#endif

class LDebugEvents : public LStream
{
public:
	virtual ~LDebugEvents() {}
	
	virtual void OnState(bool Debugging, bool Running) = 0;
	virtual void OnFileLine(const char *File, int Line, bool CurrentIp) = 0;
	virtual void OnError(LError err) = 0;
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

	struct Variable
	{
		TScope Scope;
		LString Name;
		LString Type;
		LVariant Value;
		LString Detail;
	};

	using TVarArray = LArray<Variable>;

	using TStatusCb = std::function<void(bool)>;
	using TStringCb = std::function<void(LString)>;
	using TStringsCb = std::function<void(LString::Array&)>;
	using TVarsCb = std::function<void(LError&,TVarArray&)>;
	using TStatusIntCb = std::function<void(LError&,int)>;
	
	virtual ~LDebugger() {}
	
	virtual bool Load(LDebugEvents *EventHandler, const char *Exe, const char *Args, bool RunAsAdmin, const char *InitDir, const char *Env) = 0;
	virtual bool AttachTo(LDebugEvents *EventHandler, int Pid) = 0;
	virtual void Restart(TStatusCb cb) = 0;
	virtual void Unload(TStatusCb cb) = 0;
	
	virtual void GetCallStack(TStringsCb cb) = 0;
	virtual void GetThreads(std::function<void(LArray<LString>&, int)> cb) = 0;
	virtual void SetCurrentThread(int ThreadId, TStatusCb cb) = 0;
	virtual void SetFrame(int Frame, TStatusCb cb) = 0;

	virtual void GetVariables(bool Locals, bool Detailed, TVarArray *init, TVarsCb cb) = 0;
	virtual void PrintObject(const char *Var, TStringCb cb) = 0;
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

extern LDebugger *CreateGdbDebugger(BreakPointStore *bpStore,
									LStream *Log,
									class SystemIntf *Backend,
									SysPlatform platform,
									LStream *networkLog);

#endif
