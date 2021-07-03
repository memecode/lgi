#ifndef __HTTP_TOOLS__
#define __HTTP_TOOLS__

#include "INet.h"

extern LXmlTag *ExtractForms(char *Html, GStream *Log);
extern void XmlToStream(GStream *s, LXmlTag *x, char *Css = 0);
extern LXmlTag *GetFormField(LXmlTag *Form, char *Field);
extern char *HtmlTidy(char *Html);
extern LSurface *GetHttpImage(char *Uri);
extern void StrFormEncode(GStream &p, char *s, bool InValue);

struct WebPage
{
	char *Html;
	char *Script;
	char *Charset;
	LXmlTag *Parsed;

	WebPage(char *Page, GStream *Log = 0);
	~WebPage();
	
	LXmlTag *GetRoot(GStream *Log = 0);
	char *GetFormValue(char *field);
	char *GetCharSet();
};

class FormValue
{
public:
	GAutoString Field;
	GAutoString Value;
};

struct FormPost
{
	LXmlTag *Form;
	GArray<FormValue> Values;
	
	FormPost(LXmlTag *f);
	
	char *GetActionUri();
	char *EncodeFields(GStream *Debug = 0, char *RealFields = 0, bool EncodePlus = false);
	FormValue *Get(char *Field, bool Create = true);
	bool Set(char *field, char *value, GStream *Log, bool AllowCreate);
	LXmlTag *GetField(char *n);
};

class CookieJar : public LHashTbl<StrKey<char,false>,char*>
{
public:
	~CookieJar() { Empty(); }

	void Empty();
	void Set(char *Headers);
	void Set(char *Cookie, char *Value);
	char *Get();
};

class HttpTools
{
	void DumpView(GViewI *v, char *p);

protected:
	GViewI *Wnd;

public:
	HttpTools();
	~HttpTools();

	void SetWnd(GViewI *i) { Wnd = i; }
	char *Fetch(char *uri, GStream *Log, GViewI *Dump, CookieJar *Cookies = 0);
	char *Post(char *uri, char *headers, char *body, GStream *Log = 0, GViewI *Dump = 0);
};

#endif