#ifndef __GPath_h__
#define __GPath_h__

#include <math.h>
#include "GMatrix.h"

#define GPATH_DBG	1

class GSeg;
class GVector;

extern bool _Disable_ActiveList;
extern bool _Disable_XSort;
extern bool _Disable_Alpha;
extern bool _Disable_Rops;

enum GPathFillRule
{
	FILLRULE_ODDEVEN,
	FILLRULE_NONZERO
};

#include "LPoint.h"
#include "LRectF.h"

class LgiClass GBrush
{
	friend class GPath;

protected:
	uchar AlphaLut[65];

	class GRopArgs
	{
	public:
		uchar *Pixels;
		uchar *EndOfMem;
		uchar *Alpha;
		int Len;
		int x, y;

		GColourSpace Cs;
		int BytesPerPixel;

		GSurface *pDC;
	};

	virtual bool Start(GRopArgs &a) { return true; }
	virtual void Rop(GRopArgs &a) = 0;
	virtual int Alpha() { return 255; }

	void MakeAlphaLut();

public:
	GBrush() {}
	virtual ~GBrush() {}
};

class LgiClass GEraseBrush : public GBrush
{
	void Rop(GRopArgs &a);

public:
	GEraseBrush()
	{
		MakeAlphaLut();
	}
};

// In premultiplied:
//		Dca' = Sca + Dca.(1 - Sa)
// -or-
// In non-premultiplied:
//		Dc' = (Sc.Sa + Dc.Da.(1 - Sa))/Da'
#define NonPreMulOver16(c, sh)	d->c = (((s.c * sa) + (DivLut[(d->c << sh) * 255] * o)) / 255) >> sh
#define NonPreMulOver32(c)		d->c = ((s.c * sa) + (DivLut[d->c * da] * o)) / d->a
#define NonPreMulOver24(c)		d->c = ((s.c * sa) + (DivLut[d->c * 255] * o)) / 255
#define NonPreMulAlpha			d->a = (d->a + sa) - DivLut[d->a * sa]

// Define 'uint8 dc, sc;' first. Takes 16bit source and 8bit dest.
#define Rgb16to8PreMul(c)						\
	sc = DivLut[(s->c >> 8) * (s->a >> 8)];		\
	dc = DivLut[d->c * d->a];					\
	d->c = sc + DivLut[dc * o]

class LgiClass GSolidBrush : public GBrush
{
	COLOUR c32;

	void Rop(GRopArgs &a);
	int Alpha() { return A32(c32); }

public:
	GSolidBrush(COLOUR c)
	{
		c32 = c;
		MakeAlphaLut();
	}

	GSolidBrush(GColour c)
	{
		c32 = c.c32();
		MakeAlphaLut();
	}

	GSolidBrush(int r, int g, int b, int a = 255)
	{
		c32 = Rgba32(r, g, b, a);
		MakeAlphaLut();
	}

	template<typename T>
	void SolidRop16(T *d, int Len, uint8_t *Alpha, COLOUR c32)
	{
		uchar *DivLut = Div255Lut;
		T *end = d + Len;
		GRgb24 s;

		s.r = R32(c32);
		s.g = G32(c32);
		s.b = B32(c32);
		
		while (d < end)
		{
			uchar sa = AlphaLut[*Alpha++];
			if (sa == 255)
			{
				d->r = s.r >> 3;
				d->g = s.g >> 2;
				d->b = s.b >> 3;
			}
			else if (sa)
			{
				uchar o = 0xff - sa;
				NonPreMulOver16(r, 3);
				NonPreMulOver16(g, 2);
				NonPreMulOver16(b, 3);
			}

			d++;
		}
	}
	
	template<typename T>
	void SolidRop24(T *d, int Len, uint8_t *Alpha, COLOUR c32)
	{
		uchar *DivLut = Div255Lut;
		T *end = d + Len;
		T s;

		s.r = R32(c32);
		s.g = G32(c32);
		s.b = B32(c32);
		
		while (d < end)
		{
			uchar sa = AlphaLut[*Alpha++];
			if (sa == 255)
			{
				*d = s;
			}
			else if (sa)
			{
				uchar o = 0xff - sa;
				NonPreMulOver24(r);
				NonPreMulOver24(g);
				NonPreMulOver24(b);
			}

			d++;
		}
	}

