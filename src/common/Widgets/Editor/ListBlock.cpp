#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"

#include "RichTextEditPriv.h"

#define DEBUG_COVERAGE_TEST		0

LRichTextPriv::ListBlock::ListBlock(LRichTextPriv *priv, TType lstType) :
	Block(priv),
	type(lstType)
{
}

LRichTextPriv::ListBlock::ListBlock(ListBlock *Copy) : Block(Copy->d)
{
	type = Copy->type;
	for (unsigned i = 0; i < Copy->blocks.Length(); i++)
		Add(Copy->blocks[i]->Clone());
}

LRichTextPriv::ListBlock::~ListBlock()
{
	LAssert(Cursors == 0);
}

LRichTextPriv::TextBlock *LRichTextPriv::ListBlock::GetTextBlock()
{
	if (startItem)
	{
		startItem = false;
		Add(new LRichTextPriv::TextBlock(d));
	}

	if (!blocks.Length())
	{
		LAssert(!"there should be at least one block right?");
		return nullptr;
	}

	auto tb = dynamic_cast<LRichTextPriv::TextBlock*>(blocks.Last());
	LAssert(tb); // we should have a text block to insert into right?
	// if it's something else, maybe there should be special handling?
	return tb;
}

bool LRichTextPriv::ListBlock::IsValid()
{
	return blocks.Length() > 0;
}

ssize_t LRichTextPriv::ListBlock::Length()
{
	ssize_t Len = 0;
	for (auto b : blocks)
		Len += b->Length();
	return Len;
}

int LRichTextPriv::ListBlock::GetLines()
{
	int Lines = 0;
	for (auto b : blocks)
		Lines += MAX(b->GetLines(), 1);
	return Lines;
}

bool LRichTextPriv::ListBlock::OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY)
{
	ssize_t Start = 0;
	int StartLine = 0;
	for (auto b : blocks)
	{
		ssize_t Len = b->Length();
		int ChildLines = MAX(b->GetLines(), 1);
		if (Offset <= Start + Len || b == blocks.Last())
		{
			LArray<int> ChildLine;
			if (!b->OffsetToLine(MIN(Offset - Start, Len), ColX, &ChildLine) || !ChildLine.Length())
				return false;
			if (LineY)
			{
				for (auto Line : ChildLine)
					LineY->Add(StartLine + Line);
			}
			return true;
		}
		Start += Len;
		StartLine += ChildLines;
	}
	return false;
}

ssize_t LRichTextPriv::ListBlock::LineToOffset(ssize_t Line)
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

LNamedStyle *LRichTextPriv::ListBlock::GetStyle(ssize_t At)
{
	ssize_t Pos = 0;
	for (auto b: blocks)
	{
		ssize_t Len = b->Length();
		if (At < 0 || (At >= Pos && At < Pos + Len))
			return b->GetStyle(At < 0 ? -1 : At - Pos);
		Pos += Len;
	}
	return blocks.Length() ? blocks.Last()->GetStyle() : nullptr;
}

void LRichTextPriv::ListBlock::SetStyle(LNamedStyle *s)
{
	for (auto b : blocks)
	{
		if (auto tb = dynamic_cast<TextBlock*>(b))
			tb->SetStyle(s);
		else if (auto list = dynamic_cast<ListBlock*>(b))
			list->SetStyle(s);
	}
}

const char *LRichTextPriv::ListBlock::TypeToElem()
{
	switch (type)
	{
		case TType::ListInherit:
		case TType::ListNone:
		case TType::ListDisc:
		case TType::ListCircle:
		case TType::ListSquare:
			return "ul";

		case TType::ListDecimal:
		case TType::ListDecimalLeadingZero:
		case TType::ListLowerRoman:
		case TType::ListUpperRoman:
		case TType::ListLowerGreek:
		case TType::ListUpperGreek:
		case TType::ListLowerAlpha:
		case TType::ListUpperAlpha:
		case TType::ListArmenian:
		case TType::ListGeorgian:
			return "ol";
	}
	LAssert(!"invalid type");
	return nullptr;
}

