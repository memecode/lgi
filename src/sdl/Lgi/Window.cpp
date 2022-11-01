#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "LPopup.h"
#include "LPanel.h"

#define DEBUG_SETFOCUS			0
#define DEBUG_HANDLEVIEWKEY		0

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	LWindowHookType Flags;
	LView *Target;
};

class GWindowPrivate
{
public:
	int Sx, Sy;
	bool Dynamic;
	LKey LastKey;
	::LArray<HookInfo> Hooks;
	bool SnapToEdge;
	::LString Icon;
	
	// State
	bool HadCreateEvent;
	
	// Focus stuff
	OsView FirstFocus;
	LViewI *Focus;
	bool Active;

	GWindowPrivate()
	{
		FirstFocus = NULL;
		Focus = NULL;
		Active = false;
		HadCreateEvent = false;
		
		Sx = Sy = 0;
		Dynamic = true;
		SnapToEdge = false;
		// ZeroObj(LastKey);
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
};

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

LWindow::LWindow()
{
	d = new GWindowPrivate;
	_QuitOnClose = false;
	Menu = NULL;
	Wnd = NULL;
	_Window = this;
	_Default = NULL;
	WndFlags |= GWND_CREATE;
	ClearFlag(WndFlags, GWF_VISIBLE);

    _Lock = new ::LMutex;
}

LWindow::~LWindow()
{
	if (LAppInst->AppWnd == this)
	{
		LAppInst->AppWnd = 0;
	}

	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

bool LWindow::SetIcon(const char *FileName)
{
	LAutoString a;
	if (Wnd)
	{
		if (!FileExists(FileName))
		{
			if (a.Reset(LgiFindFile(FileName)))
				FileName = a;
		}

		if (!FileExists(FileName))
		{
			LgiTrace("%s:%i - SetIcon failed to find '%s'\n", _FL);
			return false;
		}
		else
		{		
		}
	}
	
	if (FileName != d->Icon.Get())
		d->Icon = FileName;

	return d->Icon != NULL;
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
	return d->Active;
}

bool LWindow::Visible()
{
	return LView::Visible();
}

void LWindow::Visible(bool i)
{
	ThreadCheck();
	LView::Visible(i);
}

bool LWindow::Obscured()
{
	return	false;
}

void LWindow::_SetDynamic(bool i)
{
	d->Dynamic = i;
}

void LWindow::_OnViewDelete()
{
	if (d->Dynamic)
	{
		delete this;
	}
}

bool LWindow::Attach(LViewI *p)
{
	bool Status = false;

	ThreadCheck();
	
	{		
		// Do a rough layout of child windows
		PourAll();

		// Setup default button...
		if (!_Default)
		{
			_Default = FindControl(IDOK);
		}
		if (_Default)
		{
			_Default->Invalidate();
		}
		
		// Call on create
		OnCreate();

		// Add icon
		if (d->Icon)
		{
			SetIcon(d->Icon);
			d->Icon.Empty();
		}
		
		Status = true;
	}
	
	return Status;
}

bool LWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LCloseApp();
	}

	return LView::OnRequestClose(OsShuttingDown);
}

