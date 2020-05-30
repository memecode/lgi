#ifndef _GVIEW_H_
#define _GVIEW_H_

#if defined(LGI_CARBON)
LgiFunc void DumpHnd(HIViewRef v, int depth = 0);
#elif LGI_COCOA && defined(__OBJC__)
#include "LCocoaView.h"
#endif


/// \brief The base class for all windows in the GUI.
///
/// This is the core object that all on screen windows inherit from. It encapsulates
/// a HWND on Win32, a GtkWidget on Linux, and a NSView for Mac OS X. Used by
/// itself it's not a top level window, for that see the GWindow class.
///
/// To create a top level window see GWindow or GDialog.
///
/// For a GView with scroll bars use GLayout.
///
class LgiClass GView : virtual public GViewI, virtual public GBase
{
	friend		class GWindow;
	friend		class GLayout;
	friend		class GControl;
	friend		class LMenu;
	friend		class LSubMenu;
	friend		class GWnd;
	friend		class GScrollBar;
	friend		class GFileTarget;
	friend		class GDialog;
	friend		class GDragDropTarget;
	friend		class GPopup;
	friend		class GWindowPrivate;

	friend		bool SysOnKey(GView *w, GMessage *m);

	#if defined(__GTK_H__)

		friend Gtk::gboolean lgi_widget_expose(Gtk::GtkWidget *widget, Gtk::GdkEventExpose *e);
		friend Gtk::gboolean lgi_widget_draw(Gtk::GtkWidget *widget, Gtk::cairo_t *cr);
		friend Gtk::gboolean lgi_widget_click(Gtk::GtkWidget *widget, Gtk::GdkEventButton *ev);
		friend Gtk::gboolean lgi_widget_motion(Gtk::GtkWidget *widget, Gtk::GdkEventMotion *ev);
		friend Gtk::gboolean GViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent  *event, GView *view);
		friend Gtk::gboolean PopupEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, class GPopup *This);
		friend Gtk::gboolean GtkViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, GView *This);
	
		virtual Gtk::gboolean OnGtkEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event);
public:
		virtual void OnGtkRealize();
		virtual void OnGtkDelete();

private:
	#endif
	
	#if defined WIN32

		friend		class GWin32Class;
		friend		class GCombo;
		friend		LRESULT CALLBACK DlgRedir(OsView hWnd, UINT m, WPARAM a, LPARAM b);
		static		void CALLBACK TimerProc(OsView hwnd, UINT uMsg, UINT_PTR idEvent, uint32_t dwTime);

	#elif defined MAC
	
		#if LGI_COCOA

		#elif LGI_CARBON

			friend OSStatus LgiWindowProc(EventHandlerCallRef, EventRef, void *);
			friend OSStatus LgiRootCtrlProc(EventHandlerCallRef, EventRef, void *);
			friend OSStatus CarbonControlProc(EventHandlerCallRef, EventRef, void *);
			friend OSStatus GViewProc(EventHandlerCallRef, EventRef, void *);
			friend OSStatus LgiViewDndHandler(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);

		#endif

	#endif

	#if defined(LGI_SDL)

		friend Uint32 SDL_PulseCallback(Uint32 interval, GView *v);
		friend class GApp;

	#endif


	GRect				Pos;
	int					_InLock;

