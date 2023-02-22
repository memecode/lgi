#include <errno.h>
#include <unistd.h>
#include <OS.h>
#include <pthread.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/EventTargetThread.h"

OsThreadId GetCurrentThreadId()
{
	return find_thread(NULL);
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

		// Do thread's work
		Thread->OnBeforeMain();
		Thread->ReturnValue = Thread->Main();
		Thread->OnAfterMain();

		// mark thread over...
		Thread->State = LThread::THREAD_EXITED;
		bool DelayDelete = false;
		if (Thread->ViewHandle >= 0)
		{
			// If DeleteOnExit is set AND ViewHandle then the LView::OnEvent handle will
			// process the delete... don't do it here.
			DelayDelete = PostThreadEvent(Thread->ViewHandle, M_THREAD_COMPLETED, (LMessage::Param)Thread);
			// However if PostThreadEvent fails... do honour DeleteOnExit.
		}
		
		if (!DelayDelete && Thread->DeleteOnExit)
		{
			DeleteObj(Thread);
		}


		pthread_exit(0);
	}
	return 0;
}

const OsThread LThread::InvalidHandle = NULL;
const OsThreadId LThread::InvalidId = 0;

LThread::LThread(const char *ThreadName, int viewHandle)
{
	Name = ThreadName;
	ThreadId = 0;
	State = LThread::THREAD_INIT;
	ReturnValue = -1;
	hThread = 0;
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
	}
}

int LThread::Main()
{
	return 0;
}

