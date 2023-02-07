#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
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

bool LKey::IsContextMenu() const
{
	#if WINNATIVE || defined(LINUX)
	return !IsChar && vkey == LK_CONTEXTKEY;
	#else
	return false;
	#endif
}

//////////////////////////////////////////////////////////////////////////////////
LString LMouse::ToString() const
{
	LString s;
	s.Printf("LMouse(pos=%i,%i view=%p/%s btns=%i/%i/%i/%i/%i dwn=%i dbl=%i "
			"ctrl=%i alt=%i sh=%i sys=%i)",
		x, y, // pos
		Target, Target?Target->GetClass():NULL, // view
		Left(), Middle(), Right(), Button1(), Button2(), // btns
		Down(), Double(), // dwn
		Ctrl(), Alt(), Shift(), System()); // mod keys
	return s;
}
	
bool LMouse::IsContextMenu() const
{
	if (Right())
		return true;
	
	#if defined(MAC)
	if (Left() && Ctrl())
		return true;
	#endif
	
	return false;
}

bool LMouse::ToScreen()
{
	if (ViewCoords)
	{
		if (!Target)
		{
			printf("%s:%i - ToScreen Error: Target=%p ViewCoords=%i\n",
				_FL,
				Target,
				ViewCoords);
			return false;
		}
		
		LPoint p(x, y);
		Target->PointToScreen(p);
		x = p.x; y = p.y;
		ViewCoords = false;
	}
	
	return true;
}

bool LMouse::ToView()
{
	if (!ViewCoords)
	{
		if (!Target)
		{
			printf("%s:%i - ToView Error: Target=%p ViewCoords=%i\n",
				_FL,
				Target,
				ViewCoords);
			return false;
		}
		
		LPoint p(x, y);
		Target->PointToView(p);
		x = p.x; y = p.y;
		ViewCoords = true;
	}
	
	return true;
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

LPointF GDisplayInfo::Scale()
{
	LPointF p((double)Dpi.x / 96.0, (double)Dpi.y / 96.0);
	return p;
}

bool LGetDisplays(::LArray<GDisplayInfo*> &Displays, LRect *AllDisplays)
{
	#if WINNATIVE
	
		if (AllDisplays)
			AllDisplays->ZOff(-1, -1);

		LLibrary User32("User32");
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
					Dsp->Name = disp.DeviceString;
					Dsp->Device = disp.DeviceName;
					Dsp->Dpi.x = Dsp->Dpi.y = mode.dmLogPixels;

					DISPLAY_DEVICEW temp = disp;
					if (EnumDisplayDevicesW(temp.DeviceName, 0, &disp, 0))
						Dsp->Monitor = disp.DeviceString;

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
				di->Device = Gtk::gdk_monitor_get_manufacturer(m);
				di->Name = Gtk::gdk_monitor_get_model(m);
				di->Refresh = Gtk::gdk_monitor_get_refresh_rate(m);

				Displays.Add(di);
			}
	
		#endif

	#elif LGI_COCOA
	
		for (NSScreen *s in [NSScreen screens])
		{
			GDisplayInfo *di = new GDisplayInfo;
			di->r = s.frame;
			Displays.Add(di);
		}

	#endif

	return Displays.Length() > 0;
}

void GetChildrenList(LViewI *w, List<LViewI> &l)
{
	if (!w)
		return;

	for (auto v: w->IterateViews())
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
}


LViewI *GetNextTabStop(LViewI *v, bool Back)
{
	if (v)
	{
		LWindow *Wnd = v->GetWindow();
		if (Wnd)
		{
			List<LViewI> All;
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



