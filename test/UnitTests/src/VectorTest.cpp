#include <array>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

#include "lgi/common/Lgi.h"
#include "UnitTests.h"

static_assert(!std::is_copy_constructible<LSurface::Vector>::value, "Vector sessions cannot be copied.");
static_assert(std::is_move_constructible<LSurface::Vector>::value, "Vector sessions can be moved.");

namespace
{
template<typename Pixel>
bool TestAlphaFormat(LColourSpace cs, bool premul)
{
	LMemDC dc(_FL, 12, 12, cs, LSurface::SurfaceRequireExactCs);
	if (!dc[0] || dc.GetColourSpace() != cs)
		return false;
	for (int y = 0; y < dc.Y(); y++)
	{
		auto row = reinterpret_cast<Pixel*>(dc[y]);
		for (int x = 0; x < dc.X(); x++)
		{
			row[x].r = 17;
			row[x].g = 37;
			row[x].b = 59;
			row[x].a = 31;
		}
	}
	if (premul && !dc.ConvertPreMulAlpha(true))
		return false;
	Pixel untouched = reinterpret_cast<Pixel*>(dc[0])[0];
	{
		auto v = dc.Vec();
		if (!v)
			return false;
		v.Rect(2, 2, 8, 8);
		v.Fill(LColour(0, 0, 255));
		v.Fill(LColour(255, 0, 0, 128));
		auto pixel = reinterpret_cast<Pixel*>(dc[5])[5];
		if (pixel.r != 128 || pixel.g != 0 || pixel.b != 127 || pixel.a != 255)
			return false;
		v.ClearPath();
		v.Rect(3, 3, 6, 6);
		v.Fill(LColour(0, 0, 0, 0));
		v.Stroke(LColour(0, 255, 0), 0);
		v.Stroke(LColour(0, 255, 0), std::numeric_limits<float>::infinity());
		if (memcmp(&pixel, &reinterpret_cast<Pixel*>(dc[5])[5], sizeof(pixel)))
			return false;
	}
	if (dc.IsPreMultipliedAlpha() != premul ||
		memcmp(&untouched, dc[0], sizeof(untouched)))
		return false;

	for (int y = 0; y < dc.Y(); y++)
		memset(dc[y], 0, dc.X() * sizeof(Pixel));
	auto v = dc.Vec();
	v.Rect(2, 2, 8, 8);
	v.Fill(LColour(255, 0, 0, 128));
	auto pixel = reinterpret_cast<Pixel*>(dc[5])[5];
	return pixel.r == (premul ? 128 : 255) && pixel.g == 0 && pixel.b == 0 && pixel.a == 128;
}

template<typename Pixel>
bool TestPaddingFormat(LColourSpace cs)
{
	LMemDC dc(_FL, 8, 8, cs, LSurface::SurfaceRequireExactCs);
	if (!dc[0] || dc.GetColourSpace() != cs)
		return false;
	for (int y = 0; y < dc.Y(); y++)
	{
		auto row = reinterpret_cast<Pixel*>(dc[y]);
		for (int x = 0; x < dc.X(); x++)
		{
			row[x].r = row[x].g = 0;
			row[x].b = 255;
			row[x].pad = 43;
		}
	}
	auto v = dc.Vec();
	if (!v)
		return false;
	v.Rect(1, 1, 6, 6);
	v.Fill(LColour(255, 0, 0, 128));
	auto pixel = reinterpret_cast<Pixel*>(dc[3])[3];
	return pixel.r == 128 && pixel.g == 0 && pixel.b == 127 && pixel.pad == 43;
}

#if !defined(HAIKU)
class StrideBitmap : public LMemDC
{
	std::array<unsigned char, 4 * 24> Storage = {};

public:
	explicit StrideBitmap(bool negative) : LMemDC(_FL)
	{
		pMem = new LBmpMem;
		pMem->Base = Storage.data() + (negative ? 3 * 24 : 0);
		pMem->x = pMem->y = 4;
		pMem->Line = negative ? -24 : 24;
		pMem->Cs = ColourSpace = CsBgra32;
		pMem->Flags = 0;
		Clip = Bounds();
	}

	void InvalidateStride() { pMem->Line = 4; }
	void HidePixels() { pMem->Base = nullptr; }
};
#endif
}

