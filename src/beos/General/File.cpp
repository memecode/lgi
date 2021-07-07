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
#include <SymLink.h>
#include <Directory.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "LFile.h"
#include "LString.h"

/****************************** Defines *************************************************************************************/

#define FILEDEBUG

#define FLOPPY_360K				0x0001
#define FLOPPY_720K				0x0002
#define FLOPPY_1_2M				0x0004
#define FLOPPY_1_4M				0x0008
#define FLOPPY_5_25				(FLOPPY_360K | FLOPPY_1_2M)
#define FLOPPY_3_5				(FLOPPY_720K | FLOPPY_1_4M)

/****************************** Helper Functions ****************************************************************************/

extern void GetFlagsStr(char *flagstr, short flags);
extern int LeapYear(int year);

int SizeofStr(const char *s)
{
	return sizeof(uint32) + ((s) ? strlen(s) : 0);
}

void WriteStr(LFile &f, const char *s)
{
	uint32 Len = (s) ? strlen(s) : 0;
	f << Len;
	if (Len > 0)
	{
		f.Write(s, Len);
	}
}

char *ReadStr(LFile &f DeclDebugArgs)
{
	uint32 Len;
	f >> Len;
	char *s = 0;

	if (Len > 0)
	{
		s = new char[Len+1];
		if (s)
		{
			f.Read(s, Len);
			s[Len] = 0;
		}
		else
		{
			f.Seek(Len, SEEK_CUR);
		}
	}

	return s;
}

bool ResolveShortcut(const char *LinkFile, char *Path, int Length) 
{
	bool Status = FALSE;

	if (LinkFile && Path)
	{
		BSymLink Link(LinkFile);
		if (Link.InitCheck())
		{
			Link.ReadLink(Path, Length);
		}
		else
		{
			strcpy(Path, LinkFile);
		}
	}
	
	return Status;
}

int64 LFileSize(const char *FileName)
{
	BEntry e(FileName);
	off_t s = 0;
	e.GetSize(&s);
	return s;
}

bool LFileExists(const char *FileName, char *CorrectCase)
{
	BEntry e(FileName);
	return e.Exists();
}

bool LDirExists(const char *Dir, char *CorrectCase)
{
	BEntry e(Dir);
	return e.Exists();
}

bool TrimDir(char *Path)
{
	if (Path)
	{
		char *p = Path + (strlen(Path) - 1);
		if (*p == DIR_CHAR)
		{
			*p = 0;
			p--;
		}
		
		for (; p > Path; p--)
		{
			if (*p == DIR_CHAR)
			{
				*p = 0;
				return true;
			}
		}

		p[1] = 0;
		return true;
	}
	return false;
}

char *ReadTextFile(const char *File)
{
	if (File)
	{
		LFile f;
		if (f.Open(File, O_READ))
		{
			int Size = f.GetSize();
			char *m = new char[Size+1];
			if (m)
			{
				int r = f.Read(m, Size);
				if (r > 0)
				{
					m[r] = 0;
					return m;
				}
				DeleteArray(m);
			}
		}
	}
	
	return 0;
}

/****************************** Classes *************************************************************************************/
LFileSystem *LFileSystem::Instance = 0;
#include "GContainers.h"
#include "LgiCommon.h"
#include <Volume.h>
#include <Path.h>
#include <VolumeRoster.h>

LVolume::LVolume()
{
	_Name = 0;
	_Path = 0;
	_Type = VT_NONE;
	_Flags = 0;
	_Size = 0;
	_Free = 0;
}

class BeOSVol : public LVolume
{
	List<LVolume> c;

public:
	BeOSVol(BVolume *Vol = 0)
	{
		_Type = VT_HARDDISK;
		if (Vol)
		{
			_Size = Vol->Capacity();
			_Free = Vol->FreeBytes();
			
			BDirectory d;
			if (Vol->GetRootDirectory(&d) == B_OK)
			{
				BPath p(&d, ".", true);
				_Name = NewStr((char*)p.Path());
				_Path = NewStr((char*)p.Path());
			}
		}
		else
		{
			// Desktop...
			_Name = NewStr("Desktop");

			char d[256];
			if (LgiGetSystemPath(LSP_DESKTOP, d, sizeof(d)))
			{
				_Path = NewStr(d);
			}
			
			// Iterate Children
			BVolumeRoster r;
			BVolume v;
			while (r.GetNextVolume(&v) == B_NO_ERROR)
			{
				c.Insert(new BeOSVol(&v));
			}
		}
	}
	
