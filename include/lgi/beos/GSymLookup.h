/// \file
/// \author Matthew Allen
#ifndef _GSYMLOOKUP_H_
#define _GSYMLOOKUP_H_

#include "LSubProcess.h"

/// Lookup the file/line information for an instruction pointer value
class LSymLookup
{
public:
	typedef int Addr;

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
		return false;
	}
	
	/// Gets the current stack
	int BackTrace(int Epb, int Eip, Addr *addr, int len)
	{
		return false;
	}
};

#endif
