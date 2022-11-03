/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief File association query and edit.

#ifndef _FILE_ASSOC_H_
#define _FILE_ASSOC_H_

/// A single file association action
class LFileAssocAction
{
public:
	char *App;
	char *Action;

	LFileAssocAction();
	~LFileAssocAction();
};

/// A file type, with a list of associated actions
class LFileAssoc
{
	class LFileAssocPrivate *d;

public:
	LFileAssoc(char *MimeType, char *Extension);
	~LFileAssoc();

	/// Returns the mimetype
	char *GetMimeType();

	/// Returns the extension
	char *GetExtension();

	/// Gets the extensions that map to this assocaition.
	bool GetExtensions(LArray<char*> &Ext);

	/// Gets the action for this association.
	bool GetActions(LArray<LFileAssocAction*> &Actions);

	/// Sets an action.
	bool SetAction(LFileAssocAction *Action);

	/// Sets the icon assocaited with the file type
	bool SetIcon(char *File, int Index);
};

#endif