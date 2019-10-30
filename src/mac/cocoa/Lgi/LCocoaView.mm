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

#define Check() if (!self.w) return
static int LCocoaView_Count = 0;

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
	List<char> InputFmts, AcceptedFmts;
	GdcPt2 Pt;
	int Keys;

	int result;
	
	DndEvent(LCocoaView *Cv, id <NSDraggingInfo> sender) : cv(Cv)
	{
		v = cv.w;
		target = NULL;
		Keys = 0;
		result = 0;

		setPoint(sender.draggingLocation);
		if (!target)
			return;
		
		NSPasteboard *pb = sender.draggingPasteboard;
		if (!pb)
			return;

		List<char> Fmts;
		if (pb.types)
		{
			for (id type in pb.types)
				InputFmts.Add(NewStr([type UTF8String]));
		}
	}
	
	~DndEvent()
	{
		InputFmts.DeleteArrays();
		AcceptedFmts.DeleteArrays();
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

		NSRect r = {{0, 0},{4000,2000}};
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

	static CGFloat total = 0.0;
	auto deltaY = ev.scrollingDeltaY;
	total += deltaY;
	
	int lines;
    if ([ev hasPreciseScrollingDeltas])
		lines = (int) (deltaY * 0.1);
    else
		lines = (int) total;

	if (lines)
	{
		total -= lines;
		t->OnMouseWheel(-lines);
	}
}

GKey KeyEvent(NSEvent *ev)
{
	LAutoPool Pool;
	GKey k;
	GString s = [ev.characters UTF8String];
	auto mod = ev.modifierFlags;
	#if 0
	bool capsLock = (mod & NSEventModifierFlagCapsLock) != 0;
	bool shift = (mod & NSEventModifierFlagShift) != 0;
	bool ctrl = (mod & NSEventModifierFlagControl) != 0;
	bool numPad = (mod & NSEventModifierFlagNumericPad) != 0;
	bool help = (mod & NSEventModifierFlagHelp) != 0;
	#endif
	bool option = (mod & NSEventModifierFlagOption) != 0;
	bool cmd = (mod & NSEventModifierFlagCommand) != 0;
	bool func = (mod & NSEventModifierFlagFunction) != 0;

	uint8_t *p = (uint8_t*) s.Get();
	ssize_t len = s.Length();
	uint32_t u32 = LgiUtf8To32(p, len);
	k.c16 = u32;
	k.vkey = u32;
	
	k.SetModifer((uint32_t)mod);
	
	k.IsChar =	!func
				&&
				!cmd
				&&
				!option
				&&
				(
					k.c16 >= ' ' ||
					k.vkey == LK_RETURN ||
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

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	DeleteObj(self->dnd);
	self->dnd = new DndEvent(self, sender);
	if (!self->dnd)
		return NSDragOperationNone;
	DndEvent &e = *self->dnd;
	if (!e.target)
		return NSDragOperationNone;

	e.AcceptedFmts.DeleteArrays();
	for (auto f: e.InputFmts)
		e.AcceptedFmts.Add(NewStr(f));
	
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

	e.AcceptedFmts.DeleteArrays();
	for (auto f: e.InputFmts)
		e.AcceptedFmts.Add(NewStr(f));

	e.result = e.target->WillAccept(e.AcceptedFmts, e.Pt, e.Keys);
	auto ret = LgiToCocoaDragOp(self->dnd->result);
	
	printf("draggingUpdated pt=%i,%i len=%i ret=%i\n", e.Pt.x, e.Pt.y, (int)e.AcceptedFmts.Length(), (int)ret);
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
	for (auto f: e.AcceptedFmts)
	{
		auto &dd = Data.New();
		dd.Format = f;
		NSString *NsFmt = dd.Format.NsStr();
		NSData *d = [pb dataForType:NsFmt];
		if (d)
			dd.Data[0].SetBinary(d.length, (void*)d.bytes);
		
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

