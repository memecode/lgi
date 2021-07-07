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

template<class X, bool Arr = false>
class LAutoPtr
{
	X *Ptr;

public:
 	explicit LAutoPtr(X* p=0)
	{
		Ptr = p;
	}

	LAutoPtr(LAutoPtr &ap)
	{
		Ptr = ap.Release();
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
		if (Arr)
			delete [] Ptr;
		else
			delete Ptr;
		
		// This is needed to be able to use LAutoPtr inside LArray's.
		Ptr = 0;
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
		LgiAssert(Ptr != 0); 
		return Ptr;
	}

	X* Get() const
	{
		return Ptr;
	}

	X* Release()
	{
		X *p = Ptr;
		Ptr = 0;
		return p;
	}

	bool Reset(X* p=0)
	{
		if (Ptr == p)
			return Ptr != 0;

		if (Arr)
			delete [] Ptr;
		else
			delete Ptr;
		Ptr = p;
		return Ptr != 0;
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