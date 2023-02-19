/*hdr
**	FILE:			File.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			8/10/2000
**	DESCRIPTION:	The file subsystem
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes **********************************/
#include <pwd.h>

#include "Path.h"
#include "VolumeRoster.h"
#include "Directory.h"

#include "lgi/common/LgiDefs.h"
#include "lgi/common/File.h"
#include "lgi/common/Containers.h"
#include "lgi/common/Gdc2.h"
#include "lgi/common/LgiCommon.h"
#include "lgi/common/DateTime.h"

/****************************** Helper Functions **************************/
LString BGetFullPath(BEntry &entry)
{
	BPath path;
	auto r = entry.GetPath(&path);
	if (r != B_OK)
		return LString();
	return path.Path();
}

LString BGetFullPath(BDirectory &dir)
{
	BEntry entry;
	auto r = dir.GetEntry(&entry);
	if (r != B_OK)
		return LString();
	return BGetFullPath(entry);
}					

LString BGetFullPath(BVolume &volume)
{				
	BDirectory directory;
	auto r = volume.GetRootDirectory(&directory);
	if (r != B_OK)
		return LString();
	return BGetFullPath(directory);
}					

char *LReadTextFile(const char *File)
{
	char *s = 0;
	LFile f;
	if (File && f.Open(File, O_READ))
	{
		int Len = f.GetSize();
		s = new char[Len+1];
		if (s)
		{
			int Read = f.Read(s, Len);
			s[Read] = 0;
		}
	}
	return s;
}

int64 LFileSize(const char *FileName)
{
	BEntry e(FileName);
	if (e.InitCheck() != B_OK)
		return 0;
		
	off_t size = 0;
	if (e.GetSize(&size) != B_OK)
		return 0;
		
	return size;
}

bool LDirExists(const char *FileName, char *CorrectCase)
{
	if (!FileName)
		return false;

	struct stat s;
	auto r = lstat(FileName, &s);
	if (r == 0)
	{
		return	S_ISDIR(s.st_mode) ||
				S_ISLNK(s.st_mode);
	}
	else
	{
		r = stat(FileName, &s);
		if (r == 0)
		{
			return	S_ISDIR(s.st_mode) ||
					S_ISLNK(s.st_mode);
		}
	}

	return false;
}

bool LFileExists(const char *FileName, char *CorrectCase)
{
	bool Status = false;
	
	if (FileName)
	{
		struct stat s;
		
		// Check for exact match...
		if (stat(FileName, &s) == 0)
		{
			Status = !S_ISDIR(s.st_mode);
		}
		else if (CorrectCase)
		{
			// Look for altenate case by enumerating the directory
			char d[256];
			strcpy(d, FileName);
			char *e = strrchr(d, DIR_CHAR);
			if (e)
			{
				*e++ = 0;
				
				DIR *Dir = opendir(d);
				if (Dir)
				{
					dirent *De;
					while ((De = readdir(Dir)))
					{
						if (stricmp(De->d_name, e) == 0)
						{
							try
							{
								// Tell the calling program the actual case of the file...
								e = (char*) strrchr(FileName, DIR_CHAR);
								
								// If this crashes because the argument is read only then we get caught by the try catch
								strcpy(e+1, De->d_name);
								
								// It worked!
								Status = true;
							}
							catch (...)
							{
								// It didn't work :(
								#ifdef _DEBUG
								printf("%s,%i - LFileExists(%s) found an alternate case version but couldn't return it to the caller.\n",
										__FILE__, __LINE__, FileName);
								#endif
							}
							
							break;
						}
					}
					
					closedir(Dir);
				}
			}
		}
	}

	return Status;
}

bool LResolveShortcut(const char *LinkFile, char *Path, ssize_t Len)
{
	if (!LinkFile || !Path || Len < 1)
		return false;

	BEntry e(LinkFile);
	auto r = e.InitCheck();
	if (r != B_OK)
	{
		printf("%s:%i - LResolveShortcut: %i\n", _FL, r);
		return false;
	}
	
	if (!e.IsSymLink())
	{
		return false;
	}
	
	r = e.SetTo(LinkFile, true);
	if (r != B_OK)
	{
		printf("%s:%i - LResolveShortcut: %i\n", _FL, r);
		return false;
	}
	
	auto p = BGetFullPath(e);
	strcpy_s(Path, Len, p);
	return true;
}

