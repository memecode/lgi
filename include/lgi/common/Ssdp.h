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
	LString UserAgent;
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
		auto os = LGetOsName();
		LArray<int> osVer;
		LGetOs(&osVer);
		UserAgent.Printf("%s/%i.%i UPnP/1.1 Lgi/LSsdp", os, osVer[0], osVer[1]);

		udp.SetUdp(true);
		Run();
	}
	
	~LSsdp()
	{
		Cancel();
		WaitForExit();
	}
	
	LString GetUserAgent() const { return UserAgent; }
	void SetUserAgent(LString ua) { UserAgent = ua; }

	void Search(LString filter, TCallback cb, int maxResponse = 2)
	{
		if (!cb)
			return;

		Auto lck(this, _FL);
		
		auto &s = searches.New();
		s.filter = filter;
		s.cb = cb;
		s.req.Printf("M-SEARCH * HTTP/1.1\r\n"
					"Host: %%s:1900\r\n"
					"MAN: \"ssdp:discover\"\r\n"
					// "ST: ssdp:all\r\n" // service type
					"ST: upnp:rootdevice\r\n"
					"MX: %i\r\n" // max response
					"User-Agent: %s\r\n"
					"\r\n",
					maxResponse,
					UserAgent.Get());
	}
	
	int Main()
	{
		log->Print("Ssdp setup: %s\n", UserAgent.Get());
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
					#if 0
					auto eol = pkt.Find("\r");
					auto firstLine = pkt(0, eol);
					log->Print("Got %i bytes from %s:%i '%s'\n", (int)pkt.Length(), LIpToStr(ip).Get(), port, firstLine.Get());
					#endif

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
						LArray<uint32_t> ips = { MC_IP/*, 0xffffffff*/ };
						for (auto ip: ips)
						{
							LString pkt = LString::Fmt(s.req, LIpToStr(ip).Get());
							auto result = udp.WriteUdp(pkt.Get(), (int)pkt.Length(), 0, ip, PORT, 2);
							log->Print("Ssdp: search sent... %i to %s\n", result, LIpToStr(ip).Get());
						}

						s.req.Empty();
					}
				}
			}
		}
		
		return 0;
	}
};