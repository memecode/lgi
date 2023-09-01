#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "LgiIde.h"

LString FindHeader(char *Short, LArray<LString::Array*> &Paths)
{
	// Search through the paths
	for (auto Arr: Paths)
	{
		for (auto Path: *Arr)
		{
			char f[MAX_PATH_LEN];
			if (!LMakePath(f, sizeof(f), Path, Short))
				continue;

			// Convert slashes to native:
			char Opposite = DIR_CHAR == '/' ? '\\' : '/';
			for (char *s = f; *s; s++)
				if (*s == Opposite)
					*s = DIR_CHAR;

			if (LFileExists(f))
				return f;
		}
	}
	
	return LString();
}

bool BuildHeaderList(const char *Cpp, LString::Array &Headers, bool Recurse, std::function<LString(LString)> LookupHdr)
{
	char Include[] = "include";
	
	for (auto c = Cpp; c && *c; )
	{
		skipws(c);
		if (*c == '#')
		{
			// preprocessor instruction
			if (strncmp(++c, Include, 7) == 0)
			{
				auto s = c + 7;
				skipws(s);
				if (*s == '\"' || *s == '<')
				{
					char d = (*s == '\"') ? '\"' : '>';					
					char *e = strchr(++s, d);
					LString Short(s, e - s);					
					if (Short)
					{
						auto File = LookupHdr(Short);
						if (File)
						{
							bool Has = false;
							for (auto h: Headers)
							{
								if (h == File)
								{
									Has = true;
									break;
								}
							}
							if (!Has)
							{
								Headers.Add(File);
								if (Recurse)
								{
									// Recursively add includes...
									LAutoString c8(LReadTextFile(File));
									if (c8)
										BuildHeaderList(c8, Headers, Recurse, LookupHdr);
								}
							}
						}
						else
						{
							#if DEBUG_FIND_DEFN
							static bool First = true;
							printf("%s:%i - didn't find '%s'%s\n", _FL, Short, First?" in:":"");
							if (First)
							{
								First = false;
								for (int i=0; i<IncPaths.Length(); i++)
								{
									printf("    '%s'\n", IncPaths[i]);
								}
							}
							#endif
						}
					}
				}
			}
		}
		
		while (*c && *c != '\n')
			c++;
		if (*c == '\n')
			c++;
	}
	
	return true;
}
