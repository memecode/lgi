#ifndef __TOKEN_H
#define __TOKEN_H

#include "lgi/common/Array.h"
#include "lgi/common/LgiString.h"

/// Skips over a set of delimiters, returning the next non-delimiter
LgiFunc char *LSkipDelim(char *p, const char *Delimiter = " \r\n\t", bool NotDelim = false);

/// A simple token parser
class LToken : public LArray<char*>
{
	char *Raw;

public:
	LToken()
	{
		Raw = 0;
		fixed = true;
	}
	
	LToken(const char *Str, const char *Delimiters = " \r\n\t,", bool GroupDelim = true, ssize_t Length = -1)
	{
		Raw = 0;
		fixed = true;
		Parse(Str, Delimiters, GroupDelim, Length);
	}

	~LToken()
	{
		Empty();
	}

	/// Parses a string with set delimiters
	void Parse(const char *Str, const char *Delimiters = " \r\n\t,", bool GroupDelim = true, ssize_t Length = -1)
	{
		Empty();
		if (Str)
		{
			// Prepare lookup table
			bool Lut[256];
			ZeroObj(Lut);
			if (Delimiters)
			{
				for (const char *d=Delimiters; *d; d++)
				{
					Lut[(const uchar)*d] = true;
				}
			}

			// Do tokenisation
			Raw = NewStr(Str, Length);
			if (Raw)
			{
				fixed = false;

				if (Delimiters)
				{
					if (GroupDelim)
					{
						for (char *s=Raw; *s;)
						{
							// Skip Delimiters
							while (*s && Lut[(uchar)*s]) *s++ = 0;

							// Emit pointer
							if (*s)
							{
								Add(s);
							}

							// Skip Non-delimiters
							while (*s && !Lut[(unsigned char)*s]) s++;
						}
					}
					else
					{
						for (char *s=Raw; *s;)
						{
							// Emit pointer
							if (*s)
							{
								Add(Lut[(uchar)*s] ? (char*)0 : s);
							}

							// Skip Non-delimiters
							while (*s && !Lut[(unsigned char)*s]) s++;

							// Skip Delimiter
							if (*s && Lut[(uchar)*s]) *s++ = 0;
						}
					}
				}
				else
				{
					Add(Raw);
				}

				fixed = true;
			}
		}
	}
	
	/// Empties the object
	void Empty()
	{
		DeleteArray(Raw);
		Length(0);
	}

	/// Appends some tokens on the end of this list.
	void AppendTokens(LArray<char*> *T)
	{
		if (T)
		{
			// int64 Total = Length() + T->Length();
			size_t Bytes = 0;
			unsigned i;
			for (i=0; i<Length(); i++)
			{
				Bytes += strlen((*this)[i]) + 1;
			}
			for (i=0; i<T->Length(); i++)
			{
				Bytes += strlen((*T)[i]) + 1;
			}

			char *Buf = new char[Bytes];
			if (Buf)
			{
				char *p = Buf;

				fixed = false;

				for (i=0; i<Length(); i++)
				{
					char *Ptr = (*this)[i];
					size_t Len = strlen(Ptr) + 1;
					memcpy(p, Ptr, Len);
					(*this)[i] = p;
					p += Len;
				}

				DeleteArray(Raw);
			
				for (i=0; i<T->Length(); i++)
				{
					char *Ptr = (*T)[i];
					size_t Len = strlen(Ptr) + 1;
					memcpy(p, Ptr, Len);
					Add(p);
					p += Len;
				}

				fixed = true;
				Raw = Buf;
			}
		}
	}

	char *&operator [](size_t i)
	{
		if (i < Length())
			return LArray<char*>::operator[](i);

		static char *Null = 0;
		return Null;
	}
};

#endif
