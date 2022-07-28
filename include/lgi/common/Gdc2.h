/**
	\file
	\author Matthew Allen
	\date 20/2/1997
	\brief GDC v2.xx header
*/

#ifndef __GDC2_H_
#define __GDC2_H_

#include <stdio.h>

// sub-system headers
#include "LgiOsDefs.h"				// Platform specific
#include "lgi/common/LgiInc.h"
#include "lgi/common/LgiUiBase.h"
#include "lgi/common/File.h"
#include "lgi/common/Mem.h"
#include "lgi/common/Core.h"
#include "lgi/common/Containers.h"
#include "lgi/common/Capabilities.h"
#include "lgi/common/RefCount.h"
#include "lgi/common/Palette.h"

// Alpha Blting
#ifdef WIN32
#include "wingdi.h"
#endif
#ifndef AC_SRC_OVER
#define AC_SRC_OVER					0
#endif
#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA				1
#endif
#include "lgi/common/Library.h"

//				Defines

/// The default gamma curve (none) used by the gamma LUT GdcDevice::GetGamma
#define LGI_DEFAULT_GAMMA				1.0

#ifndef LGI_PI
/// The value of PI
#define LGI_PI						3.141592654
#endif

/// Converts degrees to radians
#define LGI_DegToRad(i)				((i)*LGI_PI/180)

/// Converts radians to degrees
#define LGI_RadToDeg(i)				((i)*180/LGI_PI)

#if defined(WIN32) && !defined(_WIN64)
/// Use intel assembly instructions, comment out for porting
#define GDC_USE_ASM
#endif

/// Blending mode: overwrite
#define GDC_SET						0
/// Blending mode: bitwise AND with background
#define GDC_AND						1
/// Blending mode: bitwise OR with background
#define GDC_OR						2
/// Blending mode: bitwise XOR with background
#define GDC_XOR						3
/// Blending mode: alpha blend with background
#define GDC_ALPHA					4
#define GDC_REMAP					5
#define GDC_MAXOP					6
#define GDC_CACHE_SIZE				4

// Channel types
#define GDCC_MONO					0
#define GDCC_GREY					1
#define GDCC_INDEX					2
#define GDCC_R						3
#define GDCC_G						4
#define GDCC_B						5
#define GDCC_ALPHA					6

// Native data formats
#define GDC_8BIT					0
#define GDC_16BIT					1
#define GDC_24BIT					2
#define GDC_32BIT					3
#define GDC_MAXFMT					4

// Colour spaces
#define GDC_8I						0	// 8 bit paletted
#define GDC_5R6G5B					1	// 16 bit
#define GDC_8B8G8B					2	// 24 bit
#define GDC_8A8R8G8B				3	// 32 bit
#define GDC_MAXSPACE				4

// Update types
#define GDC_PAL_CHANGE				0x1
#define GDC_BITS_CHANGE				0x2

// Flood fill types

/// LSurface::FloodFill to a different colour
#define GDC_FILL_TO_DIFFERENT		0
/// LSurface::FloodFill to a certain colour
#define GDC_FILL_TO_BORDER			1
/// LSurface::FloodFill while colour is near to the seed colour
#define GDC_FILL_NEAR				2

// Gdc options

/// Used in GdcApp8Set::Blt when doing colour depth reduction to 8 bit.
#define GDC_REDUCE_TYPE				0
	/// No conversion
	#define REDUCE_NONE					0
	/// Nearest colour pixels
	#define REDUCE_NEAREST				1
	/// Halftone the pixels
	#define REDUCE_HALFTONE				2
	/// Error diffuse the pixels
	#define REDUCE_ERROR_DIFFUSION		3
	/// Not used.
	#define REDUCE_DL1					4
/// Not used.
#define GDC_HALFTONE_BASE_INDEX		1
/// When in 8-bit defined the behaviour of GdcDevice::GetColour
#define GDC_PALETTE_TYPE			2
	/// Allocate colours from the palette
	#define PALTYPE_ALLOC				0
	/// Use an RGB cube
	#define PALTYPE_RGB_CUBE			1
	/// Use a HSL palette
	#define PALTYPE_HSL					2
/// Converts images to the specified bit-depth on load, does nothing if 0
#define GDC_PROMOTE_ON_LOAD			3
#define GDC_MAX_OPTION				4

// LSurface Flags
#define GDC_ON_SCREEN				0x0002
#define GDC_ALPHA_CHANNEL			0x0004
#define GDC_UPDATED_PALETTE			0x0008
#define GDC_CAPTURE_CURSOR			0x0010
#define GDC_OWN_APPLICATOR			0x0020
#define GDC_CACHED_APPLICATOR		0x0040
#define GDC_OWN_PALETTE				0x0080
#define GDC_DRAW_ON_ALPHA			0x0100

// Region types
#define GDC_RGN_NONE				0	// No clipping
#define GDC_RGN_SIMPLE				1	// Single rectangle
#define GDC_RGN_COMPLEX				2	// Many rectangles

// Error codes
#define GDCERR_NONE					0
#define GDCERR_ERROR				1
#define GDCERR_CANT_SET_SCAN_WIDTH	2
#define GDC_INVALIDMODE				-1

// Font display flags
#define GDCFNT_TRANSPARENT			0x0001	// not set - SOLID
#define GDCFNT_UNDERLINE			0x0002

// Palette file types
#define GDCPAL_JASC					1
#define GDCPAL_MICROSOFT			2

// Misc
#define BMPWIDTH(bits)				((((bits)+31)/32)<<2)

#include "lgi/common/ColourSpace.h"

// Look up tables
#define Div255Lut					(GdcDevice::GetInst()->GetDiv255())

//				Classes
class LFilter;
class LSurface;

