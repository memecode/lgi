/*hdr
**	FILE:			LScreenDC.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			14/10/2000
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>
#include <cmath>
#include <new>

#include "lgi/common/Lgi.h"

using namespace Gtk;

namespace
{
class CairoVectorContext : public LSurfaceVector
{
	cairo_t *Context = nullptr;
	cairo_path_t *SavedPath = nullptr;

	void SetColour(LColour colour)
	{
		cairo_set_source_rgba(Context, colour.r() / 255.0, colour.g() / 255.0,
			colour.b() / 255.0, colour.a() / 255.0);
	}

public:
	explicit CairoVectorContext(cairo_t *context)
	{
		if (!context || cairo_status(context) != CAIRO_STATUS_SUCCESS)
			return;
		SavedPath = cairo_copy_path(context);
		if (!SavedPath || SavedPath->status != CAIRO_STATUS_SUCCESS)
			return;

		Context = cairo_reference(context);
		cairo_save(Context);
		cairo_new_path(Context);
		cairo_set_operator(Context, CAIRO_OPERATOR_OVER);
		cairo_set_fill_rule(Context, CAIRO_FILL_RULE_WINDING);
		cairo_set_line_cap(Context, CAIRO_LINE_CAP_BUTT);
		cairo_set_line_join(Context, CAIRO_LINE_JOIN_MITER);
		cairo_set_miter_limit(Context, 10.0);
		cairo_set_dash(Context, nullptr, 0, 0);
		cairo_set_antialias(Context, CAIRO_ANTIALIAS_DEFAULT);
	}

	~CairoVectorContext() override
	{
		if (Context)
		{
			cairo_restore(Context);
			cairo_new_path(Context);
			cairo_append_path(Context, SavedPath);
			cairo_destroy(Context);
		}
		if (SavedPath)
			cairo_path_destroy(SavedPath);
	}

	bool IsValid() const override
	{
		return Context && cairo_status(Context) == CAIRO_STATUS_SUCCESS;
	}

	void MoveTo(float x, float y) override { cairo_move_to(Context, x, y); }
	void LineTo(float x, float y) override { cairo_line_to(Context, x, y); }
	void CubicTo(float x1, float y1, float x2, float y2, float x, float y) override
	{
		cairo_curve_to(Context, x1, y1, x2, y2, x, y);
	}
	void ArcTo(float rx, float ry, float rotation, bool largeArc, bool sweep, float x, float y) override
	{
		double startX = 0, startY = 0;
		if (cairo_has_current_point(Context))
			cairo_get_current_point(Context, &startX, &startY);
		else
			cairo_move_to(Context, startX, startY);
		if (startX == x && startY == y)
			return;
		double radiusX = std::fabs((double)rx), radiusY = std::fabs((double)ry);
		if (radiusX == 0 || radiusY == 0)
		{
			cairo_line_to(Context, x, y);
			return;
		}

		double angle = std::fmod((double)rotation, 360.0) * LGI_PI / 180.0;
		double cosine = std::cos(angle), sine = std::sin(angle);
		double halfX = (startX - x) / 2, halfY = (startY - y) / 2;
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
		double startAngle = std::atan2((localY - centerY) / radiusY, (localX - centerX) / radiusX);
		double endAngle = std::atan2((-localY - centerY) / radiusY, (-localX - centerX) / radiusX);

		cairo_save(Context);
		cairo_translate(Context, cosine * centerX - sine * centerY + (startX + x) / 2,
			sine * centerX + cosine * centerY + (startY + y) / 2);
		cairo_rotate(Context, angle);
		cairo_scale(Context, radiusX, radiusY);
		if (sweep)
			cairo_arc(Context, 0, 0, 1, startAngle, endAngle);
		else
			cairo_arc_negative(Context, 0, 0, 1, startAngle, endAngle);
		cairo_restore(Context);
	}
	void Rect(float x, float y, float width, float height) override
	{
		cairo_rectangle(Context, x, y, width, height);
	}
	void ClosePath() override { cairo_close_path(Context); }
	void ClearPath() override { cairo_new_path(Context); }

	void Fill(LColour colour) override
	{
		SetColour(colour);
		cairo_fill_preserve(Context);
	}
	void Stroke(LColour colour, float width) override
	{
		if (!std::isfinite(width) || width <= 0)
		{
			LgiTrace("LSurface::Vector::Stroke: width must be finite and positive.\n");
			return;
		}
		SetColour(colour);
		cairo_set_line_width(Context, width);
		cairo_stroke_preserve(Context);
	}
	bool ClipRect(LRect rect) override
	{
		auto path = cairo_copy_path(Context);
		if (!path || path->status != CAIRO_STATUS_SUCCESS)
		{
			if (path)
				cairo_path_destroy(path);
			return false;
		}
		cairo_new_path(Context);
		cairo_rectangle(Context, rect.x1, rect.y1,
			rect.Valid() ? rect.X() : 0, rect.Valid() ? rect.Y() : 0);
		cairo_clip(Context);
		cairo_append_path(Context, path);
		cairo_path_destroy(path);
		return IsValid();
	}
};
}

class LScreenPrivate
{
public:
	int x = 0, y = 0, Bits = 0;
	bool Own = false;
	LColour Col;
	LRect Client;
	int ConstAlpha = 255;

	LView *View = nullptr;
	OsView v = nullptr;
	OsDrawable *d = nullptr;
	cairo_t *cr = nullptr;
	cairo_matrix_t matrix;

	LScreenPrivate()
	{
		Client.ZOff(-1, -1);
	}
	
	~LScreenPrivate()
	{
	}
};

// Translates are cumulative... so we undo the last one before resetting it.
/////////////////////////////////////////////////////////////////////////////////////////////////////
LScreenDC::LScreenDC()
{
	d = new LScreenPrivate;
	d->x = GdcD->X();
	d->y = GdcD->Y();
}

LScreenDC::LScreenDC(int x, int y, int bits)
{
	d = new LScreenPrivate;
	d->x = x;
	d->y = y;
	d->Bits = bits;
}

LScreenDC::LScreenDC(Gtk::cairo_t *cr, int x, int y)
{
	d = new LScreenPrivate;
	d->Own = false;
	d->cr = cr;
	d->x = x;
	d->y = y;
	d->Bits = 32;
	ColourSpace = GdcD->GetColourSpace();
	cairo_get_matrix(d->cr, &d->matrix);
}

LScreenDC::LScreenDC(OsDrawable *Drawable)
{
	d = new LScreenPrivate;
	d->Own = false;
	d->d = Drawable;
}

LScreenDC::LScreenDC(LView *view, void *param)
{
	d = new LScreenPrivate;
	d->View = view;
	if (view)
	{
		LWindow *w = view->GetWindow();
		OsView v = w? GTK_WIDGET(w->WindowHandle()) : NULL;
		if (v)
		{
			d->v = v;
			LAssert(!"Gtk3 FIXME");
		}
		else
		{
			d->x = view->X();
			d->y = view->Y();
			d->Bits = 0;
			d->Own = false;

		    GdkScreen *s = gdk_display_get_default_screen(gdk_display_get_default());
		    if (s)
		    {
		        GdkVisual *v = gdk_screen_get_system_visual(s);
		        if (v)
		        {
					d->Bits = gdk_visual_get_depth(v);
		            // d->Bits = v->depth;
			        ColourSpace = GdkVisualToColourSpace(v, d->Bits);
		        }
		    }
		}
    }
    else
    {
    	printf("%s:%i - No view?\n", _FL);
    }
}

LScreenDC::~LScreenDC()
{
	DeleteObj(d);
}

LSurfaceVector *LScreenDC::CreateVectorContext()
{
	return new (std::nothrow) CairoVectorContext(d->cr);
}

OsPainter LScreenDC::Handle()
{
	return d->cr;
}

LPoint LScreenDC::GetDpi()
{
	return LScreenDpi();
}

LString LScreenDC::Dump()
{
	LString s;
	s.Printf("LScreenDC size=%i,%i\n", d->x, d->y);
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

bool LScreenDC::SupportsAlphaCompositing()
{
	// GTK/X11 doesn't seem to support alpha compositing.
	return false;
}

LPoint LScreenDC::GetSize()
{
	return LPoint(d->x, d->y);
}

bool LScreenDC::GetClient(LRect *c)
{
	if (!c)
		return false;

	*c = d->Client;
	return true;
}

LPoint LScreenDC::GetOrigin()
{
	if (d->Client.Valid())
	{
		return LPoint(OriginX + d->Client.x1,
					  OriginY + d->Client.y1);
	}

	return LPoint(OriginX, OriginY);
}

void LScreenDC::SetOrigin(LPoint pt)
{
	if (d->Client.Valid())
	{
		OriginX = pt.x - d->Client.x1;
		OriginY = pt.y - d->Client.y1;
	}
	else
	{
		OriginX = pt.x;
		OriginY = pt.y;
	}

	cairo_matrix_t m;
	cairo_get_matrix(d->cr, &m);
	m.x0 = d->matrix.x0 - OriginX;
	m.y0 = d->matrix.y0 - OriginY;
	cairo_set_matrix(d->cr, &m);
}

void LScreenDC::SetClient(LRect *c)
{
	if (c)
	{
		d->Client = *c;

		cairo_save(d->cr);
		cairo_rectangle(d->cr, c->x1, c->y1, c->X(), c->Y());
		cairo_clip(d->cr);		
		cairo_translate(d->cr, c->x1, c->y1);

		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		if (Clip.Valid())
			ClipRgn(NULL);
		
		OriginX = 0;
		OriginY = 0;	

		cairo_restore(d->cr);

		d->Client.ZOff(-1, -1);
	}
}

LRect *LScreenDC::GetClient()
{
	return &d->Client;
}

uint LScreenDC::LineStyle()
{
	return LSurface::LineStyle();
}

uint LScreenDC::LineStyle(uint32_t Bits, uint32_t Reset)
{
	return LSurface::LineStyle(Bits);
}

int LScreenDC::GetFlags()
{
	return 0;
}

LRect LScreenDC::ClipRgn()
{
	return Clip;
}

LRect LScreenDC::ClipRgn(LRect *c)
{
	LRect Prev = Clip;

	if (c)
	{
		Clip = *c;

		// Don't add d->Client on here, as the translate makes it redundant.
		cairo_save(d->cr);
		cairo_new_path(d->cr);
		cairo_rectangle(d->cr, c->x1, c->y1, c->X(), c->Y());
		cairo_clip(d->cr);
	}
	else
	{
	    Clip.ZOff(-1, -1);
		cairo_restore(d->cr);
	}

	return Prev;
}

COLOUR LScreenDC::Colour()
{
	return d->Col.Get(GetBits());
}

COLOUR LScreenDC::Colour(COLOUR c, int Bits)
{
	auto b = Bits?Bits:GetBits();
	d->Col.Set(c, b);
	return Colour(d->Col).Get(b);
}

LColour LScreenDC::Colour(LColour c)
{
	LColour Prev = d->Col;
	d->Col = c;

	if (d->cr)
	{
		double r = (double)d->Col.r()/255.0;
		double g = (double)d->Col.g()/255.0;
		double b = (double)d->Col.b()/255.0;
		double a = (double)d->Col.a()/255.0;
		cairo_set_source_rgba(d->cr, r, g, b, a);
	}

	return Prev;
}

int LScreenDC::Op(int ROP, NativeInt Param)
{
	int Prev = Op();

	switch (ROP)
	{
		case GDC_SET:
		{
			//d->p.setRasterOp(XPainter::CopyROP);
			break;
		}
		case GDC_OR:
		{
			//d->p.setRasterOp(XPainter::OrROP);
			break;
		}
		case GDC_AND:
		{
			//d->p.setRasterOp(XPainter::AndROP);
			break;
		}
		case GDC_XOR:
		{
			//d->p.setRasterOp(XPainter::XorROP);
			break;
		}
	}

	return Prev;
}

int LScreenDC::Op()
{
	/*
	switch (d->p.rasterOp())
	{
		case XPainter::CopyROP:
		{
			return GDC_SET;
			break;
		}
		case XPainter::OrROP:
		{
			return GDC_OR;
			break;
		}
		case XPainter::AndROP:
		{
			return GDC_AND;
			break;
		}
		case XPainter::XorROP:
		{
			return GDC_XOR;
			break;
		}
	}
	*/

	return GDC_SET;
}

