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

#if defined(MAC) || defined(POSIX)

	#define _GNU_SOURCE
	#include <errno.h>
	#include <signal.h>
	#include <sys/wait.h>
	#include <sys/ioctl.h>
	#include <sys/types.h>

#elif defined(WINDOWS)

	#define NTDDI_VERSION 0x0A000006

#endif

#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"

#define DEBUG_SUBPROCESS		0
#define DEBUG_ARGS				0

#if defined(WIN32)
	
	#include <ConsoleApi.h>
	#include <Lm.h>
	#pragma comment(lib, "netapi32.lib")

	#define NULL_PIPE NULL
	#define ClosePipe CloseHandle

	typedef HRESULT (WINAPI *ProcCreatePseudoConsole)(_In_ COORD size, _In_ HANDLE hInput, _In_ HANDLE hOutput, _In_ DWORD dwFlags, _Out_ HPCON* phPC);

#else

	#define NULL_PIPE -1
	#define ClosePipe close
	#define INVALID_PID -1

#endif

#if defined(POSIX)

	#include <pwd.h>
	#if !defined(MAC)
		#include <crypt.h>
		#include <shadow.h>
	#endif

#endif

#ifdef HAIKU
#include <Roster.h>
#endif

LSubProcess::Pipe::Pipe()
{
	Read = Write = NULL_PIPE;
}
	
bool LSubProcess::Pipe::Create
(
	#ifdef WIN32
	LPSECURITY_ATTRIBUTES pAttr
	#else
	void *UnusedParam
	#endif
)
{
	#if defined(WIN32)
	return CreatePipe(&Read, &Write, pAttr, 0) != 0;
	#else
	return pipe(Handles) != NULL_PIPE;
	#endif
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
	while (*s && strchr(WhiteSpace, *s))
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
		while (*s && !strchr(WhiteSpace, *s))
			s++;
		return NewStr(start, s - start);
	}
}

struct LSubProcessPriv
{
	LString Exe;
	LString InitialFolder;
	LArray<char*> Args;
	bool NewGroup;
	bool PseudoConsole;

	bool EnvironmentChanged;
	LArray<LSubProcess::Variable> Environment;
	uint32_t ErrorCode;

	LSubProcess::PipeHandle ExternIn, ExternOut;
	LSubProcess::ProcessId ChildPid;

	#if defined(POSIX)
		LSubProcess::Pipe Io;
		int ExitValue; // was uint32
		int UserId = -1;
		int GrpId = -1;
	#elif defined(WIN32)
		HANDLE ChildHnd;
		DWORD ExitValue;
		LSubProcess::Pipe ChildOutput, ChildInput;
		HPCON hConsole;
		LLibrary Kernel;
		ProcCreatePseudoConsole CreatePseudoConsole;
		LString User, Password;
	#endif

