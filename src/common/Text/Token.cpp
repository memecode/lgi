// Basic tokeniser

#include <string.h>
#include "lgi/common/Mem.h"
#include "lgi/common/Containers.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/Token.h"

/////////////////////////////////////////////////////////////////////////
char *LSkipDelim(char *p, const char *Delimiter, bool NotDelim)
{
	if (!p)
		return NULL;
	
	if (NotDelim)
	{
		while (*p && !strchr(Delimiter, *p))
			p++;
	}
	else
	{
		while (*p && strchr(Delimiter, *p))
			p++;
	}
	
	return p;
}
