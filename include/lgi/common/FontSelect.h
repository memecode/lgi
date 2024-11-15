/// \file
/// \author Matthew Allen
#ifndef __GFONTSELECT_H
#define __GFONTSELECT_H

/// \brief A font selection dialog.
///
/// Example:
/// \code
/// #if defined WIN32
/// LOGFONT Info;
/// #else
/// char Info[256];
/// #endif
/// ZeroObj(Info);
/// 
/// auto select = new LFontSelect(MyWindow, Info);
/// select->DoModal([this, select](auto s, auto code)
/// {
///		if (code == IDOK)
/// 		select->Serialize(Info, true);
/// }
/// \endcode
class LFontSelect : public LDialog
{
	struct LFontSelectPriv *d;
	
	const char *GetSelectedFace();
	void InsertFont(const char *Face);
	void EnumerateFonts();
	void OnCreate() override;
	void UpdatePreview();
	void UiToThis();

public:
	/// The face of the font selected
	LString Face;
	/// The point size of the font selected
	int Size;
	/// True if the font should be bold
	bool Bold;
	/// True if the font should be underline
	bool Underline;
	/// True if the font should be italic
	bool Italic;

	/// Create the dialog
	LFontSelect
	(
		/// The parent window
		LView *Parent,
		/// The initial font information, or NULL if not available
		void *Init = NULL,
		/// Buffer length of Init
		int InitLen = 0
	);
	~LFontSelect();

	int OnNotify(LViewI *Ctrl, const LNotification &n) override;

	/// Read/Write the font information to a OS specific structure
	///
	/// Currently on Windows that is a LOGFONT structure and on all
	/// other platforms it's a char buffer of at least 256 chars.
	bool Serialize(void *Data, int DataLen, bool Write);
};

#endif
