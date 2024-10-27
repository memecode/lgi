#pragma once

#include <initializer_list>
#include "lgi/common/File.h"
#ifndef _STRUCTURED_IO_H_
#include "lgi/common/StructuredIo.h"
#endif
#include "lgi/common/CommsBus.h"

class LStructuredLog
{
	LStructuredIo io;

	LAutoPtr<LFile> file;
	LAutoPtr<LCommsBus> bus;
	LString endPoint;
	LStream *log = NULL;

	template<typename T>
	void Store(T &&t)
	{
		StructIo(io, t);
	}

	void Process()
	{
		if (file)
		{
			io.ToStream(file);
		}
		else if (bus)
		{
			LStringPipe p;
			if (io.ToStream(&p))
				bus->SendMsg(endPoint, p.NewLStr());
		}
	}

public:
	enum TTargetType
	{
		TFile, // Param is a file name
		TData, // Param is literal data
		TNetworkEndpoint, // Param is a network endpoint
	};

	constexpr static const char *sDefaultEndpoint = "struct.log";

	LStructuredLog(TTargetType target, LString param, bool write = true, LStream *logger = NULL) :
		io(write),
		log(logger)
	{
		switch (target)
		{
			case TFile:
			{
				LString fn;
				if (LIsRelativePath(param))
				{
					LFile::Path p(LSP_APP_INSTALL);
					p += param;
					fn = p.GetFull();
				}
				else fn = param;

				if (file.Reset(new LFile) &&
					file->Open(fn, write ? O_WRITE : O_READ) && write)
					file->SetSize(0);
				break;
			}
			case TData:
			{
				io.FromString(param);
				break;
			}
			case TNetworkEndpoint:
			{
				endPoint = param;
				bus.Reset(new LCommsBus(log));

				if (bus && !write)
				{
					bus->Listen(endPoint,
						[this](auto msg)
						{
							io.FromString(msg);
						});
				}
				break;
			}
		}
	}

	virtual ~LStructuredLog()
	{
		Process();
	}

	LStream *GetLog() const { return log; }
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
		Process();
	}

	// Read and decode to string objects.
	bool Read(std::function<void(LVariantType, size_t, void*, const char*)> callback, Progress *prog = NULL)
	{
		if (io.GetWrite())
		{
			LAssert(!"Not open for reading.");
			return false;
		}

		if (bus)
		{
			LAssert(!"impl me");
		}
		else if (file)
		{
			if (io.GetPos() >= io.Length())
			{
				if (!io.Length(file->GetSize()))
					return false;

				if (prog)
					prog->SetRange(file->GetSize());

				for (int64_t i=0; i<file->GetSize(); )
				{
					size_t blk = MIN(file->GetSize() - i, 128 << 20);
					if (file->Read(io.AddressOf(i), blk) != blk)
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
		}

		if (prog && prog->IsCancelled())
			return false;

		return io.Decode(callback, prog);
	}

	bool Read(int64_t &i)
	{
		bool typeErr = false;
		Read([&](auto type, auto sz, auto ptr, auto name)
		{
			switch (type)
			{
				case GV_INT32:
				{
					LAssert(sz == 4);
					i = *(int32_t*)ptr;
					break;
				}
				case GV_INT64:
				{
					if (sz == 4)
						i = *(int32_t*)ptr;
					else if (sz == 8)
						i = *(int64_t*)ptr;
					else
						LAssert(!"Unknown size");
					break;
				}
				default:
				{
					typeErr = true;
					break;
				}
			}
		});

		return !typeErr;
	}

	// Read and convert to a string.
	bool Read(LString &s)
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

		s = p.NewLStr();
		return s != NULL;
	}

	static void UnitTest()
	{
		auto fn = "my-test-log.struct";
		{
			LStructuredLog Log(TFile, fn);
			LString asd = LString("1234568,");
			int ert = 123;
			LRect rc(10, 10, 200, 100);
			Log.Log("asd:", asd, rc, ert);
		}
		{
			LStructuredLog Rd(TFile, fn, false);
			LString s;
			if (Rd.Read(s))
				LgiTrace("%s\n", s.Get());
		}
	}
};
