/*hdr
**      FILE:           LView.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           23/4/98
**      DESCRIPTION:    Linux LView Implementation
**
**      Copyright (C) 1998-2003, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Popup.h"
#include "lgi/common/Css.h"
#include "lgi/common/EventTargetThread.h"
#include "ViewPriv.h"

using namespace Gtk;
#include "LgiWidget.h"

#define DEBUG_MOUSE_EVENTS			0

#if 0
#define DEBUG_INVALIDATE(...)		printf(__VA_ARGS__)
#else
#define DEBUG_INVALIDATE(...)
#endif

#define ADJ_LEFT					1
#define ADJ_RIGHT					2
#define ADJ_UP						3
#define ADJ_DOWN					4

#if GtkVer(2, 14)
#else
#define gtk_widget_get_window(widget) ((widget)->window)
#endif

struct CursorInfo
{
public:
	LRect Pos;
	LPoint HotSpot;
}
CursorMetrics[] =
{
	// up arrow
	{ LRect(0, 0, 8, 15),			LPoint(4, 0) },
	// cross hair
	{ LRect(20, 0, 38, 18),			LPoint(29, 9) },
	// hourglass
	{ LRect(40, 0, 51, 15),			LPoint(45, 8) },
	// I beam
	{ LRect(60, 0, 66, 17),			LPoint(63, 8) },
	// N-S arrow
	{ LRect(80, 0, 91, 16),			LPoint(85, 8) },
	// E-W arrow
	{ LRect(100, 0, 116, 11),		LPoint(108, 5) },
	// NW-SE arrow
	{ LRect(120, 0, 132, 12),		LPoint(126, 6) },
	// NE-SW arrow
	{ LRect(140, 0, 152, 12),		LPoint(146, 6) },
	// 4 way arrow
	{ LRect(160, 0, 178, 18),		LPoint(169, 9) },
	// Blank
	{ LRect(0, 0, 0, 0),			LPoint(0, 0) },
	// Vertical split
	{ LRect(180, 0, 197, 16),		LPoint(188, 8) },
	// Horizontal split
	{ LRect(200, 0, 216, 17),		LPoint(208, 8) },
	// Hand
	{ LRect(220, 0, 233, 13),		LPoint(225, 0) },
	// No drop
	{ LRect(240, 0, 258, 18),		LPoint(249, 9) },
	// Copy drop
	{ LRect(260, 0, 279, 19),		LPoint(260, 0) },
	// Move drop
	{ LRect(280, 0, 299, 19),		LPoint(280, 0) },
};

// CursorData is a bitmap in an array of uint32's. This is generated from a graphics file:
// ./Code/cursors.png
//
// The pixel values are turned into C code by a program called i.Mage:
//	http://www.memecode.com/image.php
//
// Load the graphic into i.Mage and then go Edit->CopyAsCode
// Then paste the text into the CursorData variable at the bottom of this file.
//
// This saves a lot of time finding and loading an external resouce, and even having to
// bundle extra files with your application. Which have a tendancy to get lost along the
// way etc.
extern uint32_t CursorData[];
LInlineBmp Cursors =
{
	300, 20, 8, CursorData
};

LCaptureThread *LViewPrivate::CaptureThread = NULL;

////////////////////////////////////////////////////////////////////////////
bool LgiIsKeyDown(int Key)
{
	LAssert(0);
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
LCaptureThread::LCaptureThread(LView *v) : LThread("CaptureThread")
{
	name = v->GetClass();
	view = v->AddDispatch();
	DeleteOnExit = true;
	Run();
}

LCaptureThread::~LCaptureThread()
{
	Cancel();
	// Don't wait.. the thread will exit and delete itself asnyronously.
}

int LCaptureThread::Main()
{
	while (!IsCancelled())
	{
		LSleep(EventMs);
		if (!IsCancelled())
			PostThreadEvent(view, M_CAPTURE_PULSE);
	}
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LViewPrivate::LViewPrivate(LView *view) : View(view)
{
	PrevMouse.x = PrevMouse.y = -1;
}

LViewPrivate::~LViewPrivate()
{
	LAssert(PulseThread == 0);

	while (EventTargets.Length())
		delete EventTargets[0];

	if (Font && FontOwnType == GV_FontOwned)
		DeleteObj(Font);
}

void LView::OnGtkRealize()
{
	if (!d->GotOnCreate)
	{
		d->GotOnCreate = true;
		
		if (d->WantsFocus)
		{
			d->WantsFocus = false;
			if (GetWindow())
				GetWindow()->SetFocus(this, LWindow::GainFocus);
		}
		
		OnCreate();
	}

	for (auto c : Children)
	{
		auto gv = c->GetLView();
		if (gv)
			gv->OnGtkRealize();
	}
}

void LView::_Focus(bool f)
{
	ThreadCheck();
	
	if (f)
		SetFlag(WndFlags, GWF_FOCUS);
	else
		ClearFlag(WndFlags, GWF_FOCUS);

	OnFocus(f);	
	Invalidate();

	if (f)
	{
		auto w = GetWindow();
		if (w && w->_Root)
			gtk_widget_grab_focus(w->_Root);
		else
			d->WantsFocus = f;
	}
}

void LView::_Delete()
{
	ThreadCheck();
	SetPulse();

	// Remove static references to myself
	if (_Over == this)
		_Over = NULL;
	if (_Capturing == this)
		_Capturing = NULL;
	
	auto *Wnd = GetWindow();
	if (Wnd && Wnd->GetFocus() == static_cast<LViewI*>(this))
		Wnd->SetFocus(this, LWindow::ViewDelete);

	if (LAppInst && LAppInst->AppWnd == this)
		LAppInst->AppWnd = NULL;

	// Hierarchy
	LViewI *c;
	while ((c = Children[0]))
	{
		if (c->GetParent() != (LViewI*)this)
		{
			printf("%s:%i - ~LView error, %s not attached correctly: %p(%s) Parent: %p(%s)\n",
				_FL, c->GetClass(), c, c->Name(), c->GetParent(), c->GetParent() ? c->GetParent()->Name() : "");
			Children.Delete(c);
		}

		DeleteObj(c);
	}

	Detach();

	// Misc
	Pos.ZOff(-1, -1);
}

LView *&LView::PopupChild()
{
	return d->Popup;
}

void LgiToGtkCursor(LViewI *v, LCursor c)
{
	static LCursor CurrentCursor = LCUR_Normal;

	if (!v || c == CurrentCursor)
		return;
	
	CurrentCursor = c;
	GdkCursorType type = GDK_ARROW;
	switch (c)
	{
		// No cursor
		#ifdef GDK_BLANK_CURSOR
		case LCUR_Blank:
			type = GDK_BLANK_CURSOR;
			break;
		#endif
		/// Normal arrow
		case LCUR_Normal:
			type = GDK_ARROW;
			break;
		/// Upwards arrow
		case LCUR_UpArrow:
			type = GDK_SB_UP_ARROW;
			break;
		/// Downwards arrow
		case LCUR_DownArrow:
			type = GDK_SB_DOWN_ARROW;
			break;
		/// Left arrow
		case LCUR_LeftArrow:
			type = GDK_SB_LEFT_ARROW;
			break;
		/// Right arrow
		case LCUR_RightArrow:
			type = GDK_SB_RIGHT_ARROW;
			break;
		/// Crosshair
		case LCUR_Cross:
			type = GDK_CROSSHAIR;
			break;
		/// Hourglass/watch
		case LCUR_Wait:
			type = GDK_WATCH;
			break;
		/// Ibeam/text entry
		case LCUR_Ibeam:
			type = GDK_XTERM;
			break;
		/// Vertical resize (|)
		case LCUR_SizeVer:
			type = GDK_DOUBLE_ARROW;
			break;
		/// Horizontal resize (-)
		case LCUR_SizeHor:
			type = GDK_SB_H_DOUBLE_ARROW;
			break;
		/// Diagonal resize (/)
		case LCUR_SizeBDiag:
			type = GDK_BOTTOM_LEFT_CORNER;
			break;
		/// Diagonal resize (\)
		case LCUR_SizeFDiag:
			type = GDK_BOTTOM_RIGHT_CORNER;
			break;
		/// All directions resize
		case LCUR_SizeAll:
			type = GDK_FLEUR;
			break;
		/// A pointing hand
		case LCUR_PointingHand:
			type = GDK_HAND2;
			break;
		/// A slashed circle
		case LCUR_Forbidden:
			type = GDK_X_CURSOR;
			break;

		default:
			type = GDK_ARROW;
			break;

		/*			
		case LCUR_SplitV:
		case LCUR_SplitH:
		case LCUR_DropCopy:
		case LCUR_DropMove:
			LAssert(0);
			break;
		*/
	}
	
	OsView h = NULL;
	auto *w = v->GetWindow();
	if (w)
		h = GTK_WIDGET(w->WindowHandle());
	
	LAssert(v->InThread());
	auto wnd = gtk_widget_get_window(h);
	// LAssert(wnd);
	if (wnd)
	{
		if (type == GDK_ARROW)
			gdk_window_set_cursor(wnd, NULL);
		else
		{
			GdkCursor *cursor = gdk_cursor_new_for_display(gdk_display_get_default(), type);
			if (cursor)
				gdk_window_set_cursor(wnd, cursor);
			else
				gdk_window_set_cursor(wnd, NULL);
		}
	}
}

