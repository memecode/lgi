#pragma once

#include "lgi/common/Rect.h"
#include "lgi/common/Mutex.h"

#if defined(LGI_CARBON)
	LgiFunc void DumpHnd(HIViewRef v, int depth = 0);
#elif LGI_COCOA && defined(__OBJC__)
	#include "LCocoaView.h"
#endif


/// \brief The base class for all windows in the GUI.
///
/// This is the core object that all on screen windows inherit from. It encapsulates
/// a HWND on Win32, a GtkWidget on Linux, and a NSView for Mac OS X. Used by
/// itself it's not a top level window, for that see the LWindow class.
///
/// To create a top level window see LWindow or LDialog.
///
/// For a LView with scroll bars use LLayout.
///
class LgiClass LView : virtual public LViewI, virtual public LBase
{
	friend		class LWindow;
	friend		class LLayout;
	friend		class LControl;
	friend		class LMenu;
	friend		class LSubMenu;
	friend		class LScrollBar;
	friend		class LDialog;
	friend		class LDragDropTarget;
	friend		class LPopup;
	friend		class LWindowPrivate;
	friend		class LViewPrivate;

	friend		bool SysOnKey(LView *w, LMessage *m);

	#if defined(__GTK_H__)

		friend Gtk::gboolean lgi_widget_draw(Gtk::GtkWidget *widget, Gtk::cairo_t *cr);
		friend Gtk::gboolean lgi_widget_click(Gtk::GtkWidget *widget, Gtk::GdkEventButton *ev);
		friend Gtk::gboolean lgi_widget_motion(Gtk::GtkWidget *widget, Gtk::GdkEventMotion *ev);
		friend Gtk::gboolean GViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent  *event, LView *view);
		friend Gtk::gboolean PopupEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, class LPopup *This);
		friend Gtk::gboolean GtkViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, LView *This);
	
		virtual Gtk::gboolean OnGtkEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event);
public:
		virtual void OnGtkRealize();
		virtual void OnGtkDelete();

private:
	#endif
	
	#if defined WIN32

		friend		class LWindowsClass;
		friend		class LCombo;
		friend		LRESULT CALLBACK DlgRedir(OsView hWnd, UINT m, WPARAM a, LPARAM b);
		static		void CALLBACK TimerProc(OsView hwnd, UINT uMsg, UINT_PTR idEvent, uint32_t dwTime);

	#elif defined HAIKU
	
		friend class LAppPrivate;
		virtual void HaikuEvent(LMessage::Events event, BMessage *m);
	
	#endif
	
	#if defined(HAIKU) || defined(MAC)
	
		template<typename Parent> friend class LBView;
		static bool RecentlyDeleted(LViewI *v);

	#endif

	#if defined(LGI_SDL)

		friend Uint32 SDL_PulseCallback(Uint32 interval, LView *v);
		friend class LApp;

	#endif


	LRect				Pos;
	int					_InLock = 0;
	OsThreadId			_LockingThread = -1;

protected:
	class LViewPrivate	*d = NULL;

	#if LGI_VIEW_HANDLE && !defined(HAIKU)
	OsView				_View; // OS specific handle to view object
	#endif

	LView				*_Window = NULL;
	#ifndef HAIKU
	LMutex				*_Lock = NULL;
	#endif
	uint16				_BorderSize = 0;
	uint16				_IsToolBar = 0;
	int					WndFlags = 0;

	static LViewI		*_Capturing;
	static LViewI		*_Over;
	
	#ifndef LGI_VIEW_HASH
	#error "Define LGI_VIEW_HASH to 0 or 1"
	#endif
	#if LGI_VIEW_HASH
	
public:
	enum LockOp
	{
		OpCreate,
		OpDelete,
		OpExists,
	};

	static bool LockHandler(LViewI *v, LockOp Op);

	#endif

