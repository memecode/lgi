# RTE2 Table Support

## Current state

At the moment the RTE2 control in `lgi/trunk/src/common/Widgets/richText2` has a partial table implementation. Tables are no longer completely stripped by the basic `FromHtml()`/`ToHtml()` round trip: `TableBlock`, `TdBlock`, `FromHtml()`, `ToHtml()`, `TableGrid`, and measurement work are present and conserve table structure for the Scribe `BodyAsHtml` quoted-reply path. The active work is border serialization fidelity and td/th style inheritance. Historically tables were stripped from the input HTML and never reconstituted, which was problematic for the Scribe email client when configured to use `BodyAsHtml` style replies.

There are currently several HTML table style layout engines in Lgi. Perhaps one or more could be generalized to be used in the RTE2 control. For instance the LHtml2 control in 'lgi/trunk/src/common/Text/Html2.cpp' has table layout functionality.

This document is the start of a design for adding table support. A new TableBlock element will need to be created, living in a 'TableBlock.cpp' file. The parsing of HTML will need to create those as needed. Likely there may also need to be TrBlock and TdBlock? But the code for those can stay in the same TableBlock.cpp.

How to build the RTE2 control:
- Test harness:
    make -C lgi/trunk/test/HtmlEditor -f linux/Makefile.linux
- Scribe email client:
    make -C scribe/trunk_os -f linux/Makefile.linux

## Round Trip Testing:

To confirm the behavior of the table support I've set up a round trip test:

The input file:
    ~/code/lgi/trunk/test/HtmlEditor/testcases/example-reply.html

To call the editor:
    cd ~/code/lgi/trunk/test/HtmlEditor
    ./htmleditor testcases/example-reply.html -export reply-out.html

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

### Round-trip check (testcases/example-reply.html)

Run through the harness as above and compared the input with `reply-out.html`.
Aside from the span duplication fixed above, the output is structurally correct but
not faithful.

The border and td/th inheritance bullets below were historical findings from the
initial round trip. Current border fidelity and td/th inheritance behavior is
summarized in `Current working state`.

- Historically invalid CSS was emitted: `border:Inherit none` / `border-top:Inherit none`
  (browsers drop the whole declaration), `cellpadding: Npx` as a CSS property (should be
  `padding`), and `font-family: sans-serifVerdana,Geneva,Arial,Helvetica` with the
  generic family glued to the list. The `border:Inherit none` case is fixed by the
  border helper. `cellpadding` is still emitted as a CSS property, but td styles no
  longer inherit it from the table.
- Historically cell and span styles inherited the table's `width: 600px`, and cell
  styles picked up `border-left`/`border-top` from the `table` rule, which does not
  inherit borders. This is fixed for td/th styles by stripping inherited table box
  properties before td/th selector matching. `table.page_header td { border:none }`
  now exports as valid `border: none`.
- td percentage `width` (`50%`/`100%`) is now preserved through the stripped style path.
- Other round-trip losses remain historical/current gaps: presentation `align`
  on some `td`/`th` cases (`text-align: left` may not be emitted, and the
  right-aligned headers can serialize as centre), `<font color="red">`,
  `&nbsp;` runs (27 → 9), `<h2>`, the `div.scribe_reply`/`div.scribe_sig`
  wrappers, `<tr bgcolor>`, `lang`/`target`/`rel`, and the named classes
  (all flattened to `StyleN`).
- Artifacts: `.Style1` is emitted in the style block but never used (the input span
  that used it lost its class), an empty `<p></p>`, and a second `<a>` wrapping only a
  space from a source newline.

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

## Current implementation state

### Files touched

