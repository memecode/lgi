# Add Vector Graphics API

## Context

The existing `LSurface` API provides basic primitives such as lines, boxes and
circles, but does not provide a consistent path-based vector API. The new API
should be a small, backend-neutral layer for common vector drawing in Lgi
applications. It should not expose Cairo, GDI+, Cocoa, Haiku or PlutoVG types in
its public interface.

PlutoVG is bundled under `lgi/deps/lunasvg/plutovg`, is linked by the Lgi
build, and is used by the memory backend in `src/common/Gdc2/vectorSurface.cpp`. It is
the backend for memory surfaces. Native screen surfaces use their platform
drawing APIs. The default skin is now the vector skin
(`src/common/Skins/vector/vectorUi.cpp`), which draws through `Vec()` directly
rather than into memory bitmaps. The PlutoVG skin
(`src/common/Skins/plutovg/plutoVg.cpp`) remains in the tree for comparison.

### Backend status

The base `LSurface::CreateVectorContext()` logs and returns no context. The
following surfaces override it:

| Target surface | Linux (GTK/Cairo) | macOS (Quartz) | Windows (GDI+) | Haiku (BView) | SDL |
|---|---|---|---|---|---|
| Screen (`LScreenDC`) | Implemented (`CairoVectorContext`), covered by `LVectorTest` | Implemented (`QuartzVectorContext`), not runtime-verified | Implemented (`GdiPlusVectorContext`), not runtime-verified | Implemented (`BViewVectorContext`), not runtime-verified | None |
| Memory bitmap (`LMemDC`) | PlutoVG, verified | PlutoVG, not runtime-verified | PlutoVG, not runtime-verified | PlutoVG, not runtime-verified | Not applicable |
| Print (`LPrintDC`) | No override; not verified | Inherits `LScreenDC` context; not verified | Inherits `LScreenDC` context; not verified | No override; not verified | None |

Print support is not established on any platform. On Windows and macOS,
`LPrintDC` derives from `LScreenDC`, so it inherits the screen context. On the
other platforms it has no vector override. The SDL surface has no vector
backend. Any entry marked "not runtime-verified" has its adapter code and build
wiring in place, but has not been exercised by the tests.

## Proposed API

Add a lightweight nested `LSurface::Vector` facade and a factory on `LSurface`.
The facade is returned by value, is non-copyable and movable, and owns an
opaque private implementation handle for its temporary native/vector drawing
context:

```cpp
class LSurface
{
public:
    class Vector
    {
    public:
        Vector(Vector&&);
        Vector& operator=(Vector&&);
        Vector(const Vector&) = delete;
        Vector& operator=(const Vector&) = delete;
        ~Vector();

        explicit operator bool() const;

        void MoveTo(float x, float y);
        void LineTo(float x, float y);
        void CubicTo(float x1, float y1, float x2, float y2, float x, float y);
        void ArcTo(float rx, float ry, float rotation, bool largeArc, bool sweep,
                   float x, float y);
        void Rect(float x, float y, float width, float height);
        void RoundRect(float x, float y, float width, float height, float radius);
        void ClosePath();
        void ClearPath();

        void Fill(LColour colour);
        void Fill(const LPointF &start, const LPointF &end,
                  const LArray<LColourStop> &stops);
        void Stroke(LColour colour, float width);
        bool ClipRect(LRect rect);
    };

    Vector Vec();
};
```

This declaration is in `lgi/trunk/include/lgi/common/Gdc2.h`. The
`LSurface::Vector` facade implements the backend-neutral `LSurfaceVector`
interface, which also has `IsValid()`. Its surface-only constructor is private;
callers obtain sessions through `Vec()`. Floating-point coordinates preserve
sub-pixel placement; colours use `LColour`. The backend types and ownership
details stay in implementation files behind the private implementation object.

`Vec()` begins a drawing session and saves the target's graphics state.
Destruction releases the backend context and restores the caller's state,
including the original clip. The target surface must outlive its `Vector`
session. A failed or unsupported context is reported by a false
`operator bool()`; callers must be able to check it before drawing. Operations
which can fail during setup or clipping return `bool` rather than silently
reporting success.

### First-iteration behavior

- Support move, line, cubic Bézier, elliptical arc-to, rectangle and close-path
  commands, plus solid-color fill and stroke.
- `Fill` and `Stroke` operate on the current path without consuming it, so a
  path can be filled and then outlined. `ClearPath` explicitly starts a new
  path. `ArcTo` uses degrees for rotation and the SVG elliptical-arc flag
  semantics. Path and rectangle coordinates use the surface's coordinate units;
  rectangle width and height are extents. Stroke width uses the same units and
  must be finite and positive.
