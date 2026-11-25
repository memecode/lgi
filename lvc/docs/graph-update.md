# Lvc Graph Support

## Current State

The graphing code in Lvc ties together the various branches and commits to understand their relationships. However the current code is quite old and doesn't handle git very well. Ideally it would be updated to be more modern. Potentially using the new vector drawing API in LSurface.

You'll have to find where the graphing code is in 'lgi/trunk/lvc/src/VcFolder.cpp', it's drawn into the list column 'LGraph'.

Come up with a new design to draw the graphical relationship between commits. The existing code can be used for reference but we don't need to keep it if it's getting in the way. Add a new design to this document. And then work on an implementation.

To build Lvc use: lgi/trunk/lvc/linux/Makefile.linux

## New Design

### Goals
- Anti-aliased, DPI-aware rendering via the LSurface vector API (`LSurface::Vector`).
- Smooth ribbons (curved bends when a branch lane changes between rows), not crude straight diagonals.
- Keep the existing lane model (which already solves most git graphs correctly) but make the geometry nicer and the code easier to change.
- Separate layout from drawing so the renderer is small and testable.

### Model (unchanged in spirit)
- Each commit is a node placed on a lane (`NodeIdx`).
- Each parent->child link is an edge assigned a lane (`VcEdge::Idx`).
- `Pos` records, for the current row, the edge lane for every edge passing through or touching that commit. The commit circle is at `NodeIdx`; endpoint drawing connects the circle to the edge lane in `Pos`, so merged edges can fan from the node.
- `LinkParents()` computes these lanes. This is the layout; we keep it.

### Per-row rendering (the new part)
For each row we draw the band `[0, H]` (row height `H = Ctx.Y()`):

1. Metrics derived from the row and DPI: left inset, lane pitch, node radius, ribbon stroke width.
2. For every edge in `Pos`, draw exactly one segment for that edge in the current row:
   - **Pass-through** (edge neither parent nor child of this commit): draw from the row top boundary lane, normally the previous row lane for that edge, to the current row lane, then continue to the row bottom at the current row lane. The bottom boundary is the current row lane so the next row starts at the same lane. If the top lane differs, the segment bends to the current lane at the row midpoint; if it is the same lane, it is a straight vertical.
   - **Parent side** (`e->Parent == this`): draw only the top half, from the row top boundary lane to the commit circle at `NodeIdx`. If `Prev` contains the edge, start at its previous `Pos` lane; otherwise start at the current `Pos` lane. If that start lane differs from the circle lane, draw a smooth cubic bend.
   - **Child side** (`e->Child == this`): draw only the bottom half, from the commit circle at `NodeIdx` to the row bottom at the current `Pos` edge lane. If `Next` has a lane for the edge, use the edge colour; otherwise use a muted stub.
   - **Dangling** (no adjacent row contains the edge on that side): draw a muted black stub from the boundary lane to the commit circle, or from the commit circle to the boundary lane.
3. Node drawn last so ribbon ends are covered: filled circle in the branch colour, dark outline ring, larger ring for the current/HEAD commit.

Ribbons are stroked with `LSurface::Vector::Stroke`; the node uses `Fill` + `Stroke`. Bends use `CubicTo` with control points placed at the vertical midpoint, giving the classic git-graph comma/S shape. Each ribbon keeps the branch colour it belongs to (`VcEdge::Colour`), so merged branches change colour correctly.

Because the vector backend requires a premultiplied BGRA32 target, we draw directly onto the painting surface (`Ctx.pDC`, a cairo `LScreenDC`) using its vector context, with coordinates offset to the row origin, rather than through an intermediate `LMemDC` (which was not premultiplied).

### Column sizing
The graph column must fit the widest lane set. `LinkParents()` records the peak lane count (`GraphLanes`), and `GraphColumnWidth()` sizes the column from the same pitch/inset used by the renderer. `ResizeColumnsToContent()` cannot measure a drawn column, so `ApplyGraphColumnWidth()` re-applies the correct width after content resizing (in `OnSelectUpdateItems` and `ShowLog`).

