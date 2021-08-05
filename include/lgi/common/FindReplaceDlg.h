/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef __GFINDREPLACEDLG_H
#define __GFINDREPLACEDLG_H

/// Common find/replace window parameters
class LgiClass GFindReplaceCommon : public LDialog
{
	// bool OnViewKey(LView *v, LKey &k);

public:
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
	
	GFindReplaceCommon();
};

typedef bool (*GFrCallback)(GFindReplaceCommon *Dlg, bool Replace, void *User);

/// Find dialog
class LgiClass GFindDlg : public GFindReplaceCommon
{
	class GFindDlgPrivate *d;

public:
	/// Constructor
	GFindDlg
	(
		/// The parent window
		LView *Parent,
		/// The initial string for the find argument
		char *Init = 0,
		/// Callback
		GFrCallback Callback = 0,
		/// User defined data for callback
		void *UserData = 0
	);
	~GFindDlg();

	void OnCreate();
	void OnPosChange();
	int OnNotify(LViewI *Ctrl, LNotification &n);
};

/// The find command on the Replace dialog
#define IDC_FR_FIND					21002
/// The replace command on the Replace dialog
#define IDC_FR_REPLACE				21007

/// Replace dialog
class LgiClass GReplaceDlg : public GFindReplaceCommon
{
	class GReplaceDlgPrivate *d;

public:
	LString Replace;

	/// Constructor
	GReplaceDlg
	(
		/// The parent window
		LView *Parent,
		/// The initial value to find
		char *InitFind = 0,
		/// The initial value to replace with
		char *InitReplace = 0,
		/// Callback
		GFrCallback Callback = 0,
		/// User defined data for callback
		void *UserData = 0
	);
	~GReplaceDlg();

	void OnCreate();
	void OnPosChange();

	/// \returns DoModal will return one of #IDC_FR_FIND,
	/// #IDC_FR_REPLACE, #IDCANCEL or #IDOK (which means 'Replace All', the default action)
	int OnNotify(LViewI *Ctrl, LNotification &n);
};


#endif

