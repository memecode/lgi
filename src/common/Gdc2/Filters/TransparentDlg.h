#ifndef __GTransparentDlg_h
#define __GTransparentDlg_h

#ifdef FILTER_UI

#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/RadioGroup.h"

class GTransparentDlg : public LDialog
{
	LRadioGroup *Grp;
	LVariant *Trans;

public:
	GTransparentDlg(LView *parent, LVariant *trans);
	int OnNotify(LViewI *Ctrl, LNotification n);
};
#endif


#endif
