#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"
#include "RichTextEditPriv.h"
#include "lgi/common/Emoji.h"
#include "lgi/common/DocView.h"
#include "lgi/common/Menu.h"
#include "lgi/common/GdcTools.h"

#define DEBUG_LAYOUT				0

//////////////////////////////////////////////////////////////////////////////////////////////////
LRichTextPriv::StyleText::StyleText(const StyleText *St)
{
	Emoji   = St->Emoji;
	Element = St->Element;
	Param   = St->Param;
	if (St->Style)
		SetStyle(St->Style);
	Add((uint32_t*)&St->ItemAt(0), St->Length());
}
		
LRichTextPriv::StyleText::StyleText(const uint32_t *t, ssize_t Chars, LNamedStyle *style)
{
	Emoji = false;
	Element = CONTENT;
	if (style)
		SetStyle(style);
	if (t)
	{
		if (Chars < 0)
			Chars = Strlen(t);
		Add((uint32_t*)t, (int)Chars);
	}
}

uint32_t *LRichTextPriv::StyleText::At(ssize_t i)
{
	if (i >= 0 && i < (int)Length())
		return &(*this)[i];
	LAssert(0);
	return NULL;
}
		
LNamedStyle *LRichTextPriv::StyleText::GetStyle()
{
	return Style;
}
				
void LRichTextPriv::StyleText::SetStyle(LNamedStyle *s)
{
	if (Style != s)
	{
		Style = s;
		Colours.Empty();
				
		if (Style)
		{			
			LCss::ColorDef c = Style->Color();
			if (c.Type == LCss::ColorRgb)
				Colours.Fore.Set(c.Rgb32, 32);
			c = Style->BackgroundColor();
			if (c.Type == LCss::ColorRgb)
				Colours.Back.Set(c.Rgb32, 32);
		}				
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
LRichTextPriv::EmojiDisplayStr::EmojiDisplayStr(StyleText *src,
												LSurface *img,
												LCss::Len &fntSize,
												const uint32_t *s,
												ssize_t l) :
	DisplayStr(src, NULL, s, l)
{
	Img = img;

	#if defined(_MSC_VER)
		Utf32.Add(s, l);
		uint32_t *u = &Utf32[0];
	#else
		LAssert(sizeof(char16) == 4);
		uint32_t *u = (uint32_t*)Wide;
		Chars = Strlen(u);
	#endif

	CharPx = EMOJI_CELL_SIZE;
	Size = fntSize;
	if (Size.IsValid())
	{
		int Px = Size.ToPx();
		if (Px > 0)
			CharPx = (int)(Px / 0.8);
	}

	for (int i=0; i<Chars; )
	{
		auto Emoji = EmojiToIconIndex(u + i, Chars - i);
		LAssert(Emoji.Index >= 0);
		if (Emoji.Index >= 0)
		{
			int x = Emoji.Index % EMOJI_GROUP_X;
			int y = Emoji.Index / EMOJI_GROUP_X;
			LRect &rc = SrcRect[i];
			rc.ZOff(EMOJI_CELL_SIZE-1, EMOJI_CELL_SIZE-1);
			rc.Offset(x * EMOJI_CELL_SIZE, y * EMOJI_CELL_SIZE);

			i += Emoji.Size;
		}
		else
		{
			LAssert(!"Not an emoji.");
			break;
		}
	}

	x = (int)SrcRect.Length() * CharPx;
	y = CharPx;
	xf = IntToFixed(x);
	yf = IntToFixed(y);
}

LAutoPtr<LRichTextPriv::DisplayStr> LRichTextPriv::EmojiDisplayStr::Clone(ssize_t Start, ssize_t Len)
{
	if (Len < 0)
		Len = Chars - Start;
	#if defined(_MSC_VER)
		LAssert(	Start >= 0 &&
					Start < (int)Utf32.Length() &&
					Start + Len <= (int)Utf32.Length());
	#endif
	LAutoPtr<DisplayStr> s(new EmojiDisplayStr(Src, Img, Size,
		#if defined(_MSC_VER)
		&Utf32[Start]
		#else
		(uint32_t*)(const char16*)(*this)
		#endif
		, Len));
	return s;
}

void LRichTextPriv::EmojiDisplayStr::Paint(LSurface *pDC, int &FixX, int FixY, LColour &Back)
{
	LRect f(0, 0, x-1, y-1);

	f.Offset(FixedToInt(FixX), FixedToInt(FixY));
	pDC->Colour(Back);
	pDC->Rectangle(&f);

	int Op = pDC->Op(GDC_ALPHA);
	for (unsigned i=0; i<SrcRect.Length(); i++)
	{
		if (CharPx != EMOJI_CELL_SIZE)
		{
			LMemDC mem(CharPx, CharPx, System32BitColourSpace);
			ResampleDC(&mem, Img, &SrcRect[i]);
			pDC->Blt(f.x1, f.y1, &mem);
		}
		else
		{
			pDC->Blt(f.x1, f.y1, Img, &SrcRect[i]);
		}
		f.x1 += CharPx;
		FixX += IntToFixed(CharPx);
	}

	pDC->Op(Op);
}

double LRichTextPriv::EmojiDisplayStr::GetAscent()
{
	return CharPx * 0.8;
}

ssize_t LRichTextPriv::EmojiDisplayStr::PosToIndex(int XPos, bool Nearest)
{
	if (XPos >= (int)x)
		return Chars;
	if (XPos <= 0)
		return 0;
	return (XPos + (Nearest ? CharPx >> 1 : 0)) / CharPx;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
LRichTextPriv::TextLine::TextLine(int XOffsetPx, int WidthPx, int YOffsetPx)
{
	NewLine = 0;
	PosOff.ZOff(0, 0);
	PosOff.Offset(XOffsetPx, YOffsetPx);
}

ssize_t LRichTextPriv::TextLine::Length()
{
	ssize_t Len = NewLine;
	for (unsigned i=0; i<Strs.Length(); i++)
		Len += Strs[i]->Chars;
	return Len;
}
		
/// This runs after the layout line has been filled with display strings.
/// It measures the line and works out the right offsets for each strings
/// so that their baselines all match up correctly.
void LRichTextPriv::TextLine::LayoutOffsets(int DefaultFontHt)
{
	double BaseLine = 0.0;
	int HtPx = 0;
			
	for (unsigned i=0; i<Strs.Length(); i++)
	{
		DisplayStr *ds = Strs[i];
		double Ascent = ds->GetAscent();
		BaseLine = MAX(BaseLine, Ascent);
		HtPx = MAX(HtPx, ds->Y());
	}
			
	if (Strs.Length() == 0)
		HtPx = DefaultFontHt;
	else
		LAssert(HtPx > 0);
			
	for (unsigned i=0; i<Strs.Length(); i++)
	{
		DisplayStr *ds = Strs[i];
		double Ascent = ds->GetAscent();
		if (Ascent > 0.0)
			ds->OffsetY = (int)(BaseLine - Ascent);
		LAssert(ds->OffsetY >= 0);
		HtPx = MAX(HtPx, ds->OffsetY+ds->Y());
	}
			
	PosOff.y2 = PosOff.y1 + HtPx - 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
LRichTextPriv::TextBlock::TextBlock(LRichTextPriv *priv) : Block(priv)
{
	LayoutDirty = false;
	Len = 0;
	Pos.ZOff(-1, -1);
	Style = NULL;
	Fnt = NULL;
	ClickErrIdx = -1;
			
	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}

LRichTextPriv::TextBlock::TextBlock(const TextBlock *Copy) : Block(Copy)
{
	LayoutDirty = true;
	Len = Copy->Len;
	Pos = Copy->Pos;
	Style = Copy->Style;
	Fnt = Copy->Fnt;
			
	Margin = Copy->Margin;
	Border = Copy->Border;
	Padding = Copy->Padding;

	for (unsigned i=0; i<Copy->Txt.Length(); i++)
	{
		Txt.Add(new StyleText(Copy->Txt.ItemAt(i)));
	}
}
		
LRichTextPriv::TextBlock::~TextBlock()
{
	LAssert(Cursors == 0);
	Txt.DeleteObjects();
}

void LRichTextPriv::TextBlock::Dump()
{
	LgiTrace("    Txt.Len=%i, margin=%s, border=%s, padding=%s\n",
				Txt.Length(),
				Margin.GetStr(),
				Border.GetStr(),
				Padding.GetStr());
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		LString s(t->Length() ?
					#ifndef WINDOWS
					(char16*)
					#endif
					t->At(0) : NULL, t->Length());
		s = s.Strip();
				
		LgiTrace("    %p: style=%p/%s, len=%i\n",
			t,
			t->GetStyle(),
			t->GetStyle() ? t->GetStyle()->Name.Get() : NULL,
			t->Length());
	}
}
		
LNamedStyle *LRichTextPriv::TextBlock::GetStyle(ssize_t At)
{
	if (At >= 0)
	{
		LArray<StyleText*> t;
		if (GetTextAt(At, t))
			return t[0]->GetStyle();
	}

	return Style;
}
		
void LRichTextPriv::TextBlock::SetStyle(LNamedStyle *s)
{
	if ((Style = s))
	{
		Fnt = d->GetFont(s);
		LayoutDirty = true;
		LAssert(Fnt != NULL);

		Margin.x1 = Style->MarginLeft().ToPx(Pos.X(), Fnt);
		Margin.y1 = Style->MarginTop().ToPx(Pos.Y(), Fnt);
		Margin.x2 = Style->MarginRight().ToPx(Pos.X(), Fnt);
		Margin.y2 = Style->MarginBottom().ToPx(Pos.Y(), Fnt);

		Border.x1 = Style->BorderLeft().ToPx(Pos.X(), Fnt);
		Border.y1 = Style->BorderTop().ToPx(Pos.Y(), Fnt);
		Border.x2 = Style->BorderRight().ToPx(Pos.X(), Fnt);
		Border.y2 = Style->BorderBottom().ToPx(Pos.Y(), Fnt);

		Padding.x1 = Style->PaddingLeft().ToPx(Pos.X(), Fnt);
		Padding.y1 = Style->PaddingTop().ToPx(Pos.Y(), Fnt);
		Padding.x2 = Style->PaddingRight().ToPx(Pos.X(), Fnt);
		Padding.y2 = Style->PaddingBottom().ToPx(Pos.Y(), Fnt);
	}
}

ssize_t LRichTextPriv::TextBlock::Length()
{
	return Len;
}

HtmlTag IsDefaultStyle(HtmlTag Id, LCss *Css)
{
	if (!Css)
		return CONTENT;

	if (Css->Length() == 2)
	{
		LCss::ColorDef c = Css->Color();
		if ((LColour)c != LColour::Blue)
			return CONTENT;
				
		LCss::TextDecorType td = Css->TextDecoration();
		if (td != LCss::TextDecorUnderline)
			return CONTENT;

		return TAG_A;
	}
	else if (Css->Length() == 1)
	{
		LCss::FontWeightType fw = Css->FontWeight();
		if (fw == LCss::FontWeightBold ||
			fw == LCss::FontWeightBolder ||
			fw >= LCss::FontWeight700)
			return TAG_B;

		LCss::TextDecorType td = Css->TextDecoration();
		if (td == LCss::TextDecorUnderline)
			return TAG_U;

		LCss::FontStyleType fs = Css->FontStyle();
		if (fs == LCss::FontStyleItalic)
			return TAG_I;
	}

	return CONTENT;
}

bool LRichTextPriv::TextBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
{
	s.Print("<p>");

	LRange All(0, Length());
	if (!Rng)
		Rng = &All;

	size_t Pos = 0;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		LNamedStyle *style = t->GetStyle();
		ssize_t tlen = t->Length();
		if (!tlen)
			continue;
		LRange TxtRange(Pos, tlen);
		LRange Common = TxtRange.Overlap(*Rng);
		if (Common.Valid())
		{		
			LString utf(
				#ifndef WINDOWS
				(char16*)
				#endif
				t->At(Common.Start - Pos), Common.Len);
			char *str = utf;

			const char *ElemName = NULL;
			if (t->Element != CONTENT)
			{
				LHtmlElemInfo *e = d->Inst.Static->GetTagInfo(t->Element);
				if (!e)
					return false;
				ElemName = e->Tag;
				if (style)
				{
					HtmlTag tag = IsDefaultStyle(t->Element, style);
					if (tag == t->Element)
						style = NULL;
				}
			}
			else
			{
				HtmlTag tag = IsDefaultStyle(t->Element, style);
				if (tag != CONTENT)
				{
					LHtmlElemInfo *e = d->Inst.Static->GetTagInfo(tag);
					if (e)
					{
						ElemName = e->Tag;
						style = NULL;
					}
				}			
			}

			if (style && !ElemName)
				ElemName = "span";
			if (ElemName)
				s.Print("<%s", ElemName);
			if (style)
				s.Print(" class='%s'", style->Name.Get());
			if (t->Element == TAG_A && t->Param)
				s.Print(" href='%s'", t->Param.Get());
			if (ElemName)
				s.Print(">");
		
			// Encode entities...
			LUtf8Ptr last(str);
			LUtf8Ptr cur(str);
			LUtf8Ptr end(str + utf.Length());
			while (cur < end)
			{
				int32 ch = cur;
				switch (ch)
				{
					case '<':
						s.Print("%.*s&lt;", cur - last, last.GetPtr());
						last = ++cur;
						break;
					case '>':
						s.Print("%.*s&gt;", cur - last, last.GetPtr());
						last = ++cur;
						break;
					case '\n':
						s.Print("%.*s<br>\n", cur - last, last.GetPtr());
						last = ++cur;
						break;
					case '&':
						s.Print("%.*s&amp;", cur - last, last.GetPtr());
						last = ++cur;
						break;
					case 0xa0:
						s.Print("%.*s&nbsp;", cur - last, last.GetPtr());
						last = ++cur;
						break;
					default:
						cur++;
						break;
				}
			}
			s.Print("%.*s", cur - last, last.GetPtr());

			if (ElemName)
				s.Print("</%s>", ElemName);
		}

		Pos += tlen;
	}
	s.Print("</p>\n");
	return true;
}		

bool LRichTextPriv::TextBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor)
		return d->Error(_FL, "No cursor param.");

	if (LayoutDirty)
	{
		Cursor->Pos.ZOff(-1, -1); // This is valid behaviour... need to 
								// wait for layout before getting cursor
								// position.
		return false;
	}
		
	ssize_t CharPos = 0;
	int LastY = 0;
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		PtrCheckBreak(tl);

		LRect r = tl->PosOff;
		r.Offset(Pos.x1, Pos.y1);
				
		int FixX = 0;
		for (unsigned n=0; n<tl->Strs.Length(); n++)
		{
			DisplayStr *ds = tl->Strs[n];
			ssize_t dsChars = ds->Chars;
					
			if
			(
				Cursor->Offset >= CharPos
				&&
				Cursor->Offset <= CharPos + dsChars
				&&
				(
					Cursor->LineHint < 0
					||
					Cursor->LineHint == i
				)
			)
			{
				ssize_t CharOffset = Cursor->Offset - CharPos;
				if (CharOffset == 0)
				{
					// First char
					Cursor->Pos.x1 = r.x1 + FixedToInt(FixX);
				}
				else if (CharOffset == dsChars)
				{
					// Last char
					Cursor->Pos.x1 = r.x1 + FixedToInt(FixX + ds->FX());
				}
				else
				{
					// In the middle somewhere...
					LAutoPtr<DisplayStr> Tmp = ds->Clone(0, CharOffset);
					// LDisplayString Tmp(ds->GetFont(), *ds, CharOffset);
					if (Tmp)
						Cursor->Pos.x1 = r.x1 + FixedToInt(FixX + Tmp->FX());
				}

				Cursor->Pos.y1 = r.y1 + ds->OffsetY;
				Cursor->Pos.y2 = Cursor->Pos.y1 + ds->Y() - 1;
				Cursor->Pos.x2 = Cursor->Pos.x1 + 1;

				Cursor->Line.Set(Pos.x1, r.y1, Pos.x2, r.y2);
				return true;
			}					
					
			FixX += ds->FX();
			CharPos += ds->Chars;
		}
				
		if
		(
			(
				tl->Strs.Length() == 0
				||
				i == Layout.Length() - 1
			)
			&&
			Cursor->Offset == CharPos
		)
		{
			// Cursor at the start of empty line.
			Cursor->Pos.x1 = r.x1;
			Cursor->Pos.x2 = Cursor->Pos.x1 + 1;
			Cursor->Pos.y1 = r.y1;
			Cursor->Pos.y2 = r.y2;

			Cursor->Line.Set(Pos.x1, r.y1, Pos.x2, r.y2);
			return true;
		}
				
		CharPos += tl->NewLine;
		LastY = tl->PosOff.y2;
	}
	
	if (Cursor->Offset == 0 &&
		Len == 0)
	{
		Cursor->Pos.x1 = Pos.x1;
		Cursor->Pos.x2 = Pos.x1 + 1;
		Cursor->Pos.y1 = Pos.y1;
		Cursor->Pos.y2 = Pos.y2;

		Cursor->Line = Pos;
		return true;
	}
			
	return false;
}
		
