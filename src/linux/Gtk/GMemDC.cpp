/*hdr
**	FILE:			GMemDC.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			14/10/2000
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GString.h"
using namespace Gtk;

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define ROUND_UP(bits) (((bits) + 7) / 8)

class GMemDCPrivate
{
public:
	GRect Client;
	cairo_t *cr;
	cairo_surface_t *Img;
	GColourSpace CreateCs;

    GMemDCPrivate()
    {
		Client.ZOff(-1, -1);
		cr = NULL;
		Img = NULL;
		CreateCs = CsNone;
    }

    ~GMemDCPrivate()
    {
    	Empty();
	}

	void Empty()
	{
		if (cr)
		{
			cairo_destroy(cr);
			cr = NULL;
		}
		if (Img)
		{
			cairo_surface_destroy(Img);
			Img = NULL;
		}
	}
};

GMemDC::GMemDC(int x, int y, GColourSpace cs, int flags)
{
	d = new GMemDCPrivate;
	if (cs != CsNone)
		Create(x, y, cs, flags);
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	
	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetColourSpace()))
	{
		Blt(0, 0, pDC);
	}
}

GMemDC::~GMemDC()
{
	Empty();
	DeleteObj(d);
}

cairo_surface_t *GMemDC::GetSurface(GRect &r)
{
	if (!d->Img)
		return NULL;
	
	#if GTK_MAJOR_VERSION == 3
	cairo_format_t fmt = cairo_image_surface_get_format(d->Img);
	#else
	cairo_format_t fmt = CAIRO_FORMAT_ARGB32;
	switch (d->Img->depth)
	{
		case 8: fmt = CAIRO_FORMAT_A8; break;
		case 16: fmt = CAIRO_FORMAT_RGB16_565; break;
		case 24: fmt = CAIRO_FORMAT_RGB24; break;
		case 32: fmt = CAIRO_FORMAT_ARGB32; break;
		default:
			printf("%s:%i - '%i' bit depth that cairo supports\n", _FL, d->Img->depth);
			return NULL;
	}
	#endif

	int pixel_bytes = GColourSpaceToBits(ColourSpace) >> 3;
	#if GTK_MAJOR_VERSION == 3
	return cairo_image_surface_create_for_data(	pMem->Base + (r.y1 * pMem->Line) + (r.x1 * pixel_bytes),
												fmt,
												r.X(),
												r.Y(),
												pMem->Line);
	#else
	return cairo_image_surface_create_for_data(	((uchar*)d->Img->mem) + (r.y1 * d->Img->bpl) + (r.x1 * pixel_bytes),
												fmt,
												r.X(),
												r.Y(),
												d->Img->bpl);
	#endif
}

OsPainter GMemDC::Handle()
{
	if (!d->cr)
		d->cr = cairo_create(d->Img);
	return d->cr;
}

void FreeMemDC(guchar *pixels, GMemDC *data)
{
	delete data;	
}

GdkPixbuf *GMemDC::CreatePixBuf()
{
	GMemDC *Tmp = new GMemDC(X(), Y(), CsRgba32, SurfaceRequireExactCs);
	if (!Tmp)
		return NULL;

	Tmp->Colour(0, 32);
	Tmp->Rectangle();
	Tmp->Op(GDC_ALPHA);
	Tmp->Blt(0, 0, this);

	GdkPixbuf *Pb =
		gdk_pixbuf_new_from_data ((*Tmp)[0],
								  GDK_COLORSPACE_RGB,
								  true,
								  8,
								  Tmp->X(),
								  Tmp->Y(),
								  Tmp->GetRowStep(),
								  (GdkPixbufDestroyNotify) FreeMemDC,
								  Tmp);
	if (!Pb)
		printf("%s:%i - gdk_pixbuf_new_from_data failed.\n", _FL);

	return Pb;
}

bool GMemDC::SupportsAlphaCompositing()
{
	// We can blend RGBA into memory buffers, mostly because the code is in Lgi not GTK.
	return true;
}

#if GTK_MAJOR_VERSION == 3
Gtk::cairo_surface_t *GMemDC::GetImage()
#else
GdkImage *GMemDC::GetImage()
#endif
{
    return d->Img;
}

GdcPt2 GMemDC::GetSize()
{
	return GdcPt2(pMem->x, pMem->y);
}

GRect GMemDC::ClipRgn(GRect *Rgn)
{
	GRect Old = Clip;
	
	if (Rgn)
	{
		GRect Dc(0, 0, X()-1, Y()-1);
		
		Clip = *Rgn;
		Clip.Offset(-OriginX, -OriginY);
		Clip.Bound(&Dc);
	}
	else
	{
		Clip.ZOff(X()-1, Y()-1);
	}
	
	return Old;
}

void GMemDC::SetClient(GRect *c)
{
	if (c)
	{
		d->Client = *c;
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		OriginX = 0;
		OriginY = 0;
	}
}

void GMemDC::Empty()
{
	d->Empty();
	DeleteObj(pMem);
}

bool GMemDC::Lock()
{
	return false;
}

bool GMemDC::Unlock()
{
	return false;
}

void GMemDC::GetOrigin(int &x, int &y)
{
	GSurface::GetOrigin(x, y);
}

void GMemDC::SetOrigin(int x, int y)
{
	Handle();
	GSurface::SetOrigin(x, y);

	if (d->cr)
	{		
		cairo_matrix_t m;
		cairo_get_matrix(d->cr, &m);
		m.x0 = -x;
		m.y0 = -y;
		cairo_set_matrix(d->cr, &m);
	}
}

GColourSpace GMemDC::GetCreateCs()
{
	// Sometimes the colour space we get is different to the requested colour space.
	// This function returns the original requested colour space.
	return d->CreateCs;
}

bool GMemDC::Create(int x, int y, GColourSpace Cs, int Flags)
{
	int Bits = GColourSpaceToBits(Cs);
	if (x < 1 || y < 1 || Bits < 1)
		return false;
	if (Bits < 8)
		Bits = 8;

	Empty();

	bool Exact = (Flags & SurfaceRequireExactCs) != 0;
	d->CreateCs = Cs;
	
	cairo_format_t fmt = CAIRO_FORMAT_RGB24;
	switch (Bits)
	{
		case 8:
			fmt = CAIRO_FORMAT_A8;
			ColourSpace = CsIndex8;
			break;
		case 16:
			fmt = CAIRO_FORMAT_RGB16_565;
			ColourSpace = CsRgb16;
			break;
		case 24:
			fmt = CAIRO_FORMAT_RGB24;
			ColourSpace = CsBgrx32;
			break;
		case 32:
			fmt = CAIRO_FORMAT_ARGB32;
			ColourSpace = CsBgra32;
			break;
		default:
			ColourSpace = CsNone;
			return false;
	}

	if (!pMem)
		pMem = new GBmpMem;
	if (!pMem)
		return false;

	pMem->x = x;
	pMem->y = y;
	pMem->Flags = 0;
	pMem->Cs = CsNone;

	if (d->CreateCs != ColourSpace  &&
		Exact)
	{
		// Don't bother creating a cairo imaage surface, as the colour space we want is not
		// supported. Just create an in memory bitmap.
		pMem->OwnMem(true);
		pMem->Line = ((pMem->x * Bits + 31) / 32) * 4;
		LgiAssert(pMem->Line > 0);
		pMem->Base = new uchar[pMem->Line * pMem->y];
		pMem->Cs = ColourSpace = Cs;
	}
	else
	{
		LgiAssert(d->Img == NULL);
		d->Img = cairo_image_surface_create (fmt, x, y);
		if (!d->Img)
			return false;

		switch (cairo_image_surface_get_format(d->Img))
		{
			case CAIRO_FORMAT_ARGB32:
				pMem->Cs = CsBgra32;
				break;
			case CAIRO_FORMAT_RGB24:
				pMem->Cs = CsBgrx32;
				break;
			case CAIRO_FORMAT_A8:
				pMem->Cs = CsIndex8;
				break;
			case CAIRO_FORMAT_RGB16_565:
				pMem->Cs = CsRgb16;
				break;
			default:
				LgiAssert(0);
				return false;
		}			
		pMem->Base = cairo_image_surface_get_data(d->Img);
		pMem->Line = cairo_image_surface_get_stride(d->Img);
	}

	int NewOp = (pApp) ? Op() : GDC_SET;

	if ( (Flags & GDC_OWN_APPLICATOR) &&
		!(Flags & GDC_CACHED_APPLICATOR))
	{
		DeleteObj(pApp);
	}

	for (int i=0; i<GDC_CACHE_SIZE; i++)
	{
		DeleteObj(pAppCache[i]);
	}

	if (NewOp < GDC_CACHE_SIZE && !DrawOnAlpha())
	{
		pApp = (pAppCache[NewOp]) ? pAppCache[NewOp] : pAppCache[NewOp] = CreateApplicator(NewOp);
		Flags &= ~GDC_OWN_APPLICATOR;
		Flags |= GDC_CACHED_APPLICATOR;
	}
	else
	{
		pApp = CreateApplicator(NewOp, pMem->Cs);
		Flags &= ~GDC_CACHED_APPLICATOR;
		Flags |= GDC_OWN_APPLICATOR;
	}

	if (!pApp)
	{
		printf("GMemDC::Create(%i,%i,%i) No Applicator.\n", x, y, Bits);
		LgiAssert(0);
	}

	Clip.ZOff(X()-1, Y()-1);

	return true;
}

void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
		return;

	bool Status = false;
	GBlitRegions br(this, x, y, Src, a);
	if (!br.Valid())
		return;

	GScreenDC *Screen;
	if ((Screen = Src->IsScreen()))
	{
		if (pMem->Base)
		{
			// Screen -> Memory
			GdkWindow *root_window = gdk_get_default_root_window();
			if (root_window)
			{
				gint x_orig, y_orig;
				gint width, height;

				#if GTK_MAJOR_VERSION == 3
				LgiAssert(!"Gtk3 FIXME");
				#else
				gdk_drawable_get_size(root_window, &width, &height);      
				gdk_window_get_origin(root_window, &x_orig, &y_orig);
				gdk_drawable_copy_to_image(	root_window,
											d->Img,
											br.SrcClip.x1,
											br.SrcClip.y1,
											x,
											y,
											br.SrcClip.X(),
											br.SrcClip.Y());
				#endif
				
				// Call the capture screen handler to draw anything between the screen and
				// cursor layers
				OnCaptureScreen();
				
				if (TestFlag(Flags, GDC_CAPTURE_CURSOR))
				{
					// Capture the cursor as well...
				}
				
				Status = true;
			}
		}
		
		if (!Status)
		{
			Colour(Rgb24(255, 0, 255), 24);
			Rectangle();
		}
	}
	else if ((*Src)[0])
	{
		// Memory -> Memory (Source alpha used)
		GSurface::Blt(x, y, Src, a);
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
    LgiAssert(!"Not implemented");
}

void GMemDC::HorzLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LgiSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (x1 <= x2 &&
		y >= Clip.y1 &&
		y <= Clip.y2 &&
		pApp)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x1, y);
		for (; x1 <= x2; x1++)
		{
			if (x1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncX();
		}

		pApp->c = Prev;
	}
}

void GMemDC::VertLine(int x, int y1, int y2, COLOUR a, COLOUR b)
{
	if (y1 > y2) LgiSwap(y1, y2);
	
	if (y1 < Clip.y1) y1 = Clip.y1;
	if (y2 > Clip.y2) y2 = Clip.y2;
	if (y1 <= y2 &&
		x >= Clip.x1 &&
		x <= Clip.x2 &&
		pApp)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x, y1);
		for (; y1 <= y2; y1++)
		{
			if (y1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncY();
		}

		pApp->c = Prev;
	}
}
