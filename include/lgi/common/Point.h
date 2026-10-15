/// 2d Point classes
#pragma once

#include <math.h>

class LgiClass LPoint
{
public:
	int x, y;

	LPoint()
	{
		x = 0;
		y = 0;
	}

	LPoint(int Ix, int Iy)
	{
		x = Ix;
		y = Iy;
	}

	LPoint(const LPoint &p)
	{
		x = p.x;
		y = p.y;
	}

	bool Inside(class LRect &r);
	
	LPoint operator +(const LPoint &p)
	{
		LPoint r;
		r.x = x + p.x;
		r.y = y + p.y;
		return r;
	}

	LPoint &operator +=(const LPoint &p)
	{
		x += p.x;
		y += p.y;
		return *this;
	}

	LPoint operator -(const LPoint &p)
	{
		LPoint r;
		r.x = x - p.x;
		r.y = y - p.y;
		return r;
	}

	LPoint &operator -=(const LPoint &p)
	{
		x -= p.x;
		y -= p.y;
		return *this;
	}

	LPoint &operator *=(int factor)
	{
		x *= factor;
		y *= factor;
		return *this;
	}

	LPoint &operator /=(int factor)
	{
		x /= factor;
		y /= factor;
		return *this;
	}

	bool operator ==(const LPoint &p)
	{
		return x == p.x && y == p.y;
	}

	bool operator !=(const LPoint &p)
	{
		return !(*this == p);
	}

	void Set(int X, int Y)
	{
		x = X;
		y = Y;
	}

	void Zero()
	{
		x = 0;
		y = 0;
	}

	LString GetStr()
	{
		LString s;
		s.Printf("%i,%i", x, y);
		return s;
	}

	bool SetStr(LString s)
	{
		auto p = s.SplitDelimit(" ,");
		if (p.Length() != 2)
			return false;
		x = (int)p[0].Int();
		y = (int)p[1].Int();
		return true;
	}
	
	#if HAIKU
	LPoint(const BPoint &p)
	{
		x = (int)p.x;
		y = (int)p.y;
	}
	
	operator BPoint() const
	{
		return BPoint(x, y);
	}
	#endif
};

/// 3d Point
class LgiClass LPoint3
{
public:
	int x, y, z;
};


class LgiClass LPointF
{
public:
	static double Threshold;
	float x, y;

	LPointF()
	{
		x = y = 0;
	}

	LPointF(float X, float Y)
	{
		x = X;
		y = Y;
	}

	LPointF(const LPointF &p)
	{
		x = p.x;
		y = p.y;
	}

	LPointF(const LPoint &p)
	{
		x = (float)p.x;
		y = (float)p.y;
	}

	LPointF &operator =(const LPointF &p)
	{
		x = p.x;
		y = p.y;
		return *this;
	}

	LPointF &operator -(const LPointF &p)
	{
		static LPointF Result;
		Result.x = x - p.x;
		Result.y = y - p.y;
		return Result;
	}

	LPointF &operator +(const LPointF &p)
	{
		static LPointF Result;
		Result.x = x + p.x;
		Result.y = y + p.y;
		return Result;
	}

	bool operator ==(const LPointF &p)
	{
		float dx = x - p.x;
		if (dx < 0) dx = -dx;
		float dy = y - p.y;
		if (dy < 0) dy = -dy;
		return dx<Threshold && dy<Threshold;
	}

	bool operator !=(const LPointF &p)
	{
		return !(*this == p);
	}

	void Set(float X, float Y)
	{
		x = X;
		y = Y;
	}
	
	LPointF &Translate(float tx, float ty)
	{
		x += tx;
		y += ty;
		return *this;
	}
	
	LPointF operator *(float scale)
	{
		LPointF p(x * scale, y * scale);
		return p;
	}

	LPointF &operator *=(float scale)
	{
		x *= scale;
		y *= scale;
		return *this;
	}

	// Angle in radians
	LPointF &Rotate(float a)
	{
		double ix = x;
		double iy = y;
		x = (float) ((cos(a)*ix) - (sin(a)*iy));
		y = (float) ((sin(a)*ix) + (cos(a)*iy));
		return *this;
	}
};
