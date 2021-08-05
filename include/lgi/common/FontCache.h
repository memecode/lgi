#pragma once

#include "lgi/common/App.h"
#include "lgi/common/Font.h"

class LFontCache
{
	LFont *DefaultFont;
	LArray<LFont*> Fonts;
	LHashTbl<ConstStrKey<char>, LString> FontName;
	
public:
	/// Constructor for font cache
	LFontCache
	(
		/// This is an externally owned default font... or optionally 
		/// NULL if there is no default.
		LFont *DefFnt = NULL
	)
	{
		DefaultFont = DefFnt;
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
	
	LFont *AddFont(	const char *Face,
					LCss::Len Size,
					LCss::FontWeightType Weight,
					LCss::FontStyleType Style,
					LCss::TextDecorType Decor)
	{
		// Matching existing fonts...
		for (unsigned i=0; i<Fonts.Length(); i++)
		{
			LFont *f = Fonts[i];
			if
			(
				f->Face() &&
				Face &&
				!_stricmp(f->Face(), Face) &&
				f->Size() == Size &&
				f->Bold() == (Weight == LCss::FontWeightBold) &&
				f->Italic() == (Style == LCss::FontStyleItalic) &&
				f->Underline() == (Decor == LCss::TextDecorUnderline)
			)
				return f;
		}
		
		// No matching font... create a new one
		LFont *f = new LFont;
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

			if (!f->Create(Face, Sz))
			{
				LAssert(0);
				DeleteObj(f);
				return NULL;
			}
			
			Fonts.Add(f);
		}
		
		return f;
	}

	LFont *GetFont(LCss *Style)
	{
		if (!Style || !DefaultFont)
			return DefaultFont;
		
		LCss::StringsDef Fam = Style->FontFamily();
		bool FamHasDefFace = false;
		for (unsigned i=0; i<Fam.Length(); i++)
		{
			LString s = FontName.Find(Fam[i]);
			if (s.Get())
			{
				// Resolve label here...
				DeleteArray(Fam[i]);
				Fam[i] = NewStr(s);
			}
			
			if (DefaultFont && Fam[i])
			{
				FamHasDefFace |= !_stricmp(DefaultFont->Face(), Fam[i]);
			}
		}
		if (!FamHasDefFace)
			Fam.Add(NewStr(DefaultFont->Face()));
		
		LCss::Len Sz = Style->FontSize();
		if (!Sz.IsValid())
			Sz = DefaultFont->Size();
		LCss::FontWeightType Weight = Style->FontWeight();
		LCss::FontWeightType DefaultWeight = DefaultFont && DefaultFont->Bold() ?
											LCss::FontWeightBold :
											LCss::FontWeightNormal;
		LCss::FontStyleType FontStyle = Style->FontStyle();
		LCss::TextDecorType Decor = Style->TextDecoration();

		LFont *f = NULL;
		for (unsigned i = 0; !f && i<Fam.Length(); i++)
		{		
			f = AddFont(Fam[i],
						Sz,
						Weight != LCss::FontWeightInherit ? Weight : DefaultWeight,
						FontStyle != LCss::FontStyleInherit ? FontStyle : LCss::FontStyleNormal,
						Decor != LCss::TextDecorInherit ? Decor : LCss::TextDecorNone);
		}

		return f;
	}
};

