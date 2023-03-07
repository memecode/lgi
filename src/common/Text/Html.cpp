#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <cmath>

#include "lgi/common/Lgi.h"
#include "lgi/common/Html.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Variant.h"
#include "lgi/common/FindReplaceDlg.h"
#include "lgi/common/Unicode.h"
#include "lgi/common/Emoji.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Button.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Combo.h"
#include "lgi/common/GdcTools.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Palette.h"
#include "lgi/common/Path.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Net.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Menu.h"
#include "lgi/common/FindReplaceDlg.h"
#include "lgi/common/Homoglyphs.h"
#include "lgi/common/Charset.h"

#include "HtmlPriv.h"

#define DEBUG_TABLE_LAYOUT			1
#define DEBUG_DRAW_TD				0
#define DEBUG_RESTYLE				0
#define DEBUG_TAG_BY_POS			0
#define DEBUG_SELECTION				0
#define DEBUG_TEXT_AREA				0

#define ENABLE_IMAGE_RESIZING		1
#define DOCUMENT_LOAD_IMAGES		1
#define MAX_RECURSION_DEPTH			300
#define ALLOW_TABLE_GROWTH			1
#define LGI_HTML_MAXPAINT_TIME		350 // ms
#define FLOAT_TOLERANCE				0.001

#define CRASH_TRACE					0
#ifdef MAC
#define HTML_USE_DOUBLE_BUFFER		0
#else
#define HTML_USE_DOUBLE_BUFFER		1
#endif
#define GT_TRANSPARENT				0x00000000
#ifndef IDC_HAND
#define IDC_HAND					MAKEINTRESOURCE(32649)
#endif

#undef CellSpacing
#define DefaultCellSpacing			0
#define DefaultCellPadding			1

#ifdef MAC
#define MinimumPointSize			9
#define MinimumBodyFontSize			12
#else
#define MinimumPointSize			8
#define MinimumBodyFontSize			11
#endif

// #define DefaultFont					"font-family: Times; font-size: 16pt;"
#define DefaultBodyMargin			"5px"
#define DefaultImgSize				16
#define DefaultMissingCellColour	GT_TRANSPARENT // Rgb32(0xf0,0xf0,0xf0)
#define ShowNbsp					0

#define FontPxHeight(fnt)			(fnt->GetHeight() - (int)(fnt->Leading() + 0.5))

#if 0 // def _DEBUG
#define DefaultTableBorder			Rgb32(0xf8, 0xf8, 0xf8)
#else
#define DefaultTableBorder			GT_TRANSPARENT
#endif

#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
#define DEBUG_LOG(...)				if (Table->Debug) LgiTrace(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif

#define IsTableCell(id)				( ((id) == TAG_TD) || ((id) == TAG_TH) )
#define IsTableTag()				(TagId == TAG_TABLE || TagId == TAG_TR || TagId == TAG_TD || TagId == TAG_TH)
#define GetCssLen(a, b)				a().Type == LCss::LenInherit ? b() : a()

static char WordDelim[]	=			".,<>/?[]{}()*&^%$#@!+|\'\"";
static char16 WhiteW[] =			{' ', '\t', '\r', '\n', 0};

#if 0
static char DefaultCss[] = {
"a				{ color: blue; text-decoration: underline; }"
"body			{ margin: 8px; }"
"strong			{ font-weight: bolder; }"
"pre				{ font-family: monospace }"
"h1				{ font-size: 2em; margin: .67em 0px; }"
"h2				{ font-size: 1.5em; margin: .75em 0px; }"
"h3				{ font-size: 1.17em; margin: .83em 0px; }"
"h4, p,"
"blockquote, ul,"
"fieldset, form,"
"ol, dl, dir,"
"menu            { margin: 1.12em 0px; }"
"h5              { font-size: .83em; margin: 1.5em 0px; }"
"h6              { font-size: .75em; margin: 1.67em 0px; }"
"strike, del		{ text-decoration: line-through; }"
"hr              { border: 1px inset; }"
"center          { text-align: center; }"
"h1, h2, h3, h4,"
"h5, h6, b,"
"strong          { font-weight: bolder; }"
};
#endif

template<typename T>
void RemoveChars(T *str, T *remove_list)
{
	T *i = str, *o = str, *c;
	while (*i)
	{
		for (c = remove_list; *c; c++)
		{
			if (*c == *i)
				break;
		}
		if (*c == 0)
			*o++ = *i;
		i++;
	}
	*o++ = NULL;
}

//////////////////////////////////////////////////////////////////////
using namespace Html1;

namespace Html1
{

class LHtmlPrivate
{
public:
	LHashTbl<ConstStrKey<char>, LTag*> Loading;
	LHtmlStaticInst Inst;
	bool CursorVis;
	LRect CursorPos;
	LPoint Content;
	bool WordSelectMode;
	bool LinkDoubleClick;
	LAutoString OnLoadAnchor;
	bool DecodeEmoji;
	LAutoString EmojiImg;
	int NextCtrlId;
	uint64 SetScrollTime;
	int DeferredLoads;

	bool IsParsing;
	bool IsLoaded;
	bool StyleDirty;

	// Paint time limits...
	bool MaxPaintTimeout = false;
	int MaxPaintTime = LGI_HTML_MAXPAINT_TIME;
	
	// Find settings
	LAutoWString FindText;
	bool MatchCase;
	
	LHtmlPrivate()
	{
		IsLoaded = false;
		StyleDirty = false;
		IsParsing = false;
		LinkDoubleClick = true;
		WordSelectMode = false;
		NextCtrlId = 2000;
		SetScrollTime = 0;
		CursorVis = false;
		CursorPos.ZOff(-1, -1);
		DeferredLoads = 0;

		char EmojiPng[MAX_PATH_LEN];
		#ifdef MAC
		LMakePath(EmojiPng, sizeof(EmojiPng), LGetExeFile(), "Contents/Resources/Emoji.png");
		#else
		LGetSystemPath(LSP_APP_INSTALL, EmojiPng, sizeof(EmojiPng));
		LMakePath(EmojiPng, sizeof(EmojiPng), EmojiPng, "resources/emoji.png");
		#endif
		if (LFileExists(EmojiPng))
		{
			DecodeEmoji = true;
			EmojiImg.Reset(NewStr(EmojiPng));
		}
		else
			DecodeEmoji = false;
	}

	~LHtmlPrivate()
	{
	}
};

class InputButton : public LButton
{
	LTag *Tag;
	
public:
	InputButton(LTag *tag, int Id, const char *Label) : LButton(Id, 0, 0, -1, -1, Label)
	{
		Tag = tag;
	}
	
	void OnClick(const LMouse &m)
	{
		Tag->OnClick(m);
	}
};

class LFontCache
{
	LHtml *Owner;
	List<LFont> Fonts;

public:
	LFontCache(LHtml *owner)
	{
		Owner = owner;
	}

	~LFontCache()
	{
		Fonts.DeleteObjects();
	}

	LFont *FontAt(int i)
	{
		return Fonts.ItemAt(i);
	}
	
	LFont *FindMatch(LFont *m)
	{
		for (auto f: Fonts)
		{
			if (*f == *m)
			{
				return f;
			}
		}
		
		return 0;
	}

	LFont *GetFont(LCss *Style)
	{
		if (!Style)
			return NULL;
		
		LFont *Default = Owner->GetFont();
		LCss::StringsDef Face = Style->FontFamily();
		if (Face.Length() < 1 || !ValidStr(Face[0]))
		{
			Face.Empty();
			const char *DefFace = Default->Face();
			LAssert(ValidStr(DefFace));
			Face.Add(NewStr(DefFace));
		}
		LAssert(ValidStr(Face[0]));
		LCss::Len Size = Style->FontSize();
		LCss::FontWeightType Weight = Style->FontWeight();
		bool IsBold =	Weight == LCss::FontWeightBold ||
						Weight == LCss::FontWeightBolder ||
						Weight > LCss::FontWeight400;
		bool IsItalic = Style->FontStyle() == LCss::FontStyleItalic;
		bool IsUnderline = Style->TextDecoration() == LCss::TextDecorUnderline;

		if (Size.Type == LCss::LenInherit ||
			Size.Type == LCss::LenNormal)
		{
			Size.Type = LCss::LenPt;
			Size.Value = (float)Default->PointSize();
		}

		auto Scale = Owner->GetDpiScale();
		if (Size.Type == LCss::LenPx)
		{
			Size.Value *= (float) Scale.y;
		    int RequestPx = (int) Size.Value;

			// Look for cached fonts of the right size...
			for (auto f: Fonts)
			{
				if (f->Face() &&
					_stricmp(f->Face(), Face[0]) == 0 &&
					f->Bold() == IsBold &&
					f->Italic() == IsItalic &&
					f->Underline() == IsUnderline)
				{
				    int Px = FontPxHeight(f);
				    int Diff = Px - RequestPx;
					if (Diff >= 0 && Diff <= 2)
						return f;
				}
			}
		}
		else if (Size.Type == LCss::LenPt)
		{
			double Pt = Size.Value;
			for (auto f: Fonts)
			{
				if (!f->Face() || Face.Length() == 0)
				{
					LAssert(0);
					break;
				}
				
				auto FntSz = f->Size();
				if (f->Face() &&
					_stricmp(f->Face(), Face[0]) == 0 &&
					FntSz.Type == LCss::LenPt &&
					std::abs(FntSz.Value - Pt) < FLOAT_TOLERANCE &&
					f->Bold() == IsBold &&
					f->Italic() == IsItalic &&
					f->Underline() == IsUnderline)
				{
					// Return cached font
					return f;
				}
			}
		}
		else if (Size.Type == LCss::LenPercent)
		{
			// Most of the percentages will be resolved in the "Apply" stage
			// of the CSS calculations, any that appear here have no "font-size"
			// in their parent tree, so we just use the default font size times
			// the requested percent
			Size.Type = LCss::LenPt;
			Size.Value *= Default->PointSize() / 100.0f;
			if (Size.Value < MinimumPointSize)
				Size.Value = MinimumPointSize;
		}
		else if (Size.Type == LCss::LenEm)
		{
			// Most of the relative sizes will be resolved in the "Apply" stage
			// of the CSS calculations, any that appear here have no "font-size"
			// in their parent tree, so we just use the default font size times
			// the requested percent
			Size.Type = LCss::LenPt;
			Size.Value *= Default->PointSize();
			if (Size.Value < MinimumPointSize)
				Size.Value = MinimumPointSize;
		}
		else if (Size.Type == LCss::SizeXXSmall ||
				Size.Type == LCss::SizeXSmall ||
				Size.Type == LCss::SizeSmall ||
				Size.Type == LCss::SizeMedium ||
				Size.Type == LCss::SizeLarge ||
				Size.Type == LCss::SizeXLarge ||
				Size.Type == LCss::SizeXXLarge)
		{
			int Idx = Size.Type-LCss::SizeXXSmall;
			LAssert(Idx >= 0 && Idx < CountOf(LCss::FontSizeTable));
			Size.Type = LCss::LenPt;
			Size.Value = Default->PointSize() * LCss::FontSizeTable[Idx];
			if (Size.Value < MinimumPointSize)
				Size.Value = MinimumPointSize;
		}
		else if (Size.Type == LCss::SizeSmaller)
		{
			Size.Type = LCss::LenPt;
			Size.Value = (float)(Default->PointSize() - 1);
		}
		else if (Size.Type == LCss::SizeLarger)
		{
			Size.Type = LCss::LenPt;
			Size.Value = (float)(Default->PointSize() + 1);
		}
		else LAssert(!"Not impl.");

		LFont *f;
		if ((f = new LFont))
		{
			auto ff = ValidStr(Face[0]) ? Face[0] : Default->Face();
			f->Face(ff);
			f->Size(Size.IsValid() ? Size : Default->Size());
			f->Bold(IsBold);
			f->Italic(IsItalic);
			f->Underline(IsUnderline);
			
			// printf("Add cache font %s,%i %i,%i,%i\n", f->Face(), f->PointSize(), f->Bold(), f->Italic(), f->Underline());
			if (std::abs(Size.Value) < FLOAT_TOLERANCE)
				;
			else if (!f->Create((char*)0, 0))
			{
				// Broken font...
				f->Face(Default->Face());
				LFont *DefMatch = FindMatch(f);
				// printf("Falling back to default face for '%s:%i', DefMatch=%p\n", ff, f->PointSize(), DefMatch);
				if (DefMatch)
				{
					DeleteObj(f);
					return DefMatch;
				}
				else
				{
					if (!f->Create((char*)0, 0))
					{
						DeleteObj(f);
						return Fonts[0];
					}
				}
			}

			// Not already cached
			Fonts.Insert(f);
			if (!f->Face())
			{
				LAssert(0);
			}
			
			return f;
		}

		return 0;
	}
};

class LFlowRegion
{
	LCss::LengthType Align = LCss::LenInherit;
	List<LFlowRect> Line;	// These pointers aren't owned by the flow region
							// When the line is finish, all the tag regions
							// will need to be vertically aligned

	struct LFlowStack
	{
		int LeftAbs;
		int RightAbs;
		int TopAbs;
	};
	LArray<LFlowStack> Stack;

public:
	LHtml *Html;
	int x1, x2;					// Left and right margins
	int y1;						// Current y position
	int y2;						// Maximum used y position
	int cx;						// Current insertion point
	int my;						// How much of the area above y2 was just margin
	LPoint MAX;					// Max dimensions
	int Inline;
	int InBody;

	LFlowRegion(LHtml *html, bool inbody)
	{
		Html = html;
		x1 = x2 = y1 = y2 = cx = my = 0;
		Inline = 0;
		InBody = inbody;
	}

	LFlowRegion(LHtml *html, LRect r, bool inbody)
	{
		Html = html;
		MAX.x = cx = x1 = r.x1;
		MAX.y = y1 = y2 = r.y1;
		x2 = r.x2;
		my = 0;
		Inline = 0;
		InBody = inbody;
	}

	LFlowRegion(LFlowRegion &r)
	{
		Html = r.Html;
		x1 = r.x1;
		x2 = r.x2;
		y1 = r.y1;
		MAX.x = cx = r.cx;
		MAX.y = y2 = r.y2;
		my = r.my;
		Inline = r.Inline;
		InBody = r.InBody; 
	}

	LString ToString()
	{
		LString s;
		s.Printf("Flow: x=%i(%i)%i y=%i,%i my=%i inline=%i",
			x1, cx, x2,
			y1, y2, my,
			Inline);
		return s;
	}

	int X()
	{
		return x2 - cx;
	}

	void X(int newx)
	{
		x2 = x1 + newx - 1;
	}

	int Width()
	{
		return x2 - x1 + 1;
	}

	LFlowRegion &operator +=(LRect r)
	{
		x1 += r.x1;
		cx += r.x1;
		x2 -= r.x2;
		y1 += r.y1;
		y2 += r.y1;
		
		return *this;
	}

	LFlowRegion &operator -=(LRect r)
	{
		x1 -= r.x1;
		cx -= r.x1;
		x2 += r.x2;
		y1 += r.y2;
		y2 += r.y2;
	
		return *this;
	}
	
	void AlignText();
	void FinishLine(bool Margin = false);
	void EndBlock();
	void Insert(LFlowRect *Tr, LCss::LengthType Align);
	LRect *LineBounds();

	void Indent(LTag *Tag,
				LCss::Len Left,
				LCss::Len Top,
				LCss::Len Right,
				LCss::Len Bottom,
				bool IsMargin)
	{
		LFlowRegion This(*this);
		LFlowStack &Fs = Stack.New();

		Fs.LeftAbs = Left.IsValid() ? ResolveX(Left, Tag, IsMargin) : 0;
		Fs.RightAbs = Right.IsValid() ? ResolveX(Right, Tag, IsMargin) : 0;
		Fs.TopAbs = Top.IsValid() ? ResolveY(Top, Tag, IsMargin) : 0;

		x1 += Fs.LeftAbs;
		cx += Fs.LeftAbs;
		x2 -= Fs.RightAbs;
		y1 += Fs.TopAbs;
		y2 += Fs.TopAbs;
		if (IsMargin)
			my += Fs.TopAbs;
	}

	void Indent(LRect &Px,
				bool IsMargin)
	{
		LFlowRegion This(*this);
		LFlowStack &Fs = Stack.New();

		Fs.LeftAbs = Px.x1;
		Fs.RightAbs = Px.x2;
		Fs.TopAbs = Px.y1;

		x1 += Fs.LeftAbs;
		cx += Fs.LeftAbs;
		x2 -= Fs.RightAbs;
		y1 += Fs.TopAbs;
		y2 += Fs.TopAbs;
		
		if (IsMargin)
			my += Fs.TopAbs;
	}

	void Outdent(LRect &Px,
				bool IsMargin)
	{
		LFlowRegion This = *this;

		ssize_t len = Stack.Length();
		if (len > 0)
		{
			LFlowStack &Fs = Stack[len-1];

			int &BottomAbs = Px.y2;

			x1 -= Fs.LeftAbs;
			cx -= Fs.LeftAbs;
			x2 += Fs.RightAbs;
			y2 += BottomAbs;
			if (IsMargin)
				my += BottomAbs;

			Stack.Length(len-1);
		}
		else LAssert(!"Nothing to pop.");
	}

	void Outdent(LTag *Tag,
				LCss::Len Left,
				LCss::Len Top,
				LCss::Len Right,
				LCss::Len Bottom,
				bool IsMargin)
	{
		LFlowRegion This = *this;

		ssize_t len = Stack.Length();
		if (len > 0)
		{
			LFlowStack &Fs = Stack[len-1];

			int BottomAbs = Bottom.IsValid() ? ResolveY(Bottom, Tag, IsMargin) : 0;

			x1 -= Fs.LeftAbs;
			cx -= Fs.LeftAbs;
			x2 += Fs.RightAbs;
			y2 += BottomAbs;
			if (IsMargin)
				my += BottomAbs;

			Stack.Length(len-1);
		}
		else LAssert(!"Nothing to pop.");
	}

	int ResolveX(LCss::Len l, LTag *t, bool IsMargin)
	{
		LFont *f = t->GetFont();
		switch (l.Type)
		{
			default:
			case LCss::LenInherit:
				return IsMargin ? 0 : X();
			case LCss::LenPx:
				// return MIN((int)l.Value, X());
				return (int)l.Value;
			case LCss::LenPt:
				return (int) (l.Value * LScreenDpi().x / 72.0);
			case LCss::LenCm:
				return (int) (l.Value * LScreenDpi().x / 2.54);
			case LCss::LenEm:
			{
				if (!f)
				{
					LAssert(!"No font?");
					f = LSysFont;
				}
				return (int)(l.Value * f->GetHeight());
			}
			case LCss::LenEx:
			{
				if (!f)
				{
					LAssert(!"No font?");
					f = LSysFont;
				}
				return (int) (l.Value * f->GetHeight() / 2); // More haha, who uses 'ex' anyway?
			}
			case LCss::LenPercent:
			{
				int my_x = X();
				int px = (int) (l.Value * my_x / 100.0);
				return px;
			}
			case LCss::LenAuto:
			{
				if (IsMargin)
					return 0;
				else
					return X();
				break;
			}
			case LCss::SizeSmall:
			{
				return 1; // px
			}
			case LCss::SizeMedium:
			{
				return 2; // px
			}
			case LCss::SizeLarge:
			{
				return 3; // px
			}
		}

		return 0;
	}
	
	bool LimitX(int &x, LCss::Len Min, LCss::Len Max, LFont *f)
	{
		bool Limited = false;
		if (Min.IsValid())
		{
			int Px = Min.ToPx(x2 - x1 + 1, f, false);
			if (Px > x)
			{
				x = Px;
				Limited = true;
			}
		}
		if (Max.IsValid())
		{
			int Px = Max.ToPx(x2 - x1 + 1, f, false);
			if (Px < x)
			{
				x = Px;
				Limited = true;
			}
		}
		return Limited;
	}

	int ResolveY(LCss::Len l, LTag *t, bool IsMargin)
	{
		LFont *f = t->GetFont();
		switch (l.Type)
		{
			case LCss::LenInherit:
			case LCss::LenAuto:
			case LCss::LenNormal:
			case LCss::LenPx:
				return (int)l.Value;
			case LCss::LenPt:
				return (int) (l.Value * LScreenDpi().y / 72.0);
			case LCss::LenCm:
				return (int) (l.Value * LScreenDpi().y / 2.54);

			case LCss::LenEm:
			{
				if (!f)
				{
					f = LSysFont;
					LAssert(!"No font");
				}
				return (int) (l.Value * f->GetHeight());
			}
			case LCss::LenEx:
			{
				if (!f)
				{
					f = LSysFont;
					LAssert(!"No font");
				}
				return (int) (l.Value * f->GetHeight() / 2); // More haha, who uses 'ex' anyway?
			}
			case LCss::LenPercent:
			{
				// Walk up tree of tags to find an absolute size...
				LCss::Len Ab;
				for (LTag *p = ToTag(t->Parent); p; p = ToTag(p->Parent))
				{
					auto h = p->Height();
					if (h.IsValid() && !h.IsDynamic())
					{
						Ab = h;
						break;
					}
				}

				if (!Ab.IsValid())
				{
					LAssert(Html != NULL);
					Ab.Type = LCss::LenPx;
					Ab.Value = (float)Html->Y();
				}

				LCss::Len m = Ab * l;
				
				return (int)m.ToPx(0, f);;
			}
			case LCss::SizeSmall:
			{
				return 1; // px
			}
			case LCss::SizeMedium:
			{
				return 2; // px
			}
			case LCss::SizeLarge:
			{
				return 3; // px
			}
			case LCss::AlignLeft:
			case LCss::AlignRight:
			case LCss::AlignCenter:
			case LCss::AlignJustify:
			case LCss::VerticalBaseline:
			case LCss::VerticalSub:
			case LCss::VerticalSuper:
			case LCss::VerticalTop:
			case LCss::VerticalTextTop:
			case LCss::VerticalMiddle:
			case LCss::VerticalBottom:
			case LCss::VerticalTextBottom:
			{
				// Meaningless in this context
				break;
			}
			default:
			{
				LAssert(!"Not supported.");
				break;
			}
		}

		return 0;
	}

	bool LimitY(int &y, LCss::Len Min, LCss::Len Max, LFont *f)
	{
		bool Limited = false;
		int TotalY = Html ? Html->Y() : 0;
		if (Min.IsValid())
		{
			int Px = Min.ToPx(TotalY, f, false);
			if (Px > y)
			{
				y = Px;
				Limited = true;
			}
		}
		if (Max.IsValid())
		{
			int Px = Max.ToPx(TotalY, f, false);
			if (Px < y)
			{
				y = Px;
				Limited = true;
			}
		}
		return Limited;
	}

	LRect ResolveMargin(LCss *Src, LTag *Tag)
	{
		LRect r;
		r.x1 = ResolveX(Src->MarginLeft(), Tag, true);
		r.y1 = ResolveY(Src->MarginTop(), Tag, true);
		r.x2 = ResolveX(Src->MarginRight(), Tag, true);
		r.y2 = ResolveY(Src->MarginBottom(), Tag, true);
		return r;
	}

	LRect ResolveBorder(LCss *Src, LTag *Tag)
	{
		LRect r;
		r.x1 = ResolveX(Src->BorderLeft(), Tag, true);
		r.y1 = ResolveY(Src->BorderTop(), Tag, true);
		r.x2 = ResolveX(Src->BorderRight(), Tag, true);
		r.y2 = ResolveY(Src->BorderBottom(), Tag, true);
		return r;
	}

	LRect ResolvePadding(LCss *Src, LTag *Tag)
	{
		LRect r;
		r.x1 = ResolveX(Src->PaddingLeft(), Tag, true);
		r.y1 = ResolveY(Src->PaddingTop(), Tag, true);
		r.x2 = ResolveX(Src->PaddingRight(), Tag, true);
		r.y2 = ResolveY(Src->PaddingBottom(), Tag, true);
		return r;
	}
};

};

//////////////////////////////////////////////////////////////////////
static bool ParseDistance(char *s, float &d, char *units = 0)
{
	if (!s)
		return false;

	while (*s && IsWhiteSpace(*s)) s++;

	if (!IsDigit(*s) && !strchr("-.", *s))
		return false;

	d = (float)atof(s);

	while (*s && (IsDigit(*s) || strchr("-.", *s))) s++;
	while (*s && IsWhiteSpace(*s)) s++;
	
	char _units[128];
	char *o = units = units ? units : _units;
	while (*s && (IsAlpha(*s) || *s == '%'))
	{
		*o++ = *s++;
	}
	*o++ = 0;

	return true;
}

LHtmlLength::LHtmlLength()
{
	d = 0;
	PrevAbs = 0;
	u = LCss::LenInherit;
}

LHtmlLength::LHtmlLength(char *s)
{
	Set(s);
}

bool LHtmlLength::IsValid()
{
	return u != LCss::LenInherit;
}

bool LHtmlLength::IsDynamic()
{
	return u == LCss::LenPercent || d == 0.0;
}

LHtmlLength::operator float ()
{
	return d;
}

LHtmlLength &LHtmlLength::operator =(float val)
{
	d = val;
	u = LCss::LenPx;
	return *this;
}

LCss::LengthType LHtmlLength::GetUnits()
{
	return u;
}

void LHtmlLength::Set(char *s)
{
	if (ValidStr(s))
	{
		char Units[256] = "";
		if (ParseDistance(s, d, Units))
		{
			if (Units[0])
			{
				if (strchr(Units, '%'))
				{
					u = LCss::LenPercent;
				}
				else if (stristr(Units, "pt"))
				{
					u = LCss::LenPt;
				}
				else if (stristr(Units, "em"))
				{
					u = LCss::LenEm;
				}
				else if (stristr(Units, "ex"))
				{
					u = LCss::LenEx;
				}
				else
				{
					u = LCss::LenPx;
				}
			}
			else
			{
				u = LCss::LenPx;
			}
		}
	}
}

float LHtmlLength::Get(LFlowRegion *Flow, LFont *Font, bool Lock)
{
	switch (u)
	{
		default: break;
		case LCss::LenEm:
		{
			return PrevAbs = d * (Font ? Font->GetHeight() : 14);
			break;
		}
		case LCss::LenEx:
		{
			return PrevAbs = (Font ? Font->GetHeight() * d : 14) / 2;
			break;
		}
		case LCss::LenPercent:
		{
			if (Lock || PrevAbs == 0.0)
			{
				return PrevAbs = (Flow->X() * d / 100);
			}
			else
			{
				return PrevAbs;
			}
			break;
		}
	}

	float FlowX = Flow ? Flow->X() : d;
	return PrevAbs = MIN(FlowX, d);
}

LHtmlLine::LHtmlLine()
{
	LineStyle = -1;
	LineReset = 0x80000000;
}

LHtmlLine::~LHtmlLine()
{
}

LHtmlLine &LHtmlLine::operator =(int i)
{
	d = (float)i;
	return *this;
}

