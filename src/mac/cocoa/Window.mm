#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Popup.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"
#include "LCocoaView.h"

extern void NextTabStop(LViewI *v, int dir);
extern void SetDefaultFocus(LViewI *v);
extern void BuildTabStops(LArray<LViewI*> &Stops, LViewI *v);

#define DEBUG_KEYS			0
#define DEBUG_SETFOCUS		0
#define DEBUG_LOGGING		0

#if DEBUG_LOGGING
#define LOG(...)			printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

/*

Deleting a LWindow senarios:
 
	Users clicks close:
        NSWindowDelegate::windowWillClose
            GWindowPrivate::OnClose(CloseUser)
                LNsWindow::onDelete

    Something deletes the LWindow programmatically:
        LWindow::~LWindow
			GWindowPriv::OnClose(CloseDestructor)
				LNsWindow::onDelete
					self.close
						windowWillClose -> block

	Something calls LWindow::Quit()
		LNsWindow::onQuit (async)
			self.close
				NSWindowDelegate::windowWillClose
					GWindowPrivate::OnClose(CloseUser)
						LNsWindow::onDelete

*/

#if DEBUG_SETFOCUS || DEBUG_KEYS
static LString DescribeView(LViewI *v)
{
	if (!v)
		return GString();

	char s[512];
	int ch = 0;
	LArray<LViewI*> p;
	for (LViewI *i = v; i; i = i->GetParent())
	{
		p.Add(i);
	}
	for (int n=MIN(3, (int)p.Length()-1); n>=0; n--)
	{
		char Buf[256] = "";
		if (!stricmp(v->GetClass(), "LMdiChild"))
			sprintf(Buf, "'%s'", v->Name());
		v = p[n];
		
		ch += sprintf_s(s + ch, sizeof(s) - ch, "%s>%s", Buf, v->GetClass());
	}
	return s;
}
#endif

