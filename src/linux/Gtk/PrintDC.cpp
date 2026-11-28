#include "lgi/common/Lgi.h"
#include "CairoVectorContext.h"
#include "PrintGeometry.h"
// #include <cups/ipp.h>
// #include <cups/cups.h>

#define PS_SCALE			10
#define DEFAULT_BITS		24

///////////////////////////////////////////////////////////////////////////////////////
class LPrintDCPrivate // : public GCups
{
public:
	class PrintPainter *p = nullptr;
	Gtk::GtkPrintContext *Handle = nullptr;
	LString PrintJobName;
	LString PrinterName;
	int Pages = 0;
	LColour c;
	LRect Clip;
	LRect PageBounds;
	Gtk::cairo_t *cr = nullptr;
	Gtk::cairo_matrix_t baseMatrix;
	bool fullPage = false;
	
	LPrintDCPrivate(Gtk::GtkPrintContext *handle)
	{
		Handle = handle;
	}
	
	~LPrintDCPrivate()
	{
	}
	
	bool IsOk()
	{
		#ifndef __clang__
		return	this != 0;
		#else
		return true;
		#endif
	}
};

/////////////////////////////////////////////////////////////////////////////////////
LPrintDC::LPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName)
{
	d = new LPrintDCPrivate((Gtk::GtkPrintContext*)Handle);
	d->PrintJobName = PrintJobName;
	d->PrinterName = PrinterName;
	ColourSpace = CsRgb24;
	SetHandle(gtk_print_context_get_cairo_context(d->Handle));
	d->PageBounds = Bounds();
	d->Clip = d->PageBounds;
}

LPrintDC::~LPrintDC()
{
	DeleteObj(d);
}

LSurfaceVector *LPrintDC::CreateVectorContext()
{
	return CreateGtkCairoVectorContext(d->cr);
}

Gtk::GtkPrintContext *LPrintDC::GetPrintContext()
{
	return d->Handle;
}

OsPainter LPrintDC::Handle()
{
	return d->cr;
}

void LPrintDC::SetHandle(OsPainter hnd)
{
	d->cr = hnd;
	if (d->cr)
	{
		cairo_get_matrix(d->cr, &d->baseMatrix);
		LSurface::SetOrigin(LPoint(0, 0));
	}
}

void LPrintDC::SetFullPage(bool fullPage)
{
	d->fullPage = fullPage;
	LRect printable;
	if (GetGtkPrintableRect(printable))
		d->PageBounds = d->Clip = printable;
}

void LPrintDC::SetDefaultClip(const LRect &rect)
{
	d->PageBounds = d->Clip = rect;
}

bool LPrintDC::GetGtkPrintableRect(LRect &rect)
{
	if (!d->Handle || X() <= 0 || Y() <= 0)
		return false;
	rect = Bounds();
	auto setup = gtk_print_context_get_page_setup(d->Handle);
	if (!setup)
		return false;

	double top, bottom, left, right;
	if (!gtk_print_context_get_hard_margins(d->Handle, &top, &bottom, &left, &right))
		return true;

	rect = LGtkPrintableRect(setup, gtk_print_context_get_width(d->Handle),
		gtk_print_context_get_height(d->Handle), top, bottom, left, right, d->fullPage);
	return rect.Valid();
}

