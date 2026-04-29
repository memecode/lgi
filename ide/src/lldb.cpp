#include "lgi/common/Lgi.h"

#include "Debugger.h"
#include "lldb.h"

/*	Install:

	API docs:
		https://lldb.llvm.org/cpp_reference/namespacelldb.html

	Linux:
		sudo apt install liblldb-dev
		headers in: /usr/lib/llvm-18/include/lldb
*/

#if __has_include ("lldb/API/LLDB.h")
#include <lldb/API/LLDB.h>
#define HAVE_LLDB	1

class LLdb : public LDebugger
{
	BreakPointStore *bpStore = nullptr;
	LStream *log = nullptr;
	SystemIntf *backend = nullptr;
	SysPlatform platform = PlatformUnknown;
	LStream *networkLog = nullptr;

public:
	LLdb(	BreakPointStore *BpStore,
			LStream *Log,
			SystemIntf *Backend,
			SysPlatform Platform,
			LStream *NetworkLog)
			: bpStore(BpStore)
			, log(Log)
			, backend(Backend)
			, platform(Platform)
			, networkLog(NetworkLog)			
	{
	}
	
	bool Load(LDebugEvents *EventHandler, const char *Exe, const char *Args, bool RunAsAdmin, const char *InitDir, const char *Env)
	{
		return false;
	}
	
	bool AttachTo(LDebugEvents *EventHandler, int Pid)
	{
		return false;
	}
	
	void Restart(TStatusCb cb)
	{
	}
	
	void Unload(TStatusCb cb)
	{
	}
	
	void GetCallStack(TStringsCb cb)
	{
	}
	
	void GetThreads(std::function<void(LArray<LString>&, int)> cb)
	{
	}
	
	void SetCurrentThread(int ThreadId, TStatusCb cb)
	{
	}
	
	void SetFrame(int Frame, TStatusCb cb)
	{
	}

	void GetVariables(bool Locals, bool Detailed, TVarArray *init, TVarsCb cb)
	{
	}
	
	void PrintObject(const char *Var, TStringCb cb)
	{
	}
	
	bool ReadMemory(LString &BaseAddr, int Length, LArray<uint8_t> &OutBuf, LString *ErrorMsg = NULL)
	{
		return false;
	}
	
	void GetRegisters(TStringsCb cb)
	{
	}

	bool GetLocation(LString &File, int &Line)
	{
		return false;
	}
	
	bool SetLocation(const char *File, int Line)
	{
		return false;
	}

	bool GetRunning()
	{
		return false;
	}
	
	void SetRunning(bool Run, TStatusCb cb)
	{
	}	
	
	void StepInto(TStatusCb cb)
	{
	}
	
	void StepOver(TStatusCb cb)
	{
	}
	
	void StepOut(TStatusCb cb)
	{
	}
	
	void Break(TStatusCb cb, bool SuppressFileLine = false)
	{
	}	
	
	void UserCommand(const char *Cmd, TStatusCb cb)
	{
	}
};

#else
#define HAVE_LLDB	0
#endif

LDebugger *CreateLldbDebugger(	BreakPointStore *bpStore,
								LStream *Log,
								SystemIntf *Backend,
								SysPlatform platform,
								LStream *networkLog)
{
	#if HAVE_LLDB
		return new LLdb(bpStore, Log, Backend, platform, networkLog);
	#else
		return nullptr;
	#endif
}
