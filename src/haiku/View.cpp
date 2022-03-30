/*hdr
**      FILE:           LView.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           29/11/2021
**      DESCRIPTION:    Haiku LView Implementation
**
**      Copyright (C) 2021, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Popup.h"
#include "lgi/common/Css.h"

#include "ViewPriv.h"
#include <Cursor.h>

#define DEBUG_MOUSE_EVENTS			0

#if 0
#define DEBUG_INVALIDATE(...)		printf(__VA_ARGS__)
#else
#define DEBUG_INVALIDATE(...)
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

////////////////////////////////////////////////////////////////////////////
void _lgi_yield()
{
	LAppInst->Yield();
}

void *IsAttached(BView *v)
{
	auto pview = v->Parent();
	auto pwnd = v->Window();
	return pwnd ? (void*)pwnd : (void*)pview;
}

bool LgiIsKeyDown(int Key)
{
	LAssert(0);
	return false;
}

LKey::LKey(int Vkey, uint32_t flags)
{
	vkey = Vkey;
	Flags = flags;
	IsChar = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename Parent = BView>
struct LBView : public Parent
{
	LViewPrivate *d = NULL;
	static uint32 MouseButtons;

	LBView(LViewPrivate *priv) :
		d(priv),
		Parent
		(
			"",
			B_FULL_UPDATE_ON_RESIZE | 
			B_WILL_DRAW |
			B_NAVIGABLE |
			B_FRAME_EVENTS
		)
	{
		SetName(d->View->GetClass());
	}
	
	~LBView()
	{
		if (d)
			d->Hnd = NULL;
	}		

	void AttachedToWindow()
	{
		if (d)
			d->View->OnCreate();
	}
	
	LKey ConvertKey(const char *bytes, int32 numBytes)
	{
		LKey k;
		
		uint8_t *utf = (uint8_t*)bytes;
		ssize_t len = numBytes;
		auto w = LgiUtf8To32(utf, len);

		#if 1
		LString::Array a;
		for (int i=0; i<numBytes; i++)
			a.New().Printf("%i(%x)", (uint8_t)bytes[i], (uint8_t)bytes[i]);		
		printf("ConvertKey(%s)=%i\n", LString(",").Join(a).Get(), w);
		#endif
		
		if (w)
		{
			if (w == B_FUNCTION_KEY)
			{
				auto bmsg = Window()->CurrentMessage();
				if (bmsg)
				{
					int32 key = 0;
					if (bmsg->FindInt32("key", &key) == B_OK)
					{
						// Translate the function keys into the LGI address space...
						switch (key)
						{
							case B_F1_KEY: w = LK_F1; break;
							case B_F2_KEY: w = LK_F2; break;
							case B_F3_KEY: w = LK_F3; break;
							case B_F4_KEY: w = LK_F4; break;
							case B_F5_KEY: w = LK_F5; break;
							case B_F6_KEY: w = LK_F6; break;
							case B_F7_KEY: w = LK_F7; break;
							case B_F8_KEY: w = LK_F8; break;
							case B_F9_KEY: w = LK_F9; break;
							case B_F10_KEY: w = LK_F10; break;
							case B_F11_KEY: w = LK_F11; break;
							case B_F12_KEY: w = LK_F12; break;
							default:
								printf("%s:%i - Upsupported key %i.\n", _FL, key);
								break;
						}
					}
					else printf("%s:%i - No 'key' in BMessage.\n", _FL);
				}
				else printf("%s:%i - No BMessage.\n", _FL);
			}

			k.c16 = k.vkey = w;
			
			key_info KeyInfo;
			int flags = 0;
			if (get_key_info(&KeyInfo) == B_OK)
			{
				k.Ctrl(TestFlag(KeyInfo.modifiers, B_CONTROL_KEY));
				k.Alt(TestFlag(KeyInfo.modifiers, B_MENU_KEY));
				k.Shift(TestFlag(KeyInfo.modifiers, B_SHIFT_KEY));
				k.System(TestFlag(KeyInfo.modifiers, B_COMMAND_KEY));
			}
		}

		k.IsChar = (k.c16 >= ' ' && k.c16 < LK_DELETE) ||
			k.c16 == LK_BACKSPACE ||
			k.c16 == LK_TAB ||
			k.c16 == LK_RETURN;

		return k;
	}
	
	void KeyDown(const char *bytes, int32 numBytes)
	{
		if (!d) return;
		
		auto k = ConvertKey(bytes, numBytes);
		k.Down(true);

		auto wnd = d->View->GetWindow();
		if (wnd)
			wnd->HandleViewKey(d->View, k);
		else
			d->View->OnKey(k);
	}
	
	void KeyUp(const char *bytes, int32 numBytes)
	{
		if (!d) return;

		auto k = ConvertKey(bytes, numBytes);
		auto wnd = d->View->GetWindow();
		if (wnd)
			wnd->HandleViewKey(d->View, k);
		else
			d->View->OnKey(k);
	}

	void FrameMoved(BPoint newPosition)
	{
		if (!d) return;
		d->View->Pos = Frame();
		d->View->OnPosChange();
	}

	void FrameResized(float newWidth, float newHeight)
	{
		if (!d) return;
		d->View->Pos = Frame();
		d->View->OnPosChange();
	}

	void MessageReceived(BMessage *message)
	{
		if (!d) return;
		void *v = NULL;
		if (message->FindPointer(LMessage::PropView, &v) == B_OK)
		{
			// Proxy'd event for child view...
			((LView*)v)->OnEvent((LMessage*)message);
			return;
		}
		else d->View->OnEvent((LMessage*)message);
		
		if (message->what == B_MOUSE_WHEEL_CHANGED)
		{
			float x = 0.0f, y = 0.0f;
			message->FindFloat("be:wheel_delta_x", &x);
			message->FindFloat("be:wheel_delta_y", &y);
			d->View->OnMouseWheel(y * 3.0f);
		}
		else if (message->what == M_SET_SCROLL)
		{
			return;
		}
		
		Parent::MessageReceived(message);
	}

	void Draw(BRect updateRect)
	{
		if (!d) return;
		LScreenDC dc(d->View);
		LPoint off(0,0);
		d->View->_Paint(&dc, &off, NULL);
	}

	LMouse ConvertMouse(BPoint where, bool down = false)
	{
		LMouse m;
		BPoint loc;
		uint32 buttons = 0;
		
		m.Target = d->View;
		
		m.x = where.x;
		m.y = where.y;
		
		if (down)
		{
			m.Down(true);
			GetMouse(&loc, &buttons, false);
			MouseButtons = buttons; // save for up click
		}
		else
		{
			buttons = MouseButtons;
		}
		
		if (buttons & B_PRIMARY_MOUSE_BUTTON) m.Left(true);
		if (buttons & B_TERTIARY_MOUSE_BUTTON) m.Middle(true);
		if (buttons & B_SECONDARY_MOUSE_BUTTON) m.Right(true);
		
		uint32 mod = modifiers();
		if (mod & B_SHIFT_KEY) m.Shift(true);
		if (mod & B_OPTION_KEY) m.Alt(true);
		if (mod & B_CONTROL_KEY) m.Ctrl(true);
		if (mod & B_COMMAND_KEY) m.System(true);
		
		return m;
	}
	
	void MouseDown(BPoint where)
	{
		if (!d) return;
		static uint64_t lastClick = 0;
		bigtime_t interval = 0;
		status_t r = get_click_speed(&interval);
		auto now = LCurrentTime();
		bool doubleClick = now-lastClick < (interval/1000);
		lastClick = now;
		
		LMouse m = ConvertMouse(where, true);
		m.Double(doubleClick);
		d->View->_Mouse(m, false);
	}
	
	void MouseUp(BPoint where) 
	{
		if (!d) return;
		LMouse m = ConvertMouse(where);
		m.Down(false);
		d->View->_Mouse(m, false);
	}
	
	void MouseMoved(BPoint where, uint32 code, const BMessage *dragMessage)
	{
		if (!d) return;
		LMouse m = ConvertMouse(where);
		m.Down(	m.Left() ||
				m.Middle() ||
				m.Right());
		m.IsMove(true);
		d->View->_Mouse(m, true);
	}

	void MakeFocus(bool focus=true)
	{
		if (!d) return;
		Parent::MakeFocus(focus);
		d->View->OnFocus(focus);
	}
};

template<typename Parent>
uint32 LBView<Parent>::MouseButtons = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
LViewPrivate::LViewPrivate(LView *view) :
	View(view),
	Hnd(new LBView<BView>(this))
{
}

LViewPrivate::~LViewPrivate()
{
	View->d = NULL;

	if (Font && FontOwnType == GV_FontOwned)
		DeleteObj(Font);

	if (Hnd)
	{
		auto *bv = dynamic_cast<LBView<BView>*>(Hnd);
		// printf("%p::~LViewPrivate View=%p bv=%p th=%u\n", this, View, bv, GetCurrentThreadId());
		if (bv)
			bv->d = NULL;

		auto Wnd = Hnd->Window();

		if (Wnd)
			Wnd->LockLooper();
			
		if (Hnd->Parent())
			Hnd->RemoveSelf();
		
		DeleteObj(Hnd);

		if (Wnd)
			Wnd->UnlockLooper();
	}
}

void LView::_Focus(bool f)
{
	ThreadCheck();
	
	if (f)
		SetFlag(WndFlags, GWF_FOCUS);
	else
		ClearFlag(WndFlags, GWF_FOCUS);

	LLocker lck(d->Hnd, _FL);
	if (lck.Lock())
	{
		d->Hnd->MakeFocus(f);
		lck.Unlock();
	}

	// OnFocus will be called by the LBview handler...
	Invalidate();
}

void LView::_Delete()
{
	SetPulse();

	// Remove static references to myself
	if (_Over == this)
		_Over = 0;
	if (_Capturing == this)
		_Capturing = 0;
	
	auto *Wnd = GetWindow();
	if (Wnd && Wnd->GetFocus() == static_cast<LViewI*>(this))
		Wnd->SetFocus(this, LWindow::ViewDelete);

	if (LAppInst && LAppInst->AppWnd == this)
	{
		LAppInst->AppWnd = 0;
	}

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

BCursorID LgiToHaiku(LCursor c)
{
	switch (c)
	{
		#define _(l,h) case l: return h;
		_(LCUR_Blank, B_CURSOR_ID_NO_CURSOR)
		_(LCUR_Normal, B_CURSOR_ID_SYSTEM_DEFAULT)
		_(LCUR_UpArrow, B_CURSOR_ID_RESIZE_NORTH)
		_(LCUR_DownArrow, B_CURSOR_ID_RESIZE_SOUTH)
		_(LCUR_LeftArrow, B_CURSOR_ID_RESIZE_WEST)
		_(LCUR_RightArrow, B_CURSOR_ID_RESIZE_EAST)
		_(LCUR_Cross, B_CURSOR_ID_CROSS_HAIR)
		_(LCUR_Wait, B_CURSOR_ID_PROGRESS)
		_(LCUR_Ibeam, B_CURSOR_ID_I_BEAM)
		_(LCUR_SizeVer, B_CURSOR_ID_RESIZE_NORTH_SOUTH)
		_(LCUR_SizeHor, B_CURSOR_ID_RESIZE_EAST_WEST)
		_(LCUR_SizeBDiag, B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST)
		_(LCUR_SizeFDiag, B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST)
		_(LCUR_PointingHand, B_CURSOR_ID_GRAB)
		_(LCUR_Forbidden, B_CURSOR_ID_NOT_ALLOWED)
		_(LCUR_DropCopy, B_CURSOR_ID_COPY)
		_(LCUR_DropMove, B_CURSOR_ID_MOVE)
		// _(LCUR_SizeAll,
		// _(LCUR_SplitV,
		// _(LCUR_SplitH,
		#undef _
	}
	
	return B_CURSOR_ID_SYSTEM_DEFAULT;
}

bool LView::_Mouse(LMouse &m, bool Move)
{
	ThreadCheck();
	
	#if DEBUG_MOUSE_EVENTS
	if (!Move)
		LgiTrace("%s:%i - %s\n", _FL, m.ToString().Get());
	#endif

	if
	(
		GetWindow()
		&&
		!GetWindow()->HandleViewMouse(this, m)
	)
	{
		#if DEBUG_MOUSE_EVENTS
		LgiTrace("%s:%i - HandleViewMouse consumed event, cls=%s\n", _FL, GetClass());
		#endif
		return false;
	}

	#if 0 //DEBUG_MOUSE_EVENTS
	if (!Move)
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
		
		int cursor = GetCursor(m.x, m.y);
		if (cursor >= 0)
		{
			BCursorID haikuId = LgiToHaiku((LCursor)cursor);
			static BCursorID curId = B_CURSOR_ID_SYSTEM_DEFAULT;
			if (curId != haikuId)
			{
				curId = haikuId;
				
				LLocker lck(Handle(), _FL);
				if (lck.Lock())
				{
					Handle()->SetViewCursor(new BCursor(curId));
					lck.Unlock();
				}
			}
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
		if (Move)
			Target->OnMouseMove(m);
		else
			Target->OnMouseClick(m);
	}
	else if (!Move)
	{
		#if DEBUG_MOUSE_EVENTS
		LgiTrace("%s:%i - Click outside %s %s %i,%i\n", _FL, Target->GetClass(), Client.GetStr(), m.x, m.y);
		#endif
	}
	
	return true;
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

LViewI *LView::FindControl(OsView hCtrl)
{
	if (d->Hnd == hCtrl)
	{
		return this;
	}

	for (auto i : Children)
	{
		LViewI *Ctrl = i->FindControl(hCtrl);
		if (Ctrl)
		{
			return Ctrl;
		}
	}
	return 0;
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

		LLocker lck(d->Hnd, _FL);
		if (lck.Lock())
		{
			d->Hnd->ResizeTo(Pos.X(), Pos.Y());
			d->Hnd->MoveTo(Pos.x1, Pos.y1);
			lck.Unlock();
		}

		OnPosChange();
	}

	return true;
}

bool LView::Invalidate(LRect *rc, bool Repaint, bool Frame)
{
	auto *ParWnd = GetWindow();
	if (!ParWnd)
		return false; // Nothing we can do till we attach
	if (!InThread())
	{
		DEBUG_INVALIDATE("%s::Invalidate out of thread\n", GetClass());
		return PostEvent(M_INVALIDATE, NULL, (LMessage::Param)this);
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

		if (d->Hnd)
		{
			LLocker lck(d->Hnd, _FL);
			if (lck.Lock())
				d->Hnd->Invalidate();
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
	DeleteObj(d->PulseThread);
	if (Length > 0)
		d->PulseThread = new LPulseThread(this, Length);
}

LMessage::Param LView::OnEvent(LMessage *Msg)
{
	ThreadCheck();
	
	int Id;
	switch (Id = Msg->Msg())
	{
		case M_HANDLE_IN_THREAD:
		{
			LMessage::InThreadCb *Cb = NULL;
			if (Msg->FindPointer(LMessage::PropCallback, &Cb) == B_OK)
			{
				(*Cb)();
				delete Cb;
			}
			else printf("%s:%i - No Callback.\n", _FL);
			break;
		}
		case M_INVALIDATE:
		{
			if ((LView*)this == (LView*)Msg->B())
			{
				LAutoPtr<LRect> r((LRect*)Msg->A());
				Invalidate(r);
			}
			break;
		}
		case M_PULSE:
		{
			OnPulse();
			break;
		}
		case M_CHANGE:
		{
			LViewI *Ctrl;
			if (GetViewById(Msg->A(), Ctrl))
				return OnNotify(Ctrl, Msg->B());
			break;
		}
		case M_COMMAND:
		{
			// printf("M_COMMAND %i\n", (int)Msg->A());
			return OnCommand(Msg->A(), 0, 0);
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
	}

	return 0;
}

OsView LView::Handle() const
{
	if (!d)
	{
		printf("%s:%i - No priv?\n", _FL);
		return NULL;
	}
	
	return d->Hnd;
}

bool LView::PointToScreen(LPoint &p)
{
	if (!Handle())
	{
		LgiTrace("%s:%i - No handle.\n", _FL);
		return false;
	}

	LPoint Offset;
	WindowVirtualOffset(&Offset);
	// printf("p=%i,%i offset=%i,%i\n", p.x, p.y, Offset.x, Offset.y);
	p += Offset;
	// printf("p=%i,%i\n", p.x, p.y);

	LLocker lck(Handle(), _FL);
	if (!lck.Lock())
	{
		LgiTrace("%s:%i - Can't lock.\n", _FL);
		return false;
	}
	
	BPoint pt = Handle()->ConvertToScreen(BPoint(p.x, p.y));
	// printf("pt=%g,%g\n", pt.x, pt.y);
	p.x = pt.x;
	p.y = pt.y;
	// printf("p=%i,%i\n\n", p.x, p.y);

	return true;
}

bool LView::PointToView(LPoint &p)
{
	if (!Handle())
	{
		LgiTrace("%s:%i - No handle.\n", _FL);
		return false;
	}

	LPoint Offset;
	WindowVirtualOffset(&Offset);
	p -= Offset;

	LLocker lck(Handle(), _FL);
	if (!lck.Lock())
	{
		LgiTrace("%s:%i - Can't lock.\n", _FL);
		return false;
	}
	
	BPoint pt = Handle()->ConvertFromScreen(BPoint(Offset.x, Offset.y));
	Offset.x = pt.x;
	Offset.y = pt.y;

	return true;
}

bool LView::GetMouse(LMouse &m, bool ScreenCoords)
{
	LLocker Locker(d->Hnd, _FL);
	if (!Locker.Lock())
		return false;

	// get mouse state
	BPoint Cursor;
	uint32 Buttons;
	
	d->Hnd->GetMouse(&Cursor, &Buttons);
	if (ScreenCoords)
		d->Hnd->ConvertToScreen(&Cursor);
	
	// position
	m.x = Cursor.x;
	m.y = Cursor.y;

	// buttons
	m.Left(TestFlag(Buttons, B_PRIMARY_MOUSE_BUTTON));
	m.Middle(TestFlag(Buttons, B_TERTIARY_MOUSE_BUTTON));
	m.Right(TestFlag(Buttons, B_SECONDARY_MOUSE_BUTTON));

	// key states
	key_info KeyInfo;
	if (get_key_info(&KeyInfo) == B_OK)
	{
		m.Ctrl(TestFlag(KeyInfo.modifiers, B_CONTROL_KEY));
		m.Alt(TestFlag(KeyInfo.modifiers, B_MENU_KEY));
		m.Shift(TestFlag(KeyInfo.modifiers, B_SHIFT_KEY));
	}
	
	return true;
}

bool LView::IsAttached()
{
	bool attached = false;
	
	LLocker lck(d->Hnd, _FL);
	if (lck.Lock())
	{
		auto pview = d->Hnd->Parent();
		auto pwnd = d->Hnd->Window();
		attached = pview != NULL || pwnd != NULL;
	}
	
	return attached;
}

const char *LView::GetClass()
{
	return "LView";
}

bool LView::Attach(LViewI *parent)
{
	bool Status = false;

	LView *Parent = d->GetParent();
	LAssert(Parent == NULL || Parent == parent);

	SetParent(parent);
	Parent = d->GetParent();
	
	auto WndNull = _Window == NULL;
	_Window = Parent ? Parent->_Window : this;

	if (!parent)
	{
		LgiTrace("%s:%i - No parent window.\n", _FL);
	}
	else
	{
		auto w = GetWindow();
		if (w && TestFlag(WndFlags, GWF_FOCUS))
			w->SetFocus(this, LWindow::GainFocus);

		auto bview = parent->Handle();
		if (bview)
		{
			LLocker lck(bview, _FL);
			if (lck.Lock())
			{
				#if 0
				LgiTrace("%s:%i - Attaching %s to view %s\n",
					_FL, GetClass(), parent->GetClass());
				#endif

				d->Hnd->SetName(GetClass());
				if (::IsAttached(d->Hnd))
					d->Hnd->RemoveSelf();
				bview->AddChild(d->Hnd);

				d->Hnd->ResizeTo(Pos.X(), Pos.Y());
				d->Hnd->MoveTo(Pos.x1, Pos.y1);
				if (TestFlag(GViewFlags, GWF_VISIBLE) && d->Hnd->IsHidden())
					d->Hnd->Show();

				Status = true;

				#if 0
				LgiTrace("%s:%i - Attached %s/%p to view %s/%p, success\n",
					_FL,
					GetClass(), d->Hnd,
					parent->GetClass(), bview);
				#endif
			}
			else
			{
				#if 0
				LgiTrace("%s:%i - Error attaching %s to view %s, can't lock.\n",
					_FL, GetClass(), parent->GetClass());
				#endif
			}
		}

		if (!Parent->HasView(this))
		{
			OnAttach();
			if (!d->Parent->HasView(this))
				d->Parent->AddView(this);
			d->Parent->OnChildrenChanged(this, true);
		}
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