int LScreenDC::X()
{
	return d->Client.Valid() ? d->Client.X() : d->x;
}

int LScreenDC::Y()
{
	return d->Client.Valid() ? d->Client.Y() : d->y;
}

int LScreenDC::GetBits()
{
	return d->Bits;
}

LPalette *LScreenDC::Palette()
{
	return 0;
}

void LScreenDC::Palette(LPalette *pPal, bool bOwnIt)
{
}

void LScreenDC::Set(int x, int y)
{
	cairo_rectangle(d->cr, x, y, 1, 1);
	cairo_fill(d->cr);
}

COLOUR LScreenDC::Get(int x, int y)
{
	return 0;
}

void LScreenDC::HLine(int x1, int x2, int y)
{
	// cairo_rectangle is not reliable if the x1 and x2 are a 
	// long way off the edge of the screen.
	if (x1 < OriginX)
		x1 = OriginX;
	if (x2 >= d->x + OriginX)
		x2 = d->x + OriginX;
		
	cairo_rectangle(d->cr, x1, y, x2-x1+1, 1);
	cairo_fill(d->cr);
}

void LScreenDC::VLine(int x, int y1, int y2)
{
	// stupid cairo_rectangle
	if (y1 < OriginY)
		y1 = OriginY;
	if (y2 >= d->y + OriginY)
		y2 = d->y + OriginY;

	cairo_rectangle(d->cr, x, y1, 1, y2-y1+1);
	cairo_fill(d->cr);
}

