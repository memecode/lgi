#include "Lgi.h"
#include "GDragAndDrop.h"

#define DEBUG_KEY_EVENT		0

using namespace Gtk;
#include "LgiWidget.h"
#include "gdk/gdkkeysyms.h"

typedef struct _LgiWidget LgiWidget;

struct _LgiWidget
{
	GtkContainer widget;
	#if GTK_MAJOR_VERSION == 3
	GdkWindow *window;
	#endif

	GViewI *target;
	int w, h;
	bool pour_largest;
	bool drag_over_widget;
	char *drop_format;
	bool debug;

	struct ChildInfo
	{
		int x;
		int y;
		GtkWidget *w;
	};

	::GArray<ChildInfo> child;
};

static void lgi_widget_class_init(LgiWidgetClass *klass);

static void
lgi_widget_forall(	GtkContainer   *container,
					gboolean	include_internals,
					GtkCallback     callback,
					gpointer        callback_data)
{
	LgiWidget *w = LGI_WIDGET(container);
	for (int i=0; i<w->child.Length(); i++)
		(*callback)(w->child[i].w, callback_data);
}

static
GType lgi_widget_child_type(GtkContainer   *container)
{
	return GTK_TYPE_WIDGET;
}

#if GTK_MAJOR_VERSION == 3
G_DEFINE_TYPE(LgiWidget, lgi_widget, GTK_TYPE_CONTAINER)
#else
lgi_widget_get_type(void)
{
	static GtkType lgi_widget_type = 0;

	if (!lgi_widget_type)
	{
		static const GtkTypeInfo lgi_widget_info =
		{
			"LgiWidget",
			sizeof(LgiWidget),
			sizeof(LgiWidgetClass),
			(GtkClassInitFunc) lgi_widget_class_init,
			(GtkObjectInitFunc) lgi_widget_init,
			NULL,
			NULL,
			(GtkClassInitFunc) NULL
		};
		lgi_widget_type = gtk_type_unique(GTK_TYPE_CONTAINER, &lgi_widget_info);
	}

	return lgi_widget_type;
}
#endif

GtkWidget *lgi_widget_new(GViewI *target, int w, int h, bool pour_largest)
{
	#if GTK_MAJOR_VERSION == 3
	LgiWidget *p = LGI_WIDGET(g_object_new(lgi_widget_get_type(), NULL));
	#else
	LgiWidget *p = LGI_WIDGET(gtk_type_new(lgi_widget_get_type()));
	#endif
	if (p)
	{
		// printf("Created %p for %s:%p\n", p, target->GetClass(), target);
		p->target = target;
		p->w = w;
		p->h = h;
		p->pour_largest = pour_largest;
		
		g_object_set_data(G_OBJECT(p), "GViewI", target);

        if (target->GetTabStop())
        {
			#if GtkVer(2, 18)
            gtk_widget_set_can_focus(GTK_WIDGET(p), TRUE);
			#else			
			GTK_OBJECT_FLAGS(GTK_WIDGET(p)) |= GTK_CAN_FOCUS;
			#endif
        }

		gtk_widget_add_events(GTK_WIDGET(p), GDK_ALL_EVENTS_MASK);
	}	
	return GTK_WIDGET(p);
}

static void
lgi_widget_remove(GtkContainer *wid, GtkWidget *child)
{
	LgiWidget *p = LGI_WIDGET(wid);
	if (p)
	{
		for (int i=0; i<p->child.Length(); i++)
		{
			_LgiWidget::ChildInfo &c = p->child[i];
			if (c.w == child)
			{
				bool widget_was_visible = gtk_widget_get_visible(child);
				
				LgiWidget *cw = LGI_WIDGET(c.w);
				// if (cw)
					// printf("Unparenting %p, %s.%p\n", cw, cw->target->GetClass(), cw->target);
				gtk_widget_unparent(child);
				
				p->child.DeleteAt(i, true);
				if (widget_was_visible)
					gtk_widget_queue_resize(GTK_WIDGET(wid));
				break;
			}
		}
	}
}

