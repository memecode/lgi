#ifndef __GTransparentDlg_h
#define __GTransparentDlg_h

#ifdef FILTER_UI

#include "Lgi.h"
#include "LVariant.h"

class GTransparentDlg : public GDialog
{
	GRadioGroup *Grp;
	LVariant *Trans;

public:
	GTransparentDlg(GView *parent, LVariant *trans);
	int OnNotify(GViewI *Ctrl, int Flags);
};
#endif


#endif
