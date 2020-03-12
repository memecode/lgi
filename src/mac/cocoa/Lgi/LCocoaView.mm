//
//  LCocoaView.m
//  LgiTest
//
//  Created by Matthew Allen on 20/10/18.
//  Copyright © 2018 Matthew Allen. All rights reserved.
//

#import "Lgi.h"
#import <Foundation/Foundation.h>
#import "LCocoaView.h"
#include "GEventTargetThread.h"
#include "GClipBoard.h"

#define Check() if (!self.w) return
static int LCocoaView_Count = 0;

NSCursor *LCocoaCursor(LgiCursor lc)
{
	switch (lc)
	{
		/// Blank/invisible cursor
		case LCUR_Blank: return NSCursor.disappearingItemCursor;
		/// Normal arrow
		case LCUR_Normal: return NSCursor.arrowCursor;
		/// Upwards arrow
		case LCUR_UpArrow: return NSCursor.resizeUpCursor;
		/// Downwards arrow
		case LCUR_DownArrow: return NSCursor.resizeDownCursor;
		/// Left arrow
		case LCUR_LeftArrow: return NSCursor.resizeLeftCursor;
		/// Right arrow
		case LCUR_RightArrow: return NSCursor.resizeRightCursor;
		/// Crosshair
		case LCUR_Cross: return NSCursor.crosshairCursor;
		/// Ibeam/text entry
		case LCUR_Ibeam: return NSCursor.IBeamCursor;
		/// Vertical resize (|)
		case LCUR_SizeVer: return NSCursor.resizeUpDownCursor;
		/// Horizontal resize (-)
		case LCUR_SizeHor: return NSCursor.resizeLeftRightCursor;
		/// A pointing hand
		case LCUR_PointingHand: return NSCursor.pointingHandCursor;
		/// A slashed circle
		case LCUR_Forbidden: return NSCursor.operationNotAllowedCursor;
		/// Copy Drop
		case LCUR_DropCopy: return NSCursor.dragCopyCursor;
		/// Copy Move
		case LCUR_DropMove: return NSCursor.dragLinkCursor;

		default:
		/// Hourglass/watch
		case LCUR_Wait:
		/// Diagonal resize (/)
		case LCUR_SizeBDiag:
		/// Diagonal resize (\)
		case LCUR_SizeFDiag:
		/// All directions resize
		case LCUR_SizeAll:
		/// Vertical splitting
		case LCUR_SplitV:
		/// Horziontal splitting
		case LCUR_SplitH:
			return NULL;
	}
}


@implementation LCocoaMsg

- (id)init:(GViewI*)View msg:(int)Msg a:(GMessage::Param)A b:(GMessage::Param)B
{
	if ((self = [super init]) != nil)
	{
		self.v = View;
		self.m = Msg;
		self.a = A;
		self.b = B;
	}
	
	return self;
}

@end

@implementation LCocoaAssert

- (id)init:(GString)m
{
	if ((self = [super init]) != nil)
	{
		self.msg = m;
	}
	
	return self;
}

@end

static NSDragOperation LgiToCocoaDragOp(int op)
{
	switch (op)
	{
		case DROPEFFECT_COPY: return NSDragOperationCopy;
		case DROPEFFECT_MOVE: return NSDragOperationMove;
		case DROPEFFECT_LINK: return NSDragOperationLink;
	}
	
	return NSDragOperationNone; //DROPEFFECT_NONE
}

struct DndEvent
{
	LCocoaView *cv;
	GViewI *v;
	GDragDropTarget *target;
	GDragFormats InputFmts, AcceptedFmts;
	GdcPt2 Pt;
	int Keys;

	int result;
	
