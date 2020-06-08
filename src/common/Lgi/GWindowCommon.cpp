//
//  GWindowCommon.cpp
//  LgiCarbon
//
//  Created by Matthew Allen on 11/09/14.
//
//
#include "Lgi.h"
#include "INet.h"
#include "GDropFiles.h"

void GWindow::BuildShortcuts(ShortcutMap &Map, GViewI *v)
{
	if (!v) v = this;

	GAutoPtr<GViewIterator> Ch(v->IterateViews());
	for (GViewI *c = Ch->First(); c; c = Ch->Next())
	{
		const char *n = c->Name(), *amp;
		if (n && (amp = strchr(n, '&')))		
		{
			amp++;
			if (*amp && *amp != '&')
			{
				uint8_t *i = (uint8_t*)amp;
				ssize_t sz = Strlen(amp);
				int32 ch = LgiUtf8To32(i, sz);
				if (ch)
					Map.Add(ToUpper(ch), c);
			}
		}
		
		BuildShortcuts(Map, c);
	}
}

void GWindow::MoveOnScreen()
{
	GRect p = GetPos();
	GArray<GDisplayInfo*> Displays;
	GRect Screen(0, 0, -1, -1);

	if (
		#if WINNATIVE
		!IsZoomed(Handle()) &&
		!IsIconic(Handle()) &&
		#endif
		LgiGetDisplays(Displays))
	{
		int Best = -1;
		int Pixels = 0;
		int Close = 0x7fffffff;
		for (int i=0; i<Displays.Length(); i++)
		{
			GRect o = p;
			o.Bound(&Displays[i]->r);
			if (o.Valid())
			{
				int Pix = o.X()*o.Y();
				if (Best < 0 || Pix > Pixels)
				{
					Best = i;
					Pixels = Pix;
				}
			}
			else if (Pixels == 0)
			{
				int n = Displays[i]->r.Near(p);
				if (n < Close)
				{
					Best = i;
					Close = n;
				}
			}
		}

		if (Best >= 0)
			Screen = Displays[Best]->r;
	}

	if (!Screen.Valid())
		Screen.Set(0, 0, GdcD->X()-1, GdcD->Y()-1);

	if (p.x2 >= Screen.x2)
		p.Offset(Screen.x2 - p.x2, 0);
	if (p.y2 >= Screen.y2)
		p.Offset(0, Screen.y2 - p.y2);
	if (p.x1 < Screen.x1)
		p.Offset(Screen.x1 - p.x1, 0);
	if (p.y1 < Screen.y1)
		p.Offset(0, Screen.y1 - p.y1);

	SetPos(p, true);

	Displays.DeleteObjects();
}

void GWindow::MoveToCenter()
{
	GRect Screen(0, 0, GdcD->X()-1, GdcD->Y()-1);
	::GArray<GDisplayInfo*> Displays;
	GRect p = GetPos();

	p.Offset(-p.x1, -p.y1);
	if (LgiGetDisplays(Displays, &Screen) && Displays.Length() > 0)
	{
		GDisplayInfo *Dsp = NULL;
		for (auto d: Displays)
		{
			if (d->r.Overlap(&p))
			{
				Dsp = d;
				break;
			}
		}

		if (!Dsp)
			goto ScreenPos;

		p.Offset(Dsp->r.x1 + ((Dsp->r.X() - p.X()) / 2), Dsp->r.y1 + ((Dsp->r.Y() - p.Y()) / 2));
	}
	else
	{
		ScreenPos:
		p.Offset((Screen.X() - p.X()) / 2, (Screen.Y() - p.Y()) / 2);
	}

	SetPos(p, true);
	Displays.DeleteObjects();
}

void GWindow::MoveToMouse()
{
	GMouse m;
	if (GetMouse(m, true))
	{
		GRect p = GetPos();

		p.Offset(-p.x1, -p.y1);
		p.Offset(m.x-(p.X()/2), m.y-(p.Y()/2));

		SetPos(p, true);
		MoveOnScreen();
	}
}

bool GWindow::MoveSameScreen(GViewI *View)
{
	if (!View)
	{
		LgiAssert(0);
		return false;
	}

	auto Wnd = View->GetWindow();
	GRect p = Wnd ? Wnd->GetPos() : View->GetPos();
	int cx = p.x1 + (p.X() >> 4);
	int cy = p.y1 + (p.Y() >> 4);
	
	GRect np = GetPos();
	np.Offset(cx - np.x1, cy - np.y1);
	SetPos(np);
	
	MoveOnScreen();
	return true;
}

int GWindow::WillAccept(GDragFormats &Formats, GdcPt2 Pt, int KeyState)
{
	Formats.SupportsFileDrops();
	return true;
}

int GWindow::OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;
	
	#ifdef DEBUG_DND	
	LgiTrace("%s:%i - OnDrop Data=%i Pt=%i,%i Key=0x%x\n", _FL, Data.Length(), Pt.x, Pt.y, KeyState);
	#endif
	for (auto &dd: Data)
	{
		#ifdef DEBUG_DND
		LgiTrace("\tFmt=%s\n", dd.Format.Get());
		#endif
		if (dd.IsFileDrop())
		{
			GDropFiles Files(dd);
			if (Files.Length())
			{
				Status = DROPEFFECT_COPY;
				OnReceiveFiles(Files);
				break;
			}
		}
	}
	
	return Status;
}
