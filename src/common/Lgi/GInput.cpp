/*hdr
**      FILE:           GuiCommon.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           18/7/98
**      DESCRIPTION:    Common windows and controls
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Button.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/TableLayout.h"

enum InputCtrls
{
	IDC_CALLBACK = 100,
	IDC_EDIT,
	IDC_TABLE,
	IDC_CUSTOM,
};

//////////////////////////////////////////////////////////////////////////////
GInput::GInput(GViewI *parent, const char *InitStr, const char *Msg, const char *Title, bool Password, GInputCallback callback, void *callbackparam)
{
	Callback = callback;
	CallbackParam = callbackparam;

	GTableLayout *Tbl = new GTableLayout(IDC_TABLE);
	if (!Tbl)
	{
		LgiAssert(0);
		return;
	}
	AddView(Tbl);

	int Cy = 0;
	GLayoutCell *c = Tbl->GetCell(0, Cy++);
	GView *Txt;
	c->Add(Txt = new GTextLabel(-1, 5, 5, -1, -1, Msg));

	LDisplayString MsgDs(SysFont, ValidStr(InitStr)?InitStr:"A");
	int Dx = LgiApp->GetMetric(LGI_MET_DECOR_X) + 10;
	int Dy = LgiApp->GetMetric(LGI_MET_DECOR_Y);
	
	int ContextX = 400;
	ContextX = MAX(ContextX, MsgDs.X() + 40);
	ContextX = MIN(ContextX, (int)(GdcD->X() * 0.8));
	int EditX = ContextX;
	int CallbackX = callback ? GBUTTON_MIN_X + 20 : 0;
	ContextX = MAX(ContextX, Txt->X() + CallbackX);

	LRect r(0, 0, ContextX + CallbackX + Dx, 80 + MAX(SysFont->GetHeight(), Txt->Y()) + Dy);

	SetParent(parent);
	Name(Title);

	c = Tbl->GetCell(0, Cy++);
	c->Add(Edit = new GEdit(IDC_EDIT, 5, Txt->GetPos().y2 + 5, EditX - 1, MsgDs.Y()+7, InitStr));
	if (Edit)
	{
		Edit->Password(Password);
		Edit->Focus(true);
		if (Callback)
			c->Add(new GButton(IDC_CALLBACK, 0, 0, -1, -1, "..."));
	}

	c = Tbl->GetCell(0, Cy++);
	c->TextAlign(GCss::AlignRight);

	GButton *Ok;
	c->Add(Ok = new GButton(IDOK, 0, 0, -1, -1, LgiLoadString(L_BTN_OK, "Ok")));
	c->Add(new GButton(IDCANCEL, 0, 0, -1, -1, LgiLoadString(L_BTN_CANCEL, "Cancel")));
	Ok->Default(true);

	SetPos(r);
	MoveToCenter();
}

int GInput::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_CALLBACK:
		{
			if (Callback)
				Callback(this, Edit, CallbackParam);
			break;
		}
		case IDOK:
		{
			Str = Edit->Name();
			// fall thru
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId() == IDOK);
			break;
		}
	}

	return 0;
}

