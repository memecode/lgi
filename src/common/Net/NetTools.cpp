#include <string.h>
#include <ctype.h>

#include "lgi/common/Mem.h"
#include "LgiOsDefs.h"
#include "lgi/common/NetTools.h"
#include "lgi/common/LgiString.h"

#define IsAlpha(c)					(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

// Gets a field value
char *InetGetField(const char *s)
{
	const char *e = s;
	static const char *WhiteSpace = " \r\t\n";

	// Look for the end of the string
	while (*e)
	{
		if (*e == '\n')
		{
			if (!strchr(" \t", e[1]))
			{
				break;
			}
		}

		e++;
	}

	// Trim whitespace off each end of the string
	while (s < e && strchr(WhiteSpace, *s))
		s++;
	while (e > s && strchr(" \r\t\n", e[-1]))
		e--;

	// Calc the length
	size_t Size = e - s;
	char *Str = new char[Size+1];
	if (Str)
	{
		const char *In = s;
		char *Out = Str;

		while (In < e)
		{
			if (*In == '\r')
			{
			}
			else if (*In == 9)
			{
				*Out++ = ' ';
			}
			else
			{
				*Out++ = *In;
			}

			In++;
		}

		*Out++ = 0;
	}

	return Str;
}

const char *SeekNextLine(const char *s, const char *End)
{
	if (s)
	{
		for (; *s && *s != '\n' && (!End || s < End); s++);
		if (*s == '\n' && (!End || s < End)) s++;
	}

	return s;
}

// Search through headers for a field
char *InetGetHeaderField(	// Returns an allocated string or NULL on failure
	const char *Headers,	// Pointer to all the headers
	const char *Field,		// The field your after
	ssize_t Len)			// Maximum len to run, or -1 for NULL terminated
{
	if (Headers && Field)
	{
		// for all lines
		const char *End = Len < 0 ? 0 : Headers + Len;
		size_t FldLen = strlen(Field);
		
		for (const char *s = Headers;
			*s && (!End || s < End);
			s = SeekNextLine(s, End))
		{
			if (*s != 9 &&
				_strnicmp(s, Field, FldLen) == 0)
			{
				// found a match
				s += FldLen;
				if (*s == ':')
				{
					s++;
					while (*s)
					{
					    if (strchr(" \t\r", *s))
					    {
						    s++;
						}
						else if (*s == '\n')
						{
						    if (strchr(" \r\n\t", s[1]))
						        s += 2;
						    else
						        break;
						}
						else break;						    
					}
					
					return InetGetField(s);
				}
			}
		}
	}

	return 0;
}

char *InetGetSubField(const char *s, const char *Field)
{
	char *Status = 0;

	if (s && Field)
	{
		s = strchr(s, ';');
		if (s)
		{
			s++;

			size_t FieldLen = strlen(Field);
			char White[] = " \t\r\n";
			while (*s)
			{
				// Skip leading whitespace
				while (*s && (strchr(White, *s) || *s == ';')) s++;

				// Parse field name
				if (IsAlpha((uint8_t)*s))
				{
					const char *f = s;
					while (*s && (IsAlpha(*s) || *s == '-')) s++;
					bool HasField = ((s-f) == FieldLen) && (_strnicmp(Field, f, FieldLen) == 0);
					while (*s && strchr(White, *s)) s++;
					if (*s == '=')
					{
						s++;
						while (*s && strchr(White, *s)) s++;
						if (*s && strchr("\'\"", *s))
						{
							// Quote Delimited Field
							char d = *s++;
							char *e = strchr((char*)s, d);
							if (e)
							{
								if (HasField)
								{
									Status = NewStr(s, e-s);
									break;
								}

								s = e + 1;
							}
							else break;
						}
						else
						{
							// Space Delimited Field
							const char *e = s;
							while (*e && !strchr(White, *e) && *e != ';') e++;

							if (HasField)
							{
								Status = NewStr(s, e-s);
								break;
							}

							s = e;
						}
					}
					else break;
				}
				else break;
			}
		}
	}

	return Status;
}