static gboolean lgi_widget_click(GtkWidget *widget, GdkEventButton *ev)
{
    bool BtnDown =  ev->type == GDK_BUTTON_PRESS ||
                    ev->type == GDK_2BUTTON_PRESS ||
                    ev->type == GDK_3BUTTON_PRESS;

	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
        GMouse m;
        m.Target = v;
        m.x = ev->x;
        m.y = ev->y;
        m.Double(ev->type == GDK_2BUTTON_PRESS ||
                ev->type == GDK_3BUTTON_PRESS);
        m.Down( ev->type == GDK_BUTTON_PRESS ||
                ev->type == GDK_2BUTTON_PRESS ||
                ev->type == GDK_3BUTTON_PRESS);
        m.Left(ev->button == 1);
        m.Middle(ev->button == 2);
        m.Right(ev->button == 3);
        m.Alt((ev->state & GDK_MOD1_MASK) != 0);
        m.Shift((ev->state & GDK_SHIFT_MASK) != 0);
        m.Ctrl((ev->state & GDK_CONTROL_MASK) != 0);

		#if 0
		char s[256];
		sprintf_s(s, sizeof(s), "%s::MouseClick", v->GetClass());
		m.Trace(s);
		#endif

        v->_Mouse(m, false);
    }    
    return TRUE;
}

static gboolean lgi_widget_motion(GtkWidget *widget, GdkEventMotion *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
        GMouse m;
        m.Target = v;
        m.x = ev->x;
        m.y = ev->y;
        m.Flags |= LGI_EF_MOVE;
        m.Down((ev->state & GDK_BUTTON_PRESS_MASK) != 0);
        m.Left(ev->state & GDK_BUTTON1_MASK);
        m.Middle(ev->state & GDK_BUTTON2_MASK);
        m.Right(ev->state & GDK_BUTTON3_MASK);

		#if 0
		char s[256];
		sprintf_s(s, sizeof(s), "%s::MouseMove", v->GetClass());
		m.Trace(s);
		#endif
        
        v->_Mouse(m, true);
    }
    
    return TRUE;
}

static gboolean lgi_widget_scroll(GtkWidget *widget, GdkEventScroll *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
		double Lines = ev->direction == GDK_SCROLL_DOWN ? 3 : -3;
		// LgiTrace("%s::OnMouseWheel %g\n", v->GetClass(), Lines);
        v->OnMouseWheel(Lines);
    }    
	return TRUE;
}

static gboolean lgi_widget_mouse_enter_leave(GtkWidget *widget, GdkEventCrossing *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
        GMouse m;
        m.Target = v;
        m.x = ev->x;
        m.y = ev->y;

		if (ev->type == GDK_LEAVE_NOTIFY)
		{
			// LgiTrace("%s::OnMouseExit %i,%i\n", v->GetClass(), m.x, m.y);
			v->OnMouseExit(m);
		}
		else
		{
			// LgiTrace("%s::OnMouseEnter %i,%i\n", v->GetClass(), m.x, m.y);
			v->OnMouseEnter(m);
		}
    }    
	return TRUE;
}

static gboolean lgi_widget_focus_event(GtkWidget *wid, GdkEventFocus *e)
{
	LgiWidget *p = LGI_WIDGET(wid);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
        v->OnFocus(e->in);
    else
		LgiAssert(0);
    
	return TRUE;
}

void BuildTabStops(GViewI *v, ::GArray<GViewI*> &a)
{
    if (v->Enabled() &&
        v->Visible() &&
        v->GetTabStop())
        a.Add(v);
    
    GAutoPtr<GViewIterator> it(v->IterateViews());
    for (GViewI *c = it->First(); c; c = it->Next())
    {
        if (c->Enabled() &&
            c->Visible())
            BuildTabStops(c, a);
    }
}

#if GTK_MAJOR_VERSION == 3
#define KEY(name) GDK_KEY_##name
#else
#define KEY(name) GDK_##name
#endif

