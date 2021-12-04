#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Token.h"
#include "lgi/common/Popup.h"
#include "lgi/common/Panel.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Menu.h"
#include "ViewPriv.h"

#define DEBUG_SETFOCUS			0
#define DEBUG_HANDLEVIEWKEY		0

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
	
	LWindowPrivate(LWindow *wnd) :
		Wnd(wnd),
		BWindow(BRect(100,100,400,400),
				"...loading...",
				B_DOCUMENT_WINDOW_LOOK,
				B_NORMAL_WINDOW_FEEL,
				0)
	{
	}

	~LWindowPrivate()
	{
		Wnd->d = NULL;		
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

	void FrameMoved(BPoint newPosition)
	{
		auto Pos = Frame();
		if (Pos != Wnd->Pos)
		{
			Wnd->Pos = Pos;
			printf("%s:%i - LWindow::OnPosChange %s\n", _FL, Wnd->Pos.GetStr());
			Wnd->OnPosChange();
		}
		else printf("%s:%i FrameMoved no change.\n", _FL);
	}

	void FrameResized(float newWidth, float newHeight)
	{
		auto Pos = Frame();
		if (Pos != Wnd->Pos)
		{
			Wnd->Pos = Pos;
			printf("%s:%i - LWindow::OnPosChange %s\n", _FL, Wnd->Pos.GetStr());
			Wnd->OnPosChange();
		}
		else printf("%s:%i FrameResized no change.\n", _FL);
	}

	bool QuitRequested()
	{
		return Wnd->OnRequestClose(false);
	}

	void MessageReceived(BMessage *message)
	{
		Wnd->OnEvent(message);
		BWindow::MessageReceived(message);
	}
};

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

LWindow::LWindow() : LView(0)
{
	d = new LWindowPrivate(this);
	_QuitOnClose = false;
	Menu = NULL;
	
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	ClearFlag(WndFlags, GWF_VISIBLE);

    _Lock = new ::LMutex;
}

LWindow::~LWindow()
{
	if (LAppInst->AppWnd == this)
		LAppInst->AppWnd = NULL;

	DeleteObj(Menu);
	DeleteObj(_Lock);

	if (d)
	{
		d->Quit();
		d = NULL;
	}
}

OsWindow LWindow::WindowHandle()
{
	return d;
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
	if (!lck.Lock())
		return false;	
	return !d->IsHidden();
}

void LWindow::Visible(bool i)
{
	LLocker lck(d, _FL);
	if (!lck.Lock())
	{
		printf("%s:%i - Can't lock.\n", _FL);
		return;
	}
	
	if (i)
		d->Show();
	else
		d->Hide();
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

bool LWindow::Attach(LViewI *p)
{
	LLocker lck(d, _FL);
	if (!lck.Lock())
		return false;
	
	// Setup default button...
	if (!_Default)
		_Default = FindControl(IDOK);

	// Do a rough layout of child windows
	PourAll();

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
	return Status;
}


void LWindow::Raise()
{
}

LWindowZoom LWindow::GetZoom()
{
	LLocker lck(d, _FL);
	if (lck.Lock())
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

LPoint LWindow::GetDpi()
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
	r = LView::GetClient(ClientSpace);
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


			GToken t(v.Str(), ";");
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

	LLocker lck(d, _FL);
	if (lck.Lock())
	{
		d->MoveTo(Pos.x1, Pos.y1);
		d->ResizeTo(Pos.X(), Pos.Y());
	}

	return true;
}

void LWindow::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	// Force repour
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

	printf("PourAll %s\n", c.GetStr());

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
				printf("tools pour pos = %s\n", OldPos.GetStr());
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
	return NULL;
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
		if (!stricmp(v->GetClass(), "GMdiChild"))
			sprintf(Buf, "'%s'", v->Name());
		v = p[n];
		
		ch += sprintf_s(s + ch, sizeof(s) - ch, "%s>%s", Buf, v->GetClass());
	}
	return LAutoString(NewStr(s));
}
#endif

void LWindow::SetFocus(LViewI *ctrl, FocusType type)
{
	/*
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
				LView *gv = d->Focus->GetGView();
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
				
				LView *gv = d->Focus->GetGView();
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
	*/
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

void LWindow::SetAlwaysOnTop(bool b)
{
}
