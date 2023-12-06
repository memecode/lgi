#ifndef _VcFile_h_
#define _VcFile_h_

#include "lgi/common/ListItemCheckBox.h"
#include "lgi/common/Uri.h"

class VcFile : public LListItem
{
public:
	enum FileStatus
	{
		SUnknown,
		SUntracked,			// Not in version control
		SClean,	
		SModified,
		SReplaced,
		SDeleted,
		SAdded,
		SMissing,
		SConflicted,
	};

private:
	AppPriv *d = NULL;
	VcFolder *Owner = NULL;
	bool LoadDiff;
	LString Diff;
	LString Revision;
	LUri Uri;
	LListItemCheckBox *Chk = NULL;
	FileStatus Status = SUnknown;
	bool Staged = false;

public:
	VcFile(AppPriv *priv, VcFolder *owner, LString revision, bool working = false);
	~VcFile();

	int Checked(int Set = -1);
	const char *GetRevision() { return Revision; }
	const char *GetDiff() { return Diff; }
	const char *GetFileName() { return GetText(COL_FILENAME); }
	FileStatus GetStatus();
	bool GetStaged() { return Staged; }
	LString GetUri();
	void SetUri(LString uri);
	void SetStatus(FileStatus s) { Status = s; Update(); }
	
	void SetDiff(LString d);
	void SetStaged(bool staged) { Staged = staged; }
	void Select(bool b);
	void OnMouseClick(LMouse &m);
};

#endif
