#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "GPopup.h"
#include "GPanel.h"
#include "GNotifications.h"

#include "GViewPriv.h"
using namespace Gtk;
#if GTK_MAJOR_VERSION == 3
#else
#include <gdk/gdkx.h>
#endif
#undef Status
#include "LgiWidget.h"

#define DEBUG_SETFOCUS			0
#define DEBUG_HANDLEVIEWKEY		1

extern Gtk::GdkDragAction EffectToDragAction(int Effect);

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	GWindowHookType Flags;
	GView *Target;
};

enum LAttachState
{
	LUnattached,
	LAttaching,
	LAttached,
	LDetaching,
};

class GWindowPrivate
{
public:
	int Sx, Sy;
	bool Dynamic;
	GKey LastKey;
	::GArray<HookInfo> Hooks;
	bool SnapToEdge;
	::GString Icon;
	GRect Decor;
	gulong DestroySig;
	GAutoPtr<GSurface> IconImg;
	LAttachState AttachState;
	
	// State
	GdkWindowState State;
	bool HadCreateEvent;
	
	// Focus stuff
	OsView FirstFocus;
	GViewI *Focus;
	bool Active;

	GWindowPrivate()
	{
		AttachState = LUnattached;
		DestroySig = 0;
		Decor.ZOff(-1, -1);
		FirstFocus = NULL;
		Focus = NULL;
		Active = false;
		State = (Gtk::GdkWindowState)0;
		HadCreateEvent = false;
		
		Sx = Sy = 0;
		Dynamic = true;
		SnapToEdge = false;

		LastKey.vkey = 0;
		LastKey.c16 = 0;
		LastKey.Data = 0;
		LastKey.IsChar = 0;
	}
	
	int GetHookIndex(GView *Target, bool Create = false)
	{
		for (int i=0; i<Hooks.Length(); i++)
		{
			if (Hooks[i].Target == Target) return i;
		}
		
		if (Create)
		{
			HookInfo *n = &Hooks[Hooks.Length()];
			if (n)
			{
				n->Target = Target;
				n->Flags = GNoEvents;
				return Hooks.Length() - 1;
			}
		}
		
		return -1;
	}
};

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

GWindow::GWindow(GtkWidget *w) : GView(0)
{
	d = new GWindowPrivate;
	_QuitOnClose = false;
	Menu = NULL;
	Wnd = GTK_WINDOW(w);
	if (Wnd)
		g_object_set_data(G_OBJECT(Wnd), "GViewI", (GViewI*)this);
	
	_Root = NULL;
	_MenuBar = NULL;
	_VBox = NULL;
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	ClearFlag(WndFlags, GWF_VISIBLE);

    _Lock = new ::LMutex;
}

