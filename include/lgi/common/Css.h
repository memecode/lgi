/// \file
/// \author Matthew Allen <fret@memecode.com>
#pragma once

/// I've using the American spelling for 'color' as opposed to the English 'colour'
/// because the CSS spec is written with 'color' as the spelling.

#include "LgiInc.h"
#include "LgiOsDefs.h"
#include "lgi/common/Mem.h"
#include "lgi/common/Gdc2.h"
#include "lgi/common/AutoPtr.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/HashTable.h"

#ifndef LINUX
#pragma pack(push, 1)
#endif

#define LCss_FontFamilyTypes() \
	_(FontFamilyInherit, "inherit") \
	_(FontFamilyInitial, "initial") \
	_(FontFamilyRevert, "revert") \
	_(FontFamilyRevertLayer, "revert-layer") \
	_(FontFamilyUnset, "unset") \
	\
	_(FontFamilySerif, "serif") \
	_(FontFamilySansSerif, "sans-serif") \
	_(FontFamilyMonospace, "monospace") \
	_(FontFamilyCursive, "cursive") \
	_(FontFamilyFantasy, "fantasy") \
	_(FontFamilySystemUi, "system-ui") \
	_(FontFamilyUiSerif, "ui-serif") \
	_(FontFamilyUiSansSerif, "ui-sans-serif") \
	_(FontFamilyUiMonospace, "ui-monospace") \
	_(FontFamilyUiRounded, "ui-rounded") \
	_(FontFamilyEmoji, "emoji") \
	_(FontFamilyMath, "math") \
	_(FontFamilyFangsong, "fangsong")

/// Css property container
class LgiClass LCss
{
	bool ReadOnly = false;

public:
	enum ParsingStyle {
		ParseStrict,
		ParseRelaxed,
	};

	enum PropTypes {
		TypeEnum = 1,
		TypeLen,
		TypeGRect,
		TypeColor,
		TypeBackground,
		TypeImage,
		TypeBorder,
		TypeFontFamilies,
	};

	/// These are all the types of CSS properties catagorised by their data type.
	/// The enum value of the prop is encoded in the bottom 8 bits, the data type of 
	/// the property is encoding in the top remaining top bits.
	enum PropType {
		PropNull = 0,

		// Enum based props
		PropDisplay = TypeEnum << 8,// DisplayType
		PropFloat,					// FloatType
		PropPosition,				// PositionType
		PropOverflow,				// OverflowType
		PropVisibility,				// VisibilityType
		PropFontStyle,				// FontStyleType
		PropFontVariant,			// FontVariantType
		PropFontWeight,				// FontWeightType
		PropBackgroundRepeat,		// RepeatType
		PropBackgroundAttachment,	// AttachmentType
		PropTextDecoration,			// TextDecorType
		PropWordWrap,				// WordWraptype
		PropListStyleType,			// ListStyleTypes
		PropLetterSpacing,
		PropFont,
		PropListStyle,
		PropBorderCollapse,			// BorderCollapseType
		PropBorderTopStyle,
		PropBorderRightStyle,
		PropBorderBottomStyle,
		PropBorderLeftStyle,

		// Length based props
		PropZIndex = TypeLen << 8,
		PropWidth,
		PropMinWidth,
		PropMaxWidth,
		PropHeight,
		PropMinHeight,
		PropMaxHeight,
		PropTop,
		PropRight,
		PropBottom,
		PropLeft,
		PropMargin,
		PropMarginTop,
		PropMarginRight,
		PropMarginBottom,
		PropMarginLeft,
		PropPadding,
		PropPaddingTop,
		PropPaddingRight,
		PropPaddingBottom,
		PropPaddingLeft,
		PropLineHeight,
		PropVerticalAlign,
		PropFontSize,
		PropBackgroundX,
		PropBackgroundY,
		PropBackgroundPos,
		PropTextAlign,
		PropBorderSpacing,
		PropBorderLeftWidth,
		PropBorderTopWidth,
		PropBorderRightWidth,
		PropBorderBottomWidth,
		PropBorderRadius,
		Prop_CellPadding, // not real CSS, but used by LHtml2 to store 'cellpadding'

		// LRect based props
		PropClip = TypeGRect<<8,
		PropXSubRect,

		// Background meta style
		PropBackground = TypeBackground << 8,

		// ColorDef based
		PropColor = TypeColor << 8,
		PropBackgroundColor,
		PropNoPaintColor,
		PropBorderTopColor,
		PropBorderRightColor,
		PropBorderBottomColor,
		PropBorderLeftColor,

