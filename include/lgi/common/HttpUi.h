#ifndef _HTTP_UI_H_
#define _HTTP_UI_H_

#include "LOptionsFile.h"

class LHttpCallback
{
public:
	virtual ~LHttpCallback() {}

	char *FormDecode(char *s);
	char *HtmlEncode(char *s);
	bool ParseHtmlWithDom(LVariant &Out, GDom *Dom, char *Html);

	virtual int OnRequest(char *Action, char *Uri, char *Headers, char *Body, LVariant &Out) = 0;
};

class LHttpServer
{
	struct LHttpServerPriv *d;

public:
	LHttpServer(LHttpCallback *cb, int port);
	~LHttpServer();
};

#endif