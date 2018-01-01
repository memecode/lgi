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

#define Check() if (!self.w) return
static int LCocoaView_Count = 0;
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
		self->tracking = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveAlways |
                                             NSTrackingMouseMoved
                                       owner:self
                                    userInfo:nil];
	
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
	LAutoPool Pool;
	Check();
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

@end
