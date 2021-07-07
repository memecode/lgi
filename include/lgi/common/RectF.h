#pragma once

class LgiClass LRectF
{
public:
	double x1, y1, x2, y2;
	bool Defined;

	LRectF()
	{
		Defined = false;
	}

	LRectF(LRect &r)
	{
		*this = r;
	}

	LRectF(double X1, double Y1, double X2, double Y2)
	{
		x1 = X1; y1 = Y1; x2 = X2; y2 = Y2;
		Defined = true;
	}

	void Set(double X1, double Y1, double X2, double Y2)
	{
		x1 = X1; y1 = Y1; x2 = X2; y2 = Y2;
		Defined = true;
	}

	LRectF(LPointF &a, LPointF &b)
	{
		x1 = a.x; y1 = a.y; x2 = b.x; y2 = b.y;
		Defined = true;
	}

	double X() { return x2 - x1; }
	double Y() { return y2 - y1; }
	bool IsNormal() { return x2 >= x1 && y2 >= y1; }
	bool Valid() { return Defined && IsNormal(); }
	
	void Normalize();
	void Union(LPointF &p);
	void Union(LRectF &p);
	void Intersect(LRectF &p);
	bool Overlap(LPointF &p);
	bool Overlap(LRectF &p);
	void Offset(double x, double y);
	void Size(double dx, double dy);
	char *Describe();

	LRectF &operator =(LRect &f);
	LRectF &operator =(LRectF &f);
	LRectF &operator =(LPointF &p);
};

