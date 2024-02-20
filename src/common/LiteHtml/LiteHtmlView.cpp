
#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/Edit.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Layout.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Menu.h"
#include "lgi/common/LiteHtmlView.h"

#undef min
#undef max
#include "litehtml/html.h"

#define NOT_IMPL \
	printf("%s:%i - %s not impl.\n", _FL, __func__); \
	LAssert(!"not impl");


struct LiteHtmlViewPriv :
	public litehtml::document_container
{
	LiteHtmlView *view = NULL;
	LWindow *wnd = NULL;
	litehtml::document::ptr doc;
	LRect client;
	LString cursorName;
	LHashTbl<IntKey<litehtml::uint_ptr>, LFont*> fontMap;

	LiteHtmlViewPriv(LiteHtmlView *v) : view(v)
	{

	}

	~LiteHtmlViewPriv()
	{
		// Do this before releasing other owned objects, like the fontMap.
		doc.reset();
	}

	LColour Convert(const litehtml::web_color &c)
	{
		return LColour(c.red, c.green, c.blue, c.alpha);
	}

	LRect Convert(const litehtml::position &p)
	{
		LRect r(p.x, p.y, p.x + p.width - 1, p.y + p.height - 1);
		return r;
	}

	void UpdateScreen(litehtml::position::vector &redraw)
	{
		if (redraw.size() > 0)
		{
			// FIXME: should invalidate just the dirty regions...
			view->Invalidate();
		}
	}


	litehtml::uint_ptr create_font(	const char* faceName,
									int size,
									int weight,
									litehtml::font_style italic,
									unsigned int decoration,
									litehtml::font_metrics *fm)
	{
		litehtml::uint_ptr hnd;
		do 
		{
			hnd = LRand(10000);
		}
		while (fontMap.Find(hnd) != NULL);

		LFont *fnt = new LFont;
		bool status = fnt->Create(faceName, LCss::Len(LCss::LenPt, size) );
		if (!status)
			LgiTrace("%s:%i - failed to create font(%s,%i)\n", _FL, faceName, size);
		fontMap.Add(hnd, fnt);

		if (fm)
		{
			LDisplayString ds(fnt, "x");
			fm->height = fnt->GetHeight();
			fm->ascent = fnt->Ascent();
			fm->descent = fnt->Descent();
			fm->x_height = ds.Y();
			fm->draw_spaces = false;
		}

		return hnd;
	}

	void delete_font(litehtml::uint_ptr hFont)
	{
		auto fnt = fontMap.Find(hFont);
		if (fnt)
		{
			delete fnt;
			fontMap.Delete(hFont);
		}		
	}

	int text_width(const char* text, litehtml::uint_ptr hFont)
	{
		auto fnt = fontMap.Find(hFont);
		if (!fnt)
			return 0;
		
		LDisplayString ds(fnt, text);
		return ds.X();
	}
	
	void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos)
	{
		auto pDC = (LSurface*)hdc;
		auto Fnt = fontMap.Find(hFont);
		if (!pDC || !Fnt)
			return;

		LDisplayString ds(Fnt, text);
		Fnt->Fore(Convert(color));
		Fnt->Transparent(true);
		ds.Draw(pDC, pos.x, pos.y);
	}
	
	int pt_to_px(int pt) const
	{
		auto dpi = wnd->GetDpi();
		int px = pt * dpi.x / 72;
		return px;
	}

	int get_default_font_size() const
	{
		return 13; // LSysFont->PointSize();
	}

	const char *get_default_font_name() const
	{
		return LSysFont->Face();
	}

	void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker)
	{
		auto pDC = (LSurface*)hdc;
		auto Fnt = fontMap.Find(marker.font);
		if (!pDC)
			return;

		pDC->Colour(Convert(marker.color));
		switch (marker.marker_type)
		{
			case litehtml::list_style_type_disc:
			{
				pDC->FilledCircle(marker.pos.x, marker.pos.y, 3);
				break;
			}
			default:
			{
				NOT_IMPL;
				break;
			}
		}
	}

	void load_image(const char *src, const char *baseurl, bool redraw_on_ready)
	{
		NOT_IMPL
	}

	void get_image_size(const char *src, const char *baseurl, litehtml::size &sz)
	{
		NOT_IMPL
	}

	void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint> &background)
	{
		auto pDC = (LSurface*)hdc;
		for (auto b: background)
		{
			pDC->Colour(Convert(b.color));
			auto rc = Convert(b.border_box);
			pDC->Rectangle(&rc);
		}
	}

	void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root)
	{
		auto pDC = (LSurface*)hdc;
		auto drawEdge = [&](const litehtml::border &b, int x, int y, int dx, int dy, int ix, int iy)
		{
			pDC->Colour(Convert(b.color));
			for (int i=0; i<b.width; i++)
			{
				pDC->Line(x, y, x+dx, y+dy);
				x += ix;
				y += iy;
			}
		};

		int x2 = draw_pos.width - 1;
		int y2 = draw_pos.height - 1;
		drawEdge(borders.left,   draw_pos.x,    draw_pos.y,    0,  y2, 1,  0);
		drawEdge(borders.top,    draw_pos.x,    draw_pos.y,    x2, 0,  0,  1);
		drawEdge(borders.right,  draw_pos.x+x2, draw_pos.y,    0,  y2, -1, 0);
		drawEdge(borders.bottom, draw_pos.x,    draw_pos.y+y2, x2, 0,  0, -1);
	}

	void set_caption(const char* caption)
	{
		wnd->Name(caption);
	}

	void set_base_url(const char* base_url)
	{
		NOT_IMPL
	}

	void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el)
	{
		NOT_IMPL
	}

	void on_anchor_click(const char* url, const litehtml::element::ptr& el)
	{
		NOT_IMPL
	}

	void set_cursor(const char* cursor)
	{
		cursorName = cursor;
	}

	void transform_text(litehtml::string& text, litehtml::text_transform tt)
	{
		NOT_IMPL
	}

	void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl)
	{
		NOT_IMPL
	}

	void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius)
	{
		NOT_IMPL
	}

	void del_clip()
	{
		NOT_IMPL
	}

	void get_client_rect(litehtml::position &out) const
	{
		out = litehtml::position(client.x1, client.y1, client.X(), client.Y());
	}

	litehtml::element::ptr create_element(	const char* tag_name,
											const litehtml::string_map& attributes,
											const std::shared_ptr<litehtml::document>& doc)
	{
		return NULL;
	}

	void get_media_features(litehtml::media_features& media) const
	{
		// NOT_IMPL
	}

	void get_language(litehtml::string& language, litehtml::string& culture) const
	{
		NOT_IMPL
	}

	/*
	litehtml::string resolve_color(const litehtml::string &color) const
	{
		NOT_IMPL
		return litehtml::string();
	}
	*/
};

