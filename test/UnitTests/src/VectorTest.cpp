#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "lgi/common/Lgi.h"
#include "UnitTests.h"

static_assert(!std::is_copy_constructible<LSurface::Vector>::value, "Vector sessions cannot be copied.");
static_assert(std::is_move_constructible<LSurface::Vector>::value, "Vector sessions can be moved.");

namespace
{
template<typename ReadPixel>
bool TestLinearGradient(LSurface &dc, ReadPixel pixel)
{
	auto expect = [&](int x, int y, uint32_t expected, int tolerance = 0)
	{
		uint32_t actual = pixel(x, y);
		bool matches = true;
		for (int shift = 0; shift < 32; shift += 8)
			if (std::abs(int((actual >> shift) & 255) - int((expected >> shift) & 255)) > tolerance)
				matches = false;
		if (!matches)
			printf("Linear gradient at %i,%i: expected %08x, got %08x\n", x, y, expected, actual);
		return matches;
	};
	LArray<LColourStop> stops;
	stops[0].Set(0.25f, LColour(255, 0, 0));
	stops[1].Set(0.5f, LColour(0, 255, 0));
	stops[2].Set(0.75f, LColour(0, 0, 255));
	auto v = dc.Vec();
	if (!v)
		return false;
	v.Rect(0, 0, 32, 32);
	v.Fill(LColour(0, 0, 255));
	v.ClearPath();
	v.Rect(1, 1, 30, 30);
	if (!v.ClipRect(LRect(2, 2, 29, 29)))
		return false;
	v.Fill(LPointF(4.5, 0), LPointF(28.5, 0), stops);
	if (!expect(3, 16, 0xffff0000) || !expect(16, 16, 0xff00ff00, 1) ||
		!expect(28, 16, 0xff0000ff) || !expect(1, 16, 0xff0000ff))
		return false;

	stops[0].Pos = 0;
	stops[2].Pos = 1;
	v.Fill(LPointF(0, 4.5), LPointF(0, 28.5), stops);
	if (!expect(16, 4, 0xffff0000) || !expect(16, 16, 0xff00ff00, 1) ||
		!expect(16, 28, 0xff0000ff, 1))
		return false;
	v.Fill(LPointF(4.5, 4.5), LPointF(28.5, 28.5), stops);
	if (!expect(4, 4, 0xffff0000) || !expect(16, 16, 0xff00ff00, 1) ||
		!expect(28, 28, 0xff0000ff, 1))
		return false;

	v.Fill(LColour(0, 0, 255));
	stops.Length(2);
	stops[0].Set(0, LColour(255, 0, 0, 128));
	stops[1].Set(1, LColour(255, 0, 0, 128));
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	if (!expect(16, 16, 0xff80007f))
		return false;
	v.Fill(LColour(0, 0, 255));
	stops[0].Colour = LColour(255, 0, 0, 0);
	v.Fill(LPointF(0.5, 0), LPointF(32.5, 0), stops);
	if (!expect(16, 16, 0xff4000bf, 1))
		return false;

	const auto before = pixel(16, 16);
	v.Fill(LPointF(0, 0), LPointF(0, 0), stops);
	v.Fill(LPointF(std::numeric_limits<double>::infinity(), 0), LPointF(32, 0), stops);
	v.Fill(LPointF(0, 0), LPointF(std::numeric_limits<double>::max(), 0), stops);
	stops[0].Pos = -0.1f;
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	stops[0].Pos = 0.8f;
	stops[1].Pos = 0.2f;
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	stops[0].Pos = stops[1].Pos = 0.5f;
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	stops[0].Pos = 0;
	stops[1].Pos = 1.1f;
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	stops[1].Pos = std::numeric_limits<float>::quiet_NaN();
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	stops.Length(1);
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	stops.Length(0);
	v.Fill(LPointF(0, 0), LPointF(32, 0), stops);
	if (!expect(16, 16, before))
		return false;
	v.Stroke(LColour(255, 255, 255), 4);
	if (!expect(2, 16, 0xffffffff) || !expect(0, 16, 0xff0000ff))
		return false;
	v.Fill(LColour(255, 0, 0));
	return expect(16, 16, 0xffff0000);
}

#if defined(__GTK_H__)
bool TestGradientBlit()
{
	using namespace Gtk;
	LMemDC source(_FL, 40, 40, System32BitColourSpace);
	if (!source[0] || source.GetColourSpace() != CsBgra32)
		return false;
	source.Colour(LColour(0, 0, 0, 0));
	source.Rectangle();
	{
		auto v = source.Vec();
		if (!v)
			return false;
		v.MoveTo(20, 4);
		v.LineTo(4, 32);
		v.LineTo(36, 28);
		v.ClosePath();
		LArray<LColourStop> stops;
		stops[0].Set(0, LColour(255, 165, 0));
		stops[1].Set(1, LColour(0, 128, 0));
		v.Fill(LPointF(4, 4), LPointF(36, 4), stops);
	}
	std::array<unsigned char, 40 * 40 * 4> original;
	for (int y = 0; y < 40; y++)
		memcpy(original.data() + y * 160, source[y], 160);
	for (bool premul : {false, true})
	{
		if (premul && !source.ConvertPreMulAlpha(true))
			return false;
		std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface(
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 40, 40), cairo_surface_destroy);
		std::unique_ptr<cairo_t, decltype(&cairo_destroy)> context(cairo_create(surface.get()), cairo_destroy);
		cairo_set_source_rgb(context.get(), 0, 0, 0);
		cairo_paint(context.get());
		LScreenDC dc(context.get(), 40, 40);
		LRect sub(2, 2, 37, 37);
		dc.Blt(2, 2, &source, &sub);
		cairo_surface_flush(surface.get());
		auto data = cairo_image_surface_get_data(surface.get());
		int stride = cairo_image_surface_get_stride(surface.get());
		int edges = 0;
		for (int y = 2; y <= 37; y++)
		{
			auto src = reinterpret_cast<const LBgra32*>(source[y]);
			auto dst = reinterpret_cast<const LBgra32*>(data + y * stride);
			for (int x = 2; x <= 37; x++)
			{
				int alpha = src[x].a;
				if (alpha > 0 && alpha < 255)
					edges++;
				auto expected = [&](int channel) { return premul ? channel : (channel * alpha + 127) / 255; };
				if (dst[x].r != expected(src[x].r) || dst[x].g != expected(src[x].g) ||
					dst[x].b != expected(src[x].b) || dst[x].a != 255)
					return false;
			}
			if (!premul && memcmp(original.data() + y * 160, source[y], 160))
				return false;
		}
		if (!edges || source.IsPreMultipliedAlpha() != premul)
			return false;
	}
	return true;
}

