/// \file
/// \author Matthew Allen, fret@memecode.com
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Palette.h"

#define LGI_PI				3.141592654
#define LGI_RAD				(360/(2*LGI_PI))

/****************************** Classes *************************************************************************************/
LPalette::LPalette()
{
	Data = 0;
	Size = 0;
}

LPalette::LPalette(LPalette *pPal)
{
	// printf("%s:%i - %p::new(%p) %i\n", _FL, this, pPal, pPal?pPal->GetSize():0);

	Size = 0;
	Data = 0;
	Set(pPal);
}

LPalette::LPalette(uchar *pPal, int s)
{
	// printf("%s:%i - %p::new(%p) %i\n", _FL, this, pPal, s);

	Size = 0;
	Data = 0;
	Set(pPal, s);
}

LPalette::~LPalette()
{
	DeleteArray(Data);
	Size = 0;
}

void LPalette::Set(LPalette *pPal)
{
	if (pPal == this)
		return;

	// printf("%s:%i - %p::Set(%p) %i\n", _FL, this, pPal, pPal?pPal->GetSize():0);
	DeleteArray(Data);
	Size = 0;
	if (pPal)
	{
		LAssert(pPal->GetSize() > 0);

		if (pPal->Data)
		{
			Data = new GdcRGB[pPal->Size];
			if (Data)
			{
				memcpy(Data, pPal->Data, sizeof(GdcRGB) * pPal->Size);
			}
		}
		
		Size = pPal->Size;
	}
}

void LPalette::Set(int Index, int r, int g, int b)
{
	GdcRGB *rgb = (*this)[Index];
	if (rgb)
	{
		rgb->r = r;
		rgb->g = g;
		rgb->b = b;
	}
}

void LPalette::Set(uchar *pPal, int s)
{
	// printf("%s:%i - SetPal %p %i\n", _FL, pPal, s);

	DeleteArray(Data);
	Size = 0;
	Data = new GdcRGB[s];
	if (Data)
	{
		if (pPal)
		{
			for (int i=0; i<s; i++)
			{
				Data[i].r = *pPal++;
				Data[i].g = *pPal++;
				Data[i].b = *pPal++;
			}
		}

		Size = s;
	}
}

bool LPalette::Update()
{
	return true;
}

bool LPalette::SetSize(int s)
{
	// printf("%s:%i - SetSz %i\n", _FL, s);

	GdcRGB *New = new GdcRGB[s];
	if (New)
	{
		memset(New, 0, s * sizeof(GdcRGB));
		if (Data)
		{
			memcpy(New, Data, MIN(s, Size)*sizeof(GdcRGB));
		}

		DeleteArray(Data);
		Data = New;
		Size = s;
		return true;
	}

	return false;
}

void LPalette::SwapRAndB()
{
	if (Data)
	{
		for (int i=0; i<GetSize(); i++)
		{
			uchar n = (*this)[i]->r;
			(*this)[i]->r = (*this)[i]->b;
			(*this)[i]->b = n;
		}
	}

	Update();
}

uchar *LPalette::MakeLut(int Bits)
{
	uchar *Lut = 0;
	// GdcRGB *p = (*this)[0];

	int Size = 1 << Bits;
	switch (Bits)
	{
		case 15:
		{
			Lut = new uchar[Size];
			if (Lut)
			{
				for (int i=0; i<Size; i++)
				{
					int r = (R15(i) << 3) | (R15(i) >> 2);
					int g = (G15(i) << 3) | (G15(i) >> 2);
					int b = (B15(i) << 3) | (B15(i) >> 2);

					Lut[i] = MatchRgb(Rgb24(r, g, b));
				}
			}
			break;
		}
		case 16:
		{
			Lut = new uchar[Size];
			if (Lut)
			{
				for (int i=0; i<Size; i++)
				{
					int r = (R16(i) << 3) | (R16(i) >> 2);
					int g = (G16(i) << 2) | (G16(i) >> 3);
					int b = (B16(i) << 3) | (B16(i) >> 2);

					Lut[i] = MatchRgb(Rgb24(r, g, b));
				}
			}
			break;
		}
	}

	return Lut;
}

