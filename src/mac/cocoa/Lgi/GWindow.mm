#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "GPopup.h"
#include "GDisplayString.h"
#include "LCocoaView.h"

extern void NextTabStop(GViewI *v, int dir);
extern void SetDefaultFocus(GViewI *v);

#define DEBUG_KEYS			0

GRect LScreenFlip(GRect r)
{
	GRect screen(0, 0, -1, -1);
	for (NSScreen *s in [NSScreen screens])
	{
		GRect pos = s.frame;
		if (r.Overlap(&pos))
		{
			screen = pos;
			break;
		}
	}
	
	if (screen.Valid())
	{
		GRect rc = r;
		rc.Offset(0, (screen.Y() - r.y1 - r.Y()) - r.y1);
		// printf("%s:%i - Flip %s -> %s (%s)\n", _FL, r.GetStr(), rc.GetStr(), screen.GetStr());
		return rc;
	}
	else
	{
		// printf("%s:%i - No Screen?\n", _FL);
		r.ZOff(-1, -1);
	}
	
	return r;
}

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	int Flags;
	GView *Target;
};

@interface LWindowDelegate : NSObject <NSWindowDelegate>
{
}

- (id)init;
- (void)dealloc;
- (void)windowDidResize:(NSNotification*)aNotification;
- (void)windowDidMove:(NSNotification*)aNotification;
- (void)windowWillClose:(NSNotification*)aNotification;
- (BOOL)windowShouldClose:(id)sender;
- (void)windowDidBecomeMain:(NSNotification*)notification;
- (void)windowDidResignMain:(NSNotification*)notification;

@end

LWindowDelegate *Delegate = nil;

class GWindowPrivate
{
public:
	GWindow *Wnd;
	GDialog *ChildDlg;
	LMenu *EmptyMenu;
	GViewI *Focus;

	int Sx, Sy;

	GKey LastKey;
	GArray<HookInfo> Hooks;

	uint64 LastMinimize;
	uint64 LastDragDrop;

	bool Dynamic;
	bool SnapToEdge;
	bool DeleteWhenDone;
	bool InitVisible;
	bool CloseRequestDone;
	bool Closing;
	
	GWindowPrivate(GWindow *wnd)
	{
		Focus = NULL;
		InitVisible = false;
		LastMinimize = 0;
		Wnd = wnd;
		LastDragDrop = 0;
		DeleteWhenDone = false;
		ChildDlg = 0;
		Sx = Sy = -1;
		Dynamic = true;
		SnapToEdge = false;
		EmptyMenu = 0;
		CloseRequestDone = false;
		Closing = false;
	}
	
	~GWindowPrivate()
	{
		DeleteObj(EmptyMenu);
	}
	
	ssize_t GetHookIndex(GView *Target, bool Create = false)
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
				return Hooks.Length() - 1;
			}
		}
		
		return -1;
	}
	
	void OnResize()
	{
		NSWindow *wnd = Wnd->WindowHandle().p;
		
		Wnd->Pos = wnd.frame;
		Wnd->OnPosChange();

		wnd.contentView.needsLayout = YES;
	}
};

@implementation LNsWindow

- (id)init:(GWindowPrivate*)priv Frame:(NSRect)rc
{
	NSUInteger windowStyleMask = NSTitledWindowMask | NSResizableWindowMask |
								 NSClosableWindowMask | NSMiniaturizableWindowMask;
	if ((self = [super initWithContentRect:rc
					styleMask:windowStyleMask
					backing:NSBackingStoreBuffered
					defer:NO ]) != nil)
	{
		self.d = priv;
		
		self.contentView = [[LCocoaView alloc] init:priv->Wnd];
		[self makeFirstResponder:self.contentView];
		self.acceptsMouseMovedEvents = true;
		self.ignoresMouseEvents = false;
		
		printf("LNsWindow.init\n");
	}
	return self;
}

- (void)dealloc
{
	if (self.d)
		self.d->Wnd->OnDealloc();

	// LAutoPool Ap;
	LCocoaView *cv = objc_dynamic_cast(LCocoaView, self.contentView);
	cv.w = NULL;
	[cv release];
	self.contentView = NULL;
	
	[super dealloc];
	printf("LNsWindow.dealloc.\n");
}

- (GWindow*)getWindow
{
	return self.d ? self.d->Wnd : nil;
}

- (BOOL)canBecomeKeyWindow
{
	return YES;
}

@end

@implementation LWindowDelegate

