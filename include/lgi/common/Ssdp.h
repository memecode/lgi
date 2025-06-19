#pragma once

#include "lgi/common/Net.h"
#include "lgi/common/Thread.h"

class LSsdp :
	public LThread,
	public LMutex,
	public LCancel
{
public:
	struct Result
	{
		uint32_t ip4;
		LString type;
		LString headers;
		
		Result &Set(uint32_t ip, LString &pkt)
		{
			ip4 = ip;
			headers = pkt;
			auto pos = headers.Find(" ");
			if (pos >= 0)
				type = headers(0, pos);
			return *this;
		}

		LString Location() { return LGetHeaderField(headers, "LOCATION"); }
		LString UniqueServiceName() { return LGetHeaderField(headers, "USN"); }
		LString NotificationType() { return LGetHeaderField(headers, "NT"); }
		LString NotificationTypeSub() { return LGetHeaderField(headers, "NTS"); }
	};

	using TCallback = std::function<void(Result*)>;

protected:
	constexpr static const char *IP4_MC_ADDR = "239.255.255.250";
	constexpr static int PORT = 1900;
	LStream *log;
	LString UserAgent = "Lgi/LSsdp";
	LSocket udp;

	LArray<Result> results;	

	struct Search
	{
		LString req;
		LString filter;
		TCallback cb;
	};
	LArray<Search> searches;

public:
	LSsdp(LStream *logger) :
		LThread("LSsdp.Thread"),
		LMutex("LSsdp.Lock"),
		log(logger)
	{
		udp.SetUdp(true);
		Run();
	}
	
	~LSsdp()
	{
		Cancel();
		WaitForExit();
	}
	
	void Search(LString filter, TCallback cb, int maxResponse = 3)
	{
		if (!cb)
			return;

		Auto lck(this, _FL);
		
		auto &s = searches.New();
		s.filter = filter;
		s.cb = cb;
		s.req.Printf("M-SEARCH * HTTP/1.1\r\n"
					"Host: %s:1900\r\n"
					"Man: \"ssdp:discover\"\r\n"
					"ST: ssdp:all\r\n" // service type
					"MX: %i\r\n" // max response
					"User-Agent: %s UPnP/1.0 GSSDP/1.4.0.1\r\n"
					"\r\n",
					IP4_MC_ADDR,
					maxResponse,
					UserAgent.Get());
	}
	
	int Main()
	{
		log->Print("Ssdp setup...\n");
		auto MC_IP = LIpToInt(IP4_MC_ADDR);
		LArray<LSocket::Interface> interfaces;
		LArray<uint32_t> interface_ips;
		if (LSocket::EnumInterfaces(interfaces))
		{
			for (auto &intf: interfaces)
			{
				if (!intf.IsLoopBack())
					interface_ips.Add(intf.Ip4);
			}
		}
		LUdpListener listen(interface_ips, MC_IP, PORT, log);
	
		auto startTs = LCurrentTime();
		
		log->Print("Ssdp looping...\n");
		while (!IsCancelled())
		{
			if (listen.IsReadable(100))
			{				
				LString pkt;
				uint32_t ip;
				uint16_t port;
				if (listen.ReadPacket(pkt, ip, port))
				{
					auto &r = results.New().Set(ip, pkt);
					auto now = LCurrentTime();
					if (auto usn = r.UniqueServiceName())
					{
						#if 0
						log->Print(	"%ims: %s, %s:\n"
							"    usn=%s\n",
							(int)(now-startTs),
							LIpToStr(ip).Get(),
							r.type.Get(),
							usn.Get());
						#endif
							
						for (auto &s: searches)
						{
							if (usn.Lower().Find(s.filter.Lower()) >= 0)
								s.cb(&r);							
						}
					}
					else if (r.type.Equals("NOTIFY"))
					{
						#if 1
						log->Print("Got %i bytes from %s:%i, no USN:\n", (int)pkt.Length(), LIpToStr(ip).Get(), port);
						auto lines = pkt.SplitDelimit("\r\n");
						for (auto &ln: lines)
							log->Print("    %s\n", ln.Get());
						#endif
					}
				}
			}
			else
			{
				// Send any new searches...
				Auto lck(this, _FL);
				for (auto &s: searches)
				{
					if (s.req)
					{
						auto result = udp.WriteUdp(s.req.Get(), s.req.Length(), 0, LIpToInt(IP4_MC_ADDR), PORT);
						log->Print("WriteUdp=%i\n", result);
						if (result == s.req.Length())
							s.req.Empty();
					}
				}
			}
		}
		
		return 0;
	}
};