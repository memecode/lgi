#pragma once

struct LConsole : public LStream
{
	struct Line
	{
		char txt[512] = "";
		int used = 0;
	};

	LPoint Cursor, Prev;
	LArray<Line> Lines;

	ssize_t GetSize()
	{
		ssize_t sz = 0;
		for (auto &l: Lines)
			sz += l.used;
		return sz + Lines.Length() - 1;
	}

	LString GetText()
	{
		LString s;
		s.Length(GetSize());

		auto o = s.Get();
		for (unsigned i=0; i<Lines.Length(); i++)
		{
			auto &l = Lines[i];
			memcpy(o, l.txt, l.used);
			o += l.used;
			if (i < Lines.Length() - 1)
				*o++ = '\n';
		}

		*o = 0;
		LAssert(o == s.Get() + s.Length());
		return s;
	}

	LString Last()
	{
		LString s;
		if (Lines.Length())
		{
			auto &l = Lines.Last();
			s.Set(l.txt, l.used);
		}
		return s;
	}

	LString Delta()
	{
		return GetFrom(Prev);
	}

	LString GetFrom(LPoint p)
	{
		LString::Array s;

		if (p == Cursor)
			return NULL;
		
		int x = p.x;
		for (int y = p.y; y <= Cursor.y; y++)
		{
			auto &l = Lines[y];
			auto end = y == Cursor.y ? Cursor.x : l.used;
			auto len = end - x;
			s.New().Set(l.txt + x, len);
			x = 0;
		}
		
		return LString("\n").Join(s);
	}

	void SetCursor(LPoint cur)
	{
		if (cur.y == Cursor.y &&
			cur.x < Cursor.x)
			Prev = Cursor;
		Cursor = cur;
	}

	void Scroll(int ln)
	{
		if (ln < 0)
		{
			for (int i=0; i<std::abs(ln); i++)
				Lines.DeleteAt(0, true);
			Prev.y = MAX(Prev.y + ln, 0);
		}
		else LAssert(0);
	}

	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0)
	{
		auto s = (char *)Ptr;
		auto e = s + Size;
		bool echo = true;

		Prev = Cursor;

		while (s < e)
		{
			if (*s == 0x1b)
			{
				auto next = SkipEscape(s);
				if (!next)
				{
					s++;
					continue;
				}

				// Is this a set cursor command?
				if (s[1] == '[')
				{
					switch (next[-1])
					{
						case 'H': // Cursor position
						{
							LString n(s + 2, next - s - 3);
							auto p = n.SplitDelimit(";");
							LPoint c;
							c.y = p.IdxCheck(0) ? p[0].Int() - 1 : 0;
							c.x = p.IdxCheck(1) ? p[1].Int() - 1 : 0;
							SetCursor(c);
							break;
						}
						case 'S': // Scroll up
						// case 'J':
						{
							Scroll(-1);
							break;
						}
						case 'T': // Scroll down
						{
							Scroll(1);
							break;
						}
						case 'h':
						case 'l':
							break; // Hide/show cursor
						default:
						{
							LgiTrace("Unhandled Control Sequence: '%c'\n", next[-1]);
							break;
						}
					}					
				}
				
				// Turn off echo for system commands
				echo = s[1] != ']' || next[-1] != ';';

				s = next;
			}
			else if (echo)
			{
				if (*s == '\n')
				{
					/*
					Cursor.y++;
					Cursor.x = 0;
					*/
				}
				else
				{
					auto &l = Lines[Cursor.y];
					if (l.used < sizeof(l.txt))
					{
						// Add char to cursor's line
						while (l.used < Cursor.x)
							l.txt[l.used++] = ' '; // Extend the line to the cursor's position
						l.txt[Cursor.x++] = *s; // Add the char
						l.used = Cursor.x;
						l.txt[l.used] = 0;
					}
				}
				s++;
			}
			else
			{
				s++;
			}
		}

		return Size;
	}
};


#ifdef _STRUCTURED_IO_H_

inline void StructIo(LStructuredIo &io, LConsole &c)
{
	auto obj = io.StartObj("LConsole");

	io.Int(c.Cursor.x, "cursor.x");
	io.Int(c.Cursor.y, "cursor.y");

	if (io.GetWrite())
	{
		for (auto &l: c.Lines)
		{
			LAutoString u((char*)LNewConvertCp("utf-8", l.txt, "windows-1252", l.used));
			io.String(u.Get(), Strlen(u.Get()));
		}
	}
	else
	{
		io.Decode([&c](auto type, auto sz, auto ptr, auto name)
		{
			if (type == GV_STRING)
			{
				auto &l = c.Lines.New();
				l.used = sz;
				memcpy(l.txt, ptr, sz);
			}
		});
	}
}

#endif
