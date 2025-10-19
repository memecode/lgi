/// \file
/// \author Matthew Allen <fret@memecode.com>
#ifdef LINUX
#include <unistd.h>
#endif

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/Button.h"
#include "lgi/common/Css.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/EventTargetThread.h"
#include "lgi/common/Popup.h"
#include "lgi/common/CssTools.h"

#include "ViewPriv.h"

#if 0
#define DEBUG_CAPTURE(...) printf(__VA_ARGS__)
#else
#define DEBUG_CAPTURE(...)
#endif

//////////////////////////////////////////////////////////////////////////////////////
// Helper
LPoint lgi_view_offset(LViewI *v, bool Debug = false)
{
	LPoint Offset;
	
	for (LViewI *p = v; p; p = p->GetParent())
	{
		if (dynamic_cast<LWindow*>(p))
			break;
		
		LRect pos = p->GetPos();
		LRect cli = p->GetClient(false);

		if (Debug)
		{
			const char *cls = p->GetClass();
			LgiTrace("	Off[%s] += %i,%i = %i,%i (%s)\n",
					cls,
					pos.x1, pos.y1,
					Offset.x + pos.x1, Offset.y + pos.y1,
					cli.GetStr());
		}
		
		Offset.x += pos.x1 + cli.x1;
		Offset.y += pos.y1 + cli.y1;
	}
	
	return Offset;
}

LMouse &lgi_adjust_click(LMouse &Info, LViewI *Wnd, bool Capturing, bool Debug)
{
	static LMouse Temp;
	 
	Temp = Info;
	if (Wnd)
	{
		if (Debug
			#if 0
			|| Capturing
			#endif
			)
			LgiTrace("AdjustClick Target=%s -> Wnd=%s, Info=%i,%i\n",
				Info.Target?Info.Target->GetClass():"",
				Wnd?Wnd->GetClass():"",
				Info.x, Info.y);

		if (Temp.Target != Wnd)
		{
			if (Temp.Target)
			{
				auto *TargetWnd = Temp.Target->GetWindow();
				auto *WndWnd = Wnd->GetWindow();
				if (TargetWnd == WndWnd)
				{
					LPoint TargetOff = lgi_view_offset(Temp.Target, Debug);
					if (Debug)
						LgiTrace("	WndOffset:\n");
					LPoint WndOffset = lgi_view_offset(Wnd, Debug);

					Temp.x += TargetOff.x - WndOffset.x;
					Temp.y += TargetOff.y - WndOffset.y;

					#if 0
					LRect c = Wnd->GetClient(false);
					Temp.x -= c.x1;
					Temp.y -= c.y1;
					if (Debug)
						LgiTrace("	CliOff -= %i,%i\n", c.x1, c.y1);
					#endif

					Temp.Target = Wnd;
				}
			}
			else
			{
				LPoint Offset;
				Temp.Target = Wnd;
				if (Wnd->WindowVirtualOffset(&Offset))
				{
					LRect c = Wnd->GetClient(false);
					Temp.x -= Offset.x + c.x1;
					Temp.y -= Offset.y + c.y1;
				}
			}
		}
	}
	
	LAssert(Temp.Target != NULL);
	
	return Temp;
}

//////////////////////////////////////////////////////////////////////////////////////
// LView class methods
LViewI *LView::_Capturing = 0;
LViewI *LView::_Over = 0;

#if LGI_VIEW_HASH

struct ViewTbl : public LMutex
{
	typedef LHashTbl<PtrKey<LViewI*>, int> T;
	
private:
	T Map;

public:
	ViewTbl() : Map(2000), LMutex("ViewTbl")
	{
	}

	T *Lock()
	{
		if (!LMutex::Lock(_FL))
			return NULL;
		return &Map;
	}
}	ViewTblInst;

bool LView::LockHandler(LViewI *v, LView::LockOp Op)
{
	ViewTbl::T *m = ViewTblInst.Lock();
	if (!m)
		return false;
	int Ref = m->Find(v);
	bool Status = false;
	switch (Op)
	{
		case OpCreate:
		{
			if (Ref == 0)
				Status = m->Add(v, 1);
			else
				LAssert(!"Already exists?");
			break;
		}
		case OpDelete:
		{
			if (Ref == 1)
				Status = m->Delete(v);
			else
				LAssert(!"Either locked or missing.");
			break;
		}
		case OpExists:
		{
			Status = Ref > 0;
			break;
		}
	}	
	ViewTblInst.Unlock();
	return Status;
}

#endif

#if defined(HAIKU) || defined(MAC)
class LDeletedViews : public LMutex
{
	struct DeletedView
	{
		uint64_t ts;
		LViewI *v;
	};
	LArray<DeletedView> views;

public:
	LDeletedViews() : LMutex("LDeletedViews")
	{

	}

	void OnCreate(LViewI *v)
	{
		// Haiku:
		// It is not uncommon for the same pointer to be added twice.
		// The allocator often reuses the same address when a view is
		// deleted and a new one created. In that case, make sure a
		// newly created (and valid) pointer is NOT in the list...
		for (size_t i = 0; i < views.Length(); i++)
		{
			auto &del = views[i];
			if (v == del.v)
			{
				views.DeleteAt(i--);
				break;
			}
		}
	}

	void OnDelete(LViewI *v)
	{
		if (!Lock(_FL))
			return;

		auto &del = views.New();
		del.v = v;
		del.ts = LCurrentTime();

		Unlock();
	}

	bool Has(LViewI *v)
	{
		if (!Lock(_FL))
			return false;

		auto now = LCurrentTime();
		bool status = false;
		for (size_t i = 0; i < views.Length(); i++)
		{
			auto &del = views[i];
			if (v == del.v)
			{
				status = true;
			}
			if (now >= del.ts + 60000)
			{
				// printf("	DeletedViews remove=%p\n", views[i].v);
				views.DeleteAt(i--);
			}
		}
		
		Unlock();
		return status;
	}

}	DeletedViews;

bool LView::RecentlyDeleted(LViewI *v)
{
	return DeletedViews.Has(v);
}
#endif

LView::LView(OsView view) :
	_Margin(0, 0, 0, 0),
	_Border(0, 0, 0, 0)
{
	#ifdef _DEBUG
    _Debug = false;
	#endif
	#if defined(HAIKU) || defined(MAC)
		DeletedViews.OnCreate(static_cast<LViewI*>(this));
	#endif

	d = new LViewPrivate(this);
	#ifdef LGI_SDL
	_View = this;
	#elif LGI_VIEW_HANDLE && !defined(HAIKU)
	_View = view;
	#endif
	Pos.ZOff(-1, -1);
	WndFlags = GWF_VISIBLE;

    #ifndef LGI_VIEW_HASH
        #error "LGI_VIEW_HASH needs to be defined"
    #elif LGI_VIEW_HASH
	    LockHandler(this, OpCreate);
	    // printf("Adding %p to hash\n", (LViewI*)this);
	#endif
}

LView::~LView()
{
	#if defined(HAIKU) || defined(MAC)
		DeletedViews.OnDelete(static_cast<LViewI*>(this));
	#endif

	if (d->SinkHnd >= 0)
	{
		LEventSinkMap::Dispatch.RemoveSink(this);
		d->SinkHnd = -1;
	}
	
	#if LGI_VIEW_HASH
		LockHandler(this, OpDelete);
	#endif

	for (unsigned i=0; i<LPopup::CurrentPopups.Length(); i++)
	{
		LPopup *pu = LPopup::CurrentPopups[i];
		if (pu->Owner == this)
			pu->Owner = NULL;
	}

	_Delete();

	// printf("%p::~LView delete %p th=%u\n", this, d, LCurrentThreadId());
	DeleteObj(d);	
	// printf("%p::~LView\n", this);
}

bool LView::CommonEvents(LMessage::Result &result, LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_VIEW_RUN_CALLBACK:
		{
			CbStore.Call((int)Msg->A());
			break;
		}
		case M_PULSE:
		{
			OnPulse();
			break;
		}
		case M_SET_CTRL_NAME:
		{
			LAutoPtr<LString> s((LString*)Msg->B());
			SetCtrlName((int)Msg->A(), s ? s->Get() : NULL);
			break;
		}
		case M_SET_CTRL_ENABLE:
		{
			SetCtrlEnabled((int)Msg->A(), Msg->B()!=0);
			break;
		}
		case M_SET_CTRL_VISIBLE:
		{
			SetCtrlVisible((int)Msg->A(), Msg->B()!=0);
			break;
		}
		default:
			return false;
	}
	
	return true;
}

int LView::AddDispatch()
{
	if (d->SinkHnd < 0)
		d->SinkHnd = LEventSinkMap::Dispatch.AddSink(this);
	return d->SinkHnd;
}

LString LView::CssStyles(const char *Set)
{
	if (Set)
	{
		d->Styles = Set;
		d->CssDirty = true;
	}
	return d->Styles;
}

LString::Array *LView::CssClasses()
{
	return &d->Classes;
}

LArray<LViewI*> LView::IterateViews()
{
	LArray<LViewI*> a;
	for (auto c: Children)
		a.Add(c);
	return a;
}

