#include "lgi/common/Lgi.h"
#include "lgi/common/Thread.h"
#include "lgi/common/CommsBus.h"
#include "lgi/common/Net.h"

#define DEFAULT_COMMS_PORT	45454
#define RETRY_SERVER		-2
#define RESEND_TIMEOUT		1000

#if 0
#define LOG(...)			printf(__VA_ARGS__)
#else
#define LOG(...)			if (log) log->Print(__VA_ARGS__)
#endif

enum MsgIds
{
	MUid			= Lgi4CC("uuid"),
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

	LString ToString()
	{
		return LString::Fmt("id='%4.4s' sz=%i", &id, GetSize());
	}

	uint32_t GetId() const { return ntohl(id); }
	uint32_t GetSize() const { return ntohl(size); }
	
	LString FirstLine() const
	{
		auto nl = Strnchr(data, '\n', GetSize());
		if (nl)
			return LString(data, nl - data);
		return LString();
	}

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
		bool status = false;
		while (used >= sizeof(Block))
		{
			if (auto b = (Block*) readBuf.AddressOf())
			{
				ssize_t bytes = b->GetSize() + sizeof(Block);
				if (used >= bytes)
				{
					// LOG("%u read bytes=%i\n", LCurrentThreadId(), bytes);

					// Call the callback
					cb(b);

					// Remove the message from the buffer
					auto remaining = used - bytes;
					if (remaining > 0)
						memmove(readBuf.AddressOf(), readBuf.AddressOf(bytes), remaining);
					used -= bytes;

					status = true;
				}
				else
				{
					// LOG("read: not enough bytes for msg\n");
					break;
				}
			}
			else
			{
				LOG("read: readbuf null\n");
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

struct Endpoint
{
	LString addr;
	LString remote;	
	std::function< void(LString) > local;
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

struct BlockInfo
{
	uint64_t sendTs = 0;
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

	// Server only:
	LArray<CommsClient*> clients;
		
	LCommsBusPriv(LStream *Log) :
		LThread("LCommsBusPriv.Thread"),
		LMutex("LCommsBusPriv.Lock"),
		log(Log)
	{
		Run();
	}
	
	~LCommsBusPriv()
	{
		Cancel();
		WaitForExit();
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

		if (auto nl = Strnchr(blk->data, '\n', blk->GetSize()))
		{
			LString ep(blk->data, nl - blk->data);
			auto payload = nl + 1;
			auto end = blk->data + blk->GetSize();
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
										LOG("%s error: write failed.\n", Describe().Get(), _FL);
									}
									break;
								}
							}
							if (!foundClient)
							{
								LOG("%s error: no client for endpoint='%s' procid=" LPrintfInt64 "\n",
									Describe().Get(),
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
					LOG("%s error: no endpoint '%s'\n",
						Describe().Get(),
						ep.Get());
				}
			}
		}
		else
		{
			LOG("%s No separator?\n", Describe().Get());
		}

		return status;
	}
	
	int Server()
	{

		// Wait for incoming connections and handle them...
		while (!IsCancelled())
		{
			if (listen.IsReadable())
			{
				// Setup a new incoming client connection
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
						LOG("%s client disconnected...\n", Describe().Get());

						clients.Delete(c);
						delete c;
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
								break;
							}
							case MCreateEndpoint:
							{
								auto lines = LString(blk->data).SplitDelimit("\r\n");
								if (lines.Length() == 2)
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
								ServerSend(blk);
								break;
							}
							default:
								break;
						}
						
						c->lastSeen = LCurrentTime();
					});
				}

				// Check writeQue for outgoing data...
				if (Lock(_FL))
				{
					for (size_t i=0; i<writeQue.Length(); i++)
					{
						auto &info = writeQue[i];
						switch (info.blk->GetId())
						{
							case MSendMsg:
							{
								if (!info.sendTs || (LCurrentTime() - info.sendTs >= RESEND_TIMEOUT))
								{
									if (ServerSend(info.blk))
										writeQue.DeleteAt(i--, true);
									else
										info.sendTs = LCurrentTime();
								}
								break;
							}
							default:
							{
								// What to do here?
								LOG("%s error: can't send msg type %s\n", Describe().Get(), info.blk->ToString().Get());
								writeQue.DeleteAt(i--, true);
								break;
							}
						}
					}

					Unlock();
				}

				LSleep(10);
			}
		}
		
		clients.DeleteObjects();
		return 0;
	}
	
	void OnEndpoint(Endpoint &e)
	{
		LOG("%s OnEndpoint: addr=%s remote=%s local=%i\n",
			Describe().Get(),
			e.addr.Get(),
			e.remote.Get(),
			(bool)e.local);
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
						// Also tell the server about our endpoints
						if (Lock(_FL))
						{
							for (auto &ep: endpoints)
							{
								if (ep.local)
								{
									auto epData = LString::Fmt("%s\n%s", ep.addr.Get(), GetUid());
									if (blk = Block::New(MCreateEndpoint, epData))
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
					LSleep(1000);
					if (connectErrs++ > 5)
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
								LOG("%s wrote msg '%4.4s'\n", Describe().Get(), &info.blk->id);
								writeQue.DeleteAt(i--, true);
							}
							else
							{
								LOG("%s error: writing msg '%4.4s'\n", Describe().Get(), &info.blk->id);
								info.sendTs = LCurrentTime();
							}
						}
					}
					Unlock();
				}
				
				LSleep(10);
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
			LOG("CommsBus %s=%s\n", isServer ? "server" : "client", GetUid());
			
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
	size_t bytes = endPoint.Length() + data.Length() + 1;
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
			exists = true;
			e.local = cb; // update existing
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

	if (msg)
	{
		if (auto blk = Block::New(MCreateEndpoint, msg))
			d->Que(blk);
	}
	return true;
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

	int Main()
	{
		auto status = ClientToServer();
		LAssert(status);

		log->Print("\n\n");

		status = ServerToClient();
		LAssert(status);

		return 0;
	}
};

void LCommsBus::UnitTests(LStream *log)
{
	new CommsBusUnitTests(log);
}
