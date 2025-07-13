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

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
	
#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"

#define DEBUG_SUBPROCESS		0
#define DEBUG_ARGS				0

#define NULL_PIPE -1
#define ClosePipe close
#define INVALID_PID -1

#include <pwd.h>
#if defined(LINUX) // !mac and !haiku
	#include <crypt.h>
#endif
#if !defined(MAC)
	#include <shadow.h>
#endif

#ifdef HAIKU
#include <Roster.h>
#endif

LSubProcess::Pipe::Pipe()
{
	Read = Write = NULL_PIPE;
}
	
bool LSubProcess::Pipe::Create(void *UnusedParam)
{
	return pipe(Handles) != NULL_PIPE;
}

void LSubProcess::Pipe::Close()
{
	if (Read != NULL_PIPE)
	{
		ClosePipe(Read);
		Read = NULL_PIPE;
	}
	if (Write != NULL_PIPE)
	{
		ClosePipe(Write);
		Write = NULL_PIPE;
	}
}

char *ArgTok(const char *&s)
{
	if (!s || !*s)
		return NULL;
	while (*s && strchr(LWhiteSpace, *s))
		s++;
	if (*s == '\'' || *s == '\"')
	{
		char delim = *s++;
		const char *start = s;
		while (*s && *s != delim)
			s++;
		char *r = NewStr(start, s - start);
		if (*s)
			s++;
		return r;
	}
	else
	{
		const char *start = s;
		while (*s && !strchr(LWhiteSpace, *s))
			s++;
		return NewStr(start, s - start);
	}
}

struct LSubProcessPriv
{
	LString Exe;
	LString InitialFolder;
	LArray<char*> Args;
	bool NewGroup = false;
	bool PseudoConsole = false;
	bool EnvironmentChanged = false;
	LArray<LSubProcess::Variable> Environment;
	uint32_t ErrorCode = 0;

	LSubProcess::PipeHandle ExternIn = NULL_PIPE, ExternOut = NULL_PIPE;
	LSubProcess::ProcessId ChildPid = INVALID_PID;

	LSubProcess::Pipe Io;
	int ExitValue = -1; // was uint32
	int UserId = -1;
	int GrpId = -1;

	LSubProcessPriv(bool pseudoConsole)
	{
	}

	~LSubProcessPriv()
	{
		Args.DeleteArrays();
	}
};

LSubProcess::LSubProcess(const char *exe, const char *args, bool pseudoConsole)
{
	d = new LSubProcessPriv(pseudoConsole);
	Parent = Child = NULL;
	d->Exe = exe;
	d->Args.Add(NewStr(d->Exe));

	#if DEBUG_SUBPROCESS
	LgiTrace("%s:%i - %p::LSubProcess('%s','%s')\n", _FL, this, exe, args);
	#endif
	
	char *s;
	while ((s = ArgTok(args)))
	{
		d->Args.Add(s);
		#if DEBUG_ARGS
		LgiTrace("a='%s'\n", s);
		#endif
	}
}

LSubProcess::~LSubProcess()
{
	d->Io.Close();
	if (Child)
	{
		LAssert(Child->Parent == this);
		Child->Parent = NULL;
	}
	if (Parent)
	{
		LAssert(Parent->Child == this);
		Parent->Child = NULL;
	}
	DeleteObj(d);
}

extern char **environ;

LString LSubProcess::FindInPath(const char *exe)
{
	for (auto path: LGetPath())
	{
		LFile::Path p(path);
		p = p / exe;
		if (p.Exists())
			return p.GetFull();
	}
	return LString();
}

bool LSubProcess::GetNewGroup()
{
	return d->NewGroup;
}

void LSubProcess::SetNewGroup(bool ng)
{
	d->NewGroup = ng;
}

LSubProcess::ProcessId LSubProcess::Handle()
{
	return d->ChildPid;
}