bool LView::_Mouse(LMouse &m, bool Move)
{
	ThreadCheck();
	
	#if DEBUG_MOUSE_EVENTS
	if (!Move)
		LgiTrace("%s:%i - _Mouse([%i,%i], %i) View=%p/%s\n", _FL, m.x, m.y, Move, _Over, _Over ? _Over->GetClass() : NULL);
	#endif

	if (GetWindow() &&
		!GetWindow()->HandleViewMouse(this, m))
	{
		#if DEBUG_MOUSE_EVENTS
		LgiTrace("%s:%i - HandleViewMouse consumed event, cls=%s\n", _FL, GetClass());
		#endif
		return false;
	}

	#if DEBUG_MOUSE_EVENTS
	// if (!Move)
		LgiTrace("%s:%i - _Capturing=%p/%s\n", _FL, _Capturing, _Capturing ? _Capturing->GetClass() : NULL);
	#endif
	if (Move)
	{
		auto *o = m.Target;
		if (_Over != o)
		{
			#if DEBUG_MOUSE_EVENTS
			// if (!o) WindowFromPoint(m.x, m.y, true);
			LgiTrace("%s:%i - _Over changing from %p/%s to %p/%s\n", _FL,
					_Over, _Over ? _Over->GetClass() : NULL,
					o, o ? o->GetClass() : NULL);
			#endif
			if (_Over)
				_Over->OnMouseExit(lgi_adjust_click(m, _Over));
			_Over = o;
			if (_Over)
				_Over->OnMouseEnter(lgi_adjust_click(m, _Over));
		}
	}
		
	LView *Target = NULL;
	if (_Capturing)
		Target = dynamic_cast<LView*>(_Capturing);
	else
		Target = dynamic_cast<LView*>(_Over ? _Over : this);
	if (!Target)
		return false;

	LRect Client = Target->LView::GetClient(false);
	
	m = lgi_adjust_click(m, Target, !Move);
	if (!Client.Valid() || Client.Overlap(m.x, m.y) || _Capturing)
	{
		LgiToGtkCursor(Target, Target->GetCursor(m.x, m.y));

		if (Move)
		{
			Target->OnMouseMove(m);
		}
		else
		{
			#if 0
			if (!Move)
			{
				char Msg[256];
				sprintf(Msg, "_Mouse Target %s", Target->GetClass());
				m.Trace(Msg);
			}
			#endif
			Target->OnMouseClick(m);
		}
	}
	else if (!Move)
	{
		#if DEBUG_MOUSE_EVENTS
		LgiTrace("%s:%i - Click outside %s %s %i,%i\n", _FL, Target->GetClass(), Client.GetStr(), m.x, m.y);
		#endif
	}
	
	return true;
}