/////////////////////////////////////////////////////////////////
LiteHtmlView::LiteHtmlView(int id)
{
	d = new LiteHtmlViewPriv(this);
	SetId(id);
}

LiteHtmlView::~LiteHtmlView()
{
	delete d;
}

void LiteHtmlView::OnAttach()
{
	d->wnd = GetWindow();
}

LCursor LiteHtmlView::GetCursor(int x, int y)
{
	if (d->cursorName == "pointer")
		return LCUR_PointingHand;
	
	return LCUR_Normal;
}

bool LiteHtmlView::SetUrl(LString url)
{
	if (LFileExists(url))
	{
		auto html_text = LReadFile(url);
		if (html_text)
		{
			d->doc = litehtml::document::createFromString(html_text, d);
			Invalidate();
			return d->doc != NULL;
		}
	}
	
	return false;
}

void LiteHtmlView::OnPaint(LSurface *pDC)
{
	#ifdef WINDOWS
	LDoubleBuffer buf(pDC);
	#endif

	d->client = GetClient();
	if (d->doc)
	{
		pDC->Colour(L_WORKSPACE);
		pDC->Rectangle();

		auto width = pDC->X();
		int r = d->doc->render(width);
		if (r)
		{
			auto width = d->doc->content_width();
			auto height = d->doc->content_height();
			if (height > Y())
			{
				SetScrollBars(false, true);
				if (VScroll)
				{
					VScroll->SetRange(height);
					VScroll->SetPage(Y());
				}
			}

			litehtml::position clip(0, 0, pDC->X(), pDC->Y());
			d->doc->draw((litehtml::uint_ptr)pDC, 0, VScroll?-VScroll->Value():0, &clip);
		}
	}
	else
	{
		LLayout::OnPaint(pDC);
	}
}

int LiteHtmlView::OnNotify(LViewI *c, LNotification n)
{
	// printf("OnNotify %i=%i, %i=%i\n", c->GetId(), IDC_VSCROLL, n.Type, LNotifyValueChanged);
	if (c->GetId() == IDC_VSCROLL &&
		n.Type == LNotifyValueChanged)
	{
		// printf("Inval\n");
		Invalidate();
	}

	return LLayout::OnNotify(c, n);
}

void LiteHtmlView::OnMouseClick(LMouse &m)
{
	if (!d->doc)
		return;

	int64_t sx, sy;
	GetScrollPos(sx, sy);
	litehtml::position::vector redraw_boxes;
	
	if (m.IsContextMenu())
	{
		LSubMenu sub;
		sub.AppendItem("notImpl: submenu", -1, false);
		sub.Float(this, m);
	}
	else if (m.Left())
	{
		if (m.Down())
			d->doc->on_lbutton_down(m.x+sx, m.y+sy, m.x, m.y, redraw_boxes);
		else
			d->doc->on_lbutton_up(m.x+sx, m.y+sy, m.x, m.y, redraw_boxes);
	}

	d->UpdateScreen(redraw_boxes);
}

void LiteHtmlView::OnMouseMove(LMouse &m)
{
	if (!d->doc)
		return;

	int64_t sx, sy;
	GetScrollPos(sx, sy);
	litehtml::position::vector redraw_boxes;
	d->doc->on_mouse_over(m.x+sx, m.y+sy, m.x, m.y, redraw_boxes);

	d->UpdateScreen(redraw_boxes);
}
