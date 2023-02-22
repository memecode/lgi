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
#elif defined(POSIX)
	LErrorNone = 0,
	LErrorAccessDenied	= EACCES,
	LErrorNoMem			= ENOMEM,
	LErrorInvalidParam	= EINVAL,
	LErrorTooManyFiles	= EMFILE,
	LErrorFileExists	= EEXIST,
	LErrorPathNotFound	= ENOTDIR,
#else
	#warning "Impl me."
#endif
};

/// Converts an OS error code into a text string
LgiExtern LString LErrorCodeToString(uint32_t ErrorCode);

class LgiClass LError
{
	// This error code is defined by the operating system
	int Code;
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
	
	LString GetNotes()
	{
		return LString("\n").Join(DevNotes);
	}
};


#endif /* _LERROR_H_ */
