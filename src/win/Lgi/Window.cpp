#include <stdio.h>
#include <math.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Popup.h"
#include "lgi/common/ToolBar.h"
#include "lgi/common/Panel.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Token.h"
#include "lgi/common/Button.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/Menu.h"

#include "ViewPriv.h"

#define DEBUG_WINDOW_PLACEMENT				0
#define DEBUG_HANDLE_VIEW_KEY				0
#define DEBUG_HANDLE_VIEW_MOUSE				0
#define DEBUG_SERIALIZE_STATE				0
#define DEBUG_SETFOCUS						0

extern bool In_SetWindowPos;

typedef UINT (WINAPI *ProcGetDpiForWindow)(_In_ HWND hwnd);
typedef UINT (WINAPI *ProcGetDpiForSystem)(VOID);
LLibrary User32("User32");

LPoint LGetDpiForWindow(HWND hwnd)
{
	static bool init = false;
	static ProcGetDpiForWindow pGetDpiForWindow = NULL;
	static ProcGetDpiForSystem pGetDpiForSystem = NULL;
	if (!init)
	{
		init = true;
		pGetDpiForWindow = (ProcGetDpiForWindow) User32.GetAddress("GetDpiForWindow");
		pGetDpiForSystem = (ProcGetDpiForSystem) User32.GetAddress("GetDpiForSystem");
	}

	if (pGetDpiForWindow && pGetDpiForSystem)
	{
		auto Dpi = hwnd ? pGetDpiForWindow(hwnd) : pGetDpiForSystem();
		return LPoint(Dpi, Dpi);
	}

	return LScreenDpi();
}

///////////////////////////////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	int Flags;
	LView *Target;
};

class LWindowPrivate
{
public:
	LArray<HookInfo> Hooks;
	bool SnapToEdge;
	bool AlwaysOnTop;
	LWindowZoom Show;
	bool InCreate;
	LAutoPtr<WINDOWPLACEMENT> Wp;
	LPoint Dpi;
	int ShowCmd = SW_NORMAL;

	// Focus stuff
	LViewI *Focus;

	LWindowPrivate()
	{
		Focus = NULL;
		InCreate = true;
		Show = LZoomNormal;
		SnapToEdge = false;
		AlwaysOnTop = false;
	}

	~LWindowPrivate()
	{
	}
	
	ssize_t GetHookIndex(LView *Target, bool Create = false)
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
				n->Flags = 0;
				return (ssize_t)Hooks.Length() - 1;
			}
		}
		
		return -1;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
LWindow::LWindow() : LView(0)
{
	_Window = this;
	d = new LWindowPrivate;
	SetStyle(GetStyle() | WS_TILEDWINDOW | WS_CLIPCHILDREN);
	SetStyle(GetStyle() & ~WS_CHILD);
	SetExStyle(GetExStyle() | WS_EX_CONTROLPARENT);
	
	LWindowsClass *c = LWindowsClass::Create(GetClass());
	if (c)
		c->Register();
	
	Visible(false);

	_Default = 0;
	_Lock = new LMutex("LWindow");
	_QuitOnClose = false;
}

LWindow::~LWindow()
{
	if (LAppInst && LAppInst->AppWnd == this)
		LAppInst->AppWnd = NULL;

	if (Menu)
	{
		Menu->Detach();
		DeleteObj(Menu);
	}

	DeleteObj(_Lock);
	DeleteObj(d);
}

int LWindow::WaitThread()
{
	// No thread to wait on...
	return 0;
}

bool LWindow::SetTitleBar(bool ShowTitleBar)
{
	if (ShowTitleBar)
	{
		SetStyle(GetStyle() | WS_TILEDWINDOW);
	}
	else
	{
		SetStyle(GetStyle() & ~WS_TILEDWINDOW);
		SetStyle(GetStyle() | WS_POPUP);
	}

	return true;
}

bool LWindow::SetIcon(const char *Icon)
{
	return CreateClassW32(LAppInst->Name(), LoadIcon(LProcessInst(), (LPCWSTR)Icon)) != 0;
}

LViewI *LWindow::GetFocus()
{
	return d->Focus;
}

