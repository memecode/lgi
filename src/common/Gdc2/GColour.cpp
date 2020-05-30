#include "Lgi.h"
#include "GPalette.h"
#include "GSkinEngine.h"

const GColour GColour::Black(0, 0, 0);
const GColour GColour::White(255, 255, 255);
const GColour GColour::Red(255, 0, 0);
const GColour GColour::Green(0, 192, 0);
const GColour GColour::Blue(0, 0, 255);

GColour::GColour()
{
	space = CsNone;
	flat = 0;
	pal = NULL;
}

GColour::GColour(const char *Str)
{
	space = CsNone;
	flat = 0;
	pal = NULL;
	SetStr(Str);
}

GColour::GColour(uint8_t idx8, GPalette *palette)
{
	c8(idx8, palette);
}

GColour::GColour(int r, int g, int b, int a)
{
	c32(Rgba32(r, g, b, a));
}

GColour::GColour(uint32_t c, int bits, GPalette *palette)
{
	Set(c, bits, palette);
}

#ifdef __GTK_H__
GColour::GColour(Gtk::GdkRGBA gtk)
{
	Rgb(gtk.red * 255.0, gtk.green * 255.0, gtk.blue * 255.0, gtk.alpha * 255.0);
}
#endif

int GColour::HlsValue(double fN1, double fN2, double fHue) const
{
	if (fHue > 360.0) fHue -= 360.0;
	else if (fHue < 0.0) fHue += 360.0;

	if (fHue < 60.0) return (int) ((fN1 + (fN2 - fN1) * fHue / 60.0) * 255.0 + 0.5);
	else if (fHue < 180.0) return (int) ((fN2 * 255.0) + 0.5);
	else if (fHue < 240.0) return (int) ((fN1 + (fN2 - fN1) * (240.0 - fHue) / 60.0) * 255.0 + 0.5);
	
	return (int) ((fN1 * 255.0) + 0.5);
}

GColourSpace GColour::GetColourSpace()
{
	return space;
}

bool GColour::SetColourSpace(GColourSpace cs)
{
	if (space == CsNone)
	{
		space = cs;
		rgb.a = 255;
		return true;
	}
	
	if (space == cs)
		return true;

	LgiAssert(!"Impl conversion.");
	return false;
}

bool GColour::IsValid()
{
	return space != CsNone;
}

void GColour::Empty()
{
	space = CsNone;
}

bool GColour::IsTransparent()
{
	if (space == System32BitColourSpace)
		return rgb.a == 0;
	else if (space == CsIndex8)
		return !pal || index >= pal->GetSize();
	return space == CsNone;
}

void GColour::Rgb(int r, int g, int b, int a)
{
	c32(Rgba32(r, g, b, a));
}

void GColour::Set(LSystemColour c)
{
	*this = LColour(c);
}

void GColour::Set(uint32_t c, int bits, GPalette *palette)
{
	pal = 0;
	switch (bits)
	{
		case 8:
		{
			index = c;
			space = CsIndex8;
			pal = palette;
			break;
		}
		case 15:
		{
			space = System32BitColourSpace;
			rgb.r = Rc15(c);
			rgb.g = Gc15(c);
			rgb.b = Bc15(c);
			rgb.a = 255;
			break;
		}
		case 16:
		{
			space = System32BitColourSpace;
			rgb.r = Rc16(c);
			rgb.g = Gc16(c);
			rgb.b = Bc16(c);
			rgb.a = 255;
			break;
		}
		case 24:
		case 48:
		{
			space = System32BitColourSpace;
			rgb.r = R24(c);
			rgb.g = G24(c);
			rgb.b = B24(c);
			rgb.a = 255;
			break;
		}
		case 32:
		case 64:
		{
			space = System32BitColourSpace;
			rgb.r = R32(c);
			rgb.g = G32(c);
			rgb.b = B32(c);
			rgb.a = A32(c);
			break;
		}
		default:
		{
			space = System32BitColourSpace;
			flat = 0;
			LgiTrace("Error: Unable to set colour %x, %i\n", c, bits);
			LgiAssert(!"Not a known colour depth.");
		}
	}
}