LSubProcess::Variable *LSubProcess::GetEnvVar(const char *Var, bool Create)
{
	if (d->Environment.Length() == 0)
	{
		// Read all variables in
		for (int i=0; environ[i]; i++)
		{
			auto p = LString(environ[i]).Split("=", 1);
			if (p.Length() == 2)
			{
				Variable &v = d->Environment.New();
				v.Var = p[0];
				v.Val = p[1];
			}
		}
	}
	
	for (unsigned i=0; i<d->Environment.Length(); i++)
	{
		if (d->Environment[i].Var.Equals(Var))
			return &d->Environment[i];
	}
	
	if (Create)
	{
		Variable &v = d->Environment.New();
		v.Var = Var;
		return &v;
	}
	
	return NULL;
}

bool LSubProcess::Dupe(PipeHandle Old, PipeHandle New)
{
	while ((dup2(Old, New) == -1) && (errno == EINTR))
		;
	return true;
}

bool LSubProcess::IsRunning()
{
	int Status = 0;
	pid_t r = waitpid(d->ChildPid, &Status, WNOHANG);
	if (r == d->ChildPid)
	{
		d->ChildPid = INVALID_PID;
		if (WIFEXITED(Status))
			d->ExitValue = WEXITSTATUS(Status);
		else
			d->ExitValue = 255;
	}
	return d->ChildPid != INVALID_PID;
}

uint32_t LSubProcess::GetErrorCode()
{
	return d->ErrorCode;
}

int32 LSubProcess::GetExitValue()
{
	if (d->ChildPid != INVALID_PID)
	 	// This will set ExitValue if the process has finished.
		IsRunning();

	return d->ExitValue;
}

bool LSubProcess::SetUser(const char *User, const char *Pass)
{
	if (!User || !Pass)
	{
		LgiTrace("%s:%i - SetUser param err.\n", _FL);
		return false;
	}

	auto entry = getpwnam(User);
	if (!entry)
	{
		LgiTrace("%s:%i - SetUser: User '%s' doesn't exist.\n", _FL, User);
		return false;
	}

	if (0 != strcmp(entry->pw_passwd, "x"))
	{
		if (strcmp(entry->pw_passwd, crypt(Pass, entry->pw_passwd)))
		{
    		LgiTrace("%s:%i - SetUser: Wrong password.\n", _FL);
    		return false;
		}
	}
	#if !defined(MAC)
	else
	{
		// password is in shadow file
		auto shadowEntry = getspnam(User);
		if (!shadowEntry)
		{
			LgiTrace("%s:%i - SetUser: Failed to read shadow entry for user '%s'\n", User);
			return false;
		}

		if (strcmp(shadowEntry->sp_pwdp, crypt(Pass, shadowEntry->sp_pwdp)))
		{
    		LgiTrace("%s:%i - SetUser: Wrong password.\n", _FL);
    		return false;
		}
	}
	#endif

	d->UserId = entry->pw_uid;
	d->GrpId = entry->pw_gid;

    return true;
}

void LSubProcess::SetInitFolder(const char *f)
{
	d->InitialFolder = f;
}

const char *LSubProcess::GetEnvironment(const char *Var)
{
	Variable *v = GetEnvVar(Var);
	return v ? v->Val.Get() : NULL;
}

