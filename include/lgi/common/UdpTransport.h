#pragma once

#include "lgi/common/Net.h"

class LUdpTransport : public LUdpListener
{
	constexpr static int HEADER_SZ = 8;
	constexpr static int PAYLOAD = MAX_UDP_SIZE - HEADER_SZ;
	enum TCommandState
	{
		TNone = 0,

		// Commands
		TData = 10,
		TAck,

		// States
		TUnsent,
		TSent,
		TReceived,
	};
	struct Hdr
	{
		uint16_t cmd;
		uint16_t stream_id;
		uint16_t index;
		uint16_t total;
		uint8_t data[PAYLOAD];
	};
	struct Part
	{
		uint16_t state = TNone;
		uint16_t size = 0;
		char data[MAX_UDP_SIZE];

		Hdr *get() { return (Hdr*)data; }
	};
	struct Stream
	{
		bool writing = false;
		uint32_t ip = 0;
		uint16_t stream_id = 0;
		int count = 0;
		uint64_t completeTs = 0;
		LArray<Part> parts;

		bool Done()
		{
			return count == parts.Length();
		}

		bool Complete()
		{
			int done = 0;
			if (writing)
			{
				for (auto &p: parts)
					if (p.state == TSent)
						done++;
			}
			else
			{
				for (auto &p: parts)
					if (p.state == TReceived)
						done++;
			}
			return done == parts.Length();
		}

		int Sizeof()
		{
			int sz = 0;
			auto state = writing ? TSent : TReceived;
			for (auto &p: parts)
				if (p.state == state)
					sz += p.size - HEADER_SZ;
				else
					return -1;
			return sz;
		}

		LString Assemble()
		{
			LString s;
			auto sz = Sizeof();
			if (sz > 0)
			{
				if (s.Length(sz))
				{
					int off = 0;
					for (auto &p: parts)
					{
						auto bytes = p.size - HEADER_SZ;
						LAssert(off + bytes <= sz);
						memcpy(s.Get() + off, p.get()->data, bytes);
						off += bytes;
					}
					s.Get()[sz] = 0;
				}
			}
			return s;
		}
	};

	LHashTbl<IntKey<uint64_t>, Stream*> map;

	uint64_t key(uint32_t ip, uint16_t stream_id)
	{
		return ((uint64_t)stream_id << 32) | (uint64_t)ip;
	}

	Stream *GetStream(uint32_t ip, int stream_id = -1)
	{
		if (stream_id < 0)
		{
			while (map.Find(key(ip, stream_id = LRand(0xffff))))
				;
		}

		auto k = key(ip, stream_id);
		auto s = map.Find(k);
		if (!s)
		{
			if (s = new Stream)
			{
				s->ip = ip;
				s->stream_id = stream_id;
				s->count = 0;
				if (!map.Add(k, s))
				{
					DeleteObj(s);
				}
			}
		}

		return s;
	}

	#define LOGGER(name) \
		void name(const char *fmt, ...) \
		{ \
			if (!Log) return; \
			va_list arg; va_start(arg, fmt); \
			LStreamPrintf(Log, 0, fmt, arg); \
			va_end(arg); \
		}
	LOGGER(ERR)
	LOGGER(INFO)
	LOGGER(DEBUG)

public:
	#define SECONDS(sec) ((sec) * 1000)
	constexpr static int TIMEOUT_SEND = SECONDS(5);

	/// callback for when a packet is received
	std::function<void(LString)> onReceive;

	/// [optional] callback for when a packet is sent.
	std::function<void(int, bool)> onSend;

	LUdpTransport(LArray<uint32_t> interface_ips, uint32_t mc_ip, uint16_t port, LStream *log = NULL)
		: LUdpListener(interface_ips, mc_ip, port, log)
	{
		int sz = sizeof(Hdr);
		LAssert(sz == MAX_UDP_SIZE);
	}

