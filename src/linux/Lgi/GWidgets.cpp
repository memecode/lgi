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
#include "GBitmap.h"
#include "GTableLayout.h"
#include "GDisplayString.h"
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
	// , GWindow(gtk_dialog_new())
	GWindow(gtk_window_new(GTK_WINDOW_TOPLEVEL)),
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

int GDialog::OnNotify(GViewI *Ctrl, int Flags)
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
		GView::Quit(DontDelete);
}

void GDialog::OnPosChange()
{
	GWindow::OnPosChange();
    if (Children.Length() == 1)
    {
        List<GViewI>::I it = Children.begin();
        GTableLayout *t = dynamic_cast<GTableLayout*>((GViewI*)it);
        if (t)
        {
            GRect r = GetClient();
            r.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
            t->SetPos(r);

			// _Dump();
        }
    }
}

bool GDialog::LoadFromResource(int Resource, char *TagList)
{
	GAutoString n;
	GRect p;

	bool Status = GLgiRes::LoadFromResource(Resource, this, &p, &n, TagList);
	if (Status)
	{
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
	#if GTK_MAJOR_VERSION == 3
	#else
	gtk_dialog_set_has_separator(GTK_DIALOG(Wnd), false);
	#endif
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

	GWindow::Visible(true);

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

extern GButton *FindDefault(GView *w);

GMessage::Param GDialog::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}


///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(OsView view) : GView(view)
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

GdcPt2 GControl::SizeOfStr(const char *Str)
{
	int y = SysFont->GetHeight();
	GdcPt2 Pt(0, 0);

	if (Str)
	{
		const char *e = 0;
		for (const char *s = Str; s && *s; s = e?e+1:0)
		{
			e = strchr(s, '\n');
			auto Len = e ? e - s : strlen(s);

			GDisplayString ds(SysFont, s, Len);
			Pt.x = MAX(Pt.x, ds.X());
			Pt.y += y;
		}
	}

	return Pt;
}

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	ResObject(Res_Slider)
{
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	Vertical = vert;
	Min = Max = 0;
	Val = 0;
	SetTabStop(true);
}

GSlider::~GSlider()
{
}

void GSlider::Value(int64 i)
{
	if (i > Max) i = Max;
	if (i < Min) i = Min;
	
	if (i != Val)
	{
		Val = i;

		GViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
		{
			n->OnNotify(this, Val);
		}
		
		Invalidate();
	}
}

int64 GSlider::Value()
{
	return Val;
}

void GSlider::GetLimits(int64 &min, int64 &max)
{
	min = Min;
	max = Max;
}

void GSlider::SetLimits(int64 min, int64 max)
{
	Min = min;
	Max = max;
}

GMessage::Param GSlider::OnEvent(GMessage *Msg)
{
	return 0;
}

void GSlider::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
	
	GRect r = GetClient();
	int y = r.Y() >> 1;
	r.y1 = y - 2;
	r.y2 = r.y1 + 3;
	r.x1 += 3;
	r.x2 -= 3;
	LgiWideBorder(pDC, r, DefaultSunkenEdge);
	
	if (Min <= Max)
	{
		int x = Val * r.X() / (Max-Min);
		Thumb.ZOff(5, 9);
		Thumb.Offset(r.x1 + x - 3, y - 5);
		GRect b = Thumb;
		LgiWideBorder(pDC, b, DefaultRaisedEdge);
		pDC->Rectangle(&b);		
	}
}

void GSlider::OnMouseClick(GMouse &m)
{
	Capture(m.Down());
	if (Thumb.Overlap(m.x, m.y))
	{
		Tx = m.x - Thumb.x1;
		Ty = m.y - Thumb.y1;
	}
}

void GSlider::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		int Rx = X() - 6;
		if (Rx > 0 && Max >= Min)
		{
			int x = m.x - Tx;
			int v = x * (Max-Min) / Rx;
			Value(v);
		}
	}
}

class GSlider_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (stricmp(Class, "GSlider") == 0)
		{
			return new GSlider(-1, 0, 0, 100, 20, 0, 0);
		}

		return 0;
	}

} GSliderFactory;

