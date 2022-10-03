#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/Button.h"

#if defined(HAIKU)
#include <Alert.h>
#endif

#if defined(__GTK_H__)
#include "gtk/gtkdialog.h"

void MsgCb(Gtk::GtkDialog *dialog, Gtk::gint response_id, Gtk::gpointer user_data)
{
	*((int*)user_data) = response_id;
}

#endif

#if defined(__GTK_H__) || defined(MAC) || defined(LGI_SDL) || defined(HAIKU)
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"

#if LGI_COCOA
#import <Cocoa/Cocoa.h>
#endif

class LMsgDlg : public LDialog
{
public:
	LMsgDlg()
	{
		RegisterHook(this, LKeyEvents);
	}

	bool OnViewKey(LView *v, LKey &k)
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
				LViewI *c = FindControl(Id);
				if (c)
				{
					EndModal(c->GetId());
				}
			}
		}
		
		return LDialog::OnViewKey(v, k);
	}
	
	int OnNotify(LViewI *Ctrl, LNotification n)
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

int LgiMsg(LViewI *Parent, const char *Str, const char *Title, int Type, ...)
{
	int Res = 0;
	va_list Arg;
	va_start(Arg, Type);
	LString Msg;
	Msg.Printf(Arg, Str);
	va_end(Arg);

	#if WINNATIVE

		if (Str)
		{
			if (LGetOs() == LGI_OS_WIN9X)
			{
				auto t = LToNativeCp(Title ? Title : (char*)"Message");
				auto m = LToNativeCp(Msg);
				Res = MessageBoxA(Parent ? Parent->Handle() : 0, m?m:"", t?t:"", Type);
				if (Res == 0)
				{
					auto Err = GetLastError();
					LAssert(!"MessageBoxA failed.");
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
					LAssert(!"MessageBoxW failed.");
				}
				DeleteArray(t);
				DeleteArray(m);
			}
		}

	#elif defined(HAIKU)

		int32 result = 0;
		BAlert *a = NULL;
		switch (Type)
		{
			default:
			case MB_OK:
				a = new BAlert(Title, Msg, "Ok", NULL, NULL);
				break;
			case MB_OKCANCEL:
				a = new BAlert(Title, Msg, "Ok", "Cancel", NULL);
				break;
			case MB_YESNO:
				a = new BAlert(Title, Msg, "Yes", "No", NULL);
				break;
			case MB_YESNOCANCEL:
				a = new BAlert(Title, Msg, "Yes", "No", "Cancel");
				break;
		}
		if (a)
		{
			printf("DoModal(%s,%s)\n", Title, Msg.Get());
			result = a->Go();
			printf("result=%i\n", result);
		}

		if (result < 0)
			return IDCANCEL;

		switch (Type)
		{
			default:
			case MB_OK:
				return IDOK;
			case MB_OKCANCEL:
				return result ? IDCANCEL : IDOK;
			case MB_YESNO:
				return result ? IDNO : IDYES;
			case MB_YESNOCANCEL:
				if (result == 0) return IDYES;
				if (result == 1) return IDNO;
				return IDCANCEL;
		}
	
	#elif LGI_COCOA
	
		NSAlert *alert = [[NSAlert alloc] init];
		auto msg = Msg.NsStr();
		auto title = LString(Title).NsStr();
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
					LAssert(0);
				break;
			}
			case MB_YESNO:
			{
				if (r == NSAlertFirstButtonReturn)
					return IDNO;
				else if (r == NSAlertSecondButtonReturn)
					return IDYES;
				else
					LAssert(0);
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
					LAssert(0);
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

		if (Str && LAppInst)
		{
			LMsgDlg Dlg;
			Dlg.SetParent(Parent);
			Dlg.Name((char*)(Title ? Title : "Message"));

			LTextLabel *Text = new LTextLabel(-1, 10, 10, -1, -1, Msg);
			Dlg.AddView(Text);

			List<LButton> Btns;
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
					Btns.Insert(new LButton(IDOK, 10, 40, BtnX, BtnY, "Ok"));
					break;
				}
				case MB_OKCANCEL:
				{
					Btns.Insert(new LButton(IDOK, 10, 40, BtnX, BtnY, "Ok"));
					Btns.Insert(new LButton(IDCANCEL, 10, 40, BtnX, BtnY, "Cancel"));
					break;
				}
				case MB_YESNO:
				{
					Btns.Insert(new LButton(IDYES, 10, 40, BtnX, BtnY, "Yes"));
					Btns.Insert(new LButton(IDNO, 10, 40, BtnX, BtnY, "No"));
					break;
				}
				case MB_YESNOCANCEL:
				{
					Btns.Insert(new LButton(IDYES, 10, 40, BtnX, BtnY, "Yes"));
					Btns.Insert(new LButton(IDNO, 10, 40, BtnX, BtnY, "No"));
					Btns.Insert(new LButton(IDCANCEL, 10, 40, BtnX, BtnY, "Cancel"));
					break;
				}
			}

			int BtnsX = (int) ((Btns.Length() * BtnX) + ((Btns.Length()-1) * 10));
			int MaxX = MAX(BtnsX, Text->X());
			LRect p(0, 0, MaxX + 30, Text->Y() + 30 + BtnY + LAppInst->GetMetric(LGI_MET_DECOR_Y) );
			Dlg.SetPos(p);
			Dlg.MoveToCenter();

			int x = (p.X() - BtnsX) / 2;
			int y = Text->Y() + 20;
			for (auto b: Btns)
			{
				LRect r(x, y, x+BtnX-1, y+BtnY-1);
				b->SetPos(r);
				Dlg.AddView(b);
				x += r.X() + 10;
			}
			
			return Dlg.DoModal();
		}
	
	#endif

	return Res;
}

#if !LGI_STATIC
void LDialogTextMsg(LViewI *Parent, const char *Title, LString Txt)
{
	LAutoPtr<LDialog> d(new LDialog);
	if (d)
	{
		LTextLog *Log = NULL;
		d->SetParent(Parent);
		d->Name(Title);
		LRect r(0, 0, 600, 500);
		d->SetPos(r);
		d->MoveSameScreen(Parent);

		LTableLayout *t = new LTableLayout(100);
		auto c = t->GetCell(0, 0);
		c->Add(Log = new LTextLog(101));
		Log->Name(Txt);
		c = t->GetCell(0, 1);
		c->Add(new LButton(IDOK, 0, 0, -1, -1, "Ok"));
		c->TextAlign(LCss::AlignCenter);
		
		d->AddView(t);
		d->DoModal();
	}
}

#endif
