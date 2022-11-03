#include "lgi/common/Lgi.h"
#include "AppPriv.h"

#ifdef LINUX
namespace Gtk {
#include "LgiWidget.h"
}
#endif

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

::LString LApp::GetConfig(const char *Variable)
{
	auto c = d->GetConfig();
	return c ? c->Get(Variable) : NULL;
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
	