- `lgi/trunk/src/common/Widgets/richText2/RichTextEditPriv.h`
  - Includes `lgi/common/TableGrid.h` after `SpellCheck.h`.
  - Adds `TdBlock` and `TableBlock` declarations after `DivBlock` and before `HorzRuleBlock`.
  - Adds table context fields to `CreateContext`:
    - `Block *TableTarget = nullptr;`
    - `int TableRow = -1;`
    - `LCss::Len CellPadding;`
    - Existing fields remain: `TargetBlocks`, `TargetParent`, `Tb`, `LastChar`, `StartOfLine`.

- `lgi/trunk/src/common/Widgets/richText2/RichTextEditPriv.cpp`
  - Adds static table presentation helpers after the closing `};` of `HtmlElementCb`, before `FromHtml`.
  - Adds table handling in `FromHtml()`.
  - Captures `HadCssWidth` before presentation attributes so presentation `width` does not override CSS width.
  - Restricts background removal to flattened containers while preserving `TD` and `TH` backgrounds.
  - Creates `TdBlock` directly inside `FromHtml()` to avoid private constructor access issues.

- `lgi/trunk/src/common/Widgets/richText2/TableBlock.cpp`
  - New file for `TdBlock` and `TableBlock`.

- `lgi/trunk/include/lgi/common/TableGrid.h`
  - Adds `Cell::MinH`, `RowMinHeights()`, and the partial-export visibility model
    (`Visible`, `BuildVisible()`, `MarkHit()`, `ResolveVisible()`). Header only, so
    no makefile or project changes are needed.

- `lgi/trunk/src/common/Widgets/richText2/RichTextEditPriv.h`
  - `ListBlock::Marker()` is public so `TableBlock::MeasureBlock()` can measure the
    marker column.
  - `TdBlock` gains `OpenTag()` and `ToHtmlContent()` so `TableBlock::ToHtml()` can
    emit an adjusted opening tag for a cell.
  - `TableBlock` gains `FillMetrics()` and `MeasureColumns()`.

- `scribe/trunk_os/linux/Makefile.linux` and `scribe/trunk_os/CMakeLists.txt`
  - `TableBlock.cpp` entries added during this pass (they were missing).

- `lgi/trunk/test/HtmlEditor/testcases/table-test.html`
  - New manual round-trip/layout test case for the HtmlEditor GUI harness.

### Implemented HTML parsing behavior

Static helpers currently inserted:

- `EnsureStyle`
- `IsTableContextTag`
- `ParseAttrInt`
- `AttrValueIs`
- `IsAllWhiteW`
- `ApplyAlign`
- `ApplyVerticalAlign`
- `ApplyLength`
- `ApplyBorder`
- `ApplyBackground`
- `ApplyTablePresentation`
- `ApplyRowPresentation`
- `ApplyCellPresentation`

Supported table tags are limited to those available in `HtmlCommon.h`:

- `TAG_TABLE`
- `TAG_TR`
- `TAG_TD`
- `TAG_TH`
- `TAG_TBODY`

Table-related tags not confirmed in `HtmlCommon.h`, such as `TAG_COL`, `TAG_COLGROUP`, `TAG_THEAD`, and `TAG_TFOOT`, are currently skipped as unknown tags when encountered inside table context.

Current parser behavior:

- Creates `TableBlock` on `TAG_TABLE`.
- Creates rows as parser/grid context, not as `Block` tree nodes.
- Creates `TdBlock` cells on `TAG_TD` and `TAG_TH`.
- Saves and restores table context fields around recursion so nested content, lists, images, and nested tables land inside the cell.
- Whitespace-only text between table elements is skipped instead of producing stray content blocks.
- Direct non-whitespace cell text is placed in the cell’s `TextBlock`.
- Empty cells get a placeholder `TextBlock`.
- Cell content is added through `TdBlock` target blocks.
- Nested tables inside `TD`/`TH` are handled recursively.

Presentation attributes currently supported in `FromHtml()`:

