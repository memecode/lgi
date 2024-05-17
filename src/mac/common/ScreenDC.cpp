/// \file
/// \author Matthew Allen
#include <stdio.h>
#include <math.h>

#include "lgi/common/Lgi.h"

class LScreenPrivate
{
public:
	CGContextRef Ctx;
	LWindow *Wnd;
	LView *View;
	LRect Rc;
	LArray<LRect> Stack;
	LColour c;
	int Bits;
	int Op;
	NativeInt ConstAlpha;
	int Clipped;
	
	void Init()
	{
		Op = GDC_ALPHA;
		Ctx = nil;
		Wnd = NULL;
		View = NULL;
		Bits = GdcD->GetBits();
		View = 0;
		Rc.ZOff(-1, -1);
		Clipped = 0;
		ConstAlpha = 255;
	}

	#if LGI_COCOA
	void SetContext(LView *v)
	{
		Rc = v->GetClient();
		Ctx = [NSGraphicsContext currentContext].CGContext;
		if (Ctx)
		{
			CGAffineTransform flipVertical = CGAffineTransformMake(1, 0, 0, -1, 0, Rc.Y());
			CGContextConcatCTM(Ctx, flipVertical);
		}
	}
	#endif
	
	LScreenPrivate()
	{
		Init();
	}
	
	LScreenPrivate(LWindow *w, void *param = NULL)
	{
		Init();
		Wnd = w;
		#if LGI_COCOA
		SetContext(w);
		#else
		Ctx = (CGContextRef)param;
		if (Ctx && Wnd)
		{
			Rect r;
			if (GetWindowBounds(Wnd->WindowHandle(), kWindowContentRgn, &r))
			{
				printf("%s:%i - GetWindowBounds failed\n", _FL);
			}
			else
			{
				Rc = r;
				Rc.Offset(-Rc.x1, -Rc.y1);
			}
			
			// CGContextTranslateCTM (d->Ctx, 0, d->Rc.Y()-1); 
			// CGContextScaleCTM (d->Ctx, 1.0, -1.0); 
			CGContextSetLineWidth(Ctx, 1.0);
		}
		else printf("%s:%i - No context.\n", __FILE__, __LINE__);
		#endif
	}

	LScreenPrivate(LView *v, void *param = NULL)
	{
		Init();
		View = v;
		#if LGI_COCOA
		SetContext(v);
		#else
		Ctx = (CGContextRef)param;
		if (Ctx)
		{
			HIRect r;
			if (View && !HIViewGetBounds(View->Handle(), &r))
			{
				Rc.ZOff((int)r.size.width, (int)r.size.height);
			}
			else
			{
				// dock icon
				Rc.ZOff(127, 127);
			}
			
			CGContextSetLineWidth(Ctx, 1.0);
		}
		else printf("%s:%i - No context.\n", _FL);
		#endif
	}
	
