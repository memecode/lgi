#include "lgi/common/Lgi.h"
#include "lgi/common/HttpUi.h"
#include "lgi/common/Net.h"

struct LHttpServerPriv;

#if false
#define LOG_HTTP(...)	printf(__VA_ARGS__)
#else
#define LOG_HTTP(...)
#endif

class LHttpThread : public LThread
{
	LSocket *s;
	LHttpServerPriv *p;

public:
	LHttpThread(LSocket *sock, LHttpServerPriv *priv);
	int Main();
};

class LHttpServer_TraceSocket : public LSocket
{
public:
	void OnInformation(char *Str)
	{
		LgiTrace("SocketInfo: %s\n", Str);
	}

	void OnError(int ErrorCode, const char *ErrorDescription)
	{
		LgiTrace("SocketError %i: %s\n", ErrorCode, ErrorDescription);
	}
};

struct LHttpServerPriv : public LThread
{
	LHttpServer::Callback *callback;
	LHttpServer_TraceSocket Listen;
	int port;
	LCancel *cancel = nullptr;
	LCancel localCancel;

	LHttpServerPriv(LHttpServer::Callback *cb, int portVal, LCancel *cancelObj) :
		LThread("LHttpServerPriv"),
		callback(cb),
		port(portVal),
		cancel(cancelObj ? cancelObj : &localCancel)
	{
		Run();
	}

	~LHttpServerPriv()
	{
		Listen.Close();
		cancel->Cancel();
		WaitForExit();
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

		LOG_HTTP("Listening on port %i.\n", port);
		while (!cancel->IsCancelled())
		{
			if (Listen.IsReadable(100))
			{
				if (auto s = new LSocket)
				{
					LOG_HTTP("Accepting...\n");
					if (Listen.Accept(s))
					{
						LOG_HTTP("Got new connection...\n");
						new LHttpThread(s, this);
					}
				}
			}
		}

		return 0;
	}
};

LHttpThread::LHttpThread(LSocket *sock, LHttpServerPriv *priv) :
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

	// Read headers...
	while (s->IsOpen())
	{
		// Extend the block to fit...
		if (Buf.Length() - Used < 1)
			Buf.Length(Buf.Length() + Block);

		// Read more data...
		auto r = s->Read(Buf.AddressOf(Used), Buf.Length()-Used-NullSz, 0);
		LOG_HTTP("%s:%i - read=%i\n", _FL, (int)r);
		if (r > 0)
			Used += r;

		if (!ReqEnd &&
			(ReqEnd = strnistr(Buf.AddressOf(), "\r\n", Used)))
		{
			auto parts = LString(Buf.AddressOf(), ReqEnd - Buf.AddressOf()).SplitDelimit();
			if (parts.Length() > 0)
				req.action = parts[0];
			if (parts.Length() > 1)
				req.uri = parts[1];
			if (parts.Length() > 2)
				req.protocol = parts[2];
		}

		if (ReqEnd &&
			!HeadersEnd &&
			(HeadersEnd = strnistr(Buf.AddressOf(), "\r\n\r\n", Used)))
		{
			// Found end of headers...
			HeadersEnd += 4;
			req.headers.Set(ReqEnd, HeadersEnd - ReqEnd);

			LOG_HTTP("%s:%i - HeaderLen=%i\n", _FL, (int)req.headers.Length());
			auto s = LGetHeaderField(req.headers, "Content-Length");
			if (s)
			{
				ContentLen = s.Int();
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
		int FirstLine = Eol - Action;

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

					LHttpServer::Response resp;
					LStringPipe RespHeaders(256), RespBody(1024);
					resp.headers = &RespHeaders;
					resp.body = &RespBody;
					auto Code = p->callback->OnRequest(&req, &resp);
					LOG_HTTP("%s:%i - Code=%i\n", _FL, Code);

					s->Print("HTTP/1.0 %i Ok\r\n", Code);
					if (auto hdrs = RespHeaders.NewLStr().Strip())
						s->Print("%s\r\n", hdrs.Get());
					s->Print("\r\n");
					
					if (auto body = RespBody.NewLStr())
					{
						LOG_HTTP("%s:%i - Response(%i)=%p\n", _FL, (int)body.Length(), body.Get());
						s->Write(body);
					}
				}
			}
		}
	}

	s->Close();
	DeleteObj(s);

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
		char *b = s;
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
	for (char *s = Html; s && *s; )
	{
		char *e = stristr(s, "<?");
		if (e)
		{
			p.Write(s, e - s);
			s = e + 2;
			while (*s && strchr(" \t\r\n", *s)) s++;
			char *v = s;
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
