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
#include "AppPriv.h"
#include <Cursor.h>

#define DEBUG_MOUSE_EVENTS			0

#if 0
#define DEBUG_INVALIDATE(...)		printf(__VA_ARGS__)
#else
#define DEBUG_INVALIDATE(...)
#endif

#define DEBUG_VIEW					0
#if DEBUG_VIEW
#define LOG(...)					printf(__VA_ARGS__)
#else
#define LOG(...)
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

////////////////////////////////////////////////////////////////////////////////////////////////////
LViewPrivate::LViewPrivate(LView *view) :
	View(view)
{
}

LViewPrivate::~LViewPrivate()
{
	View->d = NULL;

	MsgQue.DeleteObjects();

	if (Font && FontOwnType == GV_FontOwned)
		DeleteObj(Font);

	while (EventTargets.Length())
		delete EventTargets[0];
}

void LView::SetFlagAll(int flag, bool add)
{
	if (add)
		WndFlags |= flag;
	else
		WndFlags &= ~flag;

	for (auto c: Children)
		if (auto v = c->GetLView())
			v->SetFlagAll(flag, add);
}

void LView::_Focus(bool f)
{
	ThreadCheck();
	
	if (f)
		SetFlag(WndFlags, GWF_FOCUS);
	else
		ClearFlag(WndFlags, GWF_FOCUS);

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
				
				if (auto w = WindowHandle())
				{
					LLocker lck(w, _FL);
					if (lck.Lock())
					{
						if (auto v = w->ChildAt(0))
							v->SetViewCursor(new BCursor(curId));
						lck.Unlock();
					}
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
	auto border = GetBorder();

	static LRect c;
	if (ClientSpace)
	{
		c.ZOff(Pos.X() - 1 - _Border.x1 - _Border.x2, Pos.Y() - 1 - _Border.y1 - _Border.y2);
	}
	else
	{
		c.ZOff(Pos.X()-1, Pos.Y()-1);
		c.Inset(&_Border);
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
	bool debug = false;

	if (Pos != p)
	{
		Pos = p;
		OnPosChange();
		
		// FIXME: work out union of before and after position to invalidate:
		Invalidate();
	}

	return true;
}

bool LView::Invalidate(LRect *rc, bool Repaint, bool Frame)
{
	auto wnd = GetWindow();
	if (!wnd)
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
		r.Inset(&GetBorder());

	LPoint Offset;
	WindowVirtualOffset(&Offset);
	r.Offset(Offset.x, Offset.y);
	DEBUG_INVALIDATE("	voffset=%i,%i = %s\n", Offset.x, Offset.y, r.GetStr());
	if (!r.Valid())
	{
		DEBUG_INVALIDATE("	error: invalid\n");
		return false;
	}

	auto hnd = wnd->WindowHandle();
	if (!hnd)
	{
		printf("%s:%i - no handle.\n", _FL);
		return false;
	}
	
	// Ask the LWindow to paint the area, and then invalidate itself:
	BMessage m(M_HAIKU_WND_EVENT);
	m.AddPointer(LMessage::PropWindow, (void*)wnd);
	m.AddInt32(LMessage::PropEvent, LMessage::Invalidate);
	m.AddRect("rect", r);
	LAppPrivate::Post(&m);
		
	return true;
}

void LView::SetPulse(int Length)
{
	DeleteObj(d->PulseThread);
	if (Length > 0)
		d->PulseThread = new LPulseThread(this, Length);
}

void LView::HandleInThreadMessage(BMessage *Msg)
{
	LMessage::InThreadCb *Cb = NULL;
	if (Msg->FindPointer(LMessage::PropCallback, (void**)&Cb) == B_OK)
	{
		// printf("M_HANDLE_IN_THREAD before call..\n");
		(*Cb)();
		// printf("M_HANDLE_IN_THREAD after call..\n");
		delete Cb;
		// printf("M_HANDLE_IN_THREAD after delete..\n");
	}
	else printf("%s:%i - No Callback.\n", _FL);
}

LMessage::Param LView::OnEvent(LMessage *Msg)
{
	ThreadCheck();

	for (auto target: d->EventTargets)
	{
		if (target->Msgs.Length() == 0 ||
			target->Msgs.Find(Msg->Msg()))
			target->OnEvent(Msg);
	}

	int Id;
	switch (Id = Msg->Msg())
	{
		case M_HANDLE_IN_THREAD:
		{
			HandleInThreadMessage(Msg);
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
		case M_CHANGE:
		{
			LAutoPtr<LNotification> notification((LNotification*)Msg->B());
			LViewI *Ctrl = NULL;
			if (GetViewById(Msg->A(), Ctrl) && notification)
			{
				return OnNotify(Ctrl, *notification);
			}
			break;
		}
		case M_COMMAND:
		{
			// printf("M_COMMAND %i\n", (int)Msg->A());
			return OnCommand(Msg->A(), 0, 0);
		}
	}

	LMessage::Result result;
	if (CommonEvents(result, Msg))
		return result;

	return 0;
}

bool LView::PointToScreen(LPoint &p)
{
	LPoint Offset;
	WindowVirtualOffset(&Offset);
	// printf("p=%i,%i offset=%i,%i\n", p.x, p.y, Offset.x, Offset.y);
	p += Offset;
	// printf("p=%i,%i\n", p.x, p.y);

	auto w = WindowHandle();
	LLocker lck(w, _FL);
	if (!lck.Lock())
	{
		LgiTrace("%s:%i - Can't lock.\n", _FL);
		return false;
	}
	
	if (auto v = w->ChildAt(0))
	{
		BPoint pt = v->ConvertToScreen(BPoint(p.x, p.y));
		// printf("pt=%g,%g\n", pt.x, pt.y);
		p.x = pt.x;
		p.y = pt.y;
		// printf("p=%i,%i\n\n", p.x, p.y);
	}

	return true;
}

bool LView::PointToView(LPoint &p)
{
	LPoint Offset;
	WindowVirtualOffset(&Offset);
	p -= Offset;

	auto w = WindowHandle();
	LLocker lck(w, _FL);
	if (!lck.Lock())
	{
		LgiTrace("%s:%i - Can't lock.\n", _FL);
		return false;
	}
	
	if (auto v = w->ChildAt(0))
	{
		BPoint pt = v->ConvertFromScreen(BPoint(Offset.x, Offset.y));
		Offset.x = pt.x;
		Offset.y = pt.y;
	}

	return true;
}

bool LView::GetMouse(LMouse &m, bool ScreenCoords)
{
	auto w = WindowHandle();
	LLocker Locker(w, _FL);
	if (!Locker.Lock())
		return false;

	m.Target = this;

	// get mouse state
	BPoint Cursor;
	uint32 Buttons;
	
	if (auto v = w->ChildAt(0))
	{
		v->GetMouse(&Cursor, &Buttons);
		if (ScreenCoords)
			v->ConvertToScreen(&Cursor);
	}
	
	// position
	m.x = Cursor.x;
	m.y = Cursor.y;

	// buttons
	m.Left  (TestFlag(Buttons, B_PRIMARY_MOUSE_BUTTON));
	m.Middle(TestFlag(Buttons, B_TERTIARY_MOUSE_BUTTON));
	m.Right (TestFlag(Buttons, B_SECONDARY_MOUSE_BUTTON));

	// key states
	key_info KeyInfo;
	if (get_key_info(&KeyInfo) == B_OK)
	{
		m.Ctrl (TestFlag(KeyInfo.modifiers, B_CONTROL_KEY));
		m.Alt  (TestFlag(KeyInfo.modifiers, B_MENU_KEY));
		m.Shift(TestFlag(KeyInfo.modifiers, B_SHIFT_KEY));
	}
	
	return true;
}

bool LView::IsAttached()
{
	return GetParent() && GetWindow();
}

const char *LView::GetClass()
{
	return "LView";
}

bool LView::Attach(LViewI *parent)
{
	bool Status = false;
	bool Debug = false; // !Stricmp(GetClass(), "LScrollBar");

	// Parent handling
	auto wasAttached = IsAttached();
	LView *oldParent = d->GetParent();
	if (oldParent && oldParent != parent)
	{
		Detach();
	}

	SetParent(parent);
	auto Parent = d->GetParent();
	if (!Parent)
	{
		LgiTrace("%s:%i - No parent window.\n", _FL);
		return false;
	}

	auto w = GetWindow();

	if (Debug)
		LgiTrace("%s:%i - Attaching %s to view %s\n",
			_FL, GetClass(), parent->GetClass());

	Status = true;
	
	if (d->MsgQue.Length())
	{
		auto w = WindowHandle();
		LLocker lck(w, _FL);
		if (lck.Lock())
		{
			for (auto bmsg: d->MsgQue)
				w->PostMessage(bmsg);
			d->MsgQue.DeleteObjects();
		}
	}

	if (Debug)
		LgiTrace("%s:%i - Attached %s to view %S, success\n",
			_FL,
			GetClass(), parent->GetClass());

	if (w && TestFlag(WndFlags, GWF_FOCUS))
		w->SetFocus(this, LWindow::GainFocus);

	if (!Parent->HasView(this))
	{
		OnAttach();
		if (!d->Parent->HasView(this))
			d->Parent->AddView(this);
		d->Parent->OnChildrenChanged(this, true);
	}
	
	if (IsAttached() && !wasAttached && !d->onCreateEvent)
	{
		d->onCreateEvent = true;
		OnCreate();
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

