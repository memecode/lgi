#ifndef _RICH_EDIT_H_
#define _RICH_EDIT_H_

#include "LHtml.h"
#include "LTree.h"

class LHtmlEdit : public LDocView
{
	class LHtmlEditPriv *d;
	friend class HtmlEdit;
	void SetIgnorePulse(bool b);

public:
	LHtmlEdit(int Id = -1);
	~LHtmlEdit();

	const char *GetClass() { return "LHtmlEdit"; }
	const char *GetMimeType() { return "text/html"; }
	void OnPaint(LSurface *pDC);
	void OnCreate();
	void OnPosChange();
	void OnPulse();
	int OnNotify(LViewI * c, int f);

	char *Name();
	bool Name(const char *s);
	bool Sunken();
	void Sunken(bool i);
	
	// Debugging
	void DumpNodes(LTree *Out);
};

#endif