#include "lgi/common/Rect.h"
#include "lgi/common/Point.h"
#include "lgi/common/Colour.h"

class LgiClass LBmpMem
{
public:
	enum GdcMemFlags
	{
		BmpOwnMemory = 0x1,
		BmpPreMulAlpha = 0x2,
	};

	uchar *Base;
	int x, y;
	ssize_t Line;
	LColourSpace Cs;
	int Flags;

	LBmpMem();
	~LBmpMem();
	
	bool PreMul()
	{
		return (Flags & BmpPreMulAlpha) != 0;
	}

	bool PreMul(bool set)
	{
		if (set) Flags |= BmpPreMulAlpha;
		else Flags &= ~BmpPreMulAlpha;
		return PreMul();
	}

	bool OwnMem()
	{
		return (Flags & BmpOwnMemory) != 0;
	}

	bool OwnMem(bool set)
	{
		if (set) Flags |= BmpOwnMemory;
		else Flags &= ~BmpOwnMemory;
		return OwnMem();
	}

	int GetBits()
	{
		return GColourSpaceToBits(Cs);
	}
	
	void GetMemoryExtents(uchar *&Start, uchar *&End)
	{
		if (Line < 0)
		{
			Start = Base + (y * Line);
			End = Base + Line;
		}
		else
		{
			Start = Base;
			End = Base + (y * Line);
		}
	}
	
	bool Overlap(LBmpMem *Mem)
	{
		uchar *ThisStart, *ThisEnd;
		GetMemoryExtents(ThisStart, ThisEnd);

		uchar *MemStart, *MemEnd;
		GetMemoryExtents(MemStart, MemEnd);
		
		if (ThisEnd < MemStart)
			return false;
		if (ThisStart > MemEnd)
			return false;

		return true;
	}
};


#define GAPP_ALPHA_A			1
#define GAPP_ALPHA_PAL			2
#define GAPP_BACKGROUND			3
#define GAPP_ANGLE				4
#define GAPP_BOUNDS				5

/// \brief Class to draw onto a memory bitmap
///
/// This class assumes that all clipping is done by the layer above.
/// It can then implement very simple loops to do the work of filling
/// pixels
class LgiClass LApplicator
{
protected:
	LBmpMem *Dest;
	LBmpMem *Alpha;
	GPalette *Pal;
	int Op;

public:
	union
	{
		COLOUR c;				// main colour
		System24BitPixel p24;
		System32BitPixel p32;
	};

	LApplicator() { c = 0; Dest = NULL; Alpha = NULL; Pal = NULL; }
	LApplicator(COLOUR Colour) { c = Colour; }
	virtual ~LApplicator() { }

	virtual const char *GetClass() { return "LApplicator"; }

	/// Get a parameter
	virtual int GetVar(int Var) { return 0; }
	/// Set a parameter
	virtual int SetVar(int Var, NativeInt Value) { return 0; }

	LColourSpace GetColourSpace() { return Dest ? Dest->Cs : CsNone; }

	/// Sets the operator
	void SetOp(int o, int Param = -1) { Op = o; }
	/// Gets the operator
	int GetOp() { return Op; }
	/// Gets the bit depth
	int GetBits() { return (Dest) ? GColourSpaceToBits(Dest->Cs) : 0; }
	/// Gets the flags in operation
	int GetFlags() { return (Dest) ? Dest->Flags : 0; }
	/// Gets the palette
	GPalette *GetPal() { return Pal; }

	/// Sets the bitmap to write onto
	virtual bool SetSurface(LBmpMem *d, GPalette *p = 0, LBmpMem *a = 0) = 0; // sets Dest, returns FALSE on error
	/// Sets the current position to an x,y
	virtual void SetPtr(int x, int y) = 0;			// calculates Ptr from x, y
	/// Moves the current position one pixel left
	virtual void IncX() = 0;
	/// Moves the current position one scanline down
	virtual void IncY() = 0;
	/// Offset the current position
	virtual void IncPtr(int X, int Y) = 0;

	/// Sets the pixel at the current location with the current colour
	virtual void Set() = 0;
	/// Gets the colour of the pixel at the current location
	virtual COLOUR Get() = 0;
	/// Draws a vertical line from the current position down 'height' scanlines
	virtual void VLine(int height) = 0;
	/// Draws a rectangle starting from the current position, 'x' pixels across and 'y' pixels down
	virtual void Rectangle(int x, int y) = 0;
	/// Copies bitmap data to the current position
	virtual bool Blt(LBmpMem *Src, GPalette *SPal, LBmpMem *SrcAlpha = 0) = 0;
};

/// Creates applications from parameters.
class LgiClass LApplicatorFactory
{
public:
	LApplicatorFactory();
	virtual ~LApplicatorFactory();

	/// Find the application factory and create the appropriate object.
	static LApplicator *NewApp(LColourSpace Cs, int Op);
	virtual LApplicator *Create(LColourSpace Cs, int Op) = 0;
};

class LgiClass LApp15 : public LApplicatorFactory
{
public:
	LApplicator *Create(LColourSpace Cs, int Op);
};

class LgiClass LApp16 : public LApplicatorFactory
{
public:
	LApplicator *Create(LColourSpace Cs, int Op);
};

class LgiClass LApp24 : public LApplicatorFactory
{
public:
	LApplicator *Create(LColourSpace Cs, int Op);
};

class LgiClass LApp32 : public LApplicatorFactory
{
public:
	LApplicator *Create(LColourSpace Cs, int Op);
};

class LgiClass LApp8 : public LApplicatorFactory
{
public:
	LApplicator *Create(LColourSpace Cs, int Op);
};

class LAlphaFactory : public LApplicatorFactory
{
public:
	LApplicator *Create(LColourSpace Cs, int Op);
};