		// ImageDef based
		PropBackgroundImage = TypeImage << 8,
		
		// BorderDef based
		PropBorder = TypeBorder << 8,
		PropBorderStyle,
		PropBorderColor,
		PropBorderTop,
		PropBorderRight,
		PropBorderBottom,
		PropBorderLeft,

		// FontFamilies based
		PropFontFamily = TypeFontFamilies << 8,
	};

	PropTypes GetType(PropType p) { return (PropTypes) ((int)p >> 8); }

	enum BorderCollapseType {
		CollapseInherit,
		CollapseCollapse,
		CollapseSeparate,
	};

	enum WordWrapType {
		WrapNormal,
		WrapBreakWord,
		WrapNone, // Not in the CSS standard but useful for apps anyway.
	};

	enum DisplayType {
		DispInherit,
		DispBlock,
		DispInline,
		DispInlineBlock,
		DispListItem,
		DispNone,
	};

	enum PositionType {
		PosInherit,
		PosStatic,
		PosRelative,
		PosAbsolute,
		PosFixed,
	};

	enum LengthType {
		
		// Standard length types
		LenInherit,
		LenAuto,
		LenPx,
		LenPt,
		LenEm,
		LenEx,
		LenCm,
		LenPercent,
		LenNormal,
		LenMinContent,
		LenMaxContent,

		// Horizontal alignment types
		AlignLeft,
		AlignRight,
		AlignCenter,
		AlignJustify,

		// Vertical alignment types
		VerticalBaseline,
		VerticalSub,
		VerticalSuper,
		VerticalTop,
		VerticalTextTop,
		VerticalMiddle,
		VerticalBottom,
		VerticalTextBottom,

		// Absolute font sizes
		SizeXXSmall,
		SizeXSmall,
		SizeSmall,
		SizeMedium,
		SizeLarge,
		SizeXLarge,
		SizeXXLarge,

		// Relitive font sizes,
		SizeSmaller,
		SizeLarger,
	};

	enum FloatType {
		FloatInherit,
		FloatLeft,
		FloatRight,
		FloatNone,
	};

	enum OverflowType {
		OverflowInherit,
		OverflowVisible,
		OverflowHidden,
		OverflowScroll,
		OverflowAuto,
	};

	enum VisibilityType {
		VisibilityInherit,
		VisibilityVisible,
		VisibilityHidden,
		VisibilityCollapse
	};
	
	enum ListStyleTypes {
		ListInherit,
		ListNone,
		ListDisc,
		ListCircle,
		ListSquare,
		ListDecimal,
		ListDecimalLeadingZero,
		ListLowerRoman,
		ListUpperRoman,
		ListLowerGreek,
		ListUpperGreek,
		ListLowerAlpha,
		ListUpperAlpha,
		ListArmenian,
		ListGeorgian,
	};
	
	enum FontFamilyType {
		#define _(t, s) t,
		LCss_FontFamilyTypes()
		#undef _
	};
	
	static const char *ToString(FontFamilyType t);
	static bool ToEnum(FontFamilyType &e, const char *s);

	enum FontStyleType {
		FontStyleInherit,
		FontStyleNormal,
		FontStyleItalic,
		FontStyleOblique,
	};

	enum FontVariantType {
		FontVariantInherit,
		FontVariantNormal,
		FontVariantSmallCaps,
	};

	enum FontWeightType {
		FontWeightInherit,
		FontWeightNormal,
		FontWeightBold,
		FontWeightBolder,
		FontWeightLighter,
		FontWeight100,
		FontWeight200,
		FontWeight300,
		FontWeight400,
		FontWeight500,
		FontWeight600,
		FontWeight700,
		FontWeight800,
		FontWeight900,
	};

	enum TextDecorType {
		TextDecorInherit,
		TextDecorNone,
		TextDecorUnderline,
		TextDecorOverline,
		TextDecorLineThrough,
		TextDecorSquiggle, // for spelling errors
	};

	enum ColorType {
		ColorInherit,
		ColorTransparent,
		ColorRgb,
		ColorLinearGradient,
		ColorRadialGradient,
	};

	enum RepeatType {
		RepeatInherit,
		RepeatBoth,
		RepeatX,
		RepeatY,
		RepeatNone,
	};

	enum AttachmentType {
		AttachmentInherit,
		AttachmentScroll,
		AttachmentFixed,
	};
	
	static float FontSizeTable[7]; // SizeXSmall etc

