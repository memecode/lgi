#ifndef _GEVENTTARGETTHREAD_H_
#define _GEVENTTARGETTHREAD_H_

#include "lgi/common/Thread.h"
#include "lgi/common/Mutex.h"
#include "lgi/common/ThreadEvent.h"
#include "lgi/common/Cancel.h"
#include "lgi/common/HashTable.h"

#define PostThreadEvent LEventSinkMap::Dispatch.PostEvent

class LgiClass LEventSinkMap : public LMutex
{
protected:
	LHashTbl<IntKey<int>,LEventSinkI*> ToPtr;
	LHashTbl<PtrKey<void*>,int> ToHnd;

public:
	static LEventSinkMap Dispatch;

	LEventSinkMap(int SizeHint = 0) :
        LMutex("LEventSinkMap"),
		ToPtr(SizeHint),
		ToHnd(SizeHint)
	{
	}

	virtual ~LEventSinkMap()
	{
	}

	int AddSink(LEventSinkI *s)
	{
		if (!s || !Lock(_FL))
			return ToPtr.GetNullKey();

		// Find free handle...
		int Hnd;
		while (ToPtr.Find(Hnd = LRand(10000) + 1))
			;

		// Add the new sink
		ToPtr.Add(Hnd, s);
		ToHnd.Add(s, Hnd);

		Unlock();

		return Hnd;
	}

	bool RemoveSink(LEventSinkI *s)
	{
		if (!s || !Lock(_FL))
			return false;

		bool Status = false;
		int Hnd = ToHnd.Find(s);
		if (Hnd > 0)
		{
			Status |= ToHnd.Delete(s);
			Status &= ToPtr.Delete(Hnd);
		}
		else
			LAssert(!"Not a member of this sink.");

		Unlock();
		return Status;
	}

	bool RemoveSink(int Hnd)
	{
		if (!Hnd || !Lock(_FL))
			return false;

		bool Status = false;
		void *Ptr = ToPtr.Find(Hnd);
		if (Ptr)
		{
			Status |= ToHnd.Delete(Ptr);
			Status &= ToPtr.Delete(Hnd);
		}
		else
			LAssert(!"Not a member of this sink.");

		Unlock();
		return Status;
	}

	bool IsSink(int Hnd)
	{
		if (!Lock(_FL))
			return false;

		auto status = ToPtr.Find(Hnd) != NULL;
		Unlock();
		return status;
	}

	bool PostEvent(int Hnd, int Cmd, LMessage::Param a = 0, LMessage::Param b = 0)
	{
		if (!Hnd)
			return false;

		/*

		This can deadlock on Haiku:
		
		- If the target LEventSinkI is a view processing a message, and therefor locked.
		- And it's also locking this LEventSinkMap to try and send a message to it.
		- And the LEventTargetThread thread is trying to send a message back to the view.
		
		Fix is to not keep this locked, but to periodically try to lock and then send.
		If it fails, wait and then try again. While waiting, release the lock.

		*/

		bool Done = false, Status = false;
		while (!Done)
		{
			if (LockWithTimeout(10, _FL))
			{
				auto *s = ToPtr.Find(Hnd);
				Status = s ? s->PostEvent(Cmd, a, b) : false;
				#if 0 // _DEBUG
				else
					// This is not fatal, but we might want to know about it in DEBUG builds:
					LgiTrace("%s:%i - Sink associated with handle '%i' doesn't exist.\n", _FL, Hnd);
				#endif
				Done = true;

				Unlock();
				break;
			}
			
			LSleep(10);
		}

		return Status;
	}
	
	bool CancelThread(int Hnd)
	{
		if (!Hnd)
			return false;
		
		if (!Lock(_FL))
			return false;
		
		LEventSinkI *s = (LEventSinkI*)ToPtr.Find(Hnd);
		bool Status = false;
		if (s)
		{
			LCancel *c = dynamic_cast<LCancel*>(s);
			if (c)
			{
				Status = c->Cancel(true);
			}
			else
			{
				LgiTrace("%s:%i - LEventSinkI is not an LCancel object.\n", _FL);
			}
		}
		
		Unlock();
		return Status;
	}
};

