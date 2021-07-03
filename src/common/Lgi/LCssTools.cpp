#include "lgi/common/Lgi.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"

struct CssImageCache
{
	GArray<GAutoPtr<GSurface>> Store;
	LHashTbl<ConstStrKey<char, false>, GSurface*> Map;

	GSurface *Get(const char *uri)
	{
		auto Uri = GString(uri).Strip("\'\"");
		auto i = Map.Find(Uri);
		if (i)
			return i;

		// Check theme folder first...
		GString File;
		auto Res = LgiGetResObj();
		auto ThemeFolder = Res ? Res->GetThemeFolder() : NULL;
		if (ThemeFolder)
		{
			GFile::Path p = ThemeFolder;
			p += Uri;
			if (p.Exists())
				File = p.GetFull();
		}

		// Then find it normally...
		if (!File)
			File = LFindFile(Uri);
		if (!File)
			return NULL;

		GAutoPtr<GSurface> img(GdcD->Load(File));
		if (!img)
			return NULL;

		Map.Add(Uri, img);
		auto &s = Store.New();
		s = img;
		return s;
	}
}	Cache;

GColour &GCssTools::GetFore(GColour *Default)
{
	if (!ForeInit)
	{
		ForeInit = 1;
		if (Default)
			Fore = *Default;
		else
			Fore = LColour(L_TEXT);

		if (View)
		{
			Fore = View->StyleColour(LCss::PropColor, Fore);
		}
		else if (Css)
		{
			LCss::ColorDef Fill = Css->Color();
			if (Fill.Type == LCss::ColorRgb)
				Fore.Set(Fill.Rgb32, 32);
			else if (Fill.Type == LCss::ColorTransparent)
				Fore.Empty();
		}
	}
	
	return Fore;
}
	
GColour &GCssTools::GetBack(GColour *Default, int Depth)
{
	if (!BackInit)
	{
		BackInit = 1;

		if (Default)
			Back = *Default;
		else
			Back = LColour(L_MED);

		if (View)
		{
			Back = View->StyleColour(LCss::PropBackgroundColor, Back, Depth >= 0 ? Depth : 6);
		}
		else if (Css)
		{
			LCss::ColorDef Fill = Css->BackgroundColor();
			if (Fill.Type == LCss::ColorRgb)
				Back.Set(Fill.Rgb32, 32);
			else if (Fill.Type == LCss::ColorTransparent)
				Back.Empty();
		}
	}
	
	return Back;
}

LRect GCssTools::ApplyMargin(LRect &in)
{
	if (!Css)
		return in;

	LRect r = in;
	
	// Insert by the margin
	LCss::Len margin = Css->Margin();
	#define DoMargin(name, edge, box, sign) \
		{ LCss::Len m = Css->Margin##name(); \
		r.edge sign (m.IsValid() ? &m : &margin)->ToPx(in.box(), Font); }
	DoMargin(Left, x1, X, +=)
	DoMargin(Top, y1, Y, +=)
	DoMargin(Right, x2, X, -=)
	DoMargin(Bottom, y2, Y, -=)
	
	return r;
}

LRect GCssTools::GetPadding(LRect &box, LRect *def)
{
	LRect r(0, 0, 0, 0);
	if (Css)
	{
		LCss::Len padding = Css->Padding();
		#define DoPadding(name, edge, dim) \
			{ \
				LCss::Len p = Css->Padding##name(); \
				auto v = p.IsValid() ? &p : &padding; \
				r.edge = v->IsValid() ? v->ToPx(box.dim(), Font) : (def ? def->edge : 0); \
			}
		DoPadding(Left, x1, X)
		DoPadding(Top, y1, Y)
		DoPadding(Right, x2, X)
		DoPadding(Bottom, y2, Y)
		#undef DoPadding
	}
	else if (def)
	{
		return *def;
	}
	
	return r;
}

LRect GCssTools::GetBorder(LRect &box, LRect *def)
{
	LRect r(0, 0, 0, 0);
	if (Css)
	{
		LCss::Len border = Css->Border();
		#define _(name, edge, dim) \
			{ \
				LCss::Len p = Css->Border##name(); \
				auto v = p.IsValid() ? &p : &border; \
				r.edge = v->IsValid() ? v->ToPx(box.dim(), Font) : (def ? def->edge : 0); \
			}
		_(Left, x1, X)
		_(Top, y1, Y)
		_(Right, x2, X)
		_(Bottom, y2, Y)
		#undef _
	}
	else if (def)
	{
		return *def;
	}
	
	return r;
}