void WriteStr(LFile &f, const char *s)
{
	uint32_t Len = (s) ? strlen(s) : 0;
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
		#if defined(_DEBUG) && defined MEMORY_DEBUG
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
	return sizeof(ulong) + ((s) ? strlen(s) : 0);
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
	
	if (Path)
	{
		struct stat s;
		if (lstat(Path, &s) == 0)
		{
			// printf("LGetDriveInfo dev=%i\n", s.st_dev);
		}
	}
	
	return Status;
}

/////////////////////////////////////////////////////////////////////////
struct LVolumePriv
{
	LVolume *Owner = NULL;
	int64 Size = 0, Free = 0;
	int Type = VT_NONE, Flags = 0;
	LSystemPath SysPath = LSP_ROOT;
	LString Name, Path;
	LVolume *NextVol = NULL, *ChildVol = NULL;

	LVolumePriv(LVolume *owner, const char *path) : Owner(owner)
	{
		Path = path;
		Name = LGetLeaf(path);
		Type = VT_FOLDER;
	}

	LVolumePriv(LVolume *owner, LSystemPath sys, const char *name) : Owner(owner)
	{
		SysPath = sys;

		Name = name;
		if (SysPath == LSP_ROOT)
			Path = "/";
		else
			Path = LGetSystemPath(SysPath);
		
		if (Path)
			Type = sys == LSP_DESKTOP ? VT_DESKTOP : VT_FOLDER;
	}
	
	~LVolumePriv()
	{
		DeleteObj(NextVol);
		DeleteObj(ChildVol);
	}

	void Insert(LVolume *vol, LVolume *newVol)
	{
		if (!vol || !newVol)
			return;
			
		if (vol->d->ChildVol)
		{
			for (auto v = vol->d->ChildVol; v; v = v->d->NextVol)
			{
				if (!v->d->NextVol)
				{
					LAssert(newVol != v->d->Owner);
					v->d->NextVol = newVol;
					// printf("Insert %p:%s into %p:%s\n", newVol, newVol->Name(), vol, vol->Name());
					break;
				}
			}
		}
		else
		{
			LAssert(newVol != vol->d->Owner);
			vol->d->ChildVol = newVol;
			// printf("Insert %p:%s into %p:%s\n", newVol, newVol->Name(), vol, vol->Name());
		}
	}	

	LVolume *First()
	{
		if (SysPath == LSP_DESKTOP && !ChildVol)
		{
			// Get various shortcuts to points of interest
			Insert(Owner, new LVolume(LSP_ROOT, "Root"));
			
			struct passwd *pw = getpwuid(getuid());
			if (pw)
				Insert(Owner, new LVolume(LSP_HOME, "Home"));

			LSystemPath p[] = {	LSP_USER_DOCUMENTS,
								LSP_USER_MUSIC,
								LSP_USER_VIDEO,
								LSP_USER_DOWNLOADS,
								LSP_USER_PICTURES };
			for (int i=0; i<CountOf(p); i++)
			{
				LString Path = LGetSystemPath(p[i]);
				if (Path && LDirExists(Path))
				{
					auto v = new LVolume(0);
					if (Path && v)
					{
						auto Parts = Path.Split("/");
						v->d->Path = Path;
						v->d->Name = *Parts.rbegin();
						v->d->Type = VT_FOLDER;
						Insert(Owner, v);
					}
				}
			}
		}

		return ChildVol;
	}

