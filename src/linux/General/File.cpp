/*hdr
**	FILE:			File.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			8/10/2000
**	DESCRIPTION:	The new file subsystem
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes **********************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "lgi/common/LgiDefs.h"
#include "lgi/common/File.h"
#include "lgi/common/Containers.h"
#include "lgi/common/Token.h"
#include "lgi/common/Gdc2.h"
#include "lgi/common/LgiCommon.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/DateTime.h"
#if defined(WIN32)
#include "errno.h"
#endif

/****************************** Defines ***********************************/

// #define FILEDEBUG

#define FLOPPY_360K				0x0001
#define FLOPPY_720K				0x0002
#define FLOPPY_1_2M				0x0004
#define FLOPPY_1_4M				0x0008
#define FLOPPY_5_25				(FLOPPY_360K | FLOPPY_1_2M)
#define FLOPPY_3_5				(FLOPPY_720K | FLOPPY_1_4M)

/****************************** Globals ***********************************/

LString LFile::Path::Sep(DIR_STR);

struct ErrorCodeType
{
	const char *Name;
	int Code;
	const char *Desc;
}
ErrorCodes[] =
{
	#if defined(WIN32)

	{"EPERM", 1, "Not owner"},
	{"ENOENT", 2, "No such file"},
	{"ESRCH", 3, "No such process"},
	{"EINTR", 4, "Interrupted system"},
	{"EIO", 5, "I/O error"},
	{"ENXIO", 6, "No such device"},
	{"E2BIG", 7, "Argument list too long"},
	{"ENOEXEC", 8, "Exec format error"},
	{"EBADF", 9, "Bad file number"},
	{"ECHILD", 10, "No children"},
	{"EAGAIN", 11, "No more processes"},
	{"ENOMEM", 12, "Not enough core"},
	{"EACCES", 13, "Permission denied"},
	{"EFAULT", 14, "Bad address"},
	{"ENOTBLK", 15, "Block device required"},
	{"EBUSY", 16, "Mount device busy"},
	{"EEXIST", 17, "File exists"},
	{"EXDEV", 18, "Cross-device link"},
	{"ENODEV", 19, "No such device"},
	{"ENOTDIR", 20, "Not a directory"},
	{"EISDIR", 21, "Is a directory"},
	{"EINVAL", 22, "Invalid argument"},
	{"ENFILE", 23, "File table overflow"},
	{"EMFILE", 24, "Too many open file"},
	{"ENOTTY", 25, "Not a typewriter"},
	{"ETXTBSY", 26, "Text file busy"},
	{"EFBIG", 27, "File too large"},
	{"ENOSPC", 28, "No space left on"},
	{"ESPIPE", 29, "Illegal seek"},
	{"EROFS", 30, "Read-only file system"},
	{"EMLINK", 31, "Too many links"},
	{"EPIPE", 32, "Broken pipe"},
	{"EWOULDBLOCK", 35, "Operation would block"},
	{"EINPROGRESS", 36, "Operation now in progress"},
	{"EALREADY", 37, "Operation already in progress"},
	{"ENOTSOCK", 38, "Socket operation on"},
	{"EDESTADDRREQ", 39, "Destination address required"},
	{"EMSGSIZE", 40, "Message too long"},
	{"EPROTOTYPE", 41, "Protocol wrong type"},
	{"ENOPROTOOPT", 42, "Protocol not available"},
	{"EPROTONOSUPPORT", 43, "Protocol not supported"},
	{"ESOCKTNOSUPPORT", 44, "Socket type not supported"},
	{"EOPNOTSUPP", 45, "Operation not supported"},
	{"EPFNOSUPPORT", 46, "Protocol family not supported"},
	{"EAFNOSUPPORT", 47, "Address family not supported"},
	{"EADDRINUSE", 48, "Address already in use"},
	{"EADDRNOTAVAIL", 49, "Can't assign requested address"},
	{"ENETDOWN", 50, "Network is down"},
	{"ENETUNREACH", 51, "Network is unreachable"},
	{"ENETRESET", 52, "Network dropped connection"},
	{"ECONNABORTED", 53, "Software caused connection"},
	{"ECONNRESET", 54, "Connection reset by peer"},
	{"ENOBUFS", 55, "No buffer space available"},
	{"EISCONN", 56, "Socket is already connected"},
	{"ENOTCONN", 57, "Socket is not connected"	},
	{"ESHUTDOWN", 58, "Can't send after shutdown"},
	{"ETOOMANYREFS", 59, "Too many references"},
	{"ETIMEDOUT", 60, "Connection timed out"},
	{"ECONNREFUSED", 61, "Connection refused"},
	{"ELOOP", 62, "Too many levels of nesting"},
	{"ENAMETOOLONG", 63, "File name too long"	},
	{"EHOSTDOWN", 64, "Host is down"},
	{"EHOSTUNREACH", 65, "No route to host"},
	{"ENOTEMPTY", 66, "Directory not empty"},
	{"EPROCLIM", 67, "Too many processes"},
	{"EUSERS", 68, "Too many users"},
	{"EDQUOT", 69, "Disc quota exceeded"},
	{"ESTALE", 70, "Stale NFS file handle"},
	{"EREMOTE", 71, "Too many levels of remote in the path"},
	{"ENOSTR", 72, "Device is not a stream"},
	{"ETIME", 73, "Timer expired"},
	{"ENOSR", 74, "Out of streams resources"},
	{"ENOMSG", 75, "No message"},
	{"EBADMSG", 76, "Trying to read unreadable message"},
	{"EIDRM", 77, "Identifier removed"},
	{"EDEADLK", 78, "Deadlock condition"},
	{"ENOLCK", 79, "No record locks available"},
	{"ENONET", 80, "Machine is not on network"},
	{"ERREMOTE", 81, "Object is remote"},
	{"ENOLINK", 82, "The link has been severed"},
	{"EADV", 83, "ADVERTISE error"},
	{"ESRMNT", 84, "SRMOUNT error"},
	{"ECOMM", 85, "Communication error"},
	{"EPROTO", 86, "Protocol error"},
	{"EMULTIHOP", 87, "Multihop attempted"},
	{"EDOTDOT", 88, "Cross mount point"},
	{"EREMCHG", 89, "Remote address change"},

	#elif defined(LINUX) || defined(__GTK_H__)

	{"EPERM",			EPERM, "Operation not permitted"},
	{"ENOENT",			ENOENT, "No such file or directory"},
	{"ESRCH",			ESRCH, "No such process"},
	{"EINTR",			EINTR, "Interrupted system call"},
	{"EIO",				EIO, "I/O error"},
	{"ENXIO",			ENXIO, "No such device or address"},
	{"E2BIG",			E2BIG, "Argument list too long"},
	{"ENOEXEC",			ENOEXEC, "Exec format error"},
	{"EBADF",			EBADF, "Bad file number"},
	{"ECHILD",			ECHILD, "No child processes"},
	{"EAGAIN",			EAGAIN, "Try again"},
	{"ENOMEM",			ENOMEM, "Out of memory"},
	{"EACCES",			EACCES, "Permission denied"},
	{"EFAULT",			EFAULT, "Bad address"},
	{"ENOTBLK",			ENOTBLK, "Block device required"},
	{"EBUSY",			EBUSY, "Device or resource busy"},
	{"EEXIST",			EEXIST, "File exists"},
	{"EXDEV",			EXDEV, "Cross-device link"},
	{"ENODEV",			ENODEV, "No such device"},
	{"ENOTDIR",			ENOTDIR, "Not a directory"},
	{"EISDIR",			EISDIR, "Is a directory"},
	{"EINVAL",			EINVAL, "Invalid argument"},
	{"ENFILE",			ENFILE, "File table overflow"},
	{"EMFILE",			EMFILE, "Too many open files"},
	{"ENOTTY",			ENOTTY, "Not a typewriter"},
	{"ETXTBSY",			ETXTBSY, "Text file busy"},
	{"EFBIG",			EFBIG, "File too large"},
	{"ENOSPC",			ENOSPC, "No space left on device"},
	{"ESPIPE",			ESPIPE, "Illegal seek"},
	{"EROFS",			EROFS, "Read-only file system"},
	{"EMLINK",			EMLINK, "Too many links"},
	{"EPIPE",			EPIPE, "Broken pipe"},
	{"EDOM",			EDOM, "Math argument out of domain of func"},
	{"ERANGE",			ERANGE, "Math result not representable"},
	{"EDEADLK",			EDEADLK, "Resource deadlock would occur"},
	{"ENAMETOOLONG",	ENAMETOOLONG, "File name too long"},
	{"ENOLCK",			ENOLCK, "No record locks available"},
	{"ENOSYS",			ENOSYS, "Function not implemented"},
	{"ENOTEMPTY",		ENOTEMPTY, "Directory not empty"},
	{"ELOOP",			ELOOP, "Too many symbolic links encountered"},
	{"EWOULDBLOCK",		EWOULDBLOCK, "Operation would block"},
	{"ENOMSG",			ENOMSG, "No message of desired type"},
	{"EIDRM",			EIDRM, "Identifier removed"},
	{"EREMOTE",			EREMOTE, "Object is remote"},
	{"ENOLINK",			ENOLINK, "Link has been severed"},
	{"ENOSTR",			ENOSTR, "Device not a stream"},
	{"ENODATA",			ENODATA, "No data available"},
	{"ETIME",			ETIME, "Timer expired"},
	{"ENOSR",			ENOSR, "Out of streams resources"},
	{"EPROTO",			EPROTO, "Protocol error"},
	{"EMULTIHOP",		EMULTIHOP, "Multihop attempted"},
	{"EBADMSG",			EBADMSG, "Not a data message"},
	{"EOVERFLOW",		EOVERFLOW, "Value too large for defined data type"},
	{"EILSEQ",			EILSEQ, "Illegal byte sequence"},
	{"EUSERS",			EUSERS, "Too many users"},
	{"ENOTSOCK",		ENOTSOCK, "Socket operation on non-socket"},
	{"EDESTADDRREQ",	EDESTADDRREQ, "Destination address required"},
	{"EMSGSIZE",		EMSGSIZE, "Message too long"},
	{"EPROTOTYPE",		EPROTOTYPE, "Protocol wrong type for socket"},
	{"ENOPROTOOPT",		ENOPROTOOPT, "Protocol not available"},
	{"EPROTONOSUPPORT",	EPROTONOSUPPORT, "Protocol not supported"},
	{"ESOCKTNOSUPPORT",	ESOCKTNOSUPPORT, "Socket type not supported"},
	{"EOPNOTSUPP",		EOPNOTSUPP, "Operation not supported on transport endpoint"},
	{"EPFNOSUPPORT",	EPFNOSUPPORT, "Protocol family not supported"},
	{"EAFNOSUPPORT",	EAFNOSUPPORT, "Address family not supported by protocol"},
	{"EADDRINUSE",		EADDRINUSE, "Address already in use"},
	{"EADDRNOTAVAIL",	EADDRNOTAVAIL, "Cannot assign requested address"},
	{"ENETDOWN",		ENETDOWN, "Network is down"},
	{"ENETUNREACH",		ENETUNREACH, "Network is unreachable"},
	{"ENETRESET",		ENETRESET, "Network dropped connection because of reset"},
	{"ECONNABORTED",	ECONNABORTED, "Software caused connection abort"},
	{"ECONNRESET",		ECONNRESET, "Connection reset by peer"},
	{"ENOBUFS",			ENOBUFS, "No buffer space available"},
	{"EISCONN",			EISCONN, "Transport endpoint is already connected"},
	{"ENOTCONN",		ENOTCONN, "Transport endpoint is not connected"},
	{"ESHUTDOWN",		ESHUTDOWN, "Cannot send after transport endpoint shutdown"},
	{"ETOOMANYREFS",	ETOOMANYREFS, "Too many references: cannot splice"},
	{"ETIMEDOUT",		ETIMEDOUT, "Connection timed out"},
	{"ECONNREFUSED",	ECONNREFUSED, "Connection refused"},
	{"EHOSTDOWN",		EHOSTDOWN, "Host is down"},
	{"EHOSTUNREACH",	EHOSTUNREACH, "No route to host"},
	{"EALREADY",		EALREADY, "Operation already in progress"},
	{"EINPROGRESS",		EINPROGRESS, "Operation now in progress"},
	{"ESTALE",			ESTALE, "Stale NFS file handle"},

	#ifndef __GTK_H__
	{"EDQUOT",			EDQUOT, "Quota exceeded"},
	{"ENOMEDIUM",		ENOMEDIUM, "No medium found"},
	{"EMEDIUMTYPE",		EMEDIUMTYPE, "Wrong medium type"},
	{"EUCLEAN",			EUCLEAN, "Structure needs cleaning"},
	{"ENOTNAM",			ENOTNAM, "Not a XENIX named type file"},
	{"ENAVAIL",			ENAVAIL, "No XENIX semaphores available"},
	{"EISNAM",			EISNAM, "Is a named type file"},
	{"EREMOTEIO",		EREMOTEIO, "Remote I/O error"},
	{"ERESTART",		ERESTART, "Interrupted system call should be restarted"},
	{"ESTRPIPE",		ESTRPIPE, "Streams pipe error"},
	{"ECOMM",			ECOMM, "Communication error on send"},
	{"EDOTDOT",			EDOTDOT, "RFS specific error"},
	{"ENOTUNIQ",		ENOTUNIQ, "Name not unique on network"},
	{"EBADFD",			EBADFD, "File descriptor in bad state"},
	{"EREMCHG",			EREMCHG, "Remote address changed"},
	{"ELIBACC",			ELIBACC, "Can not access a needed shared library"},
	{"ELIBBAD",			ELIBBAD, "Accessing a corrupted shared library"},
	{"ELIBSCN",			ELIBSCN, ".lib section in a.out corrupted"},
	{"ELIBMAX",			ELIBMAX, "Attempting to link in too many shared libraries"},
	{"ELIBEXEC",		ELIBEXEC, "Cannot exec a shared library directly"},
	{"ECHRNG",			ECHRNG, "Channel number out of range"},
	{"EL2NSYNC",		EL2NSYNC, "Level 2 not synchronized"},
	{"EL3HLT",			EL3HLT, "Level 3 halted"},
	{"EL3RST",			EL3RST, "Level 3 reset"},
	{"ELNRNG",			ELNRNG, "Link number out of range"},
	{"EUNATCH",			EUNATCH, "Protocol driver not attached"},
	{"ENOCSI",			ENOCSI, "No CSI structure available"},
	{"EL2HLT",			EL2HLT, "Level 2 halted"},
	{"EBADE",			EBADE, "Invalid exchange"},
	{"EBADR",			EBADR, "Invalid request descriptor"},
	{"EXFULL",			EXFULL, "Exchange full"},
	{"ENOANO",			ENOANO, "No anode"},
	{"EBADRQC",			EBADRQC, "Invalid request code"},
	{"EBADSLT",			EBADSLT, "Invalid slot"},
	{"EBFONT",			EBFONT, "Bad font file format"},
	{"EADV",			EADV, "Advertise error"},
	{"ESRMNT",			ESRMNT, "Srmount error"},
	{"ENONET",			ENONET, "Machine is not on the network"},
	{"ENOPKG",			ENOPKG, "Package not installed"},
	#endif

	#endif

	{"NONE", 0, "No error"},
};