uint32_t GColour::Get(int bits)
{
	switch (bits)
	{
		case 8:
			if (space == CsIndex8)
				return index;
			LgiAssert(!"Not supported.");
			break;
		case 24:
			return c24();
		case 32:
			return c32();
	}
	
	return 0;
}

uint8_t GColour::r() const
{
	return R32(c32());
}

void GColour::r(uint8_t i)
{
	if (SetColourSpace(System32BitColourSpace))
		rgb.r = i;
	else
		LgiAssert(0);
}

uint8_t GColour::g() const
{
	return G32(c32());
}

void GColour::g(uint8_t i)
{
	if (SetColourSpace(System32BitColourSpace))
		rgb.g = i;
	else
		LgiAssert(0);
}

uint8_t GColour::b() const
{
	return B32(c32());
}

void GColour::b(uint8_t i)
{
	if (SetColourSpace(System32BitColourSpace))
		rgb.b = i;
	else
		LgiAssert(0);
}

uint8_t GColour::a() const
{
	return A32(c32());
}

void GColour::a(uint8_t i)
{
	if (SetColourSpace(System32BitColourSpace))
		rgb.a = i;
	else
		LgiAssert(0);
}

uint8_t GColour::c8() const
{
	return index;
}

void GColour::c8(uint8_t c, GPalette *p)
{
	space = CsIndex8;
	pal = p;
	index = c;
}

uint32_t GColour::c24() const
{
	if (space == System32BitColourSpace)
	{
		return Rgb24(rgb.r, rgb.g, rgb.b);
	}
	else if (space == CsIndex8)
	{
		if (pal)
		{
			// colour palette lookup
			if (index < pal->GetSize())
			{
				GdcRGB *c = (*pal)[index];
				if (c)
				{
					return Rgb24(c->r, c->g, c->b);
				}
			}

			return 0;
		}
		
		return Rgb24(index, index, index); // monochome
	}
	else if (space == CsHls32)
	{
		return Rgb32To24(c32());
	}

	// Black...
	return 0;
}

void GColour::c24(uint32_t c)
{
	space = System32BitColourSpace;
	rgb.r = R24(c);
	rgb.g = G24(c);
	rgb.b = B24(c);
	rgb.a = 255;
	pal = NULL;
}

uint32_t GColour::c32() const
{
	if (space == System32BitColourSpace)
	{
		return Rgba32(rgb.r, rgb.g, rgb.b, rgb.a);
	}
	else if (space == CsIndex8)
	{
		if (pal)
		{
			// colour palette lookup
			if (index < pal->GetSize())
			{
				GdcRGB *c = (*pal)[index];
				if (c)
				{
					return Rgb32(c->r, c->g, c->b);
				}
			}

			return 0;
		}
		
		return Rgb32(index, index, index); // monochome
	}
	else if (space == CsHls32)
	{
		// Convert from HLS back to RGB
		GColour tmp = *this;
		tmp.ToRGB();
		return tmp.c32();
	}

	// Transparent?
	return 0;
}

void GColour::c32(uint32_t c)
{
	space = System32BitColourSpace;
	pal = NULL;
	rgb.r = R32(c);
	rgb.g = G32(c);
	rgb.b = B32(c);
	rgb.a = A32(c);
}

GColour GColour::Mix(GColour Tint, float RatioOfTint) const
{
	COLOUR c1 = c32();
	COLOUR c2 = Tint.c32();
	
	double RatioThis = 1.0 - RatioOfTint;
	
	int r = (int) ((R32(c1) * RatioThis) + (R32(c2) * RatioOfTint));
	int g = (int) ((G32(c1) * RatioThis) + (G32(c2) * RatioOfTint));
	int b = (int) ((B32(c1) * RatioThis) + (B32(c2) * RatioOfTint));
	int a = (int) ((A32(c1) * RatioThis) + (A32(c2) * RatioOfTint));
	
	return GColour(r, g, b, a);
}