	/// \returns the stream ID
	int Send(uint32_t ip, LString msg)
	{
		if (auto s = GetStream(ip))
		{
			s->writing = true;

			uint16_t index = 0;
			uint16_t total_parts = (uint16_t) (msg.Length() + (PAYLOAD-1)) / PAYLOAD;
			for (size_t i=0; i<msg.Length(); i+=PAYLOAD, index++)
			{
				Part &part = s->parts[index];
				auto dataBytes = (uint16_t) MIN(PAYLOAD, msg.Length() - i);
				part.state = TUnsent;
				part.size = HEADER_SZ + dataBytes;
				
				auto hdr = part.get();
				hdr->cmd = TData;
				hdr->index = index;
				hdr->stream_id = s->stream_id;
				hdr->total = total_parts;
				memcpy(hdr->data, msg.Get() + i, dataBytes);
			}

			LAssert(total_parts == s->parts.Length());
			return true;
		}
		
		return false;
	}

	bool TimeSlice()
	{
		if (!IsReadable(10))
			return false;

		auto startTs = LCurrentTime();

		// Do reading:
		char data[MAX_UDP_SIZE];
		uint32_t ip = 0;
		uint16_t port = 0;
		int rd = ReadUdp(data, MAX_UDP_SIZE, 0, &ip, &port);
		if (rd > 0)
		{
			Hdr *hdr = (Hdr*)data;
			if (auto s = GetStream(ip, hdr->stream_id))
			{
				switch (hdr->cmd)
				{
					case TData:
					{
						Part &part = s->parts[hdr->index];
						LAssert(rd <= sizeof(part.data));
						part.state = TReceived;
						part.size = rd;
						memcpy(part.data, data, rd);
						DEBUG("%s:%i - received %x:%i.\n", _FL, s->stream_id, hdr->index);

						if (++s->count == hdr->total)
						{
							// Fully received?
							if (s->Complete())
							{
								// Send ack
								Part ack;
								ack.state = TUnsent;
								ack.size = HEADER_SZ;
								ack.get()->cmd = TAck;
								ack.get()->index = hdr->total;
								ack.get()->total = hdr->total;
								ack.get()->stream_id = hdr->stream_id;
								if (WriteUdp(ack.data, ack.size, 0, ip, port))
								{
									DEBUG("%s:%i - acked stream %x.\n", _FL, s->stream_id);
									ack.size = TSent;
								}

								// Call the handler with the message
								if (auto msg = s->Assemble())
								{
									if (onReceive)
										onReceive(msg);
									else
										ERR("%s:%i - no receive handler\n", _FL);
								}
								else ERR("%s:%i - assemble failed\n", _FL);
							}
							else ERR("%s:%i - not complete?", _FL);

							map.Delete(key(ip, hdr->stream_id));
							delete s;
						}
						break;
					}
					case TAck:
					{
						// Look through the written streams for matching one
						bool found = false;
						for (auto p: map)
						{
							Stream *s = p.value;
							if (s->writing &&
								s->stream_id == hdr->stream_id)
							{
								found = true;
								if (s->Complete())
								{
									DEBUG("%s:%i - stream %x acked.\n", _FL, s->stream_id);
									if (onSend)
										onSend(s->stream_id, true);
								}
								else ERR("%s:%i - not complete?\n", _FL);

								auto k = key(s->ip, s->stream_id);
								LAssert(map.Find(k) == s);
								map.Delete(k);
								delete s;
							}
						}
						if (!found)
						{
							ERR("%s:%i - failed to find acked stream: %x\n", _FL, hdr->stream_id);
						}
						break;
					}
				}
			}
		}

		// Do writing:
		for (auto p: map)
		{
			Stream *s = p.value;
			if (!s->writing) // ignore streams for reading
				continue;

			if (s->Done())
			{
				// Stream is fully sent... check the timeout
				auto now = LCurrentTime();
				if (now - s->completeTs > 1000)
				{
					ERR("%s:%i - written stream not acked: %x\n", _FL, s->stream_id);
					s->completeTs = now;
				}

				continue; // waiting for ack
			}

			for (int i=0; i<s->parts.Length(); i++)
			{
				auto &part = s->parts[i];

				if (LCurrentTime() - startTs >= 100)
					return true;
				
				if (part.state == TUnsent)
				{
					if (!WriteUdp(part.data, part.size, 0, s->ip, Port))
						continue;

					part.state = TSent;
					DEBUG("%s:%i - send %x:%i.\n", _FL, s->stream_id, i);

					if (++s->count == s->parts.Length())
					{
						DEBUG("%s:%i - stream %x is fully sent.\n", _FL, s->stream_id);
						s->completeTs = LCurrentTime();
					}
				}
			}
		}

		return true;
	}