	LSubProcessPriv(bool pseudoConsole)
		#ifdef WINDOWS
		: Kernel("Kernel32")
		#endif
	{
		NewGroup = false;
		ErrorCode = 0;
		EnvironmentChanged = false;
		ExternIn = NULL_PIPE;
		ExternOut = NULL_PIPE;
		PseudoConsole = false;

		#if defined(POSIX)
			ChildPid = INVALID_PID;
			ExitValue = -1;
		#elif defined(WIN32)
			ChildPid = NULL;
			ChildHnd = NULL;
			ExitValue = 0;
			hConsole = NULL;
			CreatePseudoConsole = (ProcCreatePseudoConsole)Kernel.GetAddress("CreatePseudoConsole");
			PseudoConsole = CreatePseudoConsole != NULL && pseudoConsole;
		#endif
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
	#if defined(POSIX)
	d->Io.Close();
	#endif
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

#ifndef WINDOWS
extern char **environ;
#endif

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
		#ifdef WINDOWS
		LPWCH e = GetEnvironmentStringsW();
		if (e)
		{
			char16 *s = e;
			while (*s)
			{
				char16 *eq = StrchrW(s, '=');
				if (!eq)
					break;

				ptrdiff_t NameChars = eq - s;
				if (NameChars > 0)
				{					
					Variable &v = d->Environment.New();
					v.Var.SetW(s, eq - s);
					eq++;
					v.Val.SetW(eq);
				}
				
				eq += StrlenW(eq);
				s = eq + 1;
			}
			
			FreeEnvironmentStringsW(e);
		}
		#else
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
		#endif
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
	#if defined(POSIX)
		while ((dup2(Old, New) == -1) && (errno == EINTR))
			;
		return true;
	#elif defined(WIN32)
		return DuplicateHandle(	GetCurrentProcess(), Old,
								GetCurrentProcess(), &New,
								0, false, DUPLICATE_SAME_ACCESS) != 0;
	#else
		return false;
	#endif
}

bool LSubProcess::IsRunning()
{
	#if defined(POSIX)
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
	#elif defined(WIN32)
		if (!GetExitCodeProcess(d->ChildHnd, &d->ExitValue))
			return false;
	
		if (d->ExitValue != STILL_ACTIVE)
		{
			d->ChildPid = 0;
			return false;
		}
		else
		{
			return true;
		}
	#endif
}

uint32_t LSubProcess::GetErrorCode()
{
	return d->ErrorCode;
}

int32 LSubProcess::GetExitValue()
{
	#if defined(POSIX)
	if (d->ChildPid != INVALID_PID)
	 	// This will set ExitValue if the process has finished.
		IsRunning();
	#elif defined(WIN32)
	GetExitCodeProcess(d->ChildHnd, &d->ExitValue);
	#endif
	return d->ExitValue;
}

bool LSubProcess::SetUser(const char *User, const char *Pass)
{
	if (!User || !Pass)
	{
		LgiTrace("%s:%i - SetUser param err.\n", _FL);
		return false;
	}

	#if defined(POSIX)

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

	#elif defined(WINDOWS)
	
		LAutoWString u(Utf8ToWide(User));

		USER_INFO_0 *info = NULL;
		auto result = NetUserGetInfo(NULL, u, 0, (LPBYTE*)&info);
		if (result != NERR_Success)
		{
			return false;
		}

		d->User = User;
		d->Password = Pass;

	#else

		#error "Impl me."

	#endif
    
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
	
	v->Val = a.NewGStr();
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
		v->Val = p.NewGStr();
	}
	
	d->EnvironmentChanged = true;
	
	return true;
}	

