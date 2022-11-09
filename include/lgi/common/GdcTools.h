/// \file Graphics tools
#ifndef _DC_TOOLS_H_
#define _DC_TOOLS_H_

/// Various RGB colour space types
enum RgbLutCs
{
	RgbLutSRGB,
	RgbLutLinear
};

/// Build an RGB colour space conversion lookup table from 0 to max(T) 
// and of length 'Len'
template <typename T, int Len = 256>
class RgbLut
{
public:
	T Lut[Len];

	/// Construct the LUT
	RgbLut(RgbLutCs To, RgbLutCs From)
	{
		T Mul;
		memset(&Mul, 0xff, sizeof(Mul));
		double Div = Len - 1;

		double in, out;
		if (From == RgbLutLinear && To == RgbLutSRGB)
		{
			for (int i=0; i<Len; i++)
			{
				in = (double)i / Div;
				if (in <= 0.0031308)
					out = in * 12.92;
				else
					out = pow(in, 1.0 / 2.4) * 1.055 - 0.055;

				Lut[i] = (T) (out * Mul + 0.0000001);
			}
		}
		else if (From == RgbLutSRGB && To == RgbLutLinear)
		{
			for (int i=0; i<Len; i++)
			{
				in = (double)i / Div;
				if (in <= 0.04045)
					out = in / 12.92;
				else
					out = pow((in + 0.055) / 1.055, 2.4);

				Lut[i] = (T) (out * Mul);
			}
		}
	}
};

// remap dc to new palette
extern bool RemapDC(LSurface *pDC, LPalette *DestPal);

// flip dc
#define		FLIP_X					1
#define		FLIP_Y					2

extern bool FlipDC(LSurface *pDC, int Dir);

// return true if dc is greyscale
extern bool IsGreyScale(LSurface *pDC);

// convert dc to greyscale
extern bool GreyScaleDC(LSurface *pDest, LSurface *pSrc);

// invert the dc
extern bool InvertDC(LSurface *pDC);

// rotate dc
extern bool RotateDC(LSurface *pDC, double Angle, Progress *Prog = NULL);

// flip dc
extern bool FlipXDC(LSurface *pDC, Progress *Prog = NULL);
extern bool FlipYDC(LSurface *pDC, Progress *Prog = NULL);

// resample the dc
extern bool ResampleDC(LSurface *pTo, LSurface *pFrom, LRect *FromRgn = 0, Progress *Prog = NULL);

#endif
