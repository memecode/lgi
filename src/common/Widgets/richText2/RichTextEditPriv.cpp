#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit2.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/Menu.h"
#include "lgi/common/GdcTools.h"

#include "RichTextEditPriv.h"

namespace Rte2 {

#if DEBUG_ACTIVITY
static void SetActivityRect(LJson &j, const char *key, const LRect &r)
{
	LString prefix = LString(key) + ".";
	j.Set(prefix + "x1", (int64_t)r.x1);
	j.Set(prefix + "y1", (int64_t)r.y1);
	j.Set(prefix + "x2", (int64_t)r.x2);
	j.Set(prefix + "y2", (int64_t)r.y2);
}

static LString ActivityCssLen(const LCss::Len &len)
{
	LStringPipe p;
	if (len.ToString(p))
		return p.NewLStr();
	return LString();
}

static void SetActivityLen(LJson &j, const char *key, const LCss::Len &len)
{
	LString v = ActivityCssLen(len);
	j.Set(key, v.Get());
}

static LFont *ActivityFont(LRichTextPriv *priv, LNamedStyle *st)
{
	LFont *f = priv ? priv->GetFont(st) : NULL;
	if (!f && priv && priv->View)
		f = priv->View->GetFont();
	return f;
}

static void AddActivityStringArray(LJson &j, const char *key, const LArray<LString> &a)
{
	j.Set(key, a);
}

static LString ActivityColour(const LCss::ColorDef &c)
{
	switch (c.Type)
	{
		case LCss::ColorRgb:
			return LString::Fmt("#%06x", (unsigned)(c.Rgb32 & 0xffffff));
		case LCss::ColorTransparent:
			return "transparent";
		case LCss::ColorLinearGradient:
			return "linear-gradient";
		case LCss::ColorRadialGradient:
			return "radial-gradient";
		default:
			return "inherit";
	}
}

static void SetActivityBorder(LJson &j, const char *key, const LCss::BorderDef &bd)
{
	LString prefix = LString(key) + ".";
	j.Set(prefix + "width", ActivityCssLen(bd).Get());
	j.Set(prefix + "style", (int64_t)bd.Style);
	j.Set(prefix + "color", ActivityColour(bd.Color).Get());
}

// Writes the resolved style of a node: colours, font, box model and borders.
static void SetActivityStyle(LRichTextPriv *priv, LJson &j, LNamedStyle *st)
{
	if (!st)
		return;

	j.Set("style", st->Name.Get());
	j.Set("color", ActivityColour(st->Color()).Get());
	j.Set("backgroundColor", ActivityColour(st->BackgroundColor()).Get());
	j.Set("display", (int64_t)st->Display());
	j.Set("textAlign", ActivityCssLen(st->TextAlign()).Get());
	SetActivityLen(j, "lineHeight", st->LineHeight());
	SetActivityLen(j, "fontSize", st->FontSize());
	j.Set("fontWeight", (int64_t)st->FontWeight());
	j.Set("fontStyle", (int64_t)st->FontStyle());

	if (LFont *font = ActivityFont(priv, st))
	{
		j.Set("font.face", font->Face() ? font->Face() : "");
		j.Set("font.pointSize", (int64_t)font->PointSize());
	}

	SetActivityLen(j, "margin.top", st->MarginTop());
	SetActivityLen(j, "margin.right", st->MarginRight());
	SetActivityLen(j, "margin.bottom", st->MarginBottom());
	SetActivityLen(j, "margin.left", st->MarginLeft());
	SetActivityLen(j, "padding.top", st->PaddingTop());
	SetActivityLen(j, "padding.right", st->PaddingRight());
	SetActivityLen(j, "padding.bottom", st->PaddingBottom());
	SetActivityLen(j, "padding.left", st->PaddingLeft());
	SetActivityBorder(j, "border.top", st->BorderTop());
	SetActivityBorder(j, "border.right", st->BorderRight());
	SetActivityBorder(j, "border.bottom", st->BorderBottom());
	SetActivityBorder(j, "border.left", st->BorderLeft());
}

static LJson ActivityBlockState(LRichTextPriv *priv, LRichTextPriv::Block *b)
{
	LJson j;
	if (!b)
		return j;

	LRect pos = b->GetPos();
	j.Set("uid", (int64_t)b->GetUid());
	j.Set("type", b->GetClass());
	j.Set("length", (int64_t)b->Length());
	j.Set("lines", (int64_t)b->GetLines());
	SetActivityRect(j, "bounds", pos);
	SetActivityStyle(priv, j, b->GetStyle());

	if (auto box = dynamic_cast<LCssBox<LRect>*>(b))
	{
		SetActivityRect(j, "boxMargin", box->margin);
		SetActivityRect(j, "boxBorder", box->border);
		SetActivityRect(j, "boxPadding", box->padding);
	}

	LArray<LJson> children;
	for (auto child : b->blocks)
		children.Add(ActivityBlockState(priv, child));
	j.Set("children", children);

	if (auto table = dynamic_cast<LRichTextPriv::TableBlock*>(b))
	{
		int n = table->grid.NCols;
		if (n > 0 && !table->grid.Cells.empty())
		{
			int avail = table->GetPos().X();
			if (avail <= 0)
				avail = 1;

			TableGrid::Columns cols;
			table->FillMetrics(cols, avail, 0);

			int spacing = cols.Spacing;
			int fixedX = spacing * ((int)cols.Min.size() + 1)
				+ table->border.x1 + table->border.x2
				+ table->padding.x1 + table->padding.x2;
			int outer = TableGrid::PreferredWidth(cols) + fixedX;

			auto st = table->GetStyle();
			LFont *font = ActivityFont(priv, st);
			if (st)
			{
				SetActivityLen(j, "tableCssWidth", st->Width());
				SetActivityLen(j, "tableCssMaxWidth", st->MaxWidth());
				SetActivityLen(j, "tableCssMinWidth", st->MinWidth());
			}
			if (st && font)
			{
				LCss::Len w = st->Width();
				if (w && w.Type == LCss::LenPx)
					outer = MAX(1, (int)w.ToPx(avail, font));

				LCss::Len mx = st->MaxWidth();
				if (mx && mx.Type == LCss::LenPx)
					outer = MIN(outer, MAX(1, (int)mx.ToPx(avail, font)));

				LCss::Len mn = st->MinWidth();
				if (mn && mn.Type == LCss::LenPx)
					outer = MAX(outer, (int)mn.ToPx(avail, font));
			}

			outer = MIN(outer, avail);
			outer = MAX(outer, fixedX);

			std::vector<int> cw;
			if (cols.Min.size())
				TableGrid::SolveColumns(cols, MAX(outer - fixedX, 0), cw);

			LArray<LString> minWidths, maxWidths, pctWidths, fixedWidths, columnWidths, rowHeights, actualColumnWidths;
			int cn = MIN(n, (int)cols.Min.size());
			for (int c = 0; c < cn; c++)
			{
				minWidths.Add(LString::Fmt("%d", cols.Min[c]));
				maxWidths.Add(LString::Fmt("%d", cols.Max[c]));
				pctWidths.Add(LString::Fmt("%d", cols.Pct[c]));
				fixedWidths.Add(LString::Fmt("%d", cols.Fixed[c] ? 1 : 0));
				columnWidths.Add(LString::Fmt("%d", c < (int)cw.size() ? cw[c] : 0));

				int actual = 0;
				for (auto &tc : table->grid.Cells)
				{
					if (tc.CS != 1 || tc.Col != c)
						continue;
					auto td = dynamic_cast<LRichTextPriv::TdBlock*>(tc.Node);
					if (!td)
						continue;
					actual = td->GetPos().X();
					break;
				}
				actualColumnWidths.Add(LString::Fmt("%d", actual));
			}

			int rows = MIN((int)table->grid.Rows.size(), (int)table->RowH.size());
			for (int r = 0; r < rows; r++)
				rowHeights.Add(LString::Fmt("%d", table->RowH[r]));

			j.Set("tableColumns", (int64_t)cn);
			j.Set("tableOuter", (int64_t)outer);
			j.Set("tableActualOuter", (int64_t)table->GetPos().X());
			j.Set("tableSpacing", (int64_t)spacing);
			j.Set("tableFixedX", (int64_t)fixedX);
			SetActivityLen(j, "tableCellPadding", table->CellPadding);

			AddActivityStringArray(j, "tableMinWidths", minWidths);
			AddActivityStringArray(j, "tableMaxWidths", maxWidths);
			AddActivityStringArray(j, "tablePctWidths", pctWidths);
			AddActivityStringArray(j, "tableFixedWidths", fixedWidths);
			AddActivityStringArray(j, "tableColumnWidths", columnWidths);
			AddActivityStringArray(j, "tableRowHeights", rowHeights);
			AddActivityStringArray(j, "tableActualColumnWidths", actualColumnWidths);

			LArray<LJson> tableCells;
			for (size_t idx = 0; idx < table->grid.Cells.size(); idx++)
			{
				auto &tc = table->grid.Cells[idx];
				LJson c;
				c.Set("idx", (int64_t)idx);
				c.Set("row", (int64_t)tc.Row);
				c.Set("col", (int64_t)tc.Col);
				c.Set("colSpan", (int64_t)tc.CS);
				c.Set("rowSpan", (int64_t)tc.RS);
				c.Set("min", (int64_t)tc.m.Min);
				c.Set("max", (int64_t)tc.m.Max);
				c.Set("pct", (int64_t)tc.Pct);
				c.Set("fixed", (int64_t)(tc.Fixed ? 1 : 0));
				c.Set("minH", (int64_t)tc.MinH);

				int solvedCellW = cols.Spacing * (tc.CS - 1);
				for (int cc = 0; cc < tc.CS; cc++)
				{
					int col = tc.Col + cc;
					if (col < (int)cw.size())
						solvedCellW += cw[col];
				}
				c.Set("solvedCellW", (int64_t)solvedCellW);

				auto td = dynamic_cast<LRichTextPriv::TdBlock*>(tc.Node);
				if (!td)
				{
					tableCells.Add(c);
					continue;
				}

				int actualCellW = td->GetPos().X();
				LRect e = table->CellEdges(td, actualCellW > 0 ? actualCellW : solvedCellW);

				c.Set("tdUid", (int64_t)td->GetUid());
				c.Set("fixedPx", (int64_t)td->FixedPx);
				c.Set("actualCellW", (int64_t)actualCellW);
				SetActivityLen(c, "cellPadding", table->CellPadding);
				c.Set("contentH", (int64_t)td->ContentH);
				SetActivityStyle(priv, c, td->GetStyle());

				LRect cellBox = td->GetPos();
				LRect content;
				content.x1 = cellBox.x1 + e.x1;
				content.y1 = cellBox.y1 + e.y1;
				content.x2 = cellBox.x2 - e.x2;
				content.y2 = cellBox.y2 - e.y2;

				SetActivityRect(c, "cellBox", cellBox);
				SetActivityRect(c, "contentBox", content);
				SetActivityRect(c, "edges", e);
				SetActivityRect(c, "border", td->border);
				SetActivityRect(c, "padding", td->padding);
				SetActivityRect(c, "margin", td->margin);
				tableCells.Add(c);
			}
			j.Set("tableCells", tableCells);
		}
	}

	if (auto tb = dynamic_cast<LRichTextPriv::TextBlock*>(b))
	{
		j.Set("layoutDirty", (int64_t)tb->LayoutDirty);
		LArray<LJson> runs;
		ssize_t textLength = 0;
		for (auto run : tb->Txt)
		{
			if (!run)
				continue;
			LJson runState;
			auto length = run->Length();
			textLength += length;
			runState.Set("length", (int64_t)length);
			runState.Set("tag", (int64_t)run->Element);
			runState.Set("emoji", (int64_t)run->Emoji);
			SetActivityStyle(priv, runState, run->GetStyle());
			if (length)
			{
				LAutoString utf(static_cast<char*>(
					LNewConvertCp("utf-8", run->At(0), "utf-32", length * sizeof(uint32_t))));
				if (utf)
					runState.Set("text", utf.Get());
			}
			runs.Add(runState);
		}
		j.Set("textLength", (int64_t)textLength);
		j.Set("textRuns", runs);

		LArray<LJson> lines;
		for (auto line : tb->Layout)
		{
			if (!line)
				continue;
			LJson lineState;
			SetActivityRect(lineState, "bounds", line->PosOff);
			lineState.Set("newLine", (int64_t)line->NewLine);
			lineState.Set("length", (int64_t)line->Length());
			lineState.Set("displayStrings", (int64_t)line->Strs.Length());
			lines.Add(lineState);
		}
		j.Set("lineLayout", lines);
	}
	else if (auto image = dynamic_cast<LRichTextPriv::ImageBlock*>(b))
	{
		j.Set("image.source", image->Source);
		j.Set("image.layoutDirty", (int64_t)image->LayoutDirty);
		j.Set("image.busy", (int64_t)image->IsBusy());
		SetActivityRect(j, "image.bounds", image->ImgPos);
		j.Set("image.width", (int64_t)image->Size.x);
		j.Set("image.height", (int64_t)image->Size.y);
	}

	return j;
}
#endif

LRichTextPriv::LBlockArray::~LBlockArray()
{
	blocks.DeleteObjects();
}

#if DEBUG_ACTIVITY
LJson LRichTextPriv::SnapshotActivityState()
{
	LJson state;

	if (Cursor && Cursor->Blk)
	{
		state.Set("cursor.index", (int64_t)IndexOfCursor(Cursor));
		state.Set("cursor.uid", (int64_t)Cursor->Blk->GetUid());
		state.Set("cursor.offset", (int64_t)Cursor->Offset);
		state.Set("cursor.lineHint", (int64_t)Cursor->LineHint);
		SetActivityRect(state, "cursor.position", Cursor->Pos);
		SetActivityRect(state, "cursor.line", Cursor->Line);
	}
	else
		state.Set("cursor.index", (int64_t)-1);

	if (Selection && Selection->Blk)
	{
		state.Set("selection.index", (int64_t)IndexOfCursor(Selection));
		state.Set("selection.uid", (int64_t)Selection->Blk->GetUid());
		state.Set("selection.offset", (int64_t)Selection->Offset);
		state.Set("selection.lineHint", (int64_t)Selection->LineHint);
		SetActivityRect(state, "selection.position", Selection->Pos);
		SetActivityRect(state, "selection.line", Selection->Line);
	}
	else
		state.Set("selection.index", (int64_t)-1);

	LRect client = View->GetClient();
	SetActivityRect(state, "view.client", client);
	SetActivityRect(state, "view.content", Areas[LRichTextEdit::ContentArea]);
	state.Set("view.documentWidth", (int64_t)DocumentExtent.x);
	state.Set("view.documentHeight", (int64_t)DocumentExtent.y);
	state.Set("view.scrollOffsetPx", (int64_t)ScrollOffsetPx);
	state.Set("view.scrollLinePx", (int64_t)ScrollLinePx);
	state.Set("edit.dirty", (int64_t)Dirty);
	state.Set("edit.undoPosition", (int64_t)UndoPos);
	state.Set("edit.undoCount", (int64_t)UndoQue.Length());

	LArray<LJson> blocks;
	for (auto block : Blocks)
		blocks.Add(ActivityBlockState(this, block));
	state.Set("blocks", blocks);

	return state;
}

bool LRichTextPriv::CaptureActivityBaseline()
{
	if (!ToHtml())
		return false;

	ActivityState = SnapshotActivityState();
	ActivityState.Set("html", UtfNameCache.Get());
	ActivityActions.Length(0);
	return true;
}

bool LRichTextPriv::WriteActivityRecording(const char *Path, LJson &FinalState, LJson *Diagnostic)
{
	LString initial = ActivityState.GetJson();
	LString final = FinalState.GetJson();
	if (final.IsEmpty() || (initial.IsEmpty() && !Diagnostic))
		return false;

	LStringPipe output;
	output.Print("{\n  \"format\": \"lgi-rich-text-activity\",\n"
		"  \"version\": 1,\n  \"state\": %s,\n  \"finalState\": %s,\n",
		initial.IsEmpty() ? "{}" : initial.Get(), final.Get());
	if (Diagnostic)
		output.Print("  \"diagnostic\": %s,\n", Diagnostic->GetJson().Get());
	output.Print("  \"actions\": [");
	for (size_t i = 0; i < ActivityActions.Length(); i++)
		output.Print("%s\n    %s", i ? "," : "", ActivityActions[i].GetJson().Get());
	output.Print("\n  ]\n}\n");

	LString contents = output.NewLStr();
	LFile file(Path, O_WRITE);
	return file && file.SetSize(0) >= 0 && file.SetPos(0) >= 0 &&
		file.Write(contents.Get(), contents.Length()) == contents.Length();
}

void LRichTextPriv::DumpActivityError(const char *File, int Line, const char *Message)
{
	if (ActivityDumping)
		return;
	ActivityDumping = true;

	LString path = ActivityRecording && ActivityPath ? ActivityPath : View->GetRecordingPath();
	LJson diagnostic;
	diagnostic.Set("file", File);
	diagnostic.Set("line", (int64_t)Line);
	diagnostic.Set("msg", Message);
	diagnostic.Set("recording", (int64_t)ActivityRecording);
	diagnostic.Set("replaying", (int64_t)ActivityReplaying);

	// The failing data may not survive HTML export or a second layout pass.
	auto finalState = SnapshotActivityState();
	if (WriteActivityRecording(path, finalState, &diagnostic))
		LgiTrace("Rte2 activity log: %s\n", path.Get());
	else
		LgiTrace("Rte2: could not write activity log: %s\n", path.Get());
	ActivityDumping = false;
}

void LRichTextPriv::RecordActivityAction(const LJson &Action)
{
	if (ActivityRecording && !ActivityReplaying)
		ActivityActions.Add(Action);
}
#endif

void LRichTextPriv::Assert(const char *file, int line, bool test, LString msg)
{
	if (test)
		return;

	#if DEBUG_ACTIVITY
	LJson action;
	action.Set("type", "assert");
	action.Set("file", file);
	action.Set("line", (int64_t)line);
	action.Set("msg", msg);
	RecordActivityAction(action);
	DumpActivityError(file, line, msg);
	#endif

	_lgi_assert(false, LString::Fmt("Rich text assertion at %s:%i: %s", file, line, msg.Get()), file, line);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Utf16to32(LArray<uint32_t> &Out, const uint16_t *In, ssize_t WordLen)
{
	if (WordLen == 0)
	{
		Out.Length(0);
		return true;
	}

	// Count the length of utf32 chars...
	const uint16 *Ptr = In;
	ssize_t Bytes = sizeof(*In) * WordLen;
	int Chars = 0;
	while (	Bytes >= sizeof(*In) &&
			LgiUtf16To32(Ptr, Bytes) > 0)
		Chars++;
	
	// Set the output buffer size..
	if (!Out.Length(Chars))
		return false;

	// Convert the string...
	Ptr = (uint16*)In;
	Bytes = sizeof(*In) * WordLen;
	uint32_t *o = &Out[0];
	#ifdef _DEBUG
	uint32_t *e = o + Out.Length();
	#endif
	while (	Bytes >= sizeof(*In))
	{
		*o++ = LgiUtf16To32(Ptr, Bytes);
	}
	
	LAssert(o == e);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *LRichEditElemContext::GetElement(LRichEditElem *obj)
{
	return obj->Tag;
}
	
const char *LRichEditElemContext::GetAttr(LRichEditElem *obj, const char *Attr)
{
	const char *a = NULL;
	obj->Get(Attr, a);
	return a;
}

bool LRichEditElemContext::GetClasses(LString::Array &Classes, LRichEditElem *obj)
{
	const char *c;
	if (!obj->Get("class", c))
		return false;
		
	LString cls = c;
	Classes = cls.Split(" ");
	return Classes.Length() > 0;
}

LRichEditElem *LRichEditElemContext::GetParent(LRichEditElem *obj)
{
	return dynamic_cast<LRichEditElem*>(obj->Parent);
}

LArray<LRichEditElem*> LRichEditElemContext::GetChildren(LRichEditElem *obj)
{
	LArray<LRichEditElem*> a;
	for (unsigned i=0; i<obj->Children.Length(); i++)
		a.Add(dynamic_cast<LRichEditElem*>(obj->Children[i]));
	return a;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LCssCache::LCssCache()
{
	Idx = 1;
}
	
LCssCache::~LCssCache()
{
	Styles.DeleteObjects();
}

uint32_t LCssCache::GetStyles()
{
	uint32_t c = 0;
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		c += Styles[i]->RefCount != 0;
	}
	return c;
}

void LCssCache::ZeroRefCounts()
{
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		Styles[i]->RefCount = 0;
	}
}

bool LCssCache::OutputStyles(LStream &s, int TabDepth)
{
	char Tabs[64];
	memset(Tabs, '\t', TabDepth);
	Tabs[TabDepth] = 0;
		
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		LNamedStyle *ns = Styles[i];
		if (ns && ns->RefCount > 0)
		{
			s.Print("%s.%s {\n", Tabs, ns->Name.Get());
				
			LString all = ns->ToLString();
			LString::Array lines = all.Split("\n");
			for (unsigned n=0; n<lines.Length(); n++)
			{
				s.Print("%s%s\n", Tabs, lines[n].Get());
			}
				
			s.Print("%s}\n\n", Tabs);
		}
	}
		
	return true;
}

LNamedStyle *LCssCache::AddStyleToCache(LAutoPtr<LCss> &s)
{
	if (!s)
		return NULL;

	// Look through existing styles for a match...			
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		LNamedStyle *ns = Styles[i];
		if (*ns == *s)
			return ns;
	}
		
