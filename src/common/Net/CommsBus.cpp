#include "lgi/common/Lgi.h"
#include "lgi/common/Thread.h"
#include "lgi/common/CommsBus.h"
#include "lgi/common/Net.h"

#define COMMBUS_MULTICAST	"230.6.6.10"
#define DEFAULT_COMMS_PORT	45454
#define RETRY_SERVER		-2
#define RESEND_TIMEOUT		1000

#if 0
#define LOG(...)			
#else
#define LOG(...)			if (log) log->Print(__VA_ARGS__)
#endif

// All the types of messages the system uses
enum MsgIds
{
	MUid			= Lgi4CC("uuid"),
	MCreateEndpoint	= Lgi4CC("mkep"),
	MSendMsg		= Lgi4CC("send"),
};

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

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
		if (blk)
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
	LSocket sock;
	LArray<char> readBuf;
	ssize_t used = 0;
	bool connected = false;
	LStream *log = NULL;
	
	Connection(LStream *l) :
		readBuf(8 << 10),
		log(l)
	{
	}

	bool Valid()
	{
		return ValidSocket(sock.Handle());
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

	bool Read(std::function<void(Block*)> cb)
	{
		// Check for data
		if (!sock.IsReadable())
			return false;

		// Read some data:
		auto rd = sock.Read(readBuf.AddressOf() + used, readBuf.Length() - used);
		if (rd <= 0)
		{
			// printf("read disconnected: %i\n", (int)rd);
			sock.Close();
			connected = false;
			return false;
		}
		used += rd;
		// LOG("read: got %i bytes, used=%i\n", (int)rd, (int)used);
		
		// Check if there is a full msg
		bool status = false;
		while (used >= sizeof(Block))
		{
			if (auto b = (Block*) readBuf.AddressOf())
			{
				ssize_t bytes = b->GetSize() + sizeof(Block);
				if (used >= bytes)
				{
					// LOG("read: got msg %i bytes\n", (int)bytes);

					// Call the callback
					cb(b);

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
					LOG("read: not enough bytes for msg %i < %i, sizeof(Block)=%i\n", (int)used, (int)bytes, (int)sizeof(Block));
					LOG("read:\n%s\n", Dump((uint8_t*) readBuf.AddressOf(), used).Get());
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
			auto wr = sock.Write(ptr, bytes - i);
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
	
	// The UID of the remote entity that owns the endpoint
	LString remote;	
	// The callback of the local code that wants endpoint messages
	std::function< void(LString) > local;

	// Create an end point message
	Block::Auto MakeMsg(const char *Uid)
	{
		auto data = LString::Fmt("%s\n%s", addr.Get(), Uid);
		return Block::New(MCreateEndpoint, data);		
	}
};

struct CommsClient : public Connection
{
	LString uid;
	uint64_t lastSeen = 0;
		
	CommsClient(LStream *l) : Connection(l)
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
	// Timestamp of the last attempt to send
	uint64_t sendTs = 0;

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
	bool isServer = false;
	LSocket listen;
	LStream *log = NULL;
	LString Uid;
	
	// Lock before using
	LArray< BlockInfo > writeQue;
	LArray< Endpoint > endpoints;
	LCommsBus::TCallback callback;

	// Server only:
	uint64_t trySendTs = 0;
	LArray<CommsClient*> clients;
		
	LCommsBusPriv(LStream *Log) :
		LThread("LCommsBusPriv.Thread"),
		LMutex("LCommsBusPriv.Lock"),
		log(Log)
	{
		LAssert(sizeof(Block) == 9);
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
		return Uid;
	}

	LString Describe()
	{
		return LString::Fmt("%s:%s", isServer ? "server" : "client", GetUid());
	}

	void Que(Block::Auto &blk)
	{
		if (!Lock(_FL))
			return;

		LOG("%s que msg %s\n", Describe().Get(), blk->ToString().Get());

		auto &info = writeQue.New();
		info.sendTs = 0;
		info.blk = blk.Release();

		Unlock();
	}

	bool ServerSend(Block *blk)
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
				for (auto &e: endpoints)
				{
					if (!e.addr.Equals(dest))
						continue;

					foundEndpoint = true;
					if (e.local)
					{
						e.local(blk->SecondLine());
						status = true;
					}
					else if (e.remote)
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
				Unlock();
				if (!foundEndpoint)
				{
					LOG("%s error: no endpoint '%s'\n",
						Describe().Get(),
						dest.Get());
				}
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
		if (Lock(_FL))
		{
			for (size_t i=0; i<writeQue.Length(); i++)
			{
				auto &info = writeQue[i];
				switch (info.blk->GetId())
				{
					case MSendMsg:
					{
						bool timedOut = info.sendTs && ((LCurrentTime() - info.sendTs) >= RESEND_TIMEOUT);
						if (timedOut)
						{
							writeQue.DeleteAt(i--, true);
						}
						else if (!firstAttempt || info.sendTs == 0)
						{
							if (ServerSend(info.blk))
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
							else
							{
								info.sendTs = LCurrentTime();
							}
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

	int Server()
	{
		// For inter-server connection discovery, get a list of interfaces to broadcast to
		LArray<LSocket::Interface> interfaces;
		LSocket::EnumInterfaces(interfaces);
		for (int i=0; i<interfaces.Length(); i++)
		{
			if (interfaces[i].IsLoopBack()) // We don't care about the local
				interfaces.DeleteAt(i--);
		}
		LArray<uint32_t> interface_ips;
		for (auto &intf: interfaces)
		{
			interface_ips.Add(intf.Ip4);
			LOG("interface: %s %s\n", intf.Name.Get(), LIpToStr(intf.Ip4).Get());
		}

		// Create a UDP listener
		uint32_t broadcastIp = LIpToInt(COMMBUS_MULTICAST);
		LUdpListener listener(interface_ips, broadcastIp, DEFAULT_COMMS_PORT, log);
		uint64_t broadcastTime = 0;

		// Wait for incoming connections and handle them...
		bool hasConnections = false;
		NotifyState(LCommsBus::TDisconnectedServer);

		while (!IsCancelled())
		{
			{
				// Inter-server discovery...
				
				// Listen for packets...
				LString discoverPkt;
				uint32_t discoverIp = 0;
				uint16_t discoverPort = 0;
				if (listener.ReadPacket(discoverPkt, discoverIp, discoverPort))
				{
					bool ourIp = false;
					for (auto &i: interfaces)
						if (i.Ip4 == discoverIp)
							ourIp = true;
					if (!ourIp)
						LOG("got discover packet: %i bytes, %s:%i\n", (int)discoverPkt.Length(), LIpToStr(discoverIp).Get(), discoverPort);
				}

				auto now = LCurrentTime();
				if (now - broadcastTime >= 10000)
				{
					broadcastTime = now;

					// And then broadcast packets...
					for (auto &intf: interfaces)
					{
						LUdpBroadcast bc(intf.Ip4);
						LString pkt = "PING";
						auto result = bc.BroadcastPacket(pkt, broadcastIp, DEFAULT_COMMS_PORT);
						if (!result)
							LOG("broadcast to %s = %i\n", LIpToStr(intf.Ip4).Get(), result);
					}
				}
			}

			if (listen.IsReadable())
			{
				// Setup a new incoming TCP client connection
				auto conn = new CommsClient(log);
				if (listen.Accept(&conn->sock))
				{
					clients.Add(conn);
					conn->OnConnect();
					LOG("%s got new connection, sock=" LPrintfSock "\n",
						Describe().Get(),
						conn->sock.Handle());
				}
				else
				{
					DeleteObj(conn);
				}
			}
			else
			{
				// Check connections for incoming data:
				for (auto c: clients)
				{
					if (!ValidSocket(c->sock.Handle()))
					{
						clients.Delete(c);
						delete c;

						LOG("%s client disconnected... (len=%i)\n", Describe().Get(), (int)clients.Length());
						break;
					}

					c->Read([this, c](auto blk) mutable
					{
						LOG("%s received msg %s\n", Describe().Get(), blk->ToString().Get());
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
								auto lines = LString(blk->data).SplitDelimit("\r\n");
								if (lines.Length() != 2)
								{
									LOG("%s error in ep fmt=%s\n", Describe().Get(), blk->data);
								}
								else
								{
									auto ep = lines[0];
									auto remoteUid = lines[1];
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
								if (!ServerSend(blk))
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
					});
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

		// Seeing as a new endpoint turned up... try and send any queued messages
		ServerTrySend(false);
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
		Connection c(log);
		int connectErrs = 0;
		bool hasConnection = false;
		NotifyState(LCommsBus::TDisconnectedClient);

		while (!IsCancelled())
		{
			if (!c.connected)
			{
				// Connect to the server...
				c.connected = c.sock.Open("localhost", DEFAULT_COMMS_PORT);
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
							for (auto &ep: endpoints)
							{
								if (ep.local)
								{
									if (auto blk = ep.MakeMsg(GetUid()))
									{
										auto sent = c.Write(blk);
										LOG("%s send endpoint %s = %i.\n", Describe().Get(), ep.addr.Get(), sent);
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
				c.Read([this](auto blk)
				{
					LOG("%s received msg %s\n", Describe().Get(), blk->ToString().Get());

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
				});
				
				if (Lock(_FL))
				{
					// Write any outgoing messages
					for (size_t i=0; i<writeQue.Length(); i++)
					{
						auto &info = writeQue[i];
						if (!info.sendTs || (LCurrentTime() - info.sendTs >= RESEND_TIMEOUT))
						{
							if (c.Write(info.blk))
							{
								LOG("%s wrote msg %s\n", Describe().Get(), info.blk->ToString().Get());
								writeQue.DeleteAt(i--, true);
							}
							else
							{
								LOG("%s error: writing msg %s\n", Describe().Get(), info.blk->ToString().Get());
								info.sendTs = LCurrentTime();
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
	
LCommsBus::LCommsBus(LStream *log)
{
	d = new LCommsBusPriv(log);
}

LCommsBus::~LCommsBus()
{
	delete d;
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
		d->OnEndpoint(e);
		msg.Printf("%s\n%s", endPoint.Get(), d->GetUid());
	}
	d->Unlock();

	if (msg &&
		!d->isServer) // The server doesn't need to tell anyone else about endpoints
	{		
		if (auto blk = Block::New(MCreateEndpoint, msg))
			d->Que(blk);
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
