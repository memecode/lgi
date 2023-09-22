// https://tools.ietf.org/html/rfc6455
#include "lgi/common/Lgi.h"
#include "lgi/common/WebSocket.h"
#include "../Hash/sha1/sha1.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Thread.h"
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Uri.h"
#include "../Hash/sha1/sha1.h"

#ifdef LINUX
	#include <netinet/tcp.h>
	#include <unistd.h>
	#include <poll.h>
	#ifndef htonll
	#define htonll LgiSwap64
	#endif
#endif

#define LOG_ALL 0

////////////////////////////////////////////////////////////////////////////
enum WebSocketState
{
	WsReceiveHdr,
	WsMessages,
	WsClosed,
};

LString LWebSocketBase::TimestampString(uint64 i)
{
	LString s;
	s.Printf(LPrintfInt64, LDirectory::TsToUnix(i));
	return s;
}

LString LWebSocketBase::ToString(uint64 i)
{
	LString s;
	s.Printf(LPrintfInt64, i);
	return s;
}

const char *LWebSocketBase::ToString(WsOpCode op)
{
	switch (op)
	{
		case WsContinue: return "WsContinue";
		case WsText:     return "WsText";
		case WsBinary:   return "WsBinary";
		case WsClose:    return "WsClose";
		case WsPing:     return "WsPing";
		case WsPong:     return "WsPong";
		default:         return "#unknownOpCode";
	}
}

struct LWebSocketPriv : public LWebSocketBase
{
	LWebSocket *Ws;
	LAutoPtr<LSocketI> Sock;
	bool Server; // true if on the server side of the connection

	WebSocketState State;
	LString InHdr, OutHdr;
	LArray<char> Data;
	size_t Start, Used;
	char Buf[512];
	LArray<LRange> Msg;

	LWebSocketPriv(LAutoPtr<LSocketI> sock, LWebSocket *ws, bool server) :
		Sock(sock),
		Ws(ws),
		Server(server)
	{
		State = WsReceiveHdr;
		Used = 0;
		Start = 0;
	}

	template<typename T>
	ssize_t Write(T *p, size_t len)
	{
		size_t i = 0;
		while (Sock && i < len)
		{
			auto Wr = Sock->Write(p + i, len - i);
			if (Wr <= 0)
				return i;
			i += Wr;
		}
		return i;
	}

	LString ConsumeBytes(size_t bytes)
	{
		if (bytes > Used)
			bytes = Used;

		LAssert(bytes > 0);
		auto base = Data.AddressOf();
		LString s(base, bytes);
		auto remaining = Data.Length() - bytes;
		if (remaining > 0)
		{
			memmove(base, base + bytes, remaining);
			Used -= bytes;
		}

		return s;
	}

	/// reads more data from 'Sock' into 'Data'
	ssize_t Read()
	{
		if (!Sock)
			return -1;
		
		// Check there is space to read into
		if (Data.Length() - Used < 1024)
		{
			if (!Data.Length(Data.Length() ? Data.Length() << 1 : 1024))
				return -1;
		}

		// Read data
		auto base = Data.AddressOf();
		auto rd = Sock->Read(base + Used, Data.Length() - Used);
		// LgiTrace("%s:%i - Got %i bytes\n", _FL, (int)rd);
		if (rd <= 0)
		{
			Close();
			return -1;
		}
		Used += rd;

		// Process data
		if (State == WsReceiveHdr)
		{
			auto end = Strnstr(base, "\r\n\r\n", Data.Length());
			if (end)
			{
				InHdr = ConsumeBytes(end - base + 4);
				// LgiTrace("InHdr=%s\n", InHdr.Get());

				if (Server)
				{
					// Let the application have a look at the path and decide on whether to
					// accept and move to messages mode.
					Ws->OnHeaders();
				}
				else // Client
				{					
					auto connection = LGetHeaderField(InHdr, "Connection");
					if (connection == "Upgrade")
					{
						State = WsMessages;
						Ws->OnHeaders();
					}
					else
					{
						Close();
					}
				}

			}
		}

		if (State == WsMessages)
		{
			while (CheckMsg())
				;
		}

		return rd;
	}

