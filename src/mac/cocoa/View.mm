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
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Css.h"
#include "lgi/common/Popup.h"
#include "lgi/common/DisplayString.h"
#include "ViewPriv.h"
#include "LCocoaView.h"

#define ADJ_LEFT					1
#define ADJ_RIGHT					2
#define ADJ_UP						3
#define ADJ_DOWN					4

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
extern uint32 CursorData[];
LInlineBmp Cursors =
{
	300, 20, 8, CursorData
};

struct LgiCursorInfo
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

////////////////////////////////////////////////////////////////////////////
LView *GWindowFromHandle(OsView h)
{
	if (!h)
		return 0;
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////
bool LIsKeyDown(int Key)
{
	LAssert(!"Not impl.");
	return false;
}

bool LIsMounted(char *Name)
{
	LAssert(!"Not impl.");
	return false;
}

bool LMountVolume(char *Name)
{
	LAssert(!"Not impl.");
	return false;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
#if 0
bool LViewPrivate::CursorSet = false;
LView *LViewPrivate::LastCursor = 0;
#endif

LViewPrivate::LViewPrivate(LView *view) : View(view)
{
	TabStop = false;
	AttachEvent = false;
}
	
LViewPrivate::~LViewPrivate()
{
	DeleteObj(Popup);
	LAssert(PulseThread == NULL);

	while (EventTargets.Length())
		delete EventTargets[0];
}

const char *LView::GetClass()
{
	return "LView";
}

LView *&LView::PopupChild()
{
	return d->Popup;
}

bool LView::_Mouse(LMouse &m, bool Move)
{
	#if 0
	if (Move)
	{
		static bool First = true;
		if (First)
		{
			First = false;
			//_DumpHeirarchy(GetWindow());			
			//_Dump(0, GetWindow(), HIViewGetRoot(GetWindow()->WindowHandle()));
		}
		
		printf("_Mouse %p,%s %i,%i down=%i move=%i\n",
			this, GetClass(),
			m.x, m.y,
			m.Down(), Move);
	}
	#endif

	LWindow *Wnd = GetWindow();

	if (_Capturing)
	{
		// Convert root NSView coords to capture view coords
		LViewI *par;
		for (auto i = _Capturing; i && i != this; i = par)
		{
			par = i->GetParent();
			LRect cli, p = i->GetPos();
			if (par)
				cli = i->GetClient(false);

			m.x -= p.x1 + cli.x1;
			m.y -= p.y1 + cli.y1;
		}
		
		// if (!m.IsMove()) m.Trace("Capture");
		
		if (Move)
		{
			LViewI *c = _Capturing;
			c->OnMouseMove(m);
		}
		else
		{
			if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<LView*>(_Capturing), m))
				_Capturing->OnMouseClick(m);
		}
	}
	else
	{
		// Convert root NSView coords to _Over coords
		auto t = WindowFromPoint(m.x, m.y);
		if (t)
		{
			/*
			GColour col(255, 0, 255);
			GArray<LRect> rc;
			GArray<GString> names;
			auto w = GetWindow();
			int yy = 200;
			bool Debug = !stricmp(t->GetClass(), "CtrlEditbox") && w->DebugDC.X() == 0 && !Move;
			if (Debug && w)
			{
				w->DebugDC.Create(w->X(), w->Y(), System32BitColourSpace);
				w->DebugDC.Colour(0, 32);
				w->DebugDC.Rectangle();
				w->DebugDC.Colour(col);
			}
			*/
			
			for (auto i = t; i && i != this; i = i->GetParent())
			{
				auto p = i->GetPos();
				auto cli = i->GetClient(false);
				
				/*
				if (Debug)
				{
					LRect r = p;
					r.Offset(cli.x1, cli.y1);
					rc.AddAt(0,r);
					names.AddAt(0,i->GetClass());
					printf("%s %s\n", i->GetClass(), p.GetStr());
				}
				*/

				m.x -= p.x1 + cli.x1;
				m.y -= p.y1 + cli.y1;
			}
			
			/*
			if (Debug)
			{
				int x = rc[0].x1, y = rc[0].y1;
				for (unsigned i=0; i<rc.Length(); i++)
				{
					auto &r = rc[i];
					w->DebugDC.Line(x, y, x+r.x1, y+r.y1);
					x += r.x1;
					y += r.y1;
					
					GDisplayString ds(SysFont, names[i]);
					SysFont->Fore(col);
					SysFont->Transparent(true);
					ds.Draw(&w->DebugDC, 120-ds.X(), yy);
					w->DebugDC.Line(120, yy, x, y);
					yy += 10 + ds.Y();
				}
				w->Invalidate();
			}
			*/
			
			m.Target = t;
		}
		
		// if (!m.IsMove()) m.Trace("NonCapture");

		if (_Over != m.Target)
		{
			if (_Over)
				_Over->OnMouseExit(m);
			_Over = m.Target;
			if (_Over)
				_Over->OnMouseEnter(m);
		}
			
		auto Target = dynamic_cast<LView*>(_Over ? _Over : m.Target);
		auto Lo = dynamic_cast<LLayout*>(Target);
		LRect Client = Lo ? Lo->GetClient(false) : Target->LView::GetClient(false);

		if (!Client.Valid() || Client.Overlap(m.x, m.y))
		{
			if (Move)
			{
				// Do cursor stuff
				auto cursor = Target->GetCursor(m.x, m.y);
				auto cc = NSCursor.arrowCursor;
				if (cursor != LCUR_Normal)
					cc = LCocoaCursor(cursor);
				if (cc != NSCursor.currentSystemCursor)
					[cc set];

				// Move event
				Target->OnMouseMove(m);
			}
			else if (!Wnd || Wnd->HandleViewMouse(Target, m))
			{
				// Click event
				Target->OnMouseClick(m);
			}
		}
		else return false;
	}
	
	return true;
}