	static uint32_t CheckSum(uint16_t *addr, size_t count)
	{
		uint32_t sum = 0;
		while (count > 1)
		{
			sum += *addr++;
			count -= 2;
		}
		//if any bytes left, pad the bytes and add
		if(count > 0)
			sum += ((*addr)&htons(0xFF00));
		//Fold sum to 16 bits: add carrier to result
		while (sum>>16)
			sum = (sum & 0xffff) + (sum >> 16);
		//one's complement
		return ((unsigned short)~sum);
	}

	// Unit testing:
protected:
	constexpr static const char *UNITTEST_MULTICAST = "230.6.6.11";
	constexpr static int         UNITTEST_PORT      = 45455;

	static LArray<uint32_t> GetInterfaceIPs()
	{
		LArray<Interface> interfaces;
		LSocket::EnumInterfaces(interfaces);
		LArray<uint32_t> ip;
		for (auto &i: interfaces)
			ip.Add(i.Ip4);
		return ip;
	}

	static LString UnitTestMsg()
	{
		LStringPipe p(900);
		for (int i=1000; p.GetSize() < 800; i++)
			p.Print("%i\n", i++);
		return p.NewLStr();
	};

public:
	static bool UnitTest_Receiver(LStream *log = nullptr)
	{
		bool loop = true, status = false;
		auto startTs = LCurrentTime();
		auto msg = UnitTestMsg();
		auto sum = CheckSum((uint16_t*)msg.Get(), msg.Length());

		LUdpTransport t(GetInterfaceIPs(), LIpToInt(UNITTEST_MULTICAST), UNITTEST_PORT, log);
		t.onReceive = [&](auto receivedMsg)
			{
				status = receivedMsg == msg;
				if (status)
					t.INFO("%s:%i - received msg ok.\n", _FL);
				else
					t.ERR("%s:%i - received incorrect msg!\n", _FL);
				loop = false;
			};

		t.INFO("Waiting for message...\n");
		while (loop)
		{
			t.TimeSlice();
			LSleep(10);
		}

		return status;
	}

	static bool UnitTest_Sender(uint32_t destIp, LStream *log = nullptr)
	{
		bool loop = true, status = false;
		auto startTs = LCurrentTime();
		auto msg = UnitTestMsg();
		auto sum = CheckSum((uint16_t*)msg.Get(), msg.Length());
		uint16_t stream_id = 0;

		LUdpTransport t(GetInterfaceIPs(), LIpToInt(UNITTEST_MULTICAST), UNITTEST_PORT, log);
		t.onSend = [&](auto id, bool ok)
			{
				status = id == stream_id && ok;
				if (status)
					t.INFO("%s:%i - wrote msg ok.\n", _FL);
				else
					t.ERR("%s:%i - failed to write msg.\n", _FL);
				loop = false;
			};

		t.INFO("Sending message...\n");
		stream_id = t.Send(destIp, msg);
		if (stream_id < 0)
			return false;
		
		t.INFO("Waiting for response...\n");
		while (loop)
		{
			t.TimeSlice();
			LSleep(10);
		}

		return status;
	}
};