	// Not found... create new...
	LNamedStyle *ns = new LNamedStyle;
	if (ns)
	{
		char *p = Prefix;
		ns->Name.Printf("%sStyle%i", p?p:"", Idx++);
		*(LCss*)ns = *s.Get();
		Styles.Add(ns);

		#if 0 // _DEBUG
		LAutoString ss = ns->ToString();
		if (ss)
			LgiTrace("%s = %s\n", ns->Name.Get(), ss.Get());
		#endif
	}
		
	return ns;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BlockCursorState::BlockCursorState(bool cursor, LRichTextPriv::BlockCursor *c)
{
	Cursor = cursor;
	Offset = c ? c->Offset : 0;
	LineHint = c ? c->LineHint : 0;
	BlockUid = c && c->Blk ? c->Blk->GetUid() : -1;
}

bool BlockCursorState::Apply(LRichTextPriv *Ctx, bool Forward)
{
	LAutoPtr<LRichTextPriv::BlockCursor> &Bc = Cursor ? Ctx->Cursor : Ctx->Selection;
	ssize_t o = Bc ? Bc->Offset : 0;
	int lh = Bc ? Bc->LineHint : 0;
	int uid = Bc && Bc->Blk ? Bc->Blk->GetUid() : -1;

	LRichTextPriv::Block *b = nullptr;
	if (BlockUid >= 0 && !Ctx->GetBlockByUid(b, BlockUid))
		return Ctx->Error(_FL, "Cannot restore cursor block UID %i.", BlockUid);

	if (!b)
		Bc.Reset();
	else if (!Bc)
		Bc.Reset(new LRichTextPriv::BlockCursor(b, Offset, LineHint));
	else
		Bc->Set(b, Offset, LineHint);

	Offset = o;
	LineHint = lh;
	BlockUid = uid;
	return true;
}

/// This is the simplest form of undo, just save the entire block state, and restore it if needed
CompleteTextBlockState::CompleteTextBlockState(LRichTextPriv *Ctx, LRichTextPriv::TextBlock *Tb)
{
	if (Ctx->Cursor)
		Cur.Reset(new BlockCursorState(true, Ctx->Cursor));
	if (Ctx->Selection)
		Sel.Reset(new BlockCursorState(false, Ctx->Selection));
	if (Tb)
	{
		Uid = Tb->GetUid();
		Blk.Reset(new LRichTextPriv::TextBlock(Tb));
	}
}

bool CompleteTextBlockState::Apply(LRichTextPriv *Ctx, bool Forward)
{
	int Index;
	LRichTextPriv::TextBlock *b;
	if (!Ctx->GetBlockByUid(b, Uid, &Index))
		return false;

	// Swap the local state with the block in the ctx
	Blk->UpdateSpellingAndLinks(NULL, LRange(0, Blk->Length()));
	auto &Siblings = b->parent ? b->parent->blocks : Ctx->Blocks;
	Blk->parent = b->parent;
	Siblings[Index] = Blk.Release();
	Blk.Reset(b);

	// Update cursors
	if (Cur)
		Cur->Apply(Ctx, Forward);
	if (Sel)
		Sel->Apply(Ctx, Forward);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MultiBlockState::MultiBlockState(LRichTextPriv *ctx, ssize_t Start, bool SaveCursors)
{
	Ctx = ctx;
	Index = Start;
	Length = -1;
	if (SaveCursors)
	{
		Cur.Reset(new BlockCursorState(true, ctx->Cursor));
		Sel.Reset(new BlockCursorState(false, ctx->Selection));
	}
}

bool MultiBlockState::Apply(LRichTextPriv *Ctx, bool Forward)
{
	if (Index < 0 || Length < 0)
	{
		LAssert(!"Missing parameters");
		return false;
	}
	
	// Undo: Swap 'Length' blocks Ctx->Blocks with Blks
	ssize_t OldLen = Blks.Length();
	bool Status = Blks.SwapRange(LRange(0, OldLen), Ctx->Blocks, LRange(Index, Length));
	if (Status)
	{
		Length = OldLen;
		if (Cur && !Cur->Apply(Ctx, Forward))
			return false;
		if (Sel && !Sel->Apply(Ctx, Forward))
			return false;
	}
	
	return Status;
}

bool MultiBlockState::Copy(ssize_t Idx)
{
	if (!Ctx->Blocks.AddressOf(Idx))
		return false;

	LRichTextPriv::Block *b = Ctx->Blocks[Idx]->Clone();
	if (!b)
		return false;

	Blks.Add(b);
	return true;
}

bool MultiBlockState::Cut(ssize_t Idx)
{
	if (!Ctx->Blocks.AddressOf(Idx))
		return false;

	LRichTextPriv::Block *b = Ctx->Blocks[Idx];
	if (!b)
		return false;

	Blks.Add(b);

	return Ctx->Blocks.DeleteAt(Idx, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LRichTextPriv::LRichTextPriv(LRichTextEdit *view, LRichTextPriv **Ptr) :
	LHtmlParser(view),
	LFontCache(LSysFont)
{
	if (Ptr) *Ptr = this;
	View = view;
	Log = &LogBuffer;
	ScrollLinePx = LSysFont->GetHeight();
	if (EditFont.Reset(new LFont))
		*EditFont = *LSysFont;

	for (unsigned i=0; i<CountOf(Areas); i++)
	{
		Areas[i].ZOff(-1, -1);
	}

	ClickedBtn = LRichTextEdit::MaxArea;
	OverBtn = LRichTextEdit::MaxArea;
	ZeroObj(BtnState);

	Values[LRichTextEdit::FontFamilyBtn] = "FontName";
	BtnState[LRichTextEdit::FontFamilyBtn].IsMenu = true;
	Values[LRichTextEdit::FontSizeBtn] = "14";
	BtnState[LRichTextEdit::FontSizeBtn].IsMenu = true;

	Values[LRichTextEdit::BoldBtn] = true;
	BtnState[LRichTextEdit::BoldBtn].IsPress = true;
	Values[LRichTextEdit::ItalicBtn] = false;
	BtnState[LRichTextEdit::ItalicBtn].IsPress = true;
	Values[LRichTextEdit::UnderlineBtn] = false;
	BtnState[LRichTextEdit::UnderlineBtn].IsPress = true;
	
	Values[LRichTextEdit::BulletsBtn] = ResizeIcon(IconBullets, 16);
	BtnState[LRichTextEdit::BulletsBtn].IsPress = true;
	Values[LRichTextEdit::NumberedListBtn] = ResizeIcon(IconNumbered, 16);
	BtnState[LRichTextEdit::NumberedListBtn].IsPress = true;
	
	Values[LRichTextEdit::ForegroundColourBtn] = (int64)LColour(L_TEXT).c32();
	BtnState[LRichTextEdit::ForegroundColourBtn].IsMenu = true;
	Values[LRichTextEdit::BackgroundColourBtn] = (int64)LColour(L_WORKSPACE).c32();
	BtnState[LRichTextEdit::BackgroundColourBtn].IsMenu = true;

	Values[LRichTextEdit::MakeLinkBtn] = TEXT_LINK;
	BtnState[LRichTextEdit::MakeLinkBtn].IsPress = true;
	
	Values[LRichTextEdit::RemoveLinkBtn] = TEXT_REMOVE_LINK;
	BtnState[LRichTextEdit::RemoveLinkBtn].IsPress = true;
	
	Values[LRichTextEdit::RemoveStyleBtn] = TEXT_REMOVE_STYLE;
	BtnState[LRichTextEdit::RemoveStyleBtn].IsPress = true;
	
	//Values[LRichTextEdit::CapabilityBtn] = TEXT_CAP_BTN;
	//BtnState[LRichTextEdit::CapabilityBtn].IsPress = true;
	
	Values[LRichTextEdit::EmojiBtn] = TEXT_EMOJI;
	BtnState[LRichTextEdit::EmojiBtn].IsPress = true;
	
	Values[LRichTextEdit::HorzRuleBtn] = TEXT_HORZRULE;
	BtnState[LRichTextEdit::HorzRuleBtn].IsPress = true;

	Values[LRichTextEdit::UndoBtn] = ResizeIcon(IconUndo, 16);
	BtnState[LRichTextEdit::UndoBtn].IsPress = true;
	Values[LRichTextEdit::RedoBtn] = ResizeIcon(IconRedo, 16);
	BtnState[LRichTextEdit::RedoBtn].IsPress = true;

	Padding(LCss::Len(LCss::LenPx, 4));
	EmptyDoc();
}
	
LRichTextPriv::~LRichTextPriv()
{
	if (IsBusy(true))
	{
		LgiMsg(View, "LRichText worker thread still busy with tasks.", "Error");
	}
	
	Empty();
}

bool LRichTextPriv::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (!Stricmp(Name, LGI_FILTER_SIZE_REQ))
	{
		int x = IconMax * toolbarReqHeight;
		int y = toolbarReqHeight;
		Value = LString::Fmt("%i,%i", x, y);
		return true;
	}
	
	return false;
}

#define SVG_FILE "rte-toolbar.svg"
LAutoPtr<LSurface> LRichTextPriv::ResizeIcon(ToolbarIconIndex index, int px)
{
	LAutoPtr<LSurface> out;

	if (!toolbarIcons)
	{
		toolbarReqHeight = px;
		
		auto path = LFindFile(SVG_FILE);
		if (!path)
		{
			LFile::Path exe(LSP_EXE);
			auto svg = exe / __FILE__/ ".." / SVG_FILE;
			if (svg.Exists())
				path = svg.GetFull();
			else
				LgiTrace("%s:%i - didn't find svg at '%s'\n", _FL, svg.GetFull().Get());
		}
		
		if (path)
			toolbarIcons.Reset(GdcD->Load(path, false, this));
	}

	if (toolbarIcons)
	{
		LRect r(toolbarIcons->Y(), toolbarIcons->Y());
		r.Offset(toolbarIcons->Y() * (int)index, 0);
		
		if (out.Reset(new LMemDC(_FL, px, px, System32BitColourSpace)))
		{
			// printf("%s:%i - px=%i y=%i\n", _FL, px, toolbarIcons->Y());
			if (px == toolbarIcons->Y())
			{
				// no scaling needed... yay!
				out->Blt(0, 0, toolbarIcons, &r);
			}
			else
			{
				ResampleDC(out, toolbarIcons, &r);
			}
		}
	}
	else LgiTrace("%s:%i - failed to load toolbarIcons.\n", _FL);
	
	return out;
}

bool LRichTextPriv::DeleteSelection(Transaction *Trans, char16 **Cut)
{
	if (!Cursor || !Selection)
		return false;

	LArray<uint32_t> DeletedText;
	LArray<uint32_t> *DelTxt = Cut ? &DeletedText : NULL;

	bool Cf = CursorFirst();
	LRichTextPriv::BlockCursor *Start = Cf ? Cursor : Selection;
	LRichTextPriv::BlockCursor *End = Cf ? Selection : Cursor;
	if (Start->Blk->parent || End->Blk->parent)
	{
		if (IndexOfCursor(Start) == IndexOfCursor(End))
			return false;

		LArray<Block*> Leaves;
		auto CollectLeaves = [&](auto &&Self, Block *b) -> void
		{
			if (b->blocks.Length())
				for (auto Child : b->blocks)
					Self(Self, Child);
			else
				Leaves.Add(b);
		};
		for (auto b : Blocks)
			CollectLeaves(CollectLeaves, b);

		ssize_t StartLeaf = Leaves.IndexOf(Start->Blk);
		ssize_t EndLeaf = Leaves.IndexOf(End->Blk);
		if (StartLeaf < 0 || EndLeaf < StartLeaf)
			return Error(_FL, "Selection endpoints are not in document order.");

		auto RootOf = [](Block *b)
		{
			while (b->parent)
				b = b->parent;
			return b;
		};
		ssize_t First = Blocks.IndexOf(RootOf(Start->Blk));
		ssize_t Last = Blocks.IndexOf(RootOf(End->Blk));
		if (First < 0 || Last < First)
			return Error(_FL, "Selection roots are not in document order.");

		LAutoPtr<MultiBlockState> State(new MultiBlockState(this, First, true));
		for (ssize_t n = First; n <= Last; n++)
		{
			if (Blocks[n]->IsBusy())
			{
				LgiTrace("%s:%i - Cannot delete while a selected block is busy.\n", _FL);
				return false;
			}
		}
		for (ssize_t n = First; n <= Last; n++)
			if (!State->Copy(n))
				return Error(_FL, "Could not save nested selection block for undo.");

		LArray<Block*> Removed;
		Block *Caret = nullptr;
		ssize_t CaretOffset = 0;
		for (ssize_t n = StartLeaf; n <= EndLeaf; n++)
		{
			Block *b = Leaves[n];
			ssize_t Len = b->Length();
			ssize_t From = n == StartLeaf ? Start->Offset : 0;
			ssize_t To = n == EndLeaf ? End->Offset : Len;
			if (From == 0 && To == Len && (Len > 0 || (n > StartLeaf && n < EndLeaf)))
			{
				if (DelTxt && Len > 0)
					b->CopyAt(0, Len, DelTxt);
				Removed.Add(b);
				continue;
			}
			if (To > From && b->DeleteAt(NoTransaction, From, To - From, DelTxt) != To - From)
				return Error(_FL, "Could not delete the full nested selection from %s.", b->GetClass());
			if (!Caret)
			{
				Caret = b;
				CaretOffset = From;
			}
		}
		auto RootsBefore = Blocks.Length();
		if (!Caret)
		{
			if (EndLeaf + 1 < (ssize_t)Leaves.Length())
				Caret = Leaves[EndLeaf + 1];
			else if (StartLeaf > 0)
			{
				Caret = Leaves[StartLeaf - 1];
				CaretOffset = Caret->Length();
			}
			else
			{
				// Keep an editable leaf when the entire document is selected.
				auto Parent = Leaves[StartLeaf]->parent;
				auto Text = new TextBlock(this);
				if (Parent)
				{
					if (auto Div = dynamic_cast<DivBlock*>(Parent))
						Text->SetStyle(Div->MakeChildStyle(Div->GetStyle()));
					Parent->Add(Text);
				}
				else
					Blocks.AddAt(First, Text);
				Caret = Text;
			}
		}

		Cursor->Set(Caret, CaretOffset, -1);
		Selection.Reset();
		for (auto b : Removed)
		{
			// Remove empty containers as well, but keep containers with surviving children.
			while (b)
			{
				Block *Parent = b->parent;
				auto &Siblings = Parent ? Parent->blocks : Blocks;
				Siblings.Delete(b, true);
				delete b;
				b = Parent && !Parent->blocks.Length() ? Parent : nullptr;
			}
		}
		State->Length = Last - First + 1 + (ssize_t)Blocks.Length() - (ssize_t)RootsBefore;
		Trans->Add(State.Release());
	}
	else if (Start->Blk == End->Blk)
	{
		// In the same block... just delete the text
		ssize_t Len = End->Offset - Start->Offset;
		LRichTextPriv::Block *NextBlk = Next(Start->Blk);
		if (Len >= Start->Blk->Length() && NextBlk)
		{
			// Delete entire block
			ssize_t i = Blocks.IndexOf(Start->Blk);
			LAutoPtr<MultiBlockState> MultiState(new MultiBlockState(this, i, true));
			MultiState->Cut(i);
			MultiState->Length = 0;
			Start->Set(NextBlk, 0, 0);
			Trans->Add(MultiState.Release());
		}
		else
		{
			Start->Blk->DeleteAt(Trans, Start->Offset, Len, DelTxt);
		}
	}
	else
	{
		// Multi-block delete...

		ssize_t i = Blocks.IndexOf(Start->Blk);
		ssize_t e = Blocks.IndexOf(End->Blk);
		LAutoPtr<MultiBlockState> MultiState(new MultiBlockState(this, i, true));

		// 1) Delete all the content to the end of the first block
		ssize_t StartLen = Start->Blk->Length();
		if (Start->Offset < StartLen)
		{
			MultiState->Copy(i++);
			Start->Blk->DeleteAt(NoTransaction, Start->Offset, StartLen - Start->Offset, DelTxt);
		}
		else if (Start->Offset == StartLen)
		{
			// This can happen because there is an implied '\n' at the end of each block. The
			// next block always starts on a new line. We just increment the index 'i' to avoid
			// the next loop deleting the start block.
			i++;

			// If the start and end blocks can be merged, the new line will eventually get removed
			// then. If not, then... well whatevs.
		}
		else
		{
			LAssert(0);
			return Error(_FL, "Cursor outside start block.");
		}

		// 2) Delete any blocks between 'Start' and 'End'
		if (i >= 0)
		{
			while (Blocks[i] != End->Blk && i < (int)Blocks.Length())
			{
				LRichTextPriv::Block *b = Blocks[i];
				b->CopyAt(0, -1, DelTxt);
				printf("%s:%i - inter, %i\n", _FL, (int)i);
				MultiState->Cut(i);
				e--;
			}
		}
		else
		{
			LAssert(0);
			return Error(_FL, "Start block has no index.");;
		}

		if (End->Offset > 0)
		{
			// 3) Delete any text up to the Cursor in the 'End' block
			MultiState->Copy(i);
			printf("%s:%i - end, %i-%i, %i\n", _FL, (int)0, (int)End->Offset, (int)End->Blk->Length());
			End->Blk->DeleteAt(NoTransaction, 0, End->Offset, DelTxt);
		}

		// Try and merge the start and end blocks
		bool MergeOk = Merge(NoTransaction, Start->Blk, End->Blk);
		MultiState->Length = MergeOk ? 1 : 2;
		Trans->Add(MultiState.Release());
	}

	// Set the cursor and update the screen
	if (Selection)
	{
		Cursor->Set(Start->Blk, Start->Offset, Start->LineHint);
		Selection.Reset();
	}
	View->Invalidate();

	if (Cut)
	{
		DelTxt->Add(0);
		*Cut = (char16*)LNewConvertCp(LGI_WideCharset, &DelTxt->First(), "utf-32", DelTxt->Length()*sizeof(uint32_t));
	}

	return true;
}


LRichTextPriv::Block *LRichTextPriv::Next(Block *b)
{
	for (Block *Current = b; Current; Current = Current->parent)
	{
		auto &Siblings = Current->parent ? Current->parent->blocks : Blocks;
		ssize_t Idx = Siblings.IndexOf(Current);
		if (Idx < 0 || Idx + 1 >= (ssize_t)Siblings.Length())
			continue;

		Block *Next = Siblings[Idx + 1];
		while (Next->blocks.Length())
			Next = Next->blocks.First();
		return Next;
	}
	return nullptr;
}

LRichTextPriv::Block *LRichTextPriv::Prev(Block *b)
{
	for (Block *Current = b; Current; Current = Current->parent)
	{
		auto &Siblings = Current->parent ? Current->parent->blocks : Blocks;
		ssize_t Idx = Siblings.IndexOf(Current);
		if (Idx <= 0)
			continue;

		Block *Prev = Siblings[Idx - 1];
		while (Prev->blocks.Length())
			Prev = Prev->blocks.Last();
		return Prev;
	}
	return nullptr;
}

bool LRichTextPriv::AddTrans(LAutoPtr<Transaction> &t)
{
	if (t)
	{
		if (UndoPosLock)
		{
			LgiTrace("%s:%i - AddTrans failed - UndoPosLocked.\n", _FL);
			return false;
		}

		// Delete any transaction history after 'UndoPos'
		for (size_t i=UndoPos; i<UndoQue.Length(); i++)
		{
			delete UndoQue[i];
		}
		UndoQue.Length(UndoPos);

		// Now add the new transaction
		UndoQue.Add(t.Release());

		// And the position is now at the end of the que
		UndoPos = UndoQue.Length();
	}

	return true;
}

bool LRichTextPriv::SetUndoPos(ssize_t Pos)
{
	if (UndoPosLock)
		return false;

	Pos = limit(Pos, 0, (int)UndoQue.Length());
	if (UndoQue.Length() == 0)
		return true;

	while (Pos != UndoPos)
	{
		auto Prev = UndoPos;
		if (Pos > UndoPos)
		{
			// Forward in queue
			Transaction *t = UndoQue[UndoPos];

			UndoPosLock = true;
			if (!t->Apply(this, true))
				goto ApplyError;
			UndoPosLock = false;

			LAssert(UndoPos == Prev);
			UndoPos++;
		}
		else if (Pos < UndoPos)
		{
			// Back in queue
			Transaction *t = UndoQue[UndoPos-1];

			UndoPosLock = true;
			if (!t->Apply(this, false))
				goto ApplyError;
			UndoPosLock = false;

			LAssert(UndoPos == Prev);
			UndoPos--;
		}
		else break; // We are done
	}

	Dirty = true;
	InvalidateDoc(NULL);
	return true;

ApplyError:
	UndoPosLock = false;
	return false;	
}

bool LRichTextPriv::IsBusy(bool Stop)
{
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		if (Blocks[i]->IsBusy(Stop))
			return true;
	}
	return false;
}

bool LRichTextPriv::Block::IsBusy(bool Stop)
{
	for (auto b : blocks)
		if (b->IsBusy(Stop))
			return true;
	return false;
}

bool LRichTextPriv::ContainsBlock(const Block *Target) const
{
	auto Find = [&](auto &&Self, const LArray<Block*> &Siblings) -> bool
	{
		for (unsigned i = 0; i < Siblings.Length(); i++)
		{
			auto b = Siblings.ItemAt(i);
			if (b == Target || Self(Self, b->blocks))
				return true;
		}
		return false;
	};
	return Find(Find, Blocks);
}

bool LRichTextPriv::Error(const char *file, int line, const char *fmt, ...)
{
	va_list Arg;
	va_start(Arg, fmt);
	LString s;
	LPrintf(s, fmt, Arg);
	va_end(Arg);

	LString Err;
	Err.Printf("%s:%i - Error: %s\n", file, line, s.Get());
	Log->Write(Err, Err.Length());

	Err = LogBuffer.NewLStr();
	LgiTrace("%.*s", Err.Length(), Err.Get());
	#if DEBUG_ACTIVITY
	DumpActivityError(file, line, s);
	#endif
		
	LAssert(0);
	return false;
}

void LRichTextPriv::UpdateStyleUI()
{
	if (!Cursor || !Cursor->Blk)
	{
		Error(_FL, "Not a valid cursor.");
		return;
	}

	TextBlock *b = dynamic_cast<TextBlock*>(Cursor->Blk);

	LArray<StyleText*> Styles;
	if (b)
		b->GetTextAt(Cursor->Offset, Styles);
	StyleText *st = Styles.Length() ? Styles.First() : NULL;

	LFont *f = NULL;
	if (st)
		f = GetFont(st->GetStyle());
	else if (View)
		f = View->GetFont();
	else if (LAppInst)
		f = LSysFont;
		
	if (f)
	{
		auto face = f->Face();
		Values[LRichTextEdit::FontFamilyBtn] = face;
		Values[LRichTextEdit::FontSizeBtn] = f->PointSize();
		Values[LRichTextEdit::FontSizeBtn].CastString();
		Values[LRichTextEdit::BoldBtn] = f->Bold();
		Values[LRichTextEdit::ItalicBtn] = f->Italic();
		Values[LRichTextEdit::UnderlineBtn] = f->Underline();
	}
	else
	{
		Values[LRichTextEdit::FontFamilyBtn] = "(Unknown)";
	}
		
	Values[LRichTextEdit::ForegroundColourBtn] = (int64) (st && st->Colours.Fore.IsValid() ? st->Colours.Fore.c32() : TextColour.c32());
	Values[LRichTextEdit::BackgroundColourBtn] = (int64) (st && st->Colours.Back.IsValid() ? st->Colours.Back.c32() : 0);

	if (View)
		View->Invalidate(Areas + LRichTextEdit::ToolsArea);
}

void LRichTextPriv::ScrollTo(LRect r)
{
	LRect Content = Areas[LRichTextEdit::ContentArea];
	Content.Offset(-Content.x1, ScrollOffsetPx-Content.y1);

	if (ScrollLinePx > 0)
	{
		if (r.y1 < Content.y1)
		{
			int OffsetPx = MAX(r.y1, 0);
			View->SetScrollPos(0, OffsetPx / ScrollLinePx);
			InvalidateDoc(NULL);
		}
		if (r.y2 > Content.y2)
		{
			int OffsetPx = r.y2 - Content.Y();
			View->SetScrollPos(0, (OffsetPx + ScrollLinePx - 1) / ScrollLinePx);
			InvalidateDoc(NULL);
		}
	}
}

void LRichTextPriv::InvalidateDoc(LRect *r)
{
	// Transform the coordinates from doc to screen space
	LRect &c = Areas[LRichTextEdit::ContentArea];
	if (r)
	{
		LRect t = *r;
		t.Offset(c.x1, c.y1 - ScrollOffsetPx);
		View->Invalidate(&t);
	}
	else View->Invalidate(&c);
}

void LRichTextPriv::EmptyDoc()
{
	if (auto Def = new TextBlock(this))
	{			
		Blocks.Add(Def);
		Cursor.Reset(new BlockCursor(Def, 0, 0));
		UpdateStyleUI();
	}
}
	
void LRichTextPriv::Empty()
{
	// Delete cursors first to avoid hanging references
	Cursor.Reset();
	Selection.Reset();
		
	// Clear the block list..
	Blocks.DeleteObjects();
}

bool LRichTextPriv::Seek(BlockCursor *In, SeekType Dir, bool Select)
{
	if (!In || !In->Blk || Blocks.Length() == 0)
		return Error(_FL, "Not a valid 'In' cursor, Blks=%i", Blocks.Length());
		
	LAutoPtr<BlockCursor> c;

	bool Status = false;
	switch (Dir)
	{
		case SkLineEnd:
		case SkLineStart:
		case SkUpLine:
		case SkDownLine:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			Block *b = c->Blk;
			Status = b->Seek(Dir, *c);
			if (Status)
				break;

			if (Dir == SkUpLine)
			{
				// No more lines in the current block...
				// Move to the next block.
				if (auto PrevBlock = Prev(b))
				{
					c.Reset(new BlockCursor(PrevBlock, PrevBlock->Length(), PrevBlock->GetLines() - 1));
					LAssert(c->Offset >= 0);
					Status = true;							
				}
			}
			else if (Dir == SkDownLine)
			{
				// No more lines in the current block...
				// Move to the next block.
				if (auto NextBlock = Next(b))
				{
					c.Reset(new BlockCursor(NextBlock, 0, 0));
					LAssert(c->Offset >= 0);
					Status = true;							
				}
			}
			break;
		}
		case SkDocStart:
		{
			Block *First = GetBlockByIndex(0);
			if (!First || !c.Reset(new BlockCursor(First, 0, 0)))
				break;

			Status = true;
			break;
		}
		case SkDocEnd:
		{
			if (Blocks.Length() == 0)
				break;

			Block *Last = GetBlockByIndex(0);
			while (Last && Next(Last))
				Last = Next(Last);
			if (!Last || !c.Reset(new BlockCursor(Last, Last->Length(), -1)))
				break;

			Status = true;
			break;
		}
		case SkLeftChar:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset > 0)
			{
				LArray<int> Ln;
				c->Blk->OffsetToLine(c->Offset, NULL, &Ln);
				if (Ln.Length() == 2 &&
					c->LineHint == Ln.Last())
				{
					c->LineHint = Ln.First();
				}
				else
				{
					c->Offset--;
					if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
						c->LineHint = Ln.First();
				}

				Status = true;
			}
			else // Seek to previous block
			{
				SeekPrevBlock:
				Block *b = Prev(c->Blk);
				if (!b)
					break;

				if (!c.Reset(new BlockCursor(b, b->Length(), b->GetLines()-1)))
					break;

				Status = true;
			}
			break;
		}
		case SkLeftWord:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset > 0)
			{
				LArray<uint32_t> a;
				c->Blk->CopyAt(0, c->Offset, &a);
					
				ssize_t i = c->Offset;
				while (i > 0 && IsWordBreakChar(a[i-1]))
					i--;
				while (i > 0 && !IsWordBreakChar(a[i-1]))
					i--;

				c->Offset = i;

				LArray<int> Ln;
				if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
					c->LineHint = Ln[0];

				Status = true;
			}
			else // Seek into previous block?
			{
				goto SeekPrevBlock;
			}
			break;
		}
		case SkRightChar:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset < c->Blk->Length())
			{
				LArray<int> Ln;
				if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln) &&
					Ln.Length() == 2 &&
					c->LineHint == Ln.First())
				{
					c->LineHint = Ln.Last();
				}
				else
				{
					c->Offset++;
					if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
						c->LineHint = Ln.First();
				}

				Status = true;
			}
			else // Seek to next block
			{
				SeekNextBlock:
				Block *b = Next(c->Blk);
				if (!b)
					break;

				if (!c.Reset(new BlockCursor(b, 0, 0)))
					break;

				Status = true;
			}
			break;
		}
		case SkRightWord:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset < c->Blk->Length())
			{
				LArray<uint32_t> a;
				ssize_t RemainingCh = c->Blk->Length() - c->Offset;
				c->Blk->CopyAt(c->Offset, RemainingCh, &a);
					
				int i = 0;
				while (i < RemainingCh && !IsWordBreakChar(a[i]))
					i++;
				while (i < RemainingCh && IsWordBreakChar(a[i]))
					i++;

				c->Offset += i;
				
				LArray<int> Ln;
				if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
					c->LineHint = Ln.Last();
				else
					c->LineHint = -1;
				Status = true;
			}
			else // Seek into next block?
			{
				goto SeekNextBlock;
			}
			break;
		}
		case SkUpPage:
		{
			LRect &Content = Areas[LRichTextEdit::ContentArea];
			int LineHint = -1;
			int TargetY = In->Pos.y1 - Content.Y();
			ssize_t Idx = HitTest(In->Pos.x1, MAX(TargetY, 0), LineHint);
			if (Idx >= 0)
			{
				ssize_t Offset = -1;
				Block *b = GetBlockByIndex(Idx, &Offset);
				if (b)
				{
					if (!c.Reset(new BlockCursor(b, Offset, LineHint)))
						break;

					Status = true;
				}
			}
			break;
		}
		case SkDownPage:
		{
			LRect &Content = Areas[LRichTextEdit::ContentArea];
			int LineHint = -1;
			int TargetY = In->Pos.y1 + Content.Y();
			ssize_t Idx = HitTest(In->Pos.x1, MIN(TargetY, DocumentExtent.y-1), LineHint);
			if (Idx >= 0)
			{
				ssize_t Offset = -1;
				Block *b = GetBlockByIndex(Idx, &Offset);

				if (!b ||
					Idx < IndexOfCursor(Cursor))
				{
					LAssert(!"GetBlockByIndex failed.\n");
					LgiTrace("%s:%i - GetBlockByIndex failed.\n", _FL);
				}

				if (b)
				{
					if (!c.Reset(new BlockCursor(b, Offset, LineHint)))
						break;

					Status = true;
				}
			}
			break;
		}
		default:
		{
			return Error(_FL, "Unknown seek type.");
		}
	}
		
	if (Status)
		SetCursor(c, Select);
		
	return Status;
}
	