- `ClipRect` intersects the active vector clip with the surface's existing
  `ClipRgn()` and with earlier vector clips for the rest of the session. Its
  rectangle is in surface coordinates using `LRect`'s existing edge
  conventions. The clip is restored when the session ends.
- Use the non-zero winding rule for fills. Apply `LColour` alpha with
  source-over compositing, honor the surface's antialias setting where the
  backend allows it, and do not change its current colour or drawing operator.
  Keep these semantics
  consistent across backends; document any unavoidable backend limitation
  rather than silently changing behavior.
- Read the target's current colour for default styling only if a future API
  adds no-argument fill/stroke. The first API keeps colour and stroke width
  explicit to avoid hidden dependence on backend state.

The first iteration does not include transforms, radial gradients, text, image
patterns, arbitrary path clipping, or a general save/restore stack. Add those
only after the basic path API is implemented and exercised on the target
backends.

### Linear-gradient fills

Solid fills remain available as `Fill(LColour)`. The overload
`Fill(const LPointF &start, const LPointF &end, const LArray<LColourStop> &stops)`
fills the same path with a linear gradient. It preserves the path and uses the
same non-zero fill rule, clip, origin and source-over compositing as solid fills.
Coordinates are in surface units and must be finite and representable as floats;
the endpoints must remain distinct at float precision. Supply at least two
stops, with finite, strictly increasing `Pos` values in `[0, 1]`. Duplicate
positions are rejected because the bundled PlutoVG version does not render hard
transitions correctly. Colours include alpha. The first/last stop colours
extend outside the stop range (pad behaviour). Invalid input is logged and does
not paint or consume the path.

```cpp
LArray<LColourStop> stops;
stops.Length(2);
stops[0].Set(0.0f, LColour(255, 0, 0));
stops[1].Set(1.0f, LColour(0, 0, 255));
auto vector = surface.Vec();
if (vector)
{
    vector.RoundRect(10, 10, 100, 40, 8);
    vector.Fill(LPointF(10, 10), LPointF(110, 10), stops);
}
```

Linear fills are implemented by the PlutoVG memory, Cairo, GDI+, Quartz and
Haiku vector contexts. Native platform types remain private to each implementation.
GTK screen blits convert straight-alpha ARGB memory sub-images to temporary
premultiplied Cairo surfaces without modifying the source. Already-premultiplied
sources keep the direct-view path. This prevents bright fringes around
antialiased shapes drawn into transparent memory bitmaps.

### HTML rounded borders

`Html2::LTag::PaintBorderAndBackground` draws rounded backgrounds and corner
border sections directly through `Vec()` (in its `PaintRoundedCorners` helper),
without corner bitmaps or blits.
Corners blend adjacent border colours using linear fills; matching colours use
solid fills. Each corner follows an outer circular arc and a reverse inner
elliptical arc, with radii reduced by the corresponding border widths.
Borders thicker than the radius expand their corner cells to meet the straight
edges without gaps. Transparent centres are left untouched; backgrounds are
painted once beneath the borders.

The existing pixel-rounded radius and straight-edge style handling are retained.
The radius is clamped to half of both box dimensions, separately for each inline
fragment. Rounded straight edges use converted pixel widths, matching their
corners. As before, patterned straight borders have continuous corners.
Surfaces without a vector context log the failure and use square corners.

Run the HTML pixel regression tests with:

```sh
cd lgi/trunk/test/HtmlViewer
make -f linux/Makefile.linux ./HtmlTestSuite
LD_LIBRARY_PATH=../../Debug:../../../deps/build-x64-debug/lib \
    xvfb-run -a ./HtmlTestSuite --test-rounded-borders
```

## Implementation plan

1. **Define the public contract.** Complete: the `LSurface::Vector`
   declaration and `Vec()` factory are in `Gdc2.h`. Specify path lifetime,
   clipping intersection, alpha/color behavior, coordinate units,
   unsupported-target handling, and lifetime constraints (the surface must
   outlive its vector session).
2. **Add the backend-neutral implementation.** Complete: the shared session
  implementation is in `src/common/Gdc2/vectorSurface.cpp`, and the internal context
   contract is in `private/common/Gdc2Vector.h`. `LSurface::Vector` owns its
   context with RAII, forwards drawing calls through that interface, and
   reports setup failure through its boolean conversion. The base factory
   currently returns no context; platform surface implementations will
   override the protected `CreateVectorContext()` hook in later steps. Backend
   context constructors/destructors are responsible for saving and restoring
   native graphics state. No backend headers or native types are exposed in
   the public interface.
3. **Implement memory surfaces with PlutoVG.** Complete: `LMemDC` overrides
   `CreateVectorContext()` with a shared PlutoVG implementation in
  `src/common/Gdc2/vectorSurface.cpp`. It validates bitmap access, format, dimensions
   and stride. Compatible premultiplied native ARGB data is bound directly;
   other supported 32-bit layouts use a per-session conversion surface.
   See the memory-backend contract below for supported formats and limitations.
