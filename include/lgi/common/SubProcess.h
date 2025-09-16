/**
	\file
	\brief Sub-process wrapper.

	This class runs one or more sub-processes chained together by pipes.
	
	Example:
	
		LSubProcess p1("ls", "-l");
		LSubProcess p2("grep", "string");
		p1.Connect(&p2);
		p1.Start(true, false);
		int r;
		char Buf[256];
		while ((r = p1.Read(Buf, sizeof(Buf))) > 0)
		{
			// So something with 'Buf'
		}
		
*/ 
#ifndef _SUB_PROCESS_H_
#define _SUB_PROCESS_H_

#ifdef WIN32
#define USE_SIMPLE_FORK		0
#else
#define USE_SIMPLE_FORK		1
#endif

#if USE_SIMPLE_FORK
#include <stdio.h>
#endif

#if defined(MAC)
	#include <errno.h>
	#define LSUBPROCESS_ERROR	EBADEXEC
#elif defined(LINUX) || defined(HAIKU)
	#include <errno.h>
	#define LSUBPROCESS_ERROR	ECHILD
#elif defined(WINDOWS)
	#define LSUBPROCESS_ERROR	ERROR_PROCESS_ABORTED
#endif

LgiExtern bool LIsProcess(OsProcessId Pid);

class LgiClass LSubProcess : public LStream
{
public:
	#if defined(WIN32)
	typedef HANDLE PipeHandle;
	typedef DWORD ProcessId;
	#else
	typedef int PipeHandle;
	typedef pid_t ProcessId;
	#endif

	union Pipe
	{
		PipeHandle Handles[2];
		struct
		{
			PipeHandle Read;
			PipeHandle Write;
		};
		
		Pipe();
		
		bool Create
		(
			#ifdef WIN32
			LPSECURITY_ATTRIBUTES pAttr
			#else
			void *UnusedParam
			#endif
		);
		
		void Close();
	};

protected:
	struct LSubProcessPriv *d;
	friend struct LSubProcessPriv;
	
	struct Variable
	{
		LString Var, Val;
	};

	Variable *GetEnvVar(const char *Var, bool Create = false);
	bool Dupe(PipeHandle Old, PipeHandle New);
	LSubProcess *Parent, *Child;

public:
	// Object
	LSubProcess(const char *exe, const char *args = NULL, bool pseudoConsole = false);
	~LSubProcess();

	LString FindInPath(const char *exe);

	// Environment
	void SetInitFolder(const char *f);
	const char *GetEnvironment(const char *Var);	
	bool SetEnvironment(const char *Var, const char *Value);
	bool SetUser(const char *User, const char *Pass);

	// Dom (support StreamReadable/StreamWritable)
	bool GetValue(const char *Var, LVariant &Value) override;

	// Handles
	void SetStdin(PipeHandle Hnd);
	void SetStdout(PipeHandle Hnd);

	// Process lifecycle
	bool GetNewGroup();
	void SetNewGroup(bool ng);
	ProcessId Handle();
	bool IsRunning();
	uint32_t GetErrorCode();
	int32 GetExitValue();
	void Connect(LSubProcess *child);
	bool Start(bool ReadAccess = true, bool WriteAccess = false, bool MapStderrToStdout = true);
	int Wait();
	bool Interrupt();
	bool Kill();
	bool Signal(int which);
	int32 Communicate(LStreamI *Out, LStreamI *In = NULL, LCancel *Cancel = NULL);
	
	// IO
	int Peek();
	LString Read();
	ssize_t Read(void *Buf, ssize_t Size, int Flags = 0) override;
	bool Write(LString s);
	ssize_t Write(const void *Buf, ssize_t Size, int Flags = 0) override;

	// General management thread helper class:
	class LgiClass IoThread :
		public LThread,
		public LMutex,
		public LCancel
	{
		LAutoPtr<LSubProcess> process;
		LStringPipe out;

		// Private methods:
		void doRead(bool ended);
		int Main();
		
	public:
		// Client applications can use one of these functions to get data from the process:
		using TCallback = std::function<void(LString)>;
		
			/// Handle a single line of output (will correctly handle printing over the current line with '\r'):
			/// This isn't particularly efficent.
			TCallback onLine;
			/// Handle blocks of unparsed output:
			TCallback onBlock;			
			/// Handle the entire output of the application on exit:
			/// Note: By default this will include stderr output as well.
			TCallback onStdout;
		
		/// Event fired when the process is complete:
		std::function<void(int exitValue)> onComplete;
		
		// Object		
		IoThread(const char *exe = nullptr, const char *args = nullptr);
		~IoThread();
		
		// Methods:
		bool Create(const char *exe, const char *args = nullptr);
		bool Start(bool ReadAccess = true, bool WriteAccess = false, bool MapStderrToStdout = true);
		LSubProcess *GetProcess() const { return process; }
	};
};

#endif
