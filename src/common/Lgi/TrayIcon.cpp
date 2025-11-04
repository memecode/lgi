#include "lgi/common/Lgi.h"
#include "lgi/common/TrayIcon.h"
#if LINUX
#include "lgi/common/Menu.h"
#endif

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#if LGI_COCOA

	#import <AppKit/AppKit.h>

	@interface LStatusItem : NSObject
	{
	}
	@property LWindow *parent;
	- (id)init:(LWindow*)p;
	- (void)onClick;
	@end

#elif defined __GTK_H__

	/* Change to use:
	
		AppIndicator* appind = app_indicator_new("appname", "appname", APP_INDICATOR_CATEGORY_HARDWARE);
	
		https://wiki.ubuntu.com/DesktopExperienceTeam/ApplicationIndicators
	
		Electron impl:
			https://github.com/electron/electron/blob/2a94d414f76c8d2025197996aca026f4c87e9f27/shell/browser/ui/tray_icon_linux.cc#L34
			StatusIconLinuxDbus is impl in chromium: https://github.com/chromium/chromium/blob/main/chrome/browser/ui/views/status_icons/status_icon_linux_dbus.cc#L225
		
		org.kde.StatusNotifierWatcher, what is it?
			https://deepwiki.com/ubuntu/gnome-shell-extension-appindicator/4-status-notifier-watcher

		https://github.com/search?q=repo%3AAyatanaIndicators%2Flibayatana-appindicator%20org.kde.StatusNotifierWatcher&type=code
			NOTIFICATION_WATCHER_DBUS_ADDR
			
		Need to call d->Parent callbacks:
			/// Called when the tray icon menu is about to be displayed.
			virtual void OnTrayMenu(LSubMenu &m) {}
			/// Called when the tray icon menu item has been selected.
			virtual void OnTrayMenuResult(int MenuId) {}		
	*/

	namespace Gtk {
		#include <glib.h>
		#include <gdk-pixbuf/gdk-pixbuf.h>
		// sudo apt-get install libayatana-appindicator3-dev
		#include <libayatana-appindicator/app-indicator.h>
	}
	using namespace Gtk;

#endif

////////////////////////////////////////////////////////////////////////////
class LTrayIconPrivate
{
public:
	LWindow *Parent = nullptr;	// parent window
	int64 Val = 0;				// which icon is currently visible
	bool Visible = false;

	#if WINNATIVE
	
		int TrayCreateMsg;
		int MyId;
		NOTIFYICONDATAW	TrayIcon;
		typedef HICON IconRef;
		LArray<IconRef> Icon;
	
	#elif LGI_COCOA
	
		typedef LSurface *IconRef;
		NSStatusItem *StatusItem;
		LArray<NSImage*> Icon;
		LStatusItem *Handler;

	#elif LINUX
	
		GlibWrapper<AppIndicator> appind;
		LSubMenu menuRoot;
		
		typedef LString IconRef; // path to file?
		LArray<IconRef> Icon;
		uint64 LastClickTime = 0;
		gint DoubleClickTime = 0;
	
	#else

		typedef LSurface *IconRef;
		LArray<IconRef> Icon;

	#endif


	LTrayIconPrivate(LWindow *p)
	{
		Parent = p;
		
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
					StatusItem.button.target = Handler;
					StatusItem.button.action = @selector(onClick);
				}
			}
		
		#elif LINUX

			auto settings = gtk_settings_get_default();
			DoubleClickTime = 500;
			if (settings)
				g_object_get(G_OBJECT(settings), "gtk-double-click-time", &DoubleClickTime, NULL);

			LastClickTime = 0;
			// Wait for icons to be loaded before creating...
		
		#endif
	}	
	
	~LTrayIconPrivate()
	{
		#if WINNATIVE
		
			for (int n=0; n<Icon.Length(); n++)
				DeleteObject(Icon[n]);
		
		#elif LGI_COCOA
		
			[[NSStatusBar systemStatusBar] removeStatusItem:StatusItem];
			for (auto i: Icon)
				[i release];
		
		#elif LINUX
		
		#else
		
			Icon.DeleteObjects();
		
		#endif
	}

};

#if LGI_COCOA

	@implementation LStatusItem

	- (id)init:(LWindow*)p
	{
		if ((self = [super init]) != nil)
		{
			self.parent = p;
		}
		
		return self;
	}

	- (void)onClick
	{
		LMouse m;
		m.Left(true);
		m.Down(true);
		self.parent->OnTrayClick(m);
	}

	@end

#endif

////////////////////////////////////////////////////////////////////////////
LTrayIcon::LTrayIcon(LWindow *p)
{
	d = new LTrayIconPrivate(p);
}

LTrayIcon::~LTrayIcon()
{
	Visible(false);
	DeleteObj(d);
}

