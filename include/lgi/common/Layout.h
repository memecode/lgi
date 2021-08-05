#ifndef _GLAYOUT_H_
#define _GLAYOUT_H_

///////////////////////////////////////////////////////////////////////////////////////////////
// Control or View window

/// FindLargestEdge parameter
/// \sa LView::FindLargest(LRegion &, int)
#define GV_EDGE_TOP				0x0001
/// FindLargestEdge parameter
/// \sa LView::FindLargest(LRegion &, int)
#define GV_EDGE_RIGHT			0x0002
/// FindLargestEdge parameter
/// \sa LView::FindLargest(LRegion &, int)
#define GV_EDGE_BOTTOM			0x0004
/// FindLargestEdge parameter
/// \sa LView::FindLargest(LRegion &, int)
#define GV_EDGE_LEFT			0x0008

/// Id of the vertical scroll bar in a LLayout control
#define IDC_VSCROLL				14000
/// Id of the horizontal scroll bar in a LLayout control
#define IDC_HSCROLL				14001

#ifdef MAC
#define XPLATFORM_GLAYOUT		1
#else
#define XPLATFORM_GLAYOUT		0
#endif

/// \brief A LView with scroll bars
///
/// This class adds scroll bars to the standard LView base class. The scroll bars can be
/// directly accessed using the VScroll and HScroll member variables. Although you should
/// always do a NULL check on the pointer before using, if the scroll bar is not activated
/// using LLayout::SetScrollBars then VScroll and/or HScroll will by NULL. When the scroll
/// bar is used to scroll the LLayout control you will receive an event on LView::OnNotify
/// with the control ID of the scrollbar, which is either #IDC_VSCROLL or #IDC_HSCROLL.
class LgiClass LLayout : public LView
{
	friend class GScroll;
	friend class LView;

	// Private variables
	bool			_SettingScrollBars;
	bool			_PourLargest;
	struct LayoutScroll
	{
		bool			SentMsg = 0;
		int				x = 0, y = 0;
	}					_SetScroll;
	bool			WantX, WantY;

protected:
	/// The vertical scroll bar
	LScrollBar		*VScroll;

	/// The horizontal scroll bar
	LScrollBar		*HScroll;

	/// Sets which of the scroll bars is visible
	virtual bool SetScrollBars
	(
		/// Make the horizontal scroll bar visible
		bool x,
		/// Make the vertical scroll bar visible
		bool y
	);
	
	#if defined(XPLATFORM_GLAYOUT)
	void AttachScrollBars();
	bool _SetScrollBars(bool x, bool y);
	#endif
	#if defined(MAC) && !XPLATFORM_GLAYOUT
	friend class GLayoutScrollBar;
	HISize Line;
	
	OsView RealWnd;
	bool _OnGetInfo(HISize &size, HISize &line, HIRect &bounds, HIPoint &origin);
	void _OnScroll(HIPoint &origin);
	void OnScrollConfigure();
	#endif

public:
	LLayout();
	~LLayout();

	const char *GetClass() override { return "LLayout"; }

	/// Gets the current scroll bar values.
	virtual void GetScrollPos(int64 &x, int64 &y);
	/// Sets the current scroll bar values
	virtual void SetScrollPos(int64 x, int64 y);

	/// Gets the "pour largest" setting
	bool GetPourLargest();
	/// \brief Sets the "pour largest" setting
	///
	/// When "pour largest" is switched on the pour function automatically
	/// lays the control into the largest rectangle available. This is useful
	/// for putting a single LView into a splitter pane or a tab view and having
	/// it just take up all the space.
	void SetPourLargest(bool i);

	/// Handles the incoming events.
	LMessage::Result OnEvent(LMessage *Msg) override;

	/// Lay out all the children views into the client area according to their
	/// own internal rules. Space is given in a first come first served basis.
	bool Pour(LRegion &r) override;

	// Impl
	#if defined(__GTK_H__) || !defined(WINNATIVE)

	bool Attach(LViewI *p) override;
	bool Detach() override;
	LRect &GetClient(bool InClientSpace = true) override;
	void OnCreate() override;
	
	#if defined(MAC) && !XPLATFORM_GLAYOUT

	bool Invalidate(LRect *r = NULL, bool Repaint = false, bool NonClient = false) override;
	bool Focus() override;
	void Focus(bool f) override;
	bool SetPos(LRect &p, bool Repaint = false) override;

	#else
	
	void OnPosChange() override;
	int OnNotify(LViewI *c, LNotification &n) override;
	int OnNotify(LViewI *c, int f) override;
	void OnNcPaint(LSurface *pDC, LRect &r) override;

	#endif
	#endif
	
	LViewI *FindControl(int Id) override;
	#if LGI_VIEW_HANDLE
	LViewI *FindControl(OsView hnd) override { return LView::FindControl(hnd); }
	#endif
};

#endif