	template<typename T>
	void SolidRop32(T *d, int Len, uint8_t *Alpha, COLOUR c32)
	{
		uchar *DivLut = Div255Lut;
		T *end = d + Len;
		T s;

		s.r = R32(c32);
		s.g = G32(c32);
		s.b = B32(c32);
		s.a = A32(c32);

		while (d < end)
		{
			uchar sa = DivLut[s.a * AlphaLut[*Alpha++]];
			if (sa == 255)
			{
				*d = s;
			}
			else if (sa)
			{
				int o = 0xff - sa;
				int da = d->a;
				NonPreMulAlpha;
				NonPreMulOver32(r);
				NonPreMulOver32(g);
				NonPreMulOver32(b);
			}

			d++;
		}
	}
};

struct GBlendStop
{
	double Pos;
	COLOUR c32;
};

class LgiClass GBlendBrush : public GBrush
{
protected:
	int Stops;
	GBlendStop *Stop;

	COLOUR Lut[256];
	double Base, IncX, IncY;

	LPointF p[2];

	bool Start(GRopArgs &a);

public:
	GBlendBrush(int stops, GBlendStop *stop)
	{
		MakeAlphaLut();
		Stops = 0;
		Stop = 0;
		if (stop)
		{
			SetStops(stops, stop);
		}
	}

	~GBlendBrush()
	{
		DeleteArray(Stop);
	}

	int GetStops(GBlendStop **stop = 0)
	{
		if (stop)
			*stop = Stop;
		return Stops;
	}

	void SetStops(int stops, GBlendStop *stop)
	{
		Stops = stops;
		DeleteArray(Stop);
		Stop = new GBlendStop[Stops];
		if (Stop) memcpy(Stop, stop, sizeof(*Stop) * Stops);
	}
};

class LgiClass GLinearBlendBrush : public GBlendBrush
{
	bool Start(GRopArgs &Args);
	void Rop(GRopArgs &Args);

public:
	GLinearBlendBrush(LPointF a, LPointF b, int stops = 0, GBlendStop *stop = 0) :
		GBlendBrush(stops, stop)
	{
		p[0] = a;
		p[1] = b;
	}

	template<typename T>
	void Linear16(T *d, GRopArgs &Args)
	{
		uchar *DivLut = Div255Lut;
		uchar *Alpha = Args.Alpha;

		double DPos = Base + (Args.x * IncX) + (Args.y * IncY);
		double Scale = (double)0x10000;
		int Pos = (int) (DPos * Scale);
		int Inc = (int) (IncX * Scale);
		
		T *e = d + Args.Len;
		GRgb24 s;

		while (d < e)
		{
			if (*Alpha)
			{
				// work out colour
				COLOUR c32;
				int Ci = ((Pos << 8) - Pos) >> 16;
				if (Ci < 0) c32 = Lut[0];
				else if (Ci > 255) c32 = Lut[255];
				else c32 = Lut[Ci];

				s.r = R32(c32);
				s.g = G32(c32);
				s.b = B32(c32);

				// assign pixel
				uchar sa = DivLut[AlphaLut[*Alpha] * A32(c32)];
				if (sa == 0xff)
				{
					d->r = s.r >> 3;
					d->g = s.g >> 2;
					d->b = s.b >> 3;
				}
				else if (sa)
				{
					uchar o = 0xff - sa;

					NonPreMulOver16(r, 3);
					NonPreMulOver16(g, 2);
					NonPreMulOver16(b, 3);
				}
			}

			d++;
			Alpha++;
			Pos += Inc;
		}
	}

	template<typename T>
	void Linear24(T *d, GRopArgs &Args)
	{
		uchar *DivLut = Div255Lut;
		uchar *Alpha = Args.Alpha;

		double DPos = Base + (Args.x * IncX) + (Args.y * IncY);
		double Scale = (double)0x10000;
		int Pos = (int) (DPos * Scale);
		int Inc = (int) (IncX * Scale);
		
		T *e = d + Args.Len;
		T s;

		while (d < e)
		{
			if (*Alpha)
			{
				// work out colour
				COLOUR c32;
				int Ci = ((Pos << 8) - Pos) >> 16;
				if (Ci < 0) c32 = Lut[0];
				else if (Ci > 255) c32 = Lut[255];
				else c32 = Lut[Ci];

				s.r = R32(c32);
				s.g = G32(c32);
				s.b = B32(c32);

				// assign pixel
				uchar sa = DivLut[AlphaLut[*Alpha] * A32(c32)];
				if (sa == 0xff)
				{
					*d = s;
				}
				else if (sa)
				{
					uchar o = 0xff - sa;

					NonPreMulOver24(r);
					NonPreMulOver24(g);
					NonPreMulOver24(b);
				}
			}

			d++;
			Alpha++;
			Pos += Inc;
		}
	}

