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

Ribbons are stroked with `LSurface::Vector::Stroke`; the node uses `Fill` + `Stroke`. Bends use `CubicTo` with control points placed at the vertical midpoint, giving the classic git-graph comma/S shape. Ribbons use the standard graph line colour from `GetGraphLineColour()` (`VcEdge::Colour`), independent of lane colours, so only the commit circles show branch colours.

Colours are resolved in `VcFolder::AssignColours()` once the lanes are known: each lane takes the colour of the first branch-named commit down that lane, commits on that lane use the lane colour unless they name their own branch, and edges are given the standard graph line colour. Branch names come from `VcCommit::CommitBranch()`, which uses the commit's own branch for hg/cvs/svn and falls back to the git refs pointing at the commit. `OnBranchesChange()` re-runs the pass when ref names arrive after the graph was linked.

Because the vector backend requires a premultiplied BGRA32 target, we draw directly onto the painting surface (`Ctx.pDC`, a cairo `LScreenDC`) using its vector context, with coordinates offset to the row origin, rather than through an intermediate `LMemDC` (which was not premultiplied).

### Column sizing
`LinkParents()` records the peak lane count (`GraphLanes`), and `GraphColumnWidth()` sizes the column from the pitch/inset used by the renderer, capped at 15em of the list font. Lanes beyond the column are clipped; users can widen the column manually to see them. `ResizeColumnsToContent()` cannot measure a drawn column, so `ApplyGraphColumnWidth()` re-applies the automatic width after content resizing (in `OnSelectUpdateItems` and `ShowLog`).

## Performance diagnostics

The **Local branches** checkbox and View menu's **Git history: local branches
only** option control the same setting, enabled by default. It shows commits
reachable from local branches; remote-only and tag-only history, and detached
HEAD commits not reachable from a local branch, are excluded. Turn it off to
show all refs as before. The choice is saved and applies to Git repositories,
including repositories opened over SSH.

The View menu's **Git history: first parent only** option is also enabled by
default. It follows only the first parent of each Git commit and draws only
those links, retaining merge commits but hiding merged side history unless
another selected ref reaches it through its own first-parent history. Turn it
off to see all parent histories. This setting is saved independently of the
local-branches filter.

Build from the `lvc` directory with `make -f linux/Makefile.linux`, then launch
with graph tracing enabled:

```sh
LVC_TRACE_GRAPH=1 ./lvc 2>graph-profile.log
```

Open the repository (for example `/home/matthew/work/dev`), let its history load,
then scroll the commit list. Ensure the configured Git history limit is large
enough to reproduce the slowdown. Entries prefixed `[lvc-graph]` go to stderr
and identify the repository. Unset `LVC_TRACE_GRAPH` to disable tracing; its
presence enables it, even if set to `0`. No recompilation is needed to toggle it.

- `ParseRevList`: time clearing the old graph, splitting Git output, parsing
  commits, and linking the graph, plus input bytes, commit count and errors.
  This measures processing after the Git command completes, not Git execution.
- `LinkParents`: indexing, edge creation, layout and colour times. Layout is
  further broken down into finding node lanes, starting edges, assigning edge
  positions, terminating edges and collapsing empty lanes. Counts include
  edges, missing parents (normal for truncated history), peak lanes, stored
  positions, active-edge visits, untangling comparisons, lane moves and
  collapsed columns. Large position/visit/comparison counts help explain
  layout cost as history and branch overlap grow.
- `AssignColours`: lane-colour and commit/edge-colour passes, including
  assignments triggered later when branch refs arrive.
- `Paint`: one summary per main commit-list repaint, not per row. `list_ms`
  measures the entire list paint; `graph_ms` sums graph-column row work,
  split into base cell painting, setup/vector acquisition, edge drawing
  (including adjacent-row position lookups), and node drawing. It also reports
  the slowest row, rows painted, positions visited, segments, curved segments
  and rows lacking a vector context. Repeated repaints are counted again.
  Backend work outside those row sections is included only in `list_ms`.

All times are milliseconds from microsecond clock samples. Begin markers
identify parsing/linking before they finish. Subphase times are nested within
their parent totals, so do not sum totals across entries. Instrumentation adds
clock reads and counters while enabled; compare runs using the same build,
history limit and viewport.

### Ref-name lookup

Ref names are indexed by their hash and the abbreviation lengths reported by
Git. A commit lookup probes only those lengths, rather than scanning every ref.
Matching remains case-insensitive and includes all matching abbreviated or full
hashes. Multiple names at the same hash retain their order; matches at different
abbreviation lengths follow the order those lengths were first encountered.

The index caches both matches and misses. Adding refs, replacing the branch
snapshot, or refreshing the repository clears the cache. Commit display and
colour assignment consult this cache rather than retaining stale positive
results in individual commits.

The headless regression test is `tests/GitRefNames.cpp`. It covers abbreviated
and full hashes (including SHA-256), multiple names, mixed-case queries, and
positive/negative cache invalidation. It can optionally benchmark against the
previous linear scan using two files: whitespace-separated `hash name` ref
pairs, and one commit hash per line. It verifies that both implementations
return the same names for every supplied commit, ignoring ordering.