LMemDC *LPrintDC::RenderPageToBitmap(LPoint dpi, const std::function<bool(LPrintDC*)> &paint)
{
	if (!d->Handle || X() <= 0 || Y() <= 0 || dpi.x <= 0 || dpi.y <= 0 || !paint)
	{
		LgiTrace("%s:%i - Invalid print page capture parameters.\n", _FL);
		return nullptr;
	}

	double width = ceil((double)X() * dpi.x / 72.0);
	double height = ceil((double)Y() * dpi.y / 72.0);
	// Cairo image surfaces have a maximum dimension of 32767 pixels.
	if (width < 1 || height < 1 || width > 32767 || height > 32767)
	{
		LgiTrace("%s:%i - Invalid print page bitmap dimensions: %.0f x %.0f.\n", _FL, width, height);
		return nullptr;
	}

	LAutoPtr<LMemDC> image(new LMemDC(_FL));
	if (!image || !image->Create((int)width, (int)height, System24BitColourSpace) ||
		!image->GetSurface() || !(*image)[0])
	{
		LgiTrace("%s:%i - Failed to allocate print page bitmap: %.0f x %.0f.\n", _FL, width, height);
		return nullptr;
	}
	auto surface = image->GetSurface();
	auto cr = image->Handle();
	if (!cr || cairo_surface_status(surface) != Gtk::CAIRO_STATUS_SUCCESS ||
		cairo_status(cr) != Gtk::CAIRO_STATUS_SUCCESS)
	{
		LgiTrace("%s:%i - Failed to create the print page Cairo target.\n", _FL);
		return nullptr;
	}

	image->Colour(LColour::White);
	image->Rectangle();
	cairo_surface_set_device_scale(surface, (double)dpi.x / 72.0, (double)dpi.y / 72.0);
	LPrintDC preview(d->Handle, d->PrintJobName, d->PrinterName);
	preview.SetFullPage(d->fullPage);
	preview.SetHandle(cr);
	bool success = paint(&preview);
	cairo_surface_flush(surface);
	if (!success)
	{
		LgiTrace("%s:%i - Print page bitmap paint failed.\n", _FL);
		return nullptr;
	}
	auto status = cairo_status(cr);
	if (status == Gtk::CAIRO_STATUS_SUCCESS)
		status = cairo_surface_status(surface);
	if (status != Gtk::CAIRO_STATUS_SUCCESS)
	{
		LgiTrace("%s:%i - Failed to render print page bitmap: %s.\n",
			_FL, cairo_status_to_string(status));
		return nullptr;
	}
	cairo_surface_set_device_scale(surface, 1, 1);
	return image.Release();
}

void LPrintDC::SetOrigin(LPoint pt)
{
	LSurface::SetOrigin(pt);

	if (d->cr)
	{
		auto m = d->baseMatrix;
		cairo_matrix_translate(&m, -pt.x, -pt.y);
		cairo_set_matrix(d->cr, &m);
	}
}

int LPrintDC::X()
{
	return gtk_print_context_get_width(d->Handle);
}

int LPrintDC::Y()
{
	return gtk_print_context_get_height(d->Handle);
}

int LPrintDC::GetBits()
{
	return DEFAULT_BITS;
}

LPoint LPrintDC::GetDpi()
{
	return LPoint(	Gtk::gtk_print_context_get_dpi_x(d->Handle),
					Gtk::gtk_print_context_get_dpi_y(d->Handle));
}

LRect LPrintDC::ClipRgn(LRect *Rgn)
{
	LRect Prev = d->Clip;
	if (Rgn)
		d->Clip = *Rgn;
	else
		d->Clip = d->PageBounds;
	return Prev;
}

LRect LPrintDC::ClipRgn()
{
	return d->Clip;
}

COLOUR LPrintDC::Colour()
{
	return d->c.c24();
}

COLOUR LPrintDC::Colour(COLOUR c, int Bits)
{
	LColour col(c, Bits ? Bits : DEFAULT_BITS);
	return Colour(col).c24();
}

LColour LPrintDC::Colour(LColour c)
{
	LColour Prev = d->c;
	d->c = c;
	if (d->cr)
		cairo_set_source_rgb(d->cr,
							(double)d->c.r() / 255.0,
							(double)d->c.g() / 255.0,
							(double)d->c.b() / 255.0);
	return Prev;
}

void LPrintDC::Set(int x, int y)
{
	if (d->cr)
	{
		cairo_new_path(d->cr);
		cairo_rectangle(d->cr, x, y, x+1, y+1);
		cairo_fill(d->cr);
	}
}

void LPrintDC::HLine(int x1, int x2, int y)
{
	Line(x1, y, x2, y);
}

void LPrintDC::VLine(int x, int y1, int y2)
{
	Line(x, y1, x, y2);
}