static gboolean lgi_widget_key_event(GtkWidget *wid, GdkEventKey *e)
{
    #if 0
    // This is useful for debugging...
    if (e->keyval == GDK_Shift_L ||
        e->keyval == GDK_Shift_R ||
        e->keyval == GDK_Control_L ||
        e->keyval == GDK_Control_R ||
        e->keyval == GDK_Alt_L ||
        e->keyval == GDK_Alt_R)
    {
        return TRUE;
    }
    #endif

	LgiWidget *p = LGI_WIDGET(wid);
    GView *v = dynamic_cast<GView*>(p->target);
    if (!v)
    	printf("%s:%i - No target??\n", _FL);
    else
    {
        GKey k;
        k.Down(e->type == GDK_KEY_PRESS);
        k.c16 = k.vkey = e->keyval;
        k.Shift((e->state & 1) != 0);
        k.Ctrl((e->state & 4) != 0);
        k.Alt((e->state & 8) != 0);
        
        // k.IsChar = !k.Ctrl() && (k.c16 >= ' ' && k.c16 <= 0x7f);
		k.IsChar = !k.Ctrl() &&
					!k.Alt() && 
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
			case KEY(ISO_Left_Tab):
			case KEY(Tab):
            	k.IsChar = true;
            	k.c16 = k.vkey = VK_TAB;
            	break;
			case KEY(Return):
			case KEY(KP_Enter):
            	k.IsChar = true;
            	k.c16 = k.vkey = VK_RETURN;
            	break;
            case KEY(BackSpace):
            	k.c16 = k.vkey = VK_BACKSPACE;
            	k.IsChar = !k.Ctrl() && !k.Alt();
            	break;
            case KEY(Left):
            	k.vkey = k.c16 = VK_LEFT;
            	break;
            case KEY(Right):
            	k.vkey = k.c16 = VK_RIGHT;
            	break;
            case KEY(Up):
            	k.vkey = k.c16 = VK_UP;
            	break;
            case KEY(Down):
            	k.vkey = k.c16 = VK_DOWN;
            	break;
            case KEY(Home):
            	k.vkey = k.c16 = VK_HOME;
            	break;
            case KEY(End):
            	k.vkey = k.c16 = VK_END;
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
			KeyPadMap(KEY(KP_F1), VK_F1, false)
			KeyPadMap(KEY(KP_F2), VK_F2, false)
			KeyPadMap(KEY(KP_F3), VK_F3, false)
			KeyPadMap(KEY(KP_F4), VK_F4, false)
			KeyPadMap(KEY(KP_Home), VK_HOME, false)
			KeyPadMap(KEY(KP_Left), VK_LEFT, false)
			KeyPadMap(KEY(KP_Up), VK_UP, false)
			KeyPadMap(KEY(KP_Right), VK_RIGHT, false)
			KeyPadMap(KEY(KP_Down), VK_DOWN, false)
			KeyPadMap(KEY(KP_Page_Up), VK_PAGEUP, false)
			KeyPadMap(KEY(KP_Page_Down), VK_PAGEDOWN, false)
			KeyPadMap(KEY(KP_End), VK_END, false)
			KeyPadMap(KEY(KP_Begin), VK_HOME, false)
			KeyPadMap(KEY(KP_Insert), VK_INSERT, false)
			KeyPadMap(KEY(KP_Delete), VK_DELETE, false)
			KeyPadMap(KEY(KP_Equal), '=', true)
			KeyPadMap(KEY(KP_Multiply), '*', true)
			KeyPadMap(KEY(KP_Add), '+', true)
			KeyPadMap(KEY(KP_Separator), '|', true) // is this right?
			KeyPadMap(KEY(KP_Subtract), '-', true)
			KeyPadMap(KEY(KP_Decimal), '.', true)
			KeyPadMap(KEY(KP_Divide), '/', true)
        }
        
        #if DEBUG_KEY_EVENT
        k.Trace("lgi_widget_key_event");
        #endif

        GWindow *w = v->GetWindow();
        if (w)
        {
            if (!w->HandleViewKey(v, k) &&
                (k.vkey == VK_TAB || k.vkey == KEY(ISO_Left_Tab)) &&
                k.Down())
            {
            	// Do tab between controls
                ::GArray<GViewI*> a;
                BuildTabStops(w, a);
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
        }
        else v->OnKey(k);
    }
    
    return true;
}

static void
lgi_widget_drag_begin(GtkWidget *widget, GdkDragContext *context)
{
	LgiTrace("lgi_widget_drag_begin\n");
}

static void
lgi_widget_drag_end(GtkWidget *widget, GdkDragContext *drag_context)
{
	LgiTrace("lgi_widget_drag_end\n");
}

static void
lgi_widget_drag_data_get(GtkWidget          *widget,
						GdkDragContext      *context,
						GtkSelectionData    *selection_data,
						guint               info,
						guint               time_)
{
	LgiTrace("lgi_widget_drag_data_get\n");
}

static void
lgi_widget_drag_data_delete(GtkWidget	       *widget,
							GdkDragContext     *context)
{
	LgiTrace("lgi_widget_drag_data_delete\n");
}

static void
lgi_widget_drag_leave(GtkWidget	       *widget,
				    GdkDragContext     *context,
				    guint               time)
{
	LgiWidget *v = LGI_WIDGET(widget);
	if (!v || !v->target)
	{
		// printf("%s:%i - LGI_WIDGET failed.\n", _FL);
		return;
	}
	
	GDragDropTarget *Target = v->target->DropTarget();
	if (!Target)
	{
		// printf("%s:%i - View '%s' doesn't have drop target.\n", _FL, v->target->GetClass());
		return;
	}

	v->drag_over_widget = false;
	Target->OnDragExit();
}

