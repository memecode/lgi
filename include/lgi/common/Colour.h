/// \file
/// \author Matthew Allen
/// \created 3/2/2011
#pragma once

#include "lgi/common/ColourSpace.h"

#define _SystemColour() \
	_(TRANSPARENT) \
	_(BLACK) \
	_(DKGREY) \
	_(MIDGREY) \
	_(LTGREY) \
	_(WHITE) \
	_(SHADOW) /* 3d dark shadow */ \
	_(LOW) /* 3d light shadow */ \
	_(MED) /* Flat colour for dialogs, windows and buttons */ \
	_(HIGH) /* 3d dark highlight */ \
	_(LIGHT) /* 3d light highlight */ \
	_(DIALOG) /* Dialog colour */ \
	_(WORKSPACE) /* Workspace area */ \
	_(TEXT) /* Default text colour */ \
	_(FOCUS_SEL_BACK) /* Selection background colour when in focus */ \
	_(FOCUS_SEL_FORE) /* Selection foreground colour when in focus */ \
	_(ACTIVE_TITLE) \
	_(ACTIVE_TITLE_TEXT) \
	_(INACTIVE_TITLE) \
	_(INACTIVE_TITLE_TEXT) \
	_(MENU_BACKGROUND) \
	_(MENU_TEXT) \
	_(NON_FOCUS_SEL_BACK) /* Selection background colour when not in focus */ \
	_(NON_FOCUS_SEL_FORE) /* Selection foreground colour when not in focus */ \
	_(DEBUG_CURRENT_LINE) \
	_(TOOL_TIP) \
	_(MAXIMUM)

// System Colours
enum LSystemColour
{
	#define _(n) L_##n,
	_SystemColour()
	#undef _
};

/// A colour definition
class LgiClass LColour
{
protected:
	class GPalette *pal;
	union {
		uint32_t flat;
		uint8_t  index;
		System32BitPixel rgb;
		LHls32 hls;
	};
	LColourSpace space;

	int HlsValue(double fN1, double fN2, double fHue) const;

public:
	static const LColour Black;
	static const LColour White;
	static const LColour Red;
	static const LColour Green;
	static const LColour Blue;

	/// Call if the system defined colours changed.
	static void OnChange();
	/// Gets a colour from the LGI config (not available in LGI_STATIC)
	static bool GetConfigColour(const char *Tag, LColour &c);

	/// Transparent
	LColour();
	/// System colour
	LColour(LSystemColour sc);
	/// Indexed colour
	LColour(uint8_t idx8, GPalette *palette);
	/// True colour
	LColour(int r, int g, int b, int a = 255);
	/// Conversion from COLOUR
	LColour(uint32_t c, int bits, GPalette *palette = NULL);
	/// Web colour
	LColour(const char *Str);
	#ifdef __GTK_H__
	LColour(Gtk::GdkRGBA c);
	#endif
	LColourSpace GetColourSpace();
	bool SetColourSpace(LColourSpace cs);
	
	bool operator ==(const LColour &c)
	{
		return c32() == c.c32();
	}
	bool operator !=(const LColour &c)
	{
		return c32() != c.c32();
	}
	int operator -(const LColour &c)
	{
		auto Diff = GetGray() - c.GetGray();
		return abs(Diff);
	}
	operator bool() const
	{
		return IsValid();
	}
	
	bool IsValid() const;
	void Empty();
	bool IsTransparent();
	/// Sets the colour to a rgb(a) value
	void Rgb(int r, int g, int b, int a = 255);
	/// Sets the colour
	void Set(uint32_t c, int bits, GPalette *palette = NULL);
	/// Set to a system colour
	void Set(LSystemColour c);
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
	LColour Mix(LColour Tint, float RatioOfTint = 0.5) const;
	/// Return the inverted form
	LColour Invert();
	
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

	// Conversion
	void ToRGB();
	int GetGray(int BitDepth = 8) const;
	uint32_t GetNative();
	
	// String IO
	char *GetStr();
	bool SetStr(const char *Str);
	
	#ifdef HAIKU
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

// Get a system colour
LgiExtern LColour LSysColour(LSystemColour Colour);

// Load theme colours
LgiExtern bool LColourLoad(const char *Json);