const char *EventTypeToString(int i)
{
	switch (i)
	{
		case GDK_DELETE: return "GDK_DELETE";
		case GDK_DESTROY: return "GDK_DESTROY";
		case GDK_EXPOSE: return "GDK_EXPOSE";
		case GDK_MOTION_NOTIFY: return "GDK_MOTION_NOTIFY";
		case GDK_BUTTON_PRESS: return "GDK_BUTTON_PRESS";
		case GDK_2BUTTON_PRESS: return "GDK_2BUTTON_PRESS";
		case GDK_3BUTTON_PRESS: return "GDK_3BUTTON_PRESS";
		case GDK_BUTTON_RELEASE: return "GDK_BUTTON_RELEASE";
		case GDK_KEY_PRESS: return "GDK_KEY_PRESS";
		case GDK_KEY_RELEASE: return "GDK_KEY_RELEASE";
		case GDK_ENTER_NOTIFY: return "GDK_ENTER_NOTIFY";
		case GDK_LEAVE_NOTIFY: return "GDK_LEAVE_NOTIFY";
		case GDK_FOCUS_CHANGE: return "GDK_FOCUS_CHANGE";
		case GDK_CONFIGURE: return "GDK_CONFIGURE";
		case GDK_MAP: return "GDK_MAP";
		case GDK_UNMAP: return "GDK_UNMAP";
		case GDK_PROPERTY_NOTIFY: return "GDK_PROPERTY_NOTIFY";
		case GDK_SELECTION_CLEAR: return "GDK_SELECTION_CLEAR";
		case GDK_SELECTION_REQUEST: return "GDK_SELECTION_REQUEST";
		case GDK_SELECTION_NOTIFY: return "GDK_SELECTION_NOTIFY";
		case GDK_PROXIMITY_IN: return "GDK_PROXIMITY_IN";
		case GDK_PROXIMITY_OUT: return "GDK_PROXIMITY_OUT";
		case GDK_DRAG_ENTER: return "GDK_DRAG_ENTER";
		case GDK_DRAG_LEAVE: return "GDK_DRAG_LEAVE";
		case GDK_DRAG_MOTION: return "GDK_DRAG_MOTION";
		case GDK_DRAG_STATUS: return "GDK_DRAG_STATUS";
		case GDK_DROP_START: return "GDK_DROP_START";
		case GDK_DROP_FINISHED: return "GDK_DROP_FINISHED";
		case GDK_CLIENT_EVENT: return "GDK_CLIENT_EVENT";
		case GDK_VISIBILITY_NOTIFY: return "GDK_VISIBILITY_NOTIFY";
		case GDK_SCROLL: return "GDK_SCROLL";
		case GDK_WINDOW_STATE: return "GDK_WINDOW_STATE";
		case GDK_SETTING: return "GDK_SETTING";
		case GDK_OWNER_CHANGE: return "GDK_OWNER_CHANGE";
		case GDK_GRAB_BROKEN: return "GDK_GRAB_BROKEN";
		case GDK_DAMAGE: return "GDK_DAMAGE";
		case GDK_TOUCH_BEGIN: return "GDK_TOUCH_BEGIN";
		case GDK_TOUCH_UPDATE: return "GDK_TOUCH_UPDATE";
		case GDK_TOUCH_END: return "GDK_TOUCH_END";
		case GDK_TOUCH_CANCEL: return "GDK_TOUCH_CANCEL";
		case GDK_TOUCHPAD_SWIPE: return "GDK_TOUCHPAD_SWIPE";
		case GDK_TOUCHPAD_PINCH: return "GDK_TOUCHPAD_PINCH";
		#ifdef GDK_PAD_BUTTON_PRESS
		case GDK_PAD_BUTTON_PRESS: return "GDK_PAD_BUTTON_PRESS";
		#endif
		#ifdef GDK_PAD_BUTTON_RELEASE
		case GDK_PAD_BUTTON_RELEASE: return "GDK_PAD_BUTTON_RELEASE";
		#endif
		#ifdef GDK_PAD_RING
		case GDK_PAD_RING: return "GDK_PAD_RING";
		#endif
		#ifdef GDK_PAD_STRIP
		case GDK_PAD_STRIP: return "GDK_PAD_STRIP";
		#endif
		#ifdef GDK_PAD_GROUP_MODE
		case GDK_PAD_GROUP_MODE: return "GDK_PAD_GROUP_MODE";
		#endif
	}
	return "#error";
}