static LAutoString DescribeView(LViewI *v)
{
	if (!v)
		return LAutoString(NewStr("NULL"));

	char s[512];
	int ch = 0;
	::LArray<LViewI*> p;
	for (LViewI *i = v; i; i = i->GetParent())
	{
		p.Add(i);
	}
	for (auto n=MIN(3, (ssize_t)p.Length()-1); n>=0; n--)
	{
		v = p[n];
		ch += sprintf_s(s + ch, sizeof(s) - ch, ">%s", v->GetClass());
	}
	return LAutoString(NewStr(s));
}

static bool HasParentPopup(LViewI *v)
{
	for (; v; v = v->GetParent())
	{
		if (dynamic_cast<LPopup*>(v))
			return true;
	}
	return false;
}

void LWindow::SetWillFocus(bool f)
{
	d->ShowCmd = f ? SW_NORMAL : SW_SHOWNOACTIVATE;
}

void LWindow::SetFocus(LViewI *ctrl, FocusType type)
{
	const char *TypeName = NULL;
	switch (type)
	{
		case GainFocus: TypeName = "Gain"; break;
		case LoseFocus: TypeName = "Lose"; break;
		case ViewDelete: TypeName = "Delete"; break;
	}

	switch (type)
	{
		case GainFocus:
		{
			LViewI *This = this;
			if (ctrl == This && d->Focus)
			{
				// The main LWindow is getting focus.
				// Check if we can re-focus the previous child focus...
				LView *v = d->Focus->GetGView();
				if (v && !HasParentPopup(v))
				{
					// We should never return focus to a popup, or it's child.
					if (!(v->WndFlags & GWF_FOCUS))
					{
						// Yes, the child view doesn't think it has focus...
						// So re-focus it...
						if (v->Handle())
						{
							// Non-virtual window...
							::SetFocus(v->Handle());
						}

						v->WndFlags |= GWF_FOCUS;
						v->OnFocus(true);
						v->Invalidate();

						#if DEBUG_SETFOCUS
						LAutoString _set = DescribeView(ctrl);
						LAutoString _foc = DescribeView(d->Focus);
						LgiTrace("LWindow::SetFocus(%s, %s) refocusing: %s\n",
							_set.Get(),
							TypeName,
							_foc.Get());
						#endif
						return;
					}
				}
			}
			
			// Check if the control already has focus
			if (d->Focus == ctrl)
				return;
					
			if (d->Focus)
			{
				LView *v = d->Focus->GetGView();
				if (v) v->WndFlags &= ~GWF_FOCUS;
				d->Focus->OnFocus(false);
				d->Focus->Invalidate();

				#if DEBUG_SETFOCUS
				LAutoString _foc = DescribeView(d->Focus);
				LgiTrace(".....defocus: %s\n",
					_foc.Get());
				#endif
			}
			
			d->Focus = ctrl;

			if (d->Focus)
			{
				LView *v = d->Focus->GetGView();
				if (v) v->WndFlags |= GWF_FOCUS;
				d->Focus->OnFocus(true);
				d->Focus->Invalidate();

				#if DEBUG_SETFOCUS
				LAutoString _set = DescribeView(d->Focus);
				LgiTrace("LWindow::SetFocus(%s, %s) focusing\n",
					_set.Get(),
					TypeName);
				#endif
			}
			break;
		}
		case LoseFocus:
		{
			if (ctrl == d->Focus)
			{
				LView *v = d->Focus->GetGView();
				if (v)
				{
					if (v->WndFlags & GWF_FOCUS)
					{
						// View thinks it has focus
						v->WndFlags &= ~GWF_FOCUS;
						d->Focus->OnFocus(false);
						// keep d->Focus pointer, as we want to be able to re-focus the child
						// view when we get focus again

						#if DEBUG_SETFOCUS
						LAutoString _ctrl = DescribeView(ctrl);
						LAutoString _foc = DescribeView(d->Focus);
						LgiTrace("LWindow::SetFocus(%s, %s) keep_focus: %s\n",
							_ctrl.Get(),
							TypeName,
							_foc.Get());
						#endif
					}
					// else view doesn't think it has focus anyway...
				}
				else
				{
					// Non LView handler	
					d->Focus->OnFocus(false);
					d->Focus->Invalidate();
					d->Focus = NULL;
				}
			}
			else
			{
				/*
				LgiTrace("LWindow::SetFocus(%p.%s, %s) error on losefocus: %p(%s)\n",
					ctrl,
					ctrl ? ctrl->GetClass() : NULL,
					TypeName,
					d->Focus,
					d->Focus ? d->Focus->GetClass() : NULL);
				*/
			}
			break;
		}
		case ViewDelete:
		{
			if (ctrl == d->Focus)
			{
				#if DEBUG_SETFOCUS
				LgiTrace("LWindow::SetFocus(%p.%s, %s) delete_focus: %p(%s)\n",
					ctrl,
					ctrl ? ctrl->GetClass() : NULL,
					TypeName,
					d->Focus,
					d->Focus ? d->Focus->GetClass() : NULL);
				#endif

				d->Focus = NULL;
			}
			break;
		}
	}
}