bool LView::AddView(LViewI *v, int Where)
{
	if (!v)
		return false;

	#ifdef HAIKU
	auto wasAttached = v->IsAttached();
	#endif
	
	LAssert(!Children.HasItem(v));
	if (!Children.Insert(v, Where))
		return false;
		
	auto gv = v->GetLView();
	if (gv && gv->_Window != _Window)
	{
		LAssert(!_InLock);
		gv->_Window = _Window;
	}
	v->SetParent(this);
	v->OnAttach();
	OnChildrenChanged(v, true);

	#ifdef HAIKU
	auto lv = v->GetLView();
	if (lv && !wasAttached && lv->IsAttached() && !lv->d->onCreateEvent)
	{
		lv->d->onCreateEvent = true;
		lv->OnCreate();
	}
	#endif

	return true;
}

bool LView::DelView(LViewI *v)
{
	if (!v)
		return false;

	bool Has = Children.HasItem(v);
	bool b = Children.Delete(v);
	if (Has)
		OnChildrenChanged(v, false);
	Has = Children.HasItem(v);
	LAssert(!Has);
	return b;
}

bool LView::HasView(LViewI *v)
{
	return Children.HasItem(v);
}

OsWindow LView::WindowHandle()
{
	auto w = GetWindow();
	return w ? w->WindowHandle() : nullptr;
}

LWindow *LView::GetWindow()
{
	if (!_Window)
	{
		// Walk up parent list and find someone who has a window
		auto *w = d->GetParent();
		for (; w; w = w->d ? w->d->GetParent() : NULL)
		{
			if (w->_Window)
			{
				LAssert(!_InLock);
				_Window = w->_Window;
				break;
			}
		}
	}
	
	#if 0
	// Check the window is the same thread as us
	if (Handle() && _Window)
	{
		auto ourBWnd = Handle() ? Handle()->Window() : NULL;
		auto ourThread = ourBWnd ? ourBWnd->Thread() : -1;
		if (ourThread >= 0)
		{
			auto wndBWnd = _Window->Handle() ? _Window->Handle()->Window() : NULL;
			auto wndThread = wndBWnd ? wndBWnd->Thread() : -1;
			if (ourThread != wndThread)
			{
				printf("WARNING: window thread mismatch!! cls=%s us=%i, wnd=%i\n", GetClass(), ourThread, wndThread);
			}
		}
	}
	#endif
		
	return dynamic_cast<LWindow*>(_Window);
}

bool LView::Lock(const char *file, int line, int TimeOut)
{
	#ifdef HAIKU

		bool Debug = false;
		
		auto wndHnd = WindowHandle();
		if (!wndHnd)
		{
			printf("%s:%i - can't lock, no window.\n", _FL);
			return false;
		}
		
		if (TimeOut >= 0)
		{
			auto r = wndHnd->LockLooperWithTimeout(TimeOut * 1000);
			if (r == B_OK)
			{
				if (_InLock == 0)
				{
					LAssert(_LockingThread < 0);
					_LockingThread = LCurrentThreadId();
				}
				else if (_LockingThread != LCurrentThreadId())
				{
					printf("%s:%i - relock in different thread, prev=%i/%s, cur=%i/%s\n",
						_FL,
						_LockingThread, LThread::GetThreadName(_LockingThread),
						LCurrentThreadId(), LThread::GetThreadName(LCurrentThreadId()));
				}
				_InLock++;
				if (Debug)
					printf("%s:%p - Lock() cnt=%i wndHnd=%p.\n", GetClass(), this, _InLock, wndHnd);
				return true;
			}
		
			printf("%s:%i - Lock(%i) failed with %x.\n", _FL, TimeOut, r);
			return false;
		}
	
		auto r = wndHnd->LockLooper();
		if (r)
		{
			if (_InLock == 0)
			{
				LAssert(_LockingThread < 0);
				_LockingThread = LCurrentThreadId();
			}
			else if (_LockingThread != LCurrentThreadId())
			{
				printf("%s:%i - relock in different thread, prev=%i/%s, cur=%i/%s\n",
					_FL,
					_LockingThread, LThread::GetThreadName(_LockingThread),
					LCurrentThreadId(), LThread::GetThreadName(LCurrentThreadId()));
			}
			_InLock++;
			
			if (Debug)
			{
				auto w = WindowHandle();
				printf("%s:%p - Lock() cnt=%i myThread=%i wndThread=%i.\n",
					GetClass(),
					this,
					_InLock,
					LCurrentThreadId(),
					w ? w->Thread() : -1);
			}
			return true;
		}
	
		if (Debug)
			printf("%s:%i - Lock(%s:%i) failed.\n", _FL, file, line);
			
		return false;
		
	#else
	
		if (!_Window)
			GetWindow();

		_InLock++;
		// LgiTrace("%s::%p Lock._InLock=%i %s:%i\n", GetClass(), this, _InLock, file, line);
		if (_Window && _Window->_Lock)
		{
			if (TimeOut < 0)
			{
				return _Window->_Lock->Lock(file, line);
			}
			else
			{
				return _Window->_Lock->LockWithTimeout(TimeOut, file, line);
			}
		}

		return true;
		
	#endif
}

void LView::Unlock()
{
	#ifdef HAIKU
	
		auto wndHnd = WindowHandle();
		if (!wndHnd)
		{
			printf("%s:%i - Unlock() error, no wndHnd.\n", _FL);
			return;
		}
	
		if (_InLock > 0)
		{
			if (_LockingThread != LCurrentThreadId())
			{
				printf("%s:%i - unlock in different thread, locker=%i/%s, unlocker=%i/%s\n",
					_FL,
					_LockingThread, LThread::GetThreadName(_LockingThread),
					LCurrentThreadId(), LThread::GetThreadName(LCurrentThreadId()));
			}
		
			// printf("%s:%p - Calling UnlockLooper: %i.\n", GetClass(), this, _InLock);
			wndHnd->UnlockLooper();
			_InLock--;
			// printf("%s:%p - UnlockLooper done: %i.\n", GetClass(), this, _InLock);
			
			if (_InLock <= 0)
				_LockingThread = -1;
		}
		else
		{
			printf("%s:%i - Unlock() without lock.\n", _FL);
		}
		
	#else
	
		if (_Window &&
			_Window->_Lock)
		{
			_Window->_Lock->Unlock();
		}
		_InLock--;
		// LgiTrace("%s::%p Unlock._InLock=%i\n", GetClass(), this, _InLock);
		
	#endif
}

void LView::OnMouseClick(LMouse &m)
{
}

void LView::OnMouseEnter(LMouse &m)
{
}

void LView::OnMouseExit(LMouse &m)
{
}

void LView::OnMouseMove(LMouse &m)
{
}

bool LView::OnMouseWheel(double Lines)
{
	return false;
}

bool LView::OnKey(LKey &k)
{
	return false;
}

void LView::OnAttach()
{
	List<LViewI>::I it = Children.begin();
	for (LViewI *v = *it; v; v = *++it)
	{
		if (!v->GetParent())
			v->SetParent(this);
	}

	#if 0 // defined __GTK_H__
	if (_View && !DropTarget())
	{
		// If one of our parents is drop capable we need to set a dest here
		LViewI *p;
		for (p = GetParent(); p; p = p->GetParent())
		{
			if (p->DropTarget())
			{
				break;
			}
		}
		if (p)
		{
			Gtk::gtk_drag_dest_set(	_View,
									(Gtk::GtkDestDefaults)0,
									NULL,
									0,
									Gtk::GDK_ACTION_DEFAULT);
			// printf("%s:%i - Drop dest for '%s'\n", _FL, GetClass());
		}
		else
		{
			Gtk::gtk_drag_dest_unset(_View);
			// printf("%s:%i - Not setting drop dest '%s'\n", _FL, GetClass());
		}
	}
	#endif
}

LRect &LView::GetMargin()
{
	if (!d->cssLayoutDone)
		CssLayout();
	return _Margin;
}

LRect &LView::GetBorder()
{
	if (!d->cssLayoutDone)
		CssLayout();
	return _Border;
}

bool LView::CssLayout(bool reCalculate)
{
	if (!d->cssLayoutDone || reCalculate)
	{
		LRect box;
		if (auto parent = GetParent())
			box = parent->GetClient();
		else
			box = Pos;

		d->cssLayoutDone = true;
		if (auto css = GetCss())
		{
			auto fnt = GetFont();
			auto border = css->Border();
			auto calcBorder = [&](int &out, LCss::BorderDef b, int box) {
					LCss::BorderDef *def = b ? &b : &border;
					out = def->ToPx(box, fnt);
				};
			calcBorder(_Border.x1, css->BorderLeft(), box.X());
			calcBorder(_Border.x2, css->BorderRight(), box.X());
			calcBorder(_Border.y1, css->BorderTop(), box.Y());
			calcBorder(_Border.y2, css->BorderBottom(), box.Y());

			auto margin = css->Margin();
			auto calcMargin = [&](int &out, LCss::Len b, int box) {
					LCss::Len *def = b ? &b : &margin;
					out = def->ToPx(box, fnt);
				};
			calcMargin(_Margin.x1, css->MarginLeft(), box.X());
			calcMargin(_Margin.x2, css->MarginRight(), box.X());
			calcMargin(_Margin.y1, css->MarginTop(), box.Y());
			calcMargin(_Margin.y2, css->MarginBottom(), box.Y());
		}
	}

	return true;
}

