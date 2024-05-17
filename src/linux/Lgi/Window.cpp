#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Token.h"
#include "lgi/common/Popup.h"
#include "lgi/common/Panel.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Menu.h"
#include "ViewPriv.h"

using namespace Gtk;
#undef Status
#include "LgiWidget.h"

#define DEBUG_SETFOCUS			0
#define DEBUG_HANDLEVIEWKEY		0

extern Gtk::GdkDragAction EffectToDragAction(int Effect);

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	LWindowHookType Flags;
	LView *Target;
};

enum LAttachState
{
	LUnattached,
	LAttaching,
	LAttached,
	LDetaching,
};

class LWindowPrivate
{
public:
	int Sx = 0, Sy = 0;
	LKey LastKey;
	LArray<HookInfo> Hooks;
	bool SnapToEdge = false;
	LString Icon;
	LRect Decor;
	gulong DestroySig = 0;
	LAutoPtr<LSurface> IconImg;
	LAttachState AttachState = LUnattached;
	bool ShowTitleBar = true;
	bool WillFocus = true;
	
	// State
	GdkWindowState State;
	bool HadCreateEvent = false;
	
	// Focus stuff
	OsView FirstFocus = NULL;
	LViewI *Focus = NULL;
	bool Active = false;

	LWindowPrivate()
	{
		Decor.ZOff(-1, -1);
		State = (Gtk::GdkWindowState)0;
		
		LastKey.vkey = 0;
		LastKey.c16 = 0;
		LastKey.Data = 0;
		LastKey.IsChar = 0;
	}
	
	int GetHookIndex(LView *Target, bool Create = false)
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
				n->Flags = LNoEvents;
				return Hooks.Length() - 1;
			}
		}
		
		return -1;
	}
};

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

LWindow::LWindow(GtkWidget *w) : LView(0)
{
	d = new LWindowPrivate;
	_QuitOnClose = false;
	Wnd = GTK_WINDOW(w);
	if (Wnd)
	{
		gtk_window_set_decorated(Wnd, d->ShowTitleBar);
		g_object_set_data(G_OBJECT(Wnd), "LViewI", (LViewI*)this);
	}
	
	_RootAlloc.ZOff(-1, -1);
	_Window = this;
	WndFlags |= GWND_CREATE;
	ClearFlag(WndFlags, GWF_VISIBLE);

    _Lock = new LMutex("LWindow");
}

LWindow::~LWindow()
{
	d->AttachState = LDetaching;
	if (Wnd && d->DestroySig > 0)
	{
		// As we are already in the destructor, we don't want
		// GtkWindowDestroy to try and delete the object again.
		g_signal_handler_disconnect(Wnd, d->DestroySig);
	}

	if (LAppInst &&
		LAppInst->AppWnd == this)
		LAppInst->AppWnd = NULL;

    if (_Root)
	{
		lgi_widget_detach(_Root);
        _Root = NULL;
	}
	
	// This needs to be before the 'gtk_widget_destroy' because that will delete the menu's widgets.
	DeleteObj(Menu);
	
	if (Wnd)
 	{
		gtk_widget_destroy(GTK_WIDGET(Wnd));
		Wnd = NULL;
 	}
 	
	d->AttachState = LUnattached;

	DeleteObj(d);
	DeleteObj(_Lock);
}

int LWindow::WaitThread()
{
	return 0; // Nop for linux
}