bool LWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void LWindow::SetSnapToEdge(bool b)
{
	d->SnapToEdge = b;
}

bool LWindow::GetAlwaysOnTop()
{
	return d->AlwaysOnTop;
}

void LWindow::SetAlwaysOnTop(bool b)
{
	d->AlwaysOnTop = b;
	if (_View)
		SetWindowPos(_View, b ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
}

void LWindow::Raise()
{
	if (_View)
	{
		DWORD dwFGProcessId;
		DWORD dwFGThreadId = GetWindowThreadProcessId(_View, &dwFGProcessId);
		DWORD dwThisThreadId = GetCurrentThreadId();

		AttachThreadInput(dwThisThreadId, dwFGThreadId, true);

		SetForegroundWindow(_View);
		BringWindowToTop(_View);

		if (In_SetWindowPos)
		{
			assert(0);
			LgiTrace("%s:%i - SetFocus(%p)\n", __FILE__, __LINE__, _View);
		}
		::SetFocus(_View);

		AttachThreadInput(dwThisThreadId, dwFGThreadId, false);
	}
}

LWindowZoom LWindow::GetZoom()
{
	if (IsZoomed(Handle()))
	{
		return LZoomMax;
	}
	else if (IsIconic(Handle()))
	{
		return LZoomMin;
	}

	return LZoomNormal;
}

void LWindow::SetZoom(LWindowZoom i)
{
	if (_View && IsWindowVisible(_View))
	{
		switch (i)
		{
			case LZoomMax:
			{
				ShowWindow(Handle(), SW_MAXIMIZE);
				break;
			}
			case LZoomMin:
			{
				ShowWindow(Handle(), SW_MINIMIZE);
				break;
			}
			case LZoomNormal:
			{
				if (!Visible())
				{
					Visible(true);
				}

				if (IsIconic(Handle()) || IsZoomed(Handle()))
				{
					ShowWindow(Handle(), d->ShowCmd);
				}

				LYield();

				RECT r;
				GetWindowRect(Handle(), &r);

				if (r.left != Pos.x1 ||
					r.top != Pos.y1)
				{
					int Shadow = WINDOWS_SHADOW_AMOUNT;
					SetWindowPos(Handle(),
								0,
								Pos.x1,
								Pos.y1,
								Pos.X() + Shadow,
								Pos.Y() + Shadow,
								SWP_NOZORDER);
				}
				break;
			}
		}
	}

	d->Show = i;
}

bool LWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LCloseApp();
	}

	return true;
}

bool LWindow::HandleViewMouse(LView *v, LMouse &m)
{
	#if DEBUG_HANDLE_VIEW_MOUSE
	m.Trace("HandleViewMouse");
	#endif

	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & LMouseEvents)
		{
			LView *t = d->Hooks[i].Target;
			if (!t->OnViewMouse(v, m))
			{
				#if DEBUG_HANDLE_VIEW_MOUSE
				if (m.IsMove())
					LgiTrace("   Hook %i of %i ate mouse event: '%s'\n", i, d->Hooks.Length(), d->Hooks[i].Target->GetClass());
				#endif
				return false;
			}
		}
	}
	
	#if DEBUG_HANDLE_VIEW_MOUSE
	if (!m.IsMove())
		LgiTrace("   Passing mouse event to '%s'\n", v->GetClass());
	#endif

	return true;
}

