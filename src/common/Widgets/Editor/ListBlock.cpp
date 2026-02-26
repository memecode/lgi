#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"

#include "RichTextEditPriv.h"

#define DEBUG_COVERAGE_TEST		1

LRichTextPriv::ListBlock::ListBlock(LRichTextPriv *priv, bool isNumbered) :
	Block(priv),
	numbered(isNumbered)
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
		blocks.Add(new LRichTextPriv::TextBlock(d));
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

LRect LRichTextPriv::ListBlock::GetPos()
{
	return Pos;
}

bool LRichTextPriv::ListBlock::IsValid()
{
	return blocks.Length() > 0;
}

bool LRichTextPriv::ListBlock::IsBusy(bool Stop)
{
	for (auto b: blocks)
	{
		if (b->IsBusy(Stop))
			return true;
	}

	return false;
}

int LRichTextPriv::ListBlock::GetLines()
{
	int lines = 0;
	
	for (auto b: blocks)
		lines += b->GetLines();

	return lines;
}

bool LRichTextPriv::ListBlock::OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY)
{
	return false;
}

ssize_t LRichTextPriv::ListBlock::LineToOffset(ssize_t Line)
{
	return 0;
}

void LRichTextPriv::ListBlock::Dump()
{
	for (auto b: blocks)
		b->Dump();
}

LNamedStyle *LRichTextPriv::ListBlock::GetStyle(ssize_t At)
{
	ssize_t at = At;
	
	for (auto b: blocks)
	{
		if (at <= 0)
			return nullptr; // FIXME: style of the list item
			
		at--;
		if (at >= 0 && at < b->Length())
			return b->GetStyle(at);
		
		at -= b->Length();
	}

	return nullptr;
}

ssize_t LRichTextPriv::ListBlock::Length()
{
	ssize_t len = 0;

	for (auto b: blocks)
		len += 1 + b->Length();
	
	return len;
}

bool LRichTextPriv::ListBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
{
	bool status = true;
	auto elem = numbered ? "nl" : "ul";
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

bool LRichTextPriv::ListBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor)
		return d->Error(_FL, "No cursor param.");

	return true;
}

bool LRichTextPriv::ListBlock::HitTest(HitTestResult &htr)
{
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
	Ctx.pDC->Rectangle(&Pos);
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
	Pos.x1 = flow.Left;
	Pos.y1 = flow.CurY;
	Pos.x2 = flow.Right;
	Pos.y2 = flow.CurY;
	
	int marginX1 = 20;
	flow.Left += marginX1;
	
	items.Length(blocks.Length());

	int idx = 0;
	for (auto b: blocks)
	{
		auto& i = items[idx++];
		i.x1 = Pos.x1;
		i.y1 = flow.CurY;
		i.x2 = flow.Left - 1;

		b->OnLayout(flow);
		
		auto blkPos = b->GetPos();
		Pos.y2 = blkPos.y2;
		i.y2 = blkPos.y2;
		flow.CurY = Pos.y2 + 1;
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
		case SkLineStart:
		{
			Cursor.Offset = 0;
			Cursor.LineHint = 0;
			break;
		}
		case SkLineEnd:
		{
			Cursor.Offset = 1;
			Cursor.LineHint = 0;
			break;
		}
		case SkLeftChar:
		{
			if (Cursor.Offset != 1)
				return false;
			Cursor.Offset = 0;
			Cursor.LineHint = 0;
			break;
		}
		case SkRightChar:
		{
			if (Cursor.Offset != 0)
				return false;
			Cursor.Offset = 1;
			Cursor.LineHint = 0;
			break;
		}
		default:
		{
			return false;
			break;
		}
	}

	return true;
}

ssize_t LRichTextPriv::ListBlock::FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params)
{
	// FIXME: impl
	return 0;
}

void LRichTextPriv::ListBlock::SetSpellingErrors(LArray<LSpellCheck::SpellingError> &Errors, LRange r)
{
	// FIXME: impl
}

void LRichTextPriv::ListBlock::IncAllStyleRefs()
{
	// FIXME: what should happen here?
}

bool LRichTextPriv::ListBlock::DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool TopOfMenu)
{
	// FIXME: impl
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

void LRichTextPriv::ListBlock::OnComponentInstall(LString Name)
{
	for (auto b: blocks)
		b->OnComponentInstall(Name);
}

LMessage::Result LRichTextPriv::ListBlock::OnEvent(LMessage *Msg)
{
	for (auto b: blocks)
		if (b->OnEvent(Msg))
			return true;

	return false;
}

bool LRichTextPriv::ListBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars, LNamedStyle *Style)
{
	return false;
}

bool LRichTextPriv::ListBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add)
{
	return false;
}

ssize_t LRichTextPriv::ListBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText)
{
	return false;
}

bool LRichTextPriv::ListBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
{
	bool status = true;

	for (auto b: blocks)
		if (!b->DoCase(Trans, StartIdx, Chars, Upper)) // FIXME: use correct start / chars
			status = false;

	return status;
}

LRichTextPriv::Block *LRichTextPriv::ListBlock::Split(Transaction *Trans, ssize_t AtOffset)
{
	return nullptr;
}

bool LRichTextPriv::ListBlock::OnDictionary(Transaction *Trans)
{
	bool status = true;
	
	for (auto b: blocks)
		if (!b->OnDictionary(Trans))
			status = false;
		
	return status;
}
