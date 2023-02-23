// Private LView definations
#pragma once

#if WINNATIVE

	#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x501
	#endif

	#include "commctrl.h"
	#include "Uxtheme.h"
	#define GViewFlags d->WndStyle

#else

	#define GViewFlags WndFlags

#endif

#if defined(__GTK_H__)

	struct LCaptureThread : public LThread, public LCancel
	{
		int view = -1;
		LString name;
		
	public:
		constexpr static int EventMs = 150;
	
		LCaptureThread(LView *v);
		~LCaptureThread();
		
		int Main();
	};

#elif defined(MAC)

	extern OsThread LgiThreadInPaint;

#elif defined(HAIKU)

	#include <View.h>
	extern void *IsAttached(BView *v);
	
#endif

#define PAINT_VIRTUAL_CHILDREN	0

extern bool In_SetWindowPos;
extern LMouse &lgi_adjust_click(LMouse &Info,
								LViewI *Wnd,
								bool Capturing = false,
								bool Debug = false);
#ifdef __GTK_H__
extern LPoint GtkAbsPos(Gtk::GtkWidget *w);
extern LRect GtkGetPos(Gtk::GtkWidget *w);
#endif

#if !WINNATIVE
#include "lgi/common/ThreadEvent.h"

class LPulseThread : public LThread, public LCancel
{
	LView *View = NULL;
	LString ViewClass;
	int Length = 0;
	LThreadEvent Event;
	uint64_t WarnTs = 0;

	LString MakeName(LView *v, const char *Type)
	{
		LString s;
		s.Printf("LPulseThread.%s.%s", v->GetClass(), Type);
		return s;
	}

public:
	LPulseThread(LView *view, int len) :
		View(view),
		LThread(MakeName(view, "Thread")),
		Event(MakeName(view, "Event"))
	{
		LAssert(View);
		Length = len;
		ViewClass = View->GetClass();
		
		Run();
	}
	
	~LPulseThread()
	{
		View = NULL;
		Cancel();
		Event.Signal();
		WaitForExit();
	}
	
	int Main()
	{
		while (!IsCancelled() && LAppInst)
		{
			auto s = Event.Wait(Length);
			if (!View || IsCancelled() || s == LThreadEvent::WaitError)
				break;
			
			if (!View->PostEvent(M_PULSE, 0, 0, 50/*milliseconds*/))
			{
				auto now = LCurrentTime();
				if (now - WarnTs >= 5000)
				{
					WarnTs = now;
					printf("%s:%i - PulseThread::PostEvent failed for %p/%s.\n", _FL, View, ViewClass.Get());
				}
			}
		}
		
		return 0;
	}
};

#endif

enum LViewFontType
{
	/// The LView has a pointer to an externally owned font.
	GV_FontPtr,
	/// The LView owns the font object, and must free it.
	GV_FontOwned,
	/// The LApp's font cache owns the object. In this case,
	/// calling GetCssStyle on the LView will invalidate the
	/// font ptr causing it to be re-calculated.
	GV_FontCached,
};

class LViewPrivate
{
public:
	// General
	LView			*View = NULL; // Owning view
	LDragDropSource	*DropSource = NULL;
	LDragDropTarget	*DropTarget = NULL;
	bool			IsThemed = false;
	int				CtrlId = -1;
	int				WantsPulse = -1;

	// Hierarchy
	LViewI			*ParentI = NULL;
	LView			*Parent = NULL;
	LViewI			*Notify = NULL;

	// Size
	LPoint			MinimumSize;
	
	// Font
	LFont			*Font = NULL;
	LViewFontType	FontOwnType = GV_FontPtr;
	
	// Style
	LAutoPtr<LCss>  Css;
	bool CssDirty = false;	// This is set when 'Styles' changes, the next call to GetCss(...) parses
							// the styles into the 'Css' object.
	LString			Styles; // Somewhat temporary object to store unparsed styles particular to
							// this view until runtime, where the view heirarchy is known.
	LString::Array	Classes;

	// Event dispatch handle
	int				SinkHnd = -1;

	// OS Specific
	#if WINNATIVE

		static OsView	hPrevCapture;
		int				WndStyle = 0;			// Windows hWnd Style
		int				WndExStyle = 0;			// Windows hWnd ExStyle
		int				WndDlgCode = 0;			// Windows Dialog Code (WM_GETDLGCODE)
		LString			WndClass;
		UINT_PTR		TimerId = 0;
		HTHEME			hTheme = NULL;

	#else

		// Cursor
		LPulseThread	*PulseThread = NULL;
		LView			*Popup = NULL;
		bool			TabStop = false;
		bool			WantsFocus = false;

		#if defined __GTK_H__
		bool			InPaint = false;
		bool			GotOnCreate = false;
		#elif defined(MAC) && !defined(LGI_COCOA)
		static HIObjectClassRef BaseClass;
		#endif

	#endif

	#if defined(__GTK_H__)

		static LCaptureThread *CaptureThread;
		LMouse PrevMouse;
	
	#elif defined(MAC)

		#ifdef LGI_COCOA
			LString ClassName;
			bool AttachEvent;
		#elif defined LGI_CARBON
			EventHandlerRef DndHandler;
			LAutoString AcceptedDropFormat;
		#endif

	#elif defined(LGI_SDL)
		
		SDL_TimerID PulseId;
		int PulseLength = -1;
	
	#elif defined(HAIKU)
	
		BView *Hnd = NULL;
		LArray<BMessage*> MsgQue; // For before the window is attached...
	
	#endif
	
	LViewPrivate(LView *view);
	~LViewPrivate();
	
	LView *GetParent()
	{
		if (Parent)
			return Parent;
		
		if (ParentI)
			return ParentI->GetGView();

		return 0;
	}
};


