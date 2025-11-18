#pragma once

#include <functional>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef FILE_SELECT_CLS
#define FILE_SELECT_CLS LgiClass
#endif
#ifndef FILE_SELECT_FN
#define FILE_SELECT_FN LgiFunc
#endif

FILE_SELECT_FN bool LGetUsersLinks(LString::Array &Links);

///////////////////////////////////////////////////////////////////////////////////////////////////
// File select dialog
class FILE_SELECT_CLS LFileType : public LBase
{
	LString ext;
	int data = 0;

public:
	const char *Extension() { return ext; }
	bool Extension(const char *e) { ext = e; return ext != NULL; }
	const char *Description() { return Name(); }
	bool Description(const char *d) { return Name(d); }
	int Data() { return data; }
	void Data(int i) { data = i; }

	LString DefaultExtension();
};

/// Optional asynchronous file system interface class:
class IFileSelectSystem
{
public:
	virtual ~IFileSelectSystem() {}

	virtual void SetUi(LViewI *view) = 0;
	virtual char GetDirChar() = 0;
	virtual LString PathJoin(LString base, LString leaf) = 0;
	virtual void Stat(LString path, std::function<void(struct stat*, LString, LError)> cb) = 0;
	virtual void GetInitialPath(std::function<void(LString)> cb) = 0;
	virtual void GetRootVolume(std::function<void(LVolume*)> cb) = 0;
	virtual void ReadDir(LString path, std::function<void(LDirectory&)> cb) = 0;
	virtual void CreateFolder(LString path, bool createParents, std::function<void(bool)> cb) = 0;
	virtual void DeleteFolder(LString path, std::function<void(bool)> cb) = 0;
	virtual void DeleteFile(LString path, std::function<void(bool)> cb) = 0;
	virtual void Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) = 0;
};

/// \brief File selector dialog
///
/// Handles the UI for selecting files for opening and saving and selecting directories.
/// Uses the Win32 system dialogs on Windows and has it's own native window on Linux.
/// 
/// A simple example of this class in action:
/// \code
/// auto s = new LFileSelect;
/// s->Parent(MyWindow);
/// s->Type("PNG Files", "*.png");
/// s.Open([](auto s, auto ok)
/// {
///		if (ok)
/// 		LgiMsg(MyWindow, "The file selected is '%s'", "Example", MB_OK, s.Name());
///		delete s;
/// });
/// \endcode
class FILE_SELECT_CLS LFileSelect :
	public LBase
{
	class LFileSelectPrivate *d;

public:
	typedef std::function<void(LAutoPtr<LFileSelect>, bool)> SelectCb;

	LFileSelect(LViewI *Window = nullptr, IFileSelectSystem *System = nullptr);
	~LFileSelect();

	// Properties
	
	/// Returns the first file name selected.
	const char *Name() override;
	/// Sets the file name
	bool Name(const char *n) override;
	/// Returns the n'th file name selected
	const char *operator [](size_t i);
	/// Returns the number of file names selected
	size_t Length();
	/// Returns the parent window
	LViewI *Parent();
	/// Sets the parent window
	void Parent(LViewI *Window);
	/// Returns whether the use can select multiple files
	bool MultiSelect();
	/// Sets whether the use can select multiple files
	void MultiSelect(bool Multi);
	/// Returns whether read only was selected
	bool ReadOnly();
	/// Sets whether the user sees a read only option
	void ShowReadOnly(bool ro);
	/// Gets the initial directory to open in
	const char *InitialDir();
	/// Sets the initial directory to open in
	void InitialDir(const char *InitDir);
	/// Gets the title of the dialog box
	const char *Title();
	/// Sets the title of the dialog box
	void Title(const char *Title);
	/// Gets the default extension to append to files selected without an extension
	const char *DefaultExtension();
	/// Sets the default extension to append to files selected without an extension
	void DefaultExtension(const char *DefExt);

	// File types
	
	/// Returns the number of types in the type list
	size_t Types();
	/// Returns the 0 based index of the type selected in the type list
	ssize_t SelectedType();
	/// Returns the type into at a given index
	LFileType *TypeAt(ssize_t n);
	/// Adds a file type to the type filters list.
	bool Type
	(
		/// This full description of the file type
		const char *Description,
		/// The extension(s) of the file type: e.g: '*.png;*.gif;*.jpg'
		const char *Extension,
		/// Application defined 32-bit value.
		int Data = 0
	);
	/// Empties the type list
	void ClearTypes();

	// Methods
	
	/// Shows the open file dialog
	void Open(SelectCb Cb);
	/// Shows the save file dialog
	void Save(SelectCb Cb);
	/// Shows the open folder dialog
	void OpenFolder(SelectCb Cb);
};

