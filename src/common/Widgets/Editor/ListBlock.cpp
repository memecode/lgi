#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"

#include "RichTextEditPriv.h"

#define DEBUG_COVERAGE_TEST		1

LRichTextPriv::ListBlock::ListBlock(LRichTextPriv *priv, TType lstType) :
	Block(priv),
	type(lstType)
{
}

LRichTextPriv::ListBlock::ListBlock(const ListBlock *Copy) : Block(Copy->d)
{
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

bool LRichTextPriv::ListBlock::OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY)
{
	LAssert(!"fixme");
	return false;
}

ssize_t LRichTextPriv::ListBlock::LineToOffset(ssize_t Line)
{
	LAssert(!"fixme");
	return 0;
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

bool LRichTextPriv::ListBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
{
	bool status = true;
	auto elem = TypeToElem();
	s.Print("<%s>\n", elem);
	for (auto b: blocks)
	{
		s.Print("	<li>");
		if (!b->ToHtml(s, Media, Rng))
			status = false;
	}
	s.Print("</%s>\n", elem);
	return status;
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

	const uint8_t bulletUtf8[] = { 0xE2, 0x80, 0xA2, 0 };

	int idx = 0;
	for (auto b: blocks)
	{
		LRect i = items[idx++];
		Ctx.pDC->Colour(L_MED);
		Ctx.pDC->Box(&i);
		i.Inset(1, 1);

		fnt->Fore(L_TEXT);
		fnt->Back(Back);

		LDisplayString bullet(fnt, (char*)bulletUtf8);
		bullet.Draw(Ctx.pDC, i.x1, i.y1, &i);

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
	
	int marginX1 = 20;
	flow.Left += marginX1;
	
	items.Length(blocks.Length());

	int idx = 0;
	for (auto b: blocks)
	{
		auto& i = items[idx++];
		i.x1 = pos.x1;
		i.y1 = flow.CurY;
		i.x2 = flow.Left - 1;

		b->OnLayout(flow);
		
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
	return 0;
}

bool LRichTextPriv::ListBlock::Seek(SeekType To, BlockCursor &Cursor)
{
	switch (To)
	{
		case SkSeekEnter:
		{
			if (Cursor.Offset == 0)
			{
				// move cursor to start of the first block:
				if (blocks.Length() == 0)
					return false;

				auto b = blocks[0];
				Cursor.Set(b, 0, 0);
				return true;
			}
			else
			{
				// end of block?
				LAssert(!"fixme");
			}
			break;
		}
		default:
		{
			break;
		}
	}

	return false;
}

ssize_t LRichTextPriv::ListBlock::FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params)
{
	LAssert(!"fixme");
	return 0;
}

void LRichTextPriv::ListBlock::SetSpellingErrors(LArray<LSpellCheck::SpellingError> &Errors, LRange r)
{
	LAssert(!"fixme");
}

void LRichTextPriv::ListBlock::IncAllStyleRefs()
{
	LAssert(!"fixme");
}

bool LRichTextPriv::ListBlock::DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool TopOfMenu)
{
	LAssert(!"fixme");
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
	LAssert(!"fixme");
	return false;
}

bool LRichTextPriv::ListBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add)
{
	LAssert(!"fixme");
	return false;
}

ssize_t LRichTextPriv::ListBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText)
{
	LAssert(!"fixme");
	return false;
}

bool LRichTextPriv::ListBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
{
	bool status = true;

	LAssert(!"fixme");
	for (auto b: blocks)
		if (!b->DoCase(Trans, StartIdx, Chars, Upper)) // FIXME: use correct start / chars
			status = false;

	return status;
}

LRichTextPriv::Block *LRichTextPriv::ListBlock::Split(Transaction *Trans, ssize_t AtOffset)
{
	LAssert(!"fixme");
	return nullptr;
}

