#ifndef _GFONTCACHE_H_
#define _GFONTCACHE_H_

class GFontCache
{
	LFont *DefaultFont;
	GArray<LFont*> Fonts;
	LHashTbl<ConstStrKey<char>, GString> FontName;
	
public:
	/// Constructor for font cache
	GFontCache
	(
		/// This is an externally owned default font... or optionally 
		/// NULL if there is no default.
		LFont *DefFnt = NULL
	)
	{
		DefaultFont = DefFnt;
	}
	
	~GFontCache()
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
		GString s = FontName.Find(Label);
		if (!s.Get())
			FontName.Add(Label, GString(FontFace));
	}
	
	LFont *AddFont(	const char *Face,
					GCss::Len Size,
					GCss::FontWeightType Weight,
					GCss::FontStyleType Style,
					GCss::TextDecorType Decor)
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
				f->Bold() == (Weight == GCss::FontWeightBold) &&
				f->Italic() == (Style == GCss::FontStyleItalic) &&
				f->Underline() == (Decor == GCss::TextDecorUnderline)
			)
				return f;
		}
		
		// No matching font... create a new one
		LFont *f = new LFont;
		if (f)
		{
			f->Bold(Weight == GCss::FontWeightBold);
			f->Italic(Style == GCss::FontStyleItalic);
			f->Underline(Decor == GCss::TextDecorUnderline);
			
			auto Sz = Size;
			if (Sz.Type == GCss::SizeLarger)
			{
				Sz = SysFont->Size();
				Sz.Value++;
			}
			else if (Sz.Type == GCss::SizeSmaller)
			{
				Sz = SysFont->Size();
				Sz.Value--;
			}

			if (!f->Create(Face, Sz))
			{
				LgiAssert(0);
				DeleteObj(f);
				return NULL;
			}
			
			Fonts.Add(f);
		}
		
		return f;
	}

	LFont *GetFont(GCss *Style)
	{
		if (!Style || !DefaultFont)
			return DefaultFont;
		
		GCss::StringsDef Fam = Style->FontFamily();
		bool FamHasDefFace = false;
		for (unsigned i=0; i<Fam.Length(); i++)
		{
			GString s = FontName.Find(Fam[i]);
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
		
		GCss::Len Sz = Style->FontSize();
		if (!Sz.IsValid())
			Sz = DefaultFont->Size();
		GCss::FontWeightType Weight = Style->FontWeight();
		GCss::FontWeightType DefaultWeight = DefaultFont && DefaultFont->Bold() ?
											GCss::FontWeightBold :
											GCss::FontWeightNormal;
		GCss::FontStyleType FontStyle = Style->FontStyle();
		GCss::TextDecorType Decor = Style->TextDecoration();

		LFont *f = NULL;
		for (unsigned i = 0; !f && i<Fam.Length(); i++)
		{		
			f = AddFont(Fam[i],
						Sz,
						Weight != GCss::FontWeightInherit ? Weight : DefaultWeight,
						FontStyle != GCss::FontStyleInherit ? FontStyle : GCss::FontStyleNormal,
						Decor != GCss::TextDecorInherit ? Decor : GCss::TextDecorNone);
		}

		return f;
	}
};

#endif