LRect &LView::GetClient(bool ClientSpace)
{
	int Edge = (Sunken() || Raised()) ? _BorderSize : 0;

	static LRect c;
	c = Pos;
	c.Offset(-c.x1, -c.y1);
	if (ClientSpace)
	{
		c.x2 -= Edge << 1;
		c.y2 -= Edge << 1;
	}
	else
	{
		c.Inset(Edge, Edge);
	}
	
	return c;
}

void LView::Quit(bool DontDelete)
{
	if (DontDelete)
	{
		Visible(false);
	}
	else
	{
		Detach();
		delete this;
	}
}

LPoint LView::Flip(LPoint p)
{
	auto Parent = GetParent() ? GetParent() : GetWindow();
	if (Parent)
	{
		LRect r = Parent->GetClient(false);
		p.y = r.y2 - p.y;
	}
	return p;
}

void LView::OnDealloc()
{
	#if LGI_VIEW_HASH
	LockHandler(this, OpDelete);
	#endif
	SetPulse();
	
	for (auto c: Children)
	{
		auto gv = c->GetGView();
		if (gv) gv->OnDealloc();
	}
}

LRect LView::Flip(LRect p)
{
	auto Parent = GetParent() ? GetParent() : GetWindow();
	if (Parent)
	{
		LRect r = Parent->GetClient(false);
		int y2 = r.y2 - p.y1;
		int y1 = y2 - p.Y() + 1;
		p.Offset(0, y1-p.y1);
	}
	return p;
}

bool LView::SetPos(LRect &p, bool Repaint)
{
	Pos = p;
	OnPosChange();

	return true;
}

bool LView::Invalidate(LRect *rc, bool Repaint, bool Frame)
{
	if (!InThread())
		return PostEvent(M_INVALIDATE, (LMessage::Param) (rc ? new LRect(*rc) : NULL), (LMessage::Param) this);

	LRect r;
	if (rc)
		r = *rc;
	else
		r = GetClient();
	if (!r.Valid())
		return false;
	
	auto w = GetWindow();
	LPopup *popup = NULL;
	LViewI *v;
	for (v = this; v; v = v->GetParent())
	{
		if (!v->Visible())
			return true;
		if (v == (LViewI*)w)
			break;
		if ((popup = dynamic_cast<LPopup*>(v)))
			break;

		auto p = v->GetPos();
		r.Offset(p.x1, p.y1);
	}

	NSView *nsview = NULL;
	if (popup)
	 	nsview = popup->Handle().p.contentView;
	else if (w)
		nsview = w->Handle();
	
	if (!nsview)
		return false;

	#if 0
	[nsview setNeedsDisplayInRect:v->GetGView()->Flip(r)];
	printf("%s::Inval r=%s\n", GetClass(), r.GetStr());
	#else
	nsview.needsDisplay = true;
	#endif
	return true;
}

void LView::SetPulse(int Length)
{
	DeleteObj(d->PulseThread);
	if (Length > 0)
		d->PulseThread = new LPulseThread(this, Length);
}

LCursor LView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

