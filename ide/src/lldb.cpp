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
#include <lldb/API/SBDebugger.h>
#define HAVE_LLDB	1

class LLdb :
	public LDebugger,
	public LMutex,
	public LThread,
	public LCancel
{
	// Breakpoints:
	BreakPointStore *bpStore = nullptr;
	int BpCallback = BreakPointStore::INVALID_ID;
	void OnBpStoreEvent(BreakPointStore::TEvent event, int param)
	{
		switch (event)
		{
			case BreakPointStore::TStoreDeleted:
				bpStore = nullptr;
				break;
			case BreakPointStore::TBreakPointAdded:
			case BreakPointStore::TBreakPointDeleted:
			case BreakPointStore::TBreakPointEnabled:
			case BreakPointStore::TBreakPointDisabled:
			case BreakPointStore::TBreakPointModified:
				// FIXME: impl...
				break;
		}
	}
	
	// Platform support:
	SystemIntf *backend = nullptr;
	SysPlatform platform = PlatformUnknown;
	LDebugEvents *events = nullptr;

	// Logging:
	LStream *log = nullptr;
	LStream *networkLog = nullptr;
	
	// Impl:	
	lldb::SBDebugger	debugger;
	lldb::SBTarget		target;
	lldb::SBProcess		process;
	
	struct Cmd
	{
		enum TCmd {
		} type;
		
	};

	int Main()
	{
		debugger = lldb::SBDebugger::Create(true);
		
		while (!IsCancelled())
		{
			LSleep(10);
		}
		
		return 0;
	}

public:
	LLdb(	BreakPointStore *BpStore,
			LStream *Log,
			SystemIntf *Backend,
			SysPlatform Platform,
			LStream *NetworkLog)
		: LMutex("LLdb.Lck")
		, LThread("LLdb.Th")
		, bpStore(BpStore)
		, log(Log)
		, backend(Backend)
		, platform(Platform)
		, networkLog(NetworkLog)
	{
	
		if (bpStore)
			BpCallback = bpStore->AddCallback([this](auto event, auto param)
				{
					OnBpStoreEvent(event, param);
				});
				
		Run();
	}
	
	~LLdb()
	{
		Cancel();
		
		if (bpStore && BpCallback != BreakPointStore::INVALID_ID)
			bpStore->DeleteCallback(BpCallback);
		
		WaitForExit();
	}
	
	bool Load(LDebugEvents *EventHandler, const char *Exe, const char *Args, bool RunAsAdmin, const char *InitDir, const char *Env)
	{
		events = EventHandler;
		
		// https://lldb.llvm.org/python_api/lldb.SBDebugger.html
		target = debugger.CreateTargetWithFileAndArch(Exe, LLDB_ARCH_DEFAULT);
		if (target)
		{
			process = target.LaunchSimple(nullptr, nullptr, InitDir);
		}
		
		return false;
	}
	
	bool AttachTo(LDebugEvents *EventHandler, int Pid)
	{
		events = EventHandler;

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