bool LRichTextPriv::TextBlock::HitTest(HitTestResult &htr)
{
	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	ssize_t CharPos = 0;
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		PtrCheckBreak(tl);
		// int InitCharPos = CharPos;

		LRect r = tl->PosOff;
		r.Offset(Pos.x1, Pos.y1);
		bool Over = r.Overlap(htr.In.x, htr.In.y);
		bool OnThisLine =	htr.In.y >= r.y1 &&
							htr.In.y <= r.y2;
		if (OnThisLine && htr.In.x <= r.x1)
		{
			htr.Near = true;
			htr.Idx = CharPos;
			htr.LineHint = i;
			
			LAssert(htr.Idx <= Length());
			
			return true;
		}
		
		int FixX = 0;
		int InputX = IntToFixed(htr.In.x - Pos.x1 - tl->PosOff.x1);
		for (unsigned n=0; n<tl->Strs.Length(); n++)
		{
			DisplayStr *ds = tl->Strs[n];
			int dsFixX = ds->FX();
			
			if (Over &&
				InputX >= FixX &&
				InputX < FixX + dsFixX)
			{
				int OffFix = InputX - FixX;
				int OffPx = FixedToInt(OffFix);
				ssize_t OffChar = ds->PosToIndex(OffPx, true);

				// d->DebugRects[0].Set(Pos.x1, r.y1, Pos.x1 + InputX+1, r.y2);
						
				htr.Blk = this;
				htr.Ds = ds;
				htr.Idx = CharPos + OffChar;
				htr.LineHint = i;

				LAssert(htr.Idx <= Length());

				return true;
			}
					
			FixX += ds->FX();

			CharPos += ds->Chars;
		}

		if (OnThisLine)
		{
			htr.Near = true;
			htr.Idx = CharPos;
			htr.LineHint = i;
			
			LAssert(htr.Idx <= Length());

			return true;
		}
				
		CharPos += tl->NewLine;
	}

	return false;
}

