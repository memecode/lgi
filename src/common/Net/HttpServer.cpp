#include "lgi/common/Lgi.h"
#include "lgi/common/HttpServer.h"
#include "lgi/common/Net.h"
#include "lgi/common/Thread.h"

struct LHttpServerPriv;

#if 0
#define LOG_HTTP(...)	printf(__VA_ARGS__)
#else
#define LOG_HTTP(...)
#endif

enum Msgs
{
	M_ADD_WEBSOCKET = M_USER,
	M_SEND_WEBSOCKET,
	M_BROADCAST_WEBSOCKET
};

class LHttpThread : public LThread
{
	LAutoPtr<LSocketI> s;
	LHttpServerPriv *p;

public:
	LHttpThread(LAutoPtr<LSocketI> sock, LHttpServerPriv *priv);
	int Main();
};

class LHttpServer_TraceSocket : public LSocket
{
public:
	void OnInformation(const char *Str) override
	{
		LgiTrace("SocketInfo: %s\n", Str);
	}

	void OnError(int ErrorCode, const char *ErrorDescription) override
	{
		LgiTrace("SocketError %i: %s\n", ErrorCode, ErrorDescription);
	}
};

struct LHttpServerPriv :
	public LThread,
	public LMutex,
	public LEventTargetI
{
private:
	LArray<LWebSocket*> webSockets;

public:
	constexpr static const char *sContentLength = "Content-Length";

	LHttpServer::Callback *callback;
	LHttpServer_TraceSocket Listen;
	int port;
	LCancel *cancel = nullptr;
	LCancel localCancel;
	
	// Lock before using
	using LMsgArray = LArray<LMessage*>;
	LThreadSafeInterface<LMsgArray> messages;

	LHttpServerPriv(LHttpServer::Callback *cb, int portVal, LCancel *cancelObj) :
		LThread("LHttpServerPriv.Thread"),
		LMutex("LHttpServerPriv.Lock"),
		callback(cb),
		port(portVal),
		cancel(cancelObj ? cancelObj : &localCancel),
		messages(new LMsgArray, "messages.lock")
	{
		Run();
	}

	~LHttpServerPriv()
	{
		Listen.Close();
		cancel->Cancel();
		WaitForExit();
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->m)
		{
			case M_ADD_WEBSOCKET:
			{
				Auto lck(this, _FL);
				auto ws = Msg->AutoA<LWebSocket>();
				if (ws)
					webSockets.Add(ws.Release());
				else
					printf("%s:%i error: no ws to add.\n", _FL);
				break;
			}
			case M_SEND_WEBSOCKET:
			{
				Auto lck(this, _FL);
				auto ws = (LWebSocket*)Msg->A();
				auto msg = Msg->AutoB<LString>();
				if (ws && msg)
				{
					if (webSockets.HasItem(ws))
					{
						ws->SendMessage(*msg);
					}
					else printf("%s:%i error: ws not in list.\n", _FL);
				}
				else printf("%s:%i error: no ws or msg to send.\n", _FL);
				break;
			}
			case M_BROADCAST_WEBSOCKET:
			{
				Auto lck(this, _FL);
				if (auto msg = Msg->AutoA<LString>())
				{
					for (auto ws: webSockets)
					{
						ws->SendMessage(*msg);
					}
				}
				else printf("%s:%i error: no msg param.\n", _FL);
				break;
			}
		}
		
		return 0;
	}

	int Main()
	{
	    LOG_HTTP("Attempting to listen on port %i...\n", port);
		while (!Listen.Listen(port))
		{
			LOG_HTTP("...can't listen on port %i.\n", port);
			auto start = LCurrentTime();
			while (LCurrentTime() - start < 5000)
			{
				LSleep(100);
				if (cancel->IsCancelled())
					return -1;
			}
		}

		LgiTrace("Listening on port %i.\n", port);
		while (!cancel->IsCancelled())
		{
			if (Listen.IsReadable(10))
			{
				LAutoPtr<LSocketI> s(new LSocket);
				if (s)
				{
					LOG_HTTP("Accepting...\n");
					if (Listen.Accept(s))
					{
						LOG_HTTP("Got new connection...\n");
						new LHttpThread(s, this);
					}
				}
			}

			{
				Auto lck(this, _FL);
				for (auto ws: webSockets)
					ws->Read();
			}
			
			LHttpServerPriv::LMsgArray msgArr;
			{
				if (auto msgs = messages.Lock(_FL))
					msgArr.Swap(*msgs.Get());
			}
			for (auto m: msgArr)
				OnEvent(m);
			msgArr.DeleteObjects();
			
		}

		printf("Closing listen socket: %i\n", (int)Listen.Handle());
		Listen.Close();
		return 0;
	}
};

