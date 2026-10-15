#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include "lgi/common/Gdc2.h"
#include "plutovg/plutovg.h"

namespace
{
bool FiniteCoordinates(std::initializer_list<float> coordinates)
{
	for (auto value : coordinates)
		if (!std::isfinite(value))
		{
			LgiTrace("LSurface::Vector: coordinates must be finite.\n");
			return false;
		}
	return true;
}

class PlutoMemoryContext : public LSurfaceVector
{
	LMemDC &Target;
	plutovg_surface_t *Surface = nullptr;
	plutovg_canvas_t *Canvas = nullptr;
	plutovg_path_t *Path = nullptr;
	unsigned char *Data = nullptr;
	int Width = 0, Height = 0;
	ssize_t Stride = 0;
	LColourSpace ColourSpace = CsNone;
	LRect Clip;
	LPoint Origin;
	size_t Red = 0, Green = 0, Blue = 0;
	int Alpha = -1;
	bool PreMul = false;
	bool Direct = false;
	#if defined(HAIKU)
	bool Locked = false;
	#endif

	bool SetLayout()
	{
		switch (ColourSpace)
		{
			#define ALPHA_LAYOUT(Pixel) \
				case Cs##Pixel: \
					Red = offsetof(L##Pixel, r); Green = offsetof(L##Pixel, g); \
					Blue = offsetof(L##Pixel, b); Alpha = offsetof(L##Pixel, a); \
					return true
			ALPHA_LAYOUT(Rgba32);
			ALPHA_LAYOUT(Bgra32);
			ALPHA_LAYOUT(Argb32);
			ALPHA_LAYOUT(Abgr32);
			#undef ALPHA_LAYOUT
			#define PAD_LAYOUT(Pixel) \
				case Cs##Pixel: \
					Red = offsetof(L##Pixel, r); Green = offsetof(L##Pixel, g); \
					Blue = offsetof(L##Pixel, b); return true
			PAD_LAYOUT(Rgbx32);
			PAD_LAYOUT(Bgrx32);
			PAD_LAYOUT(Xrgb32);
			PAD_LAYOUT(Xbgr32);
			#undef PAD_LAYOUT
			default:
				return false;
		}
	}

	uint32_t ReadPixel(const unsigned char *pixel) const
	{
		unsigned a = Alpha < 0 ? 255 : pixel[Alpha];
		unsigned r = pixel[Red], g = pixel[Green], b = pixel[Blue];
		if (!PreMul && Alpha >= 0)
		{
			r = (r * a + 127) / 255;
			g = (g * a + 127) / 255;
			b = (b * a + 127) / 255;
		}
		return (a << 24) | (r << 16) | (g << 8) | b;
	}

	void WritePixel(unsigned char *pixel, uint32_t colour) const
	{
		unsigned a = colour >> 24;
		unsigned r = (colour >> 16) & 255, g = (colour >> 8) & 255, b = colour & 255;
		if (!PreMul && Alpha >= 0 && a)
		{
			r = MIN(255u, (r * 255 + a / 2) / a);
			g = MIN(255u, (g * 255 + a / 2) / a);
			b = MIN(255u, (b * 255 + a / 2) / a);
		}
		pixel[Red] = r;
		pixel[Green] = g;
		pixel[Blue] = b;
		if (Alpha >= 0)
			pixel[Alpha] = a;
	}

	void Synchronize()
	{
		#if defined(__GTK_H__)
		if (auto surface = Target.GetSurface())
			Gtk::cairo_surface_flush(surface);
		#elif defined(WINNATIVE)
		GdiFlush();
		#endif
	}

	void Import()
	{
		Synchronize();
		if (Direct)
			return;
		auto pixels = plutovg_surface_get_data(Surface);
		int step = plutovg_surface_get_stride(Surface);
		for (int y = 0; y < Height; y++)
		{
			auto src = Data + y * Stride;
			auto dst = reinterpret_cast<uint32_t*>(pixels + y * step);
			for (int x = 0; x < Width; x++, src += 4)
				dst[x] = ReadPixel(src);
		}
	}

	void Export()
	{
		if (!Direct)
		{
			auto pixels = plutovg_surface_get_data(Surface);
			int step = plutovg_surface_get_stride(Surface);
			for (int y = 0; y < Height; y++)
			{
				auto dst = Data + y * Stride;
				auto src = reinterpret_cast<const uint32_t*>(pixels + y * step);
				for (int x = 0; x < Width; x++, dst += 4)
				{
					// Do not round-trip untouched straight-alpha pixels or padding.
					if (src[x] != ReadPixel(dst))
						WritePixel(dst, src[x]);
				}
			}
		}
		#if defined(__GTK_H__)
		if (auto surface = Target.GetSurface())
			Gtk::cairo_surface_mark_dirty(surface);
		#endif
	}

	void ApplyClip()
	{
		plutovg_canvas_set_matrix(Canvas, nullptr);
		plutovg_canvas_clip_rect(Canvas, (float)Clip.x1, (float)Clip.y1,
			Clip.Valid() ? (float)Clip.X() : 0.0f, Clip.Valid() ? (float)Clip.Y() : 0.0f);
		plutovg_canvas_translate(Canvas, -(float)Origin.x, -(float)Origin.y);
	}

public:
	explicit PlutoMemoryContext(LMemDC &target) : Target(target)
	{
		#if defined(HAIKU)
		Locked = Target.Lock();
		if (!Locked)
		{
			LgiTrace("LSurface::Vec: could not lock memory bitmap.\n");
			return;
		}
		#endif
		Width = Target.X();
		Height = Target.Y();
		Stride = Target.GetRowStep();
		ColourSpace = Target.GetColourSpace();
		PreMul = Target.IsPreMultipliedAlpha();
		Data = Target[0];
		if (!SetLayout() || Target.AlphaDC() || Target.DrawOnAlpha())
		{
			LgiTrace("LSurface::Vec: unsupported memory colour space or separate alpha channel.\n");
			return;
		}
		// PlutoVG's owned surfaces and rasterizer have a 32768-pixel size ceiling.
		if (!Data || Width <= 0 || Height <= 0 || Width >= 32768 || Height >= 32768 ||
			Stride > (std::numeric_limits<int>::max)() || Stride < -(std::numeric_limits<int>::max)() ||
			(Stride >= 0 ? Stride : -Stride) < Width * 4 ||
			(size_t)(Height - 1) > (size_t)((std::numeric_limits<int>::max)() - Width * 4) /
				(size_t)(Stride >= 0 ? Stride : -Stride))
		{
			LgiTrace("LSurface::Vec: inaccessible bitmap or invalid dimensions/stride.\n");
			return;
		}

		const uint32_t layout = 0x01020304;
		auto bytes = reinterpret_cast<const unsigned char*>(&layout);
		Direct = PreMul && Alpha >= 0 && bytes[Red] == 2 && bytes[Green] == 3 &&
			bytes[Blue] == 4 && bytes[Alpha] == 1 && Stride > 0 && Stride % 4 == 0 &&
			reinterpret_cast<uintptr_t>(Data) % alignof(uint32_t) == 0;
		Surface = Direct ? plutovg_surface_create_for_data(Data, Width, Height, (int)Stride) :
			plutovg_surface_create(Width, Height);
		if (Surface)
			Canvas = plutovg_canvas_create(Surface);
		if (Canvas)
			Path = plutovg_path_create();
		if (!Canvas || !Path)
		{
			LgiTrace("LSurface::Vec: could not allocate PlutoVG surface/canvas/path.\n");
			return;
		}
		Clip = static_cast<LSurface&>(Target).ClipRgn();
		LRect bounds = Target.Bounds();
		Clip.Intersection(&bounds);
		Origin = Target.GetOrigin();
		plutovg_canvas_set_fill_rule(Canvas, PLUTOVG_FILL_RULE_NON_ZERO);
		plutovg_canvas_set_operator(Canvas, PLUTOVG_OPERATOR_SRC_OVER);
		plutovg_canvas_set_line_cap(Canvas, PLUTOVG_LINE_CAP_BUTT);
		plutovg_canvas_set_line_join(Canvas, PLUTOVG_LINE_JOIN_MITER);
		ApplyClip();
	}

	~PlutoMemoryContext() override
	{
		if (Path)
			plutovg_path_destroy(Path);
		if (Canvas)
			plutovg_canvas_destroy(Canvas);
		if (Surface)
			plutovg_surface_destroy(Surface);
		#if defined(HAIKU)
		if (Locked && !Target.Unlock())
			LgiTrace("LSurface::Vec: could not unlock memory bitmap.\n");
		#endif
	}

	bool IsValid() const override
	{
		return Canvas && Path && Target[0] == Data && Target.X() == Width && Target.Y() == Height &&
			Target.GetRowStep() == Stride && Target.GetColourSpace() == ColourSpace &&
			Target.IsPreMultipliedAlpha() == PreMul && !Target.DrawOnAlpha() && !Target.AlphaDC();
	}

	void MoveTo(float x, float y) override { plutovg_path_move_to(Path, x, y); }
	void LineTo(float x, float y) override { plutovg_path_line_to(Path, x, y); }
	void CubicTo(float x1, float y1, float x2, float y2, float x, float y) override
	{
		plutovg_path_cubic_to(Path, x1, y1, x2, y2, x, y);
	}
	void ArcTo(float rx, float ry, float rotation, bool largeArc, bool sweep, float x, float y) override
	{
		plutovg_path_arc_to(Path, rx, ry, rotation, largeArc, sweep, x, y);
	}
	void Rect(float x, float y, float width, float height) override
	{
		plutovg_path_add_rect(Path, x, y, width, height);
	}
	void ClosePath() override { plutovg_path_close(Path); }
	void ClearPath() override { plutovg_path_reset(Path); }

	void Fill(LColour colour) override
	{
		Import();
		plutovg_canvas_set_rgba(Canvas, colour.r() / 255.0f, colour.g() / 255.0f,
			colour.b() / 255.0f, colour.a() / 255.0f);
		plutovg_canvas_fill_path(Canvas, Path);
		Export();
	}

	void Fill(const LPointF &start, const LPointF &end, const LArray<LColourStop> &stops) override
	{
		std::unique_ptr<plutovg_gradient_stop_t[]> nativeStops(
			new (std::nothrow) plutovg_gradient_stop_t[stops.Length()]);
		if (!nativeStops)
		{
			LgiTrace("LSurface::Vector::Fill: could not allocate gradient stops.\n");
			return;
		}
		for (size_t i = 0; i < stops.Length(); i++)
		{
			const auto &stop = stops.ItemAt(i);
			nativeStops[i] = {stop.Pos, {stop.Colour.r() / 255.0f, stop.Colour.g() / 255.0f,
				stop.Colour.b() / 255.0f, stop.Colour.a() / 255.0f}};
		}
		auto paint = plutovg_paint_create_linear_gradient(start.x, start.y, end.x, end.y,
			PLUTOVG_SPREAD_METHOD_PAD, nativeStops.get(), (int)stops.Length(), nullptr);
		if (!paint)
		{
			LgiTrace("LSurface::Vector::Fill: could not create linear gradient.\n");
			return;
		}
		Import();
		plutovg_canvas_set_paint(Canvas, paint);
		plutovg_canvas_fill_path(Canvas, Path);
		plutovg_paint_destroy(paint);
		Export();
	}

	void Stroke(LColour colour, float width) override
	{
		if (!std::isfinite(width) || width <= 0)
		{
			LgiTrace("LSurface::Vector::Stroke: width must be finite and positive.\n");
			return;
		}
		Import();
		plutovg_canvas_set_rgba(Canvas, colour.r() / 255.0f, colour.g() / 255.0f,
			colour.b() / 255.0f, colour.a() / 255.0f);
		plutovg_canvas_set_line_width(Canvas, width);
		plutovg_canvas_stroke_path(Canvas, Path);
		Export();
	}

	bool ClipRect(LRect rect) override
	{
		int64_t left = MAX((int64_t)Clip.x1, (int64_t)rect.x1 - Origin.x);
		int64_t top = MAX((int64_t)Clip.y1, (int64_t)rect.y1 - Origin.y);
		int64_t right = MIN((int64_t)Clip.x2, (int64_t)rect.x2 - Origin.x);
		int64_t bottom = MIN((int64_t)Clip.y2, (int64_t)rect.y2 - Origin.y);
		Clip = left <= right && top <= bottom ?
			LRect((int)left, (int)top, (int)right, (int)bottom) : LRect::EMPTY();
		ApplyClip();
		return true;
	}
};
}

class LSurface::Vector::Private
{
public:
	std::unique_ptr<LSurfaceVector> Context;

	explicit Private(std::unique_ptr<LSurfaceVector> context) :
		Context(std::move(context))
	{
	}

};

LSurface::Vector::Vector(LSurface *Surface)
{
	std::unique_ptr<LSurfaceVector> Context;
	if (Surface)
		Context.reset(Surface->CreateVectorContext());

	d = new (std::nothrow) Private(std::move(Context));
	if (!d)
		LgiTrace("LSurface::Vec: could not allocate vector session.\n");
}

LSurface::Vector::Vector(Vector &&v) :
	d(v.d)
{
	v.d = nullptr;
}

LSurface::Vector &LSurface::Vector::operator =(Vector &&v)
{
	if (this != &v)
	{
		delete d;
		d = v.d;
		v.d = nullptr;
	}
	return *this;
}

LSurface::Vector::~Vector()
{
	delete d;
	d = nullptr;
}

bool LSurface::Vector::IsValid() const
{
	return d && d->Context && d->Context->IsValid();
}

LSurface::Vector::operator bool() const
{
	return IsValid();
}

LSurfaceVector *LSurface::Vector::GetContext()
{
	if (*this)
		return d->Context.get();
	LgiTrace("LSurface::Vector: drawing attempted on an invalid session.\n");
	return nullptr;
}

void LSurface::Vector::MoveTo(float x, float y)
{
	if (!FiniteCoordinates({x, y}))
		return;
	if (auto context = GetContext())
		context->MoveTo(x, y);
}

void LSurface::Vector::LineTo(float x, float y)
{
	if (!FiniteCoordinates({x, y}))
		return;
	if (auto context = GetContext())
		context->LineTo(x, y);
}

void LSurface::Vector::CubicTo(float x1, float y1, float x2, float y2, float x, float y)
{
	if (!FiniteCoordinates({x1, y1, x2, y2, x, y}))
		return;
	if (auto context = GetContext())
		context->CubicTo(x1, y1, x2, y2, x, y);
}

void LSurface::Vector::ArcTo(float rx, float ry, float rotation, bool largeArc, bool sweep, float x, float y)
{
	if (!FiniteCoordinates({rx, ry, rotation, x, y}))
		return;
	if (auto context = GetContext())
		context->ArcTo(rx, ry, rotation, largeArc, sweep, x, y);
}

void LSurface::Vector::Rect(float x, float y, float width, float height)
{
	if (!FiniteCoordinates({x, y, width, height}))
		return;
	if (auto context = GetContext())
		context->Rect(x, y, width, height);
}

void LSurface::Vector::RoundRect(float x, float y, float width, float height, float radius)
{
	if (!FiniteCoordinates({x, y, width, height, radius}))
		return;
	if (auto context = GetContext())
		context->RoundRect(x, y, width, height, radius);
}

void LSurface::Vector::ClosePath()
{
	if (auto context = GetContext())
		context->ClosePath();
}

void LSurface::Vector::ClearPath()
{
	if (auto context = GetContext())
		context->ClearPath();
}

void LSurface::Vector::Fill(LColour colour)
{
	if (auto context = GetContext())
		context->Fill(colour);
}

void LSurface::Vector::Fill(const LPointF &start, const LPointF &end, const LArray<LColourStop> &stops)
{
	const double limit = (std::numeric_limits<float>::max)();
	if (!std::isfinite(start.x) || !std::isfinite(start.y) ||
		!std::isfinite(end.x) || !std::isfinite(end.y) ||
		std::fabs(start.x) > limit || std::fabs(start.y) > limit ||
		std::fabs(end.x) > limit || std::fabs(end.y) > limit ||
		((float)start.x == (float)end.x && (float)start.y == (float)end.y) ||
		stops.Length() < 2 || stops.Length() > (size_t)(std::numeric_limits<int>::max)())
	{
		LgiTrace("LSurface::Vector::Fill: linear gradients require distinct finite endpoints and at least two stops.\n");
		return;
	}
	float previous = 0;
	for (size_t i = 0; i < stops.Length(); i++)
	{
		float position = stops.ItemAt(i).Pos;
		if (!std::isfinite(position) || position < 0 || position > 1 ||
			(i && position <= previous))
		{
			LgiTrace("LSurface::Vector::Fill: gradient positions must be finite, strictly increasing, and in [0, 1].\n");
			return;
		}
		previous = position;
	}
	if (auto context = GetContext())
		context->Fill(start, end, stops);
}

void LSurface::Vector::Stroke(LColour colour, float width)
{
	if (auto context = GetContext())
		context->Stroke(colour, width);
}

bool LSurface::Vector::ClipRect(LRect rect)
{
	auto context = GetContext();
	return context && context->ClipRect(rect);
}

LSurface::Vector LSurface::Vec()
{
	return Vector(this);
}

LSurfaceVector *LSurface::CreateVectorContext()
{
	LgiTrace("LSurface::Vec: this surface has no vector backend.\n");
	return nullptr;
}

LSurfaceVector *LMemDC::CreateVectorContext()
{
	auto context = new (std::nothrow) PlutoMemoryContext(*this);
	if (!context)
		LgiTrace("LSurface::Vec: could not allocate memory vector context.\n");
	return context;
}
