#ifndef _GFONT_PRIV_H_
#define _GFONT_PRIV_H_

#if defined USE_CORETEXT
#include <ApplicationServices/ApplicationServices.h>
typedef ATSUTextMeasurement OsTextSize;
#else
typedef double OsTextSize;
#endif

class GFontPrivate
{
public:
	#ifdef WINDOWS
	static GAutoPtr<GLibrary> Gdi32;
	#endif

	// Data
	OsFont			hFont;
	int				Height;
	bool			Dirty;
	char			*Cp;
	GSurface		*pSurface;
	bool			OwnerUnderline;

	// Glyph substitution
	uchar			*GlyphMap;
	static class GlyphCache *Cache;

	#ifdef BEOS
	// Beos glyph sizes
	uint16			CharX[128]; // Widths of ascii
	LHashTbl<IntKey<int>,int> UnicodeX; // Widths of any other characters
	#endif

	#ifdef USE_CORETEXT
	CFDictionaryRef Attributes;
	#endif

	#ifdef __GTK_H__
	Gtk::PangoContext *PangoCtx;
	#endif

	GFontPrivate()
	{
		hFont = 0;
		pSurface = NULL;
		#if defined WIN32
		OwnerUnderline = false;
		#endif
		#ifdef __GTK_H__
		PangoCtx = NULL;
		#elif defined(MAC)
		Attributes = NULL;
		#endif

		GlyphMap = 0;
		Dirty = true;
		Height = 0;
		Cp = 0;
	}
	
	~GFontPrivate()
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