#define OrgX(x)			x -= OriginX
#define OrgY(y)			y -= OriginY
#define OrgXy(x, y)		x -= OriginX; y -= OriginY
#define OrgPt(p)		p.x -= OriginX; p.y -= OriginY
#define OrgRgn(r)		r.Offset(-OriginX, -OriginY)

/// Base class API for graphics operations
class LgiClass LSurface : public LRefCount, public LDom
{
	friend class LFilter;
	friend class LView;
	friend class LWindow;
	friend class LVariant;
	friend class GRegionClipDC;
	friend class LMemDC;

	void Init();
	
protected:
	int				Flags;
	int				PrevOp;
	LRect			Clip;
	LColourSpace	ColourSpace;
	LBmpMem			*pMem;
	LSurface		*pAlphaDC;
	GPalette		*pPalette;
	LApplicator		*pApp;
	LApplicator		*pAppCache[GDC_CACHE_SIZE];
	int				OriginX, OriginY;

	// Protected functions
	LApplicator		*CreateApplicator(int Op, LColourSpace Cs = CsNone);
	uint32_t			LineBits;
	uint32_t			LineMask;
	uint32_t			LineReset;

	#if WINNATIVE
		OsPainter	hDC;
		OsBitmap	hBmp;
	#endif

public:
	LSurface();
	LSurface(LSurface *pDC);
	virtual ~LSurface();

	// Win32
	#if defined(__GTK_H__)

	/// Gets the drawable size, regardless of clipping or client rect
	virtual LPoint GetSize() { LPoint p; return p; }
	virtual Gtk::GtkPrintContext *GetPrintContext() { return NULL; }
	virtual Gtk::GdkPixbuf *CreatePixBuf() { return NULL; }

	#elif defined(WINNATIVE)

	virtual HDC StartDC() { return hDC; }
	virtual void EndDC() {}
	
	#elif defined MAC
	
	#if LGI_COCOA
	#else
	virtual CGColorSpaceRef GetColourSpaceRef() { return 0; }
	#endif
	
	#endif

	virtual const char *GetClass() { return "LSurface"; }
	virtual OsBitmap GetBitmap();
	virtual OsPainter Handle();
	virtual void SetClient(LRect *c) {}
	virtual bool GetClient(LRect *c) { return false; }

	// Creation
	enum SurfaceCreateFlags
	{
		SurfaceCreateNone,
		SurfaceRequireNative,
		SurfaceRequireExactCs,
	};
	virtual bool Create(int x, int y, LColourSpace Cs, int Flags = SurfaceCreateNone) { return false; }
	virtual void Update(int Flags) {}

	// Alpha channel	
	/// Returns true if this Surface has an alpha channel
	virtual bool HasAlpha() { return pAlphaDC != 0; }
	/// Creates or destroys the alpha channel for this surface
	virtual bool HasAlpha(bool b);
	/// Returns true if we are drawing on the alpha channel
	bool DrawOnAlpha() { return ((Flags & GDC_DRAW_ON_ALPHA) != 0); }
	/// True if you want to edit the alpha channel rather than the colour bits
	bool DrawOnAlpha(bool Draw);
	/// Returns the surface of the alpha channel.
	LSurface *AlphaDC() { return pAlphaDC; }

	/// Lowers the alpha of the whole image to Alpha/255.0.
	/// Only works on bitmaps with an alpha channel (i.e. CsRgba32 or it's variants)
	bool SetConstantAlpha(uint8_t Alpha);

	// Create sub-images (that reference the memory of this object)
	LSurface *SubImage(LRect r);
	LSurface *SubImage(int x1, int y1, int x2, int y2)
	{
		LRect r(x1, y1, x2, y2);
		return SubImage(r);
	}

	// Applicator
	virtual bool Applicator(LApplicator *pApp);
	virtual LApplicator *Applicator();

	// Palette
	virtual GPalette *Palette();
	virtual void Palette(GPalette *pPal, bool bOwnIt = true);

	// Clip region
	virtual LRect ClipRgn(LRect *Rgn);
	virtual LRect ClipRgn();

	/// Gets the current colour
	virtual COLOUR Colour() { return pApp->c; }
	/// Sets the current colour
	virtual COLOUR Colour
	(
		/// The new colour
		COLOUR c,
		/// The bit depth of the new colour or 0 to indicate the depth is the same as the current Surface
		int Bits = 0
	);
	/// Sets the current colour
	virtual LColour Colour
	(
		/// The new colour
		LColour c
	);
	/// Sets the colour to a system colour
	virtual LColour Colour(LSystemColour SysCol);
	/// Gets the current blending mode in operation
	virtual int Op() { return (pApp) ? pApp->GetOp() : GDC_SET; }
	/// Sets the current blending mode in operation
	/// \sa GDC_SET, GDC_AND, GDC_OR, GDC_XOR and GDC_ALPHA
	virtual int Op(int Op, NativeInt Param = -1);
	/// Gets the width in pixels
	virtual int X() { return (pMem) ? pMem->x : 0; }
	/// Gets the height in pixels
	virtual int Y() { return (pMem) ? pMem->y : 0; }
	/// Gets the bounds of the image as a LRect
	LRect Bounds() { return LRect(0, 0, X()-1, Y()-1); }
	/// Gets the length of a scanline in bytes
	virtual ssize_t GetRowStep() { return (pMem) ? pMem->Line : 0; }
	/// Returns the resolution of the device
	virtual LPoint GetDpi() { return LPoint(96,96); }
	/// Gets the bits per pixel
	virtual int GetBits() { return (pMem) ? GColourSpaceToBits(pMem->Cs) : 0; }
	/// Gets the colour space of the pixels
	virtual LColourSpace GetColourSpace() { return ColourSpace; }
	/// Gets any flags associated with the surface
	virtual int GetFlags() { return Flags; }
	/// Returns true if the surface is on the screen
	virtual class LScreenDC *IsScreen() { return 0; }
	/// Returns true if the surface is for printing
	virtual bool IsPrint() { return false; }
	/// Returns a pointer to the start of a scanline, or NULL if not available
	virtual uchar *operator[](int y);
	/// Dumps various debug information
	virtual LString Dump() { return LString("Not implemented."); }

