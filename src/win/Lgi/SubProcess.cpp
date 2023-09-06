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

#define NTDDI_VERSION 0x0A000006

#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"

#define DEBUG_SUBPROCESS		0
#define DEBUG_ARGS				0

#include <ConsoleApi.h>
#include <Lm.h>
#pragma comment(lib, "netapi32.lib")

#define NULL_PIPE NULL
#define ClosePipe CloseHandle

typedef HRESULT (WINAPI *ProcCreatePseudoConsole)(_In_ COORD size, _In_ HANDLE hInput, _In_ HANDLE hOutput, _In_ DWORD dwFlags, _Out_ HPCON* phPC);

LSubProcess::Pipe::Pipe()
{
	Read = Write = NULL_PIPE;
}
	
bool LSubProcess::Pipe::Create
(
	LPSECURITY_ATTRIBUTES pAttr
)
{
	return CreatePipe(&Read, &Write, pAttr, 0) != 0;
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

	HANDLE ChildHnd;
	DWORD ExitValue;
	LSubProcess::Pipe ChildOutput, ChildInput;
	HPCON hConsole;
	LLibrary Kernel;
	ProcCreatePseudoConsole CreatePseudoConsole;
	LString User, Password;

	LSubProcessPriv(bool pseudoConsole)
		: Kernel("Kernel32")
	{
		NewGroup = false;
		ErrorCode = 0;
		EnvironmentChanged = false;
		ExternIn = NULL_PIPE;
		ExternOut = NULL_PIPE;
		PseudoConsole = false;

		ChildPid = NULL;
		ChildHnd = NULL;
		ExitValue = 0;
		hConsole = NULL;
		CreatePseudoConsole = (ProcCreatePseudoConsole)Kernel.GetAddress("CreatePseudoConsole");
		PseudoConsole = CreatePseudoConsole != NULL && pseudoConsole;
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
	return DuplicateHandle(	GetCurrentProcess(), Old,
							GetCurrentProcess(), &New,
							0, false, DUPLICATE_SAME_ACCESS) != 0;
}

bool LSubProcess::IsRunning()
{
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
}

uint32_t LSubProcess::GetErrorCode()
{
	return d->ErrorCode;
}

int32 LSubProcess::GetExitValue()
{
	GetExitCodeProcess(d->ChildHnd, &d->ExitValue);
	return d->ExitValue;
}

bool LSubProcess::SetUser(const char *User, const char *Pass)
{
	if (!User || !Pass)
	{
		LgiTrace("%s:%i - SetUser param err.\n", _FL);
		return false;
	}

	LAutoWString u(Utf8ToWide(User));

	USER_INFO_0 *info = NULL;
	auto result = NetUserGetInfo(NULL, u, 0, (LPBYTE*)&info);
	if (result != NERR_Success)
	{
		return false;
	}

	d->User = User;
	d->Password = Pass;

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

	// Find the end of the process list
	LArray<LSubProcess*> p;
	for (LSubProcess *s=this; s; s=s->Child)
	{
		LAssert(!s->Child || s->Child->Parent == s);
		p.Add(s);
	}
	size_t Kids = p.Length() + 1;

	SECURITY_ATTRIBUTES Attr;
	Attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	Attr.bInheritHandle = true;
	Attr.lpSecurityDescriptor = NULL;
		
	LAutoWString WExe;
	if (LFileExists(d->Exe))
	{
		WExe.Reset(Utf8ToWide(d->Exe));
	}
	else
	{
		char *Ext = LGetExtension(d->Exe);
		bool HasExt = Ext && _stricmp(Ext, "exe") == 0;
			
		LString::Array p;
		char *sPath = NULL;
		size_t sSize;
		errno_t err = _dupenv_s(&sPath, &sSize, "PATH");
		if (err == 0)
			p = LString(sPath).SplitDelimit(LGI_PATH_SEPARATOR);
		free(sPath);

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
	LAutoWString WArg(Utf8ToWide(Args.NewLStr()));

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

	return Status;
}

bool LSubProcess::Interrupt()
{
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
}

bool LSubProcess::Signal(int which)
{
	if (!d->ChildHnd)
	{
		LgiTrace("%s:%i - No child process.\n", _FL);
		return false;
	}

	LAssert(!"Impl me.");	
	return true;
}

bool LSubProcess::Kill()
{
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

	return p.NewLStr();
}

ssize_t LSubProcess::Read(void *Buf, ssize_t Size, int TimeoutMs)
{
	DWORD Rd = -1, Sz;
	if (!ReadFile(d->ChildOutput.Read, Buf, AssertCast(Sz, Size), &Rd, NULL))
		return -1;
	return Rd;
}

int LSubProcess::Peek()
{
	DWORD Rd = 0, Avail = 0;
	char Buf[32];
	if (PeekNamedPipe(d->ChildOutput.Read, Buf, sizeof(Buf), &Rd, &Avail, NULL))
		return Rd;		
	return 0;
}

bool LSubProcess::Write(LString s)
{
	auto Wr = Write(s.Get(), s.Length());
	return Wr == s.Length();
}

ssize_t LSubProcess::Write(const void *Buf, ssize_t Size, int Flags)
{
	DWORD Wr = -1, Sz;
	if (!WriteFile(d->ChildInput.Write, Buf, AssertCast(Sz, Size), &Wr, NULL))
		return -1;
	return Wr;
}

////////////////////////////////////////////////////////////////////////////////////
bool LIsProcess(OsProcessId Pid)
{
	bool Status = false;

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
	
	return Status;
}