	DndEvent(LCocoaView *Cv, id <NSDraggingInfo> sender) :
		cv(Cv), InputFmts(true), AcceptedFmts(false)
	{
		v = cv.w;
		target = NULL;
		Keys = 0;
		result = 0;

		setPoint(sender.draggingLocation);
		
		NSPasteboard *pb = sender.draggingPasteboard;
		if (!pb)
			return;

		List<char> Fmts;
		if (pb.types)
		{
			for (id type in pb.types)
			{
				auto f = [type UTF8String];
				InputFmts.Supports(f);
				printf("Drop has type '%s'\n", f);
			}
		}
	}
	
	void setPoint(NSPoint loc)
	{
		auto frame = cv.frame;
		Pt.Set(loc.x, frame.size.height - loc.y);
		
		GDragDropTarget *t = NULL;
		v = cv.w->WindowFromPoint(Pt.x, Pt.y);
		if (!v)
			return;

		// Convert co-ords to view local
		for (GViewI *view = v; view != view->GetWindow(); view = view->GetParent())
		{
			GView *gv = view->GetGView();
			GRect cli = gv ? gv->GView::GetClient(false) : view->GetClient(false);
			GRect pos = view->GetPos();
			Pt.x -= pos.x1 + cli.x1;
			Pt.y -= pos.y1 + cli.y1;
		}
	
		// Find the nearest target
		for (; !t && v; v = v->GetParent())
			t = v->DropTarget();

		if (target != t)
		{
			if (target)
				target->OnDragExit();
			target = t;
			if (target)
				target->OnDragEnter();
		}

	}
};

@implementation LCocoaView {
	NSTrackingArea *tracking;
}

- (id)init:(GView*)wnd
{
	LAutoPool Pool;
	LCocoaView_Count++;

	if ((self = [super initWithFrame:wnd->GetPos()]) != nil)
	{
		printf("LCocoaView.alloc... (%i, %s)\n", LCocoaView_Count, wnd->GetClass());
	
		self.w = wnd;
		self.WndClass = wnd->GetClass();
		self->dnd = nil;

		NSRect r = {{0, 0},{8000,8000}};
		self->tracking = [[NSTrackingArea alloc] initWithRect:r
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveAlways |
                                             NSTrackingMouseMoved
                                       owner:self
                                    userInfo:nil];
	
		[self addTrackingArea:self->tracking];
		[self setAutoresizingMask:NSViewNotSizable];
		[self setTranslatesAutoresizingMaskIntoConstraints:YES];
	}
	return self;
}

- (void)dealloc
{
	LAutoPool Pool;

	printf("LCocoaView.dealloc... (%i, %s)\n", LCocoaView_Count-1, self.WndClass.Get());

	DeleteObj(self->dnd);
	if (self->tracking)
	{
		[self->tracking release];
		self->tracking = NULL;
	}

	self.w = nil;
	[super dealloc];
	LCocoaView_Count--;
}

- (void)drawRect:(NSRect)dirtyRect
{
	Check();

	LAutoPool Pool;
	GScreenDC Dc(self.w);
	self.w->_Paint(&Dc);
}

- (void)layout
{
}

- (void)mouseDown:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();
	
	GMouse m(self.w);
	m.SetFromEvent(ev, self);
	m.Target->GetGView()->_Mouse(m, false);
}

- (void)mouseUp:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();

	GMouse m(self.w);
	m.SetFromEvent(ev, self);
	m.Target->GetGView()->_Mouse(m, false);
}

- (void)rightMouseDown:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();

	GMouse m(self.w);
	m.SetFromEvent(ev, self);
	m.Target->GetGView()->_Mouse(m, false);
}

- (void)rightMouseUp:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();

	GMouse m(self.w);
	m.SetFromEvent(ev, self);
	m.Target->GetGView()->_Mouse(m, false);
}

- (void)mouseMoved:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();

	GMouse m(self.w);
	m.SetFromEvent(ev, self);
	
	// m.Trace("moved");

	m.Target->GetGView()->_Mouse(m, true);
}

- (void)mouseDragged:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();

	GMouse m(self.w);
	m.SetFromEvent(ev, self);
	m.Target->GetGView()->_Mouse(m, true);
}

