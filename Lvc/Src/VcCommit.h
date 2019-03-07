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
	LTimeStamp,
	LMessage,
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
	GString Cache;
	bool Current;

protected:
	AppPriv *d;
	VcFolder *Folder;

	// Commit Meta
	GString Rev;
	int64_t Index;
	GString::Array Parents;
	GString Branch;
	GString Author;
	LDateTime Ts;
	GString Msg;

public:
	GString::Array Files;
	GArray<VcEdge*> Edges;
	int NodeIdx;
	int Idx;
	GColour NodeColour;
	LHashTbl<PtrKey<VcEdge*>, int> Pos;

	VcCommit(AppPriv *priv, VcFolder *folder);
	~VcCommit();

	char *GetRev();
	int64_t GetIndex() { return Index; }
	bool IsRev(const char *r) { return !Strcmp(GetRev(), r); }
	char *GetAuthor();
	char *GetMsg();
	char *GetBranch();
	GString::Array *GetParents() { return &Parents; }
	LDateTime &GetTs() { return Ts; }

	void SetCurrent(bool b);
	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c);
	char *GetText(int Col);
	const char *GetFieldText(CommitField Fld);
	bool GitParse(GString s, bool RevList);
	bool SvnParse(GString s);
	bool HgParse(GString s);
	bool CvsParse(LDateTime &Dt, GString Auth, GString Msg);
	VcFolder *GetFolder();

	// Events
	void OnMouseClick(GMouse &m);
	void Select(bool b);
};

#endif