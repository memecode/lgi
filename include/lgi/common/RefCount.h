#ifndef _REF_COUNT_H_
#define _REF_COUNT_H_

#include "lgi/common/LgiUiBase.h"

class GRefCount
{
	#if defined(_WIN32)
	LONG _Count;
	#else
	int _Count;
	#endif
	bool _DebugTrace;

public:
    #ifdef _DEBUG
    int _GetCount() { return _Count; }
    #endif

	GRefCount(bool trace = false)
	{
		_Count = 0;
		_DebugTrace = trace;
		if (_DebugTrace)
			LgiTrace("%s:%i - GRefCount.Construct=%i\n", _FL, _Count);
	}

	void SetDebug(bool b)
	{
		_DebugTrace = b;
	}

	virtual ~GRefCount()
	{
		if (_DebugTrace)
			LgiTrace("%s:%i - ~GRefCount=%i\n", _FL, _Count);
		LAssert(_Count == 0);
	}

	virtual void AddRef()
	{
		#if defined(_WIN32)
			InterlockedIncrement(&_Count);
		#elif defined(__GNUC__)
			__sync_fetch_and_add(&_Count, 1);
		#else
			#error "Impl me."
			_Count++;
		#endif
		if (_DebugTrace)
			LgiTrace("%s:%i - GRefCount.AddRef=%i\n", _FL, _Count);
	}

	virtual bool DecRef()
	{
		if (_DebugTrace)
			LgiTrace("%s:%i - GRefCount.DecRef=%i\n", _FL, _Count);
		LAssert(_Count > 0)
			;
		#if defined(_WIN32)
			if (InterlockedDecrement(&_Count) == 0)
		#elif defined(__GNUC__)
			if (__sync_sub_and_fetch(&_Count, 1) == 0)
		#else
			#error "Impl me."
			if (--_Count == 0)
		#endif
		{
			delete this;
			return true;
		}
		return false;
	}
};

template <typename T>
class GAutoRefPtr
{
	T *Ptr;
	bool Debug;

public:
	GAutoRefPtr(T *ptr = 0, bool debug = false)
	{
		Ptr = ptr;
		Debug = debug;
		if (Ptr)
			Ptr->AddRef();
	}

	~GAutoRefPtr()
	{
		Empty();
	}

	void Swap(GAutoRefPtr<T> &p)
	{
		LSwap(Ptr, p.Ptr);
	}

	operator T*()
	{
		return Ptr;
	}
	
	T *operator->() const
	{
		LAssert(Ptr != NULL);
		return Ptr;
	}

	void Empty()
	{
		if (Ptr)
		{
		    #ifdef _DEBUG
			if (Debug) printf("GAutoRefPtr.Empty %p %i->%i\n", Ptr, Ptr->_GetCount(), Ptr->_GetCount()-1);
			#endif
			Ptr->DecRef();
			Ptr = NULL;
		}
	}

	GAutoRefPtr<T> &operator =(T *p)
	{
		Empty();
		Ptr = p;
		if (Ptr)
		{
		    #ifdef _DEBUG
			if (Debug) printf("GAutoRefPtr.Assign %p %i->%i\n", Ptr, Ptr->_GetCount(), Ptr->_GetCount()+1);
			#endif
			Ptr->AddRef();
		}
		return *this;
	}

	GAutoRefPtr &operator =(const GAutoRefPtr<T> &refptr)
	{
		Empty();
		Ptr = refptr.Ptr;
		Debug = refptr.Debug;
		if (Ptr)
		{
		    #ifdef _DEBUG
			if (Debug) printf("GAutoRefPtr.ObjAssign %p %i->%i\n", Ptr, Ptr->_GetCount(), Ptr->_GetCount()+1);
			#endif
			Ptr->AddRef();
		}
		return *this;
	}

};

#endif