	bool AddData(char *p, size_t Len)
	{
		#if LOG_ALL
		LgiTrace("Websocket Read: %i\n", (int)Len);
		for (unsigned i=0; i<Len; )
		{
			char s[300];
			int ch = sprintf_s(s, sizeof(s), "%.8x: ", i);
			for (int e = i + 16; i < e && i < Len; i++)
				ch += sprintf_s(s+ch, sizeof(s)-ch, " %02x-%c", (uint8)p[i], p[i]>=' '?p[i]:' ');
			s[ch++] = '\n';
			s[ch] = 0;
			LgiTrace("%.*s", ch, s);
		}
		#endif

		if (Used + Len > Data.Length())
			Data.Length(Used + Len + 1024);
		memcpy(Data.AddressOf(Used), p, Len);
		Used += Len;
		
		if (State != WsMessages)
			return false;

		return CheckMsg();
	}

	void Close()
	{
		State = WsClosed;
		if (Sock)
		{
			Sock->Close();
			Sock.Reset();
			if (Ws->CloseCb)
				Ws->CloseCb();
		}
	}

	bool CheckMsg()
	{
		// Check for valid message..
		if (Used < 2)
			return false; // too short

		uint8_t *p = (uint8_t*)Data.AddressOf(Start);
		uint8_t *End = p + Used;
		bool Fin = (p[0] & 0x80) != 0;
		WsOpCode OpCode = (WsOpCode) (p[0] & 0xf);
		bool Masked = (p[1] & 0x80) != 0;
		uint64 Len = p[1] & 0x7f;
		if (Len == 126)
		{
			if (Used < 4)
				return false; // Too short
			Len = (p[2] << 8) | p[3];
			p += 4;
		}
		else if (Len == 127)
		{
			if (Used < 10)
				return false; // Too short
			p += 2;
			Len = ((uint64)p[0] << 54) | ((uint64)p[1] << 48) | ((uint64)p[2] << 40) | ((uint64)p[3] << 32) |
				  ((uint64)p[4] << 24) | ((uint64)p[5] << 16) | ((uint64)p[6] << 8)  | p[7];
			p += 8;
		}
		else p += 2;

		uint8_t Mask[4];
		if (Masked)
		{
			if (p > End - 4)
				return false; // Too short
			Mask[0] = *p++;
			Mask[1] = *p++;
			Mask[2] = *p++;
			Mask[3] = *p++;

			if (End - p < (ssize_t)Len)
				return false; // Too short

			// Unmask
			for (uint64 i=0; i<Len; i++)
				p[i] ^= Mask[i & 3];
		}
		else
		{
			if (End - p < (ssize_t)Len)
				return false; // Too short
		}

		bool Status = false;
		if (Fin)
		{
			if (Msg.Length() == 0)
			{
				// Process the message
				if (Ws->MsgCb)
					Ws->MsgCb(OpCode, (char*)p, Len);
				p += Len;
				Status = true;

				// Consume the data
				auto Remaining = End - p;
				if (Remaining > 0)
					memmove(Data.AddressOf(), p, Remaining);
				Used = Remaining;				
			}
			else
				LAssert(!"Impl me");
		}
		else
		{
			auto Pos = (char*)p - Data.AddressOf();
			Msg.New().Set(Pos, Len);
			Start = Pos + Len;
		}

		if (OpCode == WsClose)
			Close();

		return Status;
	}

	bool Error(const char *Msg)
	{
		return false;
	}

