# Html2 Position Absolute Support

## Current Status

The html rendering in 'lgi/trunk/src/common/Text/Html2.cpp' currently doesn't support the CSS 'position: absolute' correctly. This needs to be added so that pages like 'scribe/trunk_os/help/index.html' render correctly.

Currently the iframe element renders on the left of that page, and all the right column content is pushed down below that entire frame. Instead of sitting beside it.

Note: there is support for rendering a page out to png via the HtmlViewer test app, ie:

    ~/code/lgi/trunk/test/HtmlViewer$ ./HtmlTestSuite -render /home/matthew/code/scribe/trunk_os/help/index.html

Which will create a 'render.png' in the '~/code/lgi/trunk/test/HtmlViewer' folder.

## Design

Add the design notes here in this document. Previous attempts to solving this via AI have had errors and loss of state. Posibly due to exhausted context and compaction not working. So storing design state here may help in that regards.

### Layout architecture (current)

`LNewFlow` (Html2.cpp) is the block/inline layout engine. It works entirely in absolute
coordinates during `Layout()`, and a final `Rebase(root,0,0)` pass converts `LTag::Pos` to
parent-relative coordinates (so `AbsolutePos()` = sum of `Pos` up the tree is valid afterwards).
Painting accumulates origin by walking parent `Pos` (OnPaint), so `Pos` must be parent-relative
for paint to place a tag correctly.

Positioned tags are handled specially:
- `PositionOfTag()` / `IsFixedTag()` gate `position` on the already-parsed CSS `PositionType`
  (Css.cpp `ParsePositionType`, top/right/bottom/left accessors already exist).
- `FlowChildren()` dispatches fixed tags to `LayoutFixedBlock()` and `continue`s, so fixed
  boxes do NOT advance the flow cursor (`pc.y`) and do not enter line boxes.
- `LayoutFixedBlock()` lays out the fixed subtree against the viewport (`View`) as its
  containing block, records `origin` in `d->FixedTags`, and `RebaseFixed()` rebases that
  subtree relative to origin. Fixed tags are painted in a separate overlay pass
  (`d->PaintingFixed`) at the viewport origin.
- `Measure()` and `FixInline()` skip fixed children so out-of-flow boxes do not affect
  parent sizing or inline unions.

### Finding (baseline, before changes)

`position: fixed` already works end-to-end: a headless position dump of help/index.html
shows the fixed menu `div` out of flow at `abs=0,0` size `190x(100% height)`, and the
`.content` `div` beside it at `abs=190,0` (its `margin-left:190px`). So the "content pushed
below the frame" symptom in this document comes from `position: absolute`, which is NOT
handled at all: an `absolute` box is currently treated as a normal block, so it participates
in normal flow and pushes the content below it.

`position: relative` is also parsed but has no layout offset applied.

### Implementation plan for `position: absolute`

Absolute boxes must be removed from normal flow (do not advance the flow cursor / enter line
boxes) and positioned relative to their containing block. Reuse the fixed mechanism but with
a different containing block and normal (in-tree) painting:

1. `IsAbsoluteTag(tag)` = `PositionOfTag(tag) == PosAbsolute`.
2. `ContainingBlockRect(tag)` = padding box of nearest positioned ancestor (`absolute`,
   `fixed`, or `relative`), using the ancestor's absolute `Pos`/`Size` valid during layout;
   fall back to `View` when there is no positioned ancestor.
3. `LayoutAbsoluteBlock(tag, pc, depth)`:
   - resolves content width from `width`/`min/max`/shrink-to-fit, bounded by containing block;
   - resolves origin from `top`/`left` when set, else from `right`/`bottom` (needs box size),
     else the static position (where it would have flowed);
   - resolves content height from `height`/`min/max` or natural content, or `top`+`bottom`;
   - lays out children in an isolated `Ctx` so the parent flow cursor is not advanced;
   - sets `tag->Pos` (absolute) to the resolved origin, `Size` to the resolved box;
   - does NOT add to `FixedTags` and is painted in normal tree order (paint origin
     accumulation already places it correctly after `Rebase`).
4. `FlowChildren()`: dispatch absolute children to `LayoutAbsoluteBlock` and `continue`
   (no `FinishLine`, no cursor advance). Skip absolute/fixed in `Measure()` and `FixInline()`.

Coordinate note: during layout `Pos` is absolute, so `AbsolutePos` is only meaningful after
`Rebase`; `ContainingBlockRect` must read `Pos` directly (absolute) during layout, not
`AbsolutePos`. `Pos` is the border-box left origin, matching `LayoutBlock` (`boxX = pc.Left`).

### Ordering fix (two passes)

A positioned ancestor's `Size`/`Pos.y` is not finalized while a child is being laid out, so
`ContainingBlockRect` cannot be resolved during a single `FlowChildren` pass. `Layout()` is
now two passes:

1. `FlowChildren` records each absolute child in `absQueue` with its static position
   (`statX = c.X`, `statY = c.y + c.pend`) and does NOT advance the parent flow. Absolute
   children are also skipped in `Measure` and `FixInline`.
2. After the full flow (ancestor geometry finalized), `ResolveAbsolute()` walks `absQueue`
   (breadth-first; nested absolutes are recorded while their containing box is resolved),
   calls `LayoutAbsoluteBlock`, and then `Rebase` runs.

`LayoutAbsoluteBlock`: resolves content width, origin (`top`/`left`, else `right`/`bottom`,
else static), height, flows children in an isolated `Ctx` (`fc.Left = originX` border-box
left), sets `Pos` = resolved border-box origin, and shifts the flowed subtree when the origin
moves (bottom-based).

`bottom`/`height` percentages resolve against the initial containing block, which is the
engine's `View` (page content height), matching existing `fixed` semantics.

### Verification (headless dump, -dump)

Test `/tmp/abs_test.html`:
- abs1 (no positioned ancestor, top50/left60/100x40): `pos=3 abs=60,50 size=100,40`.
- abs2 inside relative parent (top10/right20): `abs=58,90` within CB, right edge at CB right
  - 20px, not advancing parent flow.
- abs3 bottom/left: placed out of flow at its CB offset.
- abs4 static (no offsets): placed at its static flow position, no flow advance.
- Regression: help/index.html `position: fixed` menu unchanged (`abs=0,0` 190x3000, content
  `abs=190,0`).

