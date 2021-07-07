
#ifndef __GWND_H
#define __GWND_H

class LgiClass GWnd : public BWindow
{
	LView *Notify;

public:
	GWnd(LView *notify);
	GWnd(LView *notify, BRect frame, char *title, window_type type, uint32 flags, uint32 workspaces = B_CURRENT_WORKSPACE);
	bool QuitRequested();
	void MessageReceived(BMessage *Msg);
	void FrameMoved(BPoint origin);
	void FrameResized(float width, float height);
};

#endif