bool LSubProcess::SetEnvironment(const char *Var, const char *Value)
{	
	Variable *v = GetEnvVar(Var, true);
	if (!v)
		return false;
	
	bool IsPath = !_stricmp(Var, "PATH");

	LStringPipe a;
	const char *s = Value;
	while (*s)
	{
		char *n = strchr(s, '%');
		char *e = n ? strchr(n + 1, '%') : NULL;
		if (n && e)
		{
			a.Write(s, (int) (n-s));
			
			n++;
			ptrdiff_t bytes = e - n;
			char Name[128];	
			if (bytes > sizeof(Name) - 1) bytes = sizeof(Name)-1;			
			memcpy(Name, n, bytes);
			Name[bytes] = 0;
			
			const char *existing = GetEnvironment(Name);
			if (existing)
			{
				a.Write(existing, (int)strlen(existing));
			}

			s = e + 1;
		}
		else
		{
			a.Write(s, (int)strlen(s));
			break;
		}
	}
	
	v->Val = a.NewLStr();
	if (IsPath)
	{
		// Remove missing paths from the list
		auto t = LString(v->Val).SplitDelimit(LGI_PATH_SEPARATOR);
		LStringPipe p;
		for (unsigned i=0; i<t.Length(); i++)
		{
			char *Dir = t[i];
			if (LDirExists(Dir))
				p.Print("%s%s", p.GetSize() ? LGI_PATH_SEPARATOR : "", Dir);
			// else LgiTrace("%s:%i - Removing missing path '%s'\n", _FL, Dir);
		}
		v->Val = p.NewLStr();
	}
	
	d->EnvironmentChanged = true;
	
	return true;
}	

bool LSubProcess::GetValue(const char *Var, LVariant &Value)
{
	switch (LStringToDomProp(Var))
	{
		case StreamReadable:
		{
			break;
		}
		case StreamWritable:
		{
			break;
		}
		default:
			return false;
	}
	
	return false;
}

void LSubProcess::SetStdin(OsFile Hnd)
{
	d->ExternIn = Hnd;
}

void LSubProcess::SetStdout(OsFile Hnd)
{
	d->ExternOut = Hnd;
}

void LSubProcess::Connect(LSubProcess *child)
{
	Child = child;
	if (Child)
	{
		Child->Parent = this;
	}
}

