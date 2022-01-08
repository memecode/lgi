/*hdr
**	FILE:			LFont.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			5/5/97
**	DESCRIPTION:	Gdc2 Font Support
**
**	Copyright (C) 1997-2002, Matthew Allen
**		fret@memecode.com
*/

#ifndef WIN32
#define _WIN32_WINNT	0x500
#endif

//////////////////////////////////////////////////////////////////////
// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/GdiLeak.h"
#include "lgi/common/Css.h"

#include "FontPriv.h"

#ifdef FontChange
#undef FontChange
#endif

//////////////////////////////////////////////////////////////////////
// Helpers

#if defined(LGI_SDL)

	class FreetypeLib
	{
		FT_Library  lib;
		FT_Error	err;
		
	public:
		FreetypeLib()
		{
			err = FT_Init_FreeType(&lib);
			if (err)
			{
				LAssert(0);
			}
		}
		
		~FreetypeLib()
		{
			if (!err)
			{
				FT_Done_FreeType(lib);
			}
		}
		
		FT_Library Handle()
		{
			return lib;
		}
		
		LString GetVersion()
		{
			FT_Int amajor = 0, aminor = 0, apatch = 0;
			FT_Library_Version(lib, &amajor, &aminor, &apatch);
			LString s;
			s.Printf("%i.%i.%i", amajor, aminor, apatch);
			return s;
		}
		
	} Freetype2;

	LString GetFreetypeLibraryVersion()
	{
		return Freetype2.GetVersion();
	}

#elif defined(WIN32)

	#ifndef __GNUC__
	#include <mbctype.h>
	#endif

	int WinPointToHeight(int Pt, HDC hDC)
	{
		int Ht = 0;

		HWND hDestktop = NULL;
		if (!hDC)
			hDC = GetDC(hDestktop = GetDesktopWindow());
		
		if (hDC)
			Ht = -MulDiv(Pt, GetDeviceCaps(hDC, LOGPIXELSY), 72);

		if (hDestktop)
			ReleaseDC(hDestktop, hDC);

		return Ht;
	}

	int WinHeightToPoint(int Ht, HDC hDC)
	{
		int Pt = 0;

		HWND hDestktop = NULL;
		if (!hDC)
			hDC = GetDC(hDestktop = GetDesktopWindow());
		
		if (hDC)
			Pt = -MulDiv(Ht, 72, GetDeviceCaps(hDC, LOGPIXELSY));

		if (hDestktop)
			ReleaseDC(hDestktop, hDC);

		return Pt;
	}

#elif defined(__GTK_H__)

	#include <pango/pangocairo.h>

#elif USE_CORETEXT

	// CTFontCreateUIFontForLanguage
	// #include <HIToolbox/HITheme.h>
	#include <CoreText/CTFont.h>

#endif

////////////////////////////////////////////////////////////////////
#ifdef WINDOWS
LAutoPtr<LLibrary> LFontPrivate::Gdi32;
#endif

LFont::LFont(const char *face, LCss::Len size)
{
	d = new LFontPrivate;
	if (face && size.IsValid())
	{
		Create(face, size);
	}
}

LFont::LFont(OsFont Handle)
{
	d = new LFontPrivate;
	LFontType Type;
	if (Type.GetFromRef(Handle))
	{
		Create(&Type);
	}
}

LFont::LFont(LFontType &Type)
{
	d = new LFontPrivate;
	Create(&Type);
}

LFont::LFont(LFont &Fnt)
{
	d = new LFontPrivate;
	*this = Fnt;
}

LFont::~LFont()
{
	LAssert(d->WarnOnDelete == false);
	Destroy();
	DeleteObj(d);
}

bool LFont::CreateFromCss(const char *Css)
{
	if (!Css)
		return false;
	
	LCss c;
	c.Parse(Css);
	return CreateFromCss(&c);
}

bool LFont::CreateFromCss(LCss *Css)
{
	if (!Css)
		return false;
	
	LCss::StringsDef Fam = Css->FontFamily();
	if (Fam.Length())
		Face(Fam[0]);
	else
		Face(LSysFont->Face());

	LCss::Len Sz = Css->FontSize();
	switch (Sz.Type)
	{
		case LCss::SizeSmaller:
			Size(LCss::Len(LCss::LenPt, (float)LSysFont->PointSize()-1));
			break;
		case LCss::SizeLarger:
			Size(LCss::Len(LCss::LenPt, (float)LSysFont->PointSize()+1));
			break;
		case LCss::LenInherit:
			Size(LSysFont->Size());
			break;
		default:
			Size(Sz);
			break;
	}

	LCss::FontWeightType w = Css->FontWeight();
	if (w == LCss::FontWeightBold)
		Bold(true);
	
	LCss::FontStyleType s = Css->FontStyle();
	if (s == LCss::FontStyleItalic)
		Italic(true);

	LCss::TextDecorType dec = Css->TextDecoration();
	if (dec == LCss::TextDecorUnderline)
		Underline(true);

	return Create();
}

