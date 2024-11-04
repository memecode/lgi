/// \file
/// \author Matthew Allen
/// \brief A text label

#ifndef _GTEXT_LABEL_H_
#define _GTEXT_LABEL_H_

/// A static text label widget
class LgiClass LTextLabel :
	public LView,
	public ResObject,
	public LDom
{
	class LTextPrivate *d;

public:
	/// Constructor
	LTextLabel
	(
		/// The control's ID
		int id,
		/// Left edge position
		int x,
		/// Top edge position
		int y,
		/// The width
		int cx,
		/// The height
		int cy,
		/// Utf-8 text for the label
		const char *name
	);
	~LTextLabel();
	
	const char *GetClass() override { return "LTextLabel"; }

	/// Set the text
	bool Name(const char *n) override;
	/// Set the text with a wide string
	bool NameW(const char16 *n) override;
	/// Sets the font used to render the text
	void SetFont(LFont *Fnt, bool OwnIt = false) override;

	/// Returns the numeric value of the text (atoi)
	int64 Value() override;
	/// Sets the text to a number
	void Value(int64 i) override;
	/// Gets the text
	const char *Name() override { return LView::Name(); }
	/// Gets the text as a wide string
	const char16 *NameW() override { return LView::NameW(); }
	/// Word wrap
	bool GetWrap();
	/// Sets the use of word wrap
	void SetWrap(bool b);
	
	// Events
	LMessage::Result OnEvent(LMessage *Msg) override;
	int OnNotify(LViewI *Ctrl, LNotification &n) override;
	void OnPaint(LSurface *pDC) override;
	void OnPosChange() override;
	void OnAttach() override;
	bool OnLayout(LViewLayoutInfo &Inf) override;
	void OnStyleChange();
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	void OnCreate() override;
};

#endif