LHttpThread::LHttpThread(LAutoPtr<LSocketI> sock, LHttpServerPriv *priv) :
	LThread("LHttpThread")
{
	s = sock;
	p = priv;
	Run();
}

int LHttpThread::Main()
{
	LHttpServer::Request req;
	LArray<char> Buf;
	ssize_t Block = 4 << 10, Used = 0, ContentLen = 0;
	char *ReqEnd = nullptr; // end of the first line..
	char *HeadersEnd = nullptr; // end of the headers...
	Buf.Length(Block);
	const int NullSz = 1;

	req.sock = s;

	// Read headers...
	while (req.sock->IsOpen())
	{
		// Extend the block to fit...
		if (Buf.Length() - Used < 1)
			Buf.Length(Buf.Length() + Block);

		// Read more data...
		auto r = req.sock->Read(Buf.AddressOf(Used), Buf.Length()-Used-NullSz, 0);
		LOG_HTTP("%s:%i - read=%i\n", _FL, (int)r);
		if (r > 0)
			Used += r;

		if (!ReqEnd &&
			(ReqEnd = strnistr(Buf.AddressOf(), "\r\n", Used)))
		{
			LString line(Buf.AddressOf(), ReqEnd - Buf.AddressOf());
			auto parts = line.SplitDelimit();
			if (parts.Length() > 0)
				req.action = parts[0];
			if (parts.Length() > 1)
				req.uri = parts[1];
			if (parts.Length() > 2)
				req.protocol = parts[2];

			LOG_HTTP("%s:%i - Action=%s\n", _FL, line.Get());
		}

		if (ReqEnd &&
			!HeadersEnd &&
			(HeadersEnd = strnistr(Buf.AddressOf(), "\r\n\r\n", Used)))
		{
			// Found end of headers...
			HeadersEnd += 4;
			req.headers.Set(ReqEnd, HeadersEnd - ReqEnd);

			// LOG_HTTP("%s:%i - Headers=%i\n%s\n", _FL, (int)req.headers.Length(), req.headers.Get());
			auto len = LGetHeaderField(req.headers, LHttpServerPriv::sContentLength);
			if (len)
			{
				ContentLen = len.Int();
				LOG_HTTP("%s:%i - ContentLen=%i\n", _FL, (int)ContentLen);
			}
			else break;
		}

		if (ContentLen > 0)
		{
			if (Used - (HeadersEnd - Buf.AddressOf()) >= ContentLen)
				break;
		}

		if (r < 1)
			break;
	}

	Buf[Used] = 0;
	LOG_HTTP("%s:%i - got whole request\n", _FL);

	auto Action = &Buf[0];
	auto Eol = Strnstr(Action, "\r\n", Used);
	LOG_HTTP("%s:%i - eol=%p\n", _FL, Eol);
	if (Eol)
	{
		*Eol = 0;
		Eol += 2;
		int FirstLine = (int) (Eol - Action);

		char *Uri = strchr(Action, ' ');
		if (Uri)
		{
			*Uri++ = 0;
			#if LOG_HTTP
			LgiTrace("%s:%i - uri='%s'\n", _FL, Uri);
			#endif

			char *Protocol = strrchr(Uri, ' ');
			if (Protocol)
			{
				*Protocol++ = 0;

				LOG_HTTP("%s:%i - protocol='%s'\n", _FL, Protocol);
				if (p->callback)
				{
					char *Eoh = strnistr(Eol, "\r\n\r\n", Used - FirstLine);
					if (Eoh)
						*Eoh = 0;

					req.action = Action;
					req.uri = Uri;
					req.headers = Eol;
					req.body = Eoh + 4;

					LStringPipe *RespBody = nullptr;
					LHttpServer::Response resp;
					resp.body.Reset(RespBody = new LStringPipe(256));

					auto Code = p->callback->OnRequest(&req, &resp);
					if (req.sock)
					{
						LOG_HTTP("%s:%i - Code=%i\n", _FL, Code);

						auto stream = dynamic_cast<LSocket*>(req.sock.Get());
						stream->Print("HTTP/1.0 %i Ok\r\n", Code);

						if (resp.body)
						{
							auto len = resp.body->GetSize();
							if (len >= 0)
							{
								// Make sure we have a Content-Length
								auto contentLen = resp.GetHeader(LHttpServerPriv::sContentLength);
								if (!contentLen)
									resp.SetHeader(LHttpServerPriv::sContentLength, LString::Fmt(LPrintfInt64, len));
							}
						}

						if (auto hdrs = resp.headers.Strip())
						{
							stream->Print("%s\r\n", hdrs.Get());
						}
						stream->Print("\r\n");
						
						if (resp.body)
						{
							auto bytes = resp.body->GetSize();
							LOG_HTTP("%s:%i - Response len=%i\n", _FL, (int)bytes);
							
							LCopyStreamer cp(256 << 10);
							auto copied = cp.Copy(resp.body, stream);
							if (copied != bytes)
								LOG_HTTP("%s:%i - body copy failed! " LPrintfSizeT " -> " LPrintfSSizeT "\n", bytes, copied);
						}
					}
				}
			}
		}
	}

	if (req.sock)
		req.sock->Close();

	return 0;
}

