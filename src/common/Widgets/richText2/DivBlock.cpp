#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit2.h"

#include "RichTextEditPriv.h"

namespace Rte2 {

LRichTextPriv::DivBlock::DivBlock(LRichTextPriv *priv, HtmlTag tag, const char *element) :
	Block(priv),
	TagId(tag)
{
	if (element)
		ElementName = element;
}

LRichTextPriv::DivBlock::DivBlock(const DivBlock *Copy) :
	Block(Copy),
	LCssBox<LRect>(*Copy),
	Pos(Copy->Pos),
	Style(Copy->Style),
	Fnt(Copy->Fnt),
	TagId(Copy->TagId),
	ElementName(Copy->ElementName)
{
	for (unsigned i = 0; i < Copy->blocks.Length(); i++)
		Add(Copy->blocks.ItemAt(i)->Clone());
}

LRichTextPriv::DivBlock::~DivBlock()
{
	LAssert(Cursors == 0);
}

LRichTextPriv::TextBlock *LRichTextPriv::DivBlock::GetTextBlock()
{
	if (!blocks.Length())
	{
		Add(new LRichTextPriv::TextBlock(d));
		return dynamic_cast<LRichTextPriv::TextBlock*>(blocks.Last());
	}

	auto tb = dynamic_cast<LRichTextPriv::TextBlock*>(blocks.Last());
	if (tb)
		return tb;

	return dynamic_cast<LRichTextPriv::TextBlock*>(Add(new LRichTextPriv::TextBlock(d)) ? blocks.Last() : nullptr);
}

bool LRichTextPriv::DivBlock::IsValid()
{
	return blocks.Length() > 0;
}

LNamedStyle *LRichTextPriv::DivBlock::MakeChildStyle(LNamedStyle *ParentStyle)
{
	if (!ParentStyle)
		return nullptr;

	LAutoPtr<LCss> Child(new LCss(*ParentStyle));
	if (!Child)
		return nullptr;

	static const LCss::PropType BoxProps[] =
	{
		LCss::PropMargin,
		LCss::PropMarginTop,
		LCss::PropMarginRight,
		LCss::PropMarginBottom,
		LCss::PropMarginLeft,

		LCss::PropPadding,
		LCss::PropPaddingTop,
		LCss::PropPaddingRight,
		LCss::PropPaddingBottom,
		LCss::PropPaddingLeft,

		LCss::PropBorder,
		LCss::PropBorderTop,
		LCss::PropBorderRight,
		LCss::PropBorderBottom,
		LCss::PropBorderLeft,
		LCss::PropBorderStyle,
		LCss::PropBorderColor,

		LCss::PropBorderTopStyle,
		LCss::PropBorderRightStyle,
		LCss::PropBorderBottomStyle,
		LCss::PropBorderLeftStyle,

		LCss::PropBorderTopWidth,
		LCss::PropBorderRightWidth,
		LCss::PropBorderBottomWidth,
		LCss::PropBorderLeftWidth,

		LCss::PropBorderTopColor,
		LCss::PropBorderRightColor,
		LCss::PropBorderBottomColor,
		LCss::PropBorderLeftColor,

		LCss::PropBorderSpacing,
		LCss::PropBorderRadius
	};

	for (auto p : BoxProps)
		Child->DeleteProp(p);

	return d->AddStyleToCache(Child);
}

const char *LRichTextPriv::DivBlock::GetElementName()
{
	const char *name = ElementName.Get();
	if (name && *name)
		return name;

	auto Info = d->Inst.Static->GetTagInfo(TagId);
	if (Info && Info->Tag)
		return Info->Tag;

	return "div";
}

ssize_t LRichTextPriv::DivBlock::Length()
{
	ssize_t Len = 0;
	for (auto b: blocks)
		Len += b->Length();
	return Len;
}

int LRichTextPriv::DivBlock::GetLines()
{
	int Lines = 0;
	for (auto b : blocks)
		Lines += MAX(b->GetLines(), 1);
	return Lines;
}

bool LRichTextPriv::DivBlock::OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY)
{
	ssize_t Pos = 0;
	int StartLine = 0;

	for (auto b : blocks)
	{
		ssize_t Len = b->Length();
		int ChildLines = MAX(b->GetLines(), 1);

		if (Offset <= Pos + Len || b == blocks.Last())
		{
			LArray<int> ChildLine;
			if (!b->OffsetToLine(MAX(Offset - Pos, 0), ColX, &ChildLine) || !ChildLine.Length())
				return false;

			if (LineY)
			{
				for (auto Line : ChildLine)
					LineY->Add(StartLine + Line);
			}
			return true;
		}

		Pos += Len;
		StartLine += ChildLines;
	}

	return false;
}