bool TestCairoVectorContext()
{
	using namespace Gtk;
	std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface(
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 40, 40), cairo_surface_destroy);
	std::unique_ptr<cairo_t, decltype(&cairo_destroy)> context(cairo_create(surface.get()), cairo_destroy);
	auto cr = context.get();
	cairo_translate(cr, 2, 3);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_line_width(cr, 7);
	cairo_move_to(cr, 1, 2);
	cairo_line_to(cr, 3, 4);
	LScreenDC dc(cr, 40, 40);
	{
		auto vector = dc.Vec();
		if (!vector)
			return false;
		vector.Rect(0, 0, 20, 20);
		if (!vector.ClipRect(LRect(4, 4, 15, 15)))
			return false;
		vector.Fill(LColour(0, 0, 255));
		vector.Fill(LColour(255, 0, 0, 128));
		vector.Stroke(LColour(0, 255, 0), 0);
		vector.Stroke(LColour(0, 255, 0), std::numeric_limits<float>::infinity());
		cairo_surface_flush(surface.get());
		auto data = cairo_image_surface_get_data(surface.get());
		auto stride = cairo_image_surface_get_stride(surface.get());
		auto row = reinterpret_cast<const uint32_t*>(data + 10 * stride);
		if (row[5] || row[6] != 0xff80007f || row[17] != 0xff80007f || row[18])
			return false;
		if (!vector.ClipRect(LRect::EMPTY()))
			return false;
		vector.Fill(LColour(0, 255, 0));
		cairo_surface_flush(surface.get());
		if (row[10] != 0xff80007f)
			return false;
	}
	if (!TestLinearGradient(dc, [&](int x, int y)
		{
			cairo_surface_flush(surface.get());
			auto data = cairo_image_surface_get_data(surface.get());
			auto stride = cairo_image_surface_get_stride(surface.get());
			return reinterpret_cast<const uint32_t*>(data + (y + 3) * stride)[x + 2];
		}))
		return false;
	double currentX = 0, currentY = 0;
	cairo_get_current_point(cr, &currentX, &currentY);
	double left = 0, top = 0, right = 0, bottom = 0;
	cairo_path_extents(cr, &left, &top, &right, &bottom);
	if (currentX != 3 || currentY != 4 || left != 1 || top != 2 || right != 3 || bottom != 4 ||
		cairo_get_operator(cr) != CAIRO_OPERATOR_SOURCE || cairo_get_line_width(cr) != 7)
		return false;
	cairo_clip_extents(cr, &left, &top, &right, &bottom);
	if (left != -2 || top != -3 || right != 38 || bottom != 37)
		return false;

	for (bool largeArc : {false, true})
		for (bool sweep : {false, true})
		{
			auto vector = dc.Vec();
			vector.MoveTo(8, 16);
			vector.ArcTo(-12, 6, 30, largeArc, sweep, 24, 16);
			cairo_get_current_point(cr, &currentX, &currentY);
			if (!vector || std::fabs(currentX - 24) > 0.02 || std::fabs(currentY - 16) > 0.02)
				return false;
			vector.ClearPath();
			vector.MoveTo(8, 16);
			vector.ArcTo(10, 10, 0, largeArc, sweep, 24, 16);
			cairo_path_extents(cr, &left, &top, &right, &bottom);
			if (largeArc ? bottom - top < 15 : bottom - top > 5)
				return false;
			vector.ClearPath();
			vector.MoveTo(2, 2);
			vector.ArcTo(0, 5, 0, largeArc, sweep, 12, 12);
			vector.ArcTo(5, 5, 0, largeArc, sweep, 12, 12);
			cairo_get_current_point(cr, &currentX, &currentY);
			if (!vector || currentX != 12 || currentY != 12)
				return false;
			vector.ClearPath();
			vector.MoveTo(2, 2);
			vector.ArcTo(1, 1, 45, largeArc, sweep, 12, 12);
			cairo_get_current_point(cr, &currentX, &currentY);
			if (!vector || std::fabs(currentX - 12) > 0.02 || std::fabs(currentY - 12) > 0.02)
				return false;
			vector.ClearPath();
			vector.MoveTo(2, 10);
			vector.CubicTo(2, 2, 18, 2, 18, 10);
			vector.ClosePath();
			vector.Fill(LColour(255, 0, 0));
			vector.Stroke(LColour(0, 0, 255), 2);
			cairo_path_extents(cr, &left, &top, &right, &bottom);
			if (left != 2 || top != 4 || right != 18 || bottom != 10)
				return false;
			vector.ClearPath();
			if (cairo_has_current_point(cr))
				return false;
		}
	return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}
