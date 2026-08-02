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

		// Wait briefly for creator thread to publish RUNNING; avoid an infinite spin.
		int waits = 0;
		while (__atomic_load_n((int*)&Thread->State, __ATOMIC_ACQUIRE) == LThread::THREAD_INIT)
		{
			if (++waits >= 500)
			{
				LgiTrace("%s:%i - warning: thread '%s' still in INIT after %i ms, continuing\n",
					_FL, Thread->Name.Get(), waits);
				break;
			}
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
		__atomic_store_n((int*)&Thread->State, LThread::THREAD_EXITED, __ATOMIC_RELEASE);
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
	return __atomic_load_n((int*)&State, __ATOMIC_ACQUIRE) == THREAD_EXITED;
}

void LThread::Run()
{
	Terminate();
	
	if (!hThread)
	{
		__atomic_store_n((int*)&State, THREAD_INIT, __ATOMIC_RELEASE);

		static int Creates = 0;
		int e;
		if (!(e = pthread_create(&hThread, NULL, ThreadEntryPoint, (void*)this)))
		{
			Creates++;
			__atomic_store_n((int*)&State, THREAD_RUNNING, __ATOMIC_RELEASE);
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
			
			__atomic_store_n((int*)&State, THREAD_EXITED, __ATOMIC_RELEASE);
		}
	}
}

void LThread::Terminate()
{
	if (hThread &&
		pthread_cancel(hThread) == 0)
	{
		__atomic_store_n((int*)&State, THREAD_EXITED, __ATOMIC_RELEASE);
		hThread = 0;
	}
}

int LThread::Main()
{
	return 0;
}

