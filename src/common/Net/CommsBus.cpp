#include "lgi/common/Lgi.h"
#include "lgi/common/Thread.h"
#include "lgi/common/CommsBus.h"
#include "lgi/common/Net.h"
#include "lgi/common/Json.h"
#include "lgi/common/UdpTransport.h"

#define COMMBUS_MULTICAST	"230.6.6.10"
#define DEFAULT_COMMS_PORT	45454
#define RETRY_SERVER		-2
#define USE_TRANSPORT		0

#if 0
#define LOG(...)			
#else
#define LOG(...)			if (log) { log->Print(__VA_ARGS__); } // printf(__VA_ARGS__)
#endif

// All the types of messages the system uses
enum MsgIds
{
	MUid			= Lgi4CC("uuid"),
	MCreateEndpoint	= Lgi4CC("mkep"),
	MSendMsg		= Lgi4CC("send"),
	MServerState	= Lgi4CC("srvr"),
};

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct IpList : public LArray<uint32_t>
{
	constexpr static const char *sep = "|";

	LString ToString()
	{
		LString::Array ips;
		for (auto &ip: *this)
			ips.New() = LIpToStr(ip);
		return LString(sep).Join(ips);
	}

	bool operator =(LString &s)
	{
		Length(0);
		for (auto &ip: s.SplitDelimit(sep))
		{
			if (auto i = LIpToInt(ip))
				Add(i);
		}
		return true;
	}

	operator LArray<LString>()
	{
		LArray<LString> a;
		for (auto ip: *this)
			a.Add(LIpToStr(ip));
		return a;
	}
};

static bool GatewayIp(uint32_t ip)
{
	uint8_t lsb = ip & 0xff;
	uint8_t msb = (ip >> 24) & 0xff;
	return lsb == 1;
}

static LString Indent(LString s, int depth = 4)
{
	LStringPipe p;
	auto lines = s.SplitDelimit("\n");
	auto indent = LString(" ") * depth;
	for (auto ln: lines)
		p.Print("%s%s\n", indent.Get(), ln.Get());
	return p.NewLStr();
}

// A malloc'd block of memory for messages.
class Block
{
	// The message ID in network byte order
	uint32_t id;
	// The payload size, not including header, in network byte order
	uint32_t size;

public:
	// This is the start of the payload, the actual size in bytes is
	// GetSize()+nullSz for NULL termination.
	char data[1];
	
	// The size of the NULL terminator
	constexpr static int nullSz = 1;

	// An auto pointer to manage a Block reference
	using Auto = LAutoPtr<Block, false, true /* use 'free' instead of 'delete' */>;
	
	// Creates a new block of the given size
	static Auto New(uint32_t msgId, uint32_t bytes)
	{
		Auto b((Block*) malloc(sizeof(Block) + bytes));
		if (b)
		{
			b->id = htonl(msgId);
			b->size = htonl(bytes);
			b->data[0] = 0;
		}		
		return b;
	}
	
	// Creates a new block with a LString for the payload
	static Auto New(uint32_t msgId, LString s)
	{
		auto blk = New(msgId, (uint32_t)s.Length());
		if (blk && s.Get())
			memcpy(blk->data, s.Get(), s.Length() + nullSz);
		return blk;
	}

	// Creates a copy of an exist block
	Auto Clone()
	{
		Auto b;
		auto bytes = sizeof(Block) + GetSize();
		if (b.Reset((Block*) malloc(bytes)))
		{
			memcpy(b.Get(), this, bytes);
		}
		return b;
	}

	// Describes the block for logging
	LString ToString()
	{
		return LString::Fmt("id='%4.4s' sz=%i", &id, GetSize());
	}

	// Get the ID in host byte order
	uint32_t GetId() const { return ntohl(id); }

	// Get the payload size in host byte order
	uint32_t GetSize() const { return ntohl(size); }
	
	// Return the body as a string
	LString GetBody() const
	{
		return LString(data, GetSize());
	}
	
	// Return the first line as a string
	LString FirstLine() const
	{
		auto nl = Strnchr(data, '\n', GetSize());
		if (nl)
			return LString(data, nl - data);
		return LString();
	}

	// Return the second part of the message as a string
	LString SecondLine() const
	{
		auto nl = Strnchr(data, '\n', GetSize());
		if (nl)
		{
			nl++;
			auto end = data + GetSize();
			return LString(nl, end - nl);
		}
		return LString();
	}
};

#ifdef WIN32
#pragma pack(pop, before_pack)
#else
#pragma pack(pop)
#endif

struct Connection
{
	LAutoPtr<LSocket> sock;
	LArray<char> readBuf;
	ssize_t used = 0;
	bool connected = false;
	LStream *log = NULL;
	
	Connection(LStream *l) :
		readBuf(8 << 10),
		log(l)
	{
	}

	Connection(LStream *l, LAutoPtr<LSocket> socket) :
		sock(socket),
		readBuf(8 << 10),
		log(l)
	{
	}

	bool Valid()
	{
		return sock && ValidSocket(sock->Handle());
	}
	
	LString Dump(uint8_t *ptr, ssize_t len)
	{
		LStringPipe p;
		for (int i=0; i<len; i++)
		{
			char e = i % 16 == 15 ? '\n' : ' ';
			if ((ptr[i] >= 'a' && ptr[i] <= 'z') ||
				(ptr[i] >= 'A' && ptr[i] <= 'Z'))
				p.Print(" %c%c", ptr[i], e);
			else	 		
				p.Print("%2.2x%c", ptr[i], e);
		}
		return p.NewLStr();
	}

