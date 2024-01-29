#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "lgi/common/Gdc2.h"
#include "lgi/common/Http.h"
#include "lgi/common/LgiCommon.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Variant.h"

#define DEBUG_LOGGING		0

///////////////////////////////////////////////////////////////////
class ILogProxy : public LSocketI
{
	LSocketI *Dest;

public:
	ILogProxy(LSocketI *dest)
	{
		Dest = dest;
	}

	void OnError(int ErrorCode, const char *ErrorDescription)
	{
		if (Dest && ErrorDescription)
		{
			char Str[256];
			sprintf_s(Str, sizeof(Str), "[Data] %s", ErrorDescription);
			Dest->OnInformation(Str);
		}
	}

	void OnInformation(const char *s)
	{
		if (Dest && s)
		{
			char Str[256];
			sprintf_s(Str, sizeof(Str), "[Data] %s", s);
			Dest->OnInformation(Str);
		}
	}

	void OnDisconnect()
	{
		if (Dest)
		{
			Dest->OnInformation("[Data] Disconnect");
		}
	}
};

///////////////////////////////////////////////////////////////////
LHttp::LHttp(LCancel *cancel)
{
	Cancel = cancel;
	Buffer = new char[BufferLen];
}

LHttp::~LHttp()
{
	Close();
	DeleteArray(Headers);
	DeleteArray(Buffer);
}

void LHttp::SetProxy(char *p, int Port)
{
	Proxy = p;
	ProxyPort = Port;
}

void LHttp::SetAuth(char *User, char *Pass)
{
	AuthUser = User;
	AuthPassword = Pass;
}

bool LHttp::Open(LAutoPtr<LSocketI> S, const char *RemoteHost, int Port)
{
	Close();
	Socket = S;

	if (Proxy)
	{
		RemoteHost = Proxy;
		Port = ProxyPort;
	}

	if (Socket && Cancel)
		Socket->SetCancel(Cancel);
	
	if (RemoteHost)
	{
		LUri u;
		if (stristr(RemoteHost, "://") || strchr(RemoteHost, '/'))
			u.Set(RemoteHost);
		else
			u.sHost = RemoteHost;
		if (!u.Port && Port)
			u.Port = Port;

		if (!Socket)
			ErrorMsg = "No socket object.";
		else if (Socket->Open(u.sHost, u.Port > 0 ? u.Port : HTTP_PORT))
			return true;
		else
			ErrorMsg = Socket->GetErrorString();
	}
	else
		ErrorMsg = "No remote host.";

	if (ErrorMsg)
		LgiTrace("%s:%i - %s.\n", _FL, ErrorMsg.Get());

	return false;
}

bool LHttp::Close()
{
	if (Socket)
		Socket->Close();
	return 0;
}

bool LHttp::IsOpen()
{
	return Socket != NULL;
}

enum HttpRequestType
{
	HttpNone,
	HttpGet,
	HttpPost,
	HttpOther,
};

