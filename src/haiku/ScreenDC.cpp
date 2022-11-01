/*hdr
**	FILE:			LScreenDC.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			29/11/2021
**	DESCRIPTION:	Haiku screen DC
**
**	Copyright (C) 2021, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "lgi/common/Lgi.h"
#include <Bitmap.h>

#define VIEW_CHECK(...)		if (!d->v) return __VA_ARGS__;

class LScreenPrivate
{
public:
	int x = 0, y = 0, Bits = 32;
	bool Own = false;
	LColour Col;
	LRect Client;

	LView *View = NULL;
	OsView v = NULL;

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
	d->Bits = GdcD->GetBits();
}

LScreenDC::LScreenDC(LView *view, void *param)
{
	d = new LScreenPrivate;
	if (d->View = view)
		d->v = view->Handle();

	d->Bits = GdcD->GetBits();

	if (d->v)
		d->Client = d->v->Frame();
	else
		LgiTrace("%s:%i - LScreenDC::LScreenDC - No view?\n", _FL);
}

LScreenDC::~LScreenDC()
{
	DeleteObj(d);
}

OsPainter LScreenDC::Handle()
{
	return d->v;
}

::LString LScreenDC::Dump()
{
	::LString s;
	s.Printf("LScreenDC size=%i,%i\n", d->x, d->y);
	return s;
}

bool LScreenDC::SupportsAlphaCompositing()
{
	// GTK/X11 doesn't seem to support alpha compositing.
	return false;
}

LPoint LScreenDC::GetDpi()
{
	return LScreenDpi();
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
	VIEW_CHECK()

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

	// printf("setori %i,%i\n", OriginX, OriginY);
	d->v->SetOrigin(OriginX, OriginX);
}

void LScreenDC::SetClient(LRect *c)
{
	VIEW_CHECK()

	if (c)
	{
		d->Client = *c;

		OriginX = c->x1;
		OriginY = c->y1;

		LRect clp(0, 0, c->X()-2, c->Y()-2);
		// printf("clp=%s ori=%i,%i\n", clp.GetStr(), OriginX, OriginY);
		d->v->SetOrigin(OriginX, OriginY);
		d->v->ClipToRect(clp);
	}
	else
	{
		if (Clip.Valid())
			ClipRgn(NULL);
		
		d->v->SetOrigin(OriginX = 0, OriginX = 0);
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
		d->v->ClipToRect(Clip);
	}
	else
	{
	    Clip.ZOff(-1, -1);
	    d->v->ConstrainClippingRegion(NULL);
	}

	return Prev;
}

COLOUR LScreenDC::Colour()
{
	return d->Col.Get(GetBits());
}

COLOUR LScreenDC::Colour(COLOUR c, int Bits)
{
	auto b = Bits ? Bits : GetBits();
	d->Col.Set(c, b);
	return Colour(d->Col).Get(b);
}

LColour LScreenDC::Colour(LColour c)
{
	LColour Prev = d->Col;
	
	d->Col = c;
	if (d->v)
	{
		d->v->SetLowColor(c);
		d->v->SetHighColor(c);
	}
	else LgiTrace("%s:%i - No view.\n", _FL);

	return Prev;
}

int LScreenDC::Op(int ROP, NativeInt Param)
{
	return GDC_SET; // not supported..
}

int LScreenDC::Op()
{
	return GDC_SET; // not supported..
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
	return NULL; // not supported.
}

void LScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
}

void LScreenDC::Set(int x, int y)
{
	VIEW_CHECK()
	d->v->StrokeLine(BPoint(x, y), BPoint(x, y));
}

COLOUR LScreenDC::Get(int x, int y)
{
	return 0;
}

void LScreenDC::HLine(int x1, int x2, int y)
{
	VIEW_CHECK()
	d->v->StrokeLine(BPoint(x1, y), BPoint(x2, y));
}

void LScreenDC::VLine(int x, int y1, int y2)
{
	VIEW_CHECK()
	d->v->StrokeLine(BPoint(x, y1), BPoint(x, y2));
}

void LScreenDC::Line(int x1, int y1, int x2, int y2)
{
	VIEW_CHECK()
	d->v->StrokeLine(BPoint(x1, y1), BPoint(x2, y2));
}

void LScreenDC::Circle(double cx, double cy, double radius)
{
	VIEW_CHECK()
	d->v->StrokeArc(BPoint(cx, cy), radius, radius, 0, 360);
}

void LScreenDC::FilledCircle(double cx, double cy, double radius)
{
	VIEW_CHECK()
	d->v->FillArc(BPoint(cx, cy), radius, radius, 0, 360);
}

void LScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	VIEW_CHECK()
	d->v->StrokeArc(BPoint(cx, cy), radius, radius, start, end);
}

void LScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	VIEW_CHECK()
	d->v->FillArc(BPoint(cx, cy), radius, radius, start, end);
}

void LScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	VIEW_CHECK()
	d->v->StrokeArc(BPoint(cx, cy), x, y, 0, 360);
}

void LScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	VIEW_CHECK()
	d->v->FillArc(BPoint(cx, cy), x, y, 0, 360);
}

void LScreenDC::Box(int x1, int y1, int x2, int y2)
{
	VIEW_CHECK()
	d->v->StrokeRect(LRect(x1, y1, x2, y2));
}

void LScreenDC::Box(LRect *a)
{
	VIEW_CHECK()
	if (a)
		d->v->StrokeRect(*a);
	else
		Box(0, 0, X()-1, Y()-1);
}

void LScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	VIEW_CHECK()
	if (x2 >= x1 && y2 >= y1)
	{
		// auto o = d->v->Origin();
		// printf("Rect %i,%i,%i,%i %g,%g\n", x1, y1, x2, y2, o.x, o.y);
		d->v->FillRect(LRect(x1, y1, x2, y2));
	}
}

void LScreenDC::Rectangle(LRect *a)
{
	VIEW_CHECK()
	LRect r = a ? *a : Bounds();
	BRect br(r.x1, r.y1, r.x2, r.y2);
	d->v->FillRect(br);
}

void LScreenDC::Polygon(int Points, LPoint *Data)
{
	VIEW_CHECK()
	if (!Data || Points <= 0)
		return;
}

void LScreenDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
	VIEW_CHECK()
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

	BBitmap *bmp = Src->GetBitmap();
	if (!bmp)
	{
		LAssert(!"no bmp.");
		LgiTrace("%s:%i - No bitmap.\n", _FL);
		return;
	}
	
	d->v->SetDrawingMode(B_OP_ALPHA);
	d->v->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
		
	if (a)
	{
		BRect bitmapRect = *a;
		BRect viewRect(x, y, x + a->X() - 1, y + a->Y() - 1);
		d->v->DrawBitmap(bmp, bitmapRect, viewRect);
		
		#if 0
		printf("ScreenDC::Blt %g,%g/%gx%g -> %g,%g/%gx%g %i,%i\n",
			bitmapRect.left, bitmapRect.top, bitmapRect.Width(), bitmapRect.Height(),
			viewRect.left, viewRect.top, viewRect.Width(), viewRect.Height(),
			a->X(), a->Y());
		#endif
	}
	else
	{
		BRect bitmapRect = bmp->Bounds();
		BRect viewRect(x, y, x + Src->X() - 1, y + Src->Y() - 1);
		d->v->DrawBitmap(bmp, bitmapRect, viewRect);

		#if 0
		printf("ScreenDC::Blt %g,%g/%gx%g -> %g,%g/%gx%g %i,%i\n",
			bitmapRect.left, bitmapRect.top, bitmapRect.Width(), bitmapRect.Height(),
			viewRect.left, viewRect.top, viewRect.Width(), viewRect.Height(),
			x, y);
		#endif
	}

	d->v->SetDrawingMode(B_OP_COPY);
}

void LScreenDC::StretchBlt(LRect *Dest, LSurface *Src, LRect *s)
{
	VIEW_CHECK()
	LAssert(0);
}

void LScreenDC::Bezier(int Threshold, LPoint *Pt)
{
	VIEW_CHECK()
	LAssert(0);
}

void LScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, LRect *Bounds)
{
	VIEW_CHECK()
	LAssert(0);
}

