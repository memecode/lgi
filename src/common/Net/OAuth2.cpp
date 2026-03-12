#include "lgi/common/Lgi.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/Base64.h"
#include "lgi/common/OAuth2.h"
#include "lgi/common/Json.h"
#include "lgi/common/Http.h"

//////////////////////////////////////////////////////////////////
#define LOCALHOST_PORT		54900
#define OPT_AccessToken		"OAuthAccessToken"
#define OPT_RefreshToken	"OAuthRefreshToken"

#if 0
#define LOG(...)			LgiTrace(__VA_ARGS__)
#else
#define LOG(...)
#endif

static LString GetHeaders(LSocketI *s)
{
	char Buf[256];
	ssize_t Rd;
	LString p;
	while ((Rd = s->Read(Buf, sizeof(Buf))) > 0)
	{
		p += LString(Buf, Rd);
		if (p.Find("\r\n\r\n") >= 0)
			return p;
	}
	
	s->Close();
	return LString();
}

static bool GetHttp(LSocketI *s, LString &Hdrs, LString &Body, bool IsResponse)
{
	auto Resp = GetHeaders(s);
	// printf("%s:%i - Resp=%s\n", _FL, Resp.Get());
	if (!Resp)
		return false;

	char Buf[1024];
	ssize_t Rd;
	auto BodyPos = Resp.Find("\r\n\r\n");
	LAssert(BodyPos > 0);
	Hdrs = Resp(0, BodyPos);

	auto Len = LGetHeaderField(Hdrs, "Content-Length");
	if (Len)
	{
		auto Bytes = Len.Int();
		size_t Total = BodyPos + 4 + Bytes;
		while (Resp.Length() < Total)
		{
			Rd = s->Read(Buf, sizeof(Buf));
			if (Rd > 0)
			{
				Resp += LString(Buf, Rd);
			}
		}
	}
	else if (s->IsOpen() && IsResponse)
	{
		auto TransferEncoding = LGetHeaderField(Hdrs, "Transfer-Encoding");
		bool Chunked = TransferEncoding.Equals("chunked");
		if (Chunked)
		{
			BodyPos += 4;
			
			// Transfer the remaining body into 'Buf':
			ssize_t used = Resp.Length() - BodyPos;
			if (used > sizeof(Buf))
			{
				LAssert(!"Too much body to transfer to Buf");
				return false;
			}
			memcpy(Buf, Resp.Get() + BodyPos, used);
			
			// Read the rest of the transfer:
			LStringPipe out(1024);
			auto err = LHttp::ReadChunked(s, &out, Buf, sizeof(Buf), used);
			if (err)
			{
				LAssert(!"chunked read failed");
				return false;
			}
			
			Body = out.NewLStr();
			// printf("ChunkedBody=%s\n", Body.Get());
			return true;
		}
		else
		{
			while ((Rd = s->Read(Buf, sizeof(Buf))) > 0)
				Resp += LString(Buf, Rd);
		}
	}

	Body = Resp(BodyPos + 4, -1);

	return true;
}

static LString UrlFromHeaders(LString Hdrs)
{
	auto Lines = Hdrs.Split("\r\n", 1);
	auto p = Lines[0].SplitDelimit();
	if (p.Length() < 3)
	{
		return LString();
	}

	return p[1];
}

static bool Write(LSocketI *s, LString b)
{
	for (size_t i = 0; i < b.Length(); )
	{
		auto Wr = s->Write(b.Get() + i, b.Length() - i);
		if (Wr <= 0)
			return false;
		i += Wr;
	}
	return true;
}

static LString FormEncode(const char *s, bool InValue = true)
{
	LStringPipe p;
	for (auto c = s; *c; c++)
	{
		if (isalpha(*c) || isdigit(*c) || *c == '_' || *c == '.' || (!InValue && *c == '+') || *c == '-' || *c == '%')
		{
			p.Write(c, 1);
		}
		else if (*c == ' ')
		{
			p.Write((char*)"+", 1);
		}
		else
		{
			p.Print("%%%02.2X", *c);
		}
	}
	return p.NewLStr();
}