	LVolume *Next()
	{
		if (SysPath == LSP_DESKTOP && !NextVol)
		{
			NextVol = new LVolume(LSP_MOUNT_POINT, "Mounts");

			LHashTbl<StrKey<char,false>, bool> Map;
			
			BVolumeRoster roster;
			BVolume volume;
			for (auto r = roster.GetBootVolume(&volume);
					  r == B_OK;
					  r = roster.GetNextVolume(&volume))
			{
				auto Path = BGetFullPath(volume);
				if (!Path)
					continue;
				
				if (Stricmp(Path.Get(), "/") == 0 ||
					Stricmp(Path.Get(), "/dev") == 0 ||
					Stricmp(Path.Get(), "/boot/system/var/shared_memory") == 0)
					continue;
				
				if (volume.Capacity() == (4 << 10))
					continue;

				if (!volume.IsPersistent())
					continue;
				
				char Name[B_FILE_NAME_LENGTH];
				if (volume.GetName(Name) != B_OK)
					continue;

				auto Done = Map.Find(Name);
				if (Done)
					continue;
				Map.Add(Name, true);

				auto v = new LVolume(0);
				if (v)
				{
					v->d->Name = Name;
					v->d->Path = Path;
					v->d->Type = VT_HARDDISK;
					v->d->Size = volume.Capacity();
					v->d->Free = volume.FreeBytes();
					
					// printf("%s, %s\n", Name, v->d->Path.Get());

					Insert(NextVol, v);
				}
			}
		}
		
		return NextVol;
	}
};

LVolume::LVolume(const char *Path = NULL)
{
	d = new LVolumePriv(this, Path);
}

LVolume::LVolume(LSystemPath SysPath, const char *Name)
{
	d = new LVolumePriv(this, SysPath, Name);
}

LVolume::~LVolume()
{
	DeleteObj(d);
}

const char *LVolume::Name() const
{
    return d->Name;
}

const char *LVolume::Path() const
{
    return d->Path;
}

int LVolume::Type() const
{
    return d->Type;
}

int LVolume::Flags() const
{
    return d->Flags;
}

uint64 LVolume::Size() const
{
    return d->Size;
}

uint64 LVolume::Free() const
{
    return d->Free;
}

LSurface *LVolume::Icon() const
{
    return NULL;
}

bool LVolume::IsMounted() const
{
    return true;
}

bool LVolume::SetMounted(bool Mount)
{
    return Mount;
}

LVolume *LVolume::First()
{
    return d->First();
}

LVolume *LVolume::Next()
{
    return d->Next();
}

void LVolume::Insert(LAutoPtr<LVolume> v)
{
    d->Insert(this, v.Release());
}

LDirectory *LVolume::GetContents()
{
	LDirectory *Dir = 0;
	if (d->Path)
	{
		Dir = new LDirectory;
		if (Dir && !Dir->First(d->Path))
			DeleteObj(Dir);
	}
	return Dir;
}

///////////////////////////////////////////////////////////////////////////////
LFileSystem *LFileSystem::Instance = 0;

LFileSystem::LFileSystem()
{
	Instance = this;
	Root = 0;
}

LFileSystem::~LFileSystem()
{
	DeleteObj(Root);
}

void LFileSystem::OnDeviceChange(char *Reserved)
{
}

LVolume *LFileSystem::GetRootVolume()
{
	if (!Root)
		Root = new LVolume(LSP_DESKTOP, "Desktop");

	return Root;
}

bool LFileSystem::Copy(const char *From, const char *To, LError *Status, CopyFileCallback Callback, void *Token)
{
	LArray<char> Buf;

	if (Status)
		*Status = 0;

	if (Buf.Length(2 << 20))
	{
		LFile In, Out;
		if (!In.Open(From, O_READ))
		{
			if (Status)
				*Status = In.GetError();
			return false;
		}
		
		if (!Out.Open(To, O_WRITE))
		{
			if (Status)
				*Status = Out.GetError();
			return false;
		}

		int64 Size = In.GetSize(), Done = 0;
		for (int64 i=0; i<Size; i++)
		{
			int Copy = MIN(Size - i, Buf.Length());
			int r = In.Read(&Buf[0], Copy);
			if (r <= 0)
				break;
			
			while (r > 0)
			{
				int w = Out.Write(&Buf[0], r);
				if (w <= 0)
					break;
				
				r -= w;
				Done += w;
				if (Callback)
					Callback(Token, Done, Size);
			}
			
			if (r > 0)
				break;
		}
		
		return Done == Size;
	}
	
	return false;
}

