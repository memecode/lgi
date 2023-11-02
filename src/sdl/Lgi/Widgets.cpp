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
#include "LSlider.h"
#include "LBitmap.h"
#include "LTableLayout.h"
#include "LDisplayString.h"

///////////////////////////////////////////////////////////////////////////////////////////
struct LDialogPriv
{
	int ModalStatus = 0;
	bool IsModal = true;
	bool Resizable = true;
	LWindow *PrevWindow = NULL;
};

///////////////////////////////////////////////////////////////////////////////////////////
LDialog::LDialog()
	: ResObject(Res_Dialog)
{
	d = new LDialogPriv();
	Name("Dialog");
	_View = 0;
	_SetDynamic(false);
}

LDialog::~LDialog()
{
	DeleteObj(d);
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
		LView::Quit(DontDelete);
}

void LDialog::OnPosChange()
{
    if (Children.Length() == 1)
    {
        auto it = Children.begin();
        LTableLayout *t = dynamic_cast<LTableLayout*>((LViewI*)it);
        if (t)
        {
            LRect r = GetClient();
            r.Size(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
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

int LDialog::DoModal(OsView OverrideParent)
{
	d->ModalStatus = -1;
	d->IsModal = true;
	d->PrevWindow = LAppInst->AppWnd;	
	AttachChildren();
	if (d->PrevWindow)
		d->PrevWindow->PushWindow(this);
	else
		LAppInst->PushWindow(this);
	LAppInst->Run();
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = 0;
		d->PrevWindow->PopWindow();
	}
	return d->ModalStatus;
}

void LDialog::EndModal(int Code)
{
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = Code;
		LAppInst->Exit();
		d->PrevWindow->PopWindow();
	}
	else
	{
		LAssert(!"Not a modal dialog");
	}
}

int LDialog::DoModeless()
{
	d->IsModal = false;
	return 0;
}

void LDialog::EndModeless(int Code)
{
	Quit(Code);
}

LMessage::Result LDialog::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
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
	switch (MsgCode(Msg))
	{
	}
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
			int Len = e ? (int)e-(int)s : strlen(s);

			LDisplayString ds(LSysFont, s, Len);
			Pt.x = MAX(Pt.x, ds.X());
			Pt.y += y;
		}
	}

	return Pt;
}

//////////////////////////////////////////////////////////////////////////////////
// Slider control
LSlider::LSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	ResObject(Res_Slider)
{
	SetId(id);
	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	Vertical = vert;
	Min = Max = 0;
	Val = 0;
	SetTabStop(true);
}

LSlider::~LSlider()
{
}

void LSlider::Value(int64 i)
{
	if (i > Max) i = Max;
	if (i < Min) i = Min;
	
	if (i != Val)
	{
		Val = i;

		LViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
		{
			n->OnNotify(this, Val);
		}
		
		Invalidate();
	}
}

int64 LSlider::Value()
{
	return Val;
}

void LSlider::GetLimits(int64 &min, int64 &max)
{
	min = Min;
	max = Max;
}

void LSlider::SetLimits(int64 min, int64 max)
{
	Min = min;
	Max = max;
}

LMessage::Result LSlider::OnEvent(LMessage *Msg)
{
	return 0;
}

void LSlider::OnPaint(LSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
	
	LRect r = GetClient();
	int y = r.Y() >> 1;
	r.y1 = y - 2;
	r.y2 = r.y1 + 3;
	r.x1 += 3;
	r.x2 -= 3;
	LWideBorder(pDC, r, DefaultSunkenEdge);
	
	if (Min <= Max)
	{
		int x = Val * r.X() / (Max-Min);
		Thumb.ZOff(5, 9);
		Thumb.Offset(r.x1 + x - 3, y - 5);
		LRect b = Thumb;
		LWideBorder(pDC, b, DefaultRaisedEdge);
		pDC->Rectangle(&b);		
	}
}

void LSlider::OnMouseClick(LMouse &m)
{
	Capture(m.Down());
	if (Thumb.Overlap(m.x, m.y))
	{
		Tx = m.x - Thumb.x1;
		Ty = m.y - Thumb.y1;
	}
}

void LSlider::OnMouseMove(LMouse &m)
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
