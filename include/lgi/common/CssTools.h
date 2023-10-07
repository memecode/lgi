#ifndef _GCSSTOOLS_H_
#define _GCSSTOOLS_H_

#include "lgi/common/Css.h"

class LgiClass LCssTools
{
	LView *View;
	LCss *Css;
	LFont *Font;
	
	LColour Fore, Back;
	uint8_t ForeInit : 1, BackInit : 1;
	LSurface *BackImg;
	LRect BackPos;
	
	bool SetLineStyle(LSurface *pDC, LCss::BorderDef &d);
	
public:
	LCssTools(LCss *css, LFont *font)
	{
		View = NULL;
		Css = css;
		Font = font;
		ForeInit = 0;
		BackInit = 0;
		BackImg = NULL;
	}
	
	LCssTools(LView *view)
	{
		LAssert(view != NULL);
		View = view;
		Css = view ? view->GetCss(true) : 0;
		Font = view ? view->GetFont() : 0;
		ForeInit = 0;
		BackInit = 0;
		BackImg = NULL;
	}
		
	/// Gets the foreground colour for text
	LColour &GetFore(LColour *Default = NULL);
	
	/// Gets the background colour for filling
	LColour &GetBack(LColour *Default = NULL, int Depth = -1);

	/// Gets the background image for filling
	LSurface *GetBackImage();

	/// Applies the margin to a rectangle
	LRect ApplyMargin(LRect &in);

	/// \returns the border in Px
	LRect GetBorder(LRect &box, LRect *def = NULL);
	
	/// Applies the border to a rectangle
	LRect ApplyBorder(LRect &in);

	/// \returns the padding in Px
	LRect GetPadding(LRect &box, LRect *def = NULL);
	
	/// Applies the padding to a rectangle
	LRect ApplyPadding(LRect &in);

	/// Paint the CSS border
	/// \returns the content area within the borders
	LRect PaintBorder(LSurface *pDC, LRect &in);

	/// Paint the CSS padding
	/// \returns the content area within the padding
	LRect PaintPadding(LSurface *pDC, LRect &in, LColour *DefaultColour = NULL);
	
	LRect PaintBorderAndPadding(LSurface *pDC, LRect &in)
	{
		LRect r = PaintBorder(pDC, in);
		r = PaintPadding(pDC, r);
		return r;
	}
	
	/// Paint the content area
	void PaintContent(LSurface *pDC, LRect &in, const char *utf8 = NULL, LSurface *img = NULL);

	/// Tiles an image in the space 'in'
	bool Tile(LSurface *pDC, LRect in, LSurface *Img, int Ox = 0, int Oy = 0);

	/// Get cached image
	LSurface *GetCachedImage(const char *Uri);
};

#endif