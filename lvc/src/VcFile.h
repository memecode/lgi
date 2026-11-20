#ifndef _VcFile_h_
#define _VcFile_h_

#include <memory>

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
	AppPriv *d = nullptr;
	VcFolder *Owner = nullptr;
	bool LoadDiff = false;
	std::shared_ptr<VcFile *> DiffTarget;
	LString Diff;
	LString Charset;
	LString Revision;
	LUri Uri;
	LListItemCheckBox *Chk = nullptr;
	FileStatus Status = SUnknown;
	bool Staged = false;
	bool GitIndexOnly = false;
	LString GitOriginalPath;

	void ShowDiff();

public:
	VcFile(AppPriv *priv, VcFolder *owner, LString revision, bool working = false);
	~VcFile();

	int Checked(int Set = -1);
	const char *GetRevision() { return Revision; }
	const char *GetDiff() { return Diff; }
	std::weak_ptr<VcFile *> GetDiffTarget()
	{
		if (!DiffTarget)
			DiffTarget = std::make_shared<VcFile *>(this);
		return DiffTarget;
	}
	const char *GetFileName() { return GetText(COL_FILENAME); }
	FileStatus GetStatus();
	bool GetStaged() { return Staged; }
	LString GetUri();
	LString GetCharset() { return Charset; }
	void SetUri(LString uri);
	void SetCharset(LString cs);
	void SetStatus(FileStatus s);
	
	void SetDiff(LString d);
	void SetStaged(bool staged) { Staged = staged; }
	void SetGitIndexOnly(bool indexOnly, LString originalPath)
	{
		GitIndexOnly = indexOnly;
		GitOriginalPath = originalPath;
	}
	bool GetGitIndexOnly() const { return GitIndexOnly; }
	const char *GetGitOriginalPath() const { return GitOriginalPath; }
	void Select(bool b);
	void OnMouseClick(LMouse &m);
};

#endif
