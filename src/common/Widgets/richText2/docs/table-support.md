# RTE2 Table Support

## Current state

At the moment the RTE2 control in 'lgi/trunk/src/common/Widgets/richText2' doesn't support tables. They are striped from the input HTML and never reconstituted on the other side. This is problematic for the Scribe email client when configured to use BodyAsHtml style replies. At the very least it should conserve the table inside the quoted part of the email so it can be carried through to the output.

There are currently several HTML table style layout engines in Lgi. Perhaps one or more could be generalized to be used in the RTE2 control. For instance the LHtml2 control in 'lgi/trunk/src/common/Text/Html2.cpp' has table layout functionality.

This document is the start of a design for adding table support. A new TableBlock element will need to be created, living in a 'TableBlock.cpp' file. The parsing of HTML will need to create those as needed. Likely there may also need to be TrBlock and TdBlock? But the code for those can stay in the same TableBlock.cpp.

## Research notes

### Where tables are lost today

`FromHtml()` in `RichTextEditPriv.cpp` has no special case for table tags. Block-level tags such as `table`, `tr`, and `td` only reach the generic path, which ends the previous paragraph (`IsBlock` check near `RichTextEditPriv.cpp:3000-3046`) and lets the text fall into the current `TextBlock`. The result is flattened paragraphs in source order. The explicit background-colour removal for `TAG_TABLE/TBODY/TR/TD/TH` at `RichTextEditPriv.cpp:3091-3108` is a direct acknowledgement that these are flattened containers today: backgrounds cannot be painted because there is no box to paint them on. Once real blocks exist, that removal should be limited to the flattened path and cell backgrounds should survive as box properties.

The shared element table is in `HtmlCommon.cpp:192-195` (`TI_TABLE` flags, `td/th` carry text directly). `HtmlParser.cpp:947-1044` already repairs table structure (implicit `tbody`/`tr` creation), so the HTML tree handed to RTE2 is well-formed. `HtmlCommon.h:45-56` has the `HtmlTag` enum values needed (`TAG_TABLE`, `TAG_TR`, `TAG_TD`, `TAG_TH`, `TAG_TBODY`); no parser work is needed there, only the block-creation case in `FromHtml()`.

### Existing engines

Three table layout implementations exist in Lgi:

1. `lgi/trunk/src/common/Text/Html2.cpp` — the best model. A CSS 2.1 engine over `LTag` nodes with a clean pipeline worth generalizing:
   - `BuildGrid()` (`Html2.cpp:1025`) builds the row/cell grid, including anonymous rows from cells without an explicit `tr`, and resolves `colspan`/`rowspan` into a `used` occupancy map (`TCell{Row, Col, CS, RS}`).
   - `Measure()`/`MeasureText()` (`Html2.cpp:1181,1154`) produce per-cell min/max content widths (min = widest unbreakable word, max = single-line run), which feed `ColumnMetrics()` (`Html2.cpp:1442`) including spanned-column distribution and fixed/percent column preferences.
   - `SolveColumns()` (`Html2.cpp:1549`) is the CSS 2.1 column-width algorithm against the available table width.
   - `LayoutTable()` (`Html2.cpp:1655`) positions rows, then `PlaceCell()` (`Html2.cpp:1642`) applies `vertical-align` as a post-pass shift after row heights are known. `SpacingOf()` covers `border-collapse` vs `border-spacing`.
   - Cell box painting via `PaintBorderAndBackground()` using `LCssBox<>`.
2. `lgi/trunk/src/common/Widgets/TableLayout.cpp` (`LTableLayout`) — window/control-based layout where child views are cells. Wrong shape for RTE2; useful only as a second reference for grid bookkeeping.
3. `Html.cpp` (`OldFlow` era) — known buggy; do not carry behavior over (see `refactoring-html2.md`).

The grid model and column solver are data structures + algorithms over generic elements, not dependent on `LTag` itself. Extracting them into a shared header (grid, per-column min/max/pct/fixed, the solver, the span-aware distribution) would let `TableBlock` reuse the same code that `Html2` uses, rather than writing a second column algorithm.

### RTE2 constraints that shape the design

