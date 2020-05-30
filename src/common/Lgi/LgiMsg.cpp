#include <stdio.h>
#include "Lgi.h"
#include "GTextLog.h"
#include "GTableLayout.h"
#include "GButton.h"

#if defined(__GTK_H__)
#include "gtk/gtkdialog.h"

void MsgCb(Gtk::GtkDialog *dialog, Gtk::gint response_id, Gtk::gpointer user_data)
{
	*((int*)user_data) = response_id;
}

#endif

#if defined(__GTK_H__) || defined(MAC) || defined(LGI_SDL)
#include "GTextLabel.h"
#include "GButton.h"

#if LGI_COCOA
#import <Cocoa/Cocoa.h>
#endif

class GMsgDlg : public GDialog
{
public:
	GMsgDlg()
	{
		RegisterHook(this, GKeyEvents);
	}

	bool OnViewKey(GView *v, GKey &k)
	{
		if (k.Down())
		{
			int Id = -1;
			switch (k.c16)
			{
				case 'y':
				case 'Y':
				{
					Id = IDYES;
					break;
				}
				case 'n':
				case 'N':
				{
					Id = IDNO;
					break;
				}
				case 'c':
				case 'C':
				{
					Id = IDCANCEL;
					break;
				}
				case 'o':
				case 'O':
				{
					Id = IDOK;
					break;
				}
			}
			
			if (Id >= 0)
			{
				GViewI *c = FindControl(Id);
				if (c)
				{
					EndModal(c->GetId());
				}
			}
		}
		
		return GDialog::OnViewKey(v, k);
	}
	
	int OnNotify(GViewI *Ctrl, int f)
	{
		switch (Ctrl->GetId())
		{
			case IDOK:
			case IDCANCEL:
			case IDYES:
			case IDNO:
			{
				EndModal(Ctrl->GetId());
				break;
			}
		}
		return 0;
	}
};
#endif

