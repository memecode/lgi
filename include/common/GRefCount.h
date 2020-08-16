#ifndef _REF_COUNT_H_
#define _REF_COUNT_H_

class GRefCount
{
	#if defined(_WIN32)
	LONG _Count;
	#else
	int _Count;
	#endif

public:
    #ifdef _DEBUG
    int _GetCount() { return _Count; }
    #endif

	GRefCount()
	{
		_Count = 0;
	}

	virtual ~GRefCount()
	{
		LgiAssert(_Count == 0);
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
	}

	virtual bool DecRef()
	{
		LgiAssert(_Count > 0);
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

public:
	GAutoRefPtr(T *ptr = 0)
	{
		Ptr = ptr;
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
		LgiAssert(Ptr != NULL);
		return Ptr;
	}

	void Empty()
	{
		if (Ptr)
		{
			Ptr->DecRef();
			Ptr = NULL;
		}
	}

	GAutoRefPtr<T> &operator =(T *p)
	{
		Empty();
		Ptr = p;
		if (Ptr)
			Ptr->AddRef();
		return *this;
	}

	GAutoRefPtr &operator =(const GAutoRefPtr<T> &refptr)
	{
		Empty();
		Ptr = refptr.Ptr;
		if (Ptr)
			Ptr->AddRef();
		return *this;
	}

};

#endif