bool LHttp::Request
(
	const char *Type,
	const char *Uri,
	int *ProtocolStatus,
	const char *InHeaders,
	LStreamI *InBody,
	LStreamI *Out,
	LStreamI *OutHeaders,
	ContentEncoding *OutEncoding
)
{
	// Input validation
	if (!Socket || !Uri || !Out || !Type)
		return false;

	#if DEBUG_LOGGING
	LStringPipe Log;
	#endif

	HttpRequestType ReqType = HttpNone;
	if (!_stricmp(Type, "GET"))
		ReqType = HttpGet;
	else if (!_stricmp(Type, "POST"))
		ReqType = HttpPost;
	else
		ReqType = HttpOther;

	// Generate the request string
	LStringPipe Cmd;
	LUri u(Uri);
	bool IsHTTPS = u.sProtocol && !_stricmp(u.sProtocol, "https");
	LString EncPath = u.EncodeStr(u.sPath.Get() ? u.sPath.Get() : (char*)"/"), Mem;
	char s[1024];
	LHtmlLinePrefix EndHeaders("\r\n");
	LStringPipe Headers;

	if (IsHTTPS && Proxy)
	{
		Cmd.Print(	"CONNECT %s:%i HTTP/1.1\r\n"
					"Host: %s\r\n"
					"\r\n",
					u.sHost.Get(), u.Port ? u.Port : HTTPS_PORT,
					u.sHost.Get());
		LAutoString c(Cmd.NewStr());
		size_t cLen = strlen(c);
		ssize_t r = Socket->Write(c, cLen);
		if (r == cLen)
		{
			ssize_t Length = 0;
			while (Out)
			{
				ssize_t r = Socket ? Socket->Read(s, sizeof(s)) : -1;
				if (r > 0)
				{
					ssize_t e = EndHeaders.IsEnd(s, r);
					if (e < 0)
					{
						Headers.Write(s, r);
					}
					else
					{
						e -= Length;
						Headers.Write(s, e);
						break;
					}
					
					Length += r;
				}
				else break;
			}
			
			LAutoString Hdr(Headers.NewStr());

			LVariant v;
			if (Socket)
				Socket->SetValue(LSocket_Protocol, v = "SSL");
			
			EndHeaders.Reset();
		}
		else return false;
	}

	Cmd.Print("%s %s HTTP/1.1\r\n", Type, (Proxy && !IsHTTPS) ? Uri : EncPath.Get());
	Cmd.Print("Host: %s\r\n", u.sHost.Get());
	if (InHeaders)
		Cmd.Write(InHeaders, strlen(InHeaders));

	if (AuthUser && AuthPassword)
	{
		if (1)
		{
			// Basic authentication
			char Raw[128];
			sprintf_s(Raw, sizeof(Raw), "%s:%s", AuthUser.Get(), AuthPassword.Get());
			char Base64[128];
			ZeroObj(Base64);
			ConvertBinaryToBase64(Base64, sizeof(Base64)-1, (uchar*)Raw, strlen(Raw));
			
			Cmd.Print("Authorization: Basic %s\r\n", Base64);
		}
		else
		{
			// Digest authentication
			// Not implemented yet...
			LAssert(!"Not impl.");
		}
	}
	Cmd.Push("\r\n");
	
	auto c = Cmd.NewLStr();
	#if DEBUG_LOGGING
	Log.Print("HTTP req.hdrs=%s\n-------------------------------------\nHTTP req.body=", c.Get());
	#endif

	bool Status = false;
	if (Socket && c)
	{
		// Write the headers...
		bool WriteOk = Socket->Write(c, c.Length()) == c.Length();
		if (WriteOk)
		{
			// Write any body...
			if (InBody)
			{
				ssize_t r;
				while ((r = InBody->Read(s, sizeof(s))) > 0)
				{
					ssize_t w = Socket->Write(s, r);
					if (w < r)
					{
						return false;
					}

					#if DEBUG_LOGGING
					Log.Print("%.*s", (int)w, s);
					#endif
				}
			}
			
			// Read the response
			ssize_t Total = 0;
			ssize_t Used = 0;
			
			while (Out)
			{
				ssize_t r = Socket ? Socket->Read(s, sizeof(s)) : -1;
				if (r > 0)
				{
					ssize_t e = EndHeaders.IsEnd(s, r);
					if (e < 0)
					{
						Headers.Write(s, r);
					}
					else
					{
						e -= Total;
						Headers.Write(s, e);
						if (r > e)
						{
							// Move the tailing data down to the start of the buffer
							memmove(s, s + e, r - e);
							Used = r - e;
						}
						break;
					}
					Total += r;
				}
				else break;
			}

			// Process output
			auto h = Headers.NewLStr();
			if (h)
			{
				#if DEBUG_LOGGING
				Log.Print("HTTP res.hdrs=%s\n-------------------------------------\nHTTP res.body=", h);
				#endif

				auto sContentLen = LGetHeaderField(h, "Content-Length");
				auto ContentLen = sContentLen.Int(10, 0);
				bool IsChunked = false;
				if (ContentLen > 0)
					Out->SetSize(ContentLen);
				else
				{
					auto sTransferEncoding = LGetHeaderField(h, "Transfer-Encoding");
					IsChunked = sTransferEncoding && sTransferEncoding.Equals("chunked");
					Out->SetSize(0);
				}
				auto sContentEncoding = LGetHeaderField(h, "Content-Encoding");
				ContentEncoding Encoding = EncodeRaw;
				if (sContentEncoding && sContentEncoding.Equals("gzip"))
					Encoding = EncodeGZip;
				if (OutEncoding)
					*OutEncoding = Encoding;

				int HttpStatus = 0;
				if (_strnicmp(h, "HTTP/", 5) == 0)
				{
					HttpStatus = (int)Atoi(h.Get() + 9);
					if (ProtocolStatus)
						*ProtocolStatus = HttpStatus;
				}				
				if (HttpStatus / 100 == 3)
					FileLocation = LGetHeaderField(h, "Location");
				
				if (OutHeaders)
					OutHeaders->Write(h.Get(), h.Length());

				if (IsChunked)
				{
					#ifdef _DEBUG
					LStringPipe Log;
					#endif

					while (true)
					{
						// Try and get chunk header
						char *End = Strnstr(s, "\r\n", Used);
						if (!End)
						{
							ssize_t r = Socket->Read(s + Used, sizeof(s) - Used);
							if (r < 0)
								break;
							
							Used += r;
							
							End = Strnstr(s, "\r\n", Used);
							if (!End)
							{
								LAssert(!"No chunk header");
								break;
							}
						}

						// Process the chunk header							
						End += 2;
						ssize_t HdrLen = End - s;
						int ChunkSize = htoi(s);
						if (ChunkSize <= 0)
						{
							// End of stream.
							Status = true;
							break;
						}
						
						ssize_t ChunkDone = 0;
						memmove(s, End, Used - HdrLen);
						Used -= HdrLen;
						
						// Loop over the body of the chunk
						while (Socket && ChunkDone < ChunkSize)
						{
							ssize_t Remaining = ChunkSize - ChunkDone;
							ssize_t Common = MIN(Used, Remaining);

							#ifdef _DEBUG
							Log.Print("%s:%i - remaining:%i, common=%i\n", _FL, (int)Remaining, (int)Common);
							#endif

							if (Common > 0)
							{
								ssize_t w = Out->Write(s, Common);
								#ifdef _DEBUG
								Log.Print("%s:%i - w:%i\n", _FL, (int)w);
								#endif
								if (w != Common)
								{
									LAssert(!"Write failed.");
									break;
								}
								if (Used > Common)
								{
									#ifdef _DEBUG
									Log.Print("%s:%i - common:%i, used=%i\n", _FL, (int)Common, (int)Used);
									#endif
									memmove(s, s + Common, Used - Common);
								}
								ChunkDone += Common;
								Used -= Common;

								if (ChunkDone >= ChunkSize)
								{
									#ifdef _DEBUG
									Log.Print("%s:%i - chunkdone\n", _FL);
									#endif
									break;
								}
							}

							ssize_t r = Socket->Read(s + Used, sizeof(s) - Used);
							#ifdef _DEBUG
							Log.Print("%s:%i - r:%i\n", _FL, (int)r);
							#endif
							if (r < 0)
								break;
							
							Used += r;
						}
						
						// Loop over the CRLF postfix
						if (Socket && Used < 2)
						{
							ssize_t r = Socket->Read(s + Used, sizeof(s) - Used);
							#ifdef _DEBUG
							Log.Print("%s:%i - r:%i\n", _FL, (int)r);
							#endif
							if (r < 0)
								break;
							Used += r;
						}							
						if (Used < 2 || s[0] != '\r' || s[1] != '\n')
						{
							#ifdef _DEBUG
							LgiTrace("Log: %s\n", Log.NewLStr().Get());
							#endif
							LAssert(!"Post fix missing.");
							break;
						}
						if (Used > 2)
							memmove(s, s + 2, Used - 2);
						Used -= 2;							
					}
				}
				else
				{
					// Non chunked connection.
					int64 Written = 0;
					if (Used > 0)
					{
						// auto Pos = Out->GetPos();
						ssize_t w = Out->Write(s, Used);
						/*
						LgiTrace("%s:%i - Write @ " LPrintfInt64 " of " LPrintfSSizeT " = " LPrintfSSizeT " (%x,%x,%x,%x)\n",
							_FL, Pos, Used, w, (uint8_t)s[0], (uint8_t)s[1], (uint8_t)s[2], (uint8_t)s[3]);
						*/
						#if DEBUG_LOGGING
						Log.Write(s, w);
						#endif

						if (w == Used)
						{
							Written += w;
							Used = 0;
						}
						else
						{
							LAssert(0);
							Written = -1;
						}
					}
					
					while (	Socket &&
							Written >= 0 &&
							Written < ContentLen &&
							Socket->IsOpen())
					{
						ssize_t r = Socket->Read(s, sizeof(s));
						if (r <= 0)
						{
							LgiTrace("%s:%i - Socket read failed.\n", _FL);
							break;
						}
						
						// auto Pos = Out->GetPos();
						ssize_t w = Out->Write(s, r);
						/*
						auto NewPos = Out->GetPos();
						LgiTrace("%s:%i - Write @ " LPrintfInt64 " of " LPrintfSSizeT " = " LPrintfSSizeT " (%x,%x,%x,%x) " LPrintfSSizeT "\n",
							_FL, Pos, r, w, (uint8_t)s[0], (uint8_t)s[1], (uint8_t)s[2], (uint8_t)s[3], NewPos);
						*/
						#if DEBUG_LOGGING
						Log.Print("%.*s", (int)w, s);
						#endif

						if (w != r)
						{
							LgiTrace("%s:%i - File write failed.\n", _FL);
							break;
						}
						
						Written += w;
					}

					Status = Written == ContentLen;
					if (Written != ContentLen)
					{
						LgiTrace("%s:%i - HTTP length not reached: written=" LPrintfInt64 " content=" LPrintfInt64 "\n", _FL, Written, ContentLen);
					}
					#if DEBUG_LOGGING
					Log.Print("\n---------------------------------------------\n");
					#endif
				}
			}
		}
	}

	#if DEBUG_LOGGING
	// if (ProtocolStatus && *ProtocolStatus >= 300)
	{
		auto LogData = Log.NewLStr();
		if (LogData)
		{
			LgiTrace("%.*s\n", (int)LogData.Length(), LogData.Get());
			int asd=0;
		}
	}
	#endif

	return Status;
}

