#include "lgi/common/Lgi.h"
#include "lgi/common/HttpServer.h"
#include "lgi/common/Net.h"
#include "lgi/common/Thread.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/SubProcess.h"

struct LHttpServerPriv;

#if 1
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

template<typename Base>
class LHttpServer_TraceSocket : public Base
{
public:
	void OnInformation(const char *Str) override
	{
		LgiTrace("%s:%i - info: %s\n", _FL, Str);
	}

	void OnError(int ErrorCode, const char *ErrorDescription) override
	{
		LgiTrace("%s:%i error %i: %s\n", _FL, ErrorCode, ErrorDescription);
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

	LHttpServer *server = nullptr;
	LHttpServer::Callback *callback;
	LAutoPtr<LSocketI> Listen;
	int port;
	LHttpServer::SecureSockets secureSockets;
	LCancel *cancel = nullptr;
	LCancel localCancel;
	LCapabilityTarget *capTarget = nullptr;
	
	// Lock before using
	using LMsgArray = LArray<LMessage*>;
	LThreadSafeInterface<LMsgArray> messages;

	LHttpServerPriv(LHttpServer *httpServer,
					LHttpServer::Callback *cb,
					int portVal,
					LHttpServer::SecureSockets *SecureSockets,
					LCancel *cancelObj,
					LCapabilityTarget *caps) :
		LThread("HttpSrvPrv.Th"),
		LMutex("LHttpServerPriv.Lock"),
		server(httpServer),
		callback(cb),
		capTarget(caps),
		port(portVal),
		cancel(cancelObj ? cancelObj : &localCancel),
		messages(new LMsgArray, "messages.lock")
	{
		if (SecureSockets)
			secureSockets = *SecureSockets;
		Run();
	}

	~LHttpServerPriv()
	{
		cancel->Cancel();
		WaitForExit();
		Listen.Reset(); // do this AFTER the thread is finished.
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
					// LOG_HTTP("%s:%i - broadcasting %s to %i sockets.\n", _FL, msg->Get(), (int)webSockets.Length());
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

	// Creates a socket of the right type, taking into account whether SSL is enabled or not.
	LSocketI *CreateSocket()
	{
		if (secureSockets)
		{
			if (auto s = new LHttpServer_TraceSocket<SslSocket>())
			{
				s->SetSslOnConnect(true);
				return s;
			}
			return nullptr;
		}
		
		return new LHttpServer_TraceSocket<LSocket>();
	}

	int Main()
	{
	    LOG_HTTP("Attempting to listen on port %i...\n", port);
	    
		Listen.Reset(CreateSocket());
		Listen->SetReuseAddress(true);

		if (secureSockets)
		{
			if (auto sslSock = dynamic_cast<SslSocket*>(Listen.Get()))
			{
				if (!secureSockets.setupCerts())
				{
					LOG_HTTP("%s:%i - error: failed to set certs.\n", _FL);
				}
				else
				{
					LOG_HTTP("%s:%i - configuring certs for domains: %s\n",
							_FL, LString(",").Join(secureSockets.domains).Get());
					sslSock->SetCert(secureSockets.certFile, secureSockets.keyFile);
				}
			}
			else LOG_HTTP("%s:%i - error: secureSockets.domains is set but Listen is not a SslSocket.\n", _FL);
		}

	    LAutoPtr<LSubProcess::IoThread> netstat;
		while (	!cancel->IsCancelled() &&
				!Listen->Listen(port))
		{
			LOG_HTTP("...can't listen on port %i.\n", port);
			
			if (server->onListenError)
			{
				#if LINUX
				if (netstat.Reset(new LSubProcess::IoThread("netstat", "-tulnp")))
				{
					netstat->onStdout = [this](auto str)
					{
						auto key = LString::Fmt(":%i", port);
						LString hdrs;
						bool noProcess = true;
						for (auto &ln: str.SplitDelimit("\r\n"))
						{
							if (ln.Find("Proto") == 0)
								hdrs = ln;
							else if (hdrs && ln.Find(key) > 0)
							{
								noProcess = false;
								
								auto pos = hdrs.Find("PID");
								auto parts = ln(pos, -1).SplitDelimit("/");
								if (parts.Length() == 2)
								{
									int pid = (int) parts[0].Int();
									if (pid > 0 &&
										server->onListenError(pid, parts[1].Strip()))
									{
										auto killPid = LString::Fmt("kill -9 %i", pid);
										printf("%s:%i - %s\n", _FL, killPid.Get());
										system(killPid);
									}
								}
							}
						}
						
						if (noProcess)
						{
							printf("%s:%i - no process on port %i...?\n", _FL, port);
						}
					};
					
					netstat->Start();
				}
				#endif
			}
			
			auto start = LCurrentTime();
			while (LCurrentTime() - start < 5000)
			{
				LSleep(100);
				if (cancel->IsCancelled())
					return -1;
			}
		}

		LgiTrace("Listening on port %i.\n", port);
		auto logTs = LCurrentTime();
		int readableCalls = 0;
		while (!cancel->IsCancelled())
		{
			/*
			auto now = LCurrentTime();
			if (now - logTs >= 1000)
			{
				logTs = now;
				LOG_HTTP("readbleCalls=%i\n", readableCalls);
				readableCalls = 0;
			}
			readableCalls++;
			*/
		
			if (Listen->IsReadable(20))
			{
				LAutoPtr<LSocketI> s(CreateSocket());
				if (s)
				{
					LOG_HTTP("Accepting...\n");
					if (Listen->Accept(s))
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

		printf("Closing listen socket: %i\n", (int)Listen->Handle());
		Listen->Close();
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
								LOG_HTTP("%s:%i - body copy failed! " LPrintfSizeT " -> " LPrintfSSizeT "\n", _FL, bytes, copied);
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

LHttpServer::LHttpServer(LHttpServer::Callback *cb, int port, SecureSockets *secureSockets, LCancel *cancel, LCapabilityTarget *capTarget)
{
	d = new LHttpServerPriv(this, cb, port, secureSockets, cancel, capTarget);
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
	{
		LOG_HTTP("%s:%i - error: can't lock messages.\n", _FL);
		return false;
	}

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
			while (*s && strchr(" \t\r\n", *s))
				s++;
			auto v = s;
			while (*s && (isdigit(*s) || isalpha(*s) || strchr(".[]", *s)) )
				s++;
			char *Var = NewStr(v, s - v);
			while (*s && strchr(" \t\r\n", *s))
				s++;
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

bool LHttpServer::SecureSockets::configureDefaultDomains()
{
	const char *localHosts[] = {"localhost", "127.0.0.1", "::1"};
	for (auto h: localHosts)
	{
		if (!domains.HasItem(h))
			domains.Add(h);
	}

	LArray<LSocket::Interface> intf;
	if (!LSocket::EnumInterfaces(intf))
	{
		LOG_HTTP("%s:%i - failed to enumerate interfaces.\n", _FL);
		return false;
	}

	for (auto &i: intf)
	{
		if (i.Ip4 && i.IsPrivate())
			domains.Add(LIpToStr(i.Ip4));
	}

	return true;
}

bool LHttpServer::SecureSockets::setupCerts()
{
	return	checkRequirement(CapMkcert) &&
			checkRequirement(CapHttpsCert);
}

bool LHttpServer::SecureSockets::ScanForKeyAndCert(const char* folder)
{
	LDirectory dir;
	for (auto b = dir.First(folder); b; b = dir.Next())
	{
		if (dir.IsDir())
			continue;
		auto name = dir.GetName();
		auto ext = LGetExtension(name);
		if (!Stricmp(ext, "pem"))
		{
			if (Stristr(name, "key.pem"))
				keyFile = dir.FullPath();
			else
				certFile = dir.FullPath();
		}
	}

	return	LFileExists(keyFile) &&
			LFileExists(certFile);
}

bool LHttpServer::SecureSockets::checkRequirement(const char *req)
{
	if (!Stricmp(req, CapMkcert))
	{
		// Check if mkcert is installed and available to call...
		LSubProcess sub("mkcert", "--version");
		LStringPipe out;
		if (!sub.Start() ||
			sub.Communicate(&out) ||
			!LCheckVersion(out.NewLStr(), "1.4"))
		{
			if (caps)
				caps->NeedsCapability(CapMkcert);
			return false;
		}
		
		return true;
	}
	else if (!Stricmp(req, CapHttpsCert))
	{
		// Check that we have SSL certs for the HTTPS server:
		if (LFileExists(keyFile) &&
			LFileExists(certFile))
		{
			// Probably ok? Can we validate them somehow?
			return true;
		}

		// Check the cert folder exists:
		LFile::Path appRoot(LSP_APP_ROOT);
		auto certFolder = appRoot / "certs";
		if (!LDirExists(certFolder))
		{
			if (!FileDev->CreateFolder(certFolder, true))
			{
				LOG_HTTP("%s:%i - Can't create cert folder '%s'\n", _FL, certFolder.GetFull().Get());
				return false;
			}
		}
		if (!LDirExists(certFolder))
		{
			LOG_HTTP("%s:%i - Cert folder '%s' missing\n", _FL, certFolder.GetFull().Get());
			return false;
		}

		// If the cert files exist... then ok, return success
		if (ScanForKeyAndCert(certFolder))
			return true;

		{
			// Then make sure the CA is installed into the browser(s):
			LSubProcess sub("mkcert", "-install");
			if (!sub.Start())
				return false;
			LStringPipe out;
			if (sub.Communicate(&out))
				return false;
		}

		{
			// Create the certificate with mkcert:
			auto arg = LString(" ").Join(domains);
			LSubProcess sub("mkcert", arg);
			sub.SetInitFolder(certFolder);
			if (!sub.Start())
			{
				LOG_HTTP("%s:%i - Failed to start 'mkcert'\n", _FL);
				return false;
			}

			LStringPipe out;
			if (sub.Communicate(&out))
			{
				LOG_HTTP("%s:%i - Failed to read 'mkcert' output\n", _FL);
				return false;
			}

			LOG_HTTP("%s:%i - mkcert ok for '%s'\n", _FL, arg.Get());
		}

		return ScanForKeyAndCert(certFolder);
	}
	else
	{
		LAssert(!"unknown capability");
	}
	
	return false;
}