	#define _FloatAbs(a) \
		(((a) < 0.0f) ? -(a) : (a))
	#define FloatIsEqual(a, b) \
		(_FloatAbs(a - b) < 0.0001)

	struct LgiClass Len
	{
		LengthType Type = LenInherit;
		float Value = 0.0;

		Len(const char *init = NULL, ParsingStyle ParseType = ParseRelaxed)
		{
			if (init)
				Parse(init, PropNull, ParseType);
		}

		Len(LengthType t, float v = 0.0)
		{
			Type = t;
			Value = v;
		}
		
		Len(LengthType t, int v)
		{
			Type = t;
			Value = (float)v;
		}
		
		Len(LengthType t, int64_t v)
		{
			Type = t;
			Value = (float)v;
		}
		
		Len &operator =(const Len &l)
		{
			Type = l.Type;
			Value = l.Value;
			return *this;
		}

		bool Parse(const char *&s, PropType Prop = PropNull, ParsingStyle Type = ParseStrict);
		bool IsValid() const { return Type != LenInherit; }
		bool IsDynamic() const { return	Type == LenPercent ||
									Type == LenInherit ||
									Type == LenAuto ||
									Type == SizeSmaller ||
									Type == SizeLarger; }
		bool operator ==(const Len &l) const { return Type == l.Type && FloatIsEqual(Value, l.Value); }
		bool operator !=(const Len &l) const { return !(*this == l); }
		bool ToString(LStream &p) const;
		
		/// Convert the length to pixels
		int ToPx
		(
			/// The size of the parent box if known, or -1 if unknown.
			int Box = 0,
			/// Any relevant font
			LFont *Font = 0,
			/// The DPI of the relevant device if known, or -1 if unknown
			int Dpi = -1
		);

		Len operator *(const Len &l) const;
	};

	struct LgiClass ColorStop
	{
		float Pos;
		uint32_t Rgb32;
		
		bool operator ==(const ColorStop &c)
		{
			return FloatIsEqual(Pos, c.Pos) && Rgb32 == c.Rgb32;
		}
		bool operator !=(const ColorStop &c) { return !(*this == c); }
	};

	struct LgiClass ColorDef
	{
		ColorType Type;
		uint32_t Rgb32;
		LArray<ColorStop> Stops;

		ColorDef(ColorType ct = ColorInherit, uint32_t rgb32 = 0)
		{
			Type = ct;
			Rgb32 = rgb32;
		}

		ColorDef(const LColour &col)
		{
			Type = ColorRgb;
			Rgb32 = col.c32();
		}

		ColorDef(const char *init)
		{
			Type = ColorInherit;
			Parse(init);
		}

		ColorDef(COLOUR col)
		{
			Type = ColorRgb;
			Rgb32 = Rgb24To32(col);
		}

		ColorDef(LSystemColour sys)
		{
			Type = ColorRgb;
			Rgb32 = LColour(sys).c32();
		}
		
		operator LColour()
		{
			LColour c;
			if (Type == ColorRgb)
				c.Set(Rgb32, 32);
			return c;
		}

		bool IsValid() { return Type != ColorInherit; }
		bool Parse(const char *&s);
		ColorDef &operator =(const ColorDef &c)
		{
			Type = c.Type;
			Rgb32 = c.Rgb32;
			Stops = c.Stops;
			return *this;
		}
		bool operator ==(const ColorDef &c)
		{
			if (Type != c.Type)
				return false;
			if (Type == ColorRgb)
				return Rgb32 == c.Rgb32;
			if (Stops.Length() != c.Stops.Length())
				return false;
			for (uint32_t i=0; i<Stops.Length(); i++)
			{
				if (Stops[i] != c.Stops.ItemAt(i))
					return false;
			}
			return true;
		}
		bool operator !=(const ColorDef &c) { return !(*this == c); }
		bool ToString(LStream &p);
	};

	enum BorderStyle {
		BorderNone,
		BorderHidden,
		BorderDotted,
		BorderDashed,
		BorderSolid,
		BorderDouble,
		BorderGroove,
		BorderRidge,
		BorderInset,
		BorderOutset,
	};

	struct LgiClass BorderDef : public Len
	{
		ColorDef Color;
		BorderStyle Style;
		bool Important;

		BorderDef(LCss *css = 0, const char *init = 0)
		{
			Style = BorderNone;
			Important = true;
			if (init)
				Parse(css, init);
		}

		BorderDef(const BorderDef &db) : Len(db)
		{
			Color = db.Color;
			Style = db.Style;
			Important = db.Important;
		}

