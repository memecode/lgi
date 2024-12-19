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

#define FunctionChar(ch) (IsAlpha(ch) || (ch) == '~')

size_t RemoveAnsi(char *in, size_t length)
{
	auto s = (uint8_t*) in;
	auto *out = in;
	auto e = s + length;
	bool echoChars = true;

	while (s < e)
	{
		if
		(
			*s == 0x7
			||
			(*s == 0x9c && echoChars == false) // I'm not sure if this is correct...
		)
		{
			s++; // skip
			echoChars = true;
		}
		else if
		(
			*s == 0x1b
			&&
			s + 1 < e
		)
		{
			if (s[1] == '[' || s[1] == 0x9B)
			{
				s += 2;

				// Control Sequence Introducer
				while (s < e && !FunctionChar(*s))
					s++; // skip the param chars
				if (s < e)
					s++; // skip the command char
			}
			else if (s[1] == '7' || s[1] == '8')
			{
				// Save/restore cursor
				s += 2;
			}
			else if
			(
				(s[1] == 'P' || s[1] == 0x90) // Device Control String
				||
				(s[1] == ']' || s[1] == 0x9D) // Operating System Command
				||
				(s[1] == 'X') // Start of String
				||
				(s[1] == '^') // Privacy message
				||
				(s[1] == '_') // App command
			)
			{				
				echoChars = false;
				s += 2;
			}
			else if (s[1] == '/')
			{
				echoChars = true;
				s += 2;
			}
			else
			{
				*out++ = *s++;
			}
		}
		else
		{
			if (echoChars)
				*out++ = *s++;
			else
				s++;
		}
	}

	return out - in;
}
	