void DrawDecor(LSurface *pDC, LRichTextPriv::DisplayStr *Ds, int Fx, int Fy, ssize_t Start, ssize_t Len)
{
	// LColour Old = pDC->Colour(LColour::Red);
	LDisplayString ds1(Ds->GetFont(), (const char16*)(*Ds), Start);
	LDisplayString ds2(Ds->GetFont(), (const char16*)(*Ds), Start+Len);

	int x = (Fx >> LDisplayString::FShift);
	int y = (Fy >> LDisplayString::FShift) + (int)Ds->GetAscent() + 1;
	int End = x + ds2.X();
	x += ds1.X();
	pDC->Colour(LColour::Red);
	while (x < End)
	{
		pDC->Set(x, y+(x%2));
		x++;
	}
}

bool Overlap(LSpellCheck::SpellingError *e, ssize_t start, ssize_t len)
{
	if (!e)
		return false;
	if (start+len <= e->Start)
		return false;
	if (start >= e->End())
		return false;
	return true;
}

void LRichTextPriv::TextBlock::DrawDisplayString(LSurface *pDC, DisplayStr *Ds, int &FixX, int FixY, LColour &Bk, ssize_t &Pos)
{
	int OldX = FixX;

	// Paint the string itself...
	Ds->Paint(pDC, FixX, FixY, Bk);

	// Does the a spelling error overlap this string?
	ssize_t DsEnd = Pos + Ds->Chars;
	while (Overlap(SpErr, Pos, Ds->Chars))
	{
		// Yes, work out the region of characters and paint the decor
		ssize_t Start = MAX(SpErr->Start, Pos);
		ssize_t Len = MIN(SpErr->End(), Pos + Ds->Chars) - Start;
		
		// Draw the decor for the error
		DrawDecor(pDC, Ds, OldX, FixY, Start - Pos, Len);
		
		if (SpErr->End() < DsEnd)
		{
			// Are there more errors?
			SpErr = SpellingErrors.AddressOf(++PaintErrIdx);
		}
		else break;
	}

	while (SpErr && SpErr->End() < DsEnd)
	{
		// Are there more errors?
		SpErr = SpellingErrors.AddressOf(++PaintErrIdx);
	}


	Pos += Ds->Chars;
}


void LRichTextPriv::TextBlock::OnPaint(PaintContext &Ctx)
{
	ssize_t CharPos = 0;
	ssize_t EndPoints = 0;
	ssize_t EndPoint[2] = {-1, -1};
	ssize_t CurEndPoint = 0;

	if (Cursors > 0 && Ctx.Select)
	{
		// Selection end point checks...
		if (Ctx.Cursor && Ctx.Cursor->Blk == this)
			EndPoint[EndPoints++] = Ctx.Cursor->Offset;
		if (Ctx.Select && Ctx.Select->Blk == this)
			EndPoint[EndPoints++] = Ctx.Select->Offset;
				
		// Sort the end points
		if (EndPoints > 1 &&
			EndPoint[0] > EndPoint[1])
		{
			ssize_t ep = EndPoint[0];
			EndPoint[0] = EndPoint[1];
			EndPoint[1] = ep;
		}
	}
	
	// Paint margins, borders and padding...
	LRect r = Pos;
	r.x1 -= Margin.x1;
	r.y1 -= Margin.y1;
	r.x2 -= Margin.x2;
	r.y2 -= Margin.y2;
	LCss::ColorDef BorderStyle;
	if (Style)
		BorderStyle = Style->BorderLeft().Color;
	LColour BorderCol(222, 222, 222);
	if (BorderStyle.Type == LCss::ColorRgb)
		BorderCol.Set(BorderStyle.Rgb32, 32);

	Ctx.DrawBox(r, Margin, Ctx.Colours[Unselected].Back);
	Ctx.DrawBox(r, Border, BorderCol);
	Ctx.DrawBox(r, Padding, Ctx.Colours[Unselected].Back);
	
	int CurY = Pos.y1;
	PaintErrIdx = 0;
	SpErr = SpellingErrors.AddressOf(PaintErrIdx);

	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *Line = Layout[i];

		LRect LinePos = Line->PosOff;
		LinePos.Offset(Pos.x1, Pos.y1);
		if (Line->PosOff.X() < Pos.X())
		{
			Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
			Ctx.pDC->Rectangle(LinePos.x2, LinePos.y1, Pos.x2, LinePos.y2);
		}

		int FixX = IntToFixed(LinePos.x1);
		
		if (CurY < LinePos.y1)
		{
			// Fill padded area...
			Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
			Ctx.pDC->Rectangle(Pos.x1, CurY, Pos.x2, LinePos.y1 - 1);
		}

		CurY = LinePos.y1;
		LFont *Fnt = NULL;

		#if DEBUG_NUMBERED_LAYOUTS
		LString s;
		s.Printf("%i", Ctx.Index);
		Ctx.Index++;
		#endif

		for (unsigned n=0; n<Line->Strs.Length(); n++)
		{
			DisplayStr *Ds = Line->Strs[n];
			LFont *DsFnt = Ds->GetFont();
			ColourPair &Cols = Ds->Src->Colours;
			if (DsFnt && DsFnt != Fnt)
			{
				Fnt = DsFnt;
				Fnt->Transparent(false);
			}

			// If the current text part doesn't cover the full line height we have to
			// fill in the rest here...
			if (Ds->Y() < Line->PosOff.Y())
			{
				Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
				int CurX = FixedToInt(FixX);
				if (Ds->OffsetY > 0)
					Ctx.pDC->Rectangle(CurX, CurY, CurX+Ds->X(), CurY+Ds->OffsetY-1);

				int DsY2 = Ds->OffsetY + Ds->Y();
				if (DsY2 < Pos.Y())
					Ctx.pDC->Rectangle(CurX, CurY+DsY2, CurX+Ds->X(), Pos.y2);
			}

			// Check for selection changes...
			int FixY = IntToFixed(CurY + Ds->OffsetY);

			#if DEBUG_OUTLINE_CUR_STYLE_TEXT
			LRect r(0, 0, -1, -1);
			if (Ctx.Cursor->Blk == (Block*)this)
			{
				LArray<StyleText*> CurStyle;
				if (GetTextAt(Ctx.Cursor->Offset, CurStyle) && Ds->Src == CurStyle.First())
				{
					r.ZOff(Ds->X()-1, Ds->Y()-1);
					r.Offset(FixedToInt(FixX), FixedToInt(FixY));
				}
			}
			#endif

			if (CurEndPoint < EndPoints &&
				EndPoint[CurEndPoint] >= CharPos &&
				EndPoint[CurEndPoint] <= CharPos + Ds->Chars)
			{
				// Process string into parts based on the selection boundaries
				ssize_t Ch = EndPoint[CurEndPoint] - CharPos;
				ssize_t TmpPos = CharPos;
				LAutoPtr<DisplayStr> ds1 = Ds->Clone(0, Ch);
						
				// First part...
				LColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
				if (DsFnt)
					DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
				if (ds1)
					DrawDisplayString(Ctx.pDC, ds1, FixX, FixY, Bk, TmpPos);


				Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
				CurEndPoint++;
						
				// Is there 3 parts?
				//
				// This happens when the selection starts and end in the one string.
				//
				// The alternative is that it starts or ends in the strings but the other
				// end point is in a different string. In which case there is only 2 strings
				// to draw.
				if (CurEndPoint < EndPoints &&
					EndPoint[CurEndPoint] >= CharPos &&
					EndPoint[CurEndPoint] <= CharPos + Ds->Chars)
				{
					// Yes..
					ssize_t Ch2 = EndPoint[CurEndPoint] - CharPos;

					// Part 2
					LAutoPtr<DisplayStr> ds2 = Ds->Clone(Ch, Ch2 - Ch);
					LColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
					if (DsFnt)
						DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
					if (ds2)
						DrawDisplayString(Ctx.pDC, ds2, FixX, FixY, Bk, TmpPos);
					Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
					CurEndPoint++;

					// Part 3
					if (Ch2 < Ds->Length())
					{
						LAutoPtr<DisplayStr> ds3 = Ds->Clone(Ch2);
						Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
						if (DsFnt)
							DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
						if (ds3)
							DrawDisplayString(Ctx.pDC, ds3, FixX, FixY, Bk, TmpPos);
					}
				}
				else if (Ch < Ds->Chars)
				{
					// No... draw 2nd part
					LAutoPtr<DisplayStr> ds2 = Ds->Clone(Ch);
					LColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
					if (DsFnt)
						DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
					if (ds2)	
						DrawDisplayString(Ctx.pDC, ds2, FixX, FixY, Bk, TmpPos);
				}
			}
			else
			{
				// No selection changes... draw the whole string
				LColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
				if (DsFnt)
					DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
						
				#if DEBUG_OUTLINE_CUR_DISPLAY_STR
				int OldFixX = FixX;
				#endif

				auto TmpPos = CharPos;
				DrawDisplayString(Ctx.pDC, Ds, FixX, FixY, Bk, TmpPos);

				#if DEBUG_OUTLINE_CUR_DISPLAY_STR
				if (Ctx.Cursor->Blk == (Block*)this &&
					Ctx.Cursor->Offset >= CharPos &&
					Ctx.Cursor->Offset < CharPos + Ds->Chars)
				{
					LRect r(0, 0, Ds->X()-1, Ds->Y()-1);
					r.Offset(FixedToInt(OldFixX), FixedToInt(FixY));
					Ctx.pDC->Colour(LColour::Red);
					Ctx.pDC->Box(&r);
				}
				#endif
			}

			#if DEBUG_OUTLINE_CUR_STYLE_TEXT
			if (r.Valid())
			{
				Ctx.pDC->Colour(LColour(192, 192, 192));
				Ctx.pDC->LineStyle(LSurface::LineDot);
				Ctx.pDC->Box(&r);
				Ctx.pDC->LineStyle(LSurface::LineSolid);
			}
			#endif

			CharPos += Ds->Chars;
		}
		if (Line->Strs.Length() == 0)
		{
			if (CurEndPoint < EndPoints &&
				EndPoint[CurEndPoint] == CharPos)
			{
				Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
				CurEndPoint++;
			}
		}
		if (Ctx.Type == Selected)
		{
			// Draw new line
			int x1 = FixedToInt(FixX);
			FixX += IntToFixed(5);
			int x2 = FixedToInt(FixX);
			Ctx.pDC->Colour(Ctx.Colours[Selected].Back);
			Ctx.pDC->Rectangle(x1, LinePos.y1, x2, LinePos.y2);
		}
		Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
		Ctx.pDC->Rectangle(FixedToInt(FixX), LinePos.y1, Pos.x2, LinePos.y2);
		

		#if DEBUG_NUMBERED_LAYOUTS
		LDisplayString Ds(LSysFont, s);
		LSysFont->Colour(LColour::Green, LColour::White);
		LSysFont->Transparent(false);
		Ds.Draw(Ctx.pDC, LinePos.x1, LinePos.y1);
		/*
		Ctx.pDC->Colour(LColour::Blue);
		Ctx.pDC->Line(LinePos.x1, LinePos.y1,LinePos.x2,LinePos.y2);
		*/
		#endif
				
		CurY = LinePos.y2 + 1;
		CharPos += Line->NewLine;
	}

	if (CurY < Pos.y2)
	{
		// Fill padded area...
		Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
		Ctx.pDC->Rectangle(Pos.x1, CurY, Pos.x2, Pos.y2);
	}

	if (Ctx.Cursor &&
		Ctx.Cursor->Blk == this &&
		Ctx.Cursor->Blink &&
		d->View->Focus())
	{
		Ctx.pDC->Colour(CursorColour);
		
		if (!Ctx.Cursor->Pos.Valid())
		{
			// Try and recover a valid position for the cursor...
			if (!GetPosFromIndex(Ctx.Cursor))
			{
				Ctx.Cursor->LineHint = -1;
				GetPosFromIndex(Ctx.Cursor);
			}
		}

		if (Ctx.Cursor->Pos.Valid())
			Ctx.pDC->Rectangle(&Ctx.Cursor->Pos);
		else
			Ctx.pDC->Rectangle(Pos.x1, Pos.y1, Pos.x1, Pos.y2);
	}
	#if 0 // def _DEBUG
	if (Ctx.Select &&
		Ctx.Select->Blk == this)
	{
		Ctx.pDC->Colour(LColour(255, 0, 0));
		Ctx.pDC->Rectangle(&Ctx.Select->Pos);
	}
	#endif
}
		