bool LSubProcess::GetValue(const char *Var, ::LVariant &Value)
{
	switch (LStringToDomProp(Var))
	{
		case StreamReadable:
		{
			#ifdef WINNATIVE
			char Buf[32] = "";
			DWORD lpBytesRead = 0;
			BOOL b = PeekNamedPipe(	d->ChildOutput.Read,
									Buf,
									sizeof(Buf),
									&lpBytesRead,
									NULL,
									NULL);
			Value = b && lpBytesRead > 0;
			break;
			#endif
		}
		/*
		case StreamWritable:
		{
			break;
		}
		*/
		default:
			return false;
	}
	
	return true;
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

	#if DEBUG_SUBPROCESS
	LgiTrace("%s:%i - %p::Start(%i,%i,%i)\n", _FL, this, ReadAccess, WriteAccess, MapStderrToStdout);
	#endif

	#if USE_SIMPLE_FORK
	
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
			printf("GSUBPROCESS_ERROR\n");
			exit(GSUBPROCESS_ERROR);
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
	
	#else
	
		// Find the end of the process list
		::LArray<LSubProcess*> p;
		for (LSubProcess *s=this; s; s=s->Child)
		{
			LAssert(!s->Child || s->Child->Parent == s);
			p.Add(s);
		}
		size_t Kids = p.Length() + 1;

		#ifdef WIN32
		SECURITY_ATTRIBUTES Attr;
		Attr.nLength = sizeof(SECURITY_ATTRIBUTES);
		Attr.bInheritHandle = true;
		Attr.lpSecurityDescriptor = NULL;
		#else
		int Attr = 0;
		#endif		
		
		#if defined(POSIX)
		::LArray<Pipe> Pipes;
		Pipes.Length(Kids);
		Pipes[0].Create(&Attr);
		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - *PARENT* pipe[%i].create %i,%i\n", _FL, 0, Pipes[0].Read, Pipes[0].Write);
		#endif

		Status = true;
		for (int i=1; i<Kids; i++)
		{
			Pipes[i].Create(&Attr);
			
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[%i].create %i,%i\n", _FL, i, Pipes[i].Read, Pipes[i].Write);
			#endif
			
			LSubProcess *sp = p[i-1];
			sp->ChildPid = fork();

			if (sp->ChildPid == INVALID_PID)
			{
				LgiTrace("%s:%i - fork failed with %i", _FL, errno);
				exit(1);
			}
			else if (sp->ChildPid == 0)
			{
				if (InitialFolder)
				{
					chdir(InitialFolder);
				}

				// Close irrelevant pipes
				for (int j = 0; j < i-1; j++)
				{
					#if DEBUG_SUBPROCESS
					LgiTrace("%s:%i - *CHILD* pipe[%i].close %i,%i\n", _FL, j, Pipes[j].Read, Pipes[j].Write);
					#endif
					Pipes[j].Close();
				}
				
				// Set up STDIN and STDOUT
				Pipe &in = Pipes[i-1];
				Pipe &out = Pipes[i];

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* %i) Child init %i->'%s'->%i\n", _FL, i, in.Read, sp->Exe.Get(), out.Write);
				#endif

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Dupe %i->%i\n", _FL, in.Read, STDIN_FILENO);
				#endif
				Dupe(in.Read, STDIN_FILENO);
				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Close %i\n", _FL, in.Write);
				#endif
				close(in.Write);

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Dupe %i->%i\n", _FL, out.Write, STDOUT_FILENO);
				#endif
				Dupe(out.Write, STDOUT_FILENO);
				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Dupe %i->%i\n", out.Write, STDERR_FILENO);
				#endif
				Dupe(out.Write, STDERR_FILENO);
				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - *CHILD* Close %i\n", _FL, out.Read);
				#endif
				close(out.Read);
				fsync(STDOUT_FILENO);
				LSleep(100);
				
				// Execute the child
				sp->Args.Add(NULL);
				execvp(sp->Exe, &sp->Args[0]);
			
	        	LgiTrace("%s:%i - execvp('%s').\n", _FL, sp->Exe.Get());
				for (int i=0; i<sp->Args.Length(); i++)
					LgiTrace("%s:%i - Args[%i]='%s'\n", _FL, i, sp->Args[i]);
				Status = false;
				break;
			}
		}
		
		// Close irrelevant pipes
		for (int j = 1; j < Kids - 1; j++)
		{
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[%i].close %i,%i\n", _FL, j, Pipes[j].Read, Pipes[j].Write);
			#endif
			Pipes[j].Close();
		}
		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - *PARENT* pipe[0].close %i, pipe[%i].close %i\n",
			_FL,
			Pipes[0].Read,
			Pipes.Length()-1, Pipes.Last().Write);
		#endif
		close(Pipes[0].Read);
		close(Pipes.Last().Write);

		// Set the input and output pipes for this sub-process.
		if (WriteAccess)
			Io.Write = Pipes[0].Write;
		else
		{
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[0].close %i\n", _FL, Pipes[0].Write);
			#endif
			close(Pipes[0].Write);
		}
		if (ReadAccess)
			Io.Read  = Pipes.Last().Read;
		else
		{
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - *PARENT* pipe[%i].close %i\n", _FL, Pipes.Length()-1, Pipes.Last().Read);
			#endif
			close(Pipes.Last().Read);
		}
		
		// LgiTrace("Final Handles %i, %i\n", Io.Read, Io.Write);
		
		#elif defined(WIN32)
		
		LAutoWString WExe;
		if (LFileExists(d->Exe))
		{
			WExe.Reset(Utf8ToWide(d->Exe));
		}
		else
		{
			char *Ext = LGetExtension(d->Exe);
			bool HasExt = Ext && _stricmp(Ext, "exe") == 0;
			
			#if defined(WIN32) && !defined(PLATFORM_MINGW)
			LString::Array p;
			char *sPath = NULL;
			size_t sSize;
			errno_t err = _dupenv_s(&sPath, &sSize, "PATH");
			if (err == 0)
				p = LString(sPath).SplitDelimit(LGI_PATH_SEPARATOR);
			free(sPath);
			#else
			LToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
			#endif
			for (unsigned i=0; i<p.Length(); i++)
			{
				char s[MAX_PATH_LEN];
				LMakePath(s, sizeof(s), p[i], d->Exe);
				if (LFileExists(s))
				{
					WExe.Reset(Utf8ToWide(s));
					break;
				}
				if (!HasExt)
				{
					strcat_s(s, sizeof(s), ".exe");
					if (LFileExists(s))
					{
						WExe.Reset(Utf8ToWide(s));
						break;
					}
				}
			}
		}		

		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - Exe='%S'\n", _FL, WExe.Get());
		#endif
		
		LStringPipe Args;
		for (unsigned i=0; i<d->Args.Length(); i++)
		{
			auto a = d->Args[i];
			auto sp = i ? " " : "";
			if (strchr(a, ' '))
				Args.Print("%s\"%s\"", sp, a);
			else
				Args.Print("%s%s", sp, a);
		}
		LAutoWString WArg(Utf8ToWide(Args.NewGStr()));

		#if DEBUG_SUBPROCESS || DEBUG_ARGS
		LgiTrace("%s:%i - Args='%S'\n", _FL, WArg.Get());
		#endif
		
		bool HasExternIn = d->ExternIn != NULL_PIPE;

		#if DEBUG_SUBPROCESS
		LgiTrace("%s:%i - HasExternIn=%i\n", _FL, HasExternIn);
		#endif
		
		if (d->ChildOutput.Create(&Attr) &&
			(HasExternIn || d->ChildInput.Create(&Attr)))
		{
			if (!SetHandleInformation(d->ChildOutput.Read, HANDLE_FLAG_INHERIT, 0))
				LgiTrace("%s:%i - SetHandleInformation failed.\n", _FL);
			
			if (!HasExternIn && !SetHandleInformation(d->ChildInput.Write, HANDLE_FLAG_INHERIT, 0))
				LgiTrace("%s:%i - SetHandleInformation failed.\n", _FL);

			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - Output Pipe: rd=%p, wr=%p\n", _FL, d->ChildOutput.Read, d->ChildOutput.Write);
			if (!HasExternIn)
				LgiTrace("%s:%i - Input Pipe: rd=%p, wr=%p\n", _FL, d->ChildInput.Read, d->ChildInput.Write);
			#endif

			DWORD CreateFlags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
			if (d->NewGroup)
				CreateFlags |= CREATE_NEW_PROCESS_GROUP;

			STARTUPINFOEXW InfoEx;
			ZeroObj(InfoEx);
			auto &Info = InfoEx.StartupInfo;
			Info.cb = sizeof(InfoEx);
			
			PROCESS_INFORMATION ProcInfo;
			ZeroObj(ProcInfo);

			Info.dwFlags = STARTF_USESTDHANDLES;
			if (d->PseudoConsole)
			{
				CreateFlags |= EXTENDED_STARTUPINFO_PRESENT;

				size_t size;
				InitializeProcThreadAttributeList(NULL, 1, 0, (PSIZE_T)&size);
				BYTE *attrList = new BYTE[size];
				InfoEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrList);
				auto b = InitializeProcThreadAttributeList(InfoEx.lpAttributeList, 1, 0, (PSIZE_T)&size);
				if (!b)
				{
					LgiTrace("%s:%s - InitializeProcThreadAttributeList failed.\n", _FL);
					return false;
				}

				COORD dimensions = { 500, 500 };
				auto r = d->CreatePseudoConsole(dimensions,
											HasExternIn ? d->ExternIn : d->ChildInput.Read,
											d->ChildOutput.Write,
											0,
											&d->hConsole);
				if (FAILED(r))
				{
					LgiTrace("%s:%s - CreatePseudoConsole failed.\n", _FL);
					return false;
				}
				
				b = UpdateProcThreadAttribute(	InfoEx.lpAttributeList,
                                                0,
                                                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                                &d->hConsole,
                                                sizeof(HPCON),
                                                NULL,
                                                NULL);
				if (!b)
				{
					LgiTrace("%s:%s - UpdateProcThreadAttribute failed.\n", _FL);
					return false;
				}
			}
			else
			{
				Info.hStdOutput = d->ChildOutput.Write;
				Info.hStdInput = HasExternIn ? d->ExternIn : d->ChildInput.Read;
				if (MapStderrToStdout)
					Info.hStdError = d->ChildOutput.Write;
			}
			
			LAutoWString WInitialFolder(Utf8ToWide(d->InitialFolder));

			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - WInitialFolder=%S\n", _FL, WInitialFolder.Get());
			#endif

			LAutoWString WEnv;
			if (d->EnvironmentChanged)
			{
				LMemQueue q(256);
				for (unsigned i=0; i<d->Environment.Length(); i++)
				{
					Variable &v = d->Environment[i];
					LAutoWString Var(Utf8ToWide(v.Var));
					LAutoWString Val(Utf8ToWide(v.Val));
					q.Write(Var, sizeof(char16)*(StrlenW(Var)));
					q.Write(L"=", sizeof(char16));
					q.Write(Val, sizeof(char16)*(StrlenW(Val)+1));
				}
				WEnv.Reset((char16*)q.New(4));
			}

			bool createResult = false;
			if (d->User)
			{
				LAutoWString WUser(Utf8ToWide(d->User));
				LAutoWString WPass(Utf8ToWide(d->Password));

				createResult = CreateProcessWithLogonW(	WUser,
														NULL,
														WPass,
														LOGON_NETCREDENTIALS_ONLY,
														WExe,
														WArg,
														CreateFlags,		// dwCreationFlags
														WEnv,				// lpEnvironment
														WInitialFolder,		// lpCurrentDirectory
														&Info,				// lpStartupInfo
														&ProcInfo) != 0;
			}
			else
			{
				createResult = CreateProcessW(	WExe,
												WArg,
												&Attr,				// lpProcessAttributes
												NULL,				// lpThreadAttributes
												TRUE,				// bInheritHandles
												CreateFlags,		// dwCreationFlags
												WEnv,				// lpEnvironment
												WInitialFolder,		// lpCurrentDirectory
												&Info,				// lpStartupInfo
												&ProcInfo) != 0;
			}

			if (createResult)
			{
				d->ChildPid = ProcInfo.dwProcessId;
				d->ChildHnd = ProcInfo.hProcess;
				CloseHandle(ProcInfo.hThread);

				#if DEBUG_SUBPROCESS
				LgiTrace("%s:%i - CreateProcessW OK, ChildPid=%p, ChildHnd=%p\n", _FL, d->ChildPid, d->ChildHnd);
				#endif

				Status = true;
			}
			else
			{
				d->ErrorCode = GetLastError();
				LgiTrace("%s:%i - CreateProces('%S', '%S'...) failed with %i\n",
					_FL, WExe.Get(), WArg, d->ErrorCode);
			}
			
			#if DEBUG_SUBPROCESS
			LgiTrace("%s:%i - Closing handles: %p, %p\n", _FL, d->ChildOutput.Write, d->ChildInput.Read);
			#endif
			CloseHandle(d->ChildOutput.Write);
			CloseHandle(d->ChildInput.Read);
			if (d->PseudoConsole)
				DeleteProcThreadAttributeList(InfoEx.lpAttributeList);
		}

		#endif
	
	#endif
	
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

	#if defined(POSIX)
		if (d->ChildPid != INVALID_PID)
		{
			int Status = 0;
			pid_t r = waitpid(d->ChildPid, &Status, 0);
			if (r == d->ChildPid)
			{
				d->ChildPid = INVALID_PID;
				if (WIFEXITED(Status))
					d->ExitValue = WEXITSTATUS(Status);
				else
					d->ExitValue = 255;
			}
		}
	#elif defined(WIN32)
		if (d->ChildHnd)
		{
			DWORD r = WaitForSingleObject(d->ChildHnd, INFINITE);
			if (r == WAIT_OBJECT_0)
			{
				if (GetExitCodeProcess(d->ChildHnd, &r))
				{
					Status = r;
				}
			}
		}
	#endif

	return Status;
}