bool LFileSystem::Delete(LArray<const char*> &Files, LArray<LError> *Status, bool ToTrash)
{
	bool Error = false;

	if (ToTrash)
	{
		char p[MAX_PATH_LEN];
		if (LGetSystemPath(LSP_TRASH, p, sizeof(p)))
		{
			for (int i=0; i<Files.Length(); i++)
			{
				const char *f = strrchr(Files[i], DIR_CHAR);
				LMakePath(p, sizeof(p), p, f?f+1:Files[i]);
				if (!Move(Files[i], p))
				{
					if (Status)
						(*Status)[i] = errno;
					
					LgiTrace("%s:%i - MoveFile(%s,%s) failed.\n", _FL, Files[i], p);
					Error = true;
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - LgiGetSystemPath(LSP_TRASH) failed.\n", _FL);
		}
	}
	else
	{
		for (int i=0; i<Files.Length(); i++)
		{
			if (unlink(Files[i]))
			{
				if (Status)
					(*Status)[i] = errno;
				
				Error = true;
			}			
		}
	}
	
	return !Error;
}

bool LFileSystem::Delete(const char *FileName, bool ToTrash)
{
	if (FileName)
	{
		LArray<const char*> f;
		f.Add(FileName);
		return Delete(f, 0, ToTrash);
	}
	return false;
}

bool LFileSystem::CreateFolder(const char *PathName, bool CreateParentTree, LError *ErrorCode)
{
	int r = mkdir(PathName, S_IRWXU | S_IXGRP | S_IXOTH);
	if (r)
	{
		if (ErrorCode)
			*ErrorCode = errno;

		if (CreateParentTree)
		{
			LFile::Path p(PathName);
			LString dir = DIR_STR;
			for (size_t i=1; i<=p.Length(); i++)
			{
				LFile::Path Par(dir + dir.Join(p.Slice(0, i)));
				if (!Par.Exists())
				{
					const char *ParDir = Par;
					r = mkdir(ParDir, S_IRWXU | S_IXGRP | S_IXOTH);
					if (r)
					{
						if (ErrorCode)
							*ErrorCode = errno;
						break;
					}
					LgiTrace("%s:%i - Created '%s'\n", _FL, Par.GetFull().Get());
				}
			}
			if (p.Exists())
				return true;
		}
	
		LgiTrace("%s:%i - mkdir('%s') failed with %i, errno=%i\n", _FL, PathName, r, errno);
	}
	
	return r == 0;
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
				char Str[MAX_PATH_LEN];
				Dir.Path(Str, sizeof(Str));

				if (Dir.IsDir())
					RemoveFolder(Str, Recurse);
				else
					Delete(Str, false);
			}
			while (Dir.Next());
		}
	}

	return rmdir(PathName) == 0;
}