gboolean GtkViewCallback(GtkWidget *widget, GdkEvent *event, LView *This)
{
	#if 0
	LgiTrace("GtkViewCallback, Event=%s, This=%p(%s\"%s\")\n",
		EventTypeToString(event->type),
		This, (NativeInt)This > 0x1000 ? This->GetClass() : 0, (NativeInt)This > 0x1000 ? This->Name() : 0);
	#endif
	
	if (event->type < 0 || (int)event->type > 1000)
	{
		printf("%s:%i - CORRUPT EVENT %i\n", _FL, event->type);
		return false;
	}

	return This->OnGtkEvent(widget, event);
}

gboolean LView::OnGtkEvent(GtkWidget *widget, GdkEvent *event)
{
	printf("LView::OnGtkEvent ?????\n");
	return false;
}

LRect &LView::GetClient(bool ClientSpace)
{
	int Edge = (Sunken() || Raised()) ? _BorderSize : 0;

	static LRect c;
	if (ClientSpace)
	{
		c.ZOff(Pos.X() - 1 - (Edge<<1), Pos.Y() - 1 - (Edge<<1));
	}
	else
	{
		c.ZOff(Pos.X()-1, Pos.Y()-1);
		c.Inset(Edge, Edge);
	}
	
	return c;
}

void LView::Quit(bool DontDelete)
{
	ThreadCheck();
	
	if (DontDelete)
	{
		Visible(false);
	}
	else
	{
		delete this;
	}
}

bool LView::SetPos(LRect &p, bool Repaint)
{
	if (Pos != p)
	{
		Pos = p;
		OnPosChange();
	}

	return true;
}

LRect GtkGetPos(GtkWidget *w)
{
	GtkAllocation a = {0};
	gtk_widget_get_allocation(w, &a);
	return a;
}