protected:
	
	#if defined WINNATIVE

		uint32_t GetStyle();
		void SetStyle(uint32_t i);
		uint32_t GetExStyle();
		void SetExStyle(uint32_t i);
		uint32_t GetDlgCode();
		void SetDlgCode(uint32_t i);

		/// \brief Gets the win32 class passed to CreateWindowEx()
		const char *GetClassW32();
		/// \brief Sets the win32 class passed to CreateWindowEx()
		void SetClassW32(const char *s);
		/// \brief Creates a class to pass to CreateWindowEx(). If this methed is not
		/// explicitly called then the string from GetClass() is used to create a class,
		/// which is usually the name of the object.
		LWindowsClass *CreateClassW32(const char *Class = NULL, HICON Icon = 0, int AddStyles = 0);

		virtual int		SysOnNotify(int Msg, int Code) { return 0; }
	
	#elif defined MAC
	
		bool _Attach(LViewI *parent);
		#if LGI_COCOA
		public:
			LPoint Flip(LPoint p);
			LRect Flip(LRect p);
			virtual void OnDealloc();
	
		#elif LGI_CARBON
			OsView _CreateCustomView();
			virtual bool _OnGetInfo(HISize &size, HISize &line, HIRect &bounds, HIPoint &origin) { return false; }
			virtual void _OnScroll(HIPoint &origin) {}
		#endif
	
	#endif

	#if !WINNATIVE

		LView *&PopupChild();
		virtual bool	_Mouse(LMouse &m, bool Move);
		void			_Focus(bool f);

	#endif
	
	#if LGI_COCOA
		protected:
	#endif


	// Complex Region searches
	
	/// Finds the largest rectangle in the region
	LRect *FindLargest(LRegion &r);

	/// Finds the smallest rectangle that would fit a window 'Sx' by 'Sy'
	LRect *FindSmallestFit(LRegion &r, int Sx, int Sy);

	/// Finds the largest rectangle on the  specified
	LRect *FindLargestEdge
	(
		/// The region to search
		LRegion &r,
		/// The edge to look at:
		/// \sa GV_EDGE_TOP, GV_EDGE_RIGHT, GV_EDGE_BOTTOM or GV_EDGE_LEFT
		int Edge
	);

	virtual void _Delete();
	LViewI *FindReal(LPoint *Offset = 0);
	bool HandleCapture(LView *Wnd, bool c);

	
	/// \sa LWindow::RegisterHook and LWindowHookType::LMouseEvents
	virtual bool OnViewMouse(LView *v, LMouse &m) override { return true; }

	/// \sa LWindow::RegisterHook and LWindowHookType::LKeyEvents
	virtual bool OnViewKey(LView *v, LKey &k) override { return false; }
	
	virtual void OnNcPaint(LSurface *pDC, LRect &r);

	/// List of children views.
	List<LViewI>	Children;

#if defined(LGI_SDL) || defined(LGI_COCOA)
public:
#endif
	virtual void	_Paint(LSurface *pDC = NULL, LPoint *Offset = NULL, LRect *Update = NULL);