class LgiClass LMappedEventSink : public LEventSinkI
{
protected:
	int Handle = 0;
	LEventSinkMap *Map = NULL;

public:
	LMappedEventSink()
	{
		SetMap(&LEventSinkMap::Dispatch);
	}

	virtual ~LMappedEventSink()
	{
		SetMap(NULL);
	}

	bool SetMap(LEventSinkMap *m)
	{
		if (Map)
		{
			if (!Map->RemoveSink(this))
				return false;
			Map = 0;
			Handle = 0;
		}
		Map = m;
		if (Map)
		{
			Handle = Map->AddSink(this);
			return Handle > 0;
		}
		return true;
	}

	int GetHandle()
	{
		LAssert(Handle != 0);
		return Handle;
	}
};

/// This class is a worker thread that accepts messages on it's LEventSinkI interface.
/// To use, sub class and implement the OnEvent handler.
class LgiClass LEventTargetThread :
	public LThread,
	public LMutex,
	public LMappedEventSink,
	public LCancel,
	public LEventTargetI // Sub-class has to implement OnEvent
{
	LArray<LMessage*> Msgs;
	LThreadEvent Event;

	// This makes the event name unique on windows to 
	// prevent multiple instances clashing.
	LString ProcessName(LString obj, const char *desc)
	{
		OsProcessId process = LGetCurrentProcess();
		OsThreadId thread = LCurrentThreadId();
		LString s;
		s.Printf("%s.%s.%i.%i", obj.Get(), desc, process, thread);
		return s;
	}

protected:
	int PostTimeout = 1000;
	size_t Processing = 0;
	uint32_t TimerMs = 0; // Milliseconds between timer ticks.
	uint64 TimerTs = 0; // Time for next tick

public:
	LEventTargetThread(LString Name, bool RunImmediately = true) :
		LThread(Name + ".Th"),
		LMutex(Name + ".Mutex"),
		Event(ProcessName(Name, "Event"))
	{
		if (RunImmediately)
			Run();
	}
	
	virtual ~LEventTargetThread()
	{
		EndThread();
	}
	
	/// Set or clear a timer. Every time the timer expires, the function
	/// OnPulse is called. Until SetPulse() is called.
	bool SetPulse(uint32_t Ms = 0)
	{
		TimerMs = Ms;
		TimerTs = Ms ? LCurrentTime() + Ms : 0;
		return Event.Signal();
	}
	
	/// Called roughly every 'TimerMs' milliseconds.
	/// Be aware however that OnPulse is called from the worker thread, not your main
	/// GUI thread. So best to send a message or something thread safe.
	virtual void OnPulse() {}

	void EndThread()
	{
		// We can't be locked here, because LEventTargetThread::Main needs
		// to lock to check for messages...
		Cancel();
			
		if (LCurrentThreadId() == LThread::GetId())
		{
			// Being called from within the thread, in which case we can't signal 
			// the event because we'll be stuck in this loop and not waitin on it.
			//
			// Also there is no waiting to do, the message loop will exit because
			// cancel was called.
			#ifdef _DEBUG
			LgiTrace("%s:%i - EndThread called from inside thread.\n", _FL);
			#endif
			return;
		}
		
		// Signal the event to wake up the thread:
		Event.Signal();
		
		// Wait for it to exit:
		uint64 Start = LCurrentTime();				
		while (!IsExited())
		{
			LSleep(10);
					
			auto Now = LCurrentTime();
			if (Now - Start > 2000)
			{
				#ifdef LINUX
					int val = 1111;
					int r = sem_getvalue(Event.Handle(), &val);

					printf("%s:%i - EndThread() hung waiting for %s to exit (caller.thread=%i, worker.thread=%i, event=%p, r=%i, val=%i).\n",
						_FL, LThread::GetName(),
						LCurrentThreadId(),
						GetId(),
						Event.Handle(),
						r,
						val);
				#else
					printf("%s:%i - EndThread() hung waiting for %s to exit (caller.thread=0x%x, worker.thread=0x%x, event=%p).\n",
						_FL, LThread::GetName(),
						(int)LCurrentThreadId(),
						(int)GetId(),
						(void*)(ssize_t)Event.Handle());
				#endif
						
				Start = Now;
			}
		}
	}

	size_t GetQueueSize()
	{
		return Msgs.Length() + Processing;
	}

	bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t timeout = -1)
	{
		if (IsCancelled())
			return false;
		if (!Lock(_FL))
			return false;
		
		auto Msg = new LMessage(Cmd, a, b);
		if (Msg)
			Msgs.Add(Msg);
		Unlock();
		
		return Event.Signal();
	}
	
	int Main()
	{
		while (!IsCancelled())
		{
			int WaitLength = 100;
			if (TimerTs)
			{
				auto Now = LCurrentTime();
				if (TimerTs > Now)
				{
					WaitLength = (int) (TimerTs - Now);
				}
				else
				{
					OnPulse();
					if (TimerMs)
					{
						TimerTs = Now + TimerMs;
						WaitLength = TimerMs;
					}
				}
			}
			
			LThreadEvent::WaitStatus s = Event.Wait(WaitLength);
			if (s == LThreadEvent::WaitSignaled)
			{
				LArray<LMessage*> m;
				if (Lock(_FL))
				{
					if (Msgs.Length())
						m.Swap(Msgs);
					Unlock();
				}

				Processing = m.Length();
				for (unsigned i=0; !IsCancelled() && i < m.Length(); i++)
				{
					Processing--;
					auto Msg = m[i];
					if (Msg)
						OnEvent(Msg);
					else
						LAssert(!"NULL Msg?");
				}
				m.DeleteObjects();
			}
			else if (s == LThreadEvent::WaitError)
			{
				LgiTrace("%s:%i - Event.Wait failed.\n", _FL);
				break;
			}
		}
		
		return 0;
	}

	template<typename T>
	bool PostObject(int Hnd, int Cmd, LAutoPtr<T> A)
	{
		uint64 Start = LCurrentTime();
		bool Status;
		while (!(Status = LEventSinkMap::Dispatch.PostEvent(Hnd, Cmd, (LMessage::Param) A.Get())))
		{
			LSleep(2);
			if (LCurrentTime() - Start >= PostTimeout)
				break;
		}
		if (Status)
			A.Release();
		return Status;
	}

	template<typename T>
	bool PostObject(int Hnd, int Cmd, LMessage::Param A, LAutoPtr<T> B)
	{
		uint64 Start = LCurrentTime();
		bool Status;
		while (!(Status = LEventSinkMap::Dispatch.PostEvent(Hnd, Cmd, A, (LMessage::Param) B.Get())))
		{
			LSleep(2);
			if (LCurrentTime() - Start >= PostTimeout) break;
		}
		if (Status)
			B.Release();
		return Status;
	}

	template<typename T>
	bool PostObject(int Hnd, int Cmd, LAutoPtr<T> A, LAutoPtr<T> B)
	{
		uint64 Start = LCurrentTime();
		bool Status;
		while (!(Status = LEventSinkMap::Dispatch.PostEvent(Hnd, Cmd, (LMessage::Param) A.Get(), (LMessage::Param) B.Get())))
		{
			LSleep(2);
			if (LCurrentTime() - Start >= PostTimeout) break;
		}
		if (Status)
		{
			A.Release();
			B.Release();
		}
		return Status;
	}

	/* Use LMessage::AutoA
	template<typename T>
	bool ReceiveA(LAutoPtr<T> &Obj, LMessage *m)
	{
		return Obj.Reset((T*)m->A());
	}
	*/

	
	/* Use LMessage::AutoB
	template<typename T>
	bool ReceiveB(LAutoPtr<T> &Obj, LMessage *m)
	{
		return Obj.Reset((T*)m->B());
	}
	*/
};



#endif
