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
		SelectModeType OldType = Ctx.Type;

		if (Ctx.Select && Ctx.Select->Blk == this)
			Ctx.Type = Selected;

		LRect r = GetPos();
		if (r.X() <= 0)
		{
			Ctx.Type = OldType;
			DivBlock::OnPaint(Ctx);
			return;
		}

		LColour Back = Ctx.Back();
		if (Ctx.Type != Selected && Style)
		{
			LCss::ColorDef bg = Style->BackgroundColor();
			if (bg.Type == LCss::ColorRgb)
				Back.Set(bg.Rgb32, 32);
		}

		Ctx.pDC->Colour(Back);
		Ctx.pDC->Rectangle(r.x1, r.y1, r.x2, r.y2);

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

		Ctx.Type = OldType;
	}

	bool LRichTextPriv::TdBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
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
		}

		if (auto box = dynamic_cast<LCssBox<LRect>*>(b))
		{
			LNamedStyle *st = b->GetStyle();
			if (st && font)
			{
				LRect pb;
				pb.Set(0, 0, MAX(avail, 1), MAX(avail, 1));
				box->SetStyle(font, st, pb);

				if (dynamic_cast<TdBlock*>(b))
					box->margin.ZOff(0, 0);

				int ex = box->border.x1 + box->border.x2 + box->padding.x1 + box->padding.x2;
				int ey = box->border.y1 + box->border.y2 + box->padding.y1 + box->padding.y2;

				if (ex || ey)
				{
					r.Min += ex;
					r.Max += ex;
					r.Has = true;
				}
			}

			if (auto td = dynamic_cast<TdBlock*>(b))
			{
				if (td->Fixed && td->FixedPx > r.Max)
					r.Max = td->FixedPx;
			}
		}

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

		int spacing = Spacing();

		TableGrid::Columns cols;
		cols.Spacing = spacing;

		for (auto &tc : grid.Cells)
		{
			auto td = dynamic_cast<TdBlock*>(tc.Node);
			if (!td)
				continue;

			tc.m = MeasureBlock(td, 0, avail);
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
			}
		}

		TableGrid::ColumnMetrics(grid, cols);

		int fixedX = spacing * (n + 1) + border.x1 + border.x2 + padding.x1 + padding.x2;
		int outer = TableGrid::PreferredWidth(cols) + fixedX;

		if (st)
		{
			LCss::Len w = st->Width();
			if (w && w.Type == LCss::LenPx)
				outer = MIN(outer, MAX(1, w.ToPx(avail, font)));

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

			RowH[r] = MAX(rh, font->GetHeight());
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

		flow.Left += margin.x1;
		flow.Right -= margin.x2;
		flow.CurY += margin.y1;

		int x0 = flow.Left + spacing;
		int y0 = flow.CurY + spacing;

		Pos.x1 = flow.Left;
		Pos.y1 = flow.CurY;
		Pos.x2 = Pos.x1 + innerW + spacing * (n - 1) + spacing * 2 - 1;
		Pos.y2 = Pos.y1 + innerH + spacing * (rows - 1) + spacing * 2 - 1;

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
		SelectModeType OldType = Ctx.Type;

		if (Ctx.Select && Ctx.Select->Blk == this)
			Ctx.Type = Selected;

		LColour Back = Ctx.Back();
		if (Ctx.Type != Selected && Style)
		{
			LCss::ColorDef bg = Style->BackgroundColor();
			if (bg.Type == LCss::ColorRgb)
				Back.Set(bg.Rgb32, 32);
		}

		LRect r = Pos;
		if (r.X() <= 0)
		{
			Ctx.Type = OldType;
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

		Ctx.Type = OldType;
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

		// Cell offsets are in blocks order, which is the same as grid order.
		std::vector<ssize_t> CellOff(grid.Cells.size(), 0);
		ssize_t Pos = 0;
		for (unsigned i = 0; i < blocks.Length(); i++)
		{
			auto td = dynamic_cast<TdBlock*>(blocks[i]);
			if (!td)
				continue;

			for (size_t k = 0; k < grid.Cells.size(); k++)
				if (grid.Cells[k].Node == td)
					CellOff[k] = Pos;

			Pos += td->Length();
		}

		bool status = true;

		for (size_t r = 0; r < grid.Rows.size(); r++)
		{
			auto &row = grid.Rows[r];
			if (row.Cells.empty())
				continue;

			std::vector<bool> Emit(row.Cells.size(), false);
			int maxCol = 0;
			bool any = false;

			for (size_t k = 0; k < row.Cells.size(); k++)
			{
				auto &tc = grid.Cells[row.Cells[k]];
				auto td = dynamic_cast<TdBlock*>(tc.Node);
				if (!td)
					continue;

				LRange cr = LRange(CellOff[row.Cells[k]], td->Length()).Overlap(*Rng);
				if (FullList || cr.Valid())
				{
					Emit[k] = true;
					any = true;
					maxCol = MAX(maxCol, tc.Col + tc.CS - 1);
				}
			}

			if (!any)
				continue;

			s.Print("<tr>\n");

			for (size_t k = 0; k < row.Cells.size(); k++)
			{
				auto &tc = grid.Cells[row.Cells[k]];
				auto td = dynamic_cast<TdBlock*>(tc.Node);
				if (!td)
					continue;

				if (!Emit[k])
				{
					// Emit an empty cell to keep the original column index.
					if (tc.Col < maxCol)
					{
						s.Print("<td");
						if (tc.CS > 1)
							s.Print(" colspan='%d'", tc.CS);
						if (tc.RS > 1)
							s.Print(" rowspan='%d'", tc.RS);
						s.Print("></td>\n");
					}
					continue;
				}

				LRange cr = LRange(CellOff[row.Cells[k]], td->Length()).Overlap(*Rng);
				if (!FullList)
					cr.Start -= CellOff[row.Cells[k]];

				if (!td->ToHtml(s, Media, FullList ? nullptr : &cr))
					status = false;
			}

			s.Print("</tr>\n");
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
