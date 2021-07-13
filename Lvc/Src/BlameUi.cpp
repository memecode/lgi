#include "lgi/common/Lgi.h"
#include "Lvc.h"
#include "lgi/common/TextLog.h"

#define OPT_WND_STATE		"BlameUiState"
struct BlameUiPriv
{
	LString Output;
	AppPriv *Priv;
	LTextLog *Log;
};

BlameUi::BlameUi(AppPriv *priv, VersionCtrl Vc, LString Output)
{
	d = new BlameUiPriv;
	d->Log = NULL;
	d->Output = Output;
	d->Priv = priv;
	Name("Lvc Blame");

	if (!SerializeState(&d->Priv->Opts, OPT_WND_STATE, true))
	{
		LRect r(0, 0, 800, 500);
		SetPos(r);
		MoveToCenter();
	}

	if (Attach(0))
	{
		AddView(d->Log = new LTextLog(100));
		AttachChildren();
		Visible(true);

		d->Log->Name(Output);
	}
}

BlameUi::~BlameUi()
{
	SerializeState(&d->Priv->Opts, OPT_WND_STATE, false);
	DeleteObj(d);
}