LString LFont::FontToCss()
{
	LCss c;
	c.FontFamily(Face());
	c.FontSize(Size());
	if (Bold())
		c.FontWeight(LCss::FontWeightBold);
	if (Italic())
		c.FontStyle(LCss::FontStyleItalic);
	auto aStr = c.ToString();
	return aStr.Get();
}

void LFont::WarnOnDelete(bool w)
{
	d->WarnOnDelete = w;
}

bool LFont::Destroy()
{
	bool Status = true;
	
	if (d->hFont)
	{
		#if LGI_SDL
			FT_Done_Face(d->hFont);
		#elif defined(WIN32)
			DeleteObject(d->hFont);
		#elif defined __GTK_H__
			if (d->PangoCtx)
			{
				g_object_unref(d->PangoCtx);
				d->PangoCtx = NULL;
			}
			Gtk::pango_font_description_free(d->hFont);
		#elif defined MAC
			#if USE_CORETEXT
			CFRelease(d->hFont);
			#else
			ATSUDisposeStyle(d->hFont);
			#endif
		#elif defined(HAIKU)
			DeleteObj(d->hFont);
		#else
			LAssert(0);
		#endif
		
		d->hFont = 0;
	}
	DeleteArray(d->GlyphMap);
	
	return Status;
}

#if USE_CORETEXT
CFDictionaryRef LFont::GetAttributes()
{
	return d->Attributes;
}
#endif

uchar *LFont::GetGlyphMap()
{
	return d->GlyphMap;
}

bool LFont::GetOwnerUnderline()
{
	return d->OwnerUnderline;
}

void LFont::_OnPropChange(bool FontChange)
{
	if (FontChange)
	{
		Destroy();
	}
}

OsFont LFont::Handle()
{
	return d->hFont;
}

int LFont::GetHeight()
{
	if (!d->hFont)
	{
		Create();
	}
	
	// I've decided for the moment to allow zero pt fonts to make a HTML test case render correctly.
	// LAssert(d->Height != 0);
	return d->Height;
}

bool LFont::IsValid()
{
	bool Status = false;

	// recreate font
	#ifdef WIN32
	if (!d->hFont)
	{
		Status = Create(Face(), Size());
	}
	#else
	if (d->Dirty)
	{
		Status = Create(Face(), Size());
		d->Dirty = false;
	}
	#endif
	else
	{
		Status = true;
	}

	return Status;
}

#ifdef WIN32
#define ENCODING_TABLE_SIZE 8

class type_4_cmap
{
public:
	uint16 format;
	uint16 length;
	uint16 language;
	uint16 seg_count_x_2;
	uint16 search_range;
	uint16 entry_selector;
	uint16 range_shift;
//	uint16 reserved;
	uint16 arrays[1];

	uint16 SegCount()
	{
		return seg_count_x_2 / 2;
	}

	uint16 *GetIdRangeOffset()
	{
		return arrays + 1 + (SegCount()*3);
	}

	uint16 *GetStartCount()
	{
		return arrays + 1 + SegCount();
	}

	uint16 *GetEndCount()
	{
		/* Apparently the reseved spot is not reserved for 
			 the end_count array!? */
		return arrays;
	}
};

class cmap_encoding_subtable
{
public:
	uint16 platform_id;
	uint16 encoding_id;
	uint32_t offset;
};

#define INT16_SWAP(i)					(	((i)>>8) | (((i)&0xff)<<8) )
#define INT32_SWAP(i)					(	( ((i)&0x000000ff) << 24) |		\
											( ((i)&0x0000ff00) << 8 ) |		\
											( ((i)&0x00ff0000) >> 8 ) |		\
											( ((i)&0xff000000) >> 24) )
#define MAKE_TT_TABLE_NAME(c1, c2, c3, c4) \
	(((uint32_t)c4) << 24 | ((uint32_t)c3) << 16 | ((uint32_t)c2) << 8 | ((uint32_t)c1))
#define CMAP							(MAKE_TT_TABLE_NAME('c','m','a','p'))
#define CMAP_HEADER_SIZE				4
#define APPLE_UNICODE_PLATFORM_ID		0
#define MACINTOSH_PLATFORM_ID			1
#define ISO_PLATFORM_ID					2
#define MICROSOFT_PLATFORM_ID			3
#define SYMBOL_ENCODING_ID				0
#define UNICODE_ENCODING_ID				1
#define UCS4_ENCODING_ID				10