	template<typename T>
	void Linear32(T *d, GRopArgs &Args)
	{
		uchar *DivLut = Div255Lut;
		uchar *Alpha = Args.Alpha;

		double DPos = Base + (Args.x * IncX) + (Args.y * IncY);
		double Scale = (double)0x10000;
		int Pos = (int) (DPos * Scale);
		int Inc = (int) (IncX * Scale);

		T *End = d + Args.Len;
		T s;

		while (d < End)
		{
			if (*Alpha)
			{
				// work out colour
				COLOUR c;
				int Ci = ((Pos << 8) - Pos) >> 16;
				if (Ci < 0) c = Lut[0];
				else if (Ci > 255) c = Lut[255];
				else c = Lut[Ci];

				s.r = R32(c);
				s.g = G32(c);
				s.b = B32(c);
				s.a = A32(c);

				// assign pixel
				uchar sa = DivLut[AlphaLut[*Alpha] * A32(c)];
				if (sa == 0xff)
				{
					*d = s;
				}
				else if (sa)
				{
					uchar o = 0xff - sa;
					int da = d->a;

					NonPreMulAlpha;
					NonPreMulOver32(r);
					NonPreMulOver32(g);
					NonPreMulOver32(b);
				}
			}

			d++;
			Alpha++;
			Pos += Inc;
		}
	}
};

class LgiClass GRadialBlendBrush : public GBlendBrush
{
	void Rop(GRopArgs &Args);

public:
	GRadialBlendBrush(LPointF center, LPointF rim, int stops = 0, GBlendStop *stop = 0) :
		GBlendBrush(stops, stop)
	{
		p[0] = center;
		p[1] = rim;
	}

	template<typename T>
	void Radial16(T *d, GRopArgs &Args)
	{
		uchar *DivLut = Div255Lut;
		uchar *Alpha = Args.Alpha;

		double dx = p[1].x - p[0].x;
		double dy = p[1].y - p[0].y;
		double Radius = sqrt((dx*dx)+(dy*dy)) / 255; // scaled by 255 to index into the Lut

		int dysq = (int) (Args.y - p[0].y);
		dysq *= dysq;

		int x = (int) (Args.x - p[0].x);
		int dxsq = x * x;	// forward difference the (x * x) calculation
		int xd = 2 * x + 1;	// to remove a MUL from the inner loop

		int Ci = 0;
		COLOUR c32;
		uchar sa;
		T *End = (T*) d + Args.Len;
		GRgb24 s;

		if (Radius < 0.0000000001)
		{
			// No blend... just the end colour
			c32 = Lut[255];
			s.r = R32(c32);
			s.g = G32(c32);
			s.b = B32(c32);

			for (; d<End; Alpha++)
			{
				if (*Alpha)
				{
					// composite the pixel
					sa = DivLut[AlphaLut[*Alpha] * A32(c32)];
					if (sa == 0xff)
					{
						d->r = s.r >> 3;
						d->g = s.g >> 2;
						d->b = s.b >> 3;
					}
					else if (sa)
					{
						uchar o = 0xff - sa;

						NonPreMulOver16(r, 3);
						NonPreMulOver16(g, 2);
						NonPreMulOver16(b, 3);
					}
				}
				
				d++;
			}
		}
		else
		{
			// Radial blend
			for (; d<End; Alpha++)
			{
				if (*Alpha)
				{
					// work out colour
					
					// sqrt??? gee can we get rid of this?
					Ci = (int) (sqrt((double) (dxsq+dysq)) / Radius);

					// clamp colour to the ends of the Lut
					if (Ci < 0) c32 = Lut[0];
					else if (Ci > 255) c32 = Lut[255];
					else c32 = Lut[Ci];

					s.r = R32(c32);
					s.g = G32(c32);
					s.b = B32(c32);

					// composite the pixel
					sa = DivLut[AlphaLut[*Alpha] * A32(c32)];
					if (sa == 0xff)
					{
						d->r = s.r >> 3;
						d->g = s.g >> 2;
						d->b = s.b >> 3;
					}
					else if (sa)
					{
						uchar o = 0xff - sa;

						NonPreMulOver16(r, 3);
						NonPreMulOver16(g, 2);
						NonPreMulOver16(b, 3);
					}
				}

				// apply forward difference to the x values
				x++;
				dxsq += xd;
				xd += 2;
				d++;
			}
		}
	}