int LPalette::MatchRgb(COLOUR Rgb)
{
	if (Data)
	{
		GdcRGB *Entry = (*this)[0];
		int r = (R24(Rgb) & 0xF8) + 4;
		int g = (G24(Rgb) & 0xF8) + 4;
		int b = (B24(Rgb) & 0xF8) + 4;
		ulong *squares = GdcD->GetCharSquares();
		ulong mindist = 200000;
		ulong bestcolor;
		ulong curdist;
		long rdist;
		long gdist;
		long bdist;

		for (int i = 0; i < Size; i++)
		{
			rdist = Entry[i].r - r;
			gdist = Entry[i].g - g;
			bdist = Entry[i].b - b;
			curdist = squares[rdist] + squares[gdist] + squares[bdist];

			if (curdist < mindist)
			{
				mindist = curdist;
				bestcolor = i;
			}
		}

		return bestcolor;
	}

	return 0;
}

void LPalette::CreateGreyScale()
{
	SetSize(256);
	GdcRGB *p = (*this)[0];

	for (int i=0; i<256; i++)
	{
		p->r = i;
		p->g = i;
		p->b = i;
		p->a = 0;
		p++;
	}
}

void LPalette::CreateCube()
{
	SetSize(256);
	GdcRGB *p = (*this)[0];

	for (int r=0; r<6; r++)
	{
		for (int g=0; g<6; g++)
		{
			for (int b=0; b<6; b++)
			{
				p->r = r * 51;
				p->g = g * 51;
				p->b = b * 51;
				p->a = 0;
				p++;
			}
		}
	}

	for (int i=216; i<256; i++)
	{
		(*this)[i]->r = 0;
		(*this)[i]->g = 0;
		(*this)[i]->b = 0;
	}
}


void TrimWhite(char *s)
{
	auto *White = " \r\n\t";
	char *c = s;
	while (*c && strchr(White, *c)) c++;
	if (c != s)
	{
		strcpy(s, c);
	}

	c = s + strlen(s) - 1;
	while (c > s && strchr(White, *c))
	{
		*c = 0;
		c--;
	}
}

bool LPalette::Load(LFile &F)
{
	bool Status = false;
	char Buf[256];
	
	F.ReadStr(Buf, sizeof(Buf));
	TrimWhite(Buf);
	if (strcmp(Buf, "JASC-PAL") == 0)
	{
		// is JASC palette
		// skip hex length
		F.ReadStr(Buf, sizeof(Buf));

		// read decimal length
		F.ReadStr(Buf, sizeof(Buf));
		SetSize(atoi(Buf));
		for (int i=0; i<GetSize() && !F.Eof(); i++)
		{
			F.ReadStr(Buf, sizeof(Buf));
			GdcRGB *p = (*this)[i];
			if (p)
			{
				p->r = atoi(strtok(Buf, " "));
				p->g = atoi(strtok(NULL, " "));
				p->b = atoi(strtok(NULL, " "));

			}

			Status = true;
		}
	}
	else
	{
		// check for microsoft format
	}

	return Status;
}

bool LPalette::Save(LFile &F, int Format)
{
	bool Status = false;

	switch (Format)
	{
		case GDCPAL_JASC:
		{
			char Buf[256];

			sprintf(Buf, "JASC-PAL\r\n%04.4X\r\n%i\r\n", GetSize(), GetSize());
			F.Write(Buf, strlen(Buf));

			for (int i=0; i<GetSize(); i++)
			{
				GdcRGB *p = (*this)[i];
				if (p)
				{
					sprintf(Buf, "%i %i %i\r\n", p->r, p->g, p->b);
					F.Write(Buf, strlen(Buf));
				}
			}

			Status = true;
			break;
		}
	}

	return Status;
}