protected:
	class GViewPrivate	*d;

	#if LGI_VIEW_HANDLE
	OsView				_View; // OS specific handle to view object
	#endif

	GView				*_Window;
	LMutex				*_Lock;
	uint16				_BorderSize;
	uint16				_IsToolBar;
	int					WndFlags;

	static GViewI		*_Capturing;
	static GViewI		*_Over;
	
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

	static bool LockHandler(GViewI *v, LockOp Op);

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
		GWin32Class *CreateClassW32(const char *Class = 0, HICON Icon = 0, int AddStyles = 0);

		virtual int		SysOnNotify(int Msg, int Code) { return 0; }
	
	#elif defined MAC
	
		bool _Attach(GViewI *parent);
		#if LGI_COCOA
		public:
			GdcPt2 Flip(GdcPt2 p);
			GRect Flip(GRect p);
			virtual void OnDealloc();
	
		#elif LGI_CARBON
			OsView _CreateCustomView();
			virtual bool _OnGetInfo(HISize &size, HISize &line, HIRect &bounds, HIPoint &origin) { return false; }
			virtual void _OnScroll(HIPoint &origin) {}
		#endif
	
	#endif

	#if !WINNATIVE

		GView *&PopupChild();
		virtual bool	_Mouse(GMouse &m, bool Move);
		void			_Focus(bool f);

	#endif
	
	#if LGI_COCOA
		protected:
	#endif


	// Complex Region searches
	
	/// Finds the largest rectangle in the region
	GRect *FindLargest(GRegion &r);

	/// Finds the smallest rectangle that would fit a window 'Sx' by 'Sy'
	GRect *FindSmallestFit(GRegion &r, int Sx, int Sy);

	/// Finds the largest rectangle on the  specified
	GRect *FindLargestEdge
	(
		/// The region to search
		GRegion &r,
		/// The edge to look at:
		/// \sa GV_EDGE_TOP, GV_EDGE_RIGHT, GV_EDGE_BOTTOM or GV_EDGE_LEFT
		int Edge
	);

	virtual void _Delete();
	GViewI *FindReal(GdcPt2 *Offset = 0);
	bool HandleCapture(GView *Wnd, bool c);

	
	virtual bool OnViewMouse(GView *v, GMouse &m) { return true; }
	virtual bool OnViewKey(GView *v, GKey &k) { return false; }
	virtual void OnNcPaint(GSurface *pDC, GRect &r);

	/// List of children views.
	friend class GViewIter;
	List<GViewI>	Children;

#if defined(LGI_SDL) || defined(LGI_COCOA)
public:
#endif
	virtual void	_Paint(GSurface *pDC = NULL, GdcPt2 *Offset = NULL, GRect *Update = NULL);