	LRect Client()
	{
		LRect r = Rc;
		r.Offset(-r.x1, -r.y1);
		return r;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
LScreenDC::LScreenDC()
{
	d = new LScreenPrivate;
	ColourSpace = System32BitColourSpace;
}

LScreenDC::LScreenDC(LPrintDcParams *Params)
{
	d = new LScreenPrivate;
	ColourSpace = System32BitColourSpace;

	#if LGI_COCOA
	#else
	d->Ctx = Params->Ctx;
	if (d->Ctx)
	{
		d->Rc.x1 = Params->Page.left * Params->Dpi.hRes;
		d->Rc.y1 = Params->Page.top * Params->Dpi.vRes;
		d->Rc.x2 = Params->Page.right * Params->Dpi.hRes;
		d->Rc.y2 = Params->Page.bottom * Params->Dpi.vRes;
		
		CGContextSetLineWidth(d->Ctx, 1.0);
	}
	#endif
}

LScreenDC::LScreenDC(LWindow *w, void *param)
{
	d = new LScreenPrivate(w, param);
	ColourSpace = System32BitColourSpace;
	d->Wnd = w;
}

LScreenDC::LScreenDC(LView *v, void *param)
{
	d = new LScreenPrivate(v, param);
	ColourSpace = System32BitColourSpace;
}

LScreenDC::~LScreenDC()
{
	DeleteObj(d);
}

LString LScreenDC::Dump()
{
	auto Ctx = Handle();
	CGAffineTransform t = CGContextGetCTM(Ctx);
	LRect cr = CGContextGetClipBoundingBox(Ctx);
	LString s;
	s.Printf("ScreenDC transform=%g,%g,%g,%g-%g,%g clip=%s",
			t.a, t.b, t.c, t.d, t.tx, t.ty,
			cr.GetStr());
	return s;
}

bool LScreenDC::SupportsAlphaCompositing()
{
	return true;
}

LView *LScreenDC::GetView()
{
	return d->View;
}

LPoint LScreenDC::GetDpi()
{
	return LScreenDpi();
}

void LScreenDC::PushState()
{
	if (d->Ctx)
        CGContextSaveGState(d->Ctx);
}

void LScreenDC::PopState()
{
	if (d->Ctx)
        CGContextRestoreGState(d->Ctx);
}

bool LScreenDC::GetClient(LRect *c)
{
	if (!c)
		return false;
	
	*c = d->Rc;
	return true;
}

// bool SetClientDebug = false;

void LScreenDC::SetClient(LRect *c)
{
	// 'c' is in absolute coordinates
	if (d->Ctx)
	{
		if (c)
		{
			CGContextSaveGState(d->Ctx);

			d->Stack.Add(d->Rc);
			d->Rc = *c;

			#if 0
			CGAffineTransform t1 = CGContextGetCTM(d->Ctx);
			CGRect old_bounds = CGContextGetClipBoundingBox(d->Ctx);
			#endif
			
			CGContextTranslateCTM(d->Ctx, c->x1, c->y1);
			CGRect rect = {0};
			rect.size.width = c->X();
			rect.size.height = c->Y();
			CGContextClipToRect(d->Ctx, rect);

			#if 0
			{
				CGAffineTransform t2 = CGContextGetCTM(d->Ctx);

				LRect r1, r2;
				r1 = old_bounds;
				CGRect new_bounds = CGContextGetClipBoundingBox(d->Ctx);
				r2 = new_bounds;
				printf("SetClient(%s) %s (%f, %f) -> %s (%f, %f)\n",
						c->GetStr(),
						r1.GetStr(),
						t1.tx, t1.ty,
						r2.GetStr(),
						t2.tx, t2.ty
						);
				if (r1.x2 == 12 && r1.y2 == 12)
				{
					int asd=0;
				}
			}
			#endif
		}
		else
		{
			d->Rc = d->Stack.Last();
			d->Stack.PopLast();
			CGContextRestoreGState(d->Ctx);
		}
		
	}
	else printf("%s:%i - No context?\n", _FL);
}

int LScreenDC::GetFlags()
{
	return 0;
}

OsPainter LScreenDC::Handle()
{
	return d->Ctx;
}

LPoint LScreenDC::GetOrigin()
{
	return LSurface::GetOrigin();
}

void LScreenDC::SetOrigin(LPoint pt)
{
	int Ox = OriginX;
	int Oy = OriginY;

	LSurface::SetOrigin(pt);

	if (d->Ctx)
	{
		CGContextTranslateCTM(d->Ctx,
			Ox - pt.x,
			Oy - pt.y
			);
	}
}

LPalette *LScreenDC::Palette()
{
	return LSurface::Palette();
}

void LScreenDC::Palette(LPalette *pPal, bool bOwnIt)
{
	LSurface::Palette(pPal, bOwnIt);
}

LRect LScreenDC::ClipRgn(LRect *Rgn)
{
	LRect Prev = Clip;

	if (Rgn)
	{
		LRect c = *Rgn;
		auto Client = d->Client();
		Client.Offset(OriginX, OriginY);
		c.Bound(&Client);
		
		CGContextSaveGState(d->Ctx);
		CGRect rect = {{(float)c.x1, (float)c.y1}, {(float)c.X(), (float)c.Y()}};
		CGContextClipToRect(d->Ctx, rect);
		d->Clipped++;
	}
	else if (d->Clipped > 0)
	{
		CGContextRestoreGState(d->Ctx);			
		d->Clipped--;
	}

	return Prev;
}

LRect LScreenDC::ClipRgn()
{
	return Clip;
}

LColour LScreenDC::Colour(LColour c)
{
	LColour Prev = d->c;

	d->c = c;

	if (d->Ctx)
	{
		float r = (float)d->c.r()/255.0;
		float g = (float)d->c.g()/255.0;
		float b = (float)d->c.b()/255.0;
		float a = (float)d->c.a()/255.0;
		
		CGContextSetRGBFillColor(d->Ctx, r, g, b, a);
		CGContextSetRGBStrokeColor(d->Ctx, r, g, b, a);
	}

	return Prev;
}

COLOUR LScreenDC::Colour(COLOUR c, int Bits)
{
	LColour Prev = d->c;

	d->c.Set(c, Bits ? Bits : GetBits());
	if (d->Ctx)
	{
		float r = (float)d->c.r()/255.0;
		float g = (float)d->c.g()/255.0;
		float b = (float)d->c.b()/255.0;
		float a = (float)d->c.a()/255.0;
		
		CGContextSetRGBFillColor(d->Ctx, r, g, b, a);
		CGContextSetRGBStrokeColor(d->Ctx, r, g, b, a);
	}

	return CBit(d->Bits, Prev.c32(), 32);
}

COLOUR LScreenDC::Colour()
{
	return CBit(d->Bits, d->c.c32(), 32);
}

int LScreenDC::Op()
{
	return d->Op;
}

int LScreenDC::Op(int op, NativeInt Param)
{
	int Old = d->Op;
	d->Op = op;
	d->ConstAlpha = Param;
	return Old;
}

int LScreenDC::X()
{
	return d->Rc.X();
}

int LScreenDC::Y()
{
	return d->Rc.Y();
}

int LScreenDC::GetBits()
{
	return d->Bits;
}

void LScreenDC::Set(int x, int y)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)x, (float)y}, {1.0, 1.0}};
		CGContextFillRect(d->Ctx, r);
	}
}