	bool Read(std::function<void(Block*)> onMsg, std::function<void()> onDisconnect)
	{
		// Check for data
		if (!sock ||
			!sock->IsReadable())
			return false;

		// Read some data:
		auto rd = sock->Read(readBuf.AddressOf() + used, readBuf.Length() - used);
		if (rd <= 0)
		{
			// printf("read disconnected: %i\n", (int)rd);
			sock->Close();
			connected = false;
			if (onDisconnect)
				onDisconnect();
			return false;
		}
		used += rd;
		// LOG("read: got %i bytes, used=%i\n", (int)rd, (int)used);
		
		// Check if there is a full msg
		bool status = false;
		while (used >= sizeof(Block))
		{
			// ie there is at least enough data to check the Block size
			if (auto b = (Block*) readBuf.AddressOf())
			{
				ssize_t bytes = b->GetSize() + sizeof(Block); // Total message size
				if (used >= bytes)
				{
					// LOG("read: got msg %i bytes\n", (int)bytes);

					// Call the callback
					if (onMsg)
						onMsg(b);

					// Remove the message from the buffer
					auto remaining = used - bytes;
					if (remaining > 0)
						memmove(readBuf.AddressOf(), readBuf.AddressOf(bytes), remaining);
					used -= bytes;
					// LOG("read: consume msg used=%i\n", (int)used);

					status = true;
				}
				else
				{
					if (bytes > (ssize_t) readBuf.Length())
					{
						ssize_t blk = 1 << 20; // 1 MiB
						ssize_t newSz = bytes;
						if (newSz % blk)
							newSz += blk - (newSz % blk);
						
						// Not enough storage for this message, try to increase the size to fit it
						if (readBuf.Length(newSz))
						{
							LOG("read: increased buf size to %s\n", LFormatSize(newSz).Get());
						}
						else
						{					
							LOG("read: not enough bytes for msg %i < %i, sizeof(Block)=%i\n", (int)used, (int)bytes, (int)sizeof(Block));
							LOG("read:\n%s\n", Dump((uint8_t*) readBuf.AddressOf(), used).Get());
						}
					}	
				
					// Otherwise... wait for more data
					break;
				}
			}
			else
			{
				// LOG("read: readbuf null\n");
				break;
			}
		}
		
		return status;
	}
	
	bool Write(Block *b)
	{
		if (!b)
			return false;

		size_t bytes = b->GetSize() + sizeof(Block);
		char *ptr = (char*)b;
		size_t i = 0;

		while (i < bytes)
		{
			auto wr = sock->Write(ptr, bytes - i);
			if (wr > 0)
			{
				i += wr;
			}
			else
			{
				LOG("write: disconnect wr=%i\n", (int)wr);
				connected = false;
				break;
			}
		}
		
		// LOG("%u write bytes=%i\n", LCurrentThreadId(), i);
		return i == bytes;
	}
};

// Info about known endpoints.
//
// An endpoint can either be local or remote, not both.
//
// The server should know about all endpoints, both local and remote.
// The clients should know about only their local endpoints.
// 
struct Endpoint
{
	// Each end point is referenced by a string name 'addr'
	// All comparisons should be case insensitive
	// Style wise endpoints should be "$app.$description"
	LString addr;
	
	// The UID of the separate process that owns the endpoint
	LString remote;	
	// The callback of the local code that wants endpoint messages
	std::function< void(LString) > local;

	// Create an end point message
	Block::Auto MakeMsg(const char *Uid)
	{
		auto data = LString::Fmt("%s\n%s", addr.Get(), Uid);
		return Block::New(MCreateEndpoint, data);		
	}

	// Json IO
	constexpr static const char *sAddr = "addr";
	constexpr static const char *sRemote = "remote";

	LJson ToJson() const
	{
		LJson j;
		j.Set(sAddr, addr);
		if (remote)
			j.Set(sRemote, remote);
		return j;
	}

	template<typename J>
	bool FromJson(J &j)
	{
		addr = j.Get(sAddr);
		remote = j.Get(sRemote);
		return true;
	}
};

struct CommsClient : public Connection
{
	LString uid;
	uint64_t lastSeen = 0;
		
	CommsClient(LStream *l, LAutoPtr<LSocket> sock) : Connection(l, sock)
	{
	}

	void OnConnect()
	{
		lastSeen = LCurrentTime();
	}
};

// A block owned by the writeQue.
struct BlockInfo
{
	// Timestamp of the first attempt to send
	uint64_t firstSendTs = 0;

	// Timestamp of the first attempt to send
	uint64_t recentSendTs = 0;

	// A block ptr owned by this object
	Block *blk = NULL;

	~BlockInfo()
	{
		if (blk)
			free(blk);
	}
};

