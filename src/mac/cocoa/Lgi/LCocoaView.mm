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


@implementation LCocoaView {
	NSTrackingArea *tracking;
}

- (id)init:(GWindow*)wnd
{
	LAutoPool Pool;
	LCocoaView_Count++;

	if ((self = [super initWithFrame:wnd->GetPos()]) != nil)
	{
		printf("LCocoaView.alloc... (%i, %s)\n", LCocoaView_Count, wnd->GetClass());
	
		self.w = wnd;
		self.WndClass = wnd->GetClass();
		
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

	auto scrollingDeltaY = ev.scrollingDeltaY;
	int px = SysFont->GetHeight();
	float lines = ((-scrollingDeltaY) + px) / px;
	// printf("Scroll %g\n", scrollingDeltaY);
	t->OnMouseWheel(lines);
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
	self.w->HandleViewKey(NULL, k);
}

- (void)keyUp:(NSEvent*)event
{
	LAutoPool Pool;
	Check();

	GKey k = KeyEvent(event);
	self.w->HandleViewKey(NULL, k);
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

- (BOOL) acceptsFirstResponder
{
    return YES;
}

@end
