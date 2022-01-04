#ifndef _GHTMLSTATIC_H_
#define _GHTMLSTATIC_H_

#include "lgi/common/Css.h"
#include "lgi/common/HashTable.h"

extern char16 LHtmlListItem[];

#define SkipWhiteSpace(s)			while (*s && IsWhiteSpace(*s)) s++;

enum HtmlTag
{
	CONTENT,
	CONDITIONAL,
	ROOT,
	TAG_UNKNOWN,
	TAG_HTML,
	TAG_HEAD,
	TAG_BODY,
	TAG_B,
	TAG_I,
	TAG_U,
	TAG_P,
	TAG_BR,
	TAG_UL,
	TAG_OL,
	TAG_LI,
	TAG_FONT,
	TAG_A,
	TAG_TABLE,
		TAG_TR,
		TAG_TD,
		TAG_TH,
	TAG_IMG,
	TAG_DIV,
	TAG_SPAN,
	TAG_CENTER,
	TAG_META,
	TAG_TBODY,
	TAG_STYLE,
	TAG_SCRIPT,
	TAG_STRONG,
	TAG_BLOCKQUOTE,
	TAG_PRE,
	TAG_H1,
	TAG_H2,
	TAG_H3,
	TAG_H4,
	TAG_H5,
	TAG_H6,
	TAG_HR,
	TAG_IFRAME,
	TAG_LINK,
	TAG_BIG,
	TAG_INPUT,
	TAG_SELECT,
	TAG_LABEL,
	TAG_FORM,
	TAG_NOSCRIPT,
	TAG_SUP,
	TAG_SUB,
	TAG_EM,
	TAG_TITLE,
	TAG_LAST
};

/// Common element info
struct LHtmlElemInfo
{
public:
	enum InfoFlags
	{
		TI_NONE			= 0x00,
		TI_NEVER_CLOSES	= 0x01,
		TI_NO_TEXT		= 0x02,
		TI_BLOCK		= 0x04,
		TI_TABLE		= 0x08,
		TI_SINGLETON	= 0x10,
	};

	HtmlTag Id;
	const char *Tag;
	bool Reattach;
	int Flags;

	bool NeverCloses()	{ return TestFlag(Flags, TI_NEVER_CLOSES); }
	bool NoText()		{ return TestFlag(Flags, TI_NO_TEXT); }
	bool Block()		{ return TestFlag(Flags, TI_BLOCK); }
};

/// Common data for HTML related classes
class LHtmlStatic
{
	friend class LHtmlStaticInst;
	LHtmlElemInfo *UnknownElement;
	LHashTbl<ConstStrKey<char,false>,LHtmlElemInfo*> TagMap;
	LHashTbl<IntKey<int>,LHtmlElemInfo*> TagIdMap;

public:
	static LHtmlStatic *Inst;

	int Refs;
	LHashTbl<StrKey<char16,true>,uint32_t>				VarMap;
	LHashTbl<ConstStrKey<char,false>,LCss::PropType>	StyleMap;
	LHashTbl<ConstStrKey<char,false>,int>				ColourMap;

	LHtmlStatic();
	~LHtmlStatic();

	LHtmlElemInfo *GetTagInfo(const char *Tag);
	LHtmlElemInfo *GetTagInfo(HtmlTag TagId);
	void OnSystemColourChange();
};

/// Static data setup/pulldown
class LHtmlStaticInst
{
public:
	LHtmlStatic *Static;

	LHtmlStaticInst()
	{
		if (!LHtmlStatic::Inst)
		{
			LHtmlStatic::Inst = new LHtmlStatic;
		}
		if (LHtmlStatic::Inst)
		{
			LHtmlStatic::Inst->Refs++;
		}
		Static = LHtmlStatic::Inst;
	}

	~LHtmlStaticInst()
	{
		if (LHtmlStatic::Inst)
		{
			LHtmlStatic::Inst->Refs--;
			if (LHtmlStatic::Inst->Refs == 0)
			{
				DeleteObj(LHtmlStatic::Inst);
			}
		}
	}
};

class GCssStyle : public LDom
{
public:
	LCss *Css;

	GCssStyle()
	{
		Css = NULL;
	}

	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
};

/// Common base class for a HTML element
class LHtmlElement : public LDom, public LCss
{
	friend class LHtmlParser;
	friend class HtmlEdit;

protected:
	LAutoWString Txt;
	uint8_t WasClosed : 1;
	GCssStyle StyleDom;

public:
	HtmlTag TagId;
	LAutoString Tag;
	LHtmlElemInfo *Info;
	LAutoString Condition;
	
	LHtmlElement *Parent;
	LArray<LHtmlElement*> Children;
	
	LHtmlElement(LHtmlElement *parent);
	~LHtmlElement();
	
	// Methods
	char16 *GetText() { return Txt; }
	
	// Heirarchy
	bool Attach(LHtmlElement *Child, ssize_t Idx = -1);
	void Detach();
	bool HasChild(LHtmlElement *Child);

	// Virtuals
	virtual bool Get(const char *attr, const char *&val) { return false; }
	virtual void Set(const char *attr, const char *val) {}
	virtual void SetStyle() {}
	virtual LAutoString DescribeElement() { return LAutoString(); }
	virtual void OnStyleChange(const char *name) {}

	// Helper
	void Set(const char *attr, const char16 *val)
	{
		LAutoString utf8(WideToUtf8(val));
		Set(attr, utf8);
	}

	#ifdef _DEBUG
	bool Debug()
	{
		const char *sDebug = NULL;
		if (Get("debug", sDebug))
			return atoi(sDebug) != 0;
		return false;
	}
	#endif
};

#endif