void LHtmlLine::Set(char *s)
{
	LToken t(s, " \t");
	LineReset = 0x80000000;
	LineStyle = -1;
	char *Style = 0;

	for (unsigned i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		if
		(
			*c == '#'
			||
			LHtmlStatic::Inst->ColourMap.Find(c)
		)
		{
			LHtmlParser::ParseColour(c, Colour);
		}
		else if (_strnicmp(c, "rgb(", 4) == 0)
		{
			char Buf[256];
			strcpy_s(Buf, sizeof(Buf), c);
			while (!strchr(c, ')') &&
					(c = t[++i]))
			{
				strcat(Buf, c);
			}
			LHtmlParser::ParseColour(Buf, Colour);
		}
		else if (IsDigit(*c))
		{
			LHtmlLength::Set(c);
		}
		else if (_stricmp(c, "none") == 0)
		{
			Style = 0;
		}
		else if (	_stricmp(c, "dotted") == 0 ||
					_stricmp(c, "dashed") == 0 ||
					_stricmp(c, "solid") == 0 ||
					_stricmp(c, "float") == 0 ||
					_stricmp(c, "groove") == 0 ||
					_stricmp(c, "ridge") == 0 ||
					_stricmp(c, "inset") == 0 ||
					_stricmp(c, "outse") == 0)
		{
			Style = c;
		}
		else
		{
			// ???
		}
	}

	if (Style && _stricmp(Style, "dotted") == 0)
	{
		switch ((int)d)
		{
			case 2:
			{
				LineStyle = 0xcccccccc;
				break;
			}
			case 3:
			{
				LineStyle = 0xe38e38;
				LineReset = 0x800000;
				break;
			}
			case 4:
			{
				LineStyle = 0xf0f0f0f0;
				break;
			}
			case 5:
			{
				LineStyle = 0xf83e0;
				LineReset = 0x80000;
				break;
			}
			case 6:
			{
				LineStyle = 0xfc0fc0;
				LineReset = 0x800000;
				break;
			}
			case 7:
			{
				LineStyle = 0xfe03f80;
				LineReset = 0x8000000;
				break;
			}
			case 8:
			{
				LineStyle = 0xff00ff00;
				break;
			}
			case 9:
			{
				LineStyle = 0x3fe00;
				LineReset = 0x20000;
				break;
			}
			default:
			{
				LineStyle = 0xaaaaaaaa;
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////
LRect LTag::GetRect(bool Client)
{
	LRect r(Pos.x, Pos.y, Pos.x + Size.x - 1, Pos.y + Size.y - 1);
	if (!Client)
	{
		for (LTag *p = ToTag(Parent); p; p=ToTag(p->Parent))
		{
			r.Offset(p->Pos.x, p->Pos.y);
		}
	}
	return r;
}

LCss::LengthType LTag::GetAlign(bool x)
{
	for (LTag *t = this; t; t = ToTag(t->Parent))
	{
		LCss::Len l;
		
		if (x)
		{
			if (IsTableCell(TagId) && Cell && Cell->XAlign)
				l.Type = Cell->XAlign;
			else
				l = t->TextAlign();
		}
		else
		{
			l = t->VerticalAlign();
		}
		
		if (l.Type != LenInherit)
		{
			return l.Type;
		}

		if (t->TagId == TAG_TABLE)
			break;
	}
	
	return LenInherit;
}

//////////////////////////////////////////////////////////////////////
void LFlowRegion::EndBlock()
{
	if (cx > x1)
		FinishLine();
}

void LFlowRegion::AlignText()
{
	if (Align != LCss::AlignLeft)
	{
		int Used = 0;
		for (auto l : Line)
			Used += l->X();
		int Total = x2 - x1 + 1;
		if (Used < Total)
		{
			int Offset = 0;
			if (Align == LCss::AlignCenter)
				Offset = (Total - Used) / 2;
			else if (Align == LCss::AlignRight)
				Offset = Total - Used;
			if (Offset)
				for (auto l : Line)
				{
					if (l->Tag->Display() != LCss::DispInlineBlock)
						l->Offset(Offset, 0);
				}
		}
	}
}

void LFlowRegion::FinishLine(bool Margin)
{
	// AlignText();

	if (y2 > y1)
	{
		my = Margin ? y2 - y1 : 0;
		y1 = y2;
	}
	else
	{
		int fy = Html->DefFont()->GetHeight();
		my = Margin ? fy : 0;
		y1 += fy;
	}
	cx = x1;
	y2 = y1;

	Line.Empty();
}

LRect *LFlowRegion::LineBounds()
{
	auto It = Line.begin();
	LFlowRect *Prev = *It;
	LFlowRect *r=Prev;
	if (r)
	{
		LRect b;
		
		b = *r;
		int Ox = r->Tag->AbsX();
		int Oy = r->Tag->AbsY();
		b.Offset(Ox, Oy);

		// int Ox = 0, Oy = 0;
		while ((r = *(++It) ))
		{
			LRect c = *r;
			Ox = r->Tag->AbsX();
			Oy = r->Tag->AbsY();
			c.Offset(Ox, Oy);

			/*
			Ox += r->Tag->Pos.x - Prev->Tag->Pos.x;
			Oy += r->Tag->Pos.y - Prev->Tag->Pos.y;
			c.Offset(Ox, Oy);
			*/

			b.Union(&c);
			Prev = r;
		}

		static LRect Rgn;
		Rgn = b;
		return &Rgn;
	}

	return 0;
}

void LFlowRegion::Insert(LFlowRect *Tr, LCss::LengthType align)
{
	if (Tr)
	{
		Align = align;
		Line.Insert(Tr);
	}
}

//////////////////////////////////////////////////////////////////////
LTag::LTag(LHtml *h, LHtmlElement *p) :
	LHtmlElement(p),
	Attr(8)
{
	Ctrl = 0;
	CtrlType = CtrlNone;
	TipId = 0;
	Display(DispInline);
	Html = h;
	
	ImageResized = false;
	Cursor = -1;
	Selection = -1;
	Font = 0;
	LineHeightCache = -1;
	HtmlId = NULL;
	// TableBorder = 0;
	Cell = NULL;
	TagId = CONTENT;
	Info = 0;
	Pos.x = Pos.y = 0;

	#ifdef _DEBUG
	Debug = false;
	#endif
}

LTag::~LTag()
{
	if (Html->Cursor == this)
	{
		Html->Cursor = 0;
	}
	if (Html->Selection == this)
	{
		Html->Selection = 0;
	}

	DeleteObj(Ctrl);
	Attr.DeleteArrays();

	DeleteObj(Cell);
}

void LTag::OnChange(PropType Prop)
{
}

bool LTag::OnClick(const LMouse &m)
{
	if (!Html->Environment)
		return false;

	const char *OnClick = NULL;
	if (Get("onclick", OnClick))
	{
		Html->Environment->OnExecuteScript(Html, (char*)OnClick);
	}
	else
	{
		OnNotify(LNotification(m));
	}

	return true;
}

void LTag::Set(const char *attr, const char *val)
{
	char *existing = Attr.Find(attr);
	if (existing) DeleteArray(existing);

	if (val)
		Attr.Add(attr, NewStr(val));
}

bool LTag::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty Fld = LStringToDomProp(Name);
	switch (Fld)
	{
		case ObjStyle: // Type: LCssStyle
		{
			Value = &StyleDom;
			return true;
		}
		case ObjTextContent: // Type: String
		{
			Value = Text();
			return true;
		}
		default:
		{
			char *a = Attr.Find(Name);
			if (a)
			{
				Value = a;
				return true;
			}
			break;
		}
	}
	
	return false;
}

bool LTag::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty Fld = LStringToDomProp(Name);
	switch (Fld)
	{
		case ObjStyle:
		{
			const char *Defs = Value.Str();
			if (!Defs)
				return false;
				
			return Parse(Defs, ParseRelaxed);
		}
		case ObjTextContent:
		{
			const char *s = Value.Str();
			if (s)
			{
				LAutoWString w(CleanText(s, strlen(s), "utf-8", true, true));
				Txt = w;
				return true;
			}
			break;
		}
		case ObjInnerHtml: // Type: String
		{
			// Clear out existing tags..
			Children.DeleteObjects();
		
			char *Doc = Value.CastString();
			if (Doc)
			{
				// Create new tags...
				bool BackOut = false;
				
				while (Doc && *Doc)
				{
					LTag *t = new LTag(Html, this);
					if (t)
					{
						Doc = Html->ParseHtml(t, Doc, 1, false, &BackOut);
						if (!Doc)
							break;
					}
					else break;
				}
			}
			else return false;
			break;
		}
		default:
		{
			Set(Name, Value.CastString());
			SetStyle();
			break;
		}
	}

	Html->ViewWidth = -1;
	return true;
}

ssize_t LTag::GetTextStart()
{
	if (PreText() && TextPos.Length() > 1)
	{
		LFlowRect *t = TextPos[1];
		if (t)
			return t->Text - Text();
	}
	else if (TextPos.Length() > 0)
	{
		LFlowRect *t = TextPos[0];
		if (t && Text())
		{
			LAssert(t->Text >= Text() && t->Text <= Text()+2);
			return t->Text - Text();
		}
	}

	return 0;
}

static bool TextToStream(LStream &Out, char16 *Text)
{
	if (!Text)
		return true;

	uint8_t Buf[256];
	uint8_t *s = Buf;
	ssize_t Len = sizeof(Buf);
	while (*Text)
	{
		#define WriteExistingContent() \
			if (s > Buf) \
				Out.Write(Buf, (int)(s - Buf)); \
			s = Buf; \
			Len = sizeof(Buf); \
			Buf[0] = 0;

		if (*Text == '<' || *Text == '>')
		{
			WriteExistingContent();
			Out.Print("&%ct;", *Text == '<' ? 'l' : 'g');
		}
		else if (*Text == 0xa0)
		{
			WriteExistingContent();
			Out.Write((char*)"&nbsp;", 6);
		}
		else
		{
			LgiUtf32To8(*Text, s, Len);
			if (Len < 16)
			{
				WriteExistingContent();
			}
		}

		Text++;
	}

	if (s > Buf)
		Out.Write(Buf, s - Buf);

	return true;
}

bool LTag::CreateSource(LStringPipe &p, int Depth, bool LastWasBlock)
{
	char *Tabs = new char[Depth+1];
	memset(Tabs, '\t', Depth);
	Tabs[Depth] = 0;

	if (ValidStr(Tag))
	{
		if (IsBlock())
		{
			p.Print("%s%s<%s", TagId != TAG_HTML ? "\n" : "", Tabs, Tag.Get());
		}
		else
		{
			p.Print("<%s", Tag.Get());
		}

		if (Attr.Length())
		{
			// const char *a;
			// for (char *v = Attr.First(&a); v; v = Attr.Next(&a))
			for (auto v : Attr)
			{
				if (_stricmp(v.key, "style"))
					p.Print(" %s=\"%s\"", v.key, v.value);
			}
		}
		if (Props.Length())
		{
			LCss *Css = this;
			LCss Tmp;
			#define DelProp(p) \
				if (Css == this) { Tmp = *Css; Css = &Tmp; } \
				Css->DeleteProp(p);
			
			// Clean out any default CSS properties where we can...
			LHtmlElemInfo *i = LHtmlStatic::Inst->GetTagInfo(Tag);
			if (i)
			{
				if (Props.Find(PropDisplay)
					&&
					(
						(!i->Block() && Display() == DispInline)
						||
						(i->Block() && Display() == DispBlock)
					))
				{
					DelProp(PropDisplay);
				}
				switch (TagId)
				{
					default:
						break;
					case TAG_A:
					{
						LCss::ColorDef Blue(LCss::ColorRgb, Rgb32(0, 0, 255));
						if (Props.Find(PropColor) && Color() == Blue)
							DelProp(PropColor);
						if (Props.Find(PropTextDecoration) && TextDecoration() == LCss::TextDecorUnderline)
							DelProp(PropTextDecoration)
						break;
					}
					case TAG_BODY:
					{
						LCss::Len FivePx(LCss::LenPx, 5.0f);
						if (Props.Find(PropPaddingLeft) && PaddingLeft() == FivePx)
							DelProp(PropPaddingLeft)
						if (Props.Find(PropPaddingTop) && PaddingTop() == FivePx)
							DelProp(PropPaddingTop)
						if (Props.Find(PropPaddingRight) && PaddingRight() == FivePx)
							DelProp(PropPaddingRight)
						break;
					}
					case TAG_B:
					{
						if (Props.Find(PropFontWeight) && FontWeight() == LCss::FontWeightBold)
							DelProp(PropFontWeight);
						break;
					}
					case TAG_U:
					{
						if (Props.Find(PropTextDecoration) && TextDecoration() == LCss::TextDecorUnderline)
							DelProp(PropTextDecoration);
						break;
					}
					case TAG_I:
					{
						if (Props.Find(PropFontStyle) && FontStyle() == LCss::FontStyleItalic)
							DelProp(PropFontStyle);
						break;
					}
				}
			}
			
			// Convert CSS props to a string and emit them...
			auto s = Css->ToString();
			if (ValidStr(s))
			{			
				// Clean off any trailing whitespace...
				char *e = s ? s + strlen(s) : NULL;
				while (e && strchr(WhiteSpace, e[-1]))
					*--e = 0;
				
				// Print them to the tags attributes...
				p.Print(" style=\"%s\"", s.Get());
			}
		}
	}

	if (Children.Length() || TagId == TAG_STYLE) // <style> tags always have </style>
	{
		if (Tag)
		{
			p.Write((char*)">", 1);
			TextToStream(p, Text());
		}

		bool Last = IsBlock();
		for (unsigned i=0; i<Children.Length(); i++)
		{
			LTag *c = ToTag(Children[i]);
			c->CreateSource(p, Parent ? Depth+1 : 0, Last);
			Last = c->IsBlock();
		}

		if (Tag)
		{
			if (IsBlock())
			{
				if (Children.Length())
					p.Print("\n%s", Tabs);
			}

			p.Print("</%s>", Tag.Get());
		}
	}
	else if (Tag)
	{
		if (Text())
		{
			p.Write((char*)">", 1);
			TextToStream(p, Text());
			p.Print("</%s>", Tag.Get());
		}
		else
		{
			p.Print("/>\n");
		}
	}
	else
	{
		TextToStream(p, Text());
	}

	DeleteArray(Tabs);

	return true;
}

void LTag::SetTag(const char *NewTag)
{
	Tag.Reset(NewStr(NewTag));

	if (NewTag)
	{
		Info = Html->GetTagInfo(Tag);
		if (Info)
		{
			TagId = Info->Id;
			Display(Info->Flags & LHtmlElemInfo::TI_BLOCK ? LCss::DispBlock : LCss::DispInline);
		}
	}
	else
	{
		Info = NULL;
		TagId = CONTENT;
	}

	SetStyle();
}

LColour LTag::_Colour(bool f)
{
	for (LTag *t = this; t; t = ToTag(t->Parent))
	{
		ColorDef c = f ? t->Color() : t->BackgroundColor();
		if (c.Type != ColorInherit)
		{
			return LColour(c.Rgb32, 32);
		}

		#if 1
		if (!f && t->TagId == TAG_TABLE)
			break;
		#else
		/*	This implements some basic level of colour inheritance for
			background colours. See test case 'cisra-cqs.html'. */
		if (!f && t->TagId == TAG_TABLE)
			break;
		#endif
	}

	return LColour();
}

void LTag::CopyClipboard(LMemQueue &p, bool &InSelection)
{
	ssize_t Min = -1;
	ssize_t Max = -1;

	if (Cursor >= 0 && Selection >= 0)
	{
		Min = MIN(Cursor, Selection);
		Max = MAX(Cursor, Selection);
	}
	else if (InSelection)
	{
		Max = MAX(Cursor, Selection);
	}
	else
	{
		Min = MAX(Cursor, Selection);
	}

	ssize_t Off = -1;
	ssize_t Chars = 0;
	auto Start = GetTextStart();
	if (Min >= 0 && Max >= 0)
	{
		Off = Min + Start;
		Chars = Max - Min;
	}
	else if (Min >= 0)
	{
		Off = Min + Start;
		Chars = StrlenW(Text()) - Min;
		InSelection = true;
	}
	else if (Max >= 0)
	{
		Off = Start;
		Chars = Max;
		InSelection = false;
	}
	else if (InSelection)
	{
		Off = Start;
		Chars = StrlenW(Text());
	}

	if (Off >= 0 && Chars > 0)
	{
		p.Write((uchar*) (Text() + Off), Chars * sizeof(char16));
	}

	if (InSelection)
	{
		switch (TagId)
		{
			default: break;
			case TAG_BR:
			{
				char16 NL[] = {'\n', 0};
				p.Write((uchar*) NL, sizeof(char16));
				break;
			}
			case TAG_P:
			{
				char16 NL[] = {'\n', '\n', 0};
				p.Write((uchar*) NL, sizeof(char16) * 2);
				break;
			}
		}
	}

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *c = ToTag(Children[i]);
		c->CopyClipboard(p, InSelection);
	}
}

static
char*
_DumpColour(LCss::ColorDef c)
{
	static char Buf[4][32];
	#ifdef _MSC_VER
	static LONG Cur = 0;
	LONG Idx = InterlockedIncrement(&Cur);
	#else
	static int Cur = 0;
	int Idx = __sync_fetch_and_add(&Cur, 1);
	#endif
	char *b = Buf[Idx % 4];
	
	if (c.Type == LCss::ColorInherit)
		strcpy_s(b, 32, "Inherit");
	else
		sprintf_s(b, 32, "%2.2x,%2.2x,%2.2x(%2.2x)", R32(c.Rgb32),G32(c.Rgb32),B32(c.Rgb32),A32(c.Rgb32));

	return b;
}

void LTag::_Dump(LStringPipe &Buf, int Depth)
{
	LString Tabs;
	Tabs.Set(NULL, Depth);
	memset(Tabs.Get(), '\t', Depth);	

	const char *Empty = "";
	char *ElementName = TagId == CONTENT ? (char*)"Content" :
				(TagId == ROOT ? (char*)"Root" : Tag);

	Buf.Print(	"%s%s(%p)%s%s%s (%i) Pos=%i,%i Size=%i,%i Color=%s/%s",
				Tabs.Get(),
				ElementName,
				this,
				HtmlId ? "#" : Empty,
				HtmlId ? HtmlId : Empty,
				#ifdef _DEBUG
				Debug ? " debug" : Empty,
				#else
				Empty,
				#endif
				WasClosed,
				Pos.x, Pos.y,
				Size.x, Size.y,
				_DumpColour(Color()), _DumpColour(BackgroundColor()));

	for (unsigned i=0; i<TextPos.Length(); i++)
	{
		LFlowRect *Tr = TextPos[i];
		
		LAutoString Utf8(WideToUtf8(Tr->Text, Tr->Len));
		if (Utf8)
		{
			size_t Len = strlen(Utf8);
			if (Len > 40)
			{
				Utf8[40] = 0;
			}
		}
		else if (Tr->Text)
		{
			Utf8.Reset(NewStr(""));
		}

		Buf.Print("Tr(%i,%i %ix%i '%s') ", Tr->x1, Tr->y1, Tr->X(), Tr->Y(), Utf8.Get());
	}
	
	Buf.Print("\r\n");
	
	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		t->_Dump(Buf, Depth+1);
	}

	if (Children.Length())
	{
		Buf.Print("%s/%s\r\n", Tabs.Get(), ElementName);
	}
}

LAutoWString LTag::DumpW()
{
	LStringPipe Buf;
	// Buf.Print("Html pos=%s\n", Html?Html->GetPos().GetStr():0);
	_Dump(Buf, 0);
	LAutoString a(Buf.NewStr());
	LAutoWString w(Utf8ToWide(a));
	return w;
}

LAutoString LTag::DescribeElement()
{
	LStringPipe s(256);
	s.Print("%s", Tag ? Tag.Get() : "CONTENT");
	if (HtmlId)
		s.Print("#%s", HtmlId);
	for (unsigned i=0; i<Class.Length(); i++)
		s.Print(".%s", Class[i].Get());
	return LAutoString(s.NewStr());
}

LFont *LTag::NewFont()
{
	LFont *f = new LFont;
	if (f)
	{
		LFont *Cur = GetFont();
		if (Cur) *f = *Cur;
		else *f = *Html->DefFont();
	}
	return f;
}

LFont *LTag::GetFont()
{
	if (!Font)
	{
		if (PropAddress(PropFontFamily) != 0                    ||
			FontSize().Type             != LenInherit           ||
			FontStyle()                 != FontStyleInherit     ||
			FontVariant()               != FontVariantInherit   ||
			FontWeight()                != FontWeightInherit    ||
			TextDecoration()            != TextDecorInherit)
		{
			LCss c;
			
            LCss::PropMap Map;
            Map.Add(PropFontFamily, new LCss::PropArray);
			Map.Add(PropFontSize, new LCss::PropArray);
			Map.Add(PropFontStyle, new LCss::PropArray);
			Map.Add(PropFontVariant, new LCss::PropArray);
			Map.Add(PropFontWeight, new LCss::PropArray);
			Map.Add(PropTextDecoration, new LCss::PropArray);

			for (LTag *t = this; t; t = ToTag(t->Parent))
			{
				if (t->TagId == TAG_IFRAME)
					break;
				if (!c.InheritCollect(*t, Map))
					break;
			}
			
			c.InheritResolve(Map);
			
			Map.DeleteObjects();

			if ((Font = Html->FontCache->GetFont(&c)))
				return Font;
		}
		else
		{
			LTag *t = this;
			while (!t->Font && t->Parent)
			{
				t = ToTag(t->Parent);
			}

			if (t->Font)
				return t->Font;
		}
		
		Font = Html->DefFont();
	}
	
	return Font;
}

LTag *LTag::PrevTag()
{
	if (Parent)
	{
		ssize_t i = Parent->Children.IndexOf(this);
		if (i >= 0)
		{
			return ToTag(Parent->Children[i - 1]);
		}
	}

	return 0;
}

void LTag::Invalidate()
{
	LRect p = GetRect();
	for (LTag *t=ToTag(Parent); t; t=ToTag(t->Parent))
	{
		p.Offset(t->Pos.x, t->Pos.y);
	}
	Html->Invalidate(&p);
}

LTag *LTag::IsAnchor(LString *Uri)
{
	LTag *a = 0;
	for (LTag *t = this; t; t = ToTag(t->Parent))
	{
		if (t->TagId == TAG_A)
		{
			a = t;
			break;
		}
	}

	if (a && Uri)
	{
		const char *u = 0;
		if (a->Get("href", u))
		{
			LAutoWString w(CleanText(u, strlen(u), "utf-8"));
			if (w)
			{
				*Uri = w;
			}
		}
	}

	return a;
}

bool LTag::OnMouseClick(LMouse &m)
{
	bool Processed = false;

	if (m.IsContextMenu())
	{
		LString Uri;
		const char *ImgSrc = NULL;
		LTag *a = IsAnchor(&Uri);
		bool IsImg = TagId == TAG_IMG;
		if (IsImg)
			Get("src", ImgSrc);
		bool IsAnchor = a && ValidStr(Uri);
		if (IsAnchor || IsImg)
		{
			LSubMenu RClick;

			#define IDM_COPY_LINK	100
			#define IDM_COPY_IMG	101
			if (Html->GetMouse(m, true))
			{
				int Id = 0;

				if (IsAnchor)
					RClick.AppendItem(LLoadString(L_COPY_LINK_LOCATION, "&Copy Link Location"), IDM_COPY_LINK, Uri != NULL);
				if (IsImg)
					RClick.AppendItem("Copy Image Location", IDM_COPY_IMG, ImgSrc != NULL);
				if (Html->GetEnv())
					Html->GetEnv()->AppendItems(&RClick, Uri);

				switch (Id = RClick.Float(Html, m.x, m.y))
				{
					case IDM_COPY_LINK:
					{
						LClipBoard Clip(Html);
						Clip.Text(Uri);
						break;
					}
					case IDM_COPY_IMG:
					{
						LClipBoard Clip(Html);						
						Clip.Text(ImgSrc);
						break;
					}
					default:
					{
						if (Html->GetEnv())
							Html->GetEnv()->OnMenu(Html, Id, a);
						break;
					}
				}
			}

			Processed = true;
		}
	}
	else if (m.Down() && m.Left())
	{
		#ifdef _DEBUG
		if (m.Ctrl())
		{
			auto Style = ToString();
			LStringPipe p(256);
			p.Print("Tag: %s\n", Tag ? Tag.Get() : "CONTENT");
			if (Class.Length())
			{
				p.Print("Class(es): ");
				for (unsigned i=0; i<Class.Length(); i++)
				{
					p.Print("%s%s", i?", ":"", Class[i].Get());
				}
				p.Print("\n");
			}
			if (HtmlId)
			{
				p.Print("Id: %s\n", HtmlId);
			}
			p.Print("Pos: %i,%i   Size: %i,%i\n\n", Pos.x, Pos.y, Size.x, Size.y);
			p.Print("Style:\n", Style.Get());
			LToken s(Style, "\n");
			for (unsigned i=0; i<s.Length(); i++)
				p.Print("    %s\n", s[i]);
			
			p.Print("\nParent tags:\n");
			LDisplayString Sp(LSysFont, " ");
			for (LTag *t=ToTag(Parent); t && t->Parent; t=ToTag(t->Parent))
			{
				LStringPipe Tmp;
				Tmp.Print("    %s", t->Tag ? t->Tag.Get() : "CONTENT");
				if (t->HtmlId)
				{
					Tmp.Print("#%s", t->HtmlId);
				}
				for (unsigned i=0; i<t->Class.Length(); i++)
				{
					Tmp.Print(".%s", t->Class[i].Get());
				}
				LAutoString Txt(Tmp.NewStr());
				p.Print("%s", Txt.Get());
				LDisplayString Ds(LSysFont, Txt);
				int Px = 170 - Ds.X();
				int Chars = Px / Sp.X();
				for (int c=0; c<Chars; c++)
					p.Print(" ");

				p.Print("(%i,%i)-(%i,%i)\n",
					t->Pos.x,
					t->Pos.y,
					t->Size.x,
					t->Size.y);
			}
			
			LAutoString a(p.NewStr());
			
			LgiMsg(	Html,
					"%s",
					Html->GetClass(),
					MB_OK,
					a.Get());
		}
		else
		#endif
		{
			LString Uri;
			
			if (Html &&
				Html->Environment)
			{
				if (IsAnchor(&Uri))
				{
					if (Uri)
					{
						if (!Html->d->LinkDoubleClick || m.Double())
						{
							Html->Environment->OnNavigate(Html, Uri);
							Processed = true;
						}
					}
					
					const char *OnClk = NULL;
					if (!Processed && Get("onclick", OnClk))
					{
						Html->Environment->OnExecuteScript(Html, (char*)OnClk);
					}
				}
				else
				{
					Processed = OnClick(m);
				}
			}
		}
	}

	return Processed;
}

LTag *LTag::GetBlockParent(ssize_t *Idx)
{
	if (IsBlock())
	{
		if (Idx)
			*Idx = 0;

		return this;
	}

	for (LTag *t = this; t; t = ToTag(t->Parent))
	{
		if (ToTag(t->Parent)->IsBlock())
		{
			if (Idx)
			{
				*Idx = t->Parent->Children.IndexOf(t);
			}

			return ToTag(t->Parent);
		}
	}

	return 0;
}

LTag *LTag::GetAnchor(char *Name)
{
	if (!Name)
		return 0;

	const char *n;
	if (IsAnchor(0) && Get("name", n) && n && !_stricmp(Name, n))
	{
		return this;
	}

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		LTag *Result = t->GetAnchor(Name);
		if (Result) return Result;
	}

	return 0;
}

LTag *LTag::GetTagByName(const char *Name)
{
	if (Name)
	{
		if (Tag && _stricmp(Tag, Name) == 0)
		{
			return this;
		}

		for (unsigned i=0; i<Children.Length(); i++)
		{
			LTag *t = ToTag(Children[i]);
			LTag *Result = t->GetTagByName(Name);
			if (Result) return Result;
		}
	}

	return 0;
}

static int IsNearRect(LRect *r, int x, int y)
{
	if (r->Overlap(x, y))
	{
		return 0;
	}
	else if (x >= r->x1 && x <= r->x2)
	{
		if (y < r->y1)
			return r->y1 - y;
		else
			return y - r->y2;
	}
	else if (y >= r->y1 && y <= r->y2)
	{
		if (x < r->x1)
			return r->x1 - x;
		else
			return x - r->x2;
	}

	int64 dx = 0;
	int64 dy = 0;
	
	if (x < r->x1)
	{
		if (y < r->y1)
		{
			// top left
			dx = r->x1 - x;
			dy = r->y1 - y;
		}
		else
		{
			// bottom left
			dx = r->x1 - x;
			dy = y - r->y2;
		}
	}
	else
	{
		if (y < r->y1)
		{
			// top right
			dx = x - r->x2;
			dy = r->y1 - y;
		}
		else
		{
			// bottom right
			dx = x - r->x2;
			dy = y - r->y2;
		}
	}

	return (int) sqrt( (double) ( (dx * dx) + (dy * dy) ) );
}

ssize_t LTag::NearestChar(LFlowRect *Tr, int x, int y)
{
	LFont *f = GetFont();
	if (f)
	{
		LDisplayString ds(f, Tr->Text, Tr->Len);
		ssize_t c = ds.CharAt(x - Tr->x1);
		
		if (Tr->Text == PreText())
		{
			return 0;
		}
		else
		{
			char16 *t = Tr->Text + c;
			size_t Len = StrlenW(Text());
			if (t >= Text() &&
				t <= Text() + Len)
			{
				return (t - Text()) - GetTextStart();
			}
			else
			{
				LgiTrace("%s:%i - Error getting char at position.\n", _FL);
			}
		}
	}

	return -1;
}

void LTag::GetTagByPos(LTagHit &TagHit, int x, int y, int Depth, bool InBody, bool DebugLog)
{
	/*
	InBody: Originally I had this test in the code but it seems that some test cases
	have actual content after the body. And testing for "InBody" breaks functionality
	for those cases (see "spam4.html" and the unsubscribe link at the end of the doc).	
	*/

	if (TagId == TAG_IMG)
	{
		LRect img(0, 0, Size.x - 1, Size.y - 1);
		if (/*InBody &&*/ img.Overlap(x, y))
		{
			TagHit.Direct = this;
			TagHit.Block = 0;
		}
	}
	else if (/*InBody &&*/ TextPos.Length())
	{
		for (unsigned i=0; i<TextPos.Length(); i++)
		{
			LFlowRect *Tr = TextPos[i];
			if (!Tr)
				break;
			
			bool SameRow = y >= Tr->y1 && y <= Tr->y2;			
			int Near = IsNearRect(Tr, x, y);
			if (Near >= 0 && Near < 100)
			{
				if
				(
					!TagHit.NearestText
					||
					(
						SameRow
						&&
						!TagHit.NearSameRow
					)
					||
					(
						SameRow == TagHit.NearSameRow
						&&
						Near < TagHit.Near
					)
				)
				{
					TagHit.NearestText = this;
					TagHit.NearSameRow = SameRow;
					TagHit.Block = Tr;
					TagHit.Near = Near;
					TagHit.Index = NearestChar(Tr, x, y);
					
					if (DebugLog)
					{
						LgiTrace("%i:GetTagByPos HitText %s #%s, idx=%i, near=%i, txt='%S'\n",
							Depth,
							Tag.Get(),
							HtmlId,
							TagHit.Index,
							TagHit.Near,
							Tr->Text);
					}

					if (!TagHit.Near)
					{
						TagHit.Direct = this;
						TagHit.LocalCoords.x = x;
						TagHit.LocalCoords.y = y;
					}
				}
			}
		}
	}
	else if
	(
		TagId != TAG_TR &&
		Tag &&
		x >= 0 &&
		y >= 0 &&
		x < Size.x &&
		y < Size.y
		// && InBody
	)
	{
		// Direct hit
		TagHit.Direct = this;
		TagHit.LocalCoords.x = x;
		TagHit.LocalCoords.y = y;

		if (DebugLog)
		{
			LgiTrace("%i:GetTagByPos DirectHit %s #%s, idx=%i, near=%i\n",
				Depth,
				Tag.Get(),
				HtmlId,
				TagHit.Index,
				TagHit.Near);
		}
	}
	
	if (TagId == TAG_BODY)
		InBody = true;

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		if (t->Pos.x >= 0 &&
			t->Pos.y >= 0)
		{
			t->GetTagByPos(TagHit, x - t->Pos.x, y - t->Pos.y, Depth + 1, InBody, DebugLog);
		}
	}
}

