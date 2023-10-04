/*hdr
**      FILE:           GuiDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Dialog components
**
**      Copyright (C) 1998 Matthew Allen
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

using namespace Gtk;
#include "LgiWidget.h"

///////////////////////////////////////////////////////////////////////////////////////////
#define GreyBackground()

struct LDialogPriv
{
	int ModalStatus;
	int BtnId;
	bool IsModal, IsModeless;
	bool Resizable;
	
	LDialogPriv()
	{
		IsModal = false;
		IsModeless = false;
		Resizable = true;
		ModalStatus = 0;
		BtnId = -1;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
LDialog::LDialog(LViewI *parent)
	:
	#ifdef __GTK_H__
	// , LWindow(gtk_dialog_new())
	LWindow(gtk_window_new(GTK_WINDOW_TOPLEVEL)),
	#endif
	ResObject(Res_Dialog)
{
	d = new LDialogPriv();
	Name("Dialog");
	_SetDynamic(false);
	
	if (parent)
		SetParent(parent);
}

LDialog::~LDialog()
{
	DeleteObj(d);
}

bool LDialog::IsModal()
{
	return d->IsModal;
}

int LDialog::GetButtonId()
{
	return d->BtnId;
}

int LDialog::OnNotify(LViewI *Ctrl, LNotification n)
{
	LButton *b = dynamic_cast<LButton*>(Ctrl);
	if (b)
	{
		d->BtnId = b->GetId();
		
		if (d->IsModal)
			EndModal();
		else if (d->IsModeless)
			EndModeless();
	}

	return 0;
}


void LDialog::Quit(bool DontDelete)
{
	if (d->IsModal)
		EndModal(0);
	else
		LView::Quit(DontDelete);
}

void LDialog::OnPosChange()
{
	LWindow::OnPosChange();
    if (Children.Length() == 1)
    {
        List<LViewI>::I it = Children.begin();
        LTableLayout *t = dynamic_cast<LTableLayout*>((LViewI*)it);
        if (t)
        {
            LRect r = GetClient();
            r.Inset(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
            t->SetPos(r);

			// _Dump();
        }
    }
}

bool LDialog::LoadFromResource(int Resource, const char *TagList)
{
	LString n;
	LRect p;

	bool Status = LResourceLoad::LoadFromResource(Resource, this, &p, &n, TagList);
	if (Status)
	{
		// Prof.Add("Name.");
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

bool LDialog::IsResizeable()
{
    return d->Resizable;
}

void LDialog::IsResizeable(bool r)
{
	d->Resizable = r;
}

bool LDialog::SetupDialog(bool Modal)
{
	if (IsResizeable())
	{
	    gtk_window_set_default_size(Wnd, Pos.X(), Pos.Y());
	}
	else
	{
	    gtk_widget_set_size_request(GTK_WIDGET(Wnd), Pos.X(), Pos.Y());
	    gtk_window_set_resizable(Wnd, FALSE);
	}

	auto p = GetParent();
	if (!Attach(p))
		return false;

	LWindow::Visible(true);

	return true;
}

void LDialog::DoModal(OnClose Callback, OsView OverrideParent)
{
	d->ModalStatus = -1;
	
	auto Parent = GetParent();
	if (Parent)
	{
		gtk_window_set_transient_for(GTK_WINDOW(Wnd), Parent->WindowHandle());
		MoveSameScreen(Parent);
	}

	d->IsModal = true;
	d->IsModeless = false;
	SetupDialog(true);
	LAppInst->Run();
	
	if (Callback)
		Callback(this, d->ModalStatus);
	else
		delete this;
}

void LDialog::EndModal(int Code)
{
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = Code;
		LAppInst->Exit();
	}
	else
	{
		// LAssert(0);
	}
}

int LDialog::DoModeless()
{
	d->IsModal = false;
	d->IsModeless = true;
	SetupDialog(false);
	return 0;
}

void LDialog::EndModeless(int Code)
{
	Quit(Code);
}

extern LButton *FindDefault(LView *w);

LMessage::Param LDialog::OnEvent(LMessage *Msg)
{
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

LMessage::Param LControl::OnEvent(LMessage *Msg)
{
	return 0;
}

LPoint LControl::SizeOfStr(const char *Str)
{
	int y = LSysFont->GetHeight();
	LPoint Pt(0, 0);

	if (Str)
	{
		const char *e = 0;
		for (const char *s = Str; s && *s; s = e?e+1:0)
		{
			e = strchr(s, '\n');
			auto Len = e ? e - s : strlen(s);

			LDisplayString ds(LSysFont, s, Len);
			Pt.x = MAX(Pt.x, ds.X());
			Pt.y += y;
		}
	}

	return Pt;
}

