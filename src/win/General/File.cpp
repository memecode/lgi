/*hdr
**	FILE:		File.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		11/7/95
**	DESCRIPTION:	The new file subsystem
**
**	Copyright (C) 1995, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes ************************************************************************************/
#include <tchar.h>
#include <winsock2.h>
#include <shlobj.h>
#include <fcntl.h>
#include <Userenv.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifdef _MSC_VER
#include <direct.h>
#include <tchar.h>
#endif

#include "lgi/common/Lgi.h"
#include "lgi/common/Thread.h"

/****************************** Defines *************************************************************************************/
#define FILEDEBUG
#define CHUNK					0xFFF0

#define FLOPPY_360K				0x0001
#define FLOPPY_720K				0x0002
#define FLOPPY_1_2M				0x0004
#define FLOPPY_1_4M				0x0008
#define FLOPPY_5_25				(FLOPPY_360K | FLOPPY_1_2M)
#define FLOPPY_3_5				(FLOPPY_720K | FLOPPY_1_4M)

/****************************** Helper Functions ****************************************************************************/
LString LFile::Path::Sep(DIR_STR);

bool LFile::Path::FixCase()
{
	// Windows is case insensitive.. so meh... do nothing.
	return true;
}

char *LReadTextFile(const char *File)
{
	char *s = 0;
	LFile f;
	if (File && f.Open(File, O_READ))
	{
		auto Len = f.GetSize();
		s = new char[Len+1];
		if (s)
		{
			auto Read = f.Read(s, Len);
			s[Read] = 0;
		}
	}
	return s;
}

int64 LFileSize(const char *FileName)
{
	int64 Size = -1;
	LDirectory Dir;
	if (Dir.First(FileName, 0))
	{
		Size = Dir.GetSize();
	}
	return Size;
}

bool LFileExists(const char *Name, char *CorrectCase)
{
	bool Status = false;
	if (Name)
	{
		HANDLE hFind = INVALID_HANDLE_VALUE;
		
		LAutoWString n(Utf8ToWide(Name));
		if (n)
		{
			WIN32_FIND_DATAW Info;
			hFind = FindFirstFileW(n, &Info);
			if (hFind != INVALID_HANDLE_VALUE)
				Status = StrcmpW(Info.cFileName, L".") != 0;
		}

		if (hFind != INVALID_HANDLE_VALUE)
		{
			FindClose(hFind);
			return true;
		}
	}

	return false;
}

bool LDirExists(const char *Dir, char *CorrectCase)
{
	LAutoWString n(Utf8ToWide(Dir));
	DWORD e = GetFileAttributesW(n);
	return e != 0xFFFFFFFF && TestFlag(e, FILE_ATTRIBUTE_DIRECTORY);
}

const char *GetErrorName(int e)
{
	static char Buf[256];
	char *s = Buf;

	Buf[0] = 0;
	::FormatMessageA(	// FORMAT_MESSAGE_ALLOCATE_BUFFER | 
						FORMAT_MESSAGE_IGNORE_INSERTS |
						FORMAT_MESSAGE_FROM_SYSTEM,
						NULL, 
						e,
						MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
						(LPSTR)Buf, 
						sizeof(Buf), 
						NULL);

	s = Buf + strlen(Buf);
	while (s > Buf && strchr(" \t\r\n", s[-1]))
	{
		s--;
		*s = 0;
	}

	return Buf[0] ? Buf : 0;
}

template<typename T>
int _GetLongPathName
(
	T *Short,
	T *Long,
	int LongLen
)
{
	bool Status = false;
	int Len = 0;

	if (Short && Long)
	{
		HMODULE hDll = 0;

		hDll = LoadLibrary(_T("kernel32.dll"));
		if (hDll)
		{
			// Win98/ME/2K... short->long conversion supported
			#ifdef UNICODE
			typedef DWORD (__stdcall *Proc_GetLongPathName)(LPCWSTR, LPWSTR, DWORD cchBuffer);
			Proc_GetLongPathName Proc = (Proc_GetLongPathName)GetProcAddress(hDll, "GetLongPathNameW");
			#else
			typedef DWORD (__stdcall *Proc_GetLongPathName)(LPCSTR, LPSTR, DWORD);
			Proc_GetLongPathName Proc = (Proc_GetLongPathName)GetProcAddress(hDll, "GetLongPathNameA");
			#endif
			if (Proc)
			{
				Len = Proc(Short, Long, LongLen);
				Status = true;
			}
			FreeLibrary(hDll);
		}
		
		if (!Status)
		{
			// Win95/NT4 doesn't support this function... so we do the conversion ourselves
			TCHAR *In = Short;
			TCHAR *Out = Long;

			*Out = 0;

			while (*In)
			{
				// find end of segment
				TCHAR *e = Strchr(In, DIR_CHAR);
				if (!e) e = In + Strlen(In);

				// process segment
				TCHAR Old = *e;
				*e = 0;
				if (Strchr(In, ':'))
				{
					// drive specification
					Strcat(Long, LongLen, In); // just copy over
				}
				else
				{
					TCHAR Sep[] = {DIR_CHAR, 0};
					Strcat(Out, LongLen, Sep);

					if (Strlen(In) > 0)
					{
						// has segment to work on
						WIN32_FIND_DATA Info;
						HANDLE Search = FindFirstFile(Short, &Info);
						if (Search != INVALID_HANDLE_VALUE)
						{
							// have long name segment, copy over
							Strcat(Long, LongLen, Info.cFileName);
							FindClose(Search);
						}
						else
						{
							// no long name segment... copy over short segment :(
							Strcat(Long, LongLen, In);
						}
					}
					// else is a double path char... i.e. as in a M$ network path
				}

				In = (Old) ? e + 1 : e;
				Out += Strlen(Out);
				*e = Old;
			}
		}
	}

	return Len;
}

