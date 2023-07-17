#ifndef __GTHREAD_H
#define __GTHREAD_H

//////////////////////////////////////////////////////////////////////////
// Thread types are defined in LMutex.h
#include "lgi/common/Mutex.h"

class LgiClass LThread
{
public:
    enum ThreadState
    {
	    THREAD_INIT = 1,
	    THREAD_RUNNING = 2,
	    THREAD_ASLEEP = 3,
	    THREAD_EXITED = 4,
	    THREAD_ERROR = 5
    };
    
    enum ThreadPriority
    {
		ThreadPriorityIdle,
		ThreadPriorityNormal,
		ThreadPriorityHigh,
		ThreadPriorityRealtime,
    };

	static const OsThread InvalidHandle;
	static const OsThreadId InvalidId;

protected:
	ThreadState State       = THREAD_INIT;
	ThreadPriority Priority = ThreadPriorityNormal;
	OsThread hThread        = InvalidHandle;
	OsThreadId ThreadId     = InvalidId;
	int ReturnValue         = -1;
	LString Name;

	/// Auto deletes the thread after ::Main has finished.
	bool DeleteOnExit       = false;

	/// Aka from LView::AddDispatch().
	int ViewHandle          = -1;

	friend bool LView::CommonEvents(LMessage::Result &result, LMessage *Msg);
	#if defined WIN32
		friend uint WINAPI ThreadEntryPoint(void *i);
	    void Create(class LThread *Thread, OsThread &hThread, OsThreadId &ThreadId);
	#elif defined POSIX
		friend void *ThreadEntryPoint(void *i);
	#else
		friend int32 ThreadEntryPoint(void *i);
	#endif

public:
	LThread(
		/// Name for the thread.
		const char *Name,
		/// [Optional] Handle from LView::AddDispatch()
		/// This enables the OnComplete event to be called
		/// from the GUI thread after the thread exits.
		int viewHandle = -1
	);
	virtual ~LThread();

	// Properties
	OsThread Handle() { return hThread; }
	const char *GetName() { return Name; }
	OsThreadId GetId() { return ThreadId; }
	ThreadState GetState() { return State; } // Volatile at best... only use for 'debug'
	bool GetDeleteOnExit() { return DeleteOnExit; }
	virtual int ExitCode();
	virtual bool IsExited();

	// Methods
	virtual void Run();
	virtual void Terminate();
	virtual void WaitForExit(int WarnAfterMs = 2000);

	// Override to do something
	virtual int Main() = 0;

	// Events
	virtual void OnBeforeMain() {}
	virtual void OnAfterMain() {}

	/// This event runs after the thread has finished but 
	/// is called from the main GUI thread. It requires a valid
	/// viewHandle to be passed to the constructor. Which can
	/// be aquired by calling LView::AddDispatch().
	virtual void OnComplete() {}
};

////////////////////////////////////////////////////////////////////////////////////////
class LThreadOwner;
class LThreadTarget;
class LThreadWorker;

/// A generic threaded job parent class.
class LgiClass LThreadJob
{
	friend class LThreadWorker;
	LThreadTarget *Owner;

public:
	LThreadJob(LThreadTarget *o) { Owner = o; }
	virtual ~LThreadJob() {}
	virtual void Do() {}
};

/// The thread target is a virtual API to receive work units executed 
/// in the worker thread.
class LgiClass LThreadTarget : public LMutex
{
	friend class LThreadWorker;

protected:
	/// The thread doing the work.
	LThreadWorker *Worker;

public:
	LThreadTarget();
	virtual ~LThreadTarget() {}
	
	virtual void SetWorker(LThreadWorker *w);
	virtual void Detach();
	
	/// This function gets called when the job is finished
	virtual void OnDone(LAutoPtr<LThreadJob> j) {}
};

#undef AddJob

/// This parent class does the actual work of processing jobs.
class LgiClass LThreadWorker : public LThread, public LMutex
{
	LArray<LThreadTarget*> Owners;
	LArray<LThreadJob*> Jobs;
	bool Loop;

public:
	LThreadWorker(LThreadTarget *First, const char *ThreadName);
	virtual ~LThreadWorker();
	
	void Stop();
	void Attach(LThreadTarget *o);
	void Detach(LThreadTarget *o);
	virtual void AddJob(LThreadJob *j);
	virtual void DoJob(LThreadJob *j);
	int Main();
};

class LgiClass LThreadOwner : public LThreadTarget
{
public:
	virtual ~LThreadOwner();
};

#endif
