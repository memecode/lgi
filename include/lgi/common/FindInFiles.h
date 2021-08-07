#ifndef _GFINDINFILES_H_
#define _GFINDINFILES_H_

#define OPT_SearchTerm		"FIF_Search"
#define OPT_SearchFolder	"FIF_Folder"
#define OPT_SearchPattern	"FIF_Pattern"

class GFindInFiles : public LDialog
{
	struct GFindInFilesPriv *d;
	
public:
	GFindInFiles(LViewI *Parent, LAutoString Search, GDom *Store);
	~GFindInFiles();
	
	int OnNotify(LViewI *Ctrl, LNotification n);
};

#endif
