/*
	\file
	\author Matthew Allen
	\date 22/8/1997
*/

#ifndef __GDCREGION_H
#define __GDCREGION_H

#if defined __GTK_H__

	#define CornerOffset 1
	typedef Gtk::GdkRectangle OsRect;

#elif defined WIN32

	#define CornerOffset 1
	typedef RECT OsRect;

#elif defined ATHEOS

	#include <gui/rect.h>
	#define CornerOffset 0
	typedef os::Rect OsRect;

#elif defined MAC

	#define CornerOffset 1
	#if LGI_COCOA
		#ifdef __OBJC__
			#include <Foundation/NSGeometry.h>
			typedef NSRect				OsRect;
		#else
			typedef CGRect				OsRect;
		#endif
	#else
		typedef Rect OsRect;
	#endif

#else

	// Implement as required
	#define CornerOffset 0
	struct OsRect
	{
		int left, right, top, bottom;
	};

#endif

#include "lgi/common/Point.h"

/// Rectangle class
class LgiClass LRect
{
	friend LgiClass bool operator ==(LRect &a, LRect &b);
	friend LgiClass bool operator !=(LRect &a, LRect &b);

public:
	int x1, y1, x2, y2;

	LRect() {}
	LRect(int X1, int Y1, int X2, int Y2)
	{
		x1 = X1;
		x2 = X2;
		y1 = Y1;
		y2 = Y2;
	}

	LRect(int Width, int Height)
	{
		x1 = 0;
		x2 = Width - 1;
		y1 = 0;
		y2 = Height - 1;
	}

	LRect(LRect *r)
	{
		x1 = r->x1;
		x2 = r->x2;
		y1 = r->y1;
		y2 = r->y2;
	}

	/// Returns the width
	int X() const { return x2 - x1 + 1; }
	
	/// Returns the height
	int Y() const { return y2 - y1 + 1; }
	
	/// Sets the rectangle
	void Set(int X1, int Y1, int X2, int Y2)
	{
		x1 = X1;
		x2 = X2;
		y1 = Y1;
		y2 = Y2;
	}

	/// Zero offset, sets the top left to 0,0 and the bottom right to x,y
	void ZOff(int x, int y);
	
	/// Normalizes the rectangle so that left is less than the right and so on
	void Normal();
	
	/// Returns true if the rectangle is valid
	bool Valid();
	
	/// Moves the rectangle by an offset
	void Offset(int x, int y);
	void Offset(LPoint *p);
	
	/// Moves the edges by an offset
	void Offset(LRect *a);

	LRect Move(int x, int y)
	{
		LRect r = *this;
		r.Offset(x, y);
		return r;
	}
	
	/// Zooms the rectangle
	void Size(int x, int y);

	/// Zooms the rectangle
	void Size(LRect *a);

	/// Sets the width and height
	void Dimension(int x, int y);

	/// Sets the width and height
	void Dimension(LRect *a);

	/// Sets the rectangle to the intersection of this object and 'b'
	void Bound(LRect *b);

	/// Returns true if the point 'x,y' is in this rectangle
	bool Overlap(int x, int y);

	/// Returns true if the rectangle 'b' overlaps this rectangle
	bool Overlap(LRect *b);
	
	/// Enlarges this rectangle to include the point 'x,y'
	void Union(int x, int y);

	/// Enlarges this rectangle to include all points in 'a'
	void Union(LRect *a);

	/// Makes this rectangle include all points in 'a' OR 'b'
	void Union(LRect *a, LRect *b);

	/// Makes this rectangle the intersection of 'this' AND 'a'
	void Intersection(LRect *a);

	/// Makes this rectangle the intersection of 'a' AND 'b'
	void Intersection(LRect *a, LRect *b);

	/// Returns a static string formated to include the points in the order: x1,y1,x2,y2
	char *GetStr();
	char *Describe() { return GetStr(); }

	/// Sets the rect from a string containing: x1,y1,x2,y2
	bool SetStr(const char *s);

	/// Returns how near a point is to a rectangle
	int Near(int x, int y);
	/// Returns how near a point is to a rectangle
	int Near(LRect &r);

	LRect ZeroTranslate()
	{
		return LRect(0, 0, X()-1, Y()-1);
	}

	#if LGI_COCOA

		operator OsRect()
		{
			OsRect r;
			
			r.origin.x = x1;
			r.origin.y = y1;
			r.size.width = X();
			r.size.height = Y();
			
			return r;
		}
		
		LRect &operator =(OsRect &r)
		{
			if (isinf(r.origin.x) || isinf(r.origin.y))
			{
				ZOff(-1, -1);
			}
			else
			{
				x1 = r.origin.x;
				y1 = r.origin.y;
				x2 = x1 + r.size.width - 1;
				y2 = y1 + r.size.height - 1;
			}
			return *this;
		}
		
		LRect(OsRect r)
		{
			if (isinf(r.origin.x) || isinf(r.origin.y))
			{
				ZOff(-1, -1);
			}
			else
			{
				x1 = r.origin.x;
				y1 = r.origin.y;
				x2 = x1 + r.size.width - 1;
				y2 = y1 + r.size.height - 1;
			}
		}
	
	#elif !defined(__GTK_H__)
	
	/// Returns an operating system specific rectangle
		operator OsRect()
		{
			OsRect r;

			r.left = x1;
			r.top = y1;
			r.right = x2+CornerOffset;
			r.bottom = y2+CornerOffset;

			return r;
		}

