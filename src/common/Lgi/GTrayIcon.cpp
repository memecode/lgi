#include "Lgi.h"

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#if LGI_COCOA

	#import <AppKit/AppKit.h>

	@interface LStatusItem : NSObject
	{
	}
	@property GWindow *parent;
	- (id)init:(GWindow*)p;
	- (void)onClick;
	@end

#elif defined __GTK_H__

	namespace Gtk {
		#include <glib.h>
		#include <gdk-pixbuf/gdk-pixbuf.h>
	}
	using namespace Gtk;

	class GTrayIconPrivate;
	static void tray_icon_on_click(GtkStatusIcon *status_icon, GTrayIconPrivate *d);
	static void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, GTrayIconPrivate *d);

#endif

////////////////////////////////////////////////////////////////////////////
class GTrayIconPrivate
{
public:
	GWindow *Parent;	// parent window
	int64 Val;			// which icon is currently visible
	bool Visible;

	#if WINNATIVE
	
		int TrayCreateMsg;
		int MyId;
		NOTIFYICONDATAW	TrayIcon;
		typedef HICON IconRef;
		::GArray<IconRef> Icon;
	
	#elif LGI_COCOA
	
		typedef GSurface *IconRef;
		NSStatusItem *StatusItem;
		GArray<NSImage*> Icon;
		LStatusItem *Handler;

	#elif defined(__GTK_H__)
	
		::GArray<GSurface*> Images;
		GlibWrapper<GtkStatusIcon> tray_icon;
		typedef GdkPixbuf *IconRef;
		::GArray<IconRef> Icon;
		uint64 LastClickTime;
		gint DoubleClickTime;
	
		void OnClick()
		{
			if (!tray_icon)
				return;

			uint64 Now = LgiCurrentTime();
			GdkScreen *s = gtk_status_icon_get_screen(tray_icon);
			GdkDisplay *dsp = gdk_screen_get_display(s);
			gint x, y;
			GdkModifierType mask;
			gdk_display_get_pointer(dsp, &s, &x, &y, &mask);
			
			GMouse m;
			m.x = x;
			m.y = y;
			m.SetModifer(mask);
			m.Left(true);
			m.Down(true);
			m.Double(Now - LastClickTime < DoubleClickTime);
			Parent->OnTrayClick(m);
			LastClickTime = Now;
		}
	
		void OnMenu(guint button, guint activate_time)
		{
			GMouse m;
			m.Left(button == 1);
			m.Middle(button == 2);
			m.Right(button == 3);
			m.Down(true);
			
			Parent->OnTrayClick(m);
		}

	#else

		typedef GSurface *IconRef;
		::GArray<IconRef> Icon;

	#endif


	GTrayIconPrivate(GWindow *p)
	{
		Parent = p;
		Val = 0;
		Visible = false;
		
		#if WINNATIVE
		
			ZeroObj(TrayIcon);
			MyId = 0;
			TrayCreateMsg = RegisterWindowMessage(TEXT("TaskbarCreated"));
		
		#elif LGI_COCOA
		
			Handler = NULL;
			StatusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:24];
			if (StatusItem)
			{
				StatusItem.visible = false;
				Handler = [[LStatusItem alloc] init:p];
				if (Handler)
				{
					StatusItem.target = Handler;
					StatusItem.action = @selector(onClick);
				}
			}
		
		#elif defined(__GTK_H__)

			GtkSettings *settings = gtk_settings_get_default();
			DoubleClickTime = 500;
			if (settings)
				g_object_get(G_OBJECT(settings), "gtk-double-click-time", &DoubleClickTime, NULL);

			LastClickTime = 0;
			tray_icon = Gtk::gtk_status_icon_new();
			if (tray_icon)
			{
				tray_icon.Connect("activate", G_CALLBACK(tray_icon_on_click), this);
				tray_icon.Connect("popup-menu", G_CALLBACK(tray_icon_on_menu), this);
			}
		
		#endif
	}	
	
	~GTrayIconPrivate()
	{
		#if WINNATIVE
		
			for (int n=0; n<Icon.Length(); n++)
				DeleteObject(Icon[n]);
		
		#elif LGI_COCOA
		
			[[NSStatusBar systemStatusBar] removeStatusItem:StatusItem];
			for (auto i: Icon)
				[i release];
		
		#elif defined(__GTK_H__)
		
			for (int n=0; n<Icon.Length(); n++)
				g_object_unref(Icon[n]);
			Images.DeleteObjects();
		
		#else
		
			Icon.DeleteObjects();
		
		#endif
	}

};

#if defined(__GTK_H__)

	void tray_icon_on_click(GtkStatusIcon *status_icon, GTrayIconPrivate *d)
	{
		d->OnClick();
	}

	void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, GTrayIconPrivate *d)
	{
		d->OnMenu(button, activate_time);
	}

