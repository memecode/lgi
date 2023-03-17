/// \file
/// \author Matthew Allen
/// \brief Growable, type-safe array.
#pragma once 

#include <stdlib.h>
#include <assert.h>
#include <cstdlib>
#include <initializer_list>
#include <functional>

#define GARRAY_MIN_SIZE			16

#if defined(LGI_CHECK_MALLOC) && !defined(LGI_MEM_DEBUG)
#error "Include GMem.h first"
#endif

#if defined(PLATFORM_MINGW)
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _QSORT_S_DEFINED
#define _QSORT_S_DEFINED
_CRTIMP void __cdecl qsort_s(void *_Base,
							size_t _NumOfElements,
							size_t _SizeOfElements,
							int (__cdecl *_PtFuncCompare)(void *,const void *,const void *),
							void *_Context);
#endif
#ifdef __cplusplus
}
#endif
#endif

#include "lgi/common/Range.h"

/// \brief Growable type-safe array.
/// \ingroup Base
///
/// You can store simple objects inline in this array, but all their contents are initialized 
/// to the octet 0x00. Which limits use to objects that don't have a virtual table and
/// don't need construction (constructors are not called).
///
/// The objects are copied around during use so you can't assume their pointer
/// will remain the same over time either. However when objects are deleted from the
/// array their destructors WILL be called. This allows you to have simple objects that
/// have dynamically allocated pointers that are freed properly. A good example of this
/// type of object is the LVariant or LAutoString class.
///
/// If you want to store objects with a virtual table, or that need their constructor
/// to be called then you should create the LArray with pointers to the objects instead
/// of inline objects. And to clean up the memory you can call LArray::DeleteObjects or
/// LArray::DeleteArrays.
template <class Type>
class LArray
{
	Type *p;
	size_t len;
	size_t alloc;

#ifdef _DEBUG
public:
	size_t GetAlloc() { return alloc; }
#endif

protected:
	bool fixed = false;
	bool warnResize = true;

public:
	typedef Type ItemType;

	/// Constructor
	LArray(size_t PreAlloc = 0)
	{
		p = 0;
		alloc = len = PreAlloc;
		fixed = false;
		if (alloc)
		{
			size_t Bytes = sizeof(Type) * alloc;
			p = (Type*) malloc(Bytes);
			if (p)
			{
				memset(p, 0, Bytes);
			}
			else
			{
				alloc = len = 0;
			}
		}
	}

	LArray(std::initializer_list<Type> il)
	{
		p = NULL;
		alloc = len = 0;
		fixed = false;
		if (Length(il.size()))
		{
			size_t n = 0;
			for (auto i: il)
				p[n++] = i;
		}
	}

	LArray(const LArray<Type> &c)
	{
		p = 0;
		alloc = len = 0;
		fixed = false;
		*this = c;
	}

	/// Destructor	
	~LArray()
	{
		Length(0);
	}
	
	/// This frees the memory without calling the destructor of the elements.
	/// Useful if you're storing large amounts of plain data types, like char or int.
	void Free()
	{
		if (p)
		{
			free(p);
			p = NULL;
		}
		len = alloc = 0;
	}

	/// Does a range check on a pointer...
	/// \returns true if the pointer is pointing to a valid object
	/// in this array.
	bool PtrCheck(void *Ptr)
	{
		return	p != NULL &&
				Ptr >= p &&
				Ptr < &p[len];
	}

	/// Does a range check on an index...
	bool IdxCheck(ssize_t i) const
	{
		return i >= 0 && i < (ssize_t)len;
	}
	
	/// Returns the number of used entries
	size_t Length() const
	{
		return len;
	}
	
	/// Makes the length fixed..
	void SetFixedLength(bool fix = true)
	{
		fixed = fix;
	}

	/// Emtpies the array of all objects.
	bool Empty()
	{
		return Length(0);
	}

