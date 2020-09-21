#include "Lgi.h"
#include "DeEscape.h"

char *SkipEscape(char *c)
{
	if (*c != 0x1b)
	{
		LgiAssert(!"Not an escape seq.");
		return NULL;
	}

	c++;
	if (	(*c >= '@' && *c <= 'Z') ||
			(*c >= '\\' && *c <= '_') )
		return c + 1;

	if (*c != '[')
	{
		LgiAssert(!"Invalid escape seq.");
		return NULL;
	}
	c++;
	while (*c >= '0' && *c <= '?')
		c++;
	while (*c >= ' ' && *c <= '/')
		c++;
	if (*c >= '@' && *c <= '~')
		c++;
	return c;
}

void DeEscape(GString &s)
{
	char *c = s, *out = s;
	while (*c)
	{
		if (*c == 0x1b)
		{
			auto e = SkipEscape(c);
			if (!e) goto OnError;
			c = e;
		}
		else if (*c == 7)
		{
			// Delete last line?
			auto start = s.Get();
			while (out > start && out[-1] != '\n')
				out--;
			c++;
		}
		else
			*out++ = *c++;
	}

	OnError:
	size_t new_len = out - s.Get();
	if (new_len < s.Length())
		s.Length(new_len);
}

void DeEscape(char *s, ssize_t *bytes)
{
	char *c = s, *out = s;
	while (*c)
	{
		if (*c == 0x1b)
		{
			auto e = SkipEscape(c);
			if (!e) goto OnError;
			c = e;
		}
		else if (*c == 7)
		{
			// Delete last line?
			auto start = s;
			while (out > start && out[-1] != '\n')
				out--;
			c++;
		}
		else
			*out++ = *c++;
	}

	OnError:
	*out = 0;
	if (bytes)
		*bytes = out-s;
}

