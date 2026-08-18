/// \file
/// \author Matthew Allen
#pragma once

#include "execinfo.h"
#include "lgi/common/SubProcess.h"

#include <dlfcn.h>

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
	
	static void TrimNewline(char *s)
	{
		if (!s)
			return;
		for (char *p = s + strlen(s); p > s && (p[-1] == '\n' || p[-1] == '\r'); p--)
			p[-1] = 0;
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
		{
			bool Resolved = false;
			void *Addr = (void*)Ip[i];
			Dl_info Info;
			if (dladdr(Addr, &Info) && Info.dli_fname && Info.dli_fname[0])
			{
				char Cmd[1024];
				int n = snprintf(Cmd, sizeof(Cmd), "addr2line -C -f -p -e %s %p 2>/dev/null", Info.dli_fname, Addr);
				if (n > 0 && n < (int)sizeof(Cmd))
				{
					FILE *Pipe = popen(Cmd, "r");
					if (Pipe)
					{
						char Line1[512] = "";
						char Line2[512] = "";
						if (fgets(Line1, sizeof(Line1), Pipe) && fgets(Line2, sizeof(Line2), Pipe))
						{
							TrimNewline(Line1);
							TrimNewline(Line2);
							if (Line1[0] && Line2[0] && strcmp(Line1, "??") != 0 && strcmp(Line2, "0") != 0)
							{
								char Src[1024];
								snprintf(Src, sizeof(Src), "%s: %s", Line2, Line1);
								if (buflen-ch > 0)
									ch += snprintf(buf+ch, buflen-ch, "%s%s\n", Prefix ? Prefix : "", Src);
								Resolved = true;
							}
						}
						pclose(Pipe);
					}
				}
			}

			if (!Resolved)
			{
				if (buflen-ch > 0)
					ch += snprintf(buf+ch, buflen-ch, "%s%p: %s\n", Prefix ? Prefix : "", (void*)Ip[i], Sym[i]);
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