LHttpServer::LHttpServer(LHttpServer::Callback *cb, int port, LCancel *cancel)
{
	d = new LHttpServerPriv(cb, port, cancel);
}

LHttpServer::~LHttpServer()
{
	DeleteObj(d);
}

bool LHttpServer::IsExited()
{
	return d->IsExited();
}

void LHttpServer::WebsocketSend(LWebSocket *ws, LString msg)
{
	PostEvent(M_SEND_WEBSOCKET, (LMessage::Param)ws, (LMessage::Param)new LString(msg));
}

void LHttpServer::WebsocketBroadcast(LString msg)
{
	PostEvent(M_BROADCAST_WEBSOCKET, (LMessage::Param)new LString(msg));
}

bool LHttpServer::WebsocketUpgrade(LHttpServer::Request *req, LWebSocketBase::OnMsg onMsg)
{
	if (!req || !onMsg)
		return false;

	if (auto ws = new LWebSocket(req->sock, true, onMsg))
	{
		// printf("Upgrading ws...\n");
		if (ws->Upgrade(req->headers))
		{
			// printf("Adding ws...\n");
			PostEvent(M_ADD_WEBSOCKET, (LMessage::Param)ws);
			return true;
		}
		else
		{
			printf("Error: upgrade failed...\n");
			delete ws;
		}
	}

	return false;
}

bool LHttpServer::PostEvent(int Cmd, LMessage::Param a, LMessage::Param b, int64_t TimeoutMs)
{
	auto msgs = d->messages.Lock(_FL);
	if (!msgs)
		return false;

	msgs->Add(new LMessage(Cmd, a, b));	
	return true;
}

char *LHttpServer::Callback::FormDecode(char *s)
{
	char *i = s, *o = s;
	while (*i)
	{
		if (*i == '+')
		{
			*o++ = ' ';
			i++;
		}
		else if (*i == '%')
		{
			char h[3] = {i[1], i[2], 0};
			*o++ = htoi(h);
			i += 3;
		}
		else
		{
			*o++ = *i++;
		}
	}
	*o = 0;
	return s;
}

char *LHttpServer::Callback::HtmlEncode(const char *s)
{
	LStringPipe p;
	const char *e = "<>";

	while (s && *s)
	{
		auto b = s;
		while (*s && !strchr(e, *s)) s++;
		
		if (s > b)
			p.Write(b, s - b);
		
		if (*s)
		{
			p.Print("&#%i;", *s);
			s++;
		}
		else break;
	}

	return p.NewStr();
}

bool LHttpServer::Callback::ParseHtmlWithDom(LVariant &Out, LDom *Dom, const char *Html)
{
	if (!Dom || !Html)
		return false;

	LStringPipe p;
	for (auto s = Html; s && *s; )
	{
		char *e = stristr(s, "<?");
		if (e)
		{
			p.Write(s, e - s);
			s = e + 2;
			while (*s && strchr(" \t\r\n", *s)) s++;
			auto v = s;
			while (*s && (isdigit(*s) || isalpha(*s) || strchr(".[]", *s)) ) s++;
			char *Var = NewStr(v, s - v);
			while (*s && strchr(" \t\r\n", *s)) s++;
			if (strncmp(s, "?>", 2) == 0)
			{
				LVariant Value;
				if (Dom->GetValue(Var, Value))
				{
					p.Push(Value.CastString());
				}

				s += 2;
			}
			else break;
		}
		else
		{
			p.Push(s);
			break;
		}
	}

	Out.Empty();
	Out.Type = GV_STRING;
	Out.Value.String = p.NewStr();
	return true;
}