	/// Returns true if this surface supports alpha compositing when using Blt
	virtual bool SupportsAlphaCompositing() { return false; }
	/// \returns whether if pixel data is pre-multiplied alpha
	virtual bool IsPreMultipliedAlpha();
	/// Converts the pixel data between pre-mul alpha or non-pre-mul alpha
	virtual bool ConvertPreMulAlpha(bool ToPreMul);
	/// Makes the alpha channel opaque
	virtual bool MakeOpaque();

	/// Gets the surface origin
	virtual void GetOrigin(int &x, int &y) { x = OriginX; y = OriginY; }
	/// Gets the surface origin
	LPoint GetOrigin() { int x, y; GetOrigin(x, y); return LPoint(x, y); }
	/// Sets the surface origin
	virtual void SetOrigin(int x, int y) { OriginX = x; OriginY = y; }
	/// Sets the surface origin
	void SetOrigin(LPoint p) { SetOrigin(p.x, p.y); }
	
	/// Sets a pixel with the current colour
	virtual void Set(int x, int y);
	/// Gets a pixel (doesn't work on some types of image, i.e. LScreenDC)
	virtual COLOUR Get(int x, int y);

	// Line

	/// Draw a horizontal line in the current colour
	virtual void HLine(int x1, int x2, int y);
	/// Draw a vertical line in the current colour
	virtual void VLine(int x, int y1, int y2);
	/// Draw a line in the current colour
	virtual void Line(int x1, int y1, int x2, int y2);
	
	/// Some surfaces only support specific line styles (e.g. GDI/Win)
	enum LineStyles
	{
		LineNone = 0x0,
		LineSolid = 0xffffffff,
		LineAlternate = 0xaaaaaaaa,
		LineDash = 0xf0f0f0f0,
		LineDot = 0xcccccccc,
		LineDashDot = 0xF33CCF30,
		LineDashDotDot = 0xf0ccf0cc,
	};
	
	virtual uint LineStyle(uint32_t Bits, uint32_t Reset = 0x80000000)
	{
		uint32_t B = LineBits;
		LineBits = Bits;
		LineMask = LineReset = Reset;
		return B;
	}
	virtual uint LineStyle() { return LineBits; }

	// Curve

	/// Stroke a circle in the current colour
	virtual void Circle(double cx, double cy, double radius);
	/// Fill a circle in the current colour
	virtual void FilledCircle(double cx, double cy, double radius);
	/// Stroke an arc in the current colour
	virtual void Arc(double cx, double cy, double radius, double start, double end);
	/// Fill an arc in the current colour
	virtual void FilledArc(double cx, double cy, double radius, double start, double end);
	/// Stroke an ellipse in the current colour
	virtual void Ellipse(double cx, double cy, double x, double y);
	/// Fill an ellipse in the current colour
	virtual void FilledEllipse(double cx, double cy, double x, double y);

	// Rectangular

	/// Stroke a rectangle in the current colour
	virtual void Box(int x1, int y1, int x2, int y2);
	/// Stroke a rectangle in the current colour
	virtual void Box
	(
		/// The rectangle, or NULL to stroke the edge of the entire surface
		LRect *a = NULL
	);
	/// Fill a rectangle in the current colour
	virtual void Rectangle(int x1, int y1, int x2, int y2);
	/// Fill a rectangle in the current colour
	virtual void Rectangle
	(
		/// The rectangle, or NULL to fill the entire surface
		LRect *a = NULL
	);
	/// Copy an image onto the surface
	virtual void Blt
	(
		/// The destination x coord
		int x,
		/// The destination y coord
		int y,
		/// The source surface
		LSurface *Src,
		/// The optional area of the source to use, if not specified the whole source is used
		LRect *a = NULL
	);
	void Blt(int x, int y, LSurface *Src, LRect a) { Blt(x, y, Src, &a); }
	/// Not implemented
	virtual void StretchBlt(LRect *d, LSurface *Src, LRect *s);

	// Other

	/// Fill a polygon in the current colour
	virtual void Polygon(int Points, LPoint *Data);
	/// Stroke a bezier in the current colour
	virtual void Bezier(int Threshold, LPoint *Pt);
	/// Flood fill in the current colour (doesn't work on a LScreenDC)
	virtual void FloodFill
	(
		/// Start x coordinate
		int x,
		/// Start y coordinate
		int y,
		/// Use #GDC_FILL_TO_DIFFERENT, #GDC_FILL_TO_BORDER or #GDC_FILL_NEAR
		int Mode,
		/// Fill colour
		COLOUR Border = 0,
		/// The bounds of the filled area or NULL if you don't care
		LRect *Bounds = NULL
	);
	
	/// Describes the image
	virtual LString GetStr();

	// LDom interface
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL);
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL);
	bool CallMethod(const char *Name, LVariant *ReturnValue, LArray<LVariant*> &Args);
};

#if defined(MAC) && !defined(__GTK_H__)

struct LPrintDcParams
{
	#if LGI_COCOA
	#else
	PMRect Page;
	CGContextRef Ctx;
	PMResolution Dpi;
	#endif
};

#endif

