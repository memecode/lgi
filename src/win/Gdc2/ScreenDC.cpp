/*hdr
**	FILE:		Gdc2.h
**	AUTHOR:		Matthew Allen
**	DATE:		20/2/97
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>
#include <cmath>
#include <memory>
#include <new>
#include <vector>

#include "lgi/common/Gdc2.h"
#include "lgi/common/GdiLeak.h"
#include "lgi/common/Palette.h"
#include "lgi/common/Variant.h"

bool LgiInitGdiPlus();

namespace
{
class GdiPlusVectorContext : public LSurfaceVector
{
	LSurface &Surface;
	std::unique_ptr<Gdiplus::Graphics> OwnedGraphics;
	Gdiplus::Graphics *Graphics = nullptr;
	HDC Dc = nullptr;
	Gdiplus::GraphicsState State = 0;
	Gdiplus::GraphicsPath Path;
	bool Saved = false;
	bool HasCurrent = false;
	Gdiplus::PointF Current;
	Gdiplus::PointF FigureStart;

	static Gdiplus::Color ToGdiColour(LColour colour)
	{
		return Gdiplus::Color((BYTE)colour.a(), (BYTE)colour.r(), (BYTE)colour.g(), (BYTE)colour.b());
	}

public:
	explicit GdiPlusVectorContext(LSurface &surface) :
		Surface(surface),
		Path(Gdiplus::FillModeWinding)
	{
		GdiFlush();
		Dc = Surface.StartDC();
		if (!Dc)
			return;

		OwnedGraphics.reset(new Gdiplus::Graphics(Dc));
		Graphics = OwnedGraphics.get();
		if (!Graphics || Graphics->GetLastStatus() != Gdiplus::Ok)
			return;

		State = Graphics->Save();
		Saved = Graphics->GetLastStatus() == Gdiplus::Ok;
		if (Saved)
		{
			Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			Graphics->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
		}
	}

	~GdiPlusVectorContext() override
	{
		if (Graphics && Saved)
		{
			Graphics->Flush(Gdiplus::FlushIntentionSync);
			Graphics->Restore(State);
		}
		OwnedGraphics.reset();
		if (Dc)
			Surface.EndDC();
	}

	bool IsValid() const override
	{
		if (!Graphics)
		{
			return false;
		}
		if (!Saved)
		{
			return false;
		}
		
		auto gfxStatus = Graphics->GetLastStatus();
		if (gfxStatus != Gdiplus::Ok)
		{
			return false;
		}

		auto pathStatus = Path.GetLastStatus();
		if (pathStatus != Gdiplus::Ok)
		{
			return false;
		}

		return true;
	}

	void MoveTo(float x, float y) override
	{
		Path.StartFigure();
		Current = FigureStart = Gdiplus::PointF(x, y);
		HasCurrent = true;
	}

	void LineTo(float x, float y) override
	{
		if (!HasCurrent)
		{
			MoveTo(x, y);
			return;
		}
		Path.AddLine(Current, Gdiplus::PointF(x, y));
		Current = Gdiplus::PointF(x, y);
	}

	void CubicTo(float x1, float y1, float x2, float y2, float x, float y) override
	{
		if (!HasCurrent)
			MoveTo(x1, y1);
		Path.AddBezier(Current, Gdiplus::PointF(x1, y1), Gdiplus::PointF(x2, y2), Gdiplus::PointF(x, y));
		Current = Gdiplus::PointF(x, y);
	}

	void ArcTo(float rx, float ry, float rotation, bool largeArc, bool sweep, float x, float y) override
	{
		if (!HasCurrent)
			MoveTo(0, 0);
		if (Current.X == x && Current.Y == y)
			return;

		double radiusX = std::fabs((double)rx), radiusY = std::fabs((double)ry);
		if (radiusX == 0 || radiusY == 0)
		{
			LineTo(x, y);
			return;
		}

		double angle = std::fmod((double)rotation, 360.0) * LGI_PI / 180.0;
		double cosine = std::cos(angle), sine = std::sin(angle);
		double halfX = (Current.X - x) / 2, halfY = (Current.Y - y) / 2;
		double localX = cosine * halfX + sine * halfY;
		double localY = -sine * halfX + cosine * halfY;
		double scale = std::hypot(localX / radiusX, localY / radiusY);
		if (scale > 1)
		{
			radiusX *= scale;
			radiusY *= scale;
		}
		double normalX = localX / radiusX, normalY = localY / radiusY;
		double distance = normalX * normalX + normalY * normalY;
		double factor = std::sqrt(MAX(0.0, (1 - distance) / distance));
		if (largeArc == sweep)
			factor = -factor;
		double centerX = factor * radiusX * normalY;
		double centerY = -factor * radiusY * normalX;
		double worldCenterX = cosine * centerX - sine * centerY + (Current.X + x) / 2;
		double worldCenterY = sine * centerX + cosine * centerY + (Current.Y + y) / 2;
		double startAngle = std::atan2((localY - centerY) / radiusY, (localX - centerX) / radiusX);
		double endAngle = std::atan2((-localY - centerY) / radiusY, (-localX - centerX) / radiusX);
		double delta = endAngle - startAngle;
		if (sweep)
		{
			while (delta < 0) delta += 2 * LGI_PI;
		}
		else
		{
			while (delta > 0) delta -= 2 * LGI_PI;
		}

		auto Point = [&](double theta)
		{
			return Gdiplus::PointF(
				(float)(worldCenterX + cosine * radiusX * std::cos(theta) - sine * radiusY * std::sin(theta)),
				(float)(worldCenterY + sine * radiusX * std::cos(theta) + cosine * radiusY * std::sin(theta)));
		};
		int segments = (int)std::ceil(std::fabs(delta) / (LGI_PI / 2));
		double step = delta / segments;
		for (int i = 0; i < segments; i++)
		{
			double theta1 = startAngle + i * step, theta2 = theta1 + step;
			double alpha = 4.0 / 3.0 * std::tan(step / 4.0);
			auto p0 = Point(theta1), p3 = Point(theta2);
			double d0x = -cosine * radiusX * std::sin(theta1) - sine * radiusY * std::cos(theta1);
			double d0y = -sine * radiusX * std::sin(theta1) + cosine * radiusY * std::cos(theta1);
			double d1x = -cosine * radiusX * std::sin(theta2) - sine * radiusY * std::cos(theta2);
			double d1y = -sine * radiusX * std::sin(theta2) + cosine * radiusY * std::cos(theta2);
			Gdiplus::PointF p1((float)(p0.X + alpha * d0x), (float)(p0.Y + alpha * d0y));
			Gdiplus::PointF p2((float)(p3.X - alpha * d1x), (float)(p3.Y - alpha * d1y));
			Path.AddBezier(Current, p1, p2, i + 1 == segments ? Gdiplus::PointF(x, y) : p3);
			Current = i + 1 == segments ? Gdiplus::PointF(x, y) : p3;
		}
	}

	void Rect(float x, float y, float width, float height) override
	{
		Path.StartFigure();
		Path.AddRectangle(Gdiplus::RectF(x, y, width, height));
		Current = FigureStart = Gdiplus::PointF(x, y);
		HasCurrent = true;
	}

	void ClosePath() override
	{
		Path.CloseFigure();
		if (HasCurrent)
			Current = FigureStart;
	}

	void ClearPath() override
	{
		Path.Reset();
		HasCurrent = false;
	}

	void Fill(LColour colour) override
	{
		Gdiplus::SolidBrush brush(ToGdiColour(colour));
		if (brush.GetLastStatus() == Gdiplus::Ok)
			Graphics->FillPath(&brush, &Path);
	}

	void Fill(const LPointF &start, const LPointF &end, const LArray<LColourStop> &stops) override
	{
		std::vector<Gdiplus::Color> colours;
		std::vector<Gdiplus::REAL> positions;
		const auto &first = stops.ItemAt(0);
		const auto &last = stops.ItemAt(stops.Length() - 1);
		colours.reserve(stops.Length() + 2);
		positions.reserve(stops.Length() + 2);
		if (first.Pos > 0)
		{
			colours.push_back(ToGdiColour(first.Colour));
			positions.push_back(0);
		}
		for (size_t i = 0; i < stops.Length(); i++)
		{
			const auto &stop = stops.ItemAt(i);
			colours.push_back(ToGdiColour(stop.Colour));
			positions.push_back(stop.Pos);
		}
		if (last.Pos < 1)
		{
			colours.push_back(ToGdiColour(last.Colour));
			positions.push_back(1);
		}

		Gdiplus::RectF bounds;
		if (Path.GetBounds(&bounds) != Gdiplus::Ok)
		{
			LgiTrace("GdiPlusVectorContext::Fill: failed to get path bounds.\n");
			return;
		}

		double dx = end.x - start.x, dy = end.y - start.y;
		double lengthSquared = dx * dx + dy * dy;
		double x1 = dx < 0 ? bounds.GetRight() : bounds.GetLeft();
		double y1 = dy < 0 ? bounds.GetBottom() : bounds.GetTop();
		double x2 = dx < 0 ? bounds.GetLeft() : bounds.GetRight();
		double y2 = dy < 0 ? bounds.GetTop() : bounds.GetBottom();
		double firstExtent = ((x1 - start.x) * dx + (y1 - start.y) * dy) / lengthSquared;
		double lastExtent = ((x2 - start.x) * dx + (y2 - start.y) * dy) / lengthSquared;
		if (firstExtent > 0)
			firstExtent = 0;
		if (lastExtent < 1)
			lastExtent = 1;

		double extent = lastExtent - firstExtent;
		for (auto &position : positions)
			position = (Gdiplus::REAL)((position - firstExtent) / extent);
		if (firstExtent < 0)
		{
			colours.insert(colours.begin(), colours.front());
			positions.insert(positions.begin(), 0);
		}
		if (lastExtent > 1)
		{
			colours.push_back(colours.back());
			positions.push_back(1);
		}

		Gdiplus::PointF p1(
			(float)(start.x + firstExtent * dx),
			(float)(start.y + firstExtent * dy));
		Gdiplus::PointF p2(
			(float)(start.x + lastExtent * dx),
			(float)(start.y + lastExtent * dy));
		Gdiplus::LinearGradientBrush brush(p1, p2, colours.front(), colours.back());
		auto status = brush.GetLastStatus();
		if (status == Gdiplus::Ok)
			status = brush.SetInterpolationColors(colours.data(), positions.data(), (INT)colours.size());
		if (status == Gdiplus::Ok)
			status = Graphics->FillPath(&brush, &Path);
		if (status != Gdiplus::Ok)
			LgiTrace("GdiPlusVectorContext::Fill: gradient fill failed: %i\n", (int)status);
	}

	void Stroke(LColour colour, float width) override
	{
		if (!std::isfinite(width) || width <= 0)
			return;
		Gdiplus::Pen pen(ToGdiColour(colour), width);
		if (pen.GetLastStatus() != Gdiplus::Ok)
			return;
		pen.SetStartCap(Gdiplus::LineCapFlat);
		pen.SetEndCap(Gdiplus::LineCapFlat);
		pen.SetLineJoin(Gdiplus::LineJoinMiter);
		pen.SetMiterLimit(10.0f);
		Graphics->DrawPath(&pen, &Path);
	}

	bool ClipRect(LRect rect) override
	{
		Gdiplus::RectF clip((float)rect.x1, (float)rect.y1,
			rect.Valid() ? (float)rect.X() : 0, rect.Valid() ? (float)rect.Y() : 0);
		return Graphics->SetClip(clip, Gdiplus::CombineModeIntersect) == Gdiplus::Ok;
	}
};
}

class LScreenPrivate
{
public:
	#if defined WIN32

	bool		Release;
	bool		End;
	HWND		hWnd;
	PAINTSTRUCT	Ps;
	HPEN		hPen;
	HBRUSH		hBrush;
	HBITMAP		hOldBitmap;
	LRect		Client;

	int			Mode;
	COLOUR		Col;
	int			Sx, Sy;
	LGlobalColour *Gc;
	NativeInt	ConstAlpha;
	
	class NullObjects
	{
		friend class LScreenDC;
		HPEN Pen;
		HBRUSH Brush;

	public:
		NullObjects()
		{
			LOGBRUSH LogBrush;
			LogBrush.lbStyle = BS_NULL;
			LogBrush.lbColor = 0;
			LogBrush.lbHatch = 0;
			Brush = CreateBrushIndirect(&LogBrush);
			Pen = CreatePen(PS_NULL, 1, 0);
		}

		~NullObjects()
		{
			DeleteObject(Pen);
			DeleteObject(Brush);
		}
	};

	static LPalette *LastRealized;
	static NullObjects Null;

	LScreenPrivate()
	{
		Gc = 0;
		hWnd = 0;
		hPen = 0;
		hBrush = 0;
		hOldBitmap = 0;
		Mode = GDC_ALPHA;
		Col = 0;
		Sx = Sy = 0;
		Release = End = false;
		Client.ZOff(-1, -1);
		ConstAlpha = -1;
	}

	#else

		OsView		View;
		int			BitDepth;

		bool		_ClientClip;
		LRect		_Client;
		void		_SetClient(LRect *c);

		#ifdef LINUX
		friend class LFont;
		friend class LView;
		
		COLOUR		Col;
		QPainter	p;
		#endif

	#endif
};

LScreenPrivate::NullObjects LScreenPrivate::Null;
LPalette *LScreenPrivate::LastRealized = 0;


/////////////////////////////////////////////////////////////////////////////////////////////////////
LScreenDC::LScreenDC()
{
	d = new LScreenPrivate;
	ColourSpace = CsNone;
}

LScreenDC::LScreenDC(LViewI *view)
{
	d = new LScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	d->hWnd = view->Handle();
	d->End = true;
	CreateFromHandle(BeginPaint(d->hWnd, &d->Ps));

	RECT rc;
	GetClientRect(d->hWnd, &rc);
	SetSize(rc.right-rc.left, rc.bottom-rc.top);
}

LScreenDC::LScreenDC(HWND hWindow)
{
	d = new LScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	d->hWnd = hWindow;
	d->End = true;
	CreateFromHandle(BeginPaint(d->hWnd, &d->Ps));

	RECT rc;
	GetClientRect(d->hWnd, &rc);
	SetSize(rc.right-rc.left, rc.bottom-rc.top);
}

LScreenDC::LScreenDC(HDC hdc, HWND hwnd, bool Release)
{
	d = new LScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	LAssert(hdc != 0);
	d->hWnd = hwnd;
	CreateFromHandle(hdc);
	d->Release = Release;

	RECT rc;
	if (GetWindowRect(d->hWnd, &rc))
	{
		SetSize(rc.right-rc.left, rc.bottom-rc.top);
	}
}

LScreenDC::LScreenDC(HBITMAP hbmp, int Sx, int Sy)
{
	d = new LScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	CreateFromHandle(CreateCompatibleDC(0));
	hBmp = hbmp;
	d->hOldBitmap = (HBITMAP) SelectObject(hDC, hBmp);
	SetSize(Sx, Sy);
}

LScreenDC::~LScreenDC()
{
	GdiplusGfx.Reset();

	if (hDC)
	{
		if (d->hPen)
		{
			d->hPen = (HPEN) SelectObject(hDC, d->hPen);
			if (d->hPen)
			{
				DeleteObject(d->hPen);
			}
		}
		if (d->hBrush)
		{
			d->hBrush = (HBRUSH) SelectObject(hDC, d->hBrush);
			if (d->hBrush)
			{
				DeleteObject(d->hBrush);
			}
		}

		if (d->hOldBitmap)
		{
			SelectObject(hDC, d->hOldBitmap);
			DeleteDC(hDC);
			hDC = NULL;
		}
	}

	if (d->End)
	{
		EndPaint(d->hWnd, &d->Ps);
	}
	else if (d->Release)
	{
		ReleaseDC(d->hWnd, hDC);
	}

	hDC = NULL;
	DeleteObj(d);
}

LSurfaceVector *LScreenDC::CreateVectorContext()
{
	if (!LgiInitGdiPlus())
		return nullptr;
	return new (std::nothrow) GdiPlusVectorContext(*this);
}

bool LScreenDC::GetClient(LRect *c)
{
	if (!c) return false;
	*c = d->Client;
	return true;
}

void LScreenDC::SetClient(LRect *c)
{
	GdiplusGfx.Reset();

	if (c)
	{
		SetWindowOrgEx(hDC, 0, 0, NULL);
		SelectClipRgn(hDC, 0);
		
		#if 1
		HRGN hRgn = CreateRectRgn(c->x1, c->y1, c->x2+1, c->y2+1);
		if (hRgn)
		{
			// LgiTrace("%s:%i HRGN=%p\n", _FL, hRgn);
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}
		#endif

		SetWindowOrgEx(hDC, -c->x1, -c->y1, NULL);
		d->Client = *c;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		SetWindowOrgEx(hDC, 0, 0, NULL);
		SelectClipRgn(hDC, 0);
	}
}

void LScreenDC::SetSize(int x, int y)
{
	d->Sx = x;
	d->Sy = y;
	Clip.ZOff(d->Sx-1, d->Sy-1);
}

bool LScreenDC::CreateFromHandle(HDC hdc)
{
	bool Status = FALSE;

	hDC = hdc;
	if (hdc)
	{
		LOGBRUSH LogBrush;
		LogBrush.lbStyle = BS_SOLID;
		LogBrush.lbColor = 0;
		LogBrush.lbHatch = 0;
		d->hBrush = (HBRUSH) SelectObject(hDC, CreateBrushIndirect(&LogBrush));
		d->hPen = (HPEN) SelectObject(hDC, CreatePen(PS_SOLID, 1, 0));

		Status = TRUE;
	}

	if (GetBits() == 8 && d->hWnd)
	{
		d->Gc = GdcD->GetGlobalColour();
		if (d->Gc)
		{
			LPalette *Pal = d->Gc->GetPalette();
			if (Pal)
			{
				HPALETTE hpal = SelectPalette(hdc, Pal->Handle(), false);
				RealizePalette(hdc);
			}
		}
	}

	return Status;
}

LPoint LScreenDC::GetOrigin()
{
	POINT pt;
	if (GetWindowOrgEx(hDC, &pt))
		return LPoint(pt.x, pt.y);
	else
		return LPoint();
}

void LScreenDC::SetOrigin(LPoint pt)
{
	GdiplusGfx.Reset();
	LSurface::SetOrigin(pt);
	if (hDC)
		SetWindowOrgEx(hDC, pt.x, pt.y, NULL);
}

LPalette *LScreenDC::Palette()
{
	return LSurface::Palette();
}

void LScreenDC::Palette(LPalette *pPal, bool bOwnIt)
{
	LSurface::Palette(pPal, bOwnIt);
}

LRect LScreenDC::ClipRgn(LRect *Rgn)
{
	LRect Prev = Clip;
	GdiplusGfx.Reset();

	if (Rgn)
	{
		POINT Origin;
		GetWindowOrgEx(hDC, &Origin);

		Clip.x1 = max(Rgn->x1 - Origin.x, 0);
		Clip.y1 = max(Rgn->y1 - Origin.y, 0);
		Clip.x2 = min(Rgn->x2 - Origin.x, d->Sx-1);
		Clip.y2 = min(Rgn->y2 - Origin.y, d->Sy-1);

		if (Clip.x2 < Clip.x1)
			Clip.x2 = Clip.x1-1;
		if (Clip.y2 < Clip.y1)
			Clip.y2 = Clip.y1-1;

		#if 1
		HRGN hRgn = CreateRectRgn(Clip.x1, Clip.y1, Clip.x2+1, Clip.y2+1);
		if (hRgn)
		{
			// LgiTrace("%s:%i HRGN=%p\n", _FL, hRgn);
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}
		#endif
	}
	else
	{
		Clip.x1 = 0;
		Clip.y1 = 0;
		Clip.x2 = X()-1;
		Clip.y2 = Y()-1;

		SelectClipRgn(hDC, NULL);
	}

	return Prev;
}

LRect LScreenDC::ClipRgn()
{
	return Clip;
}

COLOUR LScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = d->Col;

	if (hDC)
	{
		d->hPen = (HPEN) SelectObject(hDC, d->hPen);
		d->hBrush = (HBRUSH) SelectObject(hDC, d->hBrush);
		DeleteObject(d->hPen);
		DeleteObject(d->hBrush);

		if (Bits)
		{
			d->Col = CBit(24, c, Bits, pPalette);
		}
		else
		{
			d->Col = CBit(24, c, GetBits(), pPalette);
		}

		if (d->Gc)
		{
			// Give the global colour manager a chance to map the
			// colour to a palette index (8 bit only)
			d->Col = d->Gc->GetColour(d->Col);
		}

		uint32_t WinCol = RGB( R24(d->Col), G24(d->Col), B24(d->Col) );
		LOGBRUSH LogBrush;
		LogBrush.lbStyle = BS_SOLID;
		LogBrush.lbColor = WinCol;
		LogBrush.lbHatch = 0;
		d->hBrush = (HBRUSH) SelectObject(hDC, CreateBrushIndirect(&LogBrush));
		
		if (LineBits == 0xffffffff)
		{
			d->hPen = (HPEN) SelectObject(hDC, CreatePen(PS_SOLID, 1, WinCol));
		}
		else
		{
			int Type = PS_SOLID;
			switch (LineBits)
			{
				case LineNone:
					Type = PS_NULL;
					break;
				case LineSolid:
					Type = PS_SOLID;
					break;
				case LineAlternate:
					Type = PS_ALTERNATE;
					break;
				case LineDash:
					Type = PS_DASH;
					break;
				case LineDot:
					Type = PS_DOT;
					break;
				case LineDashDot:
					Type = PS_DASHDOT;
					break;
				case LineDashDotDot:
					Type = PS_DASHDOTDOT;
					break;
			}
			LOGBRUSH br = { BS_SOLID, WinCol, HS_VERTICAL };
			d->hPen = (HPEN) SelectObject(hDC, ExtCreatePen(PS_COSMETIC | Type, 1, &br, 0, NULL));
		}
	}

	return Prev;
}

LColour LScreenDC::Colour(LColour c)
{
	LColour cPrev(d->Col, GetBits());
	Colour(c.c32(), 32);
	return cPrev;
}

LString LScreenDC::Dump()
{
	LString s;
	s.Printf("LScreenDC hnd=%p size=%i,%i\n", hDC, d->Sx, d->Sy);
	return s;
}

bool LScreenDC::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	switch (LStringToDomProp(Name))
	{
		case SurfaceConstAlpha:
		{
			Value = d->ConstAlpha;
			return true;
		}
	}

	return false;
}

bool LScreenDC::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	switch (LStringToDomProp(Name))
	{
		case SurfaceConstAlpha:
		{
			d->ConstAlpha = Value.CastInt32();
			return true;
		}
	}

	return false;
}

int LScreenDC::Op(int Op, NativeInt Param)
{
	int Prev = d->Mode;
	int Rop;

	switch (Op)
	{
		case GDC_ALPHA:
		case GDC_SET:
		{
			Rop = R2_COPYPEN;
			break;
		}
		case GDC_AND:
		{
			Rop = R2_MASKPEN;
			break;
		}
		case GDC_OR:
		{
			Rop = R2_MERGEPEN;
			break;
		}
		case GDC_XOR:
		{
			Rop = R2_XORPEN;
			break;
		}
		default:
		{
			return Prev;
		}
	}

	SetROP2(hDC, Rop);
	d->Mode = Op;
	d->ConstAlpha = Param;

	return Prev;
}

COLOUR LScreenDC::Colour()
{
	return d->Col;
}

int LScreenDC::Op()
{
	return d->Mode;
}

int LScreenDC::X()
{
	if (d->Client.Valid())
		return d->Client.X();

	return d->Sx;
}

int LScreenDC::Y()
{
	if (d->Client.Valid())
		return d->Client.Y();

	return d->Sy;
}

int LScreenDC::GetBits()
{
	return GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES);
}

bool LScreenDC::SupportsAlphaCompositing()
{
	// Windows does support blending screen content with bitmaps that have alpha
	return true;
}

LPoint LScreenDC::GetDpi()
{
	return LPoint(GetDeviceCaps(hDC, HORZRES), GetDeviceCaps(hDC, VERTRES));
}

void LScreenDC::Set(int x, int y)
{
	uint32_t WinCol = RGB( R24(d->Col), G24(d->Col), B24(d->Col) );
	SetPixel(hDC, x, y, WinCol);
}

COLOUR LScreenDC::Get(int x, int y)
{
	return GetPixel(hDC, x, y);
}

uint LScreenDC::LineStyle(uint32_t Bits, uint32_t Reset)
{
	uint Old = LineBits;
	LineBits = Bits;
	return Old;
}

uint LScreenDC::LineStyle()
{
	return LineBits;
}

void LScreenDC::HLine(int x1, int x2, int y)
{
	if (x1 > x2)
	{
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	MoveToEx(hDC, x1, y, NULL);
	LineTo(hDC, x2 + 1, y);
}

void LScreenDC::VLine(int x, int y1, int y2)
{
	if (y1 > y2)
	{
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	MoveToEx(hDC, x, y1, NULL);
	LineTo(hDC, x, y2 + 1);
}

void LScreenDC::Line(int x1, int y1, int x2, int y2)
{
	MoveToEx(hDC, x1, y1, NULL);
	LineTo(hDC, x2, y2);
	uint32_t WinCol = RGB( R24(d->Col), G24(d->Col), B24(d->Col) );
	SetPixel(hDC, x2, y2, WinCol);
}

void LScreenDC::Circle(float cx, float cy, float radius)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Ellipse(	hDC,
				(int)floor(cx - radius),
				(int)floor(cy - radius),
				(int)ceil(cx + radius),
				(int)ceil(cy + radius));
	SelectObject(hDC, hTemp);
}

void LScreenDC::FilledCircle(float cx, float cy, float radius)
{
	::Ellipse(	hDC,
				(int)floor(cx - radius),
				(int)floor(cy - radius),
				(int)ceil(cx + radius),
				(int)ceil(cy + radius));
}

void LScreenDC::Arc(float cx, float cy, float radius, float start, float end)
{
	int StartX = (int)(cx + (cos(start) * radius));
	int StartY = (int)(cy + (int)(sin(start) * radius));
	int EndX = (int)(cx + (int)(cos(end) * radius));
	int EndY = (int)(cy + (int)(sin(end) * radius));

	::Arc(	hDC,
			(int)floor(cx - radius),
			(int)floor(cy - radius),
			(int)ceil(cx + radius),
			(int)ceil(cy + radius),
			StartX, StartY,
			EndX, EndY);
}

void LScreenDC::FilledArc(float cx, float cy, float radius, float start, float end)
{
	int StartX = (int)(cx + (cos(start) * radius));
	int StartY = (int)(cy + (int)(sin(start) * radius));
	int EndX = (int)(cx + (int)(cos(end) * radius));
	int EndY = (int)(cy + (int)(sin(end) * radius));

	::Pie(	hDC,
			(int)floor(cx - radius),
			(int)floor(cy - radius),
			(int)ceil(cx + radius),
			(int)ceil(cy + radius),
			StartX, StartY,
			EndX, EndY);
}

void LScreenDC::Ellipse(float cx, float cy, float x, float y)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Ellipse(	hDC,
				(int)floor(cx - x),
				(int)floor(cy - y),
				(int)ceil(cx + x),
				(int)ceil(cy + y)
				);
	SelectObject(hDC, hTemp);
}

void LScreenDC::FilledEllipse(float cx, float cy, float x, float y)
{
	::Ellipse(	hDC,
				(int)floor(cx - x),
				(int)floor(cy - y),
				(int)ceil(cx + x),
				(int)ceil(cy + y)
				);
}

void LScreenDC::Box(int x1, int y1, int x2, int y2)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Rectangle(hDC, x1, y1, x2+1, y2+1);
	SelectObject(hDC, hTemp);
}

void LScreenDC::Box(LRect *a)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	if (a)
		::Rectangle(hDC, a->x1, a->y1, a->x2+1, a->y2+1);
	else
		::Rectangle(hDC, 0, 0, X(), Y());
	SelectObject(hDC, hTemp);
}

void LScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	::Rectangle(hDC, x1, y1, x2+1, y2+1);
}

void LScreenDC::Rectangle(LRect *a)
{
	LRect b;
	if (a)
		b = *a;
	else
		b.ZOff(X()-1, Y()-1);

	::Rectangle(hDC, b.x1, b.y1, b.x2+1, b.y2+1);
}

void LScreenDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
	if (Src)
	{
		LRect b;
		if (a)
			b = *a;
		else
			b.ZOff(Src->X()-1, Src->Y()-1);

		int RowOp;

		switch (d->Mode)
		{
			case GDC_SET:
			{
				RowOp = SRCCOPY;
				break;
			}
			case GDC_AND:
			{
				RowOp = SRCAND;
				break;
			}
			case GDC_OR:
			{
				RowOp = SRCPAINT;
				break;
			}
			case GDC_XOR:
			{
				RowOp = SRCINVERT;
				break;
			}
			case GDC_ALPHA:
			{
				if (GdcD->AlphaBlend &&
					Src->GetBits() == 32)
				{
					HDC hDestDC = StartDC();
					HDC hSrcDC = Src->StartDC();

					BLENDFUNCTION Blend;
					Blend.BlendOp = AC_SRC_OVER;
					Blend.BlendFlags = 0;
					Blend.SourceConstantAlpha = (BYTE)
												(d->ConstAlpha >= 0 &&
												d->ConstAlpha <= 255 ?
												d->ConstAlpha :
												255);
					Blend.AlphaFormat = AC_SRC_ALPHA;

					if (!GdcD->AlphaBlend(	hDestDC,
											x, y,
											b.X(), b.Y(),
											hSrcDC,
											b.x1, b.y1,
											b.X(), b.Y(),
											Blend))
					{
						printf("%s:%i - AlphaBlend failed.\n", _FL);
					}

					Src->EndDC();
					EndDC();
					return;
				}
				else
				{
					// Lame '95
					RowOp = SRCCOPY;
				}
				break;
			}
			default:
			{
				return;
			}
		}

		HDC hDestDC = StartDC();
		HDC hSrcDC = Src->StartDC();
		LMemDC Tmp(_FL);
		if (!hSrcDC)
		{
			LColourSpace Cs = GdcD->GetColourSpace();
			if (!Tmp.Create(b.X(), b.Y(), Cs))
			{
				return;
			}
			
			Tmp.Blt(0, 0, Src, &b);
			LAssert(Tmp.GetBitmap() != 0);
			Src = &Tmp;
			hSrcDC = Src->StartDC();
		}

		LPalette *Pal = Src->DrawOnAlpha() ? NULL : Src->Palette();
		HPALETTE sPal = 0, dPal = 0;
		if (Pal)
		{
			Src->Update(-1);
			dPal = SelectPalette(hDestDC, Pal->Handle(), false);
		}

		POINT Old;
		GetWindowOrgEx(hDestDC, &Old);
		int RealX = x - Old.x;
		int RealY = y - Old.y;

		BOOL Ret = BitBlt(hDestDC, x, y, b.X(), b.Y(), hSrcDC, b.x1, b.y1, RowOp);
		if (!Ret)
		{
			int asd=0;
		}

		if (Pal)
		{
			dPal = SelectPalette(hDestDC, dPal, false);
		}
		
		Src->EndDC();
		EndDC();
	}
}

void LScreenDC::StretchBlt(LRect *dst, LSurface *Src, LRect *s)
{
	if (Src)
	{
		LRect DestR;
		if (dst)
		{
			DestR = *dst;
		}
		else
		{
			DestR.ZOff(X()-1, Y()-1);
		}

		LRect SrcR;
		if (s)
		{
			SrcR = *s;
		}
		else
		{
			SrcR.ZOff(Src->X()-1, Src->Y()-1);
		}

		int RowOp;

		switch (d->Mode)
		{
			case GDC_SET:
			{
				RowOp = SRCCOPY;
				break;
			}
			case GDC_AND:
			{
				RowOp = SRCAND;
				break;
			}
			case GDC_OR:
			{
				RowOp = SRCPAINT;
				break;
			}
			case GDC_XOR:
			{
				RowOp = SRCINVERT;
				break;
			}
			default:
			{
				return;
			}
		}

		HDC hDestDC = StartDC();
		HDC hSrcDC = Src->StartDC();
		::StretchBlt(hDestDC, DestR.x1, DestR.y1, DestR.X(), DestR.Y(), hSrcDC, SrcR.x1, SrcR.y1, SrcR.X(), SrcR.Y(), RowOp);
		Src->EndDC();
		EndDC();
	}
}

void LScreenDC::Polygon(int Points, LPoint *Data)
{
	if (Points > 0 && Data)
	{
		PPOINT pt = new POINT[Points];
		if (pt)
		{
			int *d = (int*)Data;

			for (int i=0; i<Points; i++)
			{
				pt[i].x = *d++;
				pt[i].y = *d++;
			}

			::Polygon(hDC, pt, Points);
			DeleteArray(pt);
		}
	}
}

void LScreenDC::Bezier(int Threshold, LPoint *Pt)
{
}
