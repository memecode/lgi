#include "lgi/common/Lgi.h"
#include "lgi/common/Mutex.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/Thread.h"
#include "lgi/common/EventTargetThread.h"
#include <errno.h>

#include <pthread.h>
#ifndef _PTHREAD_H
#error "Pthreads not included"
#endif

OsThreadId LCurrentThreadId()
{
	uint64_t tid = LThread::InvalidId;
	pthread_threadid_np(NULL, &tid);
	return tid;
}

////////////////////////////////////////////////////////////////////////////
void *ThreadEntryPoint(void *i)
{
	if (i)
	{
		LThread *Thread = (LThread*) i;
		
		Thread->ThreadId = LCurrentThreadId();
		
		// Make sure we have finished executing the setup
		while (Thread->State == LThread::THREAD_INIT)
			LSleep(1);
		
		pthread_detach(Thread->hThread);
		
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

LThread::LThread(const char *name, int viewHnd)
{
	State = THREAD_INIT;
	ReturnValue = -1;
	hThread = InvalidHandle;
	ThreadId = InvalidId;
	ViewHandle = viewHnd;
	DeleteOnExit = false;
	Priority = ThreadPriorityNormal;
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
	return State == THREAD_EXITED;
}

void LThread::Run()
{
	if (State == THREAD_EXITED &&
		hThread)
	{
		pthread_join(hThread, NULL);
		hThread = NULL;
	}

	if (!hThread)
	{
		State = THREAD_INIT;
		
		static int Creates = 0;
		int e;
		if (!(e = pthread_create(&hThread, NULL, ThreadEntryPoint, (void*)this)))
		{
			State = THREAD_RUNNING;
			Creates++;
			
			if (Priority != ThreadPriorityNormal)
			{
				int policy;
				sched_param param;
				e = pthread_getschedparam(hThread, &policy, &param);
				int min_pri = sched_get_priority_min(policy);
				int max_pri = sched_get_priority_max(policy);
				switch (Priority)
				{
					case ThreadPriorityIdle:
						param.sched_priority = min_pri;
						break;
					case ThreadPriorityNormal:
						break;
					case ThreadPriorityHigh:
						param.sched_priority = max_pri;
						break;
					case ThreadPriorityRealtime:
						param.sched_priority = max_pri;
						break;
				}
				e = pthread_setschedparam(hThread, policy, &param);
			}
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
			printf(	"%s,%i - pthread_create failed with the error %i (%s) (After %i creates)\n",
				   _FL, e, Err, Creates);
			
			State = THREAD_EXITED;
		}
	}
}

void LThread::Terminate()
{
	if (hThread &&
		pthread_cancel(hThread) == 0)
	{
		State = THREAD_EXITED;
		hThread = NULL;
	}
}

int LThread::Main()
{
	return 0;
}