#if defined(__GTK_H__)
	typedef Gtk::GdkWindow OsDrawable;
#endif

/// \brief An implemenation of LSurface to draw onto the screen.
///
/// This is the class given to LView::OnPaint() most of the time. Which most of
/// the time doesn't matter unless your doing something unusual.
class LgiClass LScreenDC : public LSurface
{
	class LScreenPrivate *d;

public:
	LScreenDC();
	virtual ~LScreenDC();

	const char *GetClass() override { return "LScreenDC"; }

	// OS Sepcific
	#if WINNATIVE

		LScreenDC(LViewI *view);
		LScreenDC(HWND hwnd);
		LScreenDC(HDC hdc, HWND hwnd, bool Release = false);
		LScreenDC(HBITMAP hBmp, int Sx, int Sy);

		bool CreateFromHandle(HDC hdc);
		void SetSize(int x, int y);

	#else

		/// Construct a wrapper to draw on a window
		LScreenDC(LView *view, void *Param = 0);
	
		#if defined(LGI_SDL)
		#elif defined(__GTK_H__)
		
			/// Constructs a server size pixmap
			LScreenDC(int x, int y, int bits);		
			/// Constructs a wrapper around a drawable
			LScreenDC(OsDrawable *Drawable);
			/// Constructs a DC for drawing on a cairo context
			LScreenDC(Gtk::cairo_t *cr, int x, int y);
			
			// Gtk::cairo_surface_t *GetSurface(bool Render);
			LPoint GetSize();
		
		#elif defined(MAC)
	
			LScreenDC(LWindow *wnd, void *Param = 0);
			LScreenDC(LPrintDcParams *Params); // Used by LPrintDC
			LRect GetPos();
			void PushState();
			void PopState();
	
		#endif

		OsPainter Handle() override;
		LView *GetView();
		int GetFlags() override;
		LRect *GetClient();

	#endif

	// Properties
	bool GetClient(LRect *c) override;
	void SetClient(LRect *c) override;

	int X() override;
	int Y() override;

	GPalette *Palette() override;
	void Palette(GPalette *pPal, bool bOwnIt = true) override;

	uint LineStyle() override;
	uint LineStyle(uint Bits, uint32_t Reset = 0x80000000) override;

	int GetBits() override;
	LScreenDC *IsScreen() override { return this; }
	bool SupportsAlphaCompositing() override;
	LPoint GetDpi() override;

	#ifndef LGI_SDL
	uchar *operator[](int y) override { return NULL; }

	void GetOrigin(int &x, int &y) override;
	void SetOrigin(int x, int y) override;
	LRect ClipRgn() override;
	LRect ClipRgn(LRect *Rgn) override;
	COLOUR Colour() override;
	COLOUR Colour(COLOUR c, int Bits = 0) override;
	LColour Colour(LColour c) override;
	LString Dump() override;

	int Op() override;
	int Op(int Op, NativeInt Param = -1) override;

	// Primitives
	void Set(int x, int y) override;
	COLOUR Get(int x, int y) override;
	void HLine(int x1, int x2, int y) override;
	void VLine(int x, int y1, int y2) override;
	void Line(int x1, int y1, int x2, int y2) override;
	void Circle(double cx, double cy, double radius) override;
	void FilledCircle(double cx, double cy, double radius) override;
	void Arc(double cx, double cy, double radius, double start, double end) override;
	void FilledArc(double cx, double cy, double radius, double start, double end) override;
	void Ellipse(double cx, double cy, double x, double y) override;
	void FilledEllipse(double cx, double cy, double x, double y) override;
	void Box(int x1, int y1, int x2, int y2) override;
	void Box(LRect *a) override;
	void Rectangle(int x1, int y1, int x2, int y2) override;
	void Rectangle(LRect *a = NULL) override;
	void Blt(int x, int y, LSurface *Src, LRect *a = NULL) override;
	void StretchBlt(LRect *d, LSurface *Src, LRect *s = NULL) override;
	void Polygon(int Points, LPoint *Data) override;
	void Bezier(int Threshold, LPoint *Pt) override;
	void FloodFill(int x, int y, int Mode, COLOUR Border = 0, LRect *Bounds = NULL) override;
	#endif
};

/// \brief Blitting region helper class, can calculate the right source and dest rectangles
/// for a blt operation including propagating clipping back to the source rect.
class LBlitRegions
{
	// Raw image bounds
	LRect SrcBounds;
	LRect DstBounds;
	
	// Unclipped blit regions
	LRect SrcBlt;
	LRect DstBlt;

public:
	/// Clipped blit region in destination co-ords
	LRect SrcClip;
	/// Clipped blit region in source co-ords
	LRect DstClip;

	/// Calculate the rectangles.
	LBlitRegions
	(
		/// Destination surface
		LSurface *Dst,
		/// Destination blt x offset
		int x1,
		/// Destination blt y offset
		int y1,
		/// Source surface
		LSurface *Src,
		/// [Optional] Crop the source surface first, else whole surface is blt
		LRect *SrcRc = 0
	)
	{
		// Calc full image bounds
		if (Src) SrcBounds.Set(0, 0, Src->X()-1, Src->Y()-1);
		else SrcBounds.ZOff(-1, -1);
		if (Dst)
		{
			DstBounds = Dst->Bounds();
			int x = 0, y = 0;
			Dst->GetOrigin(x, y);
			DstBounds.Offset(x, y);
		}
		else DstBounds.ZOff(-1, -1);
		
		// Calc full sized blt regions
		if (SrcRc)
		{
			SrcBlt = *SrcRc;
			SrcBlt.Bound(&SrcBounds);
		}
		else SrcBlt = SrcBounds;

		DstBlt = SrcBlt;
		DstBlt.Offset(x1-DstBlt.x1, y1-DstBlt.y1);

		// Dest clipped to dest bounds
		DstClip = DstBlt;
		DstClip.Bound(&DstBounds);
		
		// Now map the dest clipping back to the source
		SrcClip = SrcBlt;
		SrcClip.x1 += DstClip.x1 - DstBlt.x1;
		SrcClip.y1 += DstClip.y1 - DstBlt.y1;
		SrcClip.x2 -= DstBlt.x2 - DstClip.x2;
		SrcClip.y2 -= DstBlt.y2 - DstClip.y2;
	}

