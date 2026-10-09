#pragma once

#include <lgi/common/WebSocket.h>
#include <lgi/common/Capabilities.h>

class LHttpServer :
	public LEventSinkI
{
	struct LHttpServerPriv *d;

public:
	constexpr static const char *CapMkcert = "mkcert";
	constexpr static const char *CapHttpsCert = "https-cert";

	struct Request
	{
		LAutoPtr<LSocketI> sock;
		
		LString action;
		LString uri;
		LString protocol;

		LString headers;
		LString body;
	};
	
	struct Response
	{
		// By default these are both LStringPipe's allocated by LHttpThread::Main,
		// However the LHttpServer::Callback::OnRequest can swap out the object for
		// something more suitable if needed. E.g. body -> LFile or something.
		LString headers;
		LAutoPtr<LStream> body;

		LString GetHeader(LString field)
		{
			return LGetHeaderField(headers, field);
		}
		
		bool SetHeader(LString field, LString value)
		{
			if (!field)
				return false;

			return LSetHeaderFeild(headers, field, value);
		}
	};

	struct Callback
	{
		virtual ~Callback() {}

		char *FormDecode(char *s);
		char *HtmlEncode(const char *s);
		bool ParseHtmlWithDom(LVariant &Out, LDom *Dom, const char *Html);

		virtual int OnRequest(Request *req, Response *resp) = 0;
	};

	struct SecureSockets
	{
		LString::Array domains;
		LString certFile;
		LString keyFile;
		LCapabilityClient *caps = nullptr;

		operator bool()
		{
			return domains.Length() > 0;
		}

		// Add the localhost domains and any private IP's to the list of domains.
		bool configureDefaultDomains();

		// Configure an external cert folder:
		bool configureCertFolder(const char *folder);

		// Configure a cert and key file.
		bool setupCerts();

	protected:
		// Check if CapMkcert or CapHttpsCert is available, creating them if required.
		bool checkRequirement(const char *req);

		// Scan a folder for key and cert files.
		bool ScanForKeyAndCert(const char* folder);
	};
	
	/// This callback is called when the server can't listen on the desired port
	/// If the function returns true, the server will try and kill the process.
	std::function<bool(int pid, LString process)> onListenError;

	LHttpServer(Callback *cb, int port, SecureSockets *secureSockets, LCancel *cancel, LCapabilityTarget *capTarget = nullptr);
	~LHttpServer();

	bool IsExited();
	bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t TimeoutMs = -1) override;

	// Websocket support:
	bool WebsocketUpgrade(Request *req, LWebSocketBase::OnMsg onMsg);
	void WebsocketSend(LWebSocket *ws, LString msg); // send to a specific websocket
	void WebsocketBroadcast(LString msg); // send to ALL connected websockets
};
