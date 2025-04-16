#pragma once

#include <lgi/common/WebSocket.h>

class LHttpServer :
	public LEventSinkI
{
	struct LHttpServerPriv *d;

public:
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
		LStream *headers = nullptr;
		LStream *body = nullptr;
	};

	struct Callback
	{
		virtual ~Callback() {}

		char *FormDecode(char *s);
		char *HtmlEncode(const char *s);
		bool ParseHtmlWithDom(LVariant &Out, LDom *Dom, const char *Html);

		virtual int OnRequest(Request *req, Response *resp) = 0;
	};

	LHttpServer(Callback *cb, int port, LCancel *cancel);
	~LHttpServer();

	bool IsExited();
	bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t TimeoutMs = -1) override;

	// Websocket support:
	bool WebsocketUpgrade(Request *req, LWebSocketBase::OnMsg onMsg);
	void WebsocketSend(LWebSocket *ws, LString msg); // send to a specific websocket
	void WebsocketBroadcast(LString msg); // send to ALL connected websockets
};