		BorderDef(const char *Init)
		{
			Parse(NULL, Init);
		}
		
		bool Parse(LCss *Css, const char *&s);
		bool ParseStyle(const char *&s);
		
		BorderDef &operator =(const BorderDef &b)
		{
			Style = b.Style;
			Color = b.Color;
			Type = b.Type;
			Value = b.Value;
			Important = b.Important;
			return *this;
		}
	};

	enum ImageType {
		ImageInherit,
		ImageNone,
		ImageUri,
		ImageOwn,
		ImageRef,
	};

	struct LgiClass ImageDef
	{
		ImageType Type;
		LString Uri;
		LSurface *Img;

		ImageDef(const char *Init = NULL)
		{
			Img = NULL;
			if (Init)
			{
				Type = ImageUri;
				Uri = Init;
			}
			else
				Type = ImageInherit;
		}

		ImageDef(const ImageDef &o)
		{
			Img = NULL;
			Type = ImageInherit;
			*this = o;
		}

		~ImageDef();

		bool Parse(const char *&s);
		bool operator !=(const ImageDef &i);
		bool IsValid();
		ImageDef &operator =(const ImageDef &o);
	};

	class FontFamilies
	{
	public:
		LString::Array Names;
		LArray<FontFamilyType> Generic;

		FontFamilies(const char *init = NULL)
		{
			if (ValidStr(init))
				*this = init;
			else
				LAssert(init == NULL);
		}

		FontFamilies(const FontFamilies &c)
		{
			*this = c;
		}

		FontFamilies(FontFamilyType t)
		{
			Generic.Add(t);
		}
		
		~FontFamilies()
		{
		}

		void Empty()
		{
			Names.Length(0);
			Generic.Length(0);
		}

		bool IsEmpty() const
		{
			return	Names.Length() == 0 &&
					Generic.Length() == 0;
		}

		size_t Length() const
		{
			return Names.Length() + Generic.Length();
		}
		
		bool Has(FontFamilyType t)
		{
			return Generic.HasItem(t);
		}
		
		FontFamilies &operator =(const char *s)
		{
			Parse(s);
			return *this;
		}
		
		FontFamilies &operator =(const FontFamilies &s)
		{
			Empty();
			Names = s.Names;
			Generic = s.Generic;
			return *this;			
		}

		bool operator !=(FontFamilies &s)
		{
			return	Names != s.Names ||
					Generic != s.Generic;
		}

		bool Parse(const char *&s)
		{
			Empty();
			
			char Delimiters[] = ";, \t\r\n";
			while (s && *s && *s != ';')
			{
				while (*s && strchr(Delimiters, *s))
					s++;
				
				if (*s == '\'' || *s == '\"')
				{
					char Delim = *s++;
					const char *Start = s;
					while (*s && *s != Delim) s++;
					
					if (s > Start)
					{
						LString n(Start, s-Start);
						FontFamilyType e;
						if (ToEnum(e, n))
							Generic.Add(e);
						else
							Names.Add(n);
					}

					if (s) s++; // Skip end delimiter
				}
				else
				{
					const char *Start = s;
					while (*s && !strchr(Delimiters, *s))
						s++;

					if (s > Start)
					{
						LString n(Start, s-Start);
						FontFamilyType e;
						if (ToEnum(e, n))
							Generic.Add(e);
						else
							Names.Add(n);
					}
				}
			}

			return true;
		}
	};
	
	class Store;

	/// This class parses and stores a selector. The job of matching selectors and
	/// hashing them is still the responsibility of the calling library. If an application
	/// needs some code to do that it can optionally use LCss::Store to do that.
	class LgiClass Selector
	{
	public:
		enum PartType
		{
			SelNull,
			
			SelType,
			SelUniversal,
			SelAttrib,
			SelClass,
			SelMedia,
			SelID,
			SelPseudo,
			SelFontFace,
			SelPage,
			SelList,
			SelImport,
			SelKeyFrames,
			SelIgnored,

			CombDesc,
			CombChild,
			CombAdjacent,
		};
		
		enum MediaType
		{
			MediaNull		= 0x0,
			MediaPrint		= 0x1,
			MediaScreen		= 0x2,
		};
		
		struct Part
		{
			PartType Type;
			LAutoString Value;
			LAutoString Param;
			int Media;
			
			Part()
			{
				Type = SelNull;
				Media = MediaNull;
			}
			
