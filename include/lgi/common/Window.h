#ifndef _LWINDOW_H_
#define _LWINDOW_H_

/// The available states for a top level window
enum LWindowZoom
{
	/// Minimized
	GZoomMin,
	/// Restored/Normal
	GZoomNormal,
	/// Maximized
	GZoomMax
};

enum LWindowHookType
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
class LgiClass LWindow :
	public GView,
	// This needs to be second otherwise is causes v-table problems.
	#ifndef LGI_SDL
	virtual
	#endif
	public GDragDropTarget
{
	friend class BViewRedir;
	friend class GApp;
	friend class GView;
	friend class GButton;
	friend class LDialog;
	friend class LWindowPrivate;
	friend struct LDialogPriv;

	bool _QuitOnClose;

protected:
	class LWindowPrivate *d;

	#if WINNATIVE

		GRect OldPos;
		LWindow *_Dialog;

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

	void SetChildDialog(LDialog *Dlg);
	void SetDragHandlers(bool On);

public:
	#ifdef _DEBUG
	GMemDC DebugDC;
	#endif

	#ifdef __GTK_H__
		LWindow(Gtk::GtkWidget *w = NULL);
	#elif LGI_CARBON
		LWindow(WindowRef wr = NULL);
	#elif LGI_COCOA
		LWindow(OsWindow wnd = NULL);
	#else
		LWindow();
	#endif
	~LWindow();

	const char *GetClass() override { return "GWindow"; }

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
	LWindowZoom GetZoom();
	
	/// Sets the current zoom
	void SetZoom(LWindowZoom i);
	
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
		LWindowHookType EventType,
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
	virtual void OnZoom(LWindowZoom Action) {}
	
	/// Called when the tray icon is clicked. (if present)
	virtual void OnTrayClick(GMouse &m);

	/// Called when the tray icon menu is about to be displayed.
	virtual void OnTrayMenu(LSubMenu &m) {}
	/// Called when the tray icon menu item has been selected.
	virtual void OnTrayMenuResult(int MenuId) {}
	
	/// Called when files are dropped on the window.
	virtual void OnReceiveFiles(GArray<const char*> &Files) {}
	
	/// Called when a URL is sent to the window
	virtual void OnUrl(const char *Url) {};

	///////////////// Implementation ////////////////////////////
	void OnPosChange() override;
	GMessage::Result OnEvent(GMessage *Msg) override;
	void OnPaint(GSurface *pDC) override;
	bool HandleViewMouse(GView *v, GMouse &m);
	bool HandleViewKey(GView *v, GKey &k);
	bool OnRequestClose(bool OsShuttingDown) override;
	bool Obscured();
	bool Visible() override;
	void Visible(bool i) override;
	bool IsActive();
	bool SetActive();
	GRect &GetPos() override;
	void SetDecor(bool Visible);
	LPoint GetDpi();
	LPointF GetDpiScale();
	
	// D'n'd
	int WillAccept(GDragFormats &Formats, LPoint Pt, int KeyState) override;
	int OnDrop(GArray<GDragData> &Data, LPoint Pt, int KeyState) override;

	#if !WINNATIVE
	
		bool Attach(GViewI *p) override;

		// Props
		OsWindow WindowHandle() override { return Wnd; }
		bool Name(const char *n) override;
		const char *Name() override;
		bool SetPos(GRect &p, bool Repaint = false) override;
		GRect &GetClient(bool InClientSpace = true) override;
	
		// Events
		void OnChildrenChanged(GViewI *Wnd, bool Attaching) override;
		void OnCreate() override;
		virtual void OnFrontSwitch(bool b);

	#endif

	#if defined(LGI_SDL)

		virtual bool PushWindow(LWindow *v);
		virtual LWindow *PopWindow();
	
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
	
		bool PostEvent(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0) override;
		void Quit(bool DontDelete = false) override;
		int OnCommand(int Cmd, int Event, OsView Wnd) override;
		GViewI *WindowFromPoint(int x, int y, int DebugDebug = 0) override;
		
		#if defined(LGI_CARBON)
			OSErr HandlerCallback(DragTrackingMessage *tracking, DragRef theDrag);
		#endif

	#endif
};

#endif
