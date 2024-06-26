#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Token.h"
#include "lgi/common/Popup.h"
#include "lgi/common/Panel.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Menu.h"

#include "ViewPriv.h"
#include "MenuBar.h"

#define DEBUG_SETFOCUS			0
#define DEBUG_HANDLEVIEWKEY		0
#define DEBUG_WAIT_THREAD		0
#define DEBUG_SERIALIZE_STATE	0

#if DEBUG_WAIT_THREAD
	#define WAIT_LOG(...)		LgiTrace(__VA_ARGS__)
#else
	#define WAIT_LOG(...)
#endif

#define DEBUG_WINDOW			0
#if DEBUG_WINDOW
#define LOG(...)				printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

LString ToString(BRect &r)
{
	LString s;
	s.Printf("%g,%g,%g,%g", r.left, r.top, r.right, r.bottom);
	return s;
}

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

class LWindowPrivate : public BWindow
{
public:
	LWindow *Wnd;
	bool SnapToEdge = false;
	LArray<HookInfo> Hooks;
	LWindow *ModalParent = NULL;
	LWindow *ModalChild = NULL;
	bool ShowTitleBar = true;
	bool ThreadMsgDone = false;
	
	LString MakeName(LWindow *w)
	{
		LString s;
		s.Printf("%p::con::%s", w, w->GetClass());
		// printf("%s\n", s.Get());
		return s;
	}
	
	LWindowPrivate(LWindow *wnd) :
		BWindow(BRect(100,100,400,400),
				MakeName(wnd),
				B_DOCUMENT_WINDOW_LOOK,
				B_NORMAL_WINDOW_FEEL,
				0),
		Wnd(wnd)
	{
	}

	~LWindowPrivate()
	{
		#if 0
		printf("%p::~LWindowPrivate start, locking=%i, cur=%i, thread=%i\n",
			this,
			LockingThread(),
			LCurrentThreadId(),
			Thread());
		#endif
	
		LAssert(ModalChild == NULL);
		DeleteObj(Wnd->Menu);
		if (Thread() >= 0 && IsMinimized())
			Wnd->_PrevZoom = LZoomMin;
		Wnd->d = NULL;

		Lock();

		// printf("%p::~LWindowPrivate end\n", this);
	}

	bool HasThread()
	{
		return Thread() > 0;
	}
	
	window_look DefaultLook()
	{
		if (ShowTitleBar)
			return B_DOCUMENT_WINDOW_LOOK;
		else
			return B_NO_BORDER_WINDOW_LOOK;
	}

