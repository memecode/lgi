#ifndef _HTTP_UI_H_
#define _HTTP_UI_H_

#include "lgi/common/OptionsFile.h"

class LHttpServer
{
	struct LHttpServerPriv *d;

public:
	struct Request
	{
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
};

#endif