- (void)scrollWheel:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();

	auto r = self.frame;
	auto pt = ev.locationInWindow;
	int x = (int)pt.x;
	int y = (int)(r.size.height - pt.y);

	auto t = self.w->WindowFromPoint(x, y);
	if (!t)
		return;

	static double total = 0.0;
	double deltaY = ev.scrollingDeltaY;
	
	int diff = 0;
	double scale = 1.0;
	bool precise = [ev hasPreciseScrollingDeltas];
    if (precise)
    	scale = 0.1;

	int old = (int)(total * scale);
	total += deltaY;
	int cur = (int)(total * scale);
	diff = cur - old;
	// printf("wheel delta=%g total=%g\n", deltaY, total);

	if (!diff && !precise)
		diff = deltaY > 0 ? 1 : -1;

	if (diff)
		t->OnMouseWheel(-diff);
}

const char *LVirtualKeyToString(LVirtualKeys c)
{
	switch (c)
	{
		#define _(k,v) case LK_ ##k: return "LK_" #k;
		MacKeyDef()
		#undef _
		default:
			break;
	}
	return "#undef";
}

GKey KeyEvent(NSEvent *ev)
{
	LAutoPool Pool;
	GKey k;
	GString s = [ev.characters UTF8String];
	auto mod = ev.modifierFlags;
	bool option = (mod & NSEventModifierFlagOption) != 0;
	bool cmd = (mod & NSEventModifierFlagCommand) != 0;
	bool func = (mod & NSEventModifierFlagFunction) != 0;

	uint8_t *p = (uint8_t*) s.Get();
	ssize_t len = s.Length();
	uint32_t u32 = LgiUtf8To32(p, len);
	k.c16 = u32;
	k.vkey = ev.keyCode;
	k.SetModifer((uint32_t)mod);

	// printf("MacKeyToStr %i/%s/%i -> %s\n", u32, s.Get(), k.vkey, LVirtualKeyToString((LVirtualKeys)k.vkey));
	switch (ev.keyCode)
	{
		case LK_KEYPADENTER:
			k.c16 = '\r';
			break;
	}
	

	k.IsChar =	!func
				&&
				!cmd
				&&
				!option
				&&
				(
					k.c16 >= ' ' ||
					k.vkey == LK_RETURN ||
					k.vkey == LK_KEYPADENTER ||
					k.vkey == LK_TAB ||
					k.vkey == LK_BACKSPACE
				)
				&&
				k.vkey != LK_DELETE;
	
	return k;
}

- (void)keyDown:(NSEvent*)event
{
	LAutoPool Pool;
	Check();

	GKey k = KeyEvent(event);
	k.Down(true);
	
	GWindow *wnd = dynamic_cast<GWindow*>(self.w);
	if (wnd)
		wnd->HandleViewKey(NULL, k);
	else
		self.w->OnKey(k);
}

- (void)keyUp:(NSEvent*)event
{
	LAutoPool Pool;
	Check();

	GKey k = KeyEvent(event);
	GWindow *wnd = dynamic_cast<GWindow*>(self.w);
	if (wnd)
		wnd->HandleViewKey(NULL, k);
	else
		self.w->OnKey(k);
}

- (void)userEvent:(LCocoaMsg*)msg
{
	LAutoPool Pool;

	if (GView::LockHandler(msg.v, GView::OpExists))
	{
		GMessage Msg(msg.m, msg.a, msg.b);
		// LgiTrace("%s::OnEvent %i,%i,%i\n", msg.v->GetClass(), msg.m, msg.a, msg.b);
		msg.v->OnEvent(&Msg);
	}
	
	[msg release];
}