int LTag::OnNotify(LNotification n)
{
	if (!Ctrl || !Html->InThread())
		return 0;
	
	switch (CtrlType)
	{
		case CtrlSubmit:
		{
			LTag *Form = this;
			while (Form && Form->TagId != TAG_FORM)
				Form = ToTag(Form->Parent);
			if (Form)
				Html->OnSubmitForm(Form);
			break;
		}
		default:
		{
			CtrlValue = Ctrl->Name();
			break;
		}
	}
	
	return 0;
}

void LTag::CollectFormValues(LHashTbl<ConstStrKey<char,false>,char*> &f)
{
	if (CtrlType != CtrlNone)
	{
		const char *Name;
		if (Get("name", Name))
		{
			char *Existing = f.Find(Name);
			if (Existing)
				DeleteArray(Existing);

			char *Val = CtrlValue.Str();
			if (Val)
			{
				LStringPipe p(256);
				for (char *v = Val; *v; v++)
				{
					if (*v == ' ')
						p.Write("+", 1);
					else if (IsAlpha(*v) || IsDigit(*v) || *v == '_' || *v == '.')
						p.Write(v, 1);
					else
						p.Print("%%%02.2X", *v);
				}
				f.Add(Name, p.NewStr());
			}
			else
			{
				f.Add(Name, NewStr(""));
			}
		}
	}

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		t->CollectFormValues(f);
	}
}

LTag *LTag::FindCtrlId(int Id)
{
	if (Ctrl && Ctrl->GetId() == Id)
		return this;

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		LTag *f = t->FindCtrlId(Id);
		if (f)
			return f;
	}
	
	return NULL;
}

void LTag::Find(int TagType, LArray<LTag*> &Out)
{
	if (TagId == TagType)
	{
		Out.Add(this);
	}
	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		t->Find(TagType, Out);
	}
}

void LTag::SetImage(const char *Uri, LSurface *Img)
{
	if (Img)
	{
		if (TagId != TAG_IMG)
		{
			ImageDef *Def = (ImageDef*)LCss::Props.Find(PropBackgroundImage);
			if (Def)
			{
				Def->Type = ImageOwn;
				DeleteObj(Def->Img);
				Def->Img = Img;
			}
		}
		else
		{
			if (Img->GetColourSpace() == CsIndex8)
			{
				if (Image.Reset(new LMemDC(Img->X(), Img->Y(), System32BitColourSpace)))
				{
					Image->Colour(0, 32);
					Image->Rectangle();
					Image->Blt(0, 0, Img);
				}
				else LgiTrace("%s:%i - SetImage can't promote 8bit image to 32bit.\n", _FL);
			}
			else Image.Reset(Img);
			
			LRect r = XSubRect();
			if (r.Valid())
			{
				LAutoPtr<LSurface> t(new LMemDC(r.X(), r.Y(), Image->GetColourSpace()));
				if (t)
				{
					t->Blt(0, 0, Image, &r);
					Image = t;
				}
			}
		}		

		for (unsigned i=0; i<Children.Length(); i++)
		{
			LTag *t = ToTag(Children[i]);
			if (t->Cell)
			{
				t->Cell->MinContent = 0;
				t->Cell->MaxContent = 0;
			}
		}
	}
	else
	{
		Html->d->Loading.Add(Uri, this);
	}
}

void LTag::LoadImage(const char *Uri)
{
	#if DOCUMENT_LOAD_IMAGES
	if (!Html->Environment)
		return;

	LUri u(Uri);
	bool LdImg = Html->GetLoadImages();
	bool IsRemote = u.sProtocol &&
					(
						!_stricmp(u.sProtocol, "http") ||
						!_stricmp(u.sProtocol, "https") ||
						!_stricmp(u.sProtocol, "ftp")
					);
	if (IsRemote && !LdImg)
	{
		Html->NeedsCapability("RemoteContent");
		return;
	}
	else if (u.IsProtocol("data"))
	{
		if (!u.sPath)
			return;

		const char *s = u.sPath;
		if (*s++ != '/')
			return;
		LAutoString Type(LTokStr(s));
		if (*s++ != ',')
			return;
		auto p = LString(Type).SplitDelimit(",;:");
		if (p.Length() != 2 || !p.Last().Equals("base64"))
			return;
		LString Name = LString("name.") + p[0];
		auto Filter = LFilterFactory::New(Name, FILTER_CAP_READ, NULL);
		if (!Filter)
			return;

		auto slen = strlen(s);
		auto blen = BufferLen_64ToBin(slen);
		LMemStream bin;
		bin.SetSize(blen);
		ConvertBase64ToBinary((uint8_t*)bin.GetBasePtr(), blen, s, slen);
		
		bin.SetPos(0);
		if (!Image.Reset(new LMemDC))
			return;
		auto result = Filter->ReadImage(Image, &bin);
		if (result != LFilter::IoSuccess)
			Image.Reset();
		return;
	}

	LDocumentEnv::LoadJob *j = Html->Environment->NewJob();
	if (j)
	{
		LAssert(Html != NULL);
		j->Uri.Reset(NewStr(Uri));
		j->Env = Html->Environment;
		j->UserData = this;
		j->UserUid = Html->GetDocumentUid();

		// LgiTrace("%s:%i - new job %p, %p\n", _FL, j, j->UserData);

		LDocumentEnv::LoadType Result = Html->Environment->GetContent(j);
		if (Result == LDocumentEnv::LoadImmediate)
		{
			SetImage(Uri, j->pDC.Release());
		}
		else if (Result == LDocumentEnv::LoadDeferred)
		{
			Html->d->DeferredLoads++;
		}
				
		DeleteObj(j);
	}
	#endif
}

void LTag::LoadImages()
{
	const char *Uri = 0;
	if (Html->Environment &&
		TagId == TAG_IMG &&
		!Image)
	{
		if (Get("src", Uri))
			LoadImage(Uri);
	}

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		t->LoadImages();
	}
}

void LTag::ImageLoaded(char *uri, LSurface *Img, int &Used)
{
	const char *Uri = 0;
	if (!Image &&
		Get("src", Uri))
	{
		if (strcmp(Uri, uri) == 0)
		{
			if (Used == 0)
			{
				SetImage(Uri, Img);
			}
			else
			{
				SetImage(Uri, new LMemDC(Img));
			}
			Used++;
		}
	}

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		t->ImageLoaded(uri, Img, Used);
	}
}

struct LTagElementCallback : public LCss::ElementCallback<LTag>
{
	const char *Val;

	const char *GetElement(LTag *obj)
	{
		return obj->Tag;
	}
	
	const char *GetAttr(LTag *obj, const char *Attr)
	{
		if (obj->Get(Attr, Val))
			return Val;
		return NULL;
	}
	
	bool GetClasses(LString::Array &Classes, LTag *obj) 
	{
		Classes = obj->Class;
		return Classes.Length() > 0;
	}
	
	LTag *GetParent(LTag *obj)
	{
		return ToTag(obj->Parent);
	}
	
	LArray<LTag*> GetChildren(LTag *obj)
	{
		LArray<LTag*> c;
		for (unsigned i=0; i<obj->Children.Length(); i++)
			c.Add(ToTag(obj->Children[i]));
		return c;
	}
};

void LTag::RestyleAll()
{
	Restyle();
	for (unsigned i=0; i<Children.Length(); i++)
	{
		LHtmlElement *c = Children[i];
		LTag *t = ToTag(c);
		if (t)
			t->RestyleAll();
	}
}

// After CSS has changed this function scans through the CSS and applies any rules
// that match the current tag.
void LTag::Restyle()
{
	// Use the matching built into the LCss Store.
	LCss::SelArray Styles;
	LTagElementCallback Context;
	if (Html->CssStore.Match(Styles, &Context, this))
	{
		for (unsigned i=0; i<Styles.Length(); i++)
		{
			LCss::Selector *s = Styles[i];
			if (s)
				SetCssStyle(s->Style);
		}
	}
	
	// Do the element specific styles
	const char *s;
	if (Get("style", s))
		SetCssStyle(s);

	#if DEBUG_RESTYLE && defined(_DEBUG)
	if (Debug)
	{
		auto Style = ToString();
		LgiTrace(">>>> %s <<<<:\n%s\n\n", Tag.Get(), Style.Get());
	}
	#endif
}

void LTag::SetStyle()
{
	const static float FntMul[] =
	{
		0.6f,	// size=1
		0.89f,	// size=2
		1.0f,	// size=3
		1.2f,	// size=4
		1.5f,	// size=5
		2.0f,	// size=6
		3.0f	// size=7
	};

	const char *s = 0;
	#ifdef _DEBUG
	if (Get("debug", s))
	{
		if ((Debug = atoi(s)))
		{
			LgiTrace("Debug Tag: %p '%s'\n", this, Tag ? Tag.Get() : "CONTENT");
		}
	}
	#endif

	if (Get("Color", s))
	{
		ColorDef Def;
		if (LHtmlParser::ParseColour(s, Def))
		{
			Color(Def);
		}
	}

	if (Get("Background", s) ||
		Get("bgcolor", s))
	{
		ColorDef Def;
		if (LHtmlParser::ParseColour(s, Def))
		{
			BackgroundColor(Def);
		}
		else
		{
			LCss::ImageDef Img;
			
			Img.Type = ImageUri;
			Img.Uri = s;
			
			BackgroundImage(Img);
			BackgroundRepeat(RepeatBoth);
		}
	}

	switch (TagId)
	{
		default:
		{
			if (!Stricmp(Tag.Get(), "o:p"))
				Display(LCss::DispNone);
			break;
		}
		case TAG_LINK:
		{
			const char *Type, *Href;
			if (Html->Environment &&
				Get("type", Type) &&
				Get("href", Href) &&
				!Stricmp(Type, "text/css") &&
				!Html->CssHref.Find(Href))
			{
				LDocumentEnv::LoadJob *j = Html->Environment->NewJob();
				if (j)
				{
					LAssert(Html != NULL);
					LTag *t = this;
					
					j->Uri.Reset(NewStr(Href));
					j->Env = Html->Environment;
					j->UserData = t;
					j->UserUid = Html->GetDocumentUid();

					LDocumentEnv::LoadType Result = Html->Environment->GetContent(j);
					if (Result == LDocumentEnv::LoadImmediate)
					{
						LStreamI *s = j->GetStream();							
						if (s)
						{
							int Len = (int)s->GetSize();
							if (Len > 0)
							{
								LAutoString a(new char[Len+1]);
								ssize_t r = s->Read(a, Len);
								a[r] = 0;

								Html->CssHref.Add(Href, true);
								Html->OnAddStyle("text/css", a);
							}
						}
					}
					else if (Result == LDocumentEnv::LoadDeferred)
					{
						Html->d->DeferredLoads++;
					}

					DeleteObj(j);
				}
			}
			break;
		}
		case TAG_BLOCKQUOTE:
		{
			MarginTop(Len("8px"));
			MarginBottom(Len("8px"));
			MarginLeft(Len("16px"));

			if (Get("Type", s))
			{
				if (_stricmp(s, "cite") == 0)
				{
					BorderLeft(BorderDef(this, "1px solid blue"));
					PaddingLeft(Len("0.5em"));
					
					/*
					ColorDef Def;
					Def.Type = ColorRgb;
					Def.Rgb32 = Rgb32(0x80, 0x80, 0x80);
					Color(Def);
					*/
				}
			}
			break;
		}
		case TAG_P:
		{
			MarginBottom(Len("1em"));
			break;
		}
		case TAG_A:
		{
			const char *Href;
			if (Get("href", Href))
			{
				ColorDef c;
				c.Type = ColorRgb;
				c.Rgb32 = Rgb32(0, 0, 255);
				Color(c);
				TextDecoration(TextDecorUnderline);
			}
			break;
		}
		case TAG_TABLE:
		{
			Len l;

			if (!Cell)
				Cell = new TblCell;

			if (Get("border", s))
			{
				BorderDef b;
				if (b.Parse(this, s))
				{
					BorderLeft(b);
					BorderRight(b);
					BorderTop(b);
					BorderBottom(b);
				}
			}

			if (Get("cellspacing", s) &&
				l.Parse(s, PropBorderSpacing, ParseRelaxed))
			{
				BorderSpacing(l);
			}
			else
			{
				// BorderSpacing(LCss::Len(LCss::LenPx, 2.0f));
			}

			if (Get("cellpadding", s) &&
				l.Parse(s, Prop_CellPadding, ParseRelaxed))
			{
				_CellPadding(l);
			}

			if (Get("align", s))
			{
				Len l;
				if (l.Parse(s))
					Cell->XAlign = l.Type;
			}
			break;
		}
		case TAG_TD:
		case TAG_TH:
		{
			if (!Cell)
				Cell = new TblCell;

			LTag *Table = GetTable();
			if (Table)
			{
				Len l = Table->_CellPadding();
				if (!l.IsValid())
				{
					l.Type = LCss::LenPx;
					l.Value = DefaultCellPadding;
				}
				PaddingLeft(l);
				PaddingRight(l);
				PaddingTop(l);
				PaddingBottom(l);
			}
			
			if (TagId == TAG_TH)
				FontWeight(LCss::FontWeightBold);
			break;
		}
		case TAG_BODY:
		{
			MarginLeft(Len(Get("leftmargin", s) ? s : DefaultBodyMargin));
			MarginTop(Len(Get("topmargin", s) ? s : DefaultBodyMargin));
			MarginRight(Len(Get("rightmargin", s) ? s : DefaultBodyMargin));
			
			if (Get("text", s))
			{
				ColorDef c;
				if (c.Parse(s))
				{
					Color(c);
				}
			}
			break;
		}
		case TAG_OL:
		case TAG_UL:
		{
			MarginLeft(Len("16px"));
			break;
		}
		case TAG_STRONG:
		case TAG_B:
		{
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_I:
		{
			FontStyle(FontStyleItalic);
			break;
		}
		case TAG_U:
		{
			TextDecoration(TextDecorUnderline);
			break;
		}
		case TAG_SUP:
		{
			VerticalAlign(VerticalSuper);
			FontSize(SizeSmaller);
			break;
		}
		case TAG_SUB:
		{
			VerticalAlign(VerticalSub);
			FontSize(SizeSmaller);
			break;
		}
		case TAG_TITLE:
		{
			Display(LCss::DispNone);
			break;
		}
	}

	if (Get("width", s))
	{
		Len l;
		if (l.Parse(s, PropWidth, ParseRelaxed))
		{
			Width(l);
		}
	}
	if (Get("height", s))
	{
		Len l;
		if (l.Parse(s, PropHeight, ParseRelaxed))
			Height(l);
	}
	if (Get("align", s))
	{
		if (_stricmp(s, "left") == 0) TextAlign(Len(AlignLeft));
		else if (_stricmp(s, "right") == 0) TextAlign(Len(AlignRight));
		else if (_stricmp(s, "center") == 0) TextAlign(Len(AlignCenter));
	}
	if (Get("valign", s))
	{
		if (_stricmp(s, "top") == 0) VerticalAlign(Len(VerticalTop));
		else if (_stricmp(s, "middle") == 0) VerticalAlign(Len(VerticalMiddle));
		else if (_stricmp(s, "bottom") == 0) VerticalAlign(Len(VerticalBottom));
	}

	Get("id", HtmlId);

	if (Get("class", s))
	{
		Class = LString(s).SplitDelimit(" \t");
	}

	Restyle();

	switch (TagId)
	{
		default: break;
	    case TAG_BIG:
	    {
            LCss::Len l;
            l.Type = SizeLarger;
	        FontSize(l);
	        break;
	    }
	    /*
		case TAG_META:
		{
			LAutoString Cs;

			const char *s;
			if (Get("http-equiv", s) &&
				_stricmp(s, "Content-Type") == 0)
			{
				const char *ContentType;
				if (Get("content", ContentType))
				{
					char *CharSet = stristr(ContentType, "charset=");
					if (CharSet)
					{
						char16 *cs = NULL;
						Html->ParsePropValue(CharSet + 8, cs);
						Cs.Reset(WideToUtf8(cs));
						DeleteArray(cs);
					}
				}
			}

			if (Get("name", s) && _stricmp(s, "charset") == 0 && Get("content", s))
			{
				Cs.Reset(NewStr(s));
			}
			else if (Get("charset", s))
			{
				Cs.Reset(NewStr(s));
			}

			if (Cs)
			{
				if (Cs &&
					_stricmp(Cs, "utf-16") != 0 &&
					_stricmp(Cs, "utf-32") != 0 &&
					LGetCsInfo(Cs))
				{
					// Html->SetCharset(Cs);
				}
			}
			break;
		}
		*/
		case TAG_BODY:
		{
			LCss::ColorDef Bk = BackgroundColor();
			if (Bk.Type != ColorInherit)
			{
				// Copy the background up to the LHtml wrapper
				Html->GetCss(true)->BackgroundColor(Bk);
			}
			
			/*
			LFont *f = GetFont();
			if (FontSize().Type == LenInherit)
			{
       		    FontSize(Len(LenPt, (float)f->PointSize()));
			}
			*/
			break;
		}
		case TAG_HEAD:
		{
			Display(DispNone);
			break;
		}
		case TAG_PRE:
		{
			LFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				LAssert(ValidStr(Type.GetFace()));
				FontFamily(StringsDef(Type.GetFace()));
			}
			break;
		}
		case TAG_TR:
			break;
		case TAG_TD:
		case TAG_TH:
		{
			LAssert(Cell != NULL);
			
			const char *s;
			if (Get("colspan", s))
				Cell->Span.x = atoi(s);
			else
				Cell->Span.x = 1;
			if (Get("rowspan", s))
				Cell->Span.y = atoi(s);
			else
				Cell->Span.y = 1;
			
			Cell->Span.x = MAX(Cell->Span.x, 1);
			Cell->Span.y = MAX(Cell->Span.y, 1);
			
			if (Display() == DispInline ||
				Display() == DispInlineBlock)
			{
				Display(DispBlock); // Inline-block TD??? Nope.
			}
			break;
		}
		case TAG_IMG:
		{
			const char *Uri;
			if (Html->Environment &&
				Get("src", Uri))
			{
				// printf("Uri: %s\n", Uri);
				LoadImage(Uri);
			}
			break;
		}
		case TAG_H1:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[5]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H2:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[4]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H3:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[3]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H4:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[2]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H5:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[1]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_H6:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%ipt", (int)((float)Html->DefFont()->PointSize() * FntMul[0]));
			FontSize(Len(s));
			FontWeight(FontWeightBold);
			break;
		}
		case TAG_FONT:
		{
			const char *s = 0;
			if (Get("Face", s))
			{
				char16 *cw = CleanText(s, strlen(s), "utf-8", true);
				char *c8 = WideToUtf8(cw);
				DeleteArray(cw);
				LToken Faces(c8, ",");
				DeleteArray(c8);
				char *face = TrimStr(Faces[0]);
				if (ValidStr(face))
				{
					FontFamily(face);
					DeleteArray(face);
				}
				else
				{
					LgiTrace("%s:%i - No face for font tag.\n", __FILE__, __LINE__);
				}
			}

			if (Get("Size", s))
			{
				bool Digit = false, NonW = false;
				for (auto *c = s; *c; c++)
				{
					if (IsDigit(*c) || *c == '-') Digit = true;
					else if (!IsWhiteSpace(*c)) NonW = true;
				}
				if (Digit && !NonW)
				{
					auto Sz = atoi(s);
					switch (Sz)
					{
						case 1: FontSize(Len(LCss::LenEm, 0.63f)); break;
						case 2: FontSize(Len(LCss::LenEm, 0.82f)); break;
						case 3: FontSize(Len(LCss::LenEm, 1.0f)); break;
						case 4: FontSize(Len(LCss::LenEm, 1.13f)); break;
						case 5: FontSize(Len(LCss::LenEm, 1.5f)); break;
						case 6: FontSize(Len(LCss::LenEm, 2.0f)); break;
						case 7: FontSize(Len(LCss::LenEm, 3.0f)); break;
					}
				}
				else
				{
					FontSize(Len(s));
				}
			}
			break;
		}
		case TAG_SELECT:
		{
			if (!Html->InThread())
				break;

			LAssert(!Ctrl);
			Ctrl = new LCombo(Html->d->NextCtrlId++, 0, 0, 100, LSysFont->GetHeight() + 8, NULL);
			CtrlType = CtrlSelect;
			break;
		}
		case TAG_INPUT:
		{
			if (!Html->InThread())
				break;

			LAssert(!Ctrl);

			const char *Type, *Value = NULL;
			Get("value", Value);
			LAutoWString CleanValue(Value ? CleanText(Value, strlen(Value), "utf-8", true, true) : NULL);
			if (CleanValue)
			{
				CtrlValue = CleanValue;
			}
			
			if (Get("type", Type))
			{
				if (!_stricmp(Type, "password")) CtrlType = CtrlPassword;
				else if (!_stricmp(Type, "email")) CtrlType = CtrlEmail;
				else if (!_stricmp(Type, "text")) CtrlType = CtrlText;
				else if (!_stricmp(Type, "button")) CtrlType = CtrlButton;
				else if (!_stricmp(Type, "submit")) CtrlType = CtrlSubmit;
				else if (!_stricmp(Type, "hidden")) CtrlType = CtrlHidden;

				DeleteObj(Ctrl);
				if (CtrlType == CtrlEmail ||
					CtrlType == CtrlText ||
					CtrlType == CtrlPassword)
				{
					LEdit *Ed;
					LAutoString UtfCleanValue(WideToUtf8(CleanValue));
					Ctrl = Ed = new LEdit(Html->d->NextCtrlId++, 0, 0, 60, LSysFont->GetHeight() + 8, UtfCleanValue);
					if (Ctrl)
					{
						Ed->Sunken(false);
						Ed->Password(CtrlType == CtrlPassword);
					}						
				}
				else if (CtrlType == CtrlButton ||
						 CtrlType == CtrlSubmit)
				{
					LAutoString UtfCleanValue(WideToUtf8(CleanValue));
					if (UtfCleanValue)
					{
						Ctrl = new InputButton(this, Html->d->NextCtrlId++, UtfCleanValue);
					}
				}
			}
			break;
		}
	}

	if (IsBlock())
	{
		LCss::ImageDef bk = BackgroundImage();
		if (bk.Type == LCss::ImageUri &&
			ValidStr(bk.Uri) &&
			!bk.Uri.Equals("transparent"))
		{
			LoadImage(bk.Uri);
		} 
	}
	
	if (Ctrl)
	{
		LFont *f = GetFont();
		if (f)
			Ctrl->SetFont(f, false);
	}
}

void LTag::OnStyleChange(const char *name)
{
	if (!Stricmp(name, "display") && Html)
	{
		Html->Layout(true);
		Html->Invalidate();
	}
}

void LTag::SetCssStyle(const char *Style)
{
	if (Style)
	{
		// Strip out comments
		char *Comment = NULL;
		while ((Comment = strstr((char*)Style, "/*")))
		{
			char *End = strstr(Comment+2, "*/");
			if (!End)
				break;
			for (char *c = Comment; c<End+2; c++)
				*c = ' ';
		}

		// Parse CSS
		const char *Ptr = Style;
		LCss::Parse(Ptr, LCss::ParseRelaxed);
	}
}

char16 *LTag::CleanText(const char *s, ssize_t Len, const char *SourceCs,  bool ConversionAllowed, bool KeepWhiteSpace)
{
	if (!s || Len <= 0)
		return NULL;

	static const char *DefaultCs = "iso-8859-1";
	char16 *t = 0;
	bool DocAndCsTheSame = false;
	if (Html->DocCharSet && Html->Charset)
	{
		DocAndCsTheSame = _stricmp(Html->DocCharSet, Html->Charset) == 0;
	}

	if (SourceCs)
	{
		t = (char16*) LNewConvertCp(LGI_WideCharset, s, SourceCs, Len);
	}
	else if (Html->DocCharSet &&
		Html->Charset &&
		!DocAndCsTheSame &&
		!Html->OverideDocCharset)
	{
		char *DocText = (char*)LNewConvertCp(Html->DocCharSet, s, Html->Charset, Len);
		t = (char16*) LNewConvertCp(LGI_WideCharset, DocText, Html->DocCharSet, -1);
		DeleteArray(DocText);
	}
	else if (Html->DocCharSet)
	{
		t = (char16*) LNewConvertCp(LGI_WideCharset, s, Html->DocCharSet, Len);
	}
	else
	{
		t = (char16*) LNewConvertCp(LGI_WideCharset, s, Html->Charset.Get() ? Html->Charset.Get() : DefaultCs, Len);
	}

	if (t && ConversionAllowed)
	{
		char16 *o = t;
		for (char16 *i=t; *i; )
		{
			switch (*i)
			{
				case '&':
				{
					i++;
					
					if (*i == '#')
					{
						// Unicode Number
						char n[32] = "", *p = n;
						
						i++;
						
						if (*i == 'x' || *i == 'X')
						{
							// Hex number
							i++;
							while (	*i &&
									(
										IsDigit(*i) ||
										(*i >= 'A' && *i <= 'F') ||
										(*i >= 'a' && *i <= 'f')
									) &&
									(p - n) < 31)
							{
								*p++ = (char)*i++;
							}
						}
						else
						{
							// Decimal number
							while (*i && IsDigit(*i) && (p - n) < 31)
							{
								*p++ = (char)*i++;
							}
						}
						*p++ = 0;
						
						char16 Ch = atoi(n);
						if (Ch)
						{
							*o++ = Ch;
						}
						
						if (*i && *i != ';')
							i--;
					}
					else
					{
						// Named Char
						char16 *e = i;
						while (*e && IsAlpha(*e) && *e != ';')
						{
							e++;
						}
						
						LAutoWString Var(NewStrW(i, e-i));							
						char16 Char = LHtmlStatic::Inst->VarMap.Find(Var);
						if (Char)
						{
							*o++ = Char;
							i = e;
						}
						else
						{
							i--;
							*o++ = *i;
						}
					}
					break;
				}
				case '\r':
				{
					break;
				}
				case ' ':
				case '\t':
				case '\n':
				{
					if (KeepWhiteSpace)
					{
						*o++ = *i;
					}
					else
					{
						*o++ = ' ';

						// Skip furthur whitespace
						while (i[1] && IsWhiteSpace(i[1]))
						{
							i++;
						}
					}
					break;
				}
				default:
				{
					// Normal char
					*o++ = *i;
					break;
				}
			}

			if (*i) i++;
			else break;
		}
		*o++ = 0;
	}

	if (t && !*t)
	{
		DeleteArray(t);
	}

	return t;
}

char *LTag::ParseText(char *Doc)
{
	ColorDef c;
	c.Type = ColorRgb;
	c.Rgb32 = LColour(L_WORKSPACE).c32();
	BackgroundColor(c);
	
	TagId = TAG_BODY;
	Tag.Reset(NewStr("body"));
	Info = Html->GetTagInfo(Tag);
	char *OriginalCp = NewStr(Html->Charset);

	LStringPipe Utf16;
	char *s = Doc;
	while (s)
	{
		if (*s == '\r')
		{
			s++;
		}
		else if (*s == '<')
		{
			// Process tag
			char *e = s;
			e++;
			while (*e && *e != '>')
			{
				if (*e == '\"' || *e == '\'')
				{
					char *q = strchr(e + 1, *e);
					if (q) e = q + 1;
					else e++;
				}
				else e++;
			}
			if (*e == '>') e++;

			// Output tag
			Html->SetCharset("iso-8859-1");
			char16 *t = CleanText(s, e - s, NULL, false);
			if (t)
			{
				Utf16.Push(t);
				DeleteArray(t);
			}
			s = e;
		}
		else if (!*s || *s == '\n')
		{
			// Output previous line
			char16 *Line = Utf16.NewStrW();
			if (Line)
			{
				LTag *t = new LTag(Html, this);
				if (t)
				{
					t->Color(LColour(L_TEXT));
					t->Text(Line);
				}
			}

			if (*s == '\n')
			{
				s++;
				LTag *t = new LTag(Html, this);
				if (t)
				{
					t->TagId = TAG_BR;
					t->Tag.Reset(NewStr("br"));
					t->Info = Html->GetTagInfo(t->Tag);
				}
			}
			else break;
		}
		else
		{
			// Seek end of text
			char *e = s;
			while (*e && *e != '\r' && *e != '\n' && *e != '<') e++;
			
			// Output text
			Html->SetCharset(OriginalCp);
			LAutoWString t(CleanText(s, e - s, NULL, false));
			if (t)
			{
				Utf16.Push(t);
			}			
			s = e;
		}
	}
	
	Html->SetCharset(OriginalCp);
	DeleteArray(OriginalCp);

	return 0;
}