- The `Block` interface (`RichTextEditPriv.h:656-832`) has ~20 pure virtuals (layout, paint, hit-test, offsets, lines, find, copy, clone, `ToHtml`, `DumpNodes`…). `ListBlock` is the closest working model of a container: offset-to-child delegation by source order (`ListBlock.cpp:53-110`), `GetTextBlock()` guaranteeing an editable leaf (`ListBlock.cpp:28-46`), `ToHtml()` emitting the wrapper plus overlapped children only for partial ranges (`ListBlock.cpp:229-257`).
- `Flow` (`RichTextEditPriv.h:438-467`) is single-pass vertical flow: `Left`, `Right`, `CurY`. There is no measure pass. Tables fundamentally need two passes — column widths require content measurement before placement, and `vertical-align: middle/bottom` requires row heights before cell placement. Options:
  - v1 single pass: column widths from measurement or equal split of `Right-Left`; top-aligned cells only. Acceptable for conserving quoted tables in Scribe, not faithful CSS.
  - proper pass: add a `Measure()`-style min/max interface to blocks, or give `TableBlock` an internal measure that walks its cells' text, then lay out. Recommend starting with the measure-plus-solve approach because the algorithms already exist in `Html2`.
- Row-major child order in the block tree maps naturally to offsets, so `ListBlock`-style delegation works for `Length()`, `OffsetToLine()`, `HitTest()`, `CopyAt()`, `FindAt()`, `ChangeStyle()`, `DeleteAt()`. `HitTest()` must additionally resolve point → grid cell → content block.
- `Split()` inside a cell should split the cell's `TextBlock` (grow the cell), never the table. `ListBlock::Split()` returning `NULL` (`ListBlock.cpp:554-557`) is the precedent for the container itself.
- `Enter`/list toggling at the editor level edits `blocks` under transactions (`RichTextEditPriv.cpp:2297-2330`); any structural table mutation (add/remove row or cell) must participate in `Transaction` + undo the same way.

### Recommended structure

