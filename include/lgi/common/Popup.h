/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A popup window
#pragma once

#include "lgi/common/Layout.h"

#if defined(LGI_CARBON) || defined(__GTK_H__)
#define LGI_POPUP_LWINDOW	1
#else
#define LGI_POPUP_LWINDOW	0
#endif

#if LGI_COCOA
ObjCWrapper(NSPanel, OsPanel)
#endif

/// A popup window: closes when the user clicks off-window.
class LgiClass LPopup :
	#if LGI_POPUP_LWINDOW
	public LWindow
	#else
	public LView
	#endif
{
	friend class _QPopup;
	friend class LWindow;
	friend class LDropDown;
	friend class LMouseHook;
	friend class LMouseHookPrivate;
    friend class LView;

    static LArray<LPopup*> CurrentPopups;

protected:
	class GPopupPrivate *d;
	bool Cancelled;
	LView *Owner;
	uint64 Start;
	LRect ScreenPos;
	
	#if LGI_COCOA
	OsPanel Panel;
	#endif

public:
	LPopup(LView *owner);
	~LPopup();

	#if LGI_COCOA
	OsPanel Handle() { return Panel; }
	LRect &GetPos() override;
	bool SetPos(LRect &r, bool repaint = false) override;
	#endif

	/// Sets whether the popup should take the focus when it's shown.
	/// The default is 'true'
	void TakeFocus(bool Take);
	const char *GetClass() override { return "LPopup"; }
	bool GetCancelled() { return Cancelled; }
	bool Attach(LViewI *p) override;
	void Visible(bool i) override;
	bool Visible() override;
	LMessage::Result OnEvent(LMessage *Msg) override;
};

/// Drop down menu, UI widget for opening a popup.
class LgiClass LDropDown : public LLayout
{
	friend class LPopup;
	
	LPopup *Popup;

public:
	LDropDown(int Id, int x, int y, int cx, int cy, LPopup *popup);
	~LDropDown();

	// Properties
	bool IsOpen();
	void SetPopup(LPopup *popup);
	LPopup *GetPopup();

	// Window events
	void OnFocus(bool f);
	void OnPaint(LSurface *pDC);
	bool OnKey(LKey &k);
	void OnMouseClick(LMouse &m);
	int OnNotify(LViewI *c, int f);

	// Override
	virtual void Activate();
	virtual void OnPopupClose() {}
};

/// Mouse hook grabs mouse events from the OS
class LPopup;
class LMouseHook
{
	class LMouseHookPrivate *d;
	
public:
	LMouseHook();
	~LMouseHook();

	void RegisterPopup(LPopup *p);
	void UnregisterPopup(LPopup *p);
	bool OnViewKey(LView *v, LKey &k);
	void TrackClick(LView *v);

	#ifdef WIN32
	static LRESULT CALLBACK MouseProc(int Code, WPARAM a, LPARAM b);
	#endif
};

