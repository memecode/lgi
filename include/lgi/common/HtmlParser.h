#pragma once

#include "lgi/common/DocView.h"
#include "lgi/common/HtmlCommon.h"

class LHtmlParser
{
	LStringPipe SourceData;
	const char *CurrentSrc;

protected:
	LDocView *View;
	LAutoString Source;
	LArray<LHtmlElement*> OpenTags;
	LAutoString DocCharSet;
	bool DocAndCsTheSame;

	void CloseTag(LHtmlElement *t)
	{
		if (!t)
			return;

		OpenTags.Delete(t);
	}

	LHtmlElement *GetOpenTag(const char *Tag);
	void _TraceOpenTags();
	char *ParseHtml(LHtmlElement *Elem, char *Doc, int Depth, bool InPreTag = false, bool *BackOut = NULL);	
	char16 *DecodeEntities(const char *s, ssize_t len);

public:
	LHtmlParser(LDocView *view = NULL)
	{
		View = view;
	}

	// Props
	LDocView *GetView() { return View; }
	void SetView(LDocView *v) { View = v; }

	// Main entry point
	bool Parse(LHtmlElement *Root, const char *Doc);
	
	// Tool methods
	LHtmlElemInfo *GetTagInfo(const char *Tag);
	static bool ParseColour(const char *s, LCss::ColorDef &c);
	static bool Is8Bit(char *s);
	char *ParsePropValue(char *s, char16 *&Value);
	char *ParseName(char *s, LAutoString &Name);
	char *ParseName(char *s, char **Name);
	char *ParsePropList(char *s, LHtmlElement *Obj, bool &Closed);
	void SkipNonDisplay(char *&s);
	char *NextTag(char *s);
	char16 *CleanText(const char *s, ssize_t Len, bool ConversionAllowed, bool KeepWhiteSpace);
	
	// Virtual callbacks
	virtual LHtmlElement *CreateElement(LHtmlElement *Parent) = 0;
	virtual bool EvaluateCondition(const char *Cond) { return false; }
};
