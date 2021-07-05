/*	Instantiation:

	LAutoPtr<LFont> f(new LEmojiFont());
	if (f && f->Create())
		LFontSystem::Inst()->AddFont(f);
*/
#ifndef _EMOJI_FONT_H_
#define _EMOJI_FONT_H_

#include "lgi/common/Font.h"

class LEmojiFont : public LFont
{
	struct LEmojiFontPriv *priv;

	void _Measure(int &x, int &y, OsChar *Str, int Len) override;
	int _CharAt(int x, OsChar *Str, int Len, LgiPxToIndexType Type) override;
	void _Draw(LSurface *pDC, int x, int y, OsChar *Str, int Len, LRect *r, GColour &fore) override;

public:
	LEmojiFont();
	~LEmojiFont();

	bool Create(const char *Face = NULL, LCss::Len Size = LCss::LenInherit, LSurface *pSurface = NULL) override;
	int GetHeight() override;
};

#endif