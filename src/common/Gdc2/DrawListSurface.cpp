#include "lgi/common/Lgi.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/DrawListSurface.h"

struct LCmd
{
	virtual ~LCmd() {}
	virtual bool OnPaint(LDrawListSurfacePriv *d, LSurface *pDC) = 0;
};

struct LDrawListSurfacePriv : public LArray<LCmd*>
{
	int x, y, Bits;
	LPoint Dpi;
	LColour Fore, Back;
	LSurface *CreationSurface;
	LFont *Font;

	LDrawListSurfacePriv()
	{
		x = y = 0;
		CreationSurface = NULL;
		Font = NULL;
	}

	~LDrawListSurfacePriv()
	{
		DeleteObjects();
	}
	
	bool OnPaint(LSurface *pDC)
	{
		for (unsigned i=0; i<Length(); i++)
		{
			LCmd *c = (*this)[i];
			if (c && !c->OnPaint(this, pDC))
			{
				return false;
			}
		}
		
		return true;
	}
};

struct LCmdTxt : public LCmd
{
	LPoint p;
	LDisplayString *Ds;

	LCmdTxt(int x, int y, LDisplayString *ds) : p(x, y)
	{
		Ds = ds;
	}
	
	bool OnPaint(LDrawListSurfacePriv *d, LSurface *pDC)
	{
		LFont *f = Ds->GetFont();
		f->Transparent(!d->Back.IsValid());
		f->Colour(d->Fore, d->Back);
		Ds->Draw(pDC, p.x, p.y);		
		return true;
	}
};

struct LCmdBlt : public LCmd
{
	LPoint p;
	LSurface *Img;
	LRect Dst;
	LRect Src;
	bool Stretch;

	LCmdBlt(int X, int Y, LSurface *img, LRect *dst, LRect *src, bool stretch = false) :
		p(X, Y)
	{
		Img = img;
		if (dst)
			Dst = *dst;
		else
			Dst.ZOff(-1,-1);
		if (src)
			Src = *src;
		else
			Src.ZOff(-1, -1);
		Stretch = stretch;
		if (Stretch)
			LAssert(Src.Valid() && Dst.Valid());
	}
	
	bool OnPaint(LDrawListSurfacePriv *d, LSurface *pDC)
	{
		if (Stretch)
			pDC->StretchBlt(&Dst, Img, &Src);
		else
			pDC->Blt(p.x, p.y, Img, Src.Valid() ? &Src : NULL);
		return true;
	}
};

struct LCmdRect : public LCmd
{
	LRect r;
	bool Filled;
	
	LCmdRect(LRect &rc, bool filled)
	{
		r = rc;
		Filled = filled;
	}

	bool OnPaint(LDrawListSurfacePriv *d, LSurface *pDC)
	{
		if (Filled)
			pDC->Rectangle(&r);
		else
			pDC->Box(&r);
		return true;
	}
};

struct LCmdPixel : public LCmd
{
	LPoint p;
	
	LCmdPixel(int x, int y) : p(x, y)
	{
	}

	bool OnPaint(LDrawListSurfacePriv *d, LSurface *pDC)
	{
		pDC->Set(p.x, p.y);
		return true;
	}
};

struct LCmdColour : public LCmd
{
	LColour c;
	
	LCmdColour(LColour &col)
	{
		c = col;
	}

	bool OnPaint(LDrawListSurfacePriv *d, LSurface *pDC)
	{
		pDC->Colour(c);
		return true;
	}
};

struct LCmdLine : public LCmd
{
	LPoint s, e;
	
	LCmdLine(int x1, int y1, int x2, int y2) : s(x1, y1), e(x2, y2)
	{
	}

	bool OnPaint(LDrawListSurfacePriv *d, LSurface *pDC)
	{
		pDC->Line(s.x, s.y, e.x, e.y);
		return true;
	}
};

LDrawListSurface::LDrawListSurface(int Width, int Height, LColourSpace Cs)
{
	d = new LDrawListSurfacePriv;
	d->x = Width;
	d->y = Height;
	d->Dpi = LScreenDpi();
	d->Bits = LColourSpaceToBits(Cs);
	d->Fore = LColour::Black;
	ColourSpace = Cs;
}

LDrawListSurface::LDrawListSurface(LSurface *FromSurface)
{
	d = new LDrawListSurfacePriv;
	d->x = FromSurface->X();
	d->y = FromSurface->Y();
	d->Dpi = FromSurface->GetDpi();
	ColourSpace = FromSurface->GetColourSpace();
	d->Bits = LColourSpaceToBits(ColourSpace);
	d->Fore = LColour::Black;
	d->CreationSurface = FromSurface;
}

LDrawListSurface::~LDrawListSurface()
{
	delete d;
}

ssize_t LDrawListSurface::Length()
{
	return d->Length();
}

bool LDrawListSurface::OnPaint(LSurface *Dest)
{
	return d->OnPaint(Dest);
}

LFont *LDrawListSurface::GetFont()
{
	return d->Font;
}

void LDrawListSurface::SetFont(LFont *Font)
{
	d->Font = Font;
}

LColour LDrawListSurface::Background()
{
	return d->Back;
}

LColour LDrawListSurface::Background(LColour c)
{
	LColour Prev = d->Back;
	d->Back = c;
	return Prev;	
}