4. **Implement native screen and print adapters.** Complete for screen
   surfaces on GTK/Cairo (`CairoVectorContext`), Windows/GDI+
   (`GdiPlusVectorContext`), macOS/Quartz (`QuartzVectorContext`) and Haiku
   (`BViewVectorContext`). Each adapter saves and restores its native state and
   intersects the vector clip with the existing surface clip. Print support is
   not complete on any platform (see Backend status). The SDL surface has no
   adapter. Runtime behaviour on macOS, Windows and Haiku has not been verified.
5. **Add focused tests and examples.** Partially complete. `LVectorTest`
   (`test/UnitTests/src/VectorTest.cpp`) covers the memory backend and the
   screen and Cairo contexts on Linux. The HTML rounded-border pixel tests
   cover the HTML use. The remaining gaps are fill-then-stroke path reuse on
   the native backends, and rendered-output comparison against expected images.
   Build each available platform target to catch adapter/API wiring issues.
6. **Document the supported matrix.** Complete for the current state; see
   Backend status. Update it as native backends are verified.

## Memory-backend contract and verification

- Supports `CsRgba32`, `CsBgra32`, `CsArgb32` and `CsAbgr32`, in both straight-
  and premultiplied-alpha representations. Also supports the opaque padded
  layouts `CsRgbx32`, `CsBgrx32`, `CsXrgb32` and `CsXbgr32`.
- Direct binding requires compatible native ARGB byte order, premultiplied
  alpha, aligned pixels, and a positive, four-byte-aligned stride. Other
  supported buffers (including negative strides) use conversion. Each fill or
  stroke imports the current destination and copies back only changed pixels,
  preserving untouched pixel bytes, padding, and the bitmap's alpha flag.
  Conversion costs a bitmap-sized temporary buffer and scans per paint call.
- Paths are retained separately from the canvas so clipping never consumes
  the current path. Drawing is immediate, not deferred until destruction.
- Coordinates follow existing Lgi origin semantics: device coordinates are
  logical coordinates minus `GetOrigin()`. The initial `ClipRgn()` is already
  in device coordinates. Vector clip rectangles use inclusive `LRect` edges
  and are mapped into device coordinates before intersection.
- The memory backend **always antialiases**, including when `AntiAlias()` is
  false. This is an agreed PlutoVG limitation: its public API has no
  antialiasing switch. The surface's antialias flag is not changed.
- Surface colour, operator, clip, origin, pixel layout and alpha representation
  are not changed by the vector session. The memory implementation uses its
  own canvas, so there is no native drawing state to restore.
- GTK memory surfaces are flushed before pixel access and marked dirty after
  writes. Windows pending GDI draws are flushed before pixel access. On Haiku,
  the session acquires and releases a bitmap lock; lock failure rejects setup.
  Elsewhere the existing `Lock()` stubs are not used as an access check.
- Rejects unavailable pixel storage, unsupported formats (including true
  24-bit buffers), separate alpha planes, alpha-only drawing, invalid strides,
  dimensions outside `1..32767`, and buffers whose row-address extent exceeds
  PlutoVG's signed-int range. Failures are logged and the session tests false.
  Invalid/non-finite stroke widths and non-finite path inputs are logged and
  rejected without painting.
- The surface must outlive the session. Do not resize, empty, unlock, change
  alpha representation, or otherwise replace its pixel storage while a session
  exists. Origin and clip are captured at creation; end the session before
  changing them. Overlapping sessions on unchanged storage see immediate
  writes from earlier paints.

Verified on Linux/GTK: the Lgi CMake target builds and the focused
`LVectorTest` (`test/UnitTests`) runs under Xvfb. Tests cover all eight layouts, both alpha
representations, source-over blending, path reuse, SVG arcs and cubic curves,
fractional coverage, clipping (including empty intersections), origin handling,
state preservation, signed/padded strides, unsupported/inaccessible storage,
invalid input, move ownership and repeated cleanup. The same test also
includes screen-context and Cairo-context checks (`TestScreenVectorContext`,
`TestCairoVectorContext`). Other platforms' adapters and build entries are
present, but their runtime behaviour has not been verified here.
The focused test and backend code also pass AddressSanitizer and
UndefinedBehaviorSanitizer checks (external libraries are not instrumented;
leak detection is disabled for the GUI application harness).

## Acceptance criteria

- A caller can draw and clip a filled/stroked path through the same Lgi API on
  every backend marked supported in the verified matrix.
- Ending a vector session releases backend resources and restores the surface's
  previous drawing state and clip, including when the caller exits early.
- Unsupported targets or setup failures are explicitly detectable.
- Existing `LSurface` primitive drawing behavior is unchanged.