#endif

template<typename Pixel>
bool TestAlphaFormat(LColourSpace cs, bool premul)
{
	LMemDC gradient(_FL, 32, 32, cs, LSurface::SurfaceRequireExactCs);
	if (!gradient[0])
		return false;
	for (int y = 0; y < gradient.Y(); y++)
		memset(gradient[y], 0, gradient.X() * sizeof(Pixel));
	if ((premul && !gradient.ConvertPreMulAlpha(true)) ||
		!TestLinearGradient(gradient, [&](int x, int y)
		{
			const auto &p = reinterpret_cast<const Pixel*>(gradient[y])[x];
			return (uint32_t(p.a) << 24) | (uint32_t(p.r) << 16) | (uint32_t(p.g) << 8) | p.b;
		}))
		return false;
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
	if (pixel.r != (premul ? 128 : 255) || pixel.g != 0 || pixel.b != 0 || pixel.a != 128)
		return false;
	for (int y = 0; y < dc.Y(); y++)
		memset(dc[y], 0, dc.X() * sizeof(Pixel));
	LArray<LColourStop> stops;
	stops[0].Set(0, LColour(255, 0, 0, 128));
	stops[1].Set(1, LColour(255, 0, 0, 128));
	v.Fill(LPointF(0, 0), LPointF(12, 0), stops);
	pixel = reinterpret_cast<Pixel*>(dc[5])[5];
	return pixel.r == (premul ? 128 : 255) && pixel.g == 0 && pixel.b == 0 && pixel.a == 128;
}