bool LView::Invalidate(LRect *rc, bool Repaint, bool Frame)
{
	#if 0 // Debug where lots of invalidate calls are from
		static uint64_t StartTs = 0;
		static int Count = 0;
		static LHashTbl<ConstStrKey<char,false>,int> Classes;
		
		Count++;
		int count = Classes.Find(GetClass());
		Classes.Add(GetClass(), count + 1);
		
		auto Now = LCurrentTime();
		if (Now - StartTs >= 1000)
		{
			printf("Inval=%i:", Count);
			for (auto p: Classes)
				printf(" %s=%i", p.key, p.value);
			printf("\n");
				
			StartTs = Now;
			Count = 0;
			Classes.Empty();
			
			if (!Stricmp(GetClass(), "LMdiChild"))
			{
				int asd=0;
			}
		}
	#endif

	auto *ParWnd = GetWindow();
	if (!ParWnd)
		return false; // Nothing we can do till we attach
	if (!InThread())
	{
		DEBUG_INVALIDATE("%s::Invalidate out of thread\n", GetClass());
		return PostEvent(M_INVALIDATE, (LMessage::Param)NULL, (LMessage::Param)this);
	}

	LRect r;
	if (rc)
	{
		r = *rc;
	}
	else
	{
		if (Frame)
			r = Pos.ZeroTranslate();
		else
			r = GetClient().ZeroTranslate();
	}

	DEBUG_INVALIDATE("%s::Invalidate r=%s frame=%i\n", GetClass(), r.GetStr(), Frame);
	if (!Frame)
		r.Offset(_BorderSize, _BorderSize);

	LPoint Offset;
	WindowVirtualOffset(&Offset);
	r.Offset(Offset.x, Offset.y);
	DEBUG_INVALIDATE("	voffset=%i,%i = %s\n", Offset.x, Offset.y, r.GetStr());
	if (!r.Valid())
	{
		DEBUG_INVALIDATE("	error: invalid\n");
		return false;
	}

	static bool Repainting = false;
	if (!Repainting)
	{
		Repainting = true;

		GtkWidget *w = GTK_WIDGET(ParWnd->WindowHandle());
		if (w)
		{
			GdkWindow *h;
			if (gtk_widget_get_has_window(w) &&
				(h = gtk_widget_get_window(w)))
			{
				GdkRectangle grc = r;
				DEBUG_INVALIDATE("	gdk_window_invalidate_rect %i,%i %i,%i\n",
					grc.x, grc.y, grc.width, grc.height);
				gdk_window_invalidate_rect(h, &grc, true);
			}
			else
			{
				DEBUG_INVALIDATE("	gtk_widget_queue_draw\n");
				gtk_widget_queue_draw(w);
			}
		}

		Repainting = false;
	}
	else
	{
		DEBUG_INVALIDATE("	error: repainting\n");
	}

	return true;
}

void LView::SetPulse(int Length)
{
	ThreadCheck();

	DeleteObj(d->PulseThread);
	if (Length > 0)
		d->PulseThread = new LPulseThread(this, Length);
}

LMessage::Param LView::OnEvent(LMessage *Msg)
{
	ThreadCheck();

	for (auto target: d->EventTargets)
	{
		if (target->Msgs.Length() == 0 ||
			target->Msgs.Find(Msg->Msg()))
		{
			if (target->OnEvent(Msg) == ViewEventTarget::OBJ_DELETED)
				return 0;
		}
	}
	
	int Id;
	switch (Id = Msg->Msg())
	{
		case M_INVALIDATE:
		{
			if ((LView*)this == (LView*)Msg->B())
			{
				LAutoPtr<LRect> r((LRect*)Msg->A());
				Invalidate(r);
			}
			break;
		}
		case M_CAPTURE_PULSE:
		{
			auto wnd = GetWindow();
			if (!wnd)
			{
				printf("%s:%i - No window.\n", _FL);
				break;
			}
			
			LMouse m;
			if (!wnd->GetMouse(m, true))
			{
				printf("%s:%i - GetMouse failed.\n", _FL);
				break;
			}
			
			auto c = wnd->GetPos();
			if (c.Overlap(m))
				break; // Over the window, so we'll get normal events...
			if (!_Capturing)
				break; // Don't care now, there is no window capturing...

			if (d->PrevMouse.x < 0)
			{
				d->PrevMouse = m; // Initialize the previous mouse var.
				break;
			}

			int mask = LGI_EF_LEFT | LGI_EF_MIDDLE | LGI_EF_RIGHT;
			bool coordsChanged = m.x != d->PrevMouse.x || m.y != d->PrevMouse.y;
			bool buttonsChanged = (m.Flags & mask) != (d->PrevMouse.Flags & mask);
			if (!coordsChanged && !buttonsChanged)
				break; // Nothing changed since last poll..

			int prevFlags = d->PrevMouse.Flags;
			d->PrevMouse = m;
			
			// Convert coords to local view...
			m.Target = _Capturing;
			#if 0
			printf("%s:%i - Mouse(%i,%i) outside window(%s, %s).. %s\n",
				_FL, m.x, m.y, c.GetStr(), wnd->Name(),
				m.ToString().Get());
			#endif
			m.ToView();

			// Process events...
			if (coordsChanged)
			{
				// printf("Move event: %s\n", m.ToString().Get());
				_Capturing->OnMouseMove(m);
			}
			/* This seems to happen anyway? Ok then... whatevs
			if (buttonsChanged)
			{
				m.Flags &= ~mask; // clear any existing
				m.Flags |= prevFlags & mask;
				m.Down(false);
				printf("Click event: %s, mask=%x flags=%x\n", m.ToString().Get(), mask, m.Flags);
				_Capturing->OnMouseClick(m);
			}
			*/
			break;
		}
		case M_CHANGE:
		{
			LViewI *Ctrl;
			if (GetViewById(Msg->A(), Ctrl))
				return OnNotify(Ctrl, (LNotifyType)Msg->B());
			break;
		}
		case M_COMMAND:
		{
			return OnCommand(Msg->A(), 0, (OsView) Msg->B());
		}
		case M_DND_END:
		{
			if (auto target = DropTarget())
				target->OnDragExit();
			break;
		}
	}

	LMessage::Result result;
	if (CommonEvents(result, Msg))
		return result;

	return 0;
}