public:
	/// \brief Creates a view/window.
	///
	/// On non-Win32 platforms the default argument is the class that redirects the
	/// C++ virtual event handlers to the LView handlers. Which is usually the
	/// 'DefaultOsView' class. If you pass NULL in a DefaultOsView will be created to
	/// do the job.
	LView
	(
		/// The handle that the OS knows the window by
		OsView wnd = NULL
	);

	/// Destructor
	virtual ~LView();

	/// Returns the OS handle of the view
	#if defined(HAIKU)
	static void HandleInThreadMessage(BMessage *Msg);
	#elif LGI_VIEW_HANDLE
	OsView Handle() const { return _View; }
	#endif

	/// Returns the ptr to a LView
	LView *GetLView() override { return this; }
	
	/// Returns the OS handle of the top level window
	OsWindow WindowHandle() override;

	// Attaching windows / heirarchy
	bool AddView(LViewI *v, int Where = -1) override;
	bool DelView(LViewI *v) override;
	bool HasView(LViewI *v) override;
	LArray<LViewI*> IterateViews() override;
	
	/// \brief Attaches the view to a parent view.
	///
	/// Each LView starts in an un-attached state. When you attach it to a Parent LView
	/// the view gains a OS-specific handle and becomes visible on the screen (if the
	/// Visible() property is TRUE). However if a view is inserted into the Children list
	/// of a LView and it's parent pointer is set correctly it will still paint on the
	/// screen without the OS knowing about it. This is known in Lgi as a "virtual window"
	/// and is primarily used to cut down on windowing resources. Mouse clicks are handled
	/// by the parent window and passed down to the virtual children. Virtual children
	/// are somewhat limited. They can't receive focus, or participate in drag and drop
	/// operations. If you want to see an example have a look at the LToolBar code.
	virtual bool Attach
	(
		/// The parent view or NULL for a top level window
		LViewI *p
	) override;
	
	/// Attachs all the views in the Children list if not already attached.
	virtual bool AttachChildren() override;
	
	/// Detachs a window from it's parent.
	virtual bool Detach() override;
	
	/// Returns true if the window is attached
	virtual bool IsAttached() override;
	
	/// Destroys the window async
	virtual void Quit(bool DontDelete = false) override;
	
	// Properties
	
	/// Gets the top level window that this view belongs to
	LWindow *GetWindow() override;
	
	/// Gets the parent view.
	LViewI *GetParent() override;

	/// \brief Sets the parent view.
	///
	/// This doesn't attach the window so that it will display. You should use LView::Attach for that.
	virtual void SetParent(LViewI *p) override;

	/// Sends a notification to the notify target or the parent chain
	void SendNotify(LNotification n = LNotifyValueChanged) override;
	
	/// Gets the window that receives event notifications
	LViewI *GetNotify() override;

	/// \brief Sets the view to receive event notifications.
	///
	/// The notify window will receive events when this view changes. By
	/// default the parent view receives the events.
	virtual void SetNotify(LViewI *n) override;

	/// \brief Each top level window (LWindow) has a lock. By calling this function
	/// you lock the whole LWindow and all it's children.
	bool Lock
	(
		/// The file name of the caller
		const char *file,
		/// The line number of the caller
		int line,
		/// The timeout in milli-seconds or -1 to block until locked.
		int TimeOut = -1
	) override;
	
	/// Unlocks the LWindow and that this view belongs to.
	void Unlock() override;

	/// Add this view to the event target sink dispatch hash table.
	/// This allows you to use PostThreadEvent with a handle. Which
	/// is safe even if the object is deleted (unlike the PostEvent
	/// member function).
	///
	/// Calling this multiple times only adds the view once, but it
	/// returns the same handle each time.
	/// The view is automatically removed from the dispatch on 
	/// deletion.
	///
	/// \returns the handle for PostThreadEvent.
	int AddDispatch() override;

	/// Called to process every message received by this window.
	/// This also calls CommonEvents which is for events common to all 
	/// platforms.
	LMessage::Result OnEvent(LMessage *Msg) override;

	/// Implements handlers for events common to all platforms.
	bool CommonEvents(LMessage::Result &result, LMessage *Msg);

	/// true if the view is enabled
	bool Enabled() override;
	
	/// Sets the enabled state
	void Enabled(bool e) override;
	
	/// true if the view is visible
	bool Visible() override;
	
	/// Hides/Shows the view
	void Visible
	(
		/// True if you want to show the view, False to hide the view/
		bool v
	) override;

	/// true if the view has keyboard focus
	bool Focus() override;
	
	/// Sets the keyboard focus state on the view.
	void Focus(bool f) override;
	
	/// Get/Set the drop source
	LDragDropSource *DropSource(LDragDropSource *Set = NULL) override;

	/// Get/Set the drop target
	LDragDropTarget *DropTarget(LDragDropTarget *Set = NULL) override;
	
	/// Sets the drop target state of this view
	bool DropTarget(bool t) override;

	/// \brief Gives this view a 1 or 2 px sunken border.
	///
	/// The size is set by the _BorderSize member variable. This border is
	/// not considered part of the client area. Mouse and drawing coordinates
	/// do not take it into account.
	bool Sunken() override;
	
	/// Sets a sunken border around the control
	void Sunken(bool i) override;

	/// true if the view has a flat border
	bool Flat() override;
	
	/// Sets the flat border state
	void Flat(bool i) override;
	
	/// \brief true if the view has a raised border
	///
	/// The size is set by the _BorderSize member variable. This border is
	/// not considered part of the client area. Mouse and drawing coordinates
	/// do not take it into account.
	bool Raised() override;
	
	/// Sets the raised border state
	void Raised(bool i) override;

	/// Draws an OS themed border
	void DrawThemeBorder(LSurface *pDC, LRect &r);

	/// \brief true if the control is currently executing in the GUI thread
	///
	/// Some OS functions are not thread safe, and can only be called in the GUI
	/// thread. In the Linux implementation the GUI thread can change from time
	/// to time. On Win32 it stays the same. In any case if this function returns
	/// true it's safe to do just about anything.
	bool InThread() override;

	/// \returns the thread the view runs in.
	/// On Windows/Linux/Mac this will be the GUI thread, there is only one.
	/// On Haiku this will be the BWindow's thread, there is one per LWindow.
	OsThreadId ViewThread();