COLOUR LScreenDC::Get(int x, int y)
{
	return 0;
}

uint LScreenDC::LineStyle(uint32_t Bits, uint32_t Reset)
{
	return LineBits;
}

uint LScreenDC::LineStyle()
{
	return LineBits;
}

void LScreenDC::HLine(int x1, int x2, int y)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)x1, (float)y}, {x2-x1+1.0f, 1.0f}};
		CGContextFillRect(d->Ctx, r);
	}
}

void LScreenDC::VLine(int x, int y1, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)x, (float)y1}, {1.0f, y2-y1+1.0f}};
		CGContextFillRect(d->Ctx, r);
	}
}

void LScreenDC::Line(int x1, int y1, int x2, int y2)
{
	if (d->Ctx)
	{
		if (y1 == y2)
		{
			// Horizontal
			if (x2 < x1)
			{
				int i = x1;
				x1 = x2;
				x2 = i;
			}
			CGRect r = {{(float)x1, (float)y1}, {x2-x1+1.0f, 1.0f}};
			CGContextFillRect(d->Ctx, r);
		}
		else if (x1 == x2)
		{
			// Vertical
			if (y2 < y1)
			{
				int i = y1;
				y1 = y2;
				y2 = i;
			}
			CGRect r = {{(float)x1, (float)y1}, {1.0f, y2-y1+1.0f}};
			CGContextFillRect(d->Ctx, r);
		}
		else
		{
			CGPoint p[] = {{x1+0.5f, y1+0.5f}, {x2+0.5f, y2+0.5f}};
			CGContextBeginPath(d->Ctx);
			CGContextAddLines(d->Ctx, p, 2);
			CGContextStrokePath(d->Ctx);
		}
	}
}

void LScreenDC::Circle(double cx, double cy, double radius)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)(cx-radius), (float)(cy-radius)}, {(float)radius*2.0f, (float)radius*2.0f}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextStrokePath(d->Ctx);
	}
}

void LScreenDC::FilledCircle(double cx, double cy, double radius)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)(cx-radius), (float)(cy-radius)}, {(float)radius*2.0f, (float)radius*2.0f}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextFillPath(d->Ctx);
	}
}

void LScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	if (d->Ctx)
	{
		CGContextBeginPath(d->Ctx);
		CGContextAddArc(d->Ctx, cx, cy, radius, LGI_DegToRad(start), LGI_DegToRad(end), true);
		CGContextStrokePath(d->Ctx);
	}
}

void LScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	if (d->Ctx)
	{
		CGContextBeginPath(d->Ctx);
		CGContextAddArc(d->Ctx, cx, cy, radius, LGI_DegToRad(start), LGI_DegToRad(end), true);
		CGContextFillPath(d->Ctx);
	}
}

void LScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)(cx-x), (float)(cy-y)}, {(float)x*2.0f, (float)y*2.0f}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextStrokePath(d->Ctx);
	}
}

void LScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)(cx-x), (float)(cy-y)}, {(float)(x*2.0), (float)(y*2.0)}};
		CGContextBeginPath(d->Ctx);
		CGContextAddEllipseInRect(d->Ctx, r);
		CGContextFillPath(d->Ctx);
	}
}

void LScreenDC::Box(int x1, int y1, int x2, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)(x1+0.5), (float)(y1+0.5)}, {(float)(x2-x1), (float)(y2-y1)}};
		CGContextSetLineWidth(d->Ctx, 1.0);
		CGContextStrokeRect(d->Ctx, r);
	}
}