	bool SendResponse(int StatusCode)
	{
		LAssert(Server);

		// Create the response hdr and send it...
		auto Upgrade = LGetHeaderField(InHdr, "Upgrade");
		if (Upgrade != "websocket")
			return false;

		auto SecWebSocketKey = LGetHeaderField(InHdr, "Sec-WebSocket-Key");
		if (!SecWebSocketKey)
			return Error("No Sec-WebSocket-Key header");
		
		LString s = SecWebSocketKey.Get();
		s += LWebSocketBase::Key;

		SHA1Context Ctx;
		SHA1Reset(&Ctx);
		SHA1Input(&Ctx, (const uchar*) s.Get(), (unsigned) s.Length());
		if (!SHA1Result(&Ctx))
			return Error("SHA1Result failed");

		uint32_t Digest[5];
		for (int i=0; i<CountOf(Digest); i++)
			Digest[i] = htonl(Ctx.Message_Digest[i]);

		auto b64 = LToBase64(Digest, sizeof(Digest));
		if (!b64)
			return Error("ConvertBinaryToBase64 failed");

		OutHdr.Printf("HTTP/1.1 %i Switching Protocols\r\n"
					"Upgrade: websocket\r\n"
					"Connection: Upgrade\r\n"
					"Sec-WebSocket-Accept: %s\r\n\r\n",
					StatusCode,
					b64.Get());
		auto Wr = Write(OutHdr.Get(), OutHdr.Length());
		if (Wr != OutHdr.Length())
			return Error("Writing HTTP response failed.");

		State = WsMessages;
		return CheckMsg();
	}
};

LWebSocket::LWebSocket(LAutoPtr<LSocketI> sock, bool Server, OnMsg onMsgCb)
{
	d = new LWebSocketPriv(sock, this, Server);
	MsgCb = onMsgCb;
}

LWebSocket::~LWebSocket()
{
	delete d;
}

bool LWebSocket::Open(const char *uri, int port)
{
	if (!d->Sock || !uri)
	{
		LgiTrace("%s:%i - param error.\n", _FL);
		return false;
	}

	LUri u(uri);
	if (!port)
		port = u.Port;
	if (!port)
	{
		LgiTrace("%s:%i - no port.\n", _FL);
		return false;
	}

	if (!d->Sock->Open(u.sHost, port))
	{
		LgiTrace("%s:%i - connect to '%s:%i' failed.\n", _FL, u.sHost.Get(), port);
		return false;
	}

	/* E.g.
		GET /memecodeApp HTTP/1.1
		Host: localhost:4567
		User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/117.0
		Accept: * *
		Accept-Language: en-US,en;q=0.5
		Accept-Encoding: gzip, deflate, br
		Sec-WebSocket-Version: 13
		Origin: null
		Sec-WebSocket-Extensions: permessage-deflate
		Sec-WebSocket-Key: Jzd4op3+h5uJ6rU+PgmPTg==
		Connection: keep-alive, Upgrade
		Sec-Fetch-Dest: empty
		Sec-Fetch-Mode: websocket
		Sec-Fetch-Site: cross-site
		Pragma: no-cache
		Cache-Control: no-cache
		Upgrade: websocket	
	*/

	LString Random;
	if (!Random.Length(16))
		return false;
	for (int i=0; i<16; i++)
		Random.Get()[i] = LRand(127-32) + 32;

	LString SecWebSocketKey = LToBase64(Random);

	LString http;
	http.Printf("GET %s HTTP/1.1\r\n"
				"Host: %s:%i\r\n"
				"Sec-WebSocket-Version: 13\r\n"
				"Sec-WebSocket-Key: %s\r\n"
				"Connection: keep-alive, Upgrade\r\n"
				"Sec-Fetch-Dest: empty\r\n"
				"Sec-Fetch-Mode: websocket\r\n"
				"Sec-Fetch-Site: cross-site\r\n"
				"Upgrade: websocket\r\n"
				"\r\n",
				u.sPath ? u.sPath.Get() : "/",
				u.sHost.Get(),
				port,
				SecWebSocketKey.Get());

	auto wr = d->Sock->Write(http.Get(), http.Length());
	if (wr != http.Length())
	{
		LgiTrace("%s:%i - failed to write HTTP header: %i\n", _FL, (int)wr);
		return false;
	}

	LgiTrace("WsSocket out hdr:\n%s", http.Get());
	return true;
}

bool LWebSocket::IsConnected()
{
	return d->State == WsMessages;
}

LSocketI *LWebSocket::GetSocket()
{
	return d->Sock;
}

bool LWebSocket::Close()
{
	SendMessage(WsClose, NULL, 0);
	
	if (d->Sock)
	{
		d->Sock->Close();
		d->Sock.Reset();
	}
	
	if (CloseCb)
		CloseCb();

	return true;
}

