#pragma once

#include "lgi/common/App.h"
#include "lgi/common/Font.h"
#include "lgi/common/LgiCommon.h"

class LFontCache
{
	bool allocIds = false;
	LFont *DefaultFont = nullptr;
	LSurface *DrawContext = nullptr;
	LArray<LFont*> Fonts;
	LHashTbl<IntKey<size_t>, LFont*> idMap;
	LHashTbl<ConstStrKey<char>, LString> FontName;
	
public:
	/// Constructor for font cache
	LFontCache
	(
		/// This is an externally owned default font... or optionally 
		/// NULL if there is no default.
		LFont *DefFnt = nullptr,
		LSurface *Context = nullptr
	)
	{
		DefaultFont = DefFnt;
		DrawContext = Context;
	}
	
	~LFontCache()
	{
		Fonts.DeleteObjects();
	}

	LFont *GetDefaultFont()
	{
		return DefaultFont;
	}

	void SetDefaultFont(LFont *Def)
	{
		DefaultFont = Def;
	}
	
	/// This defines a text label that links to a font-face. On multi-platform
	/// software sometimes you need to have one CSS font Label that links to 
	/// different available fonts.
	void DefineFontName(const char *Label, const char *FontFace)
	{
		LString s = FontName.Find(Label);
		if (!s.Get())
			FontName.Add(Label, LString(FontFace));
	}
	
	/// Turn on/off ID allocation for each font:
	void SetAllocIds(bool alloc)
	{
		allocIds = alloc;
	}
	
	/// Get the font by id:
	LFont *FontFromId(size_t id)
	{
		LAssert(allocIds); // it's not going to work otherwise
		return idMap.Find(id);
	}
	
	/// Delete by id:
	bool DeleteById(size_t id)
	{
		LAssert(allocIds); // it's not going to work otherwise
		auto f = idMap.Find(id);
		if (!f)
			return false;
		
		idMap.Delete(id);
		Fonts.Delete(f);
		delete f;
		return true;
	}
	
	/// Get a font object by characteristics:
	LFont *AddFont(	const char *Face,
					LCss::Len Size,
					LCss::FontWeightType Weight,
					LCss::FontStyleType Style,
					LCss::TextDecorType Decor)
	{
		// Matching existing fonts...
		for (unsigned i=0; i<Fonts.Length(); i++)
		{
			auto f = Fonts[i];
			if
			(
				!Stricmp(f->Face(), Face) &&
				f->Size() == Size &&
				f->Bold() == (Weight == LCss::FontWeightBold) &&
				f->Italic() == (Style == LCss::FontStyleItalic) &&
				f->Underline() == (Decor == LCss::TextDecorUnderline)
			)
				return f;
		}
		
		// No matching font... create a new one
		auto f = new LFont;
		if (f)
		{
			f->Bold(Weight == LCss::FontWeightBold);
			f->Italic(Style == LCss::FontStyleItalic);
			f->Underline(Decor == LCss::TextDecorUnderline);
			
			auto Sz = Size;
			if (Sz.Type == LCss::SizeLarger)
			{
				Sz = LSysFont->Size();
				Sz.Value++;
			}
			else if (Sz.Type == LCss::SizeSmaller)
			{
				Sz = LSysFont->Size();
				Sz.Value--;
			}

			if (!f->Create(Face, Sz, DrawContext))
			{
				LAssert(0);
				DeleteObj(f);
				return NULL;
			}
			
			Fonts.Add(f);
			
			if (allocIds)
			{
				int id;
				while (auto f = idMap.Find(id = LRand(10000)))
					;
				
				idMap.Add(id, f);
				f->SetId(id);
			}
		}
		
		return f;
	}

	LFont *GetFont(LCss *Style)
	{
		if (!Style || !DefaultFont)
			return DefaultFont;
		
		#if 0
		printf("%s:%i - GetFont() %i %s\n",
			_FL,
			(int)Style->Length(),
			Style->ToLString().Get());
		#endif
		
		auto Fam = Style->FontFamily();
		bool FamHasDefFace = false;
		for (unsigned i=0; i<Fam.Names.Length(); i++)
		{
			LString s = FontName.Find(Fam.Names[i]);
			if (s.Get())
			{
				// Resolve label here...
				Fam.Names[i] = s;
			}
			
			if (DefaultFont && Fam.Names[i])
			{
				FamHasDefFace |= !_stricmp(DefaultFont->Face(), Fam.Names[i]);
			}
		}
		if (!FamHasDefFace)
		{
			// Try and figure out what the caller wants from the
			for (auto t: Fam.Generic)
			{
				switch (t)
				{
					case LCss::FontFamilySerif:
					case LCss::FontFamilyUiSerif:
						break;
					case LCss::FontFamilySansSerif:
					case LCss::FontFamilyUiSansSerif:
						break;
					case LCss::FontFamilyUiMonospace:
					case LCss::FontFamilyMonospace:
					{
						LFontType type;
						if (type.GetSystemFont("Fixed"))
							Fam.Names.New() = type.GetFace();
						break;
					}
					case LCss::FontFamilyCursive:
						break;
					case LCss::FontFamilyFantasy:
						break;
					case LCss::FontFamilySystemUi:
						Fam.Names.New() = DefaultFont->Face();
						break;
					case LCss::FontFamilyUiRounded:
						break;
					default:
					case LCss::FontFamilyEmoji:
					case LCss::FontFamilyMath:
					case LCss::FontFamilyFangsong:
						break;
				}
			}
			
			if (!Fam.Names.Length())
				// Fall back to the default font name
				Fam.Names.New() = DefaultFont->Face();
		}
		
		LCss::Len Sz = Style->FontSize();
		if (!Sz)
			Sz = DefaultFont->Size();
		LCss::FontWeightType Weight = Style->FontWeight();
		LCss::FontWeightType DefaultWeight = DefaultFont && DefaultFont->Bold() ?
											LCss::FontWeightBold :
											LCss::FontWeightNormal;
		LCss::FontStyleType FontStyle = Style->FontStyle();
		LCss::TextDecorType Decor = Style->TextDecoration();

		LFont *f = NULL;
		for (unsigned i = 0; !f && i<Fam.Names.Length(); i++)
		{		
			f = AddFont(Fam.Names[i],
						Sz,
						Weight != LCss::FontWeightInherit ? Weight : DefaultWeight,
						FontStyle != LCss::FontStyleInherit ? FontStyle : LCss::FontStyleNormal,
						Decor != LCss::TextDecorInherit ? Decor : LCss::TextDecorNone);
		}

		return f;
	}
};

