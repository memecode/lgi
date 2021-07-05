#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"
#include "RichTextEditPriv.h"
#include "lgi/common/DocView.h"

LRichTextPriv::HorzRuleBlock::HorzRuleBlock(LRichTextPriv *priv) : Block(priv)
{
	IsDeleted = false;
}

LRichTextPriv::HorzRuleBlock::HorzRuleBlock(const HorzRuleBlock *Copy) : Block(Copy->d)
{
	IsDeleted = Copy->IsDeleted;
}

LRichTextPriv::HorzRuleBlock::~HorzRuleBlock()
{
	LgiAssert(Cursors == 0);
}

bool LRichTextPriv::HorzRuleBlock::IsValid()
{
	return true;
}

int LRichTextPriv::HorzRuleBlock::GetLines()
{
	return 1;
}

bool LRichTextPriv::HorzRuleBlock::OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY)
{
	if (ColX)
		*ColX = Offset > 0;
	if (LineY)
		LineY->Add(0);
	return true;
}

int LRichTextPriv::HorzRuleBlock::LineToOffset(int Line)
{
	return 0;
}

void LRichTextPriv::HorzRuleBlock::Dump()
{
}

LNamedStyle *LRichTextPriv::HorzRuleBlock::GetStyle(ssize_t At)
{
	return NULL;
}

void LRichTextPriv::HorzRuleBlock::SetStyle(LNamedStyle *s)
{
}

ssize_t LRichTextPriv::HorzRuleBlock::Length()
{
	return IsDeleted ? 0 : 1;
}

bool LRichTextPriv::HorzRuleBlock::ToHtml(LStream &s, LArray<GDocView::ContentMedia> *Media, LRange *Rng)
{
	s.Print("<hr>\n");
	return true;
}

bool LRichTextPriv::HorzRuleBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor)
		return d->Error(_FL, "No cursor param.");

	Cursor->Pos = Pos;
	Cursor->Line = Pos;
	if (Cursor->Offset == 0)
		Cursor->Pos.x2 = Cursor->Pos.x1 + 1;
	else
		Cursor->Pos.x1 = Cursor->Pos.x2 - 1;

	return true;
}

bool LRichTextPriv::HorzRuleBlock::HitTest(HitTestResult &htr)
{
	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	htr.Near = false;
	htr.LineHint = 0;

	int Cx = Pos.x1 + (Pos.X() / 2);
	if (htr.In.x < Cx)
		htr.Idx = 0;
	else
		htr.Idx = 1;

	return true;
}

void LRichTextPriv::HorzRuleBlock::OnPaint(PaintContext &Ctx)
{
	Ctx.SelectBeforePaint(this);

	GColour Fore, Back = Ctx.Back();
	Fore = Ctx.Fore().Mix(Back, 0.75f);
	Ctx.pDC->Colour(Back);
	Ctx.pDC->Rectangle(&Pos);
	Ctx.pDC->Colour(Fore);

	int Cy = Pos.y1 + (Pos.Y() >> 1);
	Ctx.pDC->Rectangle(Pos.x1, Cy-1, Pos.x2, Cy);

	if (Ctx.Cursor != NULL &&
		Ctx.Cursor->Blk == (Block*)this &&
		Ctx.Cursor->Blink &&
		d->View->Focus())
	{
		LRect &p = Ctx.Cursor->Pos;
		Ctx.pDC->Colour(Ctx.Fore());
		Ctx.pDC->Rectangle(&p);
	}

	Ctx.SelectAfterPaint(this);
}

bool LRichTextPriv::HorzRuleBlock::OnLayout(Flow &flow)
{
	Pos.x1 = flow.Left;
	Pos.y1 = flow.CurY;
	Pos.x2 = flow.Right;
	Pos.y2 = flow.CurY + 15; // Start with a 16px height.
	flow.CurY = Pos.y2 + 1;
	return true;
}

ssize_t LRichTextPriv::HorzRuleBlock::GetTextAt(ssize_t Offset, LArray<StyleText*> &t)
{
	return 0;
}

ssize_t LRichTextPriv::HorzRuleBlock::CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text)
{
	return 0;
}

bool LRichTextPriv::HorzRuleBlock::Seek(SeekType To, BlockCursor &Cursor)
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

ssize_t LRichTextPriv::HorzRuleBlock::FindAt(ssize_t StartIdx, const uint32_t *Str, GFindReplaceCommon *Params)
{
	return 0;
}

void LRichTextPriv::HorzRuleBlock::IncAllStyleRefs()
{
}

bool LRichTextPriv::HorzRuleBlock::DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool Spelling)
{
	return false;
}

#ifdef _DEBUG
void LRichTextPriv::HorzRuleBlock::DumpNodes(LTreeItem *Ti)
{
	Ti->SetText("HorzRuleBlock");
}
#endif

LRichTextPriv::Block *LRichTextPriv::HorzRuleBlock::Clone()
{
	return new HorzRuleBlock(this);
}

GMessage::Result LRichTextPriv::HorzRuleBlock::OnEvent(GMessage *Msg)
{
	return false;
}

bool LRichTextPriv::HorzRuleBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars, LNamedStyle *Style)
{
	return false;
}

bool LRichTextPriv::HorzRuleBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add)
{
	return false;
}

ssize_t LRichTextPriv::HorzRuleBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText)
{
	IsDeleted = BlkOffset == 0;
	if (IsDeleted)
		return true;

	return false;
}

bool LRichTextPriv::HorzRuleBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
{
	return false;
}

LRichTextPriv::Block *LRichTextPriv::HorzRuleBlock::Split(Transaction *Trans, ssize_t AtOffset)
{
	return NULL;
}

