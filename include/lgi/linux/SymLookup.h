/// \file
/// \author Matthew Allen
#pragma once

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
		int IpLen,
		/// [Optional] prefix for each line
		const char *Prefix = nullptr
	)
	{
		auto Sym = backtrace_symbols((void * const *)Ip, IpLen);
		if (!Sym)
			return false;

		int ch = 0;
		for (int i=0; Sym[i] && i<IpLen; i++)
			ch += snprintf(buf+ch, buflen-ch, "%s%p: %s\n", Prefix ? Prefix : "", (void*)Ip[i], Sym[i]);
		
		free(Sym);
		return true;
	}
	
	/// Gets the current stack
	int BackTrace(int Epb, int Eip, Addr *addr, int len)
	{
		return backtrace((void**)addr, len);
	}
};