void LScreenDC::Box(LRect *a)
{
	if (d->Ctx)
	{
		LRect in;
		if (a)
			in = *a;
		else
			in.ZOff(X()-2, Y()-2);
	
		CGRect r = {{(float)(in.x1+0.5), (float)(in.y1+0.5)}, {(float)(in.x2-in.x1), (float)(in.y2-in.y1)}};
		CGContextSetLineWidth(d->Ctx, 1.0);
		CGContextStrokeRect(d->Ctx, r);
	}
}

void LScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	if (d->Ctx)
	{
		CGRect r = {{(float)x1, (float)y1}, {(float)(x2-x1+1.0), (float)(y2-y1+1.0)}};
		CGContextFillRect(d->Ctx, r);
	}
}

void LScreenDC::Rectangle(LRect *a)
{
	if (d->Ctx)
	{
		LRect c;
		if (!a)
		{
			c = d->Client();
			a = &c;
		}

		CGRect r = {{(float)(a->x1), (float)(a->y1)}, {(float)(a->x2-a->x1+1.0), (float)(a->y2-a->y1+1.0)}};
		CGContextFillRect(d->Ctx, r);
	}
}

void LScreenDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
	if (Src && d->Ctx)
	{
		LRect b;
		if (a)
		{
			b = *a;
			LRect r = Src->Bounds();
			b.Bound(&r);
		}
		else
		{
			b = Src->Bounds();
		}
		
		if (b.Valid())
		{
			if (Src->IsScreen())
			{
				// Scroll region...
			}
			else
			{
				// Blt mem->screen
				#if !LGI_COCOA
				OSStatus err = noErr;
				#endif
				LMemDC *Mem = dynamic_cast<LMemDC*>(Src);
				if (Mem)
				{
					static int count = 0;

					CGImg *i = Mem->GetImg(a ? &b : 0, count==26);
					if (i)
					{
						#if LGI_COCOA
							CGRect r;
							r.origin.x = x;
							r.origin.y = -y;
							r.size.width = b.X();
							r.size.height = b.Y();
							CGImageRef Img = *i;
						
							CGContextSaveGState(d->Ctx);
							CGContextTranslateCTM(d->Ctx, 0.0f, b.Y());
							CGContextScaleCTM(d->Ctx, 1.0f, -1.0f);

							bool HasConstAlpha = d->ConstAlpha >= 0 && d->ConstAlpha < 255;
							if (HasConstAlpha)
								CGContextSetAlpha(d->Ctx, d->ConstAlpha / 255.0);

							CGContextDrawImage(d->Ctx, r, Img);
							count++;
						
							CGContextRestoreGState(d->Ctx);
						#else
							HIRect r;
							r.origin.x = x;
							r.origin.y = y;
							r.size.width = b.X();
							r.size.height = b.Y();
							CGImageRef Img = *i;
						
							bool HasConstAlpha = d->ConstAlpha >= 0 && d->ConstAlpha < 255;
							if (HasConstAlpha)
								CGContextSetAlpha(d->Ctx, d->ConstAlpha / 255.0);
						
							err = HIViewDrawCGImage(d->Ctx, &r, Img);

							if (HasConstAlpha)
								CGContextSetAlpha(d->Ctx, 1.0);
						#endif
						
						// CGImg manages it's own deletion via callback.
						i->Release();
					}
					
					#if !LGI_COCOA
					if (err < 0)
					{
						LMemDC Tmp(b.X(), b.Y(), GdcD->GetColourSpace());
						Tmp.Blt(0, 0, Mem, &b);

						CGImg *i = Tmp.GetImg(a ? &b : 0);
						if (i)
						{
							HIRect r;
							r.origin.x = x;
							r.origin.y = y;
							r.size.width = b.X();
							r.size.height = b.Y();
							CGImageRef Img = *i;
							
							bool HasConstAlpha = d->ConstAlpha >= 0 && d->ConstAlpha < 255;
							if (HasConstAlpha)
								CGContextSetAlpha(d->Ctx, d->ConstAlpha / 255.0);
						 
							err = HIViewDrawCGImage(d->Ctx, &r, Img);

							if (HasConstAlpha)
								CGContextSetAlpha(d->Ctx, 1.0);
							
							// CGImg manages it's own deletion via callback.
							i->Release();
						}
					}
					#endif
				}
			}
		}
	}
}

void LScreenDC::StretchBlt(LRect *dst, LSurface *Src, LRect *s)
{
	if (Src)
	{
		LRect DestR;
		if (dst)
		{
			DestR = *dst;
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

void LScreenDC::Polygon(int Points, LPoint *Data)
{
}

void LScreenDC::Bezier(int Threshold, LPoint *Pt)
{
}

void LScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, LRect *r)
{
}