struct LCommsBusPriv :
	public LThread,
	public LCancel,
	public LMutex
{
	constexpr static int SEND_TIMEOUT		= SECONDS(30);	// message expires from queue after this
	constexpr static int RESEND_TIMEOUT		= SECONDS(1);	// retry failed messages in the quueu this often
	constexpr static int UNSEEN_TIMEOUT		= SECONDS(30);
	constexpr static int CONNECT_ATTEMPT	= SECONDS(1);
	constexpr static int PEER_TIMEOUT		= SECONDS(5);

	struct ServerPeers
	{
		LCommsBusPriv *d = nullptr;
		LStream *log = nullptr;

		// Interface info
		LArray<LSocket::Interface> interfaces;

		// Server discovery info
		uint32_t broadcastIp = LIpToInt(COMMBUS_MULTICAST);
		uint64_t broadcastTime = 0;

		#if USE_TRANSPORT
		LAutoPtr<LUdpTransport> listener;
		#else
		LAutoPtr<LUdpListener> listener;
		#endif

		// Peer info
		struct LPeer : public Connection
		{
			constexpr static const char *sHostname = "hostname";
			constexpr static const char *sIp = "ip";
			constexpr static const char *sPeers = "peers";
			constexpr static const char *sDirect = "direct";
			constexpr static const char *sEndpoints = "endpoints";

			// replicated state
			LString hostName;
			IpList ip4;
			uint32_t effectiveIp = 0;
			LArray<LPeer*> peers;
			LArray<Endpoint*> endpoints;

			// local state
			uint64_t seenTs = 0; // last time we saw a broadcast UDP msg
			uint64_t connectTs = 0; // last time we trying to connect on TCP
			bool direct = false;

			LPeer(LStream *log) : Connection(log)
			{
			}

			LPeer(LPeer &p) : Connection(p.log)
			{
				*this = p;
			}

			~LPeer()
			{
				peers.DeleteObjects();
				endpoints.DeleteObjects();
			}

			LPeer &operator =(const LPeer &p)
			{
				hostName = p.hostName;
				ip4 = p.ip4;
				effectiveIp = p.effectiveIp;

				peers = p.peers;

				return *this;
			}

			bool IsConnected()
			{
				return sock && sock->IsOpen();
			}

			LJson ToJson(int depth = 0)
			{
				LJson j;
				
				j.Set(sHostname, hostName);
				j.Set(sIp, ip4);
				if (depth < 2)
				{
					LArray<LJson> peerArr;
					for (auto p: peers)
						peerArr.Add(p->ToJson(depth+1));
					j.Set(sPeers, peerArr);

					LArray<LJson> epArr;
					for (auto ep: endpoints)
						epArr.Add(ep->ToJson());
					j.Set(sEndpoints, epArr);
				}
				j.Set(sDirect, direct);
				j.Set("sock", IsConnected());
				return j;
			}

			template<typename J>
			bool FromJson(J &j)
			{
				hostName = j.Get(sHostname);
				ip4.Empty();
				for (auto ip: j.GetArray(sIp))
					ip4.Add(LIpToInt(ip));
				
				peers.DeleteObjects();
				for (LJson::Iter::IterPos it: j.GetArray(sPeers))
				{
					LAutoPtr<LPeer> newPeer(new LPeer(log));
					if (newPeer->FromJson(it))
						peers.Add(newPeer.Release());
				}

				endpoints.DeleteObjects();
				for (auto it: j.GetArray(sEndpoints))
				{
					LAutoPtr<Endpoint> ep(new Endpoint);
					if (ep && ep->FromJson(it))
						endpoints.Add(ep.Release());
				}
				
				return hostName && ip4.Length() > 0;
			}

			bool FromString(LString &data)
			{
				LJson j(data);
				return FromJson(j);
			}
		};
		LHashTbl<ConstStrKey<char,false>, LPeer*> peers;

		void SetDirty(const char *context)
		{
			// This forces a re-broadcast of state
			LOG("SetDirty(%s)\n", context);
			broadcastTime = 0;
		}

		ServerPeers(LCommsBusPriv *priv) : d(priv)
		{
			log = d->log;
			
			d->onEndpointChange = [this]()
			{
				// Re-broadcast the new endpoints to peers
				LOG("ServerPeers.onEndpointChange called.\n");
				SetDirty("onEndpointChange");
			};
		}
		
		~ServerPeers()
		{
			d->onEndpointChange = nullptr;
		}

		IpList GetIps(LArray<LSocket::Interface> &arr)
		{
			IpList ips;
			for (auto &intf: arr)
				ips.Add(intf.Ip4);
			return ips;
		}

		bool IsLocalIp(uint32_t ip)
		{
			if (ip == 0x7f000001)
				return true;

			for (auto &i: interfaces)
				if (i.Ip4 == ip)
					return true;
			
			return false;
		}

		void OnPeerTcp(LPeer *p, Block *blk)
		{
			switch (blk->GetId())
			{
				case MServerState:
				{
					LJson j(blk->data);

					// find matching host and replicate state:
					if (auto host = j.Get(LPeer::sHostname))
					{
						if (auto p = peers.Find(host))
						{
							// copy state
							p->FromJson(j);

							#if 0
							LOG("MServerState %s, endpoints=%i\n", host.Get(), (int)p->endpoints.Length());
							for (auto ep: p->endpoints)
								LOG("MServerState %s, ep=%s\n", host.Get(), ep->addr.Get());
							#endif
						}
						else LOG("%s: no host '%s'\n", __FUNCTION__, host.Get());
					}
					else LOG("%s: no host key\n", __FUNCTION__);
					break;
				}
				case MSendMsg:
				{
					// Message from remote host server, deliver to any local processes
					LOG("Got msg sent from remote machine...\n");
					d->ServerSend(blk, true /* don't allow loops back and forth between servers */);
					break;
				}
			}
		}

		void OnPeerUdp(uint32_t ip, LString &msg)
		{
			// Check if it's from ourselves...
			if (IsLocalIp(ip))
				return;
				
			// Parse the message:
			LPeer p(log);
			if (!p.FromString(msg))
			{
				LOG("error: failed to decode pkt\n");
				return;
			}

			auto peer = peers.Find(p.hostName);
			if (!peer)
			{
				OnNewPeer(peer = new LPeer(p));
			}
			else
			{
				// update the existing peer
				peer->ip4 = p.ip4;
				peer->peers = p.peers;
			}
			if (!peer)
			{
				LOG("error: no peer ptr.\n");
				return;
			}

			peer->direct = true;
			peer->seenTs = LCurrentTime();

			// Figure out the effective ip address
			if (!peer->effectiveIp)
			{
				uint32_t mask = 0xffffff00;
				for (auto peerIp: peer->ip4)
				{
					if ( (peerIp & mask) == (ip & mask) )
					{
						peer->effectiveIp = peerIp;
						LOG("effectiveIp for %s is %s\n", peer->hostName.Get(), LIpToStr(peer->effectiveIp).Get());
						break;
					}
				}
			}

			for (auto other: p.peers)
			{
				if (other->hostName.Equals(d->hostName))
					continue; // don't care about this node

				if (!peers.Find(other->hostName))
				{
					if (auto *n = new LPeer(log))
					{
						n->ip4 = other->ip4;
						n->hostName = other->hostName;
						peers.Add(other->hostName, n);
						SetDirty("addPeer");
					}
				}
			}
		}

		void OnInterfacesChange()
		{
			auto interface_ips = GetIps(interfaces);
			for (auto &intf: interfaces)
			{
				LOG("interfaceChange: %s %s\n", intf.Name.Get(), LIpToStr(intf.Ip4).Get());
			}

			// Create a UDP listener on current interfaces...
			#if USE_TRANSPORT
				if (listener.Reset(new LUdpTransport(interface_ips, broadcastIp, DEFAULT_COMMS_PORT, d->log)))
					listener->onReceive = [this](auto ip, auto msg) { OnPeerMsg(ip, msg); };
			#else
				listener.Reset(new LUdpListener(interface_ips, broadcastIp, DEFAULT_COMMS_PORT, d->log));
			#endif
			
			SetDirty("OnInterfacesChange");
		}

		void OnNewPeer(LPeer *newPeer)
		{
			if (newPeer)
			{
				LOG("newPeer: %s\n", newPeer->hostName.Get());
				peers.Add(newPeer->hostName, newPeer);
				SetDirty("onNewPeer");
			}
		}

		void OnDeletePeer(LPeer *peer)
		{
			LOG("deletePeer: %s (%p)\n", peer->hostName.Get(), peer);
			delete peer;
			SetDirty("onDeletePeer");
		}

		LArray<LJson> PeersToJson()
		{
			LArray<LJson> a;
			for (auto p: peers)
				a.Add(p.value->ToJson());
			return a;
		}

		LArray<LJson> EndpointsToJson()
		{
			LMutex::Auto lck(d, _FL);
			LArray<LJson> a;
			for (auto ep: d->endpoints)
				a.Add(ep.ToJson());
			return a;
		}

		// This is sent via UDP broadcast to peers during discovery
		LString CreatePingData()
		{
			LJson j;

			j.Set(LPeer::sIp, GetIps(interfaces));
			j.Set(LPeer::sHostname, LHostName());

			return j.GetJson();
		}

		// This is sent via TCP to peers when state changes
		LString ServerStateData()
		{
			LJson j;

			j.Set(LPeer::sIp, GetIps(interfaces));
			j.Set(LPeer::sHostname, LHostName());
			j.Set(LPeer::sPeers, PeersToJson());
			j.Set(LPeer::sEndpoints, EndpointsToJson());

			return j.GetJson();
		}

		void UpdatePeerState()
		{
			auto state = ServerStateData();
			for (auto it: peers)
			{
				LPeer *p = it.value;
				if (p->IsConnected())
				{
					// LOG("Sending MServerState to %s\n%s\n", p->hostName.Get(), state.Get());
					if (auto blk = Block::New(MServerState, (uint32_t) state.Length()))
					{
						memcpy(blk->data, state.Get(), blk->GetSize());
						p->Write(blk);
					}
				}
			}
		}

		void BroadcastUdp()
		{
			broadcastTime = LCurrentTime();

			// And then broadcast packets...
			LString pkt = CreatePingData();

			for (auto &intf: interfaces)
			{
				LUdpBroadcast bc(intf.Ip4);
				// LOG("broadcast to %s\n", LIpToStr(intf.Ip4).Get());
				bc.BroadcastPacket(pkt, broadcastIp, DEFAULT_COMMS_PORT);
			}

			// Also unicast them... in case the broadcast is only working in one direction...
			LSocket udp;
			udp.SetUdp(true);
			for (auto &i: peers)
			{
				auto p = i.value;					
				if (p->effectiveIp)
				{
					int wr = udp.WriteUdp(pkt.Get(), (int)pkt.Length(), 0, p->effectiveIp, DEFAULT_COMMS_PORT);
					if (wr != pkt.Length())
					{
						LOG("udp %s wr=%i\n", LIpToStr(p->effectiveIp).Get(), wr);
					}
				}
				else if (p->direct)
				{
					LOG("no effective ip for: %s %s\n", p->hostName.Get(), p->ip4.ToString().Get());
				}
			}
		}

		uint64_t logTs = 0;
		void Timeslice()		
		{
			// Inter-server discovery...

			// For inter-server connection discovery, get a list of interfaces to broadcast to
			LArray<LSocket::Interface> curIntf;
			LSocket::EnumInterfaces(curIntf);
			for (int i=0; i<curIntf.Length(); i++)
			{
				if (curIntf[i].IsLoopBack()) // We don't care about the local
					curIntf.DeleteAt(i--);
			}
			// sort so we can compare properly:
			curIntf.Sort([](auto *a, auto *b) { return a->Ip4 - b->Ip4; });
			
			auto diff = GetIps(curIntf) != GetIps(interfaces);
			bool debug = false;

			if (diff)
			{
				interfaces = curIntf;
				OnInterfacesChange();
			}			
			
			// Check for incoming broadcasts:
			if (listener)
			{
				// Listen for packets...
				#if USE_TRANSPORT
					listener->TimeSlice();
				#else
					if (listener->IsReadable(10))
					{
						uint32_t ip;
						uint16_t port;
						LString msg;
						if (listener->ReadPacket(msg, ip, port))
							OnPeerUdp(ip, msg);
					}
				#endif
			}

			// Check for peers that we haven't seen in a while
			auto now = LCurrentTime();
			LString::Array deletedPeers;
			for (auto p: peers)
			{
				if (p.value->direct &&
					now - p.value->seenTs > UNSEEN_TIMEOUT)
				{
					LString hostName = p.key;
					OnDeletePeer(p.value);
					deletedPeers.Add(hostName);
				}
			}
			for (auto host: deletedPeers)
				peers.Delete(host);

			// Check peers for connections and data
			for (auto it: peers)
			{
				LPeer *p = it.value;
				if (p->IsConnected())
				{
					// Check for data on the connection:
					p->Read
					(
						// On message:
						[this, p](Block *blk)
						{
							OnPeerTcp(p, blk);
						},
						// On disconnect:
						[this, p]()
						{
							LOG("Peer disconnected: %s\n", p->hostName.Get());
						}
					);
				}
				else if (now - p->connectTs >= CONNECT_ATTEMPT)
				{
					// Try and setup a TCP connection:
					p->connectTs = now;
					if (!p->sock)
					{
						if (p->sock.Reset(new LSocket))
						{
							p->sock->SetTimeout(PEER_TIMEOUT);
							p->sock->IsBlocking(false);
						}
					}
					if (p->sock)
					{
						auto addr = LIpToStr(p->effectiveIp);
						LOG("peer connecting: %s/%s\n", addr.Get(), p->hostName.Get());
						auto result = p->sock->Open(addr, DEFAULT_COMMS_PORT);
						LOG("\tconnected: %i\n", result);
						if (result)
							SetDirty("newPeerTcpConnection");
					}
				}
			}

			now = LCurrentTime();
			if (now - broadcastTime >= 10000)
			{
				debug = true;
				BroadcastUdp();
				UpdatePeerState();
			}

			if (debug)
			{
				if (d->commsState)
					d->commsState->RunCallback([view=d->commsState, state=ServerStateData()]()
					{
						view->Name(state);
					});
				/*
				else
					LOG("state: writeQue=%i\n%s\n",
						(int)d->writeQue.Length(),
						ServerStateData().Get());
				*/
			}
		}
	};

	LAutoPtr<ServerPeers> peers;

	bool isServer = false;
	LSocket listen;
	LStream *log = nullptr;
	LView *commsState = nullptr;
	LString Uid;
	LString hostName;
	
	// Lock before using
	LArray< BlockInfo > writeQue;
	LArray< Endpoint > endpoints;
	LCommsBus::TCallback callback;

	// Events
	std::function<void()> onEndpointChange;

	// Server only:
	uint64_t trySendTs = 0;
	LArray<CommsClient*> clients;
		
	LCommsBusPriv(LStream *Log, LView *commsstate) :
		LThread("LCommsBusPriv.Thread"),
		LMutex("LCommsBusPriv.Lock"),
		log(Log),
		commsState(commsstate)
	{
		LAssert(sizeof(Block) == 9);
		hostName = LHostName();
		Run();
	}
	
	~LCommsBusPriv()
	{
		Cancel();
		WaitForExit();
	}
	
	void NotifyState(LCommsBus::TState state)
	{
		LMutex::Auto lck(this, _FL);
		if (callback)
			callback(state);
	}

	const char *GetUid()
	{
		// Make sure the thread has run far enough to create the UID
		while (!Uid.Get())
			LSleep(1);

		return Uid;
	}

	LString Describe()
	{
		return LString::Fmt("%s:%s", isServer ? "server" : "client", GetUid());
	}

	void Que(Block::Auto &blk)
	{
		Auto lck(this, _FL);

		// LOG("%s que msg '%s'\n", Describe().Get(), blk->ToString().Get());

		auto &info = writeQue.New();
		info.firstSendTs = 0;
		info.recentSendTs = 0;
		info.blk = blk.Release();

		/*
		for (auto &i: writeQue)
		{
			LOG("	writeque: %s\n", i.blk->ToString().Get());
		}
		*/
	}

	bool ServerSend(Block *blk, bool localOnly)
	{
		bool status = false;

		if (blk->GetId() != MSendMsg)
		{
			LAssert(!"wrong msg type");
			return false;
		}

		if (auto dest = blk->FirstLine())
		{
			if (Lock(_FL))
			{
				bool foundEndpoint = false;

				// Look through any localhost endpoints...
				for (auto &e: endpoints)
				{
					if (!e.addr.Equals(dest))
						continue;

					foundEndpoint = true;
					if (e.local) // local process endpoint
					{
						e.local(blk->SecondLine());
						status = true;
					}
					else if (e.remote) // separate process... not remote host
					{
						// Find the matching connection and send to the remote end
						bool foundClient = false;
						for (auto &c: clients)
						{
							if (c->uid == e.remote)
							{
								foundClient = true;
										
								status = c->Write(blk);
								if (!status)
								{
									LOG("%s error: write failed.\n", Describe().Get());
								}
								break;
							}
						}
						if (!foundClient)
						{
							LOG("%s error: no client for endpoint='%s' remote=%s\n",
								Describe().Get(),
								dest.Get(),
								e.remote.Get());
						}
					}
					break;
				}

				// Now check through any remote server (other machines)
				if (!localOnly && !foundEndpoint && peers)
				{
					for (auto it: peers->peers)
					{
						ServerPeers::LPeer *p = it.value;
						for (auto ep: p->endpoints)
						{
							if (ep->addr.Equals(dest))
							{
								foundEndpoint = true;

								bool conn = p->IsConnected();
								LOG("Found remote server endpoint: %s, conn=%i\n", ep->addr.Get(), conn);
								if (conn)
								{
									status = p->Write(blk);
									LOG("Write to remote server: %s = %i\n", ep->addr.Get(), status);
								}
							}
						}
						if (status)
							break;
					}
				}

				Unlock();

				/*
				if (!foundEndpoint)
				{
					LOG("%s error: no endpoint '%s'\n",
						Describe().Get(),
						dest.Get());
				}
				*/
			}
		}
		else
		{
			LOG("%s No separator?\n", Describe().Get());
		}

		return status;
	}
	
	void ServerTrySend(bool firstAttempt)
	{
		LAssert(isServer);

		if (Lock(_FL))
		{
			for (size_t i=0; i<writeQue.Length(); i++)
			{
				auto &info = writeQue[i];
				switch (info.blk->GetId())
				{
					case MSendMsg:
					{
						auto now = LCurrentTime();
						bool timedOut = info.firstSendTs && ((now - info.firstSendTs) >= SEND_TIMEOUT);
						if (timedOut)
						{
							LOG("Deleting expired msg: %s\n%s\n",
								info.blk->FirstLine().Get(),
								peers->ServerStateData().Get());
							writeQue.DeleteAt(i--, true);
						}
						else if (!info.firstSendTs || (now - info.recentSendTs) > RESEND_TIMEOUT)
						{
							if (!info.firstSendTs)
								info.firstSendTs = now;
							info.recentSendTs = now;
							if (ServerSend(info.blk, false))
							{
								#if 1
									// LOG("%s sent %s to %s\n", Describe().Get(), info.blk->ToString().Get(), info.blk->FirstLine().Get());
								#else
									auto bytes = info.blk->GetSize() + 9;
									LString::Array b;
									b.SetFixedLength(false);
									auto ptr = (uint8_t*)info.blk;
									for (unsigned i=0; i<bytes; i++)
										b.New().Printf("%i", ptr[i]);
									LOG("%s write %s\n", Describe().Get(), LString(",").Join(b).Get());
								#endif

								writeQue.DeleteAt(i--, true);
							}
							#if 0
							else
							{
								LOG("%s: failed to write to %s\n%s\n",
									__FUNCTION__,
									info.blk->FirstLine().Get(),
									peers->ServerStateData().Get());
							}
							#endif
						}
						break;
					}
					case MCreateEndpoint:
					{
						// This can happen when a new comms bus, which starts in client mode, listens on an endpoint
						// but then becomes a server. So it's ok to discard the message.
						writeQue.DeleteAt(i--, true);
						break;
					}
					default:
					{
						LOG("%s error: can't send msg type %s\n", Describe().Get(), info.blk->ToString().Get());
						writeQue.DeleteAt(i--, true);
						break;
					}
				}
			}

			Unlock();
		}

		#if 0
		if (writeQue.Length() > 0)
			LOG("%s warning: " LPrintfSizeT " msgs still queued on the server\n", Describe().Get(), writeQue.Length());
		#endif
	}

	void OnClientDisconnect(CommsClient *c)
	{
		// Clear out any endpoints that belonged to the disconnecting client:
		Auto lck(this, _FL);
		LAssert(isServer);
		bool changed = false;
		for (unsigned i=0; i<endpoints.Length(); i++)
		{
			auto &ep = endpoints[i];
			bool match = ep.remote == c->uid;
			if (match)
			{
				LOG("%s: removing endpoint %s for client %s\n", __FUNCTION__, ep.addr.Get(), c->uid.Get());
				endpoints.DeleteAt(i--);
				changed = true;
			}
		}
		if (changed &&
			onEndpointChange)
		{
			onEndpointChange();
		}
	}

	int Server()
	{
		if (!peers.Reset(new ServerPeers(this)))
			return -1;

		// Wait for incoming connections and handle them...
		bool hasConnections = false;
		NotifyState(LCommsBus::TDisconnectedServer);

		while (!IsCancelled())
		{
			peers->Timeslice();

			if (listen.IsReadable())
			{
				// Setup a new incoming TCP connection. Connections
				// start off as a client, but may in fact be a server
				// on a different host
				LAutoPtr<LSocket> sock(new LSocket(log));
				if (sock && listen.Accept(sock))
				{
					uint32_t remoteIp = 0;
					if (!sock->GetRemoteIp(&remoteIp))
					{
						LOG("Error: GetRemoteIp failed.\n");
					}
					else if (!peers->IsLocalIp(remoteIp))
					{
						// server connection..
						bool found = false;
						for (auto it: peers->peers)
						{
							ServerPeers::LPeer *p = it.value;
							LOG("ip check %s - %s\n", LIpToStr(remoteIp).Get(), p->ip4.ToString().Get());
							if (p->ip4.HasItem(remoteIp))
							{
								// found matching..
								if (p->IsConnected())
								{
									LOG("Ignoring new accept for existing connection for: %s/%i\n",
										LIpToStr(remoteIp).Get(),
										p->hostName.Get());
								}
								else
								{
									p->sock = sock;
									found = true;
									peers->SetDirty("acceptedServerTcp");
	
									LOG("Incomming server connection existing peer: %s/%s\n",
										LIpToStr(remoteIp).Get(),
										p->hostName.Get());
								}
							}
						}
						if (!found)
						{
							LOG("Incomming server connection, new peer: '%s'\n", LIpToStr(remoteIp).Get());
						}
					}
					else if (auto conn = new CommsClient(log, sock))
					{
						clients.Add(conn);
						conn->OnConnect();
						LOG("%s got new client connection, sock=" LPrintfSock "\n",
							Describe().Get(),
							conn->sock->Handle());
					}
				}
			}
			else
			{
				// Check connections for incoming data:
				for (auto c: clients)
				{
					if (!ValidSocket(c->sock->Handle()))
					{
						clients.Delete(c);
						delete c;

						LOG("%s client disconnected... (len=%i)\n", Describe().Get(), (int)clients.Length());
						break;
					}

					c->Read
					(
						// On message:
						[this, c](auto blk) mutable
						{
							// LOG("%s received msg %s\n", Describe().Get(), blk->ToString().Get());
							switch (blk->GetId())
							{
								case MUid:
								{
									c->uid = blk->data;
									LOG("%s set client uid=%s\n", Describe().Get(), c->uid.Get());
									break;
								}
								case MCreateEndpoint:
								{
								    auto body = blk->GetBody();
									auto lines = body.SplitDelimit("\r\n");
									if (lines.Length() != 2)
									{
										LOG("%s error in ep fmt=%s\n", Describe().Get(), blk->data);
									}
									else
									{
										auto ep = lines[0];
										auto remoteUid = lines[1];
										
										LOG("%s: create ep: %s, %s\n'%s'\n",
										    Describe().Get(), ep.Get(), remoteUid.Get(), body.Get());
										
										if (Lock(_FL))
										{
											bool found = false;
											for (auto &e: endpoints)
											{
												if (e.addr == ep)
												{
													found = true;
													e.remote = remoteUid;
													OnEndpoint(e);
													break;
												}
											}
											if (!found)
											{
												auto &e = endpoints.New();
												e.addr = ep;
												e.remote = remoteUid;
												OnEndpoint(e);
											}
											Unlock();
										}
									}
									break;
								}
								case MSendMsg:
								{
									if (!ServerSend(blk, false))
									{
										// This can happen when the server doesn't know about the endpoint yet...
										// So put the message in the writeQue and hopefully the endpoint will
										// turn up soon.
										auto copy = blk->Clone();
										Que(copy);
									}
									break;
								}
								default:
								{
									LOG("%s error unknown msg %s\n", Describe().Get(), blk->ToString().Get());
									break;
								}
							}
						
							c->lastSeen = LCurrentTime();
						},
						// On disconnect:
						[this, c]()
						{
							// Remove any end points that the client has:
							OnClientDisconnect(c);
						}
					);
				}

				// Check writeQue for outgoing data...
				auto now = LCurrentTime();
				if (now - trySendTs >= 100)
				{
					trySendTs = now;
					ServerTrySend(true);
				}

				LSleep(10);
			}

			bool connected = false;
			for (auto &c: clients)
			{
				if (c->connected)
				{
					connected = true;
					break;
				}
			}
			if (hasConnections ^ connected)
			{
				hasConnections = connected;
				NotifyState(hasConnections ? LCommsBus::TConnectedServer : LCommsBus::TDisconnectedServer);
			}
		}
		
		clients.DeleteObjects();

		LOG("%s closing listen port: " LPrintfSock "\n", Describe().Get(), listen.Handle());
		listen.Close();

		return 0;
	}
	
	void OnEndpoint(Endpoint &e)
	{
		LOG("%s OnEndpoint: addr=%s remote=%s local=%i\n",
			Describe().Get(),
			e.addr.Get(),
			e.remote.Get(),
			(bool)e.local);

		if (isServer)
		{
			// Seeing as a new endpoint turned up... try and send any queued messages
			ServerTrySend(false);
		}
		
		// Send the change event if configured:
		if (onEndpointChange)
			onEndpointChange();
	}

	void Sleep(int ms)
	{
		auto end = LCurrentTime() + ms;
		while (!IsCancelled())
		{
			if (LCurrentTime() >= end)
				break;
			LSleep(10);
		}
	}

	int Client()
	{
		LAutoPtr<LSocket> initSocket(new LSocket);
		Connection c(log, initSocket);
		int connectErrs = 0;
		bool hasConnection = false;
		NotifyState(LCommsBus::TDisconnectedClient);

		while (!IsCancelled())
		{
			if (!c.connected)
			{
				// Connect to the server...
				c.connected = c.sock->Open("localhost", DEFAULT_COMMS_PORT);
				LOG("%s connected=%i, que=%i, ep=%i\n",
					Describe().Get(),
					c.connected,
					(int)writeQue.Length(),
					(int)endpoints.Length());
				if (c.connected)
				{
					auto blk = Block::New(MUid, GetUid());
					if (!c.Write(blk))
					{
						LOG("%s write failed.\n", Describe().Get());
					}
					else
					{
						// Also tell the server about our local endpoints
						if (Lock(_FL))
						{
							LOG("clientConnect: has %i endpoints\n", (int)endpoints.Length());
							for (auto &ep: endpoints)
							{
								if (ep.local)
								{
									LOG("clientConnect: send local ep %s\n", ep.addr.Get());
									if (auto blk = ep.MakeMsg(GetUid()))
									{
										auto sent = c.Write(blk);
										if (!sent)
											LOG("%s:%i - %s error sending endpoint %s, %i bytes\n'%s'\n",
												_FL, Describe().Get(), ep.addr.Get(), blk->GetSize(), blk->GetBody().Get());
									}
								}
							}
							Unlock();
						}
					}
				}
				else
				{
					Sleep(1000);
					if (connectErrs++ > 5)
						// If there enough errors connecting to the server, maybe this object should be the server?
						// Back out of the client code and restart as the server.
						//
						// Linux: this often happens when the OS is locking out listening on the port after a dirty 
						// shutdown.
						return RETRY_SERVER;
				}
			}
			else
			{
				c.Read
				(
					// On message:
					[this](auto blk)
					{
						// LOG("%s received msg %s\n", Describe().Get(), blk->ToString().Get());
						switch (blk->GetId())
						{
							case MSendMsg:
							{
								if (Lock(_FL))
								{
									auto to = blk->FirstLine();
									for (auto &ep: endpoints)
									{
										if (ep.addr.Equals(to))
										{
											if (ep.local)
												ep.local(blk->SecondLine());
											else
												LOG("%s error: not local ep %s\n", Describe().Get(), to.Get());
											break;
										}
									}

									Unlock();
								}
								break;
							}
							default:
								break;
						}
					},
					// On disconnect, do nothing...
					// the above code will reconnect when it can.
					nullptr
				);
				
				if (Lock(_FL))
				{
					// Write any outgoing messages
					for (size_t i=0; i<writeQue.Length(); i++)
					{
						auto now = LCurrentTime();
						auto &info = writeQue[i];
						if (!info.firstSendTs || (now - info.recentSendTs >= SEND_TIMEOUT))
						{
							if (c.Write(info.blk))
							{
								#if 0
								LOG("%s wrote msg %s\n'%s'\n", Describe().Get(),
									info.blk->ToString().Get(), info.blk->GetBody().Get());
								#endif
								writeQue.DeleteAt(i--, true);
							}
							else
							{
								LOG("%s error: writing msg %s\n", Describe().Get(), info.blk->ToString().Get());
								now = LCurrentTime();
								if (info.firstSendTs)
									info.firstSendTs = now;
								info.recentSendTs = now;
							}
						}
					}
					Unlock();
				}
				
				Sleep(10);
			}

			if (c.connected ^ hasConnection)
			{
				hasConnection = c.connected;
				NotifyState(hasConnection ? LCommsBus::TConnectedClient : LCommsBus::TDisconnectedClient);
			}
		}		

		return 0;
	}
	
	int Main()
	{
		Uid.Printf("%u.%u", LProcessId(), LCurrentThreadId());

		// Try and listen on the comms port..
		// If it succeeds, this is the first process to connect... and will be the server
		// If it fails this process becomes a client.
		while (!IsCancelled())
		{
			isServer = listen.Listen(DEFAULT_COMMS_PORT);
			LOG("%s created\n", Describe().Get());
			
			if (isServer)
				return Server();
			else
			{
				auto r = Client();
				if (r != RETRY_SERVER)
					return r;
			}
		}

		return 0;
	}
};

