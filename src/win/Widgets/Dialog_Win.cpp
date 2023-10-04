/*hdr
**      FILE:           GWidgets.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Dialog components
**
**      Copyright (C) 1998-2001 Matthew Allen
**              fret@memecode.com
*/

#include <stdlib.h>
#include <stdio.h>
#include "lgi/common/Lgi.h"
#include <commctrl.h>
#include "lgi/common/TableLayout.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Button.h"
#include "lgi/common/LgiRes.h"

struct LDialogPriv
{
	bool IsModal = false, IsModeless = false, _Resizable = true;
	int ModalStatus = -1;
	int BtnId = -1;
    int ModalResult = -1;

	// Modal state
	OsView ParentHnd = NULL;
	LWindow *ParentWnd = NULL;
	bool ParentWndHasThis = false;
	LDialog::OnClose Callback;
};

///////////////////////////////////////////////////////////////////////////////////////////
LDialog::LDialog(LViewI *parent)
	: ResObject(Res_Dialog)
{
	d = new LDialogPriv;
	_Window = this;
	Name("Dialog");
	if (parent)
		SetParent(parent);

	SetStyle(GetStyle() & ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
	SetStyle(GetStyle() | WS_DLGFRAME);
}

LDialog::~LDialog()
{
	LAssert(!d->ParentWndHasThis);
	LAssert(!d->IsModal && !d->IsModeless); // Can't delete while still active...
    DeleteObj(d);
}

bool LDialog::LoadFromResource(int Resource, const char *TagList)
{
	LString n;
	bool Status = LResourceLoad::LoadFromResource(Resource, this, &Pos, &n, TagList);
	if (Status && n)
		Name(n);
	return Status;
}

LRESULT CALLBACK DlgRedir(HWND hWnd, UINT m, WPARAM a, LPARAM b)
{
	if (m == WM_INITDIALOG)
	{
		LDialog *NewWnd = (LDialog*) b;
		NewWnd->_View = hWnd;
		#if _MSC_VER >= _MSC_VER_VS2005
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)(LViewI*)NewWnd);
		#else
        SetWindowLong(hWnd, GWL_USERDATA, (LONG)(LViewI*)NewWnd);
		#endif
	}

	LViewI *Wnd = (LViewI*)
		#if _MSC_VER >= _MSC_VER_VS2005
		#pragma warning(disable : 4312)
		GetWindowLongPtr(hWnd, GWLP_USERDATA);
		#pragma warning(default : 4312)
		#else
		GetWindowLong(hWnd, GWL_USERDATA);
		#endif
	if (Wnd)
	{
		LMessage Msg(m, a, b);
		return Wnd->OnEvent(&Msg);
	}

	return 0;
}

bool LDialog::OnRequestClose(bool OsClose)
{
	return true;
}

bool LDialog::IsModal()
{
	return d->IsModal;
}

void LDialog::DoModal(OnClose Callback, OsView ParentHnd)
{
	LAssert(!d->IsModeless);
	d->IsModal = true;
	d->Callback = Callback;
	
    LViewI *p = GetParent();
    if (p && p->GetWindow() != p)
        p = p->GetWindow();
    
	if (Attach(p))
	{
	    AttachChildren();
	    
	    d->ParentWnd = dynamic_cast<LWindow*>(p);
	    if (d->ParentWnd)
		{
			d->ParentWndHasThis = true;
	        d->ParentWnd->_Dialog = this;
		}

	    if (p)
	    {
	        LRect pp = p->GetPos();
			if (pp.Valid())
			{
				int cx = pp.x1 + (pp.X() >> 1);
				int cy = pp.y1 + (pp.Y() >> 1);
	        
				LRect np = GetPos();
				np.Offset(  cx - (np.X() >> 1) - np.x1,
							cy - (np.Y() >> 1) - np.y1);
	        
				SetPos(np);
				MoveOnScreen();
			}
	    }
	    
	    Visible(true);
	    
	    d->ParentHnd = ParentHnd ? ParentHnd : (p ? p->Handle() : NULL);
	    if (d->ParentHnd)
			EnableWindow(d->ParentHnd, false);
	}
	else
	{
	    LAssert(!"Attach failed.");
	}
}

void LDialog::EndModal(int Code)
{
	if (!d->IsModal)
	{
		LAssert(!"Not a modal dialog.");
		return;
	}

	// This is so the calling code can unwind all it's stack frames without
	// worrying about accessing things that have been deleted.
	PostEvent(M_DIALOG_END_MODAL, (LMessage::Param)Code);
}

