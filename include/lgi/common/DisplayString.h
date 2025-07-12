#ifndef _GDISPLAY_STRING_H_
#define _GDISPLAY_STRING_H_

#include "lgi/common/Font.h"

#if defined(LGI_SDL)
#elif defined(LINUX)
	namespace Pango
	{
	#include "pango/pango.h"
	#include "pango/pangocairo.h"
	}
#endif

#if !defined(_MSC_VER) || defined(__GTK_H__)
#define LGI_DSP_STR_CACHE		1
#endif

/// \brief Cache for text measuring, glyph substitution and painting
///
/// To paint text onto the screen several stages need to be implemented to
/// properly support unicode on multiple platforms. This class addresses all
/// of those needs and then allows you to cache the results to reduce text
/// related workload.
///
/// The first stage is converting text into the native format for the 
/// OS's API. This usually involved converting the text to wide characters for
/// Linux or Windows, or Utf-8 for Haiku. Then the text is converted into runs of
/// characters that can be rendered in the same font. If glyph substitution is
/// required to render the characters a separate run is used with a different
/// font ID. Finally you can measure or paint these runs of text. Also tab characters
/// are expanded to the current tab size setting.
class LgiClass LDisplayString
{
protected:
	LSurface *pDC = NULL;
	LFont *Font = NULL;
	uint32_t x = 0, y = 0;
	int xf = 0, yf = 0;
	int DrawOffsetF = 0;
	
	// Flags
	uint8_t LaidOut : 1;
	uint8_t AppendDots : 1;
	uint8_t VisibleTab : 1;

	/* String data:
					Str			Wide
		Windows:	utf-16		utf-16
	 	MacCocoa:	utf-16		utf-32
	 	Gtk3:		utf-8		utf-32
	 	Haiku:		utf-8		utf-32
	*/
	OsChar *Str = NULL;
	ssize_t StrWords = 0;
	#if LGI_DSP_STR_CACHE
	wchar_t *Wide = NULL;
	ssize_t WideWords = 0;
	#endif
	
	#if defined(LGI_SDL)
	
	LAutoPtr<LMemDC> Img;
	
	#elif defined(__GTK_H__)

	struct LDisplayStringPriv *d;
	friend struct LDisplayStringPriv;
	
	#elif defined(MAC)
	
	#if USE_CORETEXT
		CTLineRef Hnd;
		CFAttributedStringRef AttrStr;
		void CreateAttrStr();
	#else
		ATSUTextLayout Hnd;
		ATSUTextMeasurement fAscent;
		ATSUTextMeasurement fDescent;
	#endif
	
	#elif defined(WINNATIVE) || defined(HAIKU)
	
	class CharInfo
	{
	public:
		OsChar *Str = NULL;
		int Len = 0;
		int X = 0;
		uint8_t FontId = 0;
		int8_t SizeDelta = 0;
		uint8_t Missing = false;

		uint32_t First()
		{
			auto s = (const uint16*)Str;
			ssize_t l = Len * sizeof(*Str);
			return LgiUtf16To32(s, l);
		}
	};
	LArray<CharInfo> Info;

	#endif

	void Layout(bool Debug = false);
	void DrawWhiteSpace(LSurface *pDC, char Ch, LRect &r);

public:
	// Turn on debug logging...
	bool _debug = false;

	/// Constructor
	LDisplayString
	(
		/// The base font. Must not be destroyed during the lifetime of this object.
		LFont *f,
		/// Utf-8 input string
		const char *s,
		/// Number of bytes in the input string or -1 for NULL terminated.
		ssize_t l = -1,
		LSurface *pdc = 0
	);
	/// Constructor
	LDisplayString
	(
		/// The base font. Must not be destroyed during the lifetime of this object.
		LFont *f,
		/// A wide character input string
		const char16 *s,
		/// The number of characters in the input string (NOT the number of bytes) or -1 for NULL terminated
		ssize_t l = -1,
		LSurface *pdc = 0
	);
	#ifdef _MSC_VER
	/// Constructor
	LDisplayString
	(
		/// The base font. Must not be destroyed during the lifetime of this object.
		LFont *f,
		/// A wide character input string
		const uint32_t *s,
		/// The number of characters in the input string (NOT the number of bytes) or -1 for NULL terminated
		ssize_t l = -1,
		LSurface *pdc = 0
	);
	#endif
	virtual ~LDisplayString();
	