struct LOAuth2Priv
{
	LOAuth2::Params Params;
	LString Id;
	LStream *Log = nullptr;
	LString Token;
	LString CodeVerifier;
	LStringPipe LocalLog;
	LDom *Store = nullptr;
	LCancel *Cancel = nullptr;
	LString RedirectUri;

	LString AccessToken, RefreshToken;
	int64 ExpiresIn;

	struct OAuth2Server : public LStream
	{
		SslSocket Listen;
		LOAuth2Priv *d;
		SslSocket s = nullptr;
		bool listening = false;

	public:
		LHashTbl<ConstStrKey<char,false>,LString> Params;
		LString Body;

		OAuth2Server() :
			Listen(this, nullptr/*caps*/, true)
		{
		}

		void SetPriv(LOAuth2Priv *priv)
		{
			if (d = priv)
			{
				if (d->Params.SslKey && d->Params.SslCert)
					Listen.SetCert(d->Params.SslCert, d->Params.SslKey);
		
				if (!listening)
				{
					Listen.SetCancel(d->Cancel);
					listening = Listen.Listen(LOCALHOST_PORT);
				}
			}
		}

		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
		{
			LgiTrace("%s:%i OAuth2Server: %.*s\n", _FL, (int)Size, Ptr);
			return Size;
		}

		bool GetReq()
		{
			while (!d->Cancel->IsCancelled())
			{
				if (Listen.IsReadable(100) &&
					Listen.Accept(&s))
				{
					printf("%s:%i - accepting https connection...\n", _FL);
					
					// Read access code out of response
					LString Hdrs;
					if (GetHttp(&s, Hdrs, Body, false))
					{
						auto Url = UrlFromHeaders(Hdrs);
						printf("%s:%i - Url=%s\n", _FL, Url.Get());

						auto Vars = Url.Split("?", 1);
						if (Vars.Length() != 2)
						{
							return false;
						}

						Vars = Vars[1].Split("&");
						for (auto v : Vars)
						{
							auto p = v.Split("=", 1);
							if (p.Length() != 2)
								continue;
							Params.Add(p[0], p[1]);
						}

						return true;
					}
				}
			}

			return false;
		}

		bool Response(const char *Txt, int StatusCode = 200)
		{
			LString Msg;
			Msg.Printf("HTTP/1.0 %i OK\r\n"
						"\r\n"
						"<html>\n"
						"<body>%s</body>\n"
						"</html>",
						StatusCode, Txt);
			auto status = ::Write(&s, Msg);
			s.Close();
			return status;
		}
	};
	
	static LAutoPtr<OAuth2Server> httpsServer;

	LString Base64(LString s)
	{
		LString b;
		b.Length(BufferLen_BinTo64(s.Length()));
		ConvertBinaryToBase64(b.Get(), b.Length(), (uchar*)s.Get(), s.Length());
		b.Get()[b.Length()] = 0;
		return b;
	}

	LString ToText(LString Bin)
	{
		LArray<char> t;
		for (char i='0'; i<='9'; i++) t.Add(i);
		for (char i='a'; i<='z'; i++) t.Add(i);
		for (char i='A'; i<='Z'; i++) t.Add(i);
		t.Add('-'); t.Add('.'); t.Add('_'); t.Add('~');
		LString Txt;
		Txt.Length(Bin.Length());
		int Pos = 0;
		for (int i=0; i<Bin.Length(); i++)
		{
			uint8_t n = Bin.Get()[i];
			Pos += n;
			Txt.Get()[i] = t[Pos % t.Length()];
		}
		Txt.Get()[Bin.Length()] = 0;
		return Txt;
	}