LRect GCssTools::ApplyBorder(LRect &in)
{
	if (!Css)
		return in;

	// Insert by the padding
	LRect b = GetBorder(in);
	return LRect(in.x1 + b.x1,
				 in.y1 + b.y1,
				 in.x2 - b.x2,
				 in.y2 - b.y2);
}

LRect GCssTools::ApplyPadding(LRect &in)
{
	if (!Css)
		return in;

	// Insert by the padding
	LRect pad = GetPadding(in);
	return LRect(in.x1 + pad.x1,
				 in.y1 + pad.y1,
				 in.x2 - pad.x2,
				 in.y2 - pad.y2);
}

bool GCssTools::SetLineStyle(GSurface *pDC, LCss::BorderDef &b)
{
	if (b.Color.Type == LCss::ColorRgb)
	{
		pDC->Colour(b.Color.Rgb32, 32);
		switch (b.Style)
		{
			case LCss::BorderHidden:
				return false;
			case LCss::BorderDotted:
				pDC->LineStyle(GSurface::LineDot);
				break;
			case LCss::BorderDashed:
				pDC->LineStyle(GSurface::LineDash);
				break;
			default:
				pDC->LineStyle(GSurface::LineSolid);
				break;
		}
	}
	else return false;
	return true;
}

LRect GCssTools::PaintBorder(GSurface *pDC, LRect &in)
{
	LRect Content = in;
	if (!Css)
		return Content;

	// Draw the border
	LCss::BorderDef b = Css->Border();
	bool Drawn = false;
	switch (b.Style)
	{
		default:
		case LCss::BorderNone:
		case LCss::BorderHidden:
			// Do nothing
			break;
		case LCss::BorderInset: // Sunken
		{
			int Px = b.ToPx(Content.X(), Font);
			Drawn = true;
			if (Px == 1)
				LgiThinBorder(pDC, Content, DefaultSunkenEdge);
			else if (Px == 2)
				LgiWideBorder(pDC, Content, DefaultSunkenEdge);
			else
				LgiAssert(!"Unsupported sunken border width");
			break;
		}
		case LCss::BorderOutset: // Raised
		{
			int Px = b.ToPx(Content.X(), Font);
			Drawn = true;
			if (Px == 1)
				LgiThinBorder(pDC, Content, DefaultRaisedEdge);
			else if (Px == 2)
				LgiWideBorder(pDC, Content, DefaultRaisedEdge);
			else
				LgiAssert(!"Unsupported raised border width");
			break;
		}
		case LCss::BorderSolid:
		{
			int Px = b.ToPx(Content.X(), Font);
			if (SetLineStyle(pDC, b))
			{
				Drawn = true;
				for (int i=0; i<Px; i++)
				{
					pDC->Box(&Content);
					Content.Size(1, 1);
				}
			}
			break;
		}
		/*
		case BorderDotted:
			break;
		case BorderDashed:
			break;
		case BorderDouble:
			break;
		case BorderGroove:
			break;
		case BorderRidge:
			break;
		*/
	}

	if (!Drawn)
	{
		// Check for individual borders of different styles...
		LCss::BorderDef Top = Css->BorderTop();
		if (SetLineStyle(pDC, Top))
		{
			int Px = Top.ToPx(Content.Y(), Font);
			for (int i=0; i<Px; i++)
				pDC->HLine(Content.x1, Content.x2, Content.y1++);
		}
		LCss::BorderDef Bottom = Css->BorderBottom();
		if (SetLineStyle(pDC, Bottom))
		{
			int Px = Bottom.ToPx(Content.Y(), Font);
			for (int i=0; i<Px; i++)
				pDC->HLine(Content.x1, Content.x2, Content.y2--);
		}
		LCss::BorderDef Left = Css->BorderLeft();
		if (SetLineStyle(pDC, Left))
		{
			int Px = Left.ToPx(Content.X(), Font);
			for (int i=0; i<Px; i++)
				pDC->VLine(Content.x1++, Content.y1, Content.y2);
		}
		LCss::BorderDef Right = Css->BorderRight();
		if (SetLineStyle(pDC, Right))
		{
			int Px = Right.ToPx(Content.X(), Font);
			for (int i=0; i<Px; i++)
				pDC->VLine(Content.x2--, Content.y1, Content.y2);
		}
		
		pDC->LineStyle(GSurface::LineSolid);
	}

	return Content;
}