LRect LScreenFlip(LRect r)
{
	LRect screen(0, 0, -1, -1);
	for (NSScreen *s in [NSScreen screens])
	{
		LRect pos = s.frame;
		if (r.Overlap(&pos))
		{
			screen = pos;
			break;
		}
	}
	
	if (screen.Valid())
	{
		LRect rc = r;
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
	LView *Target;
};

@interface LWindowDelegate : NSObject<NSWindowDelegate>
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

class LWindowPrivate
{
public:
	LWindow *Wnd = NULL;
	LDialog *ChildDlg = NULL;
	LMenu *EmptyMenu = NULL;
	LViewI *Focus = NULL;
	NSView *ContentCache = NULL;

	int Sx = -1, Sy = -1;

	LKey LastKey;
	LArray<HookInfo> Hooks;

	uint64 LastMinimize = 0;
	uint64 LastDragDrop = 0;

	bool DeleteOnClose = true;
	bool SnapToEdge = false;
	bool InitVisible = false;
	
	LWindowPrivate(LWindow *wnd)
	{
		Wnd = wnd;
	}
	
	~LWindowPrivate()
	{
		DeleteObj(EmptyMenu);
	}
	
	void OnClose(LCloseContext Ctx)
	{
		LOG("LWindowPrivate::OnClose %p/%s\n", Wnd, Wnd?Wnd->GetClass():NULL);
		auto &osw = Wnd->Wnd;

		if (!osw)
			return;

		LCocoaView *cv = objc_dynamic_cast(LCocoaView, osw.p.contentView);
		if (cv)
			cv.w = NULL;

		LNsWindow *w = objc_dynamic_cast(LNsWindow, osw.p);
		if (w)
			[w onDelete:Ctx];

		osw.p.delegate = nil;
		[osw.p autorelease];
		osw = nil;
		
		if (DeleteOnClose)
			delete Wnd;
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

#define DefaultStyleMask	NSWindowStyleMaskTitled | \
							NSWindowStyleMaskResizable | \
							NSWindowStyleMaskClosable | \
							NSWindowStyleMaskMiniaturizable

@implementation LNsWindow

- (id)init:(LWindowPrivate*)priv Frame:(NSRect)rc
{
	NSUInteger windowStyleMask = DefaultStyleMask;
	if ((self = [super initWithContentRect:rc
					styleMask:windowStyleMask
					backing:NSBackingStoreBuffered
					defer:NO ]) != nil)
	{
		self.d = priv;
		self->ReqClose = CSNone;
		
		self.contentView = [[LCocoaView alloc] init:priv->Wnd];
		[self makeFirstResponder:self.contentView];
		self.acceptsMouseMovedEvents = true;
		self.ignoresMouseEvents = false;
		self.canFocus = true;
		
		// printf("LNsWindow.init\n");
	}
	return self;
}

- (void)dealloc
{
	if (self.d)
		self.d->Wnd->OnDealloc();

	LCocoaView *cv = objc_dynamic_cast(LCocoaView, self.contentView);
	cv.w = NULL;
	[cv release];
	self.contentView = NULL;
	self.canFocus = true;
	
	[super dealloc];
	// printf("LNsWindow.dealloc.\n");
}

- (LWindow*)getWindow
{
	return self.d ? self.d->Wnd : nil;
}

- (BOOL)canBecomeKeyWindow
{
	return self.canFocus;
}

- (void)onQuit
{
	#if DEBUG_LOGGING
	LWindow *wnd = self.d ? self.d->Wnd : NULL;
	auto cls = wnd ? wnd->GetClass() : NULL;
	#endif

	LOG("LNsWindow::onQuit %p/%s %i\n", wnd, cls, self->ReqClose);
	if (self->ReqClose == CSNone)
	{
		self->ReqClose = CSInRequest;
		
		if (!self.d)
			LOG("%s:%i - No priv pointer?\n", _FL);
		
		if (!self.d ||
			!self.d->Wnd ||
			!self.d->Wnd->OnRequestClose(false))
		{
			LOG("	::onQuit %p/%s no 'd' or OnReqClose failed\n", wnd, cls);
			self->ReqClose = CSNone;
			return;
		}
	}
	else return;
	
	LOG("	::onQuit %p/%s self.close\n", wnd, cls);
	self->ReqClose = CSClosed;
	self.d->Wnd->SetPulse();
	[self close];
}

- (void)onDelete:(LCloseContext)ctx
{
	LOG("LNsWindow::onDelete %p/%s\n", self.d->Wnd, self.d->Wnd->GetClass());
	
	if (ctx == CloseDestructor && self->ReqClose != CSClosed)
	{
		// This is called during the ~LWindow destructor to make sure we
		// closed the window
		self->ReqClose = CSClosed;
		LOG("	::onDelete %p self.close\n", self.d->Wnd);
		[self close];
	}
	
	self.d = NULL;
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
	if (w && w.d)
		w.d->OnClose(CloseUser);
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

LWindow::LWindow(OsWindow wnd) : LView(NULL)
{
	d = new LWindowPrivate(this);
	_QuitOnClose = false;
	Wnd = NULL;
	Menu = 0;
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	LView::Visible(false);
	
	_Lock = new LMutex("LWindow");
	
	LRect pos(200, 200, 200, 200);
	NSRect frame = pos;
	if (wnd)
		Wnd = wnd;
	else
		Wnd.p = [[LNsWindow alloc] init:d Frame:frame];
	if (Wnd)
	{
		[Wnd.p retain];
		if (!Delegate)
			Delegate = [[LWindowDelegate alloc] init];
		//[Wnd.p makeKeyAndOrderFront:NSApp];
		Wnd.p.delegate = Delegate;
		d->ContentCache = Wnd.p.contentView;
	}
}

LWindow::~LWindow()
{
	LOG("LWindow::~LWindow %p\n", this);
	if (LAppInst->AppWnd == this)
		LAppInst->AppWnd = 0;
	
	_Delete();
	d->DeleteOnClose = false; // We're already in the destructor, don't redelete.
	d->OnClose(CloseDestructor);
	
	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

NSView *LWindow::Handle()
{
    if (!InThread())
        return d->ContentCache;
    
	if (Wnd.p != nil)
		return Wnd.p.contentView;
	
	return NULL;
}

bool LWindow::SetTitleBar(bool ShowTitleBar)
{
	if (!Wnd.p)
		return false;
	
	if (ShowTitleBar)
	{
		Wnd.p.titlebarAppearsTransparent = false;
		Wnd.p.titleVisibility            = NSWindowTitleVisible;
		Wnd.p.styleMask					 = DefaultStyleMask;
	}
	else
	{
		Wnd.p.titlebarAppearsTransparent = true;
		Wnd.p.titleVisibility            = NSWindowTitleHidden;
		Wnd.p.styleMask					 = 0;
	}
		
	return true;
}

bool LWindow::SetIcon(const char *FileName)
{
	#warning "Impl LWindow::SetIcon"
	return false;
}

bool LWindow::SetWillFocus(bool f)
{
	if (!Wnd.p)
		return false;
	
	LNsWindow *w = objc_dynamic_cast(LNsWindow, Wnd.p);
	if (!w)
		return false;
		
	w.canFocus = f;
	return true;
}

LViewI *LWindow::GetFocus()
{
	return d->Focus;
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
				auto _foc = DescribeView(d->Focus);
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
				auto _set = DescribeView(d->Focus);
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
						auto _ctrl = DescribeView(ctrl);
						auto _foc = DescribeView(d->Focus);
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
					// Non GView handler
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

void LWindow::SetDragHandlers(bool On)
{
	#if 0
	if (Wnd && _View)
		SetAutomaticControlDragTrackingEnabledForWindow(Wnd, On);
	#endif
}

void LWindow::Quit(bool DontDelete)
{
	// LAutoPool Pool;
	if (_QuitOnClose)
	{
		_QuitOnClose = false;
		LCloseApp();
	}
	
	if (Wnd)
		SetDragHandlers(false);

	if (d && DontDelete)
	{
		// If DontDelete is true, we should be already in the destructor of the LWindow.
		// Which means we DON'T call onQuit, as it's too late to ask the user if they don't
		// want to close the window. The window IS closed come what may, and the object is
		// going away. Futhermore we can't access the window's memory after it's deleted and
		// that may happen if the onQuit is processed after ~LWindow.
		d->DeleteOnClose = false;
		
		if (Wnd)
			[Wnd.p close];
	}
	else if (Wnd)
	{
		[Wnd.p performSelectorOnMainThread:@selector(onQuit) withObject:nil waitUntilDone:false];
	}
}

void LWindow::SetChildDialog(LDialog *Dlg)
{
	d->ChildDlg = Dlg;
}

bool LWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void LWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

void LWindow::OnFrontSwitch(bool b)
{
	if (b && Menu)
	{
		[NSApplication sharedApplication].mainMenu = Menu->Handle().p;
	}
	else
	{
		auto m = LAppInst->Default.Get();
		[NSApplication sharedApplication].mainMenu = m ? m->Handle() : nil;
	}
	// printf("%s:%i - menu for %s is %p\n", _FL, Name(), [NSApplication sharedApplication].mainMenu);
}

bool LWindow::Visible()
{
	// LAutoPool Pool;
	if (!Wnd)
		return false;
	
	return [Wnd.p isVisible];
}

void LWindow::Visible(bool i)
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

bool LWindow::IsActive()
{
	return Wnd ? [Wnd.p isKeyWindow] : false;
}

bool LWindow::SetActive()
{
	[[NSApplication sharedApplication] activateIgnoringOtherApps : YES];
	return false;
}

void LWindow::SetDeleteOnClose(bool i)
{
	d->DeleteOnClose = i;
}

void LWindow::SetAlwaysOnTop(bool b)
{
}

bool LWindow::PostEvent(int Event, LMessage::Param a, LMessage::Param b, int64_t TimeoutMs)
{
	return LAppInst->PostEvent(this, Event, a, b);
}

bool LWindow::Attach(LViewI *p)
{
	bool Status = false;
	
	if (Wnd)
	{
		if (LBase::Name())
			Name(LBase::Name());
		
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
	if (m.Down())
	{
		bool ParentPopup = false;
		LViewI *p = m.Target;
		while (p && p->GetParent())
		{
			if (dynamic_cast<LPopup*>(p))
			{
				ParentPopup = true;
				break;
			}
			p = p->GetParent();
		}

		if (!ParentPopup)
		{
			for (int i=0; i<LPopup::CurrentPopups.Length(); i++)
			{
				auto pu = LPopup::CurrentPopups[i];
				if (pu->Visible())
				{
					// printf("Hiding popup %s\n", pu->GetClass());
					pu->Visible(false);
				}
			}
		}
		
		if (!m.IsMove() && LAppInst)
		{
			auto mh = LAppInst->GetMouseHook();
			if (mh)
				mh->TrackClick(v);
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
	LViewI *Ctrl = NULL;
	if (!v && d->Focus)
		v = d->Focus->GetGView();
	if (!v)
	{
		#if DEBUG_KEYS
		k.Trace("No focus view to handle key.");
		#endif
		return false;
	}
	
	// Give key to popups
	if (LAppInst &&
		LAppInst->GetMouseHook() &&
		LAppInst->GetMouseHook()->OnViewKey(v, k))
	{
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
		GString vv = DescribeView(v);
		printf("%s ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				vv.Get(),
			   k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
#endif
		
		Status = true;
		goto AllDone;
	}
	
	// Window didn't want the key...
	switch (k.vkey)
	{
		case LK_RETURN:
		case LK_KEYPADENTER:
		{
			Ctrl = _Default;
			break;
		}
		case LK_ESCAPE:
		{
			Ctrl = FindControl(IDCANCEL);
			break;
		}
		case LK_TAB:
		{
			// Go to the next control?
			if (k.Down())
			{
				LArray<LViewI*> Stops;
				BuildTabStops(Stops, v->GetWindow());
				ssize_t Idx = Stops.IndexOf(v);
				if (Idx >= 0)
				{
					if (k.Shift())
					{
						Idx--;
						if (Idx < 0) Idx = Stops.Length() - 1;
					}
					else
					{
						Idx++;
						if (Idx >= Stops.Length()) Idx = 0;
					}
					
					Stops[Idx]->Focus(true);
				}
			}
			return true;
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
		Quit();
		return true;
	}
	
AllDone:
	if (d)
		d->LastKey = k;
	else
		LAssert(!"Window was deleted and we are accessing unallocated mem.");
	
	return Status;
}


void LWindow::Raise()
{
	if (Wnd)
	{
		// BringToFront(Wnd);
	}
}

LWindowZoom LWindow::GetZoom()
{
	if (Wnd)
	{
		#if 0
		bool c = IsWindowCollapsed(Wnd);
		// printf("IsWindowCollapsed=%i\n", c);
		if (c)
			return LZoomMin;
		
		c = IsWindowInStandardState(Wnd, NULL, NULL);
		// printf("IsWindowInStandardState=%i\n", c);
		if (!c)
			return LZoomMax;
		#endif
	}
	
	return LZoomNormal;
}

void LWindow::SetZoom(LWindowZoom i)
{
	#if 0
	OSStatus e = 0;
	switch (i)
	{
		case LZoomMin:
		{
			e = CollapseWindow(Wnd, true);
			if (e) printf("%s:%i - CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("LZoomMin ok.\n");
			break;
		}
		default:
		case LZoomNormal:
		{
			e = CollapseWindow(Wnd, false);
			if (e) printf("%s:%i - [Un]CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("LZoomNormal ok.\n");
			break;
		}
	}
	#endif
}

LViewI *LWindow::GetDefault()
{
	return _Default;
}

void LWindow::SetDefault(LViewI *v)
{
	if (v &&
		v->GetWindow() == (LViewI*)this)
	{
		if (_Default != v)
		{
			auto Old = _Default;
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
	// LAutoPool Pool;
	bool Status = LBase::Name(n);
	
	if (Wnd)
	{
		NSString *ns = [NSString stringWithCString:n encoding:NSUTF8StringEncoding];
		Wnd.p.title = ns;
		//[ns release];
	}
	
	return Status;
}

const char *LWindow::Name()
{
	return LBase::Name();
}

LRect &LWindow::GetClient(bool ClientSpace)
{
	// LAutoPool Pool;
	static LRect r;
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

bool LWindow::SerializeState(LDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;
	
	if (Load)
	{
		LVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			LRect Position(0, 0, -1, -1);
			LWindowZoom State = LZoomNormal;
			
			auto t = LString(v.Str()).SplitDelimit(";");
			for (auto s: t)
			{
				auto v = s.SplitDelimit("=", 1);
				if (v.Length() == 2)
				{
					if (v[0].Equals("State"))
						State = (LWindowZoom)v[1].Int();
					else if (v[0].Equals("Pos"))
						Position.SetStr(v[1]);
				}
				else return false;
			}
			
			if (Position.Valid())
			{
				if (Position.X() < 64)
					Position.x2 = Position.x1 + 63;
				if (Position.Y() < 64)
					Position.y2 = Position.y1 + 63;
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
		
		LVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}
	
	return true;
}

LPoint LWindow::GetDpi()
{
	return LScreenDpi();
}

LPointF LWindow::GetDpiScale()
{
	auto Dpi = GetDpi();
	return LPointF(Dpi.x / 100.0, Dpi.y / 100.0);
}

LRect &LWindow::GetPos()
{
	// LAutoPool Pool;

	if (Wnd)
	{
		Pos = LScreenFlip(Wnd.p.frame);
		
		// printf("%s::GetPos %s\n", GetClass(), Pos.GetStr());
	}
	
	return Pos;
}

bool LWindow::SetPos(LRect &p, bool Repaint)
{
	// LAutoPool Pool;
	
	Pos = p;
	if (Wnd)
	{
		LRect r = LScreenFlip(p);
		[Wnd.p setFrame:r display:YES];
		
		// printf("%s::SetPos %s\n", GetClass(), Pos.GetStr());
	}

	return true;
}

void LWindow::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	if (dynamic_cast<LPopup*>(Wnd))
	{
		printf("%s:%i - Ignoring GPopup in OnChildrenChanged handler.\n", _FL);
		return;
	}
	PourAll();
}

void LWindow::OnCreate()
{
}

void LWindow::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
}

void LWindow::OnPosChange()
{
	LView::OnPosChange();
	
	if (d->Sx != X() ||	d->Sy != Y())
	{
		PourAll();
		d->Sx = X();
		d->Sy = Y();
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
	LRect r = GetClient();
	// printf("::Pour r=%s\n", r.GetStr());
	LRegion Client(r);
	
	LRegion Update(Client);
	bool HasTools = false;
	LViewI *v;
	List<LViewI>::I Lst = Children.begin();
	
	{
		LRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			if (IsTool(v))
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
		if (!IsTool(v))
		{
			LRect OldPos = v->GetPos();
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

LMessage::Result LWindow::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case M_CLOSE:
		{
			if (Wnd)
				[Wnd.p performSelectorOnMainThread:@selector(onQuit) withObject:nil waitUntilDone:false];
			else
				LAssert(!"No window?");
			break;
		}
		case M_DESTROY:
		{
			delete this;
			return true;
		}
	}
	
	return LView::OnEvent(m);
}

bool LWindow::RegisterHook(LView *Target, LWindowHookType EventType, int Priority)
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

bool LWindow::UnregisterHook(LView *Target)
{
	ssize_t i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}

LViewI *LWindow::WindowFromPoint(int x, int y, int DebugDepth)
{
	for (int i=0; i<LPopup::CurrentPopups.Length(); i++)
	{
		auto p = LPopup::CurrentPopups[i];
		if (p->Visible())
		{
			auto r = p->GetPos();
			if (r.Overlap(x, y))
			{
				// printf("WindowFromPoint got %s click (%i,%i)\n", p->GetClass(), x, y);
				return p->WindowFromPoint(x - r.x1, y - r.y1, DebugDepth ? DebugDepth + 1 : 0);
			}
		}
	}
	
	return LView::WindowFromPoint(x, y, DebugDepth ? DebugDepth + 1 : 0);
}

int LWindow::OnCommand(int Cmd, int Event, OsView SrcCtrl)
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

bool LWindow::Obscured()
{
	// LAutoPool Pool;
	if (!Wnd)
		return false;

	auto s = [Wnd.p occlusionState];
	return !(s & NSWindowOcclusionStateVisible);
}