bool LTag::ConvertToText(TextConvertState &State)
{
	const static char *Rule = "------------------------------------------------------";
	int DepthInc = 0;

	switch (TagId)
	{
		default: break;
		case TAG_P:
			if (State.GetPrev())
				State.NewLine();
			break;
		case TAG_UL:
		case TAG_OL:
			DepthInc = 2;
			break;
	}

	if (ValidStrW(Txt))
	{
		for (int i=0; i<State.Depth; i++)
			State.Write("  ", 2);
		
		if (TagId == TAG_LI)
			State.Write("* ", 2);

		LFont *f = GetFont();
		LAutoString u;
		if (f)
			u = f->ConvertToUnicode(Txt);
		else
			u.Reset(WideToUtf8(Txt));
		if (u)
		{
			size_t u_len = strlen(u);
			State.Write(u, u_len);
		}
	}
	
	State.Depth += DepthInc;
	
	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *c = ToTag(Children[i]);
		c->ConvertToText(State);
	}

	State.Depth -= DepthInc;
	
	if (IsBlock())
	{
		if (State.CharsOnLine)
			State.NewLine();
	}
	else
	{
		switch (TagId)
		{
			case TAG_A:
			{
				// Emit the link to the anchor if it's different from the text of the span...
				const char *Href;
				if (Get("href", Href) &&
					ValidStrW(Txt))
				{
					if (_strnicmp(Href, "mailto:", 7) == 0)
						Href += 7;
						
					size_t HrefLen = strlen(Href);
					LAutoWString h(CleanText(Href, HrefLen, "utf-8"));
					if (h && StrcmpW(h, Txt) != 0)
					{
						// Href different from the text of the link
						State.Write(" (", 2);
						State.Write(Href, HrefLen);
						State.Write(")", 1);
					}
				}
				break;
			}
			case TAG_HR:
			{
				State.Write(Rule, strlen(Rule));
				State.NewLine();
				break;
			}
			case TAG_BR:
			{
				State.NewLine();
				break;
			}
			default:
				break;
		}
	}
	
	return true;
}

char *LTag::NextTag(char *s)
{
	while (s && *s)
	{
		char *n = strchr(s, '<');
		if (n)
		{
			if (!n[1])
				return NULL;

			if (IsAlpha(n[1]) || strchr("!/", n[1]) || n[1] == '?')
			{
				return n;
			}

			s = n + 1;
		}
		else break;
	}

	return 0;
}

void LHtml::CloseTag(LTag *t)
{
	if (!t)
		return;

	OpenTags.Delete(t);
}

bool LTag::OnUnhandledColor(LCss::ColorDef *def, const char *&s)
{
	const char *e = s;
	while (*e && (IsText(*e) || *e == '_'))
		e++;

	char tmp[256];
	ssize_t len = e - s;
	memcpy(tmp, s, len);
	tmp[len] = 0;
	int m = LHtmlStatic::Inst->ColourMap.Find(tmp);
	s = e;

	if (m >= 0)
	{
		def->Type = LCss::ColorRgb;
		def->Rgb32 = Rgb24To32(m);
		return true;
	}

	return false;
}

void LTag::ZeroTableElements()
{
	if (TagId == TAG_TABLE ||
		TagId == TAG_TR ||
		IsTableCell(TagId))
	{
		Size.x = 0;
		Size.y = 0;
		if (Cell)
		{
			Cell->MinContent = 0;
			Cell->MaxContent = 0;
		}

		for (unsigned i=0; i<Children.Length(); i++)
		{
			LTag *t = ToTag(Children[i]);
			t->ZeroTableElements();
		}
	}
}

void LTag::ResetCaches()
{
	/*
	If during the parse process a callback causes a layout to happen then it's possible
	to have partial information in the LHtmlTableLayout structure, like missing TD cells.
	Because they haven't been parsed yet.
	This is called at the end of the parsing to reset all the cached info in LHtmlTableLayout.
	That way when the first real layout happens all the data is there.
	*/
	if (Cell)
		DeleteObj(Cell->Cells);
	for (size_t i=0; i<Children.Length(); i++)
		ToTag(Children[i])->ResetCaches();
}

LPoint LTag::GetTableSize()
{
	LPoint s(0, 0);
	
	if (Cell && Cell->Cells)
	{
		Cell->Cells->GetSize(s.x, s.y);
	}

	return s;
}

LTag *LTag::GetTableCell(int x, int y)
{
	LTag *t = this;
	while (	t &&
			!t->Cell &&
			!t->Cell->Cells &&
			t->Parent)
	{
		t = ToTag(t->Parent);
	}
	
	if (t && t->Cell && t->Cell->Cells)
	{
		return t->Cell->Cells->Get(x, y);
	}

	return 0;
}

// This function gets the largest and smallest piece of content
// in this cell and all it's children.
bool LTag::GetWidthMetrics(LTag *Table, uint16 &Min, uint16 &Max)
{
	bool Status = true;
	int MarginPx = 0;
	int LineWidth = 0;

	if (Display() == LCss::DispNone)
		return true;

	// Break the text into words and measure...
	if (Text())
	{
		int MinContent = 0;
		int MaxContent = 0;

		LFont *f = GetFont();
		if (f)
		{
			for (char16 *s = Text(); s && *s; )
			{
				// Skip whitespace...
				while (*s && StrchrW(WhiteW, *s)) s++;
				
				// Find end of non-whitespace
				char16 *e = s;
				while (*e && !StrchrW(WhiteW, *e)) e++;
				
				// Find size of the word
				ssize_t Len = e - s;
				if (Len > 0)
				{
					LDisplayString ds(f, s, Len);
					MinContent = MAX(MinContent, ds.X());
				}
				
				// Move to the next word.
				s = (*e) ? e + 1 : 0;
			}

			LDisplayString ds(f, Text());
			LineWidth = MaxContent = ds.X();
		}

		#if 0//def _DEBUG
		if (Debug)
		{
			LgiTrace("GetWidthMetrics Font=%p Sz=%i,%i\n", f, MinContent, MaxContent);
		}
		#endif
		
		Min = MAX(Min, MinContent);
		Max = MAX(Max, MaxContent);
	}

	// Specific tag handling?
	switch (TagId)
	{
		default:
		{
			if (IsBlock())
			{
				MarginPx = (int)(BorderLeft().ToPx() +
								BorderRight().ToPx() +
								PaddingLeft().ToPx() +
								PaddingRight().ToPx());
			}
			break;
		}
		case TAG_IMG:
		{
			Len w = Width();
			if (w.IsValid())
			{
				int x = (int) w.Value;
				Min = MAX(Min, x);
				Max = MAX(Max, x);
			}
			else if (Image)
			{
				Min = Max = Image->X();
			}
			else
			{
				Size.x = Size.y = DefaultImgSize;
				Min = MAX(Min, Size.x);
				Max = MAX(Max, Size.x);
			}
			break;
		}
		case TAG_TD:
		case TAG_TH:
		{
			Len w = Width();
			if (w.IsValid())
			{
				if (w.IsDynamic())
				{
					Min = MAX(Min, (int)w.Value);
					Max = MAX(Max, (int)w.Value);
				}
				else
				{
					Max = w.ToPx(0, GetFont());
				}
			}
			else
			{
				LCss::BorderDef BLeft = BorderLeft();
				LCss::BorderDef BRight = BorderRight();
				LCss::Len PLeft = PaddingLeft();
				LCss::Len PRight = PaddingRight();
				
				MarginPx = (int)(PLeft.ToPx()  +
								PRight.ToPx()  + 
								BLeft.ToPx());
				
				if (Table->BorderCollapse() == LCss::CollapseCollapse)
					MarginPx += BRight.ToPx();
			}
			break;
		}
		case TAG_TABLE:
		{
			Len w = Width();
			if (w.IsValid() && !w.IsDynamic())
			{
				// Fixed width table...
				int CellSpacing = BorderSpacing().ToPx(Min, GetFont());
				
				int Px = ((int)w.Value) + (CellSpacing << 1);
				Min = MAX(Min, Px);
				Max = MAX(Max, Px);
				return true;
			}
			else
			{
				LPoint s;
				LHtmlTableLayout c(this);
				c.GetSize(s.x, s.y);

				// Auto layout table
				LArray<int> ColMin, ColMax;
				for (int y=0; y<s.y; y++)
				{
					for (int x=0; x<s.x;)
					{
						LTag *t = c.Get(x, y);
						if (t)
						{
							uint16 a = 0, b = 0;							
							if (t->GetWidthMetrics(Table, a, b))
							{
								ColMin[x] = MAX(ColMin[x], a);
								ColMax[x] = MAX(ColMax[x], b);
							}
							
							x += t->Cell->Span.x;
						}
						else break;
					}
				}
				
				int MinSum = 0, MaxSum = 0;
				for (int i=0; i<s.x; i++)
				{
					MinSum += ColMin[i];
					MaxSum += ColMax[i];
				}
				
				Min = MAX(Min, MinSum);
				Max = MAX(Max, MaxSum);
				return true;
			}
			break;
		}
	}

	for (unsigned i = 0; i < Children.Length(); i++)
	{
		LTag *c = ToTag(Children[i]);
		uint16 TagMax = 0;
		
		Status &= c->GetWidthMetrics(Table, Min, TagMax);
		LineWidth += TagMax;
		if (c->TagId == TAG_BR ||
			c->TagId == TAG_LI)
		{
			Max = MAX(Max, LineWidth);
			LineWidth = 0;
		}
	}
	Max = MAX(Max, LineWidth);

	Min += MarginPx;
	Max += MarginPx;
	
	return Status;
}

static void DistributeSize(LArray<int> &a, int Start, int Span, int Size, int Border)
{
	// Calculate the current size of the cells
	int Cur = -Border;
	for (int i=0; i<Span; i++)
	{
		Cur += a[Start+i] + Border;
	}

	// Is there more to allocate?
	if (Cur < Size)
	{
		// Distribute size amongst the rows
		int Needs = Size - Cur;
		int Part = Needs / Span;
		for (int k=0; k<Span; k++)
		{
			int Add = k < Span - 1 ? Part : Needs;
			a[Start+k] = a[Start+k] + Add;
			Needs -= Add;
		}
	}
}

template <class T>
T Sum(LArray<T> &a)
{
	T s = 0;
	for (unsigned i=0; i<a.Length(); i++)
		s += a[i];
	return s;
}

void LTag::LayoutTable(LFlowRegion *f, uint16 Depth)
{
	if (!Cell->Cells)
	{
		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Debug)
		{
			//int asd=0;
		}
		#endif
		Cell->Cells = new LHtmlTableLayout(this);
		#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
		if (Cell->Cells && Debug)
			Cell->Cells->Dump();
		#endif
	}
	if (Cell->Cells)
		Cell->Cells->LayoutTable(f, Depth);
}

void LHtmlTableLayout::AllocatePx(int StartCol, int Cols, int MinPx, bool HasToFillAllAvailable)
{
	// Get the existing total size and size of the column set
	int CurrentTotalX = GetTotalX();
	int CurrentSpanX = GetTotalX(StartCol, Cols);
	int MaxAdditionalPx = AvailableX - CurrentTotalX;
	if (MaxAdditionalPx <= 0)
		return;

	// Calculate the maximum space we have for this column set
	int AvailPx = (CurrentSpanX + MaxAdditionalPx) - BorderX1 - BorderX2;
	
	// Allocate any remaining space...
	int RemainingPx = MaxAdditionalPx;
	LArray<int> Growable, NonGrowable, SizeInherit;
	int GrowablePx = 0;
	for (int x=StartCol; x<StartCol+Cols; x++)
	{
		int DiffPx = MaxCol[x] - MinCol[x];
		if (DiffPx > 0)
		{
			GrowablePx += DiffPx;
			Growable.Add(x);
		}
		else if (MinCol[x] > 0)
		{
			NonGrowable.Add(x);
		}
		else if (MinCol[x] == 0 && CurrentSpanX < AvailPx)
		{
			// Growable.Add(x);
		}
		
		if (SizeCol[x].Type == LCss::LenInherit)
			SizeInherit.Add(x);
	}
	if (GrowablePx < RemainingPx && HasToFillAllAvailable)
	{
		if (Growable.Length() == 0)
		{
			// Add any suitable non-growable columns as well
			for (unsigned i=0; i<NonGrowable.Length(); i++)
			{
				int Col = NonGrowable[i];
				
				LCss::Len c = SizeCol[Col];
				if (c.Type != LCss::LenPercent && c.IsDynamic())
					Growable.Add(Col);
			}
		}
		
		if (Growable.Length() == 0)
		{
			// Still nothing to grow... so just pick the largest column
			int Largest = -1;
			for (int i=StartCol; i<StartCol+Cols; i++)
			{
				if (Largest < 0 || MinCol[i] > MinCol[Largest])
					Largest = i;
			}
			Growable.Add(Largest);
		}
	}
	
	if (Growable.Length())
	{
		// Some growable columns...
		int Added = 0;

		// Reasonably increase the size of the columns...
		for (unsigned i=0; i<Growable.Length(); i++)
		{
			int x = Growable[i];
			int DiffPx = MaxCol[x] - MinCol[x];
			int AddPx = 0;
			if (GrowablePx < RemainingPx && DiffPx > 0)
			{
				AddPx = DiffPx;
			}
			else if (DiffPx > 0)
			{
				double Ratio = (double)DiffPx / GrowablePx;
				AddPx = (int) (Ratio * RemainingPx);
			}
			else
			{
				AddPx = RemainingPx / (int)Growable.Length();
			}
								
			LAssert(AddPx >= 0);
			MinCol[x] += AddPx;
			LAssert(MinCol[x] >= 0);
			Added += AddPx;
		}

		if (Added < RemainingPx && HasToFillAllAvailable)
		{
			// Still more to add, so
			if (SizeInherit.Length())
			{
				Growable = SizeInherit;
			}
			else
			{
				int Largest = -1;
				for (unsigned i=0; i<Growable.Length(); i++)
				{
					int x = Growable[i];
					if (Largest < 0 || MinCol[x] > MinCol[Largest])
						Largest = x;
				}
				Growable.Length(1);
				Growable[0] = Largest;
			}

			int AddPx = (RemainingPx - Added) / (int)Growable.Length();
			for (unsigned i=0; i<Growable.Length(); i++)
			{
				int x = Growable[i];
				if (i == Growable.Length() - 1)
				{
					MinCol[x] += RemainingPx - Added;
					LAssert(MinCol[x] >= 0);
				}
				else
				{
					MinCol[x] += AddPx;
					LAssert(MinCol[x] >= 0);
					Added += AddPx;
				}
			}
		}
	}
}

struct ColInfo
{
	int Large;
	int Growable;
	int Idx;
	int Px;
};

int ColInfoCmp(ColInfo *a, ColInfo *b)
{
	int LDiff = b->Large - a->Large;
	int LGrow = b->Growable - a->Growable;
	int LSize = b->Px - a->Px;
	return LDiff + LGrow + LSize;
}

void LHtmlTableLayout::DeallocatePx(int StartCol, int Cols, int MaxPx)
{
	int TotalPx = GetTotalX(StartCol, Cols);
	if (TotalPx <= MaxPx || MaxPx == 0)
		return;
	
	int TrimPx = TotalPx - MaxPx;
	LArray<ColInfo> Inf;
	int HalfMax = MaxPx >> 1;
	unsigned Interesting = 0;
	int InterestingPx = 0;
	
	for (int x=StartCol; x<StartCol+Cols; x++)
	{
		ColInfo &ci = Inf.New();
		ci.Idx = x;
		ci.Px = MinCol[x];
		ci.Large = MinCol[x] > HalfMax;
		ci.Growable = MinCol[x] < MaxCol[x];
		if (ci.Large || ci.Growable)
		{
			Interesting++;
			InterestingPx += ci.Px;
		}
	}
	
	Inf.Sort(ColInfoCmp);
	
	if (InterestingPx > 0)
	{
		for (unsigned i=0; i<Interesting; i++)
		{
			ColInfo &ci = Inf[i];
			double r = (double)ci.Px / InterestingPx;
			int DropPx = (int) (r * TrimPx);
			if (DropPx < MinCol[ci.Idx])
			{
				MinCol[ci.Idx] -= DropPx;
				LAssert(MinCol[ci.Idx] >= 0);
			}
			else
				break;
		}
	}
}

int LHtmlTableLayout::GetTotalX(int StartCol, int Cols)
{
	if (Cols < 0)
		Cols = s.x;
	
	int TotalX = BorderX1 + BorderX2 + CellSpacing;
	for (int x=StartCol; x<Cols; x++)
		TotalX += MinCol[x] + CellSpacing;
	
	return TotalX;
}

void LHtmlTableLayout::LayoutTable(LFlowRegion *f, uint16 Depth)
{
	GetSize(s.x, s.y);
	if (s.x == 0 || s.y == 0)
	{
		return;
	}

	Table->ZeroTableElements();
	MinCol.Length(0);
	MaxCol.Length(0);
	MaxRow.Length(0);
	SizeCol.Length(0);
	
	LCss::Len BdrSpacing = Table->BorderSpacing();
	CellSpacing = BdrSpacing.IsValid() ? (int)BdrSpacing.Value : 0;

	// Resolve total table width.
	TableWidth = Table->Width();
	if (TableWidth.IsValid())
		AvailableX = f->ResolveX(TableWidth, Table, false);
	else
		AvailableX = f->X();

	LCss::Len MaxWidth = Table->MaxWidth();
	if (MaxWidth.IsValid())
	{
	    int Px = f->ResolveX(MaxWidth, Table, false);
	    if (Px < AvailableX)
	        AvailableX = Px;
	}
	
	TableBorder = f->ResolveBorder(Table, Table);
	if (Table->BorderCollapse() != LCss::CollapseCollapse)
		TablePadding = f->ResolvePadding(Table, Table);
	else
		TablePadding.ZOff(0, 0);
	
	BorderX1 = TableBorder.x1 + TablePadding.x1;
	BorderX2 = TableBorder.x2 + TablePadding.x2;
	
	#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
	if (Table->Debug)
		LgiTrace("AvailableX=%i, BorderX1=%i, BorderX2=%i\n", AvailableX, BorderX1, BorderX2);
	#endif

	#ifdef _DEBUG
	if (Table->Debug)
	{
		printf("Table Debug\n");
	}
	#endif

	// Size detection pass
	int y;
	for (y=0; y<s.y; y++)
	{
		for (int x=0; x<s.x; )
		{
			LTag *t = Get(x, y);
			if (t)
			{
				// This is needed for the metrics...
				t->GetFont();
				t->Cell->BorderPx = f->ResolveBorder(t, t);
				t->Cell->PaddingPx = f->ResolvePadding(t, t);

				if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
				{
					LCss::DisplayType Disp = t->Display();
					if (Disp == LCss::DispNone)
						continue;

					LCss::Len Content = t->Width();
					if (Content.IsValid() && t->Cell->Span.x == 1)
					{
						if (SizeCol[x].IsValid())
						{
							int OldPx = f->ResolveX(SizeCol[x], t, false);
							int NewPx = f->ResolveX(Content, t, false);
							if (NewPx > OldPx)
							{
								SizeCol[x] = Content;
							}
						}
						else
						{
							SizeCol[x] = Content;
						}
					}
					
					if (!t->GetWidthMetrics(Table, t->Cell->MinContent, t->Cell->MaxContent))
					{
						t->Cell->MinContent = 16;
						t->Cell->MaxContent = 16;
					}
					
					#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
					if (Table->Debug)
						LgiTrace("Content[%i,%i] MIN=%i MAX=%i\n", x, y, t->Cell->MinContent, t->Cell->MaxContent);
					#endif

					if (t->Cell->Span.x == 1)
					{
						int BoxPx = t->Cell->BorderPx.x1 +
									t->Cell->BorderPx.x2 +
									t->Cell->PaddingPx.x1 +
									t->Cell->PaddingPx.x2;

						MinCol[x] = MAX(MinCol[x], t->Cell->MinContent + BoxPx);
						LAssert(MinCol[x] >= 0);
						MaxCol[x] = MAX(MaxCol[x], t->Cell->MaxContent + BoxPx);
					}
				}
				
				x += t->Cell->Span.x;
			}
			else break;
		}
	}
	
	// How much space used so far?
	int TotalX = GetTotalX();
	if (TotalX > AvailableX)
	{
		// FIXME:
		// Off -> 'cisra-cqs.html' renders correctly.
		// On -> 'cisra_outage.html', 'steam1.html' renders correctly.
		#if 1
		DeallocatePx(0, (int)MinCol.Length(), AvailableX);
		TotalX = GetTotalX();
		#endif
	}

	#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
	#define DumpCols(msg) \
		if (Table->Debug) \
		{ \
			LgiTrace("%s Ln%i - TotalX=%i AvailableX=%i\n", msg, __LINE__, TotalX, AvailableX); \
			for (unsigned i=0; i<MinCol.Length(); i++) \
				LgiTrace("\t[%i] = %i/%i\n", i, MinCol[i], MaxCol[i]); \
		}

	#else
	#define DumpCols(msg)
	#endif

	DumpCols("AfterSingleCells");

	#ifdef _DEBUG
	if (Table->Debug)
	{
		printf("TableDebug\n");
	}
	#endif

	// Process spanned cells
	for (y=0; y<s.y; y++)
	{
		for (int x=0; x<s.x; )
		{
			LTag *t = Get(x, y);
			if (t && t->Cell->Pos.x == x && t->Cell->Pos.y == y)
			{
				if (t->Cell->Span.x > 1 || t->Cell->Span.y > 1)
				{
					int i;
					int ColMin = -CellSpacing;
					int ColMax = -CellSpacing;
					for (i=0; i<t->Cell->Span.x; i++)
					{
						ColMin += MinCol[x + i] + CellSpacing;
						ColMax += MaxCol[x + i] + CellSpacing;
					}
					
					LCss::Len Width = t->Width();
					if (Width.IsValid())
					{
						int Px = f->ResolveX(Width, t, false);
						t->Cell->MinContent = MAX(t->Cell->MinContent, Px);
						t->Cell->MaxContent = MAX(t->Cell->MaxContent, Px);
					}

					#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
					if (Table->Debug)
						LgiTrace("Content[%i,%i] MIN=%i MAX=%i\n", x, y, t->Cell->MinContent, t->Cell->MaxContent);
					#endif

					if (t->Cell->MinContent > ColMin)
						AllocatePx(t->Cell->Pos.x, t->Cell->Span.x, t->Cell->MinContent, false);
					if (t->Cell->MaxContent > ColMax)
						DistributeSize(MaxCol, t->Cell->Pos.x, t->Cell->Span.x, t->Cell->MaxContent, CellSpacing);
				}

				x += t->Cell->Span.x;
			}
			else break;
		}
	}

	TotalX = GetTotalX();
	DumpCols("AfterSpannedCells");

	// Sometimes the web page specifies too many percentages:
	// Scale them all.	
	float PercentSum = 0.0f;
	for (int i=0; i<s.x; i++)
	{
		if (SizeCol[i].Type == LCss::LenPercent)
			PercentSum += SizeCol[i].Value;
	}	
	if (PercentSum > 100.0)
	{
		float Ratio = PercentSum / 100.0f;
		for (int i=0; i<s.x; i++)
		{
			if (SizeCol[i].Type == LCss::LenPercent)
				SizeCol[i].Value /= Ratio;
		}
	}
	
	// Do minimum column size from CSS values
	if (TotalX < AvailableX)
	{
		for (int x=0; x<s.x; x++)
		{
			LCss::Len w = SizeCol[x];
			if (w.IsValid())
			{
				int Px = f->ResolveX(w, Table, false);
				
				if (w.Type == LCss::LenPercent)
				{
					MaxCol[x] = Px;
				}
				else if (Px > MinCol[x])
				{
					int RemainingPx = AvailableX - TotalX;
					int AddPx = Px - MinCol[x];
					AddPx = MIN(RemainingPx, AddPx);
					
					TotalX += AddPx;
					MinCol[x] += AddPx;
					LAssert(MinCol[x] >= 0);
				}
			}
		}
	}

	TotalX = GetTotalX();
	DumpCols("AfterCssNonPercentageSizes");
	
	if (TotalX > AvailableX)
	{
		#if !ALLOW_TABLE_GROWTH
		// Deallocate space if overused
		// Take some from the largest column
		int Largest = 0;
		for (int i=0; i<s.x; i++)
		{
			if (MinCol[i] > MinCol[Largest])
			{
				Largest = i;
			}
		}
		int Take = TotalX - AvailableX;
		if (Take < MinCol[Largest])
		{
			MinCol[Largest] = MinCol[Largest] - Take;
			LAssert(MinCol[Largest] >= 0);
			TotalX -= Take;
		}

		DumpCols("AfterSpaceDealloc");
		#endif
	}
	else if (TotalX < AvailableX)
	{
		AllocatePx(0, s.x, AvailableX, TableWidth.IsValid());
		DumpCols("AfterRemainingAlloc");
	}

	// Layout cell horizontally and then flow the contents to get 
	// the height of all the cells
	LArray<LRect> RowPad;
	MaxRow.Length(s.y);
	for (y=0; y<s.y; y++)
	{
		int XPos = CellSpacing;
		for (int x=0; x<s.x; )
		{
			auto t = Get(x, y);
			if (!t)
			{
				x++;
				continue;
			}

			if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
			{
				t->Pos.x = XPos;
				t->Size.x = -CellSpacing;
				XPos -= CellSpacing;
					
				RowPad[y].y1 = MAX(RowPad[y].y1, t->Cell->BorderPx.y1 + t->Cell->PaddingPx.y1);
				RowPad[y].y2 = MAX(RowPad[y].y2, t->Cell->BorderPx.y2 + t->Cell->PaddingPx.y2);
					
				LRect Box(0, 0, -CellSpacing, 0);
				for (int i=0; i<t->Cell->Span.x; i++)
				{
					int ColSize = MinCol[x + i] + CellSpacing;
					LAssert(ColSize >= 0);
					if (ColSize < 0)
						break;
					t->Size.x += ColSize;
					XPos += ColSize;
					Box.x2 += ColSize;
				}
					
				LCss::Len Ht = t->Height();
				LFlowRegion r(Table->Html, Box, true);

				t->OnFlow(&r, Depth+1);

				if (r.MAX.y > r.y2)
				{
					t->Size.y = MAX(r.MAX.y, t->Size.y);
				}

					
				if (Ht.IsValid() &&
					Ht.Type != LCss::LenPercent)
				{
					int h = f->ResolveY(Ht, t, false);
					t->Size.y = MAX(h, t->Size.y);

					DistributeSize(MaxRow, y, t->Cell->Span.y, t->Size.y, CellSpacing);
				}
			}

			x += t->Cell->Span.x;
		}
	}

	#if defined(_DEBUG)
	DEBUG_LOG("%s:%i - AfterCellFlow\n", _FL);
	for (unsigned i=0; i<MaxRow.Length(); i++)
		DEBUG_LOG("[%i] = %i\n", i, MaxRow[i]);
	#endif

	// Calculate row height
	for (y=0; y<s.y; y++)
	{
		for (int x=0; x<s.x; )
		{
			LTag *t = Get(x, y);
			if (t)
			{
				if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
				{
					LCss::Len Ht = t->Height();
					if (!(Ht.IsValid() && Ht.Type != LCss::LenPercent))
					{
						DistributeSize(MaxRow, y, t->Cell->Span.y, t->Size.y, CellSpacing);
					}
				}

				x += t->Cell->Span.x;
			}
			else break;
		}			
	}
	
	// Cell positioning
	int Cx = BorderX1 + CellSpacing;
	int Cy = TableBorder.y1 + TablePadding.y1 + CellSpacing;
	
	for (y=0; y<s.y; y++)
	{
		LTag *Prev = 0;
		for (int x=0; x<s.x; )
		{
			LTag *t = Get(x, y);
			if (!t && Prev)
			{
				// Add missing cell
				LTag *Row = ToTag(Prev->Parent);
				if (Row && Row->TagId == TAG_TR)
				{
					t = new LTag(Table->Html, Row);
					if (t)
					{
						t->TagId = TAG_TD;
						t->Tag.Reset(NewStr("td"));
						t->Info = Table->Html->GetTagInfo(t->Tag);
						if ((t->Cell = new LTag::TblCell))
						{
							t->Cell->Pos.x = x;
							t->Cell->Pos.y = y;
							t->Cell->Span.x = 1;
							t->Cell->Span.y = 1;
						}
						t->BackgroundColor(LCss::ColorDef(LCss::ColorRgb, DefaultMissingCellColour));

						Set(Table);
					}
					else break;
				}
				else break;
			}
			if (t)
			{
				if (t->Cell->Pos.x == x && t->Cell->Pos.y == y)
				{
					int RowPadOffset =	RowPad[y].y1 -
										t->Cell->BorderPx.y1 -
										t->Cell->PaddingPx.y1;
					
					t->Pos.x = Cx;
					t->Pos.y = Cy + RowPadOffset;
					
					t->Size.x = -CellSpacing;
					for (int i=0; i<t->Cell->Span.x; i++)
					{
						int w = MinCol[x + i] + CellSpacing;
						t->Size.x += w;
						Cx += w;
					}
					t->Size.y = -CellSpacing;						
					for (int n=0; n<t->Cell->Span.y; n++)
					{
						t->Size.y += MaxRow[y+n] + CellSpacing;
					}
					
					Table->Size.x = MAX(Cx + BorderX2, Table->Size.x);

					#if defined(_DEBUG) && DEBUG_TABLE_LAYOUT
					if (Table->Debug)
					{
						LgiTrace("cell(%i,%i) = pos(%i,%i)+size(%i,%i)\n",
							t->Cell->Pos.x, t->Cell->Pos.y,
							t->Pos.x, t->Pos.y,
							t->Size.x, t->Size.y);
					}
					#endif
				}
				else
				{
					Cx += t->Size.x + CellSpacing;
				}
				
				x += t->Cell->Span.x;
			}
			else break;
			Prev = t;
		}
		
		Cx = BorderX1 + CellSpacing;
		Cy += MaxRow[y] + CellSpacing;
	}

	switch (Table->Cell->XAlign ? Table->Cell->XAlign : ToTag(Table->Parent)->GetAlign(true))
	{
		case LCss::AlignCenter:
		{
			int fx = f->X();
			int Ox = (fx-Table->Size.x) >> 1;
			Table->Pos.x = f->x1 + MAX(Ox, 0);
			DEBUG_LOG("%s:%i - AlignCenter fx=%i ox=%i pos.x=%i size.x=%i\n", _FL, fx, Ox, Table->Pos.x, Table->Size.x);
			break;
		}
		case LCss::AlignRight:
		{
			Table->Pos.x = f->x2 - Table->Size.x;
			DEBUG_LOG("%s:%i - AlignRight f->x2=%i size.x=%i pos.x=%i\n", _FL, f->x2, Table->Size.x, Table->Pos.x);
			break;
		}
		default:
		{
			Table->Pos.x = f->x1;
			DEBUG_LOG("%s:%i - AlignLeft f->x1=%i size.x=%i pos.x=%i\n", _FL, f->x2, Table->Size.x, Table->Pos.x);
			break;
		}
	}
	
	Table->Pos.y = f->y1;
	Table->Size.y = Cy + TablePadding.y2 + TableBorder.y2;
}

