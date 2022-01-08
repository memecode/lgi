#include <stdlib.h>
#include <locale.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/Library.h"
#include "lgi/common/LibraryUtils.h"

#if defined(LGI_STATIC)
#undef HAS_ICONV
#endif

#define DEBUG_ICONV_LOG			0

#if HAS_ICONV
//
// Get 'iconv.h' from http://www.gnu.org/software/libiconv
// Current download at time of writing:
//    http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.8.tar.gz
//
// Then add whatever include path to your project or development
// settings. Otherwise you can build without extended charset
// support by changing the HAS_ICONV define in Lgi.h to '0'
//
// Linux should always build with iconv, on windows you may or 
// may not want to bother depending on what sort of app your
// writing.
//
#include "iconv.h"

#if defined(WIN32)
typedef const char IconvChar;
#else
typedef char IconvChar;
#endif
#endif

/////////////////////////////////////////////////////////////////////
// Private growable class for binary compatability
class LFontSystemPrivate : public LLibrary
{
public:
	bool DefaultGlyphSub;
	int Used, Refs;
	bool FontTableLoaded;
	bool SubSupport;
	bool CheckedConfig;
	bool LibCheck;
	
	#ifdef __GTK_H__
	Gtk::PangoFontMap *Map;
	Gtk::PangoContext *Ctx;
	
	LFontSystemPrivate()
	{
		Map = Gtk::pango_cairo_font_map_get_default();
		if (!Map)
			LAssert(!"pango_cairo_font_map_get_default failed.\n");
		
		Ctx = Gtk::pango_cairo_font_map_create_context((Gtk::PangoCairoFontMap*)Map);
		if (!Ctx)
			LAssert(!"pango_cairo_font_map_create_context failed.\n");
	}
	#endif

	#if HAS_ICONV
	#ifdef WIN32

	DynFunc2(iconv_t,	libiconv_open, const char*, tocode, const char*, fromcode);
	DynFunc5(size_t,	libiconv, iconv_t, cd, IconvChar**, inbuf, size_t*, inbytesleft, char**, outbuf, size_t*, outbytesleft);
	DynFunc1(int,		libiconv_close, iconv_t, cd);

	#elif !defined(MAC)

	// Use glibc I guess
	iconv_t libiconv_open(const char *tocode, const char *fromcode)
	{
		return ::iconv_open(tocode, fromcode);
	}

