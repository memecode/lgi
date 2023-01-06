#include "lgi/common/Lgi.h"
#include "lgi/common/DeEscape.h"

char *SkipEscape(char *c)
{
	if (*c != 0x1b)
	{
		LAssert(!"Not an escape seq.");
		return NULL;
	}

	c++;

	if (*c == ']')
	{
		// OS cmd
		c++;
		while (*c >= '0' && *c <= '9')
			c++;
		if (*c == ';')
			c++;
		return c;
	}

	if (	(*c >= '@' && *c <= 'Z') ||
			(*c >= '\\' && *c <= '_') )
		return c + 1;

	if (*c != '[')
	{
		LgiTrace("%s:%i - Error: Expecting bracket.\n", _FL);
		return c;
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

bool IsNewLineEscape(char *s, char *e)
{
	auto ch = e - s;
	if (e[-1] == 'H' && ch > 3)
		return true;
	return false;
}

bool IsOsCmd(char *s, char *e)
{
	if (s[1] == ']')
		return true;
	return false;
}

void DeEscape(LString &s)
{
	if (!s.Length())
		return;
	char *c = s, *out = s, *end = s.Get() + s.Length();
	bool echo = true;
	while (c < end)
	{
		if (*c == 0x1b)
		{
			auto e = SkipEscape(c);
			if (!e)
				break;
			echo = !IsOsCmd(c, e);
			if (IsNewLineEscape(c, e) && *e != '\n')
				*out++ = '\n';
			c = e;
		}
		else if (*c == 7)
		{
			/*
			// Delete last line?
			auto start = s.Get();
			while (out > start && out[-1] != '\n')
				out--;
			*/
			c++; // skip BEL char
		}
		else if (echo)
			*out++ = *c++;
		else
			c++;
	}

	size_t new_len = out - s.Get();
	if (new_len < s.Length())
		s.Length(new_len);
}

void DeEscape(char *s, ssize_t *bytes)
{
	char *c = s, *out = s;
	bool echo = true;
	while (*c)
	{
		if (*c == 0x1b)
		{
			auto e = SkipEscape(c);
			if (!e)
				goto OnError;
			echo = !IsOsCmd(c, e);
			if (IsNewLineEscape(c, e))
				*out++ = '\n';
			c = e;
		}
		else if (*c == 7)
		{
			/*
			// Delete last line?
			auto start = s;
			while (out > start && out[-1] != '\n')
				out--;
			*/
			c++; // Don't output bell
		}
		else if (echo)
			*out++ = *c++;
		else
			c++;
	}

	OnError:
	*out = 0;
	if (bytes)
		*bytes = out-s;
}

