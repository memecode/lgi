// Clipboard Implementation
#include "Lgi.h"
#include "GVariant.h"
#include "GClipBoard.h"

#define DEBUG_CLIPBOARD					0
#define VAR_COUNT						16
#define LGI_CLIP_BINARY					"lgi.binary"
#define LGI_RECEIVE_CLIPBOARD_TIMEOUT	4000

using namespace Gtk;

struct ClipData : public LMutex
{
	::GVariant v[VAR_COUNT];
	
	ClipData() : LMutex("ClipData")
	{
	}
	
}	Data;

class GClipBoardPriv
{
public:
	GtkClipboard *c;
};

///////////////////////////////////////////////////////////////////////////////////////////////
GClipBoard::GClipBoard(GView *o)
{
	d = new GClipBoardPriv;
	Owner = o;
	Open = false;
	d->c = gtk_clipboard_get(GDK_NONE); // gdk_atom_intern("CLIPBOARD", false)
	if (d->c)
		Open = true;
	#if DEBUG_CLIPBOARD
	printf("d->c = %i\n", d->c);
	#endif
}

GClipBoard::~GClipBoard()
{
	d->c = 0;
	DeleteObj(d);
}

bool GClipBoard::Empty()
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

bool GClipBoard::EnumFormats(::GArray<FormatType> &Formats)
{
	return false;
}

bool GClipBoard::Html(const char *doc, bool AutoEmpty)
{
	return false;
}

::GString GClipBoard::Html()
{
	return ::GString();
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
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

char *GClipBoard::Text()
{
	char *t = 0;
	
	if (d->c)
	{
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

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	GAutoString u(WideToUtf8(Str));
	return Text(u, AutoEmpty);
}

char16 *GClipBoard::TextW()
{
	GAutoString u(Text());
	return Utf8ToWide(u);
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	if (pDC && d->c)
	{
		GMemDC *Mem = dynamic_cast<GMemDC*>(pDC);
		if (Mem)
		{
			/*
			GdkPixbuf *pb = gdk_pixbuf_new_from_data (	const guchar *data,
														GDK_COLORSPACE_RGB,
														gboolean has_alpha,
														int bits_per_sample,
														int width,
														int height,
														int rowstride,
														GdkPixbufDestroyNotify destroy_fn,
														gpointer destroy_fn_data);
			// gtk_clipboard_set_image(d->c, pb);
			*/
		}
	}
	return Status;
}

void ClipboardImageReceived(GtkClipboard *Clipboard, GdkPixbuf *Img, GAutoPtr<GSurface> *Out)
{
	auto chan = gdk_pixbuf_get_n_channels(Img);
	auto alpha = gdk_pixbuf_get_has_alpha(Img);
	GColourSpace cs = System32BitColourSpace;
	if (chan == 3)
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
		LgiAssert(!"Unexpected colourspace.");
	}

	auto x = gdk_pixbuf_get_width(Img), y = gdk_pixbuf_get_height(Img);
	GAutoPtr<GMemDC> m(new GMemDC(x, y, cs));
	if (m)
	{
		auto px = gdk_pixbuf_get_pixels(Img);
		auto row = gdk_pixbuf_get_rowstride(Img);
		for (int yy=0; yy<y; yy++)
		{
			#define Rop24(out_cs, in_cs) \
				case Cs##out_cs: \
				{ \
					G##in_cs *in = (G##in_cs*)(px + (yy*row)); \
					G##out_cs *out = (G##out_cs*) ((*m)[yy]); \
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
					G##in_cs *in = (G##in_cs*)(px + (yy*row)); \
					G##out_cs *out = (G##out_cs*) (*m)[yy]; \
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

				default:
					LgiAssert(!"Unsupported colour space.");
					yy = y;
					break;
			}
		}

		Out->Reset(m.Release());
	}
}

GSurface *GClipBoard::Bitmap()
{
	pDC.Reset();
	gtk_clipboard_request_image(d->c, (GtkClipboardImageReceivedFunc) ClipboardImageReceived, &pDC);

	uint64 Ts = LgiCurrentTime();
	while (!pDC && (LgiCurrentTime() - Ts) < LGI_RECEIVE_CLIPBOARD_TIMEOUT)
		LgiYield();

	return pDC.Release();
}

void LgiClipboardGetFunc(GtkClipboard *clipboard,
                        GtkSelectionData *data,
                        guint info,
                        gpointer user_data)
{
	if (Data.Lock(_FL))
	{
		::GVariant *p = (::GVariant*)user_data;
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
		::GVariant *p = (::GVariant*)user_data;
		#if DEBUG_CLIPBOARD
		printf("%s:%i - LgiClipboardClearFunc: %i\n", _FL, p->Type);
		#endif
		p->Empty();
		Data.Unlock();
	}
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	if (!Ptr || Len <= 0)
		return false;

	::GVariant *p = NULL;
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

::GString::Array GClipBoard::Files()
{
	::GString::Array a;
	return a;
}

bool GClipBoard::Files(::GString::Array &a, bool AutoEmpty)
{
	return false;
}

struct ReceiveData
{
	GAutoPtr<uint8_t,true> *Ptr;
	ssize_t *Len;
};

void LgiClipboardReceivedFunc(GtkClipboard *clipboard,
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

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8_t,true> &Ptr, ssize_t *Len)
{
	ReceiveData r = {&Ptr, Len};

	gtk_clipboard_request_contents(	d->c,
	                                gdk_atom_intern(LGI_CLIP_BINARY, false),
	                                LgiClipboardReceivedFunc,
	                                &r);
                                

	uint64 Start = LgiCurrentTime();
	do
	{
		if (r.Ptr->Get())
			break;
		LgiYield();
		LgiSleep(1);
	}
	while (LgiCurrentTime() - Start > LGI_RECEIVE_CLIPBOARD_TIMEOUT);

	#if DEBUG_CLIPBOARD
	printf("%s:%i - GClipBoard::Binary %p, %i\n", _FL, r.Ptr->Get(), Len ? *Len : -1);
	#endif

	return r.Ptr->Get() != NULL;
}