LDisplayString *LDrawListSurface::Text(int x, int y, const char *Str, int Len)
{
	if (!d->Font)
	{
		LAssert(!"No font.");
		return NULL;
	}
	
	LDisplayString *Ds = new LDisplayString(d->Font, Str, Len, d->CreationSurface);
	if (!Ds)
	{
		LAssert(0);
		return NULL;
	}
	
	LCmdTxt *Txt = new LCmdTxt(x, y, Ds);
	if (!Txt)
	{
		delete Ds;
		return NULL;
	}
	
	d->Add(Txt);
	return Ds;
}

LRect LDrawListSurface::ClipRgn()
{
	return Clip;
}

LRect LDrawListSurface::ClipRgn(LRect *Rgn)
{
	Clip = *Rgn;
	return Clip;
}

COLOUR LDrawListSurface::Colour()
{
	switch (d->Bits)
	{
		case 8:
			return d->Fore.c8();
		case 15:
			return CBit(d->Bits, d->Fore.c24());
		case 24:
			return d->Fore.c24();
		case 32:
			return d->Fore.c32();
	}
	
	LAssert(0);
	return 0;
}

COLOUR LDrawListSurface::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = Colour();
	d->Fore.Set(c, Bits);
	
	LCmdColour *cmd = new LCmdColour(d->Fore);
	if (cmd)
		d->Add(cmd);
	else
		LAssert(0);
	
	return Prev;
}

LColour LDrawListSurface::Colour(LColour c)
{
	LColour Prev = d->Fore;
	d->Fore = c;

	LCmdColour *cmd = new LCmdColour(d->Fore);
	if (cmd)
		d->Add(cmd);
	else
		LAssert(0);

	return Prev;
}

int LDrawListSurface::X()
{
	return d->x;
}

int LDrawListSurface::Y()
{
	return d->y;
}

ssize_t LDrawListSurface::GetRowStep()
{
	ssize_t Row = (d->Bits * d->x + 7) >> 3;
	return Row;
}

LPoint LDrawListSurface::GetDpi()
{
	return d->Dpi;
}

int LDrawListSurface::GetBits()
{
	return d->Bits;
}

void LDrawListSurface::SetOrigin(LPoint pt)
{
	LSurface::SetOrigin(pt);
}

void LDrawListSurface::Set(int x, int y)
{
	LCmdPixel *c = new LCmdPixel(x, y);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

void LDrawListSurface::HLine(int x1, int x2, int y)
{
	LCmdLine *c = new LCmdLine(x1, y, x2, y);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

void LDrawListSurface::VLine(int x, int y1, int y2)
{
	LCmdLine *c = new LCmdLine(x, y1, x, y2);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

void LDrawListSurface::Line(int x1, int y1, int x2, int y2)
{
	LCmdLine *c = new LCmdLine(x1, y1, x2, y2);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

uint LDrawListSurface::LineStyle(uint32_t Bits, uint32_t Reset)
{
	LAssert(!"Impl me.");
	return 0;
}

void LDrawListSurface::Circle(double cx, double cy, double radius)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::FilledCircle(double cx, double cy, double radius)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::Arc(double cx, double cy, double radius, double start, double end)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::FilledArc(double cx, double cy, double radius, double start, double end)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::Ellipse(double cx, double cy, double x, double y)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::FilledEllipse(double cx, double cy, double x, double y)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::Box(int x1, int y1, int x2, int y2)
{
	LRect r(x1, y1, x2, y2);
	LCmdRect *c = new LCmdRect(r, false);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

void LDrawListSurface::Box(LRect *r)
{
    LRect rc = r ? r : Bounds();
	LCmdRect *c = new LCmdRect(rc, false);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

void LDrawListSurface::Rectangle(int x1, int y1, int x2, int y2)
{
	LRect r(x1, y1, x2, y2);
	LCmdRect *c = new LCmdRect(r, true);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

void LDrawListSurface::Rectangle(LRect *r)
{
    LRect rc = r ? r : Bounds();
	LCmdRect *c = new LCmdRect(rc, true);
	if (c)
		d->Add(c);
	else
		LAssert(0);
}

void LDrawListSurface::Blt(int x, int y, LSurface *Src, LRect *SrcRc)
{
	if (!Src)
	{
		LAssert(0);
		return;
	}
	
	LCmdBlt *c = new LCmdBlt(x, y, Src, NULL, SrcRc, false);
	if (c)
		d->Add(c);
	else
		LAssert(0);	
}

void LDrawListSurface::StretchBlt(LRect *drc, LSurface *Src, LRect *s)
{
	if (!d || !Src || !s)
	{
		LAssert(0);
		return;
	}
	
	LCmdBlt *c = new LCmdBlt(drc->x1, drc->y1, Src, drc, s, true);
	if (c)
		d->Add(c);
	else
		LAssert(0);	
}

void LDrawListSurface::Polygon(int Points, LPoint *Data)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::Bezier(int Threshold, LPoint *Pt)
{
	LAssert(!"Impl me.");
}

void LDrawListSurface::FloodFill(int x, int y, int Mode, COLOUR Border, LRect *Bounds)
{
	LAssert(!"Impl me.");
}