bool LWindow::HandleViewKey(LView *v, LKey &k)
{
	#if DEBUG_HANDLE_VIEW_KEY
	char msg[256];
	sprintf_s(msg, sizeof(msg), "HandleViewKey, v=%s", v ? v->GetClass() : "NULL");
	k.Trace(msg);
	#endif

	// Any window in a pop up always gets the key...
	LViewI *p;
	for (p = v->GetParent(); p; p = p->GetParent())
	{
		if (dynamic_cast<LPopup*>(p))
		{
			#if DEBUG_HANDLE_VIEW_KEY
			LgiTrace("    Popup %s handling key.\n", p->GetClass());
			#endif
			return v->OnKey(k);
		}
	}

	// Allow any hooks to see the key...
	#if DEBUG_HANDLE_VIEW_KEY
	LgiTrace("    d->Hooks.Length()=%i.\n", (int)d->Hooks.Length());
	#endif
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & LKeyEvents)
		{
			if (d->Hooks[i].Target->OnViewKey(v, k))
			{
				#if DEBUG_HANDLE_VIEW_KEY
				LgiTrace("    Hook[%i] %s handling key.\n", i, d->Hooks[i].Target->GetClass());
				#endif
				return true;
			}
		}
	}

	// Give the key to the focused window...
	if (d->Focus && d->Focus->OnKey(k))
	{
		#if DEBUG_HANDLE_VIEW_KEY
		LgiTrace("    d->Focus %s handling key.\n", d->Focus->GetClass());
		#endif
		return true;
	}

	// Check default controls
	p = 0;
	if (k.c16 == VK_RETURN)
	{
		if (!_Default)
			p = _Default = FindControl(IDOK);
		else
			p = _Default;
		#if DEBUG_HANDLE_VIEW_KEY
		LgiTrace("    Using _Default ctrl (%s).\n", p ? p->GetClass() : "NULL");
		#endif
	}
	else if (k.c16 == VK_ESCAPE)
	{
		p = FindControl(IDCANCEL);
		if (p)
		{
			#if DEBUG_HANDLE_VIEW_KEY
			LgiTrace("    Using IDCANCEL ctrl (%s).\n", p->GetClass());
			#endif
		}
	}
	
	if (p && p->OnKey(k))
	{
		#if DEBUG_HANDLE_VIEW_KEY
		LgiTrace("    Default control %s handled key.\n", p->GetClass());
		#endif
		return true;
	}

	// Menu shortcut?
	if (Menu && Menu->OnKey(v, k))
	{
		#if DEBUG_HANDLE_VIEW_KEY
		LgiTrace("    Menu handled key.\n");
		#endif
		return true;
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

	#if DEBUG_HANDLE_VIEW_KEY
	LgiTrace("    No one handled key.\n");
	#endif
	return false;
}

void LWindow::OnPaint(LSurface *pDC)
{
	auto c = GetClient();
	LCssTools Tools(this);
	Tools.PaintContent(pDC, c);
}

bool LWindow::Obscured()
{
	RECT tRect;

	bool isObscured = false;
	if (GetWindowRect(_View, &tRect))
	{
		RECT nRect;
		HWND walker = _View;
		while (walker = ::GetNextWindow(walker, GW_HWNDPREV))
		{
			if (IsWindowVisible(walker))
			{
                if ((::GetWindowRect(walker, &nRect)))
				{
					RECT iRect;
					IntersectRect(&iRect, &tRect, &nRect);
					if (iRect.bottom || iRect.top || iRect.left || iRect.right)
					{
						  isObscured = true;
						  break;
					}
				}
			}
		}
	}

	return isObscured;
}

bool LWindow::Visible()
{
	return LView::Visible();
}