type_4_cmap *GetUnicodeTable(HFONT hFont, uint16_t &Length)
{
	bool Status = false;
	type_4_cmap *Table = 0;

	HDC hDC = GetDC(0);
	if (hDC)
	{
		HFONT Old = (HFONT)SelectObject(hDC, hFont);

		uint16_t Tables = 0;
		uint32_t Offset = 0;

		// Get The number of encoding tables, at offset 2
		auto Res = GetFontData(hDC, CMAP, 2, &Tables, sizeof(uint16_t));
		if (Res == sizeof (uint16_t))
		{
			Tables = INT16_SWAP(Tables);
			cmap_encoding_subtable *Sub = (cmap_encoding_subtable*)malloc(ENCODING_TABLE_SIZE*Tables);
			if (Sub)
			{
				Res = GetFontData(hDC, CMAP, CMAP_HEADER_SIZE, Sub, ENCODING_TABLE_SIZE*Tables);
				if (Res == ENCODING_TABLE_SIZE*Tables)
				{
					for (int i = 0; i < Tables; i++)
					{
						Sub[i].platform_id = INT16_SWAP(Sub[i].platform_id);
						Sub[i].encoding_id = INT16_SWAP(Sub[i].encoding_id);
						Sub[i].offset = INT32_SWAP(Sub[i].offset);

						if (Sub[i].platform_id == MICROSOFT_PLATFORM_ID &&
							Sub[i].encoding_id == UNICODE_ENCODING_ID)
						{
							Offset = Sub[i].offset;
						}
					}
				}
				free(Sub);
			}
		}

		if (Offset)
		{
			Length = 0;
			uint Res = GetFontData(hDC, CMAP, Offset + 2, &Length, sizeof(uint16_t));
			if (Res == sizeof(uint16))
			{
				Length = INT16_SWAP(Length);
				Table = (type_4_cmap*)malloc(Length);
				Res = GetFontData(hDC, CMAP, Offset, Table, Length);
				if (Res == Length)
				{
					Table->format = INT16_SWAP(Table->format);
					Table->length = INT16_SWAP(Table->length);
					Table->language = INT16_SWAP(Table->language);
					Table->seg_count_x_2 = INT16_SWAP(Table->seg_count_x_2);
					Table->search_range = INT16_SWAP(Table->search_range);
					Table->entry_selector = INT16_SWAP(Table->entry_selector);
					Table->range_shift = INT16_SWAP(Table->range_shift);

					if (Table->format == 4)
					{
						uint16 *tbl_end = (uint16 *)((char *)Table + Length);
						uint16 *tbl = Table->arrays;

						while (tbl < tbl_end)
						{
							*tbl = INT16_SWAP(*tbl);
							tbl++;
						}

						Status = true;
					}
				}
			}
		}

		SelectObject(hDC, Old);
		ReleaseDC(0, hDC);
	}

	if (!Status)
	{
		free(Table);
		Table = 0;
	}

	return Table;
}
#endif

LSurface *LFont::GetSurface()
{
	return d->pSurface;
}

