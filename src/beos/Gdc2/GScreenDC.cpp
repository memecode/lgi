/*hdr
**	FILE:			Gdc2.h
**	AUTHOR:			Matthew Allen
**	DATE:			20/2/97
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>
#include "Lgi.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
class LScreenPrivate
{
public:
	OsView View;
	int Depth;
	bool ClientClip;
	LRect Client;
	LRect Clip;
	int ConstAlpha;
	
	LScreenPrivate()
	{
		View = 0;
		ConstAlpha = 255;
		ClientClip = false;
		Depth = GdcD->GetBits();
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
LScreenDC::LScreenDC(LView *view, void *Param)
{
	LAssert(view);

	d = new LScreenPrivate;
	d->View = view->Handle();
}

LScreenDC::LScreenDC(BView *view)
{
	LAssert(view);

	d = new LScreenPrivate;
	d->View = view;
}

LScreenDC::~LScreenDC()
{
	DeleteObj(d);
}

int LScreenDC::GetFlags()
{
	return 0;
}

bool LScreenDC::GetClient(LRect *c)
{
	if (!c)
		return false;

	*c = d->Client;
	return true;
}

void LScreenDC::SetClient(LRect *r)
{
	// Unset previous client
	if (d->Client.Valid())
	{
		d->View->SetOrigin(0, 0);
		d->View->ConstrainClippingRegion(NULL);
	}
	
	if (r)
	{
		d->Client = *r;
	}
	else
	{
		d->Client.ZOff(-1, -1);
	}

	// Set new client
	if (d->Client.Valid())
	{
		LRect b = d->Client;
		b.Offset(-b.x1, -b.y1);
		BRegion r(b);
		d->View->ConstrainClippingRegion(&r);
		d->View->SetOrigin(d->Client.x1, d->Client.y1);
	}
}

OsView LScreenDC::Handle()
{
	return d->View;
}

GPalette *LScreenDC::Palette()
{
	return 0;
}

void LScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
}

uint LScreenDC::LineStyle()
{
	return 0;
}

uint LScreenDC::LineStyle(uint Bits, uint32 Reset)
{
	return 0;
}

void LScreenDC::GetOrigin(int &x, int &y)
{
	BPoint p = d->View->Origin();
	x = -p.x;
	y = -p.y;
}

void LScreenDC::SetOrigin(int x, int y)
{
	d->View->SetOrigin(-x, -y);
}

LRect LScreenDC::ClipRgn()
{
	return d->Clip;
}

LRect LScreenDC::ClipRgn(LRect *Rgn)
{
	if (Rgn)
	{
		BRegion b;
		b.Set(*Rgn);
		d->View->ConstrainClippingRegion(&b);
		d->Clip = *Rgn;
	}
	else
	{
		d->View->ConstrainClippingRegion(NULL);
		d->Clip.ZOff(-1, -1);
	}
}

bool LScreenDC::SupportsAlphaCompositing()
{
	return true;
}

COLOUR LScreenDC::Colour()
{
	rgb_color Rgb = d->View->HighColor();
	return CBit(GetBits(), Rgb24(Rgb.red, Rgb.green, Rgb.blue), 24);
}

LColour LScreenDC::Colour(LColour c)
{
	rgb_color Rgb = d->View->HighColor();
	LColour Prev(Rgb.red, Rgb.green, Rgb.blue);

	Rgb.red = c.r();
	Rgb.green = c.g();
	Rgb.blue = c.b();

	d->View->SetHighColor(Rgb);
	d->View->SetLowColor(Rgb);

	return Prev;
}


COLOUR LScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = Colour();

	if (Bits < 1) Bits = GetBits();
	c = CBit(24, c, Bits);
	
	rgb_color Rgb;
	Rgb.red = R24(c);
	Rgb.green = G24(c);
	Rgb.blue = B24(c);
	d->View->SetHighColor(Rgb);
	
	return Prev;
}

int LScreenDC::Op()
{
	int Mode = d->View->DrawingMode();
	switch (Mode)
	{
		case B_OP_INVERT:
		{
			return GDC_XOR;
		}
		case B_OP_OVER:
		{
			return GDC_ALPHA;
		}
	}
	return GDC_SET;
}

int LScreenDC::Op(int NewOp, NativeInt Param)
{
	int Prev = Op();
	
	drawing_mode Mode = B_OP_COPY;
	d->ConstAlpha = 255;
	
	switch (NewOp)
	{
		case GDC_XOR:
		{
			Mode = B_OP_INVERT;
			break;
		}
		case GDC_ALPHA:
		{
			Mode = B_OP_ALPHA;
			d->ConstAlpha = Param >= 0 ? Param : 255;
			break;
		}
	}
	d->View->SetDrawingMode(Mode);
	return Prev;
}

int LScreenDC::X()
{
	BRect r = d->View->Bounds();
	return r.Width();
}

int LScreenDC::Y()
{
	BRect r = d->View->Bounds();
	return r.Height();
}

int LScreenDC::GetBits()
{
	return d->Depth;
}

void LScreenDC::Set(int x, int y)
{
	d->View->StrokeLine(BPoint(x, y), BPoint(x, y));
}

COLOUR LScreenDC::Get(int x, int y)
{
	// you can't get the value of a pixel!
	// fix this Be!!!!!!!!!!
	return 0;
}

void LScreenDC::HLine(int x1, int x2, int y)
{
	d->View->StrokeLine(BPoint(x1, y), BPoint(x2, y));
}

void LScreenDC::VLine(int x, int y1, int y2)
{
	d->View->StrokeLine(BPoint(x, y1), BPoint(x, y2));
}

void LScreenDC::Line(int x1, int y1, int x2, int y2)
{
	d->View->StrokeLine(BPoint(x1, y1), BPoint(x2, y2));
}

void LScreenDC::Circle(double cx, double cy, double radius)
{
	d->View->StrokeEllipse(BPoint(cx, cy), radius, radius);
}

void LScreenDC::FilledCircle(double cx, double cy, double radius)
{
	d->View->FillEllipse(BPoint(cx, cy), radius, radius);
}

void LScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	d->View->StrokeArc(BPoint(cx, cy), radius, radius, start, end - start);
}

void LScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	d->View->FillArc(BPoint(cx, cy), radius, radius, start, end - start);
}

void LScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	d->View->StrokeEllipse(BPoint(cx, cy), x, y);
}

void LScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	d->View->FillEllipse(BPoint(cx, cy), x, y);
}

void LScreenDC::Box(int x1, int y1, int x2, int y2)
{
	d->View->StrokeRect(BRect(x1, y1, x2, y2));
}

void LScreenDC::Box(LRect *a)
{
	BRect r;
	if (a)
	{
		r = *a;
	}
	else
	{
		r.left = r.top = 0;
		r.right = X();
		r.bottom = Y();
	}
	
	d->View->StrokeRect(r);
}

void LScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	d->View->FillRect(BRect(x1, y1, x2, y2));
}

void LScreenDC::Rectangle(LRect *a)
{
	BRect r;
	if (a)
	{
		r = *a;
	}
	else
	{
		r.left = r.top = 0;
		r.right = X();
		r.bottom = Y();
	}
	
	d->View->FillRect(r);
}

void LScreenDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
	if (Src)
	{
		if (Src->IsScreen())
		{
			// screen to screen Blt
			printf("%s:%i - Error: can't do screen to screen blt.\n", _FL);
		}
		else
		{
			// memory to screen Blt
			LMemDC *Dc = dynamic_cast<LMemDC*>(Src);
			BBitmap *Bmp = Dc ? Dc->GetBitmap() : 0;
			if (Bmp)
			{
				LRect SrcRc;
				if (a)
					SrcRc = *a;
				else
					SrcRc = Src->Bounds();
				
				BRect S = SrcRc;
				BRect D(x, y, x+S.Width(), y+S.Height());
				
				if (d->ConstAlpha == 0)
				{
				}
				else if (d->ConstAlpha < 255)
				{
					// Off screen comp
					LMemDC mem(S.Width()+1, S.Height()+1, System32BitColourSpace);
					
					mem.Colour(B_TRANSPARENT_MAGIC_RGBA32, 32);
					mem.Rectangle();
					mem.Blt(0, 0, Src, a);
					mem.SetConstantAlpha(d->ConstAlpha);
					
					d->View->DrawBitmap(mem.GetBitmap(), S = mem.Bounds(), D);
				}
				else
				{
					d->View->DrawBitmap(Bmp, S, D);
				}				
			}
			else printf("%s:%i - Error: No bitmap to blt\n", _FL);
		}
	}
}

void LScreenDC::StretchBlt(LRect *d, LSurface *Src, LRect *s)
{
}

void LScreenDC::Polygon(int Points, LPoint *Data)
{
	if (Points > 0 && Data)
	{
		BPoint *p = new BPoint[Points];
		if (p)
		{
			for (int i=0; i<Points; i++)
			{
				p[i].x = Data[i].x;
				p[i].y = Data[i].y;
			}
			
			d->View->StrokePolygon(p, Points);
			DeleteArray(p);
		}
	}
}

void LScreenDC::Bezier(int Threshold, LPoint *Pt)
{
}

void LScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, LRect *Bounds)
{
}

