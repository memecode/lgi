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

#if defined(MAC)
extern OsThread LgiThreadInPaint;
#elif defined(HAIKU)
#include <View.h>
#endif

#define PAINT_VIRTUAL_CHILDREN	1

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

class LPulseThread : public LThread
{
	LView *View;
	int Length;
	LThreadEvent Event;

public:
	bool Loop;

	LPulseThread(LView *view, int len) : LThread("LPulseThread"), Event("LPulseThread")
	{
		LAssert(view);
		
		Loop = true;
		View = view;
		Length = len;
		
		Run();
	}
	
	~LPulseThread()
	{
		Loop = false;
		View = NULL;
		Event.Signal();

		while (!IsExited())
			LSleep(0);
	}
	
	int Main()
	{
		while (Loop && LAppInst)
		{
			auto s = Event.Wait(Length);
			if (!Loop || s == LThreadEvent::WaitError)
				break;
			
			if (View)
			{
				if (!View->PostEvent(M_PULSE))
					Loop = false;
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
	#ifdef HAIKU
	: public BView
	#endif
{
public:
	// General
	LView			*View = NULL;
	int				CtrlId = 0;
	LDragDropSource	*DropSource = NULL;
	LDragDropTarget	*DropTarget = NULL;
	bool			IsThemed = false;

	// Heirarchy
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
		int				WndStyle;				// Win32 hWnd Style
		int				WndExStyle;				// Win32 hWnd ExStyle
		int				WndDlgCode;				// Win32 Dialog Code (WM_GETDLGCODE)
		LString			WndClass;
		UINT_PTR		TimerId;
		HTHEME			hTheme;
		int				WantsPulse;

	#else

		// Cursor
		LPulseThread	*PulseThread = NULL;
		LView			*Popup = NULL;
		bool			TabStop = false;
		bool			WantsFocus = false;
		int				WantsPulse = 0;

		#if defined __GTK_H__
		bool			InPaint = false;
		bool			GotOnCreate = false;
		#elif defined(MAC) && !defined(LGI_COCOA)
		static HIObjectClassRef BaseClass;
		#endif

	#endif

	#if defined(MAC)
		#ifdef LGI_COCOA
		LString ClassName;
		bool AttachEvent;
		#elif defined LGI_CARBON
		EventHandlerRef DndHandler;
		LAutoString AcceptedDropFormat;
		#endif
	#endif
	
	#if defined(LGI_SDL)
	SDL_TimerID PulseId;
	int PulseLength;
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

