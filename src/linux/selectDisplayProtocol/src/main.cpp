#include "lgi/common/Lgi.h"
#include "lgi/common/Button.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Json.h"

//////////////////////////////////////////////////////////////////
// Running action as root:
// https://pardus.github.io/wiki/development/linux/polkit
//
// The file 'com.memecode.selectDisplay.policy' needs to be
// installed to '/usr/share/polkit-1/actions/'

const char *AppName = "Select Desktop Protocol";

enum Ctrls {
	ID_STATIC = -1,
	ID_TABLE = 100,	
	ID_X11,
	ID_WAYLAND,
	ID_LOGOUT
};

LColour colX11(0x66, 0x1a, 0xff);
LColour colWayland(0x00, 0x90, 0x90);
LColour colBack(0x60, 0x60, 0x60);

static const char *sPolicyFolder = "/usr/share/polkit-1/actions/";
static const char *sPolicyFile = "com.memecode.selectDisplay.policy";
static const char *sDataFile = "setDesktop.json";
static const char *sConfigFile = "/etc/gdm3/custom.conf";

LString getDataFilePath()
{
	LFile::Path appInst(LSP_APP_INSTALL);
	appInst += sDataFile;
	LFile f;
	bool preExist = appInst.Exists();
	// printf("appInst: %s\n", appInst.GetFull().Get());
	if (f.Open(appInst, O_WRITE))
	{
		f.Close();
		if (!preExist)
			FileDev->Delete(appInst.GetFull(), nullptr, false);
		
		return appInst.GetFull();
	}
	
	LFile::Path appRoot(LSP_APP_ROOT);
	appRoot += sDataFile;	
	preExist = appRoot.Exists();
	// printf("appRoot: %s\n", appRoot.GetFull().Get());
	if (f.Open(appRoot, O_WRITE))
	{
		f.Close();
		if (!preExist)
			FileDev->Delete(appRoot.GetFull(), nullptr, false);

		return appRoot.GetFull();
	}
		
	return LString();
}

class App : public LWindow
{
	LTableLayout *tbl = nullptr;

public:
	App(LString dataPath)
	{
		Name(AppName);
		LRect r(0, 0, 400, 182);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);

		int y = 0;
		LTextLabel *label;
		LButton *btn;
		if (Attach(0))
		{
			AddView(tbl = new LTableLayout(ID_TABLE));
			tbl->GetCss(true)->BorderSpacing("1px");
			
			if (dataPath) // privledged UI
			{
				LError err;
				LFile data;
				if (!data.Open(dataPath, O_READ))
					err.Set(LErrorPathNotFound, LString::Fmt("can't read '%s'", dataPath.Get()));
				else
				{
					LJson j;
					if (!j.SetJson(data.Read()))
						err.Set(LErrorFuncFailed, LString::Fmt("error parsing '%s'", dataPath.Get()));
					else if (auto desktop = j.Get("desktop"))
					{
						LFile cfg;
						if (!cfg.Open(sConfigFile, O_READWRITE))
							err.Set(LErrorPathNotFound, LString::Fmt("can't write '%s'", sConfigFile));
						else
						{
							auto lines = cfg.Read().SplitDelimit("\n", -1, false /* don't group delimiters, leave the blank lines */);
							for (auto &ln: lines)
							{
								if (ln.Find("WaylandEnable") >= 0)
									ln.Printf("WaylandEnable=%s", desktop.Find("wayland") >= 0 ?"true":"false");
							}
							cfg.SetPos(0);
							cfg.SetSize(0);
							cfg.Write(LString("\n").Join(lines)+"\n");
						}
					}
					else
						err.Set(LErrorFuncFailed, LString::Fmt("json '%s' missing param", dataPath.Get()));
				}
				
				tbl->GetCss(true)->Width("100%");

				auto c = tbl->GetCell(0, y++);
				LString msg = err ? LString::Fmt("error: %s", err.ToString().Get()) : "Success";
				c->Padding("1em");
				c->TextAlign(LCss::AlignCenter);
				c->Add(label = new LTextLabel(ID_STATIC, 0, 0, -1, -1, msg));
				
				c = tbl->GetCell(0, y++);
				c->Padding("1em");
				c->TextAlign(LCss::AlignCenter);
				c->Add(btn = new LButton(ID_LOGOUT, 0, 0, -1, -1, "Log Out"));
			}
			else // regular non-priv UI
			{		 	
				tbl->GetCss(true)->BackgroundColor(colBack);
				
				auto type = LGetEnv("XDG_SESSION_TYPE");
				auto desktop = LGetEnv("XDG_CURRENT_DESKTOP");
				
				bool x11 = type.Lower().Find("x11") >= 0;
				bool wayland = type.Lower().Find("wayland") >= 0;
				LColour col;
				if (wayland)
					col = colWayland;
				else if (x11)
					col = colX11;
				else
					col = LColour(L_MED);
				
				auto c = tbl->GetCell(0, y++, true, 2);
				c->BackgroundColor(col);
				c->TextAlign(LCss::AlignCenter);
				c->Padding("1em");
				c->Add(label = new LTextLabel(ID_STATIC, 0, 0, -1, -1, type));
					auto css = label->GetCss(true);
					css->BackgroundColor(col);
					css->Color(LColour::White);
					css->FontSize("18pt");
					label->OnStyleChange();
				
				c = tbl->GetCell(0, y++, true, 2);
				c->BackgroundColor(col);
				c->TextAlign(LCss::AlignCenter);
				c->Padding("1em");
				c->Add(label = new LTextLabel(	ID_STATIC, 0, 0, -1, -1,
												LString("Desktop: ") + desktop.Replace(":",", ")));
					label->GetCss(true)->BackgroundColor(col);
					label->GetCss()->Color(LColour::White);
					label->OnStyleChange();
				
				c = tbl->GetCell(0, y);
				c->Width("50%");
				c->Padding("1em");
				c->TextAlign(LCss::AlignCenter);
				c->BackgroundColor(colX11);
				c->Add(btn = new LButton(ID_X11, 0, 0, -1, -1, "X11"));
				btn->GetCss(true)->NoPaintColor(colX11);
				if (x11)
					btn->Enabled(false);
				
				c = tbl->GetCell(1, y++);
				c->Width("50%");
				c->Padding("1em");
				c->TextAlign(LCss::AlignCenter);
				c->BackgroundColor(colWayland);
				c->Add(btn = new LButton(ID_WAYLAND, 0, 0, -1, -1, "Wayland"));
				btn->GetCss(true)->NoPaintColor(colWayland);
				if (wayland)
					btn->Enabled(false);
			}
			
			AttachChildren();
			Visible(true);
		}
	}
	
   	
	void SetWayland(bool wayland)
	{
		/*
		auto dataPath = getDataFilePath();
		
		LJson j;
		j.Set("desktop", wayland ? "wayland" : "x11");
		
		LFile out;
		if (!out.Open(dataPath, O_WRITE))
		{
			LgiMsg(this, "Can't open '%s' for writing.", AppName, MB_OK, dataPath.Get());
			return;
		}

		out.SetSize(0);
		out.Write(j.GetJson());
		out.Close();
		*/

		LFile::Path p(LSP_EXE);
		p += "setDesktop.py";
		auto cmd = LString::Fmt("pkexec python \"%s\" %s",
			p.GetFull().Get(),
			wayland ? "wayland" : "x11");
		printf("cmd=%s\n", cmd.Get());
		auto r = system(cmd);
		printf("sys=%i\n", r);
	}

	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		switch (Ctrl->GetId())
		{
			case ID_X11:
			{
				SetWayland(false);
				break;
			}
			case ID_WAYLAND:
			{
				SetWayland(true);
				break;
			}
			case ID_LOGOUT:
			{
				system("pkexec --user matthew gnome-session-quit");
				break;
			}
		}
		
		return LWindow::OnNotify(Ctrl, n);
	}
};