bool LFont::Create(const char *face, LCss::Len size, LSurface *pSurface)
{
	bool FaceChanging = false;
	bool SizeChanging = false;
	bool ValidInitFaceSize = ValidStr(Face()) && Size().IsValid();

	if (face)
	{
		if (!Face() || strcmp(Face(), face) != 0)
		{
			FaceChanging = true;
		}
		
		Face(face);
	}
	
	if (size.IsValid())
	{
		SizeChanging = LTypeFace::d->_Size != size;
		LTypeFace::d->_Size = size;
	}
	
	if ((SizeChanging || FaceChanging) && this == LSysFont && ValidInitFaceSize)
	{
		LgiTrace("Warning: Changing sysfont definition.\n");
	}
	
	if (this == LSysFont)
	{
		printf("Setting sysfont up '%s' %i\n", Face(), PointSize());
	}

	#if LGI_SDL
	
		LString FaceName;
		#if defined(WIN32)
		const char *Ext = "ttf";
		LString FontPath = "c:\\Windows\\Fonts";
		#elif defined(LINUX)
		const char *Ext = "ttf";
		LString FontPath = "/usr/share/fonts/truetype";
		#elif defined(MAC)
		const char *Ext = "ttc";
		LString FontPath = "/System/Library/Fonts";
		#else
		#error "Put your font path here"
		#endif
		LFile::Path p = FontPath.Get();
		FaceName.Printf("%s.%s", Face(), Ext);
		p += FaceName;
		LString Full = p.GetFull();
	
		if (!LFileExists(Full))
		{
			LArray<char*> Files;
			LArray<const char*> Extensions;
			LString Pattern;
			Pattern.Printf("*.%s", Ext);
			Extensions.Add(Pattern.Get());
			LRecursiveFileSearch(FontPath, &Extensions, &Files, NULL, NULL, NULL, NULL);
			char *Match = NULL;
			for (unsigned i=0; i<Files.Length(); i++)
			{
				if (stristr(Files[i], FaceName))
					Match = Files[i];
			}
			
			if (Match)
				Full = Match;
			else
				LgiTrace("%s:%i - The file '%s' doesn't exist.\n", _FL, Full.Get());
			Files.DeleteArrays();
		}
	
		FT_Error error = FT_New_Face(Freetype2.Handle(),
									 Full,
									 0,
									 &d->hFont);
		if (error)
		{
			LgiTrace("%s:%i - FT_New_Face failed with %i\n", _FL, error);
		}
		else
		{
			int Dpi = LScreenDpi();
			int PtSize = PointSize();
			int PxSize = (int) (PtSize * Dpi / 72.0);
			
			error = FT_Set_Char_Size(	d->hFont,		/* handle to face object           */
										0,				/* char_width in 1/64th of points  */
										PtSize*64,		/* char_height in 1/64th of points */
										Dpi,			/* horizontal device resolution    */
										Dpi);
			if (error)
			{
				LgiTrace("%s:%i - FT_Set_Char_Size failed with %i\n", _FL, error);
			}
			
			d->Height = (int) (ceil((double)d->hFont->height * PxSize / d->hFont->units_per_EM) + 0.0001);
			LTypeFace::d->_Ascent = (double)d->hFont->ascender * PxSize / d->hFont->units_per_EM;
			LAssert(d->Height > LTypeFace::d->_Ascent);
			LTypeFace::d->_Descent = d->Height - LTypeFace::d->_Ascent;
			
			return true;
		}

	#elif WINNATIVE

		if (d->hFont)
		{
			DeleteObject(d->hFont);
			d->hFont = 0;
		}
	
		d->pSurface = pSurface;
		HDC hDC = pSurface ? pSurface->Handle() : GetDC(0);
		auto Sz = Size();
		int Win32Height = 0;
		if (Sz.Type == LCss::LenPt)
			Win32Height = WinPointToHeight((int)Sz.Value, hDC);
		else if (Sz.Type == LCss::LenPx)
			Win32Height = (int)(Sz.Value * 1.2);
		else
			LAssert(!"What now?");
	
		LTypeFace::d->IsSymbol = LTypeFace::d->_Face &&
									(
										stristr(LTypeFace::d->_Face, "wingdings") ||
										stristr(LTypeFace::d->_Face, "symbol")
									);
		int Cs;
		if (LTypeFace::d->IsSymbol)
			Cs = SYMBOL_CHARSET;
		else
			Cs = ANSI_CHARSET;

		d->OwnerUnderline = Face() &&
							stricmp(Face(), "Courier New") == 0 &&
							Size().Type == LCss::LenPt &&
							(PointSize() == 8 || PointSize() == 9) &&
							LTypeFace::d->_Underline;

		LAutoWString wFace(Utf8ToWide(LTypeFace::d->_Face));
		if (Win32Height)
			d->hFont = ::CreateFont(Win32Height,
									0,
									0,
									0,
									LTypeFace::d->_Weight,
									LTypeFace::d->_Italic,
									d->OwnerUnderline ? false : LTypeFace::d->_Underline,
									false,
									Cs,
									OUT_DEFAULT_PRECIS,
									CLIP_DEFAULT_PRECIS,
									LTypeFace::d->_Quality,
									FF_DONTCARE,
									wFace);
	
		if (d->hFont)
		{
			HFONT hFnt = (HFONT) SelectObject(hDC, d->hFont);

			TEXTMETRIC tm;
			ZeroObj(tm);
			if (GetTextMetrics(hDC, &tm))
			{
				d->Height = tm.tmHeight;
				LTypeFace::d->_Ascent = tm.tmAscent;
				LTypeFace::d->_Descent = tm.tmDescent;
				LTypeFace::d->_Leading = tm.tmInternalLeading;
			}
			else
			{
				SIZE Size = {0, 0};
				GetTextExtentPoint32(hDC, L"Ag", 2, &Size);
				d->Height = Size.cy;
			}

			uint Bytes = (MAX_UNICODE + 1) >> 3;
			if (!d->GlyphMap)
			{
				d->GlyphMap = new uchar[Bytes];
			}

			if (d->GlyphMap)
			{
				memset(d->GlyphMap, 0, Bytes);

				LArray<int> OsVer;
				int OsType = LGetOs(&OsVer);
				if (OsType == LGI_OS_WIN9X ||
					OsVer[0] < 5) // GetFontUnicodeRanges is supported on >= Win2k
				{
					bool HideUnihan = false;

					LAssert(sizeof(type_4_cmap)==16);
					uint16 Length = 0;
					type_4_cmap *t = GetUnicodeTable(Handle(), Length);
					if (t)
					{
						uint16 SegCount = t->seg_count_x_2 / 2;
						uint16 *EndCount = t->GetEndCount();
						uint16 *StartCount = t->GetStartCount();
						uint16 *IdRangeOffset = t->GetIdRangeOffset();

						for (int i = 0; i<SegCount; i++)
						{
							if (IdRangeOffset[i] == 0)
							{
								for (uint u = StartCount[i]; u <= EndCount[i]; u++)
								{
									if (HideUnihan &&
										u >= 0x3400 &&
										u <= 0x9FAF)
									{
										// APPROXIMATE
									}
									else
									{
										// EXACT
									}

									if ((u >> 3) < Bytes)
									{
										d->GlyphMap[u>>3] |= 1 << (u & 7);
									}
								}
							}
							else
							{
								uint16 *End = (uint16*) (((char*)t)+Length);
								ssize_t IdBytes = End - IdRangeOffset;

								for (uint u = StartCount[i]; u <= EndCount[i] && IdRangeOffset[i] != 0xffff; u++)
								{
									uint id = *(IdRangeOffset[i]/2 + (u - StartCount[i]) + &IdRangeOffset[i]);
									if (id)
									{
										if (HideUnihan &&
											u >= 0x3400 &&
											u <= 0x9FAF)
										{
											// APPROXIMATE
										}
										else
										{
											// EXACT
										}

										if ((u >> 3) < Bytes)
										{
											d->GlyphMap[u>>3] |= 1 << (u & 7);
										}
									}
								}
							}
						}
					}
					else
					{
						// not a TTF? assume that it can handle 00-ff in the current ansi cp
						/*
						char *Cp = LAnsiToLgiCp();
						for (int i=0; i<=0x7f; i++)
						{
							char16 u;
							uchar c = i;
							void *In = &c;
							int Size = sizeof(c);
							LBufConvertCp(&u, "ucs-2", sizeof(u), In, Cp, Size);

							if ((u >> 3) < Bytes)
							{
								GlyphMap[u>>3] |= 1 << (u & 7);
							}
						}
						*/
					}
				}
				else
				{
					typedef DWORD (WINAPI *Proc_GetFontUnicodeRanges)(HDC, LPGLYPHSET);

					if (!d->Gdi32)
						d->Gdi32.Reset(new LLibrary("Gdi32"));
					if (d->Gdi32)
					{
						Proc_GetFontUnicodeRanges GetFontUnicodeRanges = (Proc_GetFontUnicodeRanges)d->Gdi32->GetAddress("GetFontUnicodeRanges");
						if (GetFontUnicodeRanges)
						{
							DWORD BufSize = GetFontUnicodeRanges(hDC, 0);
							LPGLYPHSET Set = (LPGLYPHSET) new char[BufSize];
							if (Set && GetFontUnicodeRanges(hDC, Set) > 0)
							{
								for (DWORD i=0; i<Set->cRanges; i++)
								{
									WCRANGE *Range = Set->ranges + i;
									for (int n=0; n<Range->cGlyphs; n++)
									{
										DWORD u = Range->wcLow + n;
										if (u >> 3 < Bytes)
										{
											d->GlyphMap[u>>3] |= 1 << (u & 7);
										}
									}
								}
							}

							DeleteArray((char*&)Set);
						}
					}
					
					if (LTypeFace::d->IsSymbol)
					{
						// Lies! It's all Lies! Symbol doesn't support non-breaking space.
						int u = 0xa0;
						d->GlyphMap[u >> 3] &= ~(1 << (u & 7));
					}
				}

				if (d->GlyphMap)
				{
					memset(d->GlyphMap, 0xff, 128/8);
				}
			}

			SelectObject(hDC, hFnt);
		}

		if (!pSurface) ReleaseDC(0, hDC);

		return (d->hFont != 0);
	
	#elif defined __GTK_H__
	
		Destroy();
	
		d->hFont = Gtk::pango_font_description_new();
		if (!d->hFont)
			printf("%s:%i - pango_font_description_new failed: Face='%s' Size=%i Bold=%i Italic=%i\n",
				_FL, Face(), PointSize(), Bold(), Italic());
		else if (!ValidStr(Face()))
			printf("%s:%i - No font face.\n", _FL);
		else if (!Size().IsValid())
			printf("%s:%i - Invalid size.\n", _FL);
		else
		{
			auto Sz = Size();
			LString sFace = Face();
			Gtk::pango_font_description_set_family(d->hFont, sFace);
			if (Sz.Type == LCss::LenPt)
				Gtk::pango_font_description_set_size(d->hFont, Sz.Value * PANGO_SCALE);
			else if (Sz.Type == LCss::LenPx)
				Gtk::pango_font_description_set_absolute_size(d->hFont, Sz.Value * PANGO_SCALE);
			else
			{
				LAssert(0);
				return false;
			}
			
			if (Bold())
				Gtk::pango_font_description_set_weight(d->hFont, Gtk::PANGO_WEIGHT_BOLD);
			
			// Get metrics for this font...
			Gtk::GtkPrintContext *PrintCtx = pSurface ? pSurface->GetPrintContext() : NULL;
			Gtk::PangoContext *SysCtx = LFontSystem::Inst()->GetContext();
			if (PrintCtx)
				d->PangoCtx = gtk_print_context_create_pango_context(PrintCtx);
			auto EffectiveCtx = d->PangoCtx ? d->PangoCtx : SysCtx;
			Gtk::PangoFontMetrics *m = Gtk::pango_context_get_metrics(d->PangoCtx ? d->PangoCtx : SysCtx, d->hFont, 0);
			if (!m)
				printf("pango_font_get_metrics failed.\n");
			else
			{
				LTypeFace::d->_Ascent = (double)Gtk::pango_font_metrics_get_ascent(m) / PANGO_SCALE;
				LTypeFace::d->_Descent = (double)Gtk::pango_font_metrics_get_descent(m) / PANGO_SCALE;
				d->Height = ceil(LTypeFace::d->_Ascent + LTypeFace::d->_Descent + 1/*hack the underscores to work*/);
				
				#if 0
				if (PrintCtx)
				{
					LgiTrace("LFont::Create %s,%f (%i,%i,%i) (%.1f,%.1f,%i)\n",
						Gtk::pango_font_description_get_family(d->hFont),
						(double)Gtk::pango_font_description_get_size(d->hFont) / PANGO_SCALE,
						Gtk::pango_font_metrics_get_ascent(m),
						Gtk::pango_font_metrics_get_descent(m),
						PANGO_SCALE,
						LTypeFace::d->_Ascent,
						LTypeFace::d->_Descent,
						d->Height);
				}
				#endif

				Gtk::pango_font_metrics_unref(m);

				#if 1
				Gtk::PangoFont *fnt = pango_font_map_load_font(Gtk::pango_cairo_font_map_get_default(), EffectiveCtx, d->hFont);
				if (fnt)
				{
					Gtk::PangoCoverage *c = Gtk::pango_font_get_coverage(fnt, Gtk::pango_language_get_default());
					if (c)
					{
						uint Bytes = (MAX_UNICODE + 1) >> 3;
						if ((d->GlyphMap = new uchar[Bytes]))
						{
							memset(d->GlyphMap, 0, Bytes);

							int Bits = Bytes << 3;
							for (int i=0; i<Bits; i++)
							{
								if (pango_coverage_get(c, i))
									d->GlyphMap[i>>3] |= 1 << (i & 0x7);
							}
						}
						Gtk::pango_coverage_unref(c);
					}
					
					g_object_unref(fnt);
				}
				#endif
				
				return true;
			}
		}
	
	#elif defined MAC
	
		Destroy();
		
		if (this == LSysFont)
			LgiTrace("%s:%i - WARNING: you are re-creating the system font... this is bad!!!!\n", _FL);

		if (Face())
		{
			if (d->Attributes)
				CFRelease(d->Attributes);

			auto Sz = Size();
			LString sFamily(Face());
			LString sBold("Bold");
			LArray<CFStringRef> key;
			LArray<CFTypeRef> values;
			key.Add(kCTFontFamilyNameAttribute);
			values.Add(sFamily.CreateStringRef());

			if (!values[0])
				return false;

			if (Bold())
			{
				key.Add(kCTFontStyleNameAttribute);
				values.Add(sBold.CreateStringRef());
			}

			CFDictionaryRef FontAttrD = CFDictionaryCreate(	kCFAllocatorDefault,
															(const void**)key.AddressOf(),
															(const void**)values.AddressOf(),
															key.Length(),
															&kCFTypeDictionaryKeyCallBacks,
															&kCFTypeDictionaryValueCallBacks);
			if (FontAttrD)
			{
				CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(FontAttrD);
				if (descriptor)
				{
					float PtSz = 0.0;
					if (Sz.Type == LCss::LenPt)
						PtSz = Sz.Value;
					else if (Sz.Type == LCss::LenPx)
// This seems to give fonts that are too small:
//						PtSz = Sz.Value * 72.0f / LScreenDpi();
						PtSz = Sz.Value;
					else
						LAssert(!"Impl me.");
					
					d->hFont = CTFontCreateWithFontDescriptor(descriptor, PtSz, NULL);
					CFRelease(descriptor);
				}
				else LAssert(0);
				
				CFRelease(FontAttrD);
			}
			else
			{
				LAssert(0);
				return false;
			}
			
			for (size_t i=0; i<values.Length(); i++)
				CFRelease(values[i]);
			key.Length(0);
			values.Length(0);
			
			if (d->hFont)
			{
				LTypeFace::d->_Ascent = CTFontGetAscent(d->hFont);
				LTypeFace::d->_Descent = CTFontGetDescent(d->hFont);
				LTypeFace::d->_Leading = CTFontGetLeading(d->hFont);
				d->Height = ceil(LTypeFace::d->_Ascent +
								 LTypeFace::d->_Descent +
								 LTypeFace::d->_Leading);
				
				#if 0
				if (Sz.Type == LCss::LenPx)
				{
					LStringPipe p;
					Sz.ToString(p);
					LgiTrace("%s:%i - LFont::Create(%s,%s) = %f,%f,%f (%i)\n",
							_FL,
							Face(), p.NewGStr().Get(),
							LTypeFace::d->_Ascent,
							LTypeFace::d->_Descent,
							LTypeFace::d->_Leading,
							GetHeight());
				}
				#endif
				
				key.Add(kCTFontAttributeName);
				values.Add(d->hFont);
				key.Add(kCTForegroundColorFromContextAttributeName);
				values.Add(kCFBooleanTrue);

				if (Underline())
				{
					key.Add(kCTUnderlineStyleAttributeName);
					CTUnderlineStyle u = kCTUnderlineStyleSingle;
					values.Add(CFNumberCreate(NULL, kCFNumberSInt32Type, &u));
				}
				
				d->Attributes = CFDictionaryCreate(	kCFAllocatorDefault,
													(const void**)key.AddressOf(),
													(const void**)values.AddressOf(),
													key.Length(),
													&kCFTypeDictionaryKeyCallBacks,
													&kCFTypeDictionaryValueCallBacks);
				
				return true;
			}
			
			for (int i=2; i<values.Length(); i++)
				CFRelease(values[i]);
		}

	#elif defined(HAIKU)

		d->hFont = new BFont();
		d->hFont->SetSize(PointSize());
		status_t r = d->hFont->SetFamilyAndStyle(Face(), "Regular");
		// printf("SetFamilyAndFace(%s)=%i\n", Face(), r);
		if (r == B_OK)
		{
			font_height height;
			d->hFont->GetHeight(&height);
			d->Height = height.ascent + height.descent + height.leading;
			return true;
		}

	#endif

	return false;
}

