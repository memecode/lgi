/*hdr
**	FILE:			MemDC.h
**	AUTHOR:			Matthew Allen
**	DATE:			27/11/2001
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "lgi/common/Gdc2.h"
#include "lgi/common/GdiLeak.h"
#include "lgi/common/Palette.h"
#import <AppKit/NSCursor.h>

#define AlphaType		kCGImageAlphaPremultipliedLast

/////////////////////////////////////////////////////////////////////////////////////////////////////
class CGImgPriv
{
public:
	LAutoPtr<uint8,true> Data;
	CGImageRef Img = NULL;
	CGColorSpaceRef Cs = NULL;
	CGDataProviderRef Prov = NULL;
	int Debug = -1;
	
	~CGImgPriv()
	{
		Release();
	}

	void Release()
	{
		if (Img)
		{
			CGImageRelease(Img);
			Img = NULL;
		}
		
		if (Cs)
		{
			CGColorSpaceRelease(Cs);
			Cs = NULL;
		}
		
		if (Prov)
		{
			CGDataProviderRelease(Prov);
			Prov = NULL;
		}
	}
};

void ReleaseCGImg(void *info, const void *data, size_t size)
{
	CGImg *img = (CGImg*)info;
	delete img;
}

CGImg::CGImg(LSurface *pDC)
{
	d = new CGImgPriv;
	if (pDC)
	{
		uchar *a = (*pDC)[0];
		uchar *b = (*pDC)[1];
		if (a && b)
			Create(pDC->X(), pDC->Y(), pDC->GetBits(), (int)(b - a), a, 0, 0);
	}
}

CGImg::CGImg(int x, int y, int Bits, ssize_t Line, uchar *data, uchar *palette, LRect *r, int Debug)
{
	d = new CGImgPriv;
	d->Debug = Debug;
	Create(x, y, Bits, Line, data, palette, r);
}

CGImg::~CGImg()
{
	DeleteObj(d);
}

CGImg::operator CGImageRef()
{
	return d->Img;
}

void CGImg::Release()
{
	d->Release();
}

#define COPY_MODE		1

void CGImg::Create(int x, int y, int Bits, ssize_t Line, uchar *data, uchar *palette, LRect *r)
{
	LRect All(0, 0, x-1, y-1);
	LRect B;
	if (r)
	{
		B = *r;
		B.Bound(&All);
	}
	else
	{
		B = All;
	}
	
	if (!data)
		return;
	
	int Bytes = Bits / 8;
	int RowSize = Bytes * B.X();
	uchar *Base = data + (Line * B.y1) + (Bytes * B.x1);
	#if COPY_MODE
	if (d->Data.Reset(new uint8[B.Y() * RowSize]))
	{
		for (int y=0; y<B.Y(); y++)
		{
			auto i = Base + (Line * y);
			if (!i) continue;
			auto o = d->Data.Get() + (RowSize * y);
			memcpy(o, i, RowSize);
		}
	}
	#endif
	
	if (Bits <= 8)
	{
		int Entries = 1 << Bits;
		CGColorSpaceRef Rgb = CGColorSpaceCreateDeviceRGB();
		if (Rgb)
		{
			d->Cs = CGColorSpaceCreateIndexed(Rgb, Entries - 1, palette);
			CGColorSpaceRelease(Rgb);
		}
	}
	else
	{
		d->Cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	}
	
	if (d->Cs)
	{
		#if COPY_MODE
		d->Prov = CGDataProviderCreateWithData(this, d->Data.Get(), RowSize * B.Y(), ReleaseCGImg);
		#else
		d->Prov = CGDataProviderCreateWithData(this, Base, Line * B.Y(), ReleaseCGImg);
		#endif
		if (d->Prov)
		{
			d->Img = CGImageCreate
			(
				B.X(),
				B.Y(),
				Bits == 16 ? 5 : 8,
				Bits,
				#if COPY_MODE
			 	RowSize,
			 	#else
				ABS(Line),
			 	#endif
				d->Cs,
				Bits == 32 ? AlphaType : kCGImageAlphaNone,
				d->Prov,
				0,
				false,
				kCGRenderingIntentDefault
			);
			if (!d->Img)
			{
				printf("%s:%i - CGImageCreate(%i, %i, %i, %i, " LPrintfSSizeT ", ...) failed.\n",
					_FL,
					B.X(),
					B.Y(),
					Bits == 16 ? 5 : 8,
					Bits,
					Line
					);
			}
		}
		else
		{
			// printf("%s:%i - CGDataProviderCreateWithData failed.\n", _FL);
		}
	}
	else
	{
		printf("%s:%i - ColourSpace creation failed.\n", _FL);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
class LMemDCPrivate
{
public:
	uchar *Data;
	CGContextRef Bmp;
	CGColorSpaceRef Cs;
	LArray<LRect> Client;
	LAutoPtr<uchar, true> BitsMem;

	LMemDCPrivate()
	{
		Cs = NULL;
		Data = NULL;
		Bmp = NULL;
	}
	
	~LMemDCPrivate()
	{
		Empty();
	}
	
	void Empty()
	{
		if (Bmp)
		{
			CGContextRelease(Bmp);
			Bmp = 0;
		}
		
		if (Cs)
		{
			CGColorSpaceRelease(Cs);
			Cs = 0;
		}
		
		BitsMem.Reset();
		Data = NULL;
	}
};

LArray<LMemDC*> LMemDC::Instances;

LMemDC::LMemDC(const char *file, int line, int x, int y, LColourSpace cs, int flags)
{
	d = new LMemDCPrivate;
	pMem = 0;

	if (x && y && cs)
	{
		Create(x, y, cs, flags);
	}
}

LMemDC::LMemDC(const char *file, int line, LSurface *pDC)
{
	d = new LMemDCPrivate;
	pMem = 0;

	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetColourSpace()) )
	{
		if (pDC->Palette())
		{
			Palette(new LPalette(pDC->Palette()));
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
	Empty();
	DeleteObj(d);
}

void MemRelease(void * __nullable info, const void *  data, size_t size)
{
	uint8_t *p = (uint8_t*)data;
	DeleteArray(p);
}

#if LGI_COCOA
LMemDC::LMemDC(NSImage *img)
{
	d = new LMemDCPrivate;
	pMem = 0;

	if (img &&
		Create(img.size.width, img.size.height, System32BitColourSpace) )
	{
		NSPoint p = {0.0, 0.0};
		NSRect r = {{0.0, 0.0}, {img.size.width, img.size.height}};
		
		[img lockFocus];
		LAssert([NSGraphicsContext currentContext] != NULL);
		[img drawAtPoint:p fromRect:r operation:NSCompositingOperationCopy fraction:0.0];
		[img unlockFocus];
	}
}

NSImage *LMemDC::NsImage(LRect *rc)
{
	if (!pMem || !pMem->Base)
		return nil;

	LRect r;
	if (rc)
		r = *rc;
	else
		r = Bounds();
	
	size_t bitsPerComponent = 8;
	size_t bitsPerPixel = GetBits();
	size_t bytesPerRow = (bitsPerPixel * r.X() + 7) / bitsPerComponent;
	CGDataProviderRef provider = nil;
	
	LArray<uint8_t> Mem;
	LAutoPtr<LSurface> Sub(SubImage(r));
	if (!Sub)
		return nil;
	auto p = Sub->pMem;
	
	// Need to collect all the image data into one place.
	if (!Mem.Length(p->y * bytesPerRow))
		return nil;
	
	auto dst = Mem.AddressOf();
	for (int y=0; y<p->y; y++)
	{
		auto src = p->Base + (y * p->Line);
		LAssert(dst + bytesPerRow <= Mem.AddressOf() + Mem.Length());
		LAssert(src + bytesPerRow <= pMem->Base + (pMem->y * pMem->Line));
		
		memcpy(dst, src, bytesPerRow);
		dst += bytesPerRow;
	}

	auto len = Mem.Length();
	provider = CGDataProviderCreateWithData(NULL, Mem.Release(), len, MemRelease);
	
	CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
	// CLBitmapInfo bitmapInfo = kCLBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast;
	auto bitmapInfo = kCGBitmapByteOrderDefault | kCGImageAlphaLast;
	CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;

	CGImageRef iref = CGImageCreate(r.X(), r.Y(),
									bitsPerComponent,
									bitsPerPixel,
									bytesPerRow,
									colorSpaceRef,
									bitmapInfo,
									provider,   // data provider
									NULL,       // decode
									YES,        // should interpolate
									renderingIntent);

	auto img = [[NSImage alloc] initWithCGImage:iref size:NSMakeSize(r.X(), r.Y())];
	CGImageRelease(iref);
	return img;
}
#endif

void LMemDC::Empty()
{
	DeleteObj(pMem);
}

bool LMemDC::SupportsAlphaCompositing()
{
	return true;
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

CGImg *LMemDC::GetImg(LRect *Sub, int Debug)
{
	if (!pMem)
		return 0;

	uchar *rgb = pPalette ? (uchar*)((*pPalette)[0]) : 0;
	
	return new CGImg
				(
					pMem->x,
					pMem->y,
					pMem->GetBits(),
					pMem->Line,
					d->Data,
					rgb,
					Sub,
				 	Debug
				);
}

OsBitmap LMemDC::GetBitmap()
{
	return 0;
}

OsPainter LMemDC::Handle()
{
	return d->Bmp;
}

bool LMemDC::Lock()
{
	return true;
}

bool LMemDC::Unlock()
{
	return true;
}

bool LMemDC::Create(int x, int y, LColourSpace Cs, int Flags)
{
	bool Status = false;

	d->Empty();
	
	if (x > 0 && y > 0 && Cs != CsNone)
	{
        int Bits = LColourSpaceToBits(Cs);
		size_t LineLen = ((Bits * x + 31) / 32) * 4;
		if (Bits > 16)
		{
			d->Cs = CGColorSpaceCreateDeviceRGB();
			d->Bmp = CGBitmapContextCreate
				(
					NULL,
					x,
					y,
					8,
					0,
					d->Cs,
					Bits == 32 ? AlphaType : kCGImageAlphaNoneSkipLast
				);
			if (d->Bmp)
			{
				LineLen = CGBitmapContextGetBytesPerRow(d->Bmp);
				d->Data = (uint8*) CGBitmapContextGetData(d->Bmp);
			}
		}
		
		pMem = new LBmpMem;
		if (pMem)
		{
			pMem->Flags = 0;
			pMem->x = x;
			pMem->y = y;

			if (d->Bmp && d->Data)
			{
				pMem->Base = (uchar*)d->Data;
				pMem->Line = LineLen;

				switch (CGBitmapContextGetBitsPerPixel(d->Bmp))
				{
					case 24:
					case 32:
					{
						CGImageAlphaInfo ai = CGBitmapContextGetAlphaInfo(d->Bmp);
						switch (ai)
						{
							case kCGImageAlphaNone:
								pMem->Cs = CsRgb24;
								break;
							case kCGImageAlphaLast:               /* For example, non-premultiplied RGBA */
							case kCGImageAlphaNoneSkipLast:       /* For example, RGBX. */
								pMem->Cs = CsRgba32;
								break;
							case kCGImageAlphaPremultipliedLast:  /* For example, premultiplied RGBA */
								pMem->Cs = CsRgba32;
								pMem->Flags |= LBmpMem::BmpPreMulAlpha;
								break;
							case kCGImageAlphaFirst:              /* For example, non-premultiplied ARGB */
							case kCGImageAlphaNoneSkipFirst:      /* For example, XRGB. */
								pMem->Cs = CsArgb32;
								break;
							case kCGImageAlphaPremultipliedFirst: /* For example, premultiplied ARGB */
								pMem->Cs = CsArgb32;
								pMem->Flags |= LBmpMem::BmpPreMulAlpha;
								break;
							default:
								LAssert(0);
								break;
						}
						break;
					}
					default:
					{
						LAssert(0);
						break;
					}
				}
				
				if (pMem->Cs != Cs &&
					Flags == LSurface::SurfaceRequireExactCs)
				{
					// Surface type mismatch... throw away the system bitmap and allocate
					// the exact type just in our memory.
					d->Empty();
					pMem->Base = NULL;
				}
			}
			
			if (!pMem->Base)
			{
				pMem->Line = ((Bits * x + 31) / 32) * 4;
				pMem->Base = new uint8[pMem->Line * y];
				pMem->Flags |= LBmpMem::BmpOwnMemory;
				pMem->Cs = Cs;
			}
			
			ColourSpace = pMem->Cs;
			
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
			
			Status = true;
		}
	}
	
	return Status;
}

