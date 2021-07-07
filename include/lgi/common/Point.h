/// 2d Point classes
#pragma once

#include <math.h>

class LgiClass LPoint
{
public:
	int x, y;

	LPoint(int Ix = 0, int Iy = 0)
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
	double x, y;

	LPointF()
	{
		x = y = 0;
	}

	LPointF(double X, double Y)
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
		x = p.x;
		y = p.y;
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
		double dx = x - p.x;
		if (dx < 0) dx = -dx;
		double dy = y - p.y;
		if (dy < 0) dy = -dy;
		return dx<Threshold && dy<Threshold;
	}

	bool operator !=(const LPointF &p)
	{
		return !(*this == p);
	}

	void Set(double X, double Y)
	{
		x = X;
		y = Y;
	}
	
	LPointF &Translate(double tx, double ty)
	{
		x += tx;
		y += ty;
		return *this;
	}

	// Angle in radians
	LPointF &Rotate(double a)
	{
		double ix = x;
		double iy = y;
		x = (cos(a)*ix) - (sin(a)*iy);
		y = (sin(a)*ix) + (cos(a)*iy);
		return *this;
	}
};