bool LPalette::operator ==(LPalette &p)
{
	if (GetSize() == p.GetSize())
	{
		GdcRGB *a = (*this)[0];
		GdcRGB *b = p[0];

		for (int i=0; i<GetSize(); i++)
		{
			if (a->r != b->r ||
				a->g != b->g ||
				a->b != b->b)
			{
				return false;
			}

			a++;
			b++;
		}

		return true;
	}

	return false;
}

bool LPalette::operator !=(LPalette &p)
{
	return !((*this) == p);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LBmpMem::LBmpMem()
{
	Base = 0;
	Flags = 0;
}

LBmpMem::~LBmpMem()
{
	if (Base && (Flags & BmpOwnMemory))
	{
		delete [] Base;
		Base = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
class GdcDevicePrivate
{
public:
	GdcDevice *Device;

	// Current mode info
	int ScrX;
	int ScrY;
	int ScrBits;
	LColourSpace ScrColourSpace;

	// Palette
	double GammaCorrection;
	uchar GammaTable[256];
	LPalette *pSysPal;
	LGlobalColour *GlobalColour;

	// Data
	ulong *CharSquareData;
	uchar *Div255;
	// Options
	int OptVal[GDC_MAX_OPTION];

	GdcDevicePrivate(GdcDevice *d)
	{
		Device = d;
		GlobalColour = 0;
	    	ZeroObj(OptVal);
	    	ScrX = ScrY = 0;
	    	ScrBits = 0;

		// Palette information
		GammaCorrection = 1.0;

		// Get mode stuff
		Gtk::GdkDisplay *Dsp = Gtk::gdk_display_get_default();
		Gtk::gint Screens = Gtk::gdk_display_get_n_screens(Dsp);
		for (Gtk::gint i=0; i<Screens; i++)
		{
			Gtk::GdkScreen *Scr = Gtk::gdk_display_get_screen(Dsp, i);
			if (Scr)
			{
				ScrX = Gtk::gdk_screen_get_width(Scr);
				ScrY = Gtk::gdk_screen_get_height(Scr);

				Gtk::GdkVisual *Vis = Gtk::gdk_screen_get_system_visual(Scr);
				if (Vis)
				{
					ScrBits = gdk_visual_get_depth(Vis);
					ScrColourSpace = GdkVisualToColourSpace(Vis, ScrBits);
				}
			}
		}
		
		printf("Screen: %i x %i @ %i bpp (%s)\n", ScrX, ScrY, ScrBits, LColourSpaceToString(ScrColourSpace));
		
		#if !LGI_RPI
		OptVal[GDC_PROMOTE_ON_LOAD] = ScrBits;
		#endif

		// Calcuate lookups
		CharSquareData = new ulong[255+255+1];
		if (CharSquareData)
		{
			for (int i = -255; i <= 255; i++)
			{
				CharSquareData[i+255] = i*i;
			}
		}

		// Divide by 255 lookup, real handy for alpha blending 8 bit components
		int Size = (255 * 255) * 2;
		Div255 = new uchar[Size];
		if (Div255)
		{
			for (int i=0; i<Size; i++)
			{
				int n = (i + 128) / 255;
				Div255[i] = MIN(n, 255);
			}
		}

	}

	~GdcDevicePrivate()
	{
		DeleteObj(GlobalColour);
		DeleteArray(CharSquareData);
		DeleteArray(Div255);
	}
};

//////////////////////////////////////////////////////////////
GdcDevice *GdcDevice::pInstance = 0;
GdcDevice::GdcDevice()
{
	LColourSpaceTest();
	d = new GdcDevicePrivate(this);
	pInstance = this;
}

GdcDevice::~GdcDevice()
{
	// delete stuff
	pInstance = NULL;
	DeleteObj(d);
}

int GdcDevice::GetOption(int Opt)
{
	if (Opt >= 0 && Opt < GDC_MAX_OPTION)
	{
		return d->OptVal[Opt];
	}

	LAssert(0);
	return 0;
}

int GdcDevice::SetOption(int Opt, int Value)
{
	int Prev = d->OptVal[Opt];
	if (Opt >= 0 && Opt < GDC_MAX_OPTION)
	{
		d->OptVal[Opt] = Value;
	}
	else
	{
		LAssert(0);
	}
	return Prev;
}

ulong *GdcDevice::GetCharSquares()
{
	return (d->CharSquareData) ? d->CharSquareData + 255 : 0;
}

uchar *GdcDevice::GetDiv255()
{
	return d->Div255;
}

LGlobalColour *GdcDevice::GetGlobalColour()
{
	return d->GlobalColour;
}

LColourSpace GdcDevice::GetColourSpace()
{
	return d->ScrColourSpace;
}

int GdcDevice::GetBits()
{
	return d->ScrBits;
}

int GdcDevice::X()
{
	return d->ScrX;
}

int GdcDevice::Y()
{
	return d->ScrY;
}

void GdcDevice::SetGamma(double Gamma)
{
	d->GammaCorrection = Gamma;
	for (int i=0; i<256; i++)
	{
		d->GammaTable[i] = (uchar) (pow(((double)i)/256, Gamma) * 256);
	}
}

double GdcDevice::GetGamma()
{
	return d->GammaCorrection;
}

void GdcDevice::SetSystemPalette(int Start, int Size, LPalette *pPal)
{
	/*
	if (pPal)
	{
		uchar Pal[768];
		uchar *Temp = Pal;
		uchar *System = Palette + (Start * 3);
		GdcRGB *P = (*pPal)[Start];
		
		for (int i=0; i<Size; i++, P++)
		{
			*Temp++ = GammaTable[*System++ = P->R] >> PalShift;
			*Temp++ = GammaTable[*System++ = P->G] >> PalShift;
			*Temp++ = GammaTable[*System++ = P->B] >> PalShift;
		}
		
		SetPaletteBlockDirect(Pal, Start, Size * 3);
	}
	*/
}

LPalette *GdcDevice::GetSystemPalette()
{
	return d->pSysPal;
}

void GdcDevice::SetColourPaletteType(int Type)
{
	/*
	bool SetOpt = true;

	switch (Type)
	{
		case PALTYPE_ALLOC:
		{
			SetPalIndex(0, 0, 0, 0);
			SetPalIndex(255, 255, 255, 255);
			break;
		}
		case PALTYPE_RGB_CUBE:
		{
			uchar Pal[648];
			uchar *p = Pal;

			for (int r=0; r<6; r++)
			{
				for (int g=0; g<6; g++)
				{
					for (int b=0; b<6; b++)
					{
						*p++ = r * 51;
						*p++ = g * 51;
						*p++ = b * 51;
					}
				}
			}

			SetPalBlock(0, 216, Pal);
			SetPalIndex(255, 0xFF, 0xFF, 0xFF);
			break;
		}
		case PALTYPE_HSL:
		{
			for (int h = 0; h<16; h++)
			{
			}
			break;
		}
		default:
		{
			SetOpt = false;
		}
	}

	if (SetOpt)
	{
		SetOption(GDC_PALETTE_TYPE, Type);
	}
	*/
}

COLOUR GdcDevice::GetColour(COLOUR Rgb24, LSurface *pDC)
{
	int Bits = (pDC) ? pDC->GetBits() : GetBits();
	COLOUR C;

	switch (Bits)
	{
		case 8:
		{
			switch (GetOption(GDC_PALETTE_TYPE))
			{
				case PALTYPE_ALLOC:
				{
					static uchar Current = 1;

					Rgb24 &= 0xFFFFFF;
					if (Rgb24 == 0xFFFFFF)
					{
						C = 0xFF;
					}
					else if (Rgb24 == 0)
					{
						C = 0;
					}
					else
					{
						GdcRGB *p = (*d->pSysPal)[Current];
						p->r = R24(Rgb24);
						p->g = G24(Rgb24);
						p->b = B24(Rgb24);
						
						C = Current++;
						if (Current == 255) Current = 1;
					}
					break;
				}
				case PALTYPE_RGB_CUBE:
				{
					uchar r = (R24(Rgb24) + 25) / 51;
					uchar g = (G24(Rgb24) + 25) / 51;
					uchar b = (B24(Rgb24) + 25) / 51;
					
					C = (r*36) + (g*6) + b;
					break;
				}
				case PALTYPE_HSL:
				{
					C = 0;
					break;
				}
			}
			break;
		}
		case 16:
		{
			C = Rgb24To16(Rgb24);
			break;
		}
		case 24:
		{
			C = Rgb24;
			break;
		}
		case 32:
		{
			C = Rgb24To32(Rgb24);
			break;
		}
	}

	return C;
}

//////////////////////////////////////////////////////////////////////////////////////////////
static int _Factories;
static LApplicatorFactory *_Factory[16];

LApp8 Factory8;
LApp15 Factory15;
LApp16 Factory16;
LApp24 Factory24;
LApp32 Factory32;
LAlphaFactory FactoryAlpha;

LApplicatorFactory::LApplicatorFactory()
{
	LAssert(_Factories >= 0 && _Factories < CountOf(_Factory));
	if (_Factories < CountOf(_Factory) - 1)
	{
		_Factory[_Factories++] = this;
	}
}

LApplicatorFactory::~LApplicatorFactory()
{
	LAssert(_Factories >= 0 && _Factories < CountOf(_Factory));
	for (int i=0; i<_Factories; i++)
	{
		if (_Factory[i] == this)
		{
			_Factory[i] = _Factory[_Factories-1];
			_Factories--;
			break;
		}
	}
}

LApplicator *LApplicatorFactory::NewApp(LColourSpace Cs, int Op)
{
	LAssert(_Factories >= 0 && _Factories < CountOf(_Factory));
	for (int i=0; i<_Factories; i++)
	{
		LApplicator *a = _Factory[i]->Create(Cs, Op);
		if (a) return a;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
class GlobalColourEntry
{
public:
	COLOUR c24;
	bool Fixed;
	bool Used;

	GlobalColourEntry()
	{
		c24 = 0;
		Fixed = false;
		Used = false;
	}
};

class LGlobalColourPrivate
{
public:
	GlobalColourEntry c[256];
	LPalette *Global;
	List<LSurface> Cache;
	int FirstUnused;
	
	int FreeColours()
	{
		int f = 0;
		
		for (int i=0; i<256; i++)
		{
			if (!c[i].Used)
			{
				f++;
			}
		}

		return f;
	}

	LGlobalColourPrivate()
	{
	}

	~LGlobalColourPrivate()
	{
		Cache.DeleteObjects();
	}
};

LGlobalColour::LGlobalColour()
{
	d = new LGlobalColourPrivate;
}

LGlobalColour::~LGlobalColour()
{
	DeleteObj(d);
}

COLOUR LGlobalColour::AddColour(COLOUR c24)
{
	return c24;
}

bool LGlobalColour::AddBitmap(LSurface *pDC)
{
	return false;
}

bool LGlobalColour::AddBitmap(LImageList *il)
{
	return false;
}

bool LGlobalColour::MakeGlobalPalette()
{
	return 0;
}

LPalette *LGlobalColour::GetPalette()
{
	return 0;
}

COLOUR LGlobalColour::GetColour(COLOUR c24)
{
	return c24;
}

bool LGlobalColour::RemapBitmap(LSurface *pDC)
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////
union EndianTest
{
	char b[2];
	short s;
};

#define VisualToColourSpaceDebug	0

LColourSpace GdkVisualToColourSpace(Gtk::GdkVisual *v, int output_bits)
{
	uint32_t c = CsNone;
	if (v)
	{
		EndianTest Test;
		Test.b[0] = 1;
		Test.b[1] = 0;
		
		#if VisualToColourSpaceDebug
		bool LittleEndian = Test.s == 1;
		printf("GdkVisualToColourSpace, Type: %i, LittleEndian=%i\n", gdk_visual_get_visual_type(v), LittleEndian);
		#endif

		auto Depth = gdk_visual_get_depth(v);
		
		#define comp(c) \
			Gtk::guint32 mask_##c; \
			Gtk::gint shift_##c, precision_##c; \
			gdk_visual_get_red_pixel_details (v, &mask_##c, &shift_##c, &precision_##c)
		comp(r);
		comp(g);
		comp(b);

		switch (gdk_visual_get_visual_type(v))
		{
			default:
			{
				LAssert(!"impl me");
				c = LBitsToColourSpace(Depth);
				break;
			}
			case Gtk::GDK_VISUAL_PSEUDO_COLOR:
			case Gtk::GDK_VISUAL_STATIC_COLOR:
			{
				LAssert(Depth <= 16);
				c = (CtIndex << 4) | (Depth != 16 ? Depth : 0);
				break;
			}
			case Gtk::GDK_VISUAL_TRUE_COLOR:
			case Gtk::GDK_VISUAL_DIRECT_COLOR:
			{
				int red =   (CtRed   << 4) | precision_r;
				int green = (CtGreen << 4) | precision_g;
				int blue =  (CtBlue  << 4) | precision_b;
				#ifdef __arm__
				if
				(
					(Depth == 16 && shift_r < shift_b)
					||
					(Depth != 16 && shift_r > shift_b)
				)
				#else
				if (shift_r > shift_b)
				#endif
				{
					c = (red << 16) | (green << 8) | blue;
				}
				else
				{
					c = (blue << 16) | (green << 8) | red;
				}
	
				int bits = LColourSpaceToBits((LColourSpace) c);
	
				#if VisualToColourSpaceDebug
				{
					Gtk::guint32 mask[3];
					Gtk::gint shift[3], precision[3];
					gdk_visual_get_red_pixel_details(v, mask+0, shift+0, precision+0);
					gdk_visual_get_green_pixel_details(v, mask+1, shift+1, precision+1);
					gdk_visual_get_blue_pixel_details(v, mask+2, shift+2, precision+2);
					printf("GdkVisualToColourSpace, rgb: %i/%i, %i/%i, %i/%i  bits: %i  output_bits: %i\n",
						precision[0], shift[0],
						precision[1], shift[1],
						precision[2], shift[2],
						bits, output_bits
						);
				}
				#endif
				
				if (bits != output_bits)
				{
					int remaining_bits = output_bits - bits;
					LAssert(remaining_bits <= 16);
					if (remaining_bits <= 16)
					{
						c |=
							(
								(
									(CtAlpha << 4)
									|
									(remaining_bits < 16 ? remaining_bits : 0)
								)
							)
							<<
							24;
					}
				}
				break;
			}
		}

		#if 1
		if (Depth != 16)
		{
			if (gdk_visual_get_byte_order(v) == Gtk::GDK_LSB_FIRST)
			{
				#if VisualToColourSpaceDebug
				printf("GdkVisualToColourSpace swapping\n");
				#endif
				c = LgiSwap32(c);
				while (!(c & 0xff))
					c >>= 8;
			}
		}
		#endif
	}
	
	LColourSpace Cs = (LColourSpace)c;
	#if VisualToColourSpaceDebug
	printf("GdkVisualToColourSpace %x %s\n", Cs, LColourSpaceToString(Cs));
	#endif
	
	return Cs;
}