	int GetHookIndex(LView *Target, bool Create = false)
	{
		for (int i=0; i<Hooks.Length(); i++)
		{
			if (Hooks[i].Target == Target)
				return i;
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

	void FrameMoved(BPoint newPosition)
	{
		auto Pos = Frame();

		if (Pos != Wnd->Pos)
		{
			Wnd->Pos = Pos;
			Wnd->OnPosChange();
		}

		BWindow::FrameMoved(newPosition);
	}

	void FrameResized(float newWidth, float newHeight)
	{
		auto Pos = Frame();
		if (Pos != Wnd->Pos)
		{
			Wnd->Pos.SetSize(newWidth, newHeight);
			Wnd->OnPosChange();
		}
		BWindow::FrameResized(newWidth, newHeight);
	}

	bool QuitRequested()
	{
		/*
		printf("%p::QuitRequested() starting.. pos=%s cls=%s\n",
			Wnd,
			Wnd->Pos.GetStr(),
			Wnd->GetClass());
		*/
			
		auto r = Wnd->OnRequestClose(false);
		// printf("%s::QuitRequested()=%i\n", Wnd->GetClass(), r);
		return r;
	}

	void MessageReceived(BMessage *message)
	{
		if (message->what == M_ON_CREATE)
			LOG("%s:%i %s msg=M_ON_CREATE\n", _FL, __FUNCTION__);
		if (message->what == M_LWINDOW_DELETE)
		{
			// printf("Processing M_LWINDOW_DELETE th=%u\n", LCurrentThreadId());
			Wnd->Handle()->RemoveSelf();			
			Quit();
			// printf("Processed M_LWINDOW_DELETE\n");
		}
		else
		{
			BWindow::MessageReceived(message);

			LViewI *view = NULL;
			auto r = message->FindPointer(LMessage::PropView, (void**)&view);
			if (r == B_OK)
			{
				if (LView::RecentlyDeleted(view))
					LOG("%s:%i %s view is RecentlyDeleted\n", _FL, __FUNCTION__, r);
				else
				{
					if (message->what == M_ON_CREATE)
						LOG("%s:%i %s passing msg to view: %s\n", _FL, __FUNCTION__, r, view->GetClass());
					view->OnEvent((LMessage*)message);
				}
			}
			else
			{
				Wnd->OnEvent((LMessage*)message);
			}
		}
	}
	
	void WindowActivated(bool focus)
	{
		// printf("%s::WindowActivated %i\n", Wnd->GetClass(), focus);

		if (!ThreadMsgDone)
		{
			ThreadMsgDone = true;
			
			LString n;
			n.Printf("%s/%s", Wnd->GetClass(), Wnd->Name());
			LThread::RegisterThread(Thread(), n);
		}
		
		if (ModalChild && focus)
		{
			printf("%s:%i - %s dropping activate, has modal: %s.\n", _FL,
				Wnd->GetClass(),
				ModalChild->GetClass());
				
			SendBehind(ModalChild->WindowHandle());
			
			auto w = Wnd;
			while (auto p = w->GetModalParent())
			{
				p->WindowHandle()->SendBehind(w->WindowHandle());
				w = p;
			}
		}
		else
		{
			BWindow::WindowActivated(focus);
		}
	}
};

///////////////////////////////////////////////////////////////////////
LWindow::LWindow() : LView(0)
{
	d = new LWindowPrivate(this);
	_QuitOnClose = false;
	Menu = NULL;
	
	_Default = 0;
	_Window = this;
	ClearFlag(WndFlags, GWF_VISIBLE);
}

LWindow::~LWindow()
{
	if (LAppInst->AppWnd == this)
		LAppInst->AppWnd = NULL;

	DeleteObj(Menu);
	WaitThread();
}

LWindow *LWindow::GetModalParent()
{
	return d->ModalParent;
}

bool LWindow::SetModalParent(LWindow *p)
{
	if (p)
	{
		if (d->ModalParent ||
			p->GetModalChild())
		{
			printf("%s:%i - this=%s p=%s d->ModalParent=%s p->GetModalChild()=%s\n",
				_FL,
				GetClass(),
				p?p->GetClass():NULL,
				d->ModalParent?d->ModalParent->GetClass():NULL,
				p->GetModalChild()?p->GetModalChild()->GetClass():NULL);
			// LAssert(!"Already set!");
			return false;
		}
		
		d->ModalParent = p;
		p->d->ModalChild = this;
	}
	else
	{
		if (d->ModalParent)
		{
			if (d->ModalParent->GetModalChild() != this)
			{
				LAssert(!"Wrong linkage.");
				return false;
			}
			d->ModalParent->d->ModalChild = NULL;
			d->ModalParent = NULL;
		}
	}
	
	return true;
}

LWindow *LWindow::GetModalChild()
{
	return d->ModalChild;
}

bool LWindow::SetModalChild(LWindow *c)
{
	if (c)
	{
		if (d->ModalChild ||
			c->GetModalParent())
		{
			LAssert(!"Already set.");
			return false;
		}
		
		d->ModalChild = c;
		c->d->ModalParent = this;
	}
	else
	{
		if (d->ModalChild)
		{
			if (d->ModalChild->GetModalParent() != this)
			{
				LAssert(!"Wrong linkage.");
				return false;
			}
			d->ModalChild->d->ModalParent = NULL;
			d->ModalChild = NULL;
		}
	}
	
	return true;
}

int LWindow::WaitThread()
{
	if (!d)
		return -1;

	thread_id id = d->Thread();
	bool thisThread = id == LCurrentThreadId();

	WAIT_LOG("%s::~LWindow thread=%u lock=%u\n", Name(), LCurrentThreadId(), d->LockingThread());
	if (thisThread)
	{
		// We are in thread... can delete easily.
		if (d->Lock())
		{
			// printf("%s::~LWindow Quiting\n", Name());
			Handle()->RemoveSelf();
			d->Quit();
			// printf("%s::~LWindow Quit finished\n", Name());
		}

		DeleteObj(d);
		return 0; // If we're in thread... no need to wait.
	}

	status_t value = B_OK;
	if (d->Thread() >= 0)
	{
		// Post event to the window's thread to delete itself...
		WAIT_LOG("%s::~LWindow posting M_LWINDOW_DELETE from th=%u\n", Name(), LCurrentThreadId());
		d->PostMessage(new BMessage(M_LWINDOW_DELETE));

		WAIT_LOG("wait_for_thread(%u) start..\n", id);
		wait_for_thread(id, &value);
		WAIT_LOG("wait_for_thread(%u) end=%i\n", id, value);
	}
	else printf("%s:%i - No thread to wait for: %s\n", _FL, GetClass());
	
	DeleteObj(d);

	return value;
}

OsWindow LWindow::WindowHandle()
{
	return d;
}

bool LWindow::SetTitleBar(bool ShowTitleBar)
{
	if (!d)
		return false;

	d->ShowTitleBar = ShowTitleBar;

	LLocker lck(d, _FL);
	auto r = d->SetLook(d->DefaultLook());
	if (r)
		printf("%s:%i - SetFeel failed: %i\n", _FL, r);
	
	return r == B_OK;
}

bool LWindow::SetIcon(const char *FileName)
{
	LString a;
	if (!LFileExists(FileName))
	{
		if (a = LFindFile(FileName))
			FileName = a;
	}
	
	return false;
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
	LLocker lck(d, _FL);
	if (!lck.Lock())
		return false;
	return d->IsActive();
}

bool LWindow::SetActive()
{
	LLocker lck(d, _FL);
	if (!lck.Lock())
		return false;
	d->Activate();
	return true;
}

bool LWindow::Visible()
{
	LLocker lck(d, _FL);
	if (!lck.WaitForLock())
		return false;	
	return !d->IsHidden();
}

void LWindow::Visible(bool i)
{
	LLocker lck(d, _FL);
	if (!lck.WaitForLock())
	{
		printf("%s:%i - Can't lock.\n", _FL);
		return;
	}
	
	if (i)
	{
		if (d->IsHidden())
		{
			d->MoveTo(Pos.x1, Pos.y1);
			d->ResizeTo(Pos.X(), Pos.Y());
			d->Show();
		}
		else
			printf("%s already shown\n", GetClass());
	}
	else
	{
		if (!d->IsHidden())
			d->Hide();
	}
}

bool LWindow::Obscured()
{
	return	false;
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

void LWindow::UpdateRootView()
{
	auto rootView = Handle();
	if (!rootView)
	{
		printf("%s:%i - Can't update root view: no view handle.\n", _FL);
		return;
	}

	auto wnd = WindowHandle();
	auto looper = rootView->Looper();
	auto isLocked = looper->IsLocked();
	if (!isLocked)
	{
		printf("%s:%i - Can't update root view: not locked.\n", _FL);
		return;
	}

	if (rootView->IsHidden())
	{
		printf("%s::UpdateRootView() curThread=%i/%s wndThread=%i/%s\n",
			GetClass(),
			LCurrentThreadId(), LThread::GetThreadName(LCurrentThreadId()),
			wnd->Thread(), LThread::GetThreadName(wnd->Thread())
			);

		rootView->Show();
	}
	
	auto menu = wnd->KeyMenuBar();
	BRect menuPos = menu ? menu->Frame() : BRect(0, 0, 0, 0);
	
	auto f = wnd->Frame();
	rootView->ResizeTo(f.Width(), f.Height() - menuPos.Height());
	if (menu)
		rootView->MoveTo(0, menuPos.Height());
	rootView->SetResizingMode(B_FOLLOW_ALL_SIDES);
}

bool LWindow::Attach(LViewI *p)
{
	LLocker lck(d, _FL);
	if (!lck.Lock())
	{
		LOG("%s:%i error: failed to lock (%s)\n", _FL, GetClass());
		return false;
	}

	auto rootView = Handle();
	auto wnd = WindowHandle();
	if (rootView && wnd)
	{
		LOG("%s:%i attach %p to %p\n", _FL, rootView, wnd);
		wnd->AddChild(rootView);
		UpdateRootView();
	}
	else
	{
		LOG("%s:%i can't attach %p to %p\n", _FL, rootView, wnd);
	}
	
	// Setup default button...
	if (!_Default)
		_Default = FindControl(IDOK);

	// Do a rough layout of child windows
	PourAll();

	// Start the thread running...
	if (!d->HasThread())
	{
		d->Run();
		LOG("%s:%i run thread: %i\n", _FL, d->Thread());
	}
	else
	{
		LOG("%s:%i thread already running: %i\n", _FL, d->Thread());
	}
	if (d->HasThread())
	{
		for (auto msg: LView::d->MsgQue)
		{
			auto result = d->PostMessage(msg);
			LOG("%s:%i posting queued message %i = %i\n", _FL, ((LMessage*)msg)->Msg(), result);
		}
		LView::d->MsgQue.DeleteObjects();
	}

	return true;
}

bool LWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
		LCloseApp();

	return LView::OnRequestClose(OsShuttingDown);
}

bool LWindow::HandleViewMouse(LView *v, LMouse &m)
{
	if (d->ModalChild)
	{
		/*
		printf("%s:%i - %s ignoring mouse event while %s is shown.\n",
			_FL,
			GetClass(),
			d->ModalDlg->GetClass());
		*/
		return false;
	}

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
		LgiTrace("%s/%p::HandleViewKey=%i ischar=%i %s%s%s%s\n",
			v->GetClass(), v,
			k.c16,
			k.IsChar,
			(char*)(k.Down()?" Down":" Up"),
			(char*)(k.Shift()?" Shift":""),
			(char*)(k.Alt()?" Alt":""),
			(char*)(k.Ctrl()?" Ctrl":""));
	}
	#endif

	if (d->ModalChild)
	{
		/*
		printf("%s:%i - %s ignoring key event while %s is shown.\n",
			_FL,
			GetClass(),
			d->ModalChild->GetClass());
		*/
		return false;
	}

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
	return Status;
}


void LWindow::Raise()
{
}

LWindowZoom LWindow::GetZoom()
{
	if (!d)
		return _PrevZoom;

	LLocker lck(d, _FL);
	if (!IsAttached() || lck.Lock())
	{
		if (d->IsMinimized())
			return LZoomMin;
	}

	return LZoomNormal;
}

void LWindow::SetZoom(LWindowZoom i)
{
	LLocker lck(d, _FL);
	if (lck.Lock())
	{
		d->Minimize(i == LZoomMin);
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

bool LWindow::Name(const char *n)
{
	LLocker lck(d, _FL);
	if (lck.Lock())
		d->SetTitle(n);

	return LBase::Name(n);
}

const char *LWindow::Name()
{
	return LBase::Name();
}

LPoint LWindow::GetDpi() const
{
	return LPoint(96, 96);
}

LPointF LWindow::GetDpiScale()
{
	auto Dpi = GetDpi();
	return LPointF((double)Dpi.x/96.0, (double)Dpi.y/96.0);
}

LRect &LWindow::GetClient(bool ClientSpace)
{
	static LRect r;
	
	r = Pos.ZeroTranslate();

	LLocker lck(WindowHandle(), _FL);
	if (lck.Lock())
	{
		LRect br = Handle()->Bounds();
		if (br.Valid())
		{
			r = br;
		
			auto frm = d->Frame();
			frm.OffsetBy(-frm.left, -frm.top);
			// printf("%s:%i - LWindow::GetClient: Frame=%s Bounds=%s r=%s\n", _FL, ToString(frm).Get(), br.GetStr(), r.GetStr());
		}
		// else printf("%s:%i - LWindow::GetClient: Bounds not valid.\n", _FL);
		
		lck.Unlock();
	}
	// else printf("%s:%i - LWindow::GetClient: no lock.\n", _FL);
	
	return r;
}

#if DEBUG_SERIALIZE_STATE
#define SERIALIZE_LOG(...) printf(__VA_ARGS__)
#else
#define SERIALIZE_LOG(...)
#endif

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

			auto vars = LString(v.Str()).SplitDelimit(";");
			SERIALIZE_LOG("SerializeState: %s=%s, vars=%i\n", FieldName, v.Str(), (int)vars.Length());
			for (auto var: vars)
			{
				auto parts = var.SplitDelimit("=", 1);
				SERIALIZE_LOG("SerializeState: parts=%i\n", (int)parts.Length());
				if (parts.Length() == 2)
				{
					if (parts[0].Equals("State"))
					{
						State = (LWindowZoom) parts[1].Int();
						SERIALIZE_LOG("SerializeState: part=%s state=%i\n", parts[0].Get(), State);
					}
					else if (parts[0].Equals("Pos"))
					{
						Position.SetStr(parts[1]);
						SERIALIZE_LOG("SerializeState: part=%s pos=%s\n", parts[0].Get(), Position.GetStr());
					}
				}
			}
			
			if (Position.Valid())
			{
				SERIALIZE_LOG("SerializeState setpos %s\n", Position.GetStr());
				SetPos(Position);
			}
			
			SetZoom(State);
		}
		else
		{
			SERIALIZE_LOG("SerializeState: no '%s' var\n", FieldName);
			return false;
		}
	}
	else
	{
		char s[256];
		LWindowZoom State = GetZoom();
		sprintf_s(s, sizeof(s), "State=%i;Pos=%s", State, GetPos().GetStr());

		LVariant v = s;
		SERIALIZE_LOG("SerializeState: saving '%s' = '%s'\n", FieldName, s);
		if (!Store->SetValue(FieldName, v))
		{
			SERIALIZE_LOG("SerializeState: SetValue failed\n");
			return false;
		}
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

	LLocker lck(d, _FL);
	if (lck.Lock())
	{
		d->MoveTo(Pos.x1, Pos.y1);
		d->ResizeTo(Pos.X(), Pos.Y());
	}
	else printf("%s:%i - Failed to lock.\n", _FL);

	return true;
}

void LWindow::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	// Force repour
	Invalidate();
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
	LLocker lck(WindowHandle(), _FL);
	if (lck.Lock())
	{
		auto frame = WindowHandle()->Bounds();
		auto menu = WindowHandle()->KeyMenuBar();
		auto menuPos = menu ? menu->Frame() : BRect(0, 0, 0, 0);
		auto rootPos = Handle()->Frame();
		if (menu)
		{
			if (menu->IsHidden()) // Why?
			{
				menu->Show();
				if (menu->IsHidden())
				{
					// printf("Can't show menu?\n");
					for (auto p = menu->Parent(); p; p = p->Parent())
						printf("   par=%s %i\n", p->Name(), p->IsHidden());
				}
			}			
			if (menuPos.Width() < 1) // Again... WHHHHY? FFS
			{
				float x = 0.0f, y = 0.0f;
				menu->GetPreferredSize(&x, &y);
				// printf("Pref=%g,%g\n", x, y);
				if (y > 0.0f)
					menu->ResizeTo(frame.Width(), y);
			}
		}	
		#if 0
		printf("frame=%s menu=%p,%i,%s rootpos=%s\n",
			ToString(frame).Get(), menu, menu?menu->IsHidden():0, ToString(menuPos).Get(), ToString(rootPos).Get());
		#endif
		int rootTop = menu ? menuPos.bottom + 1 : 0;
		if (rootPos.top != rootTop)
		{
			Handle()->MoveTo(0, rootTop);
			Handle()->ResizeTo(rootPos.Width(), frame.Height() - menuPos.Height());
		}
	
		lck.Unlock();		
	}