LRect LTag::ChildBounds()
{
	LRect b(0, 0, -1, -1);

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		if (i)
		{
			LRect c = t->GetRect();
			b.Union(&c);
		}
		else
		{
			b = t->GetRect();
		}
	}

	return b;
}

LPoint LTag::AbsolutePos()
{
	LPoint p;
	for (LTag *t=this; t; t=ToTag(t->Parent))
	{
		p += t->Pos;
	}
	return p;
}

void LTag::SetSize(LPoint &s)
{
	Size = s;
}

LHtmlArea::~LHtmlArea()
{
	DeleteObjects();
}

LRect LHtmlArea::Bounds()
{
	LRect n(0, 0, -1, -1);

	for (unsigned i=0; i<Length(); i++)
	{
		LFlowRect *r = (*this)[i];
		if (r)
		{
			if (i)
				n.Union(r);
			else
				n = r;
		}
	}
	
	return n;
}

LRect *LHtmlArea::TopRect(LRegion *c)
{
	LRect *Top = 0;
	
	for (int i=0; i<c->Length(); i++)
	{
		LRect *r = (*c)[i];
		if (!Top || (r && (r->y1 < Top->y1)))
		{
			Top = r;
		}
	}

	return Top;
}

void LHtmlArea::FlowText(LTag *Tag, LFlowRegion *Flow, LFont *Font, int LineHeight, char16 *Text, LCss::LengthType Align)
{
	if (!Flow || !Text || !Font)
		return;

	SetFixedLength(false);
	char16 *Start = Text;
	size_t FullLen = StrlenW(Text);

	#if 1
	if (!Tag->Html->GetReadOnly() && !*Text)
	{
		// Insert a text rect for this tag, even though it's empty.
		// This allows the user to place the cursor on a blank line.
		LFlowRect *Tr = new LFlowRect;
		Tr->Tag = Tag;
		Tr->Text = Text;
		Tr->x1 = Flow->cx;
		Tr->x2 = Tr->x1 + 1;
		Tr->y1 = Flow->y1;
		Tr->y2 = Tr->y1 + Font->GetHeight();
		LAssert(Tr->y2 >= Tr->y1);
		Flow->y2 = MAX(Flow->y2, Tr->y2+1);
		Flow->cx = Tr->x2 + 1;

		Add(Tr);
		Flow->Insert(Tr, Align);
		return;				
	}
	#endif
	
	while (*Text)
	{
		LFlowRect *Tr = new LFlowRect;
		if (!Tr)
			break;

		Tr->Tag = Tag;
	Restart:
		Tr->x1 = Flow->cx;
		Tr->y1 = Flow->y1;

		#if 1 // I removed this at one stage but forget why.
		
		// Remove white space at start of line if not in edit mode..
		if (Tag->Html->GetReadOnly() && Flow->x1 == Flow->cx && *Text == ' ')
		{
			Text++;
			if (!*Text)
			{
				DeleteObj(Tr);
				break;
			}
		}
		#endif
		
		Tr->Text = Text;

		LDisplayString ds(Font, Text, MIN(1024, FullLen - (Text-Start)));
		ssize_t Chars = ds.CharAt(Flow->X());
		bool Wrap = false;
		if (Text[Chars])
		{
			// Word wrap

			// Seek back to the nearest break opportunity
			ssize_t n = Chars;
			while (n > 0 && !StrchrW(WhiteW, Text[n]))
				n--;

			if (n == 0)
			{
				if (Flow->x1 == Flow->cx)
				{
					// Already started from the margin and it's too long to 
					// fit across the entire page, just let it hang off the right edge.

					// Seek to the end of the word
					for (Tr->Len = Chars; Text[Tr->Len] && !StrchrW(WhiteW, Text[Tr->Len]); Tr->Len++)
						;

					// Wrap...
					if (*Text == ' ') Text++;
				}
				else
				{
					// Not at the start of the margin
					Flow->FinishLine();
					goto Restart;
				}
			}
			else
			{
				Tr->Len = n;
				LAssert(Tr->Len > 0);
				Wrap = true;
			}

		}
		else
		{
			// Fits..
			Tr->Len = Chars;
			LAssert(Tr->Len > 0);
		}

		LDisplayString ds2(Font, Tr->Text, Tr->Len);
		Tr->x2 = ds2.X();
		Tr->y2 = LineHeight > 0 ? LineHeight - 1 : 0;
		
		if (Wrap)
		{
			Flow->cx = Flow->x1;
			Flow->y1 += Tr->y2 + 1;
			Tr->x2 = Flow->x2 - Tag->RelX();
		}
		else
		{
			Tr->x2 += Tr->x1 - 1;
			Flow->cx = Tr->x2 + 1;
		}
		Tr->y2 += Tr->y1;
		Flow->y2 = MAX(Flow->y2, Tr->y2 + 1);
		
		Add(Tr);
		Flow->Insert(Tr, Align);
		
		Text += Tr->Len;
		if (Wrap)
		{
			while (*Text == ' ')
				Text++;
		}

		Tag->Size.x = MAX(Tag->Size.x, Tr->x2 + 1);
		Tag->Size.y = MAX(Tag->Size.y, Tr->y2 + 1);
		Flow->MAX.x = MAX(Flow->MAX.x, Tr->x2);
		Flow->MAX.y = MAX(Flow->MAX.y, Tr->y2);

		if (Tr->Len == 0)
			break;
	}
	
	SetFixedLength(true);
}

char16 htoi(char16 c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	LAssert(0);
	return 0;
}

bool LTag::Serialize(LXmlTag *t, bool Write)
{
	LRect pos;
	if (Write)
	{
		// Obj -> Tag
		if (Tag) t->SetAttr("tag", Tag);
		pos.ZOff(Size.x, Size.y);
		pos.Offset(Pos.x, Pos.y);
		t->SetAttr("pos", pos.GetStr());
		t->SetAttr("tagid", TagId);		
		if (Txt)
		{
			LStringPipe p(256);
			for (char16 *c = Txt; *c; c++)
			{
				if (*c > ' ' &&
					*c < 127 &&
					!strchr("%<>\'\"", *c))
					p.Print("%c", (char)*c);
				else
					p.Print("%%%.4x", *c);
			}
			
			LAutoString Tmp(p.NewStr());
			t->SetContent(Tmp);
		}
		if (Props.Length())
		{
			auto CssStyles = ToString();
			LAssert(!strchr(CssStyles, '\"'));
			t->SetAttr("style", CssStyles);
		}
		if (Html->Cursor == this)
		{
			LAssert(Cursor >= 0);
			t->SetAttr("cursor", (int64)Cursor);
		}
		else LAssert(Cursor < 0);
		if (Html->Selection == this)
		{
			LAssert(Selection >= 0);
			t->SetAttr("selection", (int64)Selection);
		}
		else LAssert(Selection < 0);
		
		for (unsigned i=0; i<Children.Length(); i++)
		{
			LXmlTag *child = new LXmlTag("e");
			LTag *tag = ToTag(Children[i]);
			if (!child || !tag)
			{
				LAssert(0);
				return false;
			}
			t->InsertTag(child);
			if (!tag->Serialize(child, Write))
			{
				return false;
			}
		}
	}
	else
	{
		// Tag -> Obj
		Tag.Reset(NewStr(t->GetAttr("tag")));
		TagId = (HtmlTag) t->GetAsInt("tagid");
		pos.SetStr(t->GetAttr("pos"));
		if (pos.Valid())
		{
			Pos.x = pos.x1;
			Pos.y = pos.y1;
			Size.x = pos.x2;
			Size.y = pos.y2;
		}
		if (ValidStr(t->GetContent()))
		{
			LStringPipe p(256);
			char *c = t->GetContent();
			SkipWhiteSpace(c);
			for (; *c && *c > ' '; c++)
			{
				char16 ch;
				if (*c == '%')
				{
					ch = 0;
					for (int i=0; i<4 && *c; i++)
					{
						ch <<= 4;
						ch |= htoi(*++c);
					}
				}
				else ch = *c;
				p.Write(&ch, sizeof(ch));
			}
			Txt.Reset(p.NewStrW());
		}
		const char *s = t->GetAttr("style");
		if (s)
			Parse(s, ParseRelaxed);
		s = t->GetAttr("cursor");
		if (s)
		{
			LAssert(Html->Cursor == NULL);
			Html->Cursor = this;
			Cursor = atoi(s);
			LAssert(Cursor >= 0);			
		}
		s = t->GetAttr("selection");
		if (s)
		{
			LAssert(Html->Selection == NULL);
			Html->Selection = this;
			Selection = atoi(s);
			LAssert(Selection >= 0);			
		}
		#ifdef _DEBUG
		s = t->GetAttr("debug");
		if (s && atoi(s) != 0)
			Debug = true;
		#endif
		
		for (int i=0; i<t->Children.Length(); i++)
		{
			LXmlTag *child = t->Children[i];
			if (child->IsTag("e"))
			{
				LTag *tag = new LTag(Html, NULL);
				if (!tag)
				{
					LAssert(0);
					return false;
				}
				
				if (!tag->Serialize(child, Write))
				{
					return false;
				}
				
				Attach(tag);
			}
		}
	}
	
	return true;
}

/*
/// This method centers the text in the area given to the tag. Used for inline block elements.
void LTag::CenterText()
{
	if (!Parent)
		return;

	// Find the size of the text elements.
	int ContentPx = 0;
	for (unsigned i=0; i<TextPos.Length(); i++)
	{
		LFlowRect *fr = TextPos[i];
		ContentPx += fr->X();
	}
	
	LFont *f = GetFont();
	int ParentPx = ToTag(Parent)->Size.x;
	int AvailPx = Size.x;

	// Remove the border and padding from the content area
	AvailPx -= BorderLeft().ToPx(ParentPx, f);
	AvailPx -= BorderRight().ToPx(ParentPx, f);
	AvailPx -= PaddingLeft().ToPx(ParentPx, f);
	AvailPx -= PaddingRight().ToPx(ParentPx, f);
	if (AvailPx > ContentPx)
	{
		// Now offset all the regions to the right
		int OffPx = (AvailPx - ContentPx) >> 1;
		for (unsigned i=0; i<TextPos.Length(); i++)
		{
			LFlowRect *fr = TextPos[i];
			fr->Offset(OffPx, 0);
		}
	}
}
*/

void LTag::OnFlow(LFlowRegion *Flow, uint16 Depth)
{
	if (Depth >= MAX_RECURSION_DEPTH)
		return;

	DisplayType Disp = Display();
	if (Disp == DispNone)
		return;

	LFont *f = GetFont();
	LFlowRegion Local(*Flow);
	bool Restart = true;
	int BlockFlowWidth = 0;
	const char *ImgAltText = NULL;

	Size.x = 0;
	Size.y = 0;
	
	LCssTools Tools(this, f);
	LRect rc(Flow->X(), Html->Y());
	PadPx = Tools.GetPadding(rc);

	if (TipId)
	{
		Html->Tip.DeleteTip(TipId);
		TipId = 0;
	}

	switch (TagId)
	{
		default:
			break;
		case TAG_BODY:
		{
			Flow->InBody++;
			break;
		}
		case TAG_IFRAME:
		{
			LFlowRegion Temp = *Flow;
			Flow->EndBlock();
			Flow->Indent(this, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);

			// Flow children
			for (unsigned i=0; i<Children.Length(); i++)
			{
				LTag *t = ToTag(Children[i]);
				t->OnFlow(&Temp, Depth + 1);

				if (TagId == TAG_TR)
				{
					Temp.x2 -= MIN(t->Size.x, Temp.X());
				}
			}

			Flow->Outdent(this, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);
			BoundParents();
			return;
			break;
		}
		case TAG_TR:
		{
			Size.x = Flow->X();
			break;
		}
		case TAG_IMG:
		{
			Size.x = Size.y = 0;
			
			LCss::Len w    = Width();
			LCss::Len h    = Height();
			// LCss::Len MinX = MinWidth();
			// LCss::Len MaxX = MaxWidth();
			LCss::Len MinY = MinHeight();
			LCss::Len MaxY = MaxHeight();
			LAutoPtr<LDisplayString> a;
			int ImgX, ImgY;		
			if (Image)
			{
				ImgX = Image->X();
				ImgY = Image->Y();
			}
			else if (Get("alt", ImgAltText) && ValidStr(ImgAltText))
			{
				LDisplayString a(f, ImgAltText);
				ImgX = a.X() + 4;
				ImgY = a.Y() + 4;
			}
			else
			{
				ImgX = DefaultImgSize;
				ImgY = DefaultImgSize;
			}
			
			double AspectRatio = ImgY != 0 ? (double)ImgX / ImgY : 1.0;
			bool XLimit = false, YLimit = false;
			double Scale = 1.0;

			if (w.IsValid() && w.Type != LenAuto)
			{
				Size.x = Flow->ResolveX(w, this, false);
				XLimit = true;
			}
			else
			{
				int Fx = Flow->x2 - Flow->x1 + 1;
				if (ImgX > Fx)
				{
					Size.x = Fx; //  * 0.8;
					if (Image)
						Scale = (double) Fx / ImgX;
				}
				else
				{
					Size.x = ImgX;
				}
			}
			XLimit |= Flow->LimitX(Size.x, MinWidth(), MaxWidth(), f);

			if (h.IsValid() && h.Type != LenAuto)
			{
				Size.y = Flow->ResolveY(h, this, false);
				YLimit = true;
			}
			else
			{
				Size.y = (int) (ImgY * Scale);
			}
			YLimit |= Flow->LimitY(Size.y, MinHeight(), MaxHeight(), f);

			if
			(
				(XLimit ^ YLimit)
				&&
				Image
			)
			{
				if (XLimit)
				{
					Size.y = (int) ceil((double)Size.x / AspectRatio);
				}
				else
				{
					Size.x = (int) ceil((double)Size.y * AspectRatio);
				}
			}
			if (MinY.IsValid())
			{
				int Px = Flow->ResolveY(MinY, this, false);
				if (Size.y < Px)
					Size.y = Px;
			}
			if (MaxY.IsValid())
			{
				int Px = Flow->ResolveY(MaxY, this, false);
				if (Size.y > Px)
					Size.y = Px;
			}

			if (Disp == DispInline ||
				Disp == DispInlineBlock)
			{
				Restart = false;

				if (Flow->cx > Flow->x1 &&
					Size.x > Flow->X())
				{
					Flow->FinishLine();
				}

				Pos.y = Flow->y1;
				Flow->y2 = MAX(Flow->y1, Pos.y + Size.y - 1);

				LCss::LengthType a = GetAlign(true);
				switch (a)
				{
					case AlignCenter:
					{
						int Fx = Flow->x2 - Flow->x1;
						Pos.x = Flow->x1 + ((Fx - Size.x) / 2);
						break;
					}
					case AlignRight:
					{
						Pos.x = Flow->x2 - Size.x;
						break;
					}
					default:
					{
						Pos.x = Flow->cx;
						break;
					}
				}
			}
			break;
		}
		case TAG_HR:
		{
			Flow->FinishLine();
			
			Pos.x = Flow->x1;
			Pos.y = Flow->y1 + 7;
			Size.x = Flow->X();
			Size.y = 2;
			
			Flow->cx ++;
			Flow->y2 += 16;

			Flow->FinishLine();
			return;
			break;
		}
		case TAG_TABLE:
		{
			Flow->EndBlock();
			
			LCss::Len left = GetCssLen(MarginLeft, Margin);
			LCss::Len top = GetCssLen(MarginTop, Margin);
			LCss::Len right = GetCssLen(MarginRight, Margin);
			LCss::Len bottom = GetCssLen(MarginBottom, Margin);
			Flow->Indent(this, left, top, right, bottom, true);

			LayoutTable(Flow, Depth + 1);

			Flow->y1 += Size.y;
			Flow->y2 = Flow->y1;
			Flow->cx = Flow->x1;
			Flow->my = 0;
			Flow->MAX.y = MAX(Flow->MAX.y, Flow->y2);

			Flow->Outdent(this, left, top, right, bottom, true);
			BoundParents();
			return;
		}
	}

	if (Disp == DispBlock || Disp == DispInlineBlock)
	{
		// This is a block level element, so end the previous non-block elements
		if (Disp == DispBlock)
			Flow->EndBlock();
		
		#ifdef _DEBUG
		if (Debug)
			LgiTrace("Before %s\n", Flow->ToString().Get());
		#endif

		BlockFlowWidth = Flow->X();
		
		// Indent the margin...
		LCss::Len left = GetCssLen(MarginLeft, Margin);
		LCss::Len top = GetCssLen(MarginTop, Margin);
		LCss::Len right = GetCssLen(MarginRight, Margin);
		LCss::Len bottom = GetCssLen(MarginBottom, Margin);
		Flow->Indent(this, left, top, right, bottom, true);

		// Set the width if any
		LCss::Len Wid = Width();
		if (!IsTableCell(TagId) && Wid.IsValid())
			Size.x = Flow->ResolveX(Wid, this, false);
		else if (TagId != TAG_IMG)
		{
			if (Disp == DispInlineBlock) // Flow->Inline)
				Size.x = 0; // block inside inline-block default to fit the content
			else
				Size.x = Flow->X();
		}
		else if (Disp == DispInlineBlock)
			Size.x = 0;

		if (MaxWidth().IsValid())
		{
			int Px = Flow->ResolveX(MaxWidth(), this, false);
			if (Size.x > Px)
				Size.x = Px;
		}
		if (MinWidth().IsValid())
		{
			int Px = Flow->ResolveX(MinWidth(), this, false);
			if (Size.x < Px)
				Size.x = Px;
		}

		Pos.x = Disp == DispInlineBlock ? Flow->cx : Flow->x1;
		Pos.y = Flow->y1;

		Flow->y1 -= Pos.y;
		Flow->y2 -= Pos.y;
			
		if (Disp == DispBlock)
		{
			Flow->x1 -= Pos.x;
			Flow->x2 = Flow->x1 + Size.x;
			Flow->cx -= Pos.x;

			Flow->Indent(this, LCss::BorderLeft(), LCss::BorderTop(), LCss::BorderRight(), LCss::BorderBottom(), false);
			Flow->Indent(PadPx, false);
		}
		else
		{
			Flow->x2 = Flow->X();
			Flow->x1 =	Flow->ResolveX(BorderLeft(), this, true) +
						Flow->ResolveX(PaddingLeft(), this, true);
			Flow->cx = Flow->x1;
			Flow->y1 += Flow->ResolveY(BorderTop(), this, true) +
						Flow->ResolveY(PaddingTop(), this, true);
			Flow->y2 = Flow->y1;
			
			if (!IsTableTag())
				Flow->Inline++;
		}
	}
	else
	{
		Flow->Indent(PadPx, false);
	}

	if (f)
	{
		// Clear the previous text layout...
		TextPos.DeleteObjects();

		switch (TagId)
		{
			default:
				break;
			case TAG_LI:
			{
				// Insert the list marker
				if (!PreText())
				{
					LCss::ListStyleTypes s = Parent->ListStyleType();
					if (s == ListInherit)
					{
						if (Parent->TagId == TAG_OL)
							s = ListDecimal;
						else if (Parent->TagId == TAG_UL)
							s = ListDisc;
					}
					
					switch (s)
					{
						default: break;
						case ListDecimal:
						{
							ssize_t Index = Parent->Children.IndexOf(this);
							char Txt[32];
							sprintf_s(Txt, sizeof(Txt), "%i. ", (int)(Index + 1));
							PreText(Utf8ToWide(Txt));
							break;
						}
						case ListDisc:
						{
							PreText(NewStrW(LHtmlListItem));
							break;
						}
					}
				}

				if (PreText())
					TextPos.FlowText(this, Flow, f, f->GetHeight(), PreText(), AlignLeft);
				break;
			}
			case TAG_IMG:
			{
				if (Disp == DispBlock)
				{
					Flow->cx += Size.x;
					Flow->y2 += Size.y;
				}
				break;
			}
		}

		if (Text() && Flow->InBody)
		{
			// Setup the line height cache
			if (LineHeightCache < 0)
			{
				LCss::Len LineHt;
				LFont *LineFnt = GetFont();

				for (LTag *t = this; t && !LineHt.IsValid(); t = ToTag(t->Parent))
				{
					LineHt = t->LineHeight();
					if (t->TagId == TAG_TABLE)
						break;
				}

				if (LineFnt)
				{
					int FontPx = LineFnt->GetHeight();
					
					if (!LineHt.IsValid() ||
						LineHt.Type == LCss::LenAuto ||
						LineHt.Type == LCss::LenNormal)
					{
						LineHeightCache = FontPx;
						// LgiTrace("LineHeight FontPx=%i Px=%i Auto\n", FontPx, LineHeightCache);
					}
					else if (LineHt.Type == LCss::LenPx)
					{
						auto Scale = Html->GetDpiScale().y;
						LineHt.Value *= (float)Scale;
						LineHeightCache = LineHt.ToPx(FontPx, f);
						// LgiTrace("LineHeight FontPx=%i Px=%i (Scale=%f)\n", FontPx, LineHeightCache, Scale);
					}
					else
					{
						LineHeightCache = LineHt.ToPx(FontPx, f);
						// LgiTrace("LineHeight FontPx=%i Px=%i ToPx\n", FontPx, LineHeightCache);
					}
				}
			}

			// Flow in the rest of the text...
			char16 *Txt = Text();
			LCss::LengthType Align = GetAlign(true);
			TextPos.FlowText(this, Flow, f, LineHeightCache, Txt, Align);

			#ifdef _DEBUG
			if (Debug)
				LgiTrace("%s:%i - %p.size=%p\n", _FL, this, &Size.x);
			#endif
		}
	}

	// Flow children
	PostFlowAlign.Length(0);

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);

		switch (t->Position())
		{
			case PosStatic:
			case PosAbsolute:
			case PosFixed:
			{
				LFlowRegion old = *Flow;
				t->OnFlow(Flow, Depth + 1);
				
				// Try and reset the flow to how it was before...
				Flow->x1 = old.x1;
				Flow->x2 = old.x2;
				Flow->cx = old.cx;
				Flow->y1 = old.y1;
				Flow->y2 = old.y2;
				Flow->MAX.x = MAX(Flow->MAX.x, old.MAX.x);
				Flow->MAX.y = MAX(Flow->MAX.y, old.MAX.y);
				break;
			}
			default:
			{
				t->OnFlow(Flow, Depth + 1);
				break;
			}
		}

		if (TagId == TAG_TR)
		{
			Flow->x2 -= MIN(t->Size.x, Flow->X());
		}
	}

	LCss::LengthType XAlign = GetAlign(true);
	int FlowSz = Flow->Width();

	// Align the children...
	for (auto &group: PostFlowAlign)
	{
		int MinX = FlowSz, MaxX = 0;
		for (auto &a: group)
		{
			MinX = MIN(MinX, a.t->Pos.x);
			MaxX = MAX(MaxX, a.t->Pos.x + a.t->Size.x - 1);
		}
		int TotalX = MaxX - MinX + 1;
		int FirstX = group.Length() ? group[0].t->Pos.x : 0;

		for (auto &a: group)
		{
			if (a.XAlign == LCss::AlignCenter)
			{
				int OffX = (Size.x - TotalX) >> 1;
				if (OffX > 0)
				{
					a.t->Pos.x += OffX;
				}
			}
			else if (a.XAlign == LCss::AlignRight)
			{
				int OffX = FlowSz - FirstX - TotalX;
				if (OffX > 0)
				{
					a.t->Pos.x += OffX;
				}
			}
		}
	}

	if (Disp == DispBlock || Disp == DispInlineBlock)
	{		
		LCss::Len Ht = Height();
		LCss::Len MaxHt = MaxHeight();

		// I dunno, there should be a better way... :-(
		if (MarginLeft().Type == LenAuto &&
			MarginRight().Type == LenAuto)
		{
			XAlign = LCss::AlignCenter;
		}

		bool AcceptHt = !IsTableCell(TagId) && Ht.Type != LenPercent;
		if (AcceptHt)
		{
			if (Ht.IsValid())
			{
				int HtPx = Flow->ResolveY(Ht, this, false);
				if (HtPx > Flow->y2)
					Flow->y2 = HtPx;
			}
			if (MaxHt.IsValid())
			{
				int MaxHtPx = Flow->ResolveY(MaxHt, this, false);
				if (MaxHtPx < Flow->y2)
				{
					Flow->y2 = MaxHtPx;
					Flow->MAX.y = MIN(Flow->y2, Flow->MAX.y);
				}
			}
		}

		if (Disp == DispBlock)
		{
			Flow->EndBlock();

			int OldFlowSize = Flow->x2 - Flow->x1 + 1;
			Flow->Outdent(this, PaddingLeft(), PaddingTop(), PaddingRight(), PaddingBottom(), false);
			Flow->Outdent(this, LCss::BorderLeft(), LCss::BorderTop(), LCss::BorderRight(), LCss::BorderBottom(), false);

			Size.y = Flow->y2 > 0 ? Flow->y2 : 0;

			Flow->Outdent(this, MarginLeft(), MarginTop(), MarginRight(), MarginBottom(), true);
			
			int NewFlowSize = Flow->x2 - Flow->x1 + 1;
			int Diff = NewFlowSize - OldFlowSize;
			if (Diff)
				Flow->MAX.x += Diff;
			
			Flow->y1 = Flow->y2;
			Flow->x2 = Flow->x1 + BlockFlowWidth;
		}
		else
		{
			LCss::Len Wid = Width();
			int WidPx = Wid.IsValid() ? Flow->ResolveX(Wid, this, true) : 0;
			
			Size.x = MAX(WidPx, Size.x);
			Size.x += Flow->ResolveX(PaddingRight(), this, true);
			Size.x += Flow->ResolveX(BorderRight(), this, true);

			int MarginR = Flow->ResolveX(MarginRight(), this, true);
			int MarginB = Flow->ResolveX(MarginBottom(), this, true);

			Flow->x1 = Local.x1 - Pos.x;
			Flow->cx = Local.cx + Size.x + MarginR - Pos.x;
			Flow->x2 = Local.x2 - Pos.x;

			if (Height().IsValid())
			{
				Size.y = Flow->ResolveY(Height(), this, false);
				Flow->y2 = MAX(Flow->y1 + Size.y + MarginB - 1, Flow->y2);
			}
			else
			{
				Flow->y2 += Flow->ResolveX(PaddingBottom(), this, true);
				Flow->y2 += Flow->ResolveX(BorderBottom(), this, true);
				Size.y = Flow->y2;
			}

			Flow->y1 = Local.y1 - Pos.y;
			Flow->y2 = MAX(Local.y2, Flow->y1+Size.y-1);
			
			if (!IsTableTag())
				Flow->Inline--;
		}

		// Can't do alignment here because pos is used to
		// restart the parents flow region...
	}
	else
	{
		Flow->Outdent(PadPx, false);
		
		switch (TagId)
		{
			default: break;
			case TAG_SELECT:
			case TAG_INPUT:
			{
				if (Html->InThread() && Ctrl)
				{
					LRect r = Ctrl->GetPos();

					if (Width().IsValid())
						Size.x = Flow->ResolveX(Width(), this, false);
					else
						Size.x = r.X();
					if (Height().IsValid())
						Size.y = Flow->ResolveY(Height(), this, false);
					else
						Size.y = r.Y();
					
					if (Html->IsAttached() && !Ctrl->IsAttached())
						Ctrl->Attach(Html);
				}

				Flow->cx += Size.x;
				Flow->y2 = MAX(Flow->y2, Flow->y1 + Size.y - 1);
				break;
			}
			case TAG_IMG:
			{
				Flow->cx += Size.x;
				Flow->y2 = MAX(Flow->y2, Flow->y1 + Size.y - 1);
				break;
			}
			case TAG_BR:
			{
				int OldFlowY2 = Flow->y2;
				Flow->FinishLine();
				Size.y = Flow->y2 - OldFlowY2;
				Flow->y2 = MAX(Flow->y2, Flow->y1 + Size.y - 1);
				break;
			}
			case TAG_CENTER:
			{
				int Px = Flow->X();
				for (auto e: Children)
				{
					LTag *t = ToTag(e);
					if (t && t->IsBlock() && t->Size.x < Px)
					{
						t->Pos.x = (Px - t->Size.x) >> 1;
					}
				}
				break;
			}
		}
	}

	BoundParents();

	if (Restart)
	{
		Flow->x1 += Pos.x;
		Flow->x2 += Pos.x;
		Flow->cx += Pos.x;

		Flow->y1 += Pos.y;
		Flow->y2 += Pos.y;
		Flow->MAX.y = MAX(Flow->MAX.y, Flow->y2);
	}

	if (Disp == DispBlock || Disp == DispInlineBlock)
	{
		if (XAlign == LCss::AlignCenter ||
			XAlign == LCss::AlignRight)
		{
			int Match = 0;
			auto parent = ToTag(Parent);
			for (auto &grp: parent->PostFlowAlign)
			{
				bool Overlaps = false;
				for (auto &a: grp)
				{
					if (a.Overlap(this))
					{
						Overlaps = true;
						break;
					}
				}

				if (!Overlaps)
					Match++;
			}
			
			auto &grp = parent->PostFlowAlign[Match];
			if (grp.Length() == 0)
			{
				grp.x1 = Flow->x1;
				grp.x2 = Flow->x2;
			}
			auto &pf = grp.New();
			pf.Disp = Disp;
			pf.XAlign = XAlign;
			pf.t = this;
		}
	}

	if (TagId == TAG_BODY && Flow->InBody > 0)
	{
		Flow->InBody--;
	}
}

