#pragma once

#include <cairo.h>

#ifdef __GTK_H__
using Gtk::cairo_surface_t;
#endif
class LCairoSurfaceT
{
	cairo_surface_t *surface;

public:

	LCairoSurfaceT(cairo_surface_t *s = NULL)
	{
		surface = s;
	}
	
	~LCairoSurfaceT()
	{
		Empty();
	}
	
	void Empty()
	{
		if (surface)
		{
			cairo_surface_destroy(surface);
			surface = NULL;
		}
	}
	
	operator cairo_surface_t*()
	{
		return surface;
	}
	
	LCairoSurfaceT &operator =(cairo_surface_t *s)
	{
		if (s != surface)
		{
			Empty();
			surface = s;
		}
		return *this;
	}
};

/// \brief An implemenation of GSurface to draw into a memory bitmap.
///
/// This class uses a block of memory to represent an image. You have direct
/// pixel access as well as higher level functions to manipulate the bits.
class LCairoSurface : public GSurface
{
protected:
	class LCairoSurfacePriv *d;

	#if defined WINNATIVE
	PBITMAPINFO	GetInfo();
	#endif
	
	// This is called between capturing the screen and overlaying the
	// cursor in LCairoSurface::Blt(x, y, ScreenDC, Src). It can be used to
	// overlay effects between the screen and cursor layers.
	virtual void OnCaptureScreen() {}

public:
	/// Creates a memory bitmap
	LCairoSurface
	(
		/// The width
		int x = 0,
		/// The height
		int y = 0,
		/// The colour space to use. CsNone will default to the 
		/// current screen colour space.
		GColourSpace cs = CsNone,
		/// Optional creation flags
		int Flags = SurfaceCreateNone
	);
	LCairoSurface(GSurface *pDC);
	virtual ~LCairoSurface();

	#if WINNATIVE
	
		// HDC StartDC();
		// void EndDC();
		// void Update(int Flags);
		void UpsideDown(bool upsidedown);
		
	#else

		LRect ClipRgn() { return Clip; }

		#if defined(__GTK_H__)

			/// This returns the surface owned by the LCairoSurface
			Gtk::cairo_surface_t *GetSurface();

			/// This returns a sub-image, caller is responsible to free via
			/// calling cairo_surface_destroy
			LCairoSurface GetSubImage(LRect &r);
			
			Gtk::GdkPixbuf *CreatePixBuf();

		#elif defined MAC
				
			OsBitmap GetBitmap();
	
			#if LGI_COCOA && defined(__OBJC__)
			LCairoSurface(NSImage *img);
			NSImage *NsImage(LRect *rc = NULL);
			#endif
	
			#if !defined(LGI_SDL)
				CGColorSpaceRef GetColourSpaceRef();
				CGImg *GetImg(LRect *Sub = 0, int Debug = 0);
			#endif
		
		#elif defined(LGI_SDL)

			OsBitmap GetBitmap();

		#endif

		OsPainter Handle();
		
	#endif

	// Set new clipping region
	LRect ClipRgn(LRect *Rgn);
	void SetClient(LRect *c);
	LPoint GetSize();
	GColourSpace GetCreateCs();

	/// Locks the bits for access. LCairoSurface's start in the locked state.
	bool Lock();
	/// Unlocks the bits to optimize for display. While the bitmap is unlocked you
	/// can't access the data for read or write. On linux this converts the XImage
	/// to pixmap. On other systems it doesn't do much. As a general rule if you
	/// don't need access to a bitmap after creating / loading it then unlock it.
	bool Unlock();
	
	void GetOrigin(int &x, int &y);
	void SetOrigin(int x, int y);
	void Empty();
	bool SupportsAlphaCompositing();
	bool SwapRedAndBlue();
	
	bool Create(int x, int y, GColourSpace Cs, int Flags = SurfaceCreateNone);
	void Blt(int x, int y, GSurface *Src, LRect *a = NULL);
	void StretchBlt(LRect *d, GSurface *Src, LRect *s = NULL);

	void HorzLine(int x1, int x2, int y, COLOUR a, COLOUR b);
	void VertLine(int x, int y1, int y2, COLOUR a, COLOUR b);
};