bool LFileSystem::Move(const char *OldName, const char *NewName, LError *Err)
{
	if (rename(OldName, NewName))
	{
		printf("%s:%i - rename failed, error: %s(%i)\n",
			_FL, LErrorCodeToString(errno), errno);
		return false;
	}
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
struct LDirectoryPriv
{
	char			BasePath[MAX_PATH_LEN];
	char			*BaseEnd = NULL;
	DIR				*Dir = NULL;
	struct dirent	*De = NULL;
	struct stat		Stat;
	LString			Pattern;

	LDirectoryPriv()
	{	
		BasePath[0] = 0;
	}
	
	bool Ignore()
	{
		return	De
				&&
				(
					strcmp(De->d_name, ".") == 0
					||
					strcmp(De->d_name, "..") == 0
					||
					(
						Pattern
						&&
						!MatchStr(Pattern, De->d_name)
					)
				);
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

int LDirectory::First(const char *Name, const char *Pattern)
{
	Close();

	if (!Name)
		return 0;

	strcpy_s(d->BasePath, sizeof(d->BasePath), Name);
	d->BaseEnd = NULL;
	
	if (!Pattern || stricmp(Pattern, LGI_ALL_FILES) == 0)
	{
		struct stat S;
		if (lstat(Name, &S) == 0)
		{
			if (S_ISREG(S.st_mode))
			{
				char *Dir = strrchr(d->BasePath, DIR_CHAR);
				if (Dir)
				{
					*Dir++ = 0;
					d->Pattern = Dir;
				}
			}
		}
	}
	else
	{
		d->Pattern = Pattern;
	}
	
	d->Dir = opendir(d->BasePath);
	if (d->Dir)
	{
		d->De = readdir(d->Dir);
		if (d->De)
		{
			char s[MaxPathLen];
			LMakePath(s, sizeof(s), d->BasePath, GetName());
			lstat(s, &d->Stat);

			if (d->Ignore() && !Next())
				return false;
		}
	}

	return d->Dir != NULL && d->De != NULL;
}

int LDirectory::Next()
{
	int Status = false;

	while (d->Dir && d->De)
	{
		if ((d->De = readdir(d->Dir)))
		{
			char s[MaxPathLen];
			LMakePath(s, sizeof(s), d->BasePath, GetName());			
			lstat(s, &d->Stat);

			if (!d->Ignore())
			{
				Status = true;
				break;
			}
		}
	}

	return Status;
}

int LDirectory::Close()
{
	if (d->Dir)
	{
		closedir(d->Dir);
		d->Dir = NULL;
	}
	d->De = NULL;
	d->BaseEnd = NULL;
	d->BasePath[0] = 0;

	return true;
}

const char *LDirectory::FullPath()
{
	auto nm = GetName();
	if (!nm)
		return NULL;

	if (!d->BaseEnd)
	{
		d->BaseEnd = d->BasePath + strlen(d->BasePath);
		if (d->BaseEnd > d->BasePath &&
			d->BaseEnd[-1] != DIR_CHAR)
		{
			*d->BaseEnd++ = DIR_CHAR;
		}
	}
	
	auto used = d->BaseEnd - d->BasePath;
	auto remaining = sizeof(d->BasePath) - used;

	strcpy_s(d->BaseEnd, remaining, nm);
	return d->BasePath;
}

LString LDirectory::FileName() const
{
	return GetName();
}

bool LDirectory::Path(char *s, int BufLen) const
{
	if (!s)
	{
		return false;
	}

	return LMakePath(s, BufLen, d->BasePath, GetName());
}

int LDirectory::GetType() const
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

int LDirectory::GetUser(bool Group) const
{
	if (Group)
	{
		return d->Stat.st_gid;
	}
	else
	{
		return d->Stat.st_uid;
	}
}

bool LDirectory::IsReadOnly() const
{
	if (getuid() == d->Stat.st_uid)
	{
		// Check user perms
		return !TestFlag(GetAttributes(), S_IWUSR);
	}
	else if (getgid() == d->Stat.st_gid)
	{
		// Check group perms
		return !TestFlag(GetAttributes(), S_IWGRP);
	}
	
	// Check global perms
	return !TestFlag(GetAttributes(), S_IWOTH);
}

bool LDirectory::IsHidden() const
{
	return GetName() && GetName()[0] == '.';
}

bool LDirectory::IsDir() const
{
	int a = GetAttributes();
	return !S_ISLNK(a) && S_ISDIR(a);
}

bool LDirectory::IsSymLink() const
{
	int a = GetAttributes();
	return S_ISLNK(a);
}

long LDirectory::GetAttributes() const
{
	return d->Stat.st_mode;
}

char *LDirectory::GetName() const
{
	return (d->De) ? d->De->d_name : NULL;
}

uint64 LDirectory::GetCreationTime() const
{
	return (uint64) d->Stat.st_ctime * LDateTime::Second64Bit;
}

uint64 LDirectory::GetLastAccessTime() const
{
	return (uint64) d->Stat.st_atime * LDateTime::Second64Bit;
}

uint64 LDirectory::GetLastWriteTime() const
{
	return (uint64) d->Stat.st_mtime * LDateTime::Second64Bit;
}

uint64 LDirectory::GetSize() const
{
	return d->Stat.st_size;
}

int64 LDirectory::GetSizeOnDisk()
{
	return d->Stat.st_size;
}

bool LDirectory::ConvertToTime(char *Str, int SLen, uint64 Time) const
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, SLen, "%I:%M:%S", t);
		return true;
	}
	return false;
}

bool LDirectory::ConvertToDate(char *Str, int SLen, uint64 Time) const
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, SLen, "%d/%m/%y", t);
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
class LFilePrivate
{
public:
	int hFile;
	char *Name;
	bool Swap;
	int Status;
	int Attributes;
	int ErrorCode;
	