bool LWindow::SetIcon(const char *FileName)
{
	LString a;
	if (Wnd)
	{
		if (!LFileExists(FileName))
		{
			if (a = LFindFile(FileName))
				FileName = a;
		}


		if (!LFileExists(FileName))
		{
			LgiTrace("%s:%i - SetIcon failed to find '%s'\n", _FL, FileName);
			return false;
		}
		else
		{
			#if defined(LINUX)
			LAppInst->SetApplicationIcon(FileName);
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

bool LWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void LWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

bool LWindow::IsActive()
{
	return d->Active;
}

bool LWindow::SetActive()
{
	if (!Wnd)
		return false;
	gtk_window_present(Wnd);
	return true;
}

bool LWindow::Visible()
{
	return LView::Visible();
}

void LWindow::Visible(bool i)
{
	ThreadCheck();

	auto w = GTK_WIDGET(Wnd);
	if (!w)
		return;

	if (i)
		gtk_widget_show(w);
	else
		gtk_widget_hide(w);
}

bool LWindow::Obscured()
{
	return	d->State == GDK_WINDOW_STATE_WITHDRAWN ||
			d->State == GDK_WINDOW_STATE_ICONIFIED;
}

void LWindow::_OnViewDelete()
{
	delete this;
}

void LWindow::OnGtkRealize()
{
	d->AttachState = LAttached;
	LView::OnGtkRealize();
}

void LWindow::OnGtkDelete()
{
	// Delete everything we own...
	// DeleteObj(Menu);

	#if 0
	while (Children.Length())
	{
		LViewI *c = Children.First();
		c->Detach();
	}
	#else
	for (unsigned i=0; i<Children.Length(); i++)
	{
		LViewI *c = Children[i];
		LView *v = c->GetLView();
		if (v)
			v->OnGtkDelete();
	}
	#endif
	
	// These will be destroyed by GTK after returning from LWindowCallback
	Wnd = NULL;
	#ifndef __GTK_H__
	_View = NULL;
	#endif
}

LRect *LWindow::GetDecorSize()
{
	return d->Decor.x2 >= 0 ? &d->Decor : NULL;
}

void LWindow::SetDecor(bool Visible)
{
	if (Wnd)
		gtk_window_set_decorated (Wnd, Visible);
	else
		LgiTrace("%s:%i - No window to set decor.\n", _FL);
}

LViewI *LWindow::WindowFromPoint(int x, int y, bool Debug)
{
	if (!_Root)
		return NULL;

	auto rpos = GtkGetPos(_Root).ZeroTranslate();
	if (!rpos.Overlap(x, y))
		return NULL;

	return LView::WindowFromPoint(x - rpos.x1, y - rpos.y1, Debug);
}

bool LWindow::TranslateMouse(LMouse &m)
{
	m.Target = WindowFromPoint(m.x, m.y, false);
	if (!m.Target)
		return false;

	LViewI *w = this;
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

gboolean LWindow::OnGtkEvent(GtkWidget *widget, GdkEvent *event)
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
			auto ModFlags = LAppInst->GetKeyModFlags();
			auto e = &event->key;
			#define KEY(name) GDK_KEY_##name

			LKey k;
			k.Down(e->type == GDK_KEY_PRESS);
			k.c16 = k.vkey = e->keyval;
			k.Shift((e->state & ModFlags->Shift) != 0);
			k.Ctrl((e->state & ModFlags->Ctrl) != 0);
			k.Alt((e->state & ModFlags->Alt) != 0);
			k.System((e->state & ModFlags->System) != 0);
			
			#if 0//def _DEBUG
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
		
			if (ModFlags->Debug)
			{
				::	LString Msg;
				Msg.Printf("e->state=%x %s", e->state, ModFlags->FlagsToString(e->state).Get());
				k.Trace(Msg);
			}

			auto v = d->Focus ? d->Focus : this;
			if (!HandleViewKey(v->GetLView(), k))
			{
				if (!k.Down())
					return false;

				if (k.vkey == LK_TAB || k.vkey == KEY(ISO_Left_Tab))
				{
					// Do tab between controls
					LArray<LViewI*> a;
					BuildTabStops(this, a);
					int idx = a.IndexOf((LViewI*)v);
					if (idx >= 0)
					{
						idx += k.Shift() ? -1 : 1;
						int next_idx = idx == 0 ? a.Length() -1 : idx % a.Length();                    
						LViewI *next = a[next_idx];
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
						auto AppWnd = LAppInst->AppWnd;
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
			// printf("%s::GDK_CONFIGURE %s\n", GetClass(), Pos.GetStr());
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
GtkWindowDestroy(GtkWidget *widget, LWindow *This)
{
	delete This;
	return true;
}

static
void
GtkWindowRealize(GtkWidget *widget, LWindow *This)
{
	#if 0
	LgiTrace("GtkWindowRealize, This=%p(%s\"%s\")\n",
		This, (NativeInt)This > 0x1000 ? This->GetClass() : 0, (NativeInt)This > 0x1000 ? This->Name() : 0);
	#endif

	This->OnGtkRealize();
}

void
GtkRootResize(GtkWidget *widget, GdkRectangle *r, LView *This)
{
	LWindow *w = This->GetWindow();
	if (w)
	{
		if (r)
		{
			w->_RootAlloc = *r;
			#ifdef _DEBUG
			// printf("%s got root alloc: %s\n", w->GetClass(), w->_RootAlloc.GetStr());
			#endif
		}
		else LgiTrace("%s:%i - no alloc rect param?\n", _FL);
		
		w->PourAll();
	}
}

void
LWindowUnrealize(GtkWidget *widget, LWindow *wnd)
{
	// printf("%s:%i - LWindowUnrealize %s\n", _FL, wnd->GetClass());
}

bool DndPointMap(LViewI *&v, LPoint &p, LDragDropTarget *&t, LWindow *Wnd, int x, int y)
{
	LRect cli = Wnd->GetClient();
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

	for (LViewI *view = v; !t && view; view = view->GetParent())
		t = view->DropTarget();
	if (t)
		return true;

	LgiTrace("%s:%i - No target for %s\n", _FL, v->GetClass());
	return false;
}

void
LWindowDragBegin(GtkWidget *widget, GdkDragContext *context, LWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

void
LWindowDragDataDelete(GtkWidget *widget, GdkDragContext *context, LWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

void
LWindowDragDataGet(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data, guint info, guint time, LWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

void
LWindowDragDataReceived(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, LWindow *Wnd)
{
	LPoint p;
	LViewI *v;
	LDragDropTarget *t;
	if (!DndPointMap(v, p, t, Wnd, x, y))
	{
		LgiTrace("%s:%i - DndPointMap false.\n", _FL);
		return;
	}

	bool matched = false;
	auto type = gdk_atom_name(gtk_selection_data_get_data_type(data));
	for (auto &d: t->Data)
	{
		if (d.Format.Equals(type))
		{
			matched = true;
			
			gint length = 0;
			auto ptr = gtk_selection_data_get_data_with_length(data, &length);
			if (ptr)
			{
				LgiTrace("%s:%i - LWindowDragDataReceived got data '%s'.\n", _FL, type);
				d.Data[0].SetBinary(length, (void*)ptr, false);
			}
			else
			{
				LgiTrace("%s:%i - LWindowDragDataReceived: gtk_selection_data_get_data_with_length failed for '%s'.\n", _FL, type);
			}
			break;
		}
	}
	if (!matched)
	{
		LgiTrace("%s:%i - LWindowDragDataReceived: no matching data '%s'.\n", _FL, type);
	}
	Wnd->PostEvent(M_DND_DATA_RECEIVED);
}

int GetAcceptFmts(LString::Array &Formats, GdkDragContext *context, LDragDropTarget *t, LPoint &p)
{
	int KeyState = 0;
	LDragFormats Fmts(true);
	int Flags = DROPEFFECT_NONE;

	GList *targets = gdk_drag_context_list_targets(context);
	Gtk::GList *i = targets;
	while (i)
	{
		auto a = gdk_atom_name((GdkAtom)i->data);
		if (a)
			Fmts.Supports(a);
		i = i->next;
	}

	Fmts.SetSource(false);
	Flags = t->WillAccept(Fmts, p, KeyState);
	
	auto Sup = Fmts.GetSupported();
	for (auto &s: Sup)
		Formats.New() = s;

	return Flags;
}

struct LGtkDrop : public LView::ViewEventTarget
{
	uint64_t Start = 0;
	LPoint p;
	LViewI *v = NULL;
	LDragDropTarget *t = NULL;

	LArray<LDragData> Data;
	LString::Array Formats;
	int KeyState = 0;
	int Flags = 0;
	
	const char *GetClass() { return "LGtkDrop"; }
	
	LGtkDrop(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, LWindow *Wnd) :
		LView::ViewEventTarget(Wnd, M_DND_DATA_RECEIVED)
	{
		Start = LCurrentTime();
		
		// Map the point to a view...
		if (!DndPointMap(v, p, t, Wnd, x, y))
			return;
			
		t->Data.Length(0);

		// Request the data...
		Flags = GetAcceptFmts(Formats, context, t, p);
		for (auto f: Formats)
		{
			t->Data.New().Format = f;
			gtk_drag_get_data(widget, context, gdk_atom_intern(f, true), time);
		}
		
		// Callbacks should be received by LWindowDragDataReceived
		// Once they have all arrived or the timeout has been reached we call OnComplete
		LgiTrace("%s:%i - LGtkDrop created...\n", _FL);
	}
	
	~LGtkDrop()
	{
		LgiTrace("%s:%i - LGtkDrop deleted.\n", _FL);
	}
	
	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_DND_DATA_RECEIVED:
			{
				size_t HasData = 0;
				for (auto d: t->Data)
					if (d.Data.Length() > 0)
						HasData++;

				LgiTrace("%s:%i - Got M_DND_DATA_RECEIVED %i of %i\n",
					_FL, (int)HasData, (int)Formats.Length());
				if (HasData >= Formats.Length())
				{
					LgiTrace("%s:%i - LWindowDragDataDrop got all the formats.\n", _FL);
					OnComplete(false);
					return OBJ_DELETED;
				}
				break;
			}
		}
		return 0;
	}
	
	void OnPulse()
	{
		auto now = LCurrentTime();
		if (now - Start >= 5000)
		{
			OnComplete(true);
		}
	}
	
	void OnComplete(bool isTimeout)
	{
		auto Result = t->OnDrop(t->Data, p, KeyState);
		// if (Flags != DROPEFFECT_NONE)
		// 	gdk_drag_status(context, EffectToDragAction(Flags), time);
		// return Result != DROPEFFECT_NONE;
		
		delete this;
	}
};

gboolean
LWindowDragDataDrop(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, LWindow *Wnd)
{
	auto obj = new LGtkDrop(widget, context, x, y, time, Wnd);
	return obj != NULL;
}

void
LWindowDragEnd(GtkWidget *widget, GdkDragContext *context, LWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

gboolean
LWindowDragFailed(GtkWidget *widget, GdkDragContext *context, GtkDragResult result, LWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
	return false;
}

void
LWindowDragLeave(GtkWidget *widget, GdkDragContext *context, guint time, LWindow *Wnd)
{
	LgiTrace("%s:%i - %s %s\n", _FL, Wnd->GetClass(), __func__);
}

gboolean
LWindowDragMotion(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, LWindow *Wnd)
{
	LPoint p;
	LViewI *v;
	LDragDropTarget *t;
	if (!DndPointMap(v, p, t, Wnd, x, y))
		return false;

	LString::Array Formats;
	int Flags = GetAcceptFmts(Formats, context, t, p);
	
	if (Flags != DROPEFFECT_NONE)
		gdk_drag_status(context, EffectToDragAction(Flags), time);

	return Flags != DROPEFFECT_NONE;
}

bool LWindow::Attach(LViewI *p)
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
		SetTitleBar(d->ShowTitleBar);
		SetWillFocus(d->WillFocus);
	
		auto Widget = GTK_WIDGET(Wnd);
		LView *i = this;
		if (Pos.X() > 0 && Pos.Y() > 0)
			gtk_window_resize(Wnd, Pos.X(), Pos.Y());
		
		auto Obj = G_OBJECT(Wnd);
		g_object_set_data(Obj, "LViewI", (LViewI*)this);

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
		g_signal_connect(Obj, "unrealize",				G_CALLBACK(LWindowUnrealize), i);

		g_signal_connect(Obj, "drag-begin",				G_CALLBACK(LWindowDragBegin), i);
		g_signal_connect(Obj, "drag-data-delete",		G_CALLBACK(LWindowDragDataDelete), i);
		g_signal_connect(Obj, "drag-data-get",			G_CALLBACK(LWindowDragDataGet), i);
		g_signal_connect(Obj, "drag-data-received",		G_CALLBACK(LWindowDragDataReceived), i);
		g_signal_connect(Obj, "drag-drop",				G_CALLBACK(LWindowDragDataDrop), i);
		g_signal_connect(Obj, "drag-end",				G_CALLBACK(LWindowDragEnd), i);
		g_signal_connect(Obj, "drag-failed",			G_CALLBACK(LWindowDragFailed), i);
		g_signal_connect(Obj, "drag-leave",				G_CALLBACK(LWindowDragLeave), i);
		g_signal_connect(Obj, "drag-motion",			G_CALLBACK(LWindowDragMotion), i);

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
		gtk_window_set_title(Wnd, LBase::Name());
		d->AttachState = LAttaching;

		// g_action_map_add_action_entries (G_ACTION_MAP(Wnd), app_entries, G_N_ELEMENTS (app_entries), Wnd);

		// This call sets up the GdkWindow handle
		_Root = lgi_widget_new(this, true);
		
		gtk_widget_realize(Widget);
		gtk_window_move(Wnd, Pos.x1, Pos.y1);		

		if (_Root)
        {
			g_signal_connect(_Root, "size-allocate",	G_CALLBACK(GtkRootResize), i);

			auto container = GTK_CONTAINER(Wnd);
			LAssert(container != NULL);
			if (!gtk_widget_get_parent(_Root))
				gtk_container_add(container, _Root);
            gtk_widget_show(_Root);
        }

		// Do a rough layout of child windows
		PourAll();

		// Add icon
		if (d->Icon)
		{
			SetIcon(d->Icon);
			d->Icon.Empty();
		}

		auto p = GetParent();
		if (p)
		{
			auto pHnd = p->WindowHandle();
			if (!pHnd)
				LgiTrace("%s:%i - SetParent() - no pHnd from %s.\n", _FL, p->GetClass());
			else
				gtk_window_set_transient_for(GTK_WINDOW(Wnd), pHnd);
		}
		
		Status = true;
	}
	
	return Status;
}

bool LWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LCloseApp();
	}

	return LView::OnRequestClose(OsShuttingDown);
}

bool LWindow::HandleViewMouse(LView *v, LMouse &m)
{
	if (m.Down() && !m.IsMove())
	{
		bool InPopup = false;
		for (LViewI *p = v; p; p = p->GetParent())
		{
			if (dynamic_cast<LPopup*>(p))
			{
				InPopup = true;
				break;
			}
		}
		if (!InPopup && LPopup::CurrentPopups.Length())
		{
			for (int i=0; i<LPopup::CurrentPopups.Length(); i++)
			{
				LPopup *p = LPopup::CurrentPopups[i];
				if (p->Visible())
					p->Visible(false);
			}
		}
	}

	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & LMouseEvents)
		{
			if (!d->Hooks[i].Target->OnViewMouse(v, m))
			{
				return false;
			}
		}
	}
	
	return true;
}

bool LWindow::HandleViewKey(LView *v, LKey &k)
{
	bool Status = false;
	LViewI *Ctrl = 0;
	
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
	LViewI *p;
	for (p = v->GetParent(); p; p = p->GetParent())
	{
		if (dynamic_cast<LPopup*>(p))
		{
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				LgiTrace("\tSending key to popup\n");
			#endif
			
			return v->OnKey(k);
		}
	}

	// Give key to popups
	if (LAppInst &&
		LAppInst->GetMouseHook() &&
		LAppInst->GetMouseHook()->OnViewKey(v, k))
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
		// if (Debug) LgiTrace("\tHook[%i]\n", i);
		#endif
		if (d->Hooks[i].Flags & LKeyEvents)
		{
			LView *Target = d->Hooks[i].Target;
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				LgiTrace("\tHook[%i].Target=%p %s\n", i, Target, Target->GetClass());
			#endif
			if (Target->OnViewKey(v, k))
			{
				Status = true;
				
				#if DEBUG_HANDLEVIEWKEY
				if (Debug)
					LgiTrace("\tHook[%i] ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", i, SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
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
		#ifdef LK_KEYPADENTER
		case LK_KEYPADENTER:
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
		LViewI *Wnd = GetNextTabStop(v, k.Shift());
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
		LViewI *c = Map.Find(ToUpper(k.c16));
		if (c)
		{
			c->OnNotify(c, LNotifyActivate);
			return true;
		}
	}

AllDone:
	if (d)
		d->LastKey = k;

	return Status;
}


void LWindow::Raise()
{
	if (Wnd)
		gtk_window_present(Wnd);
}


LWindowZoom LWindow::GetZoom()
{
	switch (d->State)
	{
		case GDK_WINDOW_STATE_ICONIFIED:
			return LZoomMin;
		case GDK_WINDOW_STATE_MAXIMIZED:
			return LZoomMax;
		default:
			break;
	}

	return LZoomNormal;
}

void LWindow::SetZoom(LWindowZoom i)
{
	if (!Wnd)
	{
		// LgiTrace("%s:%i - No window.\n", _FL);
		return;
	}
	

	ThreadCheck();

	switch (i)
	{
		case LZoomMin:
		{
			gtk_window_iconify(Wnd);
			break;
		}
		case LZoomNormal:
		{
			gtk_window_deiconify(Wnd);
			gtk_window_unmaximize(Wnd);
			break;
		}
		case LZoomMax:
		{
			gtk_window_maximize(Wnd);
			break;
		}
		default:
		{
			LgiTrace("%s:%i - Error: unsupported zoom.\n", _FL);
			break;
		}
	}
}

LViewI *LWindow::GetDefault()
{
	return _Default;
}

void LWindow::SetDefault(LViewI *v)
{
	if (v &&
		v->GetWindow() == this)
	{
		if (_Default != v)
		{
			LViewI *Old = _Default;
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

bool LWindow::SetTitleBar(bool ShowTitleBar)
{
	d->ShowTitleBar = ShowTitleBar;
	if (Wnd)
	{
		ThreadCheck();
		gtk_window_set_decorated(Wnd, ShowTitleBar);
	}
	
	return true;
}

bool LWindow::Name(const char *n)
{
	if (Wnd)
	{
		ThreadCheck();
		gtk_window_set_title(Wnd, n);
	}

	return LBase::Name(n);
}

const char *LWindow::Name()
{
	return LBase::Name();
}

struct CallbackParams
{
	LRect Menu;
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

LPoint LWindow::GetDpi() const
{
	return LScreenDpi();
}

LPointF LWindow::GetDpiScale()
{
	auto Dpi = GetDpi();
	return LPointF((double)Dpi.x/96.0, (double)Dpi.y/96.0);
}

LRect &LWindow::GetClient(bool ClientSpace)
{
	static LRect r;
	
	if (_RootAlloc.Valid())
	{	
		if (ClientSpace)
			r = _RootAlloc.ZeroTranslate();
		else
			r = _RootAlloc;
	}
	else
	{
		// Use something vaguely plausible before we're mapped
		r = LView::GetClient(ClientSpace);
	}

	#if 0
	if (Wnd)
	{
		CallbackParams p;
		gtk_container_forall(GTK_CONTAINER(Wnd), (GtkCallback)ClientCallback, &p);
		if (p.Menu.Valid())
		{
			if (ClientSpace)
				r.y2 -= p.Menu.Y();
			else
				r.y1 += p.Menu.Y();
		}
	}
	#endif

	return r;
}

bool LWindow::SerializeState(LDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;

	if (Load)
	{
		::LVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			LRect Position(0, 0, -1, -1);
			LWindowZoom State = LZoomNormal;

// printf("SerializeState load %s\n", v.Str());


			LToken t(v.Str(), ";");
			for (int i=0; i<t.Length(); i++)
			{
				char *Var = t[i];
				char *Value = strchr(Var, '=');
				if (Value)
				{
					*Value++ = 0;

					if (stricmp(Var, "State") == 0)
						State = (LWindowZoom) atoi(Value);
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
		LWindowZoom State = GetZoom();
		sprintf(s, "State=%i;Pos=%s", State, GetPos().GetStr());

		::LVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
}

LRect &LWindow::GetPos()
{
	return Pos;
}

bool LWindow::SetPos(LRect &p, bool Repaint)
{
	Pos = p;
	if (Wnd)
	{
		ThreadCheck();
		
		gtk_window_resize(Wnd, MAX(1, Pos.X()), Pos.Y());
		gtk_window_move(Wnd, Pos.x1, Pos.y1);
	}

	OnPosChange();
	return true;
}

void LWindow::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	// Force repour
	d->Sx = d->Sy = -1;
}

void LWindow::OnCreate()
{
	AttachChildren();
}

void LWindow::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
}

void LWindow::OnPosChange()
{
	LView::OnPosChange();

	if (d->Sx != X() ||	d->Sy != Y())
	{
		PourAll();
		d->Sx = X();
		d->Sy = Y();
	}
}

#define IsTool(v) \
	( \
		dynamic_cast<LView*>(v) \
		&& \
		dynamic_cast<LView*>(v)->_IsToolBar \
	)

void LWindow::PourAll()
{
	LRect c;
	if (_Root)
		c = GtkGetPos(_Root).ZeroTranslate();
	else
		c = GetClient();
		
	if (c.X() < 20 || c.Y() < 20)
		return; // IDK, GTK is weird sometimes... filter out low sizes.

	LRegion Client(c);
	LViewI *MenuView = 0;

	LRegion Update(Client);
	bool HasTools = false;
	LViewI *v;
	List<LViewI>::I Lst = Children.begin();

	{
		LRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			bool IsMenu = MenuView == v;
			if (!IsMenu && IsTool(v))
			{
				LRect OldPos = v->GetPos();
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

						auto vpos = v->GetPos();
						if (OldPos != vpos)
						{
							// position has changed update...
							v->Invalidate();
						}

						// Has it increased the size of the toolbar area?
						auto b = Tools.Bound();
						if (vpos.y2 >= b.y2)
						{
							LRect Bar = Client;
							Bar.y2 = vpos.y2;
							Client.Subtract(&Bar);
							// LgiTrace("IncreaseToolbar=%s\n", Bar.GetStr());
						}

						Tools.Subtract(&vpos);
						Update.Subtract(&vpos);
						// LgiTrace("vpos=%s\n", vpos.GetStr());
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

						LRect Bar(v->GetPos());
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
	for (LViewI *v = *Lst; v; v = *++Lst)
	{
		bool IsMenu = MenuView == v;
		if (!IsMenu && !IsTool(v))
		{
			LRect OldPos = v->GetPos();
			if (OldPos.Valid())
				Update.Union(&OldPos);

			if (v->Pour(Client))
			{
				// LRect p = v->GetPos();
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

LMessage::Param LWindow::OnEvent(LMessage *m)
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

	return LView::OnEvent(m);
}

bool LWindow::RegisterHook(LView *Target, LWindowHookType EventType, int Priority)
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

bool LWindow::UnregisterHook(LView *Target)
{
	int i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}

void LWindow::OnFrontSwitch(bool b)
{
}

LViewI *LWindow::GetFocus()
{
	return d->Focus;
}

#if DEBUG_SETFOCUS
static LAutoString DescribeView(LViewI *v)
{
	if (!v)
		return LAutoString(NewStr("NULL"));

	char s[512];
	int ch = 0;
	LArray<LViewI*> p;
	for (LViewI *i = v; i; i = i->GetParent())
	{
		p.Add(i);
	}
	for (int n=MIN(3, p.Length()-1); n>=0; n--)
	{
		char Buf[256] = "";
		if (!stricmp(v->GetClass(), "LMdiChild"))
			sprintf(Buf, "'%s'", v->Name());
		v = p[n];
		
		ch += sprintf_s(s + ch, sizeof(s) - ch, "%s>%s", Buf, v->GetClass());
	}
	return LAutoString(NewStr(s));
}
#endif

void LWindow::SetFocus(LViewI *ctrl, FocusType type)
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
				LAutoString _ctrl = DescribeView(ctrl);
				LgiTrace("SetFocus(%s, %s) already has focus.\n", _ctrl.Get(), TypeName);
				#endif
				return;
			}

			if (d->Focus)
			{
				LView *gv = d->Focus->GetLView();
				if (gv)
				{
					#if DEBUG_SETFOCUS
					LAutoString _foc = DescribeView(d->Focus);
					LgiTrace(".....defocus LView: %s\n", _foc.Get());
					#endif
					gv->_Focus(false);
				}
				else if (IsActive())
				{
					#if DEBUG_SETFOCUS
					LAutoString _foc = DescribeView(d->Focus);
					LgiTrace(".....defocus view: %s (active=%i)\n", _foc.Get(), IsActive());
					#endif
					d->Focus->OnFocus(false);
					d->Focus->Invalidate();
				}
			}

			d->Focus = ctrl;

			if (d->Focus)
			{
				#if DEBUG_SETFOCUS
				static int Count = 0;
				#endif
				
				LView *gv = d->Focus->GetLView();
				if (gv)
				{
					#if DEBUG_SETFOCUS
					LAutoString _set = DescribeView(d->Focus);
					LgiTrace("LWindow::SetFocus(%s, %s) %i focusing LView\n",
						_set.Get(),
						TypeName,
						Count++);
					#endif

					gv->_Focus(true);
				}
				else if (IsActive())
				{			
					#if DEBUG_SETFOCUS
					LAutoString _set = DescribeView(d->Focus);
					LgiTrace("LWindow::SetFocus(%s, %s) %i focusing nonGView (active=%i)\n",
						_set.Get(),
						TypeName,
						Count++,
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
			LAutoString _Ctrl = DescribeView(d->Focus);
			LAutoString _Focus = DescribeView(d->Focus);
			LgiTrace("LWindow::SetFocus(%s, %s) d->Focus=%s\n",
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
				LAutoString _Ctrl = DescribeView(d->Focus);
				LgiTrace("LWindow::SetFocus(%s, %s) on delete\n",
					_Ctrl.Get(),
					TypeName);
				#endif
				d->Focus = NULL;
			}
			break;
		}
	}
}

bool LWindow::SetWillFocus(bool f)
{
	d->WillFocus = f;
	if (Wnd)
	{
		if (f)
		{
		}
		else
		{
			gtk_window_set_type_hint(Wnd, GDK_WINDOW_TYPE_HINT_POPUP_MENU);
			// printf("%s:%i - gtk_window_set_type_hint=GDK_WINDOW_TYPE_HINT_POPUP_MENU.\n", _FL);
		}
	}

	return true;
}

void LWindow::SetDragHandlers(bool On)
{
}

void LWindow::SetParent(LViewI *p)
{
	LView::SetParent(p);
	if (p && Wnd)
	{
		auto pHnd = p->WindowHandle();
		if (!pHnd)
			LgiTrace("%s:%i - SetParent() - no pHnd from %s.\n", _FL, p->GetClass());
		else if (!GTK_IS_WINDOW(Wnd))
			LgiTrace("%s:%i - SetParent() - GTK_IS_WINDOW failed.\n", _FL);
		else
			gtk_window_set_transient_for(GTK_WINDOW(Wnd), pHnd);
	}
}

bool LWindow::IsAttached()
{
	return	d->AttachState == LAttaching ||
			d->AttachState == LAttached;
}

void LWindow::OnTrayClick(LMouse &m)
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

void LWindow::Quit(bool DontDelete)
{
	ThreadCheck();
	
	if (Wnd)
	{
		d->AttachState = LDetaching;
		auto wnd = Wnd;
		Wnd = NULL;
		gtk_widget_destroy(GTK_WIDGET(wnd));
	}
}


void LWindow::SetAlwaysOnTop(bool b)
{
}