	template<typename T>
	void Radial24(T *d, GRopArgs &Args)
	{
		uchar *DivLut = Div255Lut;
		uchar *Alpha = Args.Alpha;

		double dx = p[1].x - p[0].x;
		double dy = p[1].y - p[0].y;
		double Radius = sqrt((dx*dx)+(dy*dy)) / 255; // scaled by 255 to index into the Lut

		int dysq = (int) (Args.y - p[0].y);
		dysq *= dysq;

		int x = (int) (Args.x - p[0].x);
		int dxsq = x * x;	// forward difference the (x * x) calculation
		int xd = 2 * x + 1;	// to remove a MUL from the inner loop

		int Ci = 0;
		COLOUR c32;
		uchar sa;
		T *End = (T*) d + Args.Len;
		T s;

		if (Radius < 0.0000000001)
		{
			// No blend... just the end colour
			c32 = Lut[255];
			s.r = R32(c32);
			s.g = G32(c32);
			s.b = B32(c32);

			for (; d<End; Alpha++)
			{
				if (*Alpha)
				{
					// composite the pixel
					sa = DivLut[AlphaLut[*Alpha] * A32(c32)];
					if (sa == 0xff)
					{
						*d = s;
					}
					else if (sa)
					{
						uchar o = 0xff - sa;

						NonPreMulOver24(r);
						NonPreMulOver24(g);
						NonPreMulOver24(b);
					}
				}
				
				d++;
			}
		}
		else
		{
			// Radial blend
			for (; d<End; Alpha++)
			{
				if (*Alpha)
				{
					// work out colour
					
					// sqrt??? gee can we get rid of this?
					Ci = (int) (sqrt((double) (dxsq+dysq)) / Radius);

					// clamp colour to the ends of the Lut
					if (Ci < 0) c32 = Lut[0];
					else if (Ci > 255) c32 = Lut[255];
					else c32 = Lut[Ci];

					s.r = R32(c32);
					s.g = G32(c32);
					s.b = B32(c32);

					// composite the pixel
					sa = DivLut[AlphaLut[*Alpha] * A32(c32)];
					if (sa == 0xff)
					{
						*d = s;
					}
					else if (sa)
					{
						uchar o = 0xff - sa;

						NonPreMulOver24(r);
						NonPreMulOver24(g);
						NonPreMulOver24(b);
					}
				}

				// apply forward difference to the x values
				x++;
				dxsq += xd;
				xd += 2;
				d++;
			}
		}
	}

	template<typename T>
	void Radial32(T *d, GRopArgs &Args)
	{
		uchar *DivLut = Div255Lut;
		uchar *Alpha = Args.Alpha;

		double dx = p[1].x - p[0].x;
		double dy = p[1].y - p[0].y;
		double Radius = sqrt((dx*dx)+(dy*dy)) / 255; // scaled by 255 to index into the Lut

		int dysq = (int) (Args.y - p[0].y);
		dysq *= dysq;

		int x = (int) (Args.x - p[0].x);
		int dxsq = x * x;	// forward difference the (x * x) calculation
		int xd = 2 * x + 1;	// to remove a MUL from the inner loop

		int Ci = 0;
		COLOUR c;
		uchar sa;
		T *End = d + Args.Len;
		T s;

		if (Radius < 0.0000000001)
		{
			// No blend... just the end colour
			c = Lut[255];
			s.r = R32(c);
			s.g = G32(c);
			s.b = B32(c);
			s.a = A32(c);

			for (; d<End; d++, Alpha++)
			{
				if (*Alpha)
				{
					// composite the pixel
					sa = DivLut[AlphaLut[*Alpha] * A32(c)];
					if (sa == 0xff)
					{
						*d = s;
					}
					else if (sa)
					{
						uchar o = 0xff - sa;
						int da = d->a;

						NonPreMulAlpha;
						NonPreMulOver32(r);
						NonPreMulOver32(g);
						NonPreMulOver32(b);
					}
				}
			}
		}
		else
		{
			// Radial blend
			for (; d<End; d++, Alpha++)
			{
				if (*Alpha)
				{
					// work out colour
					
					// sqrt??? gee can we get rid of this?
					Ci = (int) (sqrt((double) (dxsq+dysq)) / Radius);

					// clamp colour to the ends of the Lut
					if (Ci < 0) c = Lut[0];
					else if (Ci > 255) c = Lut[255];
					else c = Lut[Ci];

					s.r = R32(c);
					s.g = G32(c);
					s.b = B32(c);
					s.a = A32(c);

					// composite the pixel
					sa = DivLut[AlphaLut[*Alpha] * A32(c)];
					if (sa == 0xff)
					{
						*d = s;
					}
					else if (sa)
					{
						uchar o = 0xff - sa;
						int da = d->a;

						NonPreMulAlpha;
						NonPreMulOver32(r);
						NonPreMulOver32(g);
						NonPreMulOver32(b);
					}
				}

				// apply forward difference to the x values
				x++;
				dxsq += xd;
				xd += 2;
			}
		}
	}
};

