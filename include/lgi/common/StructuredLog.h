#pragma once

#include <initializer_list>
#include "lgi/common/File.h"
#ifndef _STRUCTURED_IO_H_
#include "lgi/common/StructuredIo.h"
#endif

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
		io.Flush(&f);
	}

	LStructuredIo &GetIo() { return io; }

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
	bool Read(std::function<void(LVariantType, size_t, void*, const char*)> callback, Progress *prog = NULL)
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

			if (prog)
				prog->SetRange(f.GetSize());

			for (int64_t i=0; i<f.GetSize(); )
			{
				size_t blk = MIN(f.GetSize() - i, 128 << 20);
				if (f.Read(io.AddressOf(i), blk) != blk)
					return false;
				i += blk;
				if (prog)
				{
					prog->Value(i);
					if (prog->IsCancelled())
						return false;
				}
			}
		}

		if (prog && prog->IsCancelled())
			return false;

		return io.Decode(callback, prog);
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

		callback(p.NewLStr());
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