void LScreenDC::Line(int x1, int y1, int x2, int y2)
{
	cairo_move_to(d->cr, 0.5+x1, 0.5+y1);
	cairo_line_to (d->cr, 0.5+x2, 0.5+y2);
	cairo_set_line_width(d->cr, 1.0);
	cairo_set_line_cap(d->cr, CAIRO_LINE_CAP_SQUARE);
	cairo_stroke(d->cr);
	cairo_fill(d->cr);
}

void LScreenDC::Circle(float cx, float cy, float radius)
{
	cairo_arc(d->cr, cx, cy, radius, 0, 2 * LGI_PI);
	cairo_stroke(d->cr);
}

void LScreenDC::FilledCircle(float cx, float cy, float radius)
{
	cairo_arc(d->cr, cx, cy, radius, 0, 2 * LGI_PI);
	cairo_fill(d->cr);
}

void LScreenDC::Arc(float cx, float cy, float radius, float start, float end)
{
	cairo_arc(d->cr, cx, cy, radius, start, end);
	cairo_stroke(d->cr);
}

void LScreenDC::FilledArc(float cx, float cy, float radius, float start, float end)
{
	cairo_arc(d->cr, cx, cy, radius, start, end);
	cairo_fill(d->cr);
}

void LScreenDC::Ellipse(float cx, float cy, float x, float y)
{
	cairo_save(d->cr);
	cairo_translate(d->cr, cx, cy);
	cairo_scale(d->cr, x, y);
	cairo_arc(d->cr, 0.0, 0.0, 1.0, 0.0, 2 * LGI_PI);
	// cairo_stroke(d->cr);
	cairo_restore(d->cr);
}

