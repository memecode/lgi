#include "lgi/common/Lgi.h"
#include "lgi/common/TableLayout.h"
#include "TransparentDlg.h"

#ifdef FILTER_UI

#ifdef WIN32
#define TRANS_DLG_Y		145
#else
#define TRANS_DLG_Y		110
#endif

#include "lgi/common/RadioGroup.h"
#include "lgi/common/Button.h"

enum Ctrls
{
	IDC_TABLE = 100,
	IDC_GRP,
};

LTransparentDlg::LTransparentDlg(LView *parent, LVariant *trans)
{
	Trans = trans;
	SetParent(parent);
	Name("Tranparency Settings");
	
	LRect r(0, 0, 200, TRANS_DLG_Y);	
	SetPos(r);
	ScaleSizeToDpi();
	MoveToCenter();

	auto tbl = new LTableLayout(IDC_TABLE);
	AddView(tbl);
	auto c = tbl->GetCell(0, 0);
	c->Add(Grp = new LRadioGroup(IDC_GRP, "Transparency"));
	if (Grp)
	{
		Grp->Append("Opaque");
		Grp->Append("Use background colour");
		Grp->Value((Trans) ? 1 : 0);
	}

	c = tbl->GetCell(0, 1);
	c->TextAlign(LCss::AlignCenter);
	c->Add(new LButton(IDOK, 0, 0, -1, -1, "Ok"));
}

int LTransparentDlg::OnNotify(LViewI *Ctrl, const LNotification &n)
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