bool LSubProcess::Start(bool ReadAccess, bool WriteAccess, bool MapStderrToStdout)
{
	bool Status = false;

	#ifdef HAIKU
	// Haiku has issues when you try and execute something that doesn't exist:
	//		https://dev.haiku-os.org/ticket/18576
	// This is to try and work around that issue. I tried just execvp'ing 'ls' but
	// it doesn't work. The locks are still all messed up.
	if (!LFileExists(d->Exe))
	{
		auto exe = FindInPath(d->Exe);
		if (!exe)
		{
			LgiTrace("%s:%i - '%s' not found.\n", _FL, d->Exe.Get());
			return false;
		}
		d->Exe = exe;
	}
	#endif

	#if DEBUG_SUBPROCESS
	LgiTrace("%s:%i - %p::Start(%i,%i,%i)\n", _FL, this, ReadAccess, WriteAccess, MapStderrToStdout);
	#endif

	int in[2];
	if (pipe(in) == -1)
	{
		printf("parent: Failed to create stdin pipe");
		return false;
	}

	int out[2];
	if (pipe(out) == -1)
	{
		printf("parent: Failed to create stdout pipe");
		return false;
	}
	
	d->ChildPid = fork();
	if (d->ChildPid == 0)
	{
		// We are in the child process.
		if (d->InitialFolder)
		{
			chdir(d->InitialFolder);
		}

		// Child shouldn't write to its stdin.
		if (close(in[1]))
			printf("%s:%i - close failed.\n", _FL);

		// Child shouldn't read from its stdout.
		if (close(out[0]))
			printf("%s:%i - close failed.\n", _FL);

		// Redirect stdin and stdout for the child process.
		if (dup2(in[0], fileno(stdin)) == -1)
		{
			printf("%s:%i - child[pre-exec]: Failed to redirect stdin for child\n", _FL);
			return false;
		}
		if (close(in[0]))
			printf("%s:%i - close failed.\n", _FL);
		
		if (dup2(out[1], fileno(stdout)) == -1)
		{
			printf("%s:%i - child[pre-exec]: Failed to redirect stdout for child\n", _FL);
			return false;
		}

		if (dup2(out[1], fileno(stderr)) == -1)
		{
			printf("%s:%i - child[pre-exec]: Failed to redirect stderr for child\n", _FL);
			return false;
		}
		close(out[1]);

		// Execute the child
		d->Args.Add(NULL);
		
		LString::Array Path;
		if (!LFileExists(d->Exe))
		{
			// Apparently 'execve' doesn't search the path... so we're going to look up the
			// full executable path ourselves.
			if (!Path.Length())
				Path = LGetPath();
			for (auto s: Path)
			{
				LFile::Path p(s, d->Exe);
				if (p.Exists())
				{
					d->Exe = p.GetFull();
					break;
				}
			}
		}
		
		if (d->UserId >= 0)
			setuid(d->UserId);
		if (d->GrpId >= 0)
			setgid(d->GrpId);
 				
		if (d->Environment.Length())
		{
			LString::Array Vars;
			LArray<char*> Env;
            
			Vars.SetFixedLength(false);
			for (auto v : d->Environment)
			{
				LString &s = Vars.New();
				s.Printf("%s=%s", v.Var.Get(), v.Val.Get());
				Env.Add(s.Get());
                
                if (v.Var.Equals("PATH"))
                    Path = v.Val.Split(LGI_PATH_SEPARATOR);
			}
			Env.Add(NULL);
            
			#if DEBUG_SUBPROCESS
			printf("Exe=%s\n", d->Exe.Get());
			printf("Env.Len=%i\n", (int)Env.Length());
			for (int i=0; i<Env.Length(); i++)
				printf("Env[%i]=%s\n", i, Env[i]);
			#endif
			
			int r = execve(d->Exe, &d->Args[0], Env.AddressOf());
			printf("execve=%i err=%i\n", r, errno);
		}
		else
		{
			execvp(d->Exe, &d->Args[0]);
		}

		// Execution will pass to here if the 'Exe' can't run or doesn't exist
		// So by exiting with an error the parent process can handle it.
		printf("LSUBPROCESS_ERROR\n");

		#if defined(MAC) || defined(HAIKU)
		// While 'exit' would be nice and clean it does cause crashes in free the global InitLibPng object
		// We HAVE to call exec??? to replace the process... anything will do... 'ls' will just quit quickly
		char *a= {0};
		execv("/bin/ls", &a);
		#else
		exit(LSUBPROCESS_ERROR);
		#endif
	}
	else
	{
		// We are in the parent process.
		if (d->ChildPid == -1)
		{
			printf("%s:%i - parent: Failed to create child", _FL);
			return false;
		}

		// Parent shouldn't read from child's stdin.
		if (close(in[0]))
			printf("%s:%i - close failed.\n", _FL);

		// Parent shouldn't write to child's stdout.
		if (close(out[1]))
			printf("%s:%i - close failed.\n", _FL);

		d->Io.Read = out[0];
		d->Io.Write = in[1];
		
		#if DEBUG_SUBPROCESS
		printf("USE_SIMPLE_FORK success.\n");
		#endif
		return true;
	}

	return Status;
}

int32 LSubProcess::Communicate(LStreamI *Out, LStreamI *In, LCancel *Cancel)
{
	char Buf[1024];
	ssize_t r;

	LAssert(In == NULL); // Impl me.
	#define NOT_CANCELLED (!Cancel || !Cancel->IsCancelled())

	while (IsRunning() && NOT_CANCELLED)
	{
		if (Cancel)
		{
			// If there is the possibility of cancelling the operation,
			// We should peek the input stream and check for input.
			// Otherwise the Read could hang and the Cancel object wouldn't
			// be checked for some time.
			if (!Peek())
			{
				// Try not to use 100% of a CPU core.
				LSleep(1);
				continue;
			}
		}
		
		r = Read(Buf, sizeof(Buf));
		if (r > 0 && Out)
			Out->Write(Buf, r);
	}

	while (NOT_CANCELLED)
	{
		r = Read(Buf, sizeof(Buf));
		if (r > 0 && Out)
			Out->Write(Buf, r);
		else
			break;
	}

	return GetExitValue();
}

