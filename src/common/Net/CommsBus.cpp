#include "lgi/common/Lgi.h"
#include "lgi/common/Thread.h"
#include "lgi/common/CommsBus.h"
#include "lgi/common/Net.h"

#define DEFAULT_COMMS_PORT	45454
#define RETRY_SERVER		-2

#if 1
#define LOG(...)			printf(__VA_ARGS__)
#else
#define LOG(...)			if (log) log->Print(__VA_ARGS__)
#endif

enum MsgIds
{
	MProcess		= Lgi4CC("proc"),
	MCreateEndpoint	= Lgi4CC("mkep"),
	MSendMsg		= Lgi4CC("send"),
};

struct Block
{
	uint32_t id;
	uint32_t size;
	char data[1];
	
	constexpr static int nullSz = 1;
	using Auto = LAutoPtr<Block, false, true>;
	
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
	
	static Auto New(uint32_t msgId, LString s)
	{
		auto blk = New(msgId, s.Length());
		if (blk)
			memcpy(blk->data, s.Get(), s.Length() + nullSz);
		return blk;
	}
};

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

	bool Read(std::function<void(Block*)> cb)
	{
		// Check for data
		if (!sock.IsReadable())
			return false;

		// Read some data:
		auto rd = sock.Read(readBuf.AddressOf() + used, readBuf.Length() - used);
		if (rd <= 0)
		{
			printf("read disconnected: %i\n", (int)rd);
			sock.Close();
			connected = false;
			return false;
		}
		used += rd;
		
		// Check if there is a full msg
		if (used < sizeof(Block))
		{
			printf("read: not enough data for header\n");
			return false;
		}
		
		if (auto b = (Block*) readBuf.AddressOf())
		{
			ssize_t bytes = ntohl(b->size) + sizeof(Block);
			if (used >= bytes)
			{
				// Convert msg to host format...
				b->id = ntohl(b->id);
				b->size = ntohl(b->size);
				
				// Call the callback
				cb(b);

				// Remove the message from the buffer
				auto remaining = used - bytes;
				if (remaining > 0)
					memmove(readBuf.AddressOf(), readBuf.AddressOf(bytes), remaining);
				used -= bytes;
			}
			else
			{
				printf("read: not enough bytes for msg\n");
			}
		}
		else
		{
			printf("read: readbuf null\n");
		}
		
		return false;
	}
	
	bool Write(Block *b)
	{
		if (!b)
			return false;
		size_t bytes = ntohl(b->size) + sizeof(Block);
		// LOG("write: bytes=%i\n", (int)bytes);
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
		
		if (i != bytes)
			LOG("write: error - i=%i bytes=%i\n", (int)i, (int)bytes);
		return i == bytes;
	}
};

struct Endpoint
{
	LString addr;
	int64_t remote = 0;	
	std::function< void(LString) > local;
};