void LView::OnCreate()
{
}

void LView::OnDestroy()
{
}

void LView::OnFocus(bool f)
{
	// printf("%s::OnFocus(%i)\n", GetClass(), f);
}

void LView::OnPulse()
{
}

void LView::OnPosChange()
{
}

bool LView::OnRequestClose(bool OsShuttingDown)
{
	return true;
}

int LView::OnHitTest(int x, int y)
{
	return -1;
}

void LView::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
}

void LView::OnPaint(LSurface *pDC)
{
	auto c = GetClient();
	LCssTools Tools(this);
	Tools.PaintContent(pDC, c);
}

int LView::OnNotify(LViewI *Ctrl, const LNotification &Data)
{
	if (!Ctrl)
		return 0;

	if (Ctrl == (LViewI*)this && Data.Type == LNotifyActivate)
	{
		// Default activation is to focus the current control.
		Focus(true);
	}
	else if (d && d->Parent)
	{
		#if 0 // HAIKU
		// Don't let notifications blindly pass into other threads.
		auto bCur = Handle();
		auto bParent = d->Parent->Handle();
		if (bCur && bParent)
		{
			auto curThread = bCur->Looper() ? bCur->Looper()->Thread() : LCurrentThreadId();
			auto parThread = bParent->Looper() ? bParent->Looper()->Thread() : -1;
			if (curThread != parThread && parThread >= 0)
			{
				printf(	"OnNotify can't cross thread boundary!\n"
						"    this=%s/%s (thread=%i/%s)\n"
						"    parent=%s/%s (thread=%i/%s)\n",
						GetClass(), Name(), curThread, LThread::GetThreadName(curThread),
						d->Parent->GetClass(), d->Parent->Name(), parThread, LThread::GetThreadName(parThread));
				return 0;
			}
		}
		#endif

		// default behavior is just to pass the 
		// notification up to the parent

		// FIXME: eventually we need to call the 'LNotification' parent fn...
		return d->Parent->OnNotify(Ctrl, Data);
	}

	return 0;
}

int LView::OnCommand(int Cmd, int Event, OsView Wnd)
{
	return 0;
}

void LView::OnNcPaint(LSurface *pDC, LRect &r)
{
	int px = 0;
	if (_Border.x1 == _Border.x2 == _Border.y1 == _Border.y2)
		px = _Border.x1;

	if (px == 2)
	{
		LEdge e;
		if (Sunken())
			e = Focus() ? EdgeWin7FocusSunken : DefaultSunkenEdge;
		else
			e = DefaultRaisedEdge;

		#if WINNATIVE
		if (d->IsThemed)
			DrawThemeBorder(pDC, r);
		else
		#endif
			LWideBorder(pDC, r, e);
	}
	else if (px == 1)
	{
		LThinBorder(pDC, r, Sunken() ? DefaultSunkenEdge : DefaultRaisedEdge);
	}
}

#if LGI_COCOA || defined(__GTK_H__)

	/*
	uint64 nPaint = 0;
	uint64 PaintTime = 0;
	*/

	void LView::_Paint(LSurface *pDC, LPoint *Offset, LRect *Update)
	{
		/*
		uint64 StartTs = Update ? LCurrentTime() : 0;
		d->InPaint = true;
		*/

		// Create temp DC if needed...
		LAutoPtr<LSurface> Local;
		if (!pDC)
		{
			if (!Local.Reset(new LScreenDC(this)))
				return;
			pDC = Local;
		}

		#if 0
		// This is useful for coverage checking
		pDC->Colour(LColour(255, 0, 255));
		pDC->Rectangle();
		#endif

		// Non-Client drawing
		LRect r;
		if (Offset)
		{
			r = Pos;
			r.Offset(*Offset);
		}
		else
		{
			r = GetClient().ZeroTranslate();
		}

		pDC->SetClient(&r);
		LRect zr1 = r.ZeroTranslate(), zr2 = zr1;
		OnNcPaint(pDC, zr1);
		pDC->SetClient(NULL);
		if (zr2 != zr1)
		{
			r.x1 -= zr2.x1 - zr1.x1;
			r.y1 -= zr2.y1 - zr1.y1;
			r.x2 -= zr2.x2 - zr1.x2;
			r.y2 -= zr2.y2 - zr1.y2;
		}
		LPoint o(r.x1, r.y1); // Origin of client

		// Paint this view's contents...
		pDC->SetClient(&r);

		#if 0
		if (_Debug)
		{
			#if defined(__GTK_H__)
				Gtk::cairo_matrix_t matrix;
				cairo_get_matrix(pDC->Handle(), &matrix);

				double ex[4];
				cairo_clip_extents(pDC->Handle(), ex+0, ex+1, ex+2, ex+3);
				ex[0] += matrix.x0; ex[1] += matrix.y0; ex[2] += matrix.x0; ex[3] += matrix.y0;
				LgiTrace("%s::_Paint, r=%s, clip=%g,%g,%g,%g - %g,%g\n",
						GetClass(), r.GetStr(),
						ex[0], ex[1], ex[2], ex[3],
						matrix.x0, matrix.y0);
			#elif LGI_COCOA
				auto Ctx = pDC->Handle();
				CGAffineTransform t = CGContextGetCTM(Ctx);
				LRect cr = CGContextGetClipBoundingBox(Ctx);
				printf("%s::_Paint() pos=%s transform=%g,%g,%g,%g-%g,%g clip=%s r=%s\n",
						GetClass(),
						GetPos().GetStr(),
	  					t.a, t.b, t.c, t.d, t.tx, t.ty,
						cr.GetStr(),
						r.GetStr());
			#endif
		}
		#endif

		OnPaint(pDC);

		pDC->SetClient(NULL);

		// Paint all the children...
		for (auto i : Children)
		{
			LView *w = i->GetLView();
			if (w && w->Visible())
			{
				if (!w->Pos.Valid())
					continue;

				#if 0
				if (w->_Debug)
					LgiTrace("%s::_Paint %i,%i\n", w->GetClass(), o.x, o.y);
				#endif
				w->_Paint(pDC, &o);
			}
		}
	}

#else

	void LView::_Paint(LSurface *pDC, LPoint *Offset, LRect *Update)
	{
		// Create temp DC if needed...
		LAutoPtr<LSurface> Local;
		if (!pDC)
		{
			Local.Reset(new LScreenDC(this));
			pDC = Local;
			return;
		}
		if (!pDC)
		{
			printf("%s:%i - No context to draw in.\n", _FL);
			return;
		}

		#if 0
		// This is useful for coverage checking
		pDC->Colour(LColour(255, 0, 255));
		pDC->Rectangle();
		#endif

		bool HasClient = false;
		LRect r(0, 0, Pos.X()-1, Pos.Y()-1), Client;
		LPoint o;
		if (Offset)
			o = *Offset;

		#if WINNATIVE
		if (!_View)
		#endif
		{
			// Non-Client drawing
			Client = r;
			OnNcPaint(pDC, Client);
			HasClient = GetParent() && (Client != r);
			if (HasClient)
			{
				Client.Offset(o.x, o.y);
				pDC->SetClient(&Client);
			}
		}

		r.Offset(o.x, o.y);

		// Paint this view's contents
		if (Update)
		{
			LRect OldClip = pDC->ClipRgn();
			pDC->ClipRgn(Update);
			OnPaint(pDC);
			pDC->ClipRgn(OldClip.Valid() ? &OldClip : NULL);
		}
		else
		{
			OnPaint(pDC);
		}

		#if PAINT_VIRTUAL_CHILDREN
		// Paint any virtual children
		for (auto i : Children)
		{
			auto w = i->GetLView();
			if (w && w->Visible())
			{
				#if LGI_VIEW_HANDLE
				if (!w->Handle())
				#endif
				{
					LRect p = w->GetPos();
					p.Offset(o.x, o.y);
					if (HasClient)
						p.Offset(Client.x1 - r.x1, Client.y1 - r.y1);
					
					LPoint co(p.x1, p.y1);
					// LgiTrace("%s::_Paint %i,%i\n", w->GetClass(), p.x1, p.y1);
					pDC->SetClient(&p);
					w->_Paint(pDC, &co);
					pDC->SetClient(NULL);
				}
			}
		}
		#endif

		if (HasClient)
			pDC->SetClient(0);
	}

#endif

LViewI *LView::GetParent()
{
	ThreadCheck();
	return d ? d->Parent : NULL;
}

void LView::SetParent(LViewI *p)
{
	ThreadCheck();
	d->Parent = p ? p->GetLView() : NULL;
	d->ParentI = p;
	d->ParentI2 = p;
}

