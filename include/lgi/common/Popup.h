/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A popup window

#ifndef __GPOPUP_H
#define __GPOPUP_H

#if defined(LGI_CARBON) || defined(__GTK_H__)
#define LGI_POPUP_GWINDOW	1
#else
#define LGI_POPUP_GWINDOW	0
#endif

#if LGI_COCOA
ObjCWrapper(NSPanel, OsPanel)
#endif

/// A popup window: closes when the user clicks off-window.
class LgiClass GPopup :
	#if LGI_POPUP_GWINDOW
	public GWindow
	#else
	public GView
	#endif
{
	friend class _QPopup;
	friend class GWindow;
	friend class GDropDown;
	friend class GMouseHook;
	friend class GMouseHookPrivate;
    friend class GView;

    static GArray<GPopup*> CurrentPopups;

protected:
	class GPopupPrivate *d;
	bool Cancelled;
	GView *Owner;
	uint64 Start;
	GRect ScreenPos;
	
	#if LGI_COCOA
	OsPanel Panel;
	#endif

public:
	GPopup(GView *owner);
	~GPopup();

	#if LGI_COCOA
	OsPanel Handle() { return Panel; }
	GRect &GetPos() override;
	bool SetPos(GRect &r, bool repaint = false) override;
	#endif

	/// Sets whether the popup should take the focus when it's shown.
	/// The default is 'true'
	void TakeFocus(bool Take);
	const char *GetClass() override { return "GPopup"; }
	bool GetCancelled() { return Cancelled; }
	bool Attach(GViewI *p) override;
	void Visible(bool i) override;
	bool Visible() override;
	GMessage::Result OnEvent(GMessage *Msg) override;
};

/// Drop down menu, UI widget for opening a popup.
class LgiClass GDropDown : public GLayout
{
	friend class GPopup;
	
	GPopup *Popup;

public:
	GDropDown(int Id, int x, int y, int cx, int cy, GPopup *popup);
	~GDropDown();

	// Properties
	bool IsOpen();
	void SetPopup(GPopup *popup);
	GPopup *GetPopup();

	// Window events
	void OnFocus(bool f);
	void OnPaint(GSurface *pDC);
	bool OnKey(GKey &k);
	void OnMouseClick(GMouse &m);
	int OnNotify(GViewI *c, int f);

	// Override
	virtual void Activate();
	virtual void OnPopupClose() {}
};

/// Mouse hook grabs mouse events from the OS
class GPopup;
class GMouseHook
{
	class GMouseHookPrivate *d;
	
public:
	GMouseHook();
	~GMouseHook();

	void RegisterPopup(GPopup *p);
	void UnregisterPopup(GPopup *p);
	bool OnViewKey(GView *v, GKey &k);
	void TrackClick(GView *v);

	#ifdef WIN32
	static LRESULT CALLBACK MouseProc(int Code, WPARAM a, LPARAM b);
	#endif
};




#endif