uint32_t GColour::GetH()
{
	ToHLS();
	return hls.h;
}

bool GColour::HueIsUndefined()
{
	ToHLS();
	return hls.h == HUE_UNDEFINED;
}

uint32_t GColour::GetL()
{
	ToHLS();
	return hls.l;
}

uint32_t GColour::GetS()
{
	ToHLS();
	return hls.s;
}

bool GColour::ToHLS()
{
	if (space == CsHls32)
		return true;

	uint32_t nMax, nMin, nDelta, c = c32();
	int R = R32(c), G = G32(c), B = B32(c);
	double fHue;

	nMax = MAX(R, MAX(G, B));
	nMin = MIN(R, MIN(G, B));

	if (nMax == nMin)
		return false;

	hls.l = (nMax + nMin) / 2;
	if (hls.l < 128)
		hls.s = (uchar) ((255.0 * ((double)(nMax - nMin)) / (double)(nMax + nMin)) + 0.5);
	else
		hls.s = (uchar) ((255.0 * ((double)(nMax - nMin)) / (double)(511 - nMax - nMin)) + 0.5);

	nDelta = nMax - nMin;

	if (R == nMax)
		fHue = ((double) (G - B)) / (double) nDelta;
	else if (G == nMax)
		fHue = 2.0 + ((double) (B - R)) / (double) nDelta;
	else
		fHue = 4.0 + ((double) (R - G)) / (double) nDelta;

	fHue *= 60;
	if (fHue < 0.0)
		fHue += 360.0;

	hls.h = (uint16) (fHue + 0.5);
	space = CsHls32;
	pal = NULL;
	return true;
}

void GColour::SetHLS(uint16 h, uint8_t l, uint8_t s)
{
	space = CsHls32;
	hls.h = h;
	hls.l = l;
	hls.s = s;
}

void GColour::ToRGB()
{
	GHls32 Hls = hls;
	if (Hls.s == 0)
	{
		Rgb(0, 0, 0);
	}
	else
	{
		while (Hls.h >= 360)
			Hls.h -= 360;			
		while (hls.h < 0)
			Hls.h += 360;
		
		double fHue = (double) Hls.h, fM1, fM2;
		double fLightness = ((double) Hls.l) / 255.0;
		double fSaturation = ((double) Hls.s) / 255.0;
		
		if (Hls.l < 128)
			fM2 = fLightness * (1 + fSaturation);
		else
			fM2 = fLightness + fSaturation - (fLightness * fSaturation);
		
		fM1 = 2.0 * fLightness - fM2;

		Rgb(HlsValue(fM1, fM2, fHue + 120.0),
			HlsValue(fM1, fM2, fHue),
			HlsValue(fM1, fM2, fHue - 120.0));
	}
}

int GColour::GetGray(int BitDepth)
{
	if (BitDepth == 8)
	{
		int R = r() * 77;
		int G = g() * 151;
		int B = b() * 28;
		return (R + G + B) >> 8;
	}
	
	double Scale = 1 << BitDepth;
	int R = (int) ((double) r() * (0.3 * Scale) + 0.5);
	int G = (int) ((double) g() * (0.59 * Scale) + 0.5);
	int B = (int) ((double) b() * (0.11 * Scale) + 0.5);
	return (R + G + B) >> BitDepth;
}

uint32_t GColour::GetNative()
{
	#ifdef WIN32
	if (space == CsIndex8)
	{
		if (pal && index < pal->GetSize())
		{
			GdcRGB *c = (*pal)[index];
			if (c)
			{
				return RGB(c->r, c->g, c->b);
			}
		}
		
		return RGB(index, index, index);
	}
	else if (space == System32BitColourSpace)
	{
		return RGB(rgb.r, rgb.g, rgb.b);
	}
	else
	{
		LgiAssert(0);
	}
	#else
	LgiAssert(0);
	#endif		
	return c32();
}