	~BeOSVol()
	{
		c.DeleteObjects();
	}
	
	

	bool IsMounted()
	{
		return true;
	}
	
	bool SetMounted(bool Mount)
	{
		return false;
	}
	
	LVolume *First()
	{
		return c.First();
	}
	
	LVolume *Next()
	{
		return c.Next();
	}
	
	LDirectory *GetContents()
	{
		LDirectory *Dir = new LDirectory;
		if (_Path)
		{
			if (!Dir->First(_Path))
			{
				DeleteObj(Dir);
			}
		}
		return Dir;
	}
	
	void Insert(LAutoPtr<LVolume> v)
	{
		c.Insert(v.Release());
	}
};

class GFileSystemPrivate
{
public:
	LVolume *RootVol;
	char *CurDir;
	
	GFileSystemPrivate()
	{
		RootVol = 0;
		CurDir = 0;

		char e[256];
		if (LgiGetExePath(e, sizeof(e)))
		{
			CurDir = NewStr(e);
		}
	}
	
	~GFileSystemPrivate()
	{
		DeleteObj(RootVol);
		DeleteArray(CurDir);
	}
};

LFileSystem::LFileSystem()
{
	Instance = this;
	d = new GFileSystemPrivate;
}

LFileSystem::~LFileSystem()
{
	DeleteObj(d);
}

LVolume *LFileSystem::GetRootVolume()
{
	if (!d->RootVol)
	{
		d->RootVol = new BeOSVol;
	}

	return d->RootVol;
}

bool LFileSystem::Copy(char *From, char *To, int *Status, CopyFileCallback Callback, void *Token)
{
	LgiAssert(0);
	return false;
}
	
bool LFileSystem::Delete(const char *FileName, bool ToTrash)
{
	if (FileName)
	{
		return unlink(FileName) == 0;
	}
	return false;
}

bool LFileSystem::Move(char *OldName, char *NewName)
{
	if (!ValidStr(OldName) ||
		!ValidStr(NewName))
	{
		LgiTrace("%s:%i - Invalid params to move file.\n", _FL);
		return false;
	}
	
	int r = rename(OldName, NewName);
	if (r == 0)
		return true;
	
	LgiTrace("%s:%i - rename(%s, %s) failed with %i\n", _FL, OldName, NewName, errno);
	return false;
}

bool LFileSystem::CreateFolder(const char *PathName, bool CreateParentFoldersIfNeeded)
{
	BDirectory Old;
	status_t Status = Old.CreateDirectory(PathName, &Old);
	return (Status == B_OK) || (Status == B_FILE_EXISTS);
}

bool LFileSystem::RemoveFolder(char *PathName, bool Recurse)
{
	bool Status = FALSE;
	BEntry Entry(PathName);
	if (Entry.Exists())
	{
		Status = (Entry.Remove() == B_NO_ERROR);
	}
	return Status;
}

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
	strupr(Name);
	strupr(Mask);

	while (*Name && *Mask)
	{
		if (*Mask == '*')
		{
			if (*Name == *(Mask+1))
			{
				Mask++;
			}
			else
			{
				Name++;
			}
		}
		else if (*Mask == '?' || *Mask == *Name)
		{
			Mask++;
			Name++;
		}
		else
		{
			return FALSE;
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

/////////////////////////////////////////////////////////////////////////////////
bool LDirectory::ConvertToTime(char *Str, int StrSize, uint64 Time)
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, StrSize, "%I:%M:%S", t);
		return true;
	}
	return false;
}