- Table `align`, `width`, `border`, `cellspacing`, `cellpadding`, `bgcolor`.
- Cell `align`, `valign`, `width`, `border`, `bgcolor`.
- Row `align` and `valign` only (`ApplyRowPresentation`), because `TR` is grid metadata and cannot paint a background box.
- `bgcolor` is resolved to `LCss::PropBackgroundColor` through `LHtmlParser::ParseColour()`, so hex (`#336699`), named (`yellow`) and `rgb()` values survive as box properties. Cell `border` is mapped to the CSS border properties through `ApplyBorder()`.
- Cell `height` is not applied: the shared column solver has no per-row minimum height, so there is nowhere to store it yet.
- `colspan` and `rowspan` on `TD`/`TH`.
- `TH` default bold is applied only when font weight is inherit/not already set.
- Presentation width behavior:
  - Percent width sets `TdBlock::Pct`.
  - Pixel width sets `TdBlock::Fixed` and `TdBlock::FixedPx`.
  - If CSS width was already present, CSS width should win over presentation width.

### Style behavior

- `MakeChildStyle()` strips box properties from child styles.
- `cellpadding` must remain available through `TableBlock::CellPadding`, `TdBlock::GetStyle()`, or explicit edge calculation. `CellEdges()` applies the `cellpadding` fallback when a cell has no padding of its own, and `MeasureBlock()` now measures cells through `CellEdges()` so column min/max match what placement uses.
- `TD` and `TH` background properties are preserved. `TABLE` backgrounds are no longer removed because `TableBlock` is a real box that can paint them; `DIV`, `TBODY`, and `TR` are still flattened containers and their backgrounds are still removed so they do not become highlights on descendant text.
- `MakeChildStyle()` does not strip `background-color`, so text inside a coloured cell blends glyphs with the cell colour through `Cols.Back`, which is what `TextBlock::OnPaint` uses.
- Serialization uses `GetElementName()` where present, otherwise tag info, so cells serialize as `td` or `th`. Presentation `width` and `bgcolor` round-trip through the cached style (`class='stN'` plus `OutputStyles()`), not as HTML attributes, so `TdBlock::Pct`/`Fixed`/`FixedPx` only need to drive layout.

### Build integration

Build entries for `TableBlock.cpp` are now in the actual build systems. The Scribe entries were missing when this was re-checked (only the HtmlEditor ones were present), so Scribe would have failed to link against `TableBlock`/`TdBlock`; they have been added:

- `lgi/trunk/test/HtmlEditor/linux/Makefile.linux`
  - Added after `RichTextEditPriv.cpp` and before `TextBlock.cpp`.
- `scribe/trunk_os/linux/Makefile.linux`
  - Added after `RichTextEditPriv.cpp` and before `TextBlock.cpp` (this was missing before).
- `lgi/trunk/test/HtmlEditor/CMakeLists.txt`
  - Added after `ImageBlock.cpp` and before `TextBlock.cpp`.
- `scribe/trunk_os/CMakeLists.txt`
  - Added after `ImageBlock.cpp` and before `TextBlock.cpp` (this was missing before).

Important invocation note: these Makefiles should be invoked from the project directory, not from the `linux` subdirectory, because `VPATH` is relative to the current directory.

Use:

```sh
make -f linux/Makefile.linux
```

from:

```text
/home/matthew/code/lgi/trunk/test/HtmlEditor
```

for HtmlEditor, and from:

```text
/home/matthew/code/scribe/trunk_os
```

for Scribe. Direct `make` from `test/HtmlEditor/linux` or `scribe/trunk_os/linux` fails for object targets because the Makefile’s VPATH search paths are evaluated relative to the wrong working directory.

### Verified so far