static gboolean
lgi_widget_drag_motion(GtkWidget	   *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    guint               time_)
{
	LgiWidget *v = LGI_WIDGET(widget);
	if (!v || !v->target)
	{
		printf("%s:%i - LGI_WIDGET failed.\n", _FL);
		return false;
	}
	
	GViewI *view = v->target;
	#if DEBUG_DND
	printf("%s:%i - DragMotion %s\n", _FL, view->GetClass());
	#endif

	GDragDropTarget *Target = view->DropTarget();
	while (view && !Target)
	{
		view = view->GetParent();
		if (!view)
			break;
		Target = view->DropTarget();
		#if DEBUG_DND
		printf("\t%s = %p\n", view->GetClass(), Target);
		#endif
	}
	
	if (!Target)
	{
		#if DEBUG_DND
		printf("%s:%i - View '%s' doesn't have drop target.\n", _FL, v->target->GetClass());
		#endif
		return false;
	}

	#if DEBUG_DND
	// printf("%s:%i - DragMotion(%s): ", _FL, v->target->GetClass());
	#endif
	
	List<char> Formats;
	for (Gtk::GList *Types = gdk_drag_context_list_targets(context); Types; Types = Types->next)
	{
		gchar *Type = gdk_atom_name((GdkAtom)Types->data);
		if (Type)
		{
			Formats.Insert(NewStr(Type));
			#if DEBUG_DND
			// printf("%s, ", Type);
			#endif
		}
	}
	#if DEBUG_DND
	// printf("\n");
	#endif
	
	if (!v->drag_over_widget)
	{
		v->drag_over_widget = true;
		Target->OnDragEnter();
	}
	
	GdcPt2 p(x, y);
	int Result = Target->WillAccept(Formats, p, 0);
	Formats.DeleteArrays();
	if (Result != DROPEFFECT_NONE)
	{
		GdkDragAction action = DropEffectToAction(Result);
		gdk_drag_status(context, action, time_);
	}

	return Result != DROPEFFECT_NONE;
}

static gboolean
lgi_widget_drag_drop(GtkWidget	       *widget,
					GdkDragContext     *context,
					gint                x,
					gint                y,
					guint               time_)
{
	LgiWidget *v = LGI_WIDGET(widget);
	if (!v || !v->target)
	{
		printf("%s:%i - LGI_WIDGET failed.\n", _FL);
		return false;
	}
	
	GViewI *view = v->target;
	GDragDropTarget *Target = view->DropTarget();
	while (view && !Target)
	{
		view = view->GetParent();
		if (view)
			Target = view->DropTarget();
	}
	if (!Target)
	{
		#if DEBUG_DND
		printf("%s:%i - View '%s' doesn't have drop target.\n", _FL, v->target->GetClass());
		#endif
		return false;
	}

	// Convert the GTK list of formats to our own List
	List<char> Formats;
	for (Gtk::GList *Types = gdk_drag_context_list_targets(context); Types; Types = Types->next)
	{
		gchar *Type = gdk_atom_name((GdkAtom)Types->data);
		if (Type)
			Formats.Insert(NewStr(Type));
	}

	// Select a format from the supplied types
	GdcPt2 p(x, y);
	int Result = Target->WillAccept(Formats, p, 0);
	if (Result == DROPEFFECT_NONE)
		return false;

	char *drop_format = Formats[0];
	Formats.Delete(drop_format);
	Formats.DeleteArrays();
	#if DEBUG_DND
	LgiTrace("lgi_widget_drag_drop, fmt=%s\n", drop_format);
	#endif

	// Request the data...
	gtk_drag_get_data
	(
		widget,
		context,
		gdk_atom_intern(drop_format, false),
		time_
	);

	DeleteArray(drop_format);
	
	return true;
}

