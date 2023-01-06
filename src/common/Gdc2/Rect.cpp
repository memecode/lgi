/*hdr
**	FILE:			LRect.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			22/8/97
**	DESCRIPTION:	Rectangle class
**
**	Copyright (C) 1997-2015, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "lgi/common/Gdc2.h"

bool LPoint::Inside(LRect &r)
{
	return	(x >= r.x1) &&
			(x <= r.x2) &&
			(y >= r.y1) &&
			(y <= r.y2);
}

LRect &LRect::operator =(const LRect &r)
{
	x1 = r.x1;
	x2 = r.x2;
	y1 = r.y1;
	y2 = r.y2;
	
	return *this;
}

void LRect::ZOff(int x, int y)
{
	x1 = y1 = 0;
	x2 = x;
	y2 = y;
}

void LRect::Normal()
{
	if (x1 > x2)
	{
		LSwap(x1, x2);
	}
	
	if (y1 > y2)
	{
		LSwap(y1, y2);
	}
}

int LRect::Near(int x, int y)
{
	if (Overlap(x, y))
	{
		return 0;
	}
	else if (x >= x1 && x <= x2)
	{
		if (y < y1)
			return y1 - y;
		else
			return y - y2;
	}
	else if (y >= y1 && y <= y2)
	{
		if (x < x1)
			return x1 - x;
		else
			return x - x2;
	}

	int dx = 0;
	int dy = 0;	
	if (x < x1)
	{
		if (y < y1)
		{
			// top left
			dx = x1 - x;
			dy = y1 - y;
		}
		else
		{
			// bottom left
			dx = x1 - x;
			dy = y - y2;
		}
	}
	else
	{
		if (y < y1)
		{
			// top right
			dx = x - x2;
			dy = y1 - y;
		}
		else
		{
			// bottom right
			dx = x - x2;
			dy = y - y2;
		}
	}

	return (int)ceil(sqrt( (double) ((dx * dx) + (dy * dy)) ));
}

int LRect::Near(LRect &r)
{
	if (Overlap(&r))
		return 0;

	if (r.x1 > x2)
	{
		// Off the right edge
		return r.x1 - x2;
	}
	else if (r.x2 < x1)
	{
		// Off the left edge
		return x1 - r.x2;
	}
	else if (r.y2 < y1)
	{
		// Off the top edge
		return y1 - r.y2;
	}
	else
	{
		// Off the bottom edge
		return r.y1 - y2;
	}
}

bool LRect::Valid() const
{
	return (x1 <= x2 && y1 <= y2);
}

void LRect::Offset(int x, int y)
{
	x1 += x;
	y1 += y;
	x2 += x;
	y2 += y;
}

void LRect::Offset(LPoint *p)
{
	if (!p) return;
	x1 += p->x;
	y1 += p->y;
	x2 += p->x;
	y2 += p->y;
}

void LRect::Offset(LRect *a)
{
	x1 += a->x1;
	y1 += a->y1;
	x2 += a->x2;
	y2 += a->y2;
}

void LRect::Inset(int x, int y)
{
	x1 += x;
	y1 += y;
	x2 -= x;
	y2 -= y;
}

void LRect::Inset(LRect *a)
{
	x1 += a->x1;
	y1 += a->y1;
	x2 -= a->x2;
	y2 -= a->y2;
}

void LRect::SetSize(int x, int y)
{
	x2 = x1 + x - 1;
	y2 = y1 + y - 1;
}

void LRect::SetSize(LRect *a)
{
	x2 = x1 + a->X() - 1;
	y2 = y1 + a->Y() - 1;
}

void LRect::Bound(LRect *b)
{
	x1 = MAX(x1,b->x1);
	y1 = MAX(y1,b->y1);
	x2 = MIN(x2,b->x2);
	y2 = MIN(y2,b->y2);
}

bool LRect::Overlap(const LPoint &pt) const
{
	return (pt.x >= x1) && (pt.x <= x2) && (pt.y >= y1) && (pt.y <= y2);
}

bool LRect::Overlap(int x, int y) const
{
	return (x >= x1) && (x <= x2) && (y >= y1) && (y <= y2);
}

bool LRect::Overlap(const LRect *b) const
{
	if (Valid() &&
		b->Valid() &&
		(x1 > b->x2 ||
		y1 > b->y2 ||
		x2 < b->x1 ||
		y2 < b->y1))
	{
		return false;
	}

	return true;
}

void LRect::Union(int x, int y)
{
	x1 = MIN(x1, x);
	y1 = MIN(y1, y);
	x2 = MAX(x2, x);
	y2 = MAX(y2, y);
}

void LRect::Union(LRect *a)
{
	x1 = MIN(a->x1, x1);
	y1 = MIN(a->y1, y1);
	x2 = MAX(a->x2, x2);
	y2 = MAX(a->y2, y2);
}

void LRect::Union(LRect *a, LRect *b)
{
	x1 = MIN(a->x1,b->x1);
	y1 = MIN(a->y1,b->y1);
	x2 = MAX(a->x2,b->x2);
	y2 = MAX(a->y2,b->y2);
}

void LRect::Intersection(LRect *a)
{
	if (Overlap(a))
	{
		x1 = MAX(a->x1, x1);
		y1 = MAX(a->y1, y1);
		x2 = MIN(a->x2, x2);
		y2 = MIN(a->y2, y2);
	}
	else
	{
		x1 = y1 = 0;
		x2 = y2 = -1;
	}
}

void LRect::Intersection(LRect *a, LRect *b)
{
	if (a->Overlap(b))
	{
		x1 = MAX(a->x1,b->x1);
		y1 = MAX(a->y1,b->y1);
		x2 = MIN(a->x2,b->x2);
		y2 = MIN(a->y2,b->y2);
	}
	else
	{
		x1 = y1 = 0;
		x2 = y2 = -1;
	}
}

char *LRect::GetStr() const
{
	#define BUFFERS	5
	static char Str[BUFFERS][48];
	static int Cur = 0;
	
	char *b = Str[Cur];
	sprintf_s(b, 48, "%i,%i,%i,%i", x1, y1, x2, y2);	
	Cur++;
	if (Cur >= BUFFERS)
		Cur = 0;

	return b;
}

bool LRect::SetStr(const char *s)
{
	bool Status = false;
	if (s)
	{
		auto t = LString(s).SplitDelimit(",");
		if (t.Length() == 4)
		{
			x1 = (int)t[0].Int();
			y1 = (int)t[1].Int();
			x2 = (int)t[2].Int();
			y2 = (int)t[3].Int();
			Status = true;
		}
	}
	return Status;
}

bool operator ==(LRect &a, LRect &b)
{
	return (a.x1 == b.x1 &&
			a.y1 == b.y1 &&
			a.x2 == b.x2 &&
			a.y2 == b.y2);
}

bool operator !=(LRect &a, LRect &b)
{
	return !((	a.x1 == b.x1 &&
				a.y1 == b.y1 &&
				a.x2 == b.x2 &&
				a.y2 == b.y2));
}

////////////////////////////////////////////////////////////////////////////////////////////
LRegion::LRegion() : LRect(0, 0, 0, 0)
{
	Size = Alloc = 0;
	a = 0;
	Current = 0;
}

LRegion::LRegion(LRegion &c) : LRect(c.x1, c.y1, c.x2, c.y2)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetLength(c.Length());
	for (int i=0; i<Size; i++)
	{
		a[i] = c.a[i];
	}
}

LRegion::LRegion(int X1, int Y1, int X2, int Y2) : LRect(X1, Y1, X2, Y2)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetLength(1);
	if (a)
	{
		a[0].x1 = X1;
		a[0].y1 = Y1;
		a[0].x2 = X2;
		a[0].y2 = Y2;
	}
}

LRegion::LRegion(const LRect &r) : LRect(r)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetLength(1);
	if (a)
	{
		a[0].x1 = r.x1;
		a[0].y1 = r.y1;
		a[0].x2 = r.x2;
		a[0].y2 = r.y2;
	}
}

LRegion::LRegion(OsRect &r) : LRect(0, 0, 0, 0)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetLength(1);
	if (a)
	{
		#if defined(__GTK_H__)
			x1 = r.x;
			y1 = r.y;
			x2 = r.x + r.width - CornerOffset;
			y2 = r.y + r.height - CornerOffset;
		#elif LGI_CARBON
			x1 = a[0].x1 = (int) r.left;
			y1 = a[0].y1 = (int) r.top;
			x2 = a[0].x2 = (int) r.right - CornerOffset;
			y2 = a[0].y2 = (int) r.bottom - CornerOffset;
		#elif LGI_COCOA
			x1 = a[0].x1 = (int) r.origin.x;
			y1 = a[0].y1 = (int) r.origin.y;
			x2 = a[0].x2 = x1 + (int) r.size.width;
			y2 = a[0].y2 = y1 + (int) r.size.height;
		#elif WINNATIVE || defined(HAIKU)
			x1 = r.left;
			y1 = r.top;
			x2 = r.right - CornerOffset;
			y2 = r.bottom - CornerOffset;
		#else
			#error "Impl me."
		#endif
	}
}

LRegion::~LRegion()
{
	Empty();
}

LRegion &LRegion::operator =(const LRect &r)
{
	SetLength(1);
	if (a)
	{
		*a = r;
	}

	return *this;
}

LRect *LRegion::First()
{
	Current = 0;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

LRect *LRegion::Last()
{
	Current = Size-1;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

LRect *LRegion::Next()
{
	Current++;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

LRect *LRegion::Prev()
{
	Current--;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

bool LRegion::SetLength(int s)
{
	bool Status = true;

	if (s > Alloc)
	{
		int NewAlloc = (s + 15) & (~0xF);
		LRect *Temp = new LRect[NewAlloc];
		if (Temp)
		{
			int Common = MIN(Size, s);
			memcpy(Temp, a, sizeof(LRect)*Common);
			DeleteArray(a);
			a = Temp;
			Size = s;
			Alloc = NewAlloc;
		}
		else
		{
			Status = false;
		}
	}
	else
	{
		Size = s;
		if (!Size)
		{
			DeleteArray(a);
			Alloc = 0;
		}
	}

	return Status;
}

bool LRegion::Delete(int i)
{
	if (i >= 0 && i < Size)
	{
		if (i != Size - 1)
		{
			memmove(a + i, a + (i + 1), sizeof(LRect) * (Size - i - 1));
		}
		Size--;
		return true;
	}
	return false;
}

void LRegion::Empty()
{
	Size = Alloc = 0;
	DeleteArray(a);
}

void LRegion::ZOff(int x, int y)
{
	if (SetLength(1))
	{
		a[0].ZOff(x, y);
	}
}

void LRegion::Normal()
{
	for (int i=0; i<Size; i++)
	{
		a[i].Normal();
	}
}

bool LRegion::Valid()
{
	bool Status = true;

	for (int i=0; i<Size && Status; i++)
	{
		Status &= a[i].Valid();
	}

	return Status && Size > 0;
}

void LRegion::Offset(int x, int y)
{
	for (int i=0; i<Size; i++)
	{
		a[i].Offset(x, y);
	}
}

LRect LRegion::Bound()
{
	static LRect r;
	
	if (a && Size > 0)
	{
		r = a[0];
		for (int i=1; i<Size; i++)
		{
			r.Union(a+i);
		}
	}
	else
	{
		r.ZOff(-1, -1);
	}

	return r;
}

void LRegion::Bound(LRect *b)
{
	if (b)
	{
		for (int i=0; i<Size; )
		{
			if (a[i].Overlap(b))
			{
				a[i++].Bound(b);
			}
			else
			{
				Delete(i);
			}
		}
	}
}

bool LRegion::Overlap(LPoint &pt)
{
	for (int i=0; i<Size; i++)
		if (a[i].Overlap(pt))
			return true;

	return false;
}

bool LRegion::Overlap(int x, int y)
{
	for (int i=0; i<Size; i++)
		if (a[i].Overlap(x, y))
			return true;

	return false;
}

bool LRegion::Overlap(LRect *b)
{
	if (b)
	{
		for (int i=0; i<Size; i++)
		{
			if (a[i].Overlap(b))
			{
				return true;
			}
		}
	}

	return false;
}

void LRegion::Union(LRect *b)
{
	if (b && b->Valid())
	{
		Subtract(b);
		LRect *n = NewOne();
		if (n)
			*n = *b;
	}
}

void LRegion::Intersect(LRect *b)
{
	if (b)
	{
		for (int i=0; i<Size; )
		{
			if (a[i].Overlap(b))
			{
				a[i++].Bound(b);
			}
			else
			{
				Delete(i);
			}
		}
	}
}

void LRegion::Subtract(LRect *b)
{
	if (b && b->Valid())
	{
		LRect Sub = *b;
		int StartSize = Size;

		Normal();
		Sub.Normal();

		for (int i=0; i<StartSize; )
		{
			LRect c = *(a + i);
			if (c.Overlap(&Sub))
			{
				if (Sub.y1 > c.y1)
				{
					LRect *n = NewOne();
					n->x1 = c.x1;
					n->y1 = c.y1;
					n->x2 = c.x2;
					n->y2 = Sub.y1 - 1;
				}

				if (Sub.y2 < c.y2)
				{
					LRect *n = NewOne();
					n->x1 = c.x1;
					n->y1 = Sub.y2 + 1;
					n->x2 = c.x2;
					n->y2 = c.y2;
				}

				if (Sub.x1 > c.x1)
				{
					LRect *n = NewOne();
					n->x1 = c.x1;
					n->y1 = MAX(Sub.y1, c.y1);
					n->x2 = Sub.x1 - 1;
					n->y2 = MIN(Sub.y2, c.y2);
				}

				if (Sub.x2 < c.x2)
				{
					LRect *n = NewOne();
					n->x1 = Sub.x2 + 1;
					n->y1 = MAX(Sub.y1, c.y1);
					n->x2 = c.x2;
					n->y2 = MIN(Sub.y2, c.y2);
				}

				Delete(i);
				StartSize--;
			}
			else
			{
				i++;
			}
		}
	}	
}

void LRegion::Simplify(bool PerfectlyAlignOnly)
{
	int Merges;
	
	do
	{
		Merges = 0;
		for (REG int i=0; i<Size; i++)
		{
			for (REG int n=i+1; n<Size; n++)
			{
				REG LRect *r1 = a + i;
				REG LRect *r2 = a + n;
				bool Merge = false;

				if (r1->x2 == r2->x1 - 1 ||
					r1->x1 == r2->x2 + 1)
				{
					// vertical edge matches... check y axis
					if (PerfectlyAlignOnly)
						Merge = r1->y1 == r2->y1 && r1->y2 == r2->y2;
					else
						Merge = !(r1->y2 < r2->y1 || r1->y1 > r2->y2);
				}
				else if (r1->y2 == r2->y1 - 1 ||
						 r1->y1 == r2->y2 + 1)
				{
					// Horizontal edge matches... check x axis
					if (PerfectlyAlignOnly)
						Merge = r1->x1 == r2->x1 && r1->x2 == r2->x2;
					else
						Merge = !(r1->x2 < r2->x1 || r1->x1 > r2->x2);
				}
				if (Merge)
				{
					// Merge the 2 rects
					r1->Union(r2);
					
					// Delete the 2nd one
					LRect *Last = a + Size - 1;
					if (Last != r2)
						*r2 = *Last;
					Size--;
					Merges++;
					i = Size;
					n = Size;
				}
			}
		}
	}
	while (Merges > 0);
}

bool operator ==(LRegion &a, LRegion &b)
{
	return false;
}

bool operator !=(LRegion &a, LRegion &b)
{
	return false;
}