- `TableBlock.cpp` compiles with GCC C++17 `-fpermissive`, including a clean rebuild of `Debug/TableBlock.o` from the HtmlEditor project directory.
- `RichTextEditPriv.cpp` compiles with GCC C++17 `-fpermissive` after the `HtmlElementCb` closing-brace fix.
- Debug compilation passes for `RichTextEditPriv.cpp` and `TableBlock.cpp`, which exercises the `#ifdef _DEBUG` `DumpNodes()` path, so `LTreeItem` availability is confirmed for the makefile Debug builds. The CMake builds do not define `_DEBUG`, so that path is not compiled there.
- Full HtmlEditor makefile build from the project directory succeeded and linked `./htmleditor`.
- Full Scribe CMake build (`ninja scribe`) succeeded and linked `scribe` once `TableBlock.cpp` was added to `CMakeLists.txt`; `ninja HtmlEdit` also linked `test/HtmlEditor/HtmlEdit`.
- Single-object builds prove both Makefile and CMake integration:
  - `make -f linux/Makefile.linux Debug/TableBlock.o` from `scribe/trunk_os`.
  - `HtmlEdit` and `scribe` object targets in the existing ninja build trees.
- No `.vcxproj` files exist in this checkout, so MSVC entries are not applicable here and remain unverified.
- `ninja HtmlEdit` builds and links after the serialization, nested-measurement, row-height and list-marker changes.
- The header-only parts of `TableGrid.h` were exercised by a standalone C++17 test program (no Lgi link required), covering `RowMinHeights()` spanned-height distribution and the partial-export visibility model (`BuildVisible()`/`MarkHit()`/`ResolveVisible()`): span shortening for a rowspan that starts above the selection, rows without selected content not being emitted, and column trimming. The test lives outside the repo (`/tmp/kilo/tablegrid_test.cpp`) and is not part of the build.

### Runtime semantics review

Fixes applied in `TableBlock.cpp` during this pass:

- `TableBlock::OnLayout()` now places cells from the table padding-box origin:
  - `x0 = flow.Left + border.x1 + padding.x1 + spacing`
  - `y0 = flow.CurY + border.y1 + padding.y1 + spacing`
  and `Pos.x2`/`Pos.y2` cover the full table border box. Previously both the cell origin and `Pos` omitted the table border and padding, so painted cells could fall outside `GetPos()`.
- `TableBlock::OnLayout()` saves and restores `flow.Left`/`flow.Right` like `DivBlock::OnLayout()`. The top-level layout loop reuses the same `Flow` for every block, so the table was leaking its insets into following blocks.
- `MeasureBlock()` measures cells through `CellEdges()`, so the `cellpadding` fallback is counted in `cols.Min`/`cols.Max` and column widths match what the placement pass uses. Cell margins are zeroed inside `CellEdges()`.
- `TdBlock` keeps `Pos` as the content box and `GetPos()` returns `CellBox`; `ContentH` is the content height used for the `vertical-align` shift. Row heights come from the cell measurement, and spanned cells distribute extra height with `TableGrid::Distribute()`.
- Hit-testing resolves point → grid cell → content block (`TableBlock::HitTest`), with `TdBlock::HitTest` delegating to its children and falling back to the cell box at index 0.
- `Split()` returns null through `DivBlock`, so Enter inside a cell grows the cell instead of splitting the table, and `GetTextBlock()` guarantees one editable leaf per cell.
- `TableBlock::ToHtml()` exports a partial selection through a grid occupancy map
  (`TableGrid::BuildVisible()` / `MarkHit()` / `ResolveVisible()`): only rows that
  contain selected content survive, a `rowspan` cell that starts above the
  selection is placed in the first surviving row with its rowspan shortened, and
  skipped cells inside the surviving range are emitted as placeholders that keep
  their `class`, so cell border/background survive the round trip. Children of the
  table that are not grid cells are no longer dropped; they are emitted as their
  own row spanning the surviving columns. Empty cells serialize as `<td></td>`
  instead of `<td><p></p></td>`.

Fixed in this pass:

- Nested tables inside a cell are measured through the column solver:
  `TableBlock::MeasureColumns()` runs `FillMetrics()` on the nested grid, so a
  nested table contributes real column metrics to the parent measurement instead
  of being treated as plain content. Recursion is depth-limited to 16.