static void
lgi_widget_drag_data_received(	GtkWidget			*widget,
								GdkDragContext		*context,
								gint				x,
								gint				y,
								GtkSelectionData	*data,
								guint				info,
								guint				time)
{
	// LgiTrace("lgi_widget_drag_data_received\n");

	LgiWidget *v = LGI_WIDGET(widget);
	if (!v || !v->target)
	{
		// printf("%s:%i - LGI_WIDGET failed.\n", _FL);
		return;
	}
	
	GViewI *view = v->target;
	GDragDropTarget *Target = view->DropTarget();
	while (view && !Target)
	{
		view = view->GetParent();
		if (view)
			Target = view->DropTarget();
	}
	if (!Target)
	{
		#if DEBUG_DND
		printf("%s:%i - View '%s' doesn't have drop target.\n", _FL, v->target->GetClass());
		#endif
		return;
	}
	
	gchar *Type = gdk_atom_name(gtk_selection_data_get_data_type(data));
	#if DEBUG_DND
	printf("%s:%i - Type=%s, Target=%s\n", _FL, Type, v->target->GetClass());
	#endif
	GdcPt2 p(x, y);
	gint Len = gtk_selection_data_get_length(data);
	#if DEBUG_DND
	printf("%s:%i - Len=%i\n", _FL, Len);
	#endif

	const guchar *Ptr = gtk_selection_data_get_data(data);
	#if DEBUG_DND
	printf("%s:%i - Ptr=%p\n", _FL, Ptr);
	#endif
	if (!Ptr || Len <= 0)
	{
		#if DEBUG_DND
		printf("%s:%i - gtk_selection_data_get_[data/len] failed.\n", _FL);
		#endif
		return;
	}

	::GArray<GDragData> dd;
	dd[0].Format = Type;
	dd[0].Data[0].SetBinary(Len, (void*)Ptr);
	
	// Give the data to the App
	Target->OnDrop(dd, p, 0);
}

static void
lgi_widget_destroy(
	#if GTK_MAJOR_VERSION == 3
	GtkWidget *object
	#else
	GtkObject *object
	#endif
	)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(LGI_IS_WIDGET(object));

	LgiWidget *p = LGI_WIDGET(object);
	#if GTK_MAJOR_VERSION == 3
	void *cls = g_type_class_peek(gtk_widget_get_type());
	if (cls && GTK_WIDGET_CLASS(cls)->destroy)
		(*GTK_WIDGET_CLASS(cls)->destroy)(object);
	#else
	void *klass = gtk_type_class(gtk_widget_get_type());

	if (GTK_OBJECT_CLASS(klass)->destroy)
		(* GTK_OBJECT_CLASS(klass)->destroy)(object);
	#endif
}


static void
lgi_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(LGI_IS_WIDGET(widget));
	g_return_if_fail(allocation != NULL);

	#if GTK_MAJOR_VERSION == 3

		gtk_widget_set_allocation (widget, allocation);

	#else

		auto Wnd = widget->window;
		widget->allocation = *allocation;
		if (GTK_WIDGET_REALIZED(widget))
		{
			gdk_window_move_resize( Wnd,
									allocation->x, allocation->y,
									allocation->width, allocation->height);
			LgiWidget *w = LGI_WIDGET(widget);
			if (!_stricmp(w->target->GetClass(), "GWindow"))
				LgiTrace("%s - %i x %i\n", w->target->GetClass(), allocation->width, allocation->height);

			GtkAllocation child_allocation;
			GtkRequisition child_requisition;
			#if GTK_MAJOR_VERSION == 3
			auto border_width = gtk_container_get_border_width(GTK_CONTAINER(w));
			#else
			auto border_width = GTK_CONTAINER(w)->border_width;
			#endif

			for (int i=0; i<w->child.Length(); i++)
			{
				_LgiWidget::ChildInfo &c = w->child[i];
				#if GTK_MAJOR_VERSION == 3
				if (gtk_widget_get_visible(c.w))
				#else
				if (GTK_WIDGET_VISIBLE(c.w))
				#endif
				{
					gtk_widget_size_request(c.w, &child_requisition);
					child_allocation.x = c.x + border_width;
					child_allocation.y = c.y + border_width;

					#if GTK_MAJOR_VERSION == 3
					if (!gtk_widget_get_has_window(widget))
					{
						child_allocation.x += allocation->x;
						child_allocation.y += allocation->y;
					}
					#else
					if (GTK_WIDGET_NO_WINDOW(widget))
					{
						child_allocation.x += widget->allocation.x;
						child_allocation.y += widget->allocation.y;
					}
					#endif

					child_allocation.width = MAX(child_requisition.width, 1);
					child_allocation.height = MAX(child_requisition.height, 1);
					gtk_widget_size_allocate(c.w, &child_allocation);
				}
			}
		}
	
	#endif
}