#if 0
#define HTTP_POST_LOG 1

bool LHttp::Post
(
	char *File,
	const char *ContentType,
	LStreamI *In,
	int *ProtocolStatus,
	LStreamI *Out,
	LStreamI *OutHeaders,
	char *InHeaders
)
{
	bool Status = false;

	if (Socket && File && Out)
	{
		// Read in the data part of the PUT command, because we need the
		// Content-Length to put in the headers.
		LStringPipe Content;
		while (In)
		{
			char s[1024];
			int r = In->Read(s, sizeof(s));
			if (r > 0)
			{
				Content.Write(s, r);
			}
			else break;
		}
		
		// Generate the request string
		LStringPipe Cmd;
		Cmd.Print("POST %s HTTP/1.0\r\n", File);
		if (InHeaders) Cmd.Print("%s", InHeaders);
		if (ContentType) Cmd.Print("Content-Type: %s\r\n", ContentType);
		Cmd.Print("Content-Length: %i\r\n\r\n", Content.GetSize());
		char *s = Cmd.NewStr();
		if (s)
		{
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post write headers:\n%s\n\n", s);
#endif

			// Write the headers...
			int SLen = strlen(s);
			bool WriteOk = Socket->Write(s, SLen) == SLen;
			DeleteArray(s);

#if HTTP_POST_LOG
LgiTrace("IHTTP::Post WriteOk=%i\n", WriteOk);
#endif
			if (WriteOk)
			{
				// Copy all the 'Content' to the socket
				char Buf[1024];
				while (true)
				{
					int r = Content.Read(Buf, sizeof(Buf));
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Content.Read=%i\n", r);
#endif
					if (r > 0)
					{
						int w = Socket->Write(Buf, r);
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Socket.Write=%i\n", w);
#endif
						if (w <= 0)
						{
							break;
						}
					}
					else break;
				}
				
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Starting on response\n");
#endif
				// Read the response
				LHtmlLinePrefix EndHeaders("\r\n");
				int Total = 0;
				LStringPipe Headers;
				while (Out)
				{
					int r = Socket->Read(Buf, sizeof(Buf));
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Read=%i\n", r);
#endif
					if (r > 0)
					{
						int e = EndHeaders.IsEnd(Buf, r);
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post EndHeaders=%i\n", e);
#endif
						if (e < 0)
						{
							Headers.Write(Buf, r);
						}
						else
						{
							e -= Total;
							Headers.Write(Buf, e);
							Out->Write(Buf + e, r - e);
							break;
						}
						Total += r;
					}
					else break;
				}
				
				// Process output
				char *h = Headers.NewStr();
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Headers=\n%s\n\n", h);
#endif
				if (h)
				{
					if (ProtocolStatus)
					{
						char *Eol = strchr(h, '\r');
						if (Eol)
						{
							LToken Resp(h, " ", true, Eol-h);
							if (Resp.Length() > 1)
								*ProtocolStatus = atoi(Resp[1]);
						}
					}
					if (OutHeaders)
					{
						OutHeaders->Write(h, strlen(h));
					}
					DeleteArray(h);
					
					while (Socket->IsOpen())
					{
						int r = Socket->Read(Buf, sizeof(Buf));
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post ResponseBody.Read=%i\n", r);
#endif
						if (r > 0)
						{
							Out->Write(Buf, r);
						}
						else break;
					}
					
					Status = true;
				}
			}
		}
	}
	
	return Status;
}
#endif

