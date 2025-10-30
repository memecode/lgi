/*
	This needs to be migrated to GDB/MI:
	https://sourceware.org/gdb/current/onlinedocs/gdb.html/GDB_002fMI.html

	The existing implemenation isn't very robust or fast.
*/

#define DEBUG_STOP_ON_GTK_ERROR	0
#define DEBUG_STRUCT_LOGGING	1
static bool DEBUG_SHOW_GDB_IO = false;
#define ECHO_GDB_OUTPUT			0
#define ERR_CTX					__FUNCTION__, _FL

#include <atomic>
#ifdef POSIX
	#include <sys/types.h>
	#include <signal.h>
	#include <errno.h>
	#include <unistd.h>
#endif
#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/DocView.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/Token.h"
#include "lgi/common/RemoveAnsi.h"
#if DEBUG_STRUCT_LOGGING
#include "lgi/common/StructuredLog.h"
#endif
#include "lgi/common/SystemIntf.h"

#include "Debugger.h"
#include "LgiIde.h"

const char sPrompt[] = "(gdb) ";


// General logging
#if 0
#define LOG(...)	printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

// Breakpoint logging
#if 0
#define BP_LOG(...)	printf(__VA_ARGS__)
#else
#define BP_LOG(...)
#endif

// Command loggins
#if 0
#define CMD_LOG(...) printf(__VA_ARGS__)
#else
#define CMD_LOG(...)
#endif


class Callback
{
public:
	virtual void GetResponse(const char *c, LDebugger::TStringCb cb) = 0;
};

