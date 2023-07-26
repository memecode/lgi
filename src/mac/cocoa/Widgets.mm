/*hdr
 **      FILE:           GWidgets.cpp
 **      AUTHOR:         Matthew Allen
 **      DATE:           30/12/2006
 **      DESCRIPTION:    Mac dialog components
 **
 **      Copyright (C) 2006 Matthew Allen
 **              fret@memecode.com
 */

#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Slider.h"
#include "lgi/common/Bitmap.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Button.h"

///////////////////////////////////////////////////////////////////////////////////////////
#define GreyBackground()

struct LDialogPriv
{
	bool IsModal;
	int ModalStatus;
	int BtnId;
	
	LDialogPriv()
	{
		IsModal = false;
		ModalStatus = -1;
		BtnId = -1;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
LDialog::LDialog(LViewI *Parent)
	: ResObject(Res_Dialog)
{
	d = new LDialogPriv;
	Name("Dialog");
	SetDeleteOnClose(false);
	
	if (Parent)
		SetParent(Parent);
}

LDialog::~LDialog()
{
	DeleteObj(d);
}

int LDialog::GetButtonId()
{
	return d->BtnId;
}

int LDialog::OnNotify(LViewI *Ctrl, LNotification n)
{
	auto b = dynamic_cast<LButton*>(Ctrl);
	if (b)
	{
		d->BtnId = b->GetId();
		
		if (d->IsModal)
			EndModal(d->BtnId);
		else
			EndModeless(d->BtnId);
	}

	return 0;
}

bool LDialog::IsModal()
{
	return d->IsModal;
}

void LDialog::Quit(bool DontDelete)
{
	if (d->IsModal)
		EndModal(0);
	else
		EndModeless(0);
}

void LDialog::OnPosChange()
{
	if (Children.Length() == 1)
	{
		auto it = Children.begin();
		auto t = dynamic_cast<LTableLayout*>((LViewI*)it);
		if (t)
		{
			auto r = GetClient();
			r.Inset(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
			t->SetPos(r);
		}
	}
}

bool LDialog::LoadFromResource(int Resource, char *TagList)
{
	LAutoString n;
	LRect p;
	
	bool Status = LResourceLoad::LoadFromResource(Resource, this, &p, &n, TagList);
	if (Status)
	{
		Name(n);
		SetPos(p);
	}
	return Status;
}

bool LDialog::OnRequestClose(bool OsClose)
{
	if (d->IsModal)
	{
		EndModal(0);
		return false;
	}
	
	return true;
}

void LDialog::DoModal(OnClose Callback, OsView OverideParent)
{
	d->ModalStatus = 0;
	
	if (Wnd && Attach(0))
	{
		// LAutoPool Pool;
		auto Owner = GetParent() ? GetParent()->GetWindow() : NULL;
		if (Owner)
		{
			auto Pr = Owner->GetPos();
			auto Mr = GetPos();
			Mr.Offset(Pr.x1 + (Pr.X() - Mr.X()) / 2 - Mr.x1,
					  Pr.y1 + (Pr.Y() - Mr.Y()) / 2 - Mr.y1);
			SetPos(Mr);
			Owner->SetModalChild(this);
		}
		
		d->IsModal = true;
		AttachChildren();
		Visible(true);
		
		auto app = LAppInst->Handle();
		auto wnd = WindowHandle();
		[app runModalForWindow:wnd];
		
		if (Owner)
			Owner->SetModalChild(NULL);
		LWindow::Visible(false);
	}
	
	if (Callback)
	{
		// Callback is responsible for deleting 'this'
		Callback(this, d->ModalStatus);
	}
	else
	{
		delete this;
	}
}

void LDialog::EndModal(int Code)
{
	if (d->IsModal)
	{
		// LAutoPool Pool;
		d->IsModal = false;
		d->ModalStatus = Code;
		
		[LAppInst->Handle() stopModal];
	}
	else
	{
		LAssert(0);
	}
}

int LDialog::DoModeless()
{
	d->IsModal = false;
	if (Attach(0))
	{
		AttachChildren();
		Visible(true);
	}
	return 0;
}

void LDialog::EndModeless(int Code)
{
	LWindow::Quit(Code);
}

extern LButton *FindDefault(LView *w);

LMessage::Result LDialog::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_CLOSE:
		{
			Quit();
			break;
		}
	}
	
	return LView::OnEvent(Msg);
}

///////////////////////////////////////////////////////////////////////////////////////////
LControl::LControl(OsView view) : LView(view)
{
	Pos.ZOff(10, 10);
}

LControl::~LControl()
{
}

LMessage::Result LControl::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
	}
	return LView::OnEvent(Msg);
}

LPoint LControl::SizeOfStr(const char *Str)
{
	auto Fnt = GetFont();
	int y = Fnt->GetHeight();
	LPoint Pt(0, 0);
	
	if (Str)
	{
		const char *e = 0;
		for (const char *s = Str; s && *s; s = e?e+1:0)
		{
			e = strchr(s, '\n');
			size_t Len = e ? e-s : strlen(s);
			
			LDisplayString ds(Fnt, s, Len);
			Pt.x = MAX(Pt.x, ds.X());
			Pt.y += y;
		}
	}
	
	return Pt;
}