bool LSubProcess::Interrupt()
{
	#if defined(POSIX)
		return Signal(SIGINT);
	#elif defined(WIN32)
		if (!d->ChildHnd)
			return false;

		#if 1
		// It's impossible to be attached to 2 consoles at the same time,
		// so release the current one.
		FreeConsole();
 
		// This does not require the console window to be visible.
		if (AttachConsole(d->ChildPid))
		{
			// Disable Ctrl-C handling for our program
			SetConsoleCtrlHandler(NULL, true);
			BOOL b = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
 
			// Must wait here. If we don't and re-enable Ctrl-C
			// handling below too fast, we might terminate ourselves.
			LSleep(1000);
 
			FreeConsole();
 
			// Re-enable Ctrl-C handling or any subsequently started
			// programs will inherit the disabled state.
			SetConsoleCtrlHandler(NULL, false);

			return b != 0;
		}
		#else

		if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, ChildPid))
			return true;

		auto Err = GetLastError();
		LgiTrace("%s:%i - GenerateConsoleCtrlEvent failed with 0x%x\n", _FL, Err);
		#endif

		return false;
	#else
		return false;
	#endif
}

bool LSubProcess::Signal(int which)
{
	#if defined(POSIX)
		if (d->ChildPid == INVALID_PID)
		{
			printf("%s:%i - child pid doesn't exist (%s).\n", _FL, d->Exe.Get());
			return false;
		}

		if (kill(d->ChildPid, which))
		{
			printf("%s:%i - kill(%i, %i) failed.\n", _FL, d->ChildPid, which);
			return false;
		}

		printf("%s:%i - kill(%i, %i).\n", _FL, d->ChildPid, which);
		if (which == SIGTERM)
			d->ChildPid = INVALID_PID;
	#elif defined(WIN32)
		if (!d->ChildHnd)
		{
			LgiTrace("%s:%i - No child process.\n", _FL);
			return false;
		}

		LAssert(!"Impl me.");
	#endif
	
	return true;
}

