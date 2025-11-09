#include <stdio.h>
#include <errno.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Thread.h"

//////////////////////////////////////////////////////////////////////////////
char *SemPrint(OsSemaphore *s)
{
	static char Buf[256];
	
	int ch = 0;
	for (int i=0; i<sizeof(OsSemaphore); i++)
	{
		ch += sprintf_s(Buf+ch, sizeof(Buf)-ch, "%2.2X,", ((uchar*)s)[i]);
	}
	
	return Buf;
}

LMutex::LMutex(const char *name)
{
    LAssert(name != NULL);
	_Name = name;

	#if defined WIN32
	
		InitializeCriticalSection(&_Sem);
	
	#elif defined POSIX

		if (pthread_mutex_init(&_Sem, 0))
		{
			LgiTrace("%s:%i - Couldn't create mutex for LMutex\n", __FILE__, __LINE__);
		}

	#endif
}

LMutex::~LMutex()
{
	#if defined WIN32

		// CloseHandle(_Sem);
		// _Sem = 0;
		DeleteCriticalSection(&_Sem);

	#elif defined POSIX

		pthread_mutex_destroy(&_Sem);

	#endif
}

const char *LMutex::GetName()
{
	return _Name ? _Name : "LMutex";
}

void LMutex::SetName(const char *s)
{
	_Name = s;
}

bool LMutex::_Lock()
{
	#if defined WIN32

		EnterCriticalSection(&_Sem);

	#elif defined POSIX

		if (pthread_mutex_trylock(&_Sem))
		{
			// printf("\t%s:%i - pthread_mutex_trylock errored: %s\n\t_Sem=%s\n", __FILE__, __LINE__, GetErrorName(errno), SemPrint(&_Sem));
			return false;
		}

	#endif

	return true;
}

void LMutex::_Unlock()
{
	#if defined WIN32

		LeaveCriticalSection(&_Sem);

	#elif defined POSIX

		if (pthread_mutex_unlock(&_Sem))
		{
			printf("\t%s:%i - pthread_mutex_unlock errored\n", _FL);
		}

	#endif
}

bool LMutex::Lock(const char *file, int line, bool NoTrace)
{
	auto Start = LCurrentTime();
	bool Status = false;
	OsThreadId CurrentThread = LCurrentThreadId();
	bool Warn = true;

	LAssert(file != NULL && line != 0);

	while (!Status)
	{
		if (_Lock())
		{
			if (!_Thread ||
				_Thread == CurrentThread)
			{
				_Thread = CurrentThread;
				_Count++;
				File = file;
				Line = line;

				Status = true;
			}
			_Unlock();
		}

		if (!Status)
		{
			LSleep(1);
		}

		#if 1 // _DEBUG
		auto Now = LCurrentTime();
		if (Warn && Now > Start + 5000 && !NoTrace)
		{
			auto lockThreadName = LThread::GetThreadName(_Thread);
			auto thisThreadName = LThread::GetThreadName(CurrentThread);
			LgiTrace("LMutex=%p(%s): Can't lock after %ims... locking=%i/%s (%s:%i) cur=%i/%s count=%i\n",
					this,
					_Name.Get(),
					(int)(Now - Start),
					// locking:
					_Thread, lockThreadName, File, Line,
					// cur:
					CurrentThread, thisThreadName,
					_Count);
			Start = Now;

			// Warn = false;
		}
		
		if (LCurrentTime() > Start + (2 * 60 * 1000) && !NoTrace)
		{
			// Obviously we've locked up and to un-deadlock things we'll fail the lock
			LgiTrace("::Lock timeout ask_thread=%i hold_thread=%i sem=%i count=%i\n", _Thread, CurrentThread, _Sem, _Count);
			break;
		}
		#endif
	}

	return Status;
}

bool LMutex::LockWithTimeout(int Timeout, const char *file, int line)
{
	auto Start = LCurrentTime();
	bool Status = false;

	LAssert(file != NULL && line != 0);

	while (!Status &&
			LCurrentTime() < Start + Timeout)
	{
		if (_Lock())
		{
			OsThreadId CurrentThread = LCurrentThreadId();
			if (!_Thread ||
				_Thread == CurrentThread)
			{
				_Thread = CurrentThread;
				_Count++;
				File = file;
				Line = line;
				Status = true;
			}
			_Unlock();
		}

		if (!Status)
		{
			LSleep(5);
		}
	}

	return Status;
}

void LMutex::Unlock()
{
	while (!_Lock())
	{
		LSleep(1);
	}
	
	if (_Count < 1)
	{
		printf("%s:%i - _Count=%i\n", __FILE__, __LINE__, _Count);
	
		#if 0
		// I want a stack trace the next time this happens
		int *i = 0;
		*i = 0;
		#endif
		
		// if this assert fails then you tryed to unlock an object that
		// wasn't locked in the first place
		LAssert(0);
	}

	// decrement the lock counter
	if (_Count > 0)
	{
		_Count--;
	}
	if (_Count < 1)
	{
		_Thread = 0;
		File = 0;
		Line = 0;
	}

	_Unlock();
}

