/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef _DATE_TIME_CTRLS_
#define _DATE_TIME_CTRLS_

#include "MonthView.h"
#include "lgi/common/Notifications.h"

/// This is a popup window to select a time.
class LTimePopup : public LPopup
{
	LView *Owner;
	class TimeList *Times;
	bool Ignore;

public:
	LTimePopup(LView *owner);
	~LTimePopup();

	const char *GetClass() { return "LTimePopup"; }

	void SetTime(LDateTime *t);
	LString GetTime();

	void OnCreate();
	void OnPaint(LSurface *pDC);
	int OnNotify(LViewI *c, LNotification &n) override;
};

/// This class is a little button to pull down the list of times...
/// You use it by creating a custom control in LgiRes and using the control
/// name "LTimePopup". Then at runtime bind it to an editbox by getting the
/// LView handle using LView::FindControl then give it the pointer to the
/// edit box using LView::SetNotify.
class LTimeDropDown :
	public LDropDown,
	public ResObject
{
	LTimePopup *Drop;

public:
	LTimeDropDown();

	/// This sets the date source control is the notify control is empty.
	void SetDate(char *d);
	void OnMouseClick(LMouse &m);
	bool OnLayout(LViewLayoutInfo &Inf);
	int OnNotify(LViewI *Ctrl, LNotification &n) override;
	void OnChildrenChanged(LViewI *Wnd, bool Attaching);
};

/// Popup window used to select a date.
class LDatePopup : public LPopup
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
	LDatePopup(LView *owner);
	~LDatePopup();

	const char *GetClass() { return "LDatePopup"; }

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
/// name "LDatePopup". Then at runtime bind it to an editbox by getting the
/// LView handle using LView::FindControl then give it the pointer to the
/// edit box using LView::SetNotify.
class LDateDropDown :
	public LDropDown,
	public ResObject
{
	LDatePopup *Drop;

public:
	LDateDropDown();

	/// This function sets the date src edit box, the date source is used
	/// to select an appropriate starting point if the Notify control is
	/// empty.
	void SetDate(char *d);
	void OnMouseClick(LMouse &m);
	bool OnLayout(LViewLayoutInfo &Inf);
	int OnNotify(LViewI *Ctrl, LNotification &n) override;
	void OnChildrenChanged(LViewI *Wnd, bool Attaching);
};

#endif