bool LSubProcess::Kill()
{
	#if defined(POSIX)
		return Signal(SIGTERM);
	#elif defined(WIN32)
		if (!d->ChildHnd)
		{
			LgiTrace("%s:%i - No child process.\n", _FL);
			return false;
		}

		if (TerminateProcess(d->ChildHnd, -1))
		{
			d->ChildHnd = NULL;
		}
		else
		{
			LgiTrace("%s:%i - TerminateProcess failed with 0x%x.\n", _FL, GetLastError());
			return false;
		}
	#endif
	
	return true;
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

	return p.NewGStr();
}

ssize_t LSubProcess::Read(void *Buf, ssize_t Size, int TimeoutMs)
{
	#if defined(POSIX)
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
	#else		
		DWORD Rd = -1, Sz;
		if (!ReadFile(d->ChildOutput.Read, Buf, AssertCast(Sz, Size), &Rd, NULL))
			return -1;
		return Rd;
	#endif
}

int LSubProcess::Peek()
{
	#if defined(POSIX)
		int bytesAvailable = 0;
		int r = ioctl(d->Io.Read, FIONREAD, &bytesAvailable);
		return r ? -1 : bytesAvailable;
	#else		
		DWORD Rd = 0, Avail = 0;
		char Buf[32];
		if (PeekNamedPipe(d->ChildOutput.Read, Buf, sizeof(Buf), &Rd, &Avail, NULL))
			return Rd;		
		return 0;
	#endif	
}

bool LSubProcess::Write(LString s)
{
	auto Wr = Write(s.Get(), s.Length());
	return Wr == s.Length();
}

ssize_t LSubProcess::Write(const void *Buf, ssize_t Size, int Flags)
{
	#if defined(POSIX)
		return (int)write(d->Io.Write, Buf, Size);
	#else
		DWORD Wr = -1, Sz;
		if (!WriteFile(d->ChildInput.Write, Buf, AssertCast(Sz, Size), &Wr, NULL))
			return -1;
		return Wr;
	#endif
}

////////////////////////////////////////////////////////////////////////////////////
bool LIsProcess(OsProcessId Pid)
{
	bool Status = false;

	#if defined WIN32

		HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, false, Pid);
		if (hProc)
		{
			DWORD ExitCode = 0;
			if (GetExitCodeProcess(hProc, &ExitCode) &&
				ExitCode == STILL_ACTIVE)
			{
				Status = true;
			}
			CloseHandle(hProc);
		}
		else LgiTrace("%s:%i - OpenProcess failed with 0x%x\n", _FL, GetLastError());
	
	#elif defined(MAC)
	
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