	bool GetToken()
	{
		if (Token)
			return true;

		if (!httpsServer && !httpsServer.Reset(new OAuth2Server))
			return false;
		httpsServer->SetPriv(this);

		LString Endpoint;
		Endpoint.Printf(Params.ApiUri, Id.Get());
		CodeVerifier = ToText(SslSocket::Random(48));

		LUri u(Endpoint);
		LString Uri, RedirEnc, Scope;
		RedirectUri.Printf("https://localhost:%i/auth", LOCALHOST_PORT);
		// Redir.Printf("https://memecode.com/scribe/oauth2.php");
		Scope = u.EncodeStr(Params.Scope);
		RedirEnc = u.EncodeStr(RedirectUri, ":/");
		Uri.Printf("%s?client_id=%s&redirect_uri=%s&response_type=code&code_challenge=%s&scope=%s",
					Params.AuthUri.Get(),
					Params.ClientID.Get(),
					RedirEnc.Get(),
					CodeVerifier.Get(),
					Scope.Get());
		if (Log)
			Log->Print("%s:%i - Uri: %s\n", _FL, Uri.Get());

		LOG("%s: open browser: %s\n", __func__, Uri.Get());
		LExecute(Uri); // Open browser for user to auth

		LOG("%s: http get req...\n", __func__);
		if (httpsServer->GetReq())
		{
			Token = httpsServer->Params.Find("code");
			LOG("%s: Token='%s'\n", __func__, Token.Get());
			if (Log)
				Log->Print("%s:%i - Token='%s'\n", _FL, Token.Get());
			LOG("%s: sending resp...\n", __func__);
			httpsServer->Response(Token ? "Ok: Got token. You can close this window/tab now." : "Error: No token.");
			LOG("%s: sent resp.\n", __func__);
		}

		return Token != NULL;
	}

	bool GetAccess()
	{
		if (!AccessToken)
		{
			LStringPipe p(1024);
			LUri u(Params.ApiUri);
			SslSocket sock(NULL, NULL, true);
			if (!sock.Open(u.sHost, HTTPS_PORT))
			{
				Log->Print("Error: Can't connect to '%s:%i'\n", u.sHost.Get(), HTTPS_PORT);
				return NULL;
			}
		
			LString Body, Http;
			Body.Printf("code=%s&"
						"client_id=%s&"
						"redirect_uri=%s&"
						"code_verifier=%s&"
						"grant_type=authorization_code",
						FormEncode(Token).Get(),
						Params.ClientID.Get(),
						RedirectUri.Get(),
						FormEncode(CodeVerifier).Get());
			if (Params.ClientSecret)
			{
				Body += "&client_secret=";
				Body += Params.ClientSecret;
			}

			LUri Api(Params.ApiUri);
			Http.Printf("POST %s HTTP/1.1\r\n"
						"Host: %s\r\n"
						"Content-Type: application/x-www-form-urlencoded\r\n"
						"Content-length: " LPrintfSizeT "\r\n"
						"\r\n"
						"%s",
						Api.sPath.Get(),
						Api.sHost.Get(),
						Body.Length(),
						Body.Get());
			LOG("%s: writing post req\n", __func__);
			if (!Write(&sock, Http))
			{
				Log->Print("%s:%i - Error writing to socket.\n", _FL);
				return false;
			}

			LString Hdrs;
			LOG("%s: reading resp\n", __func__);
			if (!GetHttp(&sock, Hdrs, Body, true))
			{
				return false;
			}

			LOG("%s: got body='%s'\n", __func__, Body.Get());
			LJson j(Body);

			AccessToken = j.Get("access_token");
			RefreshToken = j.Get("refresh_token");
			ExpiresIn = j.Get("expires_in").Int();

			if (!AccessToken)
				Log->Print("Failed to get AccessToken: %s\n", Body.Get());
		}

		LOG("%s: AccessToken='%s'\n", __func__, AccessToken.Get());
		return AccessToken.Get() != NULL;
	}