void LPrintDC::Line(int x1, int y1, int x2, int y2)
{
	if (d->cr)
	{
		cairo_set_line_width(d->cr, 0.5);
		cairo_new_path(d->cr);
		cairo_move_to(d->cr, x1, y1);
		cairo_line_to(d->cr, x2, y2);
		cairo_stroke(d->cr);
	}
}

void LPrintDC::Circle(float cx, float cy, float radius)
{
	if (d->cr)
	{
		cairo_arc(d->cr, cx, cy, radius, 0.0, 2.0 * LGI_PI);
		cairo_stroke(d->cr);
	}
}

void LPrintDC::FilledCircle(float cx, float cy, float radius)
{
	if (d->cr)
	{
		cairo_arc(d->cr, cx, cy, radius, 0.0, 2.0 * LGI_PI);
		cairo_fill(d->cr);
	}
}

void LPrintDC::Arc(float cx, float cy, float radius, float start, float end)
{
	if (d->cr)
	{
		cairo_arc(d->cr, cx, cy, radius, start, end);
		cairo_stroke(d->cr);
	}
}

void LPrintDC::FilledArc(float cx, float cy, float radius, float start, float end)
{
	if (d->cr)
	{
		cairo_arc(d->cr, cx, cy, radius, start, end);
		cairo_fill(d->cr);
	}
}

void LPrintDC::Ellipse(float cx, float cy, float x, float y)
{
	if (d->cr)
	{
		cairo_save(d->cr);
		cairo_translate(d->cr, cx, cy);
		cairo_scale(d->cr, x, y);
		cairo_arc(d->cr, 0.0, 0.0, 1.0, 0.0, 2.0 * LGI_PI);
		cairo_stroke(d->cr);
		cairo_restore(d->cr);
	}
}

void LPrintDC::FilledEllipse(float cx, float cy, float x, float y)
{
	if (d->cr)
	{
		cairo_save(d->cr);
		cairo_translate(d->cr, cx, cy);
		cairo_scale(d->cr, x, y);
		cairo_arc(d->cr, 0.0, 0.0, 1.0, 0.0, 2.0 * LGI_PI);
		cairo_fill(d->cr);
		cairo_restore(d->cr);
	}
}

void LPrintDC::Box(int x1, int y1, int x2, int y2)
{
	LRect r(x1, y1, x2, y2);
	Box(&r);
}

void LPrintDC::Box(LRect *a)
{
	LRect r;
	if (a)
		r = *a;
	else
		r = Bounds();
	if (d->cr)
	{
		double Half = 0.5;
		cairo_set_line_width(d->cr, Half);
		cairo_new_path(d->cr);
		cairo_rectangle(d->cr,
						Half + r.x1,
						Half + r.y1,
						-Half + r.X(),
						-Half + r.Y());
		cairo_stroke(d->cr);
	}
}

void LPrintDC::Rectangle(int x1, int y1, int x2, int y2)
{
	LRect r(x1, y1, x2, y2);
	Rectangle(&r);
}

void LPrintDC::Rectangle(LRect *a)
{
	LRect r;
	if (a)
		r = *a;
	else
		r = Bounds();
	if (d->cr)
	{
		cairo_new_path(d->cr);
		cairo_rectangle(d->cr, r.x1, r.y1, r.X(), r.Y());
		cairo_fill(d->cr);
	}
}

void LPrintDC::Blt(int x, int y, LSurface *Src, LRect *SrcClip)
{
	if (!Src)
	{
		LgiTrace("%s:%i - Missing print blit source.\n", _FL);
		return;
	}
	LRect s = SrcClip ? *SrcClip : Src->Bounds();
	LRect dest(x, y, x + s.X() - 1, y + s.Y() - 1);
	StretchBlt(&dest, Src, &s);
}

