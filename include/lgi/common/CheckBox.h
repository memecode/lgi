/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A check box control

#ifndef _GCHECK_BOX_H_
#define _GCHECK_BOX_H_

/// A check box to allow the user to select a boolean setting, i.e. a non-mutually exclusive option. For
/// mutually exclusive options see LRadioButton.
class LgiClass LCheckBox :
	#ifdef WINNATIVE
	public LControl,
	#else
	public LView,
	#endif
	public LClickable,
	public ResObject
{
	class LCheckBoxPrivate *d;

public:
	enum State
	{
		CheckOff,
		CheckOn,
		CheckPartial,
	};

	/// Constructor
	LCheckBox
	(
		/// The control ID
		int id = -1,
		/// The text of the label
		const char *name = NULL,
		/// The initial state of the control
		int InitState = false
	);
	~LCheckBox();

	// Methods
	const char *GetClass() override { return "LCheckBox"; }
	
	/// Returns whether the control is 3 state
	bool ThreeState();
	/// Sets whether the control is 3 state.
	///
	/// In the case that the control is 3 state use the 'State' enum.
	void ThreeState(bool t);
	/// Returns the current value, 0 or 1. Or possibly 2 if ThreeState() is set (see also the 'State' enum)
	int64 Value() override;
	/// Sets the current value.
	void Value(int64 b) override;

	// Events
	void OnMouseClick(LMouse &m) override;
	void OnMouseEnter(LMouse &m) override;
	void OnMouseExit(LMouse &m) override;
	bool OnKey(LKey &k) override;
	void OnFocus(bool f) override;
	void OnPosChange() override;
	void OnPaint(LSurface *pDC) override;
	void OnAttach() override;
	void OnStyleChange();
	LMessage::Result OnEvent(LMessage *Msg) override;
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
	void OnClick(const LMouse &m) override;

	// Impl
	const char *Name() override { return LView::Name(); }
	const char16 *NameW() override { return LView::NameW(); }
	bool Name(const char *n) override;
	bool NameW(const char16 *n) override;
	void SetFont(LFont *Fnt, bool OwnIt = false) override;
    bool OnLayout(LViewLayoutInfo &Inf) override;
	int BoxSize();

	#if defined(WINNATIVE)
		int SysOnNotify(int Msg, int Code);
	#elif defined(HAIKU)
		void OnCreate();
	#endif	
};

#endif
