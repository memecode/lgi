#include "lgi/common/Lgi.h"
#include "lgi/common/PopupNotification.h"
#include "lgi/common/Button.h"
#include "lgi/common/CssTools.h"

#include "AppPriv.h"

#ifdef LGI_UNIT_TESTS
size_t LString_RefStrCount = 0;
#endif

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
	LFile::Path p(LSP_APP_CONFIG);
	if (!p.Exists())
		FileDev->CreateFolder(p);

	if (p.Exists())
	{
		p += "lgi.json";
		return p.GetFull();
	}
	
	return (p / ".." / ".lgi.json").GetFull();
}

LString LApp::GetConfig(const char *Variable)
{
	if (auto c = d->GetConfigJson())
		return c->Get(Variable);
	
	return LString();
}

bool LApp::SetConfig(const char *Variable, const char *Value)
{
	if (auto c = d->GetConfigJson())
	{
		if (!c->Set(Variable, Value))
			return false;

		return d->SaveConfig();
	}

	return false;
}

LString LApp::GetArgumentAt(size_t n)
{
	if (n < d->Args.Args)
		return d->Args.Arg[n];
	return LString();
}

bool LApp::GetOption(const char *Option, char *Dest, size_t DestLen)
{
	LString Buf;
	if (!GetOption(Option, Buf))
		return false;

	if (Dest && DestLen == 0)
		return false;

	strcpy_s(Dest, DestLen, Buf);
	return true;
}

