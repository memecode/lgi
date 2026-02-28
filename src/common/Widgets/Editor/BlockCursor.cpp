#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"
#include "RichTextEditPriv.h"

LRichTextPriv::BlockCursor::BlockCursor(const BlockCursor &c)
{
	*this = c;
}
		
LRichTextPriv::BlockCursor::BlockCursor(Block *b, ssize_t off, int line)
{
	Pos.ZOff(-1, -1);
	Line.ZOff(-1, -1);

	if (b)
		Set(b, off, line);
}
		
LRichTextPriv::BlockCursor::~BlockCursor()
{
	Set(NULL, 0, 0);
}
		
LRichTextPriv::BlockCursor &LRichTextPriv::BlockCursor::operator =(const BlockCursor &c)
{
	Set(c.Blk, c.Offset, c.LineHint);
	Pos = c.Pos;
	Line = c.Line;
			
	LAssert(Offset >= 0);
			
	return *this;
}
		
void LRichTextPriv::BlockCursor::Set(ssize_t off)
{
	LArray<int> Ln;
	if (!Blk->OffsetToLine(off, NULL, &Ln))
		Ln.Add(-1);

	Set(Blk, off, Ln.First());
}

void LRichTextPriv::BlockCursor::Set(LRichTextPriv::Block *b, ssize_t off, int line)
{
	if (Blk != b)
	{
		if (Blk)
		{
			LAssert(Blk->Cursors > 0);
			Blk->Cursors--;

			#if DEBUG_LOG_CURSOR_COUNT
			{
				LArray<char16> Text;
				Blk->CopyAt(0, 20, &Text);
				Text.Add(0);
				LgiTrace("%s:%i - %i del cursor %p (%.20S)\n", _FL, Blk->Cursors, Blk, &Text.First());
			}
			#endif

			Blk = NULL;
		}

		if (b)
		{
			Blk = b;
			LAssert(Blk->Cursors < 0x7f);
			Blk->Cursors++;

			#if DEBUG_LOG_CURSOR_COUNT
			{
				LArray<char16> Text;
				Blk->CopyAt(0, 20, &Text);
				Text.Add(0);
				LgiTrace("%s:%i - %i add cursor %p (%.20S)\n", _FL, Blk->Cursors, Blk, &Text.First());
			}
			#endif
		}
	}

	Offset = off;
	LineHint = line;
	if (Blk)
	{
		LRect Line;
		#ifdef _DEBUG
		ssize_t BlkLen = Blk->Length();
		#endif
		LAssert(off >= 0 && off <= BlkLen);
		Blk->GetPosFromIndex(this);
	}

	// LOG_FN("%s:%i - Cursor.Set: %i, Line: %i\n", _FL, Offset, LineHint);
			
	LAssert(Offset >= 0);
}