bool LWebSocket::SendMessage(WsOpCode Op, char *Data, uint64 Len)
{
	if (Debug)
	{
		if (Log)
			Log->Print("SendMessage %i\n", (int)Len);
		else
			LAssert(!"Setup a log handler.");
	}
	if (d->State != WsMessages)
		return false;

	uint8_t Masked = d->Server ? 0 : 0x80;
	int8 Hdr[2 + 8 + 4];
	LPointer p = {Hdr};
	*p.u8++ =	0x80 | // Fin
				(int)Op;
	if (Len < 126) // 1 byte
	{
		// Direct len
		*p.u8++ = Masked | (uint8_t)Len;
	}
	else if (Len <= 0xffff) // 2 byte
	{
		// 126 + 2 bytes
		*p.u8++ = Masked | 126;
		*p.u16++ = htons((u_short)Len);
	}
	else
	{
		// 127 + 8 bytes
		*p.u8++ = Masked | 127;
		*p.u64++ = htonll(Len);
	}

	LAutoPtr<uint8_t> MaskData;
	if (Masked)
	{
		uint8_t *Mask = p.u8;
		*p.u32++ = LRand();
		if (!MaskData.Reset(new uint8_t[Len]))
			return d->Error("Alloc failed.");
		
		uint8_t *Out = MaskData.Get();
		for (uint64 i=0; i<Len; i++)	
			Out[i] = Data[i] ^ Mask[i&3];
	}

	auto Sz = p.s8 - Hdr;
	auto Wr = d->Write(Hdr, Sz);
	if (Debug && Log)
		Log->Print("...write1(%i)=%i\n", (int)Sz, (int)Wr);
	if (Wr != Sz)
		return d->Error("SendMessage.Hdr failed to write to socket");

	Wr = d->Write(MaskData ? MaskData.Get() : (uint8_t*)Data, Len);
	if (Debug && Log)
		Log->Print("...write2(%i)=%i\n", (int)Len, (int)Wr);
	if (Wr != Len)
		return d->Error("SendMessage.Body failed to write to socket");

	return true;
}

bool LWebSocket::Read()
{
	return d->Read();
}

////////////////////////////////////////////////////////////////////////////////////////////
struct LWebSocketServerPriv :
	public LThread,
	public LCancel,
	public LMutex
{
	using Connection = LWebSocketServer::Connection;

	LStream *Log = NULL;
	int Port = 0;
	
	// Callbacks
	std::function<bool(Connection*)> OnConnection;
	LWebSocketBase::CreateSocket CreateSocket;
	
	// Listening:
	LAutoSocket Listen;
	uint64_t ListenTs = 0;
	int ListenOk = false;
	constexpr static int LISTEN_RETRY = 10000; // 10 seconds

	// Active connections	
	LArray<Connection*> Connections;
	
	struct WriteMsg
	{
		Connection *connect;
		LString msg;
	};
	LArray<WriteMsg> OutgoingMsgs;

	LWebSocketServerPriv(LStream *log, int port) :
		LThread("LWebSocketServerPriv.Thread"),
		LMutex ("LWebSocketServerPriv.Lock")
	{
		Log = log;
		Port = port;
		
		Run();
	}
	
	~LWebSocketServerPriv()
	{
		for (auto c: Connections)
		{
			c->Close();
		}
		
		Cancel();
		WaitForExit();
	}

	bool Send(Connection *c, LString msg)
	{
		if (!Lock(_FL))
			return false;

		auto &out = OutgoingMsgs.New();
		out.connect = c;
		out.msg = msg;
		Unlock();
		
		return true;
	}
	
	LAutoSocket Create()
	{
		if (CreateSocket)
			return CreateSocket();
		return LAutoSocket(new LSocket);
	}
	
	int Main()
	{
		uint64_t StartListen = 0;
	
		while (!IsCancelled())
		{
			auto Now = LCurrentTime();
			if (!ListenOk)
			{
				// Try and setup a socket to listen...
				if (!Listen)
				{
					Log->Print("Creating listen socket...\n");
					Listen = Create();
					if (Listen)
					{
						// Listen->IsBlocking(false);
						ListenOk = Listen->Listen(Port);
						StartListen = LCurrentTime();
						Log->Print("Listen(%i)=%i\n", Port, ListenOk);
					}
				}
				else if (StartListen)
				{
					if (Now - StartListen >= 10000)
					{
						Listen.Reset();
						StartListen = 0;
						// Start again...
					}
				}
			}
			else if (Listen->CanAccept(100))
			{
				auto sock = Create();
				if (sock)
				{
					if (Listen->Accept(sock))
					{
						auto c = new Connection(this, sock);
						if (c)
						{
							Connections.Add(c);
							if (OnConnection)
								OnConnection(c);
						}
					}
					else Log->Print("%s:%i - accept failed.\n", _FL);
				}
				else Log->Print("%s:%i - failed to create socket.\n", _FL);
			}			

			// Check all the connections for readability...
			LSelect sel;
			LHashTbl<PtrKey<LSocketI*>, Connection*> map;
			for (auto c: Connections)
			{
				sel += c->GetSocket();
				map.Add(c->GetSocket(), c);
			}
			auto readable = sel.Readable(50);
			LArray<Connection*> del;
			for (auto r: readable)
			{
				auto c = map.Find(r);
				if (c)
				{
					auto status = c->Read();
					if (!status)
						del.Add(c);
				}
			}
			for (auto d: del)
			{
				Connections.Delete(d);
				delete d;
			}

			// Also check for outgoing messages...
			if (Lock(_FL))
			{
				for (auto &out: OutgoingMsgs)
				{
					if (Connections.HasItem(out.connect))
					{
						// out.connect->Debug = true;
						// out.connect->Log = Log;
						out.connect->SendMessage(out.msg);
						// out.connect->Debug = false;
					}
				}
				OutgoingMsgs.Empty();
				Unlock();
			}
		}
		
		if (Listen)
		{
			Listen->Close();
		}
		
		return 0;
	}
};