LPoint GtkGetOrigin(LWindow *w)
{
	auto Hnd = w->WindowHandle();
	if (Hnd)
	{
		auto Wnd = gtk_widget_get_window(GTK_WIDGET(Hnd));
		if (Wnd)
		{
			GdkRectangle rect;
			gdk_window_get_frame_extents(Wnd, &rect);
			return LPoint(rect.x, rect.y);
			
			/*
			gint x = 0, y = 0;
			gdk_window_get_origin(Wnd, &x, &y);
			gdk_window_get_root_origin(Wnd, &x, &y);
			return LPoint(x, y);
			*/
		}
		else
		{
			LgiTrace("%s:%i - can't get Wnd for %s\n", _FL, G_OBJECT_TYPE_NAME(Hnd));
		}
	}
	
	return LPoint();
}

bool LView::PointToScreen(LPoint &p)
{
	ThreadCheck();

	LPoint Offset;
	WindowVirtualOffset(&Offset);
	p += Offset;

	auto w = GetWindow();
	if (!w)
		return false;
	auto wnd = w->WindowHandle();
	if (!wnd)
		return false;

	auto wid = GTK_WIDGET(wnd);
	auto hnd = gtk_widget_get_window(wid);

	gint x = 0, y = 0;
	gdk_window_get_origin(hnd, &x, &y);

	p.x += x;
	p.y += y;
	return true;
}

bool LView::PointToView(LPoint &p)
{
	ThreadCheck();

	LPoint Offset;
	WindowVirtualOffset(&Offset);
	p -= Offset;

	auto w = GetWindow();
	if (!w)
		return false;
	auto wnd = w->WindowHandle();
	if (!wnd)
		return false;

	auto wid = GTK_WIDGET(wnd);
	auto hnd = gtk_widget_get_window(wid);

	gint x = 0, y = 0;
	gdk_window_get_origin(hnd, &x, &y);

	p.x -= x;
	p.y -= y;
	return true;
}

bool LView::GetMouse(LMouse &m, bool ScreenCoords)
{
	bool Status = true;
	ThreadCheck();

	auto *w = GetWindow();
	GdkModifierType mask = (GdkModifierType)0;
	auto display = gdk_display_get_default();
	auto seat = gdk_display_get_default_seat(display);
	auto device = gdk_seat_get_pointer(seat);
	// auto deviceManager = gdk_display_get_device_manager(display);
	// auto device = gdk_device_manager_get_client_pointer(deviceManager);
	
	gdouble axes[2] = {0};
	
	if (gdk_device_get_axis_use(device, 0) != GDK_AXIS_X)
		gdk_device_set_axis_use(device, 0, GDK_AXIS_X);
	if (gdk_device_get_axis_use(device, 1) != GDK_AXIS_Y)
		gdk_device_set_axis_use(device, 1, GDK_AXIS_Y);

	if (ScreenCoords || !w)
	{
		gdk_device_get_state(device, gdk_get_default_root_window(), axes, &mask);
		m.x = (int)axes[0];
		m.y = (int)axes[1];
	}
	else if (auto widget = GTK_WIDGET(w->WindowHandle()))
	{
		auto wnd = gtk_widget_get_window(widget);
		gdk_device_get_state(device, wnd, axes, &mask);
		
		if (axes[0] == 0.0 || axes[0] > 10000)
		{
			// LgiTrace("%s:%i - gdk_device_get_state failed.\n", _FL);
			Status = false;
		}
		else
		{
			LPoint p;
			WindowVirtualOffset(&p);
			m.x = (int)axes[0] - p.x - _BorderSize;
			m.y = (int)axes[1] - p.y - _BorderSize;
			// printf("GetMs %g,%g %i,%i %i,%i device=%p wnd=%p\n", axes[0], axes[1], p.x, p.y, m.x, m.y, device, wnd);
		}
	}

	m.Target = this;
	m.SetModifer(mask);
	m.Left((mask & GDK_BUTTON1_MASK) != 0);
	m.Middle((mask & GDK_BUTTON2_MASK) != 0);
	m.Right((mask & GDK_BUTTON3_MASK) != 0);
	m.Down(m.Left() || m.Middle() || m.Right());
	m.ViewCoords = !ScreenCoords;
	
	return Status;
}

