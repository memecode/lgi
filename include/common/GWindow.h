#ifndef _GWINDOW_H_
#define _GWINDOW_H_

/// The available states for a top level window
enum GWindowZoom
{
	/// Minimized
	GZoomMin,
	/// Restored/Normal
	GZoomNormal,
	/// Maximized
	GZoomMax
};

enum GWindowHookType
{
	GNoEvents = 0,
	/// \sa GWindow::RegisterHook()
	GMouseEvents = 1,
	/// \sa GWindow::RegisterHook()
	GKeyEvents = 2,
	/// \sa GWindow::RegisterHook()
	GKeyAndMouseEvents = GMouseEvents | GKeyEvents,
};

/// A top level window.
class LgiClass GWindow :
	public GView,
	// This needs to be second otherwise is causes v-table problems.
	#ifndef LGI_SDL
	virtual
	#endif
	public GDragDropTarget
{
	friend class BViewRedir;
	friend class GView;
	friend class GButton;
	friend class GDialog;
	friend class GApp;
	friend class GWindowPrivate;
	friend struct GDialogPriv;

	bool _QuitOnClose;

protected:
	class GWindowPrivate *d;

	#if WINNATIVE

		GRect OldPos;
		GWindow *_Dialog;

	#else

		OsWindow Wnd;
		void SetDeleteOnClose(bool i);

	#endif

	#if defined __GTK_H__

		friend class LMenu;
		friend void lgi_widget_size_allocate(Gtk::GtkWidget *widget, Gtk::GtkAllocation *allocation);
	
		Gtk::GtkWidget *_Root, *_VBox, *_MenuBar;
		void OnGtkDelete();
		Gtk::gboolean OnGtkEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event);

	#elif defined(LGI_CARBON)

		friend pascal OSStatus LgiWindowProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
		void _Delete();
		bool _RequestClose(bool os);
	
	#elif defined(__OBJC__)
	
	public:
		// This returns the root level content NSView
		NSView *Handle();
	protected:

	#endif

	/// The default button
	GViewI *_Default;

	/// The menu on the window
	LMenu *Menu;

	void SetChildDialog(GDialog *Dlg);
	void SetDragHandlers(bool On);