bool LRichTextPriv::TextBlock::OnLayout(Flow &flow)
{
	if (Pos.X() == flow.X() && !LayoutDirty)
	{
		// Adjust position to match the flow, even if we are not dirty
		Pos.Offset(0, flow.CurY - Pos.y1);
		flow.CurY = Pos.y2 + 1;
		return true;
	}

	static int Count = 0;
	Count++;

	LayoutDirty = false;
	Layout.DeleteObjects();
			
	flow.Left += Margin.x1;
	flow.Right -= Margin.x2;
	flow.CurY += Margin.y1;
			
	Pos.x1 = flow.Left;
	Pos.y1 = flow.CurY;
	Pos.x2 = flow.Right;
	Pos.y2 = flow.CurY-1; // Start with a 0px height.
			
	flow.Left += Border.x1 + Padding.x1;
	flow.Right -= Border.x2 + Padding.x2;
	flow.CurY += Border.y1 + Padding.y1;

	int FixX = 0; // Current x offset (fixed point) on the current line
	LAutoPtr<TextLine> CurLine(new TextLine(flow.Left - Pos.x1, flow.X(), flow.CurY - Pos.y1));
	if (!CurLine)
		return flow.d->Error(_FL, "alloc failed.");

	ssize_t LayoutSize = 0;
	ssize_t TextSize = 0;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		LNamedStyle *tstyle = t->GetStyle();
		LAssert(t->Length() >= 0);			
		TextSize += t->Length();
		
		if (t->Length() == 0)
			continue;

		int AvailableX = Pos.X() - CurLine->PosOff.x1;
		if (AvailableX < 0)
			AvailableX = 1;

		// Get the font for 't'
		LFont *f = flow.d->GetFont(t->GetStyle());
		if (!f)
			return flow.d->Error(_FL, "font creation failed.");
		LCss::WordWrapType WrapType = tstyle ? tstyle->WordWrap() : LCss::WrapNormal;
		
		uint32_t *sStart = t->At(0);
		uint32_t *sEnd = sStart + t->Length();
		for (ssize_t Off = 0; Off < (ssize_t)t->Length(); )
		{
			// How much of 't' is on the same line?
			uint32_t *s = sStart + Off;

			#if DEBUG_LAYOUT
			LgiTrace("Txt[%i][%i]: FixX=%i, Txt='%.*S'\n", i, Off, FixX, t->Length() - Off, s);
			#endif

			if (*s == '\n')
			{
				// New line handling...
				Off++;
				CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX) - 1;
				FixX = 0;
				CurLine->LayoutOffsets(f->GetHeight());
				Pos.y2 = MAX(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
				CurLine->NewLine = 1;
				
				LayoutSize += CurLine->Length();
				
				#if DEBUG_LAYOUT
				LgiTrace("\tNewLineChar, LayoutSize=%i, TextSize=%i\n", LayoutSize, TextSize);
				#endif
				
				Layout.Add(CurLine.Release());
				CurLine.Reset(new TextLine(flow.Left - Pos.x1, flow.X(), Pos.Y()));

				if (Off == t->Length())
				{
					// Empty line at the end of the StyleText
					const uint32_t Empty[] = {0};
					CurLine->Strs.Add(new DisplayStr(t, f, Empty, 0, flow.pDC));
				}
				continue;
			}

			uint32_t *e = s;
			/*
			printf("e=%i sEnd=%i len=%i\n",
				(int)(e - sStart),
				(int)(sEnd - sStart),
				(int)t->Length());
			*/
			while (e < sEnd && *e != '\n')
				e++;
					
			// Add 't' to current line
			LCss::Len fntSize;
			if (f)
				fntSize = f->Size();
			
			ssize_t Chars = MIN(1024, e - s);
			LAutoPtr<DisplayStr> Ds
			(
				t->Emoji
				?
				new EmojiDisplayStr(t,
									d->GetEmojiImage(),
									fntSize,
									s,
									Chars)
				:
				new DisplayStr(t, f, s, Chars, flow.pDC)
			);
			if (!Ds)
				return flow.d->Error(_FL, "display str creation failed.");

			if (WrapType != LCss::WrapNone &&
				FixX + Ds->FX() > IntToFixed(AvailableX))
			{
				#if DEBUG_LAYOUT
				LgiTrace("\tNeedToWrap: %i, %i + %i > %i\n", WrapType, FixX, Ds->FX(), IntToFixed(AvailableX));
				#endif

				// Wrap the string onto the line...
				int AvailablePx = AvailableX - FixedToInt(FixX);
				ssize_t FitChars = Ds->PosToIndex(AvailablePx, false);
				if (FitChars < 0)
				{
					#if DEBUG_LAYOUT
					LgiTrace("\tFitChars error: %i\n", FitChars);
					#endif
					flow.d->Error(_FL, "PosToIndex(%i) failed.", AvailablePx);
					LAssert(0);
				}
				else
				{
					// Wind back to the last break opportunity
					ssize_t ch = 0;
					for (ch = FitChars; ch > 0; ch--)
					{
						if (IsWordBreakChar(s[ch-1]))
							break;
					}
					#if DEBUG_LAYOUT
					LgiTrace("\tWindBack: %i\n", (int)ch);
					#endif
					if (ch == 0)
					{
						// One word minimum per line
						for (ch = 1; ch < Chars; ch++)
						{
							if (IsWordBreakChar(s[ch]))
								break;
						}
						Chars = ch;
					}						
					else if (ch > (FitChars >> 2))
						Chars = ch;
					else
						Chars = FitChars;
							
					// Create a new display string of the right size...
					LCss::Len fntSize;
					if (f)
						fntSize = f->Size();
					if
					(
						!
						Ds.Reset
						(
							t->Emoji
							?
							new EmojiDisplayStr(t, d->GetEmojiImage(), fntSize, s, Chars)
							:
							new DisplayStr(t, f, s, Chars, flow.pDC)
						)
					)
						return flow.d->Error(_FL, "failed to create wrapped display str.");
					
					// Finish off line
					CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX + Ds->FX()) - 1;
					CurLine->Strs.Add(Ds.Release());

					CurLine->LayoutOffsets(d->Font->GetHeight());
					Pos.y2 = MAX(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
					LayoutSize += CurLine->Length();
					Layout.Add(CurLine.Release());

					#if DEBUG_LAYOUT
					LgiTrace("\tWrap, LayoutSize=%i TextSize=%i\n", LayoutSize, TextSize);
					#endif
					
					// New line...
					CurLine.Reset(new TextLine(flow.Left - Pos.x1, flow.X(), Pos.Y()));
					FixX = 0;
					Off += Chars;
					continue;
				}
			}
			else
			{
				FixX += Ds->FX();
			}
					
			if (!Ds)
				break;
					
			CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX) - 1;
			CurLine->Strs.Add(Ds.Release());
			Off += Chars;
		}
	}
	if (Txt.Length() == 0)
	{
		// Empty node case
		int y = Pos.y1 + flow.d->View->GetFont()->GetHeight() - 1;
		CurLine->PosOff.y2 = Pos.y2 = MAX(Pos.y2, y);
		LayoutSize += CurLine->Length();

		Layout.Add(CurLine.Release());
	}
			
	if (CurLine && CurLine->Strs.Length() > 0)
	{
		LFont *f = d->View ? d->View->GetFont() : LSysFont;
		CurLine->LayoutOffsets(f->GetHeight());
		Pos.y2 = MAX(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
		LayoutSize += CurLine->Length();

		#if DEBUG_LAYOUT
		LgiTrace("\tRemaining, LayoutSize=%i, TextSize=%i\n", LayoutSize, TextSize);
		#endif

		Layout.Add(CurLine.Release());
	}
	
	LAssert(LayoutSize == Len);
			
	flow.CurY = Pos.y2 + 1 + Margin.y2 + Border.y2 + Padding.y2;
	flow.Left -= Margin.x1 + Border.x1 + Padding.x1;
	flow.Right += Margin.x2 + Border.x2 + Padding.x2;
			
	return true;
}
		