	LFilePrivate()
	{
		hFile = INVALID_HANDLE;
		Name = 0;
		Swap = false;
		Status = true;
		Attributes = 0;
		ErrorCode = 0;
	}
	
	~LFilePrivate()
	{
		DeleteArray(Name);
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
	if (d && ValidHandle(d->hFile))
	{
		Close();
	}
	DeleteObj(d);
}

OsFile LFile::Handle()
{
	return d->hFile;
}

int LFile::GetError()
{
	return d->ErrorCode;
}

bool LFile::IsOpen()
{
	return ValidHandle(d->hFile);
}

#define DEBUG_OPEN_FILES 	0

#if DEBUG_OPEN_FILES
LMutex Lck;
LArray<LFile*> OpenFiles;
#endif

int LFile::Open(const char *File, int Mode)
{
	int Status = false;

	if (File)
	{
		if (TestFlag(Mode, O_WRITE) ||
			TestFlag(Mode, O_READWRITE))
		{
			Mode |= O_CREAT;
		}

		Close();
		d->hFile = open(File, Mode
						#ifdef O_LARGEFILE
		 				| O_LARGEFILE
		 				#endif
		 				, S_IRUSR | S_IWUSR);
		if (ValidHandle(d->hFile))
		{
			d->Attributes = Mode;
			d->Name = new char[strlen(File)+1];
			if (d->Name)
			{
				strcpy(d->Name, File);
			}
			Status = true;
			d->Status = true;

			#if DEBUG_OPEN_FILES			
			if (Lck.Lock(_FL))
			{
				if (!OpenFiles.HasItem(this))
					OpenFiles.Add(this);
				Lck.Unlock();
			}
			#endif
		}
		else
		{
			d->ErrorCode = errno;

			#if DEBUG_OPEN_FILES
			if (Lck.Lock(_FL))
			{
				for (unsigned i=0; i<OpenFiles.Length(); i++)
					printf("%i: %s\n", i, OpenFiles[i]->GetName());
				Lck.Unlock();
			}
			#endif

			printf("LFile::Open failed\n\topen(%s,%08.8x) = %i\n\terrno=%s\n",
				File, 
				Mode, 
				d->hFile,
				LErrorCodeToString(d->ErrorCode));
		}
	}

	return Status;
}

int LFile::Close()
{
	if (ValidHandle(d->hFile))
	{
		close(d->hFile);
		d->hFile = INVALID_HANDLE;
		DeleteArray(d->Name);

		#if DEBUG_OPEN_FILES
		if (Lck.Lock(_FL))
		{
			OpenFiles.Delete(this);
			Lck.Unlock();
		}
		#endif
	}

	return true;
}

uint64_t LFile::GetModifiedTime()
{
	struct stat s;
    stat(d->Name, &s);
    return s.st_mtime;
}

bool LFile::SetModifiedTime(uint64_t dt)
{
	struct stat s;
    stat(d->Name, &s);
    
    struct timeval times[2] = {};
    times[0].tv_sec = s.st_atime;
    times[1].tv_sec = dt;
    
	int r = utimes(d->Name, times);
	
	return r == 0;
}

void LFile::ChangeThread()
{
}

#define CHUNK		0xFFF0

ssize_t LFile::Read(void *Buffer, ssize_t Size, int Flags)
{
	int Red = 0;

	if (Buffer && Size > 0)
	{
		Red = read(d->hFile, Buffer, Size);
	}
	d->Status = Red == Size;

	return MAX(Red, 0);
}

ssize_t LFile::Write(const void *Buffer, ssize_t Size, int Flags)
{
	int Written = 0;

	if (Buffer && Size > 0)
	{
		Written = write(d->hFile, Buffer, Size);
	}
	d->Status = Written == Size;

	return MAX(Written, 0);
}

#ifdef LINUX
#define LINUX64 	1
#endif

int64 LFile::Seek(int64 To, int Whence)
{
	#if LINUX64
	return lseek64(d->hFile, To, Whence); // If this doesn't compile, switch off LINUX64
	#else
	return lseek(d->hFile, To, Whence);
	#endif
}

int64 LFile::SetPos(int64 Pos)
{
	#if LINUX64
	int64 p = lseek64(d->hFile, Pos, SEEK_SET);
	if (p < 0)
	{
		int e = errno;
		printf("%s:%i - lseek64(%Lx) failed (error %i: %s).\n",
			__FILE__, __LINE__,
			Pos, e, GetErrorName(e));
		
	}
	#else
	return lseek(d->hFile, Pos, SEEK_SET);
	#endif
}

int64 LFile::GetPos()
{
	#if LINUX64
	int64 p = lseek64(d->hFile, 0, SEEK_CUR);
	if (p < 0)
	{
		int e = errno;
		printf("%s:%i - lseek64 failed (error %i: %s).\n",
			__FILE__, __LINE__,
			e, GetErrorName(e));
		
	}
	return p;
	#else
	return lseek(d->hFile, 0, SEEK_CUR);
	#endif	
}

int64 LFile::GetSize()
{
	int64 Here = GetPos();
	#if LINUX64
	int64 Ret = lseek64(d->hFile, 0, SEEK_END);
	#else
	int64 Ret = lseek(d->hFile, 0, SEEK_END);
	#endif
	SetPos(Here);
	return Ret;
}

int64 LFile::SetSize(int64 Size)
{
	if (ValidHandle(d->hFile))
	{
		int64 Pos = GetPos();
		
		#if LINUX64
		ftruncate64(d->hFile, Size);
		#else
		ftruncate(d->hFile, Size);
		#endif
		
		if (d->hFile)
		{
			SetPos(Pos);
		}
	}

	return GetSize();
}

bool LFile::Eof()
{
	return GetPos() >= GetSize();
}

ssize_t LFile::SwapRead(uchar *Buf, ssize_t Size)
{
	ssize_t i = 0;
	ssize_t r = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		r = read(d->hFile, Buf--, 1);
		i += r;
	}
	return i;
}

