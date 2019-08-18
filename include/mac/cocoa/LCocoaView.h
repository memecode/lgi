//
//  LCocoaView.h
//  LgiCocoa
//
//  Created by Matthew Allen on 20/10/18.
//  Copyright © 2018 Memecode. All rights reserved.
//

#ifndef LCocoaView_h
#define LCocoaView_h

class GViewI;

@interface LCocoaMsg : NSObject
{
}
@property int hnd;
@property int m;
@property GMessage::Param a;
@property GMessage::Param b;
- (id)init:(int)hnd msg:(int)Msg a:(GMessage::Param)A b:(GMessage::Param)B;
@end

// This class wraps a Cocoa NSView and redirects all the calls to LGI's GWindow object.
//
@interface LCocoaView : NSView
{
}
@property GWindow *w;
@property GString WndClass;

// Object life time
- (id)init:(GWindow*)wnd;
- (void)dealloc;

// Painting
- (void)drawRect:(NSRect)dirtyRect;

// Mouse
- (void)mouseDown:(NSEvent*)ev;
- (void)mouseUp:(NSEvent*)ev;
- (void)rightMouseDown:(NSEvent*)ev;
- (void)rightMouseUp:(NSEvent*)ev;
- (void)mouseMoved:(NSEvent*)ev;
- (void)scrollWheel:(NSEvent*)ev;

// Keybaord
- (void)keyDown:(NSEvent*)event;
- (void)keyUp:(NSEvent*)event;
- (BOOL)acceptsFirstResponder;

// Message handling
- (void)userEvent:(LCocoaMsg*)ev;

@end


#endif /* LCocoaView_h */