const char *GetErrorName(int e)
{
	for (ErrorCodeType *c=ErrorCodes; c->Code; c++)
	{
		if (e == c->Code)
		{
			return c->Name;
		}
	}

	static char s[32];
	sprintf(s, "Unknown(%i)", e);
	return s;
}

const char *GetErrorDesc(int e)
{
	for (ErrorCodeType *c=ErrorCodes; c->Code; c++)
	{
		if (e == c->Code)
			return c->Desc;
	}

	return 0;
}

/****************************** Helper Functions **************************/
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
	struct stat64 s;
	if (FileName &&
		stat64(FileName, &s) == 0)
	{
		return s.st_size;
	}

	return 0;
}

bool LDirExists(const char *FileName, char *CorrectCase)
{
	bool Status = false;
	
	if (FileName)
	{
		struct stat s;
		
		// Check for exact match...
		int r = lstat(FileName, &s);
		if (r == 0)
		{
			Status = S_ISDIR(s.st_mode) ||
					 S_ISLNK(s.st_mode);
			// printf("DirStatus(%s) lstat = %i, %i\n", FileName, Status, s.st_mode);
		}
		else
		{
			r = stat(FileName, &s);
			if (r == 0)
			{
				Status = S_ISDIR(s.st_mode) ||
					 	S_ISLNK(s.st_mode);
				// printf("DirStatus(%s) stat ok = %i, %i\n", FileName, Status, s.st_mode);
			}
			else
			{
				// printf("DirStatus(%s) lstat and stat failed, r=%i, errno=%i\n", FileName, r, errno);
			}
		}
	}

	return Status;
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
						if (De->d_type != DT_DIR &&
							stricmp(De->d_name, e) == 0)
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
	if (!LinkFile || !Path || Len <= 0)
		return false;

	ssize_t r = readlink(LinkFile, Path, Len);
	return r > 0;
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
#include <sys/types.h>
#include <sys/statvfs.h>
#include <pwd.h>

struct LVolumePriv
{
	LVolume *Owner = NULL;
	int64 Size = 0, Free = 0;
	LVolumeTypes Type = VT_NONE;
	int Flags = 0;
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
			
		if (sys == LSP_ROOT)
		{
			struct statvfs s = {0};
			int r = statvfs(Path, &s);
			if (r)
				LgiTrace("%s:%i - statvfs failed with %i\n", _FL, r);
			else
			{
				Size = (uint64_t) s.f_blocks * s.f_frsize;
				Free = (uint64_t) s.f_bfree * s.f_frsize;
			}			
		}
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

		return ChildVol;
	}

	LVolume *Next()
	{
		if (SysPath == LSP_DESKTOP && !NextVol)
		{
			NextVol = new LVolume(LSP_MOUNT_POINT, "Mounts");
			
			// Get mount list
			// this is just a hack at this stage to establish some base
			// functionality. I would appreciate someone telling me how
			// to do this properly. Till then...
			LFile f;
			if (f.Open("/etc/fstab", O_READ))
			{
				auto Buf= f.Read();
				f.Close();

				auto Lines = Buf.SplitDelimit("\r\n");
				for (auto ln: Lines)
				{
					auto M = ln.Strip().SplitDelimit(" \t");
					if (M[0](0) != '#' && M.Length() > 2)
					{
						auto &Device  = M[0];
						auto &Mount   = M[1];
						auto &FileSys = M[2];
												
						if
						(
							(Device.Find("/dev/") == 0 || Mount.Find("/mnt/") == 0)
							&&
							Device.Lower().Find("/by-uuid/") < 0
							&& 
							Mount.Length() > 1
							&&
							!FileSys.Equals("swap")
						)
						{
							auto v = new LVolume(0);
							if (v)
							{
								char *MountName = strrchr(Mount, '/');
								v->d->Name = (MountName ? MountName + 1 : Mount.Get());
								v->d->Path = Mount;
								v->d->Type = VT_HARDDISK;

								struct statvfs s = {0};
								int r = statvfs(Mount, &s);
								if (r)
								{
									LgiTrace("%s:%i - statvfs(%s) failed.\n", _FL, Mount);
								}
								else
								{
									v->d->Size = (uint64_t) s.f_blocks * s.f_frsize;
									v->d->Free = (uint64_t) s.f_bfree * s.f_frsize;
								}

								char *Device = M[0];
								if (stristr(Device, "fd"))
									v->d->Type = VT_FLOPPY;
								else if (stristr(Device, "cdrom"))
									v->d->Type = VT_CDROM;

								Insert(NextVol, v);
							}
						}
					}
				}
			}
		}
		
		return NextVol;
	}
};