void LWindow::Visible(bool v)
{
	if (v)
		PourAll();

	if (v)
	{
		SetStyle(GetStyle() | WS_VISIBLE);

		if (_View)
		{
			LWindowZoom z = d->Show;
			char *Cmd = 0;

			LAutoPtr<WINDOWPLACEMENT> Wp(new WINDOWPLACEMENT);
			if (Wp)
			{
				ZeroObj(*Wp.Get());
				Wp->length = sizeof(*Wp);
				Wp->flags = 2;
				Wp->ptMaxPosition.x = -1;
				Wp->ptMaxPosition.y = -1;

				if (d->Show == LZoomMax)
				{
					Wp->showCmd = SW_MAXIMIZE;
					Cmd = "SW_MAXIMIZE";
				}
				else if (d->Show == LZoomMin)
				{
					Wp->showCmd = SW_MINIMIZE;
					Cmd = "SW_MINIMIZE";
				}
				else
				{
					Wp->showCmd = d->ShowCmd;
					Cmd = "SW_NORMAL";
				}

				Wp->rcNormalPosition = Pos;
				#if DEBUG_WINDOW_PLACEMENT
				LgiTrace("%s:%i - SetWindowPlacement, pos=%s, show=%i\n", __FILE__, __LINE__, Pos.GetStr(), Wp->showCmd);
				#endif
				SetWindowPlacement(_View, Wp);

				if (d->InCreate)
					d->Wp = Wp;
			}
		}
	}
	else
	{
		#if DEBUG_WINDOW_PLACEMENT
		LgiTrace("%s:%i - Visible(%i)\n", __FILE__, __LINE__, v);
		#endif
		LView::Visible(v);
	}

	if (v)
	{
		OnZoom(d->Show);
	}
}

static bool IsAppWnd(HWND h)
{
	if (!IsWindowVisible(h))
		return false;
	auto flags = GetWindowLong(h, GWL_STYLE);
	if (flags & WS_POPUP)
		return false;
	return true;
}

bool LWindow::IsActive()
{
	auto top = GetTopWindow(GetDesktopWindow());
	while (top && !IsAppWnd(top))
		top = ::GetWindow(top, GW_HWNDNEXT);
	return top == _View;
}

bool LWindow::SetActive()
{
	if (!_View)
		return false;
	return SetWindowPos(_View, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) != 0;
}