LCommsBus::LCommsBus(LStream *logger, LView *commsState) :
	log(logger)
{
	d = new LCommsBusPriv(logger, commsState);
}

LCommsBus::~LCommsBus()
{
	delete d;
}

LString LCommsBus::GetHostName() const
{
	return d->hostName;
}

bool LCommsBus::IsServer() const
{
	return d->isServer;
}

bool LCommsBus::IsRunning() const
{
	return !d->Uid.IsEmpty();
}

bool LCommsBus::SendMsg(LString endPoint, LString data)
{
	LAssert(endPoint);

	auto bytes = (uint32_t) (endPoint.Length() + data.Length() + 1);
	if (auto msg = Block::New(MSendMsg, bytes))
	{
		char *c = msg->data;
		memcpy(c, endPoint.Get(), endPoint.Length());
		c += endPoint.Length();
		*c++ = '\n';
		memcpy(c, data.Get(), data.Length());
		c += data.Length();
		*c = 0;
		LAssert(c - msg->data == bytes);

		d->Que(msg);
		return true;
	}

	return false;
}

bool LCommsBus::Listen(LString endPoint, std::function<void(LString)> cb)
{
	if (!d->Lock(_FL))
		return false;

	bool exists = false;
	LString msg;
	for (auto &e: d->endpoints)
	{
		if (e.addr == endPoint)
		{
			 // update existing
			exists = true;
			e.local = cb;
			e.remote.Empty();
			msg.Printf("%s\n%s", endPoint.Get(), d->GetUid());
			d->OnEndpoint(e);
		}
	}
	if (!exists)
	{
		// Create it...
		auto &e = d->endpoints.New();
		e.addr = endPoint;
		e.local = cb;
		msg.Printf("%s\n%s", endPoint.Get(), d->GetUid());
		d->OnEndpoint(e);
	}
	d->Unlock();

	if (msg &&
		!d->isServer) // The server doesn't need to tell anyone else about endpoints
	{
	    LOG("%s:%i - MCreateEndpoint queued: '%s'\n", _FL, msg.Get());
		if (auto blk = Block::New(MCreateEndpoint, msg))
		{
			LOG("%s: queuing MCreateEndpoint\n", __FUNCTION__);
			d->Que(blk);
		}
		else
		{
			LOG("%s: Block::New failed\n", __FUNCTION__);
		}
	}
	else
	{
		LOG("%s: is server\n", __FUNCTION__);
	}

	return true;
}