			bool IsSel()
			{
				return
					Type == SelType      ||
					Type == SelUniversal ||
					Type == SelAttrib    ||
					Type == SelClass     ||
					Type == SelMedia     ||
					Type == SelID        ||
					Type == SelPseudo;
			}

			Part &operator =(const Part &s)
			{
				Type = s.Type;
				Value.Reset(NewStr(s.Value));
				Param.Reset(NewStr(s.Param));
				return *this;
			}
		};
		
		LArray<Part> Parts;
		LArray<ssize_t> Combs;
		char *Style;
		int SourceIndex;
		LAutoString Raw;
		LAutoPtr<class Store> Children;

		Selector()
		{
			Style = NULL;
			SourceIndex = 0;
		}
		bool TokString(LAutoString &a, const char *&s);
		const char *PartTypeToString(PartType p);
		LAutoString Print();
		bool Parse(const char *&s);
		size_t GetSimpleIndex() { return Combs.Length() ? Combs[Combs.Length()-1] + 1 : 0; }
		bool IsAtMedia();
		bool ToString(LStream &p);
		uint32_t GetSpecificity();
		
		Selector &operator =(const Selector &s);
	};

	/// This hash table stores arrays of selectors by name.
	typedef LArray<LCss::Selector*> SelArray;
	typedef LHashTbl<ConstStrKey<char,false>,SelArray*> SelMap;
	class SelectorMap : public SelMap
	{
		
	public:
		~SelectorMap() { Empty(); }
	
		void Empty()
		{
			// for (SelArray *s = First(); s; s = Next())
			for (auto s : *this)
			{
				s.value->DeleteObjects();
				delete s.value;
			}	
			SelMap::Empty();
		}
		
		SelArray *Get(const char *s)
		{
			SelArray *a = Find(s);
			if (!a)
				Add(s, a = new SelArray);
			return a;
		}
	};
	
	template<typename T>
	struct ElementCallback
	{
	public:
		/// Returns the element name
		virtual const char *GetElement(T *obj) = 0;
		/// Returns the document unque element ID
		virtual const char *GetAttr(T *obj, const char *Attr) = 0;
		/// Returns the class
		virtual bool GetClasses(LString::Array &Classes, T *obj) = 0;
		/// Returns the parent object
		virtual T *GetParent(T *obj) = 0;
		/// Returns an array of child objects
		virtual LArray<T*> GetChildren(T *obj) = 0;
	};
	
	/// This class parses and stores the CSS selectors and styles.
	class LgiClass Store
	{
	protected:
		/// This code matches a simple part of a selector, i.e. no combinatorial operators involved.
		template<typename T>
		bool MatchSimpleSelector
		(
			/// The full selector.
			LCss::Selector *Sel,
			/// The start index of the simple selector parts. Stop at the first comb operator or the end of the parts.
			ssize_t PartIdx,
			/// Our context callback to get properties of the object
			ElementCallback<T> *Context,
			/// The object to match
			T *Obj
		)
		{
			const char *Element = Context->GetElement(Obj);
			
			for (size_t n = PartIdx; n<Sel->Parts.Length(); n++)
			{
				LCss::Selector::Part &p = Sel->Parts[n];
				switch (p.Type)
				{
					case LCss::Selector::SelType:
					{
						const char *Tag = Context->GetElement(Obj);
						if (!Tag || _stricmp(Tag, p.Value))
							return false;
						break;
					}
					case LCss::Selector::SelUniversal:
					{
						// Match everything
						break;
					}
					case LCss::Selector::SelAttrib:
					{
						if (!p.Value)
							return false;

						char *e = strchr(p.Value, '=');
						if (!e)
							return false;

						LAutoString Var(NewStr(p.Value, e - p.Value));
						const char *TagVal = Context->GetAttr(Obj, Var);
						if (!TagVal)
							return false;

						LAutoString Val(NewStr(e + 1));
						if (_stricmp(Val, TagVal))
							return false;
						break;
					}
					case LCss::Selector::SelClass:
					{
						// Check the class matches
						LString::Array Class;
						if (!Context->GetClasses(Class, Obj))
							return false;

						if (Class.Length() == 0)
							return false;

						bool Match = false;
						for (unsigned i=0; i<Class.Length(); i++)
						{
							if (!_stricmp(Class[i], p.Value))
							{
								Match = true;
								break;
							}
						}
						if (!Match)
							return false;
						break;
					}
					case LCss::Selector::SelMedia:
					case LCss::Selector::SelFontFace:
					case LCss::Selector::SelPage:
					case LCss::Selector::SelList:
					case LCss::Selector::SelImport:
					case LCss::Selector::SelKeyFrames:
					case LCss::Selector::SelIgnored:
					{
						return false;
						break;
					}
					case LCss::Selector::SelID:
					{
						const char *Id = Context->GetAttr(Obj, "id");
						if (!Id || _stricmp(Id, p.Value))
							return false;
						break;
					}
					case LCss::Selector::SelPseudo:
					{
						const char *Href = NULL;
						if
						(
							(
								Element != NULL
								&&
								!_stricmp(Element, "a")
								&&
								p.Value && !_stricmp(p.Value, "link")
								&&
								(Href = Context->GetAttr(Obj, "href")) != NULL
							)
							||
							(
								p.Value
								&&
								*p.Value == '-'
							)
						)
							break;
							
						return false;
						break;
					}
					default:
					{
						// Comb operator, so return the current match value
						return true;
					}
				}
			}
			
			return true;
		}
		
