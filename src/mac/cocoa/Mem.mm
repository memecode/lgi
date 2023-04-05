/*hdr
**      FILE:           Memory.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           10/4/98
**      DESCRIPTION:    Memory subsystem
**
**      Copyright (C) 2007, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>

#include "LgiOsDefs.h"
#include "lgi/common/Mem.h"
#include "lgi/common/Mutex.h"

bool LCheckHeap()
{
	return true;
}

bool LDumpMemoryStats(char *filename)
{
	return false;
}

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

#ifdef LGI_MEM_DEBUG

#undef malloc
#undef free

void *lgi_malloc(size_t size)
{
	if (size > 0)
		return malloc(size);

	return 0;
}

void lgi_free(void *ptr)
{
	return free(ptr);
}

#endif
