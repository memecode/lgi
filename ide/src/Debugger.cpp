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
#include "Debugger.h"

#define DEBUG_STOP_ON_GTK_ERROR	0
#define DEBUG_SHOW_GDB_IO		0
#define ECHO_GDB_OUTPUT			0

const char sPrompt[] = "(gdb) ";

class Callback
{
public:
	virtual LString GetResponse(const char *c) = 0;
};

/*
class Visualizer
{
public:
	virtual ~Visualizer() {}
	
	virtual bool Match(LString s) = 0;
	virtual bool Transform(LString name, LString val, Callback *Cb, LVariant &Value, LString &Detail) = 0;
};

class LStringVis : public Visualizer
{
public:
	bool Match(LString s)
	{
		return s == "LString";
	}
	
	bool Transform(LString name, LString val, Callback *Cb, LVariant &Value, LString &Detail)	
	{
		LString::Array a = val.SplitDelimit("{} \t\r\n");
		if (a.Length() == 3 &&
			a[1] == "=")
		{
			void *Ptr = (void*)htoi64(a[2].Get());
			if (Ptr == NULL)
			{
				Value = "NULL";
			}
			else
			{
				LString cmd;
				cmd.Printf("p (char*)%s.Str->Str", name.Get());
				LString r = Cb->GetResponse(cmd);
				auto Pos = r.Find("=");
				if (Pos >= 0)
					Value = r(Pos, r.Length()).Strip().Get();
				else
					Value = r.Get();
			}
		}
		
		return true;
	}
};
*/

class Gdb : public LDebugger, public LThread, public Callback
{
	LDebugEvents *Events = NULL;
	LAutoPtr<LSubProcess> Sp;
	LString Exe, Args, InitDir;
	LString ChildEnv;
	LString PrettyPrintPy;
	bool RunAsAdmin = false;
	bool AtPrompt = false;
	char Line[256], *LinePtr = NULL;
	int CurFrame = 0;
	bool SetAsmType = false;
	bool SetPendingOn = false;
	LArray<BreakPoint> BreakPoints;
	int BreakPointIdx = -1;
	int ProcessId = -1;
	bool SuppressNextFileLine = false;
	// LArray<Visualizer*> Vis;
	LStream *Log = NULL;

	LMutex StateMutex;
	bool DebuggingProcess = false;
	bool Running = false;

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

	// Various output modes.
	LStream *OutStream = NULL;
	LString::Array *OutLines = NULL;
	
	enum ThreadState
	{
		Init,
		Looping,
		Exiting,
		ProcessError
	}	State;