- `TableBlock::ToHtml()` emits each cell once, at the first surviving column it
  occupies (`TableGrid::Visible::Left`, filled by `ResolveVisible()`). Without an
  origin-column check a `colspan > 1` cell was printed once per spanned column, so
  a browser lays out phantom cells and the table visually splits in two. Verified
  with `testcases/example-reply.html`: the `colspan='4'` cell now serializes once
  and every row's column count matches the input.
- Partial selection export uses grid occupancy, as described above.
- `ListBlock` markers are counted in the cell measurement: `MeasureBlock()` adds
  the marker column width (font height for bullets, the widest marker plus half a
  font height for ordered lists), matching the indent computed in
  `ListBlock::OnLayout()`.
- Per-row minimum heights are supported: `TableGrid::Cell::MinH` is filled from
  the cell `height`/`min-height`, `TableGrid::RowMinHeights()` distributes a
  spanned cell's height over the rows it covers, and `TableBlock::OnLayout()`
  applies the result as a lower bound on each row. The `height` presentation
  attribute on `<td>`/`<th>` and `<tr>` is parsed through `ApplyHeight()`.

Remaining semantic gaps:

- `border-collapse: collapse` is simplified: spacing is forced to 0 and each cell
  draws its own border; shared-border merging and the table's own collapsed border
  are not implemented.
- Percentage `height` cannot be resolved because the table height is unknown during
  measurement, so percent heights are ignored.
- `TAG_COL`, `TAG_COLGROUP`, `TAG_THEAD` and `TAG_TFOOT` are not represented as
  blocks, so their attributes and styling are dropped by the parser.

## Current working state (border fidelity and td/th inheritance)

Exact commands used for this pass:

```sh
cd ~/code/lgi/trunk/test/HtmlEditor
make -C lgi/trunk/test/HtmlEditor -f linux/Makefile.linux
./htmleditor testcases/example-reply.html -export reply-out.html
```

### Files changed for this pass

- `lgi/trunk/src/common/Lgi/Css.cpp`
  - Static helpers use `LCss::` qualified names:
    - `CssBorderStyleName(LCss::BorderStyle Style)`
    - `IsBorderSide(LCss::PropType Prop)`
    - `BorderDefEquals(LCss::BorderDef &A, const LCss::BorderDef &B)`
    - `PrintBorderDef(LStream &p, LCss::BorderDef &B, bool Shorthand)`
  - `PrintBorderDef()` keeps `B` non-const because `B.Color.ToString(p)` and `((LCss::Len&)B).ToString(p)` require non-const access.
  - `ToStringPipe()` `case TypeBorder:` skips redundant border side properties when a `PropBorder` shorthand exists and the side is equal to the shorthand.
  - `Parse()` `case TypeBorder:` propagates `PropBorder` shorthand to `PropBorderTop`, `PropBorderRight`, `PropBorderBottom`, and `PropBorderLeft` when `PropId == PropBorder`.

- `lgi/trunk/src/common/Widgets/richText2/RichTextEditPriv.cpp`
  - Added `StripCellInheritedBoxProps(LCss *Style)`.
  - It is called for `TAG_TD` and `TAG_TH` in `FromHtml()` before td/th selector matching, so td/th CSS rules, inline styles, and presentation attributes can re-apply their own properties after inherited table box properties are stripped.

- `lgi/trunk/include/lgi/common/TableGrid.h`
  - `TableGrid::Visible` has `Left[]`.
  - `BuildVisible()` initializes `Left[]`.
  - `ResolveVisible()` fills `Left[]` with the first surviving column for each cell.

- `lgi/trunk/src/common/Widgets/richText2/TableBlock.cpp`
  - `TableBlock::ToHtml()` emits a cell only at the first surviving column it occupies:
    ```cpp
    if (k < 0 || vis.Top[k] != r || vis.Left[k] != c || vis.Cols[k] <= 0)
    ```