class LgiClass GPath
{
public:
	class Matrix : public GMatrix<double, 3, 3>
	{
	public:
		Matrix()
		{
			SetIdentity();
		}
		
		Matrix &Translate(double x, double y)
		{
			m[2][0] = x;
			m[2][1] = y;
			return *this;
		}
		
		Matrix &Rotate(double angle, bool anti_clockwise = false)
		{
			m[0][0] = m[1][1] = cos(angle);
			m[0][1] = (anti_clockwise ? -1 : 1) * sin(angle);
			m[1][0] = (anti_clockwise ? 1 : -1) * sin(angle);
			return *this;
		}
	};

protected:
	// Data
	List<GSeg> Segs;
	List<GVector> Vecs;
	LRectF Bounds;
	bool Aa;
	GPathFillRule FillRule;
	Matrix Mat;
	
	// Flattened representation
	int Points;
	GArray<int> Outline;
	LPointF *Point;

	// Methods
	void Unflatten();
	void Append(GPath &p);

public:
	GPath(bool aa = true);
	virtual ~GPath();

	// Primitives
	void MoveTo(		double x,
						double y);
	void MoveTo(		LPointF &pt);
	void LineTo(		double x,
						double y);
	void LineTo(		LPointF &pt);
	void QuadBezierTo(	double cx, double cy,
						double px, double py);
	void QuadBezierTo(	LPointF &c,
						LPointF &p);
	void CubicBezierTo(	double c1x, double c1y,
						double c2x, double c2y,
						double px, double py);
	void CubicBezierTo(	LPointF &c1,
						LPointF &c2,
						LPointF &p);
	void Rectangle(		double x1, double y1,
						double x2, double y2);
	void Rectangle(		LPointF &tl,
						LPointF &rb);
	void Rectangle(		LRectF &r);
	void RoundRect(		LRectF &b, double r);
	void Circle(		double cx,
						double cy,
						double radius);
	void Circle(		LPointF &c,
						double radius);
	void Ellipse(		double cx,
						double cy,
						double x,
						double y);
	void Ellipse(		LPointF &c,
						double x,
						double y);
	bool Text(			GFont *Font,
						double x,
						double y,
						char *Utf8,
						int Bytes = -1);

	// Properties
	int Segments();
	void GetBounds(LRectF *b);
	GPathFillRule GetFillRule() { return FillRule; }
	void SetFillRule(GPathFillRule r) { FillRule = r; }

	// Methods
	bool IsClosed();
	void Close();

	bool IsFlattened();
	bool Flatten();

	bool IsEmpty();
	void Empty();

	void Transform(Matrix &m);
	void DeleteSeg(int i);

	// Colouring (windows: need to call ConvertPreMulAlpha(true) on the pDC after using these)
	void Fill(GSurface *pDC, GBrush &Brush);
	void Stroke(GSurface *pDC, GBrush &Brush, double Width);

	#if GPATH_DBG
	GMemDC DbgDsp;
	#endif
};

void FlattenQuadratic(LPointF *&Out, LPointF &p1, LPointF &p2, LPointF &p3, int Steps);
void FlattenCubic(LPointF *&Out, LPointF &p1, LPointF &p2, LPointF &p3, LPointF &p4, int Steps);

#endif