ssize_t LFile::SwapWrite(uchar *Buf, ssize_t Size)
{
	ssize_t i = 0;
	ssize_t w = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		w = write(d->hFile, Buf--, 1);
		i += w;
	}
	return i;
}

ssize_t LFile::ReadStr(char *Buf, ssize_t Size)
{
	ssize_t i = 0;
	ssize_t r = 0;
	if (Buf && Size > 0)
	{
		char c;

		Size--;

		do
		{
			r = read(d->hFile, &c, 1);
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
	ssize_t i = 0;
	ssize_t w;

	while (i <= Size)
	{
		w = write(d->hFile, Buf, 1);
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

const char *LFile::GetName()
{
	return d->Name;
}

#define GFileOp(type)	LFile &LFile::operator >> (type &i) { d->Status |= ((d->Swap) ? SwapRead((uchar*) &i, sizeof(i)) : Read(&i, sizeof(i))) != sizeof(i); return *this; }
GFileOps();
#undef GFileOp

#define GFileOp(type)	LFile &LFile::operator << (type i) { d->Status |= ((d->Swap) ? SwapWrite((uchar*) &i, sizeof(i)) : Write(&i, sizeof(i))) != sizeof(i); return *this; }
GFileOps();
#undef GFileOp

//////////////////////////////////////////////////////////////////////////////
LString LFile::Path::Sep(DIR_STR);

bool LFile::Path::FixCase()
{
	LString Prev;
	bool Changed = false;
	
	// printf("FixCase '%s'\n", GetFull().Get());
	for (size_t i=0; i<Length(); i++)
	{
		auto &Part = (*this)[i];
		auto Next = (Prev ? Prev : "") + Sep + Part;
		
		struct stat Info;
		auto r = lstat(Next, &Info);
		// printf("[%i] %s=%i\n", (int)i, Next.Get(), r);
		if (r)
		{
			LDirectory Dir;
			for (auto b=Dir.First(Prev); b; b=Dir.Next())
			{
				auto Name = Dir.GetName();
				if (!stricmp(Name, Part) && strcmp(Name, Part))
				{
					// Found alt case part..?
					// printf("Altcase: %s -> %s\n", part.Get(), name);
					Part = Name;
					Next = (Prev ? Prev : "") + Sep + Part;
					Changed = true;
				}
			}
		}
		
		Prev = Next;
	}
	
	return Changed;	
}
