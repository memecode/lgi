#ifndef _GCSSTOOLS_H_
#define _GCSSTOOLS_H_

#include "GCss.h"

class LgiClass GCssTools
{
	GView *View;
	GCss *Css;
	GFont *Font;
	
	GColour Fore, Back;
	uint8_t ForeInit : 1, BackInit : 1;
	GSurface *BackImg;
	GRect BackPos;
	
	bool SetLineStyle(GSurface *pDC, GCss::BorderDef &d);
	
public:
	GCssTools(GCss *css, GFont *font)
	{
		View = NULL;
		Css = css;
		Font = font;
		ForeInit = 0;
		BackInit = 0;
		BackImg = NULL;
	}
	
	GCssTools(GView *view)
	{
		View = view;
		Css = view->GetCss(true);
		Font = view->GetFont();
		ForeInit = 0;
		BackInit = 0;
		BackImg = NULL;
	}
		
	/// Gets the foreground colour for text
	GColour &GetFore(GColour *Default = NULL);
	
	/// Gets the background colour for filling
	GColour &GetBack(GColour *Default = NULL);

	/// Gets the background image for filling
	GSurface *GetBackImage();

	/// Applies the margin to a rectangle
	GRect ApplyMargin(GRect &in);

	/// \returns the border in Px
	GRect GetBorder(GRect &box, GRect *def = NULL);
	
	/// Applies the border to a rectangle
	GRect ApplyBorder(GRect &in);

	/// \returns the padding in Px
	GRect GetPadding(GRect &box, GRect *def = NULL);
	
	/// Applies the padding to a rectangle
	GRect ApplyPadding(GRect &in);

	/// Paint the CSS border
	/// \returns the content area within the borders
	GRect PaintBorder(GSurface *pDC, GRect &in);

	/// Paint the CSS padding
	/// \returns the content area within the padding
	GRect PaintPadding(GSurface *pDC, GRect &in);
	
	GRect PaintBorderAndPadding(GSurface *pDC, GRect &in)
	{
		GRect r = PaintBorder(pDC, in);
		r = PaintPadding(pDC, r);
		return r;
	}
	
	/// Paint the content area
	void PaintContent(GSurface *pDC, GRect &in, const char *utf8 = NULL, GSurface *img = NULL);

	/// Tiles an image in the space 'in'
	bool Tile(GSurface *pDC, GRect in, GSurface *Img, int Ox = 0, int Oy = 0);

	/// Get cached image
	GSurface *GetCachedImage(const char *Uri);
};

#endif