LMessage::Result LView::OnEvent(LMessage *Msg)
{
	for (auto target: d->EventTargets)
	{
		if (target->Msgs.Length() == 0 ||
			target->Msgs.Find(Msg->Msg()))
			target->OnEvent(Msg);
	}

	switch (Msg->m)
	{
		case M_PULSE:
		{
			OnPulse();
			break;
		}
		case M_CHANGE:
		{
			LViewI *Ctrl;
			if (GetViewById((int)Msg->A(), Ctrl))
			{
				LNotification note((LNotifyType)Msg->B());
				return OnNotify(Ctrl, note);
			}
			break;
		}
		case M_COMMAND:
		{
			static int count = 0;
			if (++count == 2)
				printf("%s:%i - M_COMMAND\n", _FL);
			return OnCommand((int)Msg->A(), 0, (OsView)Msg->B());
		}
		case M_INVALIDATE:
		{
			LAutoPtr<LRect> rc((LRect*)Msg->A());
			Invalidate(rc.Get());
			break;
		}
		case M_THREAD_COMPLETED:
		{
			auto Th = (LThread*)Msg->A();
			if (!Th)
				break;

			Th->OnComplete();
			if (Th->GetDeleteOnExit())
				delete Th;

			return true;
		}
		default:
		{
			break;
		}
	}

	return 0;
}

bool LView::PointToScreen(LPoint &p)
{
	LViewI *c = this;

	// Find offset to window
	while (	c &&
			!dynamic_cast<LWindow*>(c) &&
			!dynamic_cast<LPopup*>(c))
	{
		LRect pos = c->GetPos();
		// printf("%s pos %s\n", c->GetClass(), pos.GetStr());
		p.x += pos.x1;
		p.y += pos.y1;
		c = c->GetParent();
	}
	
	if (c && c->WindowHandle())
	{
		NSWindow *w = c->WindowHandle();

		LRect wFrame = w.frame;
		LRect Screen(0, 0, -1, -1);
		for (NSScreen *s in [NSScreen screens])
		{
			LRect pos = s.frame;
			if (wFrame.Overlap(&pos))
			{
				Screen = pos;
				break;
			}
		}
		
		if (!Screen.Valid())
			return false;

		LRect cli = c->GetClient();
		p.y = cli.Y() - p.y;
		NSRect i = {{(double)p.x, (double)p.y}, {0.0, 0.0}};
		NSRect o = [w convertRectToScreen:i];
		p.x = (int)o.origin.x;
		p.y = Screen.Y() - (int)o.origin.y;
	}
	else return false;
	
	return true;
}

bool LView::PointToView(LPoint &p)
{
	LViewI *c = this;
	int Ox = 0, Oy = 0;

	// Find offset to window
	while (c && c != c->GetWindow())
	{
		LView *gv = c->GetGView();
		LRect cli = gv ? gv->LView::GetClient(false) : c->GetClient(false);
		LRect pos = c->GetPos(); 
		Ox += pos.x1 + cli.x1;
		Oy += pos.y1 + cli.y1;
		c = c->GetParent();
	}
	
	// Apply window position
	if (c && c->WindowHandle())
	{
		NSWindow *w = c->WindowHandle();

		LRect wFrame = w.frame;
		LRect Screen(0, 0, -1, -1);
		for (NSScreen *s in [NSScreen screens])
		{
			LRect pos = s.frame;
			if (wFrame.Overlap(&pos))
			{
				Screen = pos;
				break;
			}
		}
		
		if (!Screen.Valid())
			return false;

		// Flip into top-down to bottom up:
		NSRect i = {(double)p.x, (double)(Screen.Y()-p.y), 0.0, 0.0};
		NSRect o = [w convertRectFromScreen:i];
		p.x = (int)o.origin.x;
		p.y = (int)o.origin.y;
		
		// Flip back into top down.. in window space
		#if 1
		p = c->GetGView()->Flip(p);
		#else
		LRect cli = c->GetClient();
		p.y = cli.Y() - p.y;
		#endif

		// Now offset into view space.
		p.x += Ox;
		p.y += Oy;
	}
	else return false;
	
	return true;
}

#define DEBUG_GETMOUSE 0