CGColorSpaceRef LMemDC::GetColourSpaceRef()
{
	return d->Cs;
}

void LMemDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
	if (!Src)
		return;

	if (Src->IsScreen())
	{
		CGRect r;
		if (a)
			r = *a;
		else
			r = Src->Bounds();

		CGImageRef Img = CGWindowListCreateImage(r, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
		if (Img)
		{
			LRect dr(0, 0, (int)CGImageGetWidth(Img)-1, (int)CGImageGetHeight(Img)-1);
			CGContextDrawImage(d->Bmp, dr, Img);
			CGImageRelease(Img);

			// Overlay any effects between the screen and cursor layers...
			OnCaptureScreen();

			// Do we need to capture the cursor as well?
			if (TestFlag(Flags, GDC_CAPTURE_CURSOR))
			{
				// Capture the cursor as well..
				NSImage *cursor = [[[NSCursor currentSystemCursor] image] copy];
				if (cursor)
				{
					#if LGI_COCOA
					#warning "Impl cursor blt"
					#else
					// NSPoint hotSpot = [[NSCursor currentSystemCursor] hotSpot];
					HIPoint p;
					HIGetMousePosition(kHICoordSpaceScreenPixel, NULL, &p);
					LRect msr(0, 0, (int)cursor.size.width-1, (int)cursor.size.height-1);
					msr.Offset(p.x - (int)r.origin.x, p.y - (int)r.origin.y);

					printf("msr=%s\n", msr.GetStr());
					CGContextDrawImage(d->Bmp, msr, [cursor CGImageForProposedRect: NULL context: NULL hints: NULL]);
					#endif
					
					[cursor release];
				}
			}
		}
		else printf("%s:%i - CGWindowListCreateImage failed.\n", _FL);
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
	if (y1 <= y2 &&
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

void LMemDC::SetOrigin(LPoint pt)
{
	LSurface::SetOrigin(pt);
}

void LMemDC::SetClient(LRect *c)
{
	if (c)
	{
		LRect Doc;
		if (d->Client.Length())
			Doc = d->Client.Last();
		else
			Doc = Bounds();
		
		LRect r = *c;
		//r.Offset(Doc.x1, Doc.y1);
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
