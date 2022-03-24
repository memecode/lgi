#pragma once

#include <initializer_list>
#include "lgi/common/File.h"
#ifndef _STRUCTURED_IO_H_
#include "lgi/common/StructuredIo.h"
#endif

#define IntIo(type) inline void StructIo(LStructuredIo &io, type i) { io.Int(i); }
#define StrIo(type) inline void StructIo(LStructuredIo &io, type i) { io.String(i); }

IntIo(char)
IntIo(unsigned char)
IntIo(short)
IntIo(unsigned short)
IntIo(int)
IntIo(unsigned int)
IntIo(int64_t)
IntIo(uint64_t)

StrIo(char*);
StrIo(const char*);
StrIo(wchar_t*);
StrIo(const wchar_t*);

inline void StructIo(LStructuredIo &io, LString &s)
{
	if (io.GetWrite())
		io.String(s.Get(), s.Length());
	else
		io.Decode([&s](auto type, auto sz, auto ptr, auto name)
		{
			if (type == GV_STRING && ptr && sz > 0)
				s.Set((char*)ptr, sz);
		});
}

inline void StructIo(LStructuredIo &io, LRect &r)
{
	auto obj = io.StartObj("LRect");
	io.Int(r.x1, "x1");
	io.Int(r.y1, "y1");
	io.Int(r.x2, "x2");
	io.Int(r.y2, "y2");
}

class LStructuredLog
{
	LFile f;
	LStructuredIo io;
	
	template<typename T>
	void Store(T &t)
	{
		StructIo(io, t);
	}

public:
	LStructuredLog(const char *FileName, bool write = true) : io(write)
	{
		LString fn;
		if (LIsRelativePath(FileName))
		{
			LFile::Path p(LSP_APP_INSTALL);
			p += FileName;
			fn = p.GetFull();
		}
		else fn = FileName;

		if (f.Open(fn, write ? O_WRITE : O_READ) && write)
			f.SetSize(0);
	}

	virtual ~LStructuredLog()
	{
	}

	// Write objects to the log. Custom types will need to have a StructIo implementation
	template<typename... Args>
	void Log(Args&&... args)
	{
		if (!io.GetWrite())
		{
			LAssert(!"Not open for writing.");
			return;
		}
		int dummy[] = { 0, ( (void) Store(std::forward<Args>(args)), 0) ... };
		io.Flush(&f);
	}

	// Read and decode to string objects.
	bool Read(std::function<void(LVariantType, size_t, void*, const char*)> callback)
	{
		if (io.GetWrite())
		{
			LAssert(!"Not open for reading.");
			return false;
		}

		if (io.GetPos() >= io.Length())
		{
			if (!io.Length(f.GetSize()))
				return false;
			if (f.Read(io.AddressOf(), f.GetSize()) != f.GetSize())
				return false;
		}

		return io.Decode(callback);
	}

	// Read and convert to a string.
	void Read(std::function<void(LString)> callback)
	{
		LStringPipe p;
		while (Read([&p](auto type, auto sz, auto ptr, auto name)
			{
				LString prefix;

				if (type == LStructuredIo::EndRow)
					return;

				if (p.GetSize())
					prefix = " ";
				if (name)
				{
					prefix += name;
					prefix += "=";
				}

				switch (type)
				{
					case GV_STRING:
					{
						p.Print("%s%.*s", prefix ? prefix.Get() : "", (int)sz, ptr);
						break;
					}
					case GV_INT64:
					{
						if (sz > 4)
							p.Print("%s" LPrintfInt64, prefix ? prefix.Get() : "", *((int64*)ptr));
						else
							p.Print("%s%i", prefix ? prefix.Get() : "", *((int*)ptr));
						break;
					}
					case LStructuredIo::StartObject:
					{
						p.Print("%s {", name);
						break;
					}
					case LStructuredIo::EndObject:
					{
						p.Print(" }");
						break;
					}
					default:
					{
						LAssert(!"Impl me.");
						break;
					}
				}
			}));

		callback(p.NewGStr());
	}

	static void UnitTest()
	{
		auto fn = "my-test-log.struct";
		{
			LStructuredLog Log(fn);
			LString asd = LString("1234568,");
			int ert = 123;
			LRect rc(10, 10, 200, 100);
			Log.Log("asd:", asd, rc, ert);
		}
		{
			LStructuredLog Rd(fn, false);
			Rd.Read([](LString s)
			{
				LgiTrace("%s\n", s.Get());
			});
		}
	}
};
