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
GInlineBmp Cursors =
{
	300, 20, 8, CursorData
};

////////////////////////////////////////////////////////////////////////////
void _lgi_yield()
{
	LAppInst->Run(false);
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
class LBView : public BView
{
	LViewPrivate *d = NULL;

public:
	LBView(LViewPrivate *priv) :
		d(priv),
		BView
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
		d->Hnd = NULL;
	}		

	void AttachedToWindow()
	{
		d->View->OnCreate();
	}

	void FrameMoved(BPoint newPosition)
	{
		d->View->Pos = Frame();
		d->View->OnPosChange();
	}

	void FrameResized(float newWidth, float newHeight)
	{
		d->View->Pos = Frame();
		d->View->OnPosChange();
	}

	void MessageReceived(BMessage *message)
	{
		d->View->OnEvent(message);
		BView::MessageReceived(message);
	}

	void Draw(BRect updateRect)
	{
		LScreenDC dc(d->View);
		d->View->OnPaint(&dc);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
LViewPrivate::LViewPrivate(LView *view) :
	View(view),
	Hnd(new LBView(this))
{
}

LViewPrivate::~LViewPrivate()
{
	View->d = NULL;

	if (Font && FontOwnType == GV_FontOwned)
		DeleteObj(Font);

	if (Hnd)
	{
		if (Hnd->LockLooper())
			delete Hnd;
		Hnd = NULL;
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

bool LView::_Mouse(LMouse &m, bool Move)
{
	ThreadCheck();
	
	#if DEBUG_MOUSE_EVENTS
	if (!Move)
		LgiTrace("%s:%i - _Mouse([%i,%i], %i) View=%p/%s\n", _FL, m.x, m.y, Move, _Over, _Over ? _Over->GetClass() : NULL);
	#endif

	if
	(
		/*
		!_View
		||
		*/
		(
			GetWindow()
			&&
			!GetWindow()->HandleViewMouse(this, m)
		)
	)
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
		if (Move)
		{
			Target->OnMouseMove(m);
		}
		else
		{
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
		c.Size(Edge, Edge);
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

		LLocker lck(d->Hnd, _FL);
		if (lck.Lock())
		{
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
			return OnCommand(Msg->A(), 0, (OsView) Msg->B());
		}
	}

	return 0;
}

OsView LView::Handle() const
{
	return d->Hnd;
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

	LAssert(!"Fixme.");

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

	LAssert(!"Fixme.");

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

		auto *Wnd = dynamic_cast<LWindow*>(parent);
		if (Wnd)
		{
			auto bwnd = parent->WindowHandle();
			if (bwnd)
			{
				if (bwnd->LockLooper())
				{
					#if 0
					LgiTrace("%s:%i - Attaching %s to window %s (parent=%p)\n",
						_FL, GetClass(), parent->GetClass(), ::IsAttached(d));
					#endif

					if (::IsAttached(d->Hnd))
						d->Hnd->RemoveSelf();
					bwnd->AddChild(d->Hnd);
					
					d->Hnd->ResizeTo(Pos.X(), Pos.Y());
					d->Hnd->MoveTo(Pos.x1, Pos.y1);
					if (Visible())
						d->Hnd->Show();

					bwnd->Unlock();

					Status = true;

					#if 0
					LgiTrace("%s:%i - Attached %s/%p to window %s/%p, success (parent=%p)\n",
						_FL,
						GetClass(), d->Hnd,
						parent->GetClass(), bwnd,
						::IsAttached(d->Hnd));
					#endif
				}
				else
				{
					LgiTrace("%s:%i - Error attaching %s to window %s, can't lock.\n",
						_FL, GetClass(), parent->GetClass());
				}
			}
			else LgiTrace("%s:%i - Error no window handle for %s\n", _FL, parent->GetClass());
		}
		else
		{
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

					if (::IsAttached(d->Hnd))
						d->Hnd->RemoveSelf();
					bview->AddChild(d->Hnd);

					d->Hnd->ResizeTo(Pos.X(), Pos.Y());
					d->Hnd->MoveTo(Pos.x1, Pos.y1);
					if (Visible())
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

