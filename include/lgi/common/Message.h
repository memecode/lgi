#ifndef _GMESSAGE_H_
#define _GMESSAGE_H_

enum LgiMessages
{
	#if defined(__GTK_H__)

		/// Base point for system messages.
		M_SYSTEM						= 900,
		/// Message that indicates the user is trying to close a top level window.
		M_CLOSE,
		/// Implemented to handle paint requests in the GUI thread.
		M_X11_REPARENT,

		/// \brief Mouse enter event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEENTER,

		/// \brief Mouse exit event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEEXIT,

		// return (bool)
		M_WANT_DIALOG_PROC,

		M_MENU,
		M_COMMAND,
		M_DRAG_DROP,

		M_TRAY_NOTIFY,
		M_CUT,
		M_COPY,
		M_PASTE,
		M_GTHREADWORK_COMPELTE,
		
		/// Implemented to handle timer events in the GUI thread.
		M_PULSE,
		M_SET_VISIBLE,
		M_CAPTURE_PULSE,
		
		/// Sent from a worker thread when calling LTextLabel::Name
		M_TEXT_UPDATE_NAME,
		
	#elif defined(WINNATIVE)

		// [WM_APP:WM_APP+200] is reserved for LGI itself.
		// [WM_APP+200:0xBFFF] is reserved for LGI applications.
		M_CUT						= WM_CUT,
		M_COPY						= WM_COPY,
		M_PASTE						= WM_PASTE,
		M_COMMAND					= WM_COMMAND,
		M_CLOSE						= WM_CLOSE,

		// wParam = bool Inside; // is the mouse inside the client area?
		// lParam = MAKELONG(x, y); // mouse location
		M_MOUSEENTER				= WM_APP,
		M_MOUSEEXIT,

		// return (bool)
		M_WANT_DIALOG_PROC,

		// wParam = void
		// lParam = (MSG*) Msg;
		M_TRANSLATE_ACCELERATOR,

		// None
		M_TRAY_NOTIFY,

		// lParam = Style
		M_SET_WND_STYLE,

		// lParam = LScrollBar *Obj
		M_SCROLL_BAR_CHANGED,

		// lParam = HWND of window under mouse
		// This is only sent for non-LGI window in our process
		// because we'd get WM_MOUSEMOVE's for our own windows
		M_HANDLEMOUSEMOVE,

		// Calls SetWindowPlacement...
		M_SET_WINDOW_PLACEMENT,


		// Set the visibility of the window
		M_SET_VISIBLE,

		/// Sent from a worker thread when calling LTextLabel::Name
		M_TEXT_UPDATE_NAME,

		/// Send when a window is losing it's mouse capture. Usually
		// because something else has requested it.
		M_LOSING_CAPTURE,
	
	#elif defined(LGI_SDL) || defined(HAIKU)

		/// Minimum value for application defined message ID's
		M_MOUSEENTER	= 900,
		M_MOUSEEXIT,
		M_COMMAND,
		M_CUT,
		M_COPY,
		M_PASTE,
		M_PULSE,
		M_SET_VISIBLE,
		M_MOUSE_CAPTURE_POLL,
		M_TEXT_UPDATE_NAME,
		M_ASSERT_DLG,
		M_CLOSE,
	
	#elif defined(MAC)

		/// Base point for system messages.
		M_SYSTEM						= 0,
		
		/// Message that indicates the user is trying to close a top level window.
		M_CLOSE							= (M_SYSTEM+92),

		/// \brief Mouse enter event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEENTER					= (M_SYSTEM+900),

		/// \brief Mouse exit event
		///
		/// a = bool Inside; // is the mouse inside the client area?\n
		/// b = MAKELONG(x, y); // mouse location
		M_MOUSEEXIT,

		// return (bool)
		M_WANT_DIALOG_PROC,

		M_MENU,
		M_COMMAND,
		M_DRAG_DROP,