bool LRichTextPriv::CursorFirst()
{
	if (!Cursor || !Selection)
		return true;

	return IndexOfCursor(Cursor) < IndexOfCursor(Selection);
}
	
bool LRichTextPriv::SetCursor(LAutoPtr<BlockCursor> c, bool Select)
{
	if (!c || !c->Blk)
		return Error(_FL, "Invalid cursor.");

	if (Select && !Selection)
	{
		// Selection starting... save cursor as selection end point
		Selection = Cursor;
	}
	else if (!Select && Selection)
	{
		// Selection ending... invalidate selection region and delete 
		// selection end point
		LRect r = SelectionRect();
		InvalidateDoc(&r);
		Selection.Reset();

		// LgiTrace("Ending selection delete(sel) Idx=%i\n", i);
	}

	if (Cursor && !Select)
	{
		// Just moving cursor
		InvalidateDoc(&Cursor->Pos);
	}

	if (!Cursor)
		Cursor.Reset(new BlockCursor(*c));
	else
		Cursor = c;

	// LgiTrace("%s:%i - SetCursor: %i, line: %i\n", _FL, Cursor->Offset, Cursor->LineHint);
	
	if (Cursor &&
		Selection &&
		*Cursor == *Selection)
		Selection.Reset();

	Cursor->Blk->GetPosFromIndex(Cursor);
	UpdateStyleUI();
	
	#if DEBUG_OUTLINE_CUR_DISPLAY_STR || DEBUG_OUTLINE_CUR_STYLE_TEXT
	InvalidateDoc(NULL);
	#else
	if (Select)
	{
		// Blocks between the old and new end points change their highlight
		// during a drag, so repaint the whole content area
		InvalidateDoc(NULL);
	}
	else
	{
		InvalidateDoc(&Cursor->Pos);
	}
	#endif

	// Check the cursor is on the visible part of the document.
	if (Cursor->Pos.Valid())
		ScrollTo(Cursor->Pos);

	return true;
}