void LView::SendNotify(LNotification note)
{
	LViewI *n = d->Notify ? d->Notify : d->Parent;
	if (!n)
		return;

	if (
		#if LGI_VIEW_HANDLE && !defined(HAIKU)
		!_View ||
		#endif
		InThread())
	{
		n->OnNotify(this, note);
	}
	else
	{
		// We are not in the same thread as the target window. So we post a message
		// across to the view.
		if (GetId() <= 0)
		{
			// We are going to generate a control Id to help the receiver of the
			// M_CHANGE message find out view later on. The reason for doing this
			// instead of sending a pointer to the object, is that the object 
			// _could_ be deleted between the message being sent and being received.
			// Which would result in an invalid memory access on that object.
			LViewI *p = GetWindow();
			if (!p)
			{
				// No window? Find the top most parent we can...
				p = this;
				while (p->GetParent())
					p = p->GetParent();
			}
			if (p)
			{
				// Give the control a valid ID
				int i;

				for (i=10; i<1000; i++)
				{
					if (!p->FindControl(i))
					{
						printf("Giving the ctrl '%s' the id '%i' for SendNotify\n",
							GetClass(),
							i);
						SetId(i);
						break;
					}
				}
			}
			else
			{
				// Ok this is really bad... go random (better than nothing)
				SetId(5000 + LRand(2000));
			}
		}
			
        LAssert(GetId() > 0); // We must have a valid ctrl ID at this point, otherwise
								// the receiver will never be able to find our object.
            
        // printf("Post M_CHANGE %i %i\n", GetId(), Data);
        n->PostEvent(M_CHANGE, (LMessage::Param) GetId(), (LMessage::Param) new LNotification(note));
	}
}

LViewI *LView::GetNotify()
{
	ThreadCheck();
	return d->Notify;
}

void LView::SetNotify(LViewI *p)
{
	ThreadCheck();
	d->Notify = p;
}

#define ADJ_LEFT			1
#define ADJ_RIGHT			2
#define ADJ_UP				3
#define ADJ_DOWN			4

int IsAdjacent(LRect &a, LRect &b)
{
	if ( (a.x1 == b.x2 + 1) &&
		!(a.y1 > b.y2 || a.y2 < b.y1))
	{
		return ADJ_LEFT;
	}

	if ( (a.x2 == b.x1 - 1) &&
		!(a.y1 > b.y2 || a.y2 < b.y1))
	{
		return ADJ_RIGHT;
	}

	if ( (a.y1 == b.y2 + 1) &&
		!(a.x1 > b.x2 || a.x2 < b.x1))
	{
		return ADJ_UP;
	}

	if ( (a.y2 == b.y1 - 1) &&
		!(a.x1 > b.x2 || a.x2 < b.x1))
	{
		return ADJ_DOWN;
	}

	return 0;
}

LRect JoinAdjacent(LRect &a, LRect &b, int Adj)
{
	LRect t;

	switch (Adj)
	{
		case ADJ_LEFT:
		case ADJ_RIGHT:
		{
			t.y1 = MAX(a.y1, b.y1);
			t.y2 = MIN(a.y2, b.y2);
			t.x1 = MIN(a.x1, b.x1);
			t.x2 = MAX(a.x2, b.x2);
			break;
		}
		case ADJ_UP:
		case ADJ_DOWN:
		{
			t.y1 = MIN(a.y1, b.y1);
			t.y2 = MAX(a.y2, b.y2);
			t.x1 = MAX(a.x1, b.x1);
			t.x2 = MIN(a.x2, b.x2);
			break;
		}
	}

	return t;
}

LRect *LView::FindLargest(LRegion &r)
{
	ThreadCheck();

	int Pixels = 0;
	LRect *Best = 0;
	static LRect Final;

	// do initial run through the list to find largest single region
	for (LRect *i = r.First(); i; i = r.Next())
	{
		int Pix = i->X() * i->Y();
		if (Pix > Pixels)
		{
			Pixels = Pix;
			Best = i;
		}
	}

	if (Best)
	{
		Final = *Best;
		Pixels = Final.X() * Final.Y();

		int LastPixels = Pixels;
		LRect LastRgn = Final;
		int ThisPixels = Pixels;
		LRect ThisRgn = Final;

		LRegion TempList;
		for (LRect *i = r.First(); i; i = r.Next())
		{
			TempList.Union(i);
		}
		TempList.Subtract(Best);

		do
		{
			LastPixels = ThisPixels;
			LastRgn = ThisRgn;

			// search for adjoining rectangles that maybe we can add
			for (LRect *i = TempList.First(); i; i = TempList.Next())
			{
				int Adj = IsAdjacent(ThisRgn, *i);
				if (Adj)
				{
					LRect t = JoinAdjacent(ThisRgn, *i, Adj);
					int Pix = t.X() * t.Y();
					if (Pix > ThisPixels)
					{
						ThisPixels = Pix;
						ThisRgn = t;

						TempList.Subtract(i);
					}
				}
			}
		} while (LastPixels < ThisPixels);

		Final = ThisRgn;
	}
	else return 0;

	return &Final;
}

LRect *LView::FindSmallestFit(LRegion &r, int Sx, int Sy)
{
	ThreadCheck();

	int X = 1000000;
	int Y = 1000000;
	LRect *Best = 0;

	for (LRect *i = r.First(); i; i = r.Next())
	{
		if ((i->X() >= Sx && i->Y() >= Sy) &&
			(i->X() < X || i->Y() < Y))
		{
			X = i->X();
			Y = i->Y();
			Best = i;
		}
	}

	return Best;
}

LRect *LView::FindLargestEdge(LRegion &r, int Edge)
{
	LRect *Best = 0;
	ThreadCheck();

	for (LRect *i = r.First(); i; i = r.Next())
	{
		if (!Best)
		{
			Best = i;
		}

		if
		(
			((Edge & GV_EDGE_TOP) && (i->y1 < Best->y1))
			||
			((Edge & GV_EDGE_RIGHT) && (i->x2 > Best->x2))
			||
			((Edge & GV_EDGE_BOTTOM) && (i->y2 > Best->y2))
			||
			((Edge & GV_EDGE_LEFT) && (i->x1 < Best->x1))
		)
		{
			Best = i;
		}

		if
		(
			(
				((Edge & GV_EDGE_TOP) && (i->y1 == Best->y1))
				||
				((Edge & GV_EDGE_BOTTOM) && (i->y2 == Best->y2))
			)
			&&
			(
				i->X() > Best->X()
			)
		)
		{
			Best = i;
		}

		if
		(
			(
				((Edge & GV_EDGE_RIGHT) && (i->x2 == Best->x2))
				||
				((Edge & GV_EDGE_LEFT) && (i->x1 == Best->x1))
			)
			&&
			(
				i->Y() > Best->Y()
			)
		)
		{
			Best = i;
		}
	}

	return Best;
}

LViewI *LView::FindReal(LPoint *Offset)
{
	ThreadCheck();

	if (Offset)
	{
		Offset->x = 0;
		Offset->y = 0;
	}

	#if !LGI_VIEW_HANDLE
	LViewI *w = GetWindow();
	#endif
	LViewI *p = d->Parent;
	while (p &&
		#if !LGI_VIEW_HANDLE
		p != w
		#else
		!p->Handle()
		#endif
		)
	{
		if (Offset)
		{
			Offset->x += Pos.x1;
			Offset->y += Pos.y1;
		}

		p = p->GetParent();
	}
	
	if (p &&
		#if !LGI_VIEW_HANDLE
		p == w
		#else
		p->Handle()
		#endif
		)
	{
		return p;
	}

	return NULL;
}

bool LView::HandleCapture(LView *Wnd, bool c)
{
	ThreadCheck();
	
	DEBUG_CAPTURE("%s::HandleCapture(%i)=%i\n", GetClass(), c, (int)(_Capturing == Wnd));
	if (c)
	{
		if (_Capturing == Wnd)
		{
			DEBUG_CAPTURE("    %s already has capture\n", _Capturing?_Capturing->GetClass():0);
		}
		else
		{
			DEBUG_CAPTURE("    _Capturing=%s -> %s\n", _Capturing?_Capturing->GetClass():0, Wnd?Wnd->GetClass():0);
			_Capturing = Wnd;
			
			#if defined(__GTK_H__)
			
				if (d->CaptureThread)
					d->CaptureThread->Cancel();
				d->CaptureThread = new LCaptureThread(this);
			
			#elif WINNATIVE
				
				LPoint Offset;
				LViewI *v = _Capturing->Handle() ? _Capturing : FindReal(&Offset);
				HWND h = v ? v->Handle() : NULL;
				if (h)
					SetCapture(h);
				else
					LAssert(0);
					
			#elif defined(LGI_SDL)
				
				#if SDL_VERSION_ATLEAST(2, 0, 4)
					SDL_CaptureMouse(SDL_TRUE);
				#else
					LAppInst->CaptureMouse(true);
				#endif

			#endif
		}
	}
	else if (_Capturing)
	{
		DEBUG_CAPTURE("    _Capturing=%s -> NULL\n", _Capturing?_Capturing->GetClass():0);
		_Capturing = NULL;
		
		#if defined(__GTK_H__)
		
			if (d->CaptureThread)
			{
				d->CaptureThread->Cancel();
				d->CaptureThread = NULL; // It'll delete itself...
			}
		
		#elif WINNATIVE

			ReleaseCapture();

		#elif defined(LGI_SDL)

			#if SDL_VERSION_ATLEAST(2, 0, 4)
				SDL_CaptureMouse(SDL_FALSE);
			#else
				LAppInst->CaptureMouse(false);
			#endif

		#endif
	}

	return true;
}

bool LView::IsCapturing()
{
	// DEBUG_CAPTURE("%s::IsCapturing()=%p==%p==%i\n", GetClass(), _Capturing, this, (int)(_Capturing == this));
	return _Capturing == this;
}

