#include "lgi/common/Lgi.h"
#include "lgi/common/PopupNotification.h"

#include "AppPriv.h"

#ifdef LINUX
namespace Gtk {
#include "LgiWidget.h"
}
#endif

class LPopupNotificationFactory
{
public:
	LHashTbl<PtrKey<LWindow*>, LPopupNotification*> map;

	void Empty()
	{
		while (map.Length())
		{
			auto p = map.begin();
			if (p != map.end())
			{
				delete (*p).value;
			}
		}
	}

}	PopupNotificationFactory;


void LApp::CommonCleanup()
{
	PopupNotificationFactory.Empty();
}

LString LApp::GetConfigPath()
{
	#if defined(LINUX)
	::LFile::Path p(LSP_HOME);
	p += ".config";
	#else
	::LFile::Path p(LSP_USER_APP_DATA);
	p += "MemecodeLgi";
	if (!p.Exists())
		FileDev->CreateFolder(p);
	#endif

	if (p.Exists())
	{
		p += "lgi.json";
		return p.GetFull();
	}
	
	p--;
	p += ".lgi.json";
	return p.GetFull();
}

LString LApp::GetConfig(const char *Variable)
{
	auto c = d->GetConfig();
	return c ? c->Get(Variable) : LString();
}

void LApp::SetConfig(const char *Variable, const char *Value)
{
	auto c = d->GetConfig();
	if (c && c->Set(Variable, Value))
		d->SaveConfig();
}