ssize_t LRichTextPriv::TextBlock::GetTextAt(ssize_t Offset, LArray<StyleText*> &Out)
{
	if (Txt.Length() == 0)
		return 0;

	StyleText **t = &Txt[0];
	StyleText **e = t + Txt.Length();
	Out.Length(0);

	ssize_t Pos = 0;
	while (t < e)
	{
		ssize_t Len = (*t)->Length();
		if (Offset >= Pos && Offset <= Pos + Len)
			Out.Add(*t);

		t++;
		Pos += Len;
	}

	LAssert(Pos == Len);
	return Out.Length();
}

bool LRichTextPriv::TextBlock::IsValid()
{
	ssize_t TxtLen = 0;
	for (unsigned i = 0; i < Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		TxtLen += t->Length();
		for (unsigned n = 0; n < t->Length(); n++)
		{
			if ((*t)[n] == 0)
			{
				LAssert(0);
				return false;
			}
		}
	}
	
	if (Len != TxtLen)
		return d->Error(_FL, "Txt.Len vs Len mismatch: %i, %i.", TxtLen, Len);

	return true;
}

int LRichTextPriv::TextBlock::GetLines()
{
	return (int)Layout.Length();
}

bool LRichTextPriv::TextBlock::OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY)
{
	if (LayoutDirty)
		return false;

	if (LineY)
		LineY->Length(0);
	if (Offset <= 0)
	{
		if (ColX) *ColX = 0;
		if (LineY) LineY->Add(0);
		return true;
	}

	bool Found = false;
	ssize_t Pos = 0;

	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		auto Len = tl->Length();

		if (Offset >= Pos && Offset <= Pos + Len - tl->NewLine)
		{
			if (ColX) *ColX = (int)(Offset - Pos);
			if (LineY) LineY->Add(i);
			Found = true;
		}
		
		Pos += Len;
	}

	return Found;
}

ssize_t LRichTextPriv::TextBlock::LineToOffset(ssize_t Line)
{
	if (LayoutDirty)
		return -1;
	
	if (Line <= 0)
		return 0;

	ssize_t Pos = 0;
	for (size_t i=0; i<Layout.Length(); i++)
	{
		auto tl = Layout[i];
		auto Len = tl->Length();
		if (i == Line)
			return Pos;
		Pos += Len;
	}

	return (int)Length();
}


bool LRichTextPriv::TextBlock::PreEdit(Transaction *Trans)
{
	if (Trans)
	{
		bool HasThisBlock = false;
		for (unsigned i=0; i<Trans->Changes.Length(); i++)
		{
			CompleteTextBlockState *c = dynamic_cast<CompleteTextBlockState*>(Trans->Changes[i]);
			if (c)
			{
				if (c->Uid == BlockUid)
				{
					HasThisBlock = true;
					break;
				}
			}
		}

		if (!HasThisBlock)
			Trans->Add(new CompleteTextBlockState(d, this));
	}

	return true;
}

ssize_t LRichTextPriv::TextBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText)
{
	ssize_t Pos = 0;
	ssize_t Deleted = 0;

	PreEdit(Trans);

	for (unsigned i=0; i<Txt.Length() && Chars > 0; i++)
	{
		StyleText *t = Txt[i];
		ssize_t TxtOffset = BlkOffset - Pos;
		if (TxtOffset >= 0 && TxtOffset < (int)t->Length())
		{
			ssize_t MaxChars = t->Length() - TxtOffset;
			ssize_t Remove = MIN(Chars, MaxChars);
			if (Remove <= 0)
				return 0;
			ssize_t Remaining = MaxChars - Remove;
			ssize_t NewLen = t->Length() - Remove;
			
			if (DeletedText)
			{
				DeletedText->Add(t->At(TxtOffset), Remove);
			}
			if (Remaining > 0)
			{
				// Copy down
				memmove(&(*t)[TxtOffset], &(*t)[TxtOffset + Remove], Remaining * sizeof(uint32_t));
				(*t)[NewLen] = 0;
			}

			// Change length
			if (NewLen == 0)
			{
				// Remove run completely
				// LgiTrace("DelRun %p/%i '%.*S'\n", t, i, t->Length(), &(*t)[0]);
				Txt.DeleteAt(i--, true);
				DeleteObj(t);
			}
			else
			{
				// Shorten run
				t->Length(NewLen);
				// LgiTrace("ShortenRun %p/%i '%.*S'\n", t, i, t->Length(), &(*t)[0]);
			}

			LayoutDirty = true;
			Chars -= Remove;
			Len -= Remove;
			Deleted += Remove;
		}

		if (t)
			Pos += t->Length();
	}

	if (Deleted > 0)
	{
		// Adjust start of existing spelling styles
		LRange r(BlkOffset, Deleted);
		for (auto &Err : SpellingErrors)
		{
			if (Err.Overlap(r).Valid())
			{
				Err -= r;
			}
			else if (Err > r)
			{
				Err.Start -= Deleted;
			}
		}

		LayoutDirty = true;
		UpdateSpellingAndLinks(Trans, LRange(BlkOffset, 0));
	}

	IsValid();

	return Deleted;
}
		