LRect GCssTools::PaintPadding(GSurface *pDC, LRect &in)
{
	LRect Content = in;
	if (!Css)
		return Content;

	// Insert by the padding
	LRect r = ApplyPadding(Content);
	if (r != Content)
	{
		// Draw the padding in the background colour
		GColour Background = GetBack();
		pDC->Colour(Background);

		if (r.x1 > Content.x1)
			pDC->Rectangle(Content.x1, Content.y1, r.x1 - 1, Content.y2);
		if (r.x2 < Content.x2)
			pDC->Rectangle(r.x2 + 1, Content.y1, Content.x2, Content.y2);
		if (r.y1 > Content.y1)
			pDC->Rectangle(r.x1, Content.y1, r.x2, r.y1 - 1);
		if (r.y2 < Content.y2)
			pDC->Rectangle(r.x1, r.y2 + 1, r.x2, Content.y2);
		
		Content = r;
	}
	
	return Content;
}

GSurface *GCssTools::GetCachedImage(const char *Uri)
{
	return Cache.Get(Uri);
}

bool GCssTools::Tile(GSurface *pDC, LRect in, GSurface *Img, int Ox, int Oy)
{
	if (!pDC || !Img)
		return false;

	for (int y=in.y1; y<in.y2; y+=Img->Y())
		for (int x=in.x1; x<in.x2; x+=Img->X())
			pDC->Blt(x - Ox, y - Oy, Img);

	return true;
}

GSurface *GCssTools::GetBackImage()
{
	LCss::ImageDef BackDef;
	auto Parent = View ? View->GetParent() : NULL;

	if (Css)
		BackDef = Css->BackgroundImage();
	if (BackDef.IsValid() && View)
		BackPos = View->GetClient();
	else if (Parent && Parent->GetCss())
	{
		BackDef = Parent->GetCss()->BackgroundImage();
		BackPos = View->GetPos();
	}

	if (BackDef.IsValid())
		BackImg = GetCachedImage(BackDef.Uri);

	return BackImg;
}

void GCssTools::PaintContent(GSurface *pDC, LRect &in, const char *utf8, GSurface *img)
{
	bool BackgroundDrawn = false;
	auto BkImg = GetBackImage();
	if (BkImg)
		BackgroundDrawn = Tile(pDC, in, BkImg, BackPos.x1, BackPos.y1);
	if (!BackgroundDrawn)
	{
		GColour Background = GetBack();
		if (Background)
		{
			pDC->Colour(Background);
			pDC->Rectangle(&in);
			BackgroundDrawn = true;
		}
	}

	if (utf8 || img)
	{
		GAutoPtr<LDisplayString> Ds;
		auto Fnt = View ? View->GetFont() : SysFont;
		if (utf8)
		{
			Ds.Reset(new LDisplayString(Fnt, utf8));
		}

		int Spacer = Ds && img ? 8 : 0;
		int Wid = Spacer + (Ds ? Ds->X() : 0) + (img ? img->X() : 0);
		int x = in.x1 + ((in.X() - Wid) >> 1);

		// Draw any icon
		if (img)
		{
			int y = in.y1 + ((in.Y() - img->Y()) >> 1);
			int Op = pDC->Op(GDC_ALPHA);
			pDC->Blt(x, y, img);
			pDC->Op(Op);
			x += img->X() + Spacer;
		}

		// Draw any text...
		if (Ds)
		{
			int y = in.y1 + ((in.Y() - Ds->Y()) >> 1);
			LCss::ColorDef Fore;
			if (Css)
				Fore = Css->Color();
			GColour ForeCol(L_TEXT);
			if (Fore.IsValid())
				ForeCol = Fore;
			Fnt->Fore(ForeCol);
			Fnt->Transparent(true);
			Ds->Draw(pDC, x, y);
		}
	}
}