bool LView::GetMouse(LMouse &m, bool ScreenCoords)
{
	LViewI *w = GetWindow();
	if (!w)
		return false;

	NSWindow *wh = w->WindowHandle();
	if (!wh)
		return false;
	
	#if DEBUG_GETMOUSE
	GStringPipe log;
	#endif

	NSPoint pt = wh.mouseLocationOutsideOfEventStream;
	m.x = pt.x;
	m.y = pt.y;
	m.Target = this;
	
	#if DEBUG_GETMOUSE
	log.Print("Event=%i,%i", m.x, m.y);
	#endif

	LPoint p(pt.x, pt.y);
	p.y = wh.contentView.frame.size.height - p.y;
	#if DEBUG_GETMOUSE
	log.Print(" Flipped=%i,%i", p.x, p.y);
	#endif
	if (ScreenCoords)
	{
		PointToScreen(p);
		#if DEBUG_GETMOUSE
		log.Print(" Screen=%i,%i", p.x, p.y);
		#endif
	}

	m.x = p.x;
	m.y = p.y;
	m.Target = this;
	
	auto Btns = [NSEvent pressedMouseButtons];
	m.Left(Btns & 0x1);
	m.Right(Btns & 0x2);
	m.Middle(Btns & 0x4);
	
	auto Mods = [NSEvent modifierFlags];
	m.Ctrl  (Mods & NSEventModifierFlagControl);
	m.Alt   (Mods & NSEventModifierFlagOption);
	m.System(Mods & NSEventModifierFlagCommand);
	m.Shift (Mods & NSEventModifierFlagShift);

	// Find offset to window
	for (LViewI *c = this; c && c != c->GetWindow(); c = c->GetParent())
	{
		LRect pos = c->GetPos();
		m.x -= pos.x1;
		m.y -= pos.y1;
		#if DEBUG_GETMOUSE
		log.Print(" Offset(%s,%i,%i)=%i,%i", c->GetClass(), pos.x1, pos.y1, m.x, m.y);
		#endif
	}

	#if DEBUG_GETMOUSE
	printf("    GetMouse: %s\n", log.NewGStr().Get());
	#endif
	return true;
}

bool LView::IsAttached()
{
	LWindow *w = GetWindow();
	if (!w)
		return false;
	
	if (w == this)
		return WindowHandle() != 0;
	
	if (!d->AttachEvent)
		return false;
	
	if (GetParent() != NULL)
		return w->WindowHandle() != 0;

	return false;
}

void BuildTabStops(LArray<LViewI*> &Stops, LViewI *v)
{
	if (v && v->Visible())
	{
		if (v->GetTabStop() && v->Enabled())
			Stops.Add(v);

		for (LViewI *c: v->IterateViews())
			BuildTabStops(Stops, c);
	}
}

void NextTabStop(LViewI *v, int dir)
{
	LArray<LViewI*> Stops;
	BuildTabStops(Stops, v->GetWindow());
	ssize_t Idx = Stops.IndexOf(v);
	if (Idx >= 0)
		Idx += dir;
	else
		Idx = 0;
		
	if (Idx < 0) Idx = Stops.Length() - 1;
	else if (Idx >= Stops.Length()) Idx = 0;
	if (Idx < Stops.Length())
		Stops[Idx]->Focus(true);
}

void SetDefaultFocus(LViewI *v)
{
	LArray<LViewI*> Stops;
	BuildTabStops(Stops, v->GetWindow());
	
	if (Stops.Length())
	{
		int Set = -1;
		for (int i=0; i<Stops.Length(); i++)
		{
			if (Stops[i]->Focus())
			{
				Set = i;
				break;
			}
		}
		
		if (Set < 0)
			Set = 0;
	
		Stops[Set]->Focus(true);
	}
}

int VirtualKeyToLgi(int Virt)
{
	switch (Virt)
	{
		// various
		case 122: return LK_F1;
		case 120: return LK_F2;
		case 99: return LK_F3;
		case 118: return LK_F4;
		case 96: return LK_F5;
		case 97: return LK_F6;
		case 98: return LK_F7;
		case 100: return LK_F8;
		case 101: return LK_F9;
		case 109: return LK_F10;
		case 103: return LK_F11;
		case 110: return LK_APPS;
		case 111: return LK_F12;
		
		case 123: return LK_LEFT;
		case 124: return LK_RIGHT;
		case 125: return LK_DOWN;
		case 126: return LK_UP;
		case 114: return LK_INSERT;
		case 116: return LK_PAGEUP;
		case 121: return LK_PAGEDOWN;

		case 53: return LK_ESCAPE;
		case 51: return LK_BACKSPACE;
		case 117: return LK_DELETE;
		
		case 115: return LK_HOME;
		case 119: return LK_END;
		
		// whitespace
		case 76: return LK_RETURN;
		case 36: return '\r';
		case 48: return '\t';
		case 49: return ' ';
		
		// delimiters
		case 27: return '-';
		case 24: return '=';
		case 42: return '\\';
		case 47: return '.';
		case 50: return '`';
		
		// digits
		case 18: return '1';
		case 19: return '2';
		case 20: return '3';
		case 21: return '4';
		case 23: return '5';
		case 22: return '6';
		case 26: return '7';
		case 28: return '8';
		case 25: return '9';
		case 29: return '0';
		
		// alpha
		case 0: return 'a';
		case 11: return 'b';
		case 8: return 'c';
		case 2: return 'd';
		case 14: return 'e';
		case 3: return 'f';
		case 5: return 'g';
		case 4: return 'h';
		case 34: return 'i';
		case 38: return 'j';
		case 40: return 'k';
		case 37: return 'l';
		case 46: return 'm';
		case 45: return 'n';
		case 31: return 'o';
		case 35: return 'p';
		case 12: return 'q';
		case 15: return 'r';
		case 1: return 's';
		case 17: return 't';
		case 32: return 'u';
		case 9: return 'v';
		case 13: return 'w';
		case 7: return 'x';
		case 16: return 'y';
		case 6: return 'z';
		
		default:
			printf("%s:%i - unimplemented virt->lgi code mapping: %d\n", _FL, (unsigned)Virt);
			break;
	}
	
	return 0;
}

