/// \file
/// \author Matthew Allen
/// \brief A radio group view

#ifndef _GRADIO_GROUP_H_
#define _GRADIO_GROUP_H_

class LRadioButton;

/// A grouping control. All radio buttons that are children of this control will automatically 
/// have only one option selected. Other controls can be children as well but are ignored in the
/// calculation of the groups value. The value of the group is the index into a list of radio buttons
/// of the radio button that is on.
class LgiClass LRadioGroup :
	#ifdef WINNATIVE
	public LControl,
	#else
	public LView,
	#endif
	public ResObject
{
	class LRadioGroupPrivate *d;
	void OnCreate() override;

public:
	LRadioGroup(int id = -1, const char *name = NULL, int64_t Init = -1);
	~LRadioGroup();
	
	const char *GetClass() override { return "LRadioGroup"; }

	/// Returns the index of the set radio button
	int64 Value() override;
	/// Sets the 'ith' radio button to on.
	void Value(int64 i) override;
	/// Adds a radio button to the group.
	LRadioButton *Append(const char *name);
	/// Gets or sets the selected radio button
	LRadioButton *Selected(const char *newSelection = nullptr);

	// Impl
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
	void OnPaint(LSurface *pDC) override;
	void OnAttach() override;
	LMessage::Result OnEvent(LMessage *m) override;
	bool OnLayout(LViewLayoutInfo &Inf) override;
	void OnStyleChange();

	const char *Name() override { return LView::Name(); }
	const char16 *NameW() override { return LView::NameW(); }
	bool Name(const char *n) override;
	bool NameW(const char16 *n) override;
	void SetFont(LFont *Fnt, bool OwnIt = false) override;
};

/// A radio button control. A radio button is used to select between mutually exclusive options. i.e.
/// only one can be valid at any given time. For non-mutually exclusive options see the LCheckBox control.
class LgiClass LRadioButton :
	#if WINNATIVE && !XP_BUTTON
	public LControl,
	#else
	public LView,
	#endif
	public LClickable,
	public ResObject
{
	friend class LRadioGroup;
	class LRadioButtonPrivate *d;

public:
	LRadioButton(int id, const char *name);
	~LRadioButton();

	const char *GetClass() override { return "LRadioButton"; }

	// If the heirarchy of ctrls doesn't allow the radio buttons to find each other automatically,
	// this will set the group up so that the only one is "on" at any given time.
	// \return true if the group is setup correctly (Ie all ctrl IDs are present)
	bool SetGroup(LArray<int> CtrlIds);

	// Impl
	const char *Name() override { return LView::Name(); }
	const char16 *NameW() override { return LView::NameW(); }
	bool Name(const char *n) override;
	bool NameW(const char16 *n) override;
	int64 Value() override;
	void Value(int64 i) override;
	bool OnLayout(LViewLayoutInfo &Inf) override;
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;

	// Events
	void OnAttach() override;
	void OnStyleChange();
	bool OnKey(LKey &k) override;

	// Click handling:
	void OnClick(const LMouse &m) override;
	
	// Platform impl
	#if WINNATIVE && !XP_BUTTON
		int SysOnNotify(int Msg, int Code);
		LMessage::Result OnEvent(LMessage *m) override;
	#else
		void OnMouseClick(LMouse &m) override;
		void OnMouseEnter(LMouse &m) override;
		void OnMouseExit(LMouse &m) override;
		void OnFocus(bool f) override;
		void OnPaint(LSurface *pDC) override;
		void SetFont(LFont *Fnt, bool OwnIt = false) override;
	#endif
};

#endif