bool LRichTextPriv::ListBlock::IsOrdered()
{
	return !strcmp(TypeToElem(), "ol");
}

static LString ToRoman(int n, bool Upper)
{
	static const struct { int v; const char *u, *l; } Map[] =
	{
		{1000, "M", "m"}, {900, "CM", "cm"}, {500, "D", "d"}, {400, "CD", "cd"},
		{100, "C", "c"}, {90, "XC", "xc"}, {50, "L", "l"}, {40, "XL", "xl"},
		{10, "X", "x"}, {9, "IX", "ix"}, {5, "V", "v"}, {4, "IV", "iv"}, {1, "I", "i"}
	};
	LString r;
	for (auto &m : Map)
		while (n >= m.v)
		{
			r += Upper ? m.u : m.l;
			n -= m.v;
		}
	return r;
}

static LString ToAlpha(int n, bool Upper)
{
	LString r;
	while (n > 0)
	{
		n--;
		char c[2] = { (char)((Upper ? 'A' : 'a') + (n % 26)), 0 };
		r = LString(c) + r;
		n /= 26;
	}
	return r;
}

LString LRichTextPriv::ListBlock::Marker(int Index)
{
	int n = Index + 1;
	LString s;
	switch (type)
	{
		case TType::ListDecimal:
		case TType::ListGeorgian:
		case TType::ListArmenian:
			s.Printf("%i.", n);
			break;
		case TType::ListDecimalLeadingZero:
			s.Printf("%02i.", n);
			break;
		case TType::ListLowerRoman:
			s = ToRoman(n, false) + ".";
			break;
		case TType::ListUpperRoman:
			s = ToRoman(n, true) + ".";
			break;
		case TType::ListLowerAlpha:
		case TType::ListLowerGreek:
			s = ToAlpha(n, false) + ".";
			break;
		case TType::ListUpperAlpha:
		case TType::ListUpperGreek:
			s = ToAlpha(n, true) + ".";
			break;
		default:
			s = "\xE2\x80\xA2"; // bullet
			break;
	}
	return s;
}

bool LRichTextPriv::ListBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
{
	bool status = true;
	auto elem = TypeToElem();
	s.Print("<%s>\n", elem);
	bool FullList = Rng == nullptr;
	LRange All(0, Length());
	if (!Rng)
		Rng = &All;

	ssize_t Pos = 0;
	for (auto b: blocks)
	{
		ssize_t Len = b->Length();
		LRange ChildRange = LRange(Pos, Len).Overlap(*Rng);
		if (FullList || ChildRange.Valid())
		{
			if (!FullList)
				ChildRange.Start -= Pos;
			s.Print("\t<li>");
			if (!b->ToHtml(s, Media, FullList ? nullptr : &ChildRange))
				status = false;
			s.Print("</li>\n");
		}
		Pos += Len;
	}
	s.Print("</%s>\n", elem);
	return status;
}

bool LRichTextPriv::ListBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor || !Cursor->Blk)
		return false;

	for (auto b : blocks)
	{
		for (auto p = Cursor->Blk; p; p = p->parent)
			if (p == b)
				return Cursor->Blk->GetPosFromIndex(Cursor);
	}
	return false;
}

bool LRichTextPriv::ListBlock::HitTest(HitTestResult &htr)
{
	ssize_t Start = 0;
	for (auto b : blocks)
	{
		if (b->HitTest(htr))
		{
			htr.Blk = b;
			htr.Idx += Start;
			return true;
		}
		Start += b->Length();
	}
	return false;
}

