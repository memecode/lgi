#pragma once

#include <atomic>
#include <math.h>
#include <stdio.h>

class LgiClass LRectF
{
public:
	float x1, y1, x2, y2;
	bool init;

	LRectF()
	{
		x1 = y1 = x2 = y2 = 0.0f;
		init = false;
	}

	LRectF(LRect &r)
	{
		*this = r;
	}

	LRectF(float X1, float Y1, float X2, float Y2)
	{
		x1 = X1; y1 = Y1;
		x2 = X2; y2 = Y2;
		init = true;
	}

	void Set(float X1, float Y1, float X2, float Y2)
	{
		x1 = X1; y1 = Y1;
		x2 = X2; y2 = Y2;
		init = true;
	}

	LRectF(LPointF &a, LPointF &b)
	{
		x1 = a.x; y1 = a.y;
		x2 = b.x; y2 = b.y;
		init = true;
	}

	float X() { return init ? x2 - x1 : 0.0f; }
	float Y() { return init ? y2 - y1 : 0.0f; }
	bool IsNormal() { return x2 >= x1 && y2 >= y1; }
	bool Valid() { return init && IsNormal(); }
	
	void Normalize()
	{
		if (!init)
			return;

		/* Why?
		Round(x1);
		Round(y1);
		Round(x2);
		Round(y2);
		*/

		if (x1 > x2)
			LSwap(x1, x2);
		if (y1 > y2)
			LSwap(y1, y2);
	}

	void Union(LPointF &p)
	{
		if (init)
		{
			x1 = x1 < p.x ? x1 : p.x;
			y1 = y1 < p.y ? y1 : p.y;
			x2 = x2 > p.x ? x2 : p.x;
			y2 = y2 > p.y ? y2 : p.y;
		}
		else
		{
			x1 = x2 = p.x;
			y1 = y2 = p.y;
			init = true;
		}
	}

	void Union(LRectF &p)
	{
		if (!p.init)
			return;

		if (init)
		{
			p.Normalize();
			x1 = x1 < p.x1 ? x1 : p.x1;
			y1 = y1 < p.y1 ? y1 : p.y1;
			x2 = x2 > p.x2 ? x2 : p.x2;
			y2 = y2 > p.y2 ? y2 : p.y2;
		}
		else
		{
			*this = p;
		}
	}

	void Intersect(LRectF &p)
	{
		if (init && p.init)
		{
			x1 = x1 > p.x1 ? x1 : p.x1;
			y1 = y1 > p.y1 ? y1 : p.y1;
			x2 = x2 < p.x2 ? x2 : p.x2;
			y2 = y2 < p.y2 ? y2 : p.y2;
		}
	}

	bool Overlap(LPointF &p)
	{
		return init && (p.x >= x1) && (p.y >= y1) && (p.x <= x2) && (p.y <= y2);
	}

	bool Overlap(LRectF &p)
	{
		return init && p.init && (p.x1 <= x2) && (p.x2 >= x1) && (p.y1 <= y2) && (p.y2 >= y1);
	}

	LRectF &Offset(float x, float y)
	{
		if (init)
		{
			x1 += x;
			y1 += y;
			x2 += x;
			y2 += y;
		}
		return *this;
	}

	LRectF &Offset(LPointF &p)
	{
		if (init)
		{
			x1 += p.x;
			y1 += p.y;
			x2 += p.x;
			y2 += p.y;
		}
		return *this;
	}

	LRectF &Inset(float dx, float dy)
	{
		if (init)
		{
			x1 += dx;
			y1 += dy;
			x2 -= dx;
			y2 -= dy;
		}
		return *this;
	}

	LRectF &ZOff(float x, float y)
	{
		x1 = 0.0f;
		y1 = 0.0f;
		x2 = x;
		y2 = y;
		init = true;
		return *this;
	}

	LRectF &operator =(LRect &f)
	{
		x1 = f.x1;
		y1 = f.y1;
		x2 = f.x2 + 1;
		y2 = f.y2 + 1;
		init = true;
		return *this;
	}

	LRectF &operator =(LRectF &f)
	{
		if (f.init)
		{
			x1 = f.x1;
			y1 = f.y1;
			x2 = f.x2;
			y2 = f.y2;
		}
		else
		{
			x1 = y1 = x2 = y2 = 0.0f;
		}
		init = f.init;
		return *this;
	}

	LRectF &operator =(LPointF &p)
	{
		x1 = x2 = p.x;
		y1 = y2 = p.y;
		init = true;
		return *this;
	}

	const char *Describe()
	{
		constexpr int BUFFERS = 4;
		static thread_local char s[BUFFERS][64];
		static std::atomic<unsigned> nextBuffer{0};
		unsigned index = nextBuffer.fetch_add(1, std::memory_order_relaxed) % BUFFERS;
		
		sprintf_s(s[index], sizeof(s[index]), "%f,%f,%f,%f", x1, y1, x2, y2);
		return s[index];
	}

private:
	static void Round(float &n)
	{
		int i = (int)floor(n + 0.5);
		float d = i - n;

		if (d < 0)
			d = -d;

		if (d < 0.00001)
			n = i;
	}
};

