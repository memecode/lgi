#ifndef _GLAYOUT_H_
#define _GLAYOUT_H_

///////////////////////////////////////////////////////////////////////////////////////////////
// Control or View window

/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_TOP				0x0001
/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_RIGHT			0x0002
/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_BOTTOM			0x0004
/// FindLargestEdge parameter
/// \sa GView::FindLargest(GRegion &, int)
#define GV_EDGE_LEFT			0x0008

/// Id of the vertical scroll bar in a GLayout control
#define IDC_VSCROLL				14000
/// Id of the horizontal scroll bar in a GLayout control
#define IDC_HSCROLL				14001

#ifdef MAC
#define XPLATFORM_GLAYOUT		1
#else
#define XPLATFORM_GLAYOUT		0
#endif

/// \brief A GView with scroll bars
///
/// This class adds scroll bars to the standard GView base class. The scroll bars can be
/// directly accessed using the VScroll and HScroll member variables. Although you should
/// always do a NULL check on the pointer before using, if the scroll bar is not activated
/// using GLayout::SetScrollBars then VScroll and/or HScroll will by NULL. When the scroll
/// bar is used to scroll the GLayout control you will receive an event on GView::OnNotify
/// with the control ID of the scrollbar, which is either #IDC_VSCROLL or #IDC_HSCROLL.
class LgiClass GLayout : public GView
{
	friend class GScroll;
	friend class GView;

	// Private variables
	bool			_SettingScrollBars;
	bool			_PourLargest;

protected:
	/// The vertical scroll bar
	GScrollBar		*VScroll;

	/// The horizontal scroll bar
	GScrollBar		*HScroll;

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
	GLayout();
	~GLayout();

	const char *GetClass() override { return "GLayout"; }

	/// Gets the current scroll bar values.
	virtual void GetScrollPos(int &x, int &y);
	/// Sets the current scroll bar values
	virtual void SetScrollPos(int x, int y);

	/// Gets the "pour largest" setting
	bool GetPourLargest();
	/// \brief Sets the "pour largest" setting
	///
	/// When "pour largest" is switched on the pour function automatically
	/// lays the control into the largest rectangle available. This is useful
	/// for putting a single GView into a splitter pane or a tab view and having
	/// it just take up all the space.
	void SetPourLargest(bool i);

	/// Handles the incoming events.
	GMessage::Result OnEvent(GMessage *Msg) override;

	/// Lay out all the children views into the client area according to their
	/// own internal rules. Space is given in a first come first served basis.
	bool Pour(GRegion &r) override;

	// Impl
	#if defined(__GTK_H__) || !defined(WINNATIVE)

	bool Attach(GViewI *p) override;
	bool Detach() override;
	GRect &GetClient(bool InClientSpace = true) override;
	void OnCreate() override;
	
	#if defined(MAC) && !XPLATFORM_GLAYOUT

	bool Invalidate(GRect *r = NULL, bool Repaint = false, bool NonClient = false) override;
	bool Focus() override;
	void Focus(bool f) override;
	bool SetPos(GRect &p, bool Repaint = false) override;

	#else
	
	void OnPosChange() override;
	int OnNotify(GViewI *c, int f) override;
	void OnNcPaint(GSurface *pDC, GRect &r) override;

	#endif
	#endif
	
	GViewI *FindControl(int Id) override;
	#if LGI_VIEW_HANDLE
	GViewI *FindControl(OsView hnd) override { return GView::FindControl(hnd); }
	#endif
};

#endif