protected:	
	class CallbackStore : public LMutex
	{
		LHashTbl<IntKey<int>, std::function<void()>*> map;

	public:
		CallbackStore() : LMutex("CallbackStore")
		{
		}

		int Add(std::function<void()> cb)
		{
			if (!LockWithTimeout(4000, _FL))
			{
				printf(LPrintfInt64 ": CbStore.Add lock timed out...\n", (int64_t) LCurrentThreadId());
				return -1;
			}			

			// Find a free slot...
			int id = 0;
			for (; map.Find(id); id++)
				;			
			
			map.Add(id, new std::function<void()>(std::move(cb)));
			// printf("%i: CbStore.Add %i\n", LCurrentThreadId(), id);

			Unlock();
			return id;
		}

		bool Call(int id)
		{
			if (!LockWithTimeout(4000, _FL))
			{
				printf(LPrintfInt64 ": CbStore.Call(%i) lock timed out...\n", (int64_t) LCurrentThreadId(), id);
				return false;
			}			

			bool status = false;
			auto cb = map.Find(id);
			if (cb)
			{
				(*cb)();
				if (map.Delete(id))
				{
					delete cb;
					status = true;
					// printf("%i: CbStore.Call %i\n", LCurrentThreadId(), id);
				}
			}

			Unlock();
			return status;
		}

		bool Delete(int id)
		{
			if (!LockWithTimeout(4000, _FL))
			{
				printf(LPrintfInt64 ": CbStore.Call(%i) lock timed out...\n", (int64_t) LCurrentThreadId(), id);
				return false;
			}			

			bool status = false;
			auto cb = map.Find(id);
			if (cb && map.Delete(id))
			{
				status = true;
				// printf("%i: CbStore.Delete %i\n", LCurrentThreadId(), id);
				delete cb;
			}

			Unlock();
			return status;
		}

		CallbackStore &operator =(const CallbackStore &cs)
		{
			return *this;
		}

	}	CbStore;

