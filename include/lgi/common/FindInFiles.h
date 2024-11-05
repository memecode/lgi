#ifndef _GFINDINFILES_H_
#define _GFINDINFILES_H_

#define OPT_SearchTerm		"FIF_Search"
#define OPT_SearchFolder	"FIF_Folder"
#define OPT_SearchPattern	"FIF_Pattern"

class LFindInFiles : public LDialog
{
	struct LFindInFilesPriv *d;
	
public:
	LFindInFiles(LViewI *Parent, LAutoString Search, LDom *Store);
	~LFindInFiles();
	
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
};

#endif