public:
	/// \brief Creates a view/window.
	///
	/// On non-Win32 platforms the default argument is the class that redirects the
	/// C++ virtual event handlers to the GView handlers. Which is usually the
	/// 'DefaultOsView' class. If you pass NULL in a DefaultOsView will be created to
	/// do the job.
	GView
	(
		/// The handle that the OS knows the window by
		OsView wnd = NULL
	);

	/// Destructor
	virtual ~GView();

	/// Returns the OS handle of the view
	#if LGI_VIEW_HANDLE
	OsView Handle() { return _View; }
	#endif

	/// Returns the ptr to a GView
	GView *GetGView() { return this; }
	
	/// Returns the OS handle of the top level window
	OsWindow WindowHandle();

	// Attaching windows / heirarchy
	bool AddView(GViewI *v, int Where = -1);
	bool DelView(GViewI *v);
	bool HasView(GViewI *v);
	GViewIterator *IterateViews();
	
	/// \brief Attaches the view to a parent view.
	///
	/// Each GView starts in an un-attached state. When you attach it to a Parent GView
	/// the view gains a OS-specific handle and becomes visible on the screen (if the
	/// Visible() property is TRUE). However if a view is inserted into the Children list
	/// of a GView and it's parent pointer is set correctly it will still paint on the
	/// screen without the OS knowing about it. This is known in Lgi as a "virtual window"
	/// and is primarily used to cut down on windowing resources. Mouse clicks are handled
	/// by the parent window and passed down to the virtual children. Virtual children
	/// are somewhat limited. They can't receive focus, or participate in drag and drop
	/// operations. If you want to see an example have a look at the GToolBar code.
	virtual bool Attach
	(
		/// The parent view or NULL for a top level window
		GViewI *p
	);
	
	/// Attachs all the views in the Children list if not already attached.
	virtual bool AttachChildren();
	
	/// Detachs a window from it's parent.
	virtual bool Detach();
	
	/// Returns true if the window is attached
	virtual bool IsAttached();
	
	/// Destroys the window async
	virtual void Quit(bool DontDelete = false);
	
	// Properties
	
	/// Gets the top level window that this view belongs to
	GWindow *GetWindow();
	
	/// Gets the parent view.
	GViewI *GetParent();

	/// \brief Sets the parent view.
	///
	/// This doesn't attach the window so that it will display. You should use GView::Attach for that.
	virtual void SetParent(GViewI *p);

	/// Sends a notification to the notify target or the parent chain
	void SendNotify(int Data = 0);
	
	/// Gets the window that receives event notifications
	GViewI *GetNotify();

	/// \brief Sets the view to receive event notifications.
	///
	/// The notify window will receive events when this view changes. By
	/// default the parent view receives the events.
	virtual void SetNotify(GViewI *n);

	/// \brief Each top level window (GWindow) has a lock. By calling this function
	/// you lock the whole GWindow and all it's children.
	bool Lock
	(
		/// The file name of the caller
		const char *file,
		/// The line number of the caller
		int line,
		/// The timeout in milli-seconds or -1 to block until locked.
		int TimeOut = -1
	);
	
	/// Unlocks the GWindow and that this view belongs to.
	void Unlock();

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
	int AddDispatch();

	/// Called to process every message received by this window.
	GMessage::Result OnEvent(GMessage *Msg);

	/// true if the view is enabled
	bool Enabled();
	
	/// Sets the enabled state
	void Enabled(bool e);
	
	/// true if the view is visible
	bool Visible();
	
	/// Hides/Shows the view
	void Visible
	(
		/// True if you want to show the view, False to hide the view/
		bool v
	);

	/// true if the view has keyboard focus
	bool Focus();
	
	/// Sets the keyboard focus state on the view.
	void Focus(bool f);
	
	/// Get/Set the drop source
	GDragDropSource *DropSource(GDragDropSource *Set = NULL);

	/// Get/Set the drop target
	GDragDropTarget *DropTarget(GDragDropTarget *Set = NULL);
	
	/// Sets the drop target state of this view
	bool DropTarget(bool t);

	/// \brief Gives this view a 1 or 2 px sunken border.
	///
	/// The size is set by the _BorderSize member variable. This border is
	/// not considered part of the client area. Mouse and drawing coordinates
	/// do not take it into account.
	bool Sunken();
	
	/// Sets a sunken border around the control
	void Sunken(bool i);

	/// true if the view has a flat border
	bool Flat();
	
	/// Sets the flat border state
	void Flat(bool i);
	
	/// \brief true if the view has a raised border
	///
	/// The size is set by the _BorderSize member variable. This border is
	/// not considered part of the client area. Mouse and drawing coordinates
	/// do not take it into account.
	bool Raised();
	
	/// Sets the raised border state
	void Raised(bool i);

	/// Draws an OS themed border
	void DrawThemeBorder(GSurface *pDC, GRect &r);

	/// \brief true if the control is currently executing in the GUI thread
	///
	/// Some OS functions are not thread safe, and can only be called in the GUI
	/// thread. In the Linux implementation the GUI thread can change from time
	/// to time. On Win32 it stays the same. In any case if this function returns
	/// true it's safe to do just about anything.
	bool InThread();
	
	/// \brief Asyncronously posts an event to be received by this view
	virtual bool PostEvent
	(
		/// The command ID.
		/// \sa Should be M_USER or higher for custom events.
		int Cmd,
		/// The first 32-bits of data. Equivalent to wParam on Win32.
		GMessage::Param a = 0,
		/// The second 32-bits of data. Equivalent to lParam on Win32.
		GMessage::Param b = 0
	);

	template<typename T>
	bool PostEvent(int Cmd, T *Ptr)
	{
		return PostEvent(Cmd, (GMessage::Param)Ptr, 0);
	}	

	/// \brief Sets the utf-8 text associated with this view
	///
	/// Name and NameW are interchangable. Using them in any order will convert the
	/// text between utf-8 and wide to satify any requirement. Generally once the opposing
	/// version of the string is required both the utf-8 and wide copies of the string
	/// remain cached in RAM until the Name is changed.
	bool Name(const char *n);
	
	/// Returns the utf-8 text associated with this view
	char *Name();
	
	/// Sets the wide char text associated with this view
	virtual bool NameW(const char16 *n);

	/// \brief Returns the wide char text associated with this view
	///
	/// On Win32 the wide characters are 16 bits, on unix systems they are 32-bit 
	/// characters.
	virtual char16 *NameW();

	/// \brief Gets the font this control should draw with.
	///
	/// The default font is the system font, owned by the GApp object.
	virtual GFont *GetFont();
	
	/// \brief Sets the font for this control
	///
	/// The lifetime of the font passed in is the responsibility of the caller.
	/// The GView object assumes the pointer will be valid at all times.
	virtual void SetFont(GFont *Fnt, bool OwnIt = false);

	/// Returns the cursor that should be displayed for the given location
	/// \returns a cursor type. i.e. LCUR_Normal from LgiDefs.h
	LgiCursor GetCursor(int x, int y);
	
	/// \brief Get the position of the view relitive to it's parent.
	virtual GRect &GetPos() { return Pos; }
	/// Get the client region of the window relitive to itself (ie always 0,0-x,y)
	virtual GRect &GetClient(bool InClientSpace = true);
	/// Set the position of the view in terms of it's parent
	virtual bool SetPos(GRect &p, bool Repaint = false);
	/// Gets the width of the view in pixels
	int X() { return Pos.X(); }
	/// Gets the height of the view in pixels.
	int Y() { return Pos.Y(); }
	/// Gets the minimum size of the view
	GdcPt2 GetMinimumSize();
	/// \brief Set the minimum size of the view.
	///
	/// Only works for top level windows.
	void SetMinimumSize(GdcPt2 Size);	

    /// Gets the style of the control
    class GCss *GetCss(bool Create = false);

	/// Resolve a CSS colour, e.g.:
	/// auto Back = StyleColour(GCss::PropBackgroundColor, LColour(L_MED));
	GColour StyleColour(int CssPropType, GColour Default, int Depth = 5);

    /// Sets the style of the control (will take ownership of 'css')
    void SetCss(GCss *css);
    /// Sets the CSS foreground or background colour
	bool SetColour(GColour &c, bool Fore);

	/// The class' name. Should be overriden in child classes to return the
	/// right class name. Mostly used for debugging, but in the win32 port it
	/// is also the default WIN32 class name passed to RegisterClass() in 
	/// GView::CreateClass().
	///
	/// \returns the Class' name for debugging
	const char *GetClass();

	/// The array of CSS class names.
	GString::Array *CssClasses();
	/// Any element level styles
	GString CssStyles(const char *Set = NULL);

	/// \brief Captures all mouse events to this view
	///
	/// Once you have mouse capture all mouse events will be passed to this
	/// view. i.e. during a mouse click.
	bool Capture(bool c);
	/// true if this view is capturing mouse events.
	bool IsCapturing();
	/// \brief Gets the current mouse location
	/// \return true on success
	bool GetMouse
	(
		/// The mouse location information returned
		GMouse &m,
		/// Get the location in screen coordinates
		bool ScreenCoords = false
	);

	/// \brief Gets the ID associated with the view
	///
	/// The ID of a view is designed to associate controls defined in resource
	/// files with a object at runtime via a C header file define.
	int GetId();
	/// Sets the view's ID.
	void SetId(int i);
	/// true if this control is a tab stop.
	bool GetTabStop();
	/// \brief Sets whether this control is a tab stop.
	///
	/// A top stop is a control that receives focus if the user scrolls through the controls
	/// with the tab key.
	void SetTabStop(bool b);
	/// Gets the integer representation of the view's contents
	virtual int64 Value() { return 0; }
	/// Sets the integer representation of the view's contents
	virtual void Value(int64 i) {}
	#if LGI_VIEW_HANDLE
	/// Find a view by it's os handle
	virtual GViewI *FindControl(OsView hnd);
	#endif
	/// Returns the view by it's ID
	virtual GViewI *FindControl
	(
		// The ID to look for
		int Id
	);

	/// Gets the value of the control identified by the ID
	int64 GetCtrlValue(int Id);
	/// Sets the value of the control identified by the ID
	void SetCtrlValue(int Id, int64 i);
	/// Gets the name (text) of the control identified by the ID
	char *GetCtrlName(int Id);
	/// Sets the name (text) of the control identified by the ID
	void SetCtrlName(int Id, const char *s);
	/// Gets the enabled state of the control identified by the ID
	bool GetCtrlEnabled(int Id);
	/// Sets the enabled state of the control identified by the ID
	void SetCtrlEnabled(int Id, bool Enabled);
	/// Gets the visible state of the control identified by the ID
	bool GetCtrlVisible(int Id);
	/// Sets the visible state of the control identified by the ID
	void SetCtrlVisible(int Id, bool Visible);

	/// Causes the given area of the view to be repainted to update the screen
	bool Invalidate
	(
		/// The rectangle of the view to repaint, or NULL for the entire view
		GRect *r = NULL,
		/// true if you want to wait for the update to happen
		bool Repaint = false,
		/// false to update in client coordinates, true to update the non client region
		bool NonClient = false
	);

	/// Causes the given area of the view to be repainted to update the screen
	bool Invalidate
	(
		/// The region of the view to repaint
		GRegion *r,
		/// true if you want to wait for the update to happen
		bool Repaint = false,
		/// false to update in client coordinates, true to update the non client region
		bool NonClient = false
	);

	/// true if the mouse event is over the view
	bool IsOver(GMouse &m);
	/// returns the sub window located at the point x,y	
	GViewI *WindowFromPoint(int x, int y, int DebugDepth = 0);
	/// Sets a timer to call the OnPulse() event
	void SetPulse
	(
		/// The milliseconds between calls to OnPulse() or -1 to disable
		int Ms = -1
	);
	/// Convert a point form view coordinates to screen coordinates
	bool PointToScreen(GdcPt2 &p);
	/// Convert a point form screen coordinates to view coordinates
	bool PointToView(GdcPt2 &p);
	/// Get the x,y offset from the virtual window to the first real view in the parent chain
	bool WindowVirtualOffset(GdcPt2 *Offset);	
	/// Get the size of the window borders
	GdcPt2 &GetWindowBorderSize();
	/// Layout all the child views
	virtual bool Pour
	(
		/// The available space to lay out the views into
		GRegion &r
	) { return false; }

	/// The mouse was clicked over this view
	void OnMouseClick
	(
		/// The event parameters
		GMouse &m
	);
	/// Mouse moves into the area over the control
	void OnMouseEnter
	(
		/// The event parameters
		GMouse &m
	);
	/// Mouse leaves the area over the control
	void OnMouseExit
	(
		/// The event parameters
		GMouse &m
	);
	/// The mouse moves over the control
	void OnMouseMove
	(
		/// The event parameters
		GMouse &m
	);
	/// The mouse wheel was scrolled.
	bool OnMouseWheel
	(
		/// The amount scrolled
		double Lines
	);
	/// A key was pressed while this view has focus
	bool OnKey(GKey &k);
	/// The view is attached
	void OnCreate();
	/// The view is detached
	void OnDestroy();
	/// The view gains or loses the keyboard focus
	void OnFocus
	(
		/// True if the control is receiving focus
		bool f
	);
	/// \brief Called every so often by the timer system.
	/// \sa SetPulse()
	void OnPulse();
	/// Called when the view position changes
	void OnPosChange();
	/// Called on a top level window when something requests to close the window
	/// \returns true if it's ok to continue shutting down.
	bool OnRequestClose
	(
		/// True if the operating system is shutting down.
		bool OsShuttingDown
	);
	/// Return the type of cursor that should be visible when the mouse is at x,y
	/// e.g. #LCUR_Normal
	int OnHitTest
	(
		/// The x coordinate in view coordinates
		int x,
		/// The y coordinate in view coordinates
		int y
	);
	/// Called when the contents of the Children list have changed.
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	/// Called to paint the onscreen representation of the view
	void OnPaint(GSurface *pDC);
	/// \brief Called when a child view or view with it's SetNotify() set to this window changes.
	///
	/// The event by default will bubble up to the GWindow at the top of the window heirarchy visiting
	/// each GView on the way. If it reaches a GView that processes it then the event stops propergating
	/// up the heirarchy.
	int OnNotify(GViewI *Ctrl, int Flags);
	/// Called when a menu command is activated by the user.
	int OnCommand(int Cmd, int Event, OsView Wnd);
	/// Called after the view is attached to a new parent
	void OnAttach();
	
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
	bool OnLayout(GViewLayoutInfo &Inf) { return false; }

	#if defined(_DEBUG)
	bool _Debug;
	void Debug();
	void _Dump(int Depth = 0);
	#endif
};

/// \brief Factory for creating view's by name.
///
/// Inherit from this to add a new factory to create objects. Override
/// NewView() to create your control.
class LgiClass GViewFactory
{
	/** \brief Create a view by name

		\code
		if (strcmp(Class, "MyControl") == 0)
		{
			return new MyControl;
		}
		\endcode
	*/
	virtual GView *NewView
	(
		/// The name of the class to create
		const char *Class,
		/// The initial position of the view
		GRect *Pos,
		/// The initial text of the view
		const char *Text
	) = 0;

public:
	GViewFactory();
	virtual ~GViewFactory();

	/// Create a view by name.
	static GView *Create(const char *Class, GRect *Pos = 0, const char *Text = 0);
};

#define DeclFactory(CLS) \
	class CLS ## Factory : public GViewFactory \
	{ \
		GView *NewView(const char *Name, GRect *Pos, const char *Text) \
		{ \
			if (!_stricmp(Name, #CLS)) return new CLS; \
			return NULL; \
		} \
	}	CLS ## FactoryInst;


#endif
