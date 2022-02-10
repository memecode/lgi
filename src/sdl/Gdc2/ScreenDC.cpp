/*hdr
**	FILE:		Gdc2.h
**	AUTHOR:		Matthew Allen
**	DATE:		20/2/97
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

class LScreenPrivate
{
public:
	int			Op;
	LRect		Client;
	uint32		Col;
	SDL_Surface *Screen;
	
	LScreenPrivate()
	{
		Op = GDC_SET;
		Client.ZOff(-1, -1);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
LScreenDC::LScreenDC()
{
	d = new LScreenPrivate;
	ColourSpace = CsNone;
}

LScreenDC::LScreenDC(LView *view, void *Param)
{
	d = new LScreenPrivate;
	ColourSpace = CsNone;
	
	d->Screen = (SDL_Surface*)Param;
	LAssert(d->Screen);
	if (d->Screen)
	{
		ColourSpace = PixelFormat2ColourSpace(d->Screen->format);
		
		SDL_LockSurface(d->Screen);

		pMem = new LBmpMem;
		LAssert(pMem != NULL);
		if (pMem)
		{
			pMem->Base = (uchar*)d->Screen->pixels;
			pMem->x = d->Screen->w;
			pMem->y = d->Screen->h;
			pMem->Line = d->Screen->pitch;
			pMem->Cs = ColourSpace;
		}

		pApp = CreateApplicator(d->Op, ColourSpace);
		LAssert(pApp != NULL);
		
		Clip.ZOff(d->Screen->w-1, d->Screen->h-1);
	}
}

LScreenDC::~LScreenDC()
{
	if (d->Screen)
	{
		/*
		LBgra32 *p = (LBgra32*) d->Screen->pixels;
		memset(p, 0, 4*4);
		p[0].r = 255;
		p[1].g = 255;
		p[2].b = 255;
		p[3].a = 255;
		*/

		SDL_UnlockSurface(d->Screen);
		SDL_UpdateRect(d->Screen, 0, 0, 0, 0);
	}
	DeleteObj(d);
}

OsPainter LScreenDC::Handle()
{
	return d->Screen;
}

int LScreenDC::GetFlags()
{
	return 0;
}

bool LScreenDC::GetClient(LRect *c)
{
	if (!c) return false;
	*c = d->Client;
	return true;
}

void LScreenDC::SetClient(LRect *c)
{
	if (c)
	{
		LRect Doc(0, 0, pMem->x-1, pMem->y-1);
		Clip = *c;
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

GPalette *LScreenDC::Palette()
{
	return LSurface::Palette();
}

void LScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
	LSurface::Palette(pPal, bOwnIt);
}

int LScreenDC::X()
{
	if (d->Client.Valid())
		return d->Client.X();

	return d->Screen->w;
}

int LScreenDC::Y()
{
	if (d->Client.Valid())
		return d->Client.Y();

	return d->Screen->h;
}

int LScreenDC::GetBits()
{
	return d->Screen ? d->Screen->format->BitsPerPixel : 0;
}

bool LScreenDC::SupportsAlphaCompositing()
{
	// Windows does support blending screen content with bitmaps that have alpha
	return true;
}

uint LScreenDC::LineStyle(uint32 Bits, uint32 Reset)
{
	return LineBits = Bits;
}

uint LScreenDC::LineStyle()
{
	return LineBits;
}
