#ifndef __GTransparentDlg_h
#define __GTransparentDlg_h

#ifdef FILTER_UI

#include "Lgi.h"
#include "LVariant.h"

class GTransparentDlg : public GDialog
{
	LRadioGroup *Grp;
	LVariant *Trans;

public:
	GTransparentDlg(LView *parent, LVariant *trans);
	int OnNotify(LViewI *Ctrl, int Flags);
};
#endif


#endif
