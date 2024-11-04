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

#define DEBUG_DIALOG	0
#if DEBUG_DIALOG
#define LOG(...)		printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

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
};

///////////////////////////////////////////////////////////////////////////////////////////
LDialog::LDialog(LViewI *parent) :
	ResObject(Res_Dialog)
{
	d = new LDialogPriv();
	Name("Dialog");
	if (parent)
		SetParent(parent);
}

LDialog::~LDialog()
{
	if (GetModalParent())
	{
		LAssert(!"Parent still has us as modal.");
	}
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

int LDialog::OnNotify(LViewI *Ctrl, const LNotification &n)
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
	LOG("%s:%i %s modal=%i dontdelete=%i\n", _FL, __FUNCTION__, d->IsModal, DontDelete);
	if (d->IsModal)
		EndModal(0);
	else
		LView::Quit(DontDelete);
}

void LDialog::OnPosChange()
{
	LWindow::OnPosChange();
	LOG("%s:%i %s children=%i\n", _FL, __FUNCTION__, (int)Children.Length());
    if (Children.Length() == 1)
    {
        auto it = Children.begin();
		LViewI *view = *it;
        auto t = dynamic_cast<LTableLayout*>(view);
        if (t)
        {
            auto r = GetClient();
            r.Inset(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
            t->SetPos(r);
        }
		else printf("%s:%i - t=null cls=%s\n", _FL, view->GetClass());
    }
	else printf("%s:%i - children=%i\n", _FL, (int)Children.Length());
}

bool LDialog::LoadFromResource(int Resource, const char *TagList)
{
	LString n;
	LRect p;
	LProfile Prof("LDialog::LoadFromResource");

	bool Status = LResourceLoad::LoadFromResource(Resource, this, &p, &n, TagList);
	LOG("%s:%i %s status=%i\n", _FL, __FUNCTION__, Status);
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
	LOG("%s:%i %s IsModal=%i\n", _FL, __FUNCTION__, d->IsModal);
	if (d->IsModal)
	{
		EndModal(0);
		return false;
	}

	return true;
}

void LDialog::DoModal(OnClose Cb, OsView OverrideParent)
{
	LAssert(!d->IsModal); // Don't call DoModal twice!
	
	d->ModalStatus = -1;
	
	auto Parent = GetParent();
	while (	Parent &&
			Parent->GetParent() &&
			!dynamic_cast<LWindow*>(Parent))
		Parent = Parent->GetParent();
	
	d->IsModal = true;
	d->IsModeless = false;
	d->ModalCb = Cb;
	d->CallingThread = find_thread(NULL);

	if (WindowHandle() &&
		Parent &&
		Parent->WindowHandle())
	{
		/*
		This doesn't work with multiple nested levels of windows. But would otherwise
		be ideal:
		
			WindowHandle()->SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
			auto result = WindowHandle()->AddToSubset(Parent->WindowHandle());
			
		*/
		
		auto Wnd = dynamic_cast<LWindow*>(Parent);
		if (Wnd)
		{
			// printf("%s->SetModalParent(%s)\n", GetClass(), Wnd->GetClass());
			SetModalParent(Wnd);
		}
		else
		{
			LgiTrace("%s:%i - Parent(%s) not a LWindow.\n", _FL, Parent->GetClass());
			LAssert(!"Parent not a LWindow");
		}
	}
	else LgiTrace("%s:%i - Can't set parent for modal.\n", _FL);

	auto looper = BLooper::LooperForThread(d->CallingThread);
	if (!looper)
		printf(	"%s:%i - no looper for domodal thread: %i/%s cls=%s.\n",
				_FL,
				d->CallingThread,
				LThread::GetThreadName(d->CallingThread),
				GetClass());

	LOG("%s:%i %s calling attach...\n", _FL, __FUNCTION__);
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
	
	SetModalParent(NULL);

	d->IsModal = false;
	if (!d->ModalCb)
	{
		// If no callback is supplied, the default option is to just delete the
		// dialog, closing it.
		delete this;
		return;
	}

	BLooper *looper = NULL;
		
	if (GetParent() &&
		GetParent()->WindowHandle())
	{
		auto parentThreadId = GetParent()->WindowHandle()->Thread();
		if (parentThreadId != -1 &&
			parentThreadId != d->CallingThread)
		{
			printf("%s:%i - Parent wnd thread (%i,%s) different to calling thread (%i,%s)\n",
				_FL,
				parentThreadId, LThread::GetThreadName(parentThreadId),
				d->CallingThread, LThread::GetThreadName(d->CallingThread));
				
			looper = BLooper::LooperForThread(parentThreadId);
		}
	}
	
	if (!looper)
	{
		looper = BLooper::LooperForThread(d->CallingThread);
		if (!looper)
		{
			LgiTrace("%s:%i - Failed to get looper for %p\n", _FL, d->CallingThread);
			delete this;
			return;
		}
	}
	
	auto m = new BMessage(M_HANDLE_IN_THREAD);
	m->AddPointer
	(
		LMessage::PropCallback,
		new LMessage::InThreadCb
		(
			[dlg=this, cb=d->ModalCb, code=Code]()
			{
				// printf("%s:%i - Calling LDialog callback.. in original thread\n", _FL);
				LAutoPtr<LDialog> owner(dlg);
				cb(owner, code);
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

