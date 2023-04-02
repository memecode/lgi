#ifndef __GMRU_H
#define __GMRU_H

#include <functional>

#ifdef HAS_PROPERTIES
#include "GProperties.h"
#endif
#include "lgi/common/FileSelect.h"

// Message defines
#define	IDM_OPEN			15000
#define	IDM_SAVEAS			15001

// Classes
class LgiClass LMru
{
private:
	class LMruPrivate *d;
	void _Update();

protected:
	virtual void _OpenFile(const char *File, bool ReadOnly, std::function<void(bool)> Callback);
	virtual void _SaveFile(const char *File, std::function<void(LString, bool)> Callback);

	virtual const char *_GetCurFile();
	virtual void GetFileTypes(LFileSelect *Dlg, bool Write);
	virtual LFileType *GetSelectedType();
	void DoFileDlg(LAutoPtr<LFileSelect> Select, bool Open, std::function<void(bool)> OnSelect);

	/// This method converts the storage reference (which can contain user/pass credentials) 
	/// between the display, raw and stored forms. Display and Raw are used at runtime and
	/// the stored form is written to disk.
	///
	/// The implementor should fill in any NULL strings by converting from the supplied forms.
	/// The caller must supply either the Raw or Stored form.
	virtual bool SerializeEntry
	(
		/// The displayable version of the reference (this should have any passwords blanked out)
		LString *Display,
		/// The form passed to the client software to open/save. (passwords NOT blanked)
		LString *Raw,
		/// The form safe to write to disk, if a password is present it must be encrypted.
		LString *Stored
	);


public:
	LMru();
	virtual ~LMru();

	// Impl
	bool Set(LSubMenu *parent, int size = -1);
	const char *AddFile(const char *FileName, bool Update = true);
	void RemoveFile(const char *FileName, bool Update = true);
	LMessage::Result OnEvent(LMessage *Msg);
	void OnCommand(int Cmd, std::function<void(bool)> OnStatus);

	// Serialization
	bool Serialize(LDom *Store, const char *Prefix, bool Write);
	#ifdef HAS_PROPERTIES
	bool Serialize(ObjProperties *Store, char *Prefix, bool Write);
	#endif

	// Events
	virtual bool OpenFile(const char *FileName, bool ReadOnly) = 0;
	virtual void SaveFile(const char *FileName, std::function<void(LString fileName, bool status)> Callback) = 0;
};

#endif
