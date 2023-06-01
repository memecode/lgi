// Clipboard Implementation
#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/ClipBoard.h"

#define DEBUG_CLIPBOARD					0
#define VAR_COUNT						16
#define LGI_CLIP_BINARY					"lgi.binary"
#define LGI_RECEIVE_CLIPBOARD_TIMEOUT	4000

using namespace Gtk;

struct ClipData : public LMutex
{
	::LVariant v[VAR_COUNT];
	
	ClipData() : LMutex("ClipData")
	{
	}
	
}	Data;

class LClipBoardPriv
{
public:
	GtkClipboard *c;
};

static LAutoPtr<LMemDC> Img;

///////////////////////////////////////////////////////////////////////////////////////////////
LClipBoard::LClipBoard(LView *o)
{
	d = new LClipBoardPriv;
	Owner = o;
	Open = false;
	d->c = gtk_clipboard_get(GDK_NONE); // gdk_atom_intern("CLIPBOARD", false)
	if (d->c)
		Open = true;
	#if DEBUG_CLIPBOARD
	printf("d->c = %i\n", d->c);
	#endif
}

LClipBoard::~LClipBoard()
{
	d->c = 0;
	DeleteObj(d);
}

bool LClipBoard::Empty()
{
	if (d->c)
	{
		gtk_clipboard_clear(d->c);
		#if DEBUG_CLIPBOARD
		printf("gtk_clipboard_clear(%i)\n", d->c);
		#endif
		return true;
	}

	return false;
}

bool LClipBoard::EnumFormats(::LArray<FormatType> &Formats)
{
	return false;
}

bool LClipBoard::Html(const char *doc, bool AutoEmpty)
{
	return false;
}

::LString LClipBoard::Html()
{
	return ::LString();
}

bool LClipBoard::Text(const char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}
	
	if (Str && d->c)
	{
		gtk_clipboard_set_text(d->c, Str, strlen(Str));
		#if DEBUG_CLIPBOARD
		printf("gtk_clipboard_set_text(%i,%s,%i)\n", d->c, Str, strlen(Str));
		#endif
		Status = true;
	}

	return Status;
}

char *LClipBoard::Text()
{
	char *t = 0;
	
	if (d->c)
	{
		#if DEBUG_CLIPBOARD
		printf("gtk_clipboard_wait_for_text starting...\n");
		#endif
		gchar *txt = gtk_clipboard_wait_for_text(d->c);
		#if DEBUG_CLIPBOARD
		printf("gtk_clipboard_wait_for_text(%i)='%s'\n", d->c, txt);
		#endif
		if (txt)
		{
			t = NewStr(txt);
			g_free(txt);
		}
	}
	
	return t;
}

bool LClipBoard::TextW(const char16 *Str, bool AutoEmpty)
{
	LAutoString u(WideToUtf8(Str));
	return Text(u, AutoEmpty);
}

char16 *LClipBoard::TextW()
{
	LAutoString u(Text());
	return Utf8ToWide(u);
}

void LClipBoard::FreeImage(unsigned char *pixels)
{
	if (Img)
	{
		if (pixels == (*Img)[0])
			Img.Reset();
		else
			LgiTrace("%s:%i - LClipBoard::FreeImage wrong ptr.\n", _FL);
	}
}

bool LClipBoard::Bitmap(LSurface *pDC, bool AutoEmpty)
{
	if (!pDC || !d->c)
	{
		LAssert(!"Param error.");
		return false;
	}
	
	if (!Img.Reset(new LMemDC))
	{
		LAssert(!"Can't create surface...");
		return false;
	}
	
	if (!Img->Create(pDC->X(), pDC->Y(), CsArgb32))
	{
		LAssert(!"Can't create surface copy...");
		return false;
	}
	
	// We have to create a copy to get the byte order right:
	Img->Blt(0, 0, pDC);
	
	// Img->SwapRedAndBlue();
	// Img->Colour(LColour(0xff, 0, 0));
	// Img->Rectangle();
	
	// And also if the caller free's their copy of the image before the pixbuf is done it'll crash...	
	auto pb = gdk_pixbuf_new_from_data(	(*Img)[0],
										GDK_COLORSPACE_RGB,
										LColourSpaceHasAlpha(Img->GetColourSpace()),
										8,
										Img->X(),
										Img->Y(),
										Img->GetRowStep(),
										[](auto pixels, auto obj)
										{
											auto This = (LClipBoard*)obj;
											This->FreeImage(pixels);
										},
										this);
	if (!pb)
	{
		if (Img)
		{
			LgiTrace("%s:%i - gdk_pixbuf_new_from_data  failed for %s, params:\n"
					"ptr: %p, alpha: %i, bits: %i, size: %ix%i, row: %i\n",
					LColourSpaceToString(Img->GetColourSpace()),
					(*Img)[0], LColourSpaceHasAlpha(Img->GetColourSpace()),
					Img->GetBits(), Img->X(), Img->Y(), Img->GetRowStep());
		}
		LAssert(!"gdk_pixbuf_new_from_data failed.");
		return false;
	}	

	gtk_clipboard_set_image(d->c, pb);
	return true; // have to assume it worked...
}