char16 *LFont::_ToUnicode(char *In, ssize_t &Len)
{
	char16 *WStr = 0;
	
	if (In &&
		Len > 0)
	{
		WStr = (char16*)LNewConvertCp(LGI_WideCharset, In, LTypeFace::d->_CodePage, Len);
		if (WStr)
		{
			ssize_t l = StrlenW(WStr);
			if (l < Len)
			{
				Len = l;
			}
		}
	}

	return WStr;
}

#if defined WINNATIVE

bool LFont::Create(LFontType *LogFont, LSurface *pSurface)
{
	if (d->hFont)
	{
		DeleteObject(d->hFont);
		d->hFont = 0;
	}

	if (LogFont)
	{
		// set props
		PointSize(WinHeightToPoint(LogFont->Info.lfHeight));
		LString uFace = LogFont->Info.lfFaceName;
		if (ValidStr(uFace))
		{
			Face(uFace);
			Quality(LogFont->Info.lfQuality);
			Bold(LogFont->Info.lfWeight >= FW_BOLD);
			Italic(LogFont->Info.lfItalic != FALSE);
			Underline(LogFont->Info.lfUnderline != FALSE);

			// create the handle
			Create(0, 0, pSurface);
		}
	}

	return (d->hFont != 0);
}

#else

bool LFont::Create(LFontType *LogFont, LSurface *pSurface)
{
	if (LogFont)
	{
		LCss::Len Sz(LCss::LenPt, (float)LogFont->GetPointSize());
		return Create(LogFont->GetFace(), Sz, pSurface);
	}

	return false;
}
#endif