int LgiMsg(GViewI *Parent, const char *Str, const char *Title, int Type, ...)
{
	int Res = 0;
	va_list Arg;
	va_start(Arg, Type);
	GString Msg;
	Msg.Printf(Arg, Str);
	va_end(Arg);

	#if WINNATIVE

	if (Str)
	{
		if (LgiGetOs() == LGI_OS_WIN9X)
		{
			auto t = LToNativeCp(Title ? Title : (char*)"Message");
			auto m = LToNativeCp(Msg);
			Res = MessageBoxA(Parent ? Parent->Handle() : 0, m?m:"", t?t:"", Type);
			if (Res == 0)
			{
				auto Err = GetLastError();
				LgiAssert(!"MessageBoxA failed.");
			}
		}
		else
		{
			char16 *t = Utf8ToWide(Title ? Title : (char*)"Message");
			char16 *m = Utf8ToWide(Msg);
			Res = MessageBoxW(Parent ? Parent->Handle() : 0, m?m:L"", t?t:L"", Type);
			if (Res == 0)
			{
				auto Err = GetLastError();
				LgiAssert(!"MessageBoxW failed.");
			}
			DeleteArray(t);
			DeleteArray(m);
		}
	}
	
	#elif LGI_COCOA
	
	NSAlert *alert = [[NSAlert alloc] init];
	auto msg = Msg.NsStr();
	auto title = GString(Title).NsStr();
	[alert setMessageText:msg];
	[alert setInformativeText:title];
	switch (Type & ~MB_SYSTEMMODAL)
	{
		default:
		case MB_OK:
		{
			[alert addButtonWithTitle:@"Ok"];
			break;
		}
		case MB_OKCANCEL:
		{
			[alert addButtonWithTitle:@"Cancel"];
			[alert addButtonWithTitle:@"Ok"];
			break;
		}
		case MB_YESNO:
		{
			[alert addButtonWithTitle:@"No"];
			[alert addButtonWithTitle:@"Yes"];
			break;
		}
		case MB_YESNOCANCEL:
		{
			[alert addButtonWithTitle:@"Cancel"];
			[alert addButtonWithTitle:@"No"];
			[alert addButtonWithTitle:@"Yes"];
			break;
		}
	}
	auto r = [alert runModal];
	[msg release];
	[title release];
	
	Res = IDOK;
	switch (Type & ~MB_SYSTEMMODAL)
	{
		default:
		case MB_OK:
			break;
		case MB_OKCANCEL:
		{
			if (r == NSAlertFirstButtonReturn)
				return IDCANCEL;
			else if (r == NSAlertSecondButtonReturn)
				return IDOK;
			else
				LgiAssert(0);
			break;
		}
		case MB_YESNO:
		{
			if (r == NSAlertFirstButtonReturn)
				return IDNO;
			else if (r == NSAlertSecondButtonReturn)
				return IDYES;
			else
				LgiAssert(0);
			break;
		}
		case MB_YESNOCANCEL:
		{
			if (r == NSAlertFirstButtonReturn)
				return IDCANCEL;
			else if (r == NSAlertSecondButtonReturn)
				return IDNO;
			else if (r == NSAlertThirdButtonReturn)
				return IDYES;
			else
				LgiAssert(0);
			break;
		}
	}

	#elif defined(__GTK_H__)

	using namespace Gtk;

	GtkButtonsType GtkType;
	switch (Type & ~MB_SYSTEMMODAL)
	{
		default:
		case MB_OK:
			GtkType = GTK_BUTTONS_OK;
			break;
		case MB_OKCANCEL:
			GtkType = GTK_BUTTONS_OK_CANCEL;
			break;
		case MB_YESNO:
		case MB_YESNOCANCEL: // ugh, soz
			GtkType = GTK_BUTTONS_YES_NO;
			break;
	}
	GtkWidget *dlg = gtk_message_dialog_new(
		Parent ? Parent->WindowHandle() : NULL,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_INFO,
		GtkType,
		"%s",
		Msg.Get());
	if (dlg)
	{
		if (Type == MB_YESNOCANCEL)
			gtk_dialog_add_button(GTK_DIALOG(dlg), "Cancel", GTK_RESPONSE_CANCEL);
		int Response = 0;
		g_signal_connect(GTK_DIALOG(dlg), "response", G_CALLBACK(MsgCb), &Response);
		gtk_dialog_run(GTK_DIALOG(dlg));
		gtk_widget_destroy(dlg);
	
		switch (Response)
		{
			case GTK_RESPONSE_NONE:
			case GTK_RESPONSE_REJECT:
			case GTK_RESPONSE_ACCEPT:
			case GTK_RESPONSE_DELETE_EVENT:
			case GTK_RESPONSE_CLOSE:
			case GTK_RESPONSE_APPLY:
			case GTK_RESPONSE_HELP:
				break;
			case GTK_RESPONSE_OK:
				Res = IDOK;
				break;
			case GTK_RESPONSE_CANCEL:
				Res = IDCANCEL;
				break;
			case GTK_RESPONSE_YES:
				Res = IDYES;
				break;
			case GTK_RESPONSE_NO:
				Res = IDNO;
				break;
		}
	}
	
	#else // Lgi only controls (used for Linux + Mac)

	if (Str && LgiApp)
	{
		GMsgDlg Dlg;
		Dlg.SetParent(Parent);
		Dlg.Name((char*)(Title ? Title : "Message"));

		GTextLabel *Text = new GTextLabel(-1, 10, 10, -1, -1, Msg);
		Dlg.AddView(Text);

		List<GButton> Btns;
		#ifdef LGI_TOUCHSCREEN
		float Scale = 1.6f;
		#else
		float Scale = 1.0f;
		#endif		
		int BtnY = (int) (Scale * 20.0f);
		int BtnX = (int) (Scale * 70.0f);

		switch (Type & ~MB_SYSTEMMODAL)
		{
			default:
			case MB_OK:
			{
				Btns.Insert(new GButton(IDOK, 10, 40, BtnX, BtnY, "Ok"));
				break;
			}
			case MB_OKCANCEL:
			{
				Btns.Insert(new GButton(IDOK, 10, 40, BtnX, BtnY, "Ok"));
				Btns.Insert(new GButton(IDCANCEL, 10, 40, BtnX, BtnY, "Cancel"));
				break;
			}
			case MB_YESNO:
			{
				Btns.Insert(new GButton(IDYES, 10, 40, BtnX, BtnY, "Yes"));
				Btns.Insert(new GButton(IDNO, 10, 40, BtnX, BtnY, "No"));
				break;
			}
			case MB_YESNOCANCEL:
			{
				Btns.Insert(new GButton(IDYES, 10, 40, BtnX, BtnY, "Yes"));
				Btns.Insert(new GButton(IDNO, 10, 40, BtnX, BtnY, "No"));
				Btns.Insert(new GButton(IDCANCEL, 10, 40, BtnX, BtnY, "Cancel"));
				break;
			}
		}

		int BtnsX = (int) ((Btns.Length() * BtnX) + ((Btns.Length()-1) * 10));
		int MaxX = MAX(BtnsX, Text->X());
		GRect p(0, 0, MaxX + 30, Text->Y() + 30 + BtnY + LgiApp->GetMetric(LGI_MET_DECOR_Y) );
		Dlg.SetPos(p);
		Dlg.MoveToCenter();

		int x = (p.X() - BtnsX) / 2;
		int y = Text->Y() + 20;
		for (auto b: Btns)
		{
			GRect r(x, y, x+BtnX-1, y+BtnY-1);
			b->SetPos(r);
			Dlg.AddView(b);
			x += r.X() + 10;
		}
		
		return Dlg.DoModal();
	}
	
	#endif

	#if 0 // defined MAC (doesn't work reliably)

	if (Str)
	{
		SInt16 r;
		Str255 t;
		Str255 s;
		Str63 sYes = {3, 'Y', 'e', 's'}, sNo = {2, 'N', 'o'};
		AlertStdAlertParamRec p;

		c2p255(t, Title);
		c2p255(s, Buffer);
		p.movable = true;
		p.helpButton = false;
		p.filterProc = 0;
		p.defaultText = (ConstStringPtr)kAlertDefaultOtherText;
		p.cancelText = 0;
		p.otherText = 0;
		p.position = kWindowDefaultPosition;

		switch (Type)
		{
			default:
				printf("%s:%i - unhandled LgiMsg type %i\n", __FILE__, __LINE__, Type);
			case MB_OK:
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertOtherButton;
				break;
			case MB_OKCANCEL:
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertCancelButton;
				p.cancelText = (ConstStringPtr)kAlertDefaultCancelText;
				break;
			case MB_YESNO:
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertOtherButton;
				/*
				p.defaultButton = kAlertStdAlertOKButton;
				p.cancelButton = kAlertStdAlertCancelButton;
				p.defaultText = sYes;
				p.cancelText = sNo;
				*/
				break;
			case MB_YESNOCANCEL:
				p.defaultButton = kAlertStdAlertOKButton;
				p.defaultText = sYes;
				p.cancelButton = kAlertStdAlertCancelButton;
				p.cancelText = (ConstStringPtr)kAlertDefaultCancelText;
				p.otherText = sNo;
				break;
		}
		
		OSErr e = StandardAlert(kAlertPlainAlert, t, s, &p, &r);
		
		switch (Type)
		{
			default:
			case MB_OK:
				Res = IDOK;
				break;
			case MB_OKCANCEL:
				Res = (r == kAlertStdAlertOKButton) ? IDOK : IDCANCEL;
				break;
			case MB_YESNO:
				Res = (r == kAlertStdAlertOKButton) ? IDYES : IDNO;
				break;
			case MB_YESNOCANCEL:
				if (r == kAlertStdAlertOKButton)
					Res = IDYES;
				else if (r == kAlertStdAlertOtherButton)
					Res = IDNO;
				else
					Res = IDCANCEL;					
				break;
		}
	}

	#endif

	return Res;
}

#if !LGI_STATIC
void LDialogTextMsg(GViewI *Parent, const char *Title, GString Txt)
{
	GAutoPtr<GDialog> d(new GDialog);
	if (d)
	{
		GTextLog *Log = NULL;
		d->SetParent(Parent);
		d->Name(Title);
		GRect r(0, 0, 600, 500);
		d->SetPos(r);
		d->MoveSameScreen(Parent);

		GTableLayout *t = new GTableLayout(100);
		auto c = t->GetCell(0, 0);
		c->Add(Log = new GTextLog(101));
		Log->Name(Txt);
		c = t->GetCell(0, 1);
		c->Add(new GButton(IDOK, 0, 0, -1, -1, "Ok"));
		c->TextAlign(GCss::AlignCenter);
		
		d->AddView(t);
		d->DoModal();
	}
}

#endif