void LCommsBus::SetCallback(TCallback cb)
{
	LMutex::Auto lck(d, _FL);
	d->callback = std::move(cb);	
}

// Unit testing:
struct CommsBusUnitTests : public LThread
{
	LStream *log;
	int timeMs = 10000;

	CommsBusUnitTests(LStream *l) :
		LThread("CommsBusUnitTest.Thread"),
		log(l)
	{
		Run();
	}

	LAutoPtr<LCommsBus> CreateBus(bool waitServer = false)
	{
		LAutoPtr<LCommsBus> bus(new LCommsBus(log));
		if (waitServer)
		{
			while (!bus->IsServer())
				LSleep(1);
		}
		else
		{
			while (!bus->IsRunning())
				LSleep(1);
		}
		return bus;
	}

	void WaitResult(bool &result)
	{
		auto start = LCurrentTime();
		while (!result)
		{
			auto now = LCurrentTime();
			if (now - start > timeMs)
				break;
			LSleep(10);
		}
	}

	bool ClientToServer()
	{
		log->Print("---- ClientToServer...\n");
		auto srv = CreateBus(true);
		auto cli = CreateBus();
		bool result = false;

		if (srv && cli)
		{
			auto ep = "Test.ClientToServer";
			auto testData = "testData";
		
			log->Print("---- Starting listen..\n");
			srv->Listen(ep, [&](auto data)
			{
				if (data.Equals(testData))
					result = true;
			});
		
			log->Print("---- Sending..\n");
			auto send = cli->SendMsg(ep, testData);
			if (!send)
				log->Print("---- Sending failed.\n");
			
			log->Print("---- Waiting..\n");
			WaitResult(result);
			log->Print("---- %s\n", result ? "SUCCESS" : "FAIL");
		}
		else log->Print("%s:%i - error: missing obj.\n", _FL);

		return result;
	}