bool LView::IsAttached()
{
	auto w = GetWindow();
	if (!w)
		return false;

	w = dynamic_cast<LWindow*>(this);
	if (!w)
	{
		auto p = GetParent();
		return d->GotOnCreate && p && p->HasView(this);
	}

	return w->IsAttached();
}

const char *LView::GetClass()
{
	return "LView";
}

bool LView::Attach(LViewI *parent)
{
	ThreadCheck();
	
	bool Status = false;

	auto Parent = d->GetParent();
	LAssert(Parent == NULL || Parent == parent);

	SetParent(parent);
	Parent = d->GetParent();
	
	_Window = Parent ? Parent->_Window : this;

	if (parent)
	{
		auto w = GetWindow();
		if (w && TestFlag(WndFlags, GWF_FOCUS))
			w->SetFocus(this, LWindow::GainFocus);

		Status = true;

		if (!Parent->HasView(this))
		{
			OnAttach();
			if (!d->Parent->HasView(this))
				d->Parent->AddView(this);
			d->Parent->OnChildrenChanged(this, true);
		}

		if (_Window)
			OnGtkRealize();
	}
	
	return Status;
}

bool LView::Detach()
{
	ThreadCheck();
	
	// Detach view
	if (_Window)
	{
		auto *Wnd = dynamic_cast<LWindow*>(_Window);
		if (Wnd)
			Wnd->SetFocus(this, LWindow::ViewDelete);
		_Window = NULL;
	}

	LViewI *Par = GetParent();
	if (Par)
	{
		// Events
		Par->DelView(this);
		Par->OnChildrenChanged(this, false);
		Par->Invalidate(&Pos);
	}
	
	d->Parent = 0;
	d->ParentI = 0;

	#if 0 // Windows is not doing a deep detach... so we shouldn't either?
	{
		int Count = Children.Length();
		if (Count)
		{
			int Detached = 0;
			LViewI *c, *prev = NULL;

			while ((c = Children[0]))
			{
				LAssert(!prev || c != prev);
				if (c->GetParent())
					c->Detach();
				else
					Children.Delete(c);
				Detached++;
				prev = c;
			}
			LAssert(Count == Detached);
		}
	}
	#endif

	return true;
}

LCursor LView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

void LView::OnGtkDelete()
{
	List<LViewI>::I it = Children.begin();
	for (LViewI *c = *it; c; c = *++it)
	{
		LView *v = c->GetLView();
		if (v)
			v->OnGtkDelete();
	}
}

///////////////////////////////////////////////////////////////////
bool LgiIsMounted(char *Name)
{
	return false;
}

bool LgiMountVolume(char *Name)
{
	return false;
}