	/// Sets the length of available entries
	bool Length(size_t i)
	{
		if (i > 0)
		{
			if (i > len && fixed)
			{
				assert(!"Attempt to enlarged fixed array.");
				return false;
			}

			size_t nalloc = alloc;
			if (i < len)
			{
			    // Shrinking
			}
			else
			{
			    // Expanding
			    nalloc = 0x10;
				while (nalloc < i)
					nalloc <<= 1;
			    assert(nalloc >= i);
			}
			
			if (nalloc != alloc)
			{
				Type *np = (Type*)malloc(sizeof(Type) * nalloc);
				if (!np)
				{
					return false;
				}

				memset(np + len, 0, (nalloc - len) * sizeof(Type));
				if (p)
				{
					// copy across common elements
					memcpy(np, p, MIN(len, i) * sizeof(Type));
					free(p);
				}
				p = np;
				alloc = nalloc;
			}

			if (i > len)
			{
				// zero new elements
				memset(p + len, 0, sizeof(Type) * (nalloc - len));
			}
			else if (i < len)
			{
				for (size_t n=i; n<len; n++)
					p[n].~Type();
			}

			len = i;
		}
		else
		{
			if (p)
			{
				size_t Length = len;
				for (size_t i=0; i<Length; i++)
					p[i].~Type();
			}
			Free();
		}

		return true;
	}

	bool operator ==(const LArray<Type> &a) const
	{
		if (Length() != a.Length())
			return false;

		for (size_t i=0; i<len; i++)
			if (p[i] != a.ItemAt(i))
				return false;

		return true;
	}

	bool operator !=(const LArray<Type> &a) const
	{
		return !(*this == a);
	}

	LArray<Type> &operator =(const LArray<Type> &a)
	{
		Length(a.Length());
		if (p && a.p)
		{
			for (size_t i=0; i<len; i++)
				p[i] = a.p[i];
		}
		return *this;
	}

	Type &First()
	{
		if (len <= 0)
		{
			LAssert(!"Must have one element.");
			static Type t;
			return t;
		}

		return p[0];
	}

	Type &Last()
	{
		if (len <= 0)
		{
			LAssert(!"Must have one element.");
			static Type t;
			return t;
		}
		
		return p[len-1];
	}

	/// This can be used instead of the [] operator in situations
	/// where the array is const.
	const Type &ItemAt(size_t i) const
	{
		if (i >= 0 && (uint32_t)i < len)
			return p[i];

		static Type t;
		return t;
	}

	// Returns the address of an item or NULL if index is out of range
	Type *AddressOf(size_t i = 0)
	{
		return i < len ? p + i : NULL;
	}

	/// \brief Returns a reference a given entry.
	///
	/// If the entry is off the end of the array and "fixed" is false,
	/// it will grow to make it valid.
	Type &operator [](size_t i)
	{
		static Type t;

		if
		(
			(fixed && (uint32_t)i >= len)
		)
		{
			if (warnResize)
				assert(!"Attempt to enlarged fixed array.");
			return t;
		}
		
		if (i >= (int)alloc)
		{
			// increase array length
			size_t nalloc = MAX(alloc, GARRAY_MIN_SIZE);
			while (nalloc <= (uint32_t)i)
			{
				nalloc <<= 1;
			}

			#if 0
			if (nalloc > 1<<30)
			{
				#if defined(_DEBUG) && defined(_MSC_VER)
				LAssert(0);
				#endif
				
				ZeroObj(t);
				return t;
			}
			#endif

			// alloc new array
			Type *np = (Type*) malloc(sizeof(Type) * nalloc);
			if (np)
			{
				// clear new cells
				memset(np + len, 0, (nalloc - len) * sizeof(Type));
				if (p)
				{
					// copy across old cells
					memcpy(np, p, len * sizeof(Type));
					
					// clear old array
					free(p);
				}
				
				// new values
				p = np;
				alloc = nalloc;
			}
			else
			{
				static Type *t = 0;
				return *t;
			}
		}

		// adjust length of the the array
		if ((uint32_t)i + 1 > len)
		{
			len = i + 1;
		}
		
		return p[i];
	}

	/// Delete all the entries as if they are pointers to objects
	void DeleteObjects()
	{
		if (len > 0)
		{
			size_t InitialLen = len;
			delete p[0];
			if (InitialLen == len)
			{
				// Non self deleting
				for (uint i=1; i<len; i++)
				{
					delete p[i];
					p[i] = 0;
				}
				Length(0);
			}
			else
			{
				// Self deleting object...
				while (len)
				{
					delete p[0];
				}
			}
		}
	}
	