ssize_t LRichTextPriv::DivBlock::LineToOffset(ssize_t Line)
{
	ssize_t Offset = 0;

	for (auto b : blocks)
	{
		int Lines = MAX(b->GetLines(), 1);
		if (Line < Lines)
		{
			ssize_t ChildOffset = b->LineToOffset(Line);
			return ChildOffset < 0 ? -1 : Offset + ChildOffset;
		}

		Line -= Lines;
		Offset += b->Length();
	}

	return blocks.Length() ? Length() : -1;
}

LNamedStyle *LRichTextPriv::DivBlock::GetStyle(ssize_t At)
{
	if (At < 0)
		return Style;

	ssize_t Pos = 0;
	for (auto b : blocks)
	{
		ssize_t Len = b->Length();
		if (At >= Pos && At < Pos + Len)
			return b->GetStyle(At - Pos);
		Pos += Len;
	}

	return blocks.Length() ? blocks.Last()->GetStyle() : Style;
}

void LRichTextPriv::DivBlock::SetStyle(LNamedStyle *s)
{
	if ((Style = s))
	{
		Fnt = d->GetFont(s);
		LAssert(Fnt);
		LCssBox<LRect>::SetStyle(Fnt, Style, Pos);
	}

	LNamedStyle *ChildStyle = MakeChildStyle(Style);

	for (auto b : blocks)
		b->SetStyle(ChildStyle);
}

bool LRichTextPriv::DivBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
{
	const char *name = GetElementName();

	s.Print("<%s", name);
	if (Style && Style->Name.Get() && *Style->Name.Get())
		s.Print(" class='%s'", Style->Name.Get());
	s.Print(">\n");

	bool FullList = Rng == nullptr;
	LRange All(0, Length());
	if (!Rng)
		Rng = &All;

	bool status = true;
	ssize_t Pos = 0;

	for (auto b : blocks)
	{
		ssize_t Len = b->Length();
		LRange ChildRange = LRange(Pos, Len).Overlap(*Rng);

		if (FullList || ChildRange.Valid())
		{
			if (!FullList)
				ChildRange.Start -= Pos;

			if (!b->ToHtml(s, Media, FullList ? nullptr : &ChildRange))
				status = false;
		}

		Pos += Len;
	}

	s.Print("</%s>\n", name);
	return status;
}

bool LRichTextPriv::DivBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor || !Cursor->Blk)
		return false;

	for (auto b : blocks)
	{
		for (Block *p = Cursor->Blk; p; p = p->parent)
			if (p == b)
				return b->GetPosFromIndex(Cursor);
	}

	if (Cursor->Blk == this)
	{
		ssize_t Pos = 0;
		int Line = 0;

		for (auto b : blocks)
		{
			ssize_t Len = b->Length();
			int ChildLines = MAX(b->GetLines(), 1);

			if (Cursor->Offset <= Pos + Len || b == blocks.Last())
			{
				ssize_t ChildOffset = MAX(Cursor->Offset - Pos, 0);
				int ChildLine = 0;

				LArray<int> ChildLineY;
				if (b->OffsetToLine(ChildOffset, nullptr, &ChildLineY) && ChildLineY.Length())
					ChildLine = ChildLineY.First();

				LRichTextPriv::BlockCursor Temp(b, ChildOffset, ChildLine);

				Cursor->Pos = Temp.Pos;
				Cursor->Line = Temp.Line;
				Cursor->LineHint = Line;
				return true;
			}

			Pos += Len;
			Line += ChildLines;
		}

		Cursor->Pos = GetPos();
		Cursor->Line = GetPos();
		Cursor->LineHint = 0;
		return true;
	}

	return false;
}

bool LRichTextPriv::DivBlock::HitTest(HitTestResult &htr)
{
	for (auto b : blocks)
	{
		if (b->HitTest(htr))
		{
			if (!htr.Blk)
				htr.Blk = b;
			return true;
		}
	}

	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	htr.Blk = this;
	htr.Idx = 0;
	htr.LineHint = 0;
	htr.Near = true;
	return true;
}