		/// This code matches a all the parts of a selector.
		template<typename T>
		bool MatchFullSelector(LCss::Selector *Sel, ElementCallback<T> *Context, T *Obj)
		{
			bool Complex = Sel->Combs.Length() > 0;
			ssize_t CombIdx = Complex ? (ssize_t)Sel->Combs.Length() - 1 : 0;
			ssize_t StartIdx = (Complex) ? Sel->Combs[CombIdx] + 1 : 0;
			
			bool Match = MatchSimpleSelector(Sel, StartIdx, Context, Obj);
			if (!Match)
				return false;

			if (Complex)
			{
				T *CurrentParent = Context->GetParent(Obj);
				
				for (; CombIdx >= 0; CombIdx--)
				{
					if (CombIdx >= (int)Sel->Combs.Length())
						break;

					StartIdx = Sel->Combs[CombIdx];
					LAssert(StartIdx > 0);

					if (StartIdx >= (ssize_t)Sel->Parts.Length())
						break;
					
					LCss::Selector::Part &p = Sel->Parts[StartIdx];
					switch (p.Type)
					{
						case LCss::Selector::CombChild:
						{
							// LAssert(!"Not impl.");
							return false;
							break;
						}
						case LCss::Selector::CombAdjacent:
						{
							// LAssert(!"Not impl.");
							return false;
							break;
						}
						case LCss::Selector::CombDesc:
						{
							// Does the parent match the previous simple selector
							ssize_t PrevIdx = StartIdx - 1;
							while (PrevIdx > 0 && Sel->Parts[PrevIdx-1].IsSel())
							{
								PrevIdx--;
							}
							bool ParMatch = false;
							for (; !ParMatch && CurrentParent; CurrentParent = Context->GetParent(CurrentParent))
							{
								ParMatch = MatchSimpleSelector(Sel, PrevIdx, Context, CurrentParent);
							}
							if (!ParMatch)
								return false;
							break;
						}
						default:
						{
							LAssert(!"This must be a comb.");
							return false;
							break;
						}
					}
				}
			}

			return Match;
		}

		// This stores the unparsed style strings. More than one selector
		// may reference this memory.
		LArray<char*> Styles;
		
		// Sort the styles into less specific to more specific order
		void SortStyles(LCss::SelArray &Styles);
		
	public:
		SelectorMap TypeMap, ClassMap, IdMap;
		SelArray Other;
		LString Error;

		~Store()
		{
			Empty();
		}

		/// Empty the data store
		void Empty()
		{
			TypeMap.Empty();
			ClassMap.Empty();
			IdMap.Empty();
			Other.DeleteObjects();
			Error.Empty();

			Styles.DeleteArrays();
		}

		/// Parse general CSS into selectors.		
		bool Parse(const char *&s, int Depth = 0);

		/// Converts store back into string form
		bool ToString(LStream &p);

