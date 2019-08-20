/// \file
/// \author Matthew Allen
/// \created 3/2/2011
#ifndef _GCOLOUR_H_
#define _GCOLOUR_H_

#include "GColourSpace.h"

// System Colours
enum LSystemColour
{
	L_TRANSPARENT,

	/// Black
	L_BLACK,
	/// Dark gray
	L_DKGREY,
	/// Medium gray
	L_MIDGREY,
	/// Light gray
	L_LTGREY,
	/// White
	L_WHITE,

	/// 3d dark shadow
	L_SHADOW,
	/// 3d light shadow
	L_LOW,
	/// Flat colour for dialogs, windows and buttons
	L_MED,
	/// 3d dark highlight
	L_HIGH,
	/// 3d light highlight
	L_LIGHT,

	/// Dialog colour
	L_DIALOG,
	/// Workspace area
	L_WORKSPACE,
	/// Default text colour
	L_TEXT,
	/// Selection background colour when in focus
	L_FOCUS_SEL_BACK,
	/// Selection foreground colour when in focus
	L_FOCUS_SEL_FORE,

	L_ACTIVE_TITLE,
	L_ACTIVE_TITLE_TEXT,
	L_INACTIVE_TITLE,
	L_INACTIVE_TITLE_TEXT,

	L_MENU_BACKGROUND,
	L_MENU_TEXT,

	/// Selection background colour when not in focus
	L_NON_FOCUS_SEL_BACK,
	/// Selection forground colour when not in focus
	L_NON_FOCUS_SEL_FORE,

	L_DEBUG_CURRENT_LINE,
	L_TOOL_TIP, // Rgb24(255, 255, 231)

	L_MAXIMUM
};


/// A colour definition
class LgiClass GColour
{
protected:
	class GPalette *pal;
	union {
		uint32_t flat;
		uint8_t  index;
		System32BitPixel rgb;
		GHls32 hls;
	};
	GColourSpace space;

	int HlsValue(double fN1, double fN2, double fHue) const;

public:
	static const GColour Black;
	static const GColour White;
	static const GColour Red;
	static const GColour Green;
	static const GColour Blue;

	/// Call if the system defined colours changed.
	static void OnChange();
	/// Gets a colour from the LGI config (not available in LGI_STATIC)
	static bool GetConfigColour(const char *Tag, GColour &c);

	/// Transparent
	GColour();
	/// System colour
	GColour(LSystemColour sc);
	/// Indexed colour
	GColour(uint8_t idx8, GPalette *palette);
	/// True colour
	GColour(int r, int g, int b, int a = 255);
	/// Conversion from COLOUR
	GColour(uint32_t c, int bits, GPalette *palette = NULL);
	#ifdef __GTK_H__
	GColour(Gtk::GdkRGBA c);
	#endif
	GColourSpace GetColourSpace();
	bool SetColourSpace(GColourSpace cs);
	
	bool operator ==(const GColour &c)
	{
		return c32() == c.c32();
	}
	bool operator !=(const GColour &c)
	{
		return c32() != c.c32();
	}
	
	bool IsValid();
	void Empty();
	bool IsTransparent();
	/// Sets the colour to a rgb(a) value
	void Rgb(int r, int g, int b, int a = 255);
	/// Sets the colour
	void Set(uint32_t c, int bits, GPalette *palette = NULL);
	uint32_t Get(int bits);
	
	/// Gets/sets the red component (0-255)
	uint8_t r() const;
	void r(uint8_t i);
	/// Gets/sets the green component (0-255)
	uint8_t g() const;
	void g(uint8_t i);
	/// Gets/sets the blue component (0-255)
	uint8_t b() const;
	void b(uint8_t i);
	/// Gets/sets the alpha component (0-255)
	uint8_t a() const;
	void a(uint8_t i);
	