GWindow::~GWindow()
{
	d->AttachState = LDetaching;
	if (Wnd && d->DestroySig > 0)
	{
		// As we are already in the destructor, we don't want
		// GtkWindowDestroy to try and delete the object again.
		g_signal_handler_disconnect(Wnd, d->DestroySig);
	}

	if (LgiApp->AppWnd == this)
		LgiApp->AppWnd = NULL;

    if (_Root)
	{
		lgi_widget_detach(_Root);
        _Root = NULL;
	}
	if (Wnd)
 	{
		gtk_widget_destroy(GTK_WIDGET(Wnd));
		Wnd = NULL;
 	}
	d->AttachState = LUnattached;

	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

/*
static void PixbufDestroyNotify(guchar *pixels, GSurface *data)
{
	delete data;
}
*/

bool GWindow::SetIcon(const char *FileName)
{
	GAutoString a;
	if (Wnd)
	{
		if (!FileExists(FileName))
		{
			if (a.Reset(LgiFindFile(FileName)))
				FileName = a;
		}

		if (!FileExists(FileName))
		{
			LgiTrace("%s:%i - SetIcon failed to find '%s'\n", _FL, FileName);
			return false;
		}
		else
		{
			#if defined(LINUX)
			LgiApp->SetApplicationIcon(FileName);
			#endif
			
			#if _MSC_VER
			GError *error = NULL;
			if (gtk_window_set_icon_from_file(Wnd, FileName, &error))
				return true;
			#else
			// On windows this is giving a red for blue channel swap error...
			if (d->IconImg.Reset(GdcD->Load(a)))
				gtk_window_set_icon(Wnd, d->IconImg->CreatePixBuf());
			#endif
		}
	}
	
	if (FileName != d->Icon.Get())
		d->Icon = FileName;

	return d->Icon != NULL;
}

bool GWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void GWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

bool GWindow::IsActive()
{
	return d->Active;
}

bool GWindow::Visible()
{
	return GView::Visible();
}

void GWindow::Visible(bool i)
{
	ThreadCheck();

	auto w = GTK_WIDGET(Wnd);
	if (i)
		gtk_widget_show(w);
	else
		gtk_widget_hide(w);
}

bool GWindow::Obscured()
{
	return	d->State == GDK_WINDOW_STATE_WITHDRAWN ||
			d->State == GDK_WINDOW_STATE_ICONIFIED;
}

void GWindow::_SetDynamic(bool i)
{
	d->Dynamic = i;
}

void GWindow::_OnViewDelete()
{
	if (d->Dynamic)
	{
		delete this;
	}
}

void GWindow::OnGtkRealize()
{
	d->AttachState = LAttached;
	GView::OnGtkRealize();
}

void GWindow::OnGtkDelete()
{
	// Delete everything we own...
	// DeleteObj(Menu);

	#if 0
	while (Children.Length())
	{
		GViewI *c = Children.First();
		c->Detach();
	}
	#else
	for (unsigned i=0; i<Children.Length(); i++)
	{
		GViewI *c = Children[i];
		GView *v = c->GetGView();
		if (v)
			v->OnGtkDelete();
	}
	#endif
	
	// These will be destroyed by GTK after returning from GWindowCallback
	Wnd = NULL;
	#ifndef __GTK_H__
	_View = NULL;
	#endif
}

GRect *GWindow::GetDecorSize()
{
	return d->Decor.x2 >= 0 ? &d->Decor : NULL;
}

GViewI *GWindow::WindowFromPoint(int x, int y, bool Debug)
{
	if (!_Root)
		return NULL;

	auto rpos = GtkGetPos(_Root).ZeroTranslate();
	if (!rpos.Overlap(x, y))
		return NULL;

	return GView::WindowFromPoint(x - rpos.x1, y - rpos.y1, Debug);
}

bool GWindow::TranslateMouse(GMouse &m)
{
	m.Target = WindowFromPoint(m.x, m.y, false);
	if (!m.Target)
		return false;

	GViewI *w = this;
	for (auto p = m.Target; p; p = p->GetParent())
	{
		if (p == w)
		{
			auto ppos = GtkGetPos(GTK_WIDGET(WindowHandle()));
			m.x -= ppos.x1;
			m.y -= ppos.y1;
			break;
		}
		else
		{
			auto pos = p->GetPos();
			m.x -= pos.x1;
			m.y -= pos.y1;
		}
	}

	return true;
}

gboolean GWindow::OnGtkEvent(GtkWidget *widget, GdkEvent *event)
{
	if (!event)
	{
		printf("%s:%i - No event.\n", _FL);
		return FALSE;
	}

	#if 0
	if (event->type != 28)
		LgiTrace("%s::OnGtkEvent(%i) name=%s\n", GetClass(), event->type, Name());
	#endif
	switch (event->type)
	{
		case GDK_DELETE:
		{
			bool Close = OnRequestClose(false);
			if (Close)
				OnGtkDelete();     		
			return !Close;
		}
		case GDK_DESTROY:
		{
			delete this;
			return true;
		}
		case GDK_KEY_PRESS:
		case GDK_KEY_RELEASE:
		{
			// auto Class = G_OBJECT_TYPE_NAME(widget);
			auto e = &event->key;
			#define KEY(name) GDK_KEY_##name

			GKey k;
			k.Down(e->type == GDK_KEY_PRESS);
			k.c16 = k.vkey = e->keyval;
			k.Shift((e->state & GDK_SHIFT_MASK) != 0);
			k.Ctrl((e->state & GDK_CONTROL_MASK) != 0);
			k.Alt((e->state & GDK_MOD1_MASK) != 0);
			// k.System((e->state & GDK_MOD2_MASK) != 0);
		
			#ifdef _DEBUG
			if (k.vkey == GDK_KEY_Meta_L ||
				k.vkey == GDK_KEY_Meta_R)
				break;
			#endif
		
			k.IsChar = !k.Ctrl() &&
						!k.Alt() &&
						!k.System() &&
						(k.c16 >= ' ') &&
						(k.c16 >> 8 != 0xff);
			if (e->keyval > 0xff && e->string != NULL)
			{
				// Convert string to unicode char
				auto *i = e->string;
				ptrdiff_t len = strlen(i);
				k.c16 = LgiUtf8To32((uint8_t *&) i, len);
			}
		
			switch (k.vkey)
			{
				case GDK_KEY_ISO_Left_Tab:
				case KEY(Tab):
					k.IsChar = true;
					k.c16 = k.vkey = LK_TAB;
					break;
				case KEY(Return):
				case KEY(KP_Enter):
					k.IsChar = true;
					k.c16 = k.vkey = LK_RETURN;
					break;
				case GDK_KEY_BackSpace:
					k.c16 = k.vkey = LK_BACKSPACE;
					k.IsChar = !k.Ctrl() && !k.Alt() && !k.System();
					break;
				case KEY(Left):
					k.vkey = k.c16 = LK_LEFT;
					break;
				case KEY(Right):
					k.vkey = k.c16 = LK_RIGHT;
					break;
				case KEY(Up):
					k.vkey = k.c16 = LK_UP;
					break;
				case KEY(Down):
					k.vkey = k.c16 = LK_DOWN;
					break;
				case KEY(Page_Up):
					k.vkey = k.c16 = LK_PAGEUP;
					break;
				case KEY(Page_Down):
					k.vkey = k.c16 = LK_PAGEDOWN;
					break;
				case KEY(Home):
					k.vkey = k.c16 = LK_HOME;
					break;
				case KEY(End):
					k.vkey = k.c16 = LK_END;
					break;
				case KEY(Delete):
					k.vkey = k.c16 = LK_DELETE;
					break;
			
				#define KeyPadMap(gdksym, ch, is) \
					case gdksym: k.c16 = ch; k.IsChar = is; break;
			
				KeyPadMap(KEY(KP_0), '0', true)
				KeyPadMap(KEY(KP_1), '1', true)
				KeyPadMap(KEY(KP_2), '2', true)
				KeyPadMap(KEY(KP_3), '3', true)
				KeyPadMap(KEY(KP_4), '4', true)
				KeyPadMap(KEY(KP_5), '5', true)
				KeyPadMap(KEY(KP_6), '6', true)
				KeyPadMap(KEY(KP_7), '7', true)
				KeyPadMap(KEY(KP_8), '8', true)
				KeyPadMap(KEY(KP_9), '9', true)

				KeyPadMap(KEY(KP_Space), ' ', true)
				KeyPadMap(KEY(KP_Tab), '\t', true)
				KeyPadMap(KEY(KP_F1), LK_F1, false)
				KeyPadMap(KEY(KP_F2), LK_F2, false)
				KeyPadMap(KEY(KP_F3), LK_F3, false)
				KeyPadMap(KEY(KP_F4), LK_F4, false)
				KeyPadMap(KEY(KP_Home), LK_HOME, false)
				KeyPadMap(KEY(KP_Left), LK_LEFT, false)
				KeyPadMap(KEY(KP_Up), LK_UP, false)
				KeyPadMap(KEY(KP_Right), LK_RIGHT, false)
				KeyPadMap(KEY(KP_Down), LK_DOWN, false)
				KeyPadMap(KEY(KP_Page_Up), LK_PAGEUP, false)
				KeyPadMap(KEY(KP_Page_Down), LK_PAGEDOWN, false)
				KeyPadMap(KEY(KP_End), LK_END, false)
				KeyPadMap(KEY(KP_Begin), LK_HOME, false)
				KeyPadMap(KEY(KP_Insert), LK_INSERT, false)
				KeyPadMap(KEY(KP_Delete), LK_DELETE, false)
				KeyPadMap(KEY(KP_Equal), '=', true)
				KeyPadMap(KEY(KP_Multiply), '*', true)
				KeyPadMap(KEY(KP_Add), '+', true)
				KeyPadMap(KEY(KP_Separator), '|', true) // is this right?
				KeyPadMap(KEY(KP_Subtract), '-', true)
				KeyPadMap(KEY(KP_Decimal), '.', true)
				KeyPadMap(KEY(KP_Divide), '/', true)
			}
		
			#if 1 // DEBUG_KEY_EVENT
			k.Trace("gtk_key_event");
			#endif

			auto v = d->Focus ? d->Focus : this;
			if (!HandleViewKey(v->GetGView(), k))
			{
				if (!k.Down())
					return false;

				if (k.vkey == LK_TAB || k.vkey == KEY(ISO_Left_Tab))
				{
					// Do tab between controls
					::GArray<GViewI*> a;
					BuildTabStops(this, a);
					int idx = a.IndexOf((GViewI*)v);
					if (idx >= 0)
					{
						idx += k.Shift() ? -1 : 1;
						int next_idx = idx == 0 ? a.Length() -1 : idx % a.Length();                    
						GViewI *next = a[next_idx];
						if (next)
						{
							// LgiTrace("Setting focus to %i of %i: %s, %s, %i\n", next_idx, a.Length(), next->GetClass(), next->GetPos().GetStr(), next->GetId());
							next->Focus(true);
						}
					}
				}
				else if (k.System())
				{
					if (ToLower(k.c16) == 'q')
					{
						auto AppWnd = LgiApp->AppWnd;
						auto Wnd = AppWnd ? AppWnd : this;
						if (Wnd->OnRequestClose(false))
						{
							Wnd->Quit();
							return true;
						}
					}
				}
				else return false;
			}
			break;
		}
		case GDK_CONFIGURE:
		{
			GdkEventConfigure *c = &event->configure;
			Pos.Set(c->x, c->y, c->x+c->width-1, c->y+c->height-1);
			OnPosChange();
			return FALSE;
			break;
		}
		case GDK_FOCUS_CHANGE:
		{
			d->Active = event->focus_change.in;
			#if 0
			printf("%s/%s::GDK_FOCUS_CHANGE(%i)\n",
				GetClass(),
				Name(),
				event->focus_change.in);
			#endif
			break;
		}
		case GDK_WINDOW_STATE:
		{
			d->State = event->window_state.new_window_state;
			break;
		}
		case GDK_PROPERTY_NOTIFY:
		{
  			gchar *Name = gdk_atom_name (event->property.atom);
  			if (!Name)
  				break;
  			if (!_stricmp(Name, "_NET_FRAME_EXTENTS"))
  			{
	  			// printf("PropChange: %i - %s\n", event->property.atom, Name);
	  			unsigned long *extents = NULL;
				if (gdk_property_get(event->property.window,
									gdk_atom_intern ("_NET_FRAME_EXTENTS", FALSE),
									gdk_atom_intern ("CARDINAL", FALSE),
									0,
									sizeof (unsigned long) * 4,
									FALSE,
									NULL,
									NULL,
									NULL,
									(guchar **)&extents))
				{
					d->Decor.Set(extents[0], extents[2], extents[1], extents[3]);					
					g_free(extents);
				}
				else printf("%s:%i - Error: gdk_property_get failed.\n", _FL);
	  		}  	
	  		
	  		g_free(Name);		
			break;
		}
		case GDK_UNMAP:
		{
			// LgiTrace("%s:%i - Unmap %s\n", _FL, GetClass());
			break;
		}
		case GDK_VISIBILITY_NOTIFY:
		{
			// LgiTrace("%s:%i - Visible %s\n", _FL, GetClass());
			break;
		}
		case GDK_DRAG_ENTER:
		{
			LgiTrace("%s:%i - GDK_DRAG_ENTER\n", _FL);
			break;
		}
		case GDK_DRAG_LEAVE:
		{
			LgiTrace("%s:%i - GDK_DRAG_LEAVE\n", _FL);
			break;
		}
		case GDK_DRAG_MOTION:
		{
			LgiTrace("%s:%i - GDK_DRAG_MOTION\n", _FL);
			break;
		}
		case GDK_DRAG_STATUS:
		{
			LgiTrace("%s:%i - GDK_DRAG_STATUS\n", _FL);
			break;
		}
		case GDK_DROP_START:
		{
			LgiTrace("%s:%i - GDK_DROP_START\n", _FL);
			break;
		}
		case GDK_DROP_FINISHED:
		{
			LgiTrace("%s:%i - GDK_DROP_FINISHED\n", _FL);
			break;
		}
		default:
		{
			printf("%s:%i - Unknown event %i\n", _FL, event->type);
			return false;
		}
	}
	
	return true;
}

static
gboolean
GtkWindowDestroy(GtkWidget *widget, GWindow *This)
{
	delete This;
	return true;
}

static
void
GtkWindowRealize(GtkWidget *widget, GWindow *This)
{
	#if 0
	LgiTrace("GtkWindowRealize, This=%p(%s\"%s\")\n",
		This, (NativeInt)This > 0x1000 ? This->GetClass() : 0, (NativeInt)This > 0x1000 ? This->Name() : 0);
	#endif

	This->OnGtkRealize();
}

static
void
GtkRootResize(GtkWidget *widget, GdkRectangle *alloc, GView *This)
{
	GWindow *w = This->GetWindow();
	if (w)
		w->PourAll();
}

void
GWindowUnrealize(GtkWidget *widget, GWindow *wnd)
{
	// printf("%s:%i - GWindowUnrealize %s\n", _FL, wnd->GetClass());
}

bool DndPointMap(GViewI *&v, GdcPt2 &p, GDragDropTarget *&t, GWindow *Wnd, int x, int y)
{
	GRect cli = Wnd->GetClient();
	t = NULL;
	v = Wnd->WindowFromPoint(x - cli.x1, y - cli.y1, false);
	if (!v)
	{
		LgiTrace("%s:%i - <no view> @ %i,%i\n", _FL, x, y);
		return false;
	}

	v->WindowVirtualOffset(&p);
	p.x = x - p.x;
	p.y = y - p.y;

	for (GViewI *view = v; !t && view; view = view->GetParent())
		t = view->DropTarget();
	if (t)
		return true;

	LgiTrace("%s:%i - No target for %s\n", _FL, v->GetClass());
	return false;
}

void
GWindowDragBegin(GtkWidget *widget, GdkDragContext *context, GWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

void
GWindowDragDataDelete(GtkWidget *widget, GdkDragContext *context, GWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

void
GWindowDragDataGet(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data, guint info, guint time, GWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

void
GWindowDragDataReceived(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, GWindow *Wnd)
{
	GdcPt2 p;
	GViewI *v;
	GDragDropTarget *t;
	if (!DndPointMap(v, p, t, Wnd, x, y))
		return;

	for (auto &d: t->Data)
	{
		auto type = gdk_atom_name(gtk_selection_data_get_data_type(data));
		if (d.Format.Equals(type))
		{
			gint length = 0;
			auto ptr = gtk_selection_data_get_data_with_length(data, &length);
			if (ptr)
			{
				d.Data[0].SetBinary(length, (void*)ptr, false);
			}
			break;
		}
	}
}

int GetAcceptFmts(::GString::Array &Formats, GdkDragContext *context, GDragDropTarget *t, GdcPt2 &p)
{
	int KeyState = 0;
	List<char> Fmts;
	int Flags = DROPEFFECT_NONE;

	GList *targets = gdk_drag_context_list_targets(context);
	Gtk::GList *i = targets;
	while (i)
	{
		auto a = gdk_atom_name((GdkAtom)i->data);
		if (a)
			Fmts.Add(NewStr(a));
		i = i->next;
	}

	Flags = t->WillAccept(Fmts, p, KeyState);
	for (auto f: Fmts)
		Formats.New() = f;

	Fmts.DeleteArrays();
	return Flags;
}

gboolean
GWindowDragDataDrop(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, GWindow *Wnd)
{
	// Map the point to a view...
	GdcPt2 p;
	GViewI *v;
	GDragDropTarget *t;
	if (!DndPointMap(v, p, t, Wnd, x, y))
		return false;
	t->Data.Length(0);

	// Request the data...
	::GArray<GDragData> Data;
	::GString::Array Formats;
	int KeyState = 0;
	int Flags = GetAcceptFmts(Formats, context, t, p);
	for (auto f: Formats)
	{
		t->Data.New().Format = f;
		gtk_drag_get_data(widget, context, gdk_atom_intern(f, true), time);
	}

	// Wait for the data to arrive...
	uint64_t Start = LgiCurrentTime();
	while (LgiCurrentTime()-Start < 2000)
	{
		int HasData = 0;
		for (auto d: t->Data)
			if (d.Data.Length() > 0)
				HasData++;
		if (HasData >= Formats.Length())
			break;
		LgiYield();
	}

	auto Result = t->OnDrop(t->Data, p, KeyState);
	if (Flags != DROPEFFECT_NONE)
		gdk_drag_status(context, EffectToDragAction(Flags), time);
	return Result != DROPEFFECT_NONE;
}

void
GWindowDragEnd(GtkWidget *widget, GdkDragContext *context, GWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

gboolean
GWindowDragFailed(GtkWidget *widget, GdkDragContext *context, GtkDragResult result, GWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
	return false;
}

void
GWindowDragLeave(GtkWidget *widget, GdkDragContext *context, guint time, GWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

gboolean
GWindowDragMotion(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, GWindow *Wnd)
{
	GdcPt2 p;
	GViewI *v;
	GDragDropTarget *t;
	if (!DndPointMap(v, p, t, Wnd, x, y))
		return false;

	::GString::Array Formats;
	int Flags = GetAcceptFmts(Formats, context, t, p);
	
	if (Flags != DROPEFFECT_NONE)
		gdk_drag_status(context, EffectToDragAction(Flags), time);

	return Flags != DROPEFFECT_NONE;
}

bool GWindow::Attach(GViewI *p)
{
	bool Status = false;

	ThreadCheck();
	
	// Setup default button...
	if (!_Default)
		_Default = FindControl(IDOK);

	// Create a window if needed..
	if (!Wnd)
		Wnd = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

	if (Wnd)
	{
		auto Widget = GTK_WIDGET(Wnd);
		GView *i = this;
		gtk_window_resize(Wnd, Pos.X(), Pos.Y());
		gtk_window_move(Wnd, Pos.x1, Pos.y1);
		
		auto Obj = G_OBJECT(Wnd);
		g_object_set_data(Obj, "GViewI", (GViewI*)this);

		d->DestroySig = g_signal_connect(Obj, "destroy", G_CALLBACK(GtkWindowDestroy), this);
		g_signal_connect(Obj, "delete_event",			G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "key-press-event",		G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "key-release-event",		G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "focus-in-event",			G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "focus-out-event",		G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "window-state-event",		G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "property-notify-event",	G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "configure-event",		G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "unmap-event",			G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "visibility-notify-event",G_CALLBACK(GtkViewCallback), i);

		g_signal_connect(Obj, "realize",				G_CALLBACK(GtkWindowRealize), i);							
		g_signal_connect(Obj, "unrealize",				G_CALLBACK(GWindowUnrealize), i);

		g_signal_connect(Obj, "drag-begin",				G_CALLBACK(GWindowDragBegin), i);
		g_signal_connect(Obj, "drag-data-delete",		G_CALLBACK(GWindowDragDataDelete), i);
		g_signal_connect(Obj, "drag-data-get",			G_CALLBACK(GWindowDragDataGet), i);
		g_signal_connect(Obj, "drag-data-received",		G_CALLBACK(GWindowDragDataReceived), i);
		g_signal_connect(Obj, "drag-drop",				G_CALLBACK(GWindowDragDataDrop), i);
		g_signal_connect(Obj, "drag-end",				G_CALLBACK(GWindowDragEnd), i);
		g_signal_connect(Obj, "drag-failed",			G_CALLBACK(GWindowDragFailed), i);
		g_signal_connect(Obj, "drag-leave",				G_CALLBACK(GWindowDragLeave), i);
		g_signal_connect(Obj, "drag-motion",			G_CALLBACK(GWindowDragMotion), i);

		#if 0
		g_signal_connect(Obj, "button-press-event",		G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "button-release-event",	G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "motion-notify-event",	G_CALLBACK(GtkViewCallback), i);
		g_signal_connect(Obj, "scroll-event",			G_CALLBACK(GtkViewCallback), i);
		#endif

		gtk_widget_add_events(	Widget,
								GDK_KEY_PRESS_MASK |
								GDK_KEY_RELEASE_MASK |
								GDK_FOCUS_CHANGE_MASK |
								GDK_STRUCTURE_MASK |
								GDK_VISIBILITY_NOTIFY_MASK);
		gtk_window_set_title(Wnd, GBase::Name());
		d->AttachState = LAttaching;

		// g_action_map_add_action_entries (G_ACTION_MAP(Wnd), app_entries, G_N_ELEMENTS (app_entries), Wnd);

		if ((_Root = lgi_widget_new(this, true)))
        {
			g_signal_connect(_Root, "size-allocate",	G_CALLBACK(GtkRootResize), i);

			GtkContainer *AttachPoint = NULL;
			if (GTK_IS_DIALOG(Wnd))
			{
				auto content = gtk_dialog_get_content_area(GTK_DIALOG(Wnd));
				if (!content)
				{
					LgiAssert(!"No content area");
					return false;
				}
				AttachPoint = GTK_CONTAINER(content);
			}
			else
			{
				AttachPoint = GTK_CONTAINER(Wnd);
			}

			LgiAssert(AttachPoint != NULL);
			gtk_container_add(AttachPoint, _Root);

			// Check it actually worked... (would a return value kill you GTK? no it would not)
			auto p = gtk_widget_get_parent(_Root);
			if (!p)
			{
				LgiAssert(!"Add failed");
				return false;
			}

            gtk_widget_show(_Root);
        }

		// This call sets up the GdkWindow handle
		gtk_widget_realize(Widget);
		
		// Do a rough layout of child windows
		PourAll();

		// Add icon
		if (d->Icon)
		{
			SetIcon(d->Icon);
			d->Icon.Empty();
		}
		
		Status = true;
	}
	
	return Status;
}

bool GWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LgiCloseApp();
	}

	return GView::OnRequestClose(OsShuttingDown);
}

bool GWindow::HandleViewMouse(GView *v, GMouse &m)
{
	if (m.Down() && !m.IsMove())
	{
		bool InPopup = false;
		for (GViewI *p = v; p; p = p->GetParent())
		{
			if (dynamic_cast<GPopup*>(p))
			{
				InPopup = true;
				break;
			}
		}
		if (!InPopup && GPopup::CurrentPopups.Length())
		{
			for (int i=0; i<GPopup::CurrentPopups.Length(); i++)
			{
				GPopup *p = GPopup::CurrentPopups[i];
				if (p->Visible())
					p->Visible(false);
			}
		}
	}

	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GMouseEvents)
		{
			if (!d->Hooks[i].Target->OnViewMouse(v, m))
			{
				return false;
			}
		}
	}
	
	return true;
}

bool GWindow::HandleViewKey(GView *v, GKey &k)
{
	bool Status = false;
	GViewI *Ctrl = 0;
	
	#if DEBUG_HANDLEVIEWKEY
	bool Debug = 1; // k.vkey == LK_RETURN;
	char SafePrint = k.c16 < ' ' ? ' ' : k.c16;
	
	// if (Debug)
	{
		LgiTrace("%s/%p::HandleViewKey=%i ischar=%i %s%s%s%s (d->Focus=%s/%p)\n",
			v->GetClass(), v,
			k.c16,
			k.IsChar,
			(char*)(k.Down()?" Down":" Up"),
			(char*)(k.Shift()?" Shift":""),
			(char*)(k.Alt()?" Alt":""),
			(char*)(k.Ctrl()?" Ctrl":""),
			d->Focus?d->Focus->GetClass():0, d->Focus);
	}
	#endif

	// Any window in a popup always gets the key...
	GViewI *p;
	for (p = v->GetParent(); p; p = p->GetParent())
	{
		if (dynamic_cast<GPopup*>(p))
		{
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				LgiTrace("\tSending key to popup\n");
			#endif
			
			return v->OnKey(k);
		}
	}

	// Give key to popups
	if (LgiApp &&
		LgiApp->GetMouseHook() &&
		LgiApp->GetMouseHook()->OnViewKey(v, k))
	{
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			LgiTrace("\tMouseHook got key\n");
		#endif
		goto AllDone;
	}

	// Allow any hooks to see the key...
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			LgiTrace("\tHook[%i]\n", i);
		#endif
		if (d->Hooks[i].Flags & GKeyEvents)
		{
			GView *Target = d->Hooks[i].Target;
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				LgiTrace("\tHook.Target=%p %s\n", Target, Target->GetClass());
			#endif
			if (Target->OnViewKey(v, k))
			{
				Status = true;
				
				#if DEBUG_HANDLEVIEWKEY
				if (Debug)
					LgiTrace("\tHook ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
				#endif
				
				goto AllDone;
			}
		}
	}

	// Give the key to the window...
	if (v->OnKey(k))
	{
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			LgiTrace("\tView ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
		#endif
		
		Status = true;
		goto AllDone;
	}
	#if DEBUG_HANDLEVIEWKEY
	else if (Debug)
		LgiTrace("\t%s didn't eat '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
			v->GetClass(), SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
	#endif
	
	// Window didn't want the key...
	switch (k.vkey)
	{
		case LK_RETURN:
		#ifdef LK_KP_ENTER
		case LK_KP_ENTER:
		#endif
		{
			Ctrl = _Default;
			break;
		}
		case LK_ESCAPE:
		{
			Ctrl = FindControl(IDCANCEL);
			break;
		}
	}

	// printf("Ctrl=%p\n", Ctrl);
	if (Ctrl)
	{
		if (Ctrl->Enabled())
		{
			if (Ctrl->OnKey(k))
			{
				Status = true;

				#if DEBUG_HANDLEVIEWKEY
				if (Debug)
					LgiTrace("\tDefault Button ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
						SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
				#endif
				
				goto AllDone;
			}
			// else printf("OnKey()=false\n");
		}
		// else printf("Ctrl=disabled\n");
	}
	#if DEBUG_HANDLEVIEWKEY
	else if (Debug)
		LgiTrace("\tNo default ctrl to handle key.\n");
	#endif
	
	if (Menu)
	{
		Status = Menu->OnKey(v, k);
		if (Status)
		{
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				LgiTrace("\tMenu ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
		}
	}
	
	// Tab through controls
	if (k.vkey == LK_TAB && k.Down() && !k.IsChar)
	{
		GViewI *Wnd = GetNextTabStop(v, k.Shift());
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			LgiTrace("\tTab moving focus shift=%i Wnd=%p\n", k.Shift(), Wnd);
		#endif
		if (Wnd)
			Wnd->Focus(true);
	}

	// Control shortcut?
	if (k.Down() && k.Alt() && k.c16 > ' ')
	{
		ShortcutMap Map;
		BuildShortcuts(Map);
		GViewI *c = Map.Find(ToUpper(k.c16));
		if (c)
		{
			c->OnNotify(c, GNotify_Activate);
			return true;
		}
	}

AllDone:
	if (d)
		d->LastKey = k;

	return Status;
}


void GWindow::Raise()
{
	/*
	if (_View->handle())
	{
		#if defined XWIN
		XWindowChanges c;
		c.stack_mode = XAbove;
		XConfigureWindow(	_View->XDisplay(),
							_View->handle(),
							CWStackMode,
							&c);
		#endif							
	}
	*/
}


GWindowZoom GWindow::GetZoom()
{
	switch (d->State)
	{
		case GDK_WINDOW_STATE_ICONIFIED:
			return GZoomMin;
		case GDK_WINDOW_STATE_MAXIMIZED:
			return GZoomMax;
		default:
			break;
	}

	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	if (Wnd)
	{
		ThreadCheck();

		switch (i)
		{
			case GZoomMin:
			{
				gtk_window_iconify(Wnd);
				break;
			}
			case GZoomNormal:
			{
				gtk_window_deiconify(Wnd);
				gtk_window_unmaximize(Wnd);
				break;
			}
			case GZoomMax:
			{
				gtk_window_unmaximize(Wnd);
				break;
			}
		}	
	}
}

GViewI *GWindow::GetDefault()
{
	return _Default;
}

void GWindow::SetDefault(GViewI *v)
{
	if (v &&
		v->GetWindow() == this)
	{
		if (_Default != v)
		{
			GViewI *Old = _Default;
			_Default = v;

			if (Old) Old->Invalidate();
			if (_Default) _Default->Invalidate();
		}
	}
	else
	{
		_Default = 0;
	}
}

bool GWindow::Name(const char *n)
{
	if (Wnd)
	{
		ThreadCheck();
		gtk_window_set_title(Wnd, n);
	}

	return GBase::Name(n);
}

char *GWindow::Name()
{
	return GBase::Name();
}

struct CallbackParams
{
	GRect Menu;
	int Depth;
	
	CallbackParams()
	{
		Menu.ZOff(-1, -1);
		Depth = 0;
	}
};

void ClientCallback(GtkWidget *w, CallbackParams *p)
{
	const char *Name = gtk_widget_get_name(w);
	if (Name && !_stricmp(Name, "GtkMenuBar"))
	{
		GtkAllocation alloc = {0};
		gtk_widget_get_allocation(w, &alloc);
		p->Menu.ZOff(alloc.width-1, alloc.height-1);
		// LgiTrace("GtkMenuBar = %s\n", p->Menu.GetStr());
	}
	
	if (!p->Menu.Valid())
	{
		p->Depth++;
		if (GTK_IS_CONTAINER(w))
			gtk_container_forall(GTK_CONTAINER(w), (GtkCallback)ClientCallback, p);
		p->Depth--;
	}
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	static GRect r;
	r = GView::GetClient(ClientSpace);

	if (Wnd)
	{
		CallbackParams p;
		gtk_container_forall(GTK_CONTAINER(Wnd), (GtkCallback)ClientCallback, &p);
		if (p.Menu.Valid())
			r.y1 += p.Menu.Y();
	}

	return r;
}

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;

	if (Load)
	{
		::GVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			GRect Position(0, 0, -1, -1);
			GWindowZoom State = GZoomNormal;

// printf("SerializeState load %s\n", v.Str());


			GToken t(v.Str(), ";");
			for (int i=0; i<t.Length(); i++)
			{
				char *Var = t[i];
				char *Value = strchr(Var, '=');
				if (Value)
				{
					*Value++ = 0;

					if (stricmp(Var, "State") == 0)
						State = (GWindowZoom) atoi(Value);
					else if (stricmp(Var, "Pos") == 0)
						Position.SetStr(Value);
				}
				else return false;
			}
			
			if (Position.Valid())
			{
// printf("SerializeState setpos %s\n", Position.GetStr());
				SetPos(Position);
			}
			
			SetZoom(State);
		}
		else return false;
	}
	else
	{
		char s[256];
		GWindowZoom State = GetZoom();
		sprintf(s, "State=%i;Pos=%s", State, GetPos().GetStr());

		::GVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
}

GRect &GWindow::GetPos()
{
	if (Wnd)
	{
		/*
		OsRect r = _View->geometry();

		Pos.x1 = r.left;
		Pos.y1 = r.top;
		Pos.x2 = r.right;
		Pos.y2 = r.bottom;

		#if defined XWIN
		XWindowAttributes a;
		ZeroObj(a);
		XGetWindowAttributes(Handle()->XDisplay(), Handle()->handle(), &a);
		if (!a.override_redirect)
		{
			int Dx, Dy;
			_View->GetDecorationSize(Dx, Dy);
			Pos.Offset(Dx, Dy);
		}
		#endif
		*/
	}

	return Pos;
}

bool GWindow::SetPos(GRect &p, bool Repaint)
{
	Pos = p;
	if (Wnd)
	{
		ThreadCheck();
		
		gtk_window_resize(Wnd, Pos.X(), Pos.Y());
		gtk_window_move(Wnd, Pos.x1, Pos.y1);
	}

	OnPosChange();
	return true;
}

void GWindow::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	// Force repour
	d->Sx = d->Sy = -1;
}

void GWindow::OnCreate()
{
	AttachChildren();
}

void GWindow::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

void GWindow::OnPosChange()
{
	GView::OnPosChange();

	if (d->Sx != X() ||	d->Sy != Y())
	{
		PourAll();
		d->Sx = X();
		d->Sy = Y();
	}
}

#define IsTool(v) \
	( \
		dynamic_cast<GView*>(v) \
		&& \
		dynamic_cast<GView*>(v)->_IsToolBar \
	)

void GWindow::PourAll()
{
	GRect c;
	if (_Root)
		c = GtkGetPos(_Root).ZeroTranslate();
	else
		c = GetClient();
	GRegion Client(c);
	GViewI *MenuView = 0;

	GRegion Update(Client);
	bool HasTools = false;
	GViewI *v;
	List<GViewI>::I Lst = Children.begin();

	{
		GRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			bool IsMenu = MenuView == v;
			if (!IsMenu && IsTool(v))
			{
				GRect OldPos = v->GetPos();
				if (OldPos.Valid())
					Update.Union(&OldPos);
				
				if (HasTools)
				{
					// 2nd and later toolbars
					if (v->Pour(Tools))
					{
						if (!v->Visible())
						{
							v->Visible(true);
						}

						if (OldPos != v->GetPos())
						{
							// position has changed update...
							v->Invalidate();
						}

						Tools.Subtract(&v->GetPos());
						Update.Subtract(&v->GetPos());
					}
				}
				else
				{
					// First toolbar
					if (v->Pour(Client))
					{
						HasTools = true;

						if (!v->Visible())
						{
							v->Visible(true);
						}

						if (OldPos != v->GetPos())
						{
							v->Invalidate();
						}

						GRect Bar(v->GetPos());
						// LgiTrace("%s = %s\n", v->GetClass(), Bar.GetStr());
						Bar.x2 = GetClient().x2;

						Tools = Bar;
						Tools.Subtract(&v->GetPos());
						Client.Subtract(&Bar);
						Update.Subtract(&Bar);
					}
				}
			}
		}
	}

	Lst = Children.begin();
	for (GViewI *v = *Lst; v; v = *++Lst)
	{
		bool IsMenu = MenuView == v;
		if (!IsMenu && !IsTool(v))
		{
			GRect OldPos = v->GetPos();
			if (OldPos.Valid())
				Update.Union(&OldPos);

			if (v->Pour(Client))
			{
				// GRect p = v->GetPos();
				// LgiTrace("%s = %s\n", v->GetClass(), p.GetStr());
				if (!v->Visible())
					v->Visible(true);

				v->Invalidate();

				Client.Subtract(&v->GetPos());
				Update.Subtract(&v->GetPos());
			}
			else
			{
				// non-pourable
			}
		}
	}
	
	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}

	// _Dump();
}

/*
int GWindow::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;
	
	for (char *f=Formats.First(); f; )
	{
		if (stricmp(f, LGI_FileDropFormat) == 0)
		{
			f = Formats.Next();
			Status = DROPEFFECT_COPY;
		}
		else
		{
			Formats.Delete(f);
			DeleteArray(f);
			f = Formats.Current();
		}
	}
	
	return Status;
}

int GWindow::OnDrop(char *Format, ::GVariant *Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;

	if (Format && Data)
	{
		if (stricmp(Format, LGI_FileDropFormat) == 0)
		{
			::GArray<char*> Files;
			if (Data->IsBinary())
			{
				GToken Uri(	(char*)Data->Value.Binary.Data,
							"\r\n,",
							true,
							Data->Value.Binary.Length);
				for (int i=0; i<Uri.Length(); i++)
				{
					char *File = Uri[i];
					if (strnicmp(File, "file:", 5) == 0)
						File += 5;
					
					char *in = File, *out = File;
					while (*in)
					{
						if (in[0] == '%' &&
							in[1] &&
							in[2])
						{
							char h[3] = { in[1], in[2], 0 };
							*out++ = htoi(h);
							in += 3;
						}
						else
						{
							*out++ = *in++;
						}
					}
					*out++ = 0;
					
					if (FileExists(File))
					{
						Files.Add(NewStr(File));
					}
				}
			}
			
			if (Files.Length())
			{
				Status = DROPEFFECT_COPY;
				OnReceiveFiles(Files);
				Files.DeleteArrays();
			}
		}
	}
	
	return Status;
}
*/

GMessage::Param GWindow::OnEvent(GMessage *m)
{
	switch (m->Msg())
	{
		case M_CLOSE:
		{
			if (OnRequestClose(false))
			{
				Quit();
				return 0;
			}
			break;
		}
	}

	return GView::OnEvent(m);
}

bool GWindow::RegisterHook(GView *Target, GWindowHookType EventType, int Priority)
{
	bool Status = false;
	
	if (Target && EventType)
	{
		int i = d->GetHookIndex(Target, true);
		if (i >= 0)
		{
			d->Hooks[i].Flags = EventType;
			Status = true;
		}
	}
	
	return Status;
}

bool GWindow::UnregisterHook(GView *Target)
{
	int i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}

void GWindow::OnFrontSwitch(bool b)
{
}

GViewI *GWindow::GetFocus()
{
	return d->Focus;
}

#if DEBUG_SETFOCUS
static GAutoString DescribeView(GViewI *v)
{
	if (!v)
		return GAutoString(NewStr("NULL"));

	char s[512];
	int ch = 0;
	::GArray<GViewI*> p;
	for (GViewI *i = v; i; i = i->GetParent())
	{
		p.Add(i);
	}
	for (int n=min(3, p.Length()-1); n>=0; n--)
	{
		char Buf[256] = "";
		if (!stricmp(v->GetClass(), "GMdiChild"))
			sprintf(Buf, "'%s'", v->Name());
		v = p[n];
		
		ch += sprintf_s(s + ch, sizeof(s) - ch, "%s>%s", Buf, v->GetClass());
	}
	return GAutoString(NewStr(s));
}
#endif

void GWindow::SetFocus(GViewI *ctrl, FocusType type)
{
	#if DEBUG_SETFOCUS
	const char *TypeName = NULL;
	switch (type)
	{
		case GainFocus: TypeName = "Gain"; break;
		case LoseFocus: TypeName = "Lose"; break;
		case ViewDelete: TypeName = "Delete"; break;
	}
	#endif

	switch (type)
	{
		case GainFocus:
		{
			if (d->Focus == ctrl)
			{
				#if DEBUG_SETFOCUS
				GAutoString _ctrl = DescribeView(ctrl);
				LgiTrace("SetFocus(%s, %s) already has focus.\n", _ctrl.Get(), TypeName);
				#endif
				return;
			}

			if (d->Focus)
			{
				GView *gv = d->Focus->GetGView();
				if (gv)
				{
					#if DEBUG_SETFOCUS
					GAutoString _foc = DescribeView(d->Focus);
					LgiTrace(".....defocus GView: %s\n", _foc.Get());
					#endif
					gv->_Focus(false);
				}
				else if (IsActive())
				{
					#if DEBUG_SETFOCUS
					GAutoString _foc = DescribeView(d->Focus);
					LgiTrace(".....defocus view: %s (active=%i)\n", _foc.Get(), IsActive());
					#endif
					d->Focus->OnFocus(false);
					d->Focus->Invalidate();
				}
			}

			d->Focus = ctrl;

			if (d->Focus)
			{
				GView *gv = d->Focus->GetGView();
				if (gv)
				{
					#if DEBUG_SETFOCUS
					GAutoString _set = DescribeView(d->Focus);
					LgiTrace("GWindow::SetFocus(%s, %s) focusing GView %p\n",
						_set.Get(),
						TypeName,
						d->Focus->Handle());
					#endif

					gv->_Focus(true);
				}
				else if (IsActive())
				{			
					#if DEBUG_SETFOCUS
					GAutoString _set = DescribeView(d->Focus);
					LgiTrace("GWindow::SetFocus(%s, %s) focusing nonGView %p (active=%i)\n",
						_set.Get(),
						TypeName,
						d->Focus->Handle(),
						IsActive());
					#endif

					d->Focus->OnFocus(true);
					d->Focus->Invalidate();
				}
			}
			break;
		}
		case LoseFocus:
		{
			#if DEBUG_SETFOCUS
			GAutoString _Ctrl = DescribeView(d->Focus);
			GAutoString _Focus = DescribeView(d->Focus);
			LgiTrace("GWindow::SetFocus(%s, %s) d->Focus=%s\n",
				_Ctrl.Get(),
				TypeName,
				_Focus.Get());
			#endif
			if (ctrl == d->Focus)
			{
				d->Focus = NULL;
			}
			break;
		}
		case ViewDelete:
		{
			if (ctrl == d->Focus)
			{
				#if DEBUG_SETFOCUS
				GAutoString _Ctrl = DescribeView(d->Focus);
				LgiTrace("GWindow::SetFocus(%s, %s) on delete\n",
					_Ctrl.Get(),
					TypeName);
				#endif
				d->Focus = NULL;
			}
			break;
		}
	}
}

void GWindow::SetDragHandlers(bool On)
{
}

bool GWindow::IsAttached()
{
	return	d->AttachState == LAttaching ||
			d->AttachState == LAttached;
}

void GWindow::OnTrayClick(GMouse &m)
{
	if (m.Down() || m.IsContextMenu())
	{
		LSubMenu RClick;
		OnTrayMenu(RClick);
		if (GetMouse(m, true))
		{
			#if WINNATIVE
			SetForegroundWindow(Handle());
			#endif
			int Result = RClick.Float(this, m.x, m.y);
			#if WINNATIVE
			PostMessage(Handle(), WM_NULL, 0, 0);
			#endif
			OnTrayMenuResult(Result);
		}
	}
}

void GWindow::Quit(bool DontDelete)
{
	ThreadCheck();
	
	if (Wnd)
	{
		d->AttachState = LDetaching;
		gtk_widget_destroy(GTK_WIDGET(Wnd));
		Wnd = NULL;
	}
}


void GWindow::SetAlwaysOnTop(bool b)
{
}