		/// Use to finding matching selectors for an element.
		template<typename T>
		bool Match(LCss::SelArray &Styles, ElementCallback<T> *Context, T *Obj)
		{
			SelArray *s;

			if (!Context || !Obj)
				return false;
			
			// An array of potential selector matches...
			LArray<SelArray*> Maps;

			// Check element type
			const char *Type = Context->GetElement(Obj);
			if (Type && (s = TypeMap.Find(Type)))
				Maps.Add(s);
			
			// Check the ID
			const char *Id = Context->GetAttr(Obj, "id");
			if (Id && (s = IdMap.Find(Id)))
				Maps.Add(s);
			
			// Check all the classes
			LString::Array Classes;
			if (Context->GetClasses(Classes, Obj))
			{
				for (unsigned i=0; i<Classes.Length(); i++)
				{
					if ((s = ClassMap.Find(Classes[i])))
						Maps.Add(s);
				}
			}
			
			// Add in any other ones
			Maps.Add(&Other);

			// Now from the list of possibles, do the actual checking of selectors...
			for (unsigned i=0; i<Maps.Length(); i++)
			{
				LCss::SelArray *s = Maps[i];
				for (unsigned i=0; i<s->Length(); i++)
				{
					LCss::Selector *Sel = (*s)[i];
					
					if (!Styles.HasItem(Sel) &&
						MatchFullSelector(Sel, Context, Obj))
					{
						// Output the matching selector
						Styles.Add(Sel);
					}
				}
			}
			
			// Sort the selectors into less specific -> more specific order.
			SortStyles(Styles);

			return true;
		}

		/// For debugging: dumps a description of the store to a stream
		bool Dump(LStream &out);
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	LCss();
	LCss(const LCss &c);
	static void Init();
	static void Shutdown();
	virtual ~LCss();