public:
	/// Run some code in the UI thread...
	/// But don't wait for any sort of response.
	/// \returns true if the callback was sent (but not necessarily processed).
	bool RunCallback(std::function<void()> Callback)
	{
		if (!Callback)
		{
			LgiTrace("%s:%i - No callback.\n", _FL);
			return false;
		}

		int id = CbStore.Add(std::move(Callback));

		return PostEvent(M_VIEW_RUN_CALLBACK, (LMessage::Param) id);
	}

	/// Run some code in the UI thread...
	/// This will block while waiting for the UI event loop to respond or a timeout
	/// to occur. Negative timeoutMs is "wait forever".
	///
	/// This code needs to handle 5 cases:
	/// - There is a parameter error and the code returns without sending a message.
	/// - RunCallback sends a message to the view but it's not processed. (view deleted in the meantime?)
	/// - RunCallback sends a message, but the caller times out before it's processed.
	/// - RunCallback sends a message, but the cancel object is set before a response happens.
	/// - RunCallback sends a message and it's processed.
	///
	/// The life time of the variables accessed by the M_VIEW_RUN_CALLBACK event handler can't be on the
	/// stack. If the timeout occurs and RunCallback exits before M_VIEW_RUN_CALLBACK is processed the
	/// stack variable will go out of scope and crash.
	///
	/// So the variables are stored in the CallbackParams object on the heap.
	template<typename T>
	LAutoPtr<T> RunCallback(const char *file, int line, std::function<T()> Callback, int timeoutMs = -1, LCancel *cancel = NULL)
	{
		LAutoPtr<T> result;

		if (!Callback)
		{
			LgiTrace("%s:%i - RunCallback err: No callback.\n", file, line);
			return result;
		}

		int id = CbStore.Add([Callback, &result]()
		{
			result.Reset(new T(Callback()));
		});

		auto ref = LString::Fmt("f=%s,ln=%i,cur=" LPrintfThreadId ",view=" LPrintfThreadId,
			file, line,
			LCurrentThreadId(),
			ViewThread());
		printf("%s: post M_VIEW_RUN_CALLBACK for %i...\n", ref.Get(), id);
		if (!PostEvent(M_VIEW_RUN_CALLBACK, id))
		{
			LgiTrace("%s: RunCallback PostEvent failed.\n", ref.Get());
			return result;
		}

		auto StartTs = LCurrentTime();
		auto Report = StartTs;
		while (!result)
		{
			auto Now = LCurrentTime();

			if (Now - Report > 500)
			{
				Report = Now;
				printf("%s: Waiting for M_VIEW_RUN_CALLBACK %i\n",
					ref.Get(),
					(int)(Now-StartTs));
			}

			if (timeoutMs >= 0 && (Now-StartTs) > timeoutMs)
			{
				printf("%s: RunCallback timed out.\n", ref.Get());
				break;
			}

			if (cancel && cancel->IsCancelled())
			{
				printf("%s: RunCallback cancelled.\n", ref.Get());
				break;
			}

			LSleep(10);
		}

		printf("%s: RunCallback finished: %p\n", ref.Get(), result.Get());
		CbStore.Delete(id);
		return result;
	}

	
	/// \brief Asyncronously posts an event to be received by this view
	virtual bool PostEvent
	(
		/// The command ID.
		/// \sa Should be M_USER or higher for custom events.
		int Cmd,
		/// The first 32-bits of data. Equivalent to wParam on Win32.
		LMessage::Param a = 0,
		/// The second 32-bits of data. Equivalent to lParam on Win32.
		LMessage::Param b = 0,
		/// Optional timeout in milliseconds
		int64_t TimeoutMs = -1
	) override;

	template<typename T>
	bool PostEvent(int Cmd, T *Ptr)
	{
		return PostEvent(Cmd, (LMessage::Param)Ptr, 0);
	}	

	/// \brief Sets the utf-8 text associated with this view
	///
	/// Name and NameW are interchangable. Using them in any order will convert the
	/// text between utf-8 and wide to satify any requirement. Generally once the opposing
	/// version of the string is required both the utf-8 and wide copies of the string
	/// remain cached in RAM until the Name is changed.
	bool Name(const char *n) override;
	
	/// Returns the utf-8 text associated with this view
	const char *Name() override;
	
	/// Sets the wide char text associated with this view
	bool NameW(const char16 *n) override;

	/// \brief Returns the wide char text associated with this view
	///
	/// On Win32 the wide characters are 16 bits, on unix systems they are 32-bit 
	/// characters.
	const char16 *NameW() override;

	/// \brief Gets the font this control should draw with.
	///
	/// The default font is the system font, owned by the LApp object.
	virtual LFont *GetFont() override;
	
	/// \brief Sets the font for this control
	///
	/// The lifetime of the font passed in is the responsibility of the caller.
	/// The LView object assumes the pointer will be valid at all times.
	virtual void SetFont(LFont *Fnt, bool OwnIt = false) override;

	/// Returns the cursor that should be displayed for the given location
	/// \returns a cursor type. i.e. LCUR_Normal from LgiDefs.h
	LCursor GetCursor(int x, int y) override;
	
	/// \brief Get the position of the view relitive to it's parent.
	virtual LRect &GetPos() override { return Pos; }
	/// Get the client region of the window relitive to itself (ie always 0,0-x,y)
	virtual LRect &GetClient(bool InClientSpace = true) override;
	/// Set the position of the view in terms of it's parent
	virtual bool SetPos(LRect &p, bool Repaint = false) override;
	/// Gets the width of the view in pixels
	int X() override { return Pos.X(); }
	/// Gets the height of the view in pixels.
	int Y() override { return Pos.Y(); }
	/// Gets the minimum size of the view
	LPoint GetMinimumSize() override;
	/// \brief Set the minimum size of the view.
	///
	/// Only works for top level windows.
	void SetMinimumSize(LPoint Size) override;

    /// Gets the style of the control
    class LCss *GetCss(bool Create = false) override;

	/// Resolve a CSS colour, e.g.:
	/// auto Back = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
	LColour StyleColour(int CssPropType, LColour Default, int Depth = 5);

    /// Sets the style of the control (will take ownership of 'css')
    void SetCss(LCss *css) override;
    /// Sets the CSS foreground or background colour
	bool SetColour(LColour &c, bool Fore) override;

	/// The class' name. Should be overriden in child classes to return the
	/// right class name. Mostly used for debugging, but in the win32 port it
	/// is also the default WIN32 class name passed to RegisterClass() in 
	/// LView::CreateClass().
	///
	/// \returns the Class' name for debugging
	const char *GetClass() override;

	/// The array of CSS class names.
	LString::Array *CssClasses() override;
	/// Any element level styles
	LString CssStyles(const char *Set = NULL) override;

	/// \brief Captures all mouse events to this view
	///
	/// Once you have mouse capture all mouse events will be passed to this
	/// view. i.e. during a mouse click.
	bool Capture(bool c) override;
	/// true if this view is capturing mouse events.
	bool IsCapturing() override;
	/// \brief Gets the current mouse location
	/// \return true on success
	bool GetMouse
	(
		/// The mouse location information returned
		LMouse &m,
		/// Get the location in screen coordinates
		bool ScreenCoords = false
	) override;

	/// \brief Gets the ID associated with the view
	///
	/// The ID of a view is designed to associate controls defined in resource
	/// files with a object at runtime via a C header file define.
	int GetId() const override;
	/// Sets the view's ID.
	void SetId(int i) override;
	/// true if this control is a tab stop.
	bool GetTabStop() override;
	/// \brief Sets whether this control is a tab stop.
	///
	/// A top stop is a control that receives focus if the user scrolls through the controls
	/// with the tab key.
	void SetTabStop(bool b) override;
	/// Gets the integer representation of the view's contents
	virtual int64 Value() override { return 0; }
	/// Sets the integer representation of the view's contents
	virtual void Value(int64 i) override {}
	#if LGI_VIEW_HANDLE
	/// Find a view by it's os handle
	virtual LViewI *FindControl(OsView hnd);
	#endif
	/// Returns the view by it's ID
	virtual LViewI *FindControl
	(
		// The ID to look for
		int Id
	) override;

	/// Gets the value of the control identified by the ID
	int64 GetCtrlValue(int Id) override;
	/// Sets the value of the control identified by the ID
	void SetCtrlValue(int Id, int64 i) override;
	/// Gets the name (text) of the control identified by the ID
	const char *GetCtrlName(int Id) override;
	/// Sets the name (text) of the control identified by the ID
	void SetCtrlName(int Id, const char *s) override;
	/// Gets the enabled state of the control identified by the ID
	bool GetCtrlEnabled(int Id) override;
	/// Sets the enabled state of the control identified by the ID
	void SetCtrlEnabled(int Id, bool Enabled) override;
	/// Gets the visible state of the control identified by the ID
	bool GetCtrlVisible(int Id) override;
	/// Sets the visible state of the control identified by the ID
	void SetCtrlVisible(int Id, bool Visible) override;

	/// Causes the given area of the view to be repainted to update the screen
	bool Invalidate
	(
		/// The rectangle of the view to repaint, or NULL for the entire view
		LRect *r = NULL,
		/// true if you want to wait for the update to happen
		bool Repaint = false,
		/// false to update in client coordinates, true to update the non client region
		bool NonClient = false
	) override;

	/// Causes the given area of the view to be repainted to update the screen
	bool Invalidate
	(
		/// The region of the view to repaint
		LRegion *r,
		/// true if you want to wait for the update to happen
		bool Repaint = false,
		/// false to update in client coordinates, true to update the non client region
		bool NonClient = false
	) override;

	/// true if the mouse event is over the view
	bool IsOver(LMouse &m) override;
	/// returns the sub window located at the point x,y	
	LViewI *WindowFromPoint(int x, int y, int DebugDepth = 0) override;
	/// Sets a timer to call the OnPulse() event
	void SetPulse
	(
		/// The milliseconds between calls to OnPulse() or -1 to disable
		int Ms = -1
	) override;
	/// Convert a point form view coordinates to screen coordinates
	bool PointToScreen(LPoint &p) override;
	/// Convert a point form screen coordinates to view coordinates
	bool PointToView(LPoint &p) override;
	/// Get the x,y offset from the virtual window to the first real view in the parent chain
	bool WindowVirtualOffset(LPoint *Offset) override;
	/// Get the size of the window borders
	LPoint &GetWindowBorderSize() override;
	/// Layout all the child views
	virtual bool Pour
	(
		/// The available space to lay out the views into
		LRegion &r
	) override { return false; }

	/// The mouse was clicked over this view
	void OnMouseClick
	(
		/// The event parameters
		LMouse &m
	) override;
	/// Mouse moves into the area over the control
	void OnMouseEnter
	(
		/// The event parameters
		LMouse &m
	) override;
	/// Mouse leaves the area over the control
	void OnMouseExit
	(
		/// The event parameters
		LMouse &m
	) override;
	/// The mouse moves over the control
	void OnMouseMove
	(
		/// The event parameters
		LMouse &m
	) override;
	/// The mouse wheel was scrolled.
	bool OnMouseWheel
	(
		/// The amount scrolled
		double Lines
	) override;
	/// A key was pressed while this view has focus
	bool OnKey(LKey &k) override;
	/// The view is attached
	void OnCreate() override;
	/// The view is detached
	void OnDestroy() override;
	/// The view gains or loses the keyboard focus
	void OnFocus
	(
		/// True if the control is receiving focus
		bool f
	) override;
	/// \brief Called every so often by the timer system.
	/// \sa SetPulse()
	void OnPulse() override;
	/// Called when the view position changes
	void OnPosChange() override;
	/// Called on a top level window when something requests to close the window
	/// \returns true if it's ok to continue shutting down.
	bool OnRequestClose
	(
		/// True if the operating system is shutting down.
		bool OsShuttingDown
	) override;
	/// Return the type of cursor that should be visible when the mouse is at x,y
	/// e.g. #LCUR_Normal
	int OnHitTest
	(
		/// The x coordinate in view coordinates
		int x,
		/// The y coordinate in view coordinates
		int y
	) override;
	/// Called when the contents of the Children list have changed.
	void OnChildrenChanged(LViewI *Wnd, bool Attaching) override;
	/// Called to paint the on screen representation of the view
	void OnPaint(LSurface *pDC) override;

	/// \brief Called when a child view or view with it's SetNotify() set to this window changes.
	///
	/// The event by default will bubble up to the LWindow at the top of the window hierarchy visiting
	/// each LView on the way. If it reaches a LView that processes it then the event stops propagating
	/// up the hierarchy.
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;

	/// Called when a menu command is activated by the user.
	int OnCommand(int Cmd, int Event, OsView Wnd) override;
	/// Called after the view is attached to a new parent
	void OnAttach() override;
	
	/// Called to get layout information for the control. It's called
	/// up to 3 times to collect various dimensions:
	/// 1) PreLayout: Get the maximum width, and optionally the minimum width.
	///		Called with both widths set to zero.
	///		Must fill out Inf.Width.Max. Use -1 to fill all available space.
	///		Optionally fill out the min width.
	/// 2) Layout: Called to work out row height.
	///		Called with:
	///			- Width.Min unchanged from previous call.
	///			- Width.Max is limited to Cell size.
	///		Must fill out Inf.Height.Max.
	///		Min height currently not used.
	/// 3) PostLayout: Called to position view in cell.
	///		Not called.
	bool OnLayout(LViewLayoutInfo &Inf) override { return false; }

	/// This class allows some task to tap into the events the view receives.
	class LgiClass ViewEventTarget :
		public LEventSinkI,
		public LEventTargetI
	{
		friend class LView;

	protected:
		LView *view = NULL;
		LHashTbl<IntKey<int>,bool> Msgs;

	public:
		constexpr static int OBJ_DELETED = -1000;
	
		ViewEventTarget
		(
			/// The view to attach to.
			LView *View,
			/// The message to handle... more can be added to 'Msgs'.
			/// Pass 0 to get all messages, at the cost of slowing the
			/// App down a bunch. Not recommended.
			int Msg
		);
		~ViewEventTarget();

		// Post events to the view, which will return to the OnEvent handler..
		bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t TimeoutMs = -1);

		// Receive OnPulse events.
		virtual void OnPulse() {}

		// Impl LMessage::Result OnEvent(LMessage *Msg) in your subclass
	};

	#if defined(_DEBUG)
	bool _Debug;
	void Debug();
	void _Dump(int Depth = 0);
	#endif
};

