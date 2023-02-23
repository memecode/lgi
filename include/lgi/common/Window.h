#ifndef _LWINDOW_H_
#define _LWINDOW_H_

#include "lgi/common/View.h"

/// The available states for a top level window
enum LWindowZoom
{
	/// Minimized
	LZoomMin,
	/// Restored/Normal
	LZoomNormal,
	/// Maximized
	LZoomMax
};

enum LWindowHookType
{
	LNoEvents = 0,
	/// \sa LWindow::RegisterHook()
	LMouseEvents = 1,
	/// \sa LWindow::RegisterHook()
	LKeyEvents = 2,
	/// \sa LWindow::RegisterHook()
	LKeyAndMouseEvents = LMouseEvents | LKeyEvents,
};

/// A top level window.
class LgiClass LWindow :
	public LView,
	// This needs to be second otherwise is causes v-table problems.
	#ifndef LGI_SDL
	virtual
	#endif
	public LDragDropTarget
{
	friend class BViewRedir;
	friend class LApp;
	friend class LView;
	friend class LButton;
	friend class LDialog;
	friend class LWindowPrivate;
	friend struct LDialogPriv;

	bool _QuitOnClose;

protected:
	class LWindowPrivate *d;

	#if WINNATIVE

		LRect OldPos;
		LWindow *_Dialog;

	#elif defined(HAIKU)
		
		LWindowZoom _PrevZoom = LZoomNormal;
	
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
	LViewI *_Default;

	/// The menu on the window
	LMenu *Menu;

	void SetChildDialog(LDialog *Dlg);
	void SetDragHandlers(bool On);
	
	/// Haiku: This shuts down the window's thread cleanly.
	int WaitThread();

public:
	#ifdef _DEBUG
	LMemDC DebugDC;
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

	const char *GetClass() override { return "LWindow"; }

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
	/// loop when this LWindow is closed. This is really useful for your
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
	bool MoveSameScreen(LViewI *wnd);

	// Focus setting
	LViewI *GetFocus();
	enum FocusType
	{
		GainFocus,
		LoseFocus,
		ViewDelete
	};
	void SetFocus(LViewI *ctrl, FocusType type);
	
	/// Registers a watcher to receive OnView... messages before they
	/// are passed through to the intended recipient.
	bool RegisterHook
	(
		/// The target view.
		LView *Target,
		/// Combination of:
		///     #LMouseEvents - Where Target->OnViewMouse(...) is called for each click.
		/// and
		///     #LKeyEvents - Where Target->OnViewKey(...) is called for each key.
		/// OR'd together.
		LWindowHookType EventType,
		/// Not implemented
		int Priority = 0
	);

	/// Unregisters a hook target
	bool UnregisterHook(LView *Target);

	/// Gets the default view
	LViewI *GetDefault();
	/// Sets the default view
	void SetDefault(LViewI *v);

	/// Saves/loads the window's state, e.g. position, minimized/maximized etc
	bool SerializeState
	(
		/// The data store for reading/writing
		LDom *Store,
		/// The field name to use for storing settings under
		const char *FieldName,
		/// TRUE if loading the settings into the window, FALSE if saving to the store.
		bool Load
	);

	/// Builds a map of keyboard short cuts.
	typedef LHashTbl<IntKey<int>,LViewI*> ShortcutMap;
	void BuildShortcuts(ShortcutMap &Map, LViewI *v = NULL);

	////////////////////// Events ///////////////////////////////
	
	/// Called when the window zoom state changes.
	virtual void OnZoom(LWindowZoom Action) {}
	
	/// Called when the tray icon is clicked. (if present)
	virtual void OnTrayClick(LMouse &m);

	/// Called when the tray icon menu is about to be displayed.
	virtual void OnTrayMenu(LSubMenu &m) {}
	/// Called when the tray icon menu item has been selected.
	virtual void OnTrayMenuResult(int MenuId) {}
	
	/// Called when files are dropped on the window.
	virtual void OnReceiveFiles(LArray<const char*> &Files) {}
	
	/// Called when a URL is sent to the window
	virtual void OnUrl(const char *Url) {};

	///////////////// Implementation ////////////////////////////
	void OnPosChange() override;
	LMessage::Result OnEvent(LMessage *Msg) override;
	void OnPaint(LSurface *pDC) override;
	bool HandleViewMouse(LView *v, LMouse &m);
	bool HandleViewKey(LView *v, LKey &k);
	/// Return true to accept application quit
	bool OnRequestClose(bool OsShuttingDown) override;
	bool Obscured();
	bool Visible() override;
	void Visible(bool i) override;
	bool IsActive();
	bool SetActive();
	LRect &GetPos() override;
	void SetDecor(bool Visible);
	LPoint GetDpi();
	LPointF GetDpiScale();
	void ScaleSizeToDpi();
	
	// D'n'd
	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState) override;
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState) override;

	#if !WINNATIVE
	
		bool Attach(LViewI *p) override;

		// Props
		#if defined(HAIKU)
			OsWindow WindowHandle() override;
		#else
			OsWindow WindowHandle() override { return Wnd; }
		#endif
		bool Name(const char *n) override;
		const char *Name() override;
		bool SetPos(LRect &p, bool Repaint = false) override;
		LRect &GetClient(bool InClientSpace = true) override;
	
		// Events
		void OnChildrenChanged(LViewI *Wnd, bool Attaching) override;
		void OnCreate() override;
		virtual void OnFrontSwitch(bool b);

	#else

		OsWindow WindowHandle() override { return _View; }

	#endif

	#if defined(LGI_SDL)

		virtual bool PushWindow(LWindow *v);
		virtual LWindow *PopWindow();
	
	#elif defined __GTK_H__
	
		void OnGtkRealize();
		bool IsAttached();
		void Quit(bool DontDelete = false);
		LRect *GetDecorSize();
		bool TranslateMouse(LMouse &m);
		LViewI *WindowFromPoint(int x, int y, bool Debug = false);
		void _SetDynamic(bool b);
		void _OnViewDelete();
		void SetParent(LViewI *p) override;
	
	#elif defined(MAC)
	
		bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t TimeoutMs = -1) override;
		void Quit(bool DontDelete = false) override;
		int OnCommand(int Cmd, int Event, OsView Wnd) override;
		LViewI *WindowFromPoint(int x, int y, int DebugDebug = 0) override;
		
		#if defined(LGI_CARBON)
			OSErr HandlerCallback(DragTrackingMessage *tracking, DragRef theDrag);
		#endif

	#endif
};

#endif
