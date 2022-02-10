/*hdr
**	FILE:			LMemDC.h
**	AUTHOR:			Matthew Allen
**	DATE:			27/11/2001
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GdiLeak.h"
#include "GPalette.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
class LMemDCPrivate
{
public:
	void		*pBits;
	bool		UpsideDown;
	LRect		Client;

	LMemDCPrivate()
	{
		pBits = 0;
		Client.ZOff(-1, -1);
		UpsideDown = false;
	}
	
	~LMemDCPrivate()
	{
	}
};

LMemDC::LMemDC(int x, int y, LColourSpace cs, int flags)
{
	d = new LMemDCPrivate;
	ColourSpace = CsNone;
	pMem = 0;

	if (x > 0 && y > 0)
	{
		Create(x, y, cs, flags);
	}
}

LMemDC::LMemDC(LSurface *pDC)
{
	d = new LMemDCPrivate;
	ColourSpace = CsNone;
	pMem = 0;

	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetColourSpace()) )
	{
		if (pDC->Palette())
		{
			Palette(new GPalette(pDC->Palette()));
		}

		Blt(0, 0, pDC);

		if (pDC->AlphaDC() &&
			HasAlpha(true))
		{
			pAlphaDC->Blt(0, 0, pDC->AlphaDC());
		}
	}
}

LMemDC::~LMemDC()
{
	DeleteObj(pMem);
	DeleteObj(d);
}

OsPainter LMemDC::Handle()
{
	return NULL;
}

OsBitmap LMemDC::GetBitmap()
{
	return NULL;
}

void LMemDC::SetClient(LRect *c)
{
	if (c)
	{
		LRect Doc(0, 0, pMem->x-1, pMem->y-1);
		Clip = d->Client = *c;
		Clip.Bound(&Doc);
		
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);

		OriginX = 0;
		OriginY = 0;
		Clip.ZOff(pMem->x-1, pMem->y-1);
	}
}

bool LMemDC::Lock()
{
	return true;
}

bool LMemDC::Unlock()
{
	return true;
}

LRect LMemDC::ClipRgn(LRect *Rgn)
{
	LRect Prev = Clip;

	if (Rgn)
	{
		LPoint Origin(0, 0);
		/*
		if (hDC)
		    GetWindowOrgEx(hDC, &Origin);
		else
		*/
		{
			Origin.x = OriginX;
			Origin.y = OriginY;
		}

		Clip.x1 = MAX(Rgn->x1 - Origin.x, 0);
		Clip.y1 = MAX(Rgn->y1 - Origin.y, 0);
		Clip.x2 = MIN(Rgn->x2 - Origin.x, pMem->x-1);
		Clip.y2 = MIN(Rgn->y2 - Origin.y, pMem->y-1);

		/*
		HRGN hRgn = CreateRectRgn(Clip.x1, Clip.y1, Clip.x2+1, Clip.y2+1);
		if (hRgn)
		{
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}
		*/
	}
	else
	{
		Clip.x1 = 0;
		Clip.y1 = 0;
		Clip.x2 = X()-1;
		Clip.y2 = Y()-1;

		// SelectClipRgn(hDC, NULL);
	}

	return Prev;
}

bool LMemDC::SupportsAlphaCompositing()
{
	return true;
}

bool LMemDC::Create(int x, int y, LColourSpace Cs, int Flags)
{
	bool Status = false;
	LBmpMem *pOldMem = pMem;

	DrawOnAlpha(false);
	DeleteObj(pAlphaDC);

	pMem = NULL;

	if (x > 0 && y > 0)
	{
		int Bits = GColourSpaceToBits(Cs);
		int Colours = Bits <= 8 ? 1 << Bits : 0;

		{
			// Non-native colour space support...
			ColourSpace = Cs;
			if (ColourSpace)
			{
				// Non-native image data
				pMem = new LBmpMem;
				if (pMem)
				{
					pMem->x = x;
					pMem->y = y;
					pMem->Line = ((x * Bits + 31) / 32) << 2;
					pMem->Cs = ColourSpace;
					pMem->OwnMem(true);
					pMem->Base = new uchar[pMem->y * pMem->Line];
					
					Status = pMem->Base != NULL;
				}
			}
		}

		if (Status)
		{
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
				pApp = CreateApplicator(NewOp);
				Flags &= ~GDC_CACHED_APPLICATOR;
				Flags |= GDC_OWN_APPLICATOR;
			}

			Clip.ZOff(X()-1, Y()-1);
		}

		if (!Status)
		{
			DeleteObj(pMem)
		}
	}

	DeleteObj(pOldMem);

	return Status;
}


void LMemDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
    LAssert(Src);
    if (!Src)
        return;

	if (Src->IsScreen())
	{
		LRect b;
		LRect Bounds(0, 0, GdcD->X()-1, GdcD->Y()-1);
		if (a)
		{
			b = *a;
			b.Bound(&Bounds);
		}
		else
		{
			b = Bounds;
		}

	}
	else
	{
		LSurface::Blt(x, y, Src, a);
	}
}

void LMemDC::StretchBlt(LRect *d, LSurface *Src, LRect *s)
{
	if (Src)
	{
		LRect DestR;
		if (d)
		{
			DestR = *d;
		}
		else
		{
			DestR.ZOff(X()-1, Y()-1);
		}

		LRect SrcR;
		if (s)
		{
			SrcR = *s;
		}
		else
		{
			SrcR.ZOff(Src->X()-1, Src->Y()-1);
		}

	}
}

void LMemDC::HorzLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (	x1 <= x2 &&
		y >= Clip.y1 &&
		y <= Clip.y2)
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
	if (	y1 <= y2 &&
		x >= Clip.x1 &&
		x <= Clip.x2)
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

void LMemDC::SetOrigin(int x, int y)
{
	LSurface::SetOrigin(x, y);
}