bool LView::Capture(bool c)
{
	ThreadCheck();

	DEBUG_CAPTURE("%s::Capture(%i)\n", GetClass(), c);
	return HandleCapture(this, c);
}

bool LView::Enabled()
{
	ThreadCheck();

	#if WINNATIVE
	if (_View)
		return IsWindowEnabled(_View) != 0;
	#endif
	
	return !TestFlag(GViewFlags, GWF_DISABLED);
}

void LView::Enabled(bool i)
{
	ThreadCheck();

	if (!i) SetFlag(GViewFlags, GWF_DISABLED);
	else ClearFlag(GViewFlags, GWF_DISABLED);

	#if LGI_VIEW_HANDLE && !defined(HAIKU)
	if (_View)
	{
		#if WINNATIVE
		EnableWindow(_View, i);
		#elif defined LGI_CARBON
		if (i)
		{
			OSStatus e = EnableControl(_View);
			if (e) printf("%s:%i - Error enabling control (%i)\n", _FL, (int)e);
		}
		else
		{
			OSStatus e = DisableControl(_View);
			if (e) printf("%s:%i - Error disabling control (%i)\n", _FL, (int)e);
		}
		#endif
	}
	#endif

	Invalidate();
}

bool LView::Visible()
{
	// This is a read only operation... which is kinda thread-safe...
	// ThreadCheck();

	#if WINNATIVE

	if (_View)

	/*	This takes into account all the parent windows as well...
		Which is kinda not what I want. I want this to reflect just
		this window.
		
		return IsWindowVisible(_View);
	*/
		return (GetWindowLong(_View, GWL_STYLE) & WS_VISIBLE) != 0;	
	
	#endif

	return TestFlag(GViewFlags, GWF_VISIBLE);
}

void LView::Visible(bool v)
{
	ThreadCheck();
	
	bool HasChanged = TestFlag(GViewFlags, GWF_VISIBLE) ^ v;
	if (v) SetFlag(GViewFlags, GWF_VISIBLE);
	else ClearFlag(GViewFlags, GWF_VISIBLE);

 	#if LGI_VIEW_HANDLE
		if (_View)
		{
			#if WINNATIVE

				ShowWindow(_View, (v) ? SW_SHOWNORMAL : SW_HIDE);

			#elif LGI_COCOA

				LAutoPool Pool;
				[_View.p setHidden:!v];

			#elif LGI_CARBON
			
				Boolean is = HIViewIsVisible(_View);
				if (v != is)
				{
					OSErr e = HIViewSetVisible(_View, v);
					if (e) printf("%s:%i - HIViewSetVisible(%p,%i) failed with %i (class=%s)\n",
									_FL, _View, v, e, GetClass());
				}
			
			#endif
		}
		else
	#endif
	{
		if (HasChanged)
			Invalidate();
	}
}

bool LView::Focus()
{
	ThreadCheck();

	bool Has = false;
	
	#if defined(__GTK_H__)
		LWindow *w = GetWindow();
		if (w)
		{
			bool Active = w->IsActive();
			if (Active)
				Has = w->GetFocus() == static_cast<LViewI*>(this);
		}
	#elif defined(WINNATIVE)
		if (_View)
		{
			HWND hFocus = GetFocus();
			Has = hFocus == _View;
		}
	#elif HAIKU || LGI_COCOA
		Has = TestFlag(WndFlags, GWF_FOCUS);
	#elif LGI_CARBON
		LWindow *w = GetWindow();
		if (w)
		{
			ControlRef Cur;
			OSErr e = GetKeyboardFocus(w->WindowHandle(), &Cur);
			if (e)
				LgiTrace("%s:%i - GetKeyboardFocus failed with %i\n", _FL, e);
			else
				Has = (Cur == _View);
		}
  	#endif

	#if !LGI_CARBON
		if (Has)
			SetFlag(WndFlags, GWF_FOCUS);
		else
			ClearFlag(WndFlags, GWF_FOCUS);
	#endif
	
	return Has;
}

void LView::Focus(bool i)
{
	ThreadCheck();

	if (i)
		SetFlag(WndFlags, GWF_FOCUS);
	else
		ClearFlag(WndFlags, GWF_FOCUS);

	auto *Wnd = GetWindow();
	if (Wnd)
	{
		Wnd->SetFocus(this, i ? LWindow::GainFocus : LWindow::LoseFocus);
	}

	#if LGI_VIEW_HANDLE && !defined(HAIKU)
	if (_View)
	#endif
	{
		#if defined(HAIKU)
		
			_Focus(i);
		
		#elif defined(LGI_SDL) || defined(__GTK_H__)
		
			// Nop: Focus is all handled by Lgi's LWindow class.
		
		#elif WINNATIVE

			if (i)
			{

				HWND hCur = GetFocus();
				if (hCur != _View)
				{
					if (In_SetWindowPos)
					{
						assert(0);
						LgiTrace("%s:%i - SetFocus %p (%-30s)\n", _FL, Handle(), Name());
					}

					SetFocus(_View);
				}
			}
			else
			{
				if (In_SetWindowPos)
				{
					assert(0);
					LgiTrace("%s:%i - SetFocus(%p)\n", _FL, GetDesktopWindow());
				}

				SetFocus(GetDesktopWindow());
			}

		#elif defined LGI_CARBON
		
			LViewI *Wnd = GetWindow();
			if (Wnd && i)
			{
				OSErr e = SetKeyboardFocus(Wnd->WindowHandle(), _View, 1);
				if (e)
				{
					// e = SetKeyboardFocus(Wnd->WindowHandle(), _View, kControlFocusNextPart);
					// if (e)
					{
						HIViewRef p = HIViewGetSuperview(_View);
						// errCouldntSetFocus
						printf("%s:%i - SetKeyboardFocus failed: %i (%s, %p)\n", _FL, e, GetClass(), p);
					}
				}
				// else printf("%s:%i - SetFocus v=%p(%s)\n", _FL, _View, GetClass());
			}
			else printf("%s:%i - no window?\n", _FL);
		
		#endif
	}
}

LDragDropSource *LView::DropSource(LDragDropSource *Set)
{
	if (Set)
		d->DropSource = Set;

	return d->DropSource;
}

LDragDropTarget *LView::DropTarget(LDragDropTarget *Set)
{
	if (Set)
		d->DropTarget = Set;

	return d->DropTarget;
}

