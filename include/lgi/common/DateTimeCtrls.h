/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef _DATE_TIME_CTRLS_
#define _DATE_TIME_CTRLS_

#include "MonthView.h"
#include "GNotifications.h"

/// This is a popup window to select a time.
class GTimePopup : public GPopup
{
	LView *Owner;
	class TimeList *Times;
	bool Ignore;

public:
	GTimePopup(LView *owner);
	~GTimePopup();

	const char *GetClass() { return "GTimePopup"; }

	void SetTime(LDateTime *t);
	GString GetTime();

	void OnCreate();
	void OnPaint(LSurface *pDC);
	int OnNotify(LViewI *c, int f);
};

/// This class is a little button to pull down the list of times...
/// You use it by creating a custom control in LgiRes and using the control
/// name "GTimePopup". Then at runtime bind it to an editbox by getting the
/// LView handle using LView::FindControl then give it the pointer to the
/// edit box using LView::SetNotify.
class GTimeDropDown :
	public GDropDown,
	public ResObject
{
	GTimePopup *Drop;

public:
	GTimeDropDown();

	/// This sets the date source control is the notify control is empty.
	void SetDate(char *d);
	void OnMouseClick(LMouse &m);
	bool OnLayout(LViewLayoutInfo &Inf);
	int OnNotify(LViewI *Ctrl, int Flags);
	void OnChildrenChanged(LViewI *Wnd, bool Attaching);
};

/// Popup window used to select a date.
class GDatePopup : public GPopup
{
	LView *Owner;
	LRect Caption;
	LRect Date;
	LRect Prev;
	LRect Next;

	int Cx, Cy; // Cell Dimensions
	MonthView Mv;
	bool FirstPaint;
	
public:
	GDatePopup(LView *owner);
	~GDatePopup();

	const char *GetClass() { return "GDatePopup"; }

	LDateTime Get();
	void OnChange();
	void Set(LDateTime &Ts);
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void Move(int Dx, int Dy);
	bool OnKey(LKey &k);
};

/// This class is a little button to pull down a date selection control...
/// You use it by creating a custom control in LgiRes and using the control
/// name "GDatePopup". Then at runtime bind it to an editbox by getting the
/// LView handle using LView::FindControl then give it the pointer to the
/// edit box using LView::SetNotify.
class GDateDropDown :
	public GDropDown,
	public ResObject
{
	GDatePopup *Drop;

public:
	GDateDropDown();

	/// This function sets the date src edit box, the date source is used
	/// to select an appropriate starting point if the Notify control is
	/// empty.
	void SetDate(char *d);
	void OnMouseClick(LMouse &m);
	bool OnLayout(LViewLayoutInfo &Inf);
	int OnNotify(LViewI *Ctrl, int Flags);
	void OnChildrenChanged(LViewI *Wnd, bool Attaching);
};

#endif
