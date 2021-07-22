#include "lgi/common/Lgi.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/Base64.h"
#include "lgi/common/NetTools.h"
#include "lgi/common/OAuth2.h"
#include "lgi/common/Json.h"

//////////////////////////////////////////////////////////////////
#define LOCALHOST_PORT		54900
#define OPT_AccessToken		"AccessToken"
#define OPT_RefreshToken	"RefreshToken"

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
	return NULL;
}

ssize_t ChunkSize(ssize_t &Pos, LString &Buf, LString &Body)
{
	static LString Eol("\r\n");
	auto End = Buf.Find(Eol, Pos);
	if (End > Pos)
	{
		auto Sz = Buf(Pos, End).Int(16);
		if (Sz >= 0)
		{
			End += Eol.Length();

			auto Bytes = End + Sz + Eol.Length();
			if (Buf.Length() >= Bytes)
			{
				Body += Buf(End, End + Sz);
				Pos = End + Sz + Eol.Length();
				return Sz;
			}
		}
	}

	return -1;
}

static bool GetHttp(LSocketI *s, LString &Hdrs, LString &Body, bool IsResponse)
{
	LString Resp = GetHeaders(s);

	char Buf[512];
	ssize_t Rd;
	auto BodyPos = Resp.Find("\r\n\r\n");
	LAutoString Len(InetGetHeaderField(Resp, "Content-Length", BodyPos));
	if (Len)
	{
		int Bytes = atoi(Len);
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
		LAutoString Te(InetGetHeaderField(Resp, "Transfer-Encoding", BodyPos));
		bool Chunked = Te && !_stricmp(Te, "chunked");

		if (Chunked)
		{
			ssize_t Pos = 0;

			Hdrs = Resp(0, BodyPos);
			LString Raw = Resp(BodyPos + 4, -1);
			Body.Empty();

			while (s->IsOpen())
			{
				auto Sz = ChunkSize(Pos, Raw, Body);
				if (Sz == 0)
					break;
				if (Sz < 0)
				{
					Rd = s->Read(Buf, sizeof(Buf));
					if (Rd > 0)
						Raw += LString(Buf, Rd);
					else
						break;
				}
			}

			return true;
		}
		else
		{
			while ((Rd = s->Read(Buf, sizeof(Buf))) > 0)
				Resp += LString(Buf, Rd);
		}
	}

	Hdrs = Resp(0, BodyPos);
	Body = Resp(BodyPos + 4, -1);

	return true;
}

static LString UrlFromHeaders(LString Hdrs)
{
	auto Lines = Hdrs.Split("\r\n", 1);
	auto p = Lines[0].SplitDelimit();
	if (p.Length() < 3)
	{
		return NULL;
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
	return p.NewGStr();
}

struct LOAuth2Priv
{
	LOAuth2::Params Params;
	LString Id;
	LStream *Log;
	LString Token;
	LString CodeVerifier;
	LStringPipe LocalLog;
	GDom *Store;
	LCancel *Cancel;

	LString AccessToken, RefreshToken;
	int64 ExpiresIn;

	struct Server : public LSocket
	{
		LSocket Listen;
		LOAuth2Priv *d;
		LSocket s;

	public:
		LHashTbl<ConstStrKey<char,false>,LString> Params;
		LString Body;

		Server(LOAuth2Priv *cd) : d(cd)
		{
			auto Start = LCurrentTime();
			while (	!d->Cancel->IsCancelled() &&
					!Listen.Listen(LOCALHOST_PORT) &&
					(LCurrentTime() - Start) < 60000)
			{
				d->Log->Print("Error: Can't listen on %i... (%s)\n", LOCALHOST_PORT, Listen.GetErrorString());
				LgiSleep(1000);
			}
		}

		bool GetReq()
		{
			while (!d->Cancel->IsCancelled())
			{
				if (Listen.IsReadable(100) &&
					Listen.Accept(&s))
				{
					// Read access code out of response
					LString Hdrs;
					if (GetHttp(&s, Hdrs, Body, false))
					{
						auto Url = UrlFromHeaders(Hdrs);
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

		bool Response(const char *Txt)
		{
			LString Msg;
			Msg.Printf("HTTP/1.0 200 OK\r\n"
						"\r\n"
						"<html>\n"
						"<body>%s</body>\n"
						"</html>",
						Txt);
			return ::Write(&s, Msg);
		}
	};

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

		Server Svr(this);
		LString Endpoint;
		Endpoint.Printf(Params.ApiUri, Id.Get());
		CodeVerifier = ToText(SslSocket::Random(48));

		LUri u(Endpoint);
		LString Uri, Redir, RedirEnc, Scope;
		Redir.Printf("http://localhost:%i", LOCALHOST_PORT);
		Scope = u.EncodeStr(Params.Scope);
		RedirEnc = u.EncodeStr(Redir, ":/");
		Uri.Printf("%s?client_id=%s&redirect_uri=%s&response_type=code&code_challenge=%s&scope=%s",
					Params.AuthUri.Get(),
					Params.ClientID.Get(),
					RedirEnc.Get(),
					CodeVerifier.Get(),
					Scope.Get());
		if (Log)
			Log->Print("%s:%i - Uri: %s\n", _FL, Uri.Get());
		LExecute(Uri); // Open browser for user to auth

		if (Svr.GetReq())
		{
			Token = Svr.Params.Find("code");
			Svr.Response(Token ? "Ok: Got token. You can close this window/tab now." : "Error: No token.");
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
						"redirect_uri=http://localhost:%i&"
						"code_verifier=%s&"
						"grant_type=authorization_code",
						FormEncode(Token).Get(),
						Params.ClientID.Get(),
						LOCALHOST_PORT,
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

			// Log->Print("Body=%s\n", Body.Get());
			LJson j(Body);

			AccessToken = j.Get("access_token");
			RefreshToken = j.Get("refresh_token");
			ExpiresIn = j.Get("expires_in").Int();

			if (!AccessToken)
				Log->Print("Failed to get AccessToken: %s\n", Body.Get());
		}

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

	LOAuth2Priv(LOAuth2::Params &params, const char *account, GDom *store, LStream *log, LCancel *cancel)
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
		LString Key, kAccTok, kRefreshTok;
		Key.Printf("%s.%s", Params.Scope.Get(), Id.Get());
		auto KeyB64 = Base64(Key);
		kAccTok.Printf("OAuth2-%s-%s", OPT_AccessToken, KeyB64.Get());
		kAccTok = kAccTok.RStrip("=");
		kRefreshTok.Printf("OAuth2-%s-%s", OPT_RefreshToken, KeyB64.Get());
		kRefreshTok= kRefreshTok.RStrip("=");

		if (Write)
		{
			Store->SetValue(kAccTok, v = AccessToken.Get());
			Store->SetValue(kRefreshTok, v = RefreshToken.Get());
		}
		else
		{
			if (Store->GetValue(kAccTok, v)) AccessToken = v.Str();
			else return false;
			if (Store->GetValue(kRefreshTok, v)) RefreshToken = v.Str();
			else return false;
		}

		return true;
	}
};

LOAuth2::LOAuth2(LOAuth2::Params &params, const char *account, GDom *store, LCancel *cancel, LStream *log)
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

LString LOAuth2::GetAccessToken()
{
	if (d->AccessToken)
		return d->AccessToken;

	if (d->GetToken())
	{
		d->Log->Print("Got token.\n");
		if (d->GetAccess())
			return d->AccessToken;
		else
			d->Log->Print("No access.\n");
			
	}
	else d->Log->Print("No token.\n");

	return LString();
}
