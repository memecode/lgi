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

	bool PostEvent(int Hnd, int Cmd, LMessage::Param a = 0, LMessage::Param b = 0)
	{
		if (!Hnd)
			return false;
		if (!Lock(_FL))
			return false;

		auto *s = ToPtr.Find(Hnd);
		bool Status = s ? s->PostEvent(Cmd, a, b) : false;
		#if 0 // _DEBUG
		else
			// This is not fatal, but we might want to know about it in DEBUG builds:
			LgiTrace("%s:%i - Sink associated with handle '%i' doesn't exist.\n", _FL, Hnd);
		#endif

		Unlock();
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
	int Handle;
	LEventSinkMap *Map;

public:
	LMappedEventSink()
	{
		Map = NULL;
		Handle = 0;
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
		OsThreadId thread = GetCurrentThreadId();
		LString s;
		s.Printf("%s.%s.%i.%i", obj.Get(), desc, process, thread);
		return s;
	}

protected:
	int PostTimeout;
	size_t Processing;
	uint32_t TimerMs; // Milliseconds between timer ticks.
	uint64 TimerTs; // Time for next tick

public:
	LEventTargetThread(LString Name, bool RunImmediately = true) :
		LThread(Name + ".Thread"),
		LMutex(Name + ".Mutex"),
		Event(ProcessName(Name, "Event"))
	{
		PostTimeout = 1000;
		Processing = 0;
		TimerMs = 0;
		TimerTs = 0;

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
		if (!IsCancelled())
		{
			// We can't be locked here, because LEventTargetThread::Main needs
			// to lock to check for messages...
			Cancel();
			
			if (GetCurrentThreadId() == LThread::GetId())
			{
				// Being called from within the thread, in which case we can't signal 
				// the event because we'll be stuck in this loop and not waitin on it.
				#ifdef _DEBUG
				LgiTrace("%s:%i - EndThread called from inside thread.\n", _FL);
				#endif
			}
			else
			{			
				Event.Signal();
				
				uint64 Start = LCurrentTime();
				
				while (!IsExited())
				{
					LSleep(10);
					
					uint64 Now = LCurrentTime();
					if (Now - Start > 2000)
					{
						#ifdef LINUX
						int val = 1111;
						int r = sem_getvalue(Event.Handle(), &val);

						printf("%s:%i - EndThread() hung waiting for %s to exit (caller.thread=%i, worker.thread=%i, event=%i, r=%i, val=%i).\n",
							_FL, LThread::GetName(),
							GetCurrentThreadId(),
							GetId(),
							Event.Handle(),
							r,
							val);
						#else
						printf("%s:%i - EndThread() hung waiting for %s to exit (caller.thread=0x%x, worker.thread=0x%x, event=%p).\n",
							_FL, LThread::GetName(),
							(int)GetCurrentThreadId(),
							(int)GetId(),
							(void*)(ssize_t)Event.Handle());
						#endif
						
						Start = Now;
					}
				}
			}
		}
	}

	size_t GetQueueSize()
	{
		return Msgs.Length() + Processing;
	}

	bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0)
	{
		if (IsCancelled())
			return false;
		if (!Lock(_FL))
			return false;
		
		Msgs.Add(new LMessage(Cmd, a, b));
		Unlock();
		
		// printf("%x: PostEvent and sig %i\n", GetCurrentThreadId(), (int)Msgs.Length());
		return Event.Signal();
	}
	
	int Main()
	{
		while (!IsCancelled())
		{
			int WaitLength = -1;
			if (TimerTs != 0)
			{
				uint64 Now = LCurrentTime();
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
						WaitLength = (int) TimerTs;
					}
					else WaitLength = -1;
				}
			}
			
			LThreadEvent::WaitStatus s = Event.Wait(WaitLength);
			if (s == LThreadEvent::WaitSignaled)
			{
				LArray<LMessage*> m;
				if (Lock(_FL))
				{
					if (Msgs.Length())
					{
						m = Msgs;
						Msgs.Length(0);
					}
					Unlock();
				}

				Processing = m.Length();
				for (unsigned i=0; !IsCancelled() && i < m.Length(); i++)
				{
					Processing--;
					/*
					printf("%x: Processing=%i of %i\n",
						GetCurrentThreadId(),
						(int)Processing, (int)m.Length());
					*/
					OnEvent(m[i]);
				}
				// printf("%x: Done Processing, %i to go\n", GetCurrentThreadId(), (int)Msgs.Length());
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
			if (LCurrentTime() - Start >= PostTimeout) break;
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

	template<typename T>
	bool ReceiveA(LAutoPtr<T> &Obj, LMessage *m)
	{
		return Obj.Reset((T*)m->A());
	}

	template<typename T>
	bool ReceiveB(LAutoPtr<T> &Obj, LMessage *m)
	{
		return Obj.Reset((T*)m->B());
	}
};



#endif
