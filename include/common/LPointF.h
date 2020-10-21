#pragma once

#include <math.h>

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

	LPointF(const GdcPt2 &p)
	{
		x = p.x;
		y = p.y;
	}

	LPointF &operator =(LPointF &p)
	{
		x = p.x;
		y = p.y;
		return *this;
	}

	LPointF &operator -(LPointF &p)
	{
		static LPointF Result;
		Result.x = x - p.x;
		Result.y = y - p.y;
		return Result;
	}

	LPointF &operator +(LPointF &p)
	{
		static LPointF Result;
		Result.x = x + p.x;
		Result.y = y + p.y;
		return Result;
	}

	bool operator ==(LPointF &p)
	{
		double dx = x - p.x;
		if (dx < 0) dx = -dx;
		double dy = y - p.y;
		if (dy < 0) dy = -dy;
		return dx<Threshold && dy<Threshold;
	}

	bool operator !=(LPointF &p)
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

