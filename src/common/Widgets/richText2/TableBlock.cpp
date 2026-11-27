#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit2.h"

#include "RichTextEditPriv.h"

namespace Rte2
{
	using namespace TableGrid;

	// -----------------------------------------------------------------------
	// Measurement helpers
	// -----------------------------------------------------------------------

	static bool AllWhite(const char16 *s)
	{
		if (!s || !*s)
			return false;
		for (const char16 *p = s; *p; p++)
			if (!IsWhite(*p))
				return false;
		return true;
	}

	static int RunWidth(LRichTextPriv *d, LRichTextPriv::StyleText *st, ssize_t Start, ssize_t Len)
	{
		LFont *f = d->GetFont(st->GetStyle());
		if (!f || Len <= 0)
			return 0;

		LDisplayString ds(f,
			#ifndef WINDOWS
			(char16*)
			#endif
			st->At(Start), Len);

		return ds.X();
	}

	static MinMax MeasureRun(LRichTextPriv *d, LRichTextPriv::TextBlock *tb)
	{
		MinMax r;
		int run = 0;

		for (auto st : tb->Txt)
		{
			LFont *f = d->GetFont(st->GetStyle());
			if (!f)
				continue;

			int space = LDisplayString(f, L" ", 1).X();
			ssize_t len = st->Length();
			ssize_t i = 0;

			while (i < len)
			{
				uint32_t c = st->At(i)[0];

				if (c == '\n')
				{
					r.Max = MAX(r.Max, run);
					r.Has = true;
					run = 0;
					i++;
					continue;
				}

				if (IsWhite(c))
				{
					run += space;
					i++;
					continue;
				}

				ssize_t start = i;
				while (i < len)
				{
					uint32_t cc = st->At(i)[0];
					if (cc == '\n' || IsWhite(cc))
						break;
					i++;
				}

				int w = RunWidth(d, st, start, i - start);
				if (w > r.Min)
					r.Min = w;
				run += w;
				r.Has = true;
			}

			r.Max = MAX(r.Max, run);
		}

		return r;
	}

	static int VAlignOf(LNamedStyle *st)
	{
		if (!st)
			return LCss::VerticalTop;

		LCss::Len v = st->VerticalAlign();
		if (v && v.Type != LCss::LenAuto)
			return v.Type;

		return LCss::VerticalTop;
	}

	// -----------------------------------------------------------------------
	// TdBlock
	// -----------------------------------------------------------------------

	LRichTextPriv::TdBlock::TdBlock(LRichTextPriv *priv, HtmlTag tag) :
		DivBlock(priv, tag)
	{
	}

	LRichTextPriv::TdBlock::TdBlock(const TdBlock *Copy) :
		DivBlock(Copy),
		Row(Copy->Row),
		Col(Copy->Col),
		CS(Copy->CS),
		RS(Copy->RS),
		Pct(Copy->Pct),
		FixedPx(Copy->FixedPx),
		Fixed(Copy->Fixed),
		CellBox(Copy->CellBox),
		ContentH(Copy->ContentH)
	{
	}

	LRichTextPriv::Block *LRichTextPriv::TdBlock::Clone()
	{
		return new TdBlock(this);
	}

	bool LRichTextPriv::TdBlock::OnLayout(Flow &flow)
	{
		if (!flow.d)
			return false;

		int StartY = flow.CurY;

		Pos.x1 = flow.Left;
		Pos.y1 = flow.CurY;
		Pos.x2 = flow.Right;
		Pos.y2 = flow.CurY - 1;

		for (auto b : blocks)
		{
			if (!b->OnLayout(flow))
				return false;

			LRect p = b->GetPos();
			if (p.y2 > Pos.y2)
				Pos.y2 = p.y2;
		}

		if (Pos.y2 < Pos.y1)
			Pos.y2 = Pos.y1;

		ContentH = MAX(flow.CurY - StartY, 0);
		return true;
	}

	bool LRichTextPriv::TdBlock::HitTest(HitTestResult &htr)
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