	/// Delete all the entries as if they are pointers to arrays
	void DeleteArrays()
	{
		for (uint32_t i=0; i<len; i++)
		{
			DeleteArray(p[i]);
		}
		Length(0);
	}
	
	/// Find the index of entry 'n'
	ssize_t IndexOf(Type n)
	{
		for (uint i=0; i<len; i++)
		{
			if (p[i] == n) return i;
		}
		
		return -1;
	}

	/// Returns true if the item 'n' is in the array
	bool HasItem(Type n)
	{
		return IndexOf(n) >= 0;
	}

	/// Removes last element	
	bool PopLast()
	{
		if (len <= 0)
			return false;
		return Length(len - 1);
	}
	
	/// Deletes an entry
	bool DeleteAt
	(
		/// The index of the entry to delete
		size_t Index,
		/// true if the order of the array matters, otherwise false.
		bool Ordered = false
	)
	{
		if (p && Index < len)
		{
			// Delete the object
			p[Index].~Type();

			// Move the memory up
			if (Index < len - 1)
			{
				if (Ordered)
				{
					memmove(p + Index, p + Index + 1, (len - Index - 1) * sizeof(Type) );
				}
				else
				{
					p[Index] = p[len-1];
				}
			}

			// Adjust length
			len--;

			// Kill the element at the end... otherwise New() returns non-zero data.
			memset(p + len, 0, sizeof(Type));
			return true;
		}
		
		return false;
	}

	/// Deletes a range.
	/// This operation always maintains order. 
	/// \returns the number of removed elements
	ssize_t DeleteRange(LRange r)
	{
		// Truncate range to the size of this array
		if (r.Start < 0)
			r.Start = 0;
		if (r.End() >= (ssize_t)len)
			r.Len = len - r.Start;

		// Delete all the elements we are removing
		for (ssize_t i=r.Start; i<r.End(); i++)
			p[i].~Type();

		// Move the elements down
		auto Remain = len - r.End();
		if (Remain > 0)
			memmove(&p[r.Start], &p[r.End()], sizeof(Type)*Remain);

		len = r.Start + Remain;

		return r.Len;
	}

	/// Deletes the entry 'n'
	bool Delete
	(
		/// The value of the entry to delete
		Type n,
		/// true if the order of the array matters, otherwise false.
		bool Ordered = false
	)
	{
		ssize_t i = IndexOf(n);
		if (p && i >= 0)
		{
			return DeleteAt(i, Ordered);
		}
		
		return false;
	}

	/// Appends an element
	void Add
	(
		/// Item to insert
		const Type &n
	)
	{
		(*this)[len] = n;
	}

	/// Appends multiple elements
	bool Add
	(
		/// Items to insert
		const Type *s,
		/// Length of array
		ssize_t count
		
	)
	{
		if (!s || count < 1)
			return false;
			
		ssize_t i = len;
		if (!Length(len + count))
			return false;

		Type *d = p + i;
		while (count--)
		{
			*d++ = *s++;
		}

		return true;
	}

	/// Appends an array of elements
	bool Add
	(
		/// Array to insert
		const LArray<Type> &a
	)
	{
		ssize_t old = len;
		if (Length(len + a.Length()))
		{
			for (unsigned i=0; i<a.Length(); i++, old++)
				p[old] = a.ItemAt(i);
		}
		else return false;
		return true;
	}

	LArray<Type> operator +(const LArray<Type> &b)
	{
		LArray<Type> a = *this;
		a.Add(b);
		return a;
	}

	LArray<Type> &operator +=(const LArray<Type> &b)
	{
		Add(b);
		return *this;
	}

	/// Inserts an element into the array
	bool AddAt
	(
		/// Item to insert before
		size_t Index,
		/// Item to insert
		Type n
	)
	{
		// Make room
		if (!Length(len + 1))
			return false;

		if (Index < len - 1)
			// Shift elements after insert point up one
			memmove(p + Index + 1, p + Index, (len - Index - 1) * sizeof(Type) );
		else
			// Add at the end, not after the end...
			Index = len - 1;

		// Insert item
		memset(p + Index, 0, sizeof(*p));
		p[Index] = n;

		return true;
	}