static void
lgi_widget_realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;
	guint attributes_mask;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(LGI_IS_WIDGET(widget));

	LgiWidget *w = LGI_WIDGET(widget);

	#if GTK_MAJOR_VERSION == 3

		GtkAllocation allocation;
		GdkWindow *window;

		gtk_widget_get_allocation (widget, &allocation);

		gtk_widget_set_realized (widget, TRUE);

		attributes.window_type = GDK_WINDOW_CHILD;
		attributes.x = allocation.x;
		attributes.y = allocation.y;
		attributes.width = allocation.width;
		attributes.height = allocation.height;
		attributes.wclass = GDK_INPUT_ONLY;
		attributes.event_mask = gtk_widget_get_events (widget);
		attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
								GDK_BUTTON_RELEASE_MASK |
								GDK_TOUCH_MASK |
								GDK_ENTER_NOTIFY_MASK |
								GDK_LEAVE_NOTIFY_MASK);

		attributes_mask = GDK_WA_X | GDK_WA_Y;

		window = gtk_widget_get_parent_window (widget);
		if (window)
		{
			gtk_widget_set_window (widget, window);
			g_object_ref (window);

			w->window = gdk_window_new (window, &attributes, attributes_mask);
			gtk_widget_register_window (widget, w->window);
		}
		else assert(0);

	#else

		GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

		attributes.window_type = GDK_WINDOW_CHILD;
		attributes.x = widget->allocation.x;
		attributes.y = widget->allocation.y;
		attributes.width = w->w;
		attributes.height = w->h;

		attributes.wclass = GDK_INPUT_OUTPUT;
		attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;

		attributes_mask = GDK_WA_X | GDK_WA_Y;

		auto Par = gtk_widget_get_parent_window(widget);
		auto ParWnd = gdk_window_new(Par,
									&attributes,
									attributes_mask);
		widget->window = ParWnd;
		gdk_window_set_user_data(ParWnd, widget);

		widget->style = gtk_style_attach(widget->style, ParWnd);
		gtk_style_set_background(widget->style, ParWnd, GTK_STATE_NORMAL);
		lgi_widget_size_allocate(widget, &widget->allocation);
	
		GView *v = dynamic_cast<GView*>(w->target);
		if (v)
			v->OnGtkRealize();
		else
			LgiTrace("%s:%i - Failed to cast target to GView.\n", _FL);
	
	#endif
}

#if GTK_MAJOR_VERSION == 3

	static void
	lgi_widget_unrealize (GtkWidget *widget)
	{
		LgiWidget *w = LGI_WIDGET(widget);

		if (w->window)
		{
			gpointer user_data = NULL;
			gdk_window_get_user_data(w->window, &user_data);

			gtk_widget_unregister_window (widget, w->window);
			gdk_window_destroy (w->window);
			gtk_widget_set_window(widget, w->window = NULL);
		}

		GTK_WIDGET_CLASS(lgi_widget_parent_class)->unrealize(widget);
	}

	static gboolean
	lgi_widget_draw(GtkWidget *widget, cairo_t *cr)
	{
		g_return_val_if_fail(widget != NULL, FALSE);
		g_return_val_if_fail(LGI_IS_WIDGET(widget), FALSE);

		LgiWidget *p = LGI_WIDGET(widget);
		if (gtk_widget_is_drawable(widget))
		{
			if (p && p->target)
			{
				GScreenDC Dc(cr, p->w, p->h);
				GView *v = dynamic_cast<GView*>(p->target);
				if (v)
					v->_Paint(&Dc);
				else
					p->target->OnPaint(&Dc);
			}
			else printf("%s:%i - No view to paint widget.\n", _FL);
		}

		GTK_WIDGET_CLASS(lgi_widget_parent_class)->draw(widget, cr);
		return FALSE;
	}

	static void
	lgi_widget_get_preferred_height(GtkWidget *widget, gint *minimum_height, gint *natural_height)
	{
		LgiWidget *p = LGI_WIDGET(widget);
		if (p)
		{
			if (minimum_height)
				*minimum_height = p->pour_largest ? 80 : p->h;
			if (natural_height)
				*natural_height = p->h;
		}
		else LgiAssert(0);
	}

	static void
	lgi_widget_get_preferred_width(GtkWidget *widget, gint *minimum_width, gint *natural_width)
	{
		LgiWidget *p = LGI_WIDGET(widget);
		if (p)
		{
			if (minimum_width)
				*minimum_width = p->pour_largest ? 80 :p->w;
			if (natural_width)
				*natural_width = p->w;
		}
		else LgiAssert(0);
	}

	static void
	lgi_widget_get_preferred_width_for_height (GtkWidget *widget,
											   gint       for_size,
											   gint      *minimum_size,
											   gint      *natural_size)
	{
		lgi_widget_get_preferred_width(widget, minimum_size, natural_size);
	}

	static void
	gtk_button_get_preferred_height_for_width (GtkWidget *widget,
                                           gint       for_size,
                                           gint      *minimum_size,
                                           gint      *natural_size)
	{
		lgi_widget_get_preferred_height(widget, minimum_size, natural_size);
	}

	static void
	lgi_widget_map(GtkWidget *widget)
	{
		LgiWidget *p = LGI_WIDGET(widget);
		GTK_WIDGET_CLASS (lgi_widget_parent_class)->map(widget);		
		if (p->window)
			gdk_window_show(p->window);
	}

	static void
	lgi_widget_unmap (GtkWidget *widget)
	{
		LgiWidget *p = LGI_WIDGET(widget);
		if (p->window)
			gdk_window_hide(p->window);
		GTK_WIDGET_CLASS(lgi_widget_parent_class)->unmap(widget);
	}