public:
	#ifdef _DEBUG
	GMemDC DebugDC;
	#endif

	#ifdef __GTK_H__
		GWindow(Gtk::GtkWidget *w = NULL);
	#elif LGI_CARBON
		GWindow(WindowRef wr = NULL);
	#elif LGI_COCOA
		GWindow(OsWindow wnd = NULL);
	#else
		GWindow();
	#endif
	~GWindow();

	const char *GetClass() { return "GWindow"; }

	/// Lays out the child views into the client area.
	virtual void PourAll();

	/// Returns the current menu object
	LMenu *GetMenu() { return Menu; }
	
	/// Set the menu object.
	void SetMenu(LMenu *m) { Menu = m; }
	
	/// Set the window's icon
	bool SetIcon(const char *FileName);
	
	/// Gets the "quit on close" setting.
	bool GetQuitOnClose() { return _QuitOnClose; }

	/// \brief Sets the "quit on close" setting.
	///
	/// When this is switched on the application will quit the main message
	/// loop when this GWindow is closed. This is really useful for your
	/// main application window. Otherwise the UI will disappear but the
	/// application is still running.
	void SetQuitOnClose(bool i) { _QuitOnClose = i; }

	bool GetSnapToEdge();
	void SetSnapToEdge(bool b);

	bool GetAlwaysOnTop();
	void SetAlwaysOnTop(bool b);
	
	/// Gets the current zoom setting
	GWindowZoom GetZoom();
	
	/// Sets the current zoom
	void SetZoom(GWindowZoom i);
	
	/// Raises the window to the top of the stack.
	void Raise();

	/// Moves a top level window on screen.
	void MoveOnScreen();
	/// Moves a top level to the center of the screen
	void MoveToCenter();
	/// Moves a top level window to where the mouse is
	void MoveToMouse();
	/// Moves the window to somewhere on the same screen as 'wnd'
	bool MoveSameScreen(GViewI *wnd);

	// Focus setting
	GViewI *GetFocus();
	enum FocusType
	{
		GainFocus,
		LoseFocus,
		ViewDelete
	};
	void SetFocus(GViewI *ctrl, FocusType type);
	
	/// Registers a watcher to receive OnView... messages before they
	/// are passed through to the intended recipient.
	bool RegisterHook
	(
		/// The target view.
		GView *Target,
		/// Combination of:
		///     #GMouseEvents - Where Target->OnViewMouse(...) is called for each click.
		/// and
		///     #GKeyEvents - Where Target->OnViewKey(...) is called for each key.
		/// OR'd together.
		GWindowHookType EventType,
		/// Not implemented
		int Priority = 0
	);

	/// Unregisters a hook target
	bool UnregisterHook(GView *Target);

	/// Gets the default view
	GViewI *GetDefault();
	/// Sets the default view
	void SetDefault(GViewI *v);

	/// Saves/loads the window's state, e.g. position, minimized/maximized etc
	bool SerializeState
	(
		/// The data store for reading/writing
		GDom *Store,
		/// The field name to use for storing settings under
		const char *FieldName,
		/// TRUE if loading the settings into the window, FALSE if saving to the store.
		bool Load
	);

	/// Builds a map of keyboard short cuts.
	typedef LHashTbl<IntKey<int>,GViewI*> ShortcutMap;
	void BuildShortcuts(ShortcutMap &Map, GViewI *v = NULL);

	////////////////////// Events ///////////////////////////////
	
	/// Called when the window zoom state changes.
	virtual void OnZoom(GWindowZoom Action) {}
	
	/// Called when the tray icon is clicked. (if present)
	virtual void OnTrayClick(GMouse &m);

	/// Called when the tray icon menu is about to be displayed.
	virtual void OnTrayMenu(LSubMenu &m) {}
	/// Called when the tray icon menu item has been selected.
	virtual void OnTrayMenuResult(int MenuId) {}
	
	/// Called when files are dropped on the window.
	virtual void OnReceiveFiles(GArray<char*> &Files) {}
	
	/// Called when a URL is sent to the window
	virtual void OnUrl(const char *Url) {};

	///////////////// Implementation ////////////////////////////
	void OnPosChange();
	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	bool HandleViewMouse(GView *v, GMouse &m);
	bool HandleViewKey(GView *v, GKey &k);
	bool OnRequestClose(bool OsShuttingDown);
	bool Obscured();
	bool Visible();
	void Visible(bool i);
	bool IsActive();
	GRect &GetPos();

	// D'n'd
	int WillAccept(GDragFormats &Formats, GdcPt2 Pt, int KeyState);
	int OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState);

	#if !WINNATIVE
	
		bool Attach(GViewI *p);

		// Props
		OsWindow WindowHandle() { return Wnd; }
		bool Name(const char *n);
		char *Name();
		bool SetPos(GRect &p, bool Repaint = false);
		GRect &GetClient(bool InClientSpace = true);
	
		// Events
		void OnChildrenChanged(GViewI *Wnd, bool Attaching);
		void OnCreate();
		virtual void OnFrontSwitch(bool b);

	#endif

	#if defined(LGI_SDL)

		virtual bool PushWindow(GWindow *v);
		virtual GWindow *PopWindow();
	
	#elif defined __GTK_H__
	
		void OnGtkRealize();
		bool IsAttached();
		void Quit(bool DontDelete = false);
		GRect *GetDecorSize();
		bool TranslateMouse(GMouse &m);
		GViewI *WindowFromPoint(int x, int y, bool Debug);
		void _SetDynamic(bool b);
		void _OnViewDelete();
	
	#elif defined(MAC)
	
		bool PostEvent(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0);
		void Quit(bool DontDelete = false);
		int OnCommand(int Cmd, int Event, OsView Wnd);
		GViewI *WindowFromPoint(int x, int y, int DebugDebug = 0);
		
		#if defined(LGI_CARBON)
			OSErr HandlerCallback(DragTrackingMessage *tracking, DragRef theDrag);
		#endif

	#endif
};

#endif
