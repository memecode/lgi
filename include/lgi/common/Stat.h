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
	constexpr static uint64_t WINDOWS_TICK = 10000000;
	constexpr static uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;
	uint64_t UnixToNative(uint64_t ts) const
	{
		#if defined(WINDOWS)
		return (ts + SEC_TO_UNIX_EPOCH) * WINDOWS_TICK;
		#else
		return ts;
		#endif
	}
	int Result = 0;

	LStat(const char *path)
	{
		#if defined(WINDOWS)
		LAutoWString w(Utf8ToWide(path));
		Result = _wstat64(w, this);
		#else
		Result = stat(Path, this);
		#endif
	}

	operator bool() const
	{
		return Result == 0;
	}

	bool IsFile() const { return (st_mode & _S_IFREG) != 0; }
	bool IsDir() const { return (st_mode & _S_IFDIR) != 0; }
	bool IsPipe() const { return (st_mode & _S_IFIFO) != 0; }

	int64_t Size() const { return st_size; }	

	uint64_t GetCreationTime() const { return st_ctime; }
	LDateTime GetCreation() const { return LDateTime(UnixToNative(st_ctime)); }

	uint64_t GetLastAccessTime() const { return st_atime; }
	LDateTime GetLastAccess() const { return LDateTime(UnixToNative(st_atime)); }

	uint64_t GetLastWriteTime() const { return st_mtime; }
	LDateTime GetLastWrite() const { return LDateTime(UnixToNative(st_mtime)); }
};