		M_TRAY_NOTIFY,
		M_CUT,
		M_COPY,
		M_PASTE,
		M_PULSE,
		M_MOUSE_TRACK_UP,
		M_GTHREADWORK_COMPELTE,
		M_TEXT_UPDATE_NAME,
		M_SET_VISIBLE,
		M_SETPOS, // A=(LRect*)Rectangle, B=(LView*)this
		M_ASSERT_DLG,
		M_QUIT,
		M_ABOUT,
		M_PERFERENCES,
		M_HIDE,
		M_DESTROY,

	#endif
	
	M_DESCRIBE,
	M_CHANGE,
	M_DELETE,
	M_TABLE_LAYOUT,
	M_URL,
	M_LOG_TEXT,
	M_ASSERT_UI,
	M_INVALIDATE,			// A=(LRect*)Rectangle, B=(LView*)this
	M_RESIZE_TO_CONTENT,	// A=(int)Border (used by LItemContainer)
	M_SCROLL_TO,			// Sent from LTreeItem to LTree
	M_JOBS_LOADED,			// LHtml
	M_THREAD_COMPLETED,		// A=(LThread*)Thread
	M_SET_CTRL_NAME,		// A=(int)CtrlId, B=(LString*)Name
	M_SET_CTRL_ENABLE,		// A=(int)CtrlId, B=(bool)Enabled
	M_SET_CTRL_VISIBLE,		// A=(int)CtrlId, B=(bool)Visible

	#ifdef WINDOWS
	M_USER = WM_APP + 200
	#else
	M_USER = 1000
	#endif
};

class LgiClass LMessage
	#ifdef HAIKU
	: public BMessage
	#endif
{
public:
	#if defined(WINNATIVE)
		typedef LPARAM Param;
		typedef LRESULT Result;
	#elif defined(LGI_SDL)
	    typedef void *Param;
	    typedef NativeInt Result;
	#else
		typedef NativeInt Param;
		typedef NativeInt Result;
	#endif

	#if !defined(LGI_SDL) && !defined(HAIKU)
		int m;
	#endif
	#if defined(LGI_SDL)
	    SDL_Event Event;
	    struct EventParams
	    {
	    	Param a, b;
	    	EventParams(Param A, Param B)
	    	{
	    		a = A;
	    		b = B;
	    	}
	    };
	#elif defined(WINNATIVE)
		HWND hWnd;
		WPARAM a;
		LPARAM b;
	#elif !defined(HAIKU)
		Param a;
		Param b;
	#endif
	
	#if LGI_COCOA
		#if __OBJC__
		NSEvent *event;
		#else
		void *event;
		#endif
	#endif

	#if defined(HAIKU)
	static constexpr char *PropA = "lgiA";
	static constexpr char *PropB = "lgiB";
	static constexpr char *PropView = "lgiView";
	static constexpr char *PropCallback = "lgiCallback";
	typedef std::function<void()> InThreadCb;
	#endif

	LMessage()
	{
		#if defined(LGI_SDL)
			Set(0, 0, 0);
		#else
			#if defined(WINNATIVE)
				hWnd = 0;
			#endif
			#if !defined(LGI_SDL) && !defined(HAIKU)
				m = 0;
				a = 0;
				b = 0;
			#endif
		#endif
		#if LGI_COCOA
			event = NULL;
		#endif
	}

	LMessage
	(
		int M,
		#if defined(WINNATIVE)
			WPARAM A = 0, LPARAM B = 0
		#else
			Param A = 0, Param B = 0
		#endif
	)
	{
		#if defined(WINNATIVE)
			hWnd = 0;
		#endif
		#if LGI_COCOA
			event = NULL;
		#endif
		Set(M, A, B);
	}
	
	#if defined(LGI_SDL) || defined(HAIKU)
		int Msg();
		Param A();
		Param B();
	#else
		int Msg() { return m; }
		Param A() { return a; }
		Param B() { return b; }
	#endif
	void Set(int m, Param a, Param b);
	bool Send(class LViewI *View);
};

#ifdef LINUX
extern LMessage CreateMsg(int m, int a = 0, int b = 0);
#else
#define CreateMsg(m, a, b)				LMessage(m, a, b)
#endif


#endif