	void OnFileLine(const char *File, int Line, bool CurrentIp)
	{
		if (SuppressNextFileLine)
		{
			// printf("%s:%i - SuppressNextFileLine\n", _FL);
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
				printf("%i: OnFileLine...\n", GetCurrentThreadId());
				Events->OnFileLine(CurFile, CurLine, CurrentIp);
				printf("%i: OnFileLine done.\n", GetCurrentThreadId());
			}
			/*
			else
				printf("%s:%i - Error: Cur loc incomplete: %s %i.\n", _FL, CurFile.Get(), CurLine);
			*/
		}
	}

	bool ParseLocation(LString::Array &p)
	{
		for (int i=0; i<p.Length(); i++)
		{
			LString::Array a = p[i].SplitDelimit(WhiteSpace);
			if (a.Length() > 0)
			{
				int At = 0;
				for (; At < a.Length() && stricmp(a[At], "at") != 0; At++)
					;
				
				if (At < a.Length() - 1) // Found the 'at'
				{
					LString::Array ref = a[At+1].Split(":");
					if (ref.Length() == 2)
					{
						OnFileLine(NativePath(ref[0]), (int)ref[1].Int(), true);
						return true;
					}
				}
				else
				{
					int Line = (int)a[0].Int();
					if (Line)
					{
						OnFileLine(NULL, Line, true);
						return true;
					}
				}
			}
		}
		return false;
	}
	
	void SetState(bool is_debug, bool is_run)
	{
		if (StateMutex.Lock(_FL))
		{
			if (is_debug != DebuggingProcess ||
				is_run != Running)
			{
				DebuggingProcess = is_debug;
				Running = is_run;
				printf("###### Running=%i\n", Running);
				
				StateMutex.Unlock();

				if (Events)
				{
					#if 1 // DEBUG_SESSION_LOGGING
					LgiTrace("Gdb::SetRunState(%i,%i) calling OnState...\n", is_debug, is_run);
					#endif
					Events->OnState(DebuggingProcess, Running);
					#if 1 // DEBUG_SESSION_LOGGING
					LgiTrace("Gdb::SetRunState(%i,%i) OnState returned.\n", is_debug, is_run);
					#endif
				}
			}
			else
			{
				StateMutex.Unlock();
			}
		}
	}

	void LogMsg(const char *Fmt, ...)
	{
		if (Events)
		{
			va_list Arg;
			va_start(Arg, Fmt);
			char Buf[512];
			int Ch = vsprintf_s(Buf, sizeof(Buf), Fmt, Arg);
			va_end(Arg);
			
			Events->Write(Buf, Ch);
		}
	}
	
	void OnExit()
	{
		SetState(false, false);
	}
	
	char *NativePath(char *p)
	{
		for (char *c = p; *c; c++)
		{
			if (*c == '/' || *c == '\\')
				*c = DIR_CHAR;
		}
		return p;
	}
	
	void OnBreakPoint(LString f)
	{
		Events->Ungrab();
		
		if (!f.Get() || ProcessId < 0)
		{
			printf("Error: Param error: %s, %i (%s:%i)\n", f.Get(), ProcessId, _FL);
			return;
		}
		
		LString File, Line;
		LString::Array a = f.Split(" at ");

		#if 0		
		printf("%s:%i - a.len=%i\n", _FL, (int)a.Length());
		for (unsigned n=0; n<a.Length(); n++)
			printf("\t[%i]='%s'\n", n, a[n].Get());
		#endif
		
		if (a.Length() == 2)
		{
			LString k = a[1].Strip();			
			// printf("%s:%i - k='%s'\n", _FL, k.Get());
			
			if (k.Find("0x") == 0)
			{			
				LString::Array b = a[1].SplitDelimit(":,");
				// printf("%s:%i - b.len=%i\n", _FL, b.Length());
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
				// printf("%s:%i - e=%i\n", _FL, e);
				if (e > 0)
				{
					e++;
					while (e < k.Length() && IsDigit(k(e)))
						e++;
					// printf("%s:%i - e=%i\n", _FL, e);
					LString::Array b = k(0, e).RSplit(":", 1);
					//  printf("%s:%i - b.len=%i\n", _FL, b.Length());
					if (b.Length() == 2)
					{
						File = b[0];
						Line = b[1];
					}					
				}
				else printf("Error: no ':' in '%s'. (%s:%i)\n", k.Get(), _FL);
			}
		}
		else printf("Error: %i parts (%s:%i).\n", (int)a.Length(), _FL);

		if (File && Line.Int() > 0)
		{
			OnFileLine(NativePath(File), (int)Line.Int(), true);
		}
		else
		{
			printf("%s:%i - No file='%s' or line='%s'\n%s\n", _FL, File.Get(), Line.Get(), f.Get());
		}
	}
	
	void OnLine(const char *Start, int Length)
	{
		#if DEBUG_SHOW_GDB_IO
		printf("Receive: '%.*s' ParseState=%i, OutLine=%p, OutStream=%p\n", Length-1, Start, ParseState, OutLines, OutStream);
		#endif

		// Send output
		if (OutLines)
		{
			OutLines->New().Set(Start, Length - 1);
			return;
		}
		else if (OutStream)
		{
			OutStream->Write(Start, Length);
			return;
		}
		else
		{
			Untagged.New().Set(Start, Length);
			#if !ECHO_GDB_OUTPUT
			Events->Write(Start, Length);
			#endif
		}

		#if ECHO_GDB_OUTPUT
		Events->Write(Start, Length);
		#endif

		if (ParseState == ParseBreakPoint)
		{
			if (Length > 0 && IsDigit(*Start))
			{
				LString Bp = LString(" ").Join(BreakInfo).Strip();
				
				printf("%i: OnBreakPoint...\n", GetCurrentThreadId());
				OnBreakPoint(Bp);
				printf("%i: OnBreakPoint done.\n", GetCurrentThreadId());

				ParseState = ParseNone;
				BreakInfo.Length(0);
			}
			else
			{
				// printf("ParsingBp.Add=%s\n", Start);
				BreakInfo.New().Set(Start, Length);
			}
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
				{
					Untagged.Length(0);
				}
			}
		}
	}
	
	void OnRead(const char *Ptr, ssize_t Bytes)
	{
		// Parse output into lines
		const char *p = Ptr;
		const char *End = p + Bytes;
		char *LineEnd = Line + sizeof(Line) - 2;
		while (p < End)
		{
			if (*p == '\n')
			{
				*LinePtr++ = *p;
				*LinePtr = 0;
				OnLine(Line, (int) (LinePtr - Line));
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
		auto bytes = LinePtr - Line;
		if (bytes == 6)
		{
			AtPrompt = !_strnicmp(Line, sPrompt, bytes);
			LgiTrace("AtPrompt=%i Running=%i\n", AtPrompt, Running);
			if (AtPrompt)
			{
				if (Running ^ !AtPrompt)
				{
					printf("Running=%i -> %i\n", Running, !AtPrompt);
					SetState(DebuggingProcess, !AtPrompt);
				}
				
				if (OutStream)
					OutStream = NULL;
				if (OutLines)
					OutLines = NULL;

				Events->Write(Line, bytes);
			}
		}
	}
	
	int Main()
	{
		#ifdef WIN32
		const char *Shell = "C:\\Windows\\System32\\cmd.exe";
		const char *Path = "C:\\MinGW\\bin\\gdb.exe";
		#else
		const char *Path = "gdb";
		#endif
		LString p;
		if (RunAsAdmin)
			p.Printf("pkexec %s --args \"%s\"", Path, Exe.Get());
		else
			p.Printf("%s --args \"%s\"", Path, Exe.Get());
		if (Args)
		{
			p += " ";
			p += Args;
		}
		LString::Array a = p.Split(" ", 1);

		printf("Starting Debugger: %s %s\n", a[0].Get(), a[1].Get());

		if (!Sp.Reset(new LSubProcess(a[0], a[1])))
			return false;

		if (InitDir)
			Sp->SetInitFolder(InitDir);
		if (ChildEnv)
		{
			auto p = ChildEnv.Split("\n");
			for (auto &v: p)
			{
				auto a = v.Strip().Split("=", 1);
				if (a.Length() == 2)
				{
					LogMsg("%s:%i - env %s=%s\n", _FL, a[0].Get(), a[1].Get());					
					Sp->SetEnvironment(a[0], a[1]);
				}
				else LogMsg("%s:%i - Wrong parts %s.", _FL, v.Get());
			}
		}
		else LogMsg("%s:%i - No env.", _FL);

		#if DEBUG_STOP_ON_GTK_ERROR
		Sp->SetEnvironment("G_DEBUG", "fatal-criticals");
		#endif
					
		LgiTrace("Starting gdb subprocess...\n");
		if (!Sp->Start(true, true, false))
		{
			State = ProcessError;
			auto ErrMsg = LErrorCodeToString(Sp->GetErrorCode());
			char s[256];
			sprintf_s(s, sizeof(s), "Failed to start gdb, error: 0x%x (%s)\n", Sp->GetErrorCode(), ErrMsg.Get());
			Events->OnError(Sp->GetErrorCode(), s);
			return -1;
		}

		#if DEBUG_SESSION_LOGGING
		LgiTrace("Gdb::Main - entering loop...\n");
		#endif
		State = Looping;

		char Buf[513];
		bool IsRun;
		auto PrevTs = LCurrentTime();
		while (State == Looping && (IsRun = Sp->IsRunning()))
		{
			#ifdef _DEBUG
			ZeroObj(Buf);
			#endif
			auto Rd = Sp->Read(Buf, sizeof(Buf)-1, 50);
			if (Rd > 0)
			{
				#if 0 // DEBUG_SESSION_LOGGING
				printf("GDB: %.*s\n", Rd, Buf);
				#endif
				OnRead(Buf, Rd);
			}
			
			auto Now = LCurrentTime();
			if (Now - PrevTs >= 1000)
			{
				PrevTs = Now;
			}
		}

		Break();
		Cmd("q");

		#if DEBUG_SESSION_LOGGING
		LgiTrace("Gdb::Main - exited loop.\n");
		#endif
		SetState(false, false);

		LogMsg("Debugger exited.\n");
		return 0;
	}

	bool WaitPrompt()
	{
		if (State == Init)
		{
			uint64 Start = LCurrentTime();
			while (State == Init)
			{
				uint64 Now = LCurrentTime();
				if (Now - Start < 5000)
				{
					LSleep(10);
				}
				else
				{
					LgiTrace("%s:%i - WaitPrompt init wait failed.\n", _FL);
					return false;
				}
			}
		}

		uint64 Start = LCurrentTime();
		uint64 Now = Start;
		while (!AtPrompt &&
				Now - Start < 5000 &&
				State == Looping)
		{
			Now = LCurrentTime();
			LSleep(50);
			uint64 After = LCurrentTime();
			printf("...wait=%i\n", (int)(After-Start));

			/*
			if (After - Now > 65)
			{
				printf("Sleep took=%i\n", (int)(After - Now));
			}
			*/
		}

		if (!AtPrompt)
		{
			LogMsg("Error: Not at prompt...\n");
			return false;
		}

		return true;
	}

	#if 0
	#define CMD_LOG(...) printf(__VA_ARGS__)
	#else
	#define CMD_LOG(...)
	#endif

	
	bool Cmd(const char *c, LStream *Output = NULL, LString::Array *Arr = NULL)
	{
		if (!ValidStr(c))
		{
			CMD_LOG("%s:%i - Null cmd.\n", _FL);
			LAssert(!"Not a valid command.");
			return false;
		}
		
		if (!WaitPrompt())
		{
			CMD_LOG("%s:%i - WaitPrompt failed.\n", _FL);
			return false;
		}

		char str[256];
		int ch = sprintf_s(str, sizeof(str), "%s\n", c);

		#if DEBUG_SHOW_GDB_IO
		LgiTrace("Send: '%s'\n", c);
		#else
		CMD_LOG("%s:%i - send '%s'.\n", _FL, c);
		#endif
		Events->Write(str, ch);
		LinePtr = Line;
		OutStream = Output;
		OutLines = Arr;		
		AtPrompt = false;

		auto Start = LCurrentTime();
		auto Wr = Sp->Write(str, ch);
		CMD_LOG("%s:%i - wr=%i.\n", _FL, (int)Wr);
		if (Wr != ch)
		{
			CMD_LOG("%s:%i - write failed.\n", _FL);
			return false;
		}

		if (OutStream || OutLines)
		{	
			auto Wait0 = LCurrentTime();

			WaitPrompt();

			auto Wait1 = LCurrentTime();
			CMD_LOG("%s:%i - Cmd timing " LPrintfInt64 " " LPrintfInt64 "\n",
				_FL,
				Wait0 - Start,
				Wait1 - Wait0);

			LAssert(OutStream == NULL && OutLines == NULL);
		}
		
		CMD_LOG("%s:%i - cmd success.\n", _FL);
		return true;
	}
	
public:
	Gdb(LStream *log) : LThread("Gdb"), Log(log), StateMutex("Gdb.StateMutex")
	{
		State = Init;
		LinePtr = Line;
		
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
		
		// Vis.Add(new LStringVis);
	}
	
	~Gdb()
	{
		#if DEBUG_SESSION_LOGGING
		LgiTrace("Gdb::~Gdb - waiting for thread to exit...\n");
		#endif
		State = Exiting;
		while (!IsExited())
		{
			LSleep(1);
		}
		#if DEBUG_SESSION_LOGGING
		LgiTrace("Gdb::~Gdb - thread has exited.\n");
		#endif
	}

	bool Load(LDebugEvents *EventHandler, const char *exe, const char *args, bool runAsAdmin, const char *initDir, const char *Env)
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
	
	bool SetCurrentThread(int ThreadId)
	{
		if (ThreadId < 1)
			return false;
		
		LString c;
		c.Printf("thread %i", ThreadId);
		if (!Cmd(c))
			return false;
		
		return true;
	}

	bool GetThreads(LArray<LString> &Threads, int *pCurrentThread)
	{
		LString::Array t;
		if (!Cmd("info threads", NULL, &t))
			return false;
		
		LString *Cur = NULL;
		for (int i=0; i<t.Length(); i++)
		{
			char *l = t[i];			
			if (i == 0)
				continue;
			
			while (*l && IsWhiteSpace(*l))
				l++;
			
			bool Current = *l == '*';
			if (Current)
			{
				l++;
				while (*l && IsWhiteSpace(*l))
					l++;
			}
			
			if (IsDigit(*l))
			{
				if (Current && pCurrentThread != NULL)
				{
					int ThreadId = atoi(l);
					*pCurrentThread = ThreadId;
				}
				
				Cur = &Threads.New();
				*Cur = l;
			}
			else if (Cur)
			{
				LString s;
				s.Printf("%s %s", Cur->Get(), l);
				*Cur = s;
			}
		}
		
		return true;
	}

	bool GetCallStack(LArray<LAutoString> &Stack)
	{
		LString::Array Bt;
		if (!Cmd("bt", NULL, &Bt))
			return false;

		for (int i=0; i<Bt.Length(); i++)
		{
			char *l = Bt[i];
			if (*l == '#')
			{
				Stack.New().Reset(NewStr(l));
			}
			else if (Stack.Length() > 0)
			{
				// Append to the last line..
				LAutoString &Prev = Stack.Last();
				char *End = Prev + strlen(Prev);
				while (End > Prev && strchr(WhiteSpace, End[-1]))
					*(--End) = 0;
				
				LString s;
				s.Printf("%s%s", Prev.Get(), l);
				Prev.Reset(NewStr(s));
			}
		}
		
		return true;
	}

	bool GetFrame(int &Frame, LAutoString &File, int &Line)
	{
		LAssert(0);
		return false;
	}
	
	bool SetFrame(int Frame)
	{
		if (CurFrame != Frame)
		{
			CurFrame = Frame;
			
			char c[256];
			sprintf_s(c, sizeof(c), "frame %i", Frame);
			return Cmd(c);
		}
		
		return true;
	}

	bool Restart()
	{
		if (Running)
			Break(true);
		
		ProcessId = -1;

		LString a;
		if (Args)
			a.Printf("r %s", Args.Get());
		else
			a = "r";

		bool Status = Cmd(a);
		if (Status)
		{
			SetState(true, false);
		}
		
		return Status;
	}

	bool Unload()
	{
		if (Running)
			Break(true);
		
		Cmd("q");
		SetState(false, false);
		State = Exiting;
		
		return false;
	}
	
	bool GetRunning()
	{
		return Running;
	}
	
	bool SetRunning(bool Run)
	{
		if (Run)
		{
			if (!SetAsmType)
			{
				SetAsmType = true;
				Cmd("set disassembly-flavor intel");
				Cmd("handle SIGTTIN nostop");
				Cmd("handle SIGTTOU ignore nostop");
				Cmd("handle SIG34 ignore nostop");
				Cmd("handle SIGPIPE nostop");
				if (PrettyPrintPy)
				{
					LString c;
					c.Printf("python exec(open(\"%s\").read())", PrettyPrintPy.Get());
					Cmd(c);
				}
			}
			
			LString a;
			if (DebuggingProcess)
				a = "c";
			else if (Args)
				a.Printf("r %s", Args.Get());
			else
				a = "r";

			if (a(0) == 'r' && ProcessId < 0)
			{
				BreakPoint bp;
				bp.Symbol = "main";
				if (SetBreakPoint(&bp))
				{
					printf("Set break point for main\n");
					if (!Cmd(a))
						return false;
					SetState(true, true);
					
					printf("Waiting for prompt\n");
					if (!WaitPrompt())
						return false;

					printf("Removing temp bp\n");
					RemoveBreakPoint(&bp);
					
					LStringPipe p;

					#if 0 // For some reason this is returning the wrong PID... WTH gdb... WTH.
					// Get process info
					if (Cmd("info inferiors", &p))
					{
						auto s = p.NewLStr();
						// LogMsg("%s\n", s.Get());
						
						auto Ln = s.SplitDelimit("\r\n");
						if (Ln.Length() >= 2)
						{
							LString::Array a = Ln[1].SplitDelimit(" \t");
							for (unsigned i=0; i<a.Length()-1; i++)
							{
								if (!a[i].Equals("process"))
									continue;

								int Id = (int)a[i+1].Int();
								if (Id >= 0)
								{
									LogMsg("%s:%i - ProcessId was %i, now %i (%s)\n", _FL, ProcessId, Id, Ln[1].Get());
									ProcessId = Id;
								}
								break;
							}
						}
					}
					#endif
					
					// Redetect the process id from the new threads...
					ProcessId = -1;

					printf("Continue...\n");					
					bool Status = Cmd("c"); // Continue
					if (Status)
						SetState(true, true);

					LogMsg("[ProcessId=%i]\n", ProcessId);
					return Status;					
				}
			}

			if (Cmd(a))
			{
				SetState(true, true);
				return true;
			}
		}
		else
		{
			if (Break())
			{
				return true;
			}
		}
		
		return false;
	}

	bool AddBp(BreakPoint &bp)
	{
		bool Ret = false;
		if (!bp.Added)
		{
			if (!SetPendingOn)
			{
				Cmd("set breakpoint pending on");
				SetPendingOn = true;
			}
			
			char cmd[MAX_PATH_LEN];
			char *File = bp.File.Get();
			if (File)
			{
				char *Last = strrchr(File, DIR_CHAR);
				sprintf_s(cmd, sizeof(cmd), "break %s:" LPrintfSSizeT, Last ? Last + 1 : File, bp.Line);
			}
			else if (bp.Symbol)
			{
				sprintf_s(cmd, sizeof(cmd), "break %s", bp.Symbol.Get());
			}
			else return false;
			
			BreakPointIdx = 0;
			
			LString::Array Lines;
			Ret = Cmd(cmd, NULL, &Lines);
			WaitPrompt();
			
			for (unsigned i=0; i<Lines.Length(); i++)
			{
				LString s;
				s = Lines[i];
				LString::Array p = s.Split(" ");
				if (p.Length() >= 2 &&
					!_stricmp(p[0], "breakpoint"))
				{
					int Idx = (int)p[1].Int();
					if (Idx)
					{
						bp.Index = Idx;
					}
				}
			}
			
			BreakPointIdx = -1;
			if (Ret)
				bp.Added = true;
		}
		return Ret;
	}
	
	bool SetBreakPoint(BreakPoint *bp)
	{
		if (!bp)
		{
			LgiTrace("%s:%i - SetBreakPoint failed, param error.\n", _FL);
			return false;
		}
		
		// Make sure the child 'gdb' is running...
		uint64 Start = LCurrentTime();
		while (State == Init)
		{
			LSleep(5);
			if (LCurrentTime()-Start > 3000)
			{
				LgiTrace("%s:%i - SetBreakPoint init wait failed...\n", _FL);
				return false;
			}
		}

		bp->Added = false;
		if (Running)
		{
			LgiTrace("%s:%i - Can't add break point while running.\n", _FL);
			return false;
		}
		else
		{
			if (AddBp(*bp))
			{
				BreakPoint &n = BreakPoints.New();
				n = *bp;
			}
		}
		
		return true;
	}

	bool RemoveBreakPoint(BreakPoint *bp)
	{
		if (!bp)
			return false;

		// Make sure the child 'gdb' is running...
		uint64 Start = LCurrentTime();
		while (State == Init)
		{
			LSleep(5);
			if (LCurrentTime()-Start > 3000)
			{
				LgiTrace("%s:%i - SetBreakPoint init wait failed...\n", _FL);
				return false;
			}
		}

		if (Running)
		{
			LgiTrace("%s:%i - Can't add break point while running.\n", _FL);
			return false;
		}
		else
		{
			unsigned i;
			for (i=0; i<BreakPoints.Length(); i++)
			{
				if (*bp == BreakPoints[i])
				{
					break;
				}
			}
			
			if (i < BreakPoints.Length())
			{
				char c[256];
				sprintf_s(c, sizeof(c), "delete %i", BreakPoints[i].Index);
				bool Status = Cmd(c);
				if (Status)
				{				
					BreakPoints.DeleteAt(i);
				}
				else
				{
					LgiTrace("%s:%i - The cmd '%s' failed.\n", _FL, c);
					return false;
				}
			}
			else
			{
				LgiTrace("%s:%i - Failed to find break point '%s:%i'\n",
					_FL,
					bp->File.Get(),
					bp->Line);
				return false;
			}
		}
		
		return true;
	}
	
	bool GetBreakPoints(LArray<BreakPoint> &bps)
	{
		bps = BreakPoints;
		return false;
	}
	
	void ParseVariables(const char *a, LArray<Variable> &vars, LDebugger::Variable::ScopeType scope, bool Detailed)
	{
		auto t = LString(a).SplitDelimit("\r\n");
		LString CurLine;
		for (unsigned i=0; i<t.Length(); i++)
		{
			CurLine = t[i];
			while (	i < t.Length() - 1 &&
					strchr(WhiteSpace, t[i+1][0]))
			{
				CurLine += t[++i];
				continue;
			}

			auto EqPos = CurLine.Find("=");
			if (EqPos > 0)
			{
				char *end = NULL;
				char *val = CurLine.Get() + EqPos + 1;
				while (*val && strchr(WhiteSpace, *val)) val++;
				
				Variable &v = vars.New();
				v.Scope = scope;
				v.Name = CurLine(0, EqPos).Strip();
				if (!strnicmp(val, "0x", 2))
				{
					v.Value.Type = GV_VOID_PTR;
					v.Value.Value.Ptr = (void*) htoi64(val);
				}
				else if (IsDigit(*val) || strchr(".-", *val))
				{
					// Is it floating point?
					auto isFloat = strchr(val, '.') != NULL;
					// printf("isFloat for '%s' is %i\n", val, isFloat);
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
				
				if (Detailed)
				{
					LStringPipe typePipe, valPipe;
					LString c;					

					// Get the type...
					c.Printf("whatis %s", v.Name.Get());
					Cmd(c, &typePipe);
					auto type = typePipe.NewLStr();
					printf("Type='%s'\n", type.Get());

					c.Printf("p %s", v.Name.Get());
					Cmd(c, &valPipe);
					auto val = valPipe.NewLStr();					
					if (val)
					{
						for (char *s = val; s && *s; )
						{
							if (*s == '\"')
							{
								char *e = strchr(++s, '\"');
								if (!e)
									break;
								
								v.Value.OwnStr(NewStr(s, e - s));
								break;
							}
							else if (*s == '(' && !v.Type)
							{
								char *e = strchr(++s, ')');
								if (!e)
									break;

								if (strnicmp(s, "gdb", 3))
									v.Type.Set(s, e - s);
								s = e + 1;
								continue;
							}						
							
							s = LSkipDelim(s, WhiteSpace, true);
							s = LSkipDelim(s, WhiteSpace);						
						}
					}
				}
			}
		}
	}
	
	bool GetVariables(bool Locals, LArray<Variable> &vars, bool Detailed)
	{
		LStringPipe p(512);

		if (vars.Length())
		{
			LString c;

			for (unsigned i=0; i<vars.Length(); i++)
			{
				Variable &v = vars[i];

				c.Printf("whatis %s", v.Name.Get());
				if (Cmd(c, &p))
				{
					auto a = p.NewLStr();
					if (a.Find("=") >= 0)
					{
						auto tmp = a.Split("=", 1);
						v.Type = tmp[1].Strip().Replace("\n", " ");
					}
					else
					{
						v.Type = a.Get();
					}
				}
				else LgiTrace("%s:%i - Cmd failed '%s'\n", _FL, c.Get());
				
				c.Printf("p %s", v.Name.Get());
				if (Cmd(c, &p))
				{
					LString a = p.NewLStr();
					LString Val;
					if (a.Find("=") >= 0)
					{
						LString::Array tmp = a.Split("=", 1);
						Val = tmp[1].Strip().Replace("\n", " ").Get();
					}
					else
					{
						Val = a.Get();
					}
					
					/*
					unsigned i;
					for (i=0; i<Vis.Length(); i++)
					{
						Visualizer *vs = Vis[i];
						if (vs->Match(v.Type))
						{
							if (vs->Transform(v.Name, Val, this, v.Value, v.Detail))
								break;
						}
					}
					if (i >= Vis.Length())
					*/
					
					v.Value = Val.Get();
				}
				else printf("%s:%i - Cmd failed '%s'\n", _FL, c.Get());
			}
			
			return true;
		}
		else
		{
			if (!Cmd("info args", &p))
				return false;
			
			auto a = p.NewLStr();
			ParseVariables(a, vars, Variable::Arg, Detailed);

			if (!Cmd("info locals", &p))
				return false;

			a = p.NewLStr();
			ParseVariables(a, vars, Variable::Local, Detailed);
		}
		
		return true;
	}

	bool GetRegisters(LStream *Out)
	{
		if (!Out)
			return false;
		
		return Cmd("info registers", Out);
	}

	bool PrintObject(const char *Var, LStream *Output)
	{
		if (!Var || !Output)
			return false;
	
		LStringPipe q;
		char c[256];
		
		// Get type...
		sprintf_s(c, sizeof(c), "whatis %s", Var);
		if (!Cmd(c, &q))
			return false;
		auto Type = q.NewLStr().SplitDelimit("=").Last().Strip();
		bool IsPtr = Type.Find("*") >= 0;
		bool IsChar = Type.Find("const char") == 0 || Type.Find("char") == 0;

		#if 1		
		Output->Print("Type: %s\n", Type.Get());
		#else // Debugging
		Output->Print("Type: %s (IsPtr=%i, IsGString=%i)\n", Type.Get(), IsPtr, IsGString);
		#endif

		// Get value...
		sprintf_s(c, sizeof(c), "p %s%s", IsPtr && !IsChar ? "*" : "", Var);
		if (!Cmd(c, &q))
		{
			Output->Print("%s:%i - Can't get value.\n", _FL);
			return false;
		}
		
		auto val = q.NewLStr();
		if (!val)
		{
			Output->Print("%s:%i - No value.\n", _FL);
			return false;
		}
		// Output->Print("val=%s\n", val.Get());
		
		auto Eq = Strchr(val.Get(), '=');
		if (Eq)
		{
			Eq++;
			while (Strchr(" \t\r\n", *Eq))
				Eq++;
		}
		
		if (Eq && *Eq != '{')
		{
			auto s = val.SplitDelimit("=").Last().Strip();
			Output->Print("%s\n", s.Get());
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
					char *last = s-1; while (last > Start && strchr(WhiteSpace, *last)) last--; \
					Output->Print("%.*s%.*s%s\n", Depth<<IndentShift, Spaces, bytes, Start, *last == '=' ? "" : ";"); \
					Start = NULL; \
				}
			
			Output->Print("Parsed:\n");
			for (char *s = Eq ? Eq : val.Get(); *s; s++)
			{
				if (*s == '{')
				{
					Emit();
					Output->Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
					Depth++;
				}
				else if (*s == '}')
				{
					Emit();
					Depth--;
					Output->Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
				}
				else if (*s == ',')
				{
					Emit();
				}
				else if (!strchr(WhiteSpace, *s))
				{
					if (Start == NULL)
						Start = s;
				}
			}
		}
	
		return true;
	}

	bool ReadMemory(LString &BaseAddr, int Length, LArray<uint8_t> &OutBuf, LString *ErrorMsg)
	{
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
				while (*s && strchr(WhiteSpace, *s)) s++;
				uint32_t word = atoi(s);
				*ptr++ = word;
				while (*s && !strchr(WhiteSpace, *s)) s++;
				
				if (ptr >= end)
					break;
			}
		}
		
		OutBuf.Length((ptr - buf) << 2);		
		return true;
	}
	
	bool GetLocation(LAutoString &File, int &Line)
	{
		LAssert(0);
		return false;
	}
	
	bool SetLocation(const char *File, int Line)
	{
		LAssert(0);
		return false;
	}

	bool StepInto()
	{
		bool Status = Cmd("step");
		if (Status)
			SetState(DebuggingProcess, true);
		return Status;
	}

	bool StepOver()
	{
		bool Status = Cmd("next");
		if (Status)
			SetState(DebuggingProcess, true);
		return Status;
	}

	bool StepOut()
	{
		bool Status = Cmd("finish");
		if (Status)
			SetState(DebuggingProcess, true);
		return Status;
	}

	bool Break(bool SuppressFL = false)
	{
		#ifdef POSIX
		if (ProcessId < 0)
		{
			LogMsg("%s:%i - No process ID (yet?).\n", _FL);
			return false;
		}
		
		SuppressNextFileLine = SuppressFL;
		// LogMsg("Break: Sending SIGINT to %i(0x%x)...\n", ProcessId, ProcessId);
		int result = kill(ProcessId, SIGINT);
		auto ErrNo = errno;
		// LogMsg("Break: result=%i\n", result);
		if (!result)
		{
			// LogMsg("%s:%i - success... waiting prompt\n", _FL);
			return WaitPrompt();
		}
		
		LogMsg("%s:%i - SIGINT failed with %i(0x%x): %s (pid=%i)\n", _FL, ErrNo, ErrNo, LErrorCodeToString(ErrNo).Get(), ProcessId);
		return false;
		#else
		LAssert(!"Impl me");
		return false;
		#endif		
	}

	bool UserCommand(const char *cmd)
	{
		char c[256];
		sprintf_s(c, sizeof(c), "%s", cmd);
		return Cmd(c);
	}

	LString GetResponse(const char *c)
	{
		LString r;
		LStringPipe p;
		if (Cmd(c, &p))
			r = p.NewLStr();
		return r;
	}
};

LDebugger *CreateGdbDebugger(LStream *Log)
{
	return new Gdb(Log);
}