#if defined LGI_CARBON
extern pascal OSStatus LgiViewDndHandler(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
#endif

#if defined __GTK_H__
// Recursively add drag dest to all view and all children
bool GtkAddDragDest(LViewI *v, bool IsTarget)
{
	if (!v) return false;
	LWindow *w = v->GetWindow();
	if (!w) return false;
	auto wid = GtkCast(w->WindowHandle(), gtk_widget, GtkWidget);

	if (IsTarget)
	{
		Gtk::gtk_drag_dest_set(	wid,
								(Gtk::GtkDestDefaults)0,
								NULL,
								0,
								Gtk::GDK_ACTION_DEFAULT);
	}
	else
	{
		Gtk::gtk_drag_dest_unset(wid);
	}
	
	for (LViewI *c: v->IterateViews())
		GtkAddDragDest(c, IsTarget);
	
	return true;
}
#endif

bool LView::DropTarget(bool t)
{
	ThreadCheck();

	bool Status = false;

	if (t) SetFlag(GViewFlags, GWF_DROP_TARGET);
	else ClearFlag(GViewFlags, GWF_DROP_TARGET);

	#if WINNATIVE
	
		if (_View)
		{
			if (t)
			{
				if (!d->DropTarget)
					DragAcceptFiles(_View, t);
				else
					Status = RegisterDragDrop(_View, (IDropTarget*) d->DropTarget) == S_OK;
			}
			else
			{
				if (_View && d->DropTarget)
					Status = RevokeDragDrop(_View) == S_OK;
			}
		}

	#elif defined MAC && !defined(LGI_SDL)

		auto Wnd = dynamic_cast<LWindow*>(GetWindow());
		if (Wnd)
		{
			Wnd->SetDragHandlers(t);
			if (!d->DropTarget)
				d->DropTarget = t ? Wnd : 0;
		}

		#if LGI_COCOA

			if (auto w = GetWindow())
			{
				if (auto h = w->WindowHandle())
				{
					if (auto a = [[NSMutableArray<NSString*> alloc] init])
					{
						// Receive generic file items:
						[a addObject:(NSString*)kUTTypeItem];
						
						// Receive file promises:
						for (id item in NSFilePromiseReceiver.readableDraggedTypes)
							[a addObject:item];
							
						if (auto source = DropSource())
						{
							// Receive things the target knows about:
							LDragFormats formats(true);
							if (source->GetFormats(formats))
							{
								for (auto f: formats.GetAll())
								{
									auto str = f.NsStr();
									[a addObject:str];
									[str release];
								}
							}
						}
						
						#if 0 // print the registered formats:
						printf("DropTarget for %s:\n", GetClass());
						for (id str: a)
							printf("	type:%s\n", [str UTF8String]);
						#endif
						
						[h.p.contentView registerForDraggedTypes:a];
						[a release];
					}
				}
			}

		#elif LGI_CARBON
	
			if (t)
			{
				static EventTypeSpec DragEvents[] =
				{
					{ kEventClassControl, kEventControlDragEnter },
					{ kEventClassControl, kEventControlDragWithin },
					{ kEventClassControl, kEventControlDragLeave },
					{ kEventClassControl, kEventControlDragReceive },
				};
				
				if (!d->DndHandler)
				{
					OSStatus e = ::InstallControlEventHandler(	_View,
																NewEventHandlerUPP(LgiViewDndHandler),
																GetEventTypeCount(DragEvents),
																DragEvents,
																(void*)this,
																&d->DndHandler);
					if (e) LgiTrace("%s:%i - InstallEventHandler failed (%i)\n", _FL, e);
				}
				SetControlDragTrackingEnabled(_View, true);
			}
			else
			{
				SetControlDragTrackingEnabled(_View, false);
			}
	
		#endif

	#elif defined __GTK_H__

		Status = GtkAddDragDest(this, t);
		if (Status && !d->DropTarget)
			d->DropTarget = t ? GetWindow() : 0;

	#endif

	return Status;
}

bool LView::Sunken()
{
	#if WINNATIVE
		return TestFlag(d->WndExStyle, WS_EX_CLIENTEDGE);
	#else
		return TestFlag(GViewFlags, GWF_SUNKEN);
	#endif
}

void LView::Sunken(bool i)
{
	ThreadCheck();

	#if WINNATIVE
		if (i) SetFlag(d->WndExStyle, WS_EX_CLIENTEDGE);
		else ClearFlag(d->WndExStyle, WS_EX_CLIENTEDGE);
		if (_View)
			SetWindowLong(_View, GWL_EXSTYLE, d->WndExStyle);
	#else
		if (i) SetFlag(GViewFlags, GWF_SUNKEN);
		else ClearFlag(GViewFlags, GWF_SUNKEN);
	#endif

	if (i)
	{
		if (!GetBorder().x1)
			_Border.Set(2, 2, 2, 2);
	}
	else _Border.ZOff(0, 0);
}

bool LView::Flat()
{
	#if WINNATIVE
		return	!TestFlag(d->WndExStyle, WS_EX_CLIENTEDGE) &&
				!TestFlag(d->WndExStyle, WS_EX_WINDOWEDGE);
	#else
		return !TestFlag(GViewFlags, GWF_SUNKEN) &&
			   !TestFlag(GViewFlags, GWF_RAISED);
	#endif
}

void LView::Flat(bool i)
{
	ThreadCheck();
	
	#if WINNATIVE
		ClearFlag(d->WndExStyle, (WS_EX_CLIENTEDGE|WS_EX_WINDOWEDGE));
	#else
		ClearFlag(GViewFlags, (GWF_RAISED|GWF_SUNKEN));
	#endif
}

bool LView::Raised()
{
	#if WINNATIVE
		return TestFlag(d->WndExStyle, WS_EX_WINDOWEDGE);
	#else
		return TestFlag(GViewFlags, GWF_RAISED);
	#endif
}

void LView::Raised(bool i)
{
	ThreadCheck();

	#if WINNATIVE
		if (i) SetFlag(d->WndExStyle, WS_EX_WINDOWEDGE);
		else ClearFlag(d->WndExStyle, WS_EX_WINDOWEDGE);
	#else
		if (i) SetFlag(GViewFlags, GWF_RAISED);
		else ClearFlag(GViewFlags, GWF_RAISED);
	#endif

	if (i)
	{
		if (!GetBorder().x1)
			_Border.Set(2, 2, 2, 2);
	}
	else _Border.ZOff(0, 0);
}

int LView::GetId() const
{
	// This is needed by SendNotify function which is thread safe.
	// So no thread safety check here.
	return d->CtrlId;
}

void LView::SetId(int i)
{
	// This is needed by SendNotify function which is thread safe.
	// So no thread safety check here.
	d->CtrlId = i;

	#if WINNATIVE
	if (_View)
		SetWindowLong(_View, GWL_ID, d->CtrlId);
	#elif defined __GTK_H__
	#elif defined MAC
	#endif
}

bool LView::GetTabStop()
{
	ThreadCheck();

	#if WINNATIVE
	return TestFlag(d->WndStyle, WS_TABSTOP);
	#else
	return d->TabStop;
	#endif
}

void LView::SetTabStop(bool b)
{
	ThreadCheck();

	#if WINNATIVE
		if (b)
			SetFlag(d->WndStyle, WS_TABSTOP);
		else
			ClearFlag(d->WndStyle, WS_TABSTOP);
	#else
		d->TabStop = b;
	#endif
}

int64 LView::GetCtrlValue(int Id)
{
	ThreadCheck();

	LViewI *w = FindControl(Id);
	return (w) ? w->Value() : 0;
}

void LView::SetCtrlValue(int Id, int64 i)
{
	ThreadCheck();

	LViewI *w = FindControl(Id);
	if (w) w->Value(i);
}

const char *LView::GetCtrlName(int Id)
{
	ThreadCheck();

	LViewI *w = FindControl(Id);
	return (w) ? w->Name() : 0;
}

void LView::SetCtrlName(int Id, const char *s)
{
	if (!IsAttached() || InThread())
	{
		if (auto w = FindControl(Id))
			w->Name(s);
		// else printf("LView::SetCtrlName: No ctrl %i.\n", Id);
	}
	else
	{
		// printf("LView::SetCtrlName: Sending M_SET_CTRL_NAME %i.\n", Id);
		PostEvent(	M_SET_CTRL_NAME,
					(LMessage::Param)Id,
					(LMessage::Param)new LString(s));
	}
}

bool LView::GetCtrlEnabled(int Id)
{
	ThreadCheck();

	LViewI *w = FindControl(Id);
	return (w) ? w->Enabled() : 0;
}

void LView::SetCtrlEnabled(int Id, bool Enabled)
{
	if (!IsAttached() || InThread())
	{
		if (auto w = FindControl(Id))
			w->Enabled(Enabled);
	}
	else
	{
		PostEvent(	M_SET_CTRL_ENABLE,
					(LMessage::Param)Id,
					(LMessage::Param)Enabled);
	}
}

bool LView::GetCtrlVisible(int Id)
{
	ThreadCheck();

	LViewI *w = FindControl(Id);
	if (!w)
		LgiTrace("%s:%i - Ctrl %i not found.\n", _FL, Id);
	return (w) ? w->Visible() : 0;
}

void LView::SetCtrlVisible(int Id, bool v)
{
	if (!IsAttached() || InThread())
	{
		if (auto w = FindControl(Id))
			w->Visible(v);
	}
	else
	{
		PostEvent(	M_SET_CTRL_VISIBLE,
					(LMessage::Param)Id,
					(LMessage::Param)v);
	}
}

bool LView::AttachChildren()
{
	for (auto c: Children)
	{
		if (c->IsAttached())
			continue;
		if (!c->Attach(this))
		{
			LgiTrace("%s:%i - failed to attach %s\n", _FL, c->GetClass());
			return false;
		}
	}

	return true;
}

LFont *LView::GetFont()
{
	if (!d->Font &&
		d->Css &&
		LResources::GetLoadStyles())
	{
		auto fc = LAppInst->GetFontCache();
		if (fc)
		{
			// printf("%s::GetFont from cache\n", GetClass());
			auto f = fc->GetFont(d->Css);
			if (f)
			{
				if (d->FontOwnType == GV_FontOwned)
					DeleteObj(d->Font);
				d->Font = f;
				d->FontOwnType = GV_FontCached;
			}
		}
	}
	
	return d->Font ? d->Font : LSysFont;
}

void LView::SetFont(LFont *Font, bool OwnIt)
{
	bool Change = d->Font != Font;
	if (Change)
	{
		if (d->FontOwnType == GV_FontOwned)
		{
			LAssert(d->Font != LSysFont);
			DeleteObj(d->Font);
		}

		d->FontOwnType = OwnIt ? GV_FontOwned : GV_FontPtr;
		d->Font = Font;

		#if WINNATIVE
		if (_View)
			SendMessage(_View, WM_SETFONT, (WPARAM) (Font ? Font->Handle() : 0), 0);
		#endif

		for (LViewI *p = GetParent(); Font && p; p = p->GetParent())
		{
			LTableLayout *Tl = dynamic_cast<LTableLayout*>(p);
			if (Tl)
			{
				Tl->InvalidateLayout();
				break;
			}
		}

		Invalidate();
	}
}

bool LView::IsOver(LMouse &m)
{
	return	(m.x >= 0) &&
			(m.y >= 0) &&
			(m.x < Pos.X()) &&
			(m.y < Pos.Y());
}

bool LView::WindowVirtualOffset(LPoint *Offset)
{
	bool Status = false;

	if (Offset)
	{
		Offset->x = 0;
		Offset->y = 0;
		
		for (LViewI *Wnd = this; Wnd; Wnd = Wnd->GetParent())
		{
			#if !LGI_VIEW_HANDLE
			auto IsWnd = dynamic_cast<LWindow*>(Wnd);
			if (!IsWnd)
			#else
			if (!Wnd->Handle())
			#endif
			{
				LRect r = Wnd->GetPos();
				LViewI *Par = Wnd->GetParent();
				if (Par)
				{
					LRect c = Par->GetClient(false);
					Offset->x += r.x1 + c.x1;
					Offset->y += r.y1 + c.y1;
				}
				else
				{
					Offset->x += r.x1;
					Offset->y += r.y1;
				}

				Status = true;
			}
			else break;
		}
	}

	return Status;
}

LString _ViewDesc(LViewI *v)
{
	LString s;
	s.Printf("%s/%s/%i", v->GetClass(), v->Name(), v->GetId());
	return s;
}

LViewI *LView::WindowFromPoint(int x, int y, int DebugDepth)
{
	char Tabs[64];
	if (DebugDepth)
	{
		memset(Tabs, 9, DebugDepth);
		Tabs[DebugDepth] = 0;
		LgiTrace("%s%s %i\n", Tabs, _ViewDesc(this).Get(), Children.Length());
	}

	// We iterate over the child in reverse order because if they overlap the
	// end of the list is on "top". So they should get the click or whatever
	// before the the lower windows.
	auto it = Children.rbegin();
	int n = (int)Children.Length() - 1;
	for (LViewI *c = *it; c; c = *--it)
	{
		LRect CPos = c->GetPos();
		
		if (CPos.Overlap(x, y) && c->Visible())
		{
			LRect CClient;
			CClient = c->GetClient(false);

            int Ox = CPos.x1 + CClient.x1;
            int Oy = CPos.y1 + CClient.y1;
			if (DebugDepth)
			{
				LgiTrace("%s[%i] %s Pos=%s Client=%s m(%i,%i)->(%i,%i)\n",
						Tabs, n--,
						_ViewDesc(c).Get(),
						CPos.GetStr(),
						CClient.GetStr(),
						x, y,
						x - Ox, y - Oy);
			}

			LViewI *Child = c->WindowFromPoint(x - Ox, y - Oy, DebugDepth ? DebugDepth  + 1 : 0);
			if (Child)
				return Child;
		}
		else if (DebugDepth)
		{
			LgiTrace("%s[%i] MISSED %s Pos=%s m(%i,%i)\n", Tabs, n--, _ViewDesc(c).Get(), CPos.GetStr(), x, y);
		}
	}

	if (x >= 0 && y >= 0 && x < Pos.X() && y < Pos.Y())
	{
		return this;
	}

	return NULL;
}

LColour LView::StyleColour(int CssPropType, LColour Default, int Depth)
{
	LColour c = Default;

	if ((CssPropType >> 8) == LCss::TypeColor)
	{
		LViewI *v = this;
		for (int i=0; v && i<Depth; i++, v = v->GetParent())
		{
			auto Style = v->GetCss();
			if (Style)
			{
				auto Colour = (LCss::ColorDef*) Style->PropAddress((LCss::PropType)CssPropType);
				if (Colour)
				{
					if (Colour->Type == LCss::ColorRgb)
					{
						c.Set(Colour->Rgb32, 32);
						break;
					}
					else if (Colour->Type == LCss::ColorTransparent)
					{
						c.Empty();
						break;
					}
				}
			}
			
			if (dynamic_cast<LWindow*>(v) ||
				dynamic_cast<LPopup*>(v))
				break;
		}
	}
	
	return c;
}

OsThreadId LView::ViewThread()
{
	#if 0 // defined(HAIKU)

		auto h = Handle();
		if (!h)
			return 0;
		
		auto looper = h->Looper();
		if (!looper)
			return 0;
			
		return looper->Thread();
	
	#else

		return LAppInst->GetGuiThreadId();

	#endif
}

bool LView::InThread()
{
	#if WINNATIVE

		HWND Hnd = _View;
		for (LViewI *p = GetParent(); p && !Hnd; p = p->GetParent())
			Hnd = p->Handle();
		
		auto CurThreadId = LCurrentThreadId();
		auto GuiThreadId = LAppInst->GetGuiThreadId();
		DWORD ViewThread = Hnd ? GetWindowThreadProcessId(Hnd, NULL) : GuiThreadId;
		return CurThreadId == ViewThread;
	
	#elif defined(HAIKU)

		// All events should be handled in the app thread...
		// Anything in a BWindow should be forwarded to the app thread.
		auto CurThreadId = LCurrentThreadId();
		return CurThreadId == LAppInst->GetGuiThreadId();
		
	#else

		OsThreadId Me = LCurrentThreadId();
		OsThreadId Gui = LAppInst ? LAppInst->GetGuiThreadId() : 0;
		
		#if 0
		if (Gui != Me)
		    LgiTrace("%s:%i - %s Out of thread:"
				    #ifdef LGI_COCOA
					"%llx, %llx"
				    #else
					"%i, %i"
					#endif
		    		"\n",
		    		_FL,
		    		GetClass(),
		    		Gui,
		    		Me);
		#endif
		
		return Gui == Me;

	#endif
}

bool LView::PostEvent(int Cmd, LMessage::Param a, LMessage::Param b, int64_t timeoutMs)
{
	#ifdef LGI_SDL

		return LPostEvent(this, Cmd, a, b);

	#elif defined(HAIKU)

		auto bWnd = WindowHandle();
		if (!bWnd)
		{
			printf("%s:%i - no wndHnd\n", _FL);
			return false;
		}
		
		BMessage m(Cmd);
		
		auto r = m.AddInt64(LMessage::PropA, a);
		if (r != B_OK)
			printf("%s:%i - AddUInt64 failed.\n", _FL);
		r = m.AddInt64(LMessage::PropB, b);
		if (r != B_OK)
			printf("%s:%i - AddUInt64 failed.\n", _FL);
		r = m.AddPointer(LMessage::PropView, static_cast<LView*>(this));
		if (r != B_OK)
			printf("%s:%i - AddPointer failed.\n", _FL);

		if (bWnd)
		{
			auto threadId = bWnd->Thread();
			if (threadId <= 0)
			{
				printf("####### %s:%i warning, BWindow(%s) has no thread for PostEvent!?\n", _FL, GetClass());
			}
			else
			{
				r = bWnd->PostMessage(&m);
				if (r != B_OK)
					printf("%s:%i - PostMessage failed.\n", _FL);
				return r == B_OK;
			}
		}

		// Not attached yet...
		d->MsgQue.Add(new BMessage(m));
		printf("%s:%i - PostEvent.MsgQue=%i\n", _FL, (int)d->MsgQue.Length());
		
		return true;

	#elif WINNATIVE

		if (!_View)
			return false;

		BOOL Res = ::PostMessage(_View, Cmd, a, b);
		if (!Res)
		{
			auto Err = GetLastError();
			int asd=0;
		}

		return Res != 0;

	#elif !LGI_VIEW_HANDLE

		return LAppInst->PostEvent(this, Cmd, a, b);

	#else

		if (_View)
			return LPostEvent(_View, Cmd, a, b);

		LAssert(0);
		return false;

	#endif
}

bool LView::Invalidate(LRegion *r, bool Repaint, bool NonClient)
{
	if (r)
	{
		for (int i=0; i<r->Length(); i++)
		{
			bool Last = i == r->Length()-1;
			Invalidate((*r)[i], Last ? Repaint : false, NonClient);
		}
		
		return true;
	}
	
	return false;
}

LButton *FindDefault(LViewI *w)
{
	LButton *But = 0;

	for (auto c: w->IterateViews())
	{
		But = dynamic_cast<LButton*>(c);
		if (But && But->Default())
		{
			break;
		}
			
		But = FindDefault(c);
		if (But)
		{
			break;
		}
	}

	return But;
}

bool LView::Name(const char *n)
{
	LBase::Name(n);

	#if LGI_VIEW_HANDLE && !defined(HAIKU)
	if (_View)
	{
		#if WINNATIVE
		
		auto Temp = LBase::NameW();
		SetWindowTextW(_View, Temp ? Temp : L"");
		
		#endif
	}
	#endif

	Invalidate();

	return true;
}

const char *LView::Name()
{
	#if WINNATIVE
	if (_View)
	{
		LView::NameW();
	}
	#endif

	return LBase::Name();
}

bool LView::NameW(const char16 *n)
{
	LBase::NameW(n);

	#if WINNATIVE
	if (_View && n)
	{
		auto Txt = LBase::NameW();
		SetWindowTextW(_View, Txt);
	}
	#endif

	Invalidate();
	return true;
}

const char16 *LView::NameW()
{
	#if WINNATIVE
	if (_View)
	{
		int Length = GetWindowTextLengthW(_View);
		if (Length > 0)
		{
			char16 *Buf = new char16[Length+1];
			if (Buf)
			{
				Buf[0] = 0;
				int Chars = GetWindowTextW(_View, Buf, Length+1);
				Buf[Chars] = 0;
				LBase::NameW(Buf);
			}
			DeleteArray(Buf);
		}
		else
		{
			LBase::NameW(0);
		}
	}
	#endif

	return LBase::NameW();
}

LViewI *LView::FindControl(int Id)
{
	LAssert(Id != -1);

	if (GetId() == Id)
	{
		return this;
	}

	for (auto c : Children)
	{
		LViewI *Ctrl = c->FindControl(Id);
		if (Ctrl)
		{
			return Ctrl;
		}
	}
	return 0;
}

LPoint LView::GetMinimumSize()
{
	return d->MinimumSize;
}

void LView::SetMinimumSize(LPoint Size)
{
	d->MinimumSize = Size;

	bool Change = false;
	LRect p = Pos;
	if (X() < d->MinimumSize.x)
	{
		p.x2 = p.x1 + d->MinimumSize.x - 1;
		Change = true;
	}
	if (Y() < d->MinimumSize.y)
	{
		p.y2 = p.y1 + d->MinimumSize.y - 1;
		Change = true;
	}
	if (Change)
	{
		SetPos(p);
	}
}

bool LView::SetColour(LColour &c, bool Fore)
{
	LCss *css = GetCss(true);
	if (!css)
		return false;
	
	if (Fore)
	{
		if (c.IsValid())
			css->Color(LCss::ColorDef(LCss::ColorRgb, c.c32()));
		else
			css->DeleteProp(LCss::PropColor);
	}
	else
	{
		if (c.IsValid())
			css->BackgroundColor(LCss::ColorDef(LCss::ColorRgb, c.c32()));
		else
			css->DeleteProp(LCss::PropBackgroundColor);
	}
	
	return true;
}


/*
bool LView::SetCssStyle(const char *CssStyle)
{
    if (!d->Css && !d->Css.Reset(new LCss))
		return false;
    
    const char *Defs = CssStyle;
    bool b = d->Css->Parse(Defs, LCss::ParseRelaxed);
    if (b && d->FontOwnType == GV_FontCached)
    {
		d->Font = NULL;
		d->FontOwnType = GV_FontPtr;
    }
    
    return b;    
}
*/

void LView::SetCss(LCss *css)
{
	d->Css.Reset(css);
}

LCss *LView::GetCss(bool Create)
{
    if (Create && !d->Css)
        d->Css.Reset(new LCss);

	if (d->CssDirty && d->Css)
	{
		const char *Defs = d->Styles;
		if (d->Css->Parse(Defs, LCss::ParseRelaxed))
			d->CssDirty = false;
	}

    return d->Css;
}

LPoint &LView::GetWindowBorderSize()
{
	static LPoint s;

	ZeroObj(s);

	#if WINNATIVE
	if (_View)
	{
		RECT Wnd, Client;
		GetWindowRect(Handle(), &Wnd);
		GetClientRect(Handle(), &Client);
		s.x = (Wnd.right-Wnd.left) - (Client.right-Client.left);
		s.y = (Wnd.bottom-Wnd.top) - (Client.bottom-Client.top);
	}
	#elif defined __GTK_H__
	#elif defined MAC
	s.x = 0;
	s.y = 22;
	#endif

	return s;
}

#ifdef _DEBUG

#if defined(LGI_CARBON)

	void DumpHiview(HIViewRef v, int Depth = 0)
	{
		char Sp[256];
		memset(Sp, ' ', Depth << 2);
		Sp[Depth<<2] = 0;

		printf("%sHIView=%p", Sp, v);
		if (v)
		{
			Boolean vis = HIViewIsVisible(v);
			Boolean en = HIViewIsEnabled(v, NULL);
			HIRect pos;
			HIViewGetFrame(v, &pos);

			char cls[128];
			ZeroObj(cls);
			GetControlProperty(v, 'meme', 'clas', sizeof(cls), NULL, cls);

			printf(" vis=%i en=%i pos=%g,%g-%g,%g cls=%s",
				vis, en,
				pos.origin.x, pos.origin.y, pos.size.width, pos.size.height,
				cls);
		}
		printf("\n");

		for (HIViewRef c = HIViewGetFirstSubview(v); c; c = HIViewGetNextView(c))
		{
			DumpHiview(c, Depth + 1);
		}
	}

#elif defined(__GTK_H__)

	void DumpGtk(Gtk::GtkWidget *w, Gtk::gpointer Depth = NULL)
	{
		using namespace Gtk;
		if (!w)
			return;

		char Sp[65] = {0};
		if (Depth)
			memset(Sp, ' ', *((int*)Depth)*2);

		auto *Obj = G_OBJECT(w);
		LViewI *View = (LViewI*) g_object_get_data(Obj, "LViewI");

		GtkAllocation a;
		gtk_widget_get_allocation(w, &a);
		LgiTrace("%s%p(%s) = %i,%i-%i,%i\n", Sp, w, View?View->GetClass():G_OBJECT_TYPE_NAME(Obj), a.x, a.y, a.width, a.height);

		if (GTK_IS_CONTAINER(w))
		{
			auto *c = GTK_CONTAINER(w);
			if (c)
			{
				int Next = Depth ? *((int*)Depth)+1 : 1;
				gtk_container_foreach(c, DumpGtk, &Next);
			}
		}
	}

#elif defined(HAIKU) || defined(WINDOWS)

	template<typename T>
	void _Dump(int Depth, T v)
	{
		LString Sp, Name;
		Sp.Length(Depth<<1);
		memset(Sp.Get(), ' ', Depth<<1);
		Sp.Get()[Depth<<1] = 0;
		
		#if defined(HAIKU)
			LRect Frame = v->Frame();
			Name = v->Name();
			bool IsHidden = v->IsHidden();
		#else
			RECT rc; GetWindowRect(v, &rc);
			LRect Frame = rc;
			wchar_t txt[256];
			GetWindowTextW(v, txt, CountOf(txt));
			Name = txt;
			bool IsHidden = !(GetWindowLong(v, GWL_STYLE) & WS_VISIBLE);
		#endif

		LgiTrace("%s%p::%s frame=%s vis=%i\n",
			Sp.Get(), v, Name.Get(), Frame.GetStr(), !IsHidden);

		#if defined(HAIKU)
		for (int32 i=0; i<v->CountChildren(); i++)
			_Dump(Depth + 1, v->ChildAt(i));
		#else
		for (auto h = GetWindow(v, GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT))
			_Dump(Depth + 1, h);
		#endif
	}

#endif

void LView::_Dump(int Depth)
{
	char Sp[65] = {0};
	memset(Sp, ' ', Depth*2);
	
	#if 0
	
		char s[256];
		sprintf_s(s, sizeof(s), "%s%p::%s %s (_View=%p)\n", Sp, this, GetClass(), GetPos().GetStr(), _View);
		LgiTrace(s);
		List<LViewI>::I i = Children.Start();
		for (LViewI *c = *i; c; c = *++i)
		{
			LView *v = c->GetLView();
			if (v)
				v->_Dump(Depth+1);
		}
	
	#elif defined(LGI_CARBON)
	
		DumpHiview(_View);

	#elif defined(__GTK_H__)

		auto wid = GtkCast(WindowHandle(), gtk_widget, GtkWidget);
		DumpGtk(wid);
	
	#elif !defined(MAC)
	
		#if defined(HAIKU)
		LLocker lck(WindowHandle(), _FL);
		if (lck.Lock())
		#endif
			::_Dump(0, WindowHandle());
	
	#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
static LArray<LViewFactory*> *AllFactories = NULL;
#if defined(WIN32)
	static HANDLE FactoryEvent;
#else
	static pthread_once_t FactoryOnce = PTHREAD_ONCE_INIT;
	static void GFactoryInitFactories()
	{
		AllFactories = new LArray<LViewFactory*>;
	}
#endif

LViewFactory::LViewFactory()
{
	#if defined(WIN32)
		char16 Name[64];
		swprintf_s(Name, CountOf(Name), L"LgiFactoryEvent.%i", GetCurrentProcessId());
		HANDLE h = CreateEventW(NULL, false, false, Name);
		DWORD err = GetLastError();
		if (err != ERROR_ALREADY_EXISTS)
		{
			FactoryEvent = h;
			AllFactories = new LArray<LViewFactory*>;
		}
		else
		{
			LAssert(AllFactories != NULL);
			CloseHandle(h);
		}
	#else
		pthread_once(&FactoryOnce, GFactoryInitFactories);
	#endif

	if (AllFactories)
		AllFactories->Add(this);
}

LViewFactory::~LViewFactory()
{
	if (AllFactories)
	{
		AllFactories->Delete(this);
		if (AllFactories->Length() == 0)
		{
			DeleteObj(AllFactories);
			#if defined(WIN32)
			CloseHandle(FactoryEvent);
			#endif
		}
	}
}

LView *LViewFactory::Create(const char *Class, LRect *Pos, const char *Text)
{
	if (ValidStr(Class) && AllFactories)
	{
		for (int i=0; i<AllFactories->Length(); i++)
		{
			LView *v = (*AllFactories)[i]->NewView(Class, Pos, Text);
			if (v)
			{
				return v;
			}
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
LView::ViewEventTarget::ViewEventTarget(LView *View, int Msg)
{
	LAssert(View != NULL);
	view = View;
	if (Msg)
		Msgs.Add(Msg, true);
	if (view)
		view->d->EventTargets.Add(this);
}

LView::ViewEventTarget::~ViewEventTarget()
{
	if (view)
		view->d->EventTargets.Delete(this);
}

bool LView::ViewEventTarget::PostEvent(int Cmd, LMessage::Param a, LMessage::Param b, int64_t TimeoutMs)
{
	if (view)
		return view->PostEvent(Cmd, a, b, TimeoutMs);

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG

#if defined(__GTK_H__)
using namespace Gtk;
#include "LgiWidget.h"
#endif

void LView::Debug()
{
    _Debug = true;

	#if defined LGI_COCOA
    d->ClassName = GetClass();
	#endif
}
#endif

