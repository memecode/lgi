/*hdr
**	FILE:			MemDC.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			14/10/2000
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2021, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "lgi/common/Gdc2.h"
#include "lgi/common/LgiString.h"

#include <Bitmap.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define ROUND_UP(bits) (((bits) + 7) / 8)

class LMemDCPrivate
{
public:
	::LArray<LRect> Client;
	LColourSpace CreateCs = CsNone;
	BBitmap *Bmp = NULL;

    LMemDCPrivate()
    {
    }

    ~LMemDCPrivate()
    {
    	Empty();
	}

	void Empty()
	{
	}
};

LMemDC::LMemDC(int x, int y, LColourSpace cs, int flags)
{
	d = new LMemDCPrivate;
	if (cs != CsNone)
		Create(x, y, cs, flags);
}

LMemDC::LMemDC(LSurface *pDC)
{
	d = new LMemDCPrivate;
	
	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetColourSpace()))
	{
		Blt(0, 0, pDC);
	}
}

LMemDC::~LMemDC()
{
	Empty();
	DeleteObj(d);
}

OsBitmap LMemDC::GetBitmap()
{
	return d->Bmp;
}

OsPainter LMemDC::Handle()
{
	return NULL;
}

void LMemDC::SetClient(LRect *c)
{
	if (c)
	{
		Handle();

		LRect Doc;
		if (d->Client.Length())
			Doc = d->Client.Last();
		else
			Doc = Bounds();
		
		LRect r = *c;
		r.Bound(&Doc);
		d->Client.Add(r);
		
		Clip = r;
		
		OriginX = -r.x1;
		OriginY = -r.y1;
	}
	else
	{
		if (d->Client.Length())
			d->Client.PopLast();

		if (d->Client.Length())
		{
			auto &r = d->Client.Last();
			OriginX = -r.x1;
			OriginY = -r.y1;
			Clip = r;
		}
		else
		{
			OriginX = 0;
			OriginY = 0;
			Clip.ZOff(pMem->x-1, pMem->y-1);
		}
	}
}


void LMemDC::Empty()
{
	d->Empty();
	DeleteObj(pMem);
}

bool LMemDC::Lock()
{
	return false;
}

bool LMemDC::Unlock()
{
	return false;
}

bool LMemDC::Create(int x, int y, LColourSpace Cs, int Flags)
{
	BRect b(0, 0, x, y);
	d->Bmp = new BBitmap(b, B_RGB32, false, true);
	if (!d->Bmp || d->Bmp->InitCheck() != B_OK)
	{
		DeleteObj(d->Bmp);
		LgiTrace("%s:%i - Failed to create memDC(%i,%i)\n", _FL, x, y);
		return false;
	}

	pMem = new LBmpMem;
	if (!pMem)
		return false;

	pMem->x = d->Bmp->Bounds().Width();
	pMem->y = d->Bmp->Bounds().Height();
	ColourSpace = pMem->Cs = System32BitColourSpace;
	pMem->Line = d->Bmp->BytesPerRow();
	pMem->Base = d->Bmp->Bits();

	int NewOp = (pApp) ? Op() : GDC_SET;
	if ((Flags & GDC_OWN_APPLICATOR) && !(Flags & GDC_CACHED_APPLICATOR))
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
		pApp = CreateApplicator(NewOp);
		Flags &= ~GDC_CACHED_APPLICATOR;
		Flags |= GDC_OWN_APPLICATOR;
	}

	Clip.ZOff(X()-1, Y()-1);

	// printf("CreatedBmp: %i,%i %s,%i,%p\n", pMem->x, pMem->y, LColourSpaceToString(pMem->Cs), pMem->Line, pMem->Base);

	return true;
}

void LMemDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
	if (!Src)
		return;

	bool Status = false;
	LBlitRegions br(this, x, y, Src, a);
	if (!br.Valid())
		return;

	LScreenDC *Screen;
	if ((Screen = Src->IsScreen()))
	{
		if (pMem->Base)
		{

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
		LSurface::Blt(x, y, Src, a);
	}
}

void LMemDC::StretchBlt(LRect *d, LSurface *Src, LRect *s)
{
    LAssert(!"Not implemented");
}

void LMemDC::SetOrigin(int x, int y)
{
}

bool LMemDC::SupportsAlphaCompositing()
{
	return true;
}

void LMemDC::GetOrigin(int &x, int &y)
{
	LSurface::GetOrigin(x, y);
}

LRect LMemDC::ClipRgn(LRect *Rgn)
{
	LRect Old = Clip;
	
	if (Rgn)
	{
		LRect Dc(0, 0, X()-1, Y()-1);
		
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

void LMemDC::HorzLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LSwap(x1, x2);

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

void LMemDC::VertLine(int x, int y1, int y2, COLOUR a, COLOUR b)
{
	if (y1 > y2) LSwap(y1, y2);
	
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