bool LRichTextPriv::TextBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *InStr, ssize_t InChars, LNamedStyle *Style)
{
	if (!InStr)
		return d->Error(_FL, "No input text.");
	if (InChars < 0)
		InChars = Strlen(InStr);

	PreEdit(Trans);
	
	LArray<EmojiChar> EmojiIdx;
	for (int i=0; i<InChars;)
	{
		auto &e = EmojiIdx[i];
		e = EmojiToIconIndex(InStr + i, InChars - i);
		if (e.Index >= 0)
			i += e.Size;
		else
			i++;
	}

	ssize_t InitialOffset = AtOffset >= 0 ? AtOffset : Len;
	ssize_t Chars = 0; // Length of run to insert
	ssize_t Pos = 0; // Current character position in this block
	uint32_t TxtIdx = 0; // Index into Txt array
	
	for (ssize_t i = 0; i < InChars; i += Chars)
	{
		// Work out the run of chars that are either
		// Emoji or not Emoji...
		bool IsEmoji = EmojiIdx[i].Index >= 0;
		Chars = 0;
		for (auto n = i; n < InChars;)
		{
			if ( IsEmoji ^ (EmojiIdx[n].Index >= 0) )
				break;

			if (IsEmoji)
			{
				auto &e = EmojiIdx[n];
				if (e.Index >= 0 && e.Size > 0)
				{
					Chars += e.Size;
					n += e.Size;
				}
				else
				{
					LAssert(!"Something went wrong.");
					Chars++;
					n++;
				}
			}
			else
			{
				Chars++;
				n++;
			}
		}
		
		// Now process 'Char' chars
		const uint32_t *Str = InStr + i;

		if (AtOffset >= 0 && Txt.Length() > 0)
		{
			// Seek further into block?
			while (	Pos < AtOffset &&
					TxtIdx < Txt.Length())
			{
				StyleText *t = Txt[TxtIdx];
				ssize_t Len = t->Length();
				if (AtOffset <= Pos + Len)
					break;
				Pos += Len;
				TxtIdx++;
			}
			
			StyleText *t = TxtIdx >= Txt.Length() ? Txt.Last() : Txt[TxtIdx];
			ssize_t TxtLen = t->Length();
			if (AtOffset >= Pos && AtOffset <= Pos + TxtLen)
			{
				ssize_t StyleOffset = AtOffset - Pos;	// Offset into 't' in which we need to potentially break the style
														// to insert the new content.
				bool UrlEdge = t->Element == TAG_A && *Str == '\n';

				if (!Style && IsEmoji == t->Emoji && !UrlEdge)
				{
					// Insert/append to existing text run
					ssize_t After = t->Length() - StyleOffset;
					ssize_t NewSz = t->Length() + Chars;
					t->Length(NewSz);
					uint32_t *c = &t->First();

					LOG_FN("TextBlock(%i)::Add(%i,%i,%s)::Append StyleOffset=%i, After=%i\n", GetUid(), AtOffset, InChars, Style?Style->Name.Get():NULL, StyleOffset, After);

					// Do we need to move characters up to make space?
					if (After > 0)
						memmove(c + StyleOffset + Chars, c + StyleOffset, After * sizeof(*c));

					// Insert the new string...
					memcpy(c + StyleOffset, Str, Chars * sizeof(*c));
					Len += Chars;
					AtOffset += Chars;
				}
				else
				{
					// Break into 2 runs, with the new text in the middle...

					// Insert the new text+style
					StyleText *Run = new StyleText(Str, Chars, Style);
					if (!Run)
						return false;
					Run->Emoji = IsEmoji;
					
					
					/* This following code could be wrong. In terms of test cases I fixed this:
					
					A) Starting with basic empty email + signature.
						Insert a URL at the very start.
						Then hit enter.
						Buf: \n inserted BEFORE the URL.
						Changed the condition to 'StyleOffset != 0' rather than 'TxtIdx != 0'
					
					Potentially other test cases could exhibit bugs that need to be added here.
					*/
					if (StyleOffset)
						Txt.AddAt(++TxtIdx, Run);
					else
						Txt.AddAt(TxtIdx++, Run);
					////////////////////////////////////
					
					Pos += StyleOffset; // We are skipping over the run at 'TxtIdx', update pos

					LOG_FN("TextBlock(%i)::Add(%i,%i,%s)::Insert StyleOffset=%i\n", GetUid(), AtOffset, InChars, Style?Style->Name.Get():NULL, StyleOffset);

					if (StyleOffset < TxtLen)
					{
						// Insert the 2nd part of the string
						Run = new StyleText(t->At(StyleOffset), TxtLen - StyleOffset, t->GetStyle());
						if (!Run) return false;
						Pos += Chars;
						Txt.AddAt(++TxtIdx, Run);

						// Now truncate the existing text..
						t->Length(StyleOffset);
					}
					
					Len += Chars;
					AtOffset += Chars;
				}

				Str = NULL;
			}
		}

		if (Str)
		{
			// At the end
			StyleText *Last = Txt.Length() > 0 ? Txt.Last() : NULL;
			if (Last &&
				Last->GetStyle() == Style &&
				IsEmoji == Last->Emoji)
			{
				if (Last->Add((uint32_t*)Str, Chars))
				{
					Len += Chars;
					if (AtOffset >= 0)
						AtOffset += Chars;
				}
			}
			else
			{			
				StyleText *Run = new StyleText(Str, Chars, Style);
				if (!Run)
					return false;
				Run->Emoji = IsEmoji;
				Txt.Add(Run);
				Len += Chars;
				if (AtOffset >= 0)
					AtOffset += Chars;
			}
		}
	}

	// Push existing spelling styles along
	for (auto &Err : SpellingErrors)
	{
		if (Err.Start >= InitialOffset)
			Err.Start += InChars;
	}

	// Update layout and styling
	LayoutDirty = true;	
	IsValid();
	UpdateSpellingAndLinks(Trans, LRange(InitialOffset, InChars));
	
	return true;
}

bool LRichTextPriv::TextBlock::OnDictionary(Transaction *Trans)
{
	UpdateSpellingAndLinks(Trans, LRange(0, Length()));
	return true;
}

#define IsUrlWordChar(t) \
	(((t) > ' ') && !strchr("./:", (t)))

template<typename Char>
bool _ScanWord(Char *&t, Char *e)
{
	if (!IsUrlWordChar(*t))
		return false;
	
	Char *s = t;
	while (t < e && IsUrlWordChar(*t))
		t++;
	
	return t > s;
}

bool IsBracketed(int s, int e)
{
	if (s == '(' && e == ')')
		return true;
	if (s == '[' && e == ']')
		return true;
	if (s == '{' && e == '}')
		return true;
	if (s == '<' && e == '>')
		return true;
	return false;
}

#define ScanWord() \
	if (t >= e || !_ScanWord(t, e)) return false
#define ScanChar(ch) \
	if (t >= e || *t != ch) \
		return false; \
	t++

template<typename Char>
bool DetectUrl(Char *t, ssize_t &len)
{
	#ifdef _DEBUG
	LString str(t, len);
	//char *ss = str;
	#endif
	
	Char *s = t;
	Char *e = t + len;
	ScanWord(); // Protocol
	ScanChar(':');
	ScanChar('/');
	ScanChar('/');
	ScanWord(); // Host name or username..
	if (t < e && *t == ':')
	{
		t++;
		_ScanWord(t, e); // Don't return if missing... password optional
		ScanChar('@');
		ScanWord(); // First part of host name...
	}

	// Rest of host name
	while (t < e && *t == '.')
	{
		t++;
		if (t < e && IsUrlWordChar(*t))
			ScanWord(); // Second part of host name
	}
	
	if (t < e && *t == ':') // Port number
	{
		t++;
		ScanWord();
	}
	
	while (t < e && strchr("/.:", *t)) // Path
	{
		t++;
		if (t < e && (IsUrlWordChar(*t) || *t == ':'))
			ScanWord();
	}
	
	if (strchr("!.", t[-1]))
		t--;
	
	len = t - s;
	return true;
}

int ErrSort(LSpellCheck::SpellingError *a, LSpellCheck::SpellingError *b)
{
	return (int) (a->Start - b->Start);
}

void LRichTextPriv::TextBlock::SetSpellingErrors(LArray<LSpellCheck::SpellingError> &Errors, LRange r)
{
	// LgiTrace("%s:%i - SetSpellingErrors " LPrintfSSizeT ", " LPrintfSSizeT ":" LPrintfSSizeT "\n", _FL, Errors.Length(), r.Start, r.End());

	// Delete any errors overlapping 'r'
	for (unsigned i=0; i<SpellingErrors.Length(); i++)
	{
		if (SpellingErrors[i].Overlap(r).Valid())
			SpellingErrors.DeleteAt(i--, true);
	}

	// Insert the new errors and sort into place..
	for (auto &e : Errors)
	{
		auto &n = SpellingErrors.New();
		n.Start = r.Start + e.Start;
		n.Len = e.Len;
		n.Suggestions = e.Suggestions;
	}
	SpellingErrors.Sort(ErrSort);
}

#define IsWordChar(ch) ( IsAlpha(ch) || (ch) == '\'' )

void LRichTextPriv::TextBlock::UpdateSpellingAndLinks(Transaction *Trans, LRange r)
{
	LArray<uint32_t> Text;
	if (!CopyAt(0, Length(), &Text))
		return;

	// Spelling...
	if (d->SpellCheck &&
		d->SpellDictionaryLoaded)
	{
		LRange Rgn = r;
		while (Rgn.Start > 0 &&
				IsWordChar(Text[Rgn.Start-1]))
		{
			Rgn.Start--;
			Rgn.Len++;
		}
		while (Rgn.End() < Len &&
				IsWordChar(Text[Rgn.End()]))
		{
			Rgn.Len++;
		}

		LString s(Text.AddressOf(Rgn.Start), Rgn.Len);
		LArray<LVariant> Params;
		Params[SpellBlockPtr] = (Block*)this;

		// LgiTrace("%s:%i - Check(%s) " LPrintfSSizeT ":" LPrintfSSizeT "\n", _FL, s.Get(), Rgn.Start, Rgn.End());

		d->SpellCheck->Check(d->View->AddDispatch(), s, Rgn.Start, Rgn.Len, &Params);
	}

	// Link detection...
	
	// Extend the range to include whole words
	while (r.Start > 0 && !IsWhite(Text[r.Start]))
	{
		r.Start--;
		r.Len++;
	}
	while (r.End() < (ssize_t)Text.Length() && !IsWhite(Text[r.End()]))
		r.Len++;

	// Create array of words...
	LArray<LRange> Words;
	bool Ws = true;
	for (int i = 0; i < r.Len; i++)
	{
		bool w = IsWhite(Text[r.Start + i]);
		if (w ^ Ws)
		{
			Ws = w;
			if (!w)
			{
				LRange &w = Words.New();
				w.Start = r.Start + i;
				// printf("StartWord=%i, %i\n", w.Start, w.Len);
			}
			else if (Words.Length() > 0)
			{
				LRange &w = Words.Last();
				w.Len = r.Start + i - w.Start;
				// printf("EndWord=%i, %i\n", w.Start, w.Len);
			}
		}
	}
	if (!Ws && Words.Length() > 0)
	{
		LRange &w = Words.Last();
		w.Len = r.Start + r.Len - w.Start;
		// printf("TermWord=%i, %i Words=%i\n", w.Start, w.Len, Words.Length());
	}
	
	// For each word in the range of text
	for (unsigned i = 0; i<Words.Length(); i++)
	{
		LRange w = Words[i];

		// Check there is a URL at the location
		bool IsUrl = DetectUrl(Text.AddressOf(w.Start), w.Len);
		
		#if 0
		{
			LString s(Text.AddressOf(w.Start), w.Len);
			printf("DetectUrl(%s)=%i\n", s.Get(), IsUrl);
		}
		#endif
		
		if (IsUrl)
		{
			// Are there chars bracketing the URL?
			if (IsBracketed(Text[w.Start], Text[w.End()-1]))
			{
				w.Start++;
				w.Len -= 2;
			}
			
			// Make it a link...
			LString Link(Text.AddressOf(w.Start), w.Len);
			d->MakeLink(this, w.Start, w.Len, Link);
			
			// Also unlink any of the word after the URL
			if (w.End() < Words[i].End())
			{
				LCss Style;
				ChangeStyle(Trans, w.End(), Words[i].End() - w.End(), &Style, false);
			}
		}
	}
}