#elif LGI_COCOA

	@implementation LStatusItem

	- (id)init:(GWindow*)p
	{
		if ((self = [super init]) != nil)
		{
			self.parent = p;
		}
		
		return self;
	}

	- (void)onClick
	{
		GMouse m;
		m.Left(true);
		m.Down(true);
		self.parent->OnTrayClick(m);
	}

	@end

#endif

////////////////////////////////////////////////////////////////////////////
GTrayIcon::GTrayIcon(GWindow *p)
{
	d = new GTrayIconPrivate(p);
}

GTrayIcon::~GTrayIcon()
{
	Visible(false);
	DeleteObj(d);
}

bool GTrayIcon::Load(const TCHAR *Str)
{
	#if WINNATIVE
	
		HICON i = ::LoadIcon(LgiProcessInst(), Str);
		if (i)
		{
			d->Icon.Add(i);
			return true;
		}
		else LgiAssert(0);
	
	#elif LGI_COCOA
	
		GString File = LFindFile(Str);
		if (!File)
			return false;
		NSString *nf = File.NsStr();
		NSImage *img = [[NSImage alloc] initWithContentsOfFile:nf];
		if (img)
			d->Icon.Add(img);
		[nf release];
		return img != NULL;
	
	#elif defined(__GTK_H__)

		if (!Str)
			return false;

		::GString sStr = Str;
		GAutoString File(LgiFindFile(sStr));
		if (!File)
		{
			LgiTrace("%s:%i - Can't find '%s'\n", _FL, sStr.Get());
			return false;
		}

		GAutoPtr<GSurface> Ico(GdcD->Load(File));
		if (!Ico)
		{
			LgiTrace("%s:%i - Failed to load '%s'\n", _FL, sStr.Get());
			return false;
		}

		Gtk::GdkPixbuf *Pb = Ico->CreatePixBuf();
		if (!Pb)
		{
			LgiTrace("%s:%i - Failed to CreatePixBuf '%s'\n", _FL, sStr.Get());
			return false;
		}

		d->Icon.Add(Pb);
		d->Images.Add(Ico.Release());
	
	#else
	
		GAutoString File(LgiFindFile(Str));
		if (File)
		{
			GSurface *i = GdcD->Load(File);
			if (i)
			{
				if (GdcD->GetBits() != i->GetBits())
				{
					GSurface *n = new GMemDC(i->X(), i->Y(), GdcD->GetColourSpace());
					if (n)
					{
						n->Colour(0);
						n->Rectangle();
						n->Blt(0, 0, i);
						DeleteObj(i);
						i = n;
					}
				}

				d->Icon.Add(i);
			}
			else LgiTrace("%s:%i - Couldn't load '%s'\n", _FL, Str);

			return i != 0;
		}
		else LgiTrace("%s:%i - Couldn't find '%s'\n", _FL, Str);
	
	#endif

	return false;
}

bool GTrayIcon::Visible()
{
	#if WINNATIVE
		return d->TrayIcon.cbSize != 0;
	#elif LGI_COCOA
		if (d->StatusItem)
			d->Visible = [d->StatusItem isVisible];
		return d->Visible;
	#else
		return d->Visible;
	#endif
}

void GTrayIcon::Visible(bool v)
{
	if (Visible() != v)
	{
		if (v)
		{
			#if WINNATIVE
			
				static int Id = 1;
				HICON Cur = d->Icon[d->Val];

				ZeroObj(d->TrayIcon);
			
				d->TrayIcon.cbSize = sizeof(d->TrayIcon);
				d->TrayIcon.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.uID = d->MyId = Id++;
				d->TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.hIcon = Cur;
				StrncpyW(d->TrayIcon.szTip, (char16*)(NameW()?NameW():L""), CountOf(d->TrayIcon.szTip));

				if (!Shell_NotifyIconW(NIM_ADD, &d->TrayIcon))
				{
					int asd=0;
				}
			
			#elif defined(__GTK_H__)

				if (d->tray_icon)
				{
					if (d->Val < 0 || d->Val >= d->Icon.Length())
						d->Val = 0;
					if (d->Val < d->Icon.Length())
					{
						GTrayIconPrivate::IconRef Ref = d->Icon[d->Val];
						if (Ref)
						{
							Gtk::gtk_status_icon_set_from_pixbuf(d->tray_icon, Ref);
							Gtk::gtk_status_icon_set_tooltip_text(d->tray_icon, GBase::Name());
							Gtk::gtk_status_icon_set_visible(d->tray_icon, true);
						}
					}
					else LgiTrace("%s:%i - No icon to show in tray.\n", _FL);
				}
				else LgiTrace("%s:%i - No tray icon to hide.\n", _FL);
			
			#elif LGI_COCOA
			
				if (d->StatusItem)
					d->StatusItem.visible = true;
			
			#elif defined MAC

				if (!d->Visible)
				{
					int64 Ico = d->Val;
					d->Val = -1;
					Value(Ico);
				}
			
			#endif
		}
		else
		{
			#if WINNATIVE
			
				if (!Shell_NotifyIconW(NIM_DELETE, &d->TrayIcon))
				{
					int asd=0;
				}
				ZeroObj(d->TrayIcon);

			#elif LGI_COCOA

			#elif LGI_CARBON
			
				RestoreApplicationDockTileImage();

			#elif defined(__GTK_H__)

				if (d->tray_icon)
					gtk_status_icon_set_visible(d->tray_icon, false);
				else
					LgiTrace("%s:%i - No tray icon to hide.\n", _FL);

			#endif
		}
	}

	d->Visible = v;
}