int LSubProcess::Wait()
{
	int Status = -1;

	if (d->ChildPid != INVALID_PID)
	{
		pid_t r = waitpid(d->ChildPid, &Status, 0);
		printf("%s:%i - waitpid=%i pid=%i\n", _FL, r, d->ChildPid);
		if (r == d->ChildPid)
		{
			d->ChildPid = INVALID_PID;
			if (WIFEXITED(Status))
				d->ExitValue = WEXITSTATUS(Status);
			else
				d->ExitValue = 255;
			printf("%s:%i - wait, ExitValue=%i\n", _FL, r, d->ExitValue);
		}
	}
	else printf("%s:%i - wait: invalid PID.\n", _FL);

	return d->ExitValue;
}

bool LSubProcess::Interrupt()
{
	return Signal(SIGINT);
}

bool LSubProcess::Signal(int which)
{
	if (d->ChildPid == INVALID_PID)
	{
		// printf("%s:%i - child pid doesn't exist (%s).\n", _FL, d->Exe.Get());
		return false;
	}

	if (kill(d->ChildPid, which))
	{
		printf("%s:%i - kill(%i, %i) failed.\n", _FL, d->ChildPid, which);
		return false;
	}

	printf("%s:%i - kill(%i, %i).\n", _FL, d->ChildPid, which);
	/*
	if (which == SIGTERM)
		d->ChildPid = INVALID_PID;
	*/

	return true;
}

bool LSubProcess::Kill()
{
	return Signal(SIGTERM);
}

LString LSubProcess::Read()
{
	LStringPipe p(512);
	char Buf[512];
	ssize_t Rd;
	while (Peek())
	{
		Rd = Read(Buf, sizeof(Buf));
		if (Rd > 0)
			p.Write(Buf, Rd);
		else
			break;
	}

	return p.NewLStr();
}

ssize_t LSubProcess::Read(void *Buf, ssize_t Size, int TimeoutMs)
{
	bool DoRead = true;
	if (TimeoutMs)
	{
		OsSocket s = d->Io.Read;
		if (ValidSocket(s))
		{
			struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

			fd_set r;
			FD_ZERO(&r);
			FD_SET(s, &r);
			
			int v = select((int)s+1, &r, 0, 0, &t);
			if (v > 0 && FD_ISSET(s, &r))
			{
				DoRead = true;
			}
			else
			{
				// printf("SubProc not readable..\n");
				return 0;
			}
		}
		else LgiTrace("%s:%i - Invalid socket.\n", _FL);
	}
	
	return (int)read(d->Io.Read, Buf, Size);
}

int LSubProcess::Peek()
{
	int bytesAvailable = 0;
	int r = ioctl(d->Io.Read, FIONREAD, &bytesAvailable);
	return r ? -1 : bytesAvailable;
}

bool LSubProcess::Write(LString s)
{
	auto Wr = Write(s.Get(), s.Length());
	return Wr == s.Length();
}

ssize_t LSubProcess::Write(const void *Buf, ssize_t Size, int Flags)
{
	return (int)write(d->Io.Write, Buf, Size);
}

////////////////////////////////////////////////////////////////////////////////////
bool LIsProcess(OsProcessId Pid)
{
	bool Status = false;

	#if defined(MAC)
	
		#if LGI_COCOA
	
		#ifndef __OBJC__
		#error "Change this source code to ObjC++"
		#endif
		auto app = [NSRunningApplication runningApplicationWithProcessIdentifier: Pid];
		return app != nil;
	
		#elif LGI_CARBON
	
        ProcessSerialNumber psn;
        OSStatus e = GetProcessForPID(Pid, &psn);
		return e == 0;
	
		#else
	
		#warning FIXME
	
		#endif

	#elif defined(LINUX)

		char ProcPath[256];
		sprintf_s(ProcPath, sizeof(ProcPath), "/proc/%i", Pid);
		Status = LDirExists(ProcPath);
	
	#elif defined HAIKU
	
		BRoster r;
		app_info a;
		if (r.GetRunningAppInfo(Pid, &a) == B_OK)
		{
			Status = true;
		}

	#else

		#error Impl me.

	#endif

	return Status;
}