void LRichTextPriv::ListBlock::OnPaint(PaintContext &Ctx)
{
	Ctx.SelectBeforePaint(this);

	LColour Fore, Back = Ctx.Back();
	Fore = Ctx.Fore().Mix(Back, 0.75f);
	#if DEBUG_COVERAGE_TEST
		Ctx.pDC->Colour(Back.Mix(LColour(255, 0, 255)));
	#else
		Ctx.pDC->Colour(Back);
	#endif
	Ctx.pDC->Rectangle(&pos);
	Ctx.pDC->Colour(Fore);

	auto fnt = d->View->GetFont();
	fnt->Transparent(false);

	int idx = 0;
	for (auto b: blocks)
	{
		LRect i = items[idx++];
		#if 0
		Ctx.pDC->Colour(L_MED);
		Ctx.pDC->Box(&i);
		i.Inset(1, 1);
		#endif

		fnt->Fore(L_TEXT);
		fnt->Back(Back);

		LDisplayString bullet(fnt, Marker(idx - 1));
		int bx = IsOrdered() ? i.x2 - bullet.X() - fnt->GetHeight() / 4 : i.x1 + ((i.X() - bullet.X()) / 2);
		bullet.Draw(Ctx.pDC, MAX(bx, i.x1), i.y1, &i);

		b->OnPaint(Ctx);
	}

	Ctx.SelectAfterPaint(this);
}

bool LRichTextPriv::ListBlock::OnLayout(Flow &flow)
{
	pos.x1 = flow.Left;
	pos.y1 = flow.CurY;
	pos.x2 = flow.Right;
	pos.y2 = flow.CurY;
	
	auto fnt = d->View->GetFont();
	int marginX1 = fnt->GetHeight();
	if (IsOrdered())
	{
		LDisplayString widest(fnt, Marker(MAX((int)blocks.Length() - 1, 0)));
		marginX1 = MAX(marginX1, widest.X() + fnt->GetHeight() / 2);
	}
	flow.Left += marginX1;
	
	items.Length(blocks.Length());

	int idx = 0;
	for (auto b: blocks)
	{
		auto& i = items[idx++];
		i.x1 = pos.x1;
		i.y1 = flow.CurY;
		i.x2 = flow.Left - 1;

		if (!b->OnLayout(flow))
		{
			flow.Left -= marginX1;
			return false;
		}
		
		auto blkPos = b->GetPos();
		pos.y2 = blkPos.y2;
		i.y2 = blkPos.y2;
		flow.CurY = pos.y2 + 1;
	}

	flow.Left -= marginX1;
	return true;
}

ssize_t LRichTextPriv::ListBlock::CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text)
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

bool LRichTextPriv::ListBlock::Seek(SeekType To, BlockCursor &Cursor)
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
		{
			return b->Seek(To, Cursor);
		}
	}
	return false;
}

ssize_t LRichTextPriv::ListBlock::FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params)
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

void LRichTextPriv::ListBlock::SetSpellingErrors(LArray<LSpellCheck::SpellingError> &Errors, LRange r)
{
	for (auto b : blocks)
		b->SetSpellingErrors(Errors, r);
}

void LRichTextPriv::ListBlock::IncAllStyleRefs()
{
	for (auto b : blocks)
		b->IncAllStyleRefs();
}

bool LRichTextPriv::ListBlock::DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool TopOfMenu)
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
void LRichTextPriv::ListBlock::DumpNodes(LTreeItem *blockItem)
{
	blockItem->SetText("ListBlock");

	for (unsigned i=0; i<blocks.Length(); i++)
	{
		auto childItem = new LTreeItem;
		auto b = blocks[i];
		b->DumpNodes(childItem);
		childItem->SetText(LString::Fmt("[%i] %s", i, childItem->GetText()));
		blockItem->Insert(childItem);
	}
}
#endif

LRichTextPriv::Block *LRichTextPriv::ListBlock::Clone()
{
	return new ListBlock(this);
}

bool LRichTextPriv::ListBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars, LNamedStyle *Style)
{
	if (AtOffset < 0 || !blocks.Length())
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

bool LRichTextPriv::ListBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add)
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

ssize_t LRichTextPriv::ListBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText)
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

bool LRichTextPriv::ListBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
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

LRichTextPriv::Block *LRichTextPriv::ListBlock::Split(Transaction *Trans, ssize_t AtOffset)
{
	return nullptr;
}