		LRect r = GetPos();
		if (r.X() <= 0 || !r.Overlap(htr.In))
			return false;

		htr.Blk = this;
		htr.Idx = 0;
		htr.LineHint = 0;
		htr.Near = true;
		return true;
	}

	void LRichTextPriv::TdBlock::OnPaint(PaintContext &Ctx)
	{
		// The selection state is toggled by the leaf blocks in document order, so
		// it must carry through cells without being reset here
		LRect r = GetPos();
		if (r.X() <= 0)
		{
			DivBlock::OnPaint(Ctx);
			return;
		}

		LColour Back = Ctx.Colours[Unselected].Back;
		if (Style)
		{
			LCss::ColorDef bg = Style->BackgroundColor();
			if (bg.Type == LCss::ColorRgb)
				Back.Set(bg.Rgb32, 32);
		}

		LColour BorderCol(222, 222, 222);
		if (Style && Style->BorderLeft().Style != LCss::BorderNone)
		{
			LCss::ColorDef BorderColor = Style->BorderLeft().Color;
			if (BorderColor.Type == LCss::ColorRgb)
				BorderCol.Set(BorderColor.Rgb32, 32);
		}

		LRect box = r;
		Ctx.DrawBox(box, border, BorderCol);
		Ctx.DrawBox(box, padding, Back);

		for (auto b : blocks)
			b->OnPaint(Ctx);
	}

	void LRichTextPriv::TdBlock::OpenTag(LStream &s, int CS, int RS)
	{
		const char *name = GetElementName();

		s.Print("<%s", name);
		if (CS > 1)
			s.Print(" colspan='%d'", CS);
		if (RS > 1)
			s.Print(" rowspan='%d'", RS);
		if (Style && Style->Name.Get() && *Style->Name.Get())
			s.Print(" class='%s'", Style->Name.Get());
		s.Print(">\n");
	}

	bool LRichTextPriv::TdBlock::ToHtmlContent(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
	{
		const char *name = GetElementName();

		bool FullList = Rng == nullptr;
		LRange All(0, Length());
		if (!Rng)
			Rng = &All;

		if (FullList && blocks.Length() == 1)
		{
			auto tb = dynamic_cast<TextBlock*>(blocks[0]);
			if (tb && tb->Length() == 0)
			{
				s.Print("</%s>\n", name);
				return true;
			}
		}

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

	bool LRichTextPriv::TdBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
	{
		OpenTag(s, CS, RS);
		return ToHtmlContent(s, Media, Rng);
	}

	#ifdef _DEBUG
	void LRichTextPriv::TdBlock::DumpNodes(LTreeItem *blockItem)
	{
		blockItem->SetText(LString::Fmt("%s r%d c%d x%d y%d", GetClass(), Row, Col, CS, RS));

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

	// -----------------------------------------------------------------------
	// TableBlock
	// -----------------------------------------------------------------------

	LRichTextPriv::TableBlock::TableBlock(LRichTextPriv *priv) :
		DivBlock(priv, TAG_TABLE)
	{
	}

	LRichTextPriv::TableBlock::TableBlock(const TableBlock *Copy) :
		DivBlock(Copy),
		RowH(Copy->RowH),
		CellPadding(Copy->CellPadding)
	{
		BuildGrid();
	}

	LRichTextPriv::Block *LRichTextPriv::TableBlock::Clone()
	{
		return new TableBlock(this);
	}

	void LRichTextPriv::TableBlock::BuildGrid()
	{
		grid = TableGrid::Grid<Block>();

		for (unsigned i = 0; i < blocks.Length(); i++)
		{
			auto td = dynamic_cast<TdBlock*>(blocks[i]);
			if (!td)
				continue;

			int r = td->Row;
			if (r < 0)
				r = 0;

			if ((int)grid.Rows.size() <= r)
				grid.Rows.resize(r + 1);

			grid.Rows[r].Cells.push_back((int)grid.Cells.size());
			TableGrid::Cell<Block> cell(td);
			cell.CS = MAX(td->CS, 1);
			cell.RS = MAX(td->RS, 1);
			grid.Cells.push_back(cell);
		}

		TableGrid::PlaceCells(grid);
	}

	int LRichTextPriv::TableBlock::Spacing()
	{
		LNamedStyle *st = GetStyle();
		if (!st)
			return 2;

		if (st->BorderCollapse() == LCss::CollapseCollapse)
			return 0;

		LCss::Len s = st->BorderSpacing();
		if (s && s.Type != LCss::LenAuto)
		{
			LFont *f = d->GetFont(st);
			if (f)
				return MAX(s.ToPx(0, f), 0);
		}

		return 2;
	}

	LRect LRichTextPriv::TableBlock::CellEdges(TdBlock *td, int cellW)
	{
		LRect e;
		if (!td)
			return e;

		LNamedStyle *st = td->GetStyle();
		LFont *font = d->GetFont(st);
		if (!font && d->View)
			font = d->View->GetFont();

		if (st && font)
		{
			LRect pb;
			pb.Set(0, 0, MAX(cellW, 1), MAX(cellW, 1));
			td->LCssBox<LRect>::SetStyle(font, st, pb);

			// CSS table cells cannot have margins.
			td->margin.ZOff(0, 0);

			// 'cellpadding' applies to cells that have no padding of their own.
			if (td->padding.x1 <= 0 && td->padding.y1 <= 0 && CellPadding)
			{
				int cp = CellPadding.ToPx(cellW, font);
				td->padding.ZOff(cp, cp);
			}
		}

		e.x1 = td->border.x1 + td->padding.x1;
		e.y1 = td->border.y1 + td->padding.y1;
		e.x2 = td->border.x2 + td->padding.x2;
		e.y2 = td->border.y2 + td->padding.y2;
		return e;
	}

	TableGrid::MinMax LRichTextPriv::TableBlock::MeasureBlock(Block *b, int depth, int avail)
	{
		TableGrid::MinMax r;
		if (!b || depth > 16)
			return r;

		if (auto nt = dynamic_cast<TableBlock*>(b))
			return nt->MeasureColumns(MAX(avail, 1), depth + 1);

		LFont *font = d->GetFont(b->GetStyle());
		if (!font && d->View)
			font = d->View->GetFont();

		if (auto tb = dynamic_cast<TextBlock*>(b))
		{
			r = MeasureRun(d, tb);
		}
		else if (auto im = dynamic_cast<ImageBlock*>(b))
		{
			int w = MIN(MAX(im->Size.x, 0), avail);
			r.Min = MAX(r.Min, w);
			r.Max = MAX(r.Max, w);
			r.Has = true;
		}
		else if (auto hr = dynamic_cast<HorzRuleBlock*>(b))
		{
			r.Min = MAX(r.Min, avail);
			r.Max = MAX(r.Max, avail);
			r.Has = true;
		}
		else
		{
			for (auto c : b->blocks)
			{
				MinMax cm = MeasureBlock(c, depth + 1, avail);
				if (!cm.Has)
					continue;

				r.Min = MAX(r.Min, cm.Min);
				r.Max = MAX(r.Max, cm.Max);
				r.Has = true;
			}

			// The marker column of a list is content too, so it has to be part of
			// the cell measurement, as in ListBlock::OnLayout().
			if (auto lb = dynamic_cast<ListBlock*>(b))
			{
				if (font)
				{
					int indent = font->GetHeight();
					if (lb->IsOrdered())
					{
						LDisplayString widest(font, lb->Marker(MAX((int)lb->blocks.Length() - 1, 0)));
						indent = MAX(indent, widest.X() + font->GetHeight() / 2);
					}

					r.Min += indent;
					r.Max += indent;
					r.Has = true;
				}
			}
		}

		if (auto td = dynamic_cast<TdBlock*>(b))
		{
			// CellEdges() applies the 'cellpadding' fallback, so the measure
			// matches what placement uses for the cell.
			LRect e = CellEdges(td, avail);
			r.Min += e.x1 + e.x2;
			r.Max += e.x1 + e.x2;
			if (e.y1 || e.y2)
				r.Has = true;

			if (td->Fixed && td->FixedPx > r.Max)
				r.Max = td->FixedPx;

			return r;
		}

		if (auto box = dynamic_cast<LCssBox<LRect>*>(b))
		{
			LNamedStyle *st = b->GetStyle();
			if (st && font)
			{
				LRect pb;
				pb.Set(0, 0, MAX(avail, 1), MAX(avail, 1));
				box->SetStyle(font, st, pb);

				int ex = box->border.x1 + box->border.x2 + box->padding.x1 + box->padding.x2;
				int ey = box->border.y1 + box->border.y2 + box->padding.y1 + box->padding.y2;

				if (ex || ey)
				{
					r.Min += ex;
					r.Max += ex;
					r.Has = true;
				}
			}
		}

		return r;
	}

	void LRichTextPriv::TableBlock::FillMetrics(TableGrid::Columns &cols, int avail, int depth)
	{
		cols.Spacing = Spacing();

		for (auto &tc : grid.Cells)
		{
			auto td = dynamic_cast<TdBlock*>(tc.Node);
			if (!td)
				continue;

			tc.m = MeasureBlock(td, depth, avail);
			if (!tc.m.Has)
				continue;

			if (td->Pct >= 0)
				tc.Pct = td->Pct;

			LNamedStyle *cs = td->GetStyle();
			if (cs)
			{
				LCss::Len w = cs->Width();
				if (w && w.Type == LCss::LenPercent)
					tc.Pct = MAX(tc.Pct, (int)w.Value);
				else if (td->Fixed && w && w.Type == LCss::LenPx)
					tc.Fixed = true;

				// 'height' is a per-row minimum, so it is carried as a cell height
				// rather than a column metric. Percentage heights cannot be resolved
				// against an unknown table height.
				LCss::Len h = cs->MinHeight();
				if (!h)
					h = cs->Height();
				if (h && h.Type == LCss::LenPx)
				{
					LFont *f = d->GetFont(cs);
					if (!f && d->View)
						f = d->View->GetFont();
					if (f)
					{
						LRect e = CellEdges(td, avail);
						tc.MinH = MAX((int)h.ToPx(avail, f), 0) + e.y1 + e.y2;
					}
				}
			}
		}

		TableGrid::ColumnMetrics(grid, cols);
	}

	TableGrid::MinMax LRichTextPriv::TableBlock::MeasureColumns(int avail, int depth)
	{
		TableGrid::MinMax r;
		if (depth > 16)
			return r;

		BuildGrid();

		int n = grid.NCols;
		if (n <= 0 || grid.Cells.empty())
			return r;

		TableGrid::Columns cols;
		FillMetrics(cols, avail, depth);

		int fixedX = cols.Spacing * (n + 1) + border.x1 + border.x2 + padding.x1 + padding.x2;
		r.Min = fixedX;
		r.Max = fixedX;
		for (int c = 0; c < n; c++)
		{
			r.Min += cols.Min[c];
			r.Max += cols.Max[c];
		}
		r.Has = true;

		return r;
	}

	bool LRichTextPriv::TableBlock::OnLayout(Flow &flow)
	{
		if (!flow.d)
			return false;

		BuildGrid();

		int n = grid.NCols;
		if (n <= 0 || grid.Cells.empty())
			return DivBlock::OnLayout(flow);

		LNamedStyle *st = GetStyle();
		LFont *font = d->GetFont(st);
		if (!font && flow.d->View)
			font = flow.d->View->GetFont();
		if (!font)
			return DivBlock::OnLayout(flow);

		int avail = flow.X();

		LRect pb;
		pb.Set(0, 0, MAX(avail, 1), MAX(avail, 1));
		if (st)
			LCssBox<LRect>::SetStyle(font, st, pb);

		TableGrid::Columns cols;
		FillMetrics(cols, avail, 0);
		int spacing = cols.Spacing;

		int fixedX = spacing * (n + 1) + border.x1 + border.x2 + padding.x1 + padding.x2;
		int outer = TableGrid::PreferredWidth(cols) + fixedX;

		if (st)
		{
			LCss::Len w = st->Width();
			// A px width is the table's target: content narrower than it expands to fill it,
			// and wider content is wrapped down to it.
			if (w && w.Type == LCss::LenPx)
				outer = MAX(1, w.ToPx(avail, font));

			LCss::Len mx = st->MaxWidth();
			if (mx && mx.Type == LCss::LenPx)
				outer = MIN(outer, MAX(1, mx.ToPx(avail, font)));

			LCss::Len mn = st->MinWidth();
			if (mn && mn.Type == LCss::LenPx)
				outer = MAX(outer, mn.ToPx(avail, font));
		}

		outer = MIN(outer, avail);
		outer = MAX(outer, fixedX);

		std::vector<int> cw;
		TableGrid::SolveColumns(cols, outer - fixedX, cw);

		int rows = (int)grid.Rows.size();
		std::vector<int> cellH(grid.Cells.size(), 0);
		RowH.assign(rows, 0);

		struct Spanned { int idx, top; };
		std::vector<Spanned> spanned;

		std::vector<int> rowMin;
		TableGrid::RowMinHeights(grid, rowMin);

		for (size_t r = 0; r < grid.Rows.size(); r++)
		{
			LNamedStyle *rs = grid.Rows[r].Node ? grid.Rows[r].Node->GetStyle() : nullptr;
			LCss::Len h = rs ? rs->Height() : LCss::Len();
			if (h && h.Type == LCss::LenPx)
				rowMin[r] = MAX(rowMin[r], MAX((int)h.ToPx(avail, font), 0));
		}

		Flow mf(d);
		mf.pDC = flow.pDC;
		mf.Visible = false;

		for (size_t r = 0; r < grid.Rows.size(); r++)
		{
			int rh = 0;

			for (int idx : grid.Rows[r].Cells)
			{
				auto &tc = grid.Cells[idx];
				auto td = dynamic_cast<TdBlock*>(tc.Node);
				if (!td)
					continue;

				int cellW = spacing * (tc.CS - 1);
				for (int c = 0; c < tc.CS; c++)
					cellW += cw[tc.Col + c];

				LRect e = CellEdges(td, cellW);

				mf.Left = e.x1;
				mf.Right = MAX(e.x1, cellW - e.x2);
				mf.CurY = 0;

				td->OnLayout(mf);

				int h = MAX(mf.CurY, 0) + e.y1 + e.y2;
				h = MAX(h, e.y1 + e.y2 + font->GetHeight());

				cellH[idx] = h;

				if (tc.RS == 1)
					rh = MAX(rh, h);
				else
					spanned.push_back({idx, (int)r});
			}

			RowH[r] = MAX(MAX(rh, rowMin[r]), font->GetHeight());
		}

		// Give rows extra height when a spanned cell is taller than the rows it spans.
		for (auto &s : spanned)
		{
			auto &tc = grid.Cells[s.idx];

			int total = 0;
			for (int r = tc.Row; r < tc.Row + tc.RS; r++)
				total += RowH[r] + spacing;
			total -= spacing;

			int extra = cellH[s.idx] - total;
			if (extra > 0)
				TableGrid::Distribute(RowH, tc.Row, tc.RS, extra);
		}

		// Final pass: place every cell and lay out its content.
		int innerW = 0;
		for (int c = 0; c < n; c++)
			innerW += cw[c];

		int innerH = 0;
		for (int r = 0; r < rows; r++)
			innerH += RowH[r];

		int SavedLeft = flow.Left;
		int SavedRight = flow.Right;

		flow.Left += margin.x1;
		flow.Right -= margin.x2;
		flow.CurY += margin.y1;

		int x0 = flow.Left + border.x1 + padding.x1 + spacing;
		int y0 = flow.CurY + border.y1 + padding.y1 + spacing;

		Pos.x1 = flow.Left;
		Pos.y1 = flow.CurY;
		Pos.x2 = x0 + innerW + spacing * (n - 1) + spacing + padding.x2 + border.x2 - 1;
		Pos.y2 = y0 + innerH + spacing * (rows - 1) + spacing + padding.y2 + border.y2 - 1;

		if (Pos.x2 < Pos.x1)
			Pos.x2 = Pos.x1;
		if (Pos.y2 < Pos.y1)
			Pos.y2 = Pos.y1;

		for (auto &tc : grid.Cells)
		{
			auto td = dynamic_cast<TdBlock*>(tc.Node);
			if (!td)
				continue;

			int cellW = spacing * (tc.CS - 1);
			for (int c = 0; c < tc.CS; c++)
				cellW += cw[tc.Col + c];

			int totalH = 0;
			for (int r = tc.Row; r < tc.Row + tc.RS; r++)
				totalH += RowH[r] + spacing;
			totalH -= spacing;

			int cx = x0;
			for (int c = 0; c < tc.Col; c++)
				cx += cw[c] + spacing;

			int cy = y0;
			for (int r = 0; r < tc.Row; r++)
				cy += RowH[r] + spacing;

			td->Row = tc.Row;
			td->Col = tc.Col;
			td->CellBox.Set(cx, cy, cx + cellW - 1, cy + totalH - 1);

			LRect e = CellEdges(td, cellW);

			int dy = 0;
			int va = VAlignOf(td->GetStyle());
			if (va == LCss::VerticalMiddle)
				dy = MAX(totalH - cellH[(int)(&tc - &grid.Cells[0])], 0) / 2;
			else if (va == LCss::VerticalBottom)
				dy = MAX(totalH - cellH[(int)(&tc - &grid.Cells[0])], 0);

			Flow cf(d);
			cf.pDC = flow.pDC;
			cf.Visible = flow.Visible;
			cf.Left = cx + e.x1;
			cf.Right = MAX(cf.Left, cx + cellW - 1 - e.x2);
			cf.CurY = cy + e.y1 + dy;

			td->OnLayout(cf);
		}

		flow.CurY = Pos.y2 + 1 + margin.y2;
		flow.Left = SavedLeft;
		flow.Right = SavedRight;
		return true;
	}

	bool LRichTextPriv::TableBlock::HitTest(HitTestResult &htr)
	{
		for (auto &tc : grid.Cells)
		{
			auto td = dynamic_cast<TdBlock*>(tc.Node);
			if (!td)
				continue;

			if (td->CellBox.X() > 0 && td->CellBox.Overlap(htr.In))
			{
				if (td->HitTest(htr))
				{
					if (!htr.Blk)
						htr.Blk = td;
					return true;
				}
			}
		}

		if (!grid.Cells.empty() && Pos.X() > 0 && Pos.Overlap(htr.In))
		{
			htr.Blk = this;
			htr.Idx = 0;
			htr.LineHint = 0;
			htr.Near = true;
			return true;
		}

		return DivBlock::HitTest(htr);
	}

	void LRichTextPriv::TableBlock::OnPaint(PaintContext &Ctx)
	{
		LColour Back = Ctx.Colours[Unselected].Back;
		if (Style)
		{
			LCss::ColorDef bg = Style->BackgroundColor();
			if (bg.Type == LCss::ColorRgb)
				Back.Set(bg.Rgb32, 32);
		}

		LRect r = Pos;
		if (r.X() <= 0)
		{
			DivBlock::OnPaint(Ctx);
			return;
		}

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

	bool LRichTextPriv::TableBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
	{
		BuildGrid();

		const char *name = GetElementName();

		s.Print("<%s", name);
		if (Style && Style->Name.Get() && *Style->Name.Get())
			s.Print(" class='%s'", Style->Name.Get());
		s.Print(">\n");

		bool FullList = Rng == nullptr;
		LRange All(0, Length());
		if (!Rng)
			Rng = &All;

		int nchild = MAX((int)blocks.Length(), 1);
		int ncells = MAX((int)grid.Cells.size(), 1);

		std::vector<ssize_t> ChildOff((size_t)nchild, 0);
		std::vector<int> ChildCell((size_t)nchild, -1);
		std::vector<int> ChildRow((size_t)nchild, 0);
		std::vector<bool> Hit((size_t)nchild, false);
		std::vector<int> CellChild((size_t)ncells, -1);

		ssize_t Pos = 0;
		int RowSlot = 0;

		for (int i = 0; i < nchild; i++)
		{
			auto b = blocks[i];
			ChildOff[i] = Pos;
			ChildRow[i] = RowSlot;

			LRange cr = LRange(Pos, b->Length()).Overlap(*Rng);
			Hit[i] = FullList || cr.Valid();

			if (auto td = dynamic_cast<TdBlock*>(b))
			{
				for (int k = 0; k < ncells; k++)
					if (grid.Cells[k].Node == td)
					{
						ChildCell[i] = k;
						CellChild[k] = i;
						break;
					}

				if (ChildCell[i] >= 0)
					RowSlot = grid.Cells[ChildCell[i]].Row;
			}

			Pos += b->Length();
		}

		TableGrid::Visible<Block> vis;
		TableGrid::BuildVisible(grid, vis);

		for (int i = 0; i < nchild; i++)
		{
			if (!Hit[i])
				continue;

			if (ChildCell[i] >= 0)
			{
				TableGrid::MarkHit(grid, vis, ChildCell[i]);
				continue;
			}

			// Content in the table that is not a grid cell spans every column.
			if (grid.NCols > 0)
			{
				vis.MinCol = 0;
				vis.MaxCol = grid.NCols - 1;
				if (ChildRow[i] < (int)vis.EmitRow.size())
					vis.EmitRow[ChildRow[i]] = true;
			}
		}

		TableGrid::ResolveVisible(grid, vis);

		bool status = true;

		for (int r = 0; r < (int)vis.EmitRow.size(); r++)
		{
			if (!vis.EmitRow[r])
				continue;

			if (vis.MinCol <= vis.MaxCol)
			{
				s.Print("<tr>\n");

				for (int c = vis.MinCol; c <= vis.MaxCol; c++)
				{
					int k = vis.Occ[r][c];
					if (k < 0 || vis.Top[k] != r || vis.Left[k] != c || vis.Cols[k] <= 0)
						continue;

					auto td = dynamic_cast<TdBlock*>(grid.Cells[k].Node);
					if (!td)
						continue;

					td->OpenTag(s, vis.Cols[k], vis.Rows[k]);

					if (!Hit[CellChild[k]])
					{
						// Keep the position of a cell whose content is outside the
						// selection, with the style that gives it its borders.
						s.Print("</%s>\n", td->GetElementName());
						continue;
					}

					int i = CellChild[k];
					LRange cr = LRange(ChildOff[i], td->Length()).Overlap(*Rng);
					if (!FullList)
						cr.Start -= ChildOff[i];

					if (!td->ToHtmlContent(s, Media, FullList ? nullptr : &cr))
						status = false;
				}

				s.Print("</tr>\n");
			}

			// Children that are not grid cells are emitted as their own row.
			for (int i = 0; i < nchild; i++)
			{
				if (ChildCell[i] >= 0 || ChildRow[i] != r || !Hit[i])
					continue;

				auto b = blocks[i];

				s.Print("<tr>\n");
				s.Print("<td colspan='%d'>\n", MAX(vis.MaxCol - vis.MinCol + 1, 1));

				LRange cr = LRange(ChildOff[i], b->Length()).Overlap(*Rng);
				if (!FullList)
					cr.Start -= ChildOff[i];

				if (!b->ToHtml(s, Media, FullList ? nullptr : &cr))
					status = false;

				s.Print("</td>\n");
				s.Print("</tr>\n");
			}
		}

		s.Print("</%s>\n", name);
		return status;
	}

	#ifdef _DEBUG
	void LRichTextPriv::TableBlock::DumpNodes(LTreeItem *blockItem)
	{
		blockItem->SetText(LString::Fmt("%s %dx%d", GetClass(), grid.NCols, (int)grid.Rows.size()));

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
}
