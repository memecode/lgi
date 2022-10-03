#include "lgi/common/Gdc2.h"
#include "TransparentDlg.h"

#ifdef FILTER_UI

#ifdef WIN32
#define TRANS_DLG_Y		145
#else
#define TRANS_DLG_Y		110
#endif

#include "lgi/common/RadioGroup.h"
#include "lgi/common/Button.h"

GTransparentDlg::GTransparentDlg(LView *parent, LVariant *trans)
{
	Trans = trans;
	SetParent(parent);
	Name("Tranparency Settings");
	
	LRect r(0, 0, 200, TRANS_DLG_Y);
	SetPos(r);
	MoveToCenter();

	Children.Insert(Grp = new LRadioGroup(100, 10, 10, 180, 65, "Transparency"));
	if (Grp)
	{
		Grp->Append(10, 20, "Opaque");
		Grp->Append(10, 40, "Use background colour");
		Grp->Value((Trans) ? 1 : 0);
	}

	Children.Insert(new LButton(IDOK, 65, 82, 60, 20, "Ok"));
}

int GTransparentDlg::OnNotify(LViewI *Ctrl, LNotification n)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		{
			*Trans = Grp->Value() == 1;
			EndModal(1);
			break;
		}
	}
	return 0;
}
#endif
