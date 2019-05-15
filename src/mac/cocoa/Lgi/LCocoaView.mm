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

#define Check() if (!self.v) return
static int LCocoaView_Count = 0;
@implementation LCocoaView

- (id)init:(GView*)view
{
	LAutoPool Pool;
	LCocoaView_Count++;

	if ((self = [super initWithFrame:view->GetPos()]) != nil)
	{
		self.Cls = view->GetClass();
		printf("LCocoaView.alloc... (%i, %s)\n", LCocoaView_Count, self.Cls.Get());
	
		self.v = view;
	
		[self setAutoresizingMask:NSViewNotSizable];
		[self setTranslatesAutoresizingMaskIntoConstraints:YES];
	}
	return self;
}

- (void)dealloc
{
	LAutoPool Pool;

	printf("LCocoaView.dealloc... (%i, %s)\n", LCocoaView_Count-1, self.Cls.Get());

	self.v->OnCocoaDealloc();
	self.v = nil;
	[super dealloc];
	LCocoaView_Count--;
}

- (void)drawRect:(NSRect)dirtyRect
{
	LAutoPool Pool;
	Check();
	GScreenDC Dc(self.v);
	self.v->OnPaint(&Dc);
}

- (void)layout
{
	LAutoPool Pool;
	Check();
	self.v->OnCocoaLayout();
}

- (void)mouseDown:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)mouseUp:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)rightMouseDown:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)rightMouseUp:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)mouseMoved:(NSEvent*)ev
{
	LAutoPool Pool;
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseMove(m);
}

@end
