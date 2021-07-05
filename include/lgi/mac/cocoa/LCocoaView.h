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

class LViewI;
class LWindow;
class GWindowPrivate;

enum LCloseContext
{
	CloseNone,
	CloseUser,
	CloseDestructor,
};

LgiExtern LRect LScreenFlip(LRect r);

#define objc_dynamic_cast(TYPE, object) \
  ({ \
      TYPE *dyn_cast_object = (TYPE*)(object); \
      [dyn_cast_object isKindOfClass:[TYPE class]] ? dyn_cast_object : nil; \
  })


@interface LCocoaMsg : NSObject
{
}
@property LViewI *v;
@property int m;
@property GMessage::Param a;
@property GMessage::Param b;
- (id)init:(LViewI*)view msg:(int)Msg a:(GMessage::Param)A b:(GMessage::Param)B;
@end

@interface LCocoaAssert : NSObject
{
}
@property LString msg;
@property NSModalResponse result;
- (id)init:(LString)m;
@end

// This class wraps a Cocoa NSView and redirects all the calls to LGI's LWindow object.
//
@interface LCocoaView : NSView
{
	struct DndEvent *dnd;
}
@property LView *w;
@property LString WndClass;

// Object life time
- (id)init:(LView*)wnd;
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

// Keyboard
- (void)keyDown:(NSEvent*)event;
- (void)keyUp:(NSEvent*)event;
- (BOOL)acceptsFirstResponder;

// Message handling
- (void)userEvent:(LCocoaMsg*)ev;

// DnD
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender;
- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender;
- (void)draggingExited:(nullable id <NSDraggingInfo>)sender;
- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender;
- (void)concludeDragOperation:(nullable id <NSDraggingInfo>)sender;
- (void)draggingEnded:(nullable id <NSDraggingInfo>)sender;


@end

@interface LNsWindow : NSWindow
{
	enum CloseState
	{
		CSNone,
		CSInRequest,
		CSClosed,
	};
	
	CloseState ReqClose;
}

@property GWindowPrivate *d;

- (id)init:(GWindowPrivate*)priv Frame:(NSRect)rc;
- (void)dealloc;
- (BOOL)canBecomeKeyWindow;
- (LWindow*)getWindow;
- (void)onQuit;
- (void)onDelete:(LCloseContext)ctx;

@end


#endif
#endif /* LCocoaView_h */
