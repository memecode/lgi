// https://tools.ietf.org/html/rfc6455
#include "lgi/common/Lgi.h"
#include "lgi/common/WebSocket.h"
#include "../Hash/sha1/sha1.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Thread.h"
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

enum WsOpCode
{
	WsContinue = 0,
	WsText = 1,
	WsBinary = 2,
	WsClose = 8,
	WsPing = 9,
	WsPong = 10,
};

struct LWebSocketPriv
{
	LWebSocket *Ws;
	bool Server;

	WebSocketState State;
	LString InHdr, OutHdr;
	LArray<char> Data;
	size_t Start, Used;
	char Buf[512];
	LArray<LRange> Msg;
	LWebSocket::OnMsg onMsg;

	LWebSocketPriv(LWebSocket *ws, bool server) : Ws(ws), Server(server)
	{
		State = WsReceiveHdr;
		Used = 0;
		Start = 0;
	}

	template<typename T>
	ssize_t Write(T *p, size_t len)
	{
		size_t i = 0;
		while (i < len)
		{
			auto Wr = Ws->Write(p + i, len - i);
			if (Wr <= 0)
				return i;
			i += Wr;
		}
		return i;
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

	void OnMsg(char *ptr, uint64 len)
	{
		if (onMsg)
			onMsg(ptr, len);
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
				OnMsg((char*)p, Len);
				p += Len;
				Status = true;

				// Consume the data
				auto Remaining = End - p;
				if (Remaining > 0)
					memcpy(Data.AddressOf(), p, Remaining);
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
		{
			Ws->Close();
			State = WsClosed;
		}

		return Status;
	}

	bool Error(const char *Msg)
	{
		return false;
	}

	bool SendResponse()
	{
		// Create the response hdr and send it...
		static const char *Key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

		LAutoString Upgrade(InetGetHeaderField(InHdr, "Upgrade", InHdr.Length()));
		if (!Upgrade || stricmp(Upgrade, "websocket"))
			return false;

		LAutoString SecWebSocketKey(InetGetHeaderField(InHdr, "Sec-WebSocket-Key", InHdr.Length()));
		if (!SecWebSocketKey)
			return Error("No Sec-WebSocket-Key header");
		
		LString s = SecWebSocketKey.Get();
		s += Key;

		SHA1Context Ctx;
		SHA1Reset(&Ctx);
		SHA1Input(&Ctx, (const uchar*) s.Get(), (unsigned) s.Length());
		if (!SHA1Result(&Ctx))
			return Error("SHA1Result failed");

		uint32_t Digest[5];
		for (int i=0; i<CountOf(Digest); i++)
			Digest[i] = htonl(Ctx.Message_Digest[i]);

		char B64[64];
		auto c = ConvertBinaryToBase64(B64, sizeof(B64), (uchar*) &Digest[0], sizeof(Digest));
		if (c <= 0)
			return Error("ConvertBinaryToBase64 failed");

		B64[c] = 0;
		OutHdr.Printf("HTTP/1.1 101 Switching Protocols\r\n"
					"Upgrade: websocket\r\n"
					"Connection: Upgrade\r\n"
					"Sec-WebSocket-Accept: %s\r\n\r\n", B64);
		auto Wr = Write(OutHdr.Get(), OutHdr.Length());
		if (Wr != OutHdr.Length())
			return Error("Writing HTTP response failed.");

		State = WsMessages;
		return CheckMsg();
	}
};

LWebSocket::LWebSocket(bool Server, OnMsg onMsg)
{
	d = new LWebSocketPriv(this, Server);
	if (onMsg)
		ReceiveHandler(onMsg);
}

LWebSocket::~LWebSocket()
{
	delete d;
}

void LWebSocket::ReceiveHandler(OnMsg onMsg)
{
	d->onMsg = onMsg;
}

bool LWebSocket::SendMessage(char *Data, uint64 Len)
{
	if (d->State != WsMessages)
		return false;

	uint8_t Masked = d->Server ? 0 : 0x80;
	int8 Hdr[2 + 8 + 4];
	LPointer p = {Hdr};
	*p.u8++ =	0x80 | // Fin
				WsText;
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
	if (Wr != Sz)
		return d->Error("SendMessage.Hdr failed to write to socket");

	Wr = d->Write(MaskData ? MaskData.Get() : (uint8_t*)Data, Len);
	if (Wr != Len)
		return d->Error("SendMessage.Body failed to write to socket");

	return true;
}

bool LWebSocket::InitFromHeaders(LString Data, OsSocket Sock)
{
	bool HasMsg = false;

	if (d->State == WsReceiveHdr)
	{
		d->InHdr = Data;
		Handle(Sock);

		auto End = d->InHdr.Find("\r\n\r\n");
		if (End >= 0)
		{
			if (d->InHdr.Length() > (size_t)End + 4)
			{
				d->AddData(d->InHdr.Get() + End + 4, d->InHdr.Length() - End - 4);
				d->InHdr.Length(End);
			}

			HasMsg = d->SendResponse();
		}
	}

	return HasMsg;
}

bool LWebSocket::OnData()
{
	bool GotData = false;

	auto Rd = Read(d->Buf, sizeof(d->Buf));
	if (Rd > 0)
	{
		if (d->State == WsReceiveHdr)
		{
			d->InHdr += LString(d->Buf, Rd);
			auto End = d->InHdr.Find("\r\n\r\n");
			if (End >= 0)
			{
				if (d->InHdr.Length() > (size_t)End + 4)
				{
					d->AddData(d->InHdr.Get() + End + 4, d->InHdr.Length() - End - 4);
					d->InHdr.Length(End);
				}

				GotData = d->SendResponse();
			}
		}
		else
		{
			GotData = d->AddData(d->Buf, Rd);
		}
	}

	return GotData;
}


////////////////////////////////////////////////////////////////////////////////////////////
struct LWebSocketServerPriv : public LThread, public LCancel
{
	using Connection = LWebSocketServer::Connection;

