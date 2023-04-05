/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief Extension to the LWindow class to create a document processing application.
#ifndef __GDOCAPP_H
#define __GDOCAPP_H

#include <stdio.h>

#include "Lgi.h"
#include "lgi/common/Mru.h"

////////////////////////////////////////////////////////////////////////////////////////////
// Defines

// window messages
#define		IDM_SAVE			15002
#define		IDM_CLOSE			15003
#define		IDM_EXIT			15004

// Misc
// #define		MainWnd				((AppWnd*)LApp::ObjInstance()->AppWnd)

/// A base for a document processing application.
/// The 2 types you can use for OptionsFmt are (ideally) LOptionsFile or (deprecated) ObjProperties.
enum GDocAppInstallMode
{
	InstallDesktop,
	InstallPortable,
};

template<typename OptionsFmt>
class LDocApp :
	public LWindow,
	public LMru
{
public:
	#ifdef _WIN32
	typedef int LIcon;
	#else
	typedef const char *LIcon;
	#endif

private:
	class LDocAppPrivate *d;
	OptionsFmt		*Options;

	void			_Close();

protected:
	/// Loads the given menu.
	bool			_LoadMenu(const char *Resource = 0, const char *Tags = 0, int FileMenuId = -1, int RecentMenuId = -1);
	/// Call this to create the window, usually in the constructor of your main window class.
	bool			_Create(LIcon IconResource = 0);
	/// Call this to destroy the window, usually in the destructor of your main window class.
	bool			_Destroy();
	/// Use the open file dialog to select a document to open.
	void			_OpenFile(const char *File, bool ReadOnly, std::function<void(bool)> Callback) override;
	/// Save the current document.
	void			_SaveFile(const char *File, std::function<void(LString, bool)> Callback) override;

	bool			_DoSerialize(bool Write);
	bool			_SerializeFile(bool Write);

	// Data
	LSubMenu		*_FileMenu;

	/// Set this to the name of the language setting in the option file.
	char			*_LangOptsName;

public:
	/// Creates the class.
	LDocApp
	(
		/// The name of the application
		const char *appname = 0,
		/// [Optional] The icon resource for the window.
		LIcon icon = 0,
		/// [Optional] Options file base name..
		char *optsname = 0
	);
	~LDocApp();

	/// Gets the install mode
	GDocAppInstallMode GetInstallMode();
	/// Sets the current file name.
	void SetCurFile(const char *f);
	/// Gets the current file name.
	const char *GetCurFile();
	/// Sets the dirty state. If the doc is clean you are not asked to save it when you close the window.
	void SetDirty(bool Dirty, std::function<void(bool)> Callback);
	/// Gets the dirty state.
	bool GetDirty();
	/// Gets the options list.
	OptionsFmt *GetOptions();
	/// Gets the application name.
	char *GetAppName();
	/// Gets the options file name
	char *GetOptionsFileName();

	/// Implement this to clear your document from memory, i.e. "Close".
	virtual bool Empty() { return false; }
	/// Implement this to read/write your application specific options.
	virtual bool SerializeOptions(OptionsFmt *Options, bool Write) { return false; }
	/// This is called when the dirty state changes.
	virtual void OnDirty(bool NewValue) {}

	/// Set the language for the application, which causes a restart. The lang id is saved
	/// to the options file, the user is asked whether they want to restart, if yes, then
	/// the application closes, and executes itself.
	///
	/// \sa Requires _LangOptsName to be set to the option name of the language setting.
	/// \sa Uses the resource string L_DOCAPP_RESTART_APP to warn the user of the restart (there is an english default).
	bool SetLanguage(char *LangId);

	// Impl
	void OnReceiveFiles(LArray<const char*> &Files) override;
	bool OnRequestClose(bool OsShuttingDown) override;
	int OnCommand(int Cmd, int Event, OsView Window) override;
	LMessage::Result OnEvent(LMessage *m) override;
};

#endif