bool LWindow::HandleViewMouse(LView *v, LMouse &m)
{
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

#define DEBUG_KEYS			0

/*
	// Any window in a popup always gets the key...
	for (LView *p = v; p; p = p->GetParent())
	{
		LPopup *Popup;
		if (Popup = dynamic_cast<LPopup*>(p))
		{
			Status = v->OnKey(k);
			if (!Status)
			{
				if (k.c16 == VK_ESCAPE)
				{
					// Popup window (or child) didn't handle the key, and the user
					// pressed ESC, so do the default thing and close the popup.
					Popup->Cancelled = true;
					Popup->Visible(false);
				}
				else
				{
					#if DEBUG_KEYS
					printf("Popup ignores '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
					#endif
					break;
				}
			}
			
			#if DEBUG_KEYS
			printf("Popup ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
			
			goto AllDone;
		}
	}
*/

bool LWindow::HandleViewKey(LView *v, LKey &k)
{
	bool Status = false;
	LViewI *Ctrl = 0;
	
	#if DEBUG_HANDLEVIEWKEY
	bool Debug = 1; // k.vkey == VK_RETURN;
	char SafePrint = k.c16 < ' ' ? ' ' : k.c16;
	
	if (Debug)
	{
		printf("%s::HandleViewKey=%i ischar=%i %s%s%s%s\n",
			v->GetClass(),
			k.c16,
			k.IsChar,
			(char*)(k.Down()?" Down":" Up"),
			(char*)(k.Shift()?" Shift":""),
			(char*)(k.Alt()?" Alt":""),
			(char*)(k.Ctrl()?" Ctrl":""));
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
				printf("Sending key to popup\n");
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
			printf("MouseHook got key\n");
		#endif
		goto AllDone;
	}

	// Allow any hooks to see the key...
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & LKeyEvents)
		{
			if (d->Hooks[i].Target->OnViewKey(v, k))
			{
				Status = true;
				
				#if DEBUG_HANDLEVIEWKEY
				if (Debug)
					printf("Hook ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
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
			printf("View ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
		#endif
		
		Status = true;
		goto AllDone;
	}
	#if DEBUG_HANDLEVIEWKEY
	else if (Debug)
		printf("%s didn't eat '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
			v->GetClass(), SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
	#endif
	
	// Window didn't want the key...
	switch (k.vkey)
	{
		case VK_RETURN:
		#ifdef VK_KP_ENTER
		case VK_KP_ENTER:
		#endif
		{
			Ctrl = _Default;
			break;
		}
		case VK_ESCAPE:
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
					printf("Default Button ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
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
		printf("No default ctrl to handle key.\n");
	#endif
	
	if (Menu)
	{
		Status = Menu->OnKey(v, k);
		if (Status)
		{
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				printf("Menu ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
		}
	}
	
	// Tab through controls
	if (k.vkey == VK_TAB && k.Down() && !k.IsChar)
	{
		LViewI *Wnd = GetNextTabStop(v, k.Shift());
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			printf("Tab moving focus shift=%i Wnd=%p\n", k.Shift(), Wnd);
		#endif
		if (Wnd)
			Wnd->Focus(true);
	}

AllDone:
	d->LastKey = k;

	return Status;
}


void LWindow::Raise()
{
	/*
	if (_View->handle())
	{
		#if defined XWIN
		XWindowChanges c;
		c.stack_mode = XAbove;
		XConfigureWindow(	_View->XDisplay(),
							_View->handle(),
							CWStackMode,
							&c);
		#endif							
	}
	*/
}


GWindowZoom LWindow::GetZoom()
{
	return LZoomNormal;
}

void LWindow::SetZoom(GWindowZoom i)
{
	if (Wnd)
	{
		ThreadCheck();

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
	if (Wnd)
	{
		ThreadCheck();
	}

	return LBase::Name(n);
}

char *LWindow::Name()
{
	return LBase::Name();
}

struct CallbackParams
{
	LRect Menu;
	int Depth;
	
	CallbackParams()
	{
		Menu.ZOff(-1, -1);
		Depth = 0;
	}
};

LRect &LWindow::GetClient(bool ClientSpace)
{
	static LRect r;
	r = LView::GetClient(ClientSpace);
	if (Wnd)
	{
	}
	return r;
}

bool LWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;

	if (Load)
	{
		::LVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			LRect Position(0, 0, -1, -1);
			GWindowZoom State = LZoomNormal;

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
						State = (GWindowZoom) atoi(Value);
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
		GWindowZoom State = GetZoom();
		sprintf(s, "State=%i;Pos=%s", State, GetPos().GetStr());

		::LVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
}

LRect &LWindow::GetPos()
{
	if (Wnd && _View)
	{
		/*
		OsRect r = _View->geometry();

		Pos.x1 = r.left;
		Pos.y1 = r.top;
		Pos.x2 = r.right;
		Pos.y2 = r.bottom;

		#if defined XWIN
		XWindowAttributes a;
		ZeroObj(a);
		XGetWindowAttributes(Handle()->XDisplay(), Handle()->handle(), &a);
		if (!a.override_redirect)
		{
			int Dx, Dy;
			_View->GetDecorationSize(Dx, Dy);
			Pos.Offset(Dx, Dy);
		}
		#endif
		*/
	}

	return Pos;
}

bool LWindow::SetPos(LRect &p, bool Repaint)
{
	Pos = p;
	if (Wnd)
	{
		ThreadCheck();
	}
	return true;
}

void LWindow::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	// Force repour
	d->Sx = d->Sy = -1;
}

void LWindow::OnCreate()
{
}

void LWindow::OnPaint(LSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

void LWindow::OnPosChange()
{
	LView::OnPosChange();

	//if (d->Sx != X() ||	d->Sy != Y())
	{
		PourAll();
		//d->Sx = X();
		//d->Sy = Y();
	}
}

#define IsTool(v) \
	( \
		dynamic_cast<LView*>(v) \
		&& \
		dynamic_cast<LView*>(v)->_IsToolBar \
	)

void LWindow::PourAll()
{
	LRect Cli = GetClient();
	if (!Cli.Valid())
		return;
	LRegion Client(Cli);
	LViewI *MenuView = 0;

	LRegion Update(Client);
	bool HasTools = false;
	LViewI *v;
	auto Lst = Children.begin();

	{
		LRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			bool IsMenu = MenuView == v;
			if (!IsMenu && IsTool(v))
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

						if (OldPos != v->GetPos())
						{
							// position has changed update...
							v->Invalidate();
						}

						Tools.Subtract(&v->GetPos());
						Update.Subtract(&v->GetPos());
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

	Lst = Children.begin();
	for (LViewI *v = *Lst; v; v = *++Lst)
	{
		bool IsMenu = MenuView == v;
		if (!IsMenu && !IsTool(v))
		{
			LRect OldPos = v->GetPos();
			Update.Union(&OldPos);

			if (v->Pour(Client))
			{
				LRect p = v->GetPos();

				if (!v->Visible())
				{
					v->Visible(true);
				}

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
}

/*
int LWindow::WillAccept(List<char> &Formats, LPoint Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;
	
	for (char *f=Formats.First(); f; )
	{
		if (stricmp(f, LGI_FileDropFormat) == 0)
		{
			f = Formats.Next();
			Status = DROPEFFECT_COPY;
		}
		else
		{
			Formats.Delete(f);
			DeleteArray(f);
			f = Formats.Current();
		}
	}
	
	return Status;
}

int LWindow::OnDrop(char *Format, ::LVariant *Data, LPoint Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;

	if (Format && Data)
	{
		if (stricmp(Format, LGI_FileDropFormat) == 0)
		{
			::LArray<char*> Files;
			if (Data->IsBinary())
			{
				GToken Uri(	(char*)Data->Value.Binary.Data,
							"\r\n,",
							true,
							Data->Value.Binary.Length);
				for (int i=0; i<Uri.Length(); i++)
				{
					char *File = Uri[i];
					if (strnicmp(File, "file:", 5) == 0)
						File += 5;
					
					char *in = File, *out = File;
					while (*in)
					{
						if (in[0] == '%' &&
							in[1] &&
							in[2])
						{
							char h[3] = { in[1], in[2], 0 };
							*out++ = htoi(h);
							in += 3;
						}
						else
						{
							*out++ = *in++;
						}
					}
					*out++ = 0;
					
					if (FileExists(File))
					{
						Files.Add(NewStr(File));
					}
				}
			}
			
			if (Files.Length())
			{
				Status = DROPEFFECT_COPY;
				OnReceiveFiles(Files);
				Files.DeleteArrays();
			}
		}
	}
	
	return Status;
}
*/

LMessage::Result LWindow::OnEvent(LMessage *m)
{
	switch (MsgCode(m))
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

void LWindow::OnFrontSwitch(bool b)
{
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

LViewI *LWindow::GetFocus()
{
	return d->Focus ? d->Focus : this;
}

bool LWindow::PushWindow(LWindow *v)
{
	return LAppInst->PushWindow(v);
}

LWindow *LWindow::PopWindow()
{
	return LAppInst->PopWindow();
}

#if DEBUG_SETFOCUS
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
	for (int n=min(3, p.Length()-1); n>=0; n--)
	{
		char Buf[256] = "";
		if (!stricmp(v->GetClass(), "LMdiChild"))
			sprintf(Buf, "'%s'", v->Name());
		v = p[n];
		
		ch += sprintf_s(s + ch, sizeof(s) - ch, "%s>%s", Buf, v->GetClass());
	}
	return LAutoString(NewStr(s));
}
#endif

void LWindow::SetFocus(LViewI *ctrl, FocusType type)
{
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
				LView *gv = d->Focus->GetGView();
				if (gv)
				{
					#if DEBUG_SETFOCUS
					LAutoString _set = DescribeView(d->Focus);
					LgiTrace("LWindow::SetFocus(%s, %s) focusing LView %p\n",
						_set.Get(),
						TypeName,
						d->Focus->Handle());
					#endif

					gv->_Focus(true);
				}
				else if (IsActive())
				{			
					#if DEBUG_SETFOCUS
					LAutoString _set = DescribeView(d->Focus);
					LgiTrace("LWindow::SetFocus(%s, %s) focusing nonGView %p (active=%i)\n",
						_set.Get(),
						TypeName,
						d->Focus->Handle(),
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
}

void LWindow::SetDragHandlers(bool On)
{
}

void LWindow::OnTrayClick(LMouse &m)
{
	if (m.Down() || m.IsContextMenu())
	{
		GSubMenu RClick;
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