bool LApp::GetOption(const char *Option, LString &Buf)
{
	if (!IsOk() || !Option)
	{
		LgiTrace("%s:%i - param err.\n", _FL);
		return false;
	}
	
	auto OptLen = strlen(Option);
	for (int i=1; i<d->Args.Args; i++)
	{
		auto a = d->Args.Arg[i];
		if (!a)
			continue;

		// LgiTrace("\topt=%s arg=%s\n", Option, a);
		switch (*a++)
		{
			case '-':
				if (*a == '-')
					a++;
				// fall through
			case '/':
			case '\\':
				if (!strnicmp(a, Option, OptLen))
				{
					const char *Arg = nullptr;
					if (strlen(a+OptLen) > 0)
					{
						Arg = a + OptLen;
						if (*Arg == '=')
							Arg++;
					}
					else if (i < d->Args.Args - 1)
					{
						Arg = d->Args.Arg[i + 1];
					}					
					if (Arg)
					{
						if (strchr("\'\"", *Arg))
						{
							auto Delim = *Arg++;
							auto End = strchr(Arg, Delim);
							if (End)
							{
								auto Len = End-Arg;
								if (Len > 0)
									Buf.Set(Arg, Len);
								else
									return false;
							}
							else return false;
						}
						else
						{
							Buf = Arg;
						}
					}

					return true;
				}
				break;
			default:
				break;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////
LJson *LAppPrivate::GetConfigJson()
{
	if (!Config)
	{
		auto Path = Owner->GetConfigPath();
		if (Config.Reset(new LJson()))
		{
			LFile f;
			if (f.Open(Path, O_READ))
			{
				Config->SetJson(f.Read());
				f.Close();
			}
				
			bool Dirty = false;				
			#define DEFAULT(var, val) \
				if (Config->Get(var).Length() == 0) \
					Dirty |= Config->Set(var, val);

			DEFAULT(LApp::CfgHelpLink, "https://www.memecode.com/lgi/config-file.php")

			#if defined(LINUX)

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

			DEFAULT(LApp::CfgNetworkHttpProxy,   "");
			DEFAULT(LApp::CfgNetworkHttpsProxy,  "");
			DEFAULT(LApp::CfgNetworkSocks5Proxy, "");

			DEFAULT(LApp::CfgLanguage, "-");

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
			
	LFile f;
	if (!f.Open(Path, O_WRITE))
	{
		LError err(f.GetError());
		LgiTrace("%s:%i - error writing LGI config: %s\n", _FL, err.ToString().Get());
		LAssert(!"error writing config");
		return false;
	}
			
	f.SetSize(0);
	return f.Write(Config->GetJson());		
}
	
////////////////////////////////////////////////////////////////////////////////////////////////
LPopupNotification *LPopupNotification::Message(LWindow *ref, LString msg)
{
	if (!ref)
		return NULL;

	if (!LAppInst->InThread())
	{
		LAppInst->RunCallback([ref, msg]()
			{
				LPopupNotification::Message(ref, msg);
			},
			_FL);
		return nullptr;
	}
	
	auto w = PopupNotificationFactory.map.Find(ref);
	if (w)
	{
		w->Add(ref, msg);
	}
	else
	{
		w = new LPopupNotification(ref, true, msg);
		if (w)
			PopupNotificationFactory.map.Add(ref, w);
	}

	return w;
}

LPopupNotification *LPopupNotification::Create(LWindow *ref, LString msg)
{
	if (!ref)
		return NULL;

	if (!LAppInst->InThread())
	{
		LAppInst->RunCallback([ref, msg]()
			{
				LPopupNotification::Create(ref, msg);
			},
			_FL);
		return nullptr;
	}
	
	auto w = PopupNotificationFactory.map.Find(ref);
	if (w)
	{
		w->Add(ref, msg);
	}
	else
	{
		w = new LPopupNotification(ref, false, msg);
		if (w)
			PopupNotificationFactory.map.Add(ref, w);
	}

	return w;
}

LPopupNotification::LPopupNotification(LWindow *ref, bool show, LString msg)
	#if LINUX
	: LPopup(ref)
	#endif
{
	showPopup = show;

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

	if (auto css = GetCss(true))
	{
		css->Padding(LCss::Len(LCss::LenPx, DefPaddingPx));

		css->Border(LCss::BorderDef(LCss::LenPx, 1.0f, cBorder));
		css->BackgroundColor(cBack);
		css->Color(cFore);
	}

	SetTitleBar(false);
	SetWillFocus(false); // Don't take focus
	Name("Notification");

	if (ref)
	{
		RefWnd = ref;
		if (msg)
			Add(ref, msg);
	}
}

LPopupNotification::~LPopupNotification()
{
	LAssert(PopupNotificationFactory.map.Find(RefWnd) == this);
	PopupNotificationFactory.map.Delete(RefWnd);

	Actions.DeleteObjects();
}

int LPopupNotification::GetPaddingPx()
{
	if (auto css = GetCss())
		if (auto padding = css->Padding())
			return padding.ToPx(X(), GetFont());
	return DefPaddingPx;
}

LPoint LPopupNotification::CalcSize()
{
	LPoint p;
	for (auto ds: Msgs)
	{
		p.x = MAX(p.x, ds->X());
		p.y += ds->Y();
	}

	auto pad = GetPaddingPx();
	p.x += pad * 2 + Decor.x;
	p.y += pad * 2 + Decor.y;

	for (auto action: Actions)
	{
		if (action->btn)
			p.x += pad + action->btn->X();
	}

	return p;
}

LString LPopupNotification::GetMsgs()
{
	LString::Array a;
	for (auto m: Msgs)
		a.Add((const char16*)*m);
	return LString("\n").Join(a);
}

void LPopupNotification::Action(LString label, std::function<void()> callback)
{
	if (!label || !callback)
		return;

	if (auto action = new TAction())
	{
		action->label = label;
		action->callback = callback;
		action->ctrlId = 100 + Actions.Length();
		Actions.Add(action);

		if ((action->btn = new LButton(action->ctrlId, 0, 0, -1, -1, action->label)))
			AddView(action->btn);
		
		Layout();
	}
}

void LPopupNotification::Layout()
{
	auto r = RefWnd->GetPos();
	#if LINUX
	r = r.Move(-27, -30);	// Not sure why this is needed, but if the window is fullscreen... 
							// it moves things back on screen
	#endif
	auto Sz = CalcSize();

	LRect pos;
	pos.ZOff(Sz.x - 1, Sz.y - 1);
	pos.Offset(r.x2 - pos.X(), r.y2 - pos.Y());
	SetPos(pos);
}

void LPopupNotification::Show()
{
	if (!RefWnd)
	{
		LAssert(!"No reference window.");
		return;
	}

	Layout();

	SetAlwaysOnTop(true);
	SetPulse(500);		
	if (!IsAttached())
		Attach(nullptr);
	Visible(true);
	
	LgiTrace("%s:%i - showing LPopupNotification at %s: %s\n",
		_FL, GetPos().GetStr(), GetMsgs().Get());
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

	if (RefWnd &&
		Msgs.Length() > 0 &&
		showPopup)
		Show();
}

void LPopupNotification::OnPaint(LSurface *pDC)
{
	auto c = GetClient();
	auto f = GetFont();
	if (auto css = GetCss())
	{
		LCssTools tools(css, f);
		c = tools.PaintBorder(pDC, c);
		tools.PaintContent(pDC, c);
	}
	else
	{
		pDC->Colour(cBorder);
		pDC->Box(&c);
		c.Inset(1, 1);
		pDC->Colour(cBack);
		pDC->Rectangle(&c);
	}

	f->Fore(cFore);
	f->Back(cBack);
	f->Transparent(true);
		
	auto pad = GetPaddingPx();
	int x = c.x1 + pad;
	int y = c.y1 + pad;
	int maxX = 0;
	for (auto ds: Msgs)
	{
		ds->Draw(pDC, x, y);
		y += ds->Y();
		maxX = MAX(maxX, x + ds->X());
	}

	y = c.y1 + pad - (LButton::Overhead.y >> 1); // reset to top
	maxX += pad;
	for (auto a: Actions)
	{
		if (a->btn)
		{
			auto p = a->btn->GetPos();
			p.Offset(maxX - p.x1, y - p.y1);
			a->btn->SetPos(p);
			maxX += a->btn->X() + pad;
		}
	}
}

int LPopupNotification::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	for (auto a: Actions)
	{
		if (a->btn && Ctrl == a->btn)
		{
			if (n.Type == LNotifyItemClick)
			{
				if (a->callback)
					a->callback();
				else
					LgiTrace("%s:%i - no callback for action '%s'\n", _FL, a->label.Get());
				return 1;
			}
			else LgiTrace("%s:%i - unexpected notification type '%i'\n", _FL, (int)n.Type);
		}
	}
	return 0;
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

bool LApp::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	switch (LStringToDomProp(Name))
	{
		#define PATH(sym, lsp) \
			case sym: \
				Value = LGetSystemPath(lsp); \
				return true;

		PATH(PathRoot, LSP_ROOT)
		PATH(PathOs, LSP_OS)
		PATH(PathOsLib, LSP_OS_LIB)
		PATH(PathTemp, LSP_TEMP)
		PATH(PathCommonAppData, LSP_COMMON_APP_DATA)
		PATH(PathUserAppData, LSP_USER_APP_DATA)
		PATH(PathLocalAppData, LSP_LOCAL_APP_DATA)
		PATH(PathDesktop, LSP_DESKTOP)
		PATH(PathHome, LSP_HOME)
		PATH(PathUserApps, LSP_USER_APPS)
		PATH(PathExe, LSP_EXE)
		PATH(PathTrash, LSP_TRASH)
		PATH(PathAppInstall, LSP_APP_INSTALL)
		PATH(PathAppData, LSP_APP_DATA)
		PATH(PathAppConfig, LSP_APP_CONFIG)
		PATH(PathAppCache, LSP_APP_CACHE)
		PATH(PathAppRoot, LSP_APP_DATA)
		PATH(PathUserDocuments, LSP_USER_DOCUMENTS)
		PATH(PathUserMusic, LSP_USER_MUSIC)
		PATH(PathUserVideo, LSP_USER_VIDEO)
		PATH(PathUserDownloads, LSP_USER_DOWNLOADS)
		PATH(PathUserLinks, LSP_USER_LINKS)
		PATH(PathUserPictures, LSP_USER_PICTURES)
		PATH(PathSysMountPoint, LSP_SYS_MOUNT_POINT)
		PATH(PathUserMountPoint, LSP_USER_MOUNT_POINT)

		default:
			LgiTrace("%s:%i - unsupported field '%s'\n", _FL, Name);
			break;
	}

	return false;
}
