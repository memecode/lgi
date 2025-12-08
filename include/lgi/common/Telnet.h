#ifndef _LTelnet_h_
#define _LTelnet_h_

#include "lgi/common/Thread.h"
#include "lgi/common/Net.h"
#include "lgi/common/StructuredLog.h"

class LTelnet : public LStream
{
	LSocket s;
	LArray<uint8_t> buf;
	ssize_t used;
	LStructuredLog *slog = nullptr;
	
public:
	LTelnet(const char *host = NULL, int port = 23)
	{
		used = 0;
		if (host)
			Open(host, port);
	}
	
	int Open(const char *host, int port = 23) override
	{
		if (!s.Open(host, port))
			return false;
		
		s.IsBlocking(false);
		buf.Length(1024);
		return true;
	}
	
	void SetSlog(LStructuredLog *l)
	{
		slog = l;
	}
	
	bool IsOpen() override
	{
		return s.IsOpen();
	}
	
	bool IsReadable(int TimeoutMs)
	{
		return s.IsReadable(TimeoutMs) || used > 0;
	}
	
	ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override
	{
		if (!Ptr || Size <= 0 || !s.IsOpen())
			return -1;
			
		auto rd = s.Read(buf.AddressOf() + used, buf.Length() - used);
		if (rd > 0)
		{
			if (slog)
				slog->Log("telnet.rd=", rd, used, LString((const char*)buf.AddressOf() + used, rd));
			used += rd;
		}
		
		auto in = buf.AddressOf();
		auto endIn = in + used;
		auto outStart = (uint8_t*) Ptr;
		auto out = outStart;
		auto endOut = out + Size;

		while (in < endIn)
		{
			if (*in == 255)
			{
				// Skip cmd...
				auto remaining = endIn - in;
				if (remaining < 3)
					break;
				in += 3;
			}
			else if (out >= endOut)
			{
				// no more write space...
				break;
			}
			else
			{
				*out++ = *in++;
			}
		}

		auto remaining = endIn - in;
		if (remaining > 0)
			memmove(buf.AddressOf(), in, remaining);
		used = remaining;
		
		auto written = out - outStart;
		if (slog)
			slog->Log("telnet.rd.written=", used, written, LString((const char*)Ptr, written));
		return written;
	}
	
	LString Read()
	{
		char buf[512];
		auto rd = Read(buf, sizeof(buf));
		return LString(buf, rd > 0 ? rd : 0);
	}

	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
	{
		uint8_t *p = (uint8_t*)Ptr;
		int i, start = 0;
		for (i=0; i<Size; i++)
		{
			if (p[i] == 255)
			{
				if (i > start)
				{
					auto wr = s.Write(p+start, i-start);
					if (wr < i-start)
						return -1;
				}
				if (s.Write("\xff", 1) != 1)
					return -1;
				start = i+1;
			}
		}
		if (i > start)
		{
			auto wr = s.Write(p+start, i-start);
			if (wr != i-start)
				return -1;
		}
		
		return Size;
	}
	
	ssize_t Write(LString s)
	{
		return Write(s.Get(), s.Length());
	}
};

#endif