LWebSocketServer::Connection::Connection(LWebSocketServerPriv *priv, LAutoPtr<LSocketI> s)
	: LWebSocket(s)
{
	d = priv;

	if (LWebSocket::d->Sock)
	{
		char addr[64];
		if (LWebSocket::d->Sock->GetRemoteIp(addr))
			ip = addr;
	}
}

bool LWebSocketServer::Connection::Send(LString msg)
{
	return d->Send(this, msg);
}

LString Indent(LString s)
{
	LString indent = "     ";
	LString enc = s.Replace("\r", "\\r").Replace("\n", "\\n\n");
	auto lines = enc.SplitDelimit("\n");
	for (auto &l: lines)
		l = indent + l;
	return LString("\n").Join(lines);
}

LString LToHex(void *p, size_t len, char sep = 0)
{
	LString s;
	auto charSz = sep ? 3 : 2;
	if (s.Length(len * charSz))
	{
		auto out = s.Get();
		auto in = (uint8_t*)p;
		auto end = in + len;
		auto hex = "0123456789abcdef";
		while (in < end)
		{
			*out++ = hex[*in>>4];
			*out++ = hex[*in&0xf];
			if (sep)
				*out++ = sep;
			in++;
		}
		*out = 0;
	}
	return s;
}

void LWebSocketServer::Connection::OnHeaders()
{
	auto &headers = LWebSocket::d->InHdr;

	auto newLine = headers.Find("\r\n");
	auto firstLine = headers(0, newLine);
	auto parts = firstLine.SplitDelimit();
	if (parts.Length() == 3)
	{
		method = parts[0];
		path = parts[1];
		protocol = parts[2];
	}

	int status = DefaultHttpResponse;
	if (ReceiveHdrsCb)
		status = ReceiveHdrsCb(path != NULL);

	LWebSocket::d->SendResponse(status);
}

LWebSocketServer::LWebSocketServer(LStream *log, int port)
{
	d = new LWebSocketServerPriv(log, port);
}

LWebSocketServer::~LWebSocketServer()
{
	delete d;
}

void LWebSocketServer::SetCreateSocket(CreateSocket createSocketCb)
{
	d->CreateSocket = createSocketCb;
}

void LWebSocketServer::OnConnection(std::function<bool(Connection*)> onConnectCb)
{
	d->OnConnection = onConnectCb;
}