void LWindow::PourAll()
{
	LRegion Client(GetClient());
	LRegion Update;
	bool HasTools = false;
	
	{
		LRegion Tools;

		for (auto v: Children)
		{
			LView *k = dynamic_cast<LView*>(v);
			if (k && k->_IsToolBar)
			{
				LRect OldPos = v->GetPos();
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

	// LgiTrace("Client=%s\n", Client.Bound().GetStr());
	for (auto v: Children)
	{
		LView *k = dynamic_cast<LView*>(v);
		if (!(k && k->_IsToolBar))
		{
			LRect OldPos = v->GetPos();
			Update.Union(&OldPos);

			if (v->Pour(Client))
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

				Client.Subtract(&v->GetPos());
				Update.Subtract(&v->GetPos());
			}
			else
			{
				// make the view not visible
				// v->Visible(FALSE);
			}
		}
	}

	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
}

class LTempView : public LView
{
public:
	LTempView(OsView v)
	{
		_View = v;
	}

	~LTempView()
	{
		_View = 0;
	}
};

LMessage::Result LWindow::OnEvent(LMessage *Msg)
{
	int Status = 0;

	switch (Msg->Msg())
	{
		case WM_DPICHANGED:
		{
			d->Dpi.x = HIWORD(Msg->A());
			d->Dpi.y = LOWORD(Msg->A());
			OnPosChange();
			break;
		}
		case M_ASSERT_UI:
		{
			LAutoPtr<LString> Str((LString*)Msg->A());
			extern void LAssertDlg(LString Msg, std::function<void(int)> Callback);
			if (Str)
				LAssertDlg(Str ? *Str : "Error: no msg.", NULL);
			break;
		}
		case M_SET_WINDOW_PLACEMENT:
		{
			/*	Apparently if you use SetWindowPlacement inside the WM_CREATE handler,
				then the restored rect doesn't "stick", it gets stomped on by windows.
				So this code... RESETS it to be what we set earlier. Windows sucks. */
			if (!d->Wp || !_View)
				break;

			LRect r = d->Wp->rcNormalPosition;

			if (!LView::Visible())
				d->Wp->showCmd = SW_HIDE;

			#if DEBUG_WINDOW_PLACEMENT
			LgiTrace("%s:%i - SetWindowPlacement, pos=%s, show=%i\n", __FILE__, __LINE__, r.GetStr(), d->Wp->showCmd);
			#endif

			SetWindowPlacement(_View, d->Wp);
			d->Wp.Reset();
			break;
		}
		case WM_SYSCOLORCHANGE:
		{
			LColour::OnChange();
			break;
		}
		case WM_WINDOWPOSCHANGING:
		{
			bool Icon = IsIconic(Handle()) != 0;
			bool Zoom = IsZoomed(Handle()) != 0;

			if (!Icon && (_Dialog || !Zoom))
			{
				WINDOWPOS *Info = (LPWINDOWPOS) Msg->b;
				if (!Info)
				    break;
				    
				if (Info->flags == (SWP_NOSIZE | SWP_NOMOVE) && _Dialog)
				{
				    // Info->flags |= SWP_NOZORDER;
				    Info->hwndInsertAfter = _Dialog->Handle();
				}

				if (GetMinimumSize().x &&
					GetMinimumSize().x > Info->cx)
				{
					Info->cx = GetMinimumSize().x;
				}

				if (GetMinimumSize().y &&
					GetMinimumSize().y > Info->cy)
				{
					Info->cy = GetMinimumSize().y;
				}

				/* This is broken on windows 10... windows get stuck on the edge of the desktop.
				RECT Rc;
				if (d->SnapToEdge &&
					SystemParametersInfo(SPI_GETWORKAREA, 0, &Rc, SPIF_SENDCHANGE))
				{
					LRect r = Rc;
					LRect p(Info->x,
					        Info->y,
					        Info->x + Info->cx - 1,
					        Info->y + Info->cy - 1);
					
					if (r.Valid() && p.Valid())
					{
						int Snap = 12;

						if (abs(p.x1 - r.x1) <= Snap)
						{
							// Snap left edge
							Info->x = r.x1;
						}
						else if (abs(p.x2 - r.x2) <= Snap)
						{
							// Snap right edge
							Info->x = r.x2 - Info->cx + 1;
						}

						if (abs(p.y1 - r.y1) <= Snap)
						{
							// Snap top edge
							Info->y = r.y1;
						}
						else if (abs(p.y2 - r.y2) <= Snap)
						{
							// Snap bottom edge
							Info->y = r.y2 - Info->cy + 1;
						}
					}
				}
				*/
			}
			break;
		}
		case WM_SIZE:
		{
			if (Visible())
			{
				LWindowZoom z = d->Show;
				switch (Msg->a)
				{
					case SIZE_MINIMIZED:
					{
						z = LZoomMin;
						break;
					}
					case SIZE_MAXIMIZED:
					{
						z = LZoomMax;
						break;
					}
					case SIZE_RESTORED:
					{
						z = LZoomNormal;
						break;
					}
				}
				if (z != d->Show)
				{
					OnZoom(d->Show = z);
				}
			}

			Status = LView::OnEvent(Msg) != 0;
			break;
		}		
		case WM_CREATE:
		{
			if (d->AlwaysOnTop)
				SetAlwaysOnTop(true);
			PourAll();
			OnCreate();

			if (!_Default)
			{
				_Default = FindControl(IDOK);
				if (_Default)
					_Default->Invalidate();
			}

			d->InCreate = false;
			if (d->Wp)
			{
				PostEvent(M_SET_WINDOW_PLACEMENT);
			}
			break;
		}
		case WM_WINDOWPOSCHANGED:
		{
			d->Wp.Reset();
			Status = LView::OnEvent(Msg) != 0;
			break;
		}
		case WM_QUERYENDSESSION:
	 	case WM_CLOSE:
		{
			bool QuitApp;
			bool OsShuttingDown = Msg->Msg() == WM_QUERYENDSESSION;
			if (QuitApp = OnRequestClose(OsShuttingDown))
			{
				Quit();
			}

			if (Msg->Msg() == WM_CLOSE)
			{
				return 0;
			}
			else
			{
				return QuitApp;
			}
			break;
		}
		case WM_SYSCOMMAND:
		{
		    if (Msg->a == SC_CLOSE)
		    {
    			if (OnRequestClose(false))
    		    {
    		        Quit();
    		    }

   		        return 0;
		    }
		    else
		    {
			    Status = LView::OnEvent(Msg) != 0;
			}
		    break;
		}
		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP) Msg->a;
			if (hDrop)
			{
				LArray<const char*> FileNames;
				int Count = 0;
				
				Count = DragQueryFileW(hDrop, -1, NULL, 0);

				for (int i=0; i<Count; i++)
				{
					char16 FileName[256];
					if (DragQueryFileW(hDrop, i, FileName, sizeof(FileName)-1) > 0)
					{
						FileNames.Add(WideToUtf8(FileName));
					}
				}

				OnReceiveFiles(FileNames);
				FileNames.DeleteArrays();
			}
			break;
		}
		case M_HANDLEMOUSEMOVE:
		{
			// This receives events fired from the LMouseHookPrivate class so that
			// non-LGI windows create mouse hook events as well.
			LTempView v((OsView)Msg->B());
			LMouse m;
			m.x = LOWORD(Msg->A());
			m.y = HIWORD(Msg->A());
			HandleViewMouse(&v, m);
			break;
		}
		case M_COMMAND:
		{
			HWND OurWnd = Handle(); // copy onto the stack, because
									// we might lose the 'this' object in the
									// OnCommand handler which would delete
									// the memory containing the handle.

			Status = OnCommand((int) Msg->a, 0, (OsView) Msg->b);
			if (!IsWindow(OurWnd))
			{
				// The window was deleted so break out now
				break;
			}
			// otherwise fall thru to the LView handler
		}
		default:
		{
			Status = (int) LView::OnEvent(Msg);
			break;
		}
	}

	return Status;
}

