#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GToken.h"
#if defined(LINUX) && !defined(LGI_SDL)
#include "LgiWinManGlue.h"
#endif

//////////////////////////////////////////////////////////////////////////////////
#if !defined(LK_CONTEXTKEY)
	#if defined(WINDOWS)
		#define LK_CONTEXTKEY 0x5d
	#elif defined(MAC)
		#define LK_CONTEXTKEY VK_APPS
	#else
		#define LK_CONTEXTKEY 0x5d
		#warning "Check local platform def for app menu key."
	#endif
#endif

bool GKey::IsContextMenu()
{
	#if WINNATIVE || defined(LINUX)
	return !IsChar && vkey == LK_CONTEXTKEY;
	#else
	return false;
	#endif
}

//////////////////////////////////////////////////////////////////////////////////
bool GMouse::IsContextMenu()
{
	if (Right())
		return true;
	
	#if defined(MAC) || defined(LINUX)
	if (Left() && Ctrl())
		return true;
	#endif
	
	return false;
}

bool GMouse::ToScreen()
{
	if (ViewCoords && Target)
	{
		GdcPt2 p(x, y);
		Target->PointToScreen(p);
		x = p.x; y = p.y;
		ViewCoords = false;
		return true;
	}
	else
	{
		printf("%s:%i - Error: Target=%p ViewCoords=%i\n", _FL, Target, ViewCoords);
	}
	
	return false;
}

bool GMouse::ToView()
{
	if (!ViewCoords && Target)
	{
		GdcPt2 p(x, y);
		Target->PointToView(p);
		x = p.x; y = p.y;
		ViewCoords = true;
		return true;
	}
	else
	{
		printf("%s:%i - Error: Target=%p ViewCoords=%i\n",
			__FILE__,
			__LINE__,
			Target,
			ViewCoords);
	}
	
	return false;
}

#if WINNATIVE

#if !defined(DM_POSITION)
#define DM_POSITION         0x00000020L
#endif

typedef WINUSERAPI BOOL (WINAPI *pEnumDisplayDevicesA)(PVOID, DWORD, PDISPLAY_DEVICEA, DWORD);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplayDevicesW)(PVOID, DWORD, PDISPLAY_DEVICEW, DWORD);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplaySettingsA)(LPCSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEA lpDevMode);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplaySettingsW)(LPCWSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEW lpDevMode);

#endif

bool LgiGetDisplays(::GArray<GDisplayInfo*> &Displays, GRect *AllDisplays)
{
	#if WINNATIVE
	if (AllDisplays)
		AllDisplays->ZOff(-1, -1);

	GLibrary User32("User32");
	DISPLAY_DEVICEW disp;
	ZeroObj(disp);
	disp.cb = sizeof(disp);
	pEnumDisplayDevicesW EnumDisplayDevicesW = (pEnumDisplayDevicesW) User32.GetAddress("EnumDisplayDevicesW");
	pEnumDisplaySettingsW EnumDisplaySettingsW = (pEnumDisplaySettingsW) User32.GetAddress("EnumDisplaySettingsW");

	for (int i=0;
		EnumDisplayDevicesW(0, i, &disp, 0);
		i++)
	{
		DEVMODEW mode;
		ZeroObj(mode);
		mode.dmSize = sizeof(mode);
		mode.dmDriverExtra = sizeof(mode);

		if (EnumDisplaySettingsW(disp.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
		{
			GDisplayInfo *Dsp = new GDisplayInfo;
			if (Dsp)
			{
				Dsp->r.ZOff(mode.dmPelsWidth-1, mode.dmPelsHeight-1);

				if (mode.dmFields & DM_POSITION)
				{
					Dsp->r.Offset(mode.dmPosition.x, mode.dmPosition.y);
				}
				if (AllDisplays)
				{
					if (AllDisplays->Valid())
						AllDisplays->Union(&Dsp->r);
					else
						*AllDisplays = Dsp->r;
				}

				disp.cb = sizeof(disp);

				Dsp->BitDepth = mode.dmBitsPerPel;
				Dsp->Refresh = mode.dmDisplayFrequency;
				Dsp->Name = WideToUtf8(disp.DeviceString);
				Dsp->Device = WideToUtf8(disp.DeviceName);

				DISPLAY_DEVICEW temp = disp;
				if (EnumDisplayDevicesW(temp.DeviceName, 0, &disp, 0))
				{
					Dsp->Monitor = WideToUtf8(disp.DeviceString);
				}

				Displays.Add(Dsp);
			}
		}

		disp.cb = sizeof(disp);
	}

	#elif defined __GTK_H__

	Gtk::GdkDisplay *Dsp = Gtk::gdk_display_get_default();
	#if GtkVer(3, 22) 
		int monitors = Gtk::gdk_display_get_n_monitors(Dsp);
		for (int i=0; i<monitors; i++)
		{
			Gtk::GdkMonitor *m = Gtk::gdk_display_get_monitor(Dsp, i);
			if (!m)
				continue;

			GDisplayInfo *di = new GDisplayInfo;
			if (!di)
				continue;

			Gtk::GdkRectangle geometry;
			gdk_monitor_get_geometry (m, &geometry);
			di->r = geometry;
			di->Device = NewStr(Gtk::gdk_monitor_get_manufacturer(m));
			di->Name = NewStr(Gtk::gdk_monitor_get_model(m));
			di->Refresh = Gtk::gdk_monitor_get_refresh_rate(m);

			Displays.Add(di);
		}
	#endif

	#endif

	return Displays.Length() > 0;
}

void GetChildrenList(GViewI *w, List<GViewI> &l)
{
	if (w)
	{
		GViewIterator *it = w->IterateViews();
		if (it)
		{
			for (GViewI *v = it->First(); v; v = it->Next())
			{
				#if WINNATIVE
				int Style = GetWindowLong(v->Handle(), GWL_STYLE);
				if (TestFlag(Style, WS_VISIBLE) &&
					!TestFlag(Style, WS_DISABLED))
				{
					if (TestFlag(Style, WS_TABSTOP))
					{
						l.Insert(v);
					}

					GetChildrenList(v, l);
				}
				#else
				if (v->Visible() && v->Enabled())
				{
					if (v->GetTabStop())
					{
						l.Insert(v);
					}

					GetChildrenList(v, l);
				}
				#endif
			}

			DeleteObj(it);
		}
	}
}


GViewI *GetNextTabStop(GViewI *v, bool Back)
{
	if (v)
	{
		GWindow *Wnd = v->GetWindow();
		if (Wnd)
		{
			List<GViewI> All;
			GetChildrenList(Wnd, All);

			ssize_t MyIndex = All.IndexOf(v);
			if (MyIndex >= 0)
			{
				int Inc = Back ? -1 : 1;
				size_t NewIndex = (MyIndex + All.Length() + Inc) % All.Length();
				return All.ItemAt(NewIndex);
			}
			else
			{
				return All[0];
			}
		}
	}

	return 0;
}