	/// Returns non-zero if both clipped rectangles are valid.
	bool Valid()
	{
		return DstClip.Valid() && SrcClip.Valid();
	}
	
	void Dump()
	{
		printf("SrcBounds: %s\n", SrcBounds.GetStr());
		printf("DstBounds: %s\n", DstBounds.GetStr());
		
		printf("SrcBlt: %s\n", SrcBlt.GetStr());
		printf("DstBlt: %s\n", DstBlt.GetStr());

		printf("SrcClip: %s\n", SrcClip.GetStr());
		printf("DstClip: %s\n", DstClip.GetStr());
	}
};

#if defined(MAC) && !defined(__GTK_H__)
class CGImg
{
	class CGImgPriv *d;

	void Create(int x, int y, int Bits, ssize_t Line, uchar *data, uchar *palette, LRect *r);

public:
	CGImg(int x, int y, int Bits, ssize_t Line, uchar *data, uchar *palette, LRect *r, int Debug = 0);
	CGImg(LSurface *pDC);
	~CGImg();
	
	operator CGImageRef();
	void Release();
};
#endif

#ifdef __GTK_H__
#include "lgi/common/CairoSurface.h"
#endif

/// \brief An implemenation of LSurface to draw into a memory bitmap.
///
/// This class uses a block of memory to represent an image. You have direct
/// pixel access as well as higher level functions to manipulate the bits.
class LgiClass LMemDC : public LSurface
{
protected:
	class LMemDCPrivate *d;

	#if defined WINNATIVE
	PBITMAPINFO	GetInfo();
	#endif
	
	// This is called between capturing the screen and overlaying the
	// cursor in LMemDC::Blt(x, y, ScreenDC, Src). It can be used to
	// overlay effects between the screen and cursor layers.
	virtual void OnCaptureScreen() {}

public:
	/// Creates a memory bitmap
	LMemDC
	(
		/// The width
		int x = 0,
		/// The height
		int y = 0,
		/// The colour space to use. CsNone will default to the 
		/// current screen colour space.
		LColourSpace cs = CsNone,
		/// Optional creation flags
		int Flags = SurfaceCreateNone
	);
	LMemDC(LSurface *pDC);
	virtual ~LMemDC();

	const char *GetClass() { return "LMemDC"; }

	#if WINNATIVE
	
		HDC StartDC();
		void EndDC();
		void Update(int Flags);
		void UpsideDown(bool upsidedown);
		
	#else

		LRect ClipRgn() { return Clip; }

		#if defined(__GTK_H__)

			LPoint GetSize();

			/// This returns the surface owned by the LMemDC
			Gtk::cairo_surface_t *GetSurface();

			/// This returns a sub-image, caller is responsible to free via
			/// calling cairo_surface_destroy
			LCairoSurfaceT GetSubImage(LRect &r);
			
			LColourSpace GetCreateCs();
			Gtk::GdkPixbuf *CreatePixBuf();

		#elif defined MAC
				
			OsBitmap GetBitmap();
	
			#if LGI_COCOA && defined(__OBJC__)
			LMemDC(NSImage *img);
			NSImage *NsImage(LRect *rc = NULL);
			#endif
	
			#if !defined(LGI_SDL)
				CGColorSpaceRef GetColourSpaceRef();
				CGImg *GetImg(LRect *Sub = 0, int Debug = 0);
			#endif
		
		#elif defined(LGI_SDL) || defined(HAIKU)

			OsBitmap GetBitmap();

		#endif

		OsPainter Handle();
		
	#endif

	// Set new clipping region
	LRect ClipRgn(LRect *Rgn);

	void SetClient(LRect *c);

	/// Locks the bits for access. LMemDC's start in the locked state.
	bool Lock();
	/// Unlocks the bits to optimize for display. While the bitmap is unlocked you
	/// can't access the data for read or write. On linux this converts the XImage
	/// to pixmap. On other systems it doesn't do much. As a general rule if you
	/// don't need access to a bitmap after creating / loading it then unlock it.
	bool Unlock();
	
	#if !WINNATIVE && !LGI_CARBON && !LGI_COCOA
	void GetOrigin(int &x, int &y);
	#endif
	void SetOrigin(int x, int y);
	void Empty();
	bool SupportsAlphaCompositing();
	bool SwapRedAndBlue();
	
	bool Create(int x, int y, LColourSpace Cs, int Flags = SurfaceCreateNone);
	void Blt(int x, int y, LSurface *Src, LRect *a = NULL);
	void StretchBlt(LRect *d, LSurface *Src, LRect *s = NULL);

	void HorzLine(int x1, int x2, int y, COLOUR a, COLOUR b);
	void VertLine(int x, int y1, int y2, COLOUR a, COLOUR b);
};

/// \brief An implemenation of LSurface to print to a printer.
///
/// This class redirects standard graphics calls to print a page.
///
/// \sa GPrinter
class LgiClass LPrintDC
#if defined(WIN32) || defined(MAC)
	: public LScreenDC
#else
	: public LSurface