LPoint LWindow::GetDpi()
{
	if (!d->Dpi.x)
		d->Dpi = LGetDpiForWindow(_View);

	return d->Dpi;
}

LPointF LWindow::GetDpiScale()
{
	auto Dpi = GetDpi();
	LPointF r( Dpi.x / 96.0, Dpi.y / 96.0 );
	return r;
}

LRect &LWindow::GetPos()
{
	if (_View && IsZoomed(_View))
	{
		static LRect r;
		RECT rc;
		GetWindowRect(_View, &rc);
		r = rc;
		return r;
	}

	return Pos;
}

void LWindow::OnPosChange()
{
	PourAll();
}

bool LWindow::RegisterHook(LView *Target, LWindowHookType EventType, int Priority)
{
	bool Status = false;
	
	if (Target && EventType)
	{
		auto i = d->GetHookIndex(Target, true);
		if (i >= 0)
		{
			d->Hooks[i].Flags = EventType;
			Status = true;
		}
	}
	
	return Status;
}

LViewI *LWindow::GetDefault()
{
	return _Default;
}

void LWindow::SetDefault(LViewI *v)
{
	#if WINNATIVE
	LButton *Btn;
	if (Btn = dynamic_cast<LButton*>(_Default))
		Btn->Default(false);
	#endif
	_Default = v;
	#if WINNATIVE
	if (Btn = dynamic_cast<LButton*>(_Default))
		Btn->Default(true);
	#endif
}

bool LWindow::UnregisterHook(LView *Target)
{
	auto i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}

