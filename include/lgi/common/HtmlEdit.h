#ifndef _RICH_EDIT_H_
#define _RICH_EDIT_H_

#include "GHtml.h"
#include "LTree.h"

class GHtmlEdit : public LDocView
{
	class GHtmlEditPriv *d;
	friend class HtmlEdit;
	void SetIgnorePulse(bool b);

public:
	GHtmlEdit(int Id = -1);
	~GHtmlEdit();

	const char *GetClass() { return "GHtmlEdit"; }
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