bool LRichTextPriv::TextBlock::StripLast(Transaction *Trans, const char *Set)
{
	if (Txt.Length() == 0)
		return false;

	StyleText *l = Txt.Last();
	if (!l || l->Length() <= 0)
		return false;

	if (!strchr(Set, l->Last()))
		return false;
	
	PreEdit(Trans);
	if (!l->PopLast())
		return false;

	LayoutDirty = true;
	Len--;
	return true;
}

bool LRichTextPriv::TextBlock::DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool TopOfMenu)
{
	if (!TopOfMenu)
		return false;

	// Is there a spelling error at 'Offset'?		
	for (unsigned i=0; i<SpellingErrors.Length(); i++)
	{
		LSpellCheck::SpellingError &e = SpellingErrors[i];
		if (Offset >= e.Start && Offset < e.End())
		{
			ClickErrIdx = i;
			if (e.Suggestions.Length())
			{
				auto Sp = s.AppendSub("Spelling");
				if (Sp)
				{
					s.AppendSeparator();
					for (unsigned n=0; n<e.Suggestions.Length(); n++)
						Sp->AppendItem(e.Suggestions[n], SPELLING_BASE+n);
				}
				// else printf("%s:%i - No sub menu.\n", _FL);
			}
			// else printf("%s:%i - No Suggestion.\n", _FL);
			break;
		}
		// else printf("%s:%i - Outside area, Offset=%i e=%i,%i.\n", _FL, Offset, e.Start, e.End());
	}

	// Check for URLs under the cursor?
	ssize_t pos = 0;
	for (auto t: Txt)
	{
		// LgiTrace("t: %i %i\n", (int)pos, t->Element);
		if (t->Element == TAG_A)
		{
			// LgiTrace("off: %i %i %i\n", (int)Offset, (int)pos, (int)pos + (int)t->Length());
			if (Offset >= pos && Offset < pos + (ssize_t)t->Length())
			{
				// Is over a link...
				s.AppendItem("Open URL", IDM_OPEN_URL);
				ClickedUri = t->Param;
				break;
			}
		}

		pos += t->Length();
	}

	return true;
}

LMessage::Result LRichTextPriv::TextBlock::OnEvent(LMessage* Msg)
{
	switch (Msg->Msg())
	{
		case M_COMMAND:
		{
			if (Msg->A() == IDM_OPEN_URL)
			{
				if (ClickedUri)
					LExecute(ClickedUri);
			}
			else if (auto e = SpellingErrors.AddressOf(ClickErrIdx))
			{
				// Replacing text with spell check suggestion:
				int i = (int)Msg->A() - SPELLING_BASE;
				if (i >= 0 && i < (int)e->Suggestions.Length())
				{
					auto Start = e->Start;
					LString s = e->Suggestions[i];
					AutoTrans t(new LRichTextPriv::Transaction);

					// Delete the old text...
					DeleteAt(t, Start, e->Len); // 'e' might disappear here

					// Insert the new text....
					LAutoPtr<uint32_t, true> u((uint32_t*)LNewConvertCp("utf-32", s, "utf-8"));
					AddText(t, Start, u.Get(), Strlen(u.Get()));

					d->AddTrans(t);
					return true;
				}
			}
			break;
		}
	}

	return false;
}

bool LRichTextPriv::TextBlock::IsEmptyLine(BlockCursor *Cursor)
{
	if (!Cursor)
		return false;

	TextLine *Line = Layout.AddressOf(Cursor->LineHint) ? Layout[Cursor->LineHint] : NULL;
	if (!Line)
		return false;

	auto LineLen = Line->Length();
	return LineLen == 0;
}

LRichTextPriv::Block *LRichTextPriv::TextBlock::Clone()
{
	return new TextBlock(this);
}

ssize_t LRichTextPriv::TextBlock::CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text)
{
	if (!Text)
		return 0;
	if (Chars < 0)
		Chars = Length() - Offset;

	ssize_t Pos = 0;
	for (size_t i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		if (Offset >= Pos && Offset < Pos + (int)t->Length())
		{
			ssize_t Skip = Offset - Pos;
			ssize_t Remain = t->Length() - Skip;
			ssize_t Cp = MIN(Chars, Remain);
			Text->Add(&(*t)[Skip], Cp);
			Chars -= Cp;
			Offset += Cp;
		}

		Pos += t->Length();
	}

	return Text->Length();
}

ssize_t LRichTextPriv::TextBlock::FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params)
{
	if (!Str || !Params)
		return -1;

	size_t InLen = Strlen(Str);
	bool Match;
	ssize_t CharPos = 0;
	for (size_t i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		uint32_t *s = &t->First();
		uint32_t *e = s + t->Length();
		if (Params->MatchCase)
		{
			for (uint32_t *c = s; c < e; c++)
			{
				if (*c == *Str)
				{
					if (c + InLen <= e)
						Match = !Strncmp(c, Str, InLen);
					else
					{
						LArray<uint32_t> tmp;
						if (CopyAt(CharPos + (c - s), InLen, &tmp) &&
							tmp.Length() == InLen)
							Match = !Strncmp(&tmp[0], Str, InLen);
						else
							Match = false;
					}
					if (Match)
						return CharPos + (c - s);
				}
			}
		}
		else
		{
			uint32_t l = ToLower(*Str);
			for (uint32_t *c = s; c < e; c++)
			{
				if (ToLower(*c) == l)
				{
					if (c + InLen <= e)
						Match = !Strnicmp(c, Str, InLen);
					else
					{
						LArray<uint32_t> tmp;
						if (CopyAt(CharPos + (c - s), InLen, &tmp) &&
							tmp.Length() == InLen)
							Match = !Strnicmp(&tmp[0], Str, InLen);
						else
							Match = false;
					}
					if (Match)
						return CharPos + (c - s);
				}
			}
		}

		CharPos += t->Length();
	}

	return -1;
}

bool LRichTextPriv::TextBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
{
	LRange Blk(0, Len);
	LRange Inp(StartIdx, Chars < 0 ? Len - StartIdx : Chars);
	LRange Change = Blk.Overlap(Inp);

	PreEdit(Trans);

	LRange Run(0, 0);
	bool Changed = false;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *st = Txt[i];
		Run.Len = st->Length();
		LRange Edit = Run.Overlap(Change);
		if (Edit.Len > 0)
		{
			uint32_t *s = st->At(Edit.Start - Run.Start);
			for (int n=0; n<Edit.Len; n++)
			{
				if (Upper)
				{
					if (s[n] >= 'a' && s[n] <= 'z')
						s[n] = s[n] - 'a' + 'A';
				}
				else
				{
					if (s[n] >= 'A' && s[n] <= 'Z')
						s[n] = s[n] - 'A' + 'a';
				}
			}
			Changed = true;
		}

		Run.Start += Run.Len;
	}

	LayoutDirty |= Changed;

	return Changed;
}

LRichTextPriv::Block *LRichTextPriv::TextBlock::Split(Transaction *Trans, ssize_t AtOffset)
{
	if (AtOffset < 0 || 
		AtOffset >= Len)
		return NULL;

	LRichTextPriv::TextBlock *After = new LRichTextPriv::TextBlock(d);
	if (!After)
	{
		d->Error(_FL, "Alloc Err");
		return NULL;
	}

	After->SetStyle(GetStyle());
	
	ssize_t Pos = 0;
	unsigned i;
	for (i=0; i<Txt.Length(); i++)
	{
		StyleText *St = Txt[i];
		ssize_t StLen = St->Length();
		if (AtOffset >= Pos && AtOffset < Pos + StLen)
		{
			ssize_t StOff = AtOffset - Pos;
			if (StOff > 0)
			{
				// Split the text into 2 blocks...
				uint32_t *t = St->At(StOff);
				ssize_t remaining = St->Length() - StOff;
				StyleText *AfterText = new StyleText(t, remaining, St->GetStyle());
				if (!AfterText)
				{
					d->Error(_FL, "Alloc Err");
					return NULL;
				}
				St->Length(StOff);
				i++;
				Len = Pos + StOff;

				After->Txt.Add(AfterText);
				After->Len += AfterText->Length();
			}
			else
			{
				Len = Pos;
			}
			break;
		}

		Pos += StLen;
	}

	while (i < Txt.Length())
	{
		StyleText *St = Txt[i];
		Txt.DeleteAt(i, true);
		After->Txt.Add(St);
		After->Len += St->Length();
	}

	LayoutDirty = true;
	After->LayoutDirty = true;

	return After;
}

void LRichTextPriv::TextBlock::IncAllStyleRefs()
{
	if (Style)
		Style->RefCount++;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		LNamedStyle *s = Txt[i]->GetStyle();
		if (s) s->RefCount++;
	}
}