	bool ServerToClient()
	{
		log->Print("---- ServerToClient...\n");
		auto srv = CreateBus(true);
		auto cli = CreateBus();
		bool result = false;

		if (srv && cli)
		{
			auto ep = "Test.ServerToClient";
			auto testData = "testData";
		
			log->Print("---- Starting listen..\n");
			cli->Listen(ep, [&](auto data)
			{
				if (data.Equals(testData))
					result = true;
			});
		
			log->Print("---- Sending..\n");
			srv->SendMsg(ep, testData);

			log->Print("---- Waiting..\n");
			WaitResult(result);
			log->Print("---- %s\n", result ? "SUCCESS" : "FAIL");
		}
		else log->Print("%s:%i - error: missing obj.\n", _FL);

		return result;
	}

	bool ClientToClient()
	{
		log->Print("---- ServerToClient...\n");
		auto srv = CreateBus(true);
		auto cli1 = CreateBus();
		auto cli2 = CreateBus();
		bool result = false;

		if (srv && cli1 && cli2)
		{
			auto ep = "Test.ClientToClient";
			auto testData = "testData";
		
			log->Print("---- Starting listen..\n");
			LAssert(!cli2->IsServer());
			cli2->Listen(ep, [&](auto data)
			{
				if (data.Equals(testData))
					result = true;
			});
		
			log->Print("---- Sending..\n");
			LAssert(!cli1->IsServer());
			cli1->SendMsg(ep, testData);

			log->Print("---- Waiting..\n");
			WaitResult(result);
			log->Print("---- %s\n", result ? "SUCCESS" : "FAIL");
		}
		else log->Print("%s:%i - error: missing obj.\n", _FL);

		return result;
	}

	int Main()
	{
		bool status;
		
		#if 1
		status = ClientToServer();
		LAssert(status);
		#endif

		#if 1
		log->Print("\n\n");
		status = ServerToClient();
		LAssert(status);
		#endif

		#if 1
		log->Print("\n\n");
		status = ClientToClient();
		LAssert(status);
		#endif

		return 0;
	}
};

void LCommsBus::UnitTests(LStream *log)
{
	new CommsBusUnitTests(log);
}
