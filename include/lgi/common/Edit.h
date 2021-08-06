/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief An edit box control

#ifndef _GEDIT_H_
#define _GEDIT_H_

#if !WINNATIVE
#include "lgi/common/TextView3.h"
#endif

/// An edit box allowing the user to enter text
class LgiClass LEdit :
	#if WINNATIVE
	public LControl,
	public ResObject
	#else
	public LTextView3
	#endif
{
protected:
	class LEditPrivate *d;

	#if WINNATIVE
	void OnCreate();

	LAutoWString	SysName();
	bool			SysName(const char16 *Name);
	int				SysOnNotify(int Msg, int Code);
	bool			SysEmptyText();
	#endif

public:
	/// Constructor
	LEdit
	(
		/// Ctrl's ID
		int id,
		/// x coord
		int x,
		/// y coord
		int y,
		/// width
		int cx,
		/// height
		int cy,
		/// Initial text
		const char *name = NULL
	);
	~LEdit();

	const char *GetClass() { return "LEdit"; }


	/// Gets "Allow multiple lines"
	bool MultiLine();
	/// Sets "Allow multiple lines"
	void MultiLine(bool m);
	/// Is the text obsured by hashes?
	bool Password();
	/// Sets the text to be obsured by hashes.
	void Password(bool m);
	/// Interprets the text in the control as an integer
	void Value(int64 i);
	/// Sets the text in the control to an integer
	int64 Value();
	/// Selects a region of text
	void Select(int Start = 0, int Len = -1);
	/// Get the current selection in characters
	LRange GetSelectionRange();
	/// Gets the Caret position in characters
	ssize_t GetCaret(bool Cursor = true);
	/// Sets the Caret position in characters
	void SetCaret(size_t Pos, bool Select = false, bool ForceFullUpdate = false);
	/// Sets the text to display when the control is empty
	void SetEmptyText(const char *EmptyText);

	void SelectAll() { Select(); }

	bool OnKey(LKey &k);
	void KeyProcessed();
	LCursor GetCursor(int x, int y) { return LCUR_Ibeam; }

	#if WINNATIVE
	LMessage::Result OnEvent(LMessage *Msg);
	void OnFocus(bool f);
	const char *Name() override;
	bool Name(const char *s) override;
	const char16 *NameW() override;
	bool NameW(const char16 *s) override;
	#else
	bool Paste();
	void OnEnter(LKey &k);
	void SendNotify(LNotification note);
	bool OnLayout(LViewLayoutInfo &Inf) { return false; }
    void OnPaint(LSurface *pDC);
	bool SetScrollBars(bool x, bool y);
	#endif
};

#endif