bool LRichTextPriv::TextBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add)
{
	if (!Style)
		return d->Error(_FL, "No style.");

	if (Offset < 0 || Offset >= Len)
		return true;
	if (Chars < 0)
		Chars = Len;

	if (Trans)
		Trans->Add(new CompleteTextBlockState(d, this));

	ssize_t CharPos = 0;
	ssize_t RestyleEnd = Offset + Chars;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		ssize_t Len = t->Length();
		ssize_t End = CharPos + Len;

		if (End <= Offset || CharPos > RestyleEnd)
			;
		else
		{
			ssize_t Before = Offset >= CharPos ? Offset - CharPos : 0;
			LAssert(Before >= 0);
			ssize_t After = RestyleEnd < End ? End - RestyleEnd : 0;
			LAssert(After >= 0);
			ssize_t Inside = Len - Before - After;
			LAssert(Inside >= 0);


			LAutoPtr<LCss> TmpStyle(new LCss);
			if (Add)
			{
				if (t->GetStyle())
					*TmpStyle = *t->GetStyle();
				*TmpStyle += *Style;
			}
			else if (Style->Length() != 0)
			{
				if (t->GetStyle())
					*TmpStyle = *t->GetStyle();
				*TmpStyle -= *Style;
			}

			LNamedStyle *CacheStyle = TmpStyle && TmpStyle->Length() ? d->AddStyleToCache(TmpStyle) : NULL;

			if (Before && After)
			{
				// Split into 3 parts:
				// |---before----|###restyled###|---after---|
				StyleText *st = new StyleText(t->At(Before), Inside, CacheStyle);
				if (st)
					Txt.AddAt(++i, st);
				
				st = new StyleText(t->At(Before + Inside), After, t->GetStyle());
				if (st)
					Txt.AddAt(++i, st);
				
				t->Length(Before);
				LayoutDirty = true;
				return true;
			}
			else if (Before)
			{
				// Split into 2 parts:
				// |---before----|###restyled###|
				StyleText *st = new StyleText(t->At(Before), Inside, CacheStyle);
				if (st)
					Txt.AddAt(++i, st);
				
				t->Length(Before);
				LayoutDirty = true;
			}
			else if (After)
			{
				// Split into 2 parts:
				// |###restyled###|---after---|
				StyleText *st = new StyleText(t->At(0), Inside, CacheStyle);
				if (st)
					Txt.AddAt(i, st);
				
				memmove(t->At(0), t->At(Inside), After*sizeof(uint32_t));
				t->Length(After);
				LayoutDirty = true;
			}
			else if (Inside)
			{
				// Re-style the whole run
				t->SetStyle(CacheStyle);
				LayoutDirty = true;
			}
		}

		CharPos += Len;
	}

	// Merge any regions of the same style into contiguous sections
	for (unsigned i=0; i<Txt.Length()-1; i++)
	{
		StyleText *a = Txt[i];
		StyleText *b = Txt[i+1];
		if (a->GetStyle() == b->GetStyle() &&
			a->Emoji == b->Emoji)
		{
			// Merge...
			a->Add(b->AddressOf(0), b->Length());
			Txt.DeleteAt(i + 1, true);
			delete b;
			i--;
		}		
	}

	return true;
}

bool LRichTextPriv::TextBlock::Seek(SeekType To, BlockCursor &Cur)
{
	int XOffset = Cur.Pos.x1 - Pos.x1;
	ssize_t CharPos = 0;
	LArray<ssize_t> LineOffset;
	LArray<ssize_t> LineLen;
	int CurLine = -1;
	int CurLineScore = 0;
	
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *Line = Layout[i];
		PtrCheckBreak(Line);
		auto Len = Line->Length();				
				
		LineOffset[i] = CharPos;
		LineLen[i] = Len;
				
		if (Cur.Offset >= CharPos &&
			Cur.Offset <= CharPos + Len - Line->NewLine)	// Minus 'NewLine' is because the cursor can't be
															// after the '\n' on a line. It's actually on the 
															// next line.
		{
			int Score = 1;
			if (Cur.LineHint >= 0 && i == Cur.LineHint)
				Score++;
			if (Score > CurLineScore)
			{
				CurLine = i;
				CurLineScore = Score;
			}
		}				
				
		CharPos += Len;
	}
			
	if (CurLine < 0)
	{
		CharPos = 0;
		d->Log->Print("TextBlock(%i)::Seek, lines=%i\n", GetUid(), Layout.Length());
		for (unsigned i=0; i<Layout.Length(); i++)
		{
			TextLine *Line = Layout[i];
			if (Line) { d->Log->Print("\tLine[%i] @ %i+%i=%i\n", i, CharPos, Line->Length(), CharPos + Line->Length()); CharPos += Line->Length(); }
			else      { d->Log->Print("\tLine[%i] @ %i, is NULL\n", i, CharPos); break; }
		}
		return d->Error(_FL, "Index '%i' not in layout lines.", Cur.Offset);
	}
				
	TextLine *Line = NULL;
	switch (To)
	{
		case SkLineStart:
		{
			Cur.Offset = LineOffset[CurLine];
			Cur.LineHint = CurLine;
			return true;
		}
		case SkLineEnd:
		{
			Cur.Offset = LineOffset[CurLine] +
						LineLen[CurLine] -
						Layout[CurLine]->NewLine;
			Cur.LineHint = CurLine;
			return true;
		}
		case SkUpLine:
		{
			// Get previous line...
			if (CurLine == 0)
				return false;
			Line = Layout[--CurLine];
			if (!Line)
				return d->Error(_FL, "No line at %i.", CurLine);
			break;
		}				
		case SkDownLine:
		{
			// Get next line...
			if (CurLine >= (int)Layout.Length() - 1)
				return false;
			Line = Layout[++CurLine];
			if (!Line)
				return d->Error(_FL, "No line at %i.", CurLine);
			break;
		}
		default:
		{
			return false;
			break;
		}
	}
			
	if (Line)
	{
		// Work out where the cursor should be based on the 'XOffset'
		if (Line->Strs.Length() > 0)
		{
			int FixX = 0;
			ssize_t CharOffset = 0;
			for (unsigned i=0; i<Line->Strs.Length(); i++)
			{
				DisplayStr *Ds = Line->Strs[i];
				PtrCheckBreak(Ds);
						
				if (XOffset >= FixedToInt(FixX) &&
					XOffset <= FixedToInt(FixX + Ds->FX()))
				{
					// This is the matching string...
					int Px = XOffset - FixedToInt(FixX) - Line->PosOff.x1;
					ssize_t Char = Ds->PosToIndex(Px, true);
					if (Char >= 0)
					{
						Cur.Offset = LineOffset[CurLine] +	// Character offset of line
									CharOffset +			// Character offset of current string
									Char;					// Offset into current string for 'XOffset'
						Cur.LineHint = CurLine;
						return true;
					}
				}
						
				FixX += Ds->FX();
				CharOffset += Ds->Length();
			}
					
			// Cursor is nearest the end of the string...?
			Cur.Offset = LineOffset[CurLine] + Line->Length() - Line->NewLine;
			Cur.LineHint = CurLine;
			return true;
		}
		else if (Line->NewLine)
		{
			Cur.Offset = LineOffset[CurLine];
			Cur.LineHint = CurLine;
			return true;
		}
	}
			
	return false;
}
	
#ifdef _DEBUG
void LRichTextPriv::TextBlock::DumpNodes(LTreeItem *Ti)
{
	LString s;
	s.Printf("TextBlock, style=%s, pos=%s, ptr=%p", Style?Style->Name.Get():NULL, Pos.GetStr(), this);
	Ti->SetText(s);

	LTreeItem *TxtRoot = PrintNode(Ti, "Txt(%i)", Txt.Length());
	if (TxtRoot)
	{
		ssize_t Pos = 0;
		for (size_t i=0; i<Txt.Length(); i++)
		{
			StyleText *St = Txt[i];
			ssize_t Len = St->Length();
			LString u;
			if (Len)
			{
				LStringPipe p(256);
				uint32_t *Str = St->At(0);
				p.Write("\'", 1);
				for (int k=0; k<MIN(Len, 30); k++)
				{
					if (Str[k] == '\n')
						p.Write("\\n", 2);
					else if (Str[k] >= 0x10000)
						p.Print("&#%i;", Str[k]);
					else
					{
						uint8_t utf8[6], *n = utf8;
						ssize_t utf8len = sizeof(utf8);
						if (LgiUtf32To8(Str[k], n, utf8len))
							p.Write(utf8, sizeof(utf8)-utf8len);
					}
				}
				p.Write("\'", 1);
				u = p.NewLStr();
			}
			else u = "(Empty)";

			PrintNode(	TxtRoot, "[%i] range=%i-%i, len=%i, style=%s, %s",
						i,
						Pos, Pos + Len - 1,
						Len,
						St->GetStyle() ? St->GetStyle()->Name.Get() : NULL,
						u.Get());
			Pos += Len;
		}
	}

	LTreeItem *LayoutRoot = PrintNode(Ti, "Layout(%i)", Layout.Length());
	if (LayoutRoot)
	{
		ssize_t Pos = 0;
		for (size_t i=0; i<Layout.Length(); i++)
		{
			TextLine *Tl = Layout[i];
			LTreeItem *Elem = PrintNode(LayoutRoot,
										"[%i] chars=%i-%i, len=%i + %i, pos=%s",
										i,
										Pos, Pos + Tl->Length() - 1,
										Tl->Length(),
										Tl->NewLine,
										Tl->PosOff.GetStr());
			for (unsigned n=0; n<Tl->Strs.Length(); n++)
			{
				DisplayStr *Ds = Tl->Strs[n];
				LNamedStyle *Style = Ds->Src ? Ds->Src->GetStyle() : NULL;
				PrintNode(	Elem, "[%i] style=%s len=%i txt='%.20S'",
							n,
							Style ? Style->Name.Get() : NULL,
							Ds->Length(),
							(const char16*) (*Ds));
			}
			
			Pos += Tl->Length() + Tl->NewLine;
		}
	}
}
#endif


