#ifndef _GVIEW_PRIV_
#define _GVIEW_PRIV_

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

#ifdef MAC
extern OsThread LgiThreadInPaint;
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

class GPulseThread : public LThread
{
	LView *View;
	int Length;
	LThreadEvent Event;

public:
	bool Loop;

	GPulseThread(LView *view, int len) : LThread("GPulseThread"), Event("GPulseThread")
	{
		LgiAssert(view);
		
		Loop = true;
		View = view;
		Length = len;
		
		Run();
	}
	
	~GPulseThread()
	{
		Loop = false;
		View = NULL;
		Event.Signal();

		while (!IsExited())
			LgiSleep(0);
	}
	
	int Main()
	{
		// auto ts = LgiCurrentTime();
		
		while (Loop && LgiApp)
		{
			auto s = Event.Wait(Length);
			if (!Loop || s == LThreadEvent::WaitError)
				break;
			
			if (View)
			{
				// auto now = LgiCurrentTime();
				// printf("pulse %i of %i\n", (int)(now - ts), (int)Length);
				if (!View->PostEvent(M_PULSE))
					Loop = false;
					
				// ts = now;
			}
		}
		
		return 0;
	}
};
#endif

enum GViewFontType
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

class GViewPrivate
{
public:
	// General
	int				CtrlId;
	LDragDropSource	*DropSource;
	LDragDropTarget	*DropTarget;
	bool			IsThemed;

	// Heirarchy
	LViewI			*ParentI;
	LView			*Parent;
	LViewI			*Notify;

	// Size
	LPoint			MinimumSize;
	
	// Font
	LFont			*Font;
	GViewFontType	FontOwnType;
	
	// Style
	LAutoPtr<LCss>  Css;
	bool CssDirty;			// This is set when 'Styles' changes, the next call to GetCss(...) parses
							// the styles into the 'Css' object.
	LString			Styles; // Somewhat temporary object to store unparsed styles particular to
							// this view until runtime, where the view heirarchy is known.
	LString::Array	Classes;

	// Event dispatch handle
	int				SinkHnd;

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
		GPulseThread	*Pulse;
		LView			*Popup;
		bool			TabStop;
		bool			WantsFocus;
		int				WantsPulse;

		#if defined __GTK_H__
		bool			InPaint;
		bool			GotOnCreate;
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
	
	GViewPrivate();
	~GViewPrivate();
	
	LView *GetParent()
	{
		if (Parent)
			return Parent;
		
		if (ParentI)
			return ParentI->GetGView();

		return 0;
	}
};

#endif