- (id)init
{
    if ((self = [super init]) != nil)
    {
    }
    return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void)windowDidResize:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w && w.d)
		w.d->OnResize();
}

- (void)windowDidMove:(NSNotification*)event
{
	// LNsWindow *w = event.object;
	// GRect r = LScreenFlip(w.frame);
	// printf("windowDidMove: %s\n", r.GetStr());
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
	LNsWindow *w = objc_dynamic_cast(LNsWindow, sender);
	if (w && w.d && w.d->Wnd)
		return w.d->Wnd->OnRequestClose(false);
	
	return YES;
}

- (void)windowWillClose:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w)
	{
	 	if (w.d)
	 	{
			//printf("%s:%i - windowWillClose(%s) w.d->Closing=%i\n",
			//	_FL, w.d->Wnd->GetClass(), w.d->Closing);
			
	 		if (w.d->Closing)
	 		{
	 			auto gwnd = w.d->Wnd;
	 			w.d = NULL;
	 			delete gwnd;
			}
	 		else
	 		{
				w.d->Wnd->Quit();
			}
		}
		// else printf("%s:%i - w.d is NULL\n", _FL);
	}
}

- (void)windowDidBecomeMain:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w && w.d)
		w.d->Wnd->OnFrontSwitch(true);
}

- (void)windowDidResignMain:(NSNotification*)event
{
	LNsWindow *w = event.object;
	if (w && w.d)
		w.d->Wnd->OnFrontSwitch(false);
}

@end

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

#if __has_feature(objc_arc)
#error "NO ARC!"
#endif

GWindow::GWindow(OsWindow wnd) : GView(NULL)
{
	d = new GWindowPrivate(this);
	_QuitOnClose = false;
	Wnd = NULL;
	Menu = 0;
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	GView::Visible(false);
	
	_Lock = new LMutex;
	
	// LAutoPool Pool;
	
	GRect pos(200, 200, 200, 200);
	NSRect frame = pos;
	if (wnd)
		Wnd = wnd;
	else
		Wnd.p = [[LNsWindow alloc] init:d Frame:frame];
	if (Wnd)
	{
		if (!Delegate)
			Delegate = [[LWindowDelegate alloc] init];
		//[Wnd.p makeKeyAndOrderFront:NSApp];
		Wnd.p.delegate = Delegate;
	}
}