bool LResolveShortcut(const char *LinkFile, char *Path, ssize_t Len) 
{
	bool Status = false;

	HWND hwnd = NULL;
	HRESULT hres;
	IShellLink* psl;
	TCHAR szGotPath[DIR_PATH_SIZE] = {0};
	WIN32_FIND_DATA wfd;

	CoInitialize(0);
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**) &psl);

	if (SUCCEEDED(hres))
	{
		IPersistFile* ppf;

		hres = psl->QueryInterface(IID_IPersistFile, (void**) &ppf);
		if (SUCCEEDED(hres))
		{
			char16 wsz[DIR_PATH_SIZE];

			MultiByteToWideChar(CP_ACP, 0, LinkFile, -1, wsz, DIR_PATH_SIZE);

			char16 *l = StrrchrW(wsz, '.');
			if (l && StricmpW(l, L".lnk"))
			{
				StrcatW(wsz, L".lnk");
			}

			hres = ppf->Load(wsz, STGM_READ);
			if (SUCCEEDED(hres))
			{
				hres = psl->Resolve(hwnd, SLR_ANY_MATCH);
				if (SUCCEEDED(hres))
				{
					hres = psl->GetPath(szGotPath, DIR_PATH_SIZE, (WIN32_FIND_DATA *)&wfd, SLGP_SHORTPATH );

					if (SUCCEEDED(hres) && Strlen(szGotPath) > 0)
					{
						#ifdef UNICODE
						TCHAR TmpPath[MAX_PATH_LEN];
						ssize_t TpLen = _GetLongPathName(szGotPath, TmpPath, CountOf(TmpPath)) * sizeof(TmpPath[0]);
						const void *Tp = TmpPath;
						ssize_t OutLen = LBufConvertCp(Path, "utf-8", Len-1, Tp, LGI_WideCharset, TpLen);
						Path[OutLen] = 0;
						#else
						_GetLongPathName(szGotPath, Path, Len);
						#endif
						Status = true;
					}
				}
			}

			ppf->Release();
		}

		psl->Release();
	}

	CoUninitialize();

	return Status;
}

void WriteStr(LFile &f, const char *s)
{
	auto Len = (s) ? strlen(s) : 0;
	f << Len;
	if (Len > 0)
	{
		f.Write(s, Len);
	}
}

char *ReadStr(LFile &f DeclDebugArgs)
{
	char *s = 0;

	// read the strings length...
	uint32_t Len;
	f >> Len;

	if (Len > 0)
	{
		// 16mb sanity check.... anything over this
		// is _probably_ an error
		if (Len >= (16 << 20))
		{
			// LAssert(0);
			return 0;
		}

		// allocate the memory buffer
		#if defined MEMORY_DEBUG
		s = new(_file, _line) char[Len+1];
		#else
		s = new char[Len+1];
		#endif
		if (s)
		{
			// read the bytes from disk
			f.Read(s, Len);
			s[Len] = 0;


		}
		else
		{
			// memory allocation error, skip the data
			// on disk so the caller is where they think 
			// they are in the file.
			f.Seek(Len, SEEK_CUR);
		}
	}

	return s;
}

ssize_t SizeofStr(const char *s)
{
	return sizeof(uint32_t) + ((s) ? strlen(s) : 0);
}

bool LGetDriveInfo
(
	char *Path,
	uint64 *Free,
	uint64 *Size,
	uint64 *Available
)
{
	bool Status = false;
	ULARGE_INTEGER available = {0, 0};
	ULARGE_INTEGER total = {0, 0};
	ULARGE_INTEGER free = {0, 0};

	if (Path)
	{
		LAutoWString w(Utf8ToWide(Path));
		if (w)
		{
			char16 *d = StrchrW(w, DIR_CHAR);
			if (d) *d = 0;

			if (GetDiskFreeSpaceExW(w,
									&available,
									&total,
									&free))
			{
				if (Free) *Free = free.QuadPart;
				if (Size) *Size = total.QuadPart;
				if (Available) *Available = available.QuadPart;
				Status = true;
			}
		}
	}

	return Status;
}
 
/****************************** Classes ***************************************/
struct LVolumePriv
{
	LString Name;
	LString Path;
	int Type = VT_NONE;
	int Flags = 0;	// VA_??
	int64 Size = 0;
	int64 Free = 0;
	LSystemPath SysPath = LSP_ROOT;
	LAutoPtr<LSurface> Icon;
	LVolume *NextVol = NULL, *ChildVol = NULL;

	LVolumePriv(LSystemPath sysPath, const char *name)
	{
		SysPath = sysPath;
		Name = name;
		Type = VT_FOLDER;

		int Id = 0;
		switch (Type)
		{
			case LSP_HOME:
			{
				Id = CSIDL_PROFILE;
				break;
			}
			case LSP_DESKTOP:
			{
				Type = VT_DESKTOP;
				Id = CSIDL_DESKTOPDIRECTORY;
				break;
			}
		}
		
		if (Id)
			Path = WinGetSpecialFolderPath(Id);
		else
			Path = LGetSystemPath(SysPath);
	}

	LVolumePriv(const char *Drive)
	{
		int type = GetDriveTypeA(Drive);
		if (type != DRIVE_UNKNOWN &&
			type != DRIVE_NO_ROOT_DIR)
		{
			char Buf[DIR_PATH_SIZE];
			const char *Desc = NULL;
			switch (type)
			{
				case DRIVE_REMOVABLE:
				{
					Type = VT_USB_FLASH;

					if (GetVolumeInformationA(Drive, Buf, sizeof(Buf), 0, 0, 0, 0, 0) &&
						ValidStr(Buf))
						Desc = Buf;						
					else
						Desc = "Usb Drive";
					break;
				}
				case DRIVE_REMOTE:
					Desc = "Network";
					Type = VT_NETWORK_SHARE;
					break;
				case DRIVE_CDROM:
					Desc = "Cdrom";
					Type = VT_CDROM;
					break;
				case DRIVE_RAMDISK:
					Desc = "Ramdisk";
					Type = VT_RAMDISK;
					break;
				case DRIVE_FIXED:
				default:
				{
					if (GetVolumeInformationA(Drive, Buf, sizeof(Buf), 0, 0, 0, 0, 0) &&
						ValidStr(Buf))
						Desc = Buf;
					else
						Desc = "Hard Disk";
					Type = VT_HARDDISK;

					ULARGE_INTEGER Avail, TotalBytes, FreeBytes;
					if (GetDiskFreeSpaceExA(Drive, &Avail, &TotalBytes, &FreeBytes))
					{
						Free = FreeBytes.QuadPart;
						Size = TotalBytes.QuadPart;
					}
					break;
				}
			}

			char s[DIR_PATH_SIZE];
			if (Desc)
				sprintf_s(s, sizeof(s), "%s (%.2s)", Desc, Drive);
			else
				sprintf_s(s, sizeof(s), "%s", Drive);
			Name = s;
			Path = Drive;
		}
	}