LFont &LFont::operator =(LFont &f)
{
	Face(f.Face());
	Size(f.Size());
	TabSize(f.TabSize());
	Quality(f.Quality());
	Fore(f.Fore());
	Back(f.Back());
	SetWeight(f.GetWeight());
	Italic(f.Italic());
	Underline(f.Underline());
	Transparent(f.Transparent());
	
	return *this;
}

char16 WinSymbolToUnicode[256] = 
{
    /*   0 to  15 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*  16 to  31 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*  32 to  47 */ 32, 9998, 9986, 9985, 0, 0, 0, 0, 9742, 9990, 9993, 9993, 0, 0, 0, 0,
    /*  48 to  63 */ 0, 0, 0, 0, 0, 0, 8987, 9000, 0, 0, 0, 0, 0, 0, 9991, 9997,
    /*  64 to  79 */ 9997, 9996, 0, 0, 0, 9756, 9758, 9757, 9759, 0, 9786, 9786, 9785, 0, 9760, 0,
    /*  80 to  95 */ 0, 9992, 9788, 0, 10052, 10014, 10014, 10013, 10016, 10017, 9770, 9775, 2384, 9784, 9800, 9801,
    /*  96 to 111 */ 9802, 9803, 9804, 9805, 9806, 9807, 9808, 9809, 9810, 9811, 38, 38, 9679, 10061, 9632, 9633,
    /* 112 to 127 */ 9633, 10065, 10066, 9674, 9674, 9670, 10070, 9670, 8999, 9043, 8984, 10048, 10047, 10077, 10078, 0,
    /* 128 to 143 */ 9450, 9312, 9313, 9314, 9315, 9316, 9317, 9318, 9319, 9320, 9321, 0, 10102, 10103, 10104, 10105,
    /* 144 to 159 */ 10106, 10107, 10108, 10109, 10110, 10111, 10087, 9753, 9753, 10087, 10087, 9753, 9753, 10087, 8226, 9679,
    /* 160 to 175 */ 160, 9675, 9675, 9675, 9737, 9737, 10061, 9642, 9633, 0, 10022, 9733, 10038, 10039, 10040, 10037,
    /* 176 to 191 */ 0, 0, 10023, 0, 65533, 10026, 10032, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 192 to 207 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10086, 10086, 10086,
    /* 208 to 223 */ 10086, 10086, 10086, 10086, 10086, 9003, 8998, 0, 10146, 0, 0, 0, 10162, 0, 0, 0,
    /* 224 to 239 */ 0, 0, 0, 0, 0, 0, 0, 0, 10132, 0, 0, 0, 0, 0, 0, 8678,
    /* 240 to 255 */ 8680, 8679, 8681, 8660, 8661, 8662, 8663, 8665, 8664, 0, 0, 10007, 10003, 9746, 9745, 0,
};