- `TableBlock` — container block, `LCssBox<LRect>` (like `DivBlock`), owns the grid: row count, column widths, and per-cell spans recomputed at layout. Owns table CSS: `width`, `border`, `border-collapse/spacing`, caption margins.
- Cells are the actual block children. `TdBlock` can be a thin `DivBlock` subclass (or `DivBlock` itself with `ElementName = "td"/"th"`, per the blockquote doc's rule: serialize with `GetElementName()`, not `GetClass()`) plus stored `ColSpan`/`RowSpan`. Each cell guarantees one editable `TextBlock` child, mirroring the empty-quote rule.
- `TrBlock` as a tree node is not recommended: rows are only needed as grouping metadata for the grid (`vertical-align`, row height, serialization grouping), and every extra `Block` level complicates offset delegation for no benefit. Keep a `Row` struct inside `TableBlock` instead. If debug/tree output wants rows visible, show them in `DumpNodes()` from the grid, not as `Block` objects.
- Child styles should inherit text properties but not table/cell box properties, same rule as `DivBlock::MakeChildStyle()`.

### Parser sketch

In `FromHtml()`, before the generic block path: on `TAG_TABLE` create `TableBlock` (as `blockquote` does at `RichTextEditPriv.cpp:3112-3172`): end the current paragraph if not at line start, `ctx.AddBlock(TableBlock)`, then walk children → rows → cells. For each `td/th` create `TdBlock`, set its `TagId`/`ElementName` and span from the parsed attributes (`colspan`/`rowspan` are read in `Html2.cpp:4182-4186`; RTE2 needs its own read), save/restore `ctx.TargetBlocks/TargetParent/Tb` around recursion so nested content, lists, images, and nested tables land inside the cell. Empty cells get a placeholder `TextBlock`. Because `FromHtml` is already recursive over `TargetBlocks`, tables nested inside a `DivBlock` blockquote work once this case exists — which is exactly the Scribe BodyAsHtml quoted-reply requirement.

### Serialization

`ToHtml()` must emit `<table>`, `<tr>`, `<td>`/`<th>` with `colspan`/`rowspan` restored from the grid and `class`/`style` from stored styles, using the ListBlock partial-range pattern: full range → whole table; partial range → wrapper plus only overlapped rows/cells. Recomputing the grid for a partial export must keep the original column indices, otherwise a selection covering one cell of a 3-column table serializes as a 1-column table.

### Known hard cases to record as limitations for v1

- Column width fidelity: percent widths against the page width, and min/max column rules, need the measure pass; equal-split fallback will visibly differ from `Html2`.
- `vertical-align` middle/bottom needs row-height post-pass.
- Nested tables inside cells require the layout to be recursive through the measure path, not plain `Flow`.
- Whitespace-only text between `td` elements must not create stray content blocks inside the table (`Html2.cpp:1048-1063` skips it; the RTE2 parser should not emit a `TextBlock` for it either).
- Deleting a selection spanning several cells should remove fully covered cells (leaving their row skeleton) or the whole table when fully covered; partial-cell text trim delegates as usual.
- Cell count/structure changes (add row/column) via context menu are follow-up work; v1 is import + round-trip + editing text inside cells.

### Minimum viable scope for the Scribe case

Round-trip conservation does not require a faithful CSS engine. Minimum: `TableBlock` + thin `TdBlock` in `TableBlock.cpp`, parser case, serialization, simple single-pass layout with content-measured or equal columns and top alignment, cells as `DivBlock`-style containers. That satisfies "conserve the table inside the quoted part of the email". `Html2`'s grid + column solver is the upgrade path for layout fidelity.

### Test cases

Build via the same targets as `blockquote-support.md` (`test/HtmlEditor/linux/Makefile.linux` or `scribe/trunk_os/linux/Makefile.linux`).

Round-trip:

```html
<table class='styled'><tr><td>a</td><td>b</td></tr><tr><td colspan='2'>c</td></tr></table>
```

Round-trip with rowspan; table inside blockquote (Scribe reply path); table containing a list, an image, and a nested table; partial selection covering two cells of a 3x2 grid; cursor entering an empty cell; Enter inside a cell (must grow the cell, not split the table); paste of a table fragment; background/border on `td` preserved once flattened-path removal is scoped.

Expected tree for the first case:

```text
TableBlock table
  TdBlock td [row 0, col 0, span 1x1]
    TextBlock: a
  TdBlock td [row 0, col 1, span 1x1]
    TextBlock: b
  TdBlock td [row 1, col 0, span 2x1]
    TextBlock: c
```

(Rows shown as grid metadata, not block nodes.)

The ability to edit the table will come later but the design for that can be done now.

## Implementation progress

### Shared table engine (step 1, done)

The grid model and column solver were extracted into `lgi/trunk/include/lgi/common/TableGrid.h` (header only, so no platform makefile/project changes are needed). It contains no `Lgi.h` dependencies, so both `Html2` and the RTE2 `TableBlock` can use it:

- `TableGrid::MinMax`, `TableGrid::Cell<T>` (node pointer, Row/Col/CS/RS, per-cell content min/max, percent/fixed width preference), `TableGrid::Grid<T>` (rows hold node pointer, group, and cell indices; `NCols`), `TableGrid::Columns` (min/max/pct/fixed, spacing).
- `PlaceCells()` — occupancy placement of spans, `rowspan` clamping, `NCols` (formerly inline in `BuildGrid()`).
- `ColumnMetrics()` — fold single-span cells, distribute spanned-cell measurements across spanned columns with spacing gaps.
- `Distribute()`, `PreferredWidth()` (percent-column fit rule), `SolveColumns()` — the CSS 2.1 column distribution with last-column remainder rounding.

`Html2.cpp` now aliases these (`Html2.cpp:282,291-293`: `TCell`/`TGrid`/`TCols` are `TableGrid::Cell<LTag>` etc.) and delegates `BuildGrid()` (`Html2.cpp:998`, calls `TableGrid::PlaceCells` at `1074`), `ColumnMetrics()` (`Html2.cpp:1377`, engine wrapper that fills per-cell measurement then calls `TableGrid::ColumnMetrics`), and `MeasureTable()`/`LayoutTable()` to the shared `PreferredWidth`/`SolveColumns` (`Html2.cpp:1421,1531`). Engine-specific parts stayed in `Html2`: `Measure()`/`MeasureText()`, `SpacingOf()`, `VAlignOf()`, `PlaceCell()` vertical-align post-pass (`Html2.cpp:1470`), and `LCssBox` painting. Verified by compiling `Html2.cpp` with the Scribe build flags; behavior is line-for-line equivalent to the pre-refactor algorithm.

The node type is a template parameter so `TableBlock` in RTE2 can instantiate `TableGrid::Grid<Block>` and feed column metrics from a cell content measure (per the `MeasureText`-style min/max in the design above), while `Flow`-based placement continues to use `SolveColumns()` output for column widths.

