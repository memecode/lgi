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

#include "Screen.h"
#include "Region.h"

#include "lgi/common/Gdc2.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/Variant.h"

#include <Bitmap.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define ROUND_UP(bits) (((bits) + 7) / 8)

class LMemDCPrivate
{
public:
	LArray<LRect> Client;
	LColourSpace CreateCs = CsNone;
	BBitmap *Bmp = NULL;
	BView *View = NULL;
	int LockCount = 0;
	bool debug = false;

	~LMemDCPrivate()
	{
		Empty();
	}
	
	void Empty()
	{
		if (Bmp)
		{
			delete Bmp;
			Bmp = NULL;
		}
	}
};

LArray<LMemDC*> LMemDC::Instances;

LMemDC::LMemDC(const char *file, int line, int x, int y, LColourSpace cs, int flags)
{
	d = new LMemDCPrivate;
	if (cs != CsNone)
		Create(x, y, cs, flags);
}

LMemDC::LMemDC(const char *file, int line, LSurface *pDC)
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
	if (!d->View && d->Bmp)
	{
		d->View = new BView(d->Bmp->Bounds(), "BBitmapView", B_FOLLOW_NONE, B_WILL_DRAW);
		if (d->View)
			d->Bmp->AddChild(d->View);
	}
	
	return d->View;
}

bool LMemDC::GetClient(LRect *c)
{
	if (!c)
		return false;

	if (d->Client.Length())
	{
		*c = d->Client.Last();
		return true;
	}

	c->ZOff(-1, -1);
	return false;
}

static LString descRect(BRect r)
{
	return LString::Fmt("%g,%g,%g,%g", r.left, r.top, r.right, r.bottom);
}

static LString descClip(BView *view)
{
	BRegion region;
	view->GetClippingRegion(&region);

	LString::Array a;
	for (int i=0; i<region.CountRects(); i++)
		a.Add(descRect(region.RectAt(i)));
	return LString("(") + LString(" ").Join(a) + ")";
}

void LMemDC::SetClient(LRect *c)
{
	Handle();
	auto locked = d->View->LockLooper();

	if (c)
	{
		LRect doc;
		if (d->Client.Length())
			doc = d->Client.Last();
		else
			doc = Bounds();
		
		Clip = *c;
		Clip.Bound(&doc);
		d->Client.Add(Clip);
		
		auto before = descClip(d->View);
		
		d->View->ConstrainClippingRegion(NULL);
		d->View->SetOrigin(0, 0);
		
		auto mid = descClip(d->View);		
		
		BRect br = Clip;
		d->View->ClipToRect(br);
		// d->View->SetOrigin(Clip.x1, Clip.y1);
		
		auto after = descClip(d->View);
		LRect bounds = d->View->Bounds();
		
		if (d->debug)
			printf("SetClient c=%s, clip=%s doc=%s len=%i viewClip=%s -> %s -> %s\n",
				c->GetStr(),
				Clip.GetStr(),
				doc.GetStr(),
				(int)d->Client.Length(),
				before.Get(), mid.Get(), after.Get());
		
		OriginX = -Clip.x1;
		OriginY = -Clip.y1;
	}
	else
	{
		if (d->Client.Length())
			d->Client.PopLast();

		// d->View->SetOrigin(0, 0);
		d->View->ConstrainClippingRegion(NULL);

		if (d->Client.Length())
		{
			Clip = d->Client.Last();
			OriginX = -Clip.x1;
			OriginY = -Clip.y1;
			// d->View->SetOrigin(Clip.x1, Clip.y1);
			d->View->ClipToRect(Clip);
		}
		else
		{
			OriginX = 0;
			OriginY = 0;
			Clip.ZOff(pMem->x-1, pMem->y-1);
		}

		/*		
		auto after = descClip(d->View);
		printf("UnSetClient %s len=%i viewClip=%s\n", Clip.GetStr(), (int)d->Client.Length(),
			after.Get());
		*/
	}

	if (locked)	
		d->View->UnlockLooper();
}


void LMemDC::Empty()
{
	d->Empty();
	DeleteObj(pMem);
}

bool LMemDC::Lock()
{
	if (!d->Bmp)
		return false;
	
	auto result = d->Bmp->LockBits();
	if (result != B_OK)
		return false;

	d->LockCount++;
	return true;
}

bool LMemDC::Unlock()
{
	if (!d->Bmp)
		return false;

	if (d->LockCount <= 0)
		return false;

	d->LockCount--;
	d->Bmp->UnlockBits();
	return true;
}

bool LMemDC::Create(int x, int y, LColourSpace Cs, int Flags)
{
	BRect b(0, 0, x-1, y-1);
	d->Bmp = new BBitmap(b, B_RGB32, true, true);
	if (!d->Bmp || d->Bmp->InitCheck() != B_OK)
	{
		DeleteObj(d->Bmp);
		LgiTrace("%s:%i - Failed to create memDC(%i,%i)\n", _FL, x, y);
		return false;
	}
	
	pMem = new LBmpMem;
	if (!pMem)
		return false;

	pMem->x		= x;
	pMem->y		= y;
	pMem->Cs	= ColourSpace = System32BitColourSpace;
	pMem->Line	= d->Bmp->BytesPerRow();
	pMem->Base	= (uchar*)d->Bmp->Bits();

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
		BScreen scr;
		BBitmap *bitmap = NULL;
		BRect src = br.SrcClip;
		auto r = scr.GetBitmap(&bitmap, TestFlag(Flags, GDC_CAPTURE_CURSOR), &src);
		if (r == B_OK)
		{
			bitmap->LockBits();
			
			int dstPx = GetBits() / 8;
			size_t srcPx = 4, row = 0, chunk = 0;			
			get_pixel_size_for(bitmap->ColorSpace(), &srcPx, &row, &chunk);
			
			for (int y=0; y<br.SrcClip.Y(); y++)
			{
				auto *dst = (*this)[br.DstClip.y1 + y] + (br.DstClip.x1 * dstPx);
				auto *src = ((uchar*)bitmap->Bits()) + (bitmap->BytesPerRow() * (br.SrcClip.y1 + y)) + (br.SrcClip.x1 * srcPx);
				LAssert(!"Impl pixel converter.");
			}
			
			bitmap->UnlockBits();
		}
		else
		{
			Colour(Rgb24(255, 0, 255), 24);
			Rectangle(a);
		}

		delete bitmap;
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

void LMemDC::SetOrigin(LPoint pt)
{
	LSurface::SetOrigin(pt);
}

bool LMemDC::SupportsAlphaCompositing()
{
	return true;
}

LPoint LMemDC::GetOrigin()
{
	return LSurface::GetOrigin();
}

LRect LMemDC::ClipRgn(LRect *Rgn)
{
	LRect Old = Clip;
	
	auto bounds = Bounds();
	if (Rgn)
	{
		Clip = *Rgn;
		Clip.Offset(-OriginX, -OriginY);
		Clip.Bound(&bounds);
		// printf("  ClipRgn=%s\n", Clip.GetStr());
	}
	else
	{
		Clip = bounds;
		// printf("  unClipRgn=%s\n", Clip.GetStr());
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

bool LMemDC::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (!Stricmp(Name, "debug"))
	{
		d->debug = Value.CastBool();
	}

	return false;
}