#endif
{
	class GPrintDCPrivate *d;

public:
	LPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName = NULL);
	~LPrintDC();

	const char *GetClass() override { return "LPrintDC"; }

	bool IsPrint() override { return true; }
	const char *GetOutputFileName();
	int X() override;
	int Y() override;
	int GetBits() override;

	/// Returns the DPI of the printer or 0,0 on error
	LPoint GetDpi() override;

	#if defined __GTK_H__
	
		Gtk::GtkPrintContext *GetPrintContext();
	
		int Op() { return GDC_SET; }
		int Op(int Op, NativeInt Param = -1) { return GDC_SET; }
	
		LRect ClipRgn(LRect *Rgn);
		LRect ClipRgn();
		COLOUR Colour();
		COLOUR Colour(COLOUR c, int Bits = 0);
		LColour Colour(LColour c);
	
		void Set(int x, int y);
	
		void HLine(int x1, int x2, int y);
		void VLine(int x, int y1, int y2);
		void Line(int x1, int y1, int x2, int y2);
	
		void Circle(double cx, double cy, double radius);
		void FilledCircle(double cx, double cy, double radius);
		void Arc(double cx, double cy, double radius, double start, double end);
		void FilledArc(double cx, double cy, double radius, double start, double end);
		void Ellipse(double cx, double cy, double x, double y);
		void FilledEllipse(double cx, double cy, double x, double y);
	
		void Box(int x1, int y1, int x2, int y2);
		void Box(LRect *a = NULL);
		void Rectangle(int x1, int y1, int x2, int y2);
		void Rectangle(LRect *a = NULL);
		void Blt(int x, int y, LSurface *Src, LRect *a = NULL);
		void StretchBlt(LRect *d, LSurface *Src, LRect *s);
	
		void Polygon(int Points, LPoint *Data);
		void Bezier(int Threshold, LPoint *Pt);

	#endif
};

//////////////////////////////////////////////////////////////////////////////
class LgiClass LGlobalColour
{
	class LGlobalColourPrivate *d;

public:
	LGlobalColour();
	~LGlobalColour();

	// Add all the colours first
	COLOUR AddColour(COLOUR c24);
	bool AddBitmap(LSurface *pDC);
	bool AddBitmap(LImageList *il);

	// Then call this
	bool MakeGlobalPalette();

	// Which will give you a palette that
	// includes everything
	GPalette *GetPalette();

	// Convert a bitmap to the global palette
	COLOUR GetColour(COLOUR c24);
	bool RemapBitmap(LSurface *pDC);
};

/// This class is useful for double buffering in an OnPaint handler...
class LDoubleBuffer
{
	LSurface **In;
	LSurface *Screen;
	LMemDC Mem;
	LRect Rgn;
	bool Valid;

public:
	LDoubleBuffer(LSurface *&pDC, LRect *Sub = NULL) : In(&pDC)
	{
		Rgn = Sub ? *Sub : pDC->Bounds();
		Screen = pDC;

		Valid = pDC && Mem.Create(Rgn.X(), Rgn.Y(), pDC->GetColourSpace());
		if (Valid)
		{
			*In = &Mem;
			if (Sub)
				pDC->SetOrigin(Sub->x1, Sub->y1);
		}
	}
	
	~LDoubleBuffer()
	{
		if (Valid)
		{
			Mem.SetOrigin(0, 0);
			Screen->Blt(Rgn.x1, Rgn.y1, &Mem);
		}

		// Restore state
		*In = Screen;
	}

	LMemDC *GetMem()
	{
		return &Mem;
	}
};

