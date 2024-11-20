//
//  LCancel.h
//  Lgi
//
//  Created by Matthew Allen on 19/10/2017.
//
//

#ifndef _LERROR_H_
#define _LERROR_H_

#include <stdarg.h>
#ifdef POSIX
#include "errno.h"
#endif

// Some cross platform error symbols (by no means a complete list)
enum LErrorCodes
{
#if defined(WINDOWS)
	LErrorNone			= ERROR_SUCCESS,
	LErrorAccessDenied	= ERROR_ACCESS_DENIED,
	LErrorNoMem			= ERROR_NOT_ENOUGH_MEMORY,
	LErrorInvalidParam	= ERROR_INVALID_PARAMETER,
	LErrorTooManyFiles	= ERROR_NO_MORE_FILES,
	LErrorFileExists	= ERROR_FILE_EXISTS,
	LErrorPathNotFound	= ERROR_PATH_NOT_FOUND,
	LErrorReadOnly		= ERROR_FILE_READ_ONLY,
	LErrorNotSupported  = ERROR_NOT_SUPPORTED,
	LErrorTryAgain      = ERROR_RETRY,
	LErrorFuncFailed    = ERROR_FUNCTION_FAILED,
	LErrorTimerExpired  = ERROR_SEM_TIMEOUT,
	LErrorAddrInUse     = WSAEADDRINUSE,
	LErrorAddrNotAvail  = WSAEADDRNOTAVAIL,
	LErrorIoFailed      = ERROR_WRITE_FAULT,
#elif defined(POSIX)
	LErrorNone			= 0,
	LErrorAccessDenied	= EACCES,
	LErrorNoMem			= ENOMEM,
	LErrorInvalidParam	= EINVAL,
	LErrorTooManyFiles	= EMFILE,
	LErrorFileExists	= EEXIST,
	LErrorPathNotFound	= ENOENT,
	LErrorReadOnly		= EROFS,
	LErrorNotSupported  = EOPNOTSUPP,
	LErrorTryAgain      = EAGAIN,
	LErrorFuncFailed    = EFAULT,
	LErrorTimerExpired  = ETIME,
	LErrorAddrInUse     = EADDRINUSE,
	LErrorAddrNotAvail  = EADDRNOTAVAIL,
	LErrorIoFailed      = EIO,
#else
	#warning "Impl me."
#endif
};

/// Converts an OS error code into a text string
LgiExtern LString LErrorCodeToString(uint32_t ErrorCode);

class LgiClass LError
{
	// This error code is defined by the operating system
	int Code = 0;
	LString Msg;
	
public:
	LString::Array DevNotes;

	LError(int code = 0, const char *msg = NULL)
	{
		Set(code, msg);
	}

	LError &operator =(int code)
	{
		Code = code;
		return *this;
	}

	operator bool() const
	{
		return Code != 0;
	}

	void Set(int code, const char *msg = NULL)
	{
		Code = code;
		Msg = msg;
	}

	int GetCode() { return Code; }

	LString GetMsg()
	{
		if (!Msg)
			Msg = LErrorCodeToString(Code);
		return Msg;
	}
	
	void AddNote(const char *File, int Line, const char *Fmt, ...)
	{
		char buffer[512] = {0};
		va_list arg;
		va_start(arg, Fmt);
		vsprintf_s(buffer, sizeof(buffer), Fmt, arg);
		va_end(arg);
		
		DevNotes.New().Printf("%s:%i - %s", File, Line, buffer);
	}
	
	LString GetNotes() const
	{
		return LString("\n").Join(DevNotes);
	}
	
	LString ToString()
	{
		LString s;
		s.Printf("%i %s", Code, GetMsg().Get());
		if (DevNotes.Length())
			s += LString(" (") + LString(",").Join(DevNotes) + ")";
		return s.Strip();
	}
};


#endif /* _LERROR_H_ */