#if LGI_VIEW_HANDLE
LgiFunc LView *LViewFromHandle(OsView hWnd);
#endif


/// \brief Factory for creating view's by name.
///
/// Inherit from this to add a new factory to create objects. Override
/// NewView() to create your control.
class LgiClass LViewFactory
{
	/** \brief Create a view by name

		\code
		if (strcmp(Class, "MyControl") == 0)
		{
			return new MyControl;
		}
		\endcode
	*/
	virtual LView *NewView
	(
		/// The name of the class to create
		const char *Class,
		/// The initial position of the view
		LRect *Pos,
		/// The initial text of the view
		const char *Text
	) = 0;

public:
	LViewFactory();
	virtual ~LViewFactory();

	/// Create a view by name.
	static LView *Create(const char *Class, LRect *Pos = NULL, const char *Text = NULL);
};

#define DeclFactory(CLS) \
	class CLS ## Factory : public LViewFactory \
	{ \
		LView *NewView(const char *Name, LRect *Pos, const char *Text) \
		{ \
			if (!_stricmp(Name, #CLS)) \
				return new CLS; \
			return NULL; \
		} \
	}	CLS ## FactoryInst;

#define DeclFactoryParam1(CLS, Param1) \
	class CLS ## Factory : public LViewFactory \
	{ \
		LView *NewView(const char *Name, LRect *Pos, const char *Text) \
		{ \
			if (!_stricmp(Name, #CLS)) \
				return new CLS(Param1); \
			return NULL; \
		} \
	}	CLS ## FactoryInst;


/// Control widget base class
class LgiClass LControl :
	public LView
{
	friend class LDialog;

protected:
	#if WINNATIVE
	bool *SetOnDelete = NULL;
	LWindowsClass *SubClass = NULL;
	#endif

	LPoint SizeOfStr(const char *Str);

public:
	#if WINNATIVE
	LControl(const char *SubClassName = NULL);
	#else
	LControl(OsView view = NULL);
	#endif

	~LControl();

	LMessage::Result OnEvent(LMessage *Msg);
};

/// Clickable "thing"
class LgiClass LClickable
{
public:
	// Click handling: to override default action, use one of these:
	std::function<void(const LMouse &m)> onClickFn;
	virtual void OnClick(const LMouse &m)
	{
		if (onClickFn)
			onClickFn(m);
	}
};