LRect LRichTextPriv::SelectionRect()
{
	LRect SelRc;
	if (Cursor)
	{
		SelRc = Cursor->Line;
		if (Selection)
			SelRc.Union(&Selection->Line);
	}
	else if (Selection)
	{
		SelRc = Selection->Line;
	}
	return SelRc;
}

ssize_t LRichTextPriv::IndexOfCursor(BlockCursor *c)
{
	if (!c || !c->Blk)
	{
		Error(_FL, "Invalid cursor param.");
		return -1;
	}

	ssize_t CharPos = 0;
	auto Visit = [&](auto &&Self, Block *b) -> bool
	{
		if (b->blocks.Length())
		{
			for (auto Child : b->blocks)
				if (Self(Self, Child))
					return true;
			return false;
		}

		if (c->Blk == b)
			return true;

		CharPos += b->Length();
		return false;
	};

	for (auto b : Blocks)
		if (Visit(Visit, b))
			return CharPos + c->Offset;
		
	LgiTrace("%s:%i - IndexOfCursor failed.\n", _FL);
	return -1;
}

LPoint LRichTextPriv::ScreenToDoc(int x, int y)
{
	LRect &Content = Areas[LRichTextEdit::ContentArea];
	return LPoint(x - Content.x1, y - Content.y1 + ScrollOffsetPx);
}

LPoint LRichTextPriv::DocToScreen(int x, int y)
{
	LRect &Content = Areas[LRichTextEdit::ContentArea];
	return LPoint(x + Content.x1, y + Content.y1 - ScrollOffsetPx);
}

bool LRichTextPriv::Merge(Transaction *Trans, Block *a, Block *b)
{
	TextBlock *ta = dynamic_cast<TextBlock*>(a);
	TextBlock *tb = dynamic_cast<TextBlock*>(b);
	if (!ta || !tb || a == b || a->parent != b->parent)
		return false;

	auto Parent = b->parent;
	auto &Siblings = Parent ? Parent->blocks : Blocks;
	ssize_t Index = Siblings.IndexOf(b);
	if (Index <= 0 || Siblings[Index - 1] != a)
		return false;

	LAutoPtr<MultiBlockState> State;
	if (Parent && Trans)
	{
		auto Root = Parent;
		while (Root->parent)
			Root = Root->parent;
		ssize_t RootIndex = Blocks.IndexOf(Root);
		if (RootIndex < 0 || Root->IsBusy())
			return false;
		State.Reset(new MultiBlockState(this, RootIndex, true));
		if (!State->Copy(RootIndex))
			return Error(_FL, "Could not save nested merge for undo.");
		State->Length = 1;
	}

	ssize_t Join = ta->Len;
	ta->Txt.Add(tb->Txt);
	ta->LayoutDirty = true;
	ta->Len += tb->Len;
	tb->Txt.Length(0);
	tb->Len = 0;

	Siblings.DeleteAt(Index, true);
	if (Parent)
	{
		for (auto c : {Cursor.Get(), Selection.Get()})
			if (c && c->Blk == b)
				c->Set(a, Join + c->Offset, -1);
		delete b;
		if (State)
			Trans->Add(State.Release());
	}

	Dirty = true;

	return true;
}

LSurface *LEmojiImage::GetEmojiImage()
{
	if (!EmojiImg)
	{
		LString a;

		// This is more for debug builds:
		char File[MAX_PATH_LEN] = "";
		LMakePath(File, sizeof(File), __FILE__, "../../../Text/Emoji/EmojiMap.png");
		

		// And this is release builds:
		if (LFileExists(File))
			a = File;
		else
			a = LFindFile("EmojiMap.png");
		
		if (a)
			EmojiImg.Reset(GdcD->Load(a, false));
	}

	return EmojiImg;
}

ssize_t LRichTextPriv::HitTest(int x, int y, int &LineHint, Block **Blk, ssize_t *BlkOffset)
{
	ssize_t CharPos = 0;
	HitTestResult r(x, y);

	if (Blocks.Length() == 0)
	{
		Error(_FL, "No blocks.");
		return -1;
	}

	Block *First = nullptr;
	Block *Last = nullptr;
	ssize_t TotalLength = 0;
	auto FindEdges = [&](auto &&Self, Block *b) -> void
	{
		if (b->blocks.Length())
		{
			for (auto Child : b->blocks)
				Self(Self, Child);
			return;
		}

		if (!First)
			First = b;
		Last = b;
		TotalLength += b->Length();
	};
	for (auto b : Blocks)
		FindEdges(FindEdges, b);

	if (!First || !Last)
		return -1;

	auto ReturnCursor = [&](Block *b, ssize_t Offset, int Hint, ssize_t Index)
	{
		LineHint = Hint;
		if (Blk)
			*Blk = b;
		if (BlkOffset)
			*BlkOffset = Offset;
		return Index;
	};

	if (y < First->GetPos().y1)
	{
		return ReturnCursor(First, 0, 0, 0);
	}

	CharPos = 0;
	// Table cells whose box contains the point but whose text doesn't (empty space
	// above or below the text) map to the nearest text in that cell
	Block *CellAfterBlk = nullptr;
	ssize_t CellAfterPos = 0;
	Block *CellBeforeBlk = nullptr;
	ssize_t CellBeforePos = 0;
	auto Test = [&](auto &&Self, Block *b, TdBlock *Cell) -> bool
	{
		if (b->blocks.Length())
		{
			TdBlock *Td = dynamic_cast<TdBlock*>(b);
			for (auto Child : b->blocks)
				if (Self(Self, Child, Td ? Td : Cell))
					return true;
			return false;
		}

		// Cells share a row, so a cell's text must not claim points in a neighbouring cell
		bool InCell = !Cell ||
					Cell->CellBox.X() <= 0 ||
					(r.In.x >= Cell->CellBox.x1 && r.In.x <= Cell->CellBox.x2);
		LRect p = b->GetPos();
		if (InCell && b->HitTest(r))
		{
			r.Blk = b;
			return true;
		}

		if (InCell && y >= p.y1 && y <= p.y2)
			Error(_FL, "Block failed to hit, pos=%s, y=%i.", p.GetStr(), y);

		if (Cell && InCell && Cell->CellBox.X() > 0 &&
			y >= Cell->CellBox.y1 && y <= Cell->CellBox.y2)
		{
			if (y > p.y2)
			{
				CellAfterBlk = b;
				CellAfterPos = CharPos;
			}
			else if (y < p.y1 && !CellBeforeBlk)
			{
				CellBeforeBlk = b;
				CellBeforePos = CharPos;
			}
		}

		CharPos += b->Length();
		return false;
	};
	for (auto b : Blocks)
	{
		if (Test(Test, b, nullptr))
			return ReturnCursor(r.Blk, r.Idx, r.LineHint, CharPos + r.Idx);
	}

	if (CellAfterBlk)
		return ReturnCursor(CellAfterBlk, CellAfterBlk->Length(), CellAfterBlk->GetLines() - 1, CellAfterPos + CellAfterBlk->Length());
	if (CellBeforeBlk)
		return ReturnCursor(CellBeforeBlk, 0, 0, CellBeforePos);

	LRect rc = Last->GetPos();
	if (y > rc.y2)
		return ReturnCursor(Last, Last->Length(), Last->GetLines() - 1, TotalLength);
		
	return -1;
}
	
bool LRichTextPriv::CursorFromPos(int x, int y, LAutoPtr<BlockCursor> *Cursor, ssize_t *GlobalIdx)
{
	int LineHint = -1;
	ssize_t Offset = -1;
	Block *b = nullptr;
	ssize_t Index = HitTest(x, y, LineHint, &b, &Offset);
	if (Index < 0 || !b)
		return false;

	if (Cursor)
		Cursor->Reset(new BlockCursor(b, Offset, LineHint));
	if (GlobalIdx)
		*GlobalIdx = Index;
	return true;
}

LRichTextPriv::Block *LRichTextPriv::GetBlockByIndex(ssize_t Index, ssize_t *Offset, int *BlockIdx, int *LineCount)
{
	if (Index < 0)
		return nullptr;

	ssize_t CharPos = 0;
	int Lines = 0;
	ssize_t LeafIndex = 0;
	Block *Found = nullptr;
	ssize_t FoundOffset = 0;
	int FoundLines = 0;
	Block *Last = nullptr;
	ssize_t LastLength = 0;
	int LastLines = 0;
	auto Visit = [&](auto &&Self, Block *b) -> void
	{
		if (Found)
			return;

		if (b->blocks.Length())
		{
			for (auto Child : b->blocks)
				Self(Self, Child);
			return;
		}

		ssize_t Len = b->Length();
		Last = b;
		LastLength = Len;
		LastLines = Lines;

		if (Index >= CharPos && Index < CharPos + Len)
		{
			Found = b;
			FoundOffset = Index - CharPos;
			FoundLines = Lines;
			return;
		}

		CharPos += Len;
		Lines += b->GetLines();
		LeafIndex++;
	};
	for (auto b : Blocks)
		Visit(Visit, b);

	if (!Found)
	{
		Found = Last;
		FoundOffset = LastLength;
		FoundLines = LastLines;
	}

	if (!Found)
		return nullptr;
	if (Offset)
		*Offset = FoundOffset;
	if (BlockIdx)
		*BlockIdx = (int)LeafIndex;
	if (LineCount)
		*LineCount = FoundLines;
	return Found;
}

LRichTextPriv::Block *LRichTextPriv::GetBlockByLine(ssize_t Line, ssize_t *Offset, int *LineHint)
{
	if (Line < 0)
		return nullptr;

	int Lines = 0;
	Block *Found = nullptr;
	ssize_t FoundOffset = 0;
	int FoundLineHint = 0;
	Block *Last = nullptr;
	ssize_t LastOffset = 0;
	int LastLine = 0;
	auto Visit = [&](auto &&Self, Block *b) -> void
	{
		if (Found)
			return;

		if (b->blocks.Length())
		{
			for (auto Child : b->blocks)
				Self(Self, Child);
			return;
		}

		int BlockLines = MAX(b->GetLines(), 1);
		Last = b;
		LastOffset = b->LineToOffset(BlockLines - 1);
		LastLine = BlockLines - 1;

		if (Line >= Lines && Line < Lines + BlockLines)
		{
			Found = b;
			FoundLineHint = (int)(Line - Lines);
			FoundOffset = b->LineToOffset(FoundLineHint);
			return;
		}

		Lines += BlockLines;
	};
	for (auto b : Blocks)
		Visit(Visit, b);

	if (!Found)
	{
		Found = Last;
		FoundOffset = LastOffset;
		FoundLineHint = LastLine;
	}
	if (!Found)
		return nullptr;
	if (Offset)
		*Offset = FoundOffset;
	if (LineHint)
		*LineHint = FoundLineHint;
	return Found;
}

int LRichTextPriv::GetTreeLineCount()
{
	int Lines = 0;
	auto Visit = [&](auto &&Self, Block *b) -> void
	{
		if (b->blocks.Length())
		{
			for (auto Child : b->blocks)
				Self(Self, Child);
		}
		else
			Lines += b->GetLines();
	};
	for (auto b : Blocks)
		Visit(Visit, b);
	return Lines;
}

bool LRichTextPriv::GetLineOfCursor(BlockCursor *Cursor, int &Line)
{
	if (!Cursor || !Cursor->Blk)
		return false;

	int Lines = 0;
	bool Found = false;
	auto Visit = [&](auto &&Self, Block *b) -> void
	{
		if (Found)
			return;

		if (b->blocks.Length())
		{
			for (auto Child : b->blocks)
				Self(Self, Child);
		}
		else if (b == Cursor->Blk)
		{
			LArray<int> BlockLines;
			if (!b->OffsetToLine(Cursor->Offset, nullptr, &BlockLines) || !BlockLines.Length())
				return;

			Line = Lines + (Cursor->LineHint >= 0
				? Cursor->LineHint
				: BlockLines.First());
			Found = true;
		}
		else
			Lines += b->GetLines();
	};
	for (auto b : Blocks)
		Visit(Visit, b);
	return Found;
}
	
bool LRichTextPriv::Layout(LScrollBar *&ScrollY)
{
	Flow f(this);
	
	ScrollLinePx = View->GetFont()->GetHeight();
	LAssert(ScrollLinePx > 0);
	if (ScrollLinePx <= 0)
		ScrollLinePx = 16;

	LRect Client = Areas[LRichTextEdit::ContentArea];
	Client.Offset(-Client.x1, -Client.y1);
	DocumentExtent.x = Client.X();

	LCssTools Ct(this, View->GetUiFont());
	LRect Content = Ct.ApplyPadding(Client);
	f.Left = Content.x1;
	f.Right = Content.x2;
	f.Top = f.CurY = Content.y1;

	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		b->OnLayout(f);

		if ((f.CurY > Client.Y()) && ScrollY==NULL && !ScrollChange)
		{
			// We need to add a scroll bar
			View->SetScrollBars(false, true);
			View->Invalidate();
			ScrollChange = true;
			return false;
		}
	}
	DocumentExtent.y = f.CurY + (Client.y2 - Content.y2);

	if (ScrollY)
	{
		int Lines = (f.CurY + ScrollLinePx - 1) / ScrollLinePx;
		int PageLines = (Client.Y() + ScrollLinePx - 1) / ScrollLinePx;
		ScrollY->SetPage(PageLines);
		ScrollY->SetRange(Lines);
	}
		
	for (auto endpoint : {Cursor.Get(), Selection.Get()})
	{
		if (!endpoint)
			continue;
		LAssert(endpoint->Blk != NULL);
		if (endpoint->Blk)
			endpoint->Blk->GetPosFromIndex(endpoint);
	}

	return true;
}