///////////////////////////////////////////////////////////////////////////
#include "lgi/common/OpenSSLSocket.h"
#include "lgi/common/Net.h"
#define COMP_FUNCTIONS 1
#include "lgi/common/ZlibWrapper.h"

void ZLibFree(voidpf opaque, voidpf address)
{
	// Do nothing... the memory is owned by an autoptr
}

bool LgiGetUri(LCancel *Cancel, LStreamI *Out, LString *OutError, const char *InUri, const char *InHeaders, LUri *InProxy)
{
	if (!InUri || !Out)
	{
		if (OutError)
			OutError->Printf("Parameter missing error (%p,%p)", InUri, Out);
		return false;
	}

	LHttp Http;
	int RedirectLimit = 10;
	LAutoString Location;

	for (int i=0;
		i<RedirectLimit &&
		(!Cancel || !Cancel->IsCancelled());
		i++)
	{
		LUri u(InUri);
		bool IsHTTPS = u.sProtocol && !_stricmp(u.sProtocol, "https");
		int DefaultPort = IsHTTPS ? HTTPS_PORT : HTTP_PORT;

		if (InProxy)
			Http.SetProxy(InProxy->sHost, InProxy->Port ? InProxy->Port : DefaultPort);

		LAutoPtr<LSocketI> s;
		
		if (IsHTTPS)
		{
			SslSocket *ssl;
			s.Reset(ssl = new SslSocket());
			ssl->SetSslOnConnect(true);
		}
		else
		{
			s.Reset(new LSocket);
		}
		
		if (!s)
		{
			if (OutError)
				OutError->Printf("Alloc Failed");
			return false;
		}

		if (Cancel)
			s->SetCancel(Cancel);
		s->SetTimeout(10 * 1000);

		if (!Http.Open(s, InUri, DefaultPort))
		{
			if (OutError)
				OutError->Printf("Http open failed for '%s:%i'", InUri, DefaultPort);
			return false;
		}

		const char DefaultHeaders[] =	"User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:39.0) Gecko/20100101 Firefox/39.0\r\n"
										"Accept: text/html,application/xhtml+xml,application/xml,image/png,image/*;q=0.9,*/*;q=0.8\r\n"
										"Accept-Language: en-US,en;q=0.5\r\n"
										"Accept-Encoding: gzip, deflate\r\n"
										"Connection: keep-alive\r\n";
		LString InputHeaders;
		if (InHeaders)
		{
			auto Hdrs = LString(InHeaders).SplitDelimit("\r\n");
			for (auto h: Hdrs)
			{
				LString s;
				s.Printf("%s\r\n", h.Get());
				InputHeaders += s;
			}
		}
		else
		{
			InputHeaders = DefaultHeaders;
		}

		int Status = 0;
		LHttp::ContentEncoding Enc;
		LStringPipe OutHeaders;
		LStringPipe TmpFile(4 << 10);
		Http.Get(InUri, InHeaders ? InputHeaders : DefaultHeaders, &Status, &TmpFile, &Enc, &OutHeaders);
		
		int StatusCatagory = Status / 100;
		if (StatusCatagory == 3)
		{
			LString Headers = OutHeaders.NewLStr();
			LgiTrace("Header=%s\n", Headers.Get());
			LAutoString Loc(InetGetHeaderField(Headers, "Location", -1));
			if (!Loc)
			{
				if (OutError)
					*OutError = "HTTP redirect doesn't have a location header.";
				return false;
			}
			
			if (!_stricmp(Loc, InUri))
			{
				if (OutError)
					*OutError = "HTTP redirect to same URI.";
				return false;
			}

			LgiTrace("HTTP redir(%i) from '%s' to '%s'\n", i, InUri, Loc.Get());

			LUri u(Loc);
			if (!u.sHost)
			{
				LUri in(InUri);
				in.sPath = u.sPath;
				Location.Reset(NewStr(in.ToString()));
			}
			else
			{
				Location = Loc;
			}
			InUri = Location;
			continue;
		}		
		else if (StatusCatagory != 2)
		{
			// auto Hdr = OutHeaders.NewLStr();
			Enc = LHttp::EncodeRaw;

			if (OutError)
				OutError->Printf("Got %i for '%.200s'\n", Status, InUri);
			return false;
		}

		Http.Close();
		
		if (Enc == LHttp::EncodeRaw)
		{
			// Copy TmpFile to Out
			LCopyStreamer Cp;
			if (!Cp.Copy(&TmpFile, Out))
			{
				if (OutError)
					*OutError = "Stream copy failed.";
				return false;
			}
		}
		else if (Enc == LHttp::EncodeGZip)
		{
			int64 Len = TmpFile.GetSize();
			if (Len <= 0)
			{
				if (OutError)
					*OutError = "No data to ungzip.";
				return false;
			}

			LAutoPtr<uchar,true> Data(new uchar[(size_t)Len]);
			if (!Data)
			{
				if (OutError)
					*OutError = "Alloc Failed";
				return false;
			}

			TmpFile.Read(Data, (int)Len);
			
			LAutoPtr<Zlib> z;
			if (!z)
				z.Reset(new Zlib);
			else if (!z->IsLoaded())
				z->Reload();								
			if (z && z->IsLoaded())
			{
				z_stream Stream;
				ZeroObj(Stream);
				Stream.next_in = Data;
				Stream.avail_in = (uInt) Len;
				Stream.zfree = ZLibFree;
				
				int r = z->inflateInit2_(&Stream, 16+MAX_WBITS, ZLIB_VERSION, sizeof(z_stream));
				
				uchar Buf[4 << 10];
				Stream.next_out = Buf;
				Stream.avail_out = sizeof(Buf);
				while (	Stream.avail_in > 0 &&
						(r = z->inflate(&Stream, Z_NO_FLUSH)) >= 0)
				{
					Out->Write(Buf, sizeof(Buf) - Stream.avail_out);
					Stream.next_out = Buf;
					Stream.avail_out = sizeof(Buf);
				}
				
				r = z->inflateEnd(&Stream);
			}
			else
			{
				GdcD->NeedsCapability("zlib");
				if (OutError)
					OutError->Printf("Gzip decompression not available (needs %s)", z ? z->Name() : "zlib library");
				return false;
			}
		}
		
		break;
	}
	
	return true;
}