		LRect &operator =(OsRect &r)
		{
			x1 = (int) r.left;
			y1 = (int) r.top;
			x2 = (int) r.right - CornerOffset;
			y2 = (int) r.bottom - CornerOffset;
			return *this;
		}

		LRect(OsRect r)
		{
			x1 = (int) r.left;
			y1 = (int) r.top;
			x2 = (int) r.right - CornerOffset;
			y2 = (int) r.bottom - CornerOffset;
		}

		#if defined(MAC)
		LRect(const CGRect &r)
		{
			x1 = (int)r.origin.x;
			y1 = (int)r.origin.y;
			x2 = x1 + (int)r.size.width - 1;
			y2 = y1 + (int)r.size.height - 1;
		}

		LRect &operator =(const CGRect &r)
		{
			x1 = (int)r.origin.x;
			y1 = (int)r.origin.y;
			x2 = x1 + (int)r.size.width - 1;
			y2 = y1 + (int)r.size.height - 1;
			return *this;
		}

		operator CGRect()
		{
			CGRect r;
			r.origin.x = x1;
			r.origin.y = y1;
			r.size.width = x2 - x1 + 1;
			r.size.height = y2 - y1 + 1;
			return r;
		}
		#endif
	
	#endif
	
	bool operator ==(const LRect &r)
	{
		return	x1 == r.x1 &&
				y1 == r.y1 &&
				x2 == r.x2 &&
				y2 == r.y2;
	}
	
	LRect &operator =(const LRect &r);	
	
	#ifdef __GTK_H__
	operator Gtk::GdkRectangle()
	{
	    Gtk::GdkRectangle r;
	    r.x = x1;
	    r.y = y1;
	    r.width = X();
	    r.height = Y();
	    return r;
	}

	LRect &operator =(const Gtk::GdkRectangle r)
	{
		x1 = r.x;
		y1 = r.y;
		x2 = x1 + r.width - 1;
		y2 = y1 + r.height - 1;
		return *this;
	}

	LRect(const Gtk::GtkAllocation &a)
	{
		x1 = a.x;
		y1 = a.y;
		x2 = a.x + a.width - 1;
		y2 = a.y + a.height - 1;
	}
	#endif
	
	#ifdef LGI_SDL
	operator SDL_Rect()
	{
		SDL_Rect s = {(Sint16)x1, (Sint16)y1, (Uint16)X(), (Uint16)Y()};
		return s;
	}
	#endif

	LRect operator +(const LPoint &p)
	{
		LRect r = *this;
		r.Offset(p.x, p.y);
		return r;
	}
	
	LRect &operator +=(const LPoint &p)
	{
		Offset(p.x, p.y);
		return *this;
	}
	
	LRect operator -(const LPoint &p)
	{
		LRect r = *this;
		r.Offset(-p.x, -p.y);
		return r;
	}
	
	LRect &operator -=(const LPoint &p)
	{
		Offset(-p.x, -p.y);
		return *this;
	}

	LRect &operator *=(int factor)
	{
		x1 *= factor;
		y1 *= factor;
		x2 *= factor;
		y2 *= factor;
		return *this;
	}
	
	LRect &operator /=(int factor)
	{
		x1 /= factor;
		y1 /= factor;
		x2 /= factor;
		y2 /= factor;
		return *this;
	}
};

LgiClass bool operator ==(LRect &a, LRect &b);
LgiClass bool operator !=(LRect &a, LRect &b);

/// A region is a list of non-overlapping rectangles that can describe any shape
class LgiClass LRegion : public LRect
{
	/// Current number of stored rectangles
	int Size;
	/// The size of the memory allocated for rectangle
	int Alloc;
	/// The current index (must be >= 0 and < Size)
	int Current;
	/// The array of rectangles
	LRect *a;

	bool SetSize(int s);
	LRect *NewOne() { return (SetSize(Size+1)) ? a+(Size-1) : 0; }
	bool Delete(int i);

public:
	LRegion();
	LRegion(int X1, int Y1, int X2, int Y2);
	LRegion(const LRect &r);
	LRegion(OsRect &r);
	LRegion(LRegion &c);
	~LRegion();

	int X() { return x2 - x1 + 1; }
	int Y() { return y2 - y1 + 1; }
	int Length() { return Size; }
	LRect *operator [](int i) { return (i >= 0 && i < Size) ? a+i : 0; }
	LRegion &operator =(const LRect &r);	
	LRect *First();
	LRect *Last();
	LRect *Next();
	LRect *Prev();

	void Empty();
	void ZOff(int x, int y);
	void Normal();
	bool Valid();
	void Offset(int x, int y);
	void Bound(LRect *b);
	LRect Bound();
	bool Overlap(LRect *b);
	bool Overlap(int x, int y);
	
	void Union(LRect *a);
	void Intersect(LRect *a);
	void Subtract(LRect *a);
	
	/// This joins adjacent blocks into one rect where possible
	void Simplify
	(
		/// If this is set only blocks that are align perfectly will be
		/// merged. Otherwise any adjacent block will be merged as a union
		/// of the 2 blocks. Resulting in a larger region than before.
		bool PerfectlyAlignOnly
	);

	friend bool operator ==(LRegion &a, LRegion &b);
	friend bool operator !=(LRegion &a, LRegion &b);
};

#endif


