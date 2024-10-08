#ifndef _GAUTOPTR_H_
#define _GAUTOPTR_H_

#ifdef __cplusplus

template<class X>
class LAutoPtrRef
{
	typedef LAutoPtrRef<X> Self;
	Self& operator = (Self const&) {}

public:    
	X *Ptr;

	explicit
	LAutoPtrRef(X *p)
	{
		Ptr = p;
	}
};

template<class X, bool Arr = false, bool IsHeap = false>
class LAutoPtr
{
	X *Ptr;

public:
 	explicit LAutoPtr(X* p=NULL)
	{
		Ptr = p;
	}

	LAutoPtr(LAutoPtr &ap)
	{
		Ptr = ap.Release();
	}

	// Move constructor
	LAutoPtr(LAutoPtr &&from) noexcept
	{
		Ptr = from.Release();
	}
	
	LAutoPtr(LAutoPtrRef<X> apr)
	{
		Ptr = apr.Ptr;
	}

	LAutoPtr& operator=(LAutoPtr ap)
	{
		Reset(ap.Release());
		return *this;
	}
	
	LAutoPtr& operator=(LAutoPtrRef<X> ap)
	{
		if (ap.Ptr != Ptr)
		{
			Reset(ap.Ptr);
		}
		return *this;
	}

	~LAutoPtr()
	{
		if (IsHeap)
			free((void*) Ptr);
		else if (Arr)
			delete [] Ptr;
		else
			delete Ptr;
		
		// This is needed to be able to use LAutoPtr inside LArray's.
		Ptr = NULL;
	}

	void Swap(LAutoPtr<X,Arr> &p)
	{
		LSwap(Ptr, p.Ptr);
	}

	operator X*() const
	{
		return Ptr;
	}

	X* operator->() const
	{
		LAssert(Ptr != NULL);
		return Ptr;
	}

	X* Get() const
	{
		return Ptr;
	}

	X* Release()
	{
		X *p = Ptr;
		Ptr = NULL;
		return p;
	}

	bool Reset(X *p = NULL)
	{
		if (Ptr == p)
			return Ptr != NULL;

		if (IsHeap)
			free(Ptr);
		else if (Arr)
			delete [] Ptr;
		else
			delete Ptr;
		Ptr = p;
		
		return Ptr != NULL;
	}

	template<class Y>
	operator LAutoPtrRef<Y>()
	{
		return LAutoPtrRef<Y>(Release());
	}

	template<class Y>
	operator LAutoPtr<Y>()
	{
		return LAutoPtr<Y>(Release());
	}
};

typedef LAutoPtr<char, true> LAutoString;
typedef LAutoPtr<char16, true> LAutoWString;

#endif // __cplusplus

#endif // _GAUTOPTR_H_