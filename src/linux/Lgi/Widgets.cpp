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

#include "Lgi.h"
#include "GSlider.h"
#include "LBitmap.h"
#include "GTableLayout.h"
#include "LDisplayString.h"
#include "GButton.h"

using namespace Gtk;
#include "LgiWidget.h"

///////////////////////////////////////////////////////////////////////////////////////////
#define GreyBackground()

struct GDialogPriv
{
	int ModalStatus;
	int BtnId;
	bool IsModal, IsModeless;
	bool Resizable;
	
	GDialogPriv()
	{
		IsModal = false;
		IsModeless = false;
		Resizable = true;
		ModalStatus = 0;
		BtnId = -1;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog()
	:
	#ifdef __GTK_H__
	// , LWindow(gtk_dialog_new())
	LWindow(gtk_window_new(GTK_WINDOW_TOPLEVEL)),
	#endif
	ResObject(Res_Dialog)
{
	d = new GDialogPriv();
	Name("Dialog");
	_SetDynamic(false);
}

GDialog::~GDialog()
{
	DeleteObj(d);
}

bool GDialog::IsModal()
{
	return d->IsModal;
}

int GDialog::GetButtonId()
{
	return d->BtnId;
}

int GDialog::OnNotify(LViewI *Ctrl, int Flags)
{
	GButton *b = dynamic_cast<GButton*>(Ctrl);
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


void GDialog::Quit(bool DontDelete)
{
	if (d->IsModal)
		EndModal(0);
	else
		LView::Quit(DontDelete);
}

void GDialog::OnPosChange()
{
	LWindow::OnPosChange();
    if (Children.Length() == 1)
    {
        List<LViewI>::I it = Children.begin();
        GTableLayout *t = dynamic_cast<GTableLayout*>((LViewI*)it);
        if (t)
        {
            LRect r = GetClient();
            r.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
            t->SetPos(r);

			// _Dump();
        }
    }
}

bool GDialog::LoadFromResource(int Resource, char *TagList)
{
	GAutoString n;
	LRect p;
	LProfile Prof("GDialog::LoadFromResource");

	bool Status = GLgiRes::LoadFromResource(Resource, this, &p, &n, TagList);
	if (Status)
	{
		Prof.Add("Name.");
		Name(n);
		SetPos(p);
	}
	
	return Status;
}

bool GDialog::OnRequestClose(bool OsClose)
{
	if (d->IsModal)
	{
		EndModal(0);
		return false;
	}

	return true;
}

bool GDialog::IsResizeable()
{
    return d->Resizable;
}

void GDialog::IsResizeable(bool r)
{
	d->Resizable = r;
}

bool GDialog::SetupDialog(bool Modal)
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

int GDialog::DoModal(OsView OverrideParent)
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
	LgiApp->Run();
	
	return d->ModalStatus;
}

void GDialog::EndModal(int Code)
{
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = Code;
		LgiApp->Exit();
	}
	else
	{
		// LgiAssert(0);
	}
}

int GDialog::DoModeless()
{
	d->IsModal = false;
	d->IsModeless = true;
	SetupDialog(false);
	return 0;
}

void GDialog::EndModeless(int Code)
{
	Quit(Code);
}

extern GButton *FindDefault(LView *w);

GMessage::Param GDialog::OnEvent(GMessage *Msg)
{
	return LView::OnEvent(Msg);
}

///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(OsView view) : LView(view)
{
	Pos.ZOff(10, 10);
}

GControl::~GControl()
{
}

GMessage::Param GControl::OnEvent(GMessage *Msg)
{
	return 0;
}

LPoint GControl::SizeOfStr(const char *Str)
{
	int y = SysFont->GetHeight();
	LPoint Pt(0, 0);

	if (Str)
	{
		const char *e = 0;
		for (const char *s = Str; s && *s; s = e?e+1:0)
		{
			e = strchr(s, '\n');
			auto Len = e ? e - s : strlen(s);

			LDisplayString ds(SysFont, s, Len);
			Pt.x = MAX(Pt.x, ds.X());
			Pt.y += y;
		}
	}

	return Pt;
}

