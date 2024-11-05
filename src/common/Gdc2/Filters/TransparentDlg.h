#ifndef __GTransparentDlg_h
#define __GTransparentDlg_h

#ifdef FILTER_UI

#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/RadioGroup.h"

class LTransparentDlg : public LDialog
{
	LRadioGroup *Grp;
	LVariant *Trans;

public:
	LTransparentDlg(LView *parent, LVariant *trans);
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
};
#endif


#endif
