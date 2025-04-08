#pragma once

#include <lgi/common/WebSocket.h>

class LHttpServer
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
	bool WebsocketUpgrade(Request *req, LWebSocketBase::OnMsg onMsg);
};