	~LVolumePriv()
	{
		DeleteObj(NextVol);
		DeleteObj(ChildVol);
	}

	void Insert(LVolume *newVol)
	{
		if (ChildVol)
		{
			for (auto v = ChildVol; v; v = v->d->NextVol)
			{
				if (!v->d->NextVol)
				{
					v->d->NextVol = newVol;
					break;
				}
			}
		}
		else ChildVol = newVol;
	}	

	LVolume *First()
	{
		if (!ChildVol)
		{
			if (SysPath == LSP_DESKTOP)
			{
				// Add some basic shortcuts
				LSystemPath Paths[] = {LSP_HOME, LSP_USER_DOCUMENTS, LSP_USER_MUSIC, LSP_USER_VIDEO, LSP_USER_DOWNLOADS, LSP_USER_PICTURES};
				const char *Names[] = {"Home",   "Documents",        "Music",        "Video",        "Downloads",        "Pictures"};
				for (unsigned i=0; i<CountOf(Paths); i++)
					Insert(new LVolume(Paths[i], Names[i]));
			}
			else if (SysPath == LSP_MOUNT_POINT)
			{
				// Get drive list
				char Str[512];
				if (GetLogicalDriveStringsA(sizeof(Str), Str) > 0)
					for (char *p = Str; *p; p += strlen(p) + 1)
						Insert(new LVolume(p));
			}
		}

		return ChildVol;
	}

	LVolume *Next()
	{
		if (SysPath == LSP_DESKTOP && !NextVol)
		{
			NextVol = new LVolume(LSP_MOUNT_POINT, "Drives");
		}

		return NextVol;
	}
};

LVolume::LVolume(const char *Path)
{
	d = new LVolumePriv(Path);
}

LVolume::LVolume(LSystemPath SysPath, const char *Name)
{
	d = new LVolumePriv(SysPath, Name);
}

LVolume::~LVolume()
{
	DeleteObj(d);
}

const char *LVolume::Name() const { return d->Name; }
const char *LVolume::Path() const { return d->Path; }
int LVolume::Type()			const { return d->Type; } // VT_??
int LVolume::Flags()		const { return d->Flags; }
uint64 LVolume::Size()		const { return d->Size; }
uint64 LVolume::Free()		const { return d->Free; }
LSurface *LVolume::Icon()	const { return d->Icon; }

bool LVolume::IsMounted() const
{
	return false;
}

bool LVolume::SetMounted(bool Mount)
{
	return Mount;
}

void LVolume::Insert(LAutoPtr<LVolume> v)
{
    d->Insert(v.Release());
}
	
LVolume *LVolume::First()
{
	return d->First();
}

LVolume *LVolume::Next()
{
	return d->Next();
}

LDirectory *LVolume::GetContents()
{
	LDirectory *Dir = 0;
	if (d->Path.Get())
	{
		if (Dir = new LDirectory)
		{
			if (!Dir->First(d->Path))
			{
				DeleteObj(Dir);
			}
		}
	}
	return Dir;
}

////////////////////////////////////////////////////////////////////////////////
LFileSystem *LFileSystem::Instance = 0;

class LFileSystemPrivate
{
public:
	LFileSystemPrivate()
	{
	}

	~LFileSystemPrivate()
	{
	}
};

LFileSystem::LFileSystem()
{
	Instance = this;
	d = new LFileSystemPrivate;
	Root = 0;
}

LFileSystem::~LFileSystem()
{
	DeleteObj(Root);
	DeleteObj(d);
}

void LFileSystem::OnDeviceChange(char *Reserved)
{
	DeleteObj(Root);
}

LVolume *LFileSystem::GetRootVolume()
{
	if (!Root)
		Root = new LVolume(LSP_DESKTOP, "Desktop");

	return Root;
}

bool LFileSystem::Copy(const char *From, const char *To, LError *ErrorCode, CopyFileCallback Callback, void *Token)
{
	if (!From || !To)
	{
		if (ErrorCode) *ErrorCode = ERROR_INVALID_PARAMETER;
		return false;
	}

	LFile In, Out;
	if (!In.Open(From, O_READ))
	{
		if (ErrorCode) *ErrorCode = In.GetError();
		return false;
	}

	if (!Out.Open(To, O_WRITE))
	{
		if (ErrorCode) *ErrorCode = Out.GetError();
		return false;
	}

	if (Out.SetSize(0))
	{
		if (ErrorCode) *ErrorCode = ERROR_WRITE_FAULT;
		return false;
	}

	int64 Size = In.GetSize();
	if (!Size)
	{
		return true;
	}

	int64 Block = min((1 << 20), Size);
	char *Buf = new char[Block];
	if (!Buf)
	{
		if (ErrorCode) *ErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		return false;
	}

	int64 i = 0;
	while (i < Size)
	{
		auto r = In.Read(Buf, Block);
		if (r > 0)
		{
			ssize_t Written = 0;
			while (Written < r)
			{
				auto w = Out.Write(Buf + Written, r - Written);
				if (w > 0)
				{
					Written += w;
				}
				else
				{
					if (ErrorCode) *ErrorCode = ERROR_WRITE_FAULT;
					goto ExitCopyLoop;
				}
			}
			i += Written;

			if (Callback)
			{
				if (!Callback(Token, i, Size))
				{
					break;
				}
			}
		}
		else break;
	}

	ExitCopyLoop:
	DeleteArray(Buf);
	if (i == Size)
	{
		if (ErrorCode) *ErrorCode = ERROR_SUCCESS;
	}
	else
	{
		Out.Close();
		Delete(To, false);
	}

	return i == Size;
}