bool LDirectory::ConvertToDate(char *Str, int StrSize, uint64 Time)
{
	time_t k = Time;
	struct tm *t = localtime(&k);
	if (t)
	{
		strftime(Str, StrSize, "%d/%m/%y", t);
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Directory //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class LDirectoryPriv
{
public:
	char *BasePath;
	BDirectory *Dir;
	BEntry Entry;								
	struct stat Stat;

	LDirectoryPriv()
	{
		Dir = 0;
		BasePath = 0;
	}
	
	~LDirectoryPriv()
	{
		DeleteArray(BasePath);
		DeleteObj(Dir);
	}
};

LDirectory::LDirectory()
{
	d = new LDirectoryPriv;
}

LDirectory::~LDirectory()
{
	DeleteObj(d);
}

bool LDirectory::IsReadOnly()
{
	return (GetAttributes() & S_IWUSR) == 0;
}

LDirectory *LDirectory::Clone()
{
	return new LDirectory;
}

int LDirectory::GetUser(bool Group)
{
	return 0;
}

int LDirectory::GetType()
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

bool LDirectory::Path(char *s, int BufLen)
{
	if (!s)
	{
		return false;
	}
	
	if (BufLen > 0)
	{
		if (!GetName() ||
			(strlen(d->BasePath) + strlen(GetName()) + 2 > BufLen) )
		{
			return false;
		}
	}

	LgiMakePath(s, BufLen, d->BasePath, GetName());
	return true;
}

int LDirectory::First(const char *Path, const char *Pattern)
{
	bool Status = false;
	if (Path && Pattern)
	{
		DeleteArray(d->BasePath);
		d->BasePath = NewStr(Path);
		
		if (!d->Dir)
		{
			d->Dir = new BDirectory(Path);
		}
		else
		{
			d->Dir->Rewind();
		}
		if (d->Dir)
		{
			Status = d->Dir->GetNextEntry(&d->Entry) == B_OK;
			if (Status)
			{
				d->Entry.GetStat(&d->Stat);
			}
		}
	}
	return Status;
}

int LDirectory::Next()
{
	bool Status = false;
	if (d->Dir)
	{
		Status = (d->Dir->GetNextEntry(&d->Entry) == B_OK);
		if (Status)
		{
			d->Entry.GetStat(&d->Stat);
		}
	}
	return Status;
}

int LDirectory::Close()
{
	DeleteObj(d->Dir);
	return true;
}

bool LDirectory::IsSymLink()
{
	return d->Entry.IsSymLink();
}

bool LDirectory::IsDir()
{
	return d->Entry.IsDirectory();
}

bool LDirectory::IsHidden()
{
	return false;
}

long LDirectory::GetAttributes()
{
	if (d->Dir)
	{
		return d->Stat.st_mode;
	}
	return 0;
}

char *LDirectory::GetName()
{
	static char Name[MAX_PATH];
	Name[0] = 0;
	
	if (d->Dir)
	{
		d->Entry.GetName(Name);
	}
	
	return Name;
}

const uint64 LDirectory::GetCreationTime()
{
	time_t t;
	if (d->Entry.GetCreationTime(&t) == B_OK)
	{
		return t;
	}
	
	return 0;
}

const uint64 LDirectory::GetLastAccessTime()
{
	time_t t;
	if (d->Entry.GetAccessTime(&t) == B_OK)
	{
		return t;
	}
	
	return 0;
}

const uint64 LDirectory::GetLastWriteTime()
{
	time_t t;
	if (d->Entry.GetModificationTime(&t) == B_OK)
	{
		return t;
	}
	
	return 0;
}

const uint64 LDirectory::GetSize()
{
	if (d->Dir)
	{
		return d->Stat.st_size;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// File ///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
class LFilePrivate : public BFile
{
public:
	char *Name;
	int Status;
	bool Swap;
	int Mode;

	LFilePrivate()
	{
		Name = 0;
		Mode = 0;
		Status = true;
		Swap = false;
	}
	
	~LFilePrivate()
	{
		DeleteObj(Name);
	}
};

LFile::LFile()
{
	d = new LFilePrivate;
}

LFile::~LFile()
{
	Close();
	DeleteObj(d);
}

void LFile::SetSwap(bool s)
{
	d->Swap = s;
}

bool LFile::GetSwap()
{
	return d->Swap;
}

char *LFile::GetName()
{
	return d->Name;
}

int LFile::GetOpenMode()
{
	return d->Mode;
}

int LFile::Open(const char *File, int Mode)
{
	int Status = false;

	if (File)
	{
		int Other = 0;
		
		if (Mode == O_WRITE || Mode == O_READWRITE)
		{
			Other |= B_CREATE_FILE;
		}
		
		Close();
		if (d->SetTo(File, Mode | Other) == B_NO_ERROR)
		{
			d->Mode = Mode;
			d->Name = NewStr(File);
			Status = true;
		}
	}

	return Status;
}

bool LFile::IsOpen()
{
	return d->InitCheck() == B_NO_ERROR;
}

int LFile::Close()
{
	int Status = false;

	if (IsOpen())
	{
		d->Unset();
		Status = true;
	}
	DeleteArray(d->Name);

	return Status;
}

/*
int LFile::Print(char *Format, ...)
{
	int Chars = 0;

	if (Format)
	{
		char Buffer[1024];
		va_list Arg;

		va_start(Arg, Format);
		Chars = vsprintf(Buffer, Format, Arg);
		va_end(Arg);

		if (Chars > 0)
		{
			Write(Buffer, Chars);
		}
	}

	return Chars;
}
*/

#define CHUNK		0xFFF0

int LFile::Read(void *Buffer, int Size, int Flags)
{
	return d->Read(Buffer, Size);
}

int LFile::Write(const void *Buffer, int Size, int Flags)
{
	return d->Write(Buffer, Size);
}

int64 LFile::Seek(int64 To, int Whence)
{
	int64 p;
	switch (Whence)
	{
		default:
		case SEEK_SET:
			p = To;
			break;
		case SEEK_CUR:
			p = GetPos() + To;
			break;
		case SEEK_END:
			p = GetSize() +  To;
			break;
	}
	return SetPos(p);
}

int64 LFile::SetPos(int64 Pos)
{
	return d->Seek(Pos, SEEK_SET);
}

int64 LFile::GetPos()
{
	return d->Position();
}

int64 LFile::GetSize()
{
	off_t Size = 0;
	d->GetSize(&Size);
	return Size;
}

int64 LFile::SetSize(int64 Size)
{
	d->SetSize(Size);
	return GetSize();
}

bool LFile::Eof()
{
	return GetPos() >= GetSize();
}

int LFile::SwapRead(uchar *Buf, int Size)
{
	int i = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		i += d->Read(Buf--, 1);
	}
	return i;
}

int LFile::SwapWrite(uchar *Buf, int Size)
{
	int i = 0;
	Buf = &Buf[Size-1];
	while (Size--)
	{
		i += d->Write(Buf--, 1);
	}
	return i;
}

int LFile::ReadStr(char *Buf, int Size)
{
	int i = 0;
	if (Buf && Size > 0)
	{
		char c;

		Size--;

		do
		{
			i += d->Read((uchar*) &c, 1);
			if (Eof())
			{
				break;
			}
			
			*Buf++ = c;
		
		} while (i < Size - 1 && c != '\n');

		*Buf = 0;
	}

	return i;
}

int LFile::WriteStr(char *Buf, int Size)
{
	int i = 0;

	while (i <= Size)
	{
		i += d->Write((uchar*) Buf, 1);
		Buf++;
		if (!*Buf || *Buf == '\n') break;
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

#define RdIO { d->Status |= ((d->Swap) ? SwapRead((uchar*) &i, sizeof(i)) : d->Read((uchar*) &i, sizeof(i))) != sizeof(i); return *this; }
#define WrIO { d->Status |= ((d->Swap) ? SwapWrite((uchar*) &i, sizeof(i)) : d->Write((uchar*) &i, sizeof(i))) != sizeof(i); return *this; }

LFile &LFile::operator >> (char &i) RdIO
LFile &LFile::operator >> (int8 &i) RdIO
LFile &LFile::operator >> (uint8 &i) RdIO
LFile &LFile::operator >> (int16 &i) RdIO
LFile &LFile::operator >> (uint16 &i) RdIO
LFile &LFile::operator >> (signed int &i) RdIO
LFile &LFile::operator >> (unsigned int &i) RdIO
LFile &LFile::operator >> (signed long &i) RdIO
LFile &LFile::operator >> (unsigned long &i) RdIO
LFile &LFile::operator >> (double &i) RdIO
LFile &LFile::operator >> (int64 &i) RdIO
LFile &LFile::operator >> (uint64 &i) RdIO

LFile &LFile::operator << (char i) WrIO
LFile &LFile::operator << (int8 i) WrIO
LFile &LFile::operator << (uint8 i) WrIO
LFile &LFile::operator << (int16 i) WrIO
LFile &LFile::operator << (uint16 i) WrIO
LFile &LFile::operator << (signed int i) WrIO
LFile &LFile::operator << (unsigned int i) WrIO
LFile &LFile::operator << (signed long i) WrIO
LFile &LFile::operator << (unsigned long i) WrIO
LFile &LFile::operator << (double i) WrIO
LFile &LFile::operator << (int64 i) WrIO
LFile &LFile::operator << (uint64 i) WrIO

int LFile::GetError()
{
	LgiAssert(!"Impl me.");
	return 0;
}

const char *GetErrorName(int e)
{
	LgiAssert(!"Not impl.");
	return 0;
}

bool LgiGetDriveInfo(char *Path, uint64 *Free, uint64 *Size, uint64 *Available)
{
	LgiAssert(!"Not impl.");
	return false;
}
