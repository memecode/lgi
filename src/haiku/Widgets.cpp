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

///////////////////////////////////////////////////////////////////////////////////////////
#define GreyBackground()

struct LDialogPriv
{
	int ModalStatus = 0;
	int BtnId = -1;
	bool IsModal = false;
	bool IsModeless = false;
	bool Resizable = true;
	thread_id CallingThread = 0;

	/// The callback for the modal dialog:
	LDialog::OnClose ModalCb;

	/// This is set when the parent window has a pointer
	/// to this dialog. We mustn't delete or change parent
	/// while they have that.
	bool ParentModal = false;
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
	if (parent)
		SetParent(parent);
}

LDialog::~LDialog()
{
	LAssert(!d->ParentModal);
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

bool LDialog::LoadFromResource(int Resource, char *TagList)
{
	LAutoString n;
	LRect p;
	LProfile Prof("LDialog::LoadFromResource");

	bool Status = LResourceLoad::LoadFromResource(Resource, this, &p, &n, TagList);
	if (Status)
	{
		Prof.Add("Name.");
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

void LDialog::DoModal(OnClose Cb, OsView OverrideParent)
{
	d->ModalStatus = -1;
	
	auto Parent = GetParent();
	if (Parent)
		MoveSameScreen(Parent);

	d->IsModal = true;
	d->IsModeless = false;
	d->ModalCb = Cb;
	d->CallingThread = find_thread(NULL);

	if (WindowHandle() &&
		Parent &&
		Parent->WindowHandle())
	{
		/*
		#if 1
		
		// Keep this dialog above the parent window...
		WindowHandle()->SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
		auto result = WindowHandle()->AddToSubset(Parent->WindowHandle());
		printf("%s:%i - %s::AddToSubset=%i\n", _FL, GetClass(), result);
		
		#else
		
		auto Wnd = dynamic_cast<LWindow*>(Parent);
		LAssert(Wnd);
		if (Wnd)
		{
			d->ParentModal = true;
			Wnd->SetModalDialog(this);
		}
		
		#endif
		*/
	}
	else LgiTrace("%s:%i - Can't set parent for modal.\n", _FL);

	BLooper *looper = BLooper::LooperForThread(d->CallingThread);
	if (!looper)
		printf("%s:%i - no looper for domodal thread.\n",_FL);

	if (Attach(0))
	{
		AttachChildren();
		Visible(true);
	}
	else printf("%s:%i - attach failed..\n", _FL);
}

void LDialog::EndModal(int Code)
{
	if (!d->IsModal)
	{
		LgiTrace("%s:%i - EndModal error: LDialog is not model.\n", _FL);
		return;
	}
	
	/*
	if (d->ParentModal)
	{
		auto Wnd = dynamic_cast<LWindow*>(GetParent());
		LAssert(Wnd);
		if (Wnd)
		{
			Wnd->SetModalDialog(NULL);
			d->ParentModal = false;
		}
	}
	*/

	d->IsModal = false;
	if (!d->ModalCb)
	{
		// If no callback is supplied, the default option is to just delete the
		// dialog, closing it.
		delete this;
		return;
	}
	
	BLooper *looper = BLooper::LooperForThread(d->CallingThread);
	if (!looper)
	{
		LgiTrace("%s:%i - Failed to get looper for %p\n", _FL, d->CallingThread);
		delete this;
		return;
	}
	
	BMessage *m = new BMessage(M_HANDLE_IN_THREAD);
	m->AddPointer
	(
		LMessage::PropCallback,
		new LMessage::InThreadCb
		(
			[dlg=this, cb=d->ModalCb, code=Code]()
			{
				// printf("%s:%i - Calling LDialog callback.. in original thread\n", _FL);
				cb(dlg, code);
				// printf("%s:%i - Calling LDialog callback.. done\n", _FL);
			}
		)
	);
		
	looper->PostMessage(m);
}

int LDialog::DoModeless()
{
	d->IsModal = false;
	d->IsModeless = true;
	
	Visible(true);
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

