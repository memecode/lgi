#ifndef _LTelnet_h_
#define _LTelnet_h_

#include "LThread.h"
#include "INet.h"

class LTelnet : public LStream
{
	LSocket s;
	GArray<uint8_t> buf;
	ssize_t used;
	
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
	
	bool IsOpen() override { return s.IsOpen(); }
	
	ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override
	{
		if (!Ptr || Size <= 0 || !s.IsOpen())
			return -1;
			
		auto rd = s.Read(buf.AddressOf() + used, buf.Length() - used);
		// LgiTrace("telnet.rd=%i  used=%i\n", (int)rd, (int)used);
		if (rd > 0)
			used += rd;
		
		uint8_t *p = (uint8_t*)Ptr;
		ssize_t i, start = 0, wr = 0;
		for (i=0; i<used; i++)
		{
			if (buf[i] == 255)
			{
				auto common = MIN(Size-wr, i-start);
				if (common > 0)
				{
					memcpy(p+wr, buf.AddressOf(start), common);
					wr += common;
				}
				if (used - i < 2)
				{
					// Haven't got the whole command
					memmove(buf.AddressOf(0), buf.AddressOf(i), used-i);
					used -= i;
					return i;
				}
				i += 2; // skip over the command structure.
				start = i + 1;
			}
		}
		if (start < i)
		{
			auto common = MIN(Size-wr, i-start);
			if (common > 0)
			{
				memcpy(p+wr, buf.AddressOf(start), common);
				wr += common;
			}
		}
		
		used = 0;		
		return wr;
	}
	
	GString Read()
	{
		char buf[512];
		auto rd = Read(buf, sizeof(buf));
		return GString(buf, rd);
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
	
	ssize_t Write(GString s)
	{
		return Write(s.Get(), s.Length());
	}
};

#endif