template<typename Pixel>
bool TestPaddingFormat(LColourSpace cs)
{
	LMemDC dc(_FL, 32, 32, cs, LSurface::SurfaceRequireExactCs);
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
	{
		auto v = dc.Vec();
		if (!v)
			return false;
		v.Rect(1, 1, 6, 6);
		v.Fill(LColour(255, 0, 0, 128));
		auto pixel = reinterpret_cast<Pixel*>(dc[3])[3];
		if (pixel.r != 128 || pixel.g != 0 || pixel.b != 127 || pixel.pad != 43)
			return false;
	}
	if (!TestLinearGradient(dc, [&](int x, int y)
		{
			const auto &p = reinterpret_cast<const Pixel*>(dc[y])[x];
			return 0xff000000u | (uint32_t(p.r) << 16) | (uint32_t(p.g) << 8) | p.b;
		}))
		return false;
	for (int y = 0; y < dc.Y(); y++)
		for (int x = 0; x < dc.X(); x++)
			if (reinterpret_cast<const Pixel*>(dc[y])[x].pad != 43)
				return false;
	return true;
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
	#if defined(__GTK_H__)
	if (!TestGradientBlit())
		return FAIL(_FL, "Gradient blits must premultiply antialiased edges without changing the source.");
	if (!TestCairoVectorContext())
		return FAIL(_FL, "Cairo vectors must preserve state and paths, intersect clips, and support SVG arcs.");
	#endif
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
		LArray<LColourStop> stops;
		stops[0].Set(0, LColour(255, 0, 0));
		stops[1].Set(1, LColour(0, 0, 255));
		v.Fill(LPointF(-0.5, 0), LPointF(1.5, 0), stops);
		const auto &pixel = reinterpret_cast<const LBgra32*>(dc[4])[3];
		if (std::abs(int(pixel.r) - 128) > 1 || std::abs(int(pixel.b) - 128) > 1 ||
			pixel.g || pixel.a != 255 || reinterpret_cast<LBgra32*>(dc[3])[3].a)
			return FAIL(_FL, "Linear gradients must apply the surface origin to endpoints and paths equally.");
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
	dc.Colour(LColour(0, 0, 0, 0));
	dc.Rectangle();
	{
		auto v = dc.Vec();
		v.RoundRect(2, 2, 16, 16, 6);
		v.Fill(LColour::Red);
		v.ClearPath();
		v.RoundRect(2, 2, 16, 16, std::numeric_limits<float>::infinity());
		auto row = reinterpret_cast<LBgra32*>(dc[10]);
		auto top = reinterpret_cast<LBgra32*>(dc[2]);
		auto corner = reinterpret_cast<LBgra32*>(dc[2])[2];
		if (row[2].r != 255 || row[17].r != 255 || top[10].r != 255 || corner.a)
			return FAIL(_FL, "RoundRect must fill the body and leave the corners clear.");
		v.ClearPath();
		v.RoundRect(20, 20, -16, -16, 100);
		v.Fill(LColour::Blue);
		if (reinterpret_cast<LBgra32*>(dc[10])[10].b != 255)
			return FAIL(_FL, "RoundRect must normalise negative sizes and clamp the radius.");
	}
	for (int i = 0; i < 100; i++)
		if (!dc.Vec())
			return FAIL(_FL, "Repeated session creation and cleanup must remain usable.");
	return true;
}