	LDisplayString &operator=(const LDisplayString &s)
	{
		LAssert(0);
		return *this;
	}
	
	/// \returns the draw offset used to calculate tab spacing (in Px)
	int GetDrawOffset();
	/// Sets the draw offset used to calculate tab spacing (in Px)
	void SetDrawOffset(int Px);

	/// Returns the ShowVisibleTab setting.
	/// Treats Unicode-2192 (left arrow) as a tab char
	bool ShowVisibleTab();
	/// Sets the ShowVisibleTab setting.
	/// Treats Unicode-2192 (left arrow) as a tab char
	void ShowVisibleTab(bool i);

	/// \returns the font used to create the layout
	LFont *GetFont() { return Font; };

	/// Fits string into 'width' pixels, truncating with '...' if it's not going to fit
	void TruncateWithDots
	(
		/// The number of pixels the string has to fit
		int Width
	);
	/// Returns true if the string is trucated
	bool IsTruncated();

	/// Returns the words (not chars) in the OsChar string
	ssize_t Length();
	
	/// Returns the pointer to the native string
	operator const char16*()
	{
		#if LGI_DSP_STR_CACHE
		return Wide;
		#else
		return Str;
		#endif
	}
	
	/// \returns the number of words in the wide string
	ssize_t WideLength()
	{
		#if LGI_DSP_STR_CACHE
		return WideWords;
		#else
		return StrWords;
		#endif
	}

	// API that use full pixel sizes:

		/// Returns the width of the whole string
		int X();
		/// Returns the height of the whole string
		int Y();
		/// Returns the width and height of the whole string
		LPoint Size();
		/// Returns the number of characters that fit in 'x' pixels.
		ssize_t CharAt(int x, LPxToIndexType Type = LgiTruncate);

		/// Draws the string onto a device surface
		///
		/// Windows note: drawing text on a 32bit memory context will
		/// not set the alpha correctly (it'll be 0). You can call 
		/// LSurface::MakeOpaque afterwards to fix that.
		void Draw
		(
			/// The output device
			LSurface *pDC,
			/// The x coordinate of the top left corner of the output box
			int x,
			/// The y coordinate of the top left corner of the output box
			int y,
			/// An optional clipping rectangle. If the font is not transparent this rectangle will be filled with the background colour.
			LRect *r = NULL,
			/// Turn on debug logging
			bool Debug = false
		);

		/// Draws the string onto a device surface
		void DrawCenter
		(
			/// The output device
			LSurface *pDC,
			/// An rectangle to center in. If the font is not transparent this rectangle will be filled with the background colour.
			LRect *r
		)
		{
			if (r)
				Draw(pDC, r->x1 + ((r->X() - X()) >> 1), r->y1 + ((r->Y() - Y()) >> 1), r);
		}

	// API that uses fractional pixel sizes.
		
		enum
		{
			#if defined LGI_SDL
				FShift = 6,
				FScale = 1 << FShift,
			#elif defined __GTK_H__
				/// This is the value for 1 pixel.
				FScale = PANGO_SCALE,
				/// This is bitwise shift to convert between integer and fractional
				FShift = 10
			#elif defined MAC
				/// This is the value for 1 pixel.
				FScale = 0x10000,
				/// This is bitwise shift to convert between integer and fractional
				FShift = 16
			#else
				/// This is the value for 1 pixel.
				FScale = 1,
				/// This is bitwise shift to convert between integer and fractional
				FShift = 0
			#endif
		};
		
		/// \returns the draw offset used to calculate tab spacing (in fractional units)
		int GetDrawOffsetF();
		/// Sets the draw offset used to calculate tab spacing (in fractional units)
		void SetDrawOffsetF(int Fpx);	
		/// \returns the fractional width of the string
		int FX();
		/// \returns the fractional height of the string
		int FY();
		/// \returns both the fractional width and height of the string
		LPoint FSize();
		/// Draws the string using fractional co-ordinates.
		void FDraw
		(
			/// The output device
			LSurface *pDC,
			/// The fractional x coordinate of the top left corner of the output box
			int fx,
			/// The fractional y coordinate of the top left corner of the output box
			int fy,
			/// [Optional] fractional clipping rectangle. If the font is not transparent 
			/// this rectangle will be filled with the background colour.
			LRect *frc = NULL,
			/// [Optional] print debug info
			bool Debug = false
		);
};

#endif