	#define Accessor(PropName, Type, Default, BaseProp) \
		Type PropName() { Type *Member = (Type*)Props.Find(Prop##PropName); \
							if (Member) return *Member; \
							else if ((Member = (Type*)Props.Find(BaseProp))) return *Member; \
							return Default; } \
		void PropName(Type t) {	LAssert(!ReadOnly); \
								Type *Member = (Type*)Props.Find(Prop##PropName); \
								if (Member) *Member = t; \
								else { Props.Add(Prop##PropName, Member = new Type); \
										*Member = t; } \
								OnChange(Prop##PropName); }

	Accessor(Display, DisplayType, DispInherit, PropNull);
	Accessor(Float, FloatType, FloatInherit, PropNull);
	Accessor(Position, PositionType, PosInherit, PropNull);
	Accessor(ZIndex, Len, Len(), PropNull);

	Accessor(TextAlign, Len, Len(), PropNull);
	Accessor(VerticalAlign, Len, Len(), PropNull);

	Accessor(Width, Len, Len(), PropNull);
	Accessor(MinWidth, Len, Len(), PropNull);
	Accessor(MaxWidth, Len, Len(), PropNull);

	Accessor(Height, Len, Len(), PropNull);
	Accessor(MinHeight, Len, Len(), PropNull);
	Accessor(MaxHeight, Len, Len(), PropNull);
	Accessor(LineHeight, Len, Len(), PropNull);

	Accessor(Top, Len, Len(), PropNull);
	Accessor(Right, Len, Len(), PropNull);
	Accessor(Bottom, Len, Len(), PropNull);
	Accessor(Left, Len, Len(), PropNull);

	Accessor(Margin, Len, Len(), PropNull);
	Accessor(MarginTop, Len, Len(), PropNull);
	Accessor(MarginRight, Len, Len(), PropNull);
	Accessor(MarginBottom, Len, Len(), PropNull);
	Accessor(MarginLeft, Len, Len(), PropNull);

	Accessor(Padding, Len, Len(), PropNull);
	Accessor(PaddingTop, Len, Len(), PropPadding);
	Accessor(PaddingRight, Len, Len(), PropPadding);
	Accessor(PaddingBottom, Len, Len(), PropPadding);
	Accessor(PaddingLeft, Len, Len(), PropPadding);

	Accessor(Border, BorderDef, BorderDef(), PropNull);
	Accessor(BorderTop, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderRight, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderBottom, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderLeft, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderSpacing, Len, Len(), PropNull); // 'cellspacing'
	Accessor(BorderRadius, Len, Len(), PropNull);
	Accessor(BorderCollapse, BorderCollapseType, CollapseInherit, PropBorderCollapse);
	Accessor(_CellPadding, Len, Len(), PropNull); // 'cellpadding' (not CSS)

	Accessor(Overflow, OverflowType, OverflowInherit, PropNull);
	Accessor(Clip, LRect, LRect(0, 0, -1, -1), PropNull);
	Accessor(XSubRect, LRect, LRect(0, 0, -1, -1), PropNull);
	Accessor(Visibility, VisibilityType, VisibilityInherit, PropNull);
	Accessor(ListStyleType, ListStyleTypes, ListInherit, PropNull);

	Accessor(FontFamily, FontFamilies, FontFamilies(), PropNull);
	Accessor(FontSize, Len, Len(), PropNull);
	Accessor(FontStyle, FontStyleType, FontStyleInherit, PropNull);
	Accessor(FontVariant, FontVariantType, FontVariantInherit, PropNull);
	Accessor(FontWeight, FontWeightType, FontWeightInherit, PropNull);
	Accessor(TextDecoration, TextDecorType, TextDecorInherit, PropNull);
	Accessor(WordWrap, WordWrapType, WrapNormal, PropNull);

	Accessor(Color, ColorDef, ColorDef(), PropNull);
	Accessor(NoPaintColor, ColorDef, ColorDef(), PropNull);
	Accessor(BackgroundColor, ColorDef, ColorDef(), PropNull);
	Accessor(BackgroundImage, ImageDef, ImageDef(), PropNull);
	Accessor(BackgroundRepeat, RepeatType, RepeatInherit, PropNull);
	Accessor(BackgroundAttachment, AttachmentType, AttachmentInherit, PropNull);
	Accessor(BackgroundX, Len, Len(), PropNull);
	Accessor(BackgroundY, Len, Len(), PropNull);
	Accessor(BackgroundPos, Len, Len(), PropNull);
	
	bool GetReadOnly() { return ReadOnly; }
	void SetReadOnly(bool b) { ReadOnly = b; };
	void Empty();
	void DeleteProp(PropType p);
	size_t Length() { return Props.Length(); }
	virtual void OnChange(PropType Prop);
	bool CopyStyle(const LCss &c);
	bool operator ==(LCss &c);
	bool operator !=(LCss &c) { return !(*this == c); }
	LCss &operator =(const LCss &c) { Empty(); CopyStyle(c); return *this; }
	LCss &operator +=(const LCss &c) { CopyStyle(c); return *this; }
	LCss &operator -=(const LCss &c);
	void *PropAddress(PropType p) { return Props.Find(p); }
	LAutoString ToString();
	const char *ToString(DisplayType dt);
	bool HasFontStyle();
	void FontBold(bool b) { FontWeight(b ? FontWeightBold : FontWeightNormal); }

	// Parsing
	virtual bool Parse(const char *&Defs, ParsingStyle Type = ParseStrict);
	bool ParseDisplayType(const char *&s);
	void ParsePositionType(const char *&s);
	bool ParseFontStyle(PropType p, const char *&s);
	bool ParseFontVariant(PropType p, const char *&s);
	bool ParseFontWeight(PropType p, const char *&s);
	bool ParseBackgroundRepeat(const char *&s);

	template<typename T>
	T *GetOrCreate(T *&ptr, PropType PropId)
	{
		ptr = (T*)Props.Find(PropId);
		if (!ptr)
			Props.Add(PropId, ptr = new T);
		return ptr;
	}

    // Inheritance calculation
    typedef LArray<void*> PropArray;
    typedef LHashTbl<IntKey<PropType,PropNull>, PropArray*> PropMap;
	
	/// Copies valid properties from the node 'c' into the property collection 'Contrib'.
	/// Usually called for each node up the parent chain until the function returns false;
	bool InheritCollect(LCss &c, PropMap &Contrib);
	/// After calling InheritCollect on all the parent nodes, this method works out the final
	/// value of each property. e.g. multiplying percentages together etc.
    bool InheritResolve(PropMap &Map);
    /* Code sample:
    LCss::PropMap Map;
    Map.Add(PropFontFamily, new LCss::PropArray);
	Map.Add(PropFontSize, new LCss::PropArray);
	Map.Add(PropFontStyle, new LCss::PropArray);
	for (LTag *t = Parent; t; t = t->Parent)
	{
		if (!c.InheritCollect(*t, Map))
			break;
	}	
	LCss c; // Container for final values
	c.InheritResolve(Map);	
	Map.DeleteObjects();
	*/

protected:
	inline void DeleteProp(PropType p, void *Ptr);
	LHashTbl<IntKey<PropType,PropNull>, void*> Props;

	static LHashTbl<ConstStrKey<char,false>, PropType> Lut;
	static LHashTbl<IntKey<int>, PropType> ParentProp;

	static const char *PropName(PropType p);

	virtual bool OnUnhandledColor(ColorDef *def, const char *&s) { return false; }

	template<typename T>
	void StoreProp(PropType id, T *obj, bool own)
	{
		T *e = (T*)Props.Find(id);
		if (e)
		{
			*e = *obj;
			if (own)
				delete obj;
		}
		else if (own)
		{
			Props.Add(id, obj);
		}
		else
		{
			Props.Add(id, new T(*obj));
		}
	}
};

#ifndef LINUX
#pragma pack(pop)
#endif