struct LCommsBusPriv :
	public LThread,
	public LCancel,
	public LMutex
{
	bool isServer = false;
	LSocket listen;
	int uid = -1;
	LStream *log = NULL;
	
	// Lock before using
	LArray< Block::Auto > writeQue;
	LArray< Endpoint > endpoints;
	
	LCommsBusPriv(int Uid, LStream *Log) :
		LThread("LCommsBusPriv.Thread"),
		LMutex("LCommsBusPriv.Lock"),
		uid(Uid),
		log(Log)
	{
		Run();
	}
	
	~LCommsBusPriv()
	{
		Cancel();
		WaitForExit();
	}
	
	void Que(Block::Auto &blk)
	{
		if (Lock(_FL))
		{
			writeQue.New().Reset(blk.Release());
			Unlock();
			LOG("%s:%i - " LPrintfSizeT " msgs queued\n", _FL, writeQue.Length());
		}
	}

	struct CommsClient : public Connection
	{
		int processId = 0;
		uint64_t lastSeen = 0;
		
		CommsClient(LStream *l) : Connection(l)
		{
		}

		void OnConnect()
		{
			lastSeen = LCurrentTime();
		}
	};
	
	int Server()
	{
		LArray<CommsClient*> clients;
		
		// Wait for incoming connections and handle them...
		while (!IsCancelled())
		{
			if (listen.IsReadable())
			{
				// Setup a new incomming client connection
				auto conn = new CommsClient(log);
				if (listen.Accept(&conn->sock))
				{
					clients.Add(conn);
					conn->OnConnect();
					LOG("%s:%i - server got new connection, sock=%i\n",
						_FL,
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
						LOG("Client disconnected...\n");
						clients.Delete(c);
						delete c;
						break;
					}

					c->Read([this, c, clients](auto blk) mutable
					{
						LOG("server received msg..\n");
						switch (blk->id)
						{
							case MProcess:
							{
								c->processId = Atoi(blk->data);
								break;
							}
							case MCreateEndpoint:
							{
								auto lines = LString(blk->data).SplitDelimit("\r\n");
								if (lines.Length() == 2)
								{
									auto ep = lines[0];
									auto proc = lines[1];
									if (Lock(_FL))
									{
										bool found = false;
										for (auto &e: endpoints)
										{
											if (e.addr == ep)
											{
												found = true;
												e.remote = proc.Int();
												OnEndpoint(e);
												break;
											}
										}
										if (!found)
										{
											auto &e = endpoints.New();
											e.addr = ep;
											e.remote = proc.Int();
											OnEndpoint(e);
										}
										Unlock();
									}
								}
								break;
							}
							case MSendMsg:
							{
								if (auto nl = Strnchr(blk->data, '\n', blk->size))
								{
									LString ep(blk->data, nl - blk->data);
									auto payload = nl + 1;
									auto end = blk->data + blk->size;
									if (Lock(_FL))
									{
										bool foundEndpoint = false;
										for (auto &e: endpoints)
										{
											if (e.addr == ep)
											{
												foundEndpoint = true;
												if (e.local)
												{
													e.local(LString(payload, end - payload));
												}
												else if (e.remote)
												{
													// Find the matching connection and send to the remote end
													bool foundClient = false;
													for (auto &c: clients)
													{
														if (c->processId == e.remote)
														{
															foundClient = true;
															
															if (!c->Write(blk))
															{
																LOG("%s:%i - error: write failed.\n", _FL);
															}
															break;
														}
													}
													if (!foundClient)
													{
														LOG("%s:%i - error: no client for endpoint='%s' procid=" LPrintfInt64 "\n",
															_FL,
															ep.Get(),
															e.remote);
													}
												}
												break;
											}
										}
										Unlock();
										if (!foundEndpoint)
										{
											LOG("%s:%i - error: no endpoint '%s'\n",
												_FL,
												ep.Get());
										}
									}
								}
								else LOG("%s:%i - No separator?\n", _FL);
								break;
							}
							default:
								break;
						}
						
						c->lastSeen = LCurrentTime();
					});
				}
			}
		}
		
		clients.DeleteObjects();
		return 0;
	}
	
	void OnEndpoint(Endpoint &e)
	{
		LOG("OnEndpoint: addr=%s remote=" LPrintfInt64 " local=%i\n", e.addr.Get(), e.remote, (bool)e.local);
	}
	
	int Client()
	{
		Connection c(log);
		int connectErrs = 0;
		
		while (!IsCancelled())
		{
			if (!c.connected)
			{
				// Connect to the server...
				c.connected = c.sock.Open("localhost", DEFAULT_COMMS_PORT);
				// LOG("client connect: %i\n", c.connected);
				if (c.connected)
				{
					auto procId = LString::Fmt("%i", LProcessId());
					auto blk = Block::New(MProcess, procId);
					if (!c.Write(blk))
						LOG("%s:%i write failed.\n", _FL);
				}
				else
				{
					LSleep(1000);
					if (connectErrs++ > 5)
						return RETRY_SERVER;
				}
			}
			else
			{
				c.Read([this](auto blk)
				{
					switch (blk->id)
					{
						default:
							break;
					}
				});
				
				if (Lock(_FL))
				{
					// Write any outgoing messages
					LArray< Block::Auto > que;
					if (writeQue.Length())
						writeQue.Swap(que);
					Unlock();
					for (auto &m: que)
					{
						LOG("client writing msg...\n");
						c.Write(m.Get());
					}
				}
				
				LSleep(10);
			}
		}		

		return 0;
	}
	
	int Main()
	{
		// Try and listen on the comms port..
		// If it succeeds, this is the first process to connect... and will be the server
		// If it fails this process becomes a client.
		while (!IsCancelled())
		{
			isServer = listen.Listen(DEFAULT_COMMS_PORT);
			LOG("CommsBus: isServer=%i\n", isServer);
			
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
	
LCommsBus::LCommsBus(int uid, LStream *log)
{
	d = new LCommsBusPriv(uid, log);
}

LCommsBus::~LCommsBus()
{
	delete d;
}

bool LCommsBus::IsServer() const
{
	return d->isServer;
}

bool LCommsBus::SendMsg(LString endPoint, LString data)
{
	auto s = endPoint + "\n" + data;		
	if (auto msg = Block::New(MSendMsg, s))
	{
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
			exists = true;
			e.local = cb; // update existing
			msg.Printf("%s\n%i", endPoint.Get(), LProcessId());
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
		msg.Printf("%s\n%i", endPoint.Get(), LProcessId());
	}
	d->Unlock();

	if (msg)
	{
		if (auto blk = Block::New(MCreateEndpoint, msg))
			d->Que(blk);
	}
	return true;
}

// Unit testing:
static LAutoPtr<LCommsBus> CreateBus(int uid, bool waitServer = false)
{
	LAutoPtr<LCommsBus> bus(new LCommsBus(uid));
	while (waitServer && !bus->IsServer())
		LSleep(1);
	return bus;
}

static void WaitResult(bool &result, int timeMs)
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

static bool ClientToServer()
{
	auto srv = CreateBus(1, true);
	auto cli = CreateBus(2);
	bool result = false;

	if (srv && cli)
	{
		auto ep = "Test.ClientToServer";
		auto testData = "testData";
		
		cli->Listen(ep, [&](auto data)
		{
			if (data.Equals(testData))
				result = true;
		});
		
		srv->SendMsg(ep, testData);
		WaitResult(result, 1000);
	}

	return result;
}

static bool ServerToClient()
{
	auto srv = CreateBus(1, true);
	auto cli = CreateBus(2);
	bool result = false;

	if (srv && cli)
	{
		auto ep = "Test.ServerToClient";
		auto testData = "testData";
		
		srv->Listen(ep, [&](auto data)
		{
			if (data.Equals(testData))
				result = true;
		});
		
		cli->SendMsg(ep, testData);
		WaitResult(result, 1000);
	}

	return result;
}

bool LCommsBus::UnitTests()
{
	bool status = ClientToServer();	
	status &= ServerToClient();
	return status;
}