	// Gets the indexed colour
	uint8_t c8() const;
	// Sets indexed colour
	void c8(uint8_t c, GPalette *p);
	// Get as 24 bit colour
	uint32_t c24() const;
	/// Set 24 bit colour
	void c24(uint32_t c);
	/// Get 32 bit colour
	uint32_t c32() const;
	/// Set 32 bit colour
	void c32(uint32_t c);
	/// Mixes 'Tint' with the current colour and returns it 
	/// without modifying the object.
	GColour Mix(GColour Tint, float RatioOfTint = 0.5) const;
	// Hue Lum Sat methods
	
	/// Returns the hue value (0-359)
	uint32_t GetH();
	/// Returns whether the hue is value or not
	bool HueIsUndefined();
	/// Returns the luminance
	uint32_t GetL();
	/// Returns the saturation
	uint32_t GetS();	
	/// Converts the colour space to HLS
	bool ToHLS();	
	/// Sets the colour to a HLS value
	void SetHLS
	(
		/// (0-359)
		uint16_t h,
		/// (0-255)
		uint8_t l,
		/// (0-255)
		uint8_t s
	);
	// Convert HLS to RGB
	void ToRGB();
	int GetGray(int BitDepth = 8);
	uint32_t GetNative();
	
	// String IO
	char *GetStr();
	bool SetStr(const char *Str);
	
	#ifdef BEOS
	operator rgb_color()
	{
		rgb_color c;
		c.red = r();
		c.green = g();
		c.blue = b();
		return c;
	}
	#endif
};


// Old system colour mappers
#define LC_BLACK				LgiColour(L_BLACK)
#define LC_DKGREY				LgiColour(L_DKGREY)
#define LC_MIDGREY				LgiColour(L_MIDGREY)
#define LC_LTGREY				LgiColour(L_LTGREY)
#define LC_WHITE				LgiColour(L_WHITE)
#define LC_SHADOW				LgiColour(L_SHADOW)
#define LC_LOW					LgiColour(L_LOW)
#define LC_MED					LgiColour(L_MED)
#define LC_HIGH					LgiColour(L_HIGH)
#define LC_LIGHT				LgiColour(L_LIGHT)
#define LC_DIALOG				LgiColour(L_DIALOG)
#define LC_WORKSPACE			LgiColour(L_WORKSPACE)
#define LC_TEXT					LgiColour(L_TEXT)
#define LC_FOCUS_SEL_BACK		LgiColour(L_FOCUS_SEL_BACK)
#define LC_FOCUS_SEL_FORE		LgiColour(L_FOCUS_SEL_FORE)
#define LC_ACTIVE_TITLE			LgiColour(L_ACTIVE_TITLE)
#define LC_ACTIVE_TITLE_TEXT	LgiColour(L_ACTIVE_TITLE_TEXT)
#define LC_INACTIVE_TITLE		LgiColour(L_INACTIVE_TITLE)
#define LC_INACTIVE_TITLE_TEXT	LgiColour(L_INACTIVE_TITLE_TEXT)
#define LC_MENU_BACKGROUND		LgiColour(L_MENU_BACKGROUND)
#define LC_MENU_TEXT			LgiColour(L_MENU_TEXT)
#define LC_NON_FOCUS_SEL_BACK	LgiColour(L_NON_FOCUS_SEL_BACK)
#define LC_NON_FOCUS_SEL_FORE	LgiColour(L_NON_FOCUS_SEL_FORE)
#define LC_DEBUG_CURRENT_LINE	LgiColour(L_DEBUG_CURRENT_LINE)
#define LC_TOOL_TIP				LgiColour(L_TOOL_TIP)

// Get a system colour
LgiExtern GColour LColour(LSystemColour Colour);

/// \brief Returns a 24bit representation of a system colour.
///
/// You can use the macros R24(), G24() and B24() to extract the colour
/// components
LgiFunc COLOUR DEPRECATED(LgiColour(
	/// The system colour to return.
	/// \sa The define L_BLACK and those that follow in GColour.h
	LSystemColour Colour
));

#endif