////////////////////////////////
uint32_t CursorData[] = {
0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x01010101, 0x02020202, 0x02020202, 0x02010101, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x01020202, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x02020202, 0x02020202, 0x02020101, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x01000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x01020202, 0x02020201, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x00010102, 0x00000000, 0x02020101, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010001, 0x00010001, 0x01000001, 0x02020202, 0x02020202, 0x00010102, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 
0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01000102, 0x00000100, 0x02010000, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020100, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 
0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00010101, 0x01000000, 0x02020202, 0x00000001, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00000102, 0x01010100, 0x01010101, 0x01000000, 0x02020202, 0x02020201, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 
0x00000001, 0x02010000, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x01010102, 0x01010001, 0x02020101, 0x02020202, 0x02020202, 0x01020201, 0x02010000, 0x02020102, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x01000001, 0x02020101, 0x02020202, 0x02020202, 0x00010202, 0x01010000, 0x01020202, 0x00000001, 0x02020201, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01000001, 0x01010101, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 
0x00000000, 0x00000000, 0x02020201, 0x02020101, 0x00000102, 0x00000100, 0x02020201, 0x02020202, 0x01000001, 0x01000000, 0x01020202, 0x02020201, 0x02020202, 0x02020202, 0x02020201, 0x02010001, 0x02010202, 0x02020202, 0x01020202, 0x01020201, 0x02010000, 0x02010102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x01010000, 0x02020101, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x00000102, 0x02020100, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x02010001, 0x00000001, 0x00010201, 0x02020201, 0x02020202, 0x02010001, 0x00000001, 0x00010201, 0x02020201, 0x02020202, 0x01020202, 0x02020201, 0x02010001, 0x01010202, 0x02020202, 0x00010202, 0x01020201, 0x02010000, 0x01000102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02010102, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x00000102, 0x00000001, 0x02020201, 0x00010202, 0x02020100, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 
0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01010100, 0x01010101, 0x01000000, 0x02020202, 0x01000001, 0x01000000, 0x01020202, 0x02020201, 0x02020202, 0x02020101, 0x00000102, 0x00000100, 0x02020201, 0x02020202, 0x00010202, 0x02020201, 0x02010001, 0x00010202, 0x02020201, 0x00000102, 0x01010101, 0x01010000, 0x00000101, 0x02020201, 0x01010101, 0x01010101, 0x01010100, 0x01010101, 0x02020201, 0x01000001, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x00000001, 0x00000101, 0x02020100, 0x00010202, 0x02010000, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 
0x02020202, 0x02020202, 0x01010102, 0x01010101, 0x01010001, 0x01010101, 0x02020101, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020100, 0x01020202, 0x02010000, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020201, 0x02020202, 0x02020201, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x01010101, 0x01010001, 0x00010101, 0x02020100, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020100, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x00000001, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x00010202, 0x02010000, 0x01020202, 0x02010000, 0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 
0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x01020202, 0x02020100, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02010000, 0x00000102, 0x01010101, 0x01010000, 0x00000101, 0x02020201, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x00000102, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x01020202, 
0x01000000, 0x01020202, 0x02010000, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x01010102, 0x01010101, 0x01010001, 0x01010101, 0x02020101, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x01020202, 0x02020201, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x01010101, 0x01010001, 0x00010101, 0x02020100, 0x00010202, 0x01020201, 0x02010000, 0x01000102, 0x02020202, 0x01010101, 0x01010101, 0x01010100, 0x01010101, 
0x02020201, 0x00010202, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x00000001, 0x01020201, 0x02010000, 0x00000001, 0x01000000, 0x02010101, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02010101, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x01000001, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01000001, 0x01010101, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x01020202, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00010202, 0x02020201, 0x02010001, 0x00010202, 0x02020201, 0x01020202, 
0x01020201, 0x02010000, 0x02010102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x00000001, 0x02020201, 0x00000102, 0x00010100, 0x02010000, 0x00000001, 0x01000001, 0x01020202, 0x01010101, 0x01010101, 0x00000001, 0x01000001, 0x01020202, 0x01010101, 0x01010101, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01000102, 0x01000001, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 
0x02020201, 0x02020202, 0x01020202, 0x02020201, 0x02010001, 0x01010202, 0x02020202, 0x02020202, 0x01020201, 0x02010000, 0x02020102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x00000102, 0x02020201, 0x00010202, 0x00010000, 0x02020100, 0x01000001, 0x01000001, 0x01020202, 0x00000000, 0x01000000, 0x01000001, 0x01000001, 0x01020202, 0x00000000, 0x01000000, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010001, 0x00000000, 0x01000100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02010001, 0x02010202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x02020100, 0x01020202, 0x00000000, 0x02020100, 0x02010001, 0x00000102, 0x01020201, 0x01010000, 0x01000001, 0x02010001, 0x00000102, 0x01020201, 0x01010100, 0x01000001, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 
0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01010102, 0x01010001, 0x02020101, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01010202, 0x01010101, 0x02020202, 0x02020202, 0x00010202, 0x01010000, 0x01020202, 0x00000001, 0x02020201, 0x02020101, 0x00000102, 0x01020201, 0x00000100, 0x01000100, 0x02020101, 0x00000102, 0x01020201, 0x01000100, 0x01000100, 0x01020202, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x01010101, 0x02020202, 
0x02020202, 0x00010102, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00010101, 0x01000000, 0x02020202, 0x02020202, 0x00010202, 0x01020100, 0x00000100, 0x01000000, 0x02020202, 0x00010202, 0x01020100, 0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00010202, 0x01020100, 0x00000100, 0x01000100, 0x02020202, 0x00010202, 0x01020100, 
0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010101, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010102, 0x00000000, 0x02020101, 0x02020202, 
0x02020202, 0x01020202, 0x01020201, 0x01010000, 0x01000001, 0x02020202, 0x01020202, 0x01020201, 0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x01010101, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x01010101, 
};

