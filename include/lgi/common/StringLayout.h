#ifndef _GDISPLAY_STRING_LAYOUT_H_
#define _GDISPLAY_STRING_LAYOUT_H_

#include "lgi/common/DisplayString.h"
#include "lgi/common/Css.h"
#include "lgi/common/FontCache.h"

/// Section of text with the same style.
struct LLayoutRun : public GCss
{
	GString Text;

	LLayoutRun(GCss *Style)
	{
		if (Style)
		{
			GCss *This = this;
			*This = *Style;
		}
	}
};

/// Display string of a certain style.
struct LLayoutString : public LDisplayString
{
	int Fx, y, Line;
	GColour Fore, Back;
	ssize_t Offset;
	LLayoutRun *Src;
	
	LLayoutString(LFont *f,
				const char *s,
				ssize_t l = -1,
				GSurface *pdc = 0) :
		LDisplayString(f, s, l, pdc)
	{
		Offset = 0;
		Fx = y = 0;
		Layout();
		Src = NULL;
	}

	LLayoutString(LLayoutString *Ls,
				const char *s,
				ssize_t l = -1) :
		LDisplayString(Ls->GetFont(), s, l)
	{
		Fx = Ls->Fx;
		y = Ls->y;
		Fore = Ls->Fore;
		Back = Ls->Back;
		Line = Ls->Line;
		Offset = Ls->Offset;
		Src = Ls->Src;
	}

	void Set(int LineIdx, int FixX, int YPx, LLayoutRun *Lr, ssize_t Start);
};

/// This class lays out a block of text according to the given styles. It
/// builds upon the LDisplayString class to render sections of text.
class LStringLayout
{
protected:
	GFontCache *FontCache;

	// Min and max bounds (in pixels)
	LPoint Min, Max;
	int MinLines;
	
	// Setting
	bool Wrap;
	bool AmpersandToUnderline;

	// Array of display strings...
	GArray<LLayoutRun*> Text;
	GArray<LDisplayString*> Strs;
	LRect Bounds;

public:
	bool Debug;

	LStringLayout(GFontCache *fc);	
	~LStringLayout();

	void Empty();

	bool GetWrap() { return Wrap; }
	void SetWrap(bool b) { Wrap = b; }
	LPoint GetMin() { return Min; }
	LPoint GetMax() { return Max; }
	GArray<LDisplayString*> *GetStrs() { return &Strs; }
	LRect GetBounds() { return Bounds; }

	/// Adds a run of text with the same style
	bool Add(const char *Str, GCss *Style);
	uint32_t NextChar(char *s);
	uint32_t PrevChar(char *s);
	LFont *GetBaseFont();
	void SetBaseFont(LFont *f);
	LFont *GetFont();

	// Pre-layout min/max calculation
	void DoPreLayout(int32 &MinX, int32 &MaxX);

	// Creates the layout of from the runs in 'Text'
	bool DoLayout(int Width, int MinYSize = 0, bool Debug = false);
	
	/// Paints the laid out strings at 'pt'.
	void Paint(	GSurface *pDC,
				LPoint pt,
				GColour Back,
				LRect &rc,
				bool Enabled,
				bool Focused);
};

#endif