	/// Inserts an array into the array at a position
	bool AddAt
	(
		/// Item to insert before
		size_t Index,
		/// Array to insert
		const LArray<Type> &a
	)
	{
		// Make room
		if (!Length(len + a.Length()))
			return false;

		if (Index < len - 1)
			// Shift elements after insert point up one
			memmove(p + Index + a.Length(), p + Index, (len - Index - a.Length()) * sizeof(Type) );
		else
			// Add at the end, not after the end...
			Index = len - 1;

		// Insert items
		memset(p + Index, 0, a.Length() * sizeof(*p));
		for (size_t i=0; i<a.Length(); i++)
			p[Index+i] = a.ItemAt(i);

		return true;
	}

	/// Sorts the array via objects '-' operator
	void Sort()
	{
		if (len <= 0)
			return;
		std::qsort(p, len, sizeof(*p), [](const void *a, const void *b)
		{
			auto B = (const Type*)b;
			auto A = (const Type*)a;
			auto r = *B - *A;
			if (r > 0)
				return 1;
			return r < 0 ? -1 : 0;
		});
	}

	/// Sorts the array via a callback.
	void Sort(std::function<int(Type*,Type*)> Compare)
	{
		#if !defined(WINDOWS) && !defined(HAIKU) && !defined(LINUX)
		#define USER_DATA_FIRST 1
		#else
		#define USER_DATA_FIRST 0
		#endif

		#if defined(WINDOWS)
		/* _ACRTIMP void __cdecl qsort_s(void*   _Base,
										rsize_t _NumOfElements,
										rsize_t _SizeOfElements,
										int (__cdecl* _PtFuncCompare)(void*, void const*, void const*),
										void*   _Context); */
		qsort_s
		#else
		qsort_r
		#endif
			(
				p,
				len,
				sizeof(Type),
				#if USER_DATA_FIRST
					&Compare,
				#endif
				#if defined(HAIKU) || defined(LINUX)
					// typedef int (*_compare_function_qsort_r)(const void*, const void*, void*);
					// extern void qsort_r(void* base, size_t numElements, size_t sizeOfElement, _compare_function_qsort_r, void* cookie);
					[](const void *a, const void *b, void *ud) -> int
				#else
					[](void *ud, const void *a, const void *b) -> int
				#endif
				{
					return (*( (std::function<int(Type*,Type*)>*)ud ))((Type*)a, (Type*)b);
				}
				#if !USER_DATA_FIRST
					, &Compare
				#endif
			);
	}

	/// \returns a reference to a new object on the end of the array
	///
	/// Never assign this to an existing variable. e.g:
	///		LArray<MyObject> a;
	///		MyObject &o = a.New();
	///		o.Type = something;
	///		o = a.New();
	///		o.Type = something else;
	///
	/// This causes the first object to be overwritten with a blank copy.
	Type &New()
	{
		return (*this)[len];
	}

	/// Returns the memory held by the array and sets itself to empty
	Type *Release()
	{
		Type *Ptr = p;
		p = 0;
		len = alloc = 0;
		return Ptr;
	}

	void Swap(LArray<Type> &other)
	{
		LSwap(p, other.p);
		LSwap(len, other.len);
		LSwap(alloc, other.alloc);
	}

