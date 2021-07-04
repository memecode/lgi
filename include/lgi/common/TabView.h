/// \file
/// \author Matthew Allen
/// \brief A tab view
#pragma once

#include "lgi/common/Layout.h"

// Tab control
class LTabPage;
typedef List<LTabPage> TabPageList;

/// A tab control that displays multiple pages of information in a small area.
class LgiClass LTabView :
	public LLayout,
	public ResObject
{
	friend class LTabPage;
	class LTabViewPrivate *d;

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
	LTabView(	int id = -1,
				int x = 0,
				int y = 0,
				int cx = 1000,
				int cy = 1000,
				const char *name = 0,
				int Init = 0);
	~LTabView();
	
	const char *GetClass() { return "LTabView"; }

	/// Gets the selected tab
	int64 Value();
	/// Sets the selected tab
	void Value(int64 i);
	
	bool GetPourChildren();
	void SetPourChildren(bool b);

	/// Append an existing tab
	bool Append(LTabPage *Page, int Where = -1);
	/// Append a new tab with the title 'name'
	LTabPage *Append(const char *name, int Where = -1);
	/// Delete a tab
	bool Delete(LTabPage *Page);

	///.Returns the tab at position 'i'
	LTabPage *TabAt(int i);
	/// Gets a pointer to the current tab
	LTabPage *GetCurrent();
	/// Gets the number of tabs
	size_t GetTabs();
	
	// Impl
	bool Attach(LViewI *parent);
	GMessage::Result OnEvent(GMessage *Msg);
	LViewI *FindControl(int Id);
	int OnNotify(LViewI *Ctrl, int Flags);
	void OnChildrenChanged(LViewI *Wnd, bool Attaching);
	LRect &GetTabClient();

	#if defined(WINNATIVE) && !defined(LGI_SDL)
	LViewI *FindControl(HWND hCtrl);
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

class LgiClass LTabPage :
	public LView,
	public LResourceLoad,
	public ResObject
{
	friend class LTabView;
	struct LTabPagePriv *d;
	bool Attach(LViewI *parent) override;

	// Vars
	LTabView *TabCtrl;
	
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
	LTabPage(const char *name);
	~LTabPage();

	const char *GetClass() override { return "LTabPage"; }
	GColour GetBackground();

	const char *Name() override;
	bool Name(const char *n) override;
	int64 Value() override;
	void Value(int64 v) override;
	bool HasButton();
	void HasButton(bool b);
	LTabView *GetTabControl() { return TabCtrl; }

	GMessage::Result OnEvent(GMessage *Msg) override;
	void OnPaint(LSurface *pDC) override;
	bool OnKey(LKey &k) override;
	void OnFocus(bool b) override;
	void OnStyleChange();
	void SetFont(LFont *Font, bool OwnIt = false) override;

	void Append(LViewI *Wnd);
	bool Remove(LViewI *Wnd);
	bool LoadFromResource(int Resource);
	void Select();
};