char *GColour::GetStr()
{
	static char Buf[4][32];
	static int Idx = 0;
	int b = Idx++;
	if (Idx >= 4) Idx = 0;
	switch (space)
	{
		case System32BitColourSpace:
			if (rgb.a == 0xff)
			{
				sprintf_s(	Buf[b], 32,
							"rgb(%i,%i,%i)",
							rgb.r,
							rgb.g,
							rgb.b);
			}
			else
			{
				sprintf_s(	Buf[b], 32,
							"rgba(%i,%i,%i,%i)",
							rgb.r,
							rgb.g,
							rgb.b,
							rgb.a);
			}
			break;
		case CsIndex8:
			sprintf_s(	Buf[b], 32,
						"index(%i)",
						index);
			break;
		case CsHls32:
			sprintf_s(	Buf[b], 32,
						"hls(%i,%i,%i)",
						hls.h,
						hls.l,
						hls.s);
			break;
		default:
			sprintf_s(	Buf[b], 32,
						"unknown(%i)",
						space);
			break;
	}

	return Buf[b];
}

bool GColour::SetStr(const char *str)
{
	if (!str)
		return false;

	GString Str = str;
	if (*str == '#')
	{
		// Web colour
		Str = Str.Strip("# \t\r\n");
		if (Str.Length() == 3)
		{
			auto h = htoi(Str.Get());
			uint8_t r = (h >> 8) & 0xf;
			uint8_t g = (h >> 4) & 0xf;
			uint8_t b = (h) & 0xf;
			Rgb(r | (r << 4), g | (g << 4), b | (b << 4));
		}
		else if (Str.Length() == 6)
		{
			auto h = htoi(Str.Get());
			uint8_t r = (h >> 16) & 0xff;
			uint8_t g = (h >> 8) & 0xff;
			uint8_t b = (h) & 0xff;
			Rgb(r, g, b);
		}
		else return false;
		return true;
	}

	char *s = strchr(Str, '(');
	if (!s)
		return false;
	char *e = strchr(s + 1, ')');
	if (!s)
		return false;

	*s = 0;
	*e = 0;
	GString::Array Comp = GString(s+1).Split(",");
	if (!Stricmp(Str.Get(), "rgb"))
	{
		if (Comp.Length() == 3)
			Rgb((int)Comp[0].Int(), (int)Comp[1].Int(), (int)Comp[2].Int());
		else if (Comp.Length() == 4)
			Rgb((int)Comp[0].Int(), (int)Comp[1].Int(), (int)Comp[2].Int(), (int)Comp[3].Int());
		else
			return false;
	}
	else if (!Stricmp(Str.Get(), "hls"))
	{
		if (Comp.Length() == 3)
			SetHLS((uint16) Comp[0].Int(), (uint8_t) Comp[1].Int(), (uint8_t) Comp[2].Int());
		else
			return false;
	}
	else if (!Stricmp(Str.Get(), "index"))
	{
		if (Comp.Length() == 1)
		{
			index = (uint8_t) Comp[0].Int();
			space = CsIndex8;
		}
		else return false;
	}
	else return false;

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static GColour _LgiColours[L_MAXIMUM];

#define ReadColourConfig(def)	GColour::GetConfigColour("Colour."#def, _LgiColours[def])

bool GColour::GetConfigColour(const char *Tag, GColour &c)
{
	#ifdef LGI_STATIC
	return false;
	#else
	if (!Tag)
		return false;

	GXmlTag *Col = LgiApp->GetConfig(Tag);
	if (!Col)
		return false;

	char *h;
	if (!(h = Col->GetAttr("Hex")))
		return false;

	if (*h == '#') h++;
	int n = htoi(h);

	c.Rgb( n>>16, n>>8, n );
	return true;
	#endif
}

////////////////////////////////////////////////////////////////////////////
#ifdef __GTK_H__
COLOUR ColTo24(Gtk::GdkColor &c)
{
	return Rgb24(c.red >> 8, c.green >> 8, c.blue >> 8);
}
#endif

#if defined(WINDOWS)
static GColour ConvertWinColour(uint32_t c)
{
	return GColour(GetRValue(c), GetGValue(c), GetBValue(c));
}
#endif

void GColour::OnChange()
{
	// Basic colours
	_LgiColours[L_BLACK].Rgb(0, 0, 0); // LC_BLACK
	_LgiColours[L_DKGREY].Rgb(0x40, 0x40, 0x40); // LC_DKGREY
	_LgiColours[L_MIDGREY].Rgb(0x80, 0x80, 0x80); // LC_MIDGREY
	_LgiColours[L_LTGREY].Rgb(0xc0, 0xc0, 0xc0); // LC_LTGREY
	_LgiColours[L_WHITE].Rgb(0xff, 0xff, 0xff); // LC_WHITE

	// Variable colours
	#if defined _XP_CTRLS

	_LgiColours[L_SHADOW] = Rgb24(0x42, 0x27, 0x63); // LC_SHADOW
	_LgiColours[L_LOW] = Rgb24(0x7a, 0x54, 0xa9); // LC_LOW
	_LgiColours[L_MED] = Rgb24(0xbc, 0xa9, 0xd4); // LC_MED
	_LgiColours[L_HIGH] = Rgb24(0xdd, 0xd4, 0xe9); // LC_HIGH
	_LgiColours[L_LIGHT] = Rgb24(0xff, 0xff, 0xff); // LC_LIGHT
	_LgiColours[L_DIALOG] = Rgb24(0xbc, 0xa9, 0xd4); // LC_DIALOG
	_LgiColours[L_WORKSPACE] = Rgb24(0xeb, 0xe6, 0xf2); // LC_WORKSPACE
	_LgiColours[L_TEXT] = Rgb24(0x35, 0x1f, 0x4f); // LC_TEXT
	_LgiColours[L_FOCUS_SEL_BACK] = Rgb24(0xbf, 0x67, 0x93); // LC_FOCUS_SEL_BACK
	_LgiColours[L_FOCUS_SEL_FORE] = Rgb24(0xff, 0xff, 0xff); // LC_FOCUS_SEL_FORE
	_LgiColours[L_ACTIVE_TITLE] = Rgb24(0x70, 0x3a, 0xec); // LC_ACTIVE_TITLE
	_LgiColours[L_ACTIVE_TITLE_TEXT] = Rgb24(0xff, 0xff, 0xff); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[L_INACTIVE_TITLE] = Rgb24(0x80, 0x80, 0x80); // LC_INACTIVE_TITLE
	_LgiColours[L_INACTIVE_TITLE_TEXT] = Rgb24(0x40, 0x40, 0x40); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[L_MENU_BACKGROUND] = Rgb24(0xbc, 0xa9, 0xd4); // LC_MENU_BACKGROUND
	_LgiColours[L_MENU_TEXT] = Rgb24(0x35, 0x1f, 0x4f); // LC_MENU_TEXT
	_LgiColours[L_NON_FOCUS_SEL_BACK] = Rgb24(0xbc, 0xa9, 0xd4); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[L_NON_FOCUS_SEL_FORE] = Rgb24(0x35, 0x1f, 0x4f); // LC_NON_FOCUS_SEL_FORE
	LgiAssert(i == LC_MAXIMUM);
	
	#elif defined __GTK_H__

	Gtk::GtkSettings *set = Gtk::gtk_settings_get_default();
	if (!set)
	{
		printf("%s:%i - gtk_settings_get_for_screen failed.\n", _FL);
		return;
	}
	
	char PropName[] = "gtk-color-scheme";
	Gtk::gchararray Value = 0;
	Gtk::g_object_get(set, PropName, &Value, NULL);
	GString::Array Lines = GString(Value).SplitDelimit("\n");
	Gtk::g_free(Value);
	g_object_unref(set);

	LHashTbl<ConstStrKey<char,false>, int> Colours(0, -1);
	for (int i=0; i<Lines.Length(); i++)
	{
		char *var = Lines[i];
		char *col = strchr(var, ':');
		if (col)
		{
			*col++ = 0;
			
			char *val = col;
			if (*val == ' ') val++;
			if (*val == '#') val++;
			uint64 c = htoi64(val);
			// printf("%s -> %llX\n", val, c);
			COLOUR c24 = ((c >> 8) & 0xff) |
						((c >> 16) & 0xff00) |
						((c >> 24) & 0xff0000);

			// printf("ParseSysColour %s = %x\n", var, c24);
			Colours.Add(var, c24);
		}
	}
	#define LookupColour(name, default) ((Colours.Find(name) >= 0) ? GColour(Colours.Find(name),24) : default)

	GColour Med = LookupColour("bg_color", GColour(0xe8, 0xe8, 0xe8));
	GColour White(255, 255, 255);
	GColour Black(0, 0, 0);
	GColour Sel(0x33, 0x99, 0xff);
	_LgiColours[L_SHADOW] = GdcMixColour(Med, Black, 0.25); // LC_SHADOW
	_LgiColours[L_LOW] = GdcMixColour(Med, Black, 0.5); // LC_LOW
	_LgiColours[L_MED] = Med; // LC_MED
	_LgiColours[L_HIGH] = GdcMixColour(Med, White, 0.5); // LC_HIGH
	_LgiColours[L_LIGHT] = GdcMixColour(Med, White, 0.25); // LC_LIGHT
	_LgiColours[L_DIALOG] = Med; // LC_DIALOG
	_LgiColours[L_WORKSPACE] = LookupColour("base_color", White); // LC_WORKSPACE
	_LgiColours[L_TEXT] = LookupColour("text_color", Black); // LC_TEXT
	_LgiColours[L_FOCUS_SEL_BACK] = LookupColour("selected_bg_color", Sel); // LC_FOCUS_SEL_BACK
	_LgiColours[L_FOCUS_SEL_FORE] = LookupColour("selected_fg_color", White); // LC_FOCUS_SEL_FORE
	_LgiColours[L_ACTIVE_TITLE] = LookupColour("selected_bg_color", Sel); // LC_ACTIVE_TITLE
	_LgiColours[L_ACTIVE_TITLE_TEXT] = LookupColour("selected_fg_color", White); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[L_INACTIVE_TITLE].Rgb(0xc0, 0xc0, 0xc0); // LC_INACTIVE_TITLE
	_LgiColours[L_INACTIVE_TITLE_TEXT].Rgb(0x80, 0x80, 0x80); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[L_MENU_BACKGROUND] = LookupColour("bg_color", White); // LC_MENU_BACKGROUND
	_LgiColours[L_MENU_TEXT] = LookupColour("text_color", Black); // LC_MENU_TEXT
	_LgiColours[L_NON_FOCUS_SEL_BACK] = GdcMixColour(LookupColour("selected_bg_color", Sel), _LgiColours[11]); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[L_NON_FOCUS_SEL_FORE] = LookupColour("selected_fg_color", White); // LC_NON_FOCUS_SEL_FORE

	#elif defined(WINDOWS)

	/*
	for (int i=0; i<30; i++)
	{
		auto c = GetSysColor(i);
		auto r = GetRValue(c);
		auto g = GetGValue(c);
		auto b = GetBValue(c);
		LgiTrace("[%i]=%i,%i,%i %x,%x,%x\n", i, r, g, b, r, g, b);
	}
	*/

	_LgiColours[L_SHADOW] = ConvertWinColour(GetSysColor(COLOR_3DDKSHADOW)); // LC_SHADOW
	_LgiColours[L_LOW] = ConvertWinColour(GetSysColor(COLOR_3DSHADOW)); // LC_LOW
	_LgiColours[L_MED] = ConvertWinColour(GetSysColor(COLOR_3DFACE)); // LC_MED
	_LgiColours[L_HIGH] = ConvertWinColour(GetSysColor(COLOR_3DLIGHT)); // LC_HIGH
	_LgiColours[L_LIGHT] = ConvertWinColour(GetSysColor(COLOR_3DHIGHLIGHT)); // LC_LIGHT
	_LgiColours[L_DIALOG] = ConvertWinColour(GetSysColor(COLOR_3DFACE)); // LC_DIALOG
	_LgiColours[L_WORKSPACE] = ConvertWinColour(GetSysColor(COLOR_WINDOW)); // LC_WORKSPACE
	_LgiColours[L_TEXT] = ConvertWinColour(GetSysColor(COLOR_WINDOWTEXT)); // LC_TEXT
	_LgiColours[L_FOCUS_SEL_BACK] = ConvertWinColour(GetSysColor(COLOR_HIGHLIGHT)); // LC_FOCUS_SEL_BACK
	_LgiColours[L_FOCUS_SEL_FORE] = ConvertWinColour(GetSysColor(COLOR_HIGHLIGHTTEXT)); // LC_FOCUS_SEL_FORE
	_LgiColours[L_ACTIVE_TITLE] = ConvertWinColour(GetSysColor(COLOR_ACTIVECAPTION)); // LC_ACTIVE_TITLE
	_LgiColours[L_ACTIVE_TITLE_TEXT] = ConvertWinColour(GetSysColor(COLOR_CAPTIONTEXT)); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[L_INACTIVE_TITLE] = ConvertWinColour(GetSysColor(COLOR_INACTIVECAPTION)); // LC_INACTIVE_TITLE
	_LgiColours[L_INACTIVE_TITLE_TEXT] = ConvertWinColour(GetSysColor(COLOR_INACTIVECAPTIONTEXT)); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[L_MENU_BACKGROUND] = ConvertWinColour(GetSysColor(COLOR_MENU)); // LC_MENU_BACKGROUND
	_LgiColours[L_MENU_TEXT] = ConvertWinColour(GetSysColor(COLOR_MENUTEXT)); // LC_MENU_TEXT
	_LgiColours[L_NON_FOCUS_SEL_BACK] = ConvertWinColour(GetSysColor(COLOR_BTNFACE)); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[L_NON_FOCUS_SEL_FORE] = ConvertWinColour(GetSysColor(COLOR_BTNTEXT)); // LC_NON_FOCUS_SEL_FORE

	#else // defaults for non-windows, plain grays

	#if defined(LINUX) && !defined(LGI_SDL)
	WmColour c;
	Proc_LgiWmGetColour WmGetColour = 0;
	GLibrary *WmLib = LgiApp->GetWindowManagerLib();
	if (WmLib)
	{
		WmGetColour = (Proc_LgiWmGetColour) WmLib->GetAddress("LgiWmGetColour");
	}

	#define SetCol(def) \
		if (WmGetColour && WmGetColour(i, &c)) \
			_LgiColours[i++] = Rgb24(c.r, c.g, c.b); \
		else \
			_LgiColours[i++] = def;

	#else // MAC

	#define SetCol(def) \
		_LgiColours[i++] = def;

	#endif

	_LgiColours[L_SHADOW].Rgb(64, 64, 64); // LC_SHADOW
	_LgiColours[L_LOW].Rgb(128, 128, 128); // LC_LOW
	_LgiColours[L_MED].Rgb(216, 216, 216); // LC_MED
	_LgiColours[L_HIGH].Rgb(230, 230, 230); // LC_HIGH
	_LgiColours[L_LIGHT].Rgb(255, 255, 255); // LC_LIGHT
	_LgiColours[L_DIALOG].Rgb(216, 216, 216); // LC_DIALOG
	_LgiColours[L_WORKSPACE].Rgb(0xff, 0xff, 0xff); // LC_WORKSPACE
	_LgiColours[L_TEXT].Rgb(0, 0, 0); // LC_TEXT
	_LgiColours[L_FOCUS_SEL_BACK].Rgb(0x4a, 0x59, 0xa5); // LC_FOCUS_SEL_BACK
	_LgiColours[L_FOCUS_SEL_FORE].Rgb(0xff, 0xff, 0xff); // LC_FOCUS_SEL_FORE
	_LgiColours[L_ACTIVE_TITLE].Rgb(0, 0, 0x80); // LC_ACTIVE_TITLE
	_LgiColours[L_ACTIVE_TITLE_TEXT].Rgb(0xff, 0xff, 0xff); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[L_INACTIVE_TITLE].Rgb(0x80, 0x80, 0x80); // LC_INACTIVE_TITLE
	_LgiColours[L_INACTIVE_TITLE_TEXT].Rgb(0x40, 0x40, 0x40); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[L_MENU_BACKGROUND].Rgb(222, 222, 222); // LC_MENU_BACKGROUND
	_LgiColours[L_MENU_TEXT].Rgb(0, 0, 0); // LC_MENU_TEXT
	_LgiColours[L_NON_FOCUS_SEL_BACK].Rgb(222, 222, 222); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[L_NON_FOCUS_SEL_FORE].Rgb(0, 0, 0); // LC_NON_FOCUS_SEL_FORE
	#endif

	// Tweak
	if (LgiGetOs() == LGI_OS_WIN32
		||
		LgiGetOs() == LGI_OS_WIN64)
	{
		// Win32 doesn't seem to get this right, so we just tweak it here
		_LgiColours[L_LIGHT] = _LgiColours[L_MED].Mix(_LgiColours[L_LIGHT]);
	}
	
	_LgiColours[L_DEBUG_CURRENT_LINE].Rgb(0xff, 0xe0, 0x00);
	#ifdef MAC
	_LgiColours[L_TOOL_TIP].Rgb(0xef, 0xef, 0xef);
	#else
	_LgiColours[L_TOOL_TIP].Rgb(255, 255, 231);
	#endif

	// Read any settings out of config
	ReadColourConfig(L_SHADOW);
	ReadColourConfig(L_LOW);
	ReadColourConfig(L_MED);
	ReadColourConfig(L_HIGH);
	ReadColourConfig(L_LIGHT);
	ReadColourConfig(L_DIALOG);
	ReadColourConfig(L_WORKSPACE);
	ReadColourConfig(L_TEXT);
	ReadColourConfig(L_FOCUS_SEL_BACK);
	ReadColourConfig(L_FOCUS_SEL_FORE);
	ReadColourConfig(L_ACTIVE_TITLE);
	ReadColourConfig(L_ACTIVE_TITLE_TEXT);
	ReadColourConfig(L_INACTIVE_TITLE);
	ReadColourConfig(L_INACTIVE_TITLE_TEXT);
}

GColour::GColour(LSystemColour sc)
{
	if (sc < L_MAXIMUM)
		*this = _LgiColours[sc];
}

GColour LColour(LSystemColour Colour)
{
	#ifndef LGI_STATIC
	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_COLOUR))
	{
		return GApp::SkinEngine->GetColour(Colour);
	}
	#endif
	
	return Colour < L_MAXIMUM ? _LgiColours[Colour] : GColour();
}

COLOUR LgiColour(LSystemColour Colour)
{
	#ifndef LGI_STATIC
	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_COLOUR))
	{
		return GApp::SkinEngine->GetColour(Colour).c24();
	}
	#endif
	
	return Colour < L_MAXIMUM ? _LgiColours[Colour].c24() : 0;
}