void LRichTextPriv::DivBlock::OnPaint(PaintContext &Ctx)
{
	LColour Back = Ctx.Colours[Unselected].Back;
	LRect r = Pos;

	r.x1 -= margin.x1;
	r.y1 -= margin.y1;
	r.x2 += margin.x2;
	r.y2 += margin.y2;

	Ctx.DrawBox(r, margin, Back);

	LColour BorderCol(222, 222, 222);
	if (Style && Style->BorderLeft().Style != LCss::BorderNone)
	{
		LCss::ColorDef BorderColor = Style->BorderLeft().Color;
		if (BorderColor.Type == LCss::ColorRgb)
			BorderCol.Set(BorderColor.Rgb32, 32);
	}

	Ctx.DrawBox(r, border, BorderCol);
	Ctx.DrawBox(r, padding, Back);

	for (auto b : blocks)
		b->OnPaint(Ctx);
}

bool LRichTextPriv::DivBlock::OnLayout(Flow &flow)
{
	int SavedLeft = flow.Left;
	int SavedRight = flow.Right;

	flow.Left += margin.x1;
	flow.Right -= margin.x2;
	flow.CurY += margin.y1;

	Pos.x1 = flow.Left;
	Pos.y1 = flow.CurY;
	Pos.x2 = flow.Right;
	Pos.y2 = flow.CurY - 1;

	flow.Left += border.x1 + padding.x1;
	flow.Right -= border.x2 + padding.x2;
	flow.CurY += border.y1 + padding.y1;

	for (auto b : blocks)
	{
		if (!b->OnLayout(flow))
			return false;

		LRect ChildPos = b->GetPos();
		Pos.y2 = MAX(Pos.y2, MAX(ChildPos.y2, flow.CurY - 1));
	}

	if (Pos.y2 < Pos.y1)
		Pos.y2 = Pos.y1;

	Pos.y2 += border.y2 + padding.y2;
	flow.CurY = Pos.y2 + 1 + margin.y2;

	flow.Left = SavedLeft;
	flow.Right = SavedRight;

	return true;
}

ssize_t LRichTextPriv::DivBlock::CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text)
{
	if (!Text || Offset < 0 || Chars == 0)
		return 0;

	ssize_t Copied = 0;

	for (auto b : blocks)
	{
		ssize_t Len = b->Length();

		if (Offset >= Len)
		{
			Offset -= Len;
			continue;
		}

		ssize_t Count = Chars < 0 ? -1 : Chars - Copied;
		if (Count == 0)
			break;

		Copied += b->CopyAt(Offset, Count, Text);
		if (Chars >= 0 && Copied >= Chars)
			break;

		Offset = 0;
	}

	return Copied;
}

bool LRichTextPriv::DivBlock::Seek(SeekType To, BlockCursor &Cursor)
{
	for (auto b : blocks)
	{
		auto ContainsCursor = [&](auto &&Self, Block *Candidate) -> bool
		{
			if (Candidate == Cursor.Blk)
				return true;

			for (auto Child : Candidate->blocks)
				if (Self(Self, Child))
					return true;

			return false;
		};

		if (ContainsCursor(ContainsCursor, b))
			return b->Seek(To, Cursor);
	}

	if (Cursor.Blk == this)
	{
		ssize_t Pos = 0;
		int Line = 0;

		for (auto b : blocks)
		{
			ssize_t Len = b->Length();
			int ChildLines = MAX(b->GetLines(), 1);

			if (Cursor.Offset <= Pos + Len || b == blocks.Last())
			{
				Cursor.Set(b, MAX(Cursor.Offset - Pos, 0), Line);
				return b->Seek(To, Cursor);
			}

			Pos += Len;
			Line += ChildLines;
		}
	}

	return false;
}

ssize_t LRichTextPriv::DivBlock::FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params)
{
	ssize_t Pos = 0;

	for (auto b : blocks)
	{
		ssize_t Found = b->FindAt(MAX(StartIdx - Pos, 0), Str, Params);
		if (Found >= 0)
			return Pos + Found;

		Pos += b->Length();
	}

	return -1;
}

void LRichTextPriv::DivBlock::SetSpellingErrors(LArray<::LSpellCheck::SpellingError> &Errors, LRange r)
{
	for (auto b : blocks)
		b->SetSpellingErrors(Errors, r);
}

void LRichTextPriv::DivBlock::IncAllStyleRefs()
{
	if (Style)
		Style->RefCount++;
	for (auto b : blocks)
		b->IncAllStyleRefs();
}

