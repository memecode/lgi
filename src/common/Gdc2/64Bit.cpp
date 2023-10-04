/**
	\file
	\author Matthew Allen
	\date 24/2/2014
	\brief 64 bit primitives (RGBA 16 bit)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lgi/common/Gdc2.h"
#include "lgi/common/Palette.h"
#include "lgi/common/Variant.h"

#define BytePtr	((uint8_t*&)Ptr)
#undef NonPreMulOver64
#define NonPreMulOver64(c)	d->c = ((s->c * sa) + ((d->c * 0xffff) / 0xffff * o)) / 0xffff

template<typename Pixel, LColourSpace ColourSpace>
class App64 : public LApplicator
{
	union
	{
		uint8_t *u8;
		Pixel *p;
	};

	int Op;	
	int ConstAlpha;
	LPalette *PalAlpha;

public:
	App64(int op)
	{
		Op = op;
		p = NULL;
		ConstAlpha = 0xffff;
		PalAlpha = NULL;
	}

	bool GetVariant(const char *Name, LVariant &Value, const char *Array) override
	{
		switch (LStringToDomProp(Name))
		{
			case AppAlpha:
			{
				Value = ConstAlpha;
				return true;
			}
			default:
			{
				LAssert(!"impl me.");
				break;
			}
		}

		return false;
	}
	
	bool SetVariant(const char *Name, LVariant &Value, const char *Array) override
	{
		switch (LStringToDomProp(Name))
		{
			case AppAlpha:
			{
				ConstAlpha = Value.CastInt32();
				return true;
			}
			case AppPalette:
			{
				PalAlpha = (LPalette*)Value.CastVoidPtr();
				return true;
			}
			default:
			{
				LAssert(!"impl me.");
				break;
			}
		}
		
		return false;
	}

	bool SetSurface(LBmpMem *d, LPalette *pal = NULL, LBmpMem *a = NULL) override
	{
		if (d && d->Cs == ColourSpace)
		{
			Dest = d;
			Pal = pal;
			p = (Pixel*) d->Base;
			Alpha = 0;
			return true;
		}
		return false;
	}

	void SetPtr(int x, int y) override
	{
		p = (Pixel*) (Dest->Base + (y * Dest->Line) + (x * sizeof(Pixel)));
	}
	
	void IncX() override
	{
		p++;
	}
	
	void IncY() override
	{
		u8 += Dest->Line;
	}
	
	void IncPtr(int X, int Y) override
	{
		p += X;
		u8 += Y * Dest->Line;
	}
	
	void Set() override
	{
		if (p32.a == 255)
		{
			p->r = G8bitTo16bit(p32.r);
			p->g = G8bitTo16bit(p32.g);
			p->b = G8bitTo16bit(p32.b);
			p->a = -1;
		}
		else if (p32.a)
		{
			LAssert(0);
		}
	}
	
	COLOUR Get() override
	{
		return Rgb24(p->r >> 8, p->g >> 8, p->b >> 8);
	}
	
	void VLine(int height) override
	{
		Pixel cp;
		cp.r = G8bitTo16bit(p32.r);
		cp.g = G8bitTo16bit(p32.g);
		cp.b = G8bitTo16bit(p32.b);
		cp.a = G8bitTo16bit(p32.a);

		while (height-- > 0)
		{
			*p = cp;
			u8 += Dest->Line;
		}
	}
	
	void Rectangle(int x, int y) override
	{
		Pixel cp;
		cp.r = G8bitTo16bit(p32.r);
		cp.g = G8bitTo16bit(p32.g);
		cp.b = G8bitTo16bit(p32.b);
		cp.a = G8bitTo16bit(p32.a);
		
		while (y-- > 0)
		{
			REG Pixel *i = p, *e = i + x;
			while (i < e)
			{
				*i++ = cp;
			}
			u8 += Dest->Line;
		}
	}
	
	template<typename T>
	bool CopyBltNoAlphaUpScale(LBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			Pixel *d = p;
			T *s = (T*) (Src->Base + (y * Src->Line));
			T *e = s + Src->x;

			while (s < e)
			{
				d->r = G8bitTo16Bit(s->r);
				d->g = G8bitTo16Bit(s->g);
				d->b = G8bitTo16Bit(s->b);
				d->a = 0xffff;
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	template<typename T>
	bool CopyBltWithAlphaUpScale(LBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			Pixel *d = p;
			T *s = (T*) (Src->Base + (y * Src->Line));
			T *e = s + Src->x;

			while (s < e)
			{
				d->r = G8bitTo16Bit(s->r);
				d->g = G8bitTo16Bit(s->g);
				d->b = G8bitTo16Bit(s->b);
				d->a = G8bitTo16Bit(s->a);
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	template<typename T>
	bool CopyBltNoAlpha(LBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			Pixel *d = p;
			T *s = (T*) (Src->Base + (y * Src->Line));
			T *e = s + Src->x;

			while (s < e)
			{
				d->r = s->r;
				d->g = s->g;
				d->b = s->b;
				d->a = 0xffff;
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	template<typename T>
	bool CopyBltWithAlpha(LBmpMem *Src)
	{
		for (int y=0; y<Src->y; y++)
		{
			Pixel *d = p;
			T *s = (T*) (Src->Base + (y * Src->Line));
			T *e = s + Src->x;

			while (s < e)
			{
				d->r = s->r;
				d->g = s->g;
				d->b = s->b;
				d->a = s->a;
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	template<typename T>
	bool AlphaBlt(LBmpMem *Src, LBmpMem *SrcAlpha)
	{
		for (int y=0; y<Src->y; y++)
		{
			Pixel *d = p;
			T *s = (T*) (Src->Base + (y * Src->Line));
			T *e = s + Src->x;
			uint16 *a = (uint16*) (Src->Base + (y * SrcAlpha->Line));

			while (s < e)
			{
				uint16 sa = *a++;
				if (sa == 255)
				{
					d->r = s->r;
					d->g = s->g;
					d->b = s->b;
				}
				else if (sa > 0)
				{
					uint16 o = 0xffff - sa;
					d->r = ((s->r * sa) + (d->r * o)) / 0xffff;
					d->g = ((s->g * sa) + (d->g * o)) / 0xffff;
					d->b = ((s->b * sa) + (d->b * o)) / 0xffff;
				}
				
				s++;
				d++;
			}

			u8 += Dest->Line;
		}
		
		return true;
	}
	
	bool Blt(LBmpMem *Src, LPalette *SPal, LBmpMem *SrcAlpha = NULL) override
	{
		if (!Src)
			return false;
		
		if (!SrcAlpha)
		{
			if (Src->Cs == CsIndex8)
			{
				Pixel map[256];
				for (int i=0; i<256; i++)
				{
					auto rgb = SPal ? (*SPal)[i] : NULL;
					if (rgb)
					{
						map[i].r = G8bitTo16bit(rgb->r);
						map[i].g = G8bitTo16bit(rgb->g);
						map[i].b = G8bitTo16bit(rgb->b);
					}
					else
					{
						map[i].r = G8bitTo16bit(i);
						map[i].g = G8bitTo16bit(i);
						map[i].b = G8bitTo16bit(i);
					}
					map[i].a = 0xffff;
				}
				for (int y=0; y<Src->y; y++)
				{
					REG uint8_t *s = Src->Base + (y * Src->Line);
					REG Pixel *d = p, *e = d + Src->x;
					while (d < e)
					{
						*d++ = map[*s++];
					}
					u8 += Dest->Line;
				}
			}
			else
			{
				LBmpMem Dst;
				Dst.Base = u8;
				Dst.x = Src->x;
				Dst.y = Src->y;
				Dst.Cs = Dest->Cs;
				Dst.Line = Dest->Line;				
				if (!LRopUniversal(&Dst, Src, Op==GDC_ALPHA))
				{
					return false;
				}
			}
		}
		else
		{
			switch (Src->Cs)
			{
				#define AlphaCase(name) \
					case Cs##name: return AlphaBlt<L##name>(Src, SrcAlpha);

				AlphaCase(Rgb24);
				AlphaCase(Bgr24);
				AlphaCase(Xrgb32);
				AlphaCase(Xbgr32);
				AlphaCase(Rgbx32);
				AlphaCase(Bgrx32);

				AlphaCase(Argb32);
				AlphaCase(Abgr32);
				AlphaCase(Rgba32);
				AlphaCase(Bgra32);

				default:
					LAssert(!"Impl me.");
					break;
			}
		}
		
		return false;
	}
};

class G64BitFactory : public LApplicatorFactory
{
public:
	LApplicator *Create(LColourSpace Cs, int Op)
	{
		switch (Cs)
		{
			#define Case64(name) \
				case Cs##name: \
					return new App64<L##name, Cs##name>(Op);
			
			Case64(Rgba64);
			Case64(Bgra64);
			Case64(Argb64);
			Case64(Abgr64);
			default:
				break;
		}
		return 0;
	}
} App64Factory;
