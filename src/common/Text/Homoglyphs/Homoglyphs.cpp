#include "lgi/common/Lgi.h"

extern LArray<int> HomoglyphsTable[];

bool HasHomoglyphs(const char *utf8, ssize_t len)
{
	if (!utf8)
		return false;
	if (len < 0)
		len = Strlen(utf8);
	uint8_t *end = (uint8_t*)utf8 + len;

	for (LUtf8Ptr p(utf8); p.GetPtr() < end; p++)
	{
		int32 ch = p;
		// LgiTrace("ch=%i %x\n", ch, ch);
		for (int t=0; HomoglyphsTable[t].Length(); t++)
		{
			auto &a = HomoglyphsTable[t];
			for (int i=1; i<a.Length(); i++)
			{
				if (a[i] == ch)
					return true;
			}
		}
	}

	return false;
}

bool RemoveZeroWidthCharacters(char *utf8, ssize_t len)
{
	if (!utf8)
		return false;
	if (len < 0)
		len = Strlen(utf8);
	uint8_t *end = (uint8_t*)utf8 + len;
	LUtf8Ptr in(utf8), out(utf8);

	bool changed = false;
	while (in.GetPtr() < end)
	{
		int32 ch = in;
		if (ch == UNICODE_ZERO_WIDTH_JOINER ||
			ch == UNICODE_ZERO_WIDTH_SPACE)
			changed = true;
		else
			out = ch;

		in++;
	}
	if (out.GetPtr() < end)
		out = (uint32_t)0; // NULL terminate

	return changed;
}
