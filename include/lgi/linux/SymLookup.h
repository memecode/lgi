/// \file
/// \author Matthew Allen
#ifndef _GSYMLOOKUP_H_
#define _GSYMLOOKUP_H_

#include "execinfo.h"
#include "lgi/common/SubProcess.h"

/// Lookup the file/line information for an instruction pointer value
class LSymLookup
{
public:
	#if __LP64__ || defined(_WIN64)
	typedef long int Addr;
	#else
	typedef int Addr;
	#endif

	LSymLookup()
	{
	}
	
	~LSymLookup()
	{
	}
	
	bool GetStatus()
	{
		return true;
	}
	
	/// Looks up the file and line related to an instruction pointer address
	/// \returns non zero on success
	bool Lookup
	(
		/// The return string buffer
		char *buf,
		/// The buffer's length
		int buflen,
		/// The address
		Addr *Ip,
		/// The number of addresses passed
		int IpLen
	)
	{
		void *ip = (void*) Ip;
		char **Sym = backtrace_symbols((void* const*)&ip, IpLen);
		if (!Sym)
			return false;

		for (int i=0; Sym[i] && i<IpLen; i++)
		{
			/*
			char *s = strrchr(Sym[i], '('), *e;
			if (s != 0 && (e = strchr(++s, '+')))
			{
				LStringPipe un;
				un.Write(Sym[i], s - Sym[i] - 1);
				un.Write((void*)" (", 2);

				char *man = NewStr(s, e - s);
				if (man)
				{			
					LProcess p;
					p.Run("c++filt", man, 0, true, 0, &un);
					un.Write(e, strlen(e));
					
					DeleteArray(man);
				}

				man = un.NewStr();
				if (man)
				{
					char *o = man;
					for (char *i=man; *i; i++)
						if (*i != '\n') *o++ = *i;
					*o++ = 0;
					
					int CopyLen = strlen(man);
					strsafecpy(buf, man, buflen);
					buflen -= CopyLen;
					buf += CopyLen;
				}
				
				DeleteArray(man);
			}
			else
			*/
			{
				int CopyLen = strlen(Sym[i]);
				strcpy_s(buf, buflen, Sym[i]);
				buflen -= CopyLen;
				buf += CopyLen;
			}
			
			if (buflen > 1)
			{
				*buf++ = '\n';
				*buf = 0;
			}
		}
		
		free(Sym);
		return true;
	}
	
	/// Gets the current stack
	int BackTrace(int Epb, int Eip, Addr *addr, int len)
	{
		return backtrace((void**)addr, len);
	}
};

#endif