bool LRichTextPriv::DivBlock::DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool TopOfMenu)
{
	ssize_t Pos = 0;

	for (auto b : blocks)
	{
		ssize_t Len = b->Length();
		if (Offset >= Pos && Offset <= Pos + Len)
			return b->DoContext(s, Doc, Offset - Pos, TopOfMenu);

		Pos += Len;
	}

	return false;
}

#ifdef _DEBUG
void LRichTextPriv::DivBlock::DumpNodes(LTreeItem *blockItem)
{
	blockItem->SetText(GetClass());

	for (unsigned i = 0; i < blocks.Length(); i++)
	{
		auto childItem = new LTreeItem;
		auto b = blocks[i];

		b->DumpNodes(childItem);
		childItem->SetText(LString::Fmt("[%i] %s", i, childItem->GetText()));
		blockItem->Insert(childItem);
	}
}
#endif

LRichTextPriv::Block *LRichTextPriv::DivBlock::Clone()
{
	return new DivBlock(this);
}

bool LRichTextPriv::DivBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars, LNamedStyle *Style)
{
	if (!blocks.Length())
		return false;

	ssize_t Pos = 0;

	for (auto b : blocks)
	{
		ssize_t Len = b->Length();
		if (AtOffset <= Pos + Len || b == blocks.Last())
			return b->AddText(Trans, MIN(MAX(AtOffset - Pos, 0), Len), Str, Chars, Style);

		Pos += Len;
	}

	return false;
}

bool LRichTextPriv::DivBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add)
{
	if (Offset < 0 || Chars <= 0)
		return false;

	ssize_t Pos = 0;
	ssize_t End = Offset + Chars;
	bool Changed = false;

	for (auto b : blocks)
	{
		ssize_t BlockEnd = Pos + b->Length();
		ssize_t Start = MAX(Offset, Pos);
		ssize_t Stop = MIN(End, BlockEnd);

		if (Start < Stop)
			Changed |= b->ChangeStyle(Trans, Start - Pos, Stop - Start, Style, Add);

		Pos = BlockEnd;
	}

	return Changed;
}

ssize_t LRichTextPriv::DivBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText)
{
	if (BlkOffset < 0 || Chars <= 0)
		return 0;

	ssize_t Pos = 0;
	ssize_t Deleted = 0;

	for (auto b : blocks)
	{
		ssize_t BlockEnd = Pos + b->Length();
		ssize_t Start = MAX(BlkOffset, Pos);
		ssize_t Stop = MIN(BlkOffset + Chars, BlockEnd);

		if (Start < Stop)
			Deleted += b->DeleteAt(Trans, Start - Pos, Stop - Start, DeletedText);

		Pos = BlockEnd;
		if (Pos >= BlkOffset + Chars)
			break;
	}

	return Deleted;
}

bool LRichTextPriv::DivBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
{
	if (StartIdx < 0 || Chars <= 0)
		return false;

	ssize_t Pos = 0;
	ssize_t End = StartIdx + Chars;
	bool Changed = false;

	for (auto b : blocks)
	{
		ssize_t BlockEnd = Pos + b->Length();
		ssize_t Start = MAX(StartIdx, Pos);
		ssize_t Stop = MIN(End, BlockEnd);

		if (Start < Stop)
			Changed |= b->DoCase(Trans, Start - Pos, Stop - Start, Upper);

		Pos = BlockEnd;
	}

	return Changed;
}

LRichTextPriv::Block *LRichTextPriv::DivBlock::Split(Transaction *Trans, ssize_t AtOffset)
{
	return nullptr;
}

bool LRichTextPriv::DivBlock::StripLast(Transaction *Trans, const char *Set)
{
	if (!blocks.Length())
		return false;

	if (auto div = dynamic_cast<LRichTextPriv::DivBlock*>(blocks.Last()))
		return div->StripLast(Trans, Set);

	if (auto tb = dynamic_cast<LRichTextPriv::TextBlock*>(blocks.Last()))
		return tb->StripLast(Trans, Set);

	return false;
}

bool LRichTextPriv::DivBlock::OnDictionary(Transaction *Trans)
{
	for (auto b : blocks)
		b->OnDictionary(Trans);

	return true;
}

void LRichTextPriv::DivBlock::Dump()
{
	LgiTrace("    DivBlock: ptr=%p, length=%i, lines=%i\n",
		this,
		(int)Length(),
		GetLines());
}

} // namespace Rte2
