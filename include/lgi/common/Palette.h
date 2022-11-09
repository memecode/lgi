#ifndef _GPALETTE_H_
#define _GPALETTE_H_

#include "lgi/common/ColourSpace.h"
#include "lgi/common/File.h"

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#endif

/// RGB Colour
typedef LRgba32 GdcRGB;

#ifdef WIN32
#pragma pack(pop, before_pack)
#endif

/// Palette of colours
class LgiClass LPalette
{
protected:
	#if WINNATIVE
	HPALETTE	hPal;
	LOGPALETTE	*Data;
	#else
	int			Size;
	LRgba32		*Data;
	#endif
	uchar *Lut;

public:
	LPalette();
	virtual ~LPalette();

	#if WINNATIVE
	HPALETTE Handle() { return hPal; }
	#endif

	LPalette(LPalette *pPal);	
	LPalette(uchar *pPal, int s = 256);
	void Set(LPalette *pPal);
	void Set(uchar *pPal, int s = 256);
	void Set(int Index, int r, int g, int b);

	bool Update();
	bool SetSize(int s = 256);
	void SwapRAndB();
	int MatchRgb(uint32_t Rgb);
	void CreateCube();
	void CreateGreyScale();
	bool Load(LFile &F);
	bool Save(LFile &F, int Format);
	uchar *MakeLut(int Bits = 16);

	bool operator ==(LPalette &p);
	bool operator !=(LPalette &p);

	#if WINNATIVE
	LRgba32 *operator [](int i)
	{
		return (i >= 0 && i < GetSize() && Data) ? (GdcRGB*) (Data->palPalEntry + i) : NULL;
	}
	
	int GetSize()
	{
		return (Data) ? Data->palNumEntries : 0;
	}
	#else
	LRgba32 *operator [](int i)
	{
		return (i >= 0 && i < GetSize() && Data) ? (GdcRGB*) (Data + i) : NULL;
	}
	
	int GetSize()
	{
		return Size;
	}
	#endif
};

#endif