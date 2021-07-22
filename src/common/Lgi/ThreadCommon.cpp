#include "lgi/common/Lgi.h"
#include "lgi/common/EventTargetThread.h"

LEventSinkMap LEventSinkMap::Dispatch(128);

//////////////////////////////////////////////////////////////////////////////////
void LThread::WaitForExit(int WarnAfterMs)
{
	auto Start = LCurrentTime();
	while (!IsExited())
	{
		if ((LCurrentTime() - Start) >= WarnAfterMs)
			LAssert(!"Thread hasn't exited.");

		LSleep(10);
	}
}

//////////////////////////////////////////////////////////////////////////////////
LThreadTarget::LThreadTarget()
{
	Worker = 0;
}

void LThreadTarget::SetWorker(LThreadWorker *w)
{
	if (w && Lock(_FL))
	{
		Worker = w;
		Unlock();
	}
}

void LThreadTarget::Detach()
{
	if (Lock(_FL))
	{
		Worker = 0;
		Unlock();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LThreadWorker::LThreadWorker(LThreadTarget *First, const char *ThreadName) :
	LThread(ThreadName),
	LMutex("LThreadWorker")
{
	Loop = false;
	if (First)
		Attach(First);
}

LThreadWorker::~LThreadWorker()
{
	Stop();
}

void LThreadWorker::Stop()
{
	if (Loop)
	{
		Loop = false;
		while (!IsExited())
			LSleep(1);
		if (Lock(_FL))
		{
			for (uint32_t i=0; i<Owners.Length(); i++)
				Owners[i]->Detach();
			Unlock();
		}
	}
}

void LThreadWorker::Attach(LThreadTarget *o)
{
	LMutex::Auto a(this, _FL);
	if (!Owners.HasItem(o))
	{
		LAssert(o->Worker == this);
		Owners.Add(o);
		if (Owners.Length() == 1)
		{
			Loop = true;
			Run();
		}
	}
}

void LThreadWorker::Detach(LThreadTarget *o)
{
	LMutex::Auto a(this, _FL);
	LAssert(Owners.HasItem(o));
	Owners.Delete(o);
}

void LThreadWorker::AddJob(LThreadJob *j)
{
	if (Lock(_FL))
	{
		Jobs.Add(j);

		if (!Owners.HasItem(j->Owner))
			Attach(j->Owner);

		Unlock();
	}
}

void LThreadWorker::DoJob(LThreadJob *j)
{
	j->Do();
}

int LThreadWorker::Main()
{
	while (Loop)
	{
		LAutoPtr<LThreadJob> j;
		if (Lock(_FL))
		{
			if (Jobs.Length())
			{
				j.Reset(Jobs[0]);
				Jobs.DeleteAt(0, true);
			}
			Unlock();
		}
		if (j)
		{
			DoJob(j);
			if (Lock(_FL))
			{
				if (Owners.IndexOf(j->Owner) < 0)
				{
					// Owner is gone already... delete the job.
					j.Reset();
				}
				else
				{
					// Pass job back to owner.
					// This needs to be in the lock so that the owner can't delete itself
					// from the owner list...
					j->Owner->OnDone(j);
				}
				Unlock();
			}
		}
		else LSleep(50);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
LThreadOwner::~LThreadOwner()
{
	if (Lock(_FL))
	{
		if (Worker)
			Worker->Detach(this);
		Unlock();
	}
}