#else

	static void
	lgi_widget_size_request(GtkWidget *widget, GtkRequisition *requisition)
	{
		g_return_if_fail(widget != NULL);
		g_return_if_fail(LGI_IS_WIDGET(widget));
		g_return_if_fail(requisition != NULL);

		LgiWidget *p = LGI_WIDGET(widget);
		if (p->pour_largest)
		{
			requisition->width = 10;
			requisition->height = 10;
		}
		else
		{
			requisition->width = p->w;
			requisition->height = p->h;
		}

		// LgiTrace("%s::req %i,%i\n", p->target->GetClass(), requisition->width, requisition->height);
	}

	static gboolean
	lgi_widget_expose(GtkWidget *widget, GdkEventExpose *event)
	{
		g_return_val_if_fail(widget != NULL, FALSE);
		g_return_val_if_fail(LGI_IS_WIDGET(widget), FALSE);
		g_return_val_if_fail(event != NULL, FALSE);

		LgiWidget *p = LGI_WIDGET(widget);
	
		if (GTK_WIDGET_DRAWABLE(widget))
		{
			if (p && p->target)
			{
				GScreenDC Dc(p->target->GetGView());

				GView *v = dynamic_cast<GView*>(p->target);
				if (v)
					v->_Paint(&Dc);
				else
					p->target->OnPaint(&Dc);
			}
			else printf("%s:%i - No view to paint widget.\n", _FL);
		}

		return FALSE;
	}

	static gboolean lgi_widget_client_event(GtkWidget *wid, GdkEventClient *ev)
	{
		LgiWidget *p = LGI_WIDGET(wid);
		GView *v = dynamic_cast<GView*>(p->target);
		if (v)
		{
			GMessage m(ev->data.l[0], ev->data.l[1], ev->data.l[2]);
			v->OnEvent(&m);
		}
		return TRUE;
	}
	
#endif

void
lgi_widget_setsize(GtkWidget *wid, int width, int height)
{
	LgiWidget *p = LGI_WIDGET(wid);
	if (!p) return;

	if (p->w != width ||
	    p->h != height)
	{
		p->w = width;
		p->h = height;
		    
		#if GTK_MAJOR_VERSION == 3
		// LgiAssert(!"Gtk3 FIXME");
		#else
		wid->requisition.width = width;
		wid->requisition.height = height;
		#endif
	}
}

void
lgi_widget_setchildpos(GtkWidget *parent, GtkWidget *child, int x, int y)
{
	if (!LGI_IS_WIDGET(parent))
	{
		LgiAssert(0); // should this ever happen?
		return;
	}

	LgiWidget *p = LGI_WIDGET(parent);
	if (p)
	{
		for (int i=0; i<p->child.Length(); i++)
		{
			_LgiWidget::ChildInfo &c = p->child[i];
			if (c.w == child)
			{
				c.x = x;
				c.y = y;
				
				#if GTK_MAJOR_VERSION == 3
				if (gtk_widget_get_realized(c.w))
				#else
				if (GTK_WIDGET_REALIZED(c.w))
				#endif
				{
                	LgiWidget *child_wid = LGI_WIDGET(c.w);
                	
				    GtkAllocation a;
				    a.x = c.x;
				    a.y = c.y;
				    a.width = MAX(1, child_wid->w);
				    a.height = MAX(1, child_wid->h);
				    
    				gtk_widget_size_allocate(c.w, &a);

            		gdk_window_invalidate_rect(
												#if GtkVer(2, 14)
            									gtk_widget_get_window(c.w)
            									#else
            									c.w->window
							            		#endif
            									, &a, FALSE);
				}
				return;
			}
		}
	}
}

