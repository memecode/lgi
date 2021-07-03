#ifndef _GTAG_H
#define _GTAG_H

#include "LVariant.h"
#include "GMap.h"
#include "LXmlTree.h"

class GNamedVariant : public LVariant, public GObj
{
public:
	GNamedVariant(char *s = 0)
	{
		if (s) Name(s);
	}
};

class GTag : public List<GNamedVariant>, public GDom
{
protected:
	char *Element;
	bool ObscurePasswords;

	LVariantType TypeOf(char *Name);

public:
	GTag(char *e);
	~GTag();

	bool IsNumber(char *s);
	bool operator ==(char *s);
	GTag &operator =(GTag &t);
	GNamedVariant *GetNamed(char *Name);
	void Empty();
	bool Read(LXmlTag *t);
	void Write(GFile &f);
	bool GetVariant(const char *Name, LVariant &Value, char *Array = 0);
	bool SetVariant(const char *Name, LVariant &Value, char *Array = 0);
	void SerializeUI(GView *Dlg, GMap<char*,int> &Fields, bool To);
};

#endif