void LScreenDC::FilledEllipse(float cx, float cy, float x, float y)
{
	cairo_save(d->cr);
	cairo_translate(d->cr, cx, cy);
	cairo_scale(d->cr, x, y);
	cairo_arc(d->cr, 0.0, 0.0, 1.0, 0.0, 2 * LGI_PI);
	cairo_fill(d->cr);
	cairo_restore(d->cr);
}

void LScreenDC::Box(int x1, int y1, int x2, int y2)
{
	double w = x2 - x1 + 1;
	double h = y2 - y1 + 1;
	cairo_rectangle(d->cr, x1, y1, w, h);
	cairo_rectangle(d->cr, x1+1, y1+1, w-2, h-2);
	cairo_set_fill_rule(d->cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill(d->cr);
}

void LScreenDC::Box(LRect *a)
{
	if (a)
		Box(a->x1, a->y1, a->x2, a->y2);
	else
		Box(0, 0, X()-1, Y()-1);
}

void LScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	if (x2 >= x1 &&
		y2 >= y1)
	{
		cairo_rectangle (d->cr, x1, y1, x2-x1+1, y2-y1+1);
		cairo_fill(d->cr);
	}
}

void LScreenDC::Rectangle(LRect *a)
{
	if (a)
	{
		if (a->X() > 0 &&
			a->Y() > 0)
		{
			cairo_rectangle (d->cr, a->x1, a->y1, a->X(), a->Y());
			cairo_fill(d->cr);
		}
	}
	else
	{
		cairo_rectangle(d->cr, 0, 0, X(), Y());
		cairo_fill(d->cr);
	}
}

