#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#if !defined(WINDOWS)
#include <unistd.h>
#endif

struct LStat :
	#if defined(WINDOWS)
	public _stat64
	#else
	public stat
	#endif
{
	int Result = 0;

	LStat(const char *path)
	{
		#if defined(WINDOWS)
		LAutoWString w(Utf8ToWide(path));
		Result = _wstat64(w, this);
		#else
		Result = ::stat(path, this);
		#endif
	}

	operator bool() const
	{
		return Result == 0;
	}

	bool IsFile() const
	{
		#if defined(WINDOWS)
		return (st_mode & _S_IFREG) != 0;
		#else
		return S_ISREG(st_mode);
		#endif
	}
	bool IsDir()  const {
		#if defined(WINDOWS)
		return (st_mode & _S_IFDIR) != 0;
		#else
		return S_ISDIR(st_mode);
		#endif
	}
	bool IsPipe() const {
		#if defined(WINDOWS)
		return (st_mode & _S_IFIFO) != 0;
		#else
		return S_ISFIFO(st_mode);
		#endif
	}

	int64_t Size() const { return st_size; }	

	uint64_t GetCreationTime() const { return st_ctime; }
	LDateTime GetCreation() const { return LDateTime(LTimeStamp(st_ctime)); }

	uint64_t GetLastAccessTime() const { return st_atime; }
	LDateTime GetLastAccess() const { return LDateTime(LTimeStamp(st_atime)); }

	uint64_t GetLastWriteTime() const { return st_mtime; }
	LDateTime GetLastWrite() const { return LDateTime(LTimeStamp(st_mtime)); }
};
