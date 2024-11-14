#include "lgi/common/Lgi.h"
#include "lgi/common/RemoveAnsi.h"

void RemoveAnsi(LArray<char> &a)
{
	auto newSize = RemoveAnsi(a.AddressOf(), a.Length());
	a.Length(newSize);
}

void RemoveAnsi(LString &a)
{
	auto newSize = RemoveAnsi(a.Get(), a.Length());
	a.Length(newSize);
}

size_t RemoveAnsi(char *in, size_t length)
{
	auto *s = in;
	auto *out = in;
	auto e = s + length;

	while (s < e)
	{
		if (*s == 0x7)
			s++; // skip
		else if
		(
			*s == 0x1b
			&&
			s + 1 < e
			&&
			s[1] >= 0x40
			&&
			s[1] <= 0x5f
		)
		{
			// ANSI seq
			if (s[1] == '[' &&
				s[2] == '0' &&
				s[3] == ';')
			{
				s += 4;
			}
			else
			{
				s += 2;
				while (s < e && !IsAlpha(*s))
					s++;
				if (s < e)
					s++;
			}

		}
		else *out++ = *s++;
	}

	return out - in;
}
	