void
lgi_widget_add(GtkContainer *wid, GtkWidget *child)
{
	LgiWidget *p = LGI_WIDGET(wid);
	if (!p)
		return;

	for (int i=0; i<p->child.Length(); i++)
	{
		_LgiWidget::ChildInfo &c = p->child[i];
		if (c.w == child)
		{
			printf("%s:%i - Already a child.\n", _FL);
			return;
		}
	}
		
	_LgiWidget::ChildInfo &c = p->child.New();
	c.w = child;
	c.x = 0;
	c.y = 0;
	gtk_widget_set_parent(child, GTK_WIDGET(wid));
}

gboolean
lgi_widget_configure(GtkWidget *widget, GdkEventConfigure *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
	if (p)
	    p->target->OnPosChange();
    return TRUE;
}

static void
lgi_widget_class_init(LgiWidgetClass *cls)
{
	#if GTK_MAJOR_VERSION == 3
		GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(cls);
		widget_class->destroy = lgi_widget_destroy;
	#else
		GtkObjectClass *object_class = (GtkObjectClass*) cls;
		object_class->destroy = lgi_widget_destroy;
		GtkWidgetClass *widget_class = (GtkWidgetClass*) cls;
	#endif

	#if GTK_MAJOR_VERSION == 3
		widget_class->draw					= lgi_widget_draw;
		widget_class->get_preferred_height	= lgi_widget_get_preferred_height;
		widget_class->get_preferred_width	= lgi_widget_get_preferred_width;
		widget_class->get_preferred_width_for_height = lgi_widget_get_preferred_width_for_height;
		widget_class->get_preferred_height_for_width = gtk_button_get_preferred_height_for_width;
		widget_class->unrealize				= lgi_widget_unrealize;
		widget_class->map					= lgi_widget_map;
		widget_class->unmap					= lgi_widget_unmap;
	#else
		widget_class->size_request			= lgi_widget_size_request;
		widget_class->expose_event			= lgi_widget_expose;
		widget_class->client_event			= lgi_widget_client_event;
	#endif
	widget_class->realize					= lgi_widget_realize;
	widget_class->size_allocate				= lgi_widget_size_allocate;
	widget_class->button_press_event		= lgi_widget_click;
	widget_class->button_release_event		= lgi_widget_click;
	widget_class->motion_notify_event		= lgi_widget_motion;
	widget_class->scroll_event				= lgi_widget_scroll;
	widget_class->enter_notify_event		= lgi_widget_mouse_enter_leave;
	widget_class->leave_notify_event		= lgi_widget_mouse_enter_leave;
	widget_class->focus_in_event			= lgi_widget_focus_event;
	widget_class->focus_out_event			= lgi_widget_focus_event;
	widget_class->key_press_event			= lgi_widget_key_event;
	widget_class->key_release_event			= lgi_widget_key_event;
	widget_class->drag_begin				= lgi_widget_drag_begin;
	widget_class->drag_end					= lgi_widget_drag_end;
	widget_class->drag_data_get				= lgi_widget_drag_data_get;
	widget_class->drag_data_delete			= lgi_widget_drag_data_delete;
	widget_class->drag_leave				= lgi_widget_drag_leave;
	widget_class->drag_motion				= lgi_widget_drag_motion;
	widget_class->drag_drop					= lgi_widget_drag_drop;
	widget_class->drag_data_received		= lgi_widget_drag_data_received;

	GtkContainerClass *container_class = (GtkContainerClass*)cls;
	container_class->add					= lgi_widget_add;
	container_class->remove					= lgi_widget_remove;
	container_class->forall					= lgi_widget_forall;
	container_class->child_type				= lgi_widget_child_type;
}

void
lgi_widget_init(LgiWidget *w)
{
	#if GTK_MAJOR_VERSION == 3
	w->window = NULL;
	gtk_widget_set_has_window(GTK_WIDGET(w), FALSE);
	#else
	GTK_WIDGET_UNSET_FLAGS(w, GTK_NO_WINDOW);
	#endif
	w->target = 0;
	w->w = 0;
	w->h = 0;
	w->pour_largest = false;
	w->drag_over_widget = false;
	w->drop_format = NULL;
	w->debug = false;
}