class Gdb :
	public LDebugger,
	public LThread,
	public Callback,
	public LCancel,
	public LMutex
{
	LDebugEvents *Events = nullptr;
	SystemIntf *backend = nullptr;
	bool backendProcessRunning = false;
	BreakPointStore *bpStore = nullptr;
	int bpStoreCallbackId = BreakPointStore::INVALID_ID;
	SysPlatform platform = PlatformCurrent;
	constexpr static int INVALID_ID = BreakPointStore::INVALID_ID;
	using TLock = LMutex::Auto;
	
	// Either one of these should non-NULL
	LAutoPtr<LSubProcess> LocalGdb;
	LStream *RemoteGdb = NULL;
	SystemIntf::ProcessIo RemoteIo;
	
	LString Exe, Args, InitDir;
	int AttachToPid = -1;
	LString ChildEnv;
	LString PrettyPrintPy;
	bool RunAsAdmin = false;
	bool AtPrompt = false;
	char Line[512], *LinePtr = NULL;
	int CurFrame = 0;
	bool SetAsmType = false;
	bool SetPendingOn = false;
	int ProcessId = -1; // of gdb itself, if known.
	bool SuppressEchoLine = false;
	bool SuppressNextFileLine = false;
	#if DEBUG_STRUCT_LOGGING
	LAutoPtr<LStructuredLog> Log;
	#else
	LStream *Log = NULL;
	#endif

	// Break points:
	enum TBpState
	{
		TInit,
		TError,
		TAdding,
		TAdded,
		TDeleting,
		
		TPendingAdd,
		TPendingDelete,
	};
	struct BpInfo
	{
		int64_t gdbIndex = 0;
		TBpState state = TInit;

		std::function<void(int,BpInfo*)> onIndex;	// the gdbIndex is assigned
		std::function<void(int)> onDelete;			// the bp is deleted from gdb
	};
	LHashTbl<IntKey<int>, BpInfo*> BreakPoints; // requires locking
	BreakPoint mainBreakPoint;
	LString mainBreakPointCmd;

	// These things need locking:
		struct Command
		{
			Gdb *gdb = nullptr;
			LString cmd;

			// Some commands will cause the program to run
			// In which case after the command is written to gdb
			// the running state should be set...
			bool setRunning = false;

			// Various data output
			LStringPipe pipe;
			LString::Array strArr;
			LError err;

			// One of these should be non-null
			std::function<void(LError&)> statusCb;
			std::function<void(LError&,LStringPipe*)> pipeCb;
			std::function<void(LError&,LString::Array*)> arrayCb;

			// Methods
			Command(Gdb *db, const char *c, bool setRun) :
				gdb(db)
			{
				cmd = c;
				setRunning = setRun;
			}

			void OnError(LError e)
			{
				err = e;
				Finish();
			}

			void Finish()
			{
				// LOG("%s: %s - %i,%i,%i\n", __FUNCTION__, cmd.Get(), (bool)statusCb, (bool)pipeCb, (bool)arrayCb);
				if (arrayCb)
				{
					auto s = pipe.NewLStr();
					RemoveAnsi(s);
					strArr = s.SplitDelimit("\r\n");
					#if DEBUG_STRUCT_LOGGING
					// gdb->Log->Log("Command.Finish: calling arrayCb");
					#endif
					arrayCb(err, &strArr);
				}
				else if (pipeCb)
				{
					#if DEBUG_STRUCT_LOGGING
					// gdb->Log->Log("Command.Finish: calling pipeCb");
					#endif
					pipeCb(err, &pipe);
				}
				else if (statusCb)
				{
					#if DEBUG_STRUCT_LOGGING
					// gdb->Log->Log("Command.Finish: calling statusCb");
					#endif
					statusCb(err);
				}
				else
				{
					// else no callback, which is ok...
					#if DEBUG_STRUCT_LOGGING
					// gdb->Log->Log("Command.Finish: no cb");
					#endif
				}
			}
		};

		bool DebuggingProcess = false; // have we started the debugging session with 'r' yet?
		bool Running = false;
		LArray<Command*> commands;
		
	// This should only be accessed by the worker thread...
	LAutoPtr<Command> curCmd;

	// Current location tracking
	LString CurFile;
	int CurLine = -1;
	LString::Array Untagged;

	// Parse state
	enum ParseType
	{
		ParseNone,
		ParseBreakPoint,
	}	ParseState = ParseNone;
	LString::Array BreakInfo;

	enum ThreadState
	{
		Init,
		Looping,
		Exiting,
		ProcessError
	}	State;

	void BreakPointEvent(BreakPointStore::TEvent type, int id)
	{
		BP_LOG("%s:%i - BreakPointEvent(%i, %i)\n", _FL, type, id);
		switch (type)
		{
			case BreakPointStore::TBreakPointAdded:
			{
				if (Running)
				{
					TLock lck(this, _FL);
					auto inf = BreakPoints.Find(id);
					if (!inf)
					{
						inf = new BpInfo;
						BreakPoints.Add(id, inf);
					}
					if (inf)
					{
						BP_LOG("%s:%i - added pending add for %i\n", _FL, id);
						inf->state = TPendingAdd;
					}
				}
				else
				{
					AddBp(id, nullptr);
				}
				break;
			}
			case BreakPointStore::TBreakPointDeleted:
			{
				if (Running)
				{
					TLock lck(this, _FL);
					auto inf = BreakPoints.Find(id);
					if (!inf)
					{
						inf = new BpInfo;
						BreakPoints.Add(id, inf);
					}
					if (inf)
					{
						BP_LOG("%s:%i - added pending delete for %i\n", _FL, id);
						inf->state = TPendingDelete;
					}
				}
				else
				{
					DelBp(id, nullptr);
				}
				break;
			}
			default:
				break;
		}
	}

	void OnFileLine(const char *File, int Line, bool CurrentIp, const char *SrcFile, int SrcLine)
	{
		if (SuppressNextFileLine)
		{
			SuppressNextFileLine = false;
		}
		else if (Events)
		{
			if (File)
				CurFile = File;
			if (Line > 0)
				CurLine = Line;
			
			if (CurFile && CurLine > 0)
			{
				LOG("%s:%i calling OnFileLine(%s,%i)\n", SrcFile, SrcLine, CurFile.Get(), CurLine);
				Events->OnFileLine(CurFile, CurLine, CurrentIp);
			}
		}
	}

	bool ParseLocation(LString::Array &lines)
	{
		for (auto line: lines)
		{
			LString::Array a = line.SplitDelimit(LWhiteSpace);
			if (a.Length() == 0)
				continue;

			auto parts = line.Split(" at ");			
			if (parts.Length() == 2) // Found the 'at'
			{
				LString::Array ref = parts.Last().RSplit(":", 1);
				if (ref.Length() == 2)
				{
					OnFileLine(NativePath(ref[0]), (int)ref[1].Int(), true, _FL);
					return true;
				}
			}
			else
			{
				int Line = (int)parts[0].Int();
				if (Line >= 0)
				{
					OnFileLine(NULL, Line, true, _FL);
					return true;
				}
			}
		}

		return false;
	}
	
	void SetState(bool is_debug, bool is_run)
	{
		if (Lock(_FL))
		{
			if (is_debug != DebuggingProcess ||
				is_run != Running)
			{
				DebuggingProcess = is_debug;
				Running = is_run;
				Unlock();

				#if DEBUG_STRUCT_LOGGING
				Log->Log("SetState.change:", DebuggingProcess, Running);
				#else
				// Log->Print("SetState(%i,%i) changed\n", DebuggingProcess, Running);
				#endif

				if (Events)
					Events->OnState(DebuggingProcess, Running);
			}
			else
			{
				Unlock();
			}
		}
	}

	void LogMsg(const char *Fmt, ...)
	{
		if (!Events)
			return;

		va_list Arg;
		va_start(Arg, Fmt);
		char Buf[512];
		int Ch = vsprintf_s(Buf, sizeof(Buf), Fmt, Arg);
		va_end(Arg);
			
		Events->Write(Buf, Ch);

		if (DEBUG_SHOW_GDB_IO)
			LOG("LogMsg:%s", Buf);
	}
	
	void OnExit()
	{
		SetState(false, false);
	}
	
	char *NativePath(char *p)
	{
		char dirChar = platform == PlatformWin ? '\\' : '/';

		for (char *c = p; *c; c++)
		{
			if (*c == '/' || *c == '\\')
				*c = dirChar;
		}
		return p;
	}
	
	void OnBreakPoint(LString f)
	{
		Events->Ungrab();
		
		if (!f.Get() || ProcessId < 0)
		{
			BP_LOG("Error: Param error: %s, %i (%s:%i)\n", f.Get(), ProcessId, _FL);
			return;
		}
		
		LString File, Line;
		LString::Array a = f.Split(" at ");

		#if 1		
		BP_LOG("%s:%i - a.len=%i\n", _FL, (int)a.Length());
		for (unsigned n=0; n<a.Length(); n++)
			BP_LOG("\t[%i]='%s'\n", n, a[n].Get());
		#endif
		
		if (a.Length() == 2)
		{
			LString k = a.Last().Strip();			
			
			if (k.Find("0x") == 0)
			{			
				LString::Array b = a[1].SplitDelimit(":,");
				for (unsigned i=0; i<b.Length(); i++)
				{
					LString s = b[i].Strip();
					if (s.Find("file ") == 0)
					{
						File = s(5, -1);
					}
					else if (s.Find("line ") == 0)
					{
						Line = s(5, -1);
					}
				}
			}
			else
			{
				auto e = k.Find(":");
				if (e > 0)
				{
					e++;
					while (e < k.Length() && IsDigit(k(e)))
						e++;

					LString::Array b = k(0, e).RSplit(":", 1);
					if (b.Length() == 2)
					{
						File = b[0];
						Line = b[1];
						BP_LOG("%s:%i - breakpoint %s:%s hit.\n", _FL, File.Get(), Line.Get());
					}
					else BP_LOG("%s:%i - split file/line failed for '%s'\n", _FL, k.Get());
				}
				else BP_LOG("%s:%i - error: no ':' in '%s'.\n", _FL, k.Get());
			}
		}
		else
		{
			// 1 part only means something like:
			// <line-number><line-from-src>
			auto parts = a[0].SplitDelimit();
			if (parts[0].IsNumeric())
			{
				Line = parts[0];
			}
			else
			{
				if (Events && f.Find("warning:") == 0)
					Events->OnWarning(f.Strip());
				else
					BP_LOG("%s:%i Unhandled line: '%s'\n", _FL, f.Get());
			}
		}

		if (!File && CurFile)
			File = CurFile;

		if (File && Line.Int() > 0)
		{
			BP_LOG("\tBreakpoint.OnFileLine(%s,%s)\n", File.Get(), Line.Get());
			OnFileLine(NativePath(File), (int)Line.Int(), true, _FL);
		}
		else
		{
			BP_LOG("%s:%i - No file='%s' or line='%s'\n%s\n", _FL, File.Get(), Line.Get(), f.Get());
		}
	}
	
	void OnLine(const char *Start, ptrdiff_t Length)
	{
		if (SuppressEchoLine)
		{
			SuppressEchoLine = false;
			return;
		}

		// if (DEBUG_SHOW_GDB_IO)
		{
			if (Strnstr(Start, "Breakpoint 1 at", Length))
			{
				int asd=0;
			}
			// Log->Print("OnLine: '%.*s'\n", (int)Length-1, Start);
		}

		// Send output
		if (curCmd)
		{
			// If the current command wants the data in a particular format:
			if (curCmd->arrayCb ||
				curCmd->pipeCb)
			{
				curCmd->pipe.Write(Start, Length);
				return;
			}
		}

		Untagged.New().Set(Start, Length);
		#if !ECHO_GDB_OUTPUT
		Events->Write(Start, Length);
		#endif

		#if ECHO_GDB_OUTPUT
		Events->Write(Start, Length);
		#endif

		if (ParseState == ParseBreakPoint)
		{
			LOG("\tbreak:'%s'\n", BreakInfo[0].Get());
			OnBreakPoint(BreakInfo[0]);
			ParseState = ParseNone;
			BreakInfo.Length(0);
		}

		if (ParseState == ParseNone)
		{
			if (Stristr(Start, "received signal SIGSEGV"))
			{
				Events->Ungrab();
				Events->OnCrash(0);
			}
			else if (Stristr(Start, "hit Breakpoint"))
			{
				ParseState = ParseBreakPoint;
				BreakInfo.New().Set(Start, Length);
			}
			else if (*Start == '[')
			{
				if (stristr(Start, "Inferior") && stristr(Start, "exited"))
				{
					Events->Ungrab();
					OnExit();
				}
				else if (stristr(Start, "New Thread"))
				{
					LString s(Start, Length);
					LString::Array a = s.SplitDelimit("[] ()");
					int ThreadId = -1;
					for (unsigned i=0; i<a.Length(); i++)
					{
						if (a[i] == LString("LWP") && i < a.Length() - 1)
						{
							ThreadId = (int)a[i+1].Int();
							break;
						}
					}
					if (ThreadId > 0)
					{
						// Ok so whats the process ID?
						#ifdef POSIX
						
							int Pid = 
							getpgid(ThreadId);
							// LogMsg("Pid for Thread %i = %i\n", ThreadId, Pid);
							if (Pid > 0 && ProcessId < 0)
								// LgiTrace("Got the thread id: %i, and pid: %i\n", ThreadId, Pid);
								ProcessId = Pid;
						#else

							LAssert(!"Impl me.");

						#endif
					}
					else LgiTrace("%s:%i - No thread id?\n", _FL);
				}
			}
			else
			{
				// Untagged file/line?
				Events->Ungrab();
				if (ParseLocation(Untagged))
					Untagged.Empty();
			}
		}
	}

	bool Write(LString &s)
	{
		#if DEBUG_STRUCT_LOGGING
		Log->Log("Write:", s);
		#endif	

		bool status = false;
		if (LocalGdb)
		{
			LinePtr = Line;
			status = LocalGdb->Write(s);
		}
		else if (RemoteGdb)
		{
			status = RemoteGdb->Write(s);
		}
		else
		{
			LAssert(!"one of these needs to valid?");
			return false;
		}
		
		LogMsg("%s", s.Get());
		return status;
	}
	
	void ProcessCommands()
	{
		// We're at a gdb prompt, check for a command to run...
		#if DEBUG_STRUCT_LOGGING
		Log->Log(LString::Fmt("ProcessCommands: cur=%p(%s), %i", curCmd.Get(), curCmd.Get() ? curCmd->cmd.Get() : "none", (int)commands.Length()));
		#endif

		if (BreakPoints.Length() > 0)
		{
			TLock lck(this, _FL);
			BP_LOG("BreakPoints.Length()=%i\n", (int)BreakPoints.Length());
			for (auto p: BreakPoints)
			{
				if (p.value->state == TPendingAdd)
				{
					BP_LOG("%s:%i - processing pending Add: %i\n", _FL, p.key);
					p.value->state = TInit;
					AddBp(p.key);
				}
				else if (p.value->state == TPendingDelete)
				{
					BP_LOG("%s:%i - processing pending Delete: %i\n", _FL, p.key);
					DelBp(p.key);
				}
			}
		}

		//  LOG("ProcessCommands curCmd=%p: %s\n", curCmd.Get(), curCmd ? curCmd->cmd.Get() : "null");
		if (curCmd)
		{
			// LOG("Calling cmd finish: %s\n", curCmd->cmd.Get());
			
			// Finish up the current command
			curCmd->Finish();
			curCmd.Reset();
		}

		while (commands.Length())
		{
			// Pop the next command off the queue
			{
				LMutex::Auto lck(this, _FL);
				curCmd.Reset(commands.PopFirst());
			}
			if (curCmd)
			{
				// Write the command to the stream
				auto str = LString::Fmt("%s\n", curCmd->cmd.Get());
				auto wr = Write(str);
				if (RemoteGdb)
					SuppressEchoLine = wr;						
				if (wr)
				{
					if (curCmd->setRunning)
						SetState(DebuggingProcess, true);
					break;
				}

				curCmd->OnError(LError(LErrorIoFailed, "cmd write failed"));
				curCmd.Reset();
				// loop around and get another command...
			}
			else break;
		}
	}
	
	void OnRead(const char *Ptr, ssize_t Bytes)
	{
		#if DEBUG_STRUCT_LOGGING
		Log->Log("OnRead:", LString(Ptr, Bytes));
		#endif

		// Parse output into lines
		auto p = Ptr;
		auto End = p + Bytes;
		char *LineEnd = Line + sizeof(Line) - 2;
		while (p < End)
		{
			if (*p == '\r')
				;
			else if (*p == '\n')
			{
				*LinePtr++ = *p;
				*LinePtr = 0;
				auto bytes = LinePtr - Line;
				bytes = RemoveAnsi(Line, bytes);
				OnLine(Line, bytes);
				LinePtr = Line;
			}
			else if (LinePtr < LineEnd)
			{
				*LinePtr++ = *p;
			}

			p++;
		}
		*LinePtr = 0;

		// Check for prompt
		LString partial(Line, LinePtr - Line);
		if (partial.Length() == 0)
			return;
		RemoveAnsi(partial);
		#if DEBUG_STRUCT_LOGGING
		Log->Log("partial:", partial);
		#endif
		if (partial.Length() == 6)
		{
			AtPrompt = partial.Equals(sPrompt);
			#if DEBUG_STRUCT_LOGGING
			Log->Log("AtPrompt:", AtPrompt, "Running:", Running);
			#endif

			if (Running ^ !AtPrompt)
			{
				#if DEBUG_STRUCT_LOGGING
				// Log->Log("AtPrompt:", AtPrompt);
				#endif
				SetState(DebuggingProcess, !AtPrompt);
			}

			if (AtPrompt)
			{
				Events->Write(partial);
				ProcessCommands();
			}
		}
		else if (partial.Find("Quit anyway? (y or n)") >= 0)
		{
			// It's asking us if we want to quit.
			LString yes("y\n");
			Write(yes);
		}
		else if (RemoteGdb && partial.Length() > 2)
		{
			// Check for something that might be a prompt, and cancel the process if found.
			auto last = partial.Get() + partial.Length() - 2;
			if (!Strncmp(last, "> ", 2))
			{
				LScriptArguments args(nullptr);
				RemoteGdb->CallMethod(LDomPropToString(ObjCancel), args);
			}
		}
	}
	
	int Main() override
	{
		const char *Path = "gdb";
		const char *Shell = "bash";
		if (platform == PlatformWin)
		{
			Shell = "C:\\Windows\\System32\\cmd.exe";
			Path = "C:\\MinGW\\bin\\gdb.exe";
		}

		LString p;
		
		if (AttachToPid > 0)
		{
			auto Arg = LString::Fmt("--pid %i", AttachToPid);

			LgiTrace("Attaching Debugger: %s %s\n", Path, Args.Get());
			if (backend)
			{
				LAssert(!"impl me");
			}
			else
			{
				if (!LocalGdb.Reset(new LSubProcess(Path, Arg)))
					return false;
			}
		}
		else
		{
			if (RunAsAdmin)
				p.Printf("pkexec %s --args \"%s\"", Path, Exe.Get());
			else
				p.Printf("%s --args %s", Path, Exe.Get());
			if (Args)
			{
				p += " ";
				p += Args;
			}
			LString::Array a = p.Split(" ", 1);
			
			LgiTrace("Starting Debugger: %s %s\n", a[0].Get(), a[1].Get());
			if (backend)
			{
				// Don't need to do anything yet...
			}
			else
			{
				#if DEBUG_STRUCT_LOGGING
				Log->Log("Starting gdb:", a[0], a[1]);
				#endif
				if (!LocalGdb.Reset(new LSubProcess(a[0], a[1])))
					return false;
			}
		}

		if (LocalGdb)
		{
			if (InitDir)
			{
				#if DEBUG_STRUCT_LOGGING
				Log->Log("InitDir:", InitDir);
				#endif
				LocalGdb->SetInitFolder(InitDir);
			}
			if (ChildEnv)
			{
				auto p = ChildEnv.Split("\n");
				for (auto &v: p)
				{
					auto a = v.Strip().Split("=", 1);
					if (a.Length() == 2)
					{
						LogMsg("%s:%i - env %s=%s\n", _FL, a[0].Get(), a[1].Get());					
						LocalGdb->SetEnvironment(a[0], a[1]);
					}
					else LogMsg("%s:%i - Wrong parts %s.", _FL, v.Get());
				}
			}
			else LogMsg("%s:%i - No env.", _FL);
		}

		#if DEBUG_STOP_ON_GTK_ERROR
		Sp->SetEnvironment("G_DEBUG", "fatal-criticals");
		#endif
					
		LgiTrace("Starting gdb subprocess...\n");
		if (backend)
		{
			RemoteIo.ioCallback = [this](auto stream)
			{
				RemoteGdb = stream;
			};

			backendProcessRunning = true;
			if (!backend->RunProcess(NativePath(InitDir), p, &RemoteIo, this, [this](auto exitCode)
				{
					backendProcessRunning = false;
					#if DEBUG_STRUCT_LOGGING
					Log->Log("RemoteGdb exit code:", exitCode);
					#else
					Log->Print("RemoteGdb exit code: %i\n", exitCode);
					#endif
					if (!RemoteGdb)
						State = Exiting;
				}))
			{
				backendProcessRunning = false;
				State = ProcessError;
				OnError("Remote gdb failed to start", ERR_CTX);
				return -1;
			}
		}
		else if (LocalGdb)
		{
			bool processStart = LocalGdb->Start(true, true, false);
			#if DEBUG_STRUCT_LOGGING
			Log->Log("processStart:", processStart);
			#endif
			if (!processStart)
			{
				State = ProcessError;
				LError err(LocalGdb->GetErrorCode());				
				auto msg = LString::Fmt("Failed to start gdb, error: 0x%x (%s)\n", LocalGdb->GetErrorCode(), err.ToString().Get());
				#if DEBUG_STRUCT_LOGGING
				Log->Log("Error:", msg);
				#endif
				OnError(msg, ERR_CTX);
				return -1;
			}
		}

		#if DEBUG_SESSION_LOGGING
		LgiTrace("Gdb::Main - entering loop...\n");
		#endif
		State = Looping;
		
		if (AttachToPid > 0)
		{
			DebuggingProcess = true;
			Running = true; // Initial state of process is running
		}

		char Buf[513] = "";
		bool IsRun = false;
		while (State == Looping)
		{
			#ifdef _DEBUG
			ZeroObj(Buf);
			#endif

			ssize_t Rd = 0;
			if (LocalGdb)
			{
				if (!(IsRun = LocalGdb->IsRunning()))
				{
					#if DEBUG_STRUCT_LOGGING
					Log->Log("main: no longer IsRunning");
					#endif
					break;
				}
				Rd = LocalGdb->Read(Buf, sizeof(Buf)-1, 50);
			}
			else if (RemoteGdb)
			{
				Rd = RemoteGdb->Read(Buf, sizeof(Buf)-1);
				if (Rd > 0)
				{
					// Null terminate. Don't do ansi removal here,
					// as there can be partial sequences that break parsing
					Buf[Rd] = 0;
				}
				else if (Rd < 0)
				{
					// Remote process quit?
					int asd=0;
				}
			}

			if (Rd > 0)
				OnRead(Buf, Rd); // Process new data...
			else if (AtPrompt && !curCmd && commands.Length())
				ProcessCommands(); // Do the next command
			else
				LSleep(10); // don't eat CPU
		}

		SetState(false, false);

		if (RemoteGdb)
		{
			LScriptArguments args(nullptr);
			RemoteGdb->CallMethod(LDomPropToString(ObjCancel), args);

			delete RemoteGdb;
		}
		if (curCmd)
		{
			curCmd->Finish();
			curCmd.Reset();
		}

		LAssert(backendProcessRunning == false);
		return 0;
	}

	void Cmd(const char *c,
			bool setRun,
			std::function<void(LError&)> cb = nullptr)
	{
		#if DEBUG_STRUCT_LOGGING
		Log->Log("cmd reg:", c, setRun);
		#endif

		if (!ValidStr(c))
		{
			CMD_LOG("%s:%i - Null cmd.\n", _FL);
			LError err(LErrorInvalidParam, "Not a valid command.");
			if (cb)
				cb(err);
			return;
		}
		else if (auto cmd = new Command(this, c, setRun))
		{
			cmd->statusCb = cb;
			LMutex::Auto lck(this, _FL);
			commands.Add(cmd);
		}
	}
	
	void Cmd(const char *c,
			bool setRun,
			std::function<void(LError&,LString::Array*)> cb)
	{
		#if DEBUG_STRUCT_LOGGING
		Log->Log("cmd str array:", c, setRun, (bool)cb);
		#endif

		if (!ValidStr(c))
		{
			if (cb)
			{
				LError err(LErrorInvalidParam, "Not a valid command.");
				cb(err, NULL);
			}
		}
		else if (auto cmd = new Command(this, c, setRun))
		{
			cmd->arrayCb = cb;
			LMutex::Auto lck(this, _FL);
			commands.Add(cmd);
		}
	}

	void Cmd(const char *c,
			bool setRun,
			std::function<void(LError&,LStringPipe*)> cb)
	{
		#if DEBUG_STRUCT_LOGGING
		Log->Log("cmd str pipe:", c, setRun, (bool)cb);
		#endif

		if (!ValidStr(c))
		{
			if (cb)
			{
				LError err(LErrorInvalidParam, "Not a valid command.");
				cb(err, NULL);
			}
		}
		else if (auto cmd = new Command(this, c, setRun))
		{
			cmd->pipeCb = cb;
			LMutex::Auto lck(this, _FL);
			commands.Add(cmd);
		}
	}

public:
	Gdb(BreakPointStore *BpStore, LStream *log, SystemIntf *be, SysPlatform plat, LStream *netLog) :
		LThread("Gdb.Thread"),
		LMutex("Gdb.Lock"),
		bpStore(BpStore),
		#if DEBUG_STRUCT_LOGGING
			Log(new LStructuredLog(LStructuredLog::TNetworkEndpoint, LStructuredLog::sDefaultEndpoint, true, netLog)),
		#else
			Log(log),
		#endif
		backend(be),
		platform(plat)		
	{
		State = Init;
		LinePtr = Line;

		LString Val;
		if (LAppInst->GetOption("gdb", Val))
		{
			DEBUG_SHOW_GDB_IO = Val.Int() != 0;
			LOG("DEBUG_SHOW_GDB_IO=%i\n", DEBUG_SHOW_GDB_IO);
		}
		
		LFile::Path pp(LSP_APP_INSTALL);
		pp += "../utils/gdb-pretty-print.py";
		if (pp.Exists())
		{
			PrettyPrintPy = pp.GetFull();		
			LgiTrace("%s:%i - Gdb.Print='%s'\n", _FL, PrettyPrintPy.Get());
		}
		else
		{
			LgiTrace("%s:%i - Didn't find Gdb.Print='%s'\n", _FL, pp.GetFull().Get());
		}

		// Register to hear about break point changes
		bpStoreCallbackId = bpStore->AddCallback([this](auto type, int id)
		{
			BreakPointEvent(type, id);
		});

		// Also setup the exist break points:
	}
	
	~Gdb()
	{
		// The backend process should be exited cleanly before deleting the gdb object.
		LAssert(backendProcessRunning == false);

		bpStore->DeleteCallback(bpStoreCallbackId);

		State = Exiting;
		WaitForExit(10000);
		// LStackTrace("%i ~Gdb\n", LCurrentThreadId());
	}

	bool Load(LDebugEvents *EventHandler, const char *exe, const char *args, bool runAsAdmin, const char *initDir, const char *Env) override
	{
		Events = EventHandler;
		Exe = exe;
		Args = args;
		RunAsAdmin = runAsAdmin;
		ChildEnv = Env;
		InitDir = initDir;
		Running = false;
		
		Run();
		
		return true;
	}

	bool AttachTo(LDebugEvents *EventHandler, int Pid) override
	{
		Events = EventHandler;
		AttachToPid = Pid;
		
		Run();		
		
		return true;
	}
	
	void SetCurrentThread(int ThreadId, TStatusCb cb) override
	{
		if (ThreadId < 1)
		{
			if (cb) cb(false);
		}
		else
		{		
			Cmd(LString::Fmt("thread %i", ThreadId), false, cb);
		}
	}

	void GetThreads(std::function<void(LArray<LString>&, int)> cb) override
	{
		if (!cb)
			return;

		Cmd("info threads",
			false,
			[this, cb](LError &err, LString::Array *t)
			{
				LArray<LString> Threads;
				int CurrentThreadId = -1;
				for (int i=0; t && i<t->Length(); i++)
				{
					char *l = (*t)[i];
					if (i == 0)
						continue;
			
					while (*l && IsWhite(*l))
						l++;
			
					bool Current = *l == '*';
					if (Current)
					{
						l++;
						while (*l && IsWhite(*l))
							l++;
					}
			
					if (IsDigit(*l))
					{
						if (Current)
							CurrentThreadId = atoi(l);
				
						Threads.New() = l;
					}
					else
					{
						Threads.Last() += LString(" ") + l;
					}
				}

				cb(Threads, CurrentThreadId);
			});
	}

	void GetCallStack(TStringsCb cb) override
	{
		LAssert((bool)cb);

		Cmd("bt",
			false,
			[this, cb](LError &err, LString::Array *Bt)
			{
				if (err || !Bt)
					return;

				LString::Array Stack;
				Stack.SetFixedLength(false);
				for (auto ln: *Bt)
				{
					if (!ln)
						continue;
					if (*ln == '#')
					{
						Stack.New() = ln;
					}
					else if (Stack.Length() > 0)
					{
						// Append to the last line..
						auto &Prev = Stack.Last();
						Prev = Prev.RStrip() + ln;
					}
				}

				cb(Stack);
			});
	}

	void SetFrame(int Frame, TStatusCb cb) override
	{
		if (CurFrame == Frame)
			return;

		Cmd(LString::Fmt("frame %i", Frame),
			false,
			[this, cb, Frame](auto err)
			{
				if (cb)
					cb(!err);
				if (!err)
					CurFrame = Frame;
			});
	}

	void RestartInternal(TStatusCb cb)
	{
		ProcessId = -1;

		LString a;
		if (Args)
			a.Printf("r %s", Args.Get());
		else
			a = "r";

		Cmd(a,
			true,
			[this, cb](auto &err)
			{
				if (!err)
					SetState(true, false);

				if (cb)
					cb(!err);
			});
	}

	void Restart(TStatusCb cb) override
	{
		if (Running)
			Break([this, cb](auto status)
				{
					if (status)
						RestartInternal(cb);
					else if (cb)
						cb(false);
				},
				true);
		else
			RestartInternal(cb);
	}

	void UnloadInternal(TStatusCb cb)
	{
		#if DEBUG_STRUCT_LOGGING
		Log->Log("UnloadInternal called");
		#endif
		
		Cmd("q", false, [this, cb](auto &err)
		{
			SetState(false, false);
			State = Exiting;

			if (cb)
				cb(!err);
		});
	}

	void Unload(TStatusCb cb) override
	{
		if (Running)
			Break([this, cb](auto status)
				{
					if (status)
						UnloadInternal(cb);
					else if (cb)
						cb(false);
				},
				true);
		else
			UnloadInternal(cb);
	}
	
	bool GetRunning() override
	{
		return Running;
	}
	
	void SetRunning(bool Run, TStatusCb cb) override
	{
		#if DEBUG_STRUCT_LOGGING
		Log->Log("SetRunning:", Run);
		#endif	

		if (Run)
		{
			#if DEBUG_STRUCT_LOGGING
			if (Log) Log->Clear();
			#endif

			if (!SetAsmType)
			{
				SetAsmType = true;
				Cmd("set disassembly-flavor intel", false);
				Cmd("handle SIGTTIN nostop", false);
				Cmd("handle SIGTTOU ignore nostop", false);
				Cmd("handle SIG34 ignore nostop", false);
				Cmd("handle SIGPIPE nostop", false);
				Cmd("set pagination 0", false);
				if (PrettyPrintPy)
					Cmd(LString::Fmt("python exec(open(\"%s\").read())", PrettyPrintPy.Get()), false);
			}

			// Check we have BpInfo structures for all the break points
			// Then add all the break points into gdb...
			{
				auto ids = bpStore->GetAll();
				TLock lck(this, _FL);
				for (auto id: ids)
				{
					auto inf = BreakPoints.Find(id);
					if (!inf)
						BreakPoints.Add(id, inf = new BpInfo);
				}
				for (auto p: BreakPoints)
				{
					if (p.value->state < TAdding)
						AddBp(p.key);
				}
			}
			
			// Create the command to start things:
			LString a;
			if (DebuggingProcess)
				a = "c";
			else if (Args)
				a.Printf("r %s", Args.Get());
			else
				a = "r";

			if (a(0) == 'r' && (ProcessId < 0 || bpStore->Length() > 0))
			{
				mainBreakPoint.Symbol = "main";
				mainBreakPointCmd = a;
				bpStore->Add(mainBreakPoint);
			}
			else
			{
				SetState(true, Running);

				Cmd(a,
					true,
					[this, cb](auto &err)
					{
						if (cb)
							cb(!err);
					});
			}
		}
		else
		{
			Break(cb);
		}
	}

	void OnError(LString msg, const char *fn, const char *file, int line)
	{
		if (Events)
		{
			LError err(	LErrorFuncFailed,
						LString::Fmt("%s (fn=%s src=%s:%i)", msg.Get(), fn, file, line));
			err.SrcFile = file;
			err.SrcLine = line;
			Events->OnError(err);
		}
	}

	void AddBpInternal(int id, TStatusCb cb)
	{
		char cmd[MAX_PATH_LEN];		
		auto bp = bpStore->Get(id);
		if (bp.File)
		{
			LString file = bp.File;
			if (!Strncmp(file.Get(), InitDir.Get(), InitDir.Length()))
			{
				// We can remove the init dir from the start of file
				file = file(InitDir.Length(),-1).LStrip("\\/");
			}

			if (file(0) == '~')
			{
				file = file.Replace("~", "$HOME");
			}

			sprintf_s(cmd, sizeof(cmd), "break %s:" LPrintfSSizeT, file.Get(), bp.Line);
		}
		else if (bp.Symbol)
		{
			sprintf_s(cmd, sizeof(cmd), "break %s", bp.Symbol.Get());
		}
		else
		{
			if (cb)
				cb(false);
			return;
		}

		#if DEBUG_STRUCT_LOGGING
		Log->Log("AddBpInternal:", cmd);
		#endif

		{
			TLock lck(this, _FL);
			if (auto inf = BreakPoints.Find(id))
			{
				inf->state = TAdding;
			}
		}

		Cmd(cmd,
			false,
			[this, cb, id](auto &err, LString::Array *Lines)
			{
				TLock lck(this, _FL);
				auto inf = BreakPoints.Find(id);
				if (!inf)
				{
					if (cb)
						cb(false);
					return;
				}

				if (err)
				{
					inf->state = TError;
					if (cb)
						cb(false);
					return;
				}

				for (auto ln: *Lines)
				{
					auto parts = ln.SplitDelimit();
					
					if (ln.Find("No source file") >= 0)
					{
						OnError(ln.Strip(), ERR_CTX);
						if (cb)
							cb(false);
						return;						
					}
					else if (parts.Length() >= 2 &&
							 parts[0].Equals("breakpoint"))
					{
						auto Idx = parts[1].Int();
						if (Idx > 0)
						{
							inf->state = TAdded;
							inf->gdbIndex = Idx;
							OnBreakPointIndex(id, inf);
							if (inf->onIndex)
								inf->onIndex(id, inf);

							if (cb)
								cb(true);
							return;
						}
					}
				}

				if (inf->state == TAdding)
				{
					// Didn't find the index?
					inf->state = TError;
					OnError("Couldn't find index of break point.", ERR_CTX);
				}
			
				if (cb)
					cb(false);
			});
	}

	// Called when the gdb break point index is assigned
	void OnBreakPointIndex(int id, BpInfo *inf)
	{
		if (!mainBreakPoint || !mainBreakPointCmd)
			return;

		auto bp = bpStore->Get(id);
		if (bp == mainBreakPoint)
		{
			// Get the process id?

			// Install various breakpoints

			// Install a callback for the 
			inf->onDelete = [this](auto id)
				{
					// sub process has started...
					SetState(true, Running);
					Cmd(mainBreakPointCmd, true, [this](auto err)
						{
							// Clean up the state...
							mainBreakPoint.Empty();
							mainBreakPointCmd.Empty();
						});
				};

			// Delete the temporary main breakpoint...
			bpStore->Delete(id);
		}
	}

	void AddBp(int id, TStatusCb cb = nullptr)
	{
		if (id == INVALID_ID)
		{
			if (cb)
				cb(false);
			return;
		}

		TLock lck(this, _FL);
		auto inf = BreakPoints.Find(id);
		if (!inf)
			BreakPoints.Add(id, inf = new BpInfo);
		if (inf)
		{
			if (inf->state == TAdded)
			{
				if (cb)
					cb(true);
				return;
			}
			else if (inf->state == TAdding)
			{
				return;
			}

			if (!SetPendingOn)
			{
				SetPendingOn = true;
				Cmd("set breakpoint pending on",
					false,
					[this, id, cb](auto &err)
					{
						#if DEBUG_STRUCT_LOGGING
						Log->Log("PendingBreakCb:", err);
						#endif
						if (err)
						{
							if (cb)
								cb(false);
							return;
						}

						AddBpInternal(id, cb);
					});
			}
			else
			{
				AddBpInternal(id, cb);
			}
		}
	}

	// Delete a break point
	void DelBp(int id, TStatusCb cb = nullptr)
	{
		// Handle bad id error:
		if (id == INVALID_ID)
		{
			if (cb)
				cb(false);
			return;
		}

		TLock lck(this, _FL);
		auto inf = BreakPoints.Find(id);
		
		// Handle no info error:
		if (!inf)
		{
			if (cb)
				cb(false);
			return;
		}

		// Not added, just remove the info...
		if (inf->state != TAdded || inf->gdbIndex <= 0)
		{
			if (inf->onDelete)
				inf->onDelete(id);

			delete inf;
			BreakPoints.Delete(id);
			if (cb)
				cb(true);
			return;
		}

		// Actually tell gdb to remove the error.
		auto c = LString::Fmt("del " LPrintfInt64, inf->gdbIndex);
		Cmd(c,
			false,
			[this, id, cb](auto err, LStringPipe *response)
			{
				auto out = response->NewLStr();
				auto status = out.IsEmpty();
				if (status)
				{
					TLock lck(this, _FL);
					auto inf = BreakPoints.Find(id);
					if (inf)
					{
						if (inf->onDelete)
							inf->onDelete(id);
						delete inf;
					}
					BreakPoints.Delete(id);
				}
				if (cb)
					cb(status);
			});
	}

	struct VariableParseState
	{
		Gdb *gdb = NULL;
		LStream *log = NULL;
		LArray<Variable> vars;
		bool Detailed;
		TVarsCb cb;
		LError err;
		std::atomic<int> calls{ 0 };

		VariableParseState(Gdb *obj, bool detail) :
			gdb(obj),
			Detailed(detail)
		{			
			#if DEBUG_STRUCT_LOGGING
			log = gdb->Log->GetLog();
			#else
			log = gdb->Log;
			#endif
		}

		void Finish(const char *optionalErr = nullptr)
		{
			if (optionalErr)
				err.Set(LErrorFuncFailed, optionalErr);

			// log->Print("%s cb=%i\n", __FUNCTION__, (bool)cb);
			if (cb)
				cb(err, vars);

			delete this;
		}

		void Calls(int offset)
		{
			auto old = calls.load();
			calls += offset;
			// log->Print("%s(%i)=%i\n", __FUNCTION__, offset, calls.load());
			if (calls.load() == 0 && old > 0)
			{
				Finish();
			}
		}

		// Calling this should always delete the object at some point...
		void Process(LString::Array *input, LDebugger::TScope scope)
		{
			if (input)
			{
				// Parse the input:
				for (ssize_t i=0; i<input->Length(); i++)
				{
					LString line = (*input)[i];

					// Append any following lines that start with whitespace...
					while (	i < input->Length() - 1 &&
							strchr(LWhiteSpace, (*input)[i+1][0]))
					{
						line += (*input)[i+1];
						input->DeleteAt(i+1, true);
					}

					auto EqPos = line.Find("=");
					if (EqPos <= 0)
						return;

					char *end = NULL;
					char *val = line.Get() + EqPos + 1;
					while (*val && strchr(LWhiteSpace, *val))
						val++;
				
					Variable &v = vars.New();
					v.Scope = scope;
					v.Name = line(0, EqPos).Strip();
					if (!strnicmp(val, "0x", 2))
					{
						v.Value.Type = GV_VOID_PTR;
						v.Value.Value.Ptr = (void*) htoi64(val);
					}
					else if (IsDigit(*val) || strchr(".-", *val))
					{
						// Is it floating point?
						auto isFloat = strchr(val, '.') != NULL;
						// LOG("isFloat for '%s' is %i\n", val, isFloat);
						if (isFloat)
						{
							double tmp = atof(val);
							v.Value = tmp;
						}
						else
						{					
							int64 tmp = atoi64(val);
							if (tmp & 0xffffffff00000000L)
								v.Value = tmp;
							else
								v.Value = (int)tmp;
						}
					}
					else if (*val == '(' && (end = strchr(val + 1, ')')))
					{
						val++;
						v.Type.Set(val, end - val);
						v.Value.OwnStr(TrimStr(end + 1));
					}
					else
					{
						v.Value.OwnStr(TrimStr(val));
					}				
				}
			}

			if (Detailed)
			{
				for (auto &v: vars)
				{
					// Get the type...
					Calls(1);
					gdb->Cmd(
						LString::Fmt("whatis %s", v.Name.Get()),
						false,
						[this, var = &v](auto &err, LStringPipe *typePipe)
						{
							if (err)
							{
								Calls(-1);
								LAssert(!"handler err.");
								return;
							}

							var->Type = typePipe->NewLStr();
						
							Calls(1);
							gdb->Cmd(
								LString::Fmt("p %s", var->Name.Get()),
								false,
								[this, var](auto &err, LStringPipe *valPipe) mutable
								{
									if (err)
									{
										Calls(-1);
										LAssert(!"handle err");
										return;
									}

									auto val = valPipe->NewLStr();
									if (val)
									{
										for (char *s = val; s && *s; )
										{
											if (*s == '\"')
											{
												char *e = strchr(++s, '\"');
												if (!e)
													break;
								
												var->Value.OwnStr(NewStr(s, e - s));
												break;
											}
											else if (*s == '(' && !var->Type)
											{
												char *e = strchr(++s, ')');
												if (!e)
													break;

												if (strnicmp(s, "gdb", 3))
													var->Type.Set(s, e - s);
												s = e + 1;
												continue;
											}						
							
											s = LSkipDelim(s, LWhiteSpace, true);
											s = LSkipDelim(s, LWhiteSpace);						
										}
									}

									Calls(-1);
								});
						});
				}
			}
		}
	};

	void GetVariables(bool Locals, bool Detailed, TVarArray *init, TVarsCb cb) override
	{
		#if DEBUG_STRUCT_LOGGING
		#else
		Log->Print("GetVariables init=%p\n", init);
		#endif
		if (auto state = new VariableParseState(this, Detailed))
		{
			state->cb = cb;

			if (init)
			{
				state->cb = std::move(cb);
				state->vars = *init;
				state->Process(nullptr, LDebugger::Local);
			}
			else
			{
				state->Calls(1);
				Cmd("info args",
					false,
					[this, state, cb](auto &err, LString::Array *out)
					{
						if (err)
							state->err = err;
						else if (out)
							state->Process(out, LDebugger::Arg);
						state->Calls(-1);
					});

				state->Calls(1);
				Cmd("info locals",
					false,
					[this, state, cb](auto &err, LString::Array *out)
					{
						if (err)
							state->err = err;
						else if (out)
							state->Process(out, LDebugger::Local);
						state->Calls(-1);
					});
			}
		}
	}

	void GetRegisters(TStringsCb cb) override
	{
		if (cb)
			Cmd("info registers",	
				false,
				[this, cb](auto &err, LStringPipe *out)
				{
					if (out)
					{
						auto arr = out->NewLStr().SplitDelimit("\r\n");
						cb(arr);
					}
				});
	}

	void PrintObject(const char *Var, TStringCb cb) override
	{
		if (!cb)
			return;
		if (!Var)
		{
			if (cb)
				cb(LString());
			return;
		}
	
		// Get type...
		auto c = LString::Fmt("whatis %s", Var);
		Cmd
		(
			c,
			false,
			[this, cb, Var=LString(Var)](auto err, LStringPipe *pipe)
			{
				if (err)
				{
					cb(LString());
					return;
				}

				auto Type = pipe->NewLStr().SplitDelimit("=").Last().Strip();
				bool IsPtr = Type.Find("*") >= 0;
				bool IsChar = Type.Find("const char") == 0 || Type.Find("char") == 0;

				// Get value...
				auto c = LString::Fmt("p %s%s", IsPtr && !IsChar ? "*" : "", Var.Get());
				Cmd
				(
					c,
					false,
					[this, cb, Type](auto err, LStringPipe *pipe)
					{
						auto val = pipe->NewLStr();
						if (!val)
						{
							cb(LString());
							return;
						}
		
						auto Eq = Strchr(val.Get(), '=');
						if (Eq)
						{
							Eq++;
							while (Strchr(" \t\r\n", *Eq))
								Eq++;
						}
		
						LStringPipe out;
						if (Eq && *Eq != '{')
						{
							auto s = val.SplitDelimit("=").Last().Strip();
							out.Print("%s\n", s.Get());
						}
						else // Parse object format.
						{			
							int Depth = 0;
							char *Start = NULL;
							char Spaces[256];
							memset(Spaces, ' ', sizeof(Spaces));
							int IndentShift = 2;

							#define Emit() \
								if (Start) \
								{ \
									auto bytes = s - Start; \
									char *last = s-1; while (last > Start && strchr(LWhiteSpace, *last)) last--; \
									out.Print("%.*s%.*s%s\n", Depth<<IndentShift, Spaces, bytes, Start, *last == '=' ? "" : ";"); \
									Start = NULL; \
								}
			
							out.Print("Parsed:\n");
							for (char *s = Eq ? Eq : val.Get(); *s; s++)
							{
								if (*s == '{')
								{
									Emit();
									out.Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
									Depth++;
								}
								else if (*s == '}')
								{
									Emit();
									Depth--;
									out.Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
								}
								else if (*s == ',')
								{
									Emit();
								}
								else if (!strchr(LWhiteSpace, *s))
								{
									if (Start == NULL)
										Start = s;
								}
							}
						}

						cb(out.NewLStr());
					}
				);
			}
		);
	}

	bool ReadMemory(LString &BaseAddr, int Length, LArray<uint8_t> &OutBuf, LString *ErrorMsg) override
	{
		#if 0
		if (!BaseAddr)
		{
			if (ErrorMsg) *ErrorMsg = "No base address supplied.";
			return false;
		}
		BaseAddr = BaseAddr.Strip();

		LString::Array Out;
		char c[256];
		int words = Length >> 2;
		// int bytes = Length % 4;
		
		if (BaseAddr.Find("0x") >= 0)
		{
			// Looks like an literal address...
			LiteralAddr:
			sprintf_s(c, sizeof(c), "x/%iw %s", words, BaseAddr.Get());
		}
		else
		{
			// Maybe it's a ptr variable?
			LString c;
			LString::Array r;
			c.Printf("p %s", BaseAddr.Get());
			if (Cmd(c, NULL, &r))
			{
				LString::Array p = r[0].SplitDelimit(" \t");
				for (unsigned i=0; i<p.Length(); i++)
				{
					if (p[i].Find("0x") >= 0)
					{
						BaseAddr = p[i];
						goto LiteralAddr;
					}
					
					/*
					LString Msg;
					Msg.Printf("%s\n", p[i].Get());
					Events->Write(Msg, Msg.Length());
					*/
				}
				
				if (ErrorMsg) *ErrorMsg = "No address in variable value response.";
				return false;
			}
			else 
			{
				if (ErrorMsg) *ErrorMsg = "Couldn't convert variable to address.";
				return false;
			}
		}		
		
		if (!Cmd(c, NULL, &Out))
		{
			if (ErrorMsg) *ErrorMsg = "Gdb command failed.";
			return false;
		}
		
		if (!OutBuf.Length(words << 2))
		{
			if (ErrorMsg) ErrorMsg->Printf("Failed to allocate %i bytes.", words << 2);
			return false;
		}
		
		uint32_t *buf = (uint32_t*) &(OutBuf)[0];
		uint32_t *ptr = buf;
		uint32_t *end = ptr + (OutBuf.Length() / sizeof(*buf));
		
		for (int i=0; i<Out.Length() && ptr < end; i++)
		{
			char *l = Out[i];
			if (i == 0 && stristr(l, "Invalid number"))
				return false;
			
			char *s = strchr(l, ':');
			if (!s)
				continue;
			
			s++;
			while (*s)
			{
				while (*s && strchr(LWhiteSpace, *s)) s++;
				uint32_t word = atoi(s);
				*ptr++ = word;
				while (*s && !strchr(LWhiteSpace, *s)) s++;
				
				if (ptr >= end)
					break;
			}
		}
		
		OutBuf.Length((ptr - buf) << 2);
		#endif
		return true;
	}
	
	bool GetLocation(LString &File, int &Line) override
	{
		LAssert(0);
		return false;
	}
	
	bool SetLocation(const char *File, int Line) override
	{
		LAssert(0);
		return false;
	}

	void StepInto(TStatusCb cb) override
	{
		Cmd("step",
			true,
			[this, cb](auto &err)
			{
				if (!err)
					SetState(DebuggingProcess, true);
				if (cb)
					cb(!err);
			});
	}

	void StepOver(TStatusCb cb) override
	{
		Cmd("next",
			true,
			[this, cb](auto &err)
			{
				if (!err)
					SetState(DebuggingProcess, true);
				if (cb)
					cb(!err);
			});
	}

	void StepOut(TStatusCb cb) override
	{
		Cmd("finish",
			true,
			[this, cb](auto &err)
			{
				if (!err)
					SetState(DebuggingProcess, true);
				if (cb)
					cb(!err);
			});
	}

	void BreakInternal(TStatusCb cb, bool SuppressFL)
	{
		if (backend)
		{
			LAssert(ProcessId > 0);
			auto cmd = LString::Fmt("kill -s SIGINT %i", ProcessId);
			backend->ProcessOutput(cmd,
				[this, cb](auto exitVal, auto out)
				{
					if (cb)
						cb(exitVal == 0);
				});
		}
		else
		{
			LAssert(!"impl me");
		}
	}

	void Break(TStatusCb cb, bool SuppressFL = false) override
	{
		if (backend)
		{
			if (ProcessId < 0)
			{
				// need to get the process id to send it a signal
				backend->ProcessOutput(
					"ps",
					[this, cb=std::move(cb), SuppressFL](auto exitCode, auto output)
					{
						RemoveAnsi(output);
						LString header;
						for (auto ln: output.SplitDelimit("\r\n"))
						{
							if (header)
							{
								auto parts = ln.SplitDelimit();
								if (parts.Length() > 4 &&
									parts[0].Find("gdb") >= 0)
								{
									auto s = parts[parts.Length()-4];
									auto pid = s.Int();
									if (pid > 0)
										ProcessId = (int)pid; // keep the last one?
								}
							}
							else header = ln;
						}

						if (ProcessId < 0)
						{
							if (cb)
								cb(false);
						}
						else
						{
							BreakInternal(std::move(cb), SuppressFL);
						}
					});
			}
			else
			{
				BreakInternal(std::move(cb), SuppressFL);
			}
		}
		else
		{
			#ifdef POSIX
				if (ProcessId < 0)
				{
					LogMsg("%s:%i - No process ID (yet?).\n", _FL);
					return;
				}
		
				SuppressNextFileLine = SuppressFL;
				int result = kill(ProcessId, SIGINT);
				auto ErrNo = errno;
				if (result)
					LogMsg("%s:%i - SIGINT failed with %i(0x%x): %s (pid=%i)\n", _FL, ErrNo, ErrNo, LErrorCodeToString(ErrNo).Get(), ProcessId);
			#else
				LAssert(!"Impl me");
			#endif		
		}
	}

	void UserCommand(const char *cmd, TStatusCb cb) override
	{
		Cmd(cmd,
			false,
			[this, cb](auto &err)
			{
				if (cb)
					cb(!err);
			});
	}

	void GetResponse(const char *c, TStringCb cb) override
	{
		Cmd(c,
			false,
			[this, cb](auto &err, LStringPipe *p)
			{
				LString data;
				if (p) data = p->NewLStr();
				if (cb)
					cb(data);
			});
	}
};

LDebugger *CreateGdbDebugger(BreakPointStore *bpStore, LStream *Log, SystemIntf *Backend, SysPlatform platform, LStream *networkLog)
{
	return new Gdb(bpStore, Log, Backend, platform, networkLog);
}