bool LTag::PeekTag(char *s, char *tag)
{
	bool Status = false;

	if (s && tag)
	{
		if (*s == '<')
		{
			char *t = 0;
			Html->ParseName(++s, &t);
			if (t)
			{
				Status = _stricmp(t, tag) == 0;
			}

			DeleteArray(t);
		}
	}

	return Status;
}

LTag *LTag::GetTable()
{
	LTag *t = 0;
	for (t=ToTag(Parent); t && t->TagId != TAG_TABLE; t = ToTag(t->Parent))
		;
	return t;
}

void LTag::BoundParents()
{
	if (!Parent)
		return;

	LTag *np;
	for (LTag *n=this; n; n = np)
	{
		np = ToTag(n->Parent);

		if (!np || np->TagId == TAG_IFRAME)
			break;
				
		np->Size.x = MAX(np->Size.x, n->Pos.x + n->Size.x);
		np->Size.y = MAX(np->Size.y, n->Pos.y + n->Size.y);
	}
}

struct DrawBorder
{
	LSurface *pDC;
	uint32_t LineStyle;
	uint32_t LineReset;
	uint32_t OldStyle;

	DrawBorder(LSurface *pdc, LCss::BorderDef &d)
	{
		LineStyle = 0xffffffff;
		LineReset = 0x80000000;

		if (d.Style == LCss::BorderDotted)
		{
			switch ((int)d.Value)
			{
				case 2:
				{
					LineStyle = 0xcccccccc;
					break;
				}
				case 3:
				{
					LineStyle = 0xe38e38;
					LineReset = 0x800000;
					break;
				}
				case 4:
				{
					LineStyle = 0xf0f0f0f0;
					break;
				}
				case 5:
				{
					LineStyle = 0xf83e0;
					LineReset = 0x80000;
					break;
				}
				case 6:
				{
					LineStyle = 0xfc0fc0;
					LineReset = 0x800000;
					break;
				}
				case 7:
				{
					LineStyle = 0xfe03f80;
					LineReset = 0x8000000;
					break;
				}
				case 8:
				{
					LineStyle = 0xff00ff00;
					break;
				}
				case 9:
				{
					LineStyle = 0x3fe00;
					LineReset = 0x20000;
					break;
				}
				default:
				{
					LineStyle = 0xaaaaaaaa;
					break;
				}
			}
		}

		pDC = pdc;
		OldStyle = pDC->LineStyle();
	}

	~DrawBorder()
	{
		pDC->LineStyle(OldStyle);
	}
};

void LTag::GetInlineRegion(LRegion &rgn, int ox, int oy)
{
	if (TagId == TAG_IMG)
	{
		LRect rc(0, 0, Size.x-1, Size.y-1);
		rc.Offset(ox + Pos.x, oy + Pos.y);
		rgn.Union(&rc);
	}
	else
	{
		for (unsigned i=0; i<TextPos.Length(); i++)
		{
			LRect rc = *(TextPos[i]);
			rc.Offset(ox + Pos.x, oy + Pos.y);
			rgn.Union(&rc);
		}
	}
	
	for (unsigned c=0; c<Children.Length(); c++)
	{
		LTag *ch = ToTag(Children[c]);
		ch->GetInlineRegion(rgn, ox + Pos.x, oy + Pos.y);
	}
}

class CornersImg : public LMemDC
{
public:
	int Px, Px2;

	CornersImg(	float RadPx,
				LRect *BorderPx,
				LCss::BorderDef **defs,
				LColour &Back,
				bool DrawBackground)
	{
		Px = 0;
		Px2 = 0;

		//Radius.Type != LCss::LenInherit &&			
		if (RadPx > 0.0f)
		{
			Px = (int)ceil(RadPx);
			Px2 = Px << 1;
			
			if (Create(Px2, Px2, System32BitColourSpace))
			{
				#if 1
				Colour(0, 32);
				#else
				Colour(LColour(255, 0, 255));
				#endif
				Rectangle();

				LPointF ctr(Px, Px);
				LPointF LeftPt(0.0, Px);
				LPointF TopPt(Px, 0.0);
				LPointF RightPt(X(), Px);
				LPointF BottomPt(Px, Y());
				int x_px[4] = {BorderPx->x1, BorderPx->x2, BorderPx->x2, BorderPx->x1};
				int y_px[4] = {BorderPx->y1, BorderPx->y1, BorderPx->y2, BorderPx->y2};
				LPointF *pts[4] = {&LeftPt, &TopPt, &RightPt, &BottomPt};
				
				// Draw border parts..
				for (int i=0; i<4; i++)
				{
					int k = (i + 1) % 4;

					// Setup the stops					
					LBlendStop stops[2] = { {0.0, 0}, {1.0, 0} };
					uint32_t iColour = defs[i]->Color.IsValid() ? defs[i]->Color.Rgb32 : Back.c32();
					uint32_t kColour = defs[k]->Color.IsValid() ? defs[k]->Color.Rgb32 : Back.c32();
					if (defs[i]->IsValid() && defs[k]->IsValid())
					{
						stops[0].c32 = iColour;
						stops[1].c32 = kColour;
					}
					else if (defs[i]->IsValid())
					{
						stops[0].c32 = stops[1].c32 = iColour;
					}
					else
					{
						stops[0].c32 = stops[1].c32 = kColour;
					}
					
					// Create a brush
					LLinearBlendBrush br
					(
						*pts[i],
						*pts[k],
						2,
						stops
					);
					
					// Setup the clip
					LRect clip(	(int)MIN(pts[i]->x, pts[k]->x),
								(int)MIN(pts[i]->y, pts[k]->y),
								(int)MAX(pts[i]->x, pts[k]->x)-1,
								(int)MAX(pts[i]->y, pts[k]->y)-1);
					ClipRgn(&clip);
					
					// Draw the arc...
					LPath p;
					p.Circle(ctr, Px);
					if (defs[i]->IsValid() || defs[k]->IsValid())
						p.Fill(this, br);
					
					// Fill the background
					p.Empty();
					p.Ellipse(ctr, Px-x_px[i], Px-y_px[i]);
					if (DrawBackground)
					{
						LSolidBrush br(Back);
						p.Fill(this, br);					
					}
					else
					{
						LEraseBrush br;
						p.Fill(this, br);
					}
					
					ClipRgn(NULL);
				}
				
				#ifdef MAC
				ConvertPreMulAlpha(true);
				#endif
				
				#if 0
				static int count = 0;
				LString file;
				file.Printf("c:\\temp\\img-%i.bmp", ++count);
				GdcD->Save(file, Corners);
				#endif
			}
		}
	}
};

void LTag::PaintBorderAndBackground(LSurface *pDC, LColour &Back, LRect *BorderPx)
{
	LArray<LRect> r;
	LRect BorderPxRc;
	bool DrawBackground = !Back.IsTransparent();

	#ifdef _DEBUG
	if (Debug)
	{
		//int asd=0;
	}
	#endif

	if (!BorderPx)
		BorderPx = &BorderPxRc;
	BorderPx->ZOff(0, 0);

	// Get all the border info and work out the pixel sizes.
	LFont *f = GetFont();
	
	#define DoEdge(coord, axis, name) \
		BorderDef name = Border##name(); \
		BorderPx->coord = name.Style != LCss::BorderNone ? name.ToPx(Size.axis, f) : 0;
	#define BorderValid(name) \
		((name).IsValid() && (name).Style != LCss::BorderNone)
	
	DoEdge(x1, x, Left);
	DoEdge(y1, y, Top);
	DoEdge(x2, x, Right);
	DoEdge(y2, y, Bottom);
	
	LCss::BorderDef *defs[4] = {&Left, &Top, &Right, &Bottom};

	if (BorderValid(Left) ||
		BorderValid(Right) ||
		BorderValid(Top) ||
		BorderValid(Bottom) ||
		DrawBackground)
	{
		// Work out the rectangles
		switch (Display())
		{
			case DispInlineBlock:
			case DispBlock:
			{
				r[0].ZOff(Size.x-1, Size.y-1);
				break;
			}
			case DispInline:
			{
				LRegion rgn;
				GetInlineRegion(rgn);
				if (BorderPx)
				{
					for (int i=0; i<rgn.Length(); i++)
					{
						LRect *r = rgn[i];
						r->x1 -= BorderPx->x1 + PadPx.x1;
						r->y1 -= BorderPx->y1 + PadPx.y1;
						r->x2 += BorderPx->x2 + PadPx.x2;
						r->y2 += BorderPx->y2 + PadPx.y2;
					}
				}

				r.Length(rgn.Length());
				auto p = r.AddressOf();
				for (auto i = rgn.First(); i; i = rgn.Next())
					*p++ = *i;
				break;
			}
			default:
				return;
		}

		// If we are drawing rounded corners, draw them into a memory context
		LAutoPtr<CornersImg> Corners;
		int Px = 0, Px2 = 0;
		LCss::Len Radius = BorderRadius();
		float RadPx = Radius.Type == LCss::LenPx ? Radius.Value : Radius.ToPx(Size.x, GetFont());
		bool HasRadius = Radius.Type != LCss::LenInherit && RadPx > 0.0f;

		// Loop over the rectangles and draw everything
		int Op = pDC->Op(GDC_ALPHA);

		for (unsigned i=0; i<r.Length(); i++)
		{
			LRect rc = r[i];
			if (HasRadius)
			{
				Px = (int)ceil(RadPx);
				Px2 = Px << 1;
				if (Px2 > rc.Y())
				{
					Px = rc.Y() / 2;
					Px2 = Px << 1;
				}			
				if (!Corners || Corners->Px2 != Px2)
				{
					Corners.Reset(new CornersImg((float)Px, BorderPx, defs, Back, DrawBackground));
				}
			
				// top left
				LRect r(0, 0, Px-1, Px-1);
				pDC->Blt(rc.x1, rc.y1, Corners, &r);
			
				// top right
				r.Set(Px, 0, Corners->X()-1, Px-1);
				pDC->Blt(rc.x2-Px+1, rc.y1, Corners, &r);
			
				// bottom left
				r.Set(0, Px, Px-1, Corners->Y()-1);
				pDC->Blt(rc.x1, rc.y2-Px+1, Corners, &r);
			
				// bottom right
				r.Set(Px, Px, Corners->X()-1, Corners->Y()-1);
				pDC->Blt(rc.x2-Px+1, rc.y2-Px+1, Corners, &r);
			
				#if 1
				pDC->Colour(Back);
				pDC->Rectangle(rc.x1+Px, rc.y1, rc.x2-Px, rc.y2);
				pDC->Rectangle(rc.x1, rc.y1+Px, rc.x1+Px-1, rc.y2-Px);
				pDC->Rectangle(rc.x2-Px+1, rc.y1+Px, rc.x2, rc.y2-Px);
				#else
				pDC->Colour(LColour(255, 0, 0, 0x80));
				pDC->Rectangle(rc.x1+Px, rc.y1, rc.x2-Px, rc.y2);
				pDC->Colour(LColour(0, 255, 0, 0x80));
				pDC->Rectangle(rc.x1, rc.y1+Px, rc.x1+Px-1, rc.y2-Px);
				pDC->Colour(LColour(0, 0, 255, 0x80));
				pDC->Rectangle(rc.x2-Px+1, rc.y1+Px, rc.x2, rc.y2-Px);
				#endif
			}
			else if (DrawBackground)
			{
				pDC->Colour(Back);
				pDC->Rectangle(&rc);
			}

			LCss::BorderDef *b;
			if ((b = &Left) && BorderValid(*b))
			{
				pDC->Colour(b->Color.Rgb32, 32);
				DrawBorder db(pDC, *b);
				for (int i=0; i<b->Value; i++)
				{
					pDC->LineStyle(db.LineStyle, db.LineReset);
					pDC->Line(rc.x1 + i, rc.y1+Px, rc.x1+i, rc.y2-Px);
				}
			}
			if ((b = &Top) && BorderValid(*b))
			{
				pDC->Colour(b->Color.Rgb32, 32);
				DrawBorder db(pDC, *b);
				for (int i=0; i<b->Value; i++)
				{
					pDC->LineStyle(db.LineStyle, db.LineReset);
					pDC->Line(rc.x1+Px, rc.y1+i, rc.x2-Px, rc.y1+i);
				}
			}
			if ((b = &Right) && BorderValid(*b))
			{
				pDC->Colour(b->Color.Rgb32, 32);
				DrawBorder db(pDC, *b);
				for (int i=0; i<b->Value; i++)
				{
					pDC->LineStyle(db.LineStyle, db.LineReset);
					pDC->Line(rc.x2-i, rc.y1+Px, rc.x2-i, rc.y2-Px);
				}			
			}
			if ((b = &Bottom) && BorderValid(*b))
			{
				pDC->Colour(b->Color.Rgb32, 32);
				DrawBorder db(pDC, *b);
				for (int i=0; i<b->Value; i++)
				{
					pDC->LineStyle(db.LineStyle, db.LineReset);
					pDC->Line(rc.x1+Px, rc.y2-i, rc.x2-Px, rc.y2-i);
				}
			}
		}

		pDC->Op(Op);
	}
}

static void FillRectWithImage(LSurface *pDC, LRect *r, LSurface *Image, LCss::RepeatType Repeat)
{
	int Px = 0, Py = 0;
	int Old = pDC->Op(GDC_ALPHA);

	if (!Image)
		return;

	switch (Repeat)
	{
		default:
		case LCss::RepeatBoth:
		{
			for (int y=0; y<r->Y(); y += Image->Y())
			{
				for (int x=0; x<r->X(); x += Image->X())
				{
					pDC->Blt(Px + x, Py + y, Image);
				}
			}
			break;
		}
		case LCss::RepeatX:
		{
			for (int x=0; x<r->X(); x += Image->X())
			{
				pDC->Blt(Px + x, Py, Image);
			}
			break;
		}
		case LCss::RepeatY:
		{
			for (int y=0; y<r->Y(); y += Image->Y())
			{
				pDC->Blt(Px, Py + y, Image);
			}
			break;
		}
		case LCss::RepeatNone:
		{
			pDC->Blt(Px, Py, Image);
			break;
		}
	}

	pDC->Op(Old);
}

void LTag::OnPaint(LSurface *pDC, bool &InSelection, uint16 Depth)
{
	if (Depth >= MAX_RECURSION_DEPTH ||
		Display() == DispNone)
		return;
	if (
		#ifdef _DEBUG
		!Html->_Debug &&
		#endif
		LCurrentTime() - Html->PaintStart > Html->d->MaxPaintTime)
	{
		Html->d->MaxPaintTimeout = true;
		return;
	}

	int Px, Py;
	pDC->GetOrigin(Px, Py);
	
	#if 0
	if (Debug)
	{
		Gtk::cairo_matrix_t mx;
		Gtk::cairo_get_matrix(pDC->Handle(), &mx);
		LPoint Offset;
		Html->WindowVirtualOffset(&Offset);
		LRect cli;
		pDC->GetClient(&cli);
		printf("\tTag paint mx=%g,%g off=%i,%i p=%i,%i Pos=%i,%i cli=%s\n",
			mx.x0, mx.y0,
			Offset.x, Offset.y,
			Px, Py,
			Pos.x, Pos.y,
			cli.GetStr());
	}
	#endif

	switch (TagId)
	{
		case TAG_INPUT:
		case TAG_SELECT:
		{
			if (Ctrl)
			{
				int64 Sx = 0, Sy = 0;
				int64 LineY = GetFont()->GetHeight();
				Html->GetScrollPos(Sx, Sy);
				Sx *= LineY;
				Sy *= LineY;
				
				LRect r(0, 0, Size.x-1, Size.y-1), Px;
				LColour back = _Colour(false);
				PaintBorderAndBackground(pDC, back, &Px);
				if (!dynamic_cast<LButton*>(Ctrl))
				{
					r.x1 += Px.x1;
					r.y1 += Px.y1;
					r.x2 -= Px.x2;
					r.y2 -= Px.y2;
				}
				r.Offset(AbsX() - (int)Sx, AbsY() - (int)Sy);
				Ctrl->SetPos(r);
			}
			
			if (TagId == TAG_SELECT)
				return;
			break;
		}
		case TAG_BODY:
		{
			auto b = _Colour(false);
			if (!b.IsTransparent())
			{
				pDC->Colour(b);
				pDC->Rectangle(Pos.x, Pos.y, Pos.x+Size.x, Pos.y+Size.y);
			}
			if (Image)
			{
				LRect r;
				r.ZOff(Size.x-1, Size.y-1);
				FillRectWithImage(pDC, &r, Image, BackgroundRepeat());
			}
			break;
		}
		case TAG_HEAD:
		{
			// Nothing under here to draw.
			return;
		}
		case TAG_HR:
		{
			pDC->Colour(L_MED);
			pDC->Rectangle(0, 0, Size.x - 1, Size.y - 1);
			break;
		}
		case TAG_TR:
		case TAG_TBODY:
		case TAG_META:
		{
			// Draws nothing...
			break;
		}
		case TAG_IMG:
		{
			LRect Clip(0, 0, Size.x-1, Size.y-1);
			pDC->ClipRgn(&Clip);
			
			if (Image)
			{
				#if ENABLE_IMAGE_RESIZING
				if
				(
					!ImageResized &&
					(				
						Size.x != Image->X() ||
						Size.y != Image->Y()
					)
				)
				{
					ImageResized = true;

					LColourSpace Cs = Image->GetColourSpace();
					if (Cs == CsIndex8 &&
						Image->AlphaDC())
						Cs = System32BitColourSpace;
					
					LAutoPtr<LSurface> r(new LMemDC(Size.x, Size.y, Cs));
					if (r)
					{
						if (Cs == CsIndex8)
							r->Palette(new LPalette(Image->Palette()));
						ResampleDC(r, Image);
						Image = r;
					}
				}
				#endif

				int Old = pDC->Op(GDC_ALPHA);
				pDC->Blt(0, 0, Image);
				pDC->Op(Old);
			}
			else if (Size.x > 1 && Size.y > 1)
			{
				LRect b(0, 0, Size.x-1, Size.y-1);
				LColour Fill(LColour(L_MED).Mix(LColour(L_LIGHT), 0.2f));
				LColour Border(L_MED);

				// Border
				pDC->Colour(Border);
				pDC->Box(&b);
				b.Inset(1, 1);
				pDC->Box(&b);
				b.Inset(1, 1);
				pDC->Colour(Fill);
				pDC->Rectangle(&b);

				const char *Alt;
				LColour Red(LColour(255, 0, 0).Mix(Fill, 0.3f));
				if (Get("alt", Alt) && ValidStr(Alt))
				{
					LDisplayString Ds(Html->GetFont(), Alt);
					Html->GetFont()->Colour(Red, Fill);
					Ds.Draw(pDC, 2, 2, &b);
				}
				else if (Size.x >= 16 && Size.y >= 16)
				{
					// Red 'x'
					int Cx = b.x1 + (b.X()/2);
					int Cy = b.y1 + (b.Y()/2);
					LRect c(Cx-4, Cy-4, Cx+4, Cy+4);
					
					pDC->Colour(Red);
					pDC->Line(c.x1, c.y1, c.x2, c.y2);
					pDC->Line(c.x1, c.y2, c.x2, c.y1);
					pDC->Line(c.x1, c.y1 + 1, c.x2 - 1, c.y2);
					pDC->Line(c.x1 + 1, c.y1, c.x2, c.y2 - 1);
					pDC->Line(c.x1 + 1, c.y2, c.x2, c.y1 + 1);
					pDC->Line(c.x1, c.y2 - 1, c.x2 - 1, c.y1);
				}
			}
			
			pDC->ClipRgn(0);
			break;
		}
		default:
		{
			LColour fore = _Colour(true);
			LColour back = _Colour(false);

			if (Display() == DispBlock && Html->Environment)
			{
				LCss::ImageDef Img = BackgroundImage();
				if (Img.Img)
				{
					LRect Clip(0, 0, Size.x-1, Size.y-1);
					pDC->ClipRgn(&Clip);					
					FillRectWithImage(pDC, &Clip, Img.Img, BackgroundRepeat());					
					pDC->ClipRgn(NULL);

					back.Empty();
				}
			}

			PaintBorderAndBackground(pDC, back, NULL);

			LFont *f = GetFont();
			#if DEBUG_TEXT_AREA
			bool IsEditor = Html ? !Html->GetReadOnly() : false;
			#else
			bool IsEditor = false;
			#endif
			if (f && TextPos.Length())
			{
				// This is the non-display part of the font bounding box
				int LeadingPx = (int)(f->Leading() + 0.5);
				
				// This is the displayable part of the font
				int FontPx = f->GetHeight() - LeadingPx;
				
				// This is the pixel height we're aiming to fill
				int EffectiveLineHt = LineHeightCache >= 0 ? MAX(FontPx, LineHeightCache) : FontPx;
				
				// This gets added to the y coord of each piece of text
				int LineHtOff = ((EffectiveLineHt - FontPx + 1) >> 1) - LeadingPx;
								
				#define FontColour(InSelection) \
					f->Transparent(!InSelection && !IsEditor); \
					if (InSelection) \
						f->Colour(L_FOCUS_SEL_FORE, L_FOCUS_SEL_BACK); \
					else \
					{ \
						LColour bk(back.IsTransparent() ? LColour(L_WORKSPACE) : back);			\
						LColour fr(fore.IsTransparent() ? LColour(DefaultTextColour) : fore);		\
						if (IsEditor)																\
							bk = bk.Mix(LColour::Black, 0.05f);									\
						f->Colour(fr, bk);															\
					}

				if (Html->HasSelection() &&
					(Selection >= 0 || Cursor >= 0) &&
					Selection != Cursor)
				{
					ssize_t Min = -1;
					ssize_t Max = -1;

					ssize_t Base = GetTextStart();
					if (Cursor >= 0 && Selection >= 0)
					{
						Min = MIN(Cursor, Selection) + Base;
						Max = MAX(Cursor, Selection) + Base;
					}
					else if (InSelection)
					{
						Max = MAX(Cursor, Selection) + Base;
					}
					else
					{
						Min = MAX(Cursor, Selection) + Base;
					}

					LRect CursorPos;
					CursorPos.ZOff(-1, -1);

					for (unsigned i=0; i<TextPos.Length(); i++)
					{
						LFlowRect *Tr = TextPos[i];
						ssize_t Start = Tr->Text - Text();
						ssize_t Done = 0;
						int x = Tr->x1;

						if (Tr->Len == 0)
						{
							// Is this a selection edge point?
							if (!InSelection && Min == 0)
							{
								InSelection = !InSelection;
							}
							else if (InSelection && Max == 0)
							{
								InSelection = !InSelection;
							}

							if (Cursor >= 0)
							{
								// Is this the cursor, then draw it and save it's position
								if (Cursor == Start + Done - Base)
								{
									Html->d->CursorPos.Set(x, Tr->y1 + LineHtOff, x + 1, Tr->y2 - LineHtOff);

									if (Html->d->CursorPos.x1 > Tr->x2)
										Html->d->CursorPos.Offset(Tr->x2 - Html->d->CursorPos.x1, 0);

									CursorPos = Html->d->CursorPos;
									Html->d->CursorPos.Offset(AbsX(), AbsY());
								}
							}
							break;
						}

						while (Done < Tr->Len)
						{
							ssize_t c = Tr->Len - Done;

							FontColour(InSelection);

							// Is this a selection edge point?
							if (		!InSelection &&
										Min - Start >= Done &&
										Min - Start < Done + Tr->Len)
							{
								InSelection = !InSelection;
								c = Min - Start - Done;
							}
							else if (	InSelection &&
										Max - Start >= Done &&
										Max - Start <= Tr->Len)
							{
								InSelection = !InSelection;
								c = Max - Start - Done;
							}

							// Draw the text run
							LDisplayString ds(f, Tr->Text + Done, c);
							if (IsEditor)
							{
								LRect r(x, Tr->y1, x + ds.X() - 1, Tr->y2);
								ds.Draw(pDC, x, Tr->y1 + LineHtOff, &r);
							}
							else
							{
								ds.Draw(pDC, x, Tr->y1 + LineHtOff);
							}
							x += ds.X();
							Done += c;
							
							// Is this is end of the tag?
							if (Tr->Len == Done)
							{
								// Is it also a selection edge?
								if (		!InSelection &&
											Min - Start == Done)
								{
									InSelection = !InSelection;
								}
								else if (	InSelection &&
											Max - Start == Done)
								{
									InSelection = !InSelection;
								}
							}

							if (Cursor >= 0)
							{
								// Is this the cursor, then draw it and save it's position
								if (Cursor == Start + Done - Base)
								{
									Html->d->CursorPos.Set(x, Tr->y1 + LineHtOff, x + 1, Tr->y2 - LineHtOff);

									if (Html->d->CursorPos.x1 > Tr->x2)
										Html->d->CursorPos.Offset(Tr->x2 - Html->d->CursorPos.x1, 0);

									CursorPos = Html->d->CursorPos;
									Html->d->CursorPos.Offset(AbsX(), AbsY());
								}
							}

						}
					}

					if (Html->d->CursorVis && CursorPos.Valid())
					{
						pDC->Colour(L_TEXT);
						pDC->Rectangle(&CursorPos);
					}
				}
				else if (Cursor >= 0)
				{
					FontColour(InSelection);

					ssize_t Base = GetTextStart();
					for (unsigned i=0; i<TextPos.Length(); i++)
					{
						LFlowRect *Tr = TextPos[i];
						if (!Tr)
							break;
						ssize_t Pos = (Tr->Text - Text()) - Base;

						LAssert(Tr->y2 >= Tr->y1);
						LDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1 + LineHtOff, IsEditor ? Tr : NULL);

						if
						(
							(
								Tr->Text == PreText()
								&&
								!ValidStrW(Text())
							)
							||
							(
								Cursor >= Pos
								&&
								Cursor <= Pos + Tr->Len
							)
						)
						{
							ssize_t Off = Tr->Text == PreText() ? StrlenW(PreText()) : Cursor - Pos;
							pDC->Colour(L_TEXT);
							LRect c;
							if (Off)
							{
								LDisplayString ds(f, Tr->Text, Off);
								int x = ds.X();
								if (x >= Tr->X())
									x = Tr->X()-1;
								c.Set(Tr->x1 + x, Tr->y1, Tr->x1 + x + 1, Tr->y1 + f->GetHeight());
							}
							else
							{
								c.Set(Tr->x1, Tr->y1, Tr->x1 + 1, Tr->y1 + f->GetHeight());
							}
							Html->d->CursorPos = c;
							if (Html->d->CursorVis)
								pDC->Rectangle(&c);
							Html->d->CursorPos.Offset(AbsX(), AbsY());
						}
					}
				}
				else
				{
					FontColour(InSelection);

					for (auto &Tr: TextPos)
					{
						LDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1 + LineHtOff, IsEditor ? Tr : NULL);
					}
				}
			}
			break;
		}
	}
	
	#if DEBUG_TABLE_LAYOUT && 0
	if (IsTableCell(TagId))
	{
		LTag *Tbl = this;
		while (Tbl->TagId != TAG_TABLE && Tbl->Parent)
			Tbl = Tbl->Parent;
		if (Tbl && Tbl->TagId == TAG_TABLE && Tbl->Debug)
		{
			pDC->Colour(LColour(255, 0, 0));
			pDC->Box(0, 0, Size.x-1, Size.y-1);
		}
	}
	#endif

	for (unsigned i=0; i<Children.Length(); i++)
	{
		LTag *t = ToTag(Children[i]);
		pDC->SetOrigin(Px - t->Pos.x, Py - t->Pos.y);
		t->OnPaint(pDC, InSelection, Depth + 1);
		pDC->SetOrigin(Px, Py);
	}
	
	#if DEBUG_DRAW_TD
	if (TagId == TAG_TD)
	{
		LTag *Tbl = this;
		while (Tbl && Tbl->TagId != TAG_TABLE)
			Tbl = ToTag(Tbl->Parent);
		if (Tbl && Tbl->Debug)
		{
			int Ls = pDC->LineStyle(LSurface::LineDot);
			pDC->Colour(LColour::Blue);
			pDC->Box(0, 0, Size.x-1, Size.y-1);
			pDC->LineStyle(Ls);
		}
	}
	#endif
}