bool LTrayIcon::Load(const TCHAR *Str)
{
	#if WINNATIVE
	
		HICON i = ::LoadIcon(LProcessInst(), Str);
		if (i)
		{
			d->Icon.Add(i);
			return true;
		}
		else LAssert(0);
	
	#elif LGI_COCOA
	
		LString File = LFindFile(Str);
		if (!File)
			return false;
		NSString *nf = File.NsStr();
		NSImage *img = [[NSImage alloc] initWithContentsOfFile:nf];
		if (img)
			d->Icon.Add(img);
		[nf release];
		return img != NULL;
	
	#elif LINUX

		if (!Str)
			return false;

		LString sStr = Str;
		LString File = LFindFile(sStr);
		if (!File)
		{
			LgiTrace("%s:%i - Can't find '%s'\n", _FL, sStr.Get());
			return false;
		}

		d->Icon.Add(File);		
	
	#else
	
		auto File = LFindFile(Str);
		if (File)
		{
			LSurface *i = GdcD->Load(File);
			if (i)
			{
				if (GdcD->GetBits() != i->GetBits())
				{
					if (auto n = new LMemDC(_FL, i->X(), i->Y(), GdcD->GetColourSpace()))
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

bool LTrayIcon::Visible()
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

void menuMapped(GtkWidget *widget,
               GdkEvent  *event,
               gpointer   user_data)
{
	printf("menuMapped !!!!\n");
}


void LTrayIcon::Visible(bool v)
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
			
			#elif LINUX
				
				if (!d->Icon.IdxCheck(d->Val))
				{
					// Create create an app indicator without icon...?
					LAssert(!"No icon for app indicator?");
					return;
				}
				
				auto &iconRef = d->Icon[d->Val];
				auto name = LAppInst->Name();
				LAssert(name != NULL);

				if (!d->appind)
				{
					static int count = 0;
					
					auto id = LString::Fmt("%s-appindicator-%d", name, count++);
					d->appind = app_indicator_new(id, "indicator-messages", APP_INDICATOR_CATEGORY_COMMUNICATIONS);
					printf("%s:%i - app_indicator_new(%s, %s) = %p\n",
						_FL,
						id.Get(),
						iconRef.Get(),
						d->appind.obj);

					if (d->appind)
					{
						app_indicator_set_status(d->appind, APP_INDICATOR_STATUS_ACTIVE);
						app_indicator_set_icon(d->appind, iconRef);
						app_indicator_set_title(d->appind, name);

						printf("%s:%i - app_indicator_set_status(ACTIVE) called: %i, %s, %s\n",
							_FL,
							app_indicator_get_status(d->appind),
							app_indicator_get_icon(d->appind),
							app_indicator_get_title(d->appind));
							
						// Setup some event to capture the menu being made visible
						// g_signal_connect(d->menuRoot, "map-event", G_CALLBACK(menuMapped), NULL);

						if (auto menu = GTK_MENU(d->menuRoot.Handle()))
						{
							d->menuRoot.OnMenuActivate = [this](auto item)
								{
									if (!item || !d->Parent)
									{
										LgiTrace("%s:%i - OnMenuActivate bad param.\n", _FL);
										return;										
									}	
										
									LgiTrace("%s:%i - OnMenuActivate got id %i.\n", _FL, item->Id());
									d->Parent->OnTrayMenuResult(item->Id());
								};
							
			    			app_indicator_set_menu(d->appind, GTK_MENU(d->menuRoot.Handle()));
			    		}
			    		else LAssert(!"No menu?");
					}
					else printf("%s:%i - No app ind.\n", _FL);
				}
				
			#elif LGI_COCOA
			
				if (d->StatusItem)
				{
					d->StatusItem.visible = true;
					
					// Force display of the right icon...
					auto v = d->Val++;
					Value(v);
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

			#elif LINUX
				
				if (d->appind)
				{
					app_indicator_set_status(d->appind, APP_INDICATOR_STATUS_PASSIVE);
					printf("%s:%i - app_indicator_set_status(PASSIVE) called: %i, %s\n",
						_FL,
						app_indicator_get_status(d->appind),
						app_indicator_get_icon(d->appind));
				}
				else printf("%s:%i Error: no app indicator.\n", _FL);
				
			#endif
		}
	}

	d->Visible = v;
}

int64 LTrayIcon::Value()
{
	return d->Val;
}

void LTrayIcon::Value(int64 v)
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
		
		#elif LINUX
			
			if (d->appind)
			{
				LAssert(!"Impl me.");
			}
			
		#elif LGI_COCOA
		
			d->Val = limit(v, 0, d->Icon.Length()-1);
			if (d->StatusItem)
			{
				auto img = d->Icon[d->Val];
				d->StatusItem.button.image = img;
			}
			else LgiTrace("%s:%i - No status item?\n", _FL);

			Visible(true);
		
		#endif
	}
}

LMessage::Result LTrayIcon::OnEvent(LMessage *Message)
{
	#if WINNATIVE
	
		if (Message->Msg() == M_TRAY_NOTIFY &&
			Message->A() == d->MyId)
		{
			// got a notification from the icon
			LMouse m;
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

void LTrayIcon::UpdateMenu()
{
	#if LINUX
	
	if (d->Parent)
		d->Parent->OnTrayMenu(d->menuRoot);
	else
		LgiTrace("%s:%i - param error\n", _FL);
	
	#endif
}