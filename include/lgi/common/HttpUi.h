#ifndef _HTTP_UI_H_
#define _HTTP_UI_H_

#include "lgi/common/OptionsFile.h"

class LHttpCallback
{
public:
	virtual ~LHttpCallback() {}

	char *FormDecode(char *s);
	char *HtmlEncode(const char *s);
	bool ParseHtmlWithDom(LVariant &Out, LDom *Dom, const char *Html);

	virtual int OnRequest(	// [In] Request vars:
							LString ReqAction,
							LString ReqUri,
							LString ReqHeaders,
							LString ReqBody,
							// [Out] Response vars:
							LStream *RespHeaders,
							LStream *RespBody) = 0;
};

class LHttpServer
{
	struct LHttpServerPriv *d;

public:
	LHttpServer(LHttpCallback *cb, int port);
	~LHttpServer();
};

#endif