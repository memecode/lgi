#define _WIN32_WINNT 0x500
// Win32 Implementation of General LGI functions
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/RegKey.h"

#define DEBUG_LOG_WRITES		1

#if DEBUG_LOG_WRITES
#define LOG_WRITE(...) LgiTrace(__VA_ARGS__)
#else
#define LOG_WRITE(...)
#endif

////////////////////////////////////////////////////////////////
// Implementations
void LSleep(DWORD i)
{
	::Sleep(i);
}

LString LCurrentUserName()
{
	TCHAR username[256];
	DWORD username_len = sizeof(username);
	GetUserName(username, &username_len);
	return username;
}

bool LGetMimeTypeExtensions(const char *Mime, LArray<LString> &Ext)
{
	auto Start = Ext.Length();
	char *e;

	LRegKey t(false, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type\\%s", Mime);
	if (t.IsOk() && (e = t.GetStr("Extension")))
	{
		if (*e == '.') e++;
		Ext.Add(e);
	}
	else
	{
		#define HardCodeExtention(mime, Ext1, Ext2) \
			else if (!stricmp(Mime, mime)) \
			{	if (Ext1) Ext.Add(Ext1); \
				if (Ext2) Ext.Add(Ext2); }

		const char *Null = NULL;
		if (!Mime);
		HardCodeExtention("text/calendar", "ics", Null)
		HardCodeExtention("text/x-vcard", "vcf", Null)
		HardCodeExtention("text/mbox", "mbx", "mbox")
		HardCodeExtention("text/html", "html", Null)
		HardCodeExtention("text/plain", "txt", Null)
		HardCodeExtention("message/rfc822", "eml", Null)
		HardCodeExtention("audio/mpeg", "mp3", Null)
		HardCodeExtention("application/msword", "doc", Null)
		HardCodeExtention("application/pdf", "pdf", Null)
	}

	return Ext.Length() > Start;
}

LString LGetFileMimeType(const char *File)
{
	if (File)
	{
		char *Dot = strrchr((char*)File, '.');
		if (Dot)
		{
			bool AssertOnError = LRegKey::AssertOnError;
			LRegKey::AssertOnError = false;
			
			LRegKey Key(false, "HKEY_CLASSES_ROOT\\%s", Dot);
			if (Key.IsOk())
			{
				char *Ct = Key.GetStr("Content Type");
				if (Ct &&
					!stricmp(Dot, ".dsw") == 0 &&
					!stricmp(Dot, ".dsp") == 0)
				{
					return Ct;
				}
				else
				{
					// Search mime type DB.
					LRegKey Db(false, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type");
					List<char> Sub;
					Db.GetKeyNames(Sub);
					for (auto k: Sub)
					{
						LRegKey Type(false, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\MIME\\Database\\Content Type\\%s", k);
						char *Ext = Type.GetStr("Extension");
						if (Ext && stricmp(Ext, Dot) == 0)
						{
							return k;
						}
					}

					Sub.DeleteArrays();

					// This is a hack to get around file types without a MIME database entry
					// but do have a .ext entry. LGetAppsForMimeType knows about the hack too.
					LString MimeType;	
					MimeType.Printf("application/%s", Dot);
					return MimeType;
				}
			}

			LRegKey::AssertOnError = AssertOnError;
		}

		// no extension?
		// no registry entry for file type?
		return "application/octet-stream";
	}

	return LString();
}

bool _GetApps_Add(LArray<LAppInfo> &Apps, char *In)
{
	LAutoString Path;

	if (!In)
		return false;
	while (*In && strchr(LWhiteSpace, *In))
		In++;
	if (!*In)
		return false;

	for (char *i = In; true; i++)
	{
		if (*i == '\'' || *i == '\"')
		{
			char delim = *i++;
			char *end = strchr(i, delim);
			if (!end) end = i + strlen(i);
			Path.Reset(NewStr(i, end-i));
			In = end + (*end != 0);
			break;
		}
		else if (!*i || strchr(LWhiteSpace, *i))
		{
			char old = *i;
			*i = 0;
			if (LFileExists(In))
			{
				Path.Reset(NewStr(In));
			}
			*i = old;
			if (Path)
			{
				In = i + (*i != 0);
				break;
			}
		}

		if (!*i)
			break;
	}

	if (Path)
	{
		LStringPipe p;

		char *RootVar = "%SystemRoot%";
		char *SysRoot = stristr(Path, RootVar);
		if (SysRoot)
		{
			// correct path for variables
			TCHAR Temp[256];
			UINT Ch = GetWindowsDirectory(Temp, CountOf(Temp));
			LString Tmp = Temp;
			if (Tmp(-1) != DIR_CHAR)
				Tmp += DIR_STR;
			p.Push(Tmp);

			char *End = SysRoot + strlen(RootVar);
			p.Push(*End == DIR_CHAR ? End + 1 : End);
		}
		else
		{
			p.Push(Path);
		}

		auto &a = Apps.New();
		a.Params = LString(In).Strip();
		a.Path = p.NewLStr();
		if (a.Path)
		{
			char e[MAX_PATH_LEN];
			char *d = strrchr(a.Path, DIR_CHAR);
			if (d) strcpy_s(e, sizeof(e), d + 1);
			else strcpy_s(e, sizeof(e), a.Path);
			d = strchr(e, '.');
			if (d) *d = 0;
			e[0] = toupper(e[0]);
			a.Name = e;
			if (ValidStr(a.Name))
			{
				bool AllCaps = true;
				for (char *s=a.Name; *s; s++)
				{
					if (islower(*s))
					{
						AllCaps = false;
						break;
					}
				}
				if (AllCaps)
				{
					Strlwr(a.Name.Get() + 1);
				}
			}
		}

		return true;
	}

	return false;
}

bool LGetAppsForMimeType(const char *Mime, LArray<LAppInfo> &Apps, int Limit)
{
	bool Status = false;

	if (Mime)
	{
		if (stricmp(Mime, "application/email") == 0)
		{
			// get email app
			LRegKey Key(false, "HKEY_CLASSES_ROOT\\mailto\\shell\\open\\command");
			if (Key.IsOk())
			{
				// get app path
				char *Str = Key.GetStr();
				// if (RegQueryValueEx(hKey, 0, 0, &Type, (uchar*)Str, &StrLen) == ERROR_SUCCESS)
				if (Str)
				{
					Status = _GetApps_Add(Apps, Str);
				}
			}
		}
		else if (!stricmp(Mime, "application/browser"))
		{
			// get default browser
			char *Keys[] = { "HKCU", "HKLM" };
			char Base[] = "SOFTWARE\\Clients\\StartMenuInternet";
			for (int i=0; i<CountOf(Keys); i++)
			{
				LRegKey k1(false, "%s\\%s", Keys[i], Base);
				if (k1.IsOk())
				{
					char *Def = k1.GetStr();
					if (Def)
					{
						for (int n=0; n<CountOf(Keys); n++)
						{
							LRegKey k2(false, "%s\\SOFTWARE\\Classes\\Applications\\%s\\shell\\open\\command", Keys[n], Def);
							if (k2.IsOk())
							{
								char *App = k2.GetStr();
								if (Status = _GetApps_Add(Apps, App))
								{
									break;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// get generic app
			char Ext[64] = "";
			char *EndPart = strrchr((char*)Mime, '/');
			if (EndPart && EndPart[1] == '.')
			{
				strcpy_s(Ext, sizeof(Ext), EndPart);

				// This is a hack to get around file types without a MIME database entry
				// but do have a .ext entry. LGetFileMimeType knows about the hack too.
				LRegKey ExtEntry(false, "HKEY_CLASSES_ROOT\\%s", Ext + 1);
				char *Name = ExtEntry.GetStr();
				if (Name)
				{
					LRegKey Edit(false, "HKEY_CLASSES_ROOT\\%s\\shell\\edit\\command", Name);
					char *App;
					if (Edit.IsOk() &&
						(App = Edit.GetStr()))
					{
						Status = _GetApps_Add(Apps, App);
					}
					else
					{
						LRegKey Open(false, "HKEY_CLASSES_ROOT\\%s\\shell\\open\\command", Name);
						char *App = Open.GetStr();
						if (App)
						{
							Status = _GetApps_Add(Apps, App);
						}
					}
				}
			}
			else
			{
				// This branch gets the list of application available to edit/open the file

				// Map the MIME type to a .ext
				LRegKey MimeEntry(false, "HKEY_CLASSES_ROOT\\MIME\\Database\\Content Type\\%s", Mime);
				char *e = MimeEntry.IsOk() ? MimeEntry.GetStr("Extension") : NULL;
				if (e)
					strcpy_s(Ext, sizeof(Ext), e);
				if (Ext[0])
				{
					// Get list of "Open With" apps
					LRegKey Other(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithList", Ext);
					char *Mru = NewStr(Other.GetStr("MRUList"));
					if (Mru)
					{
						for (char *a = Mru; *a; a++)
						{
							char Str[] = {*a, 0};
							char *Application = Other.GetStr(Str);
							if (Application)
							{
								LRegKey Shell(false, "HKEY_CLASSES_ROOT\\Applications\\%s\\shell", Application);
								List<char> Keys;
								if (Shell.GetKeyNames(Keys))
								{
									LRegKey First(false, "HKEY_CLASSES_ROOT\\Applications\\%s\\shell\\%s\\command", Application, Keys[0]);
									char *Path;
									if (Path = First.GetStr())
									{
										Status |= _GetApps_Add(Apps, Path);
									}
								}
								Keys.DeleteArrays();
							}
						}

						DeleteArray(Mru);
					}

					if (!Status)
					{
						// Explorers file extensions
						LRegKey FileExt(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s", Ext);
						char *Application;
						if (Application = FileExt.GetStr("Application"))
						{
							LRegKey App(false, "HKEY_CLASSES_ROOT\\Applications\\%s\\shell\\open\\command", Application);
							char *Path;
							if (Path = App.GetStr())
							{
								Status = _GetApps_Add(Apps, Path);
							}
						}
					}

					if (!Status)
					{
						// get classes location
						LRegKey ExtEntry(false, "HKEY_CLASSES_ROOT\\%s", Ext);
						LRegKey TypeEntry(false, "HKEY_CLASSES_ROOT\\%s\\shell\\open\\command", ExtEntry.GetStr());
						if (TypeEntry.IsOk())
						{
							auto Path = TypeEntry.GetStr();
							if (Path)
							{
								const char *c = Path;
								char *Part = LTokStr(c);
								if (Part)
								{
									char AppPath[256];
									_snprintf_s(AppPath, sizeof(AppPath), "\"%s\"", Part);
									Status = _GetApps_Add(Apps, AppPath);

									DeleteArray(Part);
								}
								else
								{
									Status = _GetApps_Add(Apps, Path);
								}
							}
						}
					}
				}
			}
		}
	}

	return Status;
}

LString LGetAppForMimeType(const char *Mime)
{
	LString App;
	LArray<LAppInfo> Apps;
	if (LGetAppsForMimeType(Mime, Apps, 1))
		App = Apps[0].Path.Get();
	return App;
}

int LRand(int i)
{
	return (rand() % i);
}

bool LPlaySound(const char *FileName, int Flags)
{
	bool Status = false;

	HMODULE hDll = LoadLibrary(_T("winmm.dll"));
	if (hDll)
	{
		typedef BOOL (__stdcall *Proc_sndPlaySound)(LPCSTR pszSound, UINT fuSound);
		Proc_sndPlaySound psndPlaySound = (Proc_sndPlaySound)GetProcAddress(hDll, "sndPlaySoundA");

		if (psndPlaySound)
		{
			if (LGetOs() == LGI_OS_WIN9X)
			{
				// async broken on 98?
				Flags = 0;
			}

			Status = psndPlaySound(FileName, Flags) != 0;
		}

		FreeLibrary(hDll);
	}

	return Status;
}

#include <lmerr.h>

LString LErrorCodeToString(uint32_t ErrorCode)
{
	LString Str;
    HMODULE hModule = NULL;
    LPSTR MessageBuffer = NULL;
    DWORD dwBufferLength;
    DWORD dwFormatFlags =	FORMAT_MESSAGE_ALLOCATE_BUFFER |
							FORMAT_MESSAGE_IGNORE_INSERTS |
							FORMAT_MESSAGE_FROM_SYSTEM ;

    if (ErrorCode >= NERR_BASE && ErrorCode <= MAX_NERR)
    {
        hModule = LoadLibraryEx(	TEXT("netmsg.dll"),
									NULL,
									LOAD_LIBRARY_AS_DATAFILE);
        if (hModule != NULL)
            dwFormatFlags |= FORMAT_MESSAGE_FROM_HMODULE;
    }

    if (dwBufferLength = FormatMessageA(dwFormatFlags,
										hModule, // module to get message from (NULL == system)
										ErrorCode,
										MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // default language
										(LPSTR) &MessageBuffer,
										0,
										NULL))
    {
		Str.Set(MessageBuffer, dwBufferLength);
        LocalFree(MessageBuffer);
    }

    if (hModule != NULL)
        FreeLibrary(hModule);
    
    return Str;
}

bool LExecute(const char *File, const char *Arguments, const char *Dir, LString *ErrorMsg)
{
	int Error = 0;
	HINSTANCE Status = NULL;
	
	if (!File)
		return false;

	uint64 Now = LCurrentTime();
	if (LGetOs() == LGI_OS_WIN9X)
	{
		auto f = LToNativeCp(File);
		auto a = LToNativeCp(Arguments);
		auto d = LToNativeCp(Dir);
		if (f)
		{
			Status = ShellExecuteA(NULL, "open", f, a, d, 5);
			if ((size_t)Status <= 32)
				Error = GetLastError();
		}
	}
	else
	{
		LAutoWString f(Utf8ToWide(File));
		LAutoWString a(Utf8ToWide(Arguments));
		LAutoWString d(Utf8ToWide(Dir));
		if (f)
		{
			Status = ShellExecuteW(NULL, L"open", f, a, d, 5);
			if ((size_t)Status <= 32)
				Error = GetLastError();
		}
	}

	#ifdef _DEBUG
	if ((size_t)Status <= 32)
		LgiTrace("ShellExecuteW failed with %p (LastErr=0x%x)\n", Status, Error);
	if (LCurrentTime() - Now > 1000)
		LgiTrace("ShellExecuteW took %I64i\n", LCurrentTime() - Now);
	#endif

	if (ErrorMsg)
		*ErrorMsg = LErrorCodeToString(Error);
	
	return (size_t)Status > 32;
}

////////////////////////////////////////////////////////////////////////////////////
HKEY GetRootKey(const char *s)
{
	HKEY Root = 0;

	#define TestKey(Short, Name) \
		if (!strncmp(s, #Short, strlen(#Short)) || \
			!strncmp(s, #Name,  strlen(#Name))) \
		{ \
			Root = Name; \
		}

	     TestKey(HKCR, HKEY_CLASSES_ROOT)
	else TestKey(HKCC, HKEY_CURRENT_CONFIG)
	else TestKey(HKCU, HKEY_CURRENT_USER)
	else TestKey(HKLM, HKEY_LOCAL_MACHINE)
	else TestKey(HKU, HKEY_USERS)

	#undef TestKey

	LAssert(Root);
	return Root;
}

bool LRegKey::AssertOnError = true;

LRegKey::LRegKey(bool WriteAccess, const char *Key, ...)
{
	char Buffer[1025];

	Root = (HKEY)-1;

	va_list Arg;
	va_start(Arg, Key);
	vsprintf_s(Buffer, Key, Arg);
	va_end(Arg);
	KeyName = Buffer;
	if (KeyName)
	{
		size_t Len = 0;
		char *SubKey = 0;

		#define TestKey(Long, Short) \
			if (!strnicmp(KeyName, #Long, Len = strlen(#Long)) || \
				!strnicmp(KeyName, #Short, Len = strlen(#Short))) \
			{ \
				Root = Long; \
				SubKey = KeyName.Get()[Len] ? KeyName.Get() + Len + 1 : 0; \
			}
		TestKey(HKEY_CLASSES_ROOT, HKCR)
		else TestKey(HKEY_CURRENT_CONFIG, HKCC)
		else TestKey(HKEY_CURRENT_USER, HKCU)
		else TestKey(HKEY_LOCAL_MACHINE, HKLM)
		else TestKey(HKEY_USERS, HKU)
		else return;

		result = RegOpenKeyExA(Root, SubKey, 0, WriteAccess ? KEY_ALL_ACCESS : KEY_READ, &k);
		if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
		{			
			DWORD err = GetLastError();
			if (AssertOnError)
				LAssert(!"RegOpenKeyEx failed");
		}
	}
}

LRegKey::~LRegKey()
{
	if (k) RegCloseKey(k);
}

LString LRegKey::GetErrorName()
{
	LError err(result);
	return err.GetMsg();
}

bool LRegKey::IsOk()
{
	return k != NULL;
}

bool LRegKey::Create()
{
	bool Status = false;

	if (!k && KeyName)
	{
		char *Sub = strchr(KeyName, '\\');
		if (Sub)
		{
			result = RegCreateKeyA(Root, Sub+1, &k);
			if (result == ERROR_SUCCESS)
			{
				Status = IsOk();
			}
			else
			{
				DWORD err = GetLastError();
				if (AssertOnError)
					LAssert(!"RegCreateKey failed");
			}
		}
	}

	return Status;
}

const char *LRegKey::Name()
{
	return KeyName;
}

bool LRegKey::DeleteValue(const char *Name)
{
	if (k)
	{
		if ((result = RegDeleteValueA(k, Name)) == ERROR_SUCCESS)
		{
			return true;
		}
		else
		{
			DWORD Err = GetLastError();
			LAssert(!"RegDeleteValue failed");
		}
	}

	return false;
}

bool LRegKey::DeleteKey()
{
	auto p = KeyName.SplitDelimit("\\", 1);
	if (p.Length() != 2)
		return false;

	if (k)
	{
		RegCloseKey(k);
		k = NULL;
	}
	
	HKEY Root = GetRootKey(p[0]);
	result = RegDeleteKeyA(Root, p[1]);
	if (result != ERROR_SUCCESS)
	{
		LgiTrace("RegDeleteKeyA(%p, %s) == %i\n", Root, p[1].Get(), result);
		if (AssertOnError)
			LAssert(!"RegDeleteKey failed.");
		return false;
	}
	KeyName.Empty();

	return true;
}

char *LRegKey::GetStr(const char *Name)
{
	if (!k)
	{
		LAssert(!"No key to read from.");
		return NULL;
	}

	DWORD Size = sizeof(s), Type;
	result = RegQueryValueExA(k, Name, 0, &Type, (uchar*)s, &Size);
	if (result != ERROR_SUCCESS)
	{
		if (AssertOnError)
			LAssert(!"RegQueryValueEx failed.");
		return NULL;
	}

	return s;
}

bool LRegKey::GetStr(const char *Name, LString &Str)
{
	if (!k)
	{
		if (AssertOnError)
			LAssert(!"No key to read from.");
		return false;
	}

	DWORD Size = 0, Type;
	result = RegQueryValueExA(k, Name, 0, &Type, NULL, &Size);
	if (result != ERROR_SUCCESS)
		goto OnError;

	{
		LString Tmp((char*)NULL, Size);
		result = RegQueryValueExA(k, Name, 0, &Type, (LPBYTE)Tmp.Get(), &Size);
		if (result != ERROR_SUCCESS)
			goto OnError;
			
		Str = Tmp;
		return true;
	}

OnError:
	if (AssertOnError)
		LAssert(!"RegQueryValueEx failed.");
	return false;
}

bool LRegKey::SetStr(const char *Name, const char *Value)
{
	if (!k)
	{
		LAssert(!"No key open.");
		return false;
	}

	result = RegSetValueExA(k, Name, 0, REG_SZ, (uchar*)Value, Value ? (DWORD)strlen(Value) : 0);
	LOG_WRITE("RegSetValueExA(%s,%s,'%s')=%i\n", KeyName.Get(), Name, Value, result);
	if (result != ERROR_SUCCESS)
	{
		if (AssertOnError)
			LAssert(!"RegSetValueEx failed.");
		return false;
	}
	
	return true;
}

bool LRegKey::GetInt(const char *Name, uint32_t &Value)
{
	if (!k) return false;
	DWORD Size = sizeof(Value), Type;
	result = RegQueryValueExA(k, Name, 0, &Type, (uchar*)&Value, &Size);
	return result == ERROR_SUCCESS;
}

bool LRegKey::SetInt(const char *Name, uint32_t Value)
{
	if (!k)
	{
		LgiTrace("%s:%i - No key name.\n", _FL);
		return false;
	}
	
	result = RegSetValueExA(k, Name, 0, REG_DWORD, (uchar*)&Value, sizeof(Value));
	LOG_WRITE("RegSetValueExA(%s,%s,%i)=%i\n", KeyName.Get(), Name, Value, result);
	if (result == ERROR_SUCCESS)
		return true;

	LgiTrace("%s:%i - RegSetValueExA(%s) returned error: %x.\n", _FL, Name, result);
	return false;
}

bool LRegKey::GetBinary(const char *Name, void *&Ptr, int &Len)
{
	DWORD Size = 0, Type;
	if (k && (result = RegQueryValueExA(k, Name, 0, &Type, 0, &Size)) == ERROR_SUCCESS)
	{
		Len = Size;
		Ptr = new uchar[Len];
		result = RegQueryValueExA(k, Name, 0, &Type, (uchar*)Ptr, &Size);
		return result == ERROR_SUCCESS;
	}

	return false;
}

bool LRegKey::SetBinary(const char *Name, void *Ptr, int Len)
{
	LAssert(!"Not impl.");
	return false;
}

bool LRegKey::GetKeyNames(List<char> &n)
{
	FILETIME t;
	TCHAR Buf[256];
	DWORD Size = CountOf(Buf), i = 0;
	while ((result = RegEnumKeyEx(k, i++, Buf, &Size, 0, 0, 0, &t)) == ERROR_SUCCESS)
	{
		n.Insert(WideToUtf8(Buf));
		Size = sizeof(Buf);
	}
	return n.Length() > 0;
}

bool LRegKey::GetValueNames(List<char> &n)
{
	TCHAR Buf[256];
	DWORD Type, Size = CountOf(Buf), i = 0;
	while ((result = RegEnumValue(k, i++, Buf, &Size, 0, &Type, 0, 0)) == ERROR_SUCCESS)
	{
		n.Insert(WideToUtf8(Buf));
		Size = sizeof(Buf);
	}
	return n.Length() > 0;
}

//////////////////////////////////////////////////////////////////////////////////////
LString WinGetSpecialFolderPath(int Id)
{
	LLibrary Shell("Shell32");
	LString s;
	char16 wp[MAX_PATH_LEN] = { 0 };
	pSHGetSpecialFolderPathW w = (pSHGetSpecialFolderPathW) Shell.GetAddress("SHGetSpecialFolderPathW");
	if (w)
	{
		BOOL result = w(0, wp, Id, false);
		if (result && ValidStrW(wp))
		{
			LAutoString Tmp(WideToUtf8(wp));
			s = Tmp;
		}
		else
		{
			DWORD e = GetLastError();
			LAssert(!"Error getting system folder.");
		}
	}

	return s;
}

//////////////////////////////////////////////////////////////////////
#ifndef LGI_STATIC
static bool AssertUiOpen = false;

void LAssertDlg(LString Msg, std::function<void(int)> Callback)
{
	AssertUiOpen = true;
	auto a = new LAlert(LAppInst ? LAppInst->AppWnd : NULL, "Assert Failed", Msg, "Abort", "Debug", "Ignore");
	a->SetAppModal();
	a->DoModal([Callback](auto d, auto code)
	{
		if (Callback)
			Callback(code);
		delete d;
		AssertUiOpen = false;
	});
}
#endif

void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	if (!b)
	{
		#ifdef LGI_STATIC

			assert(b);

		#else

			if (AssertUiOpen || !LAppInst || !LSysFont)
			{
				// Woah boy...
				LgiTrace("%s:%i - Assert: %s failed.\n", file, line, test);
			}
			else
			{
				LgiTrace("%s:%i - Assert failed:\n%s\n", file, line, test);

				#ifdef _DEBUG

					LString Msg;
					Msg.Printf("Assert failed, file: %s, line: %i\n%s", file, line, test);

					int Result = 0;
					if (LAppInst->InThread())
					{
						// We are in the GUI thread, show the dialog inline
						LAssertDlg(Msg, [](auto Result)
						{
							switch (Result)
							{
								case 1:
								{
									exit(-1);
									break;
								}
								case 2:
								{
									// Bring up the debugger...
									#if defined(_WIN64) || !defined(_MSC_VER)
									assert(0);
									#else
									_asm int 3
									#endif
									break;
								}
								default:
								case 3:
								{
									break;
								}
							}			
						});
					}
					else
					{
						// Fall back to windows UI
						assert(0);
					}

				#endif
			}

		#endif
	}
}

//////////////////////////////////////////////////////////////////////
// The following code is from:
// Web: http://www.codeproject.com/Articles/28071/Toggle-hardware-data-read-execute-breakpoints-prog
// License: http://www.codeproject.com/info/cpol10.aspx
struct HWBRK
{
	void *a;
	HANDLE hT;
	HWBRK_TYPE Type;
	HWBRK_SIZE Size;
	HANDLE hEv;
	int iReg;
	int Opr;
	bool SUCC;

	HWBRK()
	{
		Opr = 0;
		a = 0;
		hT = 0;
		hEv = 0;
		iReg = 0;
		SUCC = false;
	}
};

static void SetBits(DWORD_PTR& dw, int lowBit, int bits, int newValue)
{
	DWORD_PTR mask = (1 << bits) - 1; 
	dw = (dw & ~(mask << lowBit)) | (newValue << lowBit);
}

static DWORD WINAPI BreakpointThread(LPVOID lpParameter)
{
	HWBRK* h = (HWBRK*)lpParameter;
	int j = 0;
	int y = 0;

	j = SuspendThread(h->hT);
    y = GetLastError();

	CONTEXT ct = {0};
	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	j = GetThreadContext(h->hT,&ct);
    y = GetLastError();

	int FlagBit = 0;

	bool Dr0Busy = false;
	bool Dr1Busy = false;
	bool Dr2Busy = false;
	bool Dr3Busy = false;
	if (ct.Dr7 & 1)
		Dr0Busy = true;
	if (ct.Dr7 & 4)
		Dr1Busy = true;
	if (ct.Dr7 & 16)
		Dr2Busy = true;
	if (ct.Dr7 & 64)
		Dr3Busy = true;

	if (h->Opr == 1)
	{
		// Remove
		if (h->iReg == 0)
		{
			FlagBit = 0;
			ct.Dr0 = 0;
			Dr0Busy = false;
		}
		if (h->iReg == 1)
		{
			FlagBit = 2;
			ct.Dr1 = 0;
			Dr1Busy = false;
		}
		if (h->iReg == 2)
		{
			FlagBit = 4;
			ct.Dr2 = 0;
			Dr2Busy = false;
		}
		if (h->iReg == 3)
		{
			FlagBit = 6;
			ct.Dr3 = 0;
			Dr3Busy = false;
		}

		ct.Dr7 &= ~(1 << FlagBit);
	}
	else
	{
		if (!Dr0Busy)
		{
			h->iReg = 0;
			ct.Dr0 = (DWORD_PTR)h->a;
			Dr0Busy = true;
		}
		else if (!Dr1Busy)
		{
			h->iReg = 1;
			ct.Dr1 = (DWORD_PTR)h->a;
			Dr1Busy = true;
		}
		else if (!Dr2Busy)
		{
			h->iReg = 2;
			ct.Dr2 = (DWORD_PTR)h->a;
			Dr2Busy = true;
		}
		else if (!Dr3Busy)
		{
			h->iReg = 3;
			ct.Dr3 = (DWORD_PTR)h->a;
			Dr3Busy = true;
		}
		else
		{
			h->SUCC = false;
			j = ResumeThread(h->hT);
			y = GetLastError();
			SetEvent(h->hEv);
			return 0;
		}
		ct.Dr6 = 0;
		int st = 0;
		if (h->Type == HWBRK_TYPE_CODE)
			st = 0;
		if (h->Type == HWBRK_TYPE_READWRITE)
			st = 3;
		if (h->Type == HWBRK_TYPE_WRITE)
			st = 1;
		int le = 0;
		if (h->Size == HWBRK_SIZE_1)
			le = 0;
		if (h->Size == HWBRK_SIZE_2)
			le = 1;
		if (h->Size == HWBRK_SIZE_4)
			le = 3;
		if (h->Size == HWBRK_SIZE_8)
			le = 2;

		SetBits(ct.Dr7, 16 + h->iReg*4, 2, st);
		SetBits(ct.Dr7, 18 + h->iReg*4, 2, le);
		SetBits(ct.Dr7, h->iReg*2,1,1);
	}

	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	j = SetThreadContext(h->hT,&ct);
    y = GetLastError();

	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	j = GetThreadContext(h->hT,&ct);
    y = GetLastError();

	j = ResumeThread(h->hT);
    y = GetLastError();

	h->SUCC = true;

	SetEvent(h->hEv);
	return 0;
}

HANDLE SetHardwareBreakpoint(HANDLE hThread, HWBRK_TYPE Type, HWBRK_SIZE Size, void *s)
{
	HWBRK *h = new HWBRK;
	h->a = s;
	h->Size = Size;
	h->Type = Type;
	h->hT = hThread;

	if (hThread == GetCurrentThread())
	{
		DWORD pid = LCurrentThreadId();
		h->hT = OpenThread(THREAD_ALL_ACCESS,0,pid);
	}

	h->hEv = CreateEvent(0, 0, 0, 0);
	h->Opr = 0; // Set Break
	HANDLE hY = CreateThread(0, 0, BreakpointThread, (LPVOID)h, 0, 0);
	WaitForSingleObject(h->hEv, INFINITE);
	CloseHandle(h->hEv);
	h->hEv = 0;

	if (hThread == GetCurrentThread())
	{
		CloseHandle(h->hT);
	}

	h->hT = hThread;

	if (!h->SUCC)
	{
		delete h;
		return 0;
	}

	return (HANDLE)h;
}

bool RemoveHardwareBreakpoint(HANDLE hBrk)
{
	HWBRK* h = (HWBRK*)hBrk;
	if (!h)
		return false;

	bool C = false;
	if (h->hT == GetCurrentThread())
	{
		DWORD pid = LCurrentThreadId();
		h->hT = OpenThread(THREAD_ALL_ACCESS,0,pid);
		C = true;
	}

	h->hEv = CreateEvent(0,0,0,0);
	h->Opr = 1; // Remove Break
	HANDLE hY = CreateThread(0,0,BreakpointThread,(LPVOID)h,0,0);
	WaitForSingleObject(h->hEv,INFINITE);
	CloseHandle(h->hEv);
	h->hEv = 0;

	if (C)
	{
		CloseHandle(h->hT);
	}

	delete h;
	return true;
}