### TD/th style inheritance decisions

- TD/th styles should strip inherited table box properties before td/th CSS selector matching.
- `StripCellInheritedBoxProps()` strips inherited box properties, including:
  - `PropWidth`, `PropMinWidth`, `PropMaxWidth`
  - `PropHeight`, `PropMinHeight`, `PropMaxHeight`
  - margin and padding shorthand/side properties
  - border shorthand, side, style, width, and color properties
  - `PropBorderCollapse`, `PropBorderSpacing`, `PropBorderRadius`, and `Prop_CellPadding`
- It preserves inherited text properties such as `PropColor`, font properties, `PropTextAlign`, and `PropVerticalAlign`.
- It does not strip `PropBackgroundColor` because td background rules and td presentation backgrounds should survive, and `MakeChildStyle()` intentionally does not strip background.
- `HadCssWidth` should reflect td CSS width only, not inherited table width. Stripping inherited table `width` before td CSS matching allows presentation td `width` attributes such as `50%` and `100%` to be applied through `ApplyLength()`.

### Verified state

- Round-trip input/output `td` count matches:
  ```text
  input td:  24
  output td: 24
  ```
- Invalid `border:Inherit none` is no longer emitted.
- `table.page_header td { border:none; }` now exports as valid `border: none`.
- Redundant border side properties are not emitted when equal to the `border` shorthand.
- `td` styles no longer inherit table `width`, `cellpadding`, `border-spacing`, or table border properties such as `border-top`/`border-left` unless td itself has those properties.
- td presentation widths now round-trip as CSS width:
  - `50%`
  - `100%`
- td backgrounds set by td rules remain preserved:
  - `table.items td`
  - `table.items td.title`
  - `table.items th`
- `MakeChildStyle()` still does not strip background, so text inside coloured cells keeps the background path.
- Minimal border tests now produce clean outputs:
  - `border.html`: valid `border: none`.
  - `border2.html`: valid `border: none` for the table/cell case.
  - `border3.html`: td emits only its own `border-right`, not table-derived `border-left`.
- Test outputs should be truncated between runs because stale tails from prior runs can otherwise look like current output.
- `TableGrid` single-emission test still passes:
  ```text
  single-emission: prints=3 left=2 cols=2
  OK
  ```

## Open issues

- Runtime layout/paint still needs visual verification in the HtmlEditor GUI harness; a table test case was added at `lgi/trunk/test/HtmlEditor/testcases/table-test.html` covering colspan, rowspan, percent and pixel width, named and hex `bgcolor`, cellpadding/border, whitespace between cells, a list inside a cell, an empty cell, and a table inside a blockquote with a nested table.
- TD/th inheritance fidelity is implemented in the parser/style path, but visual confirmation is still needed for td borders, td width presentation, td background rendering, and text inside cells.
- `StripCellInheritedBoxProps()` intentionally does not strip `PropBackgroundColor`; if the intended behavior is to strip only td’s own background while removing inherited table background, that needs a separate decision.
- `TD`/`TH`/`TABLE` background now has a full path (`ApplyBackground()` → `LCss::PropBackgroundColor` → `TdBlock::OnPaint()` / `TableBlock::OnPaint()`), pending visual confirmation.
- Cell/row `height` is now honored as a per-row minimum through `TableGrid::Cell::MinH`
  and `RowMinHeights()`; percentage heights remain unresolved.
- Percent width behavior is resolved: `TdBlock` stores `Pct` only, and `Fixed`/`FixedPx` are px-only flags, which is correct for `TableGrid::ColumnMetrics()`. Width round-trips through the cached style class rather than an HTML `width` attribute.
- MSVC entries in `RichText.vcxproj` and `RichText.vcxproj.filters` are unverified and may need to be added if MSVC builds are used.