int64 GTrayIcon::Value()
{
	return d->Val;
}

void GTrayIcon::Value(int64 v)
{
	if (d->Val != v)
	{
		d->Val = v;
		
		#if WINNATIVE
		
			if (Visible())
			{
				HICON Cur = d->Icon[d->Val];

				ZeroObj(d->TrayIcon);

				d->TrayIcon.cbSize = sizeof(d->TrayIcon);
				d->TrayIcon.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.uID = d->MyId;
				d->TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.hIcon = Cur;
				StrncpyW(d->TrayIcon.szTip, (char16*)(NameW()?NameW():L""), sizeof(d->TrayIcon.szTip));
				if (!Shell_NotifyIconW(NIM_MODIFY, &d->TrayIcon))
				{
					int asd=0;
				}
			}
		
		#elif defined __GTK_H__

			if (d->tray_icon)
			{
				if (d->Val < 0 || d->Val >= d->Icon.Length())
					d->Val = 0;
				if (d->Val < d->Icon.Length())
				{
					GTrayIconPrivate::IconRef Ref = d->Icon[d->Val];
					if (Ref)
						Gtk::gtk_status_icon_set_from_pixbuf(d->tray_icon, Ref);
				}
				else LgiTrace("%s:%i - No icon to show in tray.\n", _FL);
			}
			else LgiTrace("%s:%i - No tray icon to hide.\n", _FL);
		
		#elif LGI_COCOA
		
			d->Val = limit(v, 0, d->Icon.Length());
			if (d->StatusItem)
			{
				auto img = d->Icon[d->Val];
				[d->StatusItem setImage:img];
			}

			Visible(true);
		
		#elif LGI_CARBON
		
		GSurface *t = d->Val >= 0 ? d->Icon[d->Val] : NULL;
		if (t)
		{
			CGContextRef c = BeginCGContextForApplicationDockTile();
			if (c)
			{
				// CGContextTranslateCTM(c, 0, 128); 
				// CGContextScaleCTM(c, 1.0, -1.0); 

				GScreenDC Dc((GView*)0, c);
				GMemDC m;
				if (m.Create(t->X()*4, t->Y()*4, System32BitColourSpace))
				{
					double Sx = (double) t->X() / m.X();
					double Sy = (double) t->Y() / m.Y();
					for (int y=0; y<m.Y(); y++)
					{
						int Y = (int) (y * Sy);
						System32BitPixel *s = (System32BitPixel*)(*t)[Y];
						System32BitPixel *d = (System32BitPixel*)(m)[m.Y()-y-1];
						
						for (int x=0; x<m.X(); x++)
						{
							d[x] = s[(int)(x * Sx)];
						}
					}
					
					Dc.Blt(Dc.X()-m.X()-1, 0, &m);
				}
				else
				{
					LgiAssert(!"Failed to create memory bitmap");
				}

				CGContextFlush(c);
				EndCGContextForApplicationDockTile(c);
			}
			else
			{
				RestoreApplicationDockTileImage();
			}
		}
		else
		{
			RestoreApplicationDockTileImage();
		}

		#endif
	}
}

GMessage::Result GTrayIcon::OnEvent(GMessage *Message)
{
	#if WINNATIVE
	
	if (Message->Msg() == M_TRAY_NOTIFY &&
		Message->A() == d->MyId)
	{
		// got a notification from the icon
		GMouse m;
		ZeroObj(m);
		switch (Message->B())
		{
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDBLCLK:
			{
				m.Down(true);
				m.Double(true);
				break;
			}
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			{
				m.Down(true);
				break;
			}
		}

		switch (Message->B())
		{
			case WM_LBUTTONDBLCLK:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			{
				m.Left(true);
				break;
			}
			case WM_RBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			{
				m.Right(true);
				break;
			}
			case WM_MBUTTONDBLCLK:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			{
				m.Middle(true);
				break;
			}
		}

		switch (Message->B())
		{
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDBLCLK:
			{
				m.Double(true);
				break;
			}
		}

		if (d->Parent)
		{
			d->Parent->OnTrayClick(m);
		}
	}
	else if (Message->Msg() == d->TrayCreateMsg)
	{
		// explorer crashed... reinit the icon
		ZeroObj(d->TrayIcon);
		Visible(true);
	}
	#endif

	return 0;
}

