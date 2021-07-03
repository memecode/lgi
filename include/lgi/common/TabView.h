/// \file
/// \author Matthew Allen
/// \brief A tab view

#ifndef _GTABVIEW_H_
#define _GTABVIEW_H_

// Tab control
class GTabPage;
typedef List<GTabPage> TabPageList;

/// A tab control that displays multiple pages of information in a small area.
class LgiClass GTabView :
	public GLayout,
	public ResObject
{
	friend class GTabPage;
	class GTabViewPrivate *d;

protected:
	int TabY();
	LRect &CalcInset();

public:
	enum
	{
		NoBtn = -1,
		LeftScrollBtn = -2,
		RightScrollBtn = -3,
	};

	/// Creates the tab view object
	GTabView(	int id = -1,
				int x = 0,
				int y = 0,
				int cx = 1000,
				int cy = 1000,
				const char *name = 0,
				int Init = 0);
	~GTabView();
	
	const char *GetClass() { return "GTabView"; }

	/// Gets the selected tab
	int64 Value();
	/// Sets the selected tab
	void Value(int64 i);
	
	bool GetPourChildren();
	void SetPourChildren(bool b);

	/// Append an existing tab
	bool Append(GTabPage *Page, int Where = -1);
	/// Append a new tab with the title 'name'
	GTabPage *Append(const char *name, int Where = -1);
	/// Delete a tab
	bool Delete(GTabPage *Page);

	///.Returns the tab at position 'i'
	GTabPage *TabAt(int i);
	/// Gets a pointer to the current tab
	GTabPage *GetCurrent();
	/// Gets the number of tabs
	size_t GetTabs();
	
	// Impl
	bool Attach(GViewI *parent);
	GMessage::Result OnEvent(GMessage *Msg);
	GViewI *FindControl(int Id);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
	LRect &GetTabClient();

	#if defined(WINNATIVE) && !defined(LGI_SDL)
	GViewI *FindControl(HWND hCtrl);
	#endif

	int HitTest(LMouse &m);
	void OnMouseClick(LMouse &m);
	void OnPosChange();
	void OnPaint(LSurface *pDC);
	bool OnKey(LKey &k);
	void OnFocus(bool f);
	void OnCreate();
	void OnAttach();
	void OnStyleChange();
};

class LgiClass GTabPage :
	public GView,
	public LResourceLoad,
	public ResObject
{
	friend class GTabView;
	struct GTabPagePriv *d;
	bool Attach(GViewI *parent) override;

	// Vars
	GTabView *TabCtrl;
	
	// Position of the clickable UI element for selecting the tab.
	LRect TabPos;

	/// Draws the tab part of the control (in the parent's context)
	void PaintTab(LSurface *pDC, bool Selected);
	
	/// Gets the width of the content in the tab
	virtual int GetTabPx();
	
	/// True if the tab has a button
	bool Button;

	/// The location of the optional button after the tab's text (set during OnPaint)
	LRect BtnPos;

	/// This is called when the user clicks the button
	virtual void OnButtonClick(LMouse &m);

	/// This draws the button (should only draw within 'BtnPos')
	virtual void OnButtonPaint(LSurface *pDC);

	/// This is called when the user clicks the button
	virtual void OnTabClick(LMouse &m);

public:
	GTabPage(const char *name);
	~GTabPage();

	const char *GetClass() override { return "GTabPage"; }
	GColour GetBackground();

	const char *Name() override;
	bool Name(const char *n) override;
	int64 Value() override;
	void Value(int64 v) override;
	bool HasButton();
	void HasButton(bool b);
	GTabView *GetTabControl() { return TabCtrl; }

	GMessage::Result OnEvent(GMessage *Msg) override;
	void OnPaint(LSurface *pDC) override;
	bool OnKey(LKey &k) override;
	void OnFocus(bool b) override;
	void OnStyleChange();
	void SetFont(LFont *Font, bool OwnIt = false) override;

	void Append(GViewI *Wnd);
	bool Remove(GViewI *Wnd);
	bool LoadFromResource(int Resource);
	void Select();
};

#endif