#if 0
static int GetIsChar(LKey &k, int mods)
{
	return k.IsChar =	(mods & 0x100) == 0 &&
	(
		k.c16 >= ' ' ||
		k.c16 == VK_RETURN ||
		k.c16 == VK_TAB ||
		k.c16 == VK_BACKSPACE
	);
}
#endif


bool LView::_Attach(LViewI *parent)
{
	LAutoPool Pool;
	if (!parent)
	{
		LAssert(0);
		return false;
	}

	d->ClassName = GetClass();
	
	d->ParentI = parent;
	d->Parent = d->ParentI ? parent->GetGView() : NULL;

	LAssert(!_InLock);
	_Window = d->GetParent() ? d->GetParent()->GetWindow() : 0;
	if (_Window)
		_Lock = _Window->_Lock;

	auto p = d->GetParent();
	if (p)
	{
		LWindow *w = dynamic_cast<LWindow*>(p);
		NSView *ph = nil;
		if (w)
			ph = w->WindowHandle().p.contentView;

		if (!d->AttachEvent)
		{
			d->AttachEvent = true;
			OnCreate();
		}
	}
	else
	{
		LAssert(0);
	}

	if (!p->Children.HasItem(this))
	{
		p->Children.Add(this);
		OnAttach();
	}

	return true;
}

bool LView::Attach(LViewI *parent)
{
	if (_Attach(parent))
	{
		if (d->Parent && !d->Parent->HasView(this))
			d->Parent->AddView(this);
		
		d->Parent->OnChildrenChanged(this, true);
		return true;
	}
	
	LgiTrace("%s:%i - Attaching '%s' failed.\n", _FL, GetClass());
	return false;
}

void LView::_Delete()
{
	LAutoPool Pool;
	if (_Over == this) _Over = 0;
	if (_Capturing == this) _Capturing = 0;

	if (LAppInst && LAppInst->AppWnd == this)
	{
		LAppInst->AppWnd = NULL;
	}

	SetPulse();
	Pos.ZOff(-1, -1);

	LViewI *c;
	auto it = Children.begin();
	while ((c = *it))
	{
		LViewI *p = c->GetParent();
		if (p != (LViewI*)this)
		{
			printf("Error: LView::_Delete, child not attached correctly: %p(%s) Parent: %p(%s)\n",
				c, c->Name(),
				c->GetParent(), c->GetParent() ? c->GetParent()->Name() : "");
			Children.Delete(it);
		}

		DeleteObj(c);
	}
	
	Detach();
}

bool LView::Detach()
{
	LAutoPool Pool;
	bool Status = false;

	// Detach view
	if (_Window)
	{
		LWindow *Wnd = dynamic_cast<LWindow*>(_Window);
		if (Wnd)
			Wnd->SetFocus(this, LWindow::ViewDelete);
		_Window = NULL;
	}

	if (d->Parent)
	{
		// Remove the view from the parent
		int D = 0;
		while (d->Parent->HasView(this))
		{
			d->Parent->DelView(this);
			D++;
		}

		if (D > 1)
		{
			printf("%s:%i - Error: View %s(%p) was in %s(%p) list %i times.\n",
				_FL, GetClass(), this, d->Parent->GetClass(), d->Parent, D);
		}

		d->Parent->OnChildrenChanged(this, false);
		d->Parent = NULL;
	}

	Status = true;

	return Status;
}

////////////////////////////////
uint32 CursorData[] = {
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