bool LWindow::SerializeState(LDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;

	#if DEBUG_SERIALIZE_STATE
	LgiTrace("LWindow::SerializeState(%p, %s, %i)\n", Store, FieldName, Load);
	#endif
	if (Load)
	{
		LVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			LRect Position(0, 0, -1, -1);
			LWindowZoom State = LZoomNormal;

			#if DEBUG_SERIALIZE_STATE
			LgiTrace("\t::SerializeState:%i v=%s\n", __LINE__, v.Str());
			#endif

			LToken t(v.Str(), ";");
			for (int i=0; i<t.Length(); i++)
			{
				char *Var = t[i];
				char *Value = strchr(Var, '=');
				if (Value)
				{
					*Value++ = 0;

					if (stricmp(Var, "State") == 0)
						State = (LWindowZoom)atoi(Value);
					else if (stricmp(Var, "Pos") == 0)
					{
					    LRect r;
					    r.SetStr(Value);
					    if (r.Valid())
						    Position = r;
					}
				}
				else return false;
			}

			#if DEBUG_SERIALIZE_STATE
			LgiTrace("\t::SerializeState:%i State=%i, Pos=%s\n", __LINE__, State, Position.GetStr());
			#endif

			// Apply any shortcut override
			int Show = LAppInst->GetShow();
			if (Show == SW_SHOWMINIMIZED ||
				Show == SW_SHOWMINNOACTIVE ||
				Show == SW_MINIMIZE)
			{
				State = LZoomMin;
			}
			else if (Show == SW_SHOWMAXIMIZED ||
					 Show == SW_MAXIMIZE)
			{
				State = LZoomMax;
			}

			LAutoPtr<WINDOWPLACEMENT> Wp(new WINDOWPLACEMENT);
			if (Wp)
			{
				ZeroObj(*Wp.Get());
				Wp->length = sizeof(WINDOWPLACEMENT);
				if (Visible())
				{
					if (State == LZoomMax)
					{
						Wp->showCmd = SW_SHOWMAXIMIZED;
					}
					else if (State == LZoomMin)
					{
						Wp->showCmd = SW_MINIMIZE;
					}
					else
					{
						Wp->showCmd = d->ShowCmd;
					}
				}
				else
				{
					Wp->showCmd = SW_HIDE;
					d->Show = State;
				}

				LRect DefaultPos(100, 100, 900, 700);
				if (Position.Valid())
				{
					LArray<GDisplayInfo*> Displays;
					LRect AllDisplays;
					bool PosOk = true;
					if (LGetDisplays(Displays, &AllDisplays))
					{
						// Check that the position is on one of the screens
						PosOk = false;
						for (unsigned i=0; i<Displays.Length(); i++)
						{
							LRect Int = Displays[i]->r;
							Int.Intersection(&Position);
							if (Int.Valid() &&
								Int.X() > 20 &&
								Int.Y() > 20)
							{
								PosOk = true;
								break;
							}
						}
						Displays.DeleteObjects();
					}

					if (PosOk)
						Pos = Position;
					else
						Pos = DefaultPos;
				}
				else Pos = DefaultPos;

				Wp->rcNormalPosition = Pos;
				#if DEBUG_SERIALIZE_STATE
				LgiTrace("%s:%i - SetWindowPlacement, pos=%s, show=%i\n", _FL, Pos.GetStr(), Wp->showCmd);
				#endif
				SetWindowPlacement(Handle(), Wp);

				if (d->InCreate)
					d->Wp = Wp;
			}
		}
		else return false;
	}
	else
	{
		char s[256];
		LWindowZoom State = GetZoom();
		LRect Position;
		
		if (Handle())
		{
		    WINDOWPLACEMENT Wp;
		    ZeroObj(Wp);
		    Wp.length = sizeof(Wp);
		    GetWindowPlacement(Handle(), &Wp);
		    Position = Wp.rcNormalPosition;
		}
		else
		{
		    // A reasonable fall back if we don't have a window...
		    Position = GetPos();
		}
		
		sprintf_s(s, sizeof(s), "State=%i;Pos=%s", State, Position.GetStr());

		#if DEBUG_SERIALIZE_STATE
		LgiTrace("\t::SerializeState:%i s='%s'\n", __LINE__, s);
		#endif

		LVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
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
			int Result = RClick.Float(this, m);
			#if WINNATIVE
			PostMessage(Handle(), WM_NULL, 0, 0);
			#endif
			OnTrayMenuResult(Result);
		}
	}
}