void LRichTextPriv::OnStyleChange(LRichTextEdit::RectType t)
{
	LCss s;
	switch (t)
	{
		case LRichTextEdit::FontFamilyBtn:
		{
			LCss::FontFamilies Fam(Values[t].Str());
			s.FontFamily(Fam);
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		case LRichTextEdit::FontSizeBtn:
		{
			double Pt = Values[t].CastDouble();
			s.FontSize(LCss::Len(LCss::LenPt, (float)Pt));
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		case LRichTextEdit::BoldBtn:
		{
			s.FontWeight(LCss::FontWeightBold);
			if (!ChangeSelectionStyle(&s, Values[t].CastBool()))
				StyleDirty.Add(t);
			break;
		}
		case LRichTextEdit::ItalicBtn:
		{
			s.FontStyle(LCss::FontStyleItalic);
			if (!ChangeSelectionStyle(&s, Values[t].CastBool()))
				StyleDirty.Add(t);
			break;
		}
		case LRichTextEdit::UnderlineBtn:
		{
			s.TextDecoration(LCss::TextDecorUnderline);
			if (ChangeSelectionStyle(&s, Values[t].CastBool()))
				StyleDirty.Add(t);
			break;
		}
		case LRichTextEdit::ForegroundColourBtn:
		{
			s.Color(LCss::ColorDef(LCss::ColorRgb, (uint32_t) Values[t].Value.Int64));
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		case LRichTextEdit::BackgroundColourBtn:
		{
			s.BackgroundColor(LCss::ColorDef(LCss::ColorRgb, (uint32_t) Values[t].Value.Int64));
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		default:
			break;
	}
}

bool LRichTextPriv::ChangeSelectionStyle(LCss *Style, bool Add)
{
	if (!Selection)
		return false;

	LAutoPtr<Transaction> Trans(new Transaction);
	bool Cf = CursorFirst();
	LRichTextPriv::BlockCursor *Start = Cf ? Cursor : Selection;
	LRichTextPriv::BlockCursor *End = Cf ? Selection : Cursor;
	if (Start->Blk == End->Blk)
	{
		// Change style in the same block...
		ssize_t Len = End->Offset - Start->Offset;
		if (!Start->Blk->ChangeStyle(Trans, Start->Offset, Len, Style, Add))
			return false;
	}
	else
	{
		// Multi-block style change...

		// 1) Change style on the content to the end of the first block
		Start->Blk->ChangeStyle(Trans, Start->Offset, -1, Style, Add);

		// 2) Change style on blocks between 'Start' and 'End'
		ssize_t i = Blocks.IndexOf(Start->Blk);
		if (i >= 0)
		{
			for (++i; Blocks[i] != End->Blk && i < (int)Blocks.Length(); i++)
			{
				LRichTextPriv::Block *&b = Blocks[i];
				if (!b->ChangeStyle(Trans, 0, -1, Style, Add))
					return false;
			}
		}
		else
		{
			return Error(_FL, "Start block has no index.");
		}

		// 3) Change style up to the Cursor in the 'End' block
		if (!End->Blk->ChangeStyle(Trans, 0, End->Offset, Style, Add))
			return false;
	}

	Cursor->Pos.ZOff(-1, -1);
	InvalidateDoc(NULL);
	AddTrans(Trans);
	return true;
}

void LRichTextPriv::PaintBtn(LSurface *pDC, LRichTextEdit::RectType t)
{
	auto UiFont = View->GetUiFont();
	LRect r = Areas[t];
	LVariant &v = Values[t];
	bool Down = (v.Type == GV_BOOL && v.Value.Bool) ||
				(BtnState[t].IsPress && BtnState[t].Pressed && BtnState[t].MouseOver);

	auto backCol = BtnState[t].MouseOver ? L_LIGHT : L_MED;
	UiFont->Colour(L_TEXT, backCol);
	UiFont->Transparent(false);

	LColour Low(96, 96, 96);
	pDC->Colour(Down ? LColour::White : Low);
	pDC->Line(r.x1, r.y2, r.x2, r.y2);
	pDC->Line(r.x2, r.y1, r.x2, r.y2);
	pDC->Colour(Down ? Low : LColour::White);
	pDC->Line(r.x1, r.y1, r.x2, r.y1);
	pDC->Line(r.x1, r.y1, r.x1, r.y2);
	r.Inset(1, 1);
	
	switch (v.Type)
	{
		case GV_STRING:
		{
			LDisplayString Ds(UiFont, v.Str());
			Ds.Draw(pDC,
					r.x1 + ((r.X()-Ds.X())>>1) + Down,
					r.y1 + ((r.Y()-Ds.Y())>>1) + Down,
					&r);
			break;
		}
		case GV_INT64:
		{
			if (v.Value.Int64)
			{
				pDC->Colour((uint32_t)v.Value.Int64, 32);
				pDC->Rectangle(&r);
			}
			else
			{
				// Transparent
				int g[2] = { 128, 192 };
				pDC->ClipRgn(&r);
				for (int y=0; y<r.Y(); y+=2)
				{
					for (int x=0; x<r.X(); x+=2)
					{
						int i = ((y >> 1) % 2) ^ ((x >> 1) % 2);
						pDC->Colour(LColour(g[i], g[i], g[i]));
						pDC->Rectangle(r.x1+x, r.y1+y, r.x1+x+1, r.y1+y+1);
					}
				}
				pDC->ClipRgn(nullptr);
			}
			break;
		}
		case GV_BOOL:
		{
			const char *Label = nullptr;
			switch (t)
			{
				case LRichTextEdit::BoldBtn: Label = "B"; break;
				case LRichTextEdit::ItalicBtn: Label = "I"; break;
				case LRichTextEdit::UnderlineBtn: Label = "U"; break;
				default:
					break;
			}
			if (!Label) break;
			LDisplayString Ds(UiFont, Label);
			Ds.Draw(pDC,
					r.x1 + ((r.X()-Ds.X())>>1) + Down,
					r.y1 + ((r.Y()-Ds.Y())>>1) + Down,
					&r);
			break;
		}
		case GV_LSURFACE:
		{
			pDC->Colour(backCol);
			pDC->Rectangle(&r);
			pDC->Op(GDC_ALPHA);
			if (auto img = v.Value.Surface.Ptr)
			{
				pDC->Blt(r.x1 + Down, r.y1 + Down, img);
			}
			break;
		}
		default:
		{
			pDC->Colour(backCol);
			pDC->Rectangle(&r);
			pDC->Colour(LColour::Red);
			pDC->Line(r.x1, r.y1, r.x2, r.y2);
			break;
		}
	}
}

bool LRichTextPriv::MakeLink(TextBlock *tb, ssize_t Offset, ssize_t Len, LString Link)
{
	if (!tb)
		return false;

	LArray<StyleText*> st;
	if (!tb->GetTextAt(Offset, st))
		return false;
	
	LAutoPtr<LNamedStyle> ns(new LNamedStyle);
	if (ns)
	{
		if (st.Last()->GetStyle())
			*ns = *st.Last()->GetStyle();
		ns->TextDecoration(LCss::TextDecorUnderline);
		ns->Color(LCss::ColorDef(LCss::ColorRgb, LColour::Blue.c32()));
		
		LAutoPtr<Transaction> Trans(new Transaction);

		tb->ChangeStyle(Trans, Offset, Len, ns, true);

		if (tb->GetTextAt(Offset + 1, st))
		{
			st.First()->Element = TAG_A;
			st.First()->Param = Link;
		}

		AddTrans(Trans);
	}

	return true;
}

// Converts the root level text blocks touched by the cursor/selection into a bullet list,
// or turns the list containing the cursor back into plain paragraphs.
bool LRichTextPriv::ToggleList(LCss::ListStyleTypes Type)
{
	if (!Cursor || !Cursor->Blk)
		return false;

	auto RootOf = [](Block *b)
	{
		while (b && b->parent)
			b = b->parent;
		return b;
	};

	ssize_t First = Blocks.IndexOf(RootOf(Cursor->Blk));
	ssize_t Last = First;
	if (Selection && Selection->Blk)
	{
		ssize_t Sel = Blocks.IndexOf(RootOf(Selection->Blk));
		if (Sel >= 0)
		{
			Last = MAX(First, Sel);
			First = MIN(First, Sel);
		}
	}
	if (First < 0 || Last < First)
		return false;

	auto CurList = dynamic_cast<ListBlock*>(Blocks[First]);
	bool Unwrap = CurList != NULL;
	if (Unwrap)
		Last = First; // Only one list is un-wrapped at a time
	else
	{
		for (ssize_t i = First; i <= Last; i++)
			if (!dynamic_cast<TextBlock*>(Blocks[i]))
				return false;
	}

	AutoTrans Trans(new Transaction);
	if (!Trans)
		return false;

	ssize_t Count = Last - First + 1;
	auto State = new MultiBlockState(this, First);
	if (!State)
		return false;
	for (ssize_t i = First; i <= Last; i++)
	{
		if (!State->Copy(i))
		{
			DeleteObj(State);
			return false;
		}
	}

	auto CursorState = new BlockCursorChange(true, Cursor);
	auto SelectionState = Selection ? new BlockCursorChange(false, Selection) : NULL;

	bool Switched = false;
	if (Unwrap && (CurList->GetListType() == Type || (CurList->IsOrdered() != (Type != LCss::ListDisc))))
		; // Same kind of list: unwrap below
	else if (Unwrap)
	{
		// Different kind of list: just switch the type
		CurList->SetListType(Type);
		for (auto b : CurList->blocks)
			if (auto Tb = dynamic_cast<TextBlock*>(b))
				Tb->LayoutDirty = true;
		State->Length = 1;
		Unwrap = false;
		Switched = true;
	}

	if (Unwrap)
	{
		LArray<Block*> Items;
		for (unsigned i = 0; i < CurList->blocks.Length(); i++)
		{
			Block *b = CurList->blocks[i];
			b->parent = NULL;
			Items.Add(b);
		}
		CurList->blocks.Length(0);
		Blocks.DeleteAt(First, true);
		delete CurList;
		for (unsigned i = 0; i < Items.Length(); i++)
		{
			Blocks.AddAt(First + i, Items[i]);
			if (auto Tb = dynamic_cast<TextBlock*>(Items[i]))
				Tb->LayoutDirty = true;
		}
		State->Length = Items.Length();
	}
	else if (!Switched)
	{
		auto List = new ListBlock(this, Type);
		if (!List)
		{
			DeleteObj(State);
			DeleteObj(CursorState);
			DeleteObj(SelectionState);
			return false;
		}

		auto Tb = dynamic_cast<TextBlock*>(Blocks[First]);
		if (!Selection && Count == 1 && Tb && Tb->Length() > 0 && Cursor->Blk == Tb)
		{
			// Split the block at the cursor and insert a new list item into the gap
			ssize_t Offset = Cursor->Offset;
			bool Splitting = Offset > 0 && Offset < Tb->Length();
			TextBlock *Before = Offset > 0 ? Tb : NULL;
			TextBlock *After = Offset >= Tb->Length() ? NULL : Offset > 0 ?
								dynamic_cast<TextBlock*>(Tb->Split(Trans, Offset)) : Tb;
			if (Splitting && !After)
			{
				DeleteObj(List);
				DeleteObj(State);
				DeleteObj(CursorState);
				DeleteObj(SelectionState);
				return false;
			}

			auto Item = new TextBlock(this);
			if (!Item)
			{
				DeleteObj(List);
				DeleteObj(State);
				DeleteObj(CursorState);
				DeleteObj(SelectionState);
				return false;
			}
			if (auto Style = Tb->GetStyle())
				Item->SetStyle(Style);
			List->Add(Item);

			if (Before && After)
				Before->StripLast(NULL, "\n");

			ssize_t Idx = First;
			Blocks.DeleteAt(Idx, true);
			if (Before)
				Blocks.AddAt(Idx++, Before);
			Blocks.AddAt(Idx++, List);
			if (After && After != Before)
				Blocks.AddAt(Idx++, After);
			State->Length = Idx - First;
			Cursor->Set(Item, 0, 0);
			if (Before)
				Before->LayoutDirty = true;
			if (After)
				After->LayoutDirty = true;
			Count = 0;
		}

		for (ssize_t i = 0; i < Count; i++)
		{
			Block *b = Blocks[First];
			Blocks.DeleteAt(First, true);
			List->Add(b);
			if (auto Tb = dynamic_cast<TextBlock*>(b))
				Tb->LayoutDirty = true;
		}
		if (Count)
		{
			Blocks.AddAt(First, List);
			State->Length = 1;
		}
	}

	Trans->Add(State);
	Trans->Add(CursorState);
	if (SelectionState)
		Trans->Add(SelectionState);

	AddTrans(Trans);
	InvalidateDoc(NULL);
	View->SendNotify(LNotifyDocChanged);
	return true;
}

bool LRichTextPriv::ClickBtn(LMouse &m, LRichTextEdit::RectType t)
{
	switch (t)
	{
		case LRichTextEdit::FontFamilyBtn:
		{
			LString::Array Fonts;
			if (!LFontSystem::Inst()->EnumerateFonts(Fonts))
				return Error(_FL, "EnumerateFonts failed.");

			bool UseSub = (LSysFont->GetHeight() * Fonts.Length()) > (GdcD->Y() * 0.8);

			LSubMenu s;
			LSubMenu *Cur = NULL;
			int Idx = 1;
			char Last = 0;
			for (unsigned i=0; i<Fonts.Length(); i++)
			{
				LString &f = Fonts[i];
				if (f(0) == '@')
				{
					Fonts.DeleteAt(i--);
				}
				else if (UseSub)
				{
					if (f(0) != Last || Cur == NULL)
					{
						LString str;
						str.Printf("%c...", Last = f(0));
						Cur = s.AppendSub(str);
					}
					if (Cur)
						Cur->AppendItem(f, Idx++);
					else
						break;
				}
				else
				{
					s.AppendItem(f, Idx++);
				}
			}

			LPoint p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			int Result = s.Float(View, p.x, p.y, true);
			if (Result)
			{
				Values[t] = Fonts[Result-1];
				View->Invalidate(Areas+t);
				OnStyleChange(t);
			}
			break;
		}
		case LRichTextEdit::FontSizeBtn:
		{
			static const char *Sizes[] = { "6", "7", "8", "9", "10", "11", "12", "14", "16", "18", "20", "24",
											"28", "32", "40", "50", "60", "80", "100", "120", 0 };
			LSubMenu s;
			for (int Idx = 0; Sizes[Idx]; Idx++)
				s.AppendItem(Sizes[Idx], Idx+1);

			LPoint p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			int Result = s.Float(View, p.x, p.y, true);
			if (Result)
			{
				Values[t] = Sizes[Result-1];
				View->Invalidate(Areas+t);
				OnStyleChange(t);
			}
			break;
		}
		case LRichTextEdit::BoldBtn:
		case LRichTextEdit::ItalicBtn:
		case LRichTextEdit::UnderlineBtn:
		{
			Values[t] = !Values[t].CastBool();
			View->Invalidate(Areas+t);
			OnStyleChange(t);
			break;
		}
		case LRichTextEdit::BulletsBtn:
		{
			ToggleList(LCss::ListDisc);
			break;
		}
		case LRichTextEdit::NumberedListBtn:
		{
			ToggleList(LCss::ListDecimal);
			break;
		}
		case LRichTextEdit::ForegroundColourBtn:
		case LRichTextEdit::BackgroundColourBtn:
		{
			LPoint p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			new SelectColour(this, p, t);
			break;
		}
		case LRichTextEdit::MakeLinkBtn:
		{
			if (!Cursor || !Cursor->Blk)
				break;

			TextBlock *tb = dynamic_cast<TextBlock*>(Cursor->Blk);
			if (!tb)
				break;

			LArray<StyleText*> st;
			if (!tb->GetTextAt(Cursor->Offset, st))
				break;

			StyleText *a = st.Length() > 1 && st[1]->Element == TAG_A ? st[1] : st.First()->Element == TAG_A ? st[0] : NULL;
			if (a)
			{
				// Edit the existing link...
				auto i = new LInput(View, a->Param, "Edit link:", "Link");
				i->DoModal([this, i, a](auto dlg, auto ok)
				{
					if (ok)
						a->Param = i->GetStr();
				});
			}
			else if (Selection)
			{
				// Turn current selection into link
				auto i = new LInput(View, NULL, "Edit link:", "Link");
				i->DoModal([this, i, tb](auto dlg, auto ok)
				{
					if (ok)
					{
						BlockCursor *Start = CursorFirst() ? Cursor : Selection;
						BlockCursor *End = CursorFirst() ? Selection : Cursor;
						if (!Start || !End)
							;
						else if (Start->Blk != End->Blk)
							LgiMsg(View, "Selection too large.", "Error");
						else
							MakeLink(tb,
									Start->Offset,
									End->Offset - Start->Offset,
									i->GetStr());
					}
				});
			}
			break;
		}
		case LRichTextEdit::RemoveLinkBtn:
		{
			if (!Cursor || !Cursor->Blk)
				break;

			TextBlock *tb = dynamic_cast<TextBlock*>(Cursor->Blk);
			if (!tb)
				break;

			LArray<StyleText*> st;
			if (!tb->GetTextAt(Cursor->Offset, st))
				break;

			auto a = st.Length() > 1 && st[1]->Element == TAG_A ? st[1] : st.First()->Element == TAG_A ? st[0] : NULL;
			if (a)
			{
				// Remove the existing link...
				a->Element = CONTENT;
				a->Param.Empty();

				LAutoPtr<LCss> s(new LCss);
				LNamedStyle *Ns = a->GetStyle();
				if (Ns)
					*s = *Ns;
				if (s->TextDecoration() == LCss::TextDecorUnderline)
					s->DeleteProp(LCss::PropTextDecoration);
				if ((LColour)s->Color() == LColour::Blue)
					s->DeleteProp(LCss::PropColor);

				Ns = AddStyleToCache(s);
				a->SetStyle(Ns);

				tb->LayoutDirty = true;
				InvalidateDoc(NULL);
			}
			break;			
		}
		case LRichTextEdit::RemoveStyleBtn:
		{
			LCss s;
			ChangeSelectionStyle(&s, false);
			break;
		}
		case LRichTextEdit::EmojiBtn:
		{
			LPoint p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			new EmojiMenu(this, p);
			break;
		}
		case LRichTextEdit::HorzRuleBtn:
		{
			InsertHorzRule();
			break;
		}
		case LRichTextEdit::UndoBtn:
		{
			View->Undo();
			break;
		}
		case LRichTextEdit::RedoBtn:
		{
			View->Redo();
			break;
		}
		default:
			return false;
	}

	return true;
}

bool LRichTextPriv::BeginNestedEdit(Transaction *Trans, Block *b, LAutoPtr<Transaction> &Scratch, Transaction *&Edit)
{
	Edit = Trans;
	if (!Trans || !b->parent)
		return true;

	auto Root = b;
	while (Root->parent)
		Root = Root->parent;
	ssize_t RootIndex = Blocks.IndexOf(Root);
	if (RootIndex < 0 || Root->IsBusy())
		return false;

	LAutoPtr<MultiBlockState> State(new MultiBlockState(this, RootIndex, true));
	if (!State->Copy(RootIndex))
		return Error(_FL, "Could not save nested edit for undo.");
	State->Length = 1;
	Trans->Add(State.Release());

	Scratch.Reset(new Transaction);
	Edit = Scratch.Get();
	return true;
}

bool LRichTextPriv::InsertBlocksAt(Transaction *Edit, Block *b, ssize_t Offset, LArray<Block*> &New)
{
	auto Parent = b->parent;
	auto &Siblings = Parent ? Parent->blocks : Blocks;
	ssize_t Idx = Siblings.IndexOf(b);
	if (Idx < 0)
		return false;

	Block *After = NULL;
	ssize_t At = Idx;
	if (Offset > 0)
	{
		if (Offset < b->Length())
		{
			After = b->Split(Edit, Offset);
			if (!After)
				return false;
			// Split doesn't set the parent of the new block
			After->parent = Parent;
		}
		At = Idx + 1;
	}

	for (auto n : New)
	{
		n->parent = Parent;
		Siblings.AddAt(At++, n);
	}
	if (After)
		Siblings.AddAt(At, After);
	return true;
}

bool LRichTextPriv::InsertHorzRule()
{
	if (!Cursor || !Cursor->Blk)
		return false;

	TextBlock *tb = dynamic_cast<TextBlock*>(Cursor->Blk);
	if (!tb)
		return false;

	LAutoPtr<Transaction> Trans(new Transaction);
	LAutoPtr<Transaction> Scratch;
	Transaction *Edit = NULL;
	if (!BeginNestedEdit(Trans, tb, Scratch, Edit))
		return false;

	DeleteSelection(Edit, NULL);

	// Splitting leaves trailing whitespace on the first half, which the rule replaces
	bool SplitsText = Cursor->Offset > 0 && Cursor->Offset < tb->Length();

	HorzRuleBlock *Hr = new HorzRuleBlock(this);
	if (!Hr)
		return false;

	LArray<Block*> New;
	New.Add(Hr);
	if (!InsertBlocksAt(Edit, tb, Cursor->Offset, New))
	{
		delete Hr;
		return false;
	}
	if (SplitsText)
		tb->StripLast(Edit);

	AddTrans(Trans);
	InvalidateDoc(NULL);

	LAutoPtr<BlockCursor> c(new BlockCursor(Hr, 0, 0));
	return SetCursor(c);
}
	
void LRichTextPriv::Paint(LSurface *pDC, LScrollBar *&ScrollY)
{
	if (Areas[LRichTextEdit::ToolsArea].Valid())
	{
		// Draw tools area...
		auto UiFont = View->GetUiFont();
		LRect &t = Areas[LRichTextEdit::ToolsArea];
		#ifdef WIN32
		LDoubleBuffer Buf(pDC, &t);
		#endif
		LColour ToolBar = LColour(L_FOCUS_SEL_BACK).Mix(LColour(L_LOW));
		pDC->Colour(ToolBar);
		pDC->Rectangle(&t);

		LRect r = t;
		r.Inset(3, 3);
		#define AllocPx(sz, border) \
			LRect(r.x1, r.y1, r.x1 + (int)(sz) - 1, r.y2); r.x1 += (int)(sz) + border

		Areas[LRichTextEdit::FontFamilyBtn] = AllocPx(130, 6);
		Areas[LRichTextEdit::FontSizeBtn] = AllocPx(40, 6);

		Areas[LRichTextEdit::BoldBtn] = AllocPx(r.Y(), 0);
		Areas[LRichTextEdit::ItalicBtn] = AllocPx(r.Y(), 0);
		Areas[LRichTextEdit::UnderlineBtn] = AllocPx(r.Y(), 6);
		
		Areas[LRichTextEdit::BulletsBtn] = AllocPx(r.Y(), 0);
		Areas[LRichTextEdit::NumberedListBtn] = AllocPx(r.Y(), 6);

		Areas[LRichTextEdit::ForegroundColourBtn] = AllocPx(r.Y()*1.5, 0);
		Areas[LRichTextEdit::BackgroundColourBtn] = AllocPx(r.Y()*1.5, 6);

		{
			LDisplayString Ds(UiFont, TEXT_LINK);
			Areas[LRichTextEdit::MakeLinkBtn] = AllocPx(Ds.X() + 12, 0);
		}
		{
			LDisplayString Ds(UiFont, TEXT_REMOVE_LINK);
			Areas[LRichTextEdit::RemoveLinkBtn] = AllocPx(Ds.X() + 12, 6);
		}
		{
			LDisplayString Ds(UiFont, TEXT_REMOVE_STYLE);
			Areas[LRichTextEdit::RemoveStyleBtn] = AllocPx(Ds.X() + 12, 6);
		}

		for (unsigned int i=LRichTextEdit::EmojiBtn; i<LRichTextEdit::UndoBtn; i++)
		{
			LDisplayString Ds(UiFont, Values[i].Str());
			Areas[i] = AllocPx(Ds.X() + 12, 6);
		}

		Areas[LRichTextEdit::UndoBtn] = AllocPx(r.Y(), 0);
		Areas[LRichTextEdit::RedoBtn] = AllocPx(r.Y(), 6);

		for (unsigned i = LRichTextEdit::FontFamilyBtn; i < LRichTextEdit::MaxArea; i++)
		{
			PaintBtn(pDC, (LRichTextEdit::RectType) i);
		}

		auto verColour = ToolBar.Mix(LColour::Black, 0.3f);
		LDisplayString verDs(UiFont, "v2");
		UiFont->Fore(verColour);
		UiFont->Back(ToolBar);
		UiFont->Transparent(true);
		verDs.Draw(pDC, r.x1, r.y1);
	}

	LPoint Origin;
	
	LRect r = Areas[LRichTextEdit::ContentArea];
	#if defined(WINDOWS) && !DEBUG_NO_DOUBLE_BUF
		LMemDC Mem(_FL);
		if (!Mem.Create(r.X(), r.Y(), pDC->GetColourSpace()))
		{
			LAssert(!"MemDC creation failed.");
			return;
		}
		LSurface *pScreen = pDC;
		pDC = &Mem;
		r.Offset(-r.x1, -r.y1);
	#else
		pDC->GetOrigin(Origin.x, Origin.y);
		pDC->ClipRgn(&r);
	#endif

	ScrollOffsetPx = ScrollY ? (int)(ScrollY->Value() * ScrollLinePx) : 0;
	pDC->SetOrigin(Origin.x-r.x1, Origin.y-r.y1+ScrollOffsetPx);

	int DrawPx = ScrollOffsetPx + Areas[LRichTextEdit::ContentArea].Y();
	int ExtraPx = DrawPx > DocumentExtent.y ? DrawPx - DocumentExtent.y : 0;

	r.Set(0, 0, DocumentExtent.x-1, DocumentExtent.y-1);

	// Background colour
	#if DEBUG_COVERAGE_CHECK
	LColour Workspace(255, 0, 255);
	#else
	LCss::ColorDef cBack = BackgroundColor();
	LColour Workspace = cBack.IsValid() ? (LColour)cBack : LColour(L_WORKSPACE);
	#endif

	// Fill the padding...
	LCssTools ct(this, View->GetUiFont());
	r = ct.PaintPadding(pDC, r, &Workspace);

	// Fill the background...
	pDC->Colour(Workspace);
	pDC->Rectangle(&r);
	if (ExtraPx)
		pDC->Rectangle(0, DocumentExtent.y, DocumentExtent.x-1, DocumentExtent.y+ExtraPx);

	PaintContext Ctx;
	Ctx.pDC = pDC;
	Ctx.Cursor = Cursor;
	Ctx.Select = Selection;
	Ctx.Colours[Unselected].Fore.Set(L_TEXT);
	Ctx.Colours[Unselected].Back.Set(L_WORKSPACE);
		
	if (View->Focus())
	{
		Ctx.Colours[Selected].Fore.Set(L_FOCUS_SEL_FORE);
		Ctx.Colours[Selected].Back.Set(L_FOCUS_SEL_BACK);
	}
	else
	{
		Ctx.Colours[Selected].Fore.Set(L_NON_FOCUS_SEL_FORE);
		Ctx.Colours[Selected].Back.Set(L_NON_FOCUS_SEL_BACK);
	}
		
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		if (b)
		{
			b->OnPaint(Ctx);
			#if DEBUG_OUTLINE_BLOCKS
			pDC->Colour(LColour(192, 192, 192));
			pDC->LineStyle(LSurface::LineDot);
			pDC->Box(&b->GetPos());
			pDC->LineStyle(LSurface::LineSolid);
			#endif
		}
	}

	#ifdef _DEBUG
	pDC->Colour(LColour::Green);
	for (unsigned i=0; i<DebugRects.Length(); i++)
	{
		pDC->Box(&DebugRects[i]);
	}
	#endif

	#if 0 // Outline the line the cursor is on
	if (Cursor)
	{
		pDC->Colour(LColour::Blue);
		pDC->LineStyle(LSurface::LineDot);
		pDC->Box(&Cursor->Line);
	}
	#endif

	#if defined(WINDOWS) && !DEBUG_NO_DOUBLE_BUF
	Mem.SetOrigin(LPoint());
	pScreen->Blt(Areas[LRichTextEdit::ContentArea].x1, Areas[LRichTextEdit::ContentArea].y1, &Mem);
	#else
	pDC->ClipRgn(NULL);
	#endif
}
	
LHtmlElement *LRichTextPriv::CreateElement(LHtmlElement *Parent)
{
	return new LRichEditElem(Parent);
}

bool LRichTextPriv::ToHtml(LArray<LDocView::ContentMedia> *Media, BlockCursor *From, BlockCursor *To)
{
	UtfNameCache.Reset();
	if (!Blocks.Length())
		return false;

	LStringPipe p(256);

	p.Print("<html>\n"
			"<head>\n"
			"\t<meta name=\"charset\" content=\"utf-8\">\n");
	
	ZeroRefCounts();

	auto GetRoot = [](Block *b)
	{
		while (b && b->parent)
			b = b->parent;
		return b;
	};
	auto OffsetInTree = [&](auto &&Self, Block *b, Block *Target, ssize_t Offset, ssize_t &Result) -> bool
	{
		if (b == Target)
		{
			Result = Offset;
			return true;
		}

		ssize_t Pos = 0;
		for (auto Child : b->blocks)
		{
			if (Self(Self, Child, Target, Offset, Result))
			{
				Result += Pos;
				return true;
			}
			Pos += Child->Length();
		}
		return false;
	};

	Block *StartRoot = From ? GetRoot(From->Blk) : Blocks.First();
	Block *EndRoot = To ? GetRoot(To->Blk) : Blocks.Last();
	ssize_t Start = Blocks.IndexOf(StartRoot);
	ssize_t End = Blocks.IndexOf(EndRoot);
	ssize_t StartIdx = 0;
	ssize_t EndIdx = EndRoot ? EndRoot->Length() : 0;
	if (From && !OffsetInTree(OffsetInTree, StartRoot, From->Blk, From->Offset, StartIdx))
		return Error(_FL, "Could not resolve HTML selection start in the block tree.");
	if (To && !OffsetInTree(OffsetInTree, EndRoot, To->Blk, To->Offset, EndIdx))
		return Error(_FL, "Could not resolve HTML selection end in the block tree.");
	if (Start < 0 || End < Start)
		return Error(_FL, "HTML selection blocks are not in document order.");

	for (ssize_t i=Start; i<=End; i++)
		Blocks[i]->IncAllStyleRefs();

	if (GetStyles())
	{
		p.Print("\t<style>\n");
		OutputStyles(p, 1);		
		p.Print("\t</style>\n");
	}

	p.Print("</head>\n"
			"<body>\n");
		
	for (ssize_t i=Start; i<=End; i++)
	{
		Block *b = Blocks[i];
		ssize_t RangeStart = i == Start ? StartIdx : 0;
		ssize_t RangeEnd = i == End ? EndIdx : b->Length();
		LRange r(RangeStart, MAX(RangeEnd - RangeStart, 0));
		b->ToHtml(p, Media, (From || To) ? &r : NULL);
	}
		
	p.Print("</body>\n</html>\n");
	LAutoString a(p.NewStr());
	UtfNameCache = a;
	return UtfNameCache.Get() != NULL;
}
	
void LRichTextPriv::DumpBlocks()
{
	LgiTrace("LRichTextPriv Blocks=%i\n", Blocks.Length());
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		LgiTrace("%p::%s style=%p/%s {\n",
			b,
			b->GetClass(),
			b->GetStyle(),
			b->GetStyle() ? b->GetStyle()->Name.Get() : NULL);
		b->Dump();
		LgiTrace("}\n");
	}
}

struct HtmlElementCb : public LCss::ElementCallback<LHtmlElement>
{
	const char *GetElement(LHtmlElement *obj)
	{
		return obj->Tag;
	}

	const char *GetAttr(LHtmlElement *obj, const char *Attr)
	{
		const char *Val = NULL;
		return obj->Get(Attr, Val) ? Val : NULL;
	}

	bool GetClasses(LString::Array &Classes, LHtmlElement *obj)
	{
		const char *Cls = NULL;
		if (!obj->Get("class", Cls))
			return false;

		Classes = LString(Cls).SplitDelimit();
		return true;
	}

	LHtmlElement *GetParent(LHtmlElement *obj)
	{
		return obj->Parent;
	}

	LArray<LHtmlElement*> GetChildren(LHtmlElement *obj)
	{
		return obj->Children;
	}
};

static LCss *EnsureStyle(LAutoPtr<LCss> &Style)
{
	if (!Style.Get())
		Style.Reset(new LCss);
	return Style.Get();
}

static bool IsCellTag(HtmlTag t)
{
	return t == TAG_TD || t == TAG_TH;
}

static bool IsTableContextTag(HtmlTag t)
{
	switch (t)
	{
		case TAG_TABLE:
		case TAG_TR:
		case TAG_TD:
		case TAG_TH:
		case TAG_TBODY:
			return true;
		default:
			return false;
	}
}

static int ParseAttrInt(const char *s)
{
	int v = 0;
	bool neg = false;
	if (!s)
		return 0;
	while (*s && *s != '-' && (*s < '0' || *s > '9'))
		s++;
	if (*s == '-')
	{
		neg = true;
		s++;
	}
	while (*s && *s >= '0' && *s <= '9')
	{
		v = v * 10 + (*s - '0');
		s++;
	}
	return neg ? -v : v;
}

static void StripCellInheritedBoxProps(LCss *Style)
{
	if (!Style)
		return;

	static const LCss::PropType CellBoxProps[] =
	{
		LCss::PropWidth,
		LCss::PropMinWidth,
		LCss::PropMaxWidth,
		LCss::PropHeight,
		LCss::PropMinHeight,
		LCss::PropMaxHeight,

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

		LCss::PropBorderCollapse,
		LCss::PropBorderSpacing,
		LCss::PropBorderRadius,
		LCss::Prop_CellPadding
	};

	for (auto p : CellBoxProps)
		Style->DeleteProp(p);
}

static bool AttrValueIs(const char *s, const char *v)
{
	return s && v && strcompare(s, v, false) == 0;
}

static bool HasActualPadding(LCss *Style)
{
	if (!Style)
		return false;

	return	Style->Padding() ||
			Style->PaddingTop() ||
			Style->PaddingRight() ||
			Style->PaddingBottom() ||
			Style->PaddingLeft();
}

static bool HasOwnCssTextAlign(LCss *Style, LCss *ParentStyle)
{
	if (!Style || !ParentStyle)
		return false;

	LCss::Len Cur = Style->TextAlign();
	LCss::Len Parent = ParentStyle->TextAlign();
	if (!Parent)
		return Cur;
	return Cur && Cur != Parent;
}

static void ApplyCellPadding(LAutoPtr<LCss> &Style, const LCss::Len &CellPadding, bool HadActualPadding)
{
	if (!Style)
		return;

	if (!HadActualPadding)
	{
		LCss::Len cp = Style->_CellPadding();
		if (!cp)
			cp = CellPadding;
		if (cp)
			Style->Padding(cp);
	}

	Style->DeleteProp(LCss::Prop_CellPadding);
}

static bool IsAllWhiteW(const char16 *s)
{
	if (!s || !*s)
		return false;
	for (const char16 *p = s; *p; ++p)
		if (!IsWhite((char)*p))
			return false;
	return true;
}

static void ApplyAlign(LCss *s, const char *v, bool AllowOverride = false)
{
	if (!s || !v)
		return;
	if (!AllowOverride && s->TextAlign().Type != LCss::LenInherit)
		return;
	if (AttrValueIs(v, "left"))
		s->TextAlign(LCss::Len(LCss::AlignLeft));
	else if (AttrValueIs(v, "right"))
		s->TextAlign(LCss::Len(LCss::AlignRight));
	else if (AttrValueIs(v, "center"))
		s->TextAlign(LCss::Len(LCss::AlignCenter));
}

static void ApplyVerticalAlign(LCss *s, const char *v)
{
	if (!s || !v || s->VerticalAlign().Type != LCss::LenInherit)
		return;
	if (AttrValueIs(v, "top"))
		s->VerticalAlign(LCss::Len(LCss::VerticalTop));
	else if (AttrValueIs(v, "middle"))
		s->VerticalAlign(LCss::Len(LCss::VerticalMiddle));
	else if (AttrValueIs(v, "bottom"))
		s->VerticalAlign(LCss::Len(LCss::VerticalBottom));
}

static void ApplyLength(LCss *s, const char *v)
{
	if (!s || !v || s->Width())
		return;
	LCss::Len l(v, LCss::ParseRelaxed);
	if (l)
		s->Width(l);
}

static void ApplyHeight(LCss *s, const char *v)
{
	if (!s || !v || s->Height())
		return;
	LCss::Len l(v, LCss::ParseRelaxed);
	if (l)
		s->Height(l);
}

static void ApplyBorder(LCss *s, int px)
{
	if (!s || px <= 0)
		return;
	if (s->BorderTop().Style != LCss::BorderNone ||
		s->BorderRight().Style != LCss::BorderNone ||
		s->BorderBottom().Style != LCss::BorderNone ||
		s->BorderLeft().Style != LCss::BorderNone)
		return;

	LCss::BorderDef bd(LCss::LenPx, (float)px, LCss::ColorDef(LCss::ColorRgb, LColour::Black.c32()), LCss::BorderSolid);
	s->Border(bd);
	s->BorderTop(bd);
	s->BorderRight(bd);
	s->BorderBottom(bd);
	s->BorderLeft(bd);
}

static void ApplyBackground(LCss *s, const char *v)
{
	if (!s || !v)
		return;
	if (s->BackgroundColor().Type != LCss::ColorInherit)
		return;

	LCss::ColorDef c;
	if (LHtmlParser::ParseColour(v, c))
		s->BackgroundColor(c);
}

static void ApplyTablePresentation(LAutoPtr<LCss> &Style, LHtmlElement *e)
{
	const char *v;

	if (e->Get("cellspacing", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s && !s->BorderSpacing())
			s->BorderSpacing(LCss::Len(LCss::LenPx, ParseAttrInt(v)));
	}

	if (e->Get("cellpadding", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s && !s->_CellPadding())
			s->_CellPadding(LCss::Len(LCss::LenPx, ParseAttrInt(v)));
	}

	if (e->Get("border", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyBorder(s, ParseAttrInt(v));
	}

	if (e->Get("width", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyLength(s, v);
	}

	if (e->Get("bgcolor", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyBackground(s, v);
	}
}

static void ApplyRowPresentation(LAutoPtr<LCss> &Style, LHtmlElement *e, bool AllowAlignOverride)
{
	const char *v;
	LCss *s = EnsureStyle(Style);
	if (!s)
		return;

	if (e->Get("align", v))
		ApplyAlign(s, v, AllowAlignOverride);

	if (e->Get("valign", v))
		ApplyVerticalAlign(s, v);

	if (e->Get("height", v))
		ApplyHeight(s, v);
}

static void ApplyCellPresentation(LAutoPtr<LCss> &Style, LHtmlElement *e, HtmlTag tag, bool AllowAlignOverride)
{
	const char *v;

	if (e->Get("align", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyAlign(s, v, AllowAlignOverride);
	}

	if (e->Get("valign", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyVerticalAlign(s, v);
	}

	if (e->Get("height", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyHeight(s, v);
	}

	if (e->Get("width", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyLength(s, v);
	}

	if (e->Get("border", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyBorder(s, ParseAttrInt(v));
	}

	if (e->Get("bgcolor", v))
	{
		LCss *s = EnsureStyle(Style);
		if (s)
			ApplyBackground(s, v);
	}

	if (tag == TAG_TH)
	{
		LCss *s = EnsureStyle(Style);
		if (s && s->FontWeight() == LCss::FontWeightInherit)
			s->FontWeight(LCss::FontWeightBold);
	}
}

bool LRichTextPriv::FromHtml(LHtmlElement *e, CreateContext &ctx, LCss *ParentStyle, int Depth)
{
	char Sp[48];
	int SpLen = MIN(Depth << 1, sizeof(Sp) - 1);
	memset(Sp, ' ', SpLen);
	Sp[SpLen] = 0;
		
	for (unsigned i = 0; i < e->Children.Length(); i++)
	{
		LHtmlElement *c = e->Children[i];
		LAutoPtr<LCss> Style;
		if (ParentStyle)
			Style.Reset(new LCss(*ParentStyle));

		if (c->TagId == TAG_TD || c->TagId == TAG_TH)
			StripCellInheritedBoxProps(Style.Get());

		if (c->TagId == TAG_BLOCKQUOTE)
		{
			if (!Style)
				Style.Reset(new LCss);
			Style->MarginTop(LCss::Len("0.5em"));
			Style->MarginBottom(LCss::Len("0.5em"));
			Style->MarginLeft(LCss::Len("1em"));
		}

		LCss::SelArray Matches;
		HtmlElementCb Cb;
		if (ctx.StyleStore.Match(Matches, &Cb, c) &&
			Matches.Length() > 0 &&
			(Style.Get() || Style.Reset(new LCss)))
		{
			for (auto s : Matches)
			{
				const char *p = s->Style;
				Style->Parse(p, LCss::ParseRelaxed);
			}
		}
			
		// Check to see if the element is block level and end the previous
		// paragraph if so.
		c->Info = c->Tag ? LHtmlStatic::Inst->GetTagInfo(c->Tag) : NULL;
		bool IsBlock =	c->Info != NULL && c->Info->Block();
		switch (c->TagId) 
		{
			case TAG_STYLE:
			{
				char16 *Style = e->GetText();
				if (ValidStrW(Style))
					LAssert(!"Impl me.");
				continue;
				break;
			}
			case TAG_B:
			{
				if (!Style)
					Style.Reset(new LCss);
				if (Style)
					Style->FontWeight(LCss::FontWeightBold);
				break;
			}
			case TAG_I:
			{
				if (!Style)
					Style.Reset(new LCss);
				if (Style)
					Style->FontStyle(LCss::FontStyleItalic);
				break;
			}
			case TAG_A:
			{
				if (!Style)
					Style.Reset(new LCss);
				if (Style)
				{
					Style->TextDecoration(LCss::TextDecorUnderline);
					Style->Color(LCss::ColorDef(LCss::ColorRgb, LColour::Blue.c32()));
				}
				break;
			}
			case TAG_HR:
			{
				if (ctx.Tb)
					ctx.Tb->StripLast(NoTransaction);
				// Fall through
			}
			case TAG_IMG:
			{
				ctx.Tb = NULL;
				IsBlock = true;
				break;
			}
			default:
			{
				break;
			}
		}
			
		const char *Css, *Class;
		if (c->Get("style", Css))
		{
			if (!Style)
				Style.Reset(new LCss);
			if (Style)
				Style->Parse(Css, ParseRelaxed);
		}
		if (c->Get("class", Class))
		{
			LCss::SelArray Selectors;
			LRichEditElemContext StyleCtx;
			if (ctx.StyleStore.Match(Selectors, &StyleCtx, dynamic_cast<LRichEditElem*>(c)))
			{
				for (unsigned n=0; n<Selectors.Length(); n++)
				{
					LCss::Selector *sel = Selectors[n];
					if (sel)
					{
						const char *s = sel->Style;
						if (s)
						{
							if (!Style)
								Style.Reset(new LCss);
							if (Style)
								Style->Parse(s, LCss::ParseRelaxed);
						}
					}
				}
			}
		}

		bool HadCssWidth = Style && Style->Width();
		bool HadActualPadding = IsCellTag(c->TagId) && HasActualPadding(Style.Get());
		bool AllowAlignOverride = IsTableContextTag(c->TagId) && !HasOwnCssTextAlign(Style.Get(), ParentStyle);
		LCss::Len TableCellPadding;

		if (IsTableContextTag(c->TagId))
		{
			if (c->TagId == TAG_TABLE)
			{
				ApplyTablePresentation(Style, c);
				TableCellPadding = Style.Get() ? Style.Get()->_CellPadding() : LCss::Len();
				if (Style)
					Style->DeleteProp(LCss::Prop_CellPadding);
			}
			else if (c->TagId == TAG_TD || c->TagId == TAG_TH)
			{
				ApplyCellPresentation(Style, c, c->TagId, AllowAlignOverride);
				// A row's bgcolor applies to cells that don't set their own
				if (c->Parent && c->Parent->TagId == TAG_TR)
				{
					const char *rowBg = nullptr;
					if (c->Parent->Get("bgcolor", rowBg))
					{
						LCss *s = EnsureStyle(Style);
						if (s)
							ApplyBackground(s, rowBg);
					}
				}
				ApplyCellPadding(Style, ctx.CellPadding, HadActualPadding);
			}
			else if (c->TagId == TAG_TR)
				ApplyRowPresentation(Style, c, AllowAlignOverride);
		}

		// These containers are flattened, so their backgrounds cannot be
		// painted as boxes. Don't turn them into highlights on descendant text.
		if (Style)
		{
			switch (c->TagId)
			{
				case TAG_DIV:
				case TAG_TBODY:
				case TAG_TR:
					Style->DeleteProp(LCss::PropBackgroundColor);
					break;
				default:
					break;
			}
		}

		LNamedStyle *CachedStyle = AddStyleToCache(Style);

		if (IsTableContextTag(c->TagId) && (c->TagId == TAG_TABLE || ctx.TableTarget))
		{
			switch (c->TagId)
			{
				case TAG_TABLE:
				{
					if (!ctx.TargetBlocks)
						return Error(_FL, "HTML table has no target block array.");

					if (ctx.Tb && ctx.LastChar != '\n')
					{
						const uint32_t NewLine[] = {'\n', 0};
						if (!ctx.Tb->AddText(NoTransaction, -1, NewLine, 1))
							return Error(_FL, "Failed to end the paragraph before an HTML table.");
					}

					auto Table = new TableBlock(this);
					if (!Table || !ctx.AddBlock(Table))
					{
						DeleteObj(Table);
						return Error(_FL, "Failed to create HTML table block.");
					}

					Table->SetStyle(CachedStyle);
					Table->CellPadding = TableCellPadding;

					auto PreviousTableTarget = ctx.TableTarget;
					int PreviousTableRow = ctx.TableRow;
					LCss::Len PreviousCellPadding = ctx.CellPadding;
					auto PreviousTargetBlocks = ctx.TargetBlocks;
					auto PreviousTargetParent = ctx.TargetParent;
					auto PreviousText = ctx.Tb;
					uint32_t PreviousLastChar = ctx.LastChar;
					bool PreviousStartOfLine = ctx.StartOfLine;

					ctx.TableTarget = Table;
					ctx.TableRow = -1;
					ctx.CellPadding = Table->CellPadding;
					ctx.TargetBlocks = &Table->blocks;
					ctx.TargetParent = Table;
					ctx.Tb = nullptr;
					ctx.LastChar = '\n';
					ctx.StartOfLine = true;

					bool Imported = FromHtml(c, ctx, CachedStyle, Depth + 1);

					ctx.TableTarget = PreviousTableTarget;
					ctx.TableRow = PreviousTableRow;
					ctx.CellPadding = PreviousCellPadding;
					ctx.TargetBlocks = PreviousTargetBlocks;
					ctx.TargetParent = PreviousTargetParent;
					ctx.Tb = Imported ? nullptr : PreviousText;
					ctx.LastChar = Imported ? '\n' : PreviousLastChar;
					ctx.StartOfLine = Imported ? true : PreviousStartOfLine;

					if (!Imported)
						return Error(_FL, "Failed to import HTML table contents.");
					continue;
				}

				case TAG_TR:
				{
					if (!ctx.TableTarget)
						continue;

					ctx.TableRow++;
					if (!FromHtml(c, ctx, CachedStyle, Depth + 1))
						return Error(_FL, "Failed to import HTML table row contents.");
					continue;
				}

				case TAG_TD:
				case TAG_TH:
				{
					auto Table = ctx.TableTarget;
					if (!Table)
						continue;

					auto Cell = new TdBlock(this, c->TagId);
					if (!Cell || !Table->Add(Cell))
					{
						DeleteObj(Cell);
						return Error(_FL, "Failed to create HTML table cell.");
					}

					Cell->Row = ctx.TableRow >= 0 ? ctx.TableRow : 0;

					const char *Attr;
					if (c->Get("colspan", Attr))
					{
						int Span = ParseAttrInt(Attr);
						if (Span > 0)
							Cell->CS = Span;
					}
					if (c->Get("rowspan", Attr))
					{
						int Span = ParseAttrInt(Attr);
						if (Span > 0)
							Cell->RS = Span;
					}
					if (!HadCssWidth && c->Get("width", Attr))
					{
						LCss::Len L(Attr, LCss::ParseRelaxed);
						if (L.Type == LCss::LenPercent && L.Value > 0)
							Cell->Pct = (int)L.Value;
						else if (L.Type == LCss::LenPx && L.Value > 0)
						{
							Cell->Fixed = true;
							Cell->FixedPx = L.ToPx();
						}
					}

					Cell->SetStyle(CachedStyle);

					if (CachedStyle)
					{
						LCss::Len W = CachedStyle->Width();
						if (W.Type == LCss::LenPercent && Cell->Pct < 0)
							Cell->Pct = MAX((int)W.Value, 1);
						else if (W.Type == LCss::LenPx && Cell->Pct < 0 && Cell->FixedPx == 0)
						{
							Cell->Fixed = true;
							Cell->FixedPx = MAX((int)W.Value, 1);
						}
					}

					auto ChildStyle = Cell->MakeChildStyle(CachedStyle);
					if (!ChildStyle)
						return Error(_FL, "Failed to make HTML table cell child style.");

					auto PreviousTableTarget = ctx.TableTarget;
					int PreviousTableRow = ctx.TableRow;
					LCss::Len PreviousCellPadding = ctx.CellPadding;
					auto PreviousTargetBlocks = ctx.TargetBlocks;
					auto PreviousTargetParent = ctx.TargetParent;
					auto PreviousText = ctx.Tb;
					uint32_t PreviousLastChar = ctx.LastChar;
					bool PreviousStartOfLine = ctx.StartOfLine;

					ctx.TableTarget = nullptr;
					ctx.TargetBlocks = &Cell->blocks;
					ctx.TargetParent = Cell;
					ctx.Tb = nullptr;
					ctx.LastChar = '\n';
					ctx.StartOfLine = true;

					char16 *Txt = c->GetText();
					bool Imported = false;
					if (Txt && !IsAllWhiteW(Txt))
					{
						auto CellText = Cell->GetTextBlock();
						if (!CellText)
							return Error(_FL, "Failed to create HTML table cell text.");

						ctx.Tb = CellText;
						ctx.Tb->SetStyle(ChildStyle);
						if (ctx.AddText(ChildStyle, Txt))
							ctx.StartOfLine = false;
						Imported = true;
					}

					Imported = FromHtml(c, ctx, ChildStyle, Depth + 1);
					if (Imported && !Cell->blocks.Length())
					{
						auto Text = Cell->GetTextBlock();
						if (Text)
							Text->SetStyle(ChildStyle);
					}

					ctx.TableTarget = PreviousTableTarget;
					ctx.TableRow = PreviousTableRow;
					ctx.CellPadding = PreviousCellPadding;
					ctx.TargetBlocks = PreviousTargetBlocks;
					ctx.TargetParent = PreviousTargetParent;
					ctx.Tb = Imported ? nullptr : PreviousText;
					ctx.LastChar = Imported ? '\n' : PreviousLastChar;
					ctx.StartOfLine = Imported ? true : PreviousStartOfLine;

					if (!Imported)
						return Error(_FL, "Failed to import HTML table cell contents.");
					continue;
				}

				case TAG_TBODY:
				{
					if (!ctx.TableTarget)
						continue;

					if (!FromHtml(c, ctx, CachedStyle, Depth + 1))
						return Error(_FL, "Failed to import HTML table row group.");
					continue;
				}

				default:
				{
					continue;
				}
			}
		}

		if (c->TagId == TAG_BLOCKQUOTE)
		{
			if (!ctx.TargetBlocks)
				return Error(_FL, "HTML blockquote has no target block array.");

			if (ctx.Tb && ctx.LastChar != '\n')
			{
				const uint32_t NewLine[] = {'\n', 0};
				if (!ctx.Tb->AddText(NoTransaction, -1, NewLine, 1))
					return Error(_FL, "Failed to end the paragraph before an HTML blockquote.");
			}

			auto Quote = new DivBlock(this, TAG_BLOCKQUOTE, "blockquote");
			if (!Quote || !ctx.AddBlock(Quote))
			{
				DeleteObj(Quote);
				return Error(_FL, "Failed to create HTML blockquote.");
			}
			Quote->SetStyle(CachedStyle);
			auto ChildStyle = Quote->MakeChildStyle(CachedStyle);

			auto PreviousTarget = ctx.TargetBlocks;
			auto PreviousParent = ctx.TargetParent;
			auto PreviousText = ctx.Tb;
			uint32_t PreviousLastChar = ctx.LastChar;
			bool PreviousStartOfLine = ctx.StartOfLine;

			ctx.TargetBlocks = &Quote->blocks;
			ctx.TargetParent = Quote;
			ctx.Tb = nullptr;
			ctx.LastChar = '\n';
			ctx.StartOfLine = true;

			bool Imported = ChildStyle != nullptr;
			if (Imported && ValidStrW(c->GetText()))
			{
				ctx.Tb = Quote->GetTextBlock();
				if ((Imported = ctx.Tb != nullptr))
				{
					ctx.Tb->SetStyle(ChildStyle);
					Imported = ctx.AddText(ChildStyle, c->GetText());
					ctx.StartOfLine = false;
				}
			}
			if (Imported)
				Imported = FromHtml(c, ctx, ChildStyle, Depth + 1);
			if (Imported && !Quote->blocks.Length())
			{
				auto Text = Quote->GetTextBlock();
				if ((Imported = Text != nullptr))
					Text->SetStyle(ChildStyle);
			}

			ctx.TargetBlocks = PreviousTarget;
			ctx.TargetParent = PreviousParent;
			ctx.Tb = Imported ? nullptr : PreviousText;
			ctx.LastChar = Imported ? '\n' : PreviousLastChar;
			ctx.StartOfLine = Imported ? true : PreviousStartOfLine;
			if (!Imported)
				return Error(_FL, "Failed to import HTML blockquote contents.");
			continue;
		}

		if (c->TagId == TAG_UL || c->TagId == TAG_OL)
		{
			if (!ctx.TargetBlocks)
				return Error(_FL, "HTML list has no target block array.");

			if (ctx.Tb && ctx.LastChar != '\n')
			{
				const uint32_t NewLine[] = {'\n', 0};
				if (!ctx.Tb->AddText(NoTransaction, -1, NewLine, 1))
					return Error(_FL, "Failed to end the paragraph before an HTML list.");
			}

			LCss::ListStyleTypes ListType = c->TagId == TAG_OL ? LCss::ListDecimal : LCss::ListDisc;
			if (Style && Style->ListStyleType() != LCss::ListInherit)
				ListType = Style->ListStyleType();

			auto List = new ListBlock(this, ListType);
			if (!List || !ctx.AddBlock(List))
			{
				DeleteObj(List);
				return Error(_FL, "Failed to create HTML list block.");
			}

			auto PreviousTarget = ctx.TargetBlocks;
			auto PreviousParent = ctx.TargetParent;
			auto PreviousText = ctx.Tb;
			uint32_t PreviousLastChar = ctx.LastChar;
			bool PreviousStartOfLine = ctx.StartOfLine;

			ctx.TargetBlocks = &List->blocks;
			ctx.TargetParent = List;
			ctx.Tb = nullptr;
			for (auto Item : c->Children)
			{
				if (Item->TagId != TAG_LI)
					continue;

				auto Text = new TextBlock(this);
				if (!Text || !List->Add(Text))
				{
					DeleteObj(Text);
					ctx.TargetBlocks = PreviousTarget;
					ctx.TargetParent = PreviousParent;
					ctx.Tb = PreviousText;
					ctx.LastChar = PreviousLastChar;
					ctx.StartOfLine = PreviousStartOfLine;
					return Error(_FL, "Failed to create HTML list item.");
				}

				ctx.Tb = Text;
				ctx.LastChar = '\n';
				ctx.StartOfLine = true;
				if (ctx.AddText(CachedStyle, Item->GetText()))
					ctx.StartOfLine = false;
				if (!FromHtml(Item, ctx, Style, Depth + 1))
				{
					ctx.TargetBlocks = PreviousTarget;
					ctx.TargetParent = PreviousParent;
					ctx.Tb = PreviousText;
					ctx.LastChar = PreviousLastChar;
					ctx.StartOfLine = PreviousStartOfLine;
					return false;
				}
				ctx.Tb = nullptr;
			}

			ctx.TargetBlocks = PreviousTarget;
			ctx.TargetParent = PreviousParent;
			ctx.Tb = nullptr;
			ctx.LastChar = '\n';
			ctx.StartOfLine = true;
			continue;
		}

		if
		(
			(IsBlock && ctx.LastChar != '\n')
			||
			c->TagId == TAG_BR
		)
		{
			if (!ctx.Tb && c->TagId == TAG_BR)
			{
				// Don't do this for IMG and HR layout.
				ctx.Tb = new TextBlock(this);
				ctx.AddBlock(ctx.Tb);
				if (CachedStyle && ctx.Tb)
					ctx.Tb->SetStyle(CachedStyle);
			}

			if (ctx.Tb)
			{
				const uint32_t Nl[] = {'\n', 0};
				ctx.Tb->AddText(NoTransaction, -1, Nl, 1, NULL);
				ctx.LastChar = '\n';
				ctx.StartOfLine = true;
			}
		}
		
		bool EndStyleChange = false;

		if (c->TagId == TAG_IMG)
		{
			ctx.Ib = new ImageBlock(this);
			ctx.AddBlock(ctx.Ib);
			if (ctx.Ib)
			{
				const char *s;
				if (c->Get("src", s))
					ctx.Ib->Source = s;

				if (c->Get("width", s))
				{
					LCss::Len Sz(s);
					int Px = Sz.ToPx();
					if (Px) ctx.Ib->Size.x = Px;
				}

				if (c->Get("height", s))
				{
					LCss::Len Sz(s);
					int Px = Sz.ToPx();
					if (Px) ctx.Ib->Size.y = Px;
				}

				if (CachedStyle)
					ctx.Ib->SetStyle(CachedStyle);

				ctx.Ib->Load();
			}
		}
		else if (c->TagId == TAG_HR)
		{
			ctx.Hrb = new HorzRuleBlock(this);
			ctx.AddBlock(ctx.Hrb);
		}
		else if (c->TagId == TAG_A)
		{
			ctx.StartOfLine |= ctx.AddText(CachedStyle, c->GetText());
			
			if (ctx.Tb &&
				ctx.Tb->Txt.Length())
			{
				StyleText *st = ctx.Tb->Txt.Last();

				st->Element = TAG_A;
			
				const char *Link;
				if (c->Get("href", Link))
					st->Param = Link;
			}
		}
		else
		{
			if (IsBlock && ctx.Tb != NULL)
			{
				if (CachedStyle != ctx.Tb->GetStyle())
				{
					if (ctx.Tb->Length() == 0)
					{
						ctx.Tb->SetStyle(CachedStyle);
					}
					else
					{
						// Start a new block because the styles are different...
						EndStyleChange = true;
						auto Idx = ctx.TargetBlocks->IndexOf(ctx.Tb);
						ctx.Tb = new TextBlock(this);
						if (Idx >= 0)
							ctx.InsertBlock(Idx+1, ctx.Tb);
						else
							ctx.AddBlock(ctx.Tb);

						if (CachedStyle)
							ctx.Tb->SetStyle(CachedStyle);
					}
				}
			}

			char16 *Txt = c->GetText();
			if
			(
				Txt
				&&
				(
					!ctx.StartOfLine
					|| 
					ValidStrW(Txt)
				)
			)
			{
				if (!ctx.Tb)
				{
					ctx.Tb = new TextBlock(this);
					ctx.AddBlock(ctx.Tb);
					ctx.Tb->SetStyle(CachedStyle);
				}

				#ifdef __GTK_H__
				for (auto *i = Txt; *i; i++)
					if (*i == 0xa0)
						*i = ' ';
				#endif
			
				ctx.AddText(CachedStyle, Txt);
				ctx.StartOfLine = false;
			}
		}
			
		if (!FromHtml(c, ctx, Style, Depth + 1))
			return false;
			
		if (EndStyleChange)
			ctx.Tb = NULL;
		if (IsBlock)
			ctx.StartOfLine = true;
	}

	return true;
}

bool LRichTextPriv::GetSelection(LArray<char16> *Text, LAutoString *Html)
{
	if (!Text && !Html)
		return false;

	LArray<uint32_t> Utf32;

	bool Cf = CursorFirst();
	LRichTextPriv::BlockCursor *Start = Cf ? Cursor : Selection;
	LRichTextPriv::BlockCursor *End = Cf ? Selection : Cursor;

	if (Html)
	{		
		if (ToHtml(NULL, Start, End))
			*Html = UtfNameCache;
	}
	
	if (Text)
	{
		LArray<Block*> Leaves;
		auto CollectLeaves = [&](auto &&Self, Block *b) -> void
		{
			if (b->blocks.Length())
			{
				for (auto Child : b->blocks)
					Self(Self, Child);
			}
			else
				Leaves.Add(b);
		};
		for (auto b : Blocks)
			CollectLeaves(CollectLeaves, b);

		ssize_t StartLeaf = Leaves.IndexOf(Start->Blk);
		ssize_t EndLeaf = Leaves.IndexOf(End->Blk);
		if (StartLeaf < 0 || EndLeaf < StartLeaf)
			return Error(_FL, "Selection endpoints are not in document order.");

		for (ssize_t i = StartLeaf; i <= EndLeaf; i++)
		{
			Block *b = Leaves[i];
			ssize_t Offset = i == StartLeaf ? Start->Offset : 0;
			ssize_t EndOffset = i == EndLeaf ? End->Offset : b->Length();
			if (i > StartLeaf &&
				b->parent &&
				b->parent == Leaves[i - 1]->parent &&
				dynamic_cast<ListBlock*>(b->parent))
			{
				Utf32.Add('\n');
			}
			if (EndOffset > Offset)
				b->CopyAt(Offset, EndOffset - Offset, &Utf32);
		}

		char16 *w = (char16*)LNewConvertCp(LGI_WideCharset, &Utf32[0], "utf-32", Utf32.Length() * sizeof(uint32_t));
		if (!w)
			return Error(_FL, "Failed to convert %i utf32 to wide.", Utf32.Length());

		Text->Add(w, Strlen(w));
		Text->Add(0);
	}

	return true;
}

LRichTextEdit::RectType LRichTextPriv::PosToButton(LMouse &m)
{
	if (Areas[LRichTextEdit::ToolsArea].Overlap(m.x, m.y)
		// || Areas[LRichTextEdit::CapabilityArea].Overlap(m.x, m.y)
		)
	{
		for (unsigned i=LRichTextEdit::FontFamilyBtn; i<LRichTextEdit::MaxArea; i++)
		{
			if (Areas[i].Valid() &&
				Areas[i].Overlap(m.x, m.y))
			{
				return (LRichTextEdit::RectType)i;
				break;
			}
		}
	}

	return LRichTextEdit::MaxArea;
}

void LRichTextPriv::OnComponentInstall(LString Name)
{
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Blocks[i]->OnComponentInstall(Name);
	}
}

// This handles calculating the selection stuff for simple "one char" blocks
// like images and HR. Call this at the start of the OnPaint.
// \return TRUE if the content should be drawn selected.
bool LRichTextPriv::PaintContext::SelectBeforePaint(Block *b)
{
	// Each end point at the start of this block flips the paint state. This
	// is stateless so that blocks painted later can't reuse an earlier block's end points.
	if (b->Cursors > 0 && Select)
	{
		if (Cursor && Cursor->Blk == b && Cursor->Offset == 0)
			Type = Type == Selected ? Unselected : Selected;
		if (Select->Blk == b && Select->Offset == 0)
			Type = Type == Selected ? Unselected : Selected;
	}

	return Type == Selected;
}

// Call this after the OnPaint
// \return TRUE if the content after the block is selected.
bool LRichTextPriv::PaintContext::SelectAfterPaint(class Block *b)
{
	// Each end point at the end of this block flips the paint state
	if (b->Cursors > 0 && Select)
	{
		if (Cursor && Cursor->Blk == b && Cursor->Offset == 1)
			Type = Type == Selected ? Unselected : Selected;
		if (Select->Blk == b && Select->Offset == 1)
			Type = Type == Selected ? Unselected : Selected;
	}

	return Type == Selected;
}

#ifdef _DEBUG
void LRichTextPriv::DumpNodes(LTree *Root)
{
	if (Cursor)
	{
		LTreeItem *ti = new LTreeItem;
		ti->SetText("Cursor");
		PrintNode(ti, "Offset=%i", Cursor->Offset);
		PrintNode(ti, "Pos=%s", Cursor->Pos.GetStr());
		PrintNode(ti, "LineHint=%i", Cursor->LineHint);
		PrintNode(ti, "Blk=%i", Cursor->Blk ? Blocks.IndexOf(Cursor->Blk) : -2);
		Root->Insert(ti);
	}
	if (Selection)
	{
		LTreeItem *ti = new LTreeItem;
		ti->SetText("Selection");
		PrintNode(ti, "Offset=%i", Selection->Offset);
		PrintNode(ti, "Pos=%s", Selection->Pos.GetStr());
		PrintNode(ti, "LineHint=%i", Selection->LineHint);
		PrintNode(ti, "Blk=%i", Selection->Blk ? Blocks.IndexOf(Selection->Blk) : -2);
		Root->Insert(ti);
	}

	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		LTreeItem *ti = new LTreeItem;
		Block *b = Blocks[i];
		b->DumpNodes(ti);

		LString s;
		s.Printf("[%i] %s", i, ti->GetText());
		ti->SetText(s);

		Root->Insert(ti);
	}
}

LTreeItem *PrintNode(LTreeItem *Parent, const char *Fmt, ...)
{
	LTreeItem *i = new LTreeItem;
	LString s;

	va_list Arg;
	va_start(Arg, Fmt);
	s.Printf(Arg, Fmt);
	va_end(Arg);
	s = s.Replace("\n", "\\n");

	i->SetText(s);
	Parent->Insert(i);
	return i;
}

#endif

} // namespace Rte2