//
//  LCocoaView.h
//  LgiCocoa
//
//  Created by Matthew Allen on 20/10/18.
//  Copyright © 2018 Memecode. All rights reserved.
//

#ifndef LCocoaView_h
#define LCocoaView_h

#if defined __OBJC__

class GViewI;
class GWindow;
class GWindowPrivate;

#define objc_dynamic_cast(TYPE, object) \
  ({ \
      TYPE *dyn_cast_object = (TYPE*)(object); \
      [dyn_cast_object isKindOfClass:[TYPE class]] ? dyn_cast_object : nil; \
  })


@interface LCocoaMsg : NSObject
{
}
@property GViewI *v;
@property int m;
@property GMessage::Param a;
@property GMessage::Param b;
- (id)init:(GViewI*)view msg:(int)Msg a:(GMessage::Param)A b:(GMessage::Param)B;
@end

@interface LCocoaAssert : NSObject
{
}
@property GString msg;
@property NSModalResponse result;
- (id)init:(GString)m;
@end

// This class wraps a Cocoa NSView and redirects all the calls to LGI's GWindow object.
//
@interface LCocoaView : NSView
{
}
@property GView *w;
@property GString WndClass;

// Object life time
- (id)init:(GView*)wnd;
- (void)dealloc;

// Painting
- (void)drawRect:(NSRect)dirtyRect;

// Mouse
- (void)mouseDown:(NSEvent*)ev;
- (void)mouseUp:(NSEvent*)ev;
- (void)rightMouseDown:(NSEvent*)ev;
- (void)rightMouseUp:(NSEvent*)ev;
- (void)mouseMoved:(NSEvent*)ev;
- (void)mouseDragged:(NSEvent*)ev;
- (void)scrollWheel:(NSEvent*)ev;

// Keybaord
- (void)keyDown:(NSEvent*)event;
- (void)keyUp:(NSEvent*)event;
- (BOOL)acceptsFirstResponder;

// Message handling
- (void)userEvent:(LCocoaMsg*)ev;

@end

@interface LNsWindow : NSWindow
{
}

@property GWindowPrivate *d;

- (id)init:(GWindowPrivate*)priv Frame:(NSRect)rc;
- (void)dealloc;
- (BOOL)canBecomeKeyWindow;
- (GWindow*)getWindow;

@end


#endif
#endif /* LCocoaView_h */