	LView::OnPosChange();
	PourAll();
}

#define IsTool(v) \
	( \
		dynamic_cast<LView*>(v) \
		&& \
		dynamic_cast<LView*>(v)->_IsToolBar \
	)

void LWindow::PourAll()
{
	LRect c = GetClient();
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

bool LWindow::SetWillFocus(bool f)
{
	if (!d)
		return false;
		
	LLocker lck(d, _FL);
	auto flags = d->Flags();
	if (f)
		flags &= ~B_AVOID_FOCUS; // clear flag
	else
		flags |= B_AVOID_FOCUS; // set flag
	auto r = d->SetFlags(flags);
	if (r)
		printf("%s:%i - SetFlags failed: %i\n", _FL, r);

	return true;
}

LViewI *LWindow::GetFocus()
{
	if (!d)
		return NULL;
		
	LLocker lck(d, _FL);
	if (!lck.WaitForLock())
		return NULL;

	auto f = d->CurrentFocus();	
	return LViewFromHandle(f);
}

void LWindow::SetFocus(LViewI *ctrl, FocusType type)
{
	if (!ctrl)
		return;
		
	LLocker lck(d, _FL);
	if (lck.WaitForLock())
	{
		auto h = ctrl->Handle();
		h->MakeFocus(type == GainFocus ? true : false);
	}
}

void LWindow::SetDragHandlers(bool On)
{
}

void LWindow::OnTrayClick(LMouse &m)
{
	if (m.Down() || m.IsContextMenu())
	{
		LSubMenu RClick;
		OnTrayMenu(RClick);
		if (GetMouse(m, true))
		{
			int Result = RClick.Float(this, m.x, m.y);
			OnTrayMenuResult(Result);
		}
	}
}

void LWindow::SetAlwaysOnTop(bool b)
{
	if (!d)
		return;

	LLocker lck(d, _FL);
	status_t r;
	
	if (b)
		r = d->SetFeel(B_FLOATING_APP_WINDOW_FEEL);
	else
		r = d->SetFeel(B_NORMAL_WINDOW_FEEL);
	if (r)
		printf("%s:%i - SetFeel failed: %i\n", _FL, r);
}
