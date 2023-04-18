#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/EventTargetThread.h"

OsThreadId GetCurrentThreadId()
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
		Thread->ThreadId = GetCurrentThreadId();

		// Make sure we have finished executing the setup
		while (Thread->State == LThread::THREAD_INIT)
		{
			LSleep(1);
		}
		
		pthread_detach(Thread->hThread);
		
		LString Nm = Thread->Name;
		if (Nm)
			pthread_setname_np(pthread_self(), Nm);

		// Do thread's work
		Thread->OnBeforeMain();
		Thread->ReturnValue = Thread->Main();
		Thread->OnAfterMain();

		// Shutdown...
		Thread->State = LThread::THREAD_EXITED;
		bool DelayDelete = false;
		if (Thread->ViewHandle >= 0)
		{
			// If DeleteOnExit is set AND ViewHandle then the LView::OnEvent handle will
			// process the delete... don't do it here.
			DelayDelete = PostThreadEvent(Thread->ViewHandle, M_THREAD_COMPLETED, (LMessage::Param)Thread);
			// However if PostThreadEvent fails... do honour DeleteOnExit.
		}
		
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

LThread::LThread(const char *ThreadName, int viewHandle)
{
	Name = ThreadName;
	ViewHandle = viewHandle;
	ThreadId = InvalidId;
	State = LThread::THREAD_INIT;
	ReturnValue = -1;
	hThread = InvalidHandle;
	DeleteOnExit = false;
}

LThread::~LThread()
{
	if (!IsExited())
	{
		Terminate();
	}
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