#ifdef WIN32
typedef int (__stdcall *MsImg32_AlphaBlend)(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
#endif

/// Main singleton graphics device class. Holds all global data for graphics rendering.
class LgiClass GdcDevice : public LCapabilityClient
{
	friend class LScreenDC;
	friend class LMemDC;
	friend class LImageList;

	static GdcDevice *pInstance;
	class GdcDevicePrivate *d;
	
	#ifdef WIN32
	MsImg32_AlphaBlend AlphaBlend;
	#endif

public:
	GdcDevice();
	~GdcDevice();
	static GdcDevice *GetInst() { return pInstance; }

	/// Returns the colour space of the screen
	LColourSpace GetColourSpace();
	/// Returns the current screen bit depth
	int GetBits();
	/// Returns the current screen width
	int X();
	/// Returns the current screen height
	int Y();
	/// Returns the size of the screen as a rectangle.
	LRect Bounds() { return LRect(0, 0, X()-1, Y()-1); }

	LGlobalColour *GetGlobalColour();

	/// Set a global graphics option
	int GetOption(int Opt);
	/// Get a global graphics option
	int SetOption(int Opt, int Value);

	/// 256 lut for squares
	ulong *GetCharSquares();
	/// Divide by 255 lut, 64k entries long.
	uchar *GetDiv255();

	// Palette/Colour
	void SetGamma(double Gamma);
	double GetGamma();

	// Palette
	void SetSystemPalette(int Start, int Size, GPalette *Pal);
	GPalette *GetSystemPalette();
	void SetColourPaletteType(int Type);	// Type = PALTYPE_xxx define
	COLOUR GetColour(COLOUR Rgb24, LSurface *pDC = NULL);

    // File I/O
    
    /// \brief Loads a image from a file
    ///
    /// This function uses the compiled in codecs, some of which require external
    /// shared libraries / DLL's to function. Other just need the right source to
    /// be compiled in.
    ///
    /// Lgi comes with the following image codecs:
    /// <ul>
    ///  <li> Windows or OS/2 Bitmap: GdcBmp (LFilter.cpp)
    ///  <li> PCX: GdcPcx (Pcx.cpp)
    ///  <li> GIF: GdcGif (Gif.cpp and Lzw.cpp)
    ///  <li> JPEG: GdcJpeg (Jpeg.cpp + <a href='http://www.memecode.com/scribe/extras.php'>libjpeg</a> library)
    ///  <li> PNG: GdcPng (Png.cpp + <a href='http://www.memecode.com/scribe/extras.php'>libpng</a> library)
    /// </ul>
    /// 
    LSurface *Load
    (
        /// The full path of the file
        const char *FileName,
    	/// [Optional] Enable OS based loaders
        bool UseOSLoader = true
    );

	/// The stream version of the file loader...
    LSurface *Load
    (
        /// The full path of the file
        LStream *In,
        /// [Optional] File name hint for selecting a filter
        const char *Name = NULL,
    	/// [Optional] Enable OS based loaders
        bool UseOSLoader = true
    );
    
    /// Save an image to a stream.
    bool Save
    (
        /// The file to write to
        LStream *Out,
        /// The pixels to store
        LSurface *In,
		/// Dummy file name to determine the file type, eg: "img.jpg"
		const char *FileType
    );

    /// Save an image to a file.
    bool Save
    (
        /// The file to write to
        const char *Name,
        /// The pixels to store
        LSurface *pDC
    );
    
    #if LGI_SDL
    SDL_Surface *Handle();
    #endif
};

/// \brief Defines a bitmap inline in C++ code.
///
/// The easiest way I know of create the raw data for an LInlineBmp
/// is to use <a href='http://www.memecode.com/image.php'>i.Mage</a> to
/// load a file or create a image and then use the Edit->Copy As Code
/// menu. Then paste into your C++ and put a uint32 array declaration
/// around it. Then point the Data member to the uint32 array. Just be
/// sure to get the dimensions right.
///
/// I use this for embeding resource images directly into the code so
/// that a) they load instantly and b) they can't get lost as a separate file.
class LgiClass LInlineBmp
{
public:
	/// The width of the image.
	int X;
	/// The height of the image.
	int Y;
	/// The bitdepth of the image (8, 15, 16, 24, 32).
	int Bits;
	/// Pointer to the raw data.
	uint32_t *Data;

	/// Creates a memory DC of the image.
	LSurface *Create(uint32_t TransparentPx = 0xffffffff);
};

// file filter support
#include "lgi/common/Filter.h"

// globals
#define GdcD			GdcDevice::GetInst()

/// Converts a context to a different bit depth
LgiFunc LSurface *ConvertDC
(
	/// The source image
	LSurface *pDC,
	/// The destination bit depth
	int Bits
);

/// Converts a colour to a different bit depth
LgiFunc COLOUR CBit(int DstBits, COLOUR c, int SrcBits = 24, GPalette *Pal = 0);

#ifdef __cplusplus
/// blends 2 colours by the amount specified
LgiClass LColour GdcMixColour(LColour a, LColour b, float HowMuchA = 0.5);
#endif

/// Colour reduction option to define what palette to go to
enum LColourReducePalette
{
	CR_PAL_NONE = -1,
    CR_PAL_CUBE = 0,
    CR_PAL_OPT,
    CR_PAL_FILE
};

/// Colour reduction option to define how to deal with reduction error
enum LColourReduceMatch
{
	CR_MATCH_NONE = -1,
    CR_MATCH_NEAR = 0,
    CR_MATCH_HALFTONE,
    CR_MATCH_ERROR
};

/// Colour reduction options
class LReduceOptions
{
public:
	/// Type of palette
	LColourReducePalette PalType;
	
	/// Reduction error handling
	LColourReduceMatch MatchType;
	
	/// 1-256
	int Colours;
	
	/// Specific palette to reduce to
	GPalette *Palette;

	LReduceOptions()
	{
		Palette = 0;
		Colours = 256;
		PalType = CR_PAL_NONE;
		MatchType = CR_MATCH_NONE;
	}
};

/// Reduces a images colour depth
LgiFunc bool LReduceBitDepth(LSurface *pDC, int Bits, GPalette *Pal = 0, LReduceOptions *Reduce = 0);

struct LColourStop
{
	LColour Colour;
	float Pos;
	
	void Set(float p, LColour c)
	{
		Pos = p;
		Colour = c;
	}
};

/// Draws a horizontal or vertical gradient
LgiFunc void LFillGradient(LSurface *pDC, LRect &r, bool Vert, LArray<LColourStop> &Stops);

#ifdef WIN32
/// Draws a windows HICON onto a surface at Dx, Dy
LgiFunc void LDrawIcon(LSurface *pDC, int Dx, int Dy, HICON ico);
#endif

/// Row copy operator for full RGB (8 bit components)
LgiFunc bool LRopRgb
(
	// Pointer to destination pixel buffer
	uint8_t *Dst,
	// Destination colour space (must be 8bit components)
	LColourSpace DstCs,
	// Pointer to source pixel buffer (if this overlaps 'Dst', set 'Overlap' to true)
	uint8_t *Src,
	// Source colour space (must be 8bit components)
	LColourSpace SrcCs,
	// Number of pixels to convert
	int Px,
	// Whether to composite using alpha or copy blt
	bool Composite
);

/// Universal bit blt method
LgiFunc bool LRopUniversal(LBmpMem *Dst, LBmpMem *Src, bool Composite);

/// Gets the screens DPI
LgiClass LPoint LScreenDpi();

/// Find the bounds of an image.
/// \return true if there is some non-transparent image	 in 'rc'
LgiFunc bool LFindBounds
(
	/// [in] The image
	LSurface *pDC,
	/// [in/out] Starts off as the initial bounds to search.
	/// Returns the non-background area.
	LRect *rc
);

#if defined(LGI_SDL)
LgiFunc LColourSpace PixelFormat2ColourSpace(SDL_PixelFormat *pf);
#endif

#endif
