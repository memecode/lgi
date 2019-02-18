#ifndef _VcCommit_h_
#define _VcCommit_h_

class VcFolder;

struct Node
{
	GColour c;
	GString Rev;
	GArray<uint8_t> Next, Prev;
};

class VcCommit : public LListItem
{
	AppPriv *d;
	bool Current;
	GString Rev;
	GString::Array Parents;
	GString Author;
	LDateTime Ts;
	GString Cache;
	GString Msg;

public:
	GString::Array Files;
	GArray<Node> Nodes;
	int NodeIdx;

	VcCommit(AppPriv *priv);

	char *GetRev();
	bool IsRev(const char *r) { return !Strcmp(GetRev(), r); }
	char *GetAuthor();
	char *GetMsg();
	GString::Array *GetParents() { return &Parents; }
	LDateTime &GetTs() { return Ts; }

	void SetCurrent(bool b);
	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c);
	char *GetText(int Col);
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