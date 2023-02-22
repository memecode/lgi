/// \file
/// \author Matthew Allen (fret@memecode.com)
#pragma once

#include <functional>

/// Common find/replace window parameters
class LgiClass LFindReplaceCommon : public LDialog
{
	// bool OnViewKey(LView *v, LKey &k);

public:
	typedef std::function<void(LFindReplaceCommon *Dlg, int CtrlCode)> Callback;

	/// The string to find
	LString Find;
	/// Whether to match a whole word
	bool MatchWord;
	/// Whether to match the case
	bool MatchCase;
	/// Whether to search only in the selection
	bool SelectionOnly;
	/// Whether to search only in the selection
	bool SearchUpwards;
	
	LFindReplaceCommon();
};

/// Find dialog
class LgiClass LFindDlg : public LFindReplaceCommon
{
	class LFindDlgPrivate *d;

public:
	/// Constructor
	LFindDlg
	(
		/// The parent window
		LView *Parent,
		/// Callback
		Callback Cb,
		/// The initial string for the find argument
		const char *Init = NULL
	);
	~LFindDlg();

	void OnCreate();
	void OnPosChange();
	int OnNotify(LViewI *Ctrl, LNotification n);
};

/// The find command on the Replace dialog
#define IDC_FR_FIND					21002
/// The replace command on the Replace dialog
#define IDC_FR_REPLACE				21007

/// Replace dialog
class LgiClass LReplaceDlg : public LFindReplaceCommon
{
	class LReplaceDlgPrivate *d;

public:
	LString Replace;

	/// Constructor
	LReplaceDlg
	(
		/// The parent window
		LView *Parent,
		/// Callback
		Callback Cb,
		/// The initial value to find
		char *InitFind = NULL,
		/// The initial value to replace with
		char *InitReplace = NULL
	);
	~LReplaceDlg();

	void OnCreate();
	void OnPosChange();

	/// \returns DoModal will return one of #IDC_FR_FIND,
	/// #IDC_FR_REPLACE, #IDCANCEL or #IDOK (which means 'Replace All', the default action)
	int OnNotify(LViewI *Ctrl, LNotification n);
};