	size_t libiconv(iconv_t cd, IconvChar** inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
	{
		return ::iconv(cd, inbuf, inbytesleft, outbuf, outbytesleft);
	}

	int libiconv_close(iconv_t cd)
	{
		return ::iconv_close(cd);
	}

	bool IsLoaded()
	{
		return true;
	}

	#endif
	#endif // HAS_ICONV
};

/////////////////////////////////////////////////////////////////////
static bool FontSystemDone = false;
LFontSystem *LFontSystem::Me = 0;
LFontSystem *LFontSystem::Inst()
{
	if (!Me && !FontSystemDone)
		new LFontSystem;
	
	return Me;
}

LFontSystem::LFontSystem()
{
	Me = this;
	d = new LFontSystemPrivate;

	// Glyph sub setup
	int Os = LGetOs();

	d->SubSupport =	(Os == LGI_OS_LINUX) ||
					(Os == LGI_OS_WIN64) ||
					(Os == LGI_OS_WIN32)
					#ifdef __GTK_H__
					|| (Os == LGI_OS_MAC_OS_X)
					#endif
					; //  && Rev == 0);  // WinXP does it's own glyph substitution
	d->DefaultGlyphSub = d->SubSupport;
	d->CheckedConfig = false;
	d->FontTableLoaded = false;
	d->Used = 1;		// the '0th' font spot isn't used
						// because Lut[Char] == 0 means no font
						// available
	d->Refs = 0;
	d->LibCheck = false;
	ZeroObj(Lut);		// Clear the table to 'no font'
	ZeroObj(Font);		// Initialize the list of fonts to empty
}

LFontSystem::~LFontSystem()
{
	// Clean up all our resources
	for (int i=0; i<d->Used; i++)
	{
		DeleteObj(Font[i]);
	}

	DeleteObj(d);
	Me = 0;
	FontSystemDone = true;
}

#ifdef __GTK_H__
Gtk::PangoFontMap *LFontSystem::GetFontMap()
{
	return d->Map;
}

Gtk::PangoContext *LFontSystem::GetContext()
{
	return d->Ctx;
}
#endif

bool LFontSystem::GetGlyphSubSupport()
{
	return d->SubSupport;
}

bool LFontSystem::GetDefaultGlyphSub()
{
	if (!d->CheckedConfig && LAppInst)
	{
		auto GlyphSub = LAppInst->GetConfig("Fonts.GlyphSub");
		if (GlyphSub)
		{
			d->DefaultGlyphSub = atoi(GlyphSub) != 0;
		}
		d->CheckedConfig = true;
	}

	return d->DefaultGlyphSub;
}

void LFontSystem::SetDefaultGlyphSub(bool i)
{
	if (d->SubSupport)
		d->DefaultGlyphSub = i;
}

#ifdef WINNATIVE
int CALLBACK _EnumFonts(ENUMLOGFONT FAR *lpelf,
						NEWTEXTMETRIC FAR *lpntm,
						int FontType,
						LPARAM lParam)
{
	LString::Array *p = (LString::Array*) lParam;
	if (p)
		p->New() = lpelf->elfLogFont.lfFaceName;
	return true;
}
#endif

int StringSort(LString *a, LString *b)
{
	if (a && b) return stricmp(*a, *b);
	return 0;
}

bool LFontSystem::EnumerateFonts(LString::Array &Fonts)
{
	Fonts.SetFixedLength(false);
	if (!AllFonts.Length())
	{
		#if defined WINNATIVE

			HDC hDC = CreateCompatibleDC(NULL);
			if (hDC)
			{
				EnumFontFamilies(	hDC,
									NULL,
									(FONTENUMPROC) _EnumFonts,
									(LPARAM) &AllFonts);

				DeleteDC(hDC);
			}

		#elif defined __GTK_H__

			Gtk::PangoFontFamily ** families;
			int n_families;
			Gtk::PangoFontMap * fontmap;

			fontmap = Gtk::pango_cairo_font_map_get_default();
			Gtk::pango_font_map_list_families (fontmap, & families, & n_families);
			for (int i = 0; i < n_families; i++)
			{
				Gtk::PangoFontFamily * family = families[i];
				const char * family_name;

				family_name = Gtk::pango_font_family_get_name (family);
				AllFonts.New() = family_name;
			}
			Gtk::g_free (families);

		#elif LGI_COCOA

			auto avail = [[NSFontManager sharedFontManager] availableFontFamilies];
			for (NSString *s in avail)
			{
				AllFonts.New() = [s UTF8String];
			}

		#elif LGI_CARBON

			CFArrayRef fontFamilies = CTFontManagerCopyAvailableFontFamilyNames();
			if (fontFamilies)
			{
				for(CFIndex i = 0; i < CFArrayGetCount(fontFamilies); i++)
				{
					CFStringRef fontName = (CFStringRef) CFArrayGetValueAtIndex(fontFamilies, i);
					if (fontName)
						AllFonts.New() = fontName;
				}
				CFRelease(fontFamilies);
			}
		
		#endif

		AllFonts.Sort(StringSort);
	}

	Fonts = AllFonts;
	return true;
}

void LFontSystem::ResetLibCheck()
{
	d->LibCheck = false;
}

bool LFontSystem::HasIconv(bool Quiet)
{
	if (d->IsLoaded())
		return true;

	bool Status = false;
	if (!d->LibCheck)
	{
		d->LibCheck = true;
		
		#ifdef WINDOWS
		auto LibName = "iconv-2." LGI_LIBRARY_EXT;
		#else
		auto LibName = "libiconv." LGI_LIBRARY_EXT;
		#endif
		Status = d->Load(LibName);
		if (!Status && !Quiet)
		{
			if (!NeedsCapability("libiconv"))
			{
				static bool Warn = true;
				if (Warn)
				{
					Warn = false;
					LAssert(!"Iconv is not available");
				}
			}
		}
	}
	
	return Status;
}

// This converts a normal charset to an Apple encoding ID
#if defined LGI_CARBON
	static CFStringEncoding CharsetToEncoding(const char *cs)
	{
		CFStringRef InputCs = CFStringCreateWithCString(0, cs, kCFStringEncodingUTF8);
		if (!InputCs)
			return kCFStringEncodingUTF8; // Um what to do here?
		CFStringEncoding enc = CFStringConvertIANACharSetNameToEncoding(InputCs);
		CFRelease(InputCs);
		return enc;
	}
#elif defined LGI_COCOA
	static CFStringEncoding CharsetToEncoding(const char *cs)
	{
		LString s = cs;
		auto r = s.CreateStringRef();
		auto e = CFStringConvertIANACharSetNameToEncoding(r);
		CFRelease(r);
		return e;
	}
#endif

ssize_t LFontSystem::IconvConvert(const char *OutCs, LStreamI *Out, const char *InCs, const char *&In, ssize_t InLen)
{
	LAssert(InLen > 0);

	if (!Out || !In)
		return 0;
	    
#if defined(MAC)

	char Buf[2 << 10];
	CFStringEncoding InEnc = CharsetToEncoding(InCs);
	CFStringEncoding OutEnc = CharsetToEncoding(OutCs);
	if (InEnc != kCFStringEncodingInvalidId &&
		OutEnc != kCFStringEncodingInvalidId)
	{
		CFStringRef r = CFStringCreateWithBytes(0, (const UInt8 *)In, InLen, InEnc, false);
		if (r)
		{
			CFRange g = { 0, CFStringGetLength(r) };
			CFIndex used = 0;
			CFIndex ret;
			while ((ret = CFStringGetBytes(r, g, OutEnc, '?', false, (UInt8*)Buf, sizeof(Buf), &used)) > 0 && g.length > 0)
			{
				// char16 *b = (char16*)Buf;
				Out->Write(Buf, used);
				g.location += ret;
				g.length -= ret;
			}

			CFRelease(r);
		}
		else return 0;
	}
	else return 0;

#elif HAS_ICONV

	if (!HasIconv(false))
	{
		return 0;
	}

	char Buf[2 << 10] = {0};
	iconv_t Conv;
	if ((NativeInt)(Conv = d->libiconv_open(OutCs, InCs)) >= 0)
	{
		char *i = (char*)In;
		LAssert((NativeInt)Conv != 0xffffffff);

		while (InLen)
		{
			char *o = (char*)Buf;
			size_t OutLen = sizeof(Buf);
			ssize_t OldInLen = InLen;
			size_t InSz = InLen;
			#if DEBUG_ICONV_LOG
				printf("iconv %s,%p,%i->%s,%p,%i",
					InCs, In, (int)InSz,
					OutCs, Out, (int)sizeof(Buf));
			#endif		
			ssize_t s = d->libiconv(Conv, (IconvChar**)&i, &InSz, &o, &OutLen);
			#if DEBUG_ICONV_LOG
				printf(" = %i\n", (int)s);
			#endif

			InLen = InSz;
			Out->Write((uchar*)Buf, sizeof(Buf) - OutLen);
			if (OldInLen == InLen) break;
		}

		d->libiconv_close(Conv);
	}
	else
	{
		LgiTrace("Iconv won't load.\n");
		return 0;
	}

#endif

	return 1;
}

ssize_t LFontSystem::IconvConvert(const char *OutCs, char *Out, ssize_t OutLen, const char *InCs, const char *&In, ssize_t InLen)
{
	ssize_t Status = 0;

    if (!Out || !In || !HasIconv(false))
        return 0;

#if defined(MAC)

	CFStringEncoding InEnc = CharsetToEncoding(InCs);
	CFStringEncoding OutEnc = CharsetToEncoding(OutCs);
	if (InEnc != kCFStringEncodingInvalidId &&
		OutEnc != kCFStringEncodingInvalidId)
	{
		CFStringRef r = CFStringCreateWithBytes(0, (const UInt8 *)In, InLen, InEnc, false);
		if (r)
		{
			CFRange g = { 0, CFStringGetLength(r) };
			CFIndex ret = CFStringGetBytes(r, g, OutEnc, '?', false, (UInt8*)Out, OutLen, 0);
			CFRelease(r);
			return ret;
		}
	}

#elif HAS_ICONV

	// Set locale yet?
	static bool sl = false;
	if (!sl)
	{
		sl = true;
		setlocale(LC_ALL, "");
	}

	// Iconv conversion
	iconv_t Conv;
	if ((Conv = d->libiconv_open(OutCs, InCs)) >= 0)
	{
		size_t InSz = InLen;
		size_t OutSz = OutLen;
		char *o = Out;
		char *i = (char*)In;

		// Convert
		char *Start = o;
		
		#if DEBUG_ICONV_LOG
			printf("iconv %s,%p,%i->%s,%p,%i",
				InCs, In, (int)InSz,
				OutCs, Out, (int)OutSz);
		#endif		
		ssize_t s = d->libiconv(Conv, (IconvChar**)&i, &InSz, &o, &OutSz);
		#if DEBUG_ICONV_LOG
			printf(" = %i\n", (int)s);
		#endif
		
		InLen = InSz;
		OutLen = OutSz;
		d->libiconv_close(Conv);

		In = (const char*)i;
		Status = o - Out;
	}
	else
	{
		LgiTrace("Iconv not present/won't load.\n");
	}

#endif

	return Status;
}

LFont *LFontSystem::GetBestFont(char *Str)
{
	LFont *MatchingFont = 0;

	if (d->SubSupport)
	{
		char16 *s = Utf8ToWide(Str);
		if (s)
		{
			// Make list of possible fonts
			List<LFont> Possibles;
			char16 *i;
			for (i = s; *i; i++)
			{
				LFont *Font = GetGlyph(*i, LSysFont);
				if (Font)
				{
					bool Has = false;
					for (auto h: Possibles)
					{
						if (h == Font)
						{
							Has = true;
							break;
						}
					}
					if (!Has)
					{
						Possibles.Insert(Font);
					}
				}
			}

			// Choose best match amongst possibles
			int MatchingChars = 0;
			for (auto h: Possibles)
			{
				int Chars = 0;
				for (i = s; *i; i++)
				{
					if (h->GetGlyphMap() &&
						_HasUnicodeGlyph(h->GetGlyphMap(), *i))
					{
						Chars++;
					}
				}

				if (!MatchingFont ||
					Chars > MatchingChars)
				{
					MatchingFont = h;
				}
			}

			DeleteArray(s);	
		}
	}

	return MatchingFont;
}

typedef LHashTbl<ConstStrKey<char,false>,int> FontMap;

bool LFontSystem::AddFont(LAutoPtr<LFont> Fnt)
{
	if (!Fnt)
		return false;

	if (d->Used >= CountOf(Font))
		return false;

	Fnt->Create();

	auto *Map = Fnt->GetGlyphMap();
	if (Map)
	{
		uint8_t Used = d->Used;

		// Insert all the characters of this font into the LUT
		// so that we can map from a character back to the font
		for (int k=0; k<=MAX_UNICODE; k += 8)
		{
			// unroll the loop for maximum speed..
			uint8_t m = Map[k >> 3];
			
			#define TestLut(i) \
				if (!Lut[k+i] && (m & (1 << i))) \
					Lut[k+i] = Used;
			TestLut(0);
			TestLut(1);
			TestLut(2);
			TestLut(3);
			TestLut(4);
			TestLut(5);
			TestLut(6);
			TestLut(7);
		}
	}

	Font[d->Used++] = Fnt.Release();
	return true;
}

LFont *LFontSystem::GetGlyph(uint32_t u, LFont *UserFont)
{
	if (u > MAX_UNICODE || !UserFont)
	{
		LAssert(!"Invalid character");
		return 0;
	}

	// Check app font
	if (!d->SubSupport ||
		(UserFont->GetGlyphMap() &&
		_HasUnicodeGlyph(UserFont->GetGlyphMap(), u)))
	{
		return UserFont;
	}

	// Check LUT
	LFont *Has = 0;
	if (Lut[u])
	{
		Has = Font[Lut[u]];
		LAssert(Has != NULL);
		if (!Has)
		{
			LgiTrace("%s:%i - Font table missing pointer. u=%i Lut[u]=%i\n", _FL, u, Lut[u]);
			Has = UserFont;
		}
	}
	else if (d->Used < 255 && !d->FontTableLoaded)
	{
		// Add fonts to Lut...
		if (!SubFonts.Length())
		{
			#if LGI_EXCEPTIONS
			try
			{
			#endif
				FontMap Pref(0, 0);
				if (LFontSystem::Inst()->EnumerateFonts(SubFonts))
				{
					// Reorder font list to prefer certain known as good fonts or
					// avoid certain bad fonts.
					if (LGetOs() == LGI_OS_WIN32 ||
						LGetOs() == LGI_OS_WIN64)
					{
						Pref.Add("Microsoft Sans Serif", 1);
						Pref.Add("Arial Unicode MS", 1);
						Pref.Add("Verdana", 1);
						Pref.Add("Tahoma", 1);

						Pref.Add("Bookworm", -1);
						Pref.Add("Christmas Tree", -1);
						Pref.Add("MingLiU", -1);
					}
					
					if (LGetOs() == LGI_OS_LINUX)
					{
						// Windows fonts are much better than anything Linux 
						// has to offer.
						Pref.Add("Verdana", 1);
						Pref.Add("Tahoma", 1);
						Pref.Add("Arial Unicode MS", 1);
						
						// Most linux fonts suck... and the rest aren't much
						// good either
						Pref.Add("AR PL *", -1);
						Pref.Add("Baekmuk *", -1);
						Pref.Add("console8*", -1);
						Pref.Add("Courier*", -1);
						Pref.Add("Fangsong*", -1);
						Pref.Add("Kochi*", -1);
						Pref.Add("MiscFixed", -1);
						Pref.Add("Serto*", -1);
						Pref.Add("Standard Symbols*", -1);
						Pref.Add("Nimbus*", -1);
					}

					// Prefer these fonts...
					SubFonts.Sort([&Pref](auto a, auto b)
					{
						int ap = Pref.Find(*a);
						int bp = Pref.Find(*b);
						return bp - ap;
					});

					// Delete fonts prefixed with '@' to the end, as they are for
					// vertical rendering... and aren't suitable for what LGI uses
					// fonts for.
					for (unsigned i=0; i<SubFonts.Length(); i++)
					{
						if (SubFonts[i](0) == '@')
							SubFonts.DeleteAt(i--, true);
					}
				}
			#if LGI_EXCEPTIONS
			}
			catch (...)
			{
				LgiTrace("%s:%i - Font enumeration crashed.\n", _FL);
			}
			#endif
		}

		#if LGI_EXCEPTIONS
		try
		{
		#endif
			// Start loading in fonts from the remaining 'SubFonts' container until we find the char we're looking for, 
			// but only for a max of 10ms. This may result in a few missing glyphs but reduces the max time a display
			// string takes to layout
			auto Start = LCurrentTime();
			int Used = d->Used;
			while (SubFonts.Length() > 0 && (LCurrentTime() - Start) < 10)
			{
				LString f = SubFonts[0];
				SubFonts.DeleteAt(0, true);

				if (d->Used >= CountOf(Font))
				{
					// No more space
					SubFonts.Empty();
					break;
				}

				LAutoPtr<LFont> Fnt(new LFont);
				if (Fnt)
				{
					*Fnt.Get() = *UserFont;
					Fnt->Face(f);
					if (AddFont(Fnt))
					{
						LFont *Prev = Font[d->Used - 1];
						auto PrevMap = Prev->GetGlyphMap();
						if (PrevMap && _HasUnicodeGlyph(Prev->GetGlyphMap(), u))
						{
							Has = Prev;
							LAssert(Has != NULL);
							break;
						}
					}
				}
			}

			LgiTrace("Loaded %i fonts for glyph sub.\n", d->Used - Used);

		#if LGI_EXCEPTIONS
		}
		catch (...)
		{
			LgiTrace("%s:%i - Glyph search crashed.\n", _FL);
		}
		#endif

		if (!SubFonts.Length())
		{
			d->FontTableLoaded = true;
		}
	}

	return Has;
}


