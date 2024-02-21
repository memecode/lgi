
#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/Edit.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Layout.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Menu.h"
#include "lgi/common/LiteHtmlView.h"
#include "lgi/common/Thread.h"
#include "lgi/common/Http.h"
#include "lgi/common/PopupNotification.h"
#include "lgi/common/Path.h"

#undef min
#undef max
#include "litehtml/html.h"

#define NOT_IMPL \
	LgiTrace("%s:%i - %s not impl.\n", _FL, __func__);

struct LiteHtmlViewPriv :
	public litehtml::document_container,
	public LCancel
{
	struct NetworkThread :
		public LThread,
		public LCancel
	{
		using TCallback = std::function<void(bool status, LString data)>;
	
		LView *view = NULL;
		LString url;
		TCallback callback;

		NetworkThread(LView *View, const char *Url, TCallback cb) :
			LThread("LiteHtmlViewPriv.NetworkThread")
		{
			view = View;
			url = Url;
			callback = std::move(cb);
			Run();
		}

		~NetworkThread()
		{
			Cancel();
			WaitForExit();
		}

		int Main()
		{
			LStringPipe p;
			LString err;

			auto result = LgiGetUri(this, &p, &err, url);
			// LgiTrace("net(%s) = %i\n", url.Get(), result);			
			view->RunCallback([result, data=p.NewLStr(), this]()
			{
				callback(result, data);
			});

			return 0;
		}
	};

	struct Image
	{
		int status = -1;
		LString uri;
		LAutoPtr<LSurface> img;
	};

	LArray<NetworkThread*> threads;
	LiteHtmlView *view = NULL;
	LWindow *wnd = NULL;
	litehtml::document::ptr doc;
	LRect client;
	LString cursorName;
	LString currentUrl;
	LHashTbl<IntKey<litehtml::uint_ptr>, LFont*> fontMap;
	LHashTbl<ConstStrKey<char, false>, Image*> imageCache;

	LiteHtmlViewPriv(LiteHtmlView *v) : view(v)
	{
	}

	~LiteHtmlViewPriv()
	{
		Cancel();
		threads.DeleteObjects();

		// Do this before releasing other owned objects, like the fontMap.
		doc.reset();

		// Clean up caches
		imageCache.DeleteObjects();
		fontMap.DeleteObjects();
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

		auto faceNames = LString(faceName).SplitDelimit(",");

		printf("create_font('%s', %i, %i, %i, %i)\n",
			faceName,
			size,
			weight,
			italic,
			decoration);

		LFont *fnt = new LFont;
		fnt->Bold(weight > 400);
		if (italic == litehtml::font_style_italic)
			fnt->Italic(true);

		for (auto face: faceNames)
		{
			bool status = fnt->Create(face, LCss::Len(LCss::LenPt, size) );
			if (status)
				break;
			LgiTrace("%s:%i - failed to create font(%s,%i)\n", _FL, faceName, size);
		}
		fontMap.Add(hnd, fnt);

		if (fm)
		{
			LDisplayString ds(fnt, "x");
			fm->height = fnt->GetHeight();
			fm->ascent = ceil(fnt->Ascent());
			fm->descent = ceil(fnt->Descent());
			fm->x_height = ds.Y();
			fm->draw_spaces = false;

			printf("\tht=%i as=%i de=%i x=%i\n", 
				fm->height, fm->ascent, fm->descent, fm->x_height);
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
		return LSysFont->PointSize() * 1.1;
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
			case litehtml::list_style_type_none:
				break;
			case litehtml::list_style_type_circle:
				pDC->Circle(marker.pos.x, marker.pos.y, 3);
				break;
			case litehtml::list_style_type_disc:
				pDC->FilledCircle(marker.pos.x, marker.pos.y, 3);
				break;
			case litehtml::list_style_type_square:
				pDC->Box(marker.pos.x-2, marker.pos.y-2, marker.pos.x+2, marker.pos.y+2);
				break;
			default:
				LgiTrace("%s:%i - draw_list_marker %i not impl\n", marker.marker_type);
				break;
		}
	}

	LString FullUri(const char *src, const char *baseurl)
	{
		LUri s(src);
		if (s.sProtocol)
			return src;

		if (s.sPath)
		{
			LUri c(currentUrl);
			c += s.sPath;
			return c.ToString();
		}

		return LString();
	}

	void load_image(const char *src, const char *baseurl, bool redraw_on_ready)
	{
		auto absUri = FullUri(src, baseurl);
		if (!absUri)
			return;

		if (!imageCache.Find(absUri))
		{
			LgiTrace("load_image(%s) %s + %s\n", absUri.Get(), currentUrl.Get(), src);
			if (auto i = new Image)
			{
				i->uri = absUri;
				imageCache.Add(absUri, i);
				threads.Add(new NetworkThread(view, absUri, [i, this, redraw_on_ready](auto status, auto data)
				{
					auto leaf = i->uri.SplitDelimit("/").Last();
					if (status)
					{
						LMemStream s(data.Get(), data.Length(), false);
						i->status = i->img.Reset(GdcD->Load(&s, leaf));
						LgiTrace("load_image(%s) load status=%i\n", i->uri.Get(), i->status);
						if (i->status && redraw_on_ready)
							view->Invalidate();
					}
					else
					{
						i->status = false;
						LgiTrace("load_image(%s) network status=%i\n", i->uri.Get(), i->status);
					}
				}));
			}
		}
	}

	void get_image_size(const char *src, const char *baseurl, litehtml::size &sz)
	{
		auto absUri = FullUri(src, baseurl);
		if (!absUri)
			return;

		if (auto i = imageCache.Find(absUri))
		{
			if (i->img)
			{
				sz.width = i->img->X();
				sz.height = i->img->Y();
			}
		}
	}

	void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint> &background)
	{
		auto pDC = (LSurface*)hdc;
		for (auto b: background)
		{
			auto rc = Convert(b.border_box);
			if (!b.image.empty())
			{
				auto absUri = FullUri(b.image.c_str(), b.baseurl.c_str());
				if (auto i = imageCache.Find(absUri))
				{
					if (i->img)
					{
						auto op = pDC->Op(GDC_ALPHA);
						pDC->Blt(b.position_x, b.position_y, i->img);
						pDC->Op(op);
					}
					else LgiTrace("%s:%i - draw_background(img=%s) img no surface\n", _FL, b.image.c_str());
				}
				else LgiTrace("%s:%i - draw_background(img=%s) img not found\n", _FL, b.image.c_str());
			}
			else if (!b.gradient.is_empty())
			{
				if (b.gradient.m_type == litehtml::web_gradient::linear_gradient &&
					b.gradient.m_colors.size() == 2)
				{
					LMemDC mem(rc.X(), rc.Y(), System32BitColourSpace);
					LBlendStop stops[2] = {
						{0.0, Convert(b.gradient.m_colors[0]).c32()},
						{1.0, Convert(b.gradient.m_colors[1]).c32()}
					};
					LLinearBlendBrush brush(LPointF(0.0, 0.0), LPointF(0.0, rc.Y()), 2, stops);
					LPath path;
					path.Rectangle(0.0, 0.0, mem.X(), mem.Y());
					path.Fill(&mem, brush);
					pDC->Blt(rc.x1, rc.y1, &mem);
				}
				else LgiTrace("%s:%i - Invalid gradient.\n", _FL);
			}
			else
			{
				pDC->Colour(Convert(b.color));
				pDC->Rectangle(&rc);
			}
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
		currentUrl = base_url;
	}

	void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el)
	{
		NOT_IMPL
	}

	void on_anchor_click(const char* url, const litehtml::element::ptr& el)
	{
		if (!url)
			return;

		auto full = FullUri(url, NULL);

		threads.Add(new NetworkThread(view, full, [this, full](auto status, auto data)
		{
			if (data)
			{
				currentUrl = full;
				doc = litehtml::document::createFromString(data, this);
				view->OnNavigate(full);
				view->Invalidate();
			}
			else LPopupNotification::Message(wnd, LString::Fmt("No data for '%s'", full.Get()));

		}));
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
		LUri cur(currentUrl);
		LUri u(url.c_str());
		LString newUri;

		if (u.sProtocol && u.sHost)
		{
			// Absolute URI
			newUri = url.c_str();
		}
		else if (char *s = u.sPath.Get())
		{
			// Relative URI
			if (*s == '/')
				cur.sPath.Empty();
			cur.sPath = u.sPath;
			newUri = cur.ToString();
		}

		if (newUri)
		{
			LStringPipe out;
			LString err;			
			if (LgiGetUri(this, &out, &err, newUri))
			{
				text = out.NewLStr().Get();
			}
			else LgiTrace("%s:%i - error: LgiGetUri(%s)=%s (currentUrl=%s)\n",
				_FL, newUri.Get(), err.Get(), currentUrl.Get());
		}
		else LgiTrace("%s:%i - error: no uri for loading css.\n", _FL);
	}

	void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius)
	{
		// NOT_IMPL
	}

	void del_clip()
	{
		// NOT_IMPL
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

void LiteHtmlView::OnNavigate(LString url)
{
	d->currentUrl = url;
	Invalidate();
}

bool LiteHtmlView::SetUrl(LString url)
{
	LUri u(url);
	if (u.IsProtocol("file") ||
		(!u.sProtocol && LFileExists(url)))
	{
		auto html_text = LReadFile(url);
		if (html_text)
		{
			d->currentUrl = url;
			d->doc = litehtml::document::createFromString(html_text, d);
			OnNavigate(url);
			return d->doc != NULL;
		}
	}
	else
	{
		litehtml::element::ptr elem;
		d->on_anchor_click(url, elem);
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
	// LgiTrace("OnNotify %i=%i, %i=%i\n", c->GetId(), IDC_VSCROLL, n.Type, LNotifyValueChanged);
	if (c->GetId() == IDC_VSCROLL &&
		n.Type == LNotifyValueChanged)
	{
		// LgiTrace("Inval\n");
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