///////////////////////////////////////////////////////////////////////////////////
LJson *LAppPrivate::GetConfig()
{
	if (!Config)
	{
		auto Path = Owner->GetConfigPath();
		if (Config.Reset(new LJson()))
		{
			::LFile f;
			if (f.Open(Path, O_READ))
				Config->SetJson(f.Read());
				
			bool Dirty = false;				
			#define DEFAULT(var, val) \
				if (Config->Get(var).Length() == 0) \
					Dirty |= Config->Set(var, val);

			#ifdef LINUX

				DEFAULT(LApp::CfgLinuxKeysShift, "GDK_SHIFT_MASK");
				DEFAULT(LApp::CfgLinuxKeysCtrl, "GDK_CONTROL_MASK");
				DEFAULT(LApp::CfgLinuxKeysAlt, "GDK_MOD1_MASK");

				auto Sys =
					#if defined(MAC)
					"GDK_MOD2_MASK";
					#else
					"GDK_SUPER_MASK";
					#endif
				DEFAULT(LApp::CfgLinuxKeysSystem, Sys);
				DEFAULT("Linux.Keys.Debug", "0");
				
				auto str = [](Gtk::GDK_MouseButtons btn)
				{
					LString s;
					s.Printf("%i", btn);
					return s;
				};
				
				DEFAULT(LApp::CfgLinuxMouseLeft,    str(Gtk::GDK_LEFT_BTN));
				DEFAULT(LApp::CfgLinuxMouseMiddle,  str(Gtk::GDK_MIDDLE_BTN));
				DEFAULT(LApp::CfgLinuxMouseRight,   str(Gtk::GDK_RIGHT_BTN));
				DEFAULT(LApp::CfgLinuxMouseBack,    str(Gtk::GDK_BACK_BTN));
				DEFAULT(LApp::CfgLinuxMouseForward, str(Gtk::GDK_FORWARD_BTN));

			#endif

			DEFAULT("Language", "-");

			DEFAULT("Fonts.Comment", "Fonts are specified in the <face>:<pointSize> format.");
			DEFAULT(LApp::CfgFontsGlyphSub, "1");
			DEFAULT(LApp::CfgFontsPointSizeOffset, "0");
			DEFAULT(LApp::CfgFontsSystemFont, "-");
			DEFAULT(LApp::CfgFontsBoldFont, "-");
			DEFAULT(LApp::CfgFontsMonoFont, "-");
			DEFAULT(LApp::CfgFontsSmallFont, "-");
			DEFAULT(LApp::CfgFontsCaptionFont, "-");
			DEFAULT(LApp::CfgFontsMenuFont, "-");
				
			DEFAULT("Colours.Comment", "Use CSS hex definitions here, ie #RRGGBB in hex.");
			#define _(name) \
				if (L_##name < L_MAXIMUM) \
					DEFAULT("Colours.L_" #name, "-");
			_SystemColour();
			#undef _
				
			if (Dirty)
				SaveConfig();
				
			LgiTrace("Using LGI config: '%s'\n", Path.Get()); 
		}
		else printf("%s:%i - Alloc failed.\n", _FL);
	}
		
	return Config;
}
	
bool LAppPrivate::SaveConfig()
{
	auto Path = Owner->GetConfigPath();
	if (!Path || !Config)
		return false;
			
	::LFile f;
	if (!f.Open(Path, O_WRITE))
		return false;
			
	f.SetSize(0);
	return f.Write(Config->GetJson());		
}
	
////////////////////////////////////////////////////////////////////////////////////////////////
LPopupNotification *LPopupNotification::Message(LWindow *ref, LString msg)
{
	if (!ref)
		return NULL;

	auto w = PopupNotificationFactory.map.Find(ref);
	if (w)
	{
		w->Add(ref, msg);
	}
	else
	{
		w = new LPopupNotification(ref, msg);
		if (w)
			PopupNotificationFactory.map.Add(ref, w);
	}

	return w;
}

LPopupNotification::LPopupNotification(LWindow *ref, LString msg)
{
	#if 1
	cFore = cBack = cBorder = L_TOOL_TIP;
	if (cFore.ToHLS())
	{
		cBorder.SetHLS(cBorder.GetH(), (int)(cBorder.GetL()*0.6), cBorder.GetS());
		cFore.SetHLS  (cFore.GetH(),   (int)(cFore.GetL()*0.4),   cFore.GetS());
	}
	else
	{
		cBorder = cBorder.Mix(LColour::Black, 0.05f);
		cFore = cFore.Mix(LColour::Black, 0.5f);
	}
	#else
	cBorder = cFore = LColour(0xd4, 0xb8, 0x62);
	cBack = LColour(0xf7, 0xf0, 0xd5);
	#endif

	SetTitleBar(false);
	SetWillFocus(false); // Don't take focus
	Name("Notification");

	if (ref)
		RefWnd = ref;
	if (ref && msg)
		Add(ref, msg);
}

LPopupNotification::~LPopupNotification()
{
	LAssert(PopupNotificationFactory.map.Find(RefWnd) == this);
	PopupNotificationFactory.map.Delete(RefWnd);
}

LPoint LPopupNotification::CalcSize()
{
	LPoint p;
	for (auto ds: Msgs)
	{
		p.x = MAX(p.x, ds->X());
		p.y += ds->Y();
	}

	p.x += Border * 2 + Decor.x;
	p.y += Border * 2 + Decor.y;

	return p;
}

void LPopupNotification::Init()
{
	if (!RefWnd)
	{
		LAssert(!"No reference window.");
		return;
	}

	auto r = RefWnd->GetPos();
	auto Sz = CalcSize();

	LRect pos;
	pos.ZOff(Sz.x - 1, Sz.y - 1);
	pos.Offset(r.x2 - pos.X(), r.y2 - pos.Y());
	SetPos(pos);

	SetAlwaysOnTop(true);
	SetPulse(500);		
	if (!IsAttached())
		Attach(0);
	Visible(true);
}

void LPopupNotification::Add(LWindow *ref, LString msg)
{
	if (msg)
	{
		HideTs = LCurrentTime();
		Msgs.Add(new LDisplayString(GetFont(), msg));
	}
			
	if (ref && RefWnd != ref)
		RefWnd = ref;

	if (RefWnd && Msgs.Length() > 0)
		Init();
}

void LPopupNotification::OnPaint(LSurface *pDC)
{
	pDC->Colour(cBorder);
	auto c = GetClient();
	pDC->Box(&c);
	c.Inset(1, 1);
	pDC->Colour(cBack);
	pDC->Rectangle(&c);

	auto f = GetFont();
	f->Fore(cFore);
	f->Back(cBack);
	f->Transparent(true);
		
	int x = c.x1 + Border;
	int y = c.y1 + Border;
	for (auto ds: Msgs)
	{
		ds->Draw(pDC, x, y);
		y += ds->Y();
	}
}

void LPopupNotification::OnPulse()
{
	if (HideTs &&
		LCurrentTime() >= HideTs + ShowMs)
	{
		HideTs = 0;
		Visible(false);
		SetPulse();
		Msgs.DeleteObjects();
	}
}