LAutoString LFont::ConvertToUnicode(char16 *Input, ssize_t Len)
{
	LAutoString a;
	
	if (LTypeFace::d->IsSymbol)
	{
		// F***ing wingdings.
		if (Input)
		{
			LStringPipe p(256);
			if (Len < 0)
				Len = StrlenW(Input);
			char16 *c = Input, *e = Input + Len;
			while (c < e)
			{
				if (*c < 256 && WinSymbolToUnicode[*c])
				{
					p.Write(WinSymbolToUnicode + *c, sizeof(char16));
				}
				else
				{
					p.Write(c, sizeof(char16));
				}
				c++;
			}
			
			LAutoWString w(p.NewStrW());
			a.Reset(WideToUtf8(w));
		}
	}
	else
	{
		// Normal utf-8 text...
		a.Reset(WideToUtf8(Input, Len));
	}
	
	return a;
}

#if WINNATIVE
#include "lgi/common/FontSelect.h"
#include "lgi/common/GdiLeak.h"

/////////////////////////////////////////////////////////////////////////////
void LFont::_Measure(int &x, int &y, OsChar *Str, int Len)
{
	if (!Handle())
	{
		x = 0;
		y = 0;
		return;
	}

	HDC hDC = GetSurface() ? GetSurface()->Handle() : CreateCompatibleDC(0);
	HFONT hOldFont = (HFONT) SelectObject(hDC, Handle());

	SIZE Size;
	if (GetTextExtentPoint32W(hDC, Str, Len, &Size))
	{
		x = Size.cx;
		y = Size.cy;
	}
	else
	{
		x = y = 0;
	}

	SelectObject(hDC, hOldFont);
	if (!GetSurface())
		DeleteDC(hDC);
}