GWindow::~GWindow()
{
	// LAutoPool Pool;
	if (LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}
	
	_Delete();
	
	if (Wnd)
	{
		LCocoaView *cv = objc_dynamic_cast(LCocoaView, Wnd.p.contentView);
		if (cv)
			cv.w = NULL;

		LNsWindow *w = objc_dynamic_cast(LNsWindow, Wnd.p);
		if (w)
			w.d = NULL;

		Wnd.p.delegate = nil;
		if (!d->Closing)
			[Wnd.p close];
		[Wnd.p release];
		Wnd = nil;
	}
	
	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

NSView *GWindow::Handle()
{
	if (Wnd.p != nil)
		return Wnd.p.contentView; //Wnd.p.contentViewController.view;
	
	return NULL;
}

bool &GWindow::CloseRequestDone()
{
	return d->CloseRequestDone;
}

bool GWindow::SetIcon(const char *FileName)
{
	return false;
}

GViewI *GWindow::GetFocus()
{
	return d->Focus;
}

void GWindow::SetFocus(GViewI *ctrl, FocusType type)
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
			// Check if the control already has focus
			if (d->Focus == ctrl)
				return;
			
			if (d->Focus)
			{
				GView *v = d->Focus->GetGView();
				if (v) v->WndFlags &= ~GWF_FOCUS;
				d->Focus->OnFocus(false);
				d->Focus->Invalidate();
				
#if DEBUG_SETFOCUS
				GAutoString _foc = DescribeView(d->Focus);
				LgiTrace(".....defocus: %s\n",
						 _foc.Get());
#endif
			}
			
			d->Focus = ctrl;
			
			if (d->Focus)
			{
				GView *v = d->Focus->GetGView();
				if (v) v->WndFlags |= GWF_FOCUS;
				d->Focus->OnFocus(true);
				d->Focus->Invalidate();
				
#if DEBUG_SETFOCUS
				GAutoString _set = DescribeView(d->Focus);
				LgiTrace("GWindow::SetFocus(%s, %s) focusing\n",
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
				GView *v = d->Focus->GetGView();
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
						GAutoString _ctrl = DescribeView(ctrl);
						GAutoString _foc = DescribeView(d->Focus);
						LgiTrace("GWindow::SetFocus(%s, %s) keep_focus: %s\n",
								 _ctrl.Get(),
								 TypeName,
								 _foc.Get());
#endif
					}
					// else view doesn't think it has focus anyway...
				}
				else
				{
					// Non GView handler
					d->Focus->OnFocus(false);
					d->Focus->Invalidate();
					d->Focus = NULL;
				}
			}
			else
			{
				/*
				 LgiTrace("GWindow::SetFocus(%p.%s, %s) error on losefocus: %p(%s)\n",
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
				LgiTrace("GWindow::SetFocus(%p.%s, %s) delete_focus: %p(%s)\n",
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

void GWindow::SetDragHandlers(bool On)
{
	#if 0
	if (Wnd && _View)
		SetAutomaticControlDragTrackingEnabledForWindow(Wnd, On);
	#endif
}

void GWindow::Quit(bool DontDelete)
{
	// LAutoPool Pool;
	if (_QuitOnClose)
	{
		_QuitOnClose = false;
		LgiCloseApp();
	}
	
	if (d)
		d->DeleteWhenDone = !DontDelete;
	
	if (Wnd)
	{
		SetDragHandlers(false);
		if (d->Closing)
		{
			PostEvent(M_DESTROY);
		}
		else
		{
			d->Closing = true;
			PostEvent(M_CLOSE);
		}
	}
}

void GWindow::SetChildDialog(GDialog *Dlg)
{
	d->ChildDlg = Dlg;
}

bool GWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void GWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

void GWindow::OnFrontSwitch(bool b)
{
}

bool GWindow::Visible()
{
	// LAutoPool Pool;
	if (!Wnd)
		return false;
	
	return [Wnd.p isVisible];
}

void GWindow::Visible(bool i)
{
	// LAutoPool Pool;
	if (!Wnd)
		return;

	if (i)
	{
		d->InitVisible = true;
		PourAll();

		[Wnd.p makeKeyAndOrderFront:NULL];
		[NSApp activateIgnoringOtherApps:YES];

		SetDefaultFocus(this);
		OnPosChange();
	}
	else
	{
		[Wnd.p orderOut:Wnd.p];
	}
}

void GWindow::_SetDynamic(bool i)
{
	d->Dynamic = i;
}

void GWindow::_OnViewDelete()
{
	if (d->Dynamic)
	{
		delete this;
	}
}

void GWindow::SetAlwaysOnTop(bool b)
{
}

bool GWindow::PostEvent(int Event, GMessage::Param a, GMessage::Param b)
{
	return LgiApp->PostEvent(this, Event, a, b);
}

bool GWindow::Attach(GViewI *p)
{
	bool Status = false;
	
	if (Wnd)
	{
		if (GBase::Name())
			Name(GBase::Name());
		
		Status = true;
		
		// Setup default button...
		if (!_Default)
		{
			_Default = FindControl(IDOK);
			if (_Default)
				_Default->Invalidate();
		}
		
		OnCreate();
		OnAttach();
		OnPosChange();
		
		// Set the first control as the focus...
		NextTabStop(this, 0);
	}
	
	return Status;
}

bool GWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LgiCloseApp();
	}
	
	return GView::OnRequestClose(OsShuttingDown);
}

bool GWindow::HandleViewMouse(GView *v, GMouse &m)
{
#ifdef MAC
	if (m.Down())
	{
#if 0
		
		GAutoPtr<GViewIterator> it(IterateViews());
		for (GViewI *n = it->Last(); n; n = it->Prev())
		{
			GPopup *p = dynamic_cast<GPopup*>(n);
			if (p)
			{
				GRect pos = p->GetPos();
				if (!pos.Overlap(m.x, m.y))
				{
					printf("Closing Popup, m=%i,%i not over pos=%s\n", m.x, m.y, pos.GetStr());
					p->Visible(false);
				}
			}
			else break;
		}
		
#else
		
		bool ParentPopup = false;
		GViewI *p = m.Target;
		while (p && p->GetParent())
		{
			if (dynamic_cast<GPopup*>(p))
			{
				ParentPopup = true;
				break;
			}
			p = p->GetParent();
		}
		if (!ParentPopup)
		{
			for (int i=0; i<GPopup::CurrentPopups.Length(); i++)
			{
				GPopup *pu = GPopup::CurrentPopups[i];
				if (pu->Visible())
					pu->Visible(false);
			}
		}
		
#endif
		
		if (!m.IsMove() && LgiApp)
		{
			GMouseHook *mh = LgiApp->GetMouseHook();
			if (mh)
				mh->TrackClick(v);
		}
	}
#endif
	
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GMouseEvents)
		{
			if (!d->Hooks[i].Target->OnViewMouse(v, m))
			{
				return false;
			}
		}
	}
	
	return true;
}


bool GWindow::HandleViewKey(GView *v, GKey &k)
{
	bool Status = false;
	GViewI *Ctrl = NULL;
	if (!v && d->Focus)
		v = d->Focus->GetGView();
	if (!v)
		return false;
	
	// Give key to popups
	if (LgiApp &&
		LgiApp->GetMouseHook() &&
		LgiApp->GetMouseHook()->OnViewKey(v, k))
	{
		goto AllDone;
	}
	
	// Allow any hooks to see the key...
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GKeyEvents)
		{
			if (d->Hooks[i].Target->OnViewKey(v, k))
			{
				Status = true;
				
#if DEBUG_KEYS
				printf("Hook ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
				
				goto AllDone;
			}
		}
	}
	
	// Give the key to the window...
	if (v->OnKey(k))
	{
#if DEBUG_KEYS
		printf("View ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
			   k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
		
		Status = true;
		goto AllDone;
	}
	
	// Window didn't want the key...
	switch (k.c16)
	{
		case LK_RETURN:
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
	
	if (Ctrl && Ctrl->Enabled())
	{
		if (Ctrl->OnKey(k))
		{
			Status = true;
			
#if DEBUG_KEYS
			printf("Default Button ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				   k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
			
			goto AllDone;
		}
	}
	
	if (Menu)
	{
		Status = Menu->OnKey(v, k);
		if (Status)
		{
#if DEBUG_KEYS
			printf("Menu ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
		}
	}
	
	// Command+W closes the window... if it doesn't get nabbed earlier.
	if (k.Down() &&
		k.System() &&
		tolower(k.c16) == 'w')
	{
		// Close
		if (d->CloseRequestDone || OnRequestClose(false))
		{
			if (d) d->CloseRequestDone = true;
			delete this;
			return true;
		}
	}
	
AllDone:
	d->LastKey = k;
	
	return Status;
}


void GWindow::Raise()
{
	if (Wnd)
	{
		// BringToFront(Wnd);
	}
}

GWindowZoom GWindow::GetZoom()
{
	if (Wnd)
	{
		#if 0
		bool c = IsWindowCollapsed(Wnd);
		// printf("IsWindowCollapsed=%i\n", c);
		if (c)
			return GZoomMin;
		
		c = IsWindowInStandardState(Wnd, NULL, NULL);
		// printf("IsWindowInStandardState=%i\n", c);
		if (!c)
			return GZoomMax;
		#endif
	}
	
	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	#if 0
	OSStatus e = 0;
	switch (i)
	{
		case GZoomMin:
		{
			e = CollapseWindow(Wnd, true);
			if (e) printf("%s:%i - CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("GZoomMin ok.\n");
			break;
		}
		default:
		case GZoomNormal:
		{
			e = CollapseWindow(Wnd, false);
			if (e) printf("%s:%i - [Un]CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("GZoomNormal ok.\n");
			break;
		}
	}
	#endif
}

GViewI *GWindow::GetDefault()
{
	return _Default;
}

void GWindow::SetDefault(GViewI *v)
{
	if (v &&
		v->GetWindow() == (GViewI*)this)
	{
		if (_Default != v)
		{
			GViewI *Old = _Default;
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

bool GWindow::Name(const char *n)
{
	// LAutoPool Pool;
	bool Status = GBase::Name(n);
	
	if (Wnd)
	{
		NSString *ns = [NSString stringWithCString:n encoding:NSUTF8StringEncoding];
		Wnd.p.title = ns;
		//[ns release];
	}
	
	return Status;
}

char *GWindow::Name()
{
	return GBase::Name();
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	// LAutoPool Pool;
	static GRect r;
	if (Wnd)
	{
		r = Wnd.p.contentView.frame;
		if (ClientSpace)
			r.Offset(-r.x1, -r.y1);
	}
	else
	{
		r.ZOff(-1, -1);
	}
	return r;
}

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;
	
	if (Load)
	{
		GVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			GRect Position(0, 0, -1, -1);
			GWindowZoom State = GZoomNormal;
			
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
				// int Sy = GdcD->Y();
				// Position.y2 = min(Position.y2, Sy - 50);
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
		
		GVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}
	
	return true;
}

GRect &GWindow::GetPos()
{
	// LAutoPool Pool;

	if (Wnd)
	{
		Pos = LScreenFlip(Wnd.p.frame);
		
		// printf("%s::GetPos %s\n", GetClass(), Pos.GetStr());
	}
	
	return Pos;
}

bool GWindow::SetPos(GRect &p, bool Repaint)
{
	// LAutoPool Pool;
	
	Pos = p;
	if (Wnd)
	{
		GRect r = LScreenFlip(p);
		[Wnd.p setFrame:r display:YES];
		
		// printf("%s::SetPos %s\n", GetClass(), Pos.GetStr());
	}

	return true;
}

void GWindow::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (dynamic_cast<GPopup*>(Wnd))
	{
		printf("%s:%i - Ignoring GPopup in OnChildrenChanged handler.\n", _FL);
		return;
	}
	PourAll();
}

void GWindow::OnCreate()
{
}

void GWindow::OnPaint(GSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
}

void GWindow::OnPosChange()
{
	GView::OnPosChange();
	
	if (d->Sx != X() ||	d->Sy != Y())
	{
		PourAll();
		d->Sx = X();
		d->Sy = Y();
	}
}

#define IsTool(v) \
( \
	dynamic_cast<GView*>(v) \
	&& \
	dynamic_cast<GView*>(v)->_IsToolBar \
)

void GWindow::PourAll()
{
	GRect r = GetClient();
	// printf("::Pour r=%s\n", r.GetStr());
	GRegion Client(r);
	
	GRegion Update(Client);
	bool HasTools = false;
	GViewI *v;
	List<GViewI>::I Lst = Children.begin();
	
	{
		GRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			if (IsTool(v))
			{
				GRect OldPos = v->GetPos();
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
						
						GRect Bar(v->GetPos());
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
	for (GViewI *v = *Lst; v; v = *++Lst)
	{
		if (!IsTool(v))
		{
			GRect OldPos = v->GetPos();
			Update.Union(&OldPos);
			
			if (v->Pour(Client))
			{
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

GMessage::Result GWindow::OnEvent(GMessage *m)
{
	switch (m->Msg())
	{
		case M_CLOSE:
		{
			if (d->CloseRequestDone || OnRequestClose(false))
			{
				d->CloseRequestDone = true;
				Quit();
				return 0;
			}
			break;
		}
		case M_DESTROY:
		{
			delete this;
			return true;
		}
	}
	
	return GView::OnEvent(m);
}

bool GWindow::RegisterHook(GView *Target, GWindowHookType EventType, int Priority)
{
	bool Status = false;
	
	if (Target && EventType)
	{
		ssize_t i = d->GetHookIndex(Target, true);
		if (i >= 0)
		{
			d->Hooks[i].Flags = EventType;
			Status = true;
		}
	}
	
	return Status;
}

bool GWindow::UnregisterHook(GView *Target)
{
	ssize_t i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}

GViewI *GWindow::WindowFromPoint(int x, int y, bool Debug)
{
	for (int i=0; i<GPopup::CurrentPopups.Length(); i++)
	{
		GPopup *p = GPopup::CurrentPopups[i];
		if (p->Visible())
		{
			GRect r = p->GetPos();
			if (r.Overlap(x, y))
			{
				// printf("WindowFromPoint got %s click (%i,%i)\n", p->GetClass(), x, y);
				return p->WindowFromPoint(x - r.x1, y - r.y1, Debug);
			}
		}
	}
	
	return GView::WindowFromPoint(x, y, Debug);
}

int GWindow::OnCommand(int Cmd, int Event, OsView SrcCtrl)
{
	#if 0
	OsView v;
	switch (Cmd)
	{
		case kHICommandCut:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_CUT);
			break;
		}
		case kHICommandCopy:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_COPY);
			break;
		}
		case kHICommandPaste:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_PASTE);
			break;
		}
		case 'dele':
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_DELETE);
			break;
		}
	}
	#endif
	
	return 0;
}

void GWindow::OnTrayClick(GMouse &m)
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

bool GWindow::Obscured()
{
	// LAutoPool Pool;
	if (!Wnd)
		return false;

	auto s = [Wnd.p occlusionState];
	return !(s & NSWindowOcclusionStateVisible);
}
