#include "lgi/common/Lgi.h"
#include "GAppPriv.h"

::LString LApp::GetConfigPath()
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
			DEFAULT("Linux.Keys.Shift", "GDK_SHIFT_MASK");
			DEFAULT("Linux.Keys.Ctrl", "GDK_CONTROL_MASK");
			DEFAULT("Linux.Keys.Alt", "GDK_MOD1_MASK");

			auto Sys =
				#if defined(MAC)
				"GDK_MOD2_MASK";
				#else
				"GDK_SUPER_MASK";
				#endif
			DEFAULT("Linux.Keys.System", Sys);
			DEFAULT("Linux.Keys.Debug", "0");
			DEFAULT("Language", "-");
			DEFAULT("Fonts.GlyphSub", "1");
				
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
	