int LFont::_CharAt(int x, OsChar *Str, int Len, LPxToIndexType Type)
{
	if (!Handle())
		return -1;

	INT Fit = 0;
	HDC hDC = CreateCompatibleDC(GetSurface()?GetSurface()->Handle():0);
	HFONT hOldFont = (HFONT) SelectObject(hDC, Handle());
	if (hOldFont)
	{
		SIZE Size = {0, 0};

		if (!GetTextExtentExPointW(hDC, Str, Len, x, &Fit, 0, &Size))
		{
			DWORD e = GetLastError();
			Fit = -1;
		}
		else if (Type == LgiNearest && Fit < Len)
		{
			// Check if the next char is nearer...
			SIZE Prev, Next;
			if (GetTextExtentPoint32W(hDC, Str, Fit, &Prev) &&
				GetTextExtentPoint32W(hDC, Str, Fit + 1, &Next))
			{
				int PrevDist = abs(Prev.cx - x);
				int NextDist = abs(Next.cx - x);
				if (NextDist <= PrevDist)
					Fit++;
			}
		}

		SelectObject(hDC, hOldFont);
	}
	else
	{
		DWORD e = GetLastError();
		Fit = -1;
		LAssert(0);
	}

	DeleteDC(hDC);
	return Fit;
}

void LFont::_Draw(LSurface *pDC, int x, int y, OsChar *Str, int Len, LRect *r, LColour &fore)
{
	if (!Handle())
		return;

	HDC hDC = pDC->StartDC();
	HFONT hOldFont = (HFONT) SelectObject(hDC, Handle());
	if (hOldFont)
	{
		bool IsTransparent = Transparent();
		
		SetTextColor(hDC, fore.GetNative());
		if (!IsTransparent)
			SetBkColor(hDC, Back().GetNative());
		SetBkMode(hDC, IsTransparent ? TRANSPARENT : OPAQUE);

		SIZE Size;
		if ((!IsTransparent && !r)
			||
			(GetOwnerUnderline()))
		{
			GetTextExtentPoint32W(hDC, Str, Len, &Size);
		}

		if (Transparent() && !r)
		{
			TextOutW(hDC, x, y, Str, Len);
		}
		else
		{
			RECT rc;
			if (r)
				rc = *r;
			else
			{
				rc.left = x;
				rc.top = y;
				rc.right = x + Size.cx;
				rc.top = y + Size.cy;
			}

			/* Debugging stuff...
			POINT _ori;
			auto res = GetWindowOrgEx(hDC, &_ori);
			RECT _rc;
			int res2 = GetClipBox(hDC, &_rc);
			auto Addr = (*pDC)[y - _ori.y + 6] + ((x - _ori.x + 4) * pDC->GetBits() / 8);
			*/
			
			ExtTextOutW(hDC, x, y, ETO_CLIPPED | (Transparent()?0:ETO_OPAQUE), &rc, Str, Len, 0);
		}

		if (GetOwnerUnderline())
		{
			pDC->Colour(fore);
			pDC->Line(x, y + GetHeight() - 1, x + Size.cx + 1, y + GetHeight() - 1);
		}

		HANDLE h = SelectObject(hDC, hOldFont);
	}
	else
	{
		DWORD e = GetLastError();
		LAssert(0);
	}
	pDC->EndDC();

}

#else

void LFont::_Measure(int &x, int &y, OsChar *Str, int Len)
{
}

int LFont::_CharAt(int x, OsChar *Str, int Len, LPxToIndexType Type)
{
	return -1;
}

void LFont::_Draw(LSurface *pDC, int x, int y, OsChar *Str, int Len, LRect *r, LColour &fore)
{
}

#endif

