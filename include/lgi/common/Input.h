#pragma once

#include "lgi/common/Dialog.h"
#include "lgi/common/Edit.h"

/// This callback if supplied to the LInput will create a "..." button, which if clicked
/// will call the callback.
typedef void (*GInputCallback)(class LInput *Dlg, LViewI *EditCtrl, void *Param);

/// This class displays a window with a message and an edit box to enter a string.
/// Once constructed, use the LDialog::DoModal() call to run the input window, it
/// returns TRUE if 'Ok' is clicked.
class LgiClass LInput : public LDialog
{
	LEdit *Edit;
	GInputCallback Callback;
	void *CallbackParam;
	LString Str;

public:
	/// Constructs the dialog.
	LInput
	(
		/// The parent view
		LViewI *parent,
		/// The initial value in the edit box
		const char *InitStr = "",
		/// The message to display in the text box
		const char *Msg = "Enter String",
		/// The title of the window
		const char *Title = "Input",
		/// True if you want the edit box characters hashed out for a password
		bool Password = false,
		/// [Optional] If this parameter is supplied then a "..." button is added to the dialog.
		/// If clicked the callback is called with the dialog and editbox control ptrs.
		GInputCallback callback = 0,
		/// [Optional] Callback user parameter
		void *CallbackParam = 0
	);
	
	int OnNotify(LViewI *Ctrl, LNotification n);
	LString GetStr() { return Str; }
};

