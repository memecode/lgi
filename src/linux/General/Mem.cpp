/*hdr
**      FILE:           Memory.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           10/4/98
**      DESCRIPTION:    Memory subsystem
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>

#include "lgi/common/LgiDefs.h"
#include "LgiOsDefs.h"
#include "lgi/common/Mem.h"
#include "lgi/common/Mutex.h"

#undef malloc
#undef free

#if LGI_MEM_DEBUG

#undef new

void *operator new(std::size_t size) throw (std::bad_alloc)
{
	auto scope = new_context::scope();
    return malloc(size);
}

void *operator delete(void *ptr)
{
	free(ptr);
}

void *lgi_malloc(size_t size)
{
	// if (size > (1<<10))
	{
		printf("malloc(%llu)\n", size);
		fflush(stdout);
	}
	
	return malloc(size);
}

void lgi_free(void *ptr)
{
	return free(ptr);
}

#endif

bool LCheckHeap()
{
	return 1;
}

void MemorySizeToStr(char *Str, uint64 Size)
{
	#define K				1024.0
	#define M				(K*K)
	#define G				(M*K)

	if (Size >= G)
	{
		sprintf(Str, "%.2f G", Size / G);
	}
	else if (Size >= M)
	{
		sprintf(Str, "%.2f M", Size / M);
	}
	else if (Size >= (10 * K))
	{
		sprintf(Str, "%.2f K", Size / K);
	}
	else
	{
		sprintf(Str, LPrintfInt64 " bytes", Size);
	}

	#undef K
	#undef M
	#undef G
}

/////////////////////////////////////////////////////////////////////////
bool LgiCanReadMemory(void *p, int Len)
{
	try
	{
		int i = 0;
		for (char *Ptr = (char*)p; Len > 0; Len--)
		{
			i += *Ptr++;
		}
	}
	catch (...)
	{
		// I guess not... then..
		return false;
	}

	return true;
}

bool LDumpMemoryStats(char *filename)
{
	return false;
}