LVolume::LVolume(const char *Path)
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

LVolumeTypes LVolume::Type() const
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
LFileSystem::LFileSystem()
{
	Instance = this;
	Root = 0;
}

LFileSystem::~LFileSystem()
{
	DeleteObj(Root);
}

bool LFileSystem::Copy(const char *From, const char *To, LError *Status, std::function<bool(uint64_t pos, uint64_t total)> callback)
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
				if (callback)
					callback(Done, Size);
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
					{
						(*Status)[i] = errno;
					}
					
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
				{
					(*Status)[i] = errno;
				}
				
				Error = true;
			}			
		}
	}
	
	return !Error;
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
		LDirectory *Dir = new LDirectory;
		if (Dir && Dir->First(PathName))
		{
			do
			{
				char Str[256];
				Dir->Path(Str, sizeof(Str));

				if (Dir->IsDir())
				{
					RemoveFolder(Str, Recurse);
				}
				else
				{
					LError err;
					Delete(Str, &err, false);
				}
			}
			while (Dir->Next());
		}
		DeleteObj(Dir);
	}

	return rmdir(PathName) == 0;
}

bool LFileSystem::Move(const char *OldName, const char *NewName, LError *Err)
{
	if (rename(OldName, NewName))
	{
		printf("%s:%i - rename(%s,%s) error: %s(%i)\n",
			_FL,
			OldName, NewName,
			GetErrorName(errno), errno);
		return false;
	}
	
	return true;
}


short DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int LeapYear(int year)
{
	if (year & 3)
		return 0;

	if ((year % 100 == 0) && !(year % 400 == 0))
		return 0;
	
	return 1;
}

/////////////////////////////////////////////////////////////////////////////////
struct LDirectoryPriv
{
	char			path[MAX_PATH_LEN] = {};
	char			*end = NULL;
	
	DIR				*dir    = NULL;
	struct dirent	*entry  = NULL;
	struct stat		stat;
	
	LString			pattern;

	bool Ignore()
	{
		return	entry
				&&
				(
					strcmp(entry->d_name, ".") == 0
					||
					strcmp(entry->d_name, "..") == 0
					||
					(
						pattern
						&&
						!MatchStr(pattern, entry->d_name)
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

	strcpy_s(d->path, sizeof(d->path), Name);
	if (!Pattern || stricmp(Pattern, LGI_ALL_FILES) == 0)
	{
		struct stat S;
		if (lstat(Name, &S) == 0)
		{
			if (S_ISREG(S.st_mode)) // If it's a file...
			{
				auto Dir = strrchr(d->path, DIR_CHAR);
				if (Dir)
				{
					*Dir++ = 0; // Go up one level and make the file name 
					d->pattern = Dir; // The search pattern.
				}
			}
		}
	}
	else
	{
		d->pattern = Pattern;
	}
	
	d->end = d->path + strlen(d->path);	
	d->dir = opendir(d->path);
	if (d->dir)
	{
		d->entry = readdir(d->dir);
		if (d->entry)
		{
			char s[MaxPathLen];
			LMakePath(s, sizeof(s), d->path, GetName());
			lstat(s, &d->stat);

			if (d->Ignore() && !Next())
				return false;
		}
	}

	return d->dir != NULL && d->entry != NULL;
}

int LDirectory::Next()
{
	int Status = false;

	while (d->dir && d->entry)
	{
		if ((d->entry = readdir(d->dir)))
		{
			char s[MaxPathLen];
			LMakePath(s, sizeof(s), d->path, GetName());			
			lstat(s, &d->stat);

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
	if (d->dir)
	{
		closedir(d->dir);
		d->dir = NULL;
	}
	d->entry = NULL;

	return true;
}

const char *LDirectory::FullPath()
{
	if (!d->entry || !d->end)
	{
		LAssert(!"missing param.");
		return NULL;
	}	

	auto e = d->end;
	if (e[-1] != DIR_CHAR)
		*e++ = DIR_CHAR;
	auto remainingBuf = sizeof(d->path) - (e - d->path) - 1;
	strcpy_s(e, remainingBuf, d->entry->d_name);
	
	return d->path;
}

LString LDirectory::FileName() const
{
	return GetName();
}

bool LDirectory::Path(char *s, int BufLen) const
{
	if (!s || BufLen <= 4)
		return false;

	// We could just do LMakePath, but then it'll expand '~' names. Which
	// would break things.
	// return LMakePath(s, BufLen, d->BasePath, GetName());
	
	auto end = s + BufLen;
	strcpy_s(s, BufLen, d->path);
	auto c = s + strlen(s);
	if (c > s && c[-1] != DIR_CHAR)
		*c++ = DIR_CHAR;
	strcpy_s(c, end - c, GetName());	
	return true;
}

LVolumeTypes LDirectory::GetType() const
{
	return IsDir() ? VT_FOLDER : VT_FILE;
}

int LDirectory::GetUser(bool Group) const
{
	if (Group)
	{
		return d->stat.st_gid;
	}
	else
	{
		return d->stat.st_uid;
	}
}

bool LDirectory::IsReadOnly() const
{
	if (getuid() == d->stat.st_uid)
	{
		// Check user perms
		return !TestFlag(GetAttributes(), S_IWUSR);
	}
	else if (getgid() == d->stat.st_gid)
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
	return d->stat.st_mode;
}

const char *LDirectory::GetName() const
{
	return d->entry ? d->entry->d_name : NULL;
}

#define UNIX_TO_LGI(unixTs) \
	(((uint64) unixTs + LDateTime::Offset1800) * LDateTime::Second64Bit)
#define LGI_TO_UNIX(lgiTs) \
	(((uint64) lgiTs / LDateTime::Second64Bit) - LDateTime::Offset1800)

uint64 LDirectory::GetCreationTime() const
{
	return UNIX_TO_LGI(d->stat.st_ctime);
}

uint64 LDirectory::GetLastAccessTime() const
{
	return UNIX_TO_LGI(d->stat.st_atime);
}

uint64 LDirectory::GetLastWriteTime() const
{
	return UNIX_TO_LGI(d->stat.st_mtime);
}

uint64 LDirectory::GetSize() const
{
	return d->stat.st_size;
}

int64 LDirectory::GetSizeOnDisk()
{
	return d->stat.st_size;
}

int64_t LDirectory::TsToUnix(uint64_t timeStamp)
{
	return LGI_TO_UNIX(timeStamp);
}

LDateTime LDirectory::TsToDateTime(uint64_t timeStamp, bool convertToLocalTz)
{
	LDateTime dt;
	dt.SetTimeZone(0, false); // UTC
	dt.Set(timeStamp);

	LArray<LDateTime::LDstInfo> dst;
	if (convertToLocalTz &&
		LDateTime::GetDaylightSavingsInfo(dst, dt))
	{
		// Convert to local using the timezone in effect at 'dt'
		LDateTime::DstToLocal(dst, dt);
	}
	else
	{
		// Without knowing the timezone at 'dt' just leave it as UTC...
		// The caller will have to figure it out.
	}
	return dt;
}

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// File ///////////////////////////////////////////////
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
	return d && ValidHandle(d->hFile);
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

			printf("LFile::Open failed\n\topen(%s,%8.8x) = %i\n\terrno=%s (%s)\n",
				File, 
				Mode, 
				d->hFile,
				GetErrorName(d->ErrorCode),
				GetErrorDesc(d->ErrorCode));
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
		printf("%s:%i - lseek64(" LPrintfHex64 ") failed (error %i: %s).\n",
			__FILE__, __LINE__,
			Pos, e, GetErrorName(e));
	}
	return p;
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
		
		if (
			#if LINUX64
			ftruncate64(d->hFile, Size)
			#else
			ftruncate(d->hFile, Size)
			#endif
		)
			LgiTrace("%s:%i - ftruncate64 failed: %i\n", _FL, d->ErrorCode = errno);
		else if (d->hFile)
			SetPos(Pos);
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
