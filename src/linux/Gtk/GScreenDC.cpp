/*hdr
**	FILE:			GScreenDC.cpp
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
	GRect Client;

	GView *View;
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
GScreenDC::GScreenDC()
{
	d = new GScreenPrivate;
	d->x = GdcD->X();
	d->y = GdcD->Y();
}

GScreenDC::GScreenDC(int x, int y, int bits)
{
	d = new GScreenPrivate;
	d->x = x;
	d->y = y;
	d->Bits = bits;
}

GScreenDC::GScreenDC(Gtk::cairo_t *cr, int x, int y)
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

GScreenDC::GScreenDC(OsDrawable *Drawable)
{
	d = new GScreenPrivate;
	d->Own = false;
	d->d = Drawable;

	#if GTK_MAJOR_VERSION == 3
	#else
	if (d->gc = gdk_gc_new(Drawable))
	{
	    GdkScreen *s = gdk_gc_get_screen(d->gc);
	    if (s)
	    {
	        GdkVisual *v = gdk_screen_get_system_visual(s);
	        if (v)
	        {
	            d->Bits = v->depth;
		        ColourSpace = GdkVisualToColourSpace(v, v->depth);
	        }
	    }
	}
	#endif
}

GScreenDC::GScreenDC(GView *view, void *param)
{
	d = new GScreenPrivate;
	d->View = view;
	if (view)
	{
		GWindow *w = view->GetWindow();
		OsView v = w? GTK_WIDGET(w->WindowHandle()) : NULL;
		if (v)
		{
			d->v = v;

			#if GTK_MAJOR_VERSION == 3
			LgiAssert(!"Gtk3 FIXME");
			#else
			d->d = v->window;	
			d->x = v->allocation.width;
			d->y = v->allocation.height;	
			if (d->gc = gdk_gc_new(v->window))
			{
			    GdkScreen *s = gdk_gc_get_screen(d->gc);
			    if (s)
			    {
			        GdkVisual *v = gdk_screen_get_system_visual(s);
			        if (v)
			        {
			            d->Bits = v->depth;
				        ColourSpace = GdkVisualToColourSpace(v, v->depth);
			        }
			    }
			}
			#endif
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

GScreenDC::~GScreenDC()
{
	DeleteObj(d);
}

OsPainter GScreenDC::Handle()
{
	#if GTK_MAJOR_VERSION == 3
	return d->cr;
	#else
	if (!Cairo)
	{
		Cairo = gdk_cairo_create(d->d);
		if (Cairo)
		{
			// cairo_reset_clip(Cairo);

			double x1, y1, x2, y2;
			cairo_clip_extents (Cairo, &x1, &y1, &x2, &y2);
			
			#ifdef _DEBUG
			int x = (int) (x2 - x1);
			int y = (int) (y2 - y1);
			
			if (d->View &&
				d->View->_Debug)
			{
				int width, height;

				gdk_drawable_get_size (d->d, &width, &height);
				
				printf("%s:%i %s %g,%g,%g,%g %i,%i  %i,%i  %i,%i\n",
					_FL,
					d->View ? d->View->GetClass() : NULL,
					x1,y1,x2,y2,
					x,y,
					d->x, d->y,
					width, height);
			}
			#endif
		}
	}
	
	return Cairo;
	#endif
}

bool GScreenDC::SupportsAlphaCompositing()
{
	// GTK/X11 doesn't seem to support alpha compositing.
	return false;
}

GdcPt2 GScreenDC::GetSize()
{
	return GdcPt2(d->x, d->y);
}

bool GScreenDC::GetClient(GRect *c)
{
	if (!c)
		return false;

	*c = d->Client;
	return true;
}

void GScreenDC::GetOrigin(int &x, int &y)
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

void GScreenDC::SetOrigin(int x, int y)
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

void GScreenDC::SetClient(GRect *c)
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

GRect *GScreenDC::GetClient()
{
	return &d->Client;
}

uint GScreenDC::LineStyle()
{
	return GSurface::LineStyle();
}

uint GScreenDC::LineStyle(uint32_t Bits, uint32_t Reset)
{
	return GSurface::LineStyle(Bits);
}

int GScreenDC::GetFlags()
{
	return 0;
}

GRect GScreenDC::ClipRgn()
{
	return Clip;
}

GRect GScreenDC::ClipRgn(GRect *c)
{
	GRect Prev = Clip;

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

COLOUR GScreenDC::Colour()
{
	return d->Col.Get(GetBits());
}

COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	auto b = Bits?Bits:GetBits();
	d->Col.Set(c, b);
	return Colour(d->Col).Get(b);
}

GColour GScreenDC::Colour(GColour c)
{
	GColour Prev = d->Col;
	d->Col = c;

	#if GTK_MAJOR_VERSION == 3
	if (d->cr)
	{
		cairo_set_source_rgb(d->cr,
							(double)d->Col.r()/255.0,
							(double)d->Col.g()/255.0,
							(double)d->Col.b()/255.0);
	}
	#else
	if (d->gc)
	{
		GdkColor col;
		
		col.pixel = 0;
		
		col.red = c.r();
		col.red |= col.red << 8;
		
		col.green = c.g();
		col.green |= col.green << 8;

		col.blue = c.b();
		col.blue |= col.blue << 8;
		
		// printf("Setting Col %x, %x, %x\n", col.red, col.green, col.blue);
		
		gdk_gc_set_rgb_fg_color(d->gc, &col);
		gdk_gc_set_rgb_bg_color(d->gc, &col);
	}
	#endif

	return Prev;
}

int GScreenDC::Op(int ROP, NativeInt Param)
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

int GScreenDC::Op()
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

int GScreenDC::X()
{
	return d->Client.Valid() ? d->Client.X() : d->x;
}

int GScreenDC::Y()
{
	return d->Client.Valid() ? d->Client.Y() : d->y;
}

int GScreenDC::GetBits()
{
	return d->Bits;
}

GPalette *GScreenDC::Palette()
{
	return 0;
}

void GScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
}

void GScreenDC::Set(int x, int y)
{
	#if GTK_MAJOR_VERSION == 3
	cairo_rectangle(d->cr, x, y, 1, 1);
	cairo_fill(d->cr);
	#else
	gdk_draw_point(d->d, d->gc, x-OriginX, y-OriginY);
	#endif
}

COLOUR GScreenDC::Get(int x, int y)
{
	return 0;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	#if GTK_MAJOR_VERSION == 3
	cairo_rectangle(d->cr, x1, y, x2-x1+1, 1);
	cairo_fill(d->cr);
	#else
	gdk_draw_line(d->d, d->gc, x1-OriginX, y-OriginY, x2-OriginX, y-OriginY);
	#endif
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	#if GTK_MAJOR_VERSION == 3
	cairo_rectangle(d->cr, x, y1, 1, y2-y1+1);
	cairo_fill(d->cr);
	#else
	gdk_draw_line(d->d, d->gc, x-OriginX, y1-OriginY, x-OriginX, y2-OriginY);
	#endif
}

void GScreenDC::Line(int x1, int y1, int x2, int y2)
{
	#if GTK_MAJOR_VERSION == 3
	cairo_move_to(d->cr, 0.5+x1, 0.5+y1);
	cairo_line_to (d->cr, 0.5+x2, 0.5+y2);
	cairo_set_line_width(d->cr, 1.0);
	cairo_set_line_cap(d->cr, CAIRO_LINE_CAP_SQUARE);
	cairo_stroke(d->cr);
	cairo_fill(d->cr);
	#else
	gdk_draw_line(d->d, d->gc, x1-OriginX, y1-OriginY, x2-OriginX, y2-OriginY);
	#endif
}

void GScreenDC::Circle(double cx, double cy, double radius)
{
	cairo_arc(d->cr, cx, cy, radius, 0, 2 * M_PI);
	cairo_stroke(d->cr);
}

void GScreenDC::FilledCircle(double cx, double cy, double radius)
{
	cairo_arc(d->cr, cx, cy, radius, 0, 2 * M_PI);
	cairo_fill(d->cr);
}

void GScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	cairo_arc(d->cr, cx, cy, radius, start, end);
	cairo_stroke(d->cr);
}

void GScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	cairo_arc(d->cr, cx, cy, radius, start, end);
	cairo_fill(d->cr);
}

void GScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	#if GTK_MAJOR_VERSION == 3
	LgiAssert(!"Gtk3 FIXME");
	#else
	gdk_draw_arc(d->d, d->gc, false,
			 	cx - (x / 2), cy - (y / 2),
			 	x,
			 	y,
			 	0,
			 	360 * 64);
	#endif
}

void GScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	#if GTK_MAJOR_VERSION == 3
	LgiAssert(!"Gtk3 FIXME");
	#else
	gdk_draw_arc(d->d, d->gc, true,
			 	cx - (x / 2), cy - (y / 2),
			 	x,
			 	y,
			 	0,
			 	360 * 64);
	#endif
}

void GScreenDC::Box(int x1, int y1, int x2, int y2)
{
	#if GTK_MAJOR_VERSION == 3
	double w = x2 - x1 + 1;
	double h = y2 - y1 + 1;
	cairo_rectangle(d->cr, x1, y1, w, h);
	cairo_rectangle(d->cr, x1+1, y1+1, w-2, h-2);
	cairo_set_fill_rule(d->cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill(d->cr);
	#else
	gdk_draw_rectangle(d->d, d->gc, false, x1-OriginX, y1-OriginY, x2-x1, y2-y1);
	#endif
}

void GScreenDC::Box(GRect *a)
{
	if (a)
	{
		Box(a->x1, a->y1, a->x2, a->y2);
	}
	else
	{
		Box(0, 0, X()-1, Y()-1);
	}
}

void GScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	if (x2 >= x1 &&
		y2 >= y1)
	{
		#if GTK_MAJOR_VERSION == 3
		cairo_rectangle (d->cr, x1, y1, x2-x1+1, y2-y1+1);
		cairo_fill(d->cr);
		#else
		gdk_draw_rectangle(d->d, d->gc, true, x1-OriginX, y1-OriginY, x2-x1+1, y2-y1+1);
		#endif
	}
}

void GScreenDC::Rectangle(GRect *a)
{
	if (a)
	{
		if (a->X() > 0 &&
			a->Y() > 0)
		{
			#if GTK_MAJOR_VERSION == 3
			cairo_rectangle (d->cr, a->x1, a->y1, a->X(), a->Y());
			cairo_fill(d->cr);
			#else
			gdk_draw_rectangle(d->d, d->gc, true, a->x1-OriginX, a->y1-OriginY, a->X(), a->Y());
			#endif
		}
	}
	else
	{
		#if GTK_MAJOR_VERSION == 3
		cairo_rectangle(d->cr, 0, 0, X(), Y());
		cairo_fill(d->cr);
		#else
		gdk_draw_rectangle(d->d, d->gc, true, -OriginX, -OriginY, X(), Y());
		#endif
	}
}

void GScreenDC::Polygon(int Points, GdcPt2 *Data)
{
	if (Data)
	{
		::GArray<GdkPoint> pt;
		for (int p=0; p<Points; p++)
		{
			GdkPoint &out = pt.New();
			out.x = Data[p].x;
			out.y = Data[p].y;
		}
		
		#if GTK_MAJOR_VERSION == 3
		LgiAssert(!"Gtk3 FIXME");
		#else
		gdk_draw_polygon(d->d,
						 d->gc,
						 true,
						 &pt.First(),
						 pt.Length());
		#endif
	}
}

void GScreenDC::Blt(int x, int y, GSurface *Src, GRect *a)
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
	GRect RealClient = d->Client;
	d->Client.ZOff(-1, -1); // Clear this so the blit rgn calculation uses the
							// full context size rather than just the client.
	GBlitRegions br(this, x, y, Src, a);
	d->Client = RealClient;
	if (!br.Valid())
	{
		// LgiTrace("Blt inval pos=%i,%i a=%s bounds=%s\n", x, y, a?a->GetStr():"null", Bounds().GetStr());
		return;
	}

	GMemDC *Mem;
	if ((Mem = dynamic_cast<GMemDC*>(Src)))
	{
		cairo_surface_t *Sub = Mem->GetSurface(br.SrcClip);
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
					LgiTrace("GScreenDC::Blt xy=%i,%i clip=%g,%g+%g,%g cli=%s off=%i,%i\n",
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
			cairo_surface_destroy(Sub);				
		}
	}
}

void GScreenDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
	LgiAssert(0);
}

void GScreenDC::Bezier(int Threshold, GdcPt2 *Pt)
{
	LgiAssert(0);
}

void GScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, GRect *Bounds)
{
	LgiAssert(0);
}