void UpdateAccepted(DndEvent &e, id <NSDraggingInfo> sender)
{
	e.AcceptedFmts.Empty();
	e.AcceptedFmts.SetSource(true);
	GString LgiFmt = LBinaryDataPBoardType;
	for (size_t i=0; i<e.InputFmts.Length(); i++)
	{
		auto f = e.InputFmts[i];
		
		if (LgiFmt.Equals(f))
		{
			NSString *nsFmt = LgiFmt.NsStr();
			NSData *d = [sender.draggingPasteboard dataForType:nsFmt];
			LBinaryData *bin = [[LBinaryData alloc] init:d];
			if (bin)
			{
				GString realFmt;
				[bin getData:&realFmt data:NULL len:NULL var:NULL];
				[bin release];
				
				if (realFmt)
					e.AcceptedFmts.Supports(realFmt);
			}
			[nsFmt release];
		}
		else
		{
			e.AcceptedFmts.Supports(f);
		}
	}
	e.AcceptedFmts.SetSource(false);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	DeleteObj(self->dnd);
	self->dnd = new DndEvent(self, sender);
	if (!self->dnd)
		return NSDragOperationNone;
	DndEvent &e = *self->dnd;
	if (!e.target)
		return NSDragOperationNone;

	UpdateAccepted(e, sender);
	
	e.result = e.target->WillAccept(e.AcceptedFmts, e.Pt, e.Keys);
	auto ret = LgiToCocoaDragOp(e.result);
	// printf("draggingEntered ret=%i\n", (int)ret);
	return ret;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
	if (!self->dnd)
		return NSDragOperationNone;

	DndEvent &e = *self->dnd;
	e.setPoint(sender.draggingLocation);
	if (!e.target)
		return NSDragOperationNone;

	UpdateAccepted(e, sender);

	e.result = e.target->WillAccept(e.AcceptedFmts, e.Pt, e.Keys);
	auto ret = LgiToCocoaDragOp(self->dnd->result);
	
	// printf("draggingUpdated pt=%i,%i len=%i ret=%i\n", e.Pt.x, e.Pt.y, (int)e.AcceptedFmts.Length(), (int)ret);
	return ret;
}

- (void)draggingExited:(nullable id <NSDraggingInfo>)sender
{
	if (self->dnd)
	{
		if (self->dnd->target)
			self->dnd->target->OnDragExit();
		DeleteObj(self->dnd);
	}
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
	return true;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	if (!self->dnd)
	{
		printf("%s:%i - No DND.\n", _FL);
		return NSDragOperationNone;
	}
	DndEvent &e = *self->dnd;
	e.setPoint(sender.draggingLocation);
	if (!e.target)
		return NSDragOperationNone;

	GArray<GDragData> Data;
	auto pb = sender.draggingPasteboard;
	
	auto Accepted = e.AcceptedFmts.GetSupported();
	for (auto &f: Accepted)
	{
		auto &dd = Data.New();
		dd.Format = f;
		printf("Got drop format '%s'..\n", dd.Format.Get());

		NSString *NsFmt = dd.Format.NsStr();
		NSData *d = [pb dataForType:NsFmt];
		if (d)
		{
			// System type...
			dd.Data[0].SetBinary(d.length, (void*)d.bytes);
		}
		else
		{
			// Lgi type?
			NSArray<NSPasteboardItem *> *all = [pb pasteboardItems];
			for (id i in all)
			{
				NSData *data = [i dataForType:LBinaryDataPBoardType];
				if (data)
				{
					LBinaryData *bin = [[LBinaryData alloc] init:data];
					if (bin)
					{
						GString realFmt;
						if ([bin getData:&realFmt data:NULL len:NULL var:NULL])
						{
							if (realFmt == dd.Format)
							{
								[bin getData:NULL data:NULL len:NULL var:&dd.Data[0]];
								break;
							}
						}
						
						[bin release];
					}
				}
				if (dd.Data.Length())
					break;
			}
		}
		
		[NsFmt release];
	}
	
	auto drop = e.target->OnDrop(Data, e.Pt, e.Keys);
	return drop != 0;
}

- (void)concludeDragOperation:(nullable id <NSDraggingInfo>)sender
{
}

- (void)draggingEnded:(nullable id <NSDraggingInfo>)sender
{
}

- (BOOL) acceptsFirstResponder
{
    return YES;
}

@end