void LScreenDC::Polygon(int Points, LPoint *Data)
{
	if (!Data || Points <= 0)
		return;

	cairo_move_to(d->cr, Data[0].x, Data[0].y);
	for (int i=1; i<Points; i++)
		cairo_line_to(d->cr, Data[i].x, Data[i].y);
	cairo_close_path(d->cr);
	cairo_fill(d->cr);
}

void LScreenDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
	if (!Src)
	{
		LgiTrace("%s:%i - No source.\n", _FL);
		return;
	}
	
	if (Src->IsScreen())
	{
		LgiTrace("%s:%i - Can't do screen->screen blt.\n", _FL);
		return;
	}
		
	// memory -> screen blt
	LRect RealClient = d->Client;
	d->Client.ZOff(-1, -1); // Clear this so the blit rgn calculation uses the
							// full context size rather than just the client.
	LBlitRegions br(this, x, y, Src, a);
	d->Client = RealClient;
	if (!br.Valid())
	{
		// LgiTrace("Blt inval pos=%i,%i a=%s bounds=%s\n", x, y, a?a->GetStr():"null", Bounds().GetStr());
		return;
	}

	LMemDC *Mem;
	if ((Mem = dynamic_cast<LMemDC*>(Src)))
	{
		auto Sub = Mem->GetSubImage(br.SrcClip);
		if (Sub)
		{
			if (auto Pat = cairo_pattern_create_for_surface(Sub))
			{
				/*
				{
					Gtk::cairo_matrix_t matrix;
					cairo_get_matrix(d->cr, &matrix);
					double ex[4];
					cairo_clip_extents(d->cr, ex+0, ex+1, ex+2, ex+3);
					LgiTrace("LScreenDC::Blt xy=%i,%i clip=%g,%g+%g,%g cli=%s off=%i,%i\n",
						x, y,
						ex[0]+matrix.x0, ex[1]+ex[0],
						ex[2]+matrix.x0, ex[3]+ex[2],
						d->Client.GetStr(),
						OriginX, OriginY);
				}
				*/
				
				cairo_save(d->cr);
				cairo_translate(d->cr, br.DstClip.x1, br.DstClip.y1);
				cairo_set_source(d->cr, Pat);
		
				cairo_new_path(d->cr);
				cairo_rectangle(d->cr, 0, 0, br.DstClip.X(), br.DstClip.Y());
				if (d->ConstAlpha < 255)
				{
					cairo_clip(d->cr);
					auto alpha = MAX(0, d->ConstAlpha) / 255.0;
					cairo_paint_with_alpha(d->cr, alpha);
				}
				else
				{
					cairo_fill(d->cr);
				}
		
				cairo_restore(d->cr);
					
				cairo_pattern_destroy(Pat);
			}
		}
	}
}

void LScreenDC::StretchBlt(LRect *d, LSurface *Src, LRect *s)
{
	LAssert(0);
}

void LScreenDC::Bezier(int Threshold, LPoint *Pt)
{
	LAssert(0);
}