static void ClipboardImageReceived(GtkClipboard *Clipboard, GdkPixbuf *Img, LClipBoard::BitmapCb *Cb)
{
	auto chan = gdk_pixbuf_get_n_channels(Img);
	auto alpha = gdk_pixbuf_get_has_alpha(Img);
	LColourSpace cs = System32BitColourSpace;
	LAutoPtr<LSurface> Out;

	if (chan == 4)
	{
		cs = System32BitColourSpace;
	}
	else if (chan == 3)
	{
		if (alpha)
			cs = System32BitColourSpace;
		else
			cs = System24BitColourSpace;
	}
	else if (chan == 1)
	{
		cs = CsIndex8;
	}
	else
	{
		LString s;
		s.Printf("Unexpected colourspace: %i channels.", (int)chan);
		(*Cb)(Out, s);
		delete Cb;
		return;
	}

	auto x = gdk_pixbuf_get_width(Img), y = gdk_pixbuf_get_height(Img);
	LAutoPtr<LMemDC> m(new LMemDC(x, y, cs));
	if (!m)
	{
		(*Cb)(Out, "Alloc failed");
		delete Cb;
		return;
	}
	
	auto px = gdk_pixbuf_get_pixels(Img);
	auto row = gdk_pixbuf_get_rowstride(Img);
	for (int yy=0; yy<y; yy++)
	{
		#define Rop24(out_cs, in_cs) \
			case Cs##out_cs: \
			{ \
				auto in = (L##in_cs*)(px + (yy*row)); \
				auto out = (L##out_cs*) ((*m)[yy]); \
				auto end = out + x; \
				while (out < end) \
				{ \
					out->r = in->r; out->g = in->g; out->b = in->b; \
					out++; in++; \
				} \
				break; \
			}
		#define Rop32(out_cs, in_cs) \
			case Cs##out_cs: \
			{ \
				auto in = (L##in_cs*)(px + (yy*row)); \
				auto out = (L##out_cs*) (*m)[yy]; \
				auto end = out + x; \
				while (out < end) \
				{ \
					out->r = in->r; out->g = in->g; out->b = in->b; out->a = in->a; \
					out++; in++; \
				} \
				break; \
			}

		switch (m->GetColourSpace())
		{
			Rop24(Bgr24, Rgb24);
			Rop24(Rgb24, Rgb24);

			Rop24(Bgrx32, Rgb24);
			Rop24(Rgbx32, Rgb24);
			Rop24(Xrgb32, Rgb24);
			Rop24(Xbgr32, Rgb24);
			
			Rop32(Bgra32, Rgba32);
			Rop32(Rgba32, Rgba32);
			Rop32(Argb32, Rgba32);
			Rop32(Abgr32, Rgba32);

			default:
				LAssert(!"Unsupported colour space.");
				yy = y;
				break;
		}
	}

	Out.Reset(m.Release());
	(*Cb)(Out, LString());
	delete Cb;
}

void LClipBoard::Bitmap(LClipBoard::BitmapCb Callback)
{
	if (!Callback)
		return;
		
	gtk_clipboard_request_image(d->c, (GtkClipboardImageReceivedFunc) ClipboardImageReceived, new LClipBoard::BitmapCb(Callback));
}
		
void LgiClipboardGetFunc(GtkClipboard *clipboard,
                        GtkSelectionData *data,
                        guint info,
                        gpointer user_data)
{
	if (Data.Lock(_FL))
	{
		::LVariant *p = (::LVariant*)user_data;
		#if DEBUG_CLIPBOARD
		printf("%s:%i - LgiClipboardGetFunc: %p, %i\n", _FL, p, info);
		#endif

		switch (info)
		{
			case GV_BINARY:
			{
				if (p->Type == info)
				{
					// data->data = p->Value.Binary.Data;
					// data->length = p->Value.Binary.Length;
  					gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8, (guchar*)p->Value.Binary.Data, p->Value.Binary.Length);
				}
				else LgiTrace("%s:%i - Variant is the wrong type: %i\n", _FL, p->Type);
				break;
			}
			default:
			{
				LgiTrace("%s:%i - Undefined data type: %i\n", _FL, info);
				break;
			}
		}
		
		Data.Unlock();
	}
}

void LgiClipboardClearFunc(GtkClipboard *clipboard,
                          gpointer user_data)
{
	if (Data.Lock(_FL))
	{
		::LVariant *p = (::LVariant*)user_data;
		#if DEBUG_CLIPBOARD
		printf("%s:%i - LgiClipboardClearFunc: %i\n", _FL, p->Type);
		#endif
		p->Empty();
		Data.Unlock();
	}
}

bool LClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	if (!Ptr || Len <= 0)
		return false;

	::LVariant *p = NULL;
	if (Data.Lock(_FL))
	{
		for (int i=0; i<VAR_COUNT; i++)
		{
			if (Data.v[i].Type == GV_NULL)
			{
				p = Data.v + i;
				p->SetBinary(Len, Ptr);
				break;
			}
		}
		Data.Unlock();
	}
	
	if (!p)
	{
		#if DEBUG_CLIPBOARD
		printf("%s:%i - no slots to store data\n", _FL);
		#endif
		return false;
	}
	
	GtkTargetEntry te;
	te.target = (char*)LGI_CLIP_BINARY;
	te.flags = 0; // GTK_TARGET_SAME_APP?
	te.info = GV_BINARY; // App defined data type ID
	Gtk::gboolean r = gtk_clipboard_set_with_data(d->c,
					                             &te,
					                             1,
					                             LgiClipboardGetFunc,
					                             LgiClipboardClearFunc,
					                             p);


	#if DEBUG_CLIPBOARD
	printf("%s:%i - gtk_clipboard_set_with_data = %i\n", _FL, r);
	#endif
	
	return r;
}

::LString::Array LClipBoard::Files()
{
	::LString::Array a;
	return a;
}

bool LClipBoard::Files(::LString::Array &a, bool AutoEmpty)
{
	return false;
}

struct ReceiveData
{
	LAutoPtr<uint8_t,true> *Ptr;
	ssize_t *Len;
};

static void ClipboardBinaryReceived(GtkClipboard *clipboard,
            	                 	GtkSelectionData *data,
                	             	gpointer user_data)
{
	ReceiveData *r = (ReceiveData*)	user_data;
	if (!data || !r)
	{
		LgiTrace("%s:%i - Missing ptr: %p %p\n", _FL, data, r);
		return;
	}

	auto Bytes = gtk_selection_data_get_length(data);
	if (Bytes < 0)
	{
		LgiTrace("%s:%i - No data? (%i)\n", _FL, Bytes);
		return;
	}

	uint8_t *d = new uint8_t[Bytes];
	if (!d)
	{
		LgiTrace("%s:%i - Alloc failed %i\n", _FL, Bytes);
		return;
	}

	memcpy(d, gtk_selection_data_get_data(data), Bytes);
	if (r->Len)
		*r->Len = Bytes;
	r->Ptr->Reset(d);

	#if DEBUG_CLIPBOARD
	printf("%s:%i - LgiClipboardReceivedFunc\n", _FL);
	#endif
}

void LClipBoard::Binary(FormatType Format, BinaryCb Callback)
{
	if (!Callback)
		return;
		
	gtk_clipboard_request_contents(
		// The clipboard
		d->c,
		// The atom to return
        gdk_atom_intern(LGI_CLIP_BINARY, false),
        // Lambda callback to receive the data
        [](auto clipboard, auto data, auto ptr)
        {
        	LAutoPtr<BinaryCb> cb((BinaryCb*)ptr);
        	
			auto Bytes = gtk_selection_data_get_length(data);
			if (Bytes < 0)
			{
				LgiTrace("%s:%i - No data? (%i)\n", _FL, Bytes);
				return;
			}

			LString s;
			if (!s.Length(Bytes))
			{
				LgiTrace("%s:%i - Alloc failed %i\n", _FL, Bytes);
				return;
			}

			memcpy(s.Get(), gtk_selection_data_get_data(data), Bytes);
			(*cb)(s, LString());
        },
        // Copy of the user's callback
        new BinaryCb(Callback));
}