void LPrintDC::StretchBlt(LRect *rc, LSurface *Src, LRect *s)
{
	if (!d->cr || !rc || !Src || !rc->Valid())
	{
		LgiTrace("%s:%i - Invalid print blit parameters.\n", _FL);
		return;
	}

	LRect source = s ? *s : Src->Bounds();
	if (!source.Valid() || source.x1 < 0 || source.y1 < 0 ||
		source.x2 >= Src->X() || source.y2 >= Src->Y() || !(*Src)[0])
	{
		LgiTrace("%s:%i - Unreadable print image: %s, %ix%i, %s.\n",
			_FL, Src->GetClass(), Src->X(), Src->Y(), LColourSpaceToString(Src->GetColourSpace()));
		return;
	}

	LAutoPtr<LMemDC> converted;
	auto mem = dynamic_cast<LMemDC*>(Src);
	if (!mem || !mem->GetSurface() || Src->AlphaDC())
	{
		converted.Reset(new LMemDC(_FL));
		if (!converted || !converted->Create(source.X(), source.Y(), System32BitColourSpace) ||
			!(*converted)[0])
		{
			LgiTrace("%s:%i - Failed to allocate print image conversion.\n", _FL);
			return;
		}
		converted->Colour(0, 32);
		converted->Rectangle();
		converted->Op(GDC_ALPHA);
		converted->Blt(0, 0, Src, &source);
		mem = converted;
		source = mem->Bounds();
	}
	auto image = mem->GetSubImage(source);
	if (!image || Gtk::cairo_surface_status(image) != Gtk::CAIRO_STATUS_SUCCESS)
	{
		LgiTrace("%s:%i - Failed to create print image surface.\n", _FL);
		return;
	}

	// PDF output may retain the image after the source or conversion is freed.
	LCairoSurfaceT snapshot(Gtk::cairo_image_surface_create(
		Gtk::cairo_image_surface_get_format(image), source.X(), source.Y()));
	if (Gtk::cairo_surface_status(snapshot) != Gtk::CAIRO_STATUS_SUCCESS)
	{
		LgiTrace("%s:%i - Failed to allocate print image snapshot.\n", _FL);
		return;
	}
	auto copy = Gtk::cairo_create(snapshot);
	Gtk::cairo_set_source_surface(copy, image, 0, 0);
	Gtk::cairo_paint(copy);
	auto copyStatus = Gtk::cairo_status(copy);
	Gtk::cairo_destroy(copy);
	if (copyStatus != Gtk::CAIRO_STATUS_SUCCESS)
	{
		LgiTrace("%s:%i - Failed to copy print image: %s.\n", _FL, Gtk::cairo_status_to_string(copyStatus));
		return;
	}
	auto pattern = Gtk::cairo_pattern_create_for_surface(snapshot);
	auto status = cairo_pattern_status(pattern);
	if (status == Gtk::CAIRO_STATUS_SUCCESS)
	{
		Gtk::cairo_matrix_t matrix;
		double sx = (double)source.X() / rc->X();
		double sy = (double)source.Y() / rc->Y();
		cairo_matrix_init(&matrix, sx, 0, 0, sy, -rc->x1 * sx, -rc->y1 * sy);
		cairo_pattern_set_matrix(pattern, &matrix);
		cairo_pattern_set_extend(pattern, Gtk::CAIRO_EXTEND_PAD);

		cairo_save(d->cr);
		auto clip = ClipRgn();
		auto origin = GetOrigin();
		cairo_new_path(d->cr);
		cairo_rectangle(d->cr, clip.x1 + origin.x, clip.y1 + origin.y, clip.X(), clip.Y());
		cairo_clip(d->cr);
		cairo_set_source(d->cr, pattern);
		cairo_new_path(d->cr);
		cairo_rectangle(d->cr, rc->x1, rc->y1, rc->X(), rc->Y());
		cairo_fill(d->cr);
		cairo_restore(d->cr);
		status = cairo_status(d->cr);
	}
	if (status != Gtk::CAIRO_STATUS_SUCCESS)
		LgiTrace("%s:%i - Print image blit failed: %s.\n", _FL, Gtk::cairo_status_to_string(status));
	cairo_pattern_destroy(pattern);
}

void LPrintDC::Polygon(int Points, LPoint *Data)
{
	LAssert(!"Not impl.");
}

void LPrintDC::Bezier(int Threshold, LPoint *Pt)
{
	LAssert(!"Not impl.");
}

