/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A check box control

#ifndef _GCHECK_BOX_H_
#define _GCHECK_BOX_H_

/// A check box to allow the user to select a boolean setting, i.e. a non-mutually exclusive option. For
/// mutually exclusive options see GRadioButton.
class LgiClass GCheckBox :
	#ifdef WINNATIVE
	public GControl,
	#else
	public GView,
	#endif
	public ResObject
{
	class GCheckBoxPrivate *d;

public:
	/// Constructor
	GCheckBox
	(
		/// The control ID
		int id,
		/// The left edge x coordinate
		int x,
		/// The top edge y coordinate
		int y,
		/// The width
		int cx,
		/// The height
		int cy,
		/// The text of the label
		const char *name,
		/// The initial state of the control
		int InitState = false
	);
	~GCheckBox();

	// Methods
	const char *GetClass() override { return "GCheckBox"; }
	
	/// Returns whether the control is 3 state
	bool ThreeState();
	/// Sets whether the control is 3 state.
	///
	/// In the case that the control is 3 state, the 3 states are:
	/// <ul>
	/// 	<li> Fully off: 0
	/// 	<li> Fully on: 1
	/// 	<li> Partially on/Undefined: 2
	/// </ul>
	void ThreeState(bool t);
	/// Returns the current value, 0 or 1. Or possibly 2 if ThreeState() is set.
	int64 Value() override;
	/// Sets the current value.
	void Value(int64 b) override;

	// Impl
	const char *Name() override { return GView::Name(); }
	const char16 *NameW() override { return GView::NameW(); }
	bool Name(const char *n) override;
	bool NameW(const char16 *n) override;
	void SetFont(LFont *Fnt, bool OwnIt = false) override;
    bool OnLayout(GViewLayoutInfo &Inf) override;
	int BoxSize();

	void OnMouseClick(LMouse &m) override;
	void OnMouseEnter(LMouse &m) override;
	void OnMouseExit(LMouse &m) override;
	bool OnKey(LKey &k) override;
	void OnFocus(bool f) override;
	void OnPosChange() override;
	void OnPaint(GSurface *pDC) override;
	void OnAttach() override;
	void OnStyleChange();
	GMessage::Result OnEvent(GMessage *Msg) override;
	int OnNotify(GViewI *Ctrl, int Flags) override;

	#ifdef WINNATIVE
	int SysOnNotify(int Msg, int Code);
	#endif	
};

#endif
