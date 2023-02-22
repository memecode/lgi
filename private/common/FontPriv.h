#ifndef _GFONT_PRIV_H_
#define _GFONT_PRIV_H_

#if defined USE_CORETEXT
#include <ApplicationServices/ApplicationServices.h>
typedef ATSUTextMeasurement OsTextSize;
#else
typedef double OsTextSize;
#endif

class LTypeFacePrivate
{
public:
	// Type
	LString _Face;			// type face
	LString _CodePage;
	LCss::Len _Size;		// size
	int _Weight;
	int _Quality;
	bool _Italic;
	bool _Underline;

	// Output
	LColour _Fore;
	LColour _Back;
	LColour WhiteSpace; // Can be empty (if so it's calculated)
	int _TabSize;
	bool _Transparent;
	bool _SubGlyphs;

	// Backs
	bool IsSymbol;

	// Props
	double _Ascent, _Descent, _Leading;

	LTypeFacePrivate()
	{
		IsSymbol = false;
		_Ascent = _Descent = _Leading = 0.0;
		_Size = LCss::Len(LCss::LenPt, 8.0f);
		_Weight = FW_NORMAL;
		_Italic = false;
		_Underline = false;
		_CodePage = "utf-8";
		_Fore.Rgb(0, 0, 0);
		_Back.Rgb(255, 255, 255);
		_TabSize = 32; // px
		_Transparent = false;
		_Quality = DEFAULT_QUALITY;
		_SubGlyphs = LFontSystem::Inst()->GetDefaultGlyphSub();
	}
};

class LFontPrivate
{
public:
	#ifdef WINDOWS
	static LAutoPtr<LLibrary> Gdi32;
	#endif

	// Data
	OsFont			hFont = NULL;
	int				Height = 0;
	bool			Dirty = true;
	char			*Cp = NULL;
	LSurface		*pSurface = NULL;
	bool			OwnerUnderline = false;
	bool			WarnOnDelete = false;

	// Glyph substitution
	uchar			*GlyphMap = NULL;
	static class GlyphCache *Cache;

	#ifdef USE_CORETEXT
	CFDictionaryRef Attributes = NULL;
	#endif
	#ifdef __GTK_H__
	Gtk::PangoContext *PangoCtx = NULL;
	#endif

	LFontPrivate()
	{
	}
	
	~LFontPrivate()
	{
		#ifdef __GTK_H__
		if (PangoCtx)
			g_object_unref(PangoCtx);
		#elif defined(MAC)
		if (Attributes)
			CFRelease(Attributes);
		#endif
	}
};

#endif