bool LFileSystem::Delete(LArray<const char*> &Files, LArray<LError> *Status, bool ToTrash)
{
	bool Ret = true;

	if (ToTrash)
	{
		LArray<char16> Name;
		for (int i=0; i<Files.Length(); i++)
		{
			LAutoPtr<wchar_t> w(Utf8ToWide(Files[i]));
			auto Chars = StrlenW(w);
			auto Len = Name.Length();
			Name.Length(Len + Chars + 1);
			StrcpyW(Name.AddressOf(Len), w.Get());
		}
		Name.Add(NULL);

		SHFILEOPSTRUCTW s;
		ZeroObj(s);
		s.hwnd = 0;
		s.wFunc = FO_DELETE;
		s.pFrom = Name.AddressOf();
		s.fFlags = FOF_NOCONFIRMATION; // FOF_ALLOWUNDO;

		int e = SHFileOperationW(&s);
		Ret = e == 0;
		if (Status)
		{
			int val = ERROR_SUCCESS;
			if (s.fAnyOperationsAborted)
				val = ERROR_CANCELLED;
			else if (e)
				val = ERROR_CANT_DELETE_LAST_ITEM; // There isn't a better option here...				

			for (int i=0; i<Files.Length(); i++)
				(*Status)[i].Set(val);
		}
	}
	else
	{
		for (int i=0; i<Files.Length(); i++)
		{
			DWORD e = 0;

			LAutoWString n(Utf8ToWide(Files[i]));
			if (n)
			{
				SetFileAttributesW(n, FILE_ATTRIBUTE_ARCHIVE);
				if (!::DeleteFileW(n))
				{
					e = GetLastError();
				}
			}

			if (e && Status)
			{
				(*Status)[i].Set(e);
				Ret = false;
			}
		}
	}

	return Ret;
}

bool LFileSystem::Delete(const char *FileName, bool ToTrash)
{
	if (!FileName)
		return false;

	LArray<const char*> Files;
	Files.Add(FileName);
	return Delete(Files, 0, ToTrash);
}

bool LFileSystem::CreateFolder(const char *PathName, bool CreateParentFoldersIfNeeded, LError *Err)
{
	LAutoWString w(Utf8ToWide(PathName));
	bool Status = ::CreateDirectoryW(w, NULL) != 0;
	if (!Status)
	{
		int Code = GetLastError();
		if (Err)
			Err->Set(Code);
		
		if (CreateParentFoldersIfNeeded &&
			Code == ERROR_PATH_NOT_FOUND)
		{
			char Base[DIR_PATH_SIZE];
			strcpy_s(Base, sizeof(Base), PathName);
			do
			{
				char *Leaf = strrchr(Base, DIR_CHAR);
				if (!Leaf) return false;
				*Leaf = 0;
			}
			while (!LDirExists(Base));
			
			LString::Array Parts = LString(PathName + strlen(Base)).SplitDelimit(DIR_STR);
			for (int i=0; i<Parts.Length(); i++)
			{
				LMakePath(Base, sizeof(Base), Base, Parts[i]);
				LAutoWString w(Utf8ToWide(Base));
				Status = ::CreateDirectoryW(w, NULL) != 0;
				if (!Status)
				{
					if (Err)
						Err->Set(GetLastError());
					break;
				}
			}
		}
	}	
	
	return Status;
}

bool LFileSystem::RemoveFolder(const char *PathName, bool Recurse)
{
	if (Recurse)
	{
		LDirectory Dir;
		if (Dir.First(PathName))
		{
			do
			{
				char Str[DIR_PATH_SIZE];
				if (Dir.Path(Str, sizeof(Str)))
				{
					if (Dir.IsDir())
					{
						RemoveFolder(Str, Recurse);
					}
					else
					{
						Delete(Str, false);
					}
				}
			}
			while (Dir.Next());
		}
	}

	#ifdef UNICODE
	LAutoWString w(Utf8ToWide(PathName));
	if (!::RemoveDirectory(w))
	#else
	if (!::RemoveDirectory(PathName))
	#endif
	{
		#ifdef _DEBUG
		DWORD e = GetLastError();
		#endif

		return false;
	}

	return true;
}

bool LFileSystem::Move(const char *OldName, const char *NewName, LError *Err)
{
	if (!OldName || !NewName)
	{
		if (Err) Err->Set(LErrorInvalidParam);
		return false;
	}
		
	LAutoWString New(Utf8ToWide(NewName));
	LAutoWString Old(Utf8ToWide(OldName));
	if (!New || !Old)
	{
		if (Err) Err->Set(LErrorNoMem);
		return false;
	}

	if (!::MoveFileW(Old, New))
	{
		if (Err) Err->Set(GetLastError());
		return false;
	}

	return true;
}

/*
bool LFileSystem::GetVolumeInfomation(char Drive, VolumeInfo *pInfo)
{
	bool Status = false;

	if (pInfo)
	{
		char Name[] = "A:\\";
		uint32 Serial;

		Name[0] = Drive + 'A';

		if (GetVolumeInformation(	Name,
						pInfo->Name,
						sizeof(pInfo->Name),
						&Serial,
						&pInfo->MaxPath,
						&pInfo->Flags,
						pInfo->System,
						sizeof(pInfo->System)))
		{
			pInfo->Drive = Drive;
			Status = true;
		}
	}

	return Status;
}
*/

void GetFlagsStr(char *flagstr, short flags)
{
	if (flagstr)
	{
		flagstr[0] = 0;

		if (flags & O_WRONLY)
		{
			strcat(flagstr,"w");
		}
		else if (flags & O_APPEND)
		{
			strcat(flagstr,"a");
		}
		else
		{
			strcat(flagstr,"r");
		}

		if ((flags & O_RDWR) == O_RDWR)
		{
			strcat(flagstr,"+");
		}

		strcat(flagstr,"b");
	}
}