static char *BaseStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int LDialog::DoModeless()
{
	int Status = -1;

	LAssert(!_View);
	if (_View)
		return Status;

	LAssert(!d->IsModal);
	d->IsModeless = true;

    LViewI *p = GetParent();
    if (p && p->GetWindow() != p)
        p = p->GetWindow();
    
	if (Attach(0))
	{
	    AttachChildren();
	    
		if (p && Handle() && p->Handle())
		{
			#ifdef _WIN64
			SetWindowLongPtr(Handle(), GWLP_HWNDPARENT, (LONG_PTR)p->Handle());
			#else
			SetWindowLong(Handle(), GWL_HWNDPARENT, (LONG)p->Handle());
			#endif
		}

	    if (p)
	    {
	        LRect pp = p->GetPos();
	        int cx = pp.x1 + (pp.X() >> 1);
	        int cy = pp.y1 + (pp.Y() >> 1);
	        
	        LRect np = GetPos();
	        np.Offset(  cx - (np.X() >> 1) - np.x1,
	                    cy - (np.Y() >> 1) - np.y1);
	        
	        SetPos(np);
	        MoveOnScreen();
	    }
	    
	    Visible(true);
        Status = true;
	}
	else
	{
	    LAssert(!"Attach failed.");
	}

	return Status;
}

LMessage::Result LDialog::OnEvent(LMessage *Msg)
{
	switch (Msg->m)
	{
		case WM_CREATE:
		{
			LRect r = Pos;
			Pos.ZOff(-1, -1);
			SetPos(r);	// resets the dialog to the correct
						// size when large fonts are used

			if (GetAlwaysOnTop())
				SetAlwaysOnTop(true);

			AttachChildren();
			if (!_Default)
				SetDefault(FindControl(IDOK));

			LResources::StyleElement(this);			

			// This was commented out. I've re-introduced it until such time
			// as there is a good reason not to have it enabled. If such a reason
			// arises, update this comment to reflect that.
			OnCreate();

			// If we don't return true here the LWindow::OnEvent handler for
			// WM_CREATE will call OnCreate again.
			return true;
		}
		case M_DIALOG_END_MODAL:
		{
			// See ::EndModal for comment on why this is here.
			d->ModalResult = max((int)Msg->A(), 0);

			if (d->ParentWnd)
			{
				d->ParentWnd->_Dialog = NULL;
				d->ParentWndHasThis = false;
				EnableWindow(d->ParentHnd, true);
			}
	    
			Visible(false);

			d->IsModal = false;

			if (d->Callback)
				d->Callback(this, d->ModalResult);
			else
				delete this; // default action is to delete the dialog
			return 1;
		}
	}

    return LWindow::OnEvent(Msg);
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
			EndModal(d->BtnId);
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
    if (Children.Length() == 1)
    {
        List<LViewI>::I it = Children.begin();
        LLayout *t = dynamic_cast<LLayout*>((LViewI*)it);
        if (t)
        {
            LRect r = GetClient();
            r.Inset(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
            t->SetPos(r);
        }
    }
}

void LDialog::EndModeless(int Code)
{
	if (d->IsModeless)
	{
		d->IsModeless = false;
		Quit(Code != 0);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
LControl::LControl(const char *SubClassName) : LView(0)
{
	if (SubClassName)
	{
		SetClassW32(SubClassName);
		SubClass = LWindowsClass::Create(SubClassName); // Owned by the LWindowsClass object
	}
	Pos.ZOff(10, 10);
}

LControl::~LControl()
{
	if (SetOnDelete)
		*SetOnDelete = true;
}

LMessage::Result LControl::OnEvent(LMessage *Msg)
{
	LMessage::Result Status = 0;
	
	// Pre-OS event handler
	switch (Msg->m)
	{
		case WM_CREATE:
		{
			SetId(GetId());
			OnCreate();
			break;
		}
		case WM_SETTEXT:
		{
			if (IsWindowUnicode(_View))
				LBase::NameW((char16*)Msg->b);
			else
				LBase::Name((char*)Msg->b);
			break;
		}
		case WM_GETDLGCODE:
		case WM_NOTIFY:
		case WM_COMMAND:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			bool Deleted = false;
			SetOnDelete = &Deleted;
			Status = LView::OnEvent(Msg);
			if (Deleted)
				return Status;
			
			SetOnDelete = NULL;
			break;
		}
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
		{
			// These should never be called.. but just in case.
			return LView::OnEvent(Msg);
			break;
		}
		case WM_NCDESTROY:
		{
			Status = LView::OnEvent(Msg);
			break;
		}
	}
	
	// OS event handler
	if (SubClass)
		Status = SubClass->CallParent(Handle(), Msg->m, Msg->a, Msg->b);

	return Status;
}

LPoint LControl::SizeOfStr(const char *Str)
{
	LPoint Pt(0, 0);
	if (Str)
	{
		for (const char *s=Str; s && *s; )
		{
			const char *e = strchr(s, '\n');
			if (!e) e = s + strlen(s);

			LDisplayString ds(LSysFont, (char*)s, e - s);

			Pt.y += ds.Y();
			Pt.x = max(Pt.x, ds.X());

			s = (*e=='\n') ? e + 1 : 0;
		}
	}

	return Pt;
}