//////////////////////////////////////////////////////////////////////
LHtml::LHtml(int id, int x, int y, int cx, int cy, LDocumentEnv *e) :
	LDocView(e),
	ResObject(Res_Custom),
	LHtmlParser(NULL)
{
	View = this;
	d = new LHtmlPrivate;
	SetReadOnly(true);
	ViewWidth = -1;
	SetId(id);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Cursor = 0;
	Selection = 0;
	DocumentUid = 0;

	_New();
}

LHtml::~LHtml()
{
	_Delete();
	DeleteObj(d);

	if (JobSem.Lock(_FL))
	{
		JobSem.Jobs.DeleteObjects();
		JobSem.Unlock();
	}
}

void LHtml::_New()
{
	d->StyleDirty = false;
	d->IsLoaded = false;
	d->Content.x = d->Content.y = 0;
	d->DeferredLoads = 0;
	Tag = 0;
	DocCharSet.Reset();

	IsHtml = true;
	
	#ifdef DefaultFont
	LFont *Def = new LFont;
	if (Def)
	{
		if (Def->CreateFromCss(DefaultFont))
			SetFont(Def, true);
		else
			DeleteObj(Def);
	}
	#endif
	
	FontCache = new LFontCache(this);	
	SetScrollBars(false, false);
}

void LHtml::_Delete()
{
	LAssert(!d->IsParsing);

	CssStore.Empty();
	CssHref.Empty();
	OpenTags.Length(0);
	Source.Reset();
	DeleteObj(Tag);
	DeleteObj(FontCache);
}

LFont *LHtml::DefFont()
{
	return GetFont();
}

void LHtml::OnAddStyle(const char *MimeType, const char *Styles)
{
	if (Styles)
	{
		const char *c = Styles;
		bool Status = CssStore.Parse(c);
		if (Status)
		{
			d->StyleDirty = true;
		}

		#if 0 // def _DEBUG
		bool LogCss = false;
		if (!Status)
		{
			char p[MAX_PATH_LEN];
			sprintf_s(p, sizeof(p), "c:\\temp\\css_parse_failure_%i.txt", LRand());
			LFile f;
			if (f.Open(p, O_WRITE))
			{
				f.SetSize(0);
				if (CssStore.Error)
					f.Print("Error: %s\n\n", CssStore.Error.Get());
				f.Write(Styles, strlen(Styles));
				f.Close();
			}
		}

		if (LogCss)
		{
			LStringPipe p;
			CssStore.Dump(p);
			LAutoString a(p.NewStr());
			LFile f;
			if (f.Open("C:\\temp\\css.txt", O_WRITE))
			{
				f.Write(a, strlen(a));
				f.Close();
			}
		}
		#endif
	}
}

void LHtml::ParseDocument(const char *Doc)
{
	if (!Tag)
	{
		Tag = new LTag(this, 0);
	}

	if (GetCss())
		GetCss()->DeleteProp(LCss::PropBackgroundColor);
	if (Tag)
	{
		Tag->TagId = ROOT;
		OpenTags.Length(0);

		if (IsHtml)
		{
			Parse(Tag, Doc);

			// Add body tag if not specified...
			LTag *Html = Tag->GetTagByName("html");
			LTag *Body = Tag->GetTagByName("body");

			if (!Html && !Body)
			{
				if ((Html = new LTag(this, 0)))
					Html->SetTag("html");
				if ((Body = new LTag(this, Html)))
					Body->SetTag("body");
				
				Html->Attach(Body);

				if (Tag->Text())
				{
					LTag *Content = new LTag(this, Body);
					if (Content)
					{
						Content->TagId = CONTENT;
						Content->Text(NewStrW(Tag->Text()));
					}
				}
				while (Tag->Children.Length())
				{
					LTag *t = ToTag(Tag->Children.First());
					Body->Attach(t, Body->Children.Length());
				}
				DeleteObj(Tag);
				
				Tag = Html;
			}
			else if (!Body)
			{
				if ((Body = new LTag(this, Html)))
					Body->SetTag("body");
					
				for (unsigned i=0; i<Html->Children.Length(); i++)
				{
					LTag *t = ToTag(Html->Children[i]);
					if (t->TagId != TAG_HEAD)
					{
						Body->Attach(t);
						i--;
					}
				}
				
				Html->Attach(Body);
			}
			
			if (Html && Body)
			{
				char16 *t = Tag->Text();
				if (t)
				{
					if (ValidStrW(t))
					{
						LTag *Content = new LTag(this, 0);
						if (Content)
						{
							Content->Text(NewStrW(Tag->Text()));
							Body->Attach(Content, 0);
						}
					}
					Tag->Text(0);
				}

				#if 0 // Enabling this breaks the test file 'gw2.html'.
				for (LTag *t = Html->Tags.First(); t; )
				{
					if (t->Tag && t->Tag[0] == '!')
					{
						Tag->Attach(t, 0);
						t = Html->Tags.Current();
					}
					else if (t->TagId != TAG_HEAD &&
							t != Body)
					{
						if (t->TagId == TAG_HTML)
						{
							LTag *c;
							while ((c = t->Tags.First()))
							{
								Html->Attach(c, 0);
							}

							t->Detach();
							DeleteObj(t);
						}
						else
						{
							t->Detach();
							Body->Attach(t);
						}

						t = Html->Tags.Current();
					}
					else
					{
						t = Html->Tags.Next();
					}
				}
				#endif					

				if (Environment)
				{
					const char *OnLoad;
					if (Body->Get("onload", OnLoad))
					{
						Environment->OnExecuteScript(this, (char*)OnLoad);
					}
				}
			}
		}
		else
		{
			Tag->ParseText(Source);
		}
	}

	ViewWidth = -1;
	if (Tag)
		Tag->ResetCaches();
	Invalidate();
}

bool LHtml::NameW(const char16 *s)
{
	LAutoPtr<char, true> utf(WideToUtf8(s));
	return Name(utf);
}

const char16 *LHtml::NameW()
{
	LBase::Name(Source);
	return LBase::NameW();
}

bool LHtml::Name(const char *s)
{
	int Uid = -1;
	if (Environment)
		Uid = Environment->NextUid();
	if (Uid < 0)
		Uid = GetDocumentUid() + 1;
	SetDocumentUid(Uid);

	_Delete();
	_New();

	IsHtml = false;

	// Detect HTML
	const char *c = s;
	while ((c = strchr(c, '<')))
	{
		char *t = 0;
		c = ParseName((char*) ++c, &t);
		if (t && GetTagInfo(t))
		{
			DeleteArray(t);
			IsHtml = true;
			break;
		}
		DeleteArray(t);
	}

	// Parse
	d->IsParsing = true;
	ParseDocument(s);
	d->IsParsing = false;

	if (Tag && d->StyleDirty)
	{
		d->StyleDirty = false;
		Tag->RestyleAll();
	}

	if (d->DeferredLoads == 0)
	{
		OnLoad();
	}

	Invalidate();	

	return true;
}

const char *LHtml::Name()
{
	if (!Source && Tag)
	{
		LStringPipe s(1024);
		Tag->CreateSource(s);
		Source.Reset(s.NewStr());
	}

	return Source;
}

LMessage::Result LHtml::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_COPY:
		{
			Copy();
			break;
		}
		case M_JOBS_LOADED:
		{
			bool Update = false;
			int InitDeferredLoads = d->DeferredLoads;
			
			if (JobSem.Lock(_FL))
			{
				for (unsigned i=0; i<JobSem.Jobs.Length(); i++)
				{
					LDocumentEnv::LoadJob *j = JobSem.Jobs[i];
					int MyUid = GetDocumentUid();

					// LgiTrace("%s:%i - Receive job %p, %p\n", _FL, j, j->UserData);
					
					if (j->UserUid == MyUid &&
						j->UserData != NULL)
					{
						Html1::LTag *r = static_cast<Html1::LTag*>(j->UserData);
						
						if (d->DeferredLoads > 0)
							d->DeferredLoads--;
						
						// Check the tag is still in our tree...
						if (Tag->HasChild(r))
						{
							// Process the returned data...
							if (r->TagId == TAG_IMG)
							{
								if (j->pDC)
								{
									r->SetImage(j->Uri, j->pDC.Release());
									ViewWidth = 0;
									Update = true;
								}
								else if (j->Stream)
								{
									LAutoPtr<LSurface> pDC(GdcD->Load(dynamic_cast<LStream*>(j->Stream.Get())));
									if (pDC)
									{
										r->SetImage(j->Uri, pDC.Release());
										ViewWidth = 0;
										Update = true;
									}
									else LgiTrace("%s:%i - Image decode failed for '%s'\n", _FL, j->Uri.Get());
								}
								else if (j->Status == LDocumentEnv::LoadJob::JobOk)
									LgiTrace("%s:%i - Unexpected job type for '%s'\n", _FL, j->Uri.Get());
							}
							else if (r->TagId == TAG_LINK)
							{
								if (!CssHref.Find(j->Uri))
								{
									LStreamI *s = j->GetStream();
									if (s)
									{
										s->ChangeThread();

										int Size = (int)s->GetSize();
										LAutoString Style(new char[Size+1]);
										ssize_t rd = s->Read(Style, Size);
										if (rd > 0)
										{
											Style[rd] = 0;
											CssHref.Add(j->Uri, true);
											OnAddStyle("text/css", Style);									
											ViewWidth = 0;
											Update = true;
										}
									}
								}
							}
							else if (r->TagId == TAG_IFRAME)
							{
								// Remote IFRAME loading not support for security reasons.
							}
							else LgiTrace("%s:%i - Unexpected tag '%s' for URI '%s'\n", _FL,
								r->Tag.Get(),
								j->Uri.Get());
						}
						else
						{
							/*
							Html1::LTag *p = ToTag(r->Parent);
							while (p && p->Parent)
								p = ToTag(p->Parent);
							*/
							LgiTrace("%s:%i - No child tag for job.\n", _FL);
						}
					}
					// else it's from another (historical) HTML control, ignore
				}
				JobSem.Jobs.DeleteObjects();
				JobSem.Unlock();
			}
			
			if (InitDeferredLoads > 0 && d->DeferredLoads <= 0)
			{
				LAssert(d->DeferredLoads == 0);
				d->DeferredLoads = 0;
				OnLoad();
			}

			if (Update)
			{
				OnPosChange();
				Invalidate();
			}
			break;
		}
	}

	return LDocView::OnEvent(Msg);
}

int LHtml::OnNotify(LViewI *c, LNotification n)
{
	switch (c->GetId())
	{
		case IDC_VSCROLL:
		{
			int LineY = GetFont()->GetHeight();

			if (Tag)
				Tag->ClearToolTips();

			if (n.Type == LNotifyScrollBarCreate && VScroll && LineY > 0)
			{
				int y = Y();
				int p = MAX(y / LineY, 1);
				int fy = d->Content.y / LineY;
				VScroll->SetPage(p);
				VScroll->SetRange(fy);
			}
			
			Invalidate();
			break;
		}
		default:
		{
			LTag *Ctrl = Tag ? Tag->FindCtrlId(c->GetId()) : NULL;
			if (Ctrl)
				return Ctrl->OnNotify(n);
			break;
		}
	}

	return LLayout::OnNotify(c, n);
}

void LHtml::OnPosChange()
{
	LLayout::OnPosChange();
	if (ViewWidth != X())
	{
		Invalidate();
	}
}

bool LHtml::OnLayout(LViewLayoutInfo &Inf)
{
	if (!Inf.Width.Min)
	{
		Inf.Width.Min = Inf.FILL;
		Inf.Width.Max = Inf.FILL;
	}
	else
	{
		Inf.Height.Min = Inf.FILL;
		Inf.Height.Max = Inf.FILL;
	}

	return true;
}

LPoint LHtml::Layout(bool ForceLayout)
{
	LRect Client = GetClient();
	if (Tag && (ViewWidth != Client.X() || ForceLayout))
	{
		LFlowRegion f(this, Client, false);

		// Flow text, width is different
		Tag->OnFlow(&f, 0);
		ViewWidth = Client.X();
		d->Content.x = f.MAX.x + 1;
		d->Content.y = f.MAX.y + 1;

		// Set up scroll box
		bool Sy = f.y2 > Y();
		int LineY = GetFont()->GetHeight();

		uint64 Now = LCurrentTime();
		if (Now - d->SetScrollTime > 100)
		{
			d->SetScrollTime = Now;
			SetScrollBars(false, Sy);
			
			if (Sy && VScroll && LineY > 0)
			{
				int y = Y();
				int p = MAX(y / LineY, 1);
				int fy = f.y2 / LineY;
				VScroll->SetPage(p);
				VScroll->SetRange(fy);
			}
		}
		else
		{
			// LgiTrace("%s - Dropping SetScroll, loop detected: %i ms\n", GetClass(), (int)(Now - d->SetScrollTime));
		}
	}

	return d->Content;
}

LPointF LHtml::GetDpiScale()
{
	LPointF Scale(1.0, 1.0);
	auto Wnd = GetWindow();
	if (Wnd)
		Scale = Wnd->GetDpiScale();
	return Scale;
}

void LHtml::OnPaint(LSurface *ScreenDC)
{
	// LProfile Prof("LHtml::OnPaint");

	#if HTML_USE_DOUBLE_BUFFER
	LRect Client = GetClient();
	if (ScreenDC->IsScreen())
	{
		if (!MemDC ||
			(MemDC->X() < Client.X() || MemDC->Y() < Client.Y()))
		{
			if (MemDC.Reset(new LMemDC))
			{
				int Sx = Client.X() + 10;
				int Sy = Client.Y() + 10;
				if (!MemDC->Create(Sx, Sy, System32BitColourSpace))
				{
					MemDC.Reset();
				}
			}
		}
		if (MemDC)
		{
			MemDC->ClipRgn(NULL);
			#if 0//def _DEBUG
			MemDC->Colour(LColour(255, 0, 255));
			MemDC->Rectangle();
			#endif
		}
	}
	#endif

	LSurface *pDC = MemDC ? MemDC : ScreenDC;

	#if 0
	Gtk::cairo_matrix_t mx;
	Gtk::cairo_get_matrix(pDC->Handle(), &mx);
	LPoint Offset;
	WindowVirtualOffset(&Offset);
	printf("\tHtml paint mx=%g,%g off=%i,%i\n",
		mx.x0, mx.y0,
		Offset.x, Offset.y);
	#endif

	LColour cBack;
	if (GetCss())
	{
		LCss::ColorDef Bk = GetCss()->BackgroundColor();
		if (Bk.Type == LCss::ColorRgb)
			cBack = Bk;
	}
	if (!cBack.IsValid())
		cBack = LColour(Enabled() ? L_WORKSPACE : L_MED);
	pDC->Colour(cBack);
	pDC->Rectangle();

	if (Tag)
	{
		Layout();

		if (VScroll)
		{
			int LineY = GetFont()->GetHeight();
			int Vs = (int)VScroll->Value();
			pDC->SetOrigin(0, Vs * LineY);
		}

		bool InSelection = false;
		PaintStart = LCurrentTime();
		d->MaxPaintTimeout = false;

		Tag->OnPaint(pDC, InSelection, 0);

		if (d->MaxPaintTimeout)
		{
			LgiTrace("%s:%i - Html max paint time reached: %i ms.\n", _FL, LCurrentTime() - PaintStart);
		}
	}

	#if HTML_USE_DOUBLE_BUFFER
	if (MemDC)
	{
		pDC->SetOrigin(0, 0);
		ScreenDC->Blt(0, 0, MemDC);
	}
	#endif

	if (d->OnLoadAnchor && VScroll)
	{
		LAutoString a = d->OnLoadAnchor;
		GotoAnchor(a);
		LAssert(d->OnLoadAnchor == 0);
	}
}

bool LHtml::HasSelection()
{
	if (Cursor && Selection)
	{
		return	Cursor->Cursor >= 0 &&
				Selection->Selection >= 0 &&
				!(Cursor == Selection && Cursor->Cursor == Selection->Selection);
	}

	return false;
}

void LHtml::UnSelectAll()
{
	bool i = false;
	if (Cursor)
	{
		Cursor->Cursor = -1;
		Cursor = NULL;
		i = true;
	}
	if (Selection)
	{
		Selection->Selection = -1;
		Selection = NULL;
		i = true;
	}
	if (i)
	{
		Invalidate();
	}
}

void LHtml::SelectAll()
{
}

LTag *LHtml::GetLastChild(LTag *t)
{
	if (t && t->Children.Length())
	{
		for (LTag *i = ToTag(t->Children.Last()); i; )
		{
			LTag *c = i->Children.Length() ? ToTag(i->Children.Last()) : NULL;
			if (c)
				i = c;
			else
				return i;
		}
	}

	return 0;
}

LTag *LHtml::PrevTag(LTag *t)
{
	// This returns the previous tag in the tree as if all the tags were
	// listed via recursion using "in order".

	// Walk up the parent chain looking for a prev
	for (LTag *p = t; p; p = ToTag(p->Parent))
	{
		// Does this tag have a parent?
		if (p->Parent)
		{
			// Prev?
			LTag *pp = ToTag(p->Parent);
			ssize_t Idx = pp->Children.IndexOf(p);
			LTag *Prev = Idx > 0 ? ToTag(pp->Children[Idx - 1]) : NULL;
			if (Prev)
			{
				LTag *Last = GetLastChild(Prev);
				return Last ? Last : Prev;
			}
			else
			{
				return ToTag(p->Parent);
			}
		}
	}

	return 0;
}

LTag *LHtml::NextTag(LTag *t)
{
	// This returns the next tag in the tree as if all the tags were
	// listed via recursion using "in order".

	// Does this have a child tag?
	if (t->Children.Length() > 0)
	{
		return ToTag(t->Children.First());
	}
	else
	{
		// Walk up the parent chain
		for (LTag *p = t; p; p = ToTag(p->Parent))
		{
			// Does this tag have a next?
			if (p->Parent)
			{
				LTag *pp = ToTag(p->Parent);
				size_t Idx = pp->Children.IndexOf(p);
				
				LTag *Next = pp->Children.Length() > Idx + 1 ? ToTag(pp->Children[Idx + 1]) : NULL;
				if (Next)
				{
					return Next;
				}
			}
		}
	}

	return 0;
}

int LHtml::GetTagDepth(LTag *Tag)
{
	// Returns the depth of the tag in the tree.
	int n = 0;
	for (LTag *t = Tag; t; t = ToTag(t->Parent))
	{
		n++;
	}
	return n;
}

bool LHtml::IsCursorFirst()
{
	if (!Cursor || !Selection)
		return false;
	return CompareTagPos(Cursor, Cursor->Cursor, Selection, Selection->Selection);
}

bool LHtml::CompareTagPos(LTag *a, ssize_t AIdx, LTag *b, ssize_t BIdx)
{
	// Returns true if the 'a' is before 'b' point.
	if (!a || !b)
		return false;

	if (a == b)
	{
		return AIdx < BIdx;
	}
	else
	{
		LArray<LTag*> ATree, BTree;
		for (LTag *t = a; t; t = ToTag(t->Parent))
			ATree.AddAt(0, t);
		for (LTag *t = b; t; t = ToTag(t->Parent))
			BTree.AddAt(0, t);

		ssize_t Depth = MIN(ATree.Length(), BTree.Length());
		for (int i=0; i<Depth; i++)
		{
			LTag *at = ATree[i];
			LTag *bt = BTree[i];
			if (at != bt)
			{
				LAssert(i > 0);
				LTag *p = ATree[i-1];
				LAssert(BTree[i-1] == p);
				ssize_t ai = p->Children.IndexOf(at);
				ssize_t bi = p->Children.IndexOf(bt);
				return ai < bi;
			}
		}		
	}

	return false;
}

void LHtml::SetLoadImages(bool i)
{
	if (i ^ GetLoadImages())
	{
		LDocView::SetLoadImages(i);
		SendNotify(LNotifyShowImagesChanged);

		if (GetLoadImages() && Tag)
		{
			Tag->LoadImages();
		}
	}
}

char *LHtml::GetSelection()
{
	char *s = 0;

	if (Cursor && Selection)
	{
		LMemQueue p;
		bool InSelection = false;
		Tag->CopyClipboard(p, InSelection);

		int Len = (int)p.GetSize();
		if (Len > 0)
		{
			char16 *t = (char16*)p.New(sizeof(char16));
			if (t)
			{
				size_t Len = StrlenW(t);
				for (int i=0; i<Len; i++)
				{
					if (t[i] == 0xa0) t[i] = ' ';
				}
				s = WideToUtf8(t);
				DeleteArray(t);
			}
		}
	}

	return s;
}

bool LHtml::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (!Name)
		return false;
	if (!_stricmp(Name, "ShowImages"))
	{
		SetLoadImages(Value.CastInt32() != 0);
	}
	else return false;
	
	return true;
}

bool LHtml::Copy()
{
	LAutoString s(GetSelection());
	if (s)
	{
		RemoveZeroWidthCharacters(s);

		if (HasHomoglyphs(s, -1))
		{
			if (LgiMsg(	this,
						"Text contains homoglyph characters that maybe a phishing attack.\n"
						"Do you really want to copy it?",
						"Warning",
						MB_YESNO) == IDNO)
				return false;
		}
	
		LClipBoard c(this);
		
		LAutoWString w;
		#ifndef MAC
		w.Reset(Utf8ToWide(s));
		if (w) c.TextW(w);
		#endif

		c.Text(s, w == 0);

		return true;
	}
	else LgiTrace("%s:%i - Error: no selection\n", _FL);

	return false;
}

/*
static bool FindCallback(LFindReplaceCommon *Dlg, bool Replace, void *User)
{
	LHtml *h = (LHtml*)User;
	return h->OnFind(Dlg);
}
*/

void BuildTagList(LArray<LTag*> &t, LTag *Tag)
{
	t.Add(Tag);
	for (unsigned i=0; i<Tag->Children.Length(); i++)
	{
		LTag *c = ToTag(Tag->Children[i]);
		BuildTagList(t, c);
	}
}

static void FormEncode(LStringPipe &p, const char *c)
{
	const char *s = c;
	while (*c)
	{
		while (*c && *c != ' ')
			c++;
		
		if (c > s)
		{
			p.Write(s, c - s);
			c = s;
		}
		if (*c == ' ')
		{
			p.Write("+", 1);
			s = c;
		}
		else break;
	}
}

bool LHtml::OnSubmitForm(LTag *Form)
{
	if (!Form || !Environment)
	{
		LAssert(!"Bad param");
		return false;
	}

	const char *Method = NULL;
	const char *Action = NULL;
	if (!Form->Get("method", Method) ||
		!Form->Get("action", Action))
	{
		LAssert(!"Missing form action/method");
		return false;
	}
		
	LHashTbl<ConstStrKey<char,false>,char*> f;
	Form->CollectFormValues(f);
	bool Status = false;
	if (!_stricmp(Method, "post"))
	{
		LStringPipe p(256);
		bool First = true;

		// const char *Field;
		// for (char *Val = f.First(&Field); Val; Val = f.Next(&Field))
		for (auto v : f)
		{
			if (First)
				First = false;
			else
				p.Write("&", 1);
			
			FormEncode(p, v.key);
			p.Write("=", 1);
			FormEncode(p, v.value);
		}
		
		LAutoPtr<const char, true> Data(p.NewStr());
		Status = Environment->OnPostForm(this, Action, Data);
	}
	else if (!_stricmp(Method, "get"))
	{
		Status = Environment->OnNavigate(this, Action);
	}
	else
	{
		LAssert(!"Bad form method.");
	}
	
	f.DeleteArrays();
	return Status;
}

bool LHtml::OnFind(LFindReplaceCommon *Params)
{
	bool Status = false;

	if (Params)
	{
		if (!Params->Find)
			return Status;

		d->FindText.Reset(Utf8ToWide(Params->Find));
		d->MatchCase = Params->MatchCase;
	}		

	if (!Cursor)
		Cursor = Tag;

	if (Cursor && d->FindText)
	{
		LArray<LTag*> Tags;
		BuildTagList(Tags, Tag);
		ssize_t Start = Tags.IndexOf(Cursor);
		for (unsigned i=1; i<Tags.Length(); i++)
		{
			ssize_t Idx = (Start + i) % Tags.Length();
			LTag *s = Tags[Idx];

			if (s->Text())
			{
				char16 *Hit;
				
				if (d->MatchCase)
					Hit = StrstrW(s->Text(), d->FindText);
				else
					Hit = StristrW(s->Text(), d->FindText);
				
				if (Hit)
				{
					// found something...
					UnSelectAll();

					Selection = Cursor = s;
					Cursor->Cursor = Hit - s->Text();
					Selection->Selection = Cursor->Cursor + StrlenW(d->FindText);
					OnCursorChanged();
					
					if (VScroll)
					{
						// Scroll the tag into view...
						int y = s->AbsY();
						int LineY = GetFont()->GetHeight();
						int Val = y / LineY;
						SetVScroll(Val);
					}
					
					Invalidate();
					Status = true;
					break;
				}
			}
		}
	}

	return Status;
}

void LHtml::DoFind(std::function<void(bool)> Callback)
{
	LFindDlg *Dlg = new LFindDlg(this,
		[this](auto dlg, auto action)
		{
			OnFind(dlg);
			delete dlg;
		});
	
	Dlg->DoModal(NULL);
}

bool LHtml::OnKey(LKey &k)
{
	bool Status = false;

	if (k.Down())
	{
		int Dy = 0;
		int LineY = GetFont()->GetHeight();
		int Page = GetClient().Y() / LineY;

		switch (k.vkey)
		{
			case LK_F3:
			{
				OnFind(NULL);
				break;
			}
			#ifdef WIN32
			case LK_INSERT:
				goto DoCopy;
			#endif
			case LK_UP:
			{
				Dy = -1;
				Status = true;
				break;
			}
			case LK_DOWN:
			{
				Dy = 1;
				Status = true;
				break;
			}
			case LK_PAGEUP:
			{
				Dy = -Page;
				Status = true;
				break;
			}
			case LK_PAGEDOWN:
			{
				Dy = Page;
				Status = true;
				break;
			}
			case LK_HOME:
			{
				Dy = (int) (VScroll ? -VScroll->Value() : 0);
				Status = true;
				break;
			}
			case LK_END:
			{
				if (VScroll)
				{
					LRange r = VScroll->GetRange();
					Dy = (int)(r.End() - Page);
				}
				Status = true;
				break;
			}
			default:
			{
				switch (k.c16)
				{
					case 'f':
					case 'F':
					{
						if (k.CtrlCmd())
						{
							DoFind(NULL);
							Status = true;
						}
						break;
					}
					case 'c':
					case 'C':
					{
						#ifdef WIN32
						DoCopy:
						#endif
						if (k.CtrlCmd())
						{
							Copy();
							Status = true;
						}
						break;
					}
				}
				break;
			}
		}

		if (Dy && VScroll)
			SetVScroll(VScroll->Value() + Dy);
	}

	return Status;
}

int LHtml::ScrollY()
{
	return GetFont()->GetHeight() * (VScroll ? (int)VScroll->Value() : 0);
}