bool Match(char *Name, char *Mask)
{
	while (*Name && *Mask)
	{
		if (*Mask == '*')
		{
			if (toupper(*Name) == toupper(Mask[1]))
			{
				Mask++;
			}
			else
			{
				Name++;
			}
		}
		else if (*Mask == '?' || toupper(*Mask) == toupper(*Name))
		{
			Mask++;
			Name++;
		}
		else
		{
			return false;
		}
	}

	while (*Mask && ((*Mask == '*') || (*Mask == '.'))) Mask++;

	return (*Name == 0 && *Mask == 0);
}

short DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int LeapYear(int year)
{
	if (year & 3)
	{
		return 0;
	}
	if ((year % 100 == 0) && !(year % 400 == 0))
	{
		return 0;
	}
	
	return 1;
}

// Disk Infomation
typedef struct
{
    uchar	bsJump[3];
    uchar	bsOemName[8];
    ushort	bsBytePerSec;
    uchar	bsSecPerCluster;
    ushort	bsResSectores;
    uchar	bsFAT;
    ushort	bsRootDirEnts;
    ushort	bsSectors;
    uchar	bsMedia;
    ushort	bsFATsecs;
    ushort	bsSecPerTrack;
    ushort	bsHeads;
    ulong	bsHiddenSecs;
    ulong	bsHugeSectoes;
    uchar	bsDriveNumber;
    uchar	bsReserved;
    uchar	bsBootSig;
    ulong	bsVolumeID; // serial number
    uchar	bsVolumeLabel[11];
    uchar	bsFileSysType[8];

} BOOTSECTOR;

typedef struct
{
    ulong	diStartSector;
    ushort	diSectors;
    char	*diBuffer;
} DISKIO;

/////////////////////////////////////////////////////////////////////////////////
bool LDirectory::ConvertToTime(char *Str, int SLen, uint64 Time) const
{
	if (Str)
	{
		FILETIME t;
		SYSTEMTIME s;
		t.dwHighDateTime = Time >> 32;
		t.dwLowDateTime = Time & 0xFFFFFFFF;
		if (FileTimeToSystemTime(&t, &s))
		{
			int Hour = (s.wHour < 1) ? s.wHour + 23 : s.wHour - 1;
			char AP = (Hour >= 12) ? 'a' : 'p';
			Hour %= 12;
			if (Hour == 0) Hour = 12;
			sprintf_s(Str, SLen, "%i:%02.2i:%02.2i %c", Hour-1, s.wMinute, s.wSecond, AP);
		}
	}
	return false;
}