	bool Refresh()
	{
		if (!RefreshToken)
			return false;

		LStringPipe p(1024);
		LUri u(Params.Scope);
		SslSocket sock(NULL, NULL, true);
		if (!sock.Open(u.sHost, HTTPS_PORT))
		{
			Log->Print("Error: Can't connect to '%s:%i'\n", u.sHost.Get(), HTTPS_PORT);
			return NULL;
		}
		
		LString Body, Http;
		Body.Printf("refresh_token=%s&"
					"client_id=%s&"
					"client_secret=%s&"
					"grant_type=refresh_token",
					FormEncode(RefreshToken).Get(),
					Params.ClientID.Get(),
					Params.ClientSecret.Get());

		LUri Api(Params.ApiUri);
		Http.Printf("POST %s HTTP/1.1\r\n"
					"Host: %s\r\n"
					"Content-Type: application/x-www-form-urlencoded\r\n"
					"Content-length: " LPrintfSizeT "\r\n"
					"\r\n"
					"%s",
					Api.sPath.Get(),
					Api.sHost.Get(),
					Body.Length(),
					Body.Get());
		if (!Write(&sock, Http))
		{
			Log->Print("%s:%i - Error writing to socket.\n", _FL);
			return false;
		}

		LString Hdrs;
		if (!GetHttp(&sock, Hdrs, Body, true))
		{
			return false;
		}

		// Log->Print("Body=%s\n%s\n", Params.ApiUri.Get(), Body.Get());
		LJson j(Body);

		AccessToken = j.Get("access_token");
		ExpiresIn = j.Get("expires_in").Int();

		return AccessToken.Get() != NULL;
	}

	LOAuth2Priv(LOAuth2::Params &params, const char *account, LDom *store, LStream *log, LCancel *cancel)
	{
		Params = params;
		Id = account;
		Store = store;
		Cancel = cancel;
		Log = log ? log : &LocalLog;
	}

	bool Serialize(bool Write)
	{
		if (!Store)
			return false;

		LVariant v;
		if (Write)
		{
			Store->SetValue(OPT_AccessToken, v = AccessToken.Get());
			Store->SetValue(OPT_RefreshToken, v = RefreshToken.Get());
		}
		else
		{
			AccessToken.Empty();
			RefreshToken.Empty();
			
			LVariant r;
			if (!Store->GetValue(OPT_AccessToken, v) ||
				!Store->GetValue(OPT_RefreshToken, r))
				return false;

			AccessToken = v.Str();
			RefreshToken = r.Str();
		}

		return true;
	}
};

LAutoPtr<LOAuth2Priv::OAuth2Server> LOAuth2Priv::httpsServer;

LOAuth2::LOAuth2(LOAuth2::Params &params, const char *account, LDom *store, LCancel *cancel, LStream *log)
{
	d = new LOAuth2Priv(params, account, store, log, cancel);
	d->Serialize(false);
}

LOAuth2::~LOAuth2()
{
	d->Serialize(true);
	delete d;
}

bool LOAuth2::Refresh()
{
	d->AccessToken.Empty();
	d->Serialize(true);
	return d->Refresh();
}

bool LOAuth2::Restart()
{
	d->AccessToken.Empty();
	d->Serialize(true);
	return true;
}

LString LOAuth2::GetAccessToken()
{
	LOG("%s: AccessToken='%s'\n", __func__, d->AccessToken.Get());
	if (d->AccessToken)
		return d->AccessToken;

	LOG("%s: calling GetToken...\n", __func__);
	if (d->GetToken())
	{
		LOG("%s: got token, calling GetAccess...\n", __func__);
		d->Log->Print("Got token.\n");
		auto result = d->GetAccess();
		LOG("%s: GetAccess=%i\n", __func__, result);
		if (result)
		{
			return d->AccessToken;
		}
		else
		{
			d->Log->Print("No access.\n");
		}
			
	}
	else d->Log->Print("No token.\n");

	return LString();
}
