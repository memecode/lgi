#ifndef _HTTP_UI_H_
#define _HTTP_UI_H_

#include "LOptionsFile.h"

class LHttpCallback
{
public:
	virtual ~LHttpCallback() {}

	char *FormDecode(const char *s);
	char *HtmlEncode(const char *s);
	bool ParseHtmlWithDom(LVariant &Out, LDom *Dom, const char *Html);

	virtual int OnRequest(const char *Action, constchar *Uri, constchar *Headers, constchar *Body, LVariant &Out) = 0;
};

class LHttpServer
{
	struct LHttpServerPriv *d;

public:
	LHttpServer(LHttpCallback *cb, int port);
	~LHttpServer();
};

#endif