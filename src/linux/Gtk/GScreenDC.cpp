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

#include "Lgi.h"
using namespace Gtk;

class GScreenPrivate
{
public:
	int x, y, Bits;
	bool Own;
	GColour Col;
	LRect Client;

	LView *View;
	OsView v;
	OsDrawable *d;
	cairo_t *cr;
	cairo_matrix_t matrix;

	GScreenPrivate()
	{
		View = NULL;
		x = y = Bits = 0;
		Own = false;
		v = 0;
		d = NULL;
		cr = NULL;
		Client.ZOff(-1, -1);
	}
	
	~GScreenPrivate()
	{
	}
};

// Translates are cumulative... so we undo the last one before resetting it.
/////////////////////////////////////////////////////////////////////////////////////////////////////
LScreenDC::LScreenDC()
{
	d = new GScreenPrivate;
	d->x = GdcD->X();
	d->y = GdcD->Y();
}

LScreenDC::LScreenDC(int x, int y, int bits)
{
	d = new GScreenPrivate;
	d->x = x;
	d->y = y;
	d->Bits = bits;
}

LScreenDC::LScreenDC(Gtk::cairo_t *cr, int x, int y)
{
	d = new GScreenPrivate;
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
	d = new GScreenPrivate;
	d->Own = false;
	d->d = Drawable;
}

LScreenDC::LScreenDC(LView *view, void *param)
{
	d = new GScreenPrivate;
	d->View = view;
	if (view)
	{
		LWindow *w = view->GetWindow();
		OsView v = w? GTK_WIDGET(w->WindowHandle()) : NULL;
		if (v)
		{
			d->v = v;
			LgiAssert(!"Gtk3 FIXME");
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

OsPainter LScreenDC::Handle()
{
	return d->cr;
}

::GString LScreenDC::Dump()
{
	::GString s;
	s.Printf("LScreenDC size=%i,%i\n", d->x, d->y);
	return s;
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

void LScreenDC::GetOrigin(int &x, int &y)
{
	if (d->Client.Valid())
	{
		x = OriginX + d->Client.x1;
		y = OriginY + d->Client.y1;
	}
	else
	{
		x = OriginX;
		y = OriginY;
	}
}

void LScreenDC::SetOrigin(int x, int y)
{
	if (d->Client.Valid())
	{
		OriginX = x - d->Client.x1;
		OriginY = y - d->Client.y1;
	}
	else
	{
		OriginX = x;
		OriginY = y;
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

GColour LScreenDC::Colour(GColour c)
{
	GColour Prev = d->Col;
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

GPalette *LScreenDC::Palette()
{
	return 0;
}

void LScreenDC::Palette(GPalette *pPal, bool bOwnIt)
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
	cairo_rectangle(d->cr, x1, y, x2-x1+1, 1);
	cairo_fill(d->cr);
}

void LScreenDC::VLine(int x, int y1, int y2)
{
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

void LScreenDC::Circle(double cx, double cy, double radius)
{
	cairo_arc(d->cr, cx, cy, radius, 0, 2 * LGI_PI);
	cairo_stroke(d->cr);
}

void LScreenDC::FilledCircle(double cx, double cy, double radius)
{
	cairo_arc(d->cr, cx, cy, radius, 0, 2 * LGI_PI);
	cairo_fill(d->cr);
}

void LScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	cairo_arc(d->cr, cx, cy, radius, start, end);
	cairo_stroke(d->cr);
}

void LScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	cairo_arc(d->cr, cx, cy, radius, start, end);
	cairo_fill(d->cr);
}

void LScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	cairo_save(d->cr);
	cairo_translate(d->cr, cx, cy);
	cairo_scale(d->cr, x, y);
	cairo_arc(d->cr, 0.0, 0.0, 1.0, 0.0, 2 * LGI_PI);
	// cairo_stroke(d->cr);
	cairo_restore(d->cr);
}

void LScreenDC::FilledEllipse(double cx, double cy, double x, double y)
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
	GBlitRegions br(this, x, y, Src, a);
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
			cairo_pattern_t *Pat = cairo_pattern_create_for_surface(Sub);
			if (Pat)
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
				cairo_fill(d->cr);
		
				cairo_restore(d->cr);
					
				cairo_pattern_destroy(Pat);
			}
		}
	}
}

void LScreenDC::StretchBlt(LRect *d, LSurface *Src, LRect *s)
{
	LgiAssert(0);
}

void LScreenDC::Bezier(int Threshold, LPoint *Pt)
{
	LgiAssert(0);
}

void LScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, LRect *Bounds)
{
	LgiAssert(0);
}