bool LVectorTest::Run()
{
	for (bool premul : {false, true})
		if (!TestAlphaFormat<LRgba32>(CsRgba32, premul) ||
			!TestAlphaFormat<LBgra32>(CsBgra32, premul) ||
			!TestAlphaFormat<LArgb32>(CsArgb32, premul) ||
			!TestAlphaFormat<LAbgr32>(CsAbgr32, premul))
			return FAIL(_FL, "RGBA layouts must preserve alpha representation and source-over colours.");
	if (!TestPaddingFormat<LRgbx32>(CsRgbx32) || !TestPaddingFormat<LBgrx32>(CsBgrx32) ||
		!TestPaddingFormat<LXrgb32>(CsXrgb32) || !TestPaddingFormat<LXbgr32>(CsXbgr32))
		return FAIL(_FL, "Opaque 32-bit layouts must preserve padding and composite as opaque.");

	LMemDC dc(_FL, 20, 20, CsBgra32, LSurface::SurfaceRequireExactCs);
	dc.Colour(LColour(0, 0, 0, 0));
	dc.Rectangle();
	dc.Colour(LColour(12, 34, 56));
	dc.Op(GDC_XOR);
	dc.AntiAlias(false);
	auto oldColour = dc.Colour();
	auto oldOp = dc.Op();
	LRect clip(4, 4, 15, 15);
	dc.ClipRgn(&clip);
	{
		auto v = dc.Vec();
		v.Rect(0, 0, 20, 20);
		if (!v || !v.ClipRect(LRect(8, 8, 19, 19)))
			return FAIL(_FL, "Clipping must work on a valid memory session.");
		v.Fill(LColour::Red);
		auto row = reinterpret_cast<LBgra32*>(dc[10]);
		if (row[7].a || row[8].r != 255 || row[15].r != 255 || row[16].a)
			return FAIL(_FL, "ClipRect must intersect the surface clip and preserve the drawing path.");
		if (!v.ClipRect(LRect(30, 30, 40, 40)))
			return FAIL(_FL, "A disjoint clip is a successful empty intersection.");
		v.Fill(LColour::Blue);
		if (row[10].r != 255 || row[10].b != 0)
			return FAIL(_FL, "An empty vector clip must not paint.");
	}
	if (dc.ClipRgn() != clip || dc.Colour() != oldColour || dc.Op() != oldOp || dc.AntiAlias())
		return FAIL(_FL, "Vector sessions must preserve LSurface state.");
	{
		auto v = dc.Vec();
		v.Rect(4, 4, 12, 12);
		v.Fill(LColour::Blue);
		if (reinterpret_cast<LBgra32*>(dc[5])[5].b != 255)
			return FAIL(_FL, "A new session must not inherit a previous session's clip.");
	}

	dc.ClipRgn(nullptr);
	dc.SetOrigin(LPoint(-3, -4));
	{
		auto v = dc.Vec();
		v.Rect(0, 0, 3, 3);
		v.ClipRect(LRect(0, 0, 2, 2));
		v.Fill(LColour::Green);
		if (reinterpret_cast<LBgra32*>(dc[4])[3].g != LColour::Green.g() ||
			reinterpret_cast<LBgra32*>(dc[3])[3].a)
			return FAIL(_FL, "Paths and clip rectangles must apply the LSurface origin exactly once.");
	}
	dc.SetOrigin(LPoint(0, 0));
	dc.Op(GDC_SET);
	dc.Colour(LColour(0, 0, 0, 0));
	dc.Rectangle();
	{
		auto v = dc.Vec();
		v.Rect(4, 4, 10, 10);
		v.Fill(LColour::Red);
		v.Stroke(LColour::Blue, 2);
		if (reinterpret_cast<LBgra32*>(dc[8])[8].r != 255 ||
			reinterpret_cast<LBgra32*>(dc[8])[4].b != 255)
			return FAIL(_FL, "A path must be reusable for fill then stroke.");
		v.ClearPath();
		v.MoveTo(2, 10);
		v.ArcTo(8, 8, 0, false, true, 10, 2);
		v.LineTo(10, 10);
		v.ClosePath();
		v.Fill(LColour::Green);
		if (reinterpret_cast<LBgra32*>(dc[8])[8].g != LColour::Green.g())
			return FAIL(_FL, "SVG arc-to, line-to and close-path must form a filled path.");
		v.ClearPath();
		v.MoveTo(2, 12);
		v.CubicTo(2, 2, 18, 2, 18, 12);
		v.LineTo(2, 12);
		v.ClosePath();
		v.Fill(LColour::Red);
		if (reinterpret_cast<LBgra32*>(dc[8])[10].r != 255)
			return FAIL(_FL, "Cubic curves must form a filled path.");
		v.ClearPath();
		v.Fill(LColour::Blue);
		if (reinterpret_cast<LBgra32*>(dc[8])[10].r != 255)
			return FAIL(_FL, "ClearPath must remove all previous geometry.");
		v.Rect(16.5f, 16.5f, 2, 2);
		v.Fill(LColour::Red);
		auto edge = reinterpret_cast<LBgra32*>(dc[16])[16];
		if (!edge.a || edge.a == 255)
			return FAIL(_FL, "Fractional coordinates must retain antialiased coverage.");
		v.ClearPath();
		v.Rect(std::numeric_limits<float>::quiet_NaN(), 0, 2, 2);
		v.Fill(LColour::Blue);
		if (reinterpret_cast<LBgra32*>(dc[8])[10].r != 255)
			return FAIL(_FL, "Non-finite geometry must be rejected without corrupting the path.");
	}

	#if !defined(HAIKU)
	for (bool negative : {false, true})
	{
		StrideBitmap bitmap(negative);
		auto v = bitmap.Vec();
		if (!v)
			return FAIL(_FL, "Padded positive and negative strides must be supported.");
		v.Rect(0, 0, 4, 4);
		v.Fill(LColour::Red);
		for (int y = 0; y < 4; y++)
			if (reinterpret_cast<LBgra32*>(bitmap[y])[3].r != 255 || bitmap[y][16])
				return FAIL(_FL, "Rendering must respect row stride and leave padding untouched.");
		bitmap.InvalidateStride();
		if (v || bitmap.Vec())
			return FAIL(_FL, "Invalid stride must invalidate an existing session and reject a new one.");
	}
	StrideBitmap inaccessible(false);
	inaccessible.HidePixels();
	if (inaccessible.Vec())
		return FAIL(_FL, "Inaccessible pixel storage must reject vector setup.");
	#endif
	LMemDC unsupported(_FL, 4, 4, CsRgb24, LSurface::SurfaceRequireExactCs);
	LMemDC empty(_FL);
	if (unsupported.Vec() || empty.Vec())
		return FAIL(_FL, "Unsupported formats and empty surfaces must reject vector setup.");
	LMemDC separateAlpha(_FL, 4, 4, CsBgra32);
	if (!separateAlpha.HasAlpha(true) || separateAlpha.Vec())
		return FAIL(_FL, "Bitmaps with a separate alpha plane must explicitly reject vector setup.");
	{
		auto outer = dc.Vec();
		outer.Rect(0, 0, 2, 2);
		{
			auto inner = dc.Vec();
			inner.Rect(0, 0, 2, 2);
			inner.Fill(LColour::Blue);
		}
		outer.Fill(LColour(255, 0, 0, 128));
		auto pixel = reinterpret_cast<LBgra32*>(dc[0])[0];
		if (pixel.r != 128 || pixel.b != 127 || pixel.a != 255)
			return FAIL(_FL, "Overlapping sessions must composite against the latest destination pixels.");
	}
	{
		auto v = dc.Vec();
		auto moved = std::move(v);
		if (v || !moved)
			return FAIL(_FL, "Move construction must transfer the valid session.");
		auto assigned = dc.Vec();
		assigned = std::move(moved);
		if (moved || !assigned)
			return FAIL(_FL, "Move assignment must release and replace a session.");
		assigned.Rect(0, 0, 2, 2);
		assigned.Fill(LColour::Red);
		if (reinterpret_cast<LBgra32*>(dc[0])[0].r != 255)
			return FAIL(_FL, "A moved session must remain usable.");
	}
	for (int i = 0; i < 100; i++)
		if (!dc.Vec())
			return FAIL(_FL, "Repeated session creation and cleanup must remain usable.");
	return true;
}
