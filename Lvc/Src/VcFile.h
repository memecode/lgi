#ifndef _VcFile_h_
#define _VcFile_h_

#include "lgi/common/ListItemCheckBox.h"

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
	AppPriv *d;
	VcFolder *Owner;
	bool LoadDiff;
	LString Diff;
	LString Revision;
	LUri Uri;
	LListItemCheckBox *Chk;
	FileStatus Status;

public:
	VcFile(AppPriv *priv, VcFolder *owner, LString revision, bool working = false);
	~VcFile();

	int Checked(int Set = -1);
	const char *GetRevision() { return Revision; }
	const char *GetDiff() { return Diff; }
	const char *GetFileName() { return GetText(COL_FILENAME); }
	FileStatus GetStatus();
	LString GetUri();
	void SetUri(LString uri);
	void SetStatus(FileStatus s) { Status = s; Update(); }
	
	void SetDiff(LString d);
	void Select(bool b);
	void OnMouseClick(LMouse &m);
};

#endif