	/// Swaps a range of elements between this array and 'b'
	bool SwapRange
	(
		// The range of 'this' to swap out
		LRange aRange,
		// The other array to swap with
		LArray<Type> &b,
		// The range of 'b' to swap with this array
		LRange bRange
	)
	{
		LArray<Type> Tmp;

		// Store entries in this that will be swapped
		Tmp.Add(AddressOf(aRange.Start), aRange.Len);

		// Copy b's range into this
		ssize_t Common = MIN(bRange.Len, aRange.Len);
		ssize_t aIdx = aRange.Start;
		ssize_t bIdx = bRange.Start;
		for (int i=0; i<Common; i++) // First the common items
			(*this)[aIdx++] = b[bIdx++];
		if (aRange.Len < bRange.Len)
		{
			// Grow range to fit 'b'
			ssize_t Items = bRange.Len - aRange.Len;
			for (ssize_t i=0; i<Items; i++)
				if (!AddAt(aIdx++, b[bIdx++]))
					return false;
		}
		else if (aRange.Len > bRange.Len)
		{
			// Shrink the range in this to fit 'b'
			ssize_t Del = aRange.Len - bRange.Len;
			for (ssize_t i=0; i<Del; i++)
				if (!DeleteAt(aIdx, true))
					return false;
		}

		// Now copy Tmp into b
		int TmpIdx = 0;
		bIdx = bRange.Start;
		for (TmpIdx=0; TmpIdx<Common; TmpIdx++) // First the common items.
			b[bIdx++] = Tmp[TmpIdx];
		if (aRange.Len > bRange.Len)
		{
			// Grow range to fit this
			ssize_t Add = aRange.Len - bRange.Len;
			for (ssize_t i=0; i<Add; i++)
				if (!b.AddAt(bIdx++, Tmp[TmpIdx++]))
					return false;
		}
		else if (aRange.Len < bRange.Len)
		{
			// Shrink the range in this to fit 'b'
			ssize_t Del = bRange.Len - aRange.Len;
			for (ssize_t i=0; i<Del; i++)
				if (!b.DeleteAt(bIdx, true))
					return false;
		}

		return true;
	}

	template <class T>
	class Iter
	{
		friend class ::LArray<T>;
		ssize_t i;
		char each_dir;
		LArray<T> *a;

	public:
		Iter(LArray<T> *arr) // 'End' constructor
		{
			i = -1;
			a = arr;
			each_dir = 0;
		}

		Iter(LArray<T> *arr, size_t pos)
		{
			i = pos;
			a = arr;
			each_dir = 0;
		}

		bool operator ==(const Iter<T> &it) const
		{
			int x = (int)In() + (int)it.In();
			if (x == 2)
				return (a == it.a) && (i == it.i);
			return x == 0;
		}

		bool operator !=(const Iter<T> &it) const
		{
			return !(*this == it);
		}

		operator bool() const
		{
			return In();
		}
		
		bool In() const
		{
			return i >= 0 && i < (ssize_t)a->Length();
		}
		
		bool End() const
		{
			return i < 0 || i >= a->Length();
		}
		
		T &operator *() { return (*a)[i]; }
		Iter<T> &operator ++() { i++; return *this; }
		Iter<T> &operator --() { i--; return *this; }
		Iter<T> &operator ++(int) { i++; return *this; }
		Iter<T> &operator --(int) { i--; return *this; }
	};

	typedef Iter<Type> I;
	I begin(size_t start = 0) { return I(this, start); }
	I rbegin() { return I(this, len-1); }
	I end() { return I(this); }
	bool Delete(I &It)
	{
		return DeleteAt(It.i, true);
	}

	LArray<Type> Slice(ssize_t Start, ssize_t End = -1)
	{
		LArray<Type> a;

		// Range limit the inputs...
		if (Start < 0) Start = len + Start;
		if (Start > (ssize_t)len) Start = len;
		
		if (End < 0) End = len + End + 1;
		if (End > (ssize_t)len) End = len;

		if (End > Start)
		{
			a.Length(End - Start);
			for (size_t i=0; i<a.Length(); i++)
				a[i] = p[Start + i];
		}

		return a;
	}

	LArray<Type> Reverse()
	{
		LArray<Type> r;

		r.Length(len);
		for (size_t i=0, k=len-1; i<len; i++, k--)
			r.p[i] = p[k];

		return r;
	}

	/// This requires a sorted list...
	Type BinarySearch(std::function<int(Type&)> Compare)
	{
		static Type Empty;
		if (!Compare)
			return Empty;
		
		for (size_t s = 0, e = len - 1; s <= e; )
		{
			if (e - s < 2)
			{
				if (Compare(p[s]) == 0)
					return p[s];
				if (e > s && Compare(p[e]) == 0)
					return p[e];
				break; // Not found
			}

			size_t mid = s + ((e - s) >> 1);
			int result = Compare(p[mid]);
			if (result == 0)
				return p[mid];

			if (result < 0)
				s = mid + 1; // search after the mid point
			else
				e = mid - 1; // search before
		}

		return Empty;
	}
};

