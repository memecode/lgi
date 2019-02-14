/// \file
/// \author Matthew Allen
/// \created 3/2/2011
#ifndef _GCOLOUR_H_
#define _GCOLOUR_H_

#include "GColourSpace.h"

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

	/// Transparent
	GColour();
	/// Indexed colour
	GColour(uint8_t idx8, GPalette *palette);
	/// True colour
	GColour(int r, int g, int b, int a = 255);
	/// Conversion from COLOUR
	GColour(uint32_t c, int bits, GPalette *palette = NULL);
	GColourSpace GetColourSpace();
	
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
	/// Gets the red component (0-255)
	uint8_t r() const;
	/// Gets the green component (0-255)
	uint8_t g() const;
	/// Gets the blue component (0-255)
	uint8_t b() const;
	/// Gets the alpha component (0-255)
	uint8_t a() const;
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

#endif
