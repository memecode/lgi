/// \file
/// \author Matthew Allen
/// \brief A radio group view

#ifndef _GRADIO_GROUP_H_
#define _GRADIO_GROUP_H_

/// A grouping control. All radio buttons that are children of this control will automatically 
/// have only one option selected. Other controls can be children as well but are ignored in the
/// calculation of the groups value. The value of the group is the index into a list of radio buttons
/// of the radio button that is on.
class LgiClass GRadioGroup :
	#ifdef WINNATIVE
	public GControl,
	#else
	public GView,
	#endif
	public ResObject
{
	class GRadioGroupPrivate *d;
	void OnCreate() override;

public:
	GRadioGroup(int id, int x, int y, int cx, int cy, const char *name, int Init = 0);
	~GRadioGroup();
	
	const char *GetClass() override { return "GRadioGroup"; }

	/// Returns the index of the set radio button
	int64 Value() override;
	/// Sets the 'ith' radio button to on.
	void Value(int64 i) override;
	/// Adds a radio button to the group.
	GRadioButton *Append(int x, int y, const char *name);

	// Impl
	int OnNotify(GViewI *Ctrl, int Flags) override;
	void OnPaint(GSurface *pDC) override;
	void OnAttach() override;
	GMessage::Result OnEvent(GMessage *m) override;
	bool OnLayout(GViewLayoutInfo &Inf) override;
	void OnStyleChange();

	const char *Name() override { return GView::Name(); }
	const char16 *NameW() override { return GView::NameW(); }
	bool Name(const char *n) override;
	bool NameW(const char16 *n) override;
	void SetFont(LFont *Fnt, bool OwnIt = false) override;
};

/// A radio button control. A radio button is used to select between mutually exclusive options. i.e.
/// only one can be valid at any given time. For non-mutually exclusive options see the GCheckBox control.
class LgiClass GRadioButton :
	#if WINNATIVE && !XP_BUTTON
	public GControl,
	#else
	public GView,
	#endif
	public ResObject
{
	friend class GRadioGroup;
	class GRadioButtonPrivate *d;

public:
	GRadioButton(int id, int x, int y, int cx, int cy, const char *name);
	~GRadioButton();

	const char *GetClass() override { return "GRadioButton"; }

	// Impl
	const char *Name() override { return GView::Name(); }
	const char16 *NameW() override { return GView::NameW(); }
	bool Name(const char *n) override;
	bool NameW(const char16 *n) override;
	int64 Value() override;
	void Value(int64 i) override;
	bool OnLayout(GViewLayoutInfo &Inf) override;
	int OnNotify(GViewI *Ctrl, int Flags) override;

	// Events
	void OnAttach() override;
	void OnStyleChange();
	bool OnKey(LKey &k) override;
	
	#if WINNATIVE && !XP_BUTTON
	int SysOnNotify(int Msg, int Code);
	#else
	void OnMouseClick(LMouse &m) override;
	void OnMouseEnter(LMouse &m) override;
	void OnMouseExit(LMouse &m) override;
	void OnFocus(bool f) override;
	void OnPaint(GSurface *pDC) override;
	void SetFont(LFont *Fnt, bool OwnIt = false) override;
	#endif
};

#endif