//////////////////////////////////////////////////////////////////
void Setup(bool install)
{
	printf("%s %s...\n", __func__, install?"install":"uninstall");
	LFile::Path policyDest(sPolicyFolder);
	policyDest += sPolicyFile;
	auto destPath = policyDest.GetFull();
	
	if (install)
	{
		LFile::Path policySrc(LSP_APP_INSTALL);
		policySrc += sPolicyFile;
		auto srcPath = policySrc.GetFull();
		
		if (policySrc.Exists())
		{
			if (auto xml = LReadFile(srcPath))
			{
				LFile::Path appPath(LSP_EXE);
				auto dataPath = getDataFilePath();
				if (!dataPath)
					printf("...can't figure out suitable data path\n");
				else
				{
					printf("...getDataFilePath='%s'\n", dataPath.Get());
					xml = xml.Replace("${appPath}", appPath.GetFull());
					xml = xml.Replace("${dataPath}", dataPath);
					
					LFile out(destPath, O_WRITE);
					if (out)
					{
						if (out.Write(xml))
							printf("...success, wrote: %s\n", destPath.Get());
						else
							printf("...can't write to '%s'\n", destPath.Get());
					}
					else printf("...can't open '%s' for writing\n", destPath.Get());
				}
			}
			else printf("...can't read xml '%s'.\n", srcPath.Get());
		}
		else printf("...'%s' doesn't exist.\n", srcPath.Get());
	}
	else
	{
		if (policyDest.Exists())
		{
			if (FileDev->Delete(policyDest, nullptr, false))
				printf("...success.\n");
			else
				printf("...error deleting: %s\n", destPath.Get());
		}
		else printf("...'%s' doesn't exist.\n", destPath.Get());
	}
}

int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		// Command line process:
		if (a.GetOption("help"))
			printf("%s [command]\n"
				"\n"
				"Commands:\n"
				"	install - run as root to install the policy.\n"
				"	uninstall - run as root to uninstall the policy.\n"
				"	setDesktop - change the desktop setting and reboot.\n", AppArgs.Arg[0]);
		else if (a.GetOption("install"))
			Setup(true);
		else if (a.GetOption("uninstall"))
			Setup(false);
		else
		{
			LString dataPath;
			a.GetOption("setDesktop", dataPath);
			
			// Run GUI
			a.AppWnd = new App(dataPath);
			a.Run();
		}
	}

	return 0;
}