void LHtml::OnMouseClick(LMouse &m)
{
	Capture(m.Down());
	SetPulse(m.Down() ? 200 : -1);

	if (m.Down())
	{
		Focus(true);

		int Offset = ScrollY();
		bool TagProcessedClick = false;

		LTagHit Hit;
		if (Tag)
		{
			Tag->GetTagByPos(Hit, m.x, m.y + Offset, 0, false, DEBUG_TAG_BY_POS);
			#if DEBUG_TAG_BY_POS
			Hit.Dump("MouseClick");
			#endif
		}
		
		if (m.Left() && !m.IsContextMenu())
		{
			if (m.Double())
			{
				d->WordSelectMode = true;

				if (Cursor)
				{
					// Extend the selection out to the current word's boundaries.
					Selection = Cursor;
					Selection->Selection = Cursor->Cursor;

					if (Cursor->Text())
					{
						ssize_t Base = Cursor->GetTextStart();
						char16 *Text = Cursor->Text() + Base;
						while (Text[Cursor->Cursor])
						{
							char16 c = Text[Cursor->Cursor];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Cursor->Cursor++;
						}
					}
					if (Selection->Text())
					{
						ssize_t Base = Selection->GetTextStart();
						char16 *Sel = Selection->Text() + Base;
						while (Selection->Selection > 0)
						{
							char16 c = Sel[Selection->Selection - 1];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Selection->Selection--;
						}
					}

					Invalidate();
					SendNotify(LNotifySelectionChanged);
				}
			}
			else if (Hit.NearestText)
			{
				d->WordSelectMode = false;
				UnSelectAll();

				Cursor = Hit.NearestText;
				Cursor->Cursor = Hit.Index;

				#if DEBUG_SELECTION
				LgiTrace("StartSelect Near='%20S' Idx=%i\n", Hit.NearestText->Text(), Hit.Index);
				#endif

				OnCursorChanged();
				SendNotify(LNotifySelectionChanged);
			}
			else
			{
				#if DEBUG_SELECTION
				LgiTrace("StartSelect no text hit %p, %p\n", Cursor, Selection);
				#endif
			}
		}

		if (Hit.NearestText && Hit.Near == 0)
		{
			TagProcessedClick = Hit.NearestText->OnMouseClick(m);
		}
		else if (Hit.Direct)
		{
			TagProcessedClick = Hit.Direct->OnMouseClick(m);
		}
		#ifdef _DEBUG
		else if (m.Left() && m.Ctrl())
		{
			LgiMsg(this, "No tag under the cursor.", GetClass());
		}
		#endif

		if (!TagProcessedClick &&
			m.IsContextMenu())
		{
			LSubMenu RClick;

			enum ContextMenuCmds
			{
				IDM_DUMP = 100,
				IDM_COPY_SRC,
				IDM_VIEW_SRC,
				IDM_EXTERNAL,
				IDM_COPY,
				IDM_VIEW_IMAGES,
			};
			#define IDM_CHARSET_BASE	10000

			RClick.AppendItem					(LLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
			LMenuItem *Vs = RClick.AppendItem	(LLoadString(L_VIEW_SOURCE, "View Source"), IDM_VIEW_SRC, Source != 0);
			RClick.AppendItem					(LLoadString(L_COPY_SOURCE, "Copy Source"), IDM_COPY_SRC, Source != 0);
			LMenuItem *Load = RClick.AppendItem	(LLoadString(L_VIEW_IMAGES, "View External Images"), IDM_VIEW_IMAGES, true);
			if (Load) Load->Checked(GetLoadImages());
			RClick.AppendItem					(LLoadString(L_VIEW_IN_DEFAULT_BROWSER, "View in Default Browser"), IDM_EXTERNAL, Source != 0);
			LSubMenu *Cs = RClick.AppendSub		(LLoadString(L_CHANGE_CHARSET, "Change Charset"));

			if (Cs)
			{
				int n=0;
				for (LCharset *c = LGetCsList(); c->Charset; c++, n++)
				{
					Cs->AppendItem(c->Charset, IDM_CHARSET_BASE + n, c->IsAvailable());
				}
			}
			
			if (!GetReadOnly() || // Is editor
				#ifdef _DEBUG
				1
				#else
				0
				#endif
				)
			{
				RClick.AppendSeparator();
				RClick.AppendItem("Dump Layout", IDM_DUMP, Tag != 0);
			}

			if (Vs)
			{
				Vs->Checked(!IsHtml);
			}
			
			if (OnContextMenuCreate(Hit, RClick) &&
				GetMouse(m, true))
			{
				int Id = RClick.Float(this, m.x, m.y);
				switch (Id)
				{
					case IDM_COPY:
					{
						Copy();
						break;
					}
					case IDM_VIEW_SRC:
					{
						if (Vs)
						{
							DeleteObj(Tag);
							IsHtml = !IsHtml;
							ParseDocument(Source);
						}
						break;
					}
					case IDM_COPY_SRC:
					{
						if (Source)
						{
							LClipBoard c(this);
							const char *ViewCs = GetCharset();
							if (ViewCs)
							{
								LAutoWString w((char16*)LNewConvertCp(LGI_WideCharset, Source, ViewCs));
								if (w)
									c.TextW(w);
							}
							else c.Text(Source);
						}
						break;
					}
					case IDM_VIEW_IMAGES:
					{
						SetLoadImages(!GetLoadImages());
						break;
					}
					case IDM_DUMP:
					{
						if (Tag)
						{
							LAutoWString s = Tag->DumpW();
							if (s)
							{
								LClipBoard c(this);
								c.TextW(s);
							}
						}
						break;
					}
					case IDM_EXTERNAL:
					{
						if (!Source)
						{
							LgiTrace("%s:%i - No HTML source code.\n", _FL);
							break;
						}

						char Path[MAX_PATH_LEN];
						if (!LGetSystemPath(LSP_TEMP, Path, sizeof(Path)))
						{
							LgiTrace("%s:%i - Failed to get the system path.\n", _FL);
							break;
						}
						
						char f[32];
						sprintf_s(f, sizeof(f), "_%i.html", LRand(1000000));
						LMakePath(Path, sizeof(Path), Path, f);
						
						LFile F;
						if (!F.Open(Path, O_WRITE))
						{
							LgiTrace("%s:%i - Failed to open '%s' for writing.\n", _FL, Path);
							break;
						}

						LStringPipe Ex;
						bool Error = false;
						
						F.SetSize(0);

						LAutoWString SrcMem;
						const char *ViewCs = GetCharset();
						if (ViewCs)
							SrcMem.Reset((char16*)LNewConvertCp(LGI_WideCharset, Source, ViewCs));
						else
							SrcMem.Reset(Utf8ToWide(Source));						

						for (char16 *s=SrcMem; s && *s;)
						{
							char16 *cid = StristrW(s, L"cid:");
							while (cid && !strchr("\'\"", cid[-1]))
							{
								cid = StristrW(cid+1, L"cid:");
							}

							if (cid)
							{
								char16 Delim = cid[-1];
								char16 *e = StrchrW(cid, Delim);
								if (e)
								{
									*e = 0;
									if (StrchrW(cid, '\n'))
									{
										*e = Delim;
										Error = true;
										break;
									}
									else
									{
										char File[MAX_PATH_LEN] = "";
										if (Environment)
										{
											LDocumentEnv::LoadJob *j = Environment->NewJob();
											if (j)
											{
												j->Uri.Reset(WideToUtf8(cid));
												j->Env = Environment;
												j->Pref = LDocumentEnv::LoadJob::FmtFilename;
												j->UserUid = GetDocumentUid();

												LDocumentEnv::LoadType Result = Environment->GetContent(j);
												if (Result == LDocumentEnv::LoadImmediate)
												{
													if (j->Filename)
														strcpy_s(File, sizeof(File), j->Filename);
												}
												else if (Result == LDocumentEnv::LoadDeferred)
												{
													d->DeferredLoads++;
												}
												
												DeleteObj(j);
											}
										}
										
										*e = Delim;
										Ex.Push(s, cid - s);
										if (File[0])
										{
											char *d;
											while ((d = strchr(File, '\\')))
											{
												*d = '/';
											}
											
											Ex.Push(L"file:///");
											
											LAutoWString w(Utf8ToWide(File));
											Ex.Push(w);
										}
										s = e;
									}
								}
								else
								{
									Error = true;
									break;
								}
							}
							else
							{
								Ex.Push(s);
								break;
							}
						}

						if (!Error)
						{
							int64 WideChars = Ex.GetSize() / sizeof(char16);
							LAutoWString w(Ex.NewStrW());
							
							LAutoString u(WideToUtf8(w, WideChars));
							if (u)
								F.Write(u, strlen(u));
							F.Close();
							
							LString Err;
							if (!LExecute(Path, NULL, NULL, &Err))
							{
								LgiMsg(	this,
										"Failed to open '%s'\n%s",
										LAppInst ? LAppInst->LBase::Name() : GetClass(),
										MB_OK,
										Path,
										Err.Get());
							}
						}
						break;
					}
					default:
					{
						if (Id >= IDM_CHARSET_BASE)
						{
							LCharset *c = LGetCsList() + (Id - IDM_CHARSET_BASE);
							if (c->Charset)
							{
								Charset = c->Charset;
								OverideDocCharset = true;

								char *Src = Source.Release();
								_Delete();
								_New();
								Source.Reset(Src);
								ParseDocument(Source);

								Invalidate();

								SendNotify(LNotifyCharsetChanged);
							}								
						}
						else
						{
							OnContextMenuCommand(Hit, Id);
						}
						break;
					}
				}
			}
		}
	}
	else // Up Click
	{
		if (Selection &&
			Cursor &&
			Selection == Cursor &&
			Selection->Selection == Cursor->Cursor)
		{
			Selection->Selection = -1;
			Selection = 0;
			SendNotify(LNotifySelectionChanged);

			#if DEBUG_SELECTION
			LgiTrace("NoSelect on release\n");
			#endif
		}
	}
}

void LHtml::OnLoad()
{
	d->IsLoaded = true;
	SendNotify(LNotifyDocLoaded);
}

LTag *LHtml::GetTagByPos(int x, int y, ssize_t *Index, LPoint *LocalCoords, bool DebugLog)
{
	LTag *Status = NULL;

	if (Tag)
	{
		if (DebugLog)
			LgiTrace("GetTagByPos starting...\n");

		LTagHit Hit;
		Tag->GetTagByPos(Hit, x, y, 0, DebugLog);

		if (DebugLog)
			LgiTrace("GetTagByPos Hit=%s, %i, %i...\n\n", Hit.Direct ? Hit.Direct->Tag.Get() : 0, Hit.Index, Hit.Near);
		
		Status = Hit.NearestText && Hit.Near == 0 ? Hit.NearestText : Hit.Direct;
		if (Hit.NearestText && Hit.Near < 30)
		{
			if (Index) *Index = Hit.Index;
			if (LocalCoords) *LocalCoords = Hit.LocalCoords;
		}
	}

	return Status;
}

void LHtml::SetVScroll(int64 v)
{
	if (!VScroll)
		return;
	
	if (Tag)
		Tag->ClearToolTips();

	VScroll->Value(v);
	Invalidate();
}

bool LHtml::OnMouseWheel(double Lines)
{
	if (VScroll)
		SetVScroll(VScroll->Value() + (int64)Lines);
	return true;
}

LCursor LHtml::GetCursor(int x, int y)
{
	int Offset = ScrollY();
	ssize_t Index = -1;
	LPoint LocalCoords;
	LTag *Tag = GetTagByPos(x, y + Offset, &Index, &LocalCoords);
	if (Tag)
	{
		LString Uri;
		if (LocalCoords.x >= 0 &&
			LocalCoords.y >= 0 &&
			Tag->IsAnchor(&Uri))
		{
			LRect c = GetClient();
			c.Offset(-c.x1, -c.y1);
			if (c.Overlap(x, y) && ValidStr(Uri))
			{
				return LCUR_PointingHand;
			}
		}
	}
	
	return LCUR_Normal;
}

void LTag::ClearToolTips()
{
	if (TipId)
	{
		Html->Tip.DeleteTip(TipId);
		TipId = 0;
	}

	for (auto c: Children)
		ToTag(c)->ClearToolTips();
}


void LHtml::OnMouseMove(LMouse &m)
{
	if (!Tag)
		return;

	int Offset = ScrollY();
	LTagHit Hit;
	Tag->GetTagByPos(Hit, m.x, m.y + Offset, 0, false);
	if (!Hit.Direct && !Hit.NearestText)
		return;

	LString Uri;
	LTag *HitTag = Hit.NearestText && Hit.Near == 0 ? Hit.NearestText : Hit.Direct;
	if (HitTag &&
		HitTag->TipId == 0 &&
		Hit.LocalCoords.x >= 0 &&
		Hit.LocalCoords.y >= 0 &&
		HitTag->IsAnchor(&Uri) &&
		Uri)
	{
		if (!Tip.GetParent())
		{
			Tip.Attach(this);
		}

		LRect r = HitTag->GetRect(false);
		r.Offset(0, -Offset);
		if (!HitTag->TipId)
			HitTag->TipId = Tip.NewTip(Uri, r);
		// LgiTrace("NewTip: %s @ %s, ID=%i\n", Uri.Get(), r.GetStr(), HitTag->TipId);
	}

	if (IsCapturing() &&
		Cursor &&
		Hit.NearestText)
	{
		if (!Selection)
		{
			Selection = Cursor;
			Selection->Selection = Cursor->Cursor;
			Cursor = Hit.NearestText;
			Cursor->Cursor = Hit.Index;
			OnCursorChanged();
			Invalidate();
			
			SendNotify(LNotifySelectionChanged);

			#if DEBUG_SELECTION
			LgiTrace("CreateSelection '%20S' %i\n", Hit.NearestText->Text(), Hit.Index);
			#endif
		}
		else if ((Cursor != Hit.NearestText) ||
				 (Cursor->Cursor != Hit.Index))
		{
			// Move the cursor to track the mouse
			if (Cursor)
			{
				Cursor->Cursor = -1;
			}

			Cursor = Hit.NearestText;
			Cursor->Cursor = Hit.Index;
			#if DEBUG_SELECTION
			LgiTrace("ExtendSelection '%20S' %i\n", Hit.NearestText->Text(), Hit.Index);
			#endif

			if (d->WordSelectMode && Cursor->Text())
			{
				ssize_t Base = Cursor->GetTextStart();
				if (IsCursorFirst())
				{
					// Extend the cursor up the document to include the whole word
					while (Cursor->Cursor > 0)
					{
						char16 c = Cursor->Text()[Base + Cursor->Cursor - 1];

						if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
							break;

						Cursor->Cursor--;
					}
				}
				else
				{
					// Extend the cursor down the document to include the whole word
					while (Cursor->Text()[Base + Cursor->Cursor])
					{
						char16 c = Cursor->Text()[Base + Cursor->Cursor];

						if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
							break;

						Cursor->Cursor++;
					}
				}
			}

			OnCursorChanged();
			Invalidate();
			SendNotify(LNotifySelectionChanged);
		}
	}
}

void LHtml::OnPulse()
{
	if (VScroll && IsCapturing())
	{
		int Fy = DefFont() ? DefFont()->GetHeight() : 16;
		LMouse m;
		if (GetMouse(m, false))
		{
			LRect c = GetClient();
			int Lines = 0;
			
			if (m.y < c.y1)
			{
				// Scroll up
				Lines = (c.y1 - m.y + Fy - 1) / -Fy;
			}
			else if (m.y > c.y2)
			{
				// Scroll down
				Lines = (m.y - c.y2 + Fy - 1) / Fy;
			}
			
			if (Lines && VScroll)
				SetVScroll(VScroll->Value() +  Lines);
		}
	}
}

LRect *LHtml::GetCursorPos()
{
	return &d->CursorPos;
}

void LHtml::SetCursorVis(bool b)
{
	if (d->CursorVis ^ b)
	{
		d->CursorVis = b;
		Invalidate();
	}
}

bool LHtml::GetCursorVis()
{
	return d->CursorVis;
}

LDom *ElementById(LTag *t, char *id)
{
	if (t && id)
	{
		const char *i;
		if (t->Get("id", i) && _stricmp(i, id) == 0)
			return t;

		for (unsigned i=0; i<t->Children.Length(); i++)
		{
			LTag *c = ToTag(t->Children[i]);
			LDom *n = ElementById(c, id);
			if (n) return n;
		}
	}

	return 0;
}

LDom *LHtml::getElementById(char *Id)
{
	return ElementById(Tag, Id);
}

bool LHtml::GetLinkDoubleClick()
{
	return d->LinkDoubleClick;
}

void LHtml::SetLinkDoubleClick(bool b)
{
	d->LinkDoubleClick = b;
}

bool LHtml::GetFormattedContent(const char *MimeType, LString &Out, LArray<LDocView::ContentMedia> *Media)
{
	if (!MimeType)
	{
		LAssert(!"No MIME type for getting formatted content");
		return false;
	}

	if (!_stricmp(MimeType, "text/html"))
	{
		// We can handle this type...
		LArray<LTag*> Imgs;
		if (Media)
		{
			// Find all the image tags...
			Tag->Find(TAG_IMG, Imgs);

			// Give them CID's if they don't already have them
			for (unsigned i=0; i<Imgs.Length(); i++)
			{
				LTag *Img = Imgs[i];
				if (!Img)
					continue;
				
				const char *Cid = NULL, *Src;
				if (Img->Get("src", Src) &&
					!Img->Get("cid", Cid))
				{
					char id[256];
					sprintf_s(id, sizeof(id), "%x.%x", (unsigned)LCurrentTime(), (unsigned)LRand());
					Img->Set("cid", id);
					Img->Get("cid", Cid);
				}

				if (Src && Cid)
				{
					LFile *f = new LFile;
					if (f)
					{
						if (f->Open(Src, O_READ))
						{
							// Add the exported image stream to the media array
							LDocView::ContentMedia &m = Media->New();
							m.Id = Cid;
							m.Stream.Reset(f);
						}
					}
				}
			}
		}

		// Export the HTML, including the CID's from the first step
		Out = Name();
	}
	else if (!_stricmp(MimeType, "text/plain"))
	{
		// Convert DOM tree down to text instead...
		LStringPipe p(512);
		if (Tag)
		{
			LTag::TextConvertState State(&p);
			Tag->ConvertToText(State);
		}
		Out = p.NewLStr();
	}

	return false;
}

void LHtml::OnContent(LDocumentEnv::LoadJob *Res)
{
	if (JobSem.Lock(_FL))
	{
		JobSem.Jobs.Add(Res);
		JobSem.Unlock();		
		PostEvent(M_JOBS_LOADED);
	}
}

LHtmlElement *LHtml::CreateElement(LHtmlElement *Parent)
{
	return new LTag(this, Parent);
}

bool LHtml::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (!_stricmp(Name, "supportLists")) // Type: Bool
		Value = false;
	else if (!_stricmp(Name, "vml")) // Type: Bool
		// Vector Markup Language
		Value = false;
	else if (!_stricmp(Name, "mso")) // Type: Bool
		// mso = Microsoft Office
		Value = false;
	else
		return false;

	return true;
}

bool LHtml::EvaluateCondition(const char *Cond)
{
	if (!Cond)
		return true;
	
	// This is a really bad attempt at writing an expression evaluator.
	// I could of course use the scripting language but that would pull
	// in a fairly large dependency on the HTML control. However user
	// apps that already have that could reimplement this virtual function
	// if they feel like it.
	LArray<char*> Str;
	for (const char *c = Cond; *c; )
	{
		if (IsAlpha(*c))
		{
			Str.Add(LTokStr(c));
		}
		else if (IsWhiteSpace(*c))
		{
			c++;
		}
		else
		{
			const char *e = c;
			while (*e && !IsWhiteSpace(*e) && !IsAlpha(*e))
				e++;
			Str.Add(NewStr(c, e - c));
			LAssert(e > c);
			if (e > c)
				c = e;
			else
				break;
		}
	}

	bool Result = true;
	bool Not = false;
	for (unsigned i=0; i<Str.Length(); i++)
	{
		char *s = Str[i];
		if (!_stricmp(s, "!"))
			Not = true;
		else
		{
			LVariant v;
			if (GetValue(s, v))
			{
				Result = v.CastInt32() != 0;
				if (Not) Result = !Result;
			}
			else
			{
				// If this fires: update LHtml::GetVariant with the variable.
				// LgiTrace("%s:%i - Unsupported variable '%s'\n", _FL, s);
			}
			Not = false;
		}
	}
	
	Str.DeleteArrays();
	
	return Result;
}

bool LHtml::GotoAnchor(char *Name)
{
	if (Tag)
	{
		LTag *a = Tag->GetAnchor(Name);
		if (a)
		{
			if (VScroll)
			{
				int LineY = GetFont()->GetHeight();
				int Ay = a->AbsY();
				int Scr = Ay / LineY;
				SetVScroll(Scr);
				VScroll->SendNotify();
			}
			else
				d->OnLoadAnchor.Reset(NewStr(Name));
		}
	}

	return false;
}

bool LHtml::GetEmoji()
{
	return d->DecodeEmoji;
}

void LHtml::SetEmoji(bool i)
{
	d->DecodeEmoji = i;
}

void LHtml::SetMaxPaintTime(int Ms)
{
	d->MaxPaintTime = Ms;
}

bool LHtml::GetMaxPaintTimeout()
{
	return d->MaxPaintTimeout;
}

////////////////////////////////////////////////////////////////////////
class LHtml_Factory : public LViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "LHtml") == 0)
		{
			return new LHtml(-1, 0, 0, 100, 100, new LDefaultDocumentEnv);
		}

		return 0;
	}

} LHtml_Factory;

//////////////////////////////////////////////////////////////////////
struct BuildContext
{
	LHtmlTableLayout *Layout;
	LTag *Table;
	LTag *TBody;
	LTag *CurTr;
	LTag *CurTd;
	int cx, cy;
	
	BuildContext()
	{
		Layout = NULL;
		cx = cy = 0;

		Table = NULL;
		TBody = NULL;
		CurTr = NULL;
		CurTd = NULL;
	}

	bool Build(LTag *t, int Depth)
	{
		bool RetReattach = false;
		
		switch (t->TagId)
		{
			case TAG_TABLE:
			{
				if (!Table)
					Table = t;
				else
					return false;
				break;
			}
			case TAG_TBODY:
			{
				if (TBody)
					return false;
				TBody = t;
				break;
			}
			case TAG_TR:
			{
				CurTr = t;
				break;
			}
			case TAG_TD:
			{
				CurTd = t;
				if (t->Parent != CurTr)
				{
					if
					(
						!CurTr &&
						(Table || TBody)
					)
					{
						LTag *p = TBody ? TBody : Table;
						CurTr = new LTag(p->Html, p);
						if (CurTr)
						{
							CurTr->Tag.Reset(NewStr("tr"));
							CurTr->TagId = TAG_TR;
							

							ssize_t Idx = t->Parent->Children.IndexOf(t);
							t->Parent->Attach(CurTr, Idx);
						}
					}
					
					if (CurTr)
					{
						CurTr->Attach(t);
						RetReattach = true;
					}
					else
					{
						LAssert(0);
						return false;
					}
				}
				
				t->Cell->Pos.x = cx;
				t->Cell->Pos.y = cy;
				Layout->Set(t);
				break;
			}
			default:
			{
				if (CurTd == t->Parent)
					return false;
				break;
			}
		}

		for (unsigned n=0; n<t->Children.Length(); n++)
		{
			LTag *c = ToTag(t->Children[n]);
			bool Reattached = Build(c, Depth+1);
			if (Reattached)
				n--;
		}
		
		if (t->TagId == TAG_TR)
		{
			CurTr = NULL;
			cy++;
			cx = 0;
			Layout->s.y = cy;
		}
		if (t->TagId == TAG_TD)
		{
			CurTd = NULL;
			cx += t->Cell->Span.x;
			Layout->s.x = MAX(cx, Layout->s.x);
		}
		
		return RetReattach;
	}
};

LHtmlTableLayout::LHtmlTableLayout(LTag *table)
{
	Table = table;
	if (!Table)
		return;

	#if 0

	BuildContext Ctx;
	Ctx.Layout = this;
	Ctx.Build(table, 0);

	#else

	int y = 0;
	LTag *FakeRow = 0;
	LTag *FakeCell = 0;

	LTag *r;
	for (size_t i=0; i<Table->Children.Length(); i++)
	{
		r = ToTag(Table->Children[i]);
		if (r->Display() == LCss::DispNone)
			continue;
			
		if (r->TagId == TAG_TR)
		{
			FakeRow = 0;
			FakeCell = 0;
		}
		else if (r->TagId == TAG_TBODY)
		{
			ssize_t Index = Table->Children.IndexOf(r);
			for (size_t n=0; n<r->Children.Length(); n++)
			{
				LTag *t = ToTag(r->Children[n]);
				Table->Children.AddAt(++Index, t);
				t->Parent = Table;
				
				/*
				LgiTrace("Moving '%s'(%p) from TBODY(%p) into '%s'(%p)\n",
					t->Tag, t,
					r,
					t->Parent->Tag, t->Parent);
				*/
			}
			r->Children.Length(0);
		}
		else
		{
			if (!FakeRow)
			{
				if ((FakeRow = new LTag(Table->Html, 0)))
				{
					FakeRow->Tag.Reset(NewStr("tr"));
					FakeRow->TagId = TAG_TR;

					ssize_t Idx = Table->Children.IndexOf(r);
					Table->Attach(FakeRow, Idx);
				}
			}
			if (FakeRow)
			{
				if (!IsTableCell(r->TagId) && !FakeCell)
				{
					if ((FakeCell = new LTag(Table->Html, FakeRow)))
					{
						FakeCell->Tag.Reset(NewStr("td"));
						FakeCell->TagId = TAG_TD;
						if ((FakeCell->Cell = new LTag::TblCell))
						{
							FakeCell->Cell->Span.x = 1;
							FakeCell->Cell->Span.y = 1;
						}
					}
				}

				ssize_t Idx = Table->Children.IndexOf(r);
				r->Detach();

				if (IsTableCell(r->TagId))
				{
					FakeRow->Attach(r);
				}
				else
				{
					LAssert(FakeCell != NULL);
					FakeCell->Attach(r);
				}
				i = Idx - 1;
			}
		}
	}

	FakeCell = NULL;
	for (size_t n=0; n<Table->Children.Length(); n++)
	{
		LTag *r = ToTag(Table->Children[n]);
		if (r->TagId == TAG_TR)
		{
			int x = 0;
			for (size_t i=0; i<r->Children.Length(); i++)
			{
				LTag *cell = ToTag(r->Children[i]);
				if (!IsTableCell(cell->TagId))
				{
					if (!FakeCell)
					{
						// Make a fake TD cell
						FakeCell = new LTag(Table->Html, NULL);
						FakeCell->Tag.Reset(NewStr("td"));
						FakeCell->TagId = TAG_TD;
						if ((FakeCell->Cell = new LTag::TblCell))
						{
							FakeCell->Cell->Span.x = 1;
							FakeCell->Cell->Span.y = 1;
						}
						
						// Join the fake TD into the TR
						r->Children[i] = FakeCell;
						FakeCell->Parent = r;
					}
					else
					{
						// Not the first non-TD tag, so delete it from the TR. Only the
						// fake TD will remain in the TR.
						r->Children.DeleteAt(i--, true);
					}
					
					// Insert the tag into it as a child
					FakeCell->Children.Add(cell);
					cell->Parent = FakeCell;
					cell = FakeCell;
				}
				else
				{
					FakeCell = NULL;
				}
				
				if (IsTableCell(cell->TagId))
				{
					if (cell->Display() == LCss::DispNone)
						continue;
						
					while (Get(x, y))
					{
						x++;
					}

					cell->Cell->Pos.x = x;
					cell->Cell->Pos.y = y;
					Set(cell);
					x += cell->Cell->Span.x;
				}
			}

			y++;
			FakeCell = NULL;
		}
	}
	
	#endif
}

void LHtmlTableLayout::Dump()
{
	int Sx, Sy;
	GetSize(Sx, Sy);

	LgiTrace("Table %i x %i cells.\n", Sx, Sy);
	for (int x=0; x<Sx; x++)
	{
		LgiTrace("--- %-2i--- ", x);
	}
	LgiTrace("\n");

	for (int y=0; y<Sy; y++)
	{
		int x;
		for (x=0; x<Sx; x++)
		{
			LTag *t = Get(x, y);
			LgiTrace("%-10p", t);
		}
		LgiTrace("\n");

		for (x=0; x<Sx; x++)
		{
			LTag *t = Get(x, y);
			char s[256] = "";
			if (t)
				sprintf_s(s, sizeof(s), "%i,%i-%i,%i", t->Cell->Pos.x, t->Cell->Pos.y, t->Cell->Span.x, t->Cell->Span.y);
			LgiTrace("%-10s", s);
		}
		LgiTrace("\n");
	}

	LgiTrace("\n");
}

void LHtmlTableLayout::GetAll(List<LTag> &All)
{
	LHashTbl<PtrKey<void*>, bool> Added;
	for (size_t y=0; y<c.Length(); y++)
	{
		CellArray &a = c[y];
		for (size_t x=0; x<a.Length(); x++)
		{
			LTag *t = a[x];
			if (t && !Added.Find(t))
			{
				Added.Add(t, true);
				All.Insert(t);
			}
		}
	}
}

void LHtmlTableLayout::GetSize(int &x, int &y)
{
	x = 0;
	y = (int)c.Length();

	for (size_t i=0; i<c.Length(); i++)
	{
		x = MAX(x, (int) c[i].Length());
	}
}

LTag *LHtmlTableLayout::Get(int x, int y)
{
	if (y >= (int) c.Length())
		return NULL;
	
	CellArray &a = c[y];
	if (x >= (int) a.Length())
		return NULL;
	
	return a[x];
}

bool LHtmlTableLayout::Set(LTag *t)
{
	if (!t)
		return false;

	for (int y=0; y<t->Cell->Span.y; y++)
	{
		for (int x=0; x<t->Cell->Span.x; x++)
		{
			// LAssert(!c[y][x]);
			c[t->Cell->Pos.y + y][t->Cell->Pos.x + x] = t;
		}
	}

	return true;
}

void LTagHit::Dump(const char *Desc)
{
	LArray<LTag*> d, n;
	LTag *t = Direct;
	unsigned i;
	for (i=0; i<3 && t; t = ToTag(t->Parent), i++)
	{
		d.AddAt(0, t);
	}
	t = NearestText;
	for (i=0; i<3 && t; t = ToTag(t->Parent), i++)
	{
		n.AddAt(0, t);
	}
	
	LgiTrace("Hit: %s Direct: ", Desc);
	for (i=0; i<d.Length(); i++)
		LgiTrace(">%s", d[i]->Tag ? d[i]->Tag.Get() : "CONTENT");
	LgiTrace(" Nearest: ");
	for (i=0; i<n.Length(); i++)
		LgiTrace(">%s", n[i]->Tag ? n[i]->Tag.Get() : "CONTENT");
	LgiTrace(" Local: %ix%i Index: %i Block: %s '%.10S'\n",
		LocalCoords.x, LocalCoords.y,
		Index,
		Block ? Block->GetStr() : NULL,
		Block ? Block->Text + Index : NULL);
}

