#ifndef _VcCommit_h_
#define _VcCommit_h_

class VcFolder;
class VcCommit;

enum CommitField
{
	LNone,
	LGraph,
	LRevision,
	LIndex,
	LBranch,
	LAuthor,
	LTime,
	LMessageTxt,
	LParents,
};

struct VcEdge
{
	VcCommit *Parent, *Child;
	int Idx;

	VcEdge(VcCommit *p, VcCommit *c)
	{
		Set(p, c);
		Idx = -1;
	}

	~VcEdge();
	void Set(VcCommit *p, VcCommit *c);
	void Detach(VcCommit *c);
};

class VcCommit : public LListItem
{
	LString sIndex, sNames, sTimeStamp, sMsg;
	bool Current;

protected:
	AppPriv *d = NULL;
	VcFolder *Folder = NULL;
	VersionCtrl Vcs = VcNone;

	// Commit Meta
	LString Rev;
	int64_t Index = -1; // Revision index (svn/hg)
	LString::Array Parents;
	LString Branch;
	LString Author;
	LDateTime Ts;
	LString Msg;

	void OnParse();

public:
	static size_t Instances;

	LString::Array Files;
	LArray<VcEdge*> Edges;
	int NodeIdx = -1; // Used by LinkParents
	int Idx = -1; // Used by LinkParents
	LColour NodeColour;
	LHashTbl<PtrKey<VcEdge*>, int> Pos;

	VcCommit(AppPriv *priv, VcFolder *folder);
	VcCommit(const VcCommit &c);
	~VcCommit();

	char *GetRev();
	int64_t GetIndex() { return Index; }
	bool IsRev(const char *r) { return !Strcmp(GetRev(), r); }
	char *GetAuthor();
	char *GetMsg();
	char *GetBranch();
	LString::Array *GetParents() { return &Parents; }
	LDateTime &GetTs() { return Ts; }

	void SetCurrent(bool b);
	void OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c);
	const char *GetText(int Col);
	const char *GetFieldText(CommitField Fld);
	bool GitParse(LString s, bool RevList);
	bool SvnParse(LString s);
	bool HgParse(LString s);
	bool CvsParse(LDateTime &Dt, LString Auth, LString Msg);
	VcFolder *GetFolder();

	// Events
	void OnMouseClick(LMouse &m);
	void Select(bool b);
};

#endif