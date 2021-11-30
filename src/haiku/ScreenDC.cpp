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

class LScreenPrivate
{
public:
	int x, y, Bits;
	bool Own;
	LColour Col;
	LRect Client;

	LView *View;
	OsView v;

	LScreenPrivate()
	{
		View = NULL;
		x = y = Bits = 0;
		Own = false;
		v = 0;
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

LScreenDC::LScreenDC(LView *view, void *param)
{
	d = new LScreenPrivate;
	d->View = view;
	if (view)
	{
		LWindow *w = view->GetWindow();
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
	return NULL;
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

}

void LScreenDC::SetClient(LRect *c)
{
	if (c)
	{
		d->Client = *c;


		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		if (Clip.Valid())
			ClipRgn(NULL);
		
		OriginX = 0;
		OriginY = 0;	


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

	}
	else
	{
	    Clip.ZOff(-1, -1);
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
}

COLOUR LScreenDC::Get(int x, int y)
{
	return 0;
}

void LScreenDC::HLine(int x1, int x2, int y)
{
}

void LScreenDC::VLine(int x, int y1, int y2)
{
}

void LScreenDC::Line(int x1, int y1, int x2, int y2)
{
}

void LScreenDC::Circle(double cx, double cy, double radius)
{
}

void LScreenDC::FilledCircle(double cx, double cy, double radius)
{
}

void LScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
}

void LScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
}

void LScreenDC::Ellipse(double cx, double cy, double x, double y)
{
}

void LScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
}

void LScreenDC::Box(int x1, int y1, int x2, int y2)
{
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
	}
}

void LScreenDC::Rectangle(LRect *a)
{
	if (a)
	{
		if (a->X() > 0 &&
			a->Y() > 0)
		{
		}
	}
	else
	{
	}
}

void LScreenDC::Polygon(int Points, LPoint *Data)
{
	if (!Data || Points <= 0)
		return;

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

	
}

void LScreenDC::StretchBlt(LRect *d, LSurface *Src, LRect *s)
{
	LAssert(0);
}

void LScreenDC::Bezier(int Threshold, LPoint *Pt)
{
	LAssert(0);
}

void LScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, LRect *Bounds)
{
	LAssert(0);
}