bool LDirectory::ConvertToDate(char *Str, int SLen, uint64 Time) const
{
	if (Str)
	{
		FILETIME t;
		SYSTEMTIME s;
		t.dwHighDateTime = Time >> 32;
		t.dwLowDateTime = Time & 0xFFFFFFFF;
		if (FileTimeToSystemTime(&t, &s))
		{
			sprintf_s(Str, SLen, "%i/%i/%i", s.wDay, s.wMonth, s.wYear);
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Directory //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
struct LDirectoryPriv
{
	char			BasePath[DIR_PATH_SIZE];
	char			*BaseEnd;
	int				BaseRemaining;
	LString			Utf;
	HANDLE			Handle;
	WIN32_FIND_DATAW Data;

	LDirectoryPriv()
	{
		Handle = INVALID_HANDLE_VALUE;
		BasePath[0] = 0;
		BaseRemaining = 0;
	}

	~LDirectoryPriv()
	{
	}
};

LDirectory::LDirectory()
{
	d = new LDirectoryPriv;
}

LDirectory::~LDirectory()
{
	Close();
	DeleteObj(d);
}

LDirectory *LDirectory::Clone()
{
	return new LDirectory;
}

const char *LDirectory::FullPath()
{
	auto n = GetName();
	if (!n)
		return NULL;
	strcpy_s(d->BaseEnd, d->BaseRemaining, n);
	return d->BasePath;
}

bool LDirectory::Path(char *s, int BufLen) const
{
	auto e = d->BasePath + strlen(d->BasePath);
	auto sep = e > d->BasePath && e[-1] == DIR_CHAR ? "" : DIR_STR;
	auto ch = sprintf_s(s, BufLen, "%s%s%s", d->BasePath, sep, GetName());
	return ch > 0;
}

size_t Utf8To16Cpy(uint16_t *out, ssize_t outChar, uint8_t *in, ssize_t inChar = -1)
{
	auto start = out;
	int32 u32;
	outChar <<= 1; // words to bytes
	if (inChar < 0)
	{
		while (*in && outChar >= 4)
		{
			ssize_t len = 6;
			u32 = LgiUtf8To32(in, len);
			LgiUtf32To16(u32, out, outChar);
		}
	}
	else
	{
		while (outChar >= 4 && (u32 = LgiUtf8To32(in, inChar)))
		{
			LgiUtf32To16(u32, out, outChar);
		}
	}
	outChar >>= 1; // bytes to words
	if (outChar > 0)
		*out = 0;

	return out - start;
}

size_t Utf16To8Cpy(uint8_t *out, ssize_t outChar, const uint16_t *in, ssize_t inChar = -1)
{
	auto start = out;
	int32 u32;
	if (inChar < 0)
	{
		while (*in && outChar > 0)
		{
			ssize_t len = 4;
			u32 = LgiUtf16To32(in, len);
			LgiUtf32To8(u32, out, outChar);
		}
	}
	else
	{
		inChar <<= 1; // words to bytes
		while (outChar > 0 && (u32 = LgiUtf16To32(in, inChar)))
		{
			LgiUtf32To8(u32, out, outChar);
		}
		inChar >>= 1; // bytes to words
	}
	if (outChar > 0)
		*out = 0;

	return out - start;
}

template<typename O, typename I>
size_t UnicodeCpy(O *out, ssize_t outChar, I *in, ssize_t inChar = -1)
{
	if (out == NULL || in == NULL)
		return 0;
	if (sizeof(O) == 2 && sizeof(I) == 1)
		return Utf8To16Cpy((uint16_t*)out, outChar, (uint8_t*)in, inChar);
	else if (sizeof(O) == 1 && sizeof(I) == 2)
		return Utf16To8Cpy((uint8_t*)out, outChar, (uint16_t*)in, inChar);
	else
		LAssert(0);
	return 0;
}

int LDirectory::First(const char *InName, const char *Pattern)
{
	Close();

    d->Utf.Empty();

	if (!InName)
		return false;

	wchar_t wTmp[DIR_PATH_SIZE], wIn[DIR_PATH_SIZE];
	UnicodeCpy(wTmp, CountOf(wTmp), InName);
	auto Chars = GetFullPathNameW(wTmp, CountOf(wTmp), wIn, NULL);
	UnicodeCpy(d->BasePath, sizeof(d->BasePath), wIn, Chars);

	d->BaseEnd = d->BasePath + strlen(d->BasePath);
	if (d->BaseEnd > d->BasePath && d->BaseEnd[-1] != DIR_CHAR)
	{
		*d->BaseEnd++ = DIR_CHAR;
		*d->BaseEnd = 0;
	}
	ssize_t Used = d->BaseEnd - d->BasePath;
	d->BaseRemaining = (int) (sizeof(d->BasePath) - Used);

	char Str[DIR_PATH_SIZE];
	wchar_t *FindArg;
	if (Pattern)
	{
		if (!LMakePath(Str, sizeof(Str), d->BasePath, Pattern))
			return false;
		UnicodeCpy(FindArg = wTmp, CountOf(wTmp), Str);
	}
	else
	{
		FindArg = wIn;
	}

	d->Handle = FindFirstFileW(FindArg, &d->Data);
	if (d->Handle != INVALID_HANDLE_VALUE)
	{
		const char *n;
		while (	(n = GetName()) &&
				(stricmp(n, ".") == 0 || stricmp(n, "..") == 0))
		{
			if (!FindNextFileW(d->Handle, &d->Data))
				return false;
		    d->Utf.Empty();
		}
	}

	return d->Handle != INVALID_HANDLE_VALUE;
}

int LDirectory::Next()
{
	if (d->Handle == INVALID_HANDLE_VALUE)
		return false;

	const char *n;
	do
	{
		d->Utf.Empty();
		if (!FindNextFileW(d->Handle, &d->Data))
			return false;
		if (!(n = GetName()))
			return false;
	}
	while (stricmp(n, ".") == 0 || stricmp(n, "..") == 0);

	return true;
}

int LDirectory::Close()
{
	if (d->Handle != INVALID_HANDLE_VALUE)
	{
		FindClose(d->Handle);
		d->Handle = INVALID_HANDLE_VALUE;
	}

    d->Utf.Empty();

	return true;
}

int LDirectory::GetUser(bool Group) const
{
	return 0;
}

bool LDirectory::IsReadOnly() const
{
	return (d->Data.dwFileAttributes & FA_READONLY) != 0;
}

bool LDirectory::IsDir() const
{
	return (d->Data.dwFileAttributes & FA_DIRECTORY) != 0;
}

bool LDirectory::IsSymLink() const
{
	return (d->Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool LDirectory::IsHidden() const
{
	return (d->Data.dwFileAttributes & FA_HIDDEN) != 0;
}

long LDirectory::GetAttributes() const
{
	return d->Data.dwFileAttributes;
}

int LDirectory::GetType() const
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

const char *LDirectory::GetName() const
{
	if (!d->Utf)
	    d->Utf = d->Data.cFileName;
	return d->Utf;
}

LString LDirectory::FileName() const
{
	if (!d->Utf)
	    d->Utf = d->Data.cFileName;
	return d->Utf;
}

uint64 LDirectory::GetCreationTime() const
{
	return ((uint64) d->Data.ftCreationTime.dwHighDateTime) << 32 | d->Data.ftCreationTime.dwLowDateTime;
}

uint64 LDirectory::GetLastAccessTime() const
{
	return ((uint64) d->Data.ftLastAccessTime.dwHighDateTime) << 32 | d->Data.ftLastAccessTime.dwLowDateTime;
}

uint64 LDirectory::GetLastWriteTime() const
{
	return ((uint64) d->Data.ftLastWriteTime.dwHighDateTime) << 32 | d->Data.ftLastWriteTime.dwLowDateTime;
}

uint64 LDirectory::GetSize() const
{
	return ((uint64) d->Data.nFileSizeHigh) << 32 | d->Data.nFileSizeLow;
}

struct ClusterSizeMap
{
	ClusterSizeMap()
	{
		ZeroObj(Sizes);
	}

	uint64 GetDriveCluserSize(char Letter)
	{
		uint64 Cs = 4096;
		auto letter = ToLower(Letter) - 'a';

		Cs = Sizes[letter];
		if (!Cs)
		{
			DWORD SectorsPerCluster, BytesPerSector;
			const char Drive[] = { Letter , ':', '\\', 0 };
			BOOL b = GetDiskFreeSpaceA(Drive, &SectorsPerCluster, &BytesPerSector, NULL, NULL);
			if (b)
				Sizes[letter] = Cs = SectorsPerCluster * BytesPerSector;
			else
				LAssert(0);
		}

		return Cs;
	}

private:
	uint64 Sizes[26];
}	ClusterSizes;

int64 LDirectory::GetSizeOnDisk()
{
	auto Fp = FullPath();
	if (!Fp || !IsAlpha(Fp[0]))
		return -1;

	auto ClusterSize = ClusterSizes.GetDriveCluserSize(Fp[0]);
	DWORD HighSize = 0;
	DWORD LoSize = GetCompressedFileSizeA(FullPath(), &HighSize);
	*d->BaseEnd = 0;
	auto Size = ((uint64)HighSize << 32) | LoSize;
	
	return ((Size + ClusterSize - 1) / ClusterSize) * ClusterSize;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// File ///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class LFilePrivate
{
public:
	OsFile		hFile;
	LString		Name;
	int			Attributes;
	bool		Swap;
	bool		Status;
	DWORD		LastError;

	// Threading check stuff
	OsThreadId	CreateId, UseId;
	bool ThreadWarn;

	LFilePrivate()
	{
		hFile = INVALID_HANDLE;
		Attributes = 0;
		Swap = false;
		Status = false;
		LastError = 0;

		ThreadWarn = true;
		CreateId = LThread::InvalidId;
		UseId = LThread::InvalidId;
	}

	bool UseThreadCheck()
	{
		auto Cur = GetCurrentThreadId();
		if (UseId == LThread::InvalidId)
		{
			UseId = Cur;
		}
		else if (Cur != UseId)
		{
			if (ThreadWarn)
			{
				ThreadWarn = false;
				LgiTrace("%s:%i - Warning: multi-threaded file access (me=%x, user=%x).\n", _FL, Cur, UseId);
			}
			return false;
		}

		return true;
	}

	void OnError(const char *File, int Line, DWORD Code, const char *Ctx)
	{
		LgiTrace("%s:%i - LFile::%s(%s) Err=0x%x\n", File, Line, Ctx, Name.Get(), LastError = Code);
	}
};

LFile::LFile(const char *Path, int Mode)
{
	d = new LFilePrivate;

	if (Path)
		Open(Path, Mode);
}

LFile::~LFile()
{
	if (ValidHandle(d->hFile))
	{
		Close();
	}
	DeleteObj(d);
}

OsFile LFile::Handle()
{
	return d->hFile;
}

void LFile::ChangeThread()
{
	d->UseId = GetCurrentThreadId();
}

uint64_t LFile::GetModifiedTime()
{
    FILETIME create, access, write;
	if (!GetFileTime(Handle(), &create, &access, &write))
		return 0;

	return (((uint64_t)write.dwHighDateTime) << 32) | write.dwLowDateTime;
}

bool LFile::SetModifiedTime(uint64_t dt)
{
    FILETIME create, access, write;
	if (!GetFileTime(Handle(), &create, &access, &write))
		return false;

	write.dwHighDateTime = dt >> 32;
	write.dwLowDateTime = dt & 0xffffffff;

	auto result = SetFileTime(Handle(),
		&create,
		&access,
		&write);

	return result != 0;
}

const char *LFile::GetName()
{
	return d->Name;
}

void LFile::SetSwap(bool s)
{
	d->Swap = s;
}

bool LFile::GetSwap()
{
	return d->Swap;
}

int LFile::GetOpenMode()
{
	return d->Attributes;
}

int LFile::GetBlockSize()
{
	DWORD SectorsPerCluster;
	DWORD BytesPerSector = 0;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	
	#ifdef UNICODE
	LAutoWString n(Utf8ToWide(GetName()));
	#else
	LAutoString n(NewStr(GetName()));
	#endif
	if (n)
	{
		TCHAR *dir = n ? Strchr(n.Get(), '\\') : 0;
		if (dir) dir[1] = 0;
		GetDiskFreeSpace(	n,
							&SectorsPerCluster,
							&BytesPerSector,
							&NumberOfFreeClusters,
							&TotalNumberOfClusters);
	}

	return BytesPerSector;
}

int LFile::Open(const char *File, int Mode)
{
	int Status = false;
	bool NoCache = (Mode & O_NO_CACHE) != 0;
	Mode &= ~O_NO_CACHE;

	Close();

	if (File)
	{
		bool SharedAccess = (Mode & O_SHARE) != 0;
		Mode &= ~O_SHARE;

		if (File[0] == '/' && File[1] == '/')
		{
			// This hangs the process, so just bail safely.
			return false;
		}
		
		LAutoWString n(Utf8ToWide(File));
		if (n)
		{
			d->hFile = CreateFileW(	n,
									Mode,
									FILE_SHARE_READ | (SharedAccess ? FILE_SHARE_WRITE : 0),
									0,
									(Mode & O_WRITE) ? OPEN_ALWAYS : OPEN_EXISTING,
									NoCache ? FILE_FLAG_NO_BUFFERING : 0,
									NULL);
		}

		if (!ValidHandle(d->hFile))
		{
			// d->OnError(_FL, GetLastError(), "Open");
			switch (d->LastError = GetLastError())
			{
				case ERROR_FILE_NOT_FOUND:
				case ERROR_PATH_NOT_FOUND:
				{
					// Path/File not found;
					int i=0;
					break;
				}
				case ERROR_ACCESS_DENIED:
				{
					// Access is denied:
					// Read-Only or another process has the file open
					int i=0;
					break;
				}
			}
		}
		else
		{
			d->Attributes = Mode;
			d->Name = File;

			Status = true;
			d->Status = true;
			d->CreateId = GetCurrentThreadId();
		}
	}

	return Status;
}

bool LFile::IsOpen()
{
	return ValidHandle(d->hFile);
}

int LFile::GetError()
{
	return d->LastError;
}

int LFile::Close()
{
	int Status = false;

	d->UseThreadCheck();
	if (ValidHandle(d->hFile))
	{
		::CloseHandle(d->hFile);
		d->hFile = INVALID_HANDLE;
	}

	d->Name.Empty();
	return Status;
}

int64 LFile::GetSize()
{
	LAssert(IsOpen());
	d->UseThreadCheck();

	DWORD High = -1;
	DWORD Low = GetFileSize(d->hFile, &High);
	return Low | ((int64)High<<32);
}

int64 LFile::SetSize(int64 Size)
{
	LAssert(IsOpen());
	d->UseThreadCheck();

	LONG OldPosHigh = 0;
	DWORD OldPosLow = SetFilePointer(d->hFile, 0, &OldPosHigh, SEEK_CUR);
	
	LONG SizeHigh = Size >> 32;
	DWORD r = SetFilePointer(d->hFile, (LONG)Size, &SizeHigh, FILE_BEGIN);
	
	BOOL b = SetEndOfFile(d->hFile);
	if (!b)
	{
		DWORD err = GetLastError();
		LgiTrace("%s:%i - SetSize(" LPrintfInt64 ") failed: 0x%x\n", _FL, Size, err);
	}
	
	SetFilePointer(d->hFile, OldPosLow, &OldPosHigh, FILE_BEGIN);

	return GetSize();
}

int64 LFile::GetPos()
{
	LAssert(IsOpen());
	d->UseThreadCheck();

	LONG PosHigh = 0;
	DWORD PosLow = SetFilePointer(d->hFile, 0, &PosHigh, FILE_CURRENT);
	return PosLow | ((int64)PosHigh<<32);
}

int64 LFile::SetPos(int64 Pos)
{
	LAssert(IsOpen());
	d->UseThreadCheck();

	LONG PosHigh = Pos >> 32;
	DWORD PosLow = SetFilePointer(d->hFile, (LONG)Pos, &PosHigh, FILE_BEGIN);
	return PosLow | ((int64)PosHigh<<32);
}

ssize_t LFile::Read(void *Buffer, ssize_t Size, int Flags)
{
	ssize_t Rd = 0;
	d->UseThreadCheck();

	// This loop allows ReadFile (32bit) to read more than 4GB in one go. If need be.
	for (ssize_t Pos = 0; Pos < Size; )
	{
		DWORD Bytes = 0;
		int BlockSz = (int) MIN( Size - Pos, 1 << 30 ); // 1 GiB blocks

		if (ReadFile(d->hFile, (char*)Buffer + Pos, BlockSz, &Bytes, NULL) &&
			Bytes > 0)
		{
			Rd += Bytes;
			Pos += Bytes;
			d->Status &= Bytes > 0;
		}
		else
		{
			if (!Eof())
				d->OnError(_FL, GetLastError(), "Read");
			break;
		}
	}

	return Rd;
}

ssize_t LFile::Write(const void *Buffer, ssize_t Size, int Flags)
{
	ssize_t Wr = 0;
	d->UseThreadCheck();

	// This loop allows WriteFile (32bit) to read more than 4GB in one go. If need be.
	for (ssize_t Pos = 0; Pos < Size; )
	{
		DWORD Bytes = 0;
		int BlockSz = (int) MIN( Size - Pos, 1 << 30 ); // 1 GiB blocks

		if (WriteFile(d->hFile, (const char*)Buffer + Pos, BlockSz, &Bytes, NULL))
		{
			if (Bytes != BlockSz)
			{
				LAssert(!"Is this ok?");
			}

			Wr += Bytes;
			Pos += Bytes;
			d->Status &= Bytes > 0;
		}
		else
		{
			d->OnError(_FL, GetLastError(), "Write");
			break;
		}
	}

	return Wr;
}

int64 LFile::Seek(int64 To, int Whence)
{
	LAssert(IsOpen());
	d->UseThreadCheck();

	int Mode;
	switch (Whence)
	{
		default:
		case SEEK_SET:
			Mode = FILE_BEGIN;
			break;
		case SEEK_CUR:
			Mode = FILE_CURRENT;
			break;
		case SEEK_END:
			Mode = FILE_END;
			break;
	}

	LONG ToHigh = To >> 32;
	DWORD ToLow = SetFilePointer(d->hFile, (LONG)To, &ToHigh, Mode);
	return ToLow | ((int64)ToHigh<<32);
}

bool LFile::Eof()
{
	LAssert(IsOpen());
	return GetPos() >= GetSize();
}

ssize_t LFile::SwapRead(uchar *Buf, ssize_t Size)
{
	DWORD r = 0;
	d->UseThreadCheck();

	if (!ReadFile(d->hFile, Buf, (DWORD)Size, &r, NULL) || r != Size)
	{
		d->OnError(_FL, GetLastError(), "SwapRead");
		return 0;
	}

	// Swap the bytes
	uchar *s = Buf, *e = Buf + Size - 1;
	while (s < e)
	{
		uchar t = *s;
		*s++ = *e;
		*e-- = t;
	}		
	return r;
}

ssize_t LFile::SwapWrite(uchar *Buf, ssize_t Size)
{
	int i = 0;
	DWORD w;
	Buf = &Buf[Size-1];
	d->UseThreadCheck();

	while (Size--)
	{
		if (!(d->Status &= WriteFile(d->hFile, Buf--, 1, &w, NULL) != 0 && w == 1))
			break;
		i += w;
	}

	return i;
}

ssize_t LFile::ReadStr(char *Buf, ssize_t Size)
{
	int i = 0;
	DWORD r;
	d->UseThreadCheck();

	if (Buf && Size > 0)
	{
		char c;

		Size--;

		do
		{
			ReadFile(d->hFile, &c, 1, &r, NULL);
			if (Eof())
			{
				break;
			}
			
			*Buf++ = c;
			i++;
		
		} while (i < Size - 1 && c != '\n');

		*Buf = 0;
	}

	return i;
}

ssize_t LFile::WriteStr(char *Buf, ssize_t Size)
{
	int i = 0;
	DWORD w;
	d->UseThreadCheck();

	while (i <= Size)
	{
		WriteFile(d->hFile, Buf, 1, &w, NULL);
		Buf++;
		i++;
		if (*Buf == '\n') break;
	}

	return i;
}

void LFile::SetStatus(bool s)
{
	d->Status = s;
}

bool LFile::GetStatus()
{
	return d->Status;
}

#define GFileOp(type)	LFile &LFile::operator >> (type &i) \
{ \
	if (d->Swap) SwapRead((uchar*) &i, sizeof(i)); \
	else Read(&i, sizeof(i)); \
	return *this; \
}
GFileOps(); 
#undef GFileOp

#define GFileOp(type)	LFile &LFile::operator << (type i) \
{ \
	if (d->Swap) SwapWrite((uchar*) &i, sizeof(i)); \
	else Write(&i, sizeof(i)); \
	return *this; \
}

GFileOps(); 
#undef GFileOp

