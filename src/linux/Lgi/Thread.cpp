#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/EventTargetThread.h"

OsThreadId LCurrentThreadId()
{
	#ifdef SYS_gettid
	return syscall(SYS_gettid);
	#else
	LAssert(0);
	return 0;
	#endif
}

////////////////////////////////////////////////////////////////////////////
void *ThreadEntryPoint(void *i)
{
	if (i)
	{
		LThread *Thread = (LThread*) i;
		Thread->ThreadId = LCurrentThreadId();
		LThread::RegisterThread(Thread->ThreadId, Thread->Name);

		// Make sure we have finished executing the setup
		while (Thread->State == LThread::THREAD_INIT)
		{
			LSleep(1);
		}
		
		pthread_detach(Thread->hThread);
		
		if (Thread->Name)
		{
			auto nm = Thread->Name;
		    if (Thread->Name.Length() >= 16)
			{
				printf("%s:%i - thread %i name too long '%s'\n", _FL, LCurrentThreadId(), Thread->Name.Get());
				nm = Thread->Name(-16, -1);
			}
			pthread_setname_np(pthread_self(), nm);
		}

		// Do thread's work
		Thread->OnBeforeMain();
		Thread->ReturnValue = Thread->Main();
		Thread->OnAfterMain();

		// Shutdown...
		Thread->State = LThread::THREAD_EXITED;
		if (Thread->DeleteOnExit)
		{
			DeleteObj(Thread);
		}

		pthread_exit(0);
	}
	return 0;
}

const OsThread LThread::InvalidHandle = 0;
const OsThreadId LThread::InvalidId = 0;

LThread::LThread(const char *ThreadName)
{
	Name = ThreadName;
}

LThread::~LThread()
{
	if (!IsExited())
		Terminate();
}

int LThread::ExitCode()
{
	return ReturnValue;
}

bool LThread::IsExited()
{
	return State == LThread::THREAD_EXITED;
}

void LThread::Run()
{
	Terminate();
	
	if (!hThread)
	{
		State = LThread::THREAD_INIT;

		static int Creates = 0;
		int e;
		if (!(e = pthread_create(&hThread, NULL, ThreadEntryPoint, (void*)this)))
		{
			Creates++;
			State = LThread::THREAD_RUNNING;
		}
		else
		{
			const char *Err = "(unknown)";
			switch (e)
			{
				case EAGAIN: Err = "EAGAIN"; break;
				case EINVAL: Err = "EINVAL"; break;
				case EPERM: Err = "EPERM"; break;
				case ENOMEM: Err = "ENOMEM"; break;
			}
			printf("%s,%i - pthread_create failed with the error %i (%s) (After %i creates)\n", __FILE__, __LINE__, e, Err, Creates);
			
			State = LThread::THREAD_EXITED;
		}
	}
}

void LThread::Terminate()
{
	if (hThread &&
		pthread_cancel(hThread) == 0)
	{
		State = LThread::THREAD_EXITED;
		hThread = 0;
	}
}

int LThread::Main()
{
	return 0;
}