	LStream *Log = NULL;
	int Port = 0;
	
	// Callbacks
	std::function<bool(Connection*)> OnConnection;
	LWebSocketBase::CreateSocket CreateSocket;
	
	// Listening:
	LAutoPtr<LSocket> Listen;
	uint64_t ListenTs = 0;
	bool ListenOk = false;
	constexpr static int LISTEN_RETRY = 10000; // 10 seconds

	// Active connections	
	LArray<Connection*> Connections;

	LWebSocketServerPriv(LStream *log, int port) : LThread("LWebSocketServerPriv")
	{
		Log = log;
		Port = port;
		
		Run();
	}
	
	~LWebSocketServerPriv()
	{
		Cancel();
		WaitForExit();
	}
	
	LSocket *Create()
	{
		if (CreateSocket)
			return CreateSocket();
		return new LSocket;
	}
	
	int Main()
	{
		while (!IsCancelled())
		{
			auto Now = LCurrentTime();
			if (!Listen || (Now - ListenTs >= LISTEN_RETRY))
			{
				if (Listen.Reset(Create()))
				{
					Listen->IsBlocking(false);
					Listen->Listen(Port);
					ListenTs = Now;
				}
			}
			else
			{
				LSelect sel;
				LHashTbl<PtrKey<LSocket*>, Connection*> map;
				for (auto c: Connections)
				{
					sel += c->sock;
					map.Add(c->sock, c);
				}
				auto readable = sel.Readable(50);
				LArray<Connection*> del;
				for (auto r: readable)
				{
					auto c = map.Find(r);
					if (c)
					{
						auto status = c->OnRead();
						if (status != LWebSocketServer::ConnectOk)
							del.Add(c);
					}
				}
				for (auto d: del)
				{
					Connections.Delete(d);
					delete d;
				}
			}
		}
		
		return 0;
	}
};

LWebSocketServer::Connection::Connection(LWebSocketServerPriv *priv, LSocket *s)
{
	d = priv;
	sock.Reset(s);
}

LWebSocketServer::ConnectStatus LWebSocketServer::Connection::OnRead()
{
	if (used >= read.Length())
	{
		// Extend the size of the read buffer.
		auto newSize = read.Length() << 1;
		if (!read.Length(newSize))
		{
			d->Log->Print("%s:%i - Error: allocating space: " LPrintfSSizeT "\n", _FL);
			return ConnectError;
		}
	}
	if (!sock)
	{
		d->Log->Print("%s:%i - Error: no socket?\n", _FL);
		return ConnectError;
	}
	
	LAssert(used < read.Length());
	auto rd = sock->Read(read.AddressOf(used), read.Length() - used);
	if (rd > 0)
	{
		used += rd;
		
		// FIXME: if message complete... call MsgCb
		// Then remove the data from the read buffer.
	}
	else
	{
		// Connection finished..
		if (CloseCb)
			CloseCb();
		return ConnectClosed;
	}
	
	return ConnectOk;
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
