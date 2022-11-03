#ifndef _GMEM_POOL_H_
#define _GMEM_POOL_H_

#include "lgi/common/Array.h"
#include "lgi/common/LgiInterfaces.h"

class LMemoryPool : public LMemoryPoolI
{
	struct Block
	{
		size_t Used, Len;
		char *Ptr;

		Block()
		{
			Used = Len = 0;
			Ptr = NULL;
		}

		~Block()
		{
			delete [] Ptr;
		}

		bool Has(size_t Bytes)
		{
			return Used + Bytes <= Len;
		}
	};

	size_t BlockSize;
	LArray<Block> Mem;

public:
	LMemoryPool(int block_size = 32 << 10)
	{
		BlockSize = block_size;
	}

	void *Alloc(size_t Size)
	{
		Block *b = 0;

		if (Mem.Length() == 0 || !Mem.Last().Has(Size))
		{
			// New block
			b = &Mem.New();
			b->Len = BlockSize;
			b->Used = 0;
			b->Ptr = new char[b->Len];
		}
		else b = &Mem.Last();

		char *p = b->Ptr + b->Used;
		b->Used += Size;

		return p;
	}
	
	void Free(void *Ptr)
	{
	}
	
	void Empty()
	{
		Mem.Length(0);
	}
	
	char *Strdup(const char *s, ssize_t len = -1)
	{
		if (!s)
			return NULL;
		if (len < 0)
			len = strlen(s);
		char *n = (char*)Alloc(len + 1);
		if (!n)
			return NULL;
		memcpy(n, s, len);
		n[len] = 0;
		return n;
	}

	char16 *StrdupW(const char16 *s, ssize_t len = -1)
	{
		if (!s)
			return NULL;
		if (len < 0)
			len = StrlenW(s);
		size_t bytes = len * sizeof(char16);
		char16 *n = (char16*)Alloc(bytes + sizeof(char16));
		if (!n)
			return NULL;
		memcpy(n, s, bytes);
		n[len] = 0;
		return n;
	}
};

#endif
