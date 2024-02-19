#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/Edit.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Layout.h"
#include "lgi/common/ScrollBar.h"

#undef min
#undef max
#include "litehtml/html.h"

const char *AppName = "LgiLiteHtml";

enum Ctrls
{
	IDC_BOX = 100,
	IDC_LOCATION,
	IDC_BROWSER,
};

#define NOT_IMPL LAssert(!"not impl");

class HtmlView :
	public LLayout,
	public litehtml::document_container
{
	litehtml::document::ptr doc;

protected:
	LWindow *wnd = NULL;
	LRect client;
	LHashTbl<IntKey<litehtml::uint_ptr>, LFont*> fontMap;

	LColour Convert(const litehtml::web_color &c)
	{
		return LColour(c.red, c.green, c.blue, c.alpha);
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
		return 12; // LSysFont->PointSize();
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

	void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint> &bg)
	{
		NOT_IMPL
	}

	void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root)
	{
		auto pDC = (LSurface*)hdc;
		NOT_IMPL
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
		NOT_IMPL
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
	
public:
	HtmlView(int id)
	{
		SetId(id);
	}

	~HtmlView()
	{
		// Do this before releasing other owned objects, like the fontMap.
		doc.reset();
	}

	void OnAttach()
	{
		wnd = GetWindow();
	}

	bool SetUrl(LString url)
	{
		if (LFileExists(url))
		{
			auto html_text = LReadFile(url);
			if (html_text)
			{
				doc = litehtml::document::createFromString(html_text, this);
				Invalidate();
				return doc != NULL;
			}
		}
		
		return false;
	}

	void OnPaint(LSurface *pDC)
	{
		#ifdef WINDOWS
		LDoubleBuffer buf(pDC);
		#endif

		client = GetClient();
		if (doc)
		{
			pDC->Colour(L_WORKSPACE);
			pDC->Rectangle();

			auto width = pDC->X();
			int r = doc->render(width);
			if (r)
			{
				auto width = doc->content_width();
				auto height = doc->content_height();
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
				doc->draw((litehtml::uint_ptr)pDC, 0, VScroll?-VScroll->Value():0, &clip);
			}
		}
		else
		{
			LLayout::OnPaint(pDC);
		}
	}

	int OnNotify(LViewI *c, LNotification n)
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
};

class App : public LWindow
{
	LBox *box = NULL;
	LEdit *location = NULL;
	HtmlView *browser = NULL;

public:
	App()
	{
		LRect r(200, 200, 1400, 1000);
		SetPos(r);
		Name(AppName);
		MoveToCenter();
		SetQuitOnClose(true);

		if (Attach(0))
		{
			AddView(box = new LBox(IDC_BOX, true));
			box->AddView(location = new LEdit(IDC_LOCATION, 0, 0, 100, 20));
			box->AddView(browser = new HtmlView(IDC_BROWSER));
			box->Value(LSysFont->GetHeight() + 8);
			AttachChildren();
			location->Focus(true);
			Visible(true);
		}
	}

	void SetUrl(LString s)
	{
		if (location)
			location->Name(s);
		if (browser)
			browser->SetUrl(s);
	}

	void OnReceiveFiles(LArray<const char *> &Files)
	{
		SetUrl(Files[0]);
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	LApp app(AppArgs, "application/x-lgi-litehtml");
	if (app.IsOk())
	{
		app.AppWnd = new App;
		app.Run();
	}
	return 0;
}
