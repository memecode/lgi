/// \file
#ifndef _GMUTEX_H_
#define _GMUTEX_H_

#include "LCurrentTime.h"

/// This is a re-enterant mutex class for thread locking.
class LgiClass LMutex
{
	OsThreadId _Thread;
	OsSemaphore _Sem;
	const char *File;
	int Line;
	int MaxLockTime; // Warn if locked too long, default off (-1)
	#ifdef _DEBUG
	bool _DebugSem;
	#endif
	
	bool _Lock();
	void _Unlock();
	char *_Name;

protected:
	int _Count;

public:
	/// Constructor
	LMutex
	(
		/// Optional name for the semaphore
		const char *name = 0
	);
	virtual ~LMutex();

	/// Lock the semaphore, waiting forever
	///
	/// \returns true if locked successfully.
	bool Lock
	(
		/// The file name of the locker
		const char *file,
		/// The line number of the locker
		int line,
		/// [Optional] No trace to prevent recursion in LgiTrace.
		bool NoTrace = false
	);
	/// \returns true if the semephore was locked in the time
	bool LockWithTimeout
	(
		/// In ms
		int Timeout,
		/// The file of the locker
		const char *file,
		/// The line number of the locker
		int line
	);
	/// Unlocks the semaphore
	void Unlock();

	const char *GetName();
	void SetName(const char *s);

	int GetMaxLock() { return MaxLockTime; }
	void SetMaxLock(int Ms = -1) { MaxLockTime = Ms; }
	
	#ifdef _DEBUG
	void SetDebug(bool b = true) { _DebugSem = b; }
	int GetCount() { return _Count; }
	#endif
	
	class Auto
	{
	    LMutex *Sem;
	    bool Locked;
	    const char *File;
	    int Line;
		uint64_t LockTs;
	
	public:
	    Auto(LMutex *s, const char *file, int line)
	    {
	        LgiAssert(s != NULL);
	        Locked = (Sem = s) ? Sem->Lock(File = file, Line = line) : 0;
			LgiAssert(Locked);
			LockTs = Locked && s->MaxLockTime > 0 ? LgiCurrentTime() : 0;
	    }

	    Auto(LMutex *s, int timeout, const char *file, int line)
	    {
	        LgiAssert(s != NULL);
			Sem = s;
			if (!Sem)
				Locked = false;
			else if (timeout >= 0)
				Locked = Sem->LockWithTimeout(timeout, File = file, Line = line);
			else
				Locked = Sem->Lock(File = file, Line = line);
			LockTs = Locked && s->MaxLockTime > 0 ? LgiCurrentTime() : 0;
	    }
	    
	    ~Auto()
	    {
	        if (Locked) Sem->Unlock();
			if (LockTs)
			{
				uint64_t Now = LgiCurrentTime();
				if (Now - LockTs >= Sem->MaxLockTime)
					LgiTrace(	"Warning: %s locked for %ims (%s:%i)\n",
								Sem->GetName(),
								(int)(Now-LockTs),
								GetFile(),
								GetLine());
			}
	    }

		bool GetLocked() { return Locked; }
		const char *GetFile() { return File; }
		int GetLine() { return Line; }
		operator bool() { return Locked; }
	};
};


template<typename T>
class LThreadSafeInterface : public LMutex
{
	T *object;

public:
	class Locked : public LMutex::Auto
	{
		LThreadSafeInterface *tsi;
	
	public:
		Locked(LThreadSafeInterface *i, const char *file, int line) : LMutex::Auto(i, -1, file, line), tsi(i)
		{			
		}

		operator bool()
		{
			return GetLocked();
		}

		T *operator ->()
		{
			LgiAssert(GetLocked());
			return tsi->object;
		}
	};

	LThreadSafeInterface(T *obj, const char *name = NULL) : LMutex(name ? name : "LThreadSafeInterface")
	{
		object = obj;
	}

	~LThreadSafeInterface()
	{
		Lock(_FL);
	}

	Locked Lock(const char *file, int line)
	{
		return Locked(this, file, line);
	}
};

#endif
