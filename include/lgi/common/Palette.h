#ifndef _GPALETTE_H_
#define _GPALETTE_H_

#include "lgi/common/ColourSpace.h"
#include "lgi/common/File.h"

/// Palette of colours
class LgiClass LPalette
{
protected:
	#if WINNATIVE
	HPALETTE	hPal = NULL;
	LOGPALETTE	*Data = NULL;
	#else
	int			Size = 0;
	LRgba32		*Data = NULL;
	#endif
	uchar *Lut = NULL;

public:
	static size_t Instances;
	LPalette();
	virtual ~LPalette();
	void Empty();

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
		return (i >= 0 && i < GetSize() && Data) ? (LRgba32*) (Data->palPalEntry + i) : NULL;
	}
	
	int GetSize()
	{
		return (Data) ? Data->palNumEntries : 0;
	}
	#else
	LRgba32 *operator [](int i)
	{
		return (i >= 0 && i < GetSize() && Data) ? (Data + i) : NULL;
	}
	
	int GetSize()
	{
		return Size;
	}
	#endif
};

#endif