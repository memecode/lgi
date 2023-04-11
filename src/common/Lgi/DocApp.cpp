/*hdr
**      FILE:           LDocApp.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           20/4/01
**      DESCRIPTION:    Generic Document Application Impl
**
*/

#include <stdlib.h>
#include "lgi/common/DocApp.h"
#include "lgi/common/Token.h"
#include "lgi/common/OptionsFile.h"
#ifdef HAS_PROPERTIES
#include "GProperties.h"
#endif
#include "lgi/common/LgiRes.h"
#include "lgi/common/Menu.h"

//////////////////////////////////////////////////////////////////////////////////////////
class LDocAppPrivate
{
public:
	// Data
	LWindow				*App;
	LString				OptionsFile;
	LString				OptionsParam;
	char				*AppName;
	LString				CurFile;
	bool				Dirty;
	GDocAppInstallMode	Mode;

	LDocAppPrivate(LWindow *app, char *param)
	{
		App = app;
		OptionsParam = param;
		AppName = 0;
		Dirty = 0;
		Mode = InstallPortable;
	}
	
	~LDocAppPrivate()
	{
		// Release memory
		DeleteArray(AppName);
	}
	
	LString GetOptionsFile(const char *Ext)
	{
		// Get options file
		LString Status;
		char Opt[MAX_PATH_LEN];
		if (LAppInst->GetOption("o", Opt, sizeof(Opt)))
		{
			if (LFileExists(Opt))
				Status = Opt;
		}

		if (!Status)
		{
			auto Exe = LGetExeFile();				
			char *File = strrchr(Exe, DIR_CHAR);
			if (File)
			{
				File++;

				Status = File;
				#ifdef WIN32
				auto Dot = Status.RFind(".");
				if (Dot >= 0)
				{
					Status = Status(0, Dot+1) + Ext;
				}
				else
				#endif
				{
					// unix apps have no '.' in their name
					Status += LString(".") + Ext;
				}
				
				char p[MAX_PATH_LEN];
				if (Mode == InstallPortable)
				{
					LGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
				}
				else
				{
					if (LGetSystemPath(LSP_APP_ROOT, p, sizeof(p)) &&
						!LDirExists(p))
					{
						FileDev->CreateFolder(p);
					}
				}

				LMakePath(p, sizeof(p), p, Status);
				Status = p;
			}
		}

		return Status;
	}

	///////////////////////////////////////////////////////////////////////////
	// Xml Options
	const char *GetExtension(LOptionsFile *p)
	{
		return "xml";
	}

	bool SetOpt(LOptionsFile *p, const char *opt, char *str)
	{
		LVariant v = str;
		return p ? p->SetValue(opt, v) : false;
	}

	bool GetOpt(LOptionsFile *p, const char *opt, LVariant &v)
	{
		return p ? p->GetValue(opt, v) : false;
	}

	bool SerializeOpts(LOptionsFile *p, bool Write)
	{
		if (!p) return false;

		p->SetFile(OptionsFile);
		bool Result = p->SerializeFile(Write);
		if (Write && !Result)
		{
			// Probably because we don't have write access to the install folder?
			LgiMsg(	App,
					"Failed to write options to '%s' (mode=%s)",
					App->Name(),
					MB_OK,
					OptionsFile.Get(),
					Mode == InstallPortable ? "Portable" : "Desktop");
		}
		
		return Result;
	}

	////////////////////////////////////////////////////////////////////////////
	// Prop List Options
	#ifdef __PROP_H
	char *GetExtension(ObjProperties *p)
	{
		return "r";
	}

	bool SetOpt(ObjProperties *p, char *opt, char *str)
	{
		return p ? p->Set(opt, str) : false;
	}

	bool GetOpt(ObjProperties *p, char *opt, LVariant &v)
	{
		char *str;
		if (p && p->Get(opt, str))
		{
			v = str;
			return true;
		}

		return false;
	}

	bool SerializeOpts(ObjProperties *p, bool Write)
	{
		bool Status = false;
		if (p)
		{
			LFile f;
			if (f.Open(OptionsFile, Write?O_WRITE:O_READ))
			{
				if (Write)
					f.SetSize(0);

				Status = p->Serialize(f, Write);
			}
		}
		return Status;
	}
	#endif
};

//////////////////////////////////////////////////////////////////////////////////////////
template <typename OptionsFmt>
LDocApp<OptionsFmt>::LDocApp(const char *appname, LIcon icon, char *optsname)
{
	Options = 0;
	_LangOptsName = 0;
	d = new LDocAppPrivate(this, optsname);
	
	LRect r(0, 0, 800, 600);
	SetPos(r);
	MoveToCenter();
	_FileMenu = 0;
	d->AppName = NewStr(appname?appname:(char*)"Lgi.LDocApp");

	char p[MAX_PATH_LEN];
	if (LGetSystemPath(LSP_APP_INSTALL, p, sizeof(p)))
	{
		LMakePath(p, sizeof(p), p, "_write_test.txt");
		LFile f;
		if (!f.Open(p, O_WRITE))
		{
			d->Mode = InstallDesktop;
		}
		else
		{
			f.Close();
			FileDev->Delete(p, false);
		}
	}

	SetQuitOnClose(true);

	// Setup class
	if (icon)
	{
		#if defined WIN32
			LWindowsClass *c = LWindowsClass::Create(d->AppName);
			if (c)
			{
				if (icon < 0x10000)
					c->Class.hIcon = LoadIcon(LProcessInst(), MAKEINTRESOURCE(icon));
				else
					c->Class.hIcon = LoadIcon(LProcessInst(), (TCHAR*)(size_t)icon);
			}
		#else
			SetIcon(icon);
		#endif
	}
}

template <typename OptionsFmt>
LDocApp<OptionsFmt>::~LDocApp()
{
	DeleteObj(d);
	DeleteObj(Options);
}

template <typename OptionsFmt>
GDocAppInstallMode LDocApp<OptionsFmt>::GetInstallMode()
{
	return d->Mode;
}

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::SetLanguage(char *LangId)
{	
	if (!_LangOptsName)
		return false;
	
	if (LgiMsg(	this,
				LLoadString(L_DOCAPP_RESTART_APP, "Changing the language requires restarting the application.\nDo you want to restart?"),
				d->AppName,
				MB_YESNO) != IDYES)
		return false;

	LVariant v;
	GetOptions()->SetValue(_LangOptsName, v = LangId);
	GetOptions()->SerializeFile(true);
	LCloseApp();
	LExecute(LGetExeFile());
	return true;
}

template <typename OptionsFmt>
char *LDocApp<OptionsFmt>::GetOptionsFileName()
{
	return d->OptionsFile;
}

template <typename OptionsFmt>
void LDocApp<OptionsFmt>::_Close()
{
	Empty();
	SetCurFile(0);
}

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::_DoSerialize(bool Write)
{
	bool Status = false;

	if (!d->OptionsFile)
	{
		const char *Ext = d->GetExtension(Options);
		d->OptionsFile = d->GetOptionsFile(Ext);
	}

	if (!Options)
	{
		if (LFileExists(d->OptionsFile))
			Options = new OptionsFmt(d->OptionsFile);
		else
			Options = new OptionsFmt(LOptionsFile::PortableMode, d->OptionsParam);
	}

	if (Write)
	{
		// Save window position
		SerializeState(Options, "Pos", false);

		// save misc options
		SerializeOptions(Options, true);

		LOptionsFile *Of = dynamic_cast<LOptionsFile*>(Options);
		if (Of) Of->CreateTag("Mru");
		LMru::Serialize(Options, "Mru", true);
	}

	// do the work
	Status = _SerializeFile(Write);

	if (!Write)
	{
		LVariant Lang;
		if (_LangOptsName &&
			Options->GetValue(_LangOptsName, Lang))
		{
			LAppInst->SetConfig("language", Lang.Str());
		}

		// read misc options
		LMru::Serialize(Options, "Mru", false);
		SerializeOptions(Options, false);

		// window pos
		SerializeState(Options, "Pos", true);
	}

	return Status;
 }

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::_SerializeFile(bool Write)
{
	return d->SerializeOpts(Options, Write);
}

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::_Create(LIcon IconResource)
{
	// Load options
	_DoSerialize(false);

	// Create and setup the main application window
	#ifdef WIN32
	HICON hIcon = NULL;
	if (IconResource)
		hIcon = LoadIcon(LProcessInst(), MAKEINTRESOURCE(IconResource));
	CreateClassW32(d->AppName, hIcon);
	#endif

	if (Attach(0))
	{
		Name(d->AppName);
	
		if (X() < 1)
		{	
			LRect r(100, 100, 600, 500);
			SetPos(r);
		}
	}
	else
	{
		LAssert(!"Window create failed.");
		return false;
	}

	return true;
}

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::_Destroy()
{
	// Save options
	_DoSerialize(true);
	LAppInst->AppWnd = 0;
	return true;
}

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::_LoadMenu(const char *Resource, const char *Tags, int FileMenuId, int RecentMenuId)
{
	if ((Menu = new LMenu))
	{
		Menu->Attach(this);

		if (Resource)
		{
			Menu->Load(this, Resource, Tags);
			if (FileMenuId >= 0)
				_FileMenu = Menu->FindSubMenu(FileMenuId);
			else
				_FileMenu = Menu->AppendSub("&File", 0);
		}
		else
		{
			_FileMenu = Menu->AppendSub("&File");
		}

		if (_FileMenu)
		{
			int Idx = 0;
			if (!_FileMenu->FindItem(IDM_OPEN))
				_FileMenu->AppendItem("&Open", IDM_OPEN, true, Idx++, "CtrlCmd+O");

			if (!_FileMenu->FindItem(IDM_SAVE))
				_FileMenu->AppendItem("&Save", IDM_SAVE, true, Idx++, "CtrlCmd+S");
			
			if (!_FileMenu->FindItem(IDM_SAVEAS))
				_FileMenu->AppendItem("Save &As", IDM_SAVEAS, true, Idx++);
			
			if (!_FileMenu->FindItem(IDM_CLOSE))
			{
				_FileMenu->AppendItem("Close", IDM_CLOSE, true, Idx++, "CtrlCmd+W");
				_FileMenu->AppendSeparator(Idx++);
			}

			LSubMenu *Recent = NULL;
			if (RecentMenuId >= 0)
				Recent = _FileMenu->FindSubMenu(RecentMenuId);
			else
				Recent = _FileMenu->AppendSub("Recent...", Idx++);
			if (Recent)
			{
				Set(Recent);
				//_FileMenu->AppendSeparator();
				LMru::Serialize(Options, "Mru", false);
			}

			if (!_FileMenu->FindItem(IDM_EXIT))
				_FileMenu->AppendItem("&Quit", IDM_EXIT, true, -1, "CtrlCmd+Q");
		}
	}

	return Menu != 0;
}

template <typename OptionsFmt>
void LDocApp<OptionsFmt>::_OpenFile(const char *File, bool ReadOnly, std::function<void(bool)> Callback)
{
	SetDirty(false, [this, Callback, ReadOnly, Fn = LString(File)](auto status) mutable
	{
		if (!status)
		{
			if (Callback)
				Callback(false);
			return;
		}

		char RealPath[MAX_PATH_LEN];
		if (LResolveShortcut(Fn, RealPath, sizeof(RealPath)))
			Fn = RealPath;

		LMru::_OpenFile(Fn, ReadOnly, [this, Callback, Fn](auto ok)
		{
			if (ok)
			{
				d->Dirty = false;
				SetCurFile(Fn);
			}

			if (Callback)
				Callback(ok);
		});
	});
}

template <typename OptionsFmt>
void LDocApp<OptionsFmt>::_SaveFile(const char *File, std::function<void(LString, bool)> Callback)
{
	char RealPath[MAX_PATH_LEN];
	if (LResolveShortcut(File, RealPath, sizeof(RealPath)))
	{
		File = RealPath;
	}
	else
	{
		strcpy_s(RealPath, sizeof(RealPath), File);
	}

	LMru::_SaveFile(RealPath, [this, Callback](auto FileName, auto Status)
	{
		if (Status)
		{
			d->Dirty = false;
			SetCurFile(FileName);
			OnDirty(d->Dirty);
		}

		if (Callback)
			Callback(FileName, Status);
	});
}

template <typename OptionsFmt>
char *LDocApp<OptionsFmt>::GetAppName()
{
	return d->AppName;
}

template <typename OptionsFmt>
void LDocApp<OptionsFmt>::SetCurFile(const char *f)
{
	if (!d->CurFile.Equals(f))
	{
		d->CurFile = f;
		if (f)
			AddFile(f);
	}

	LString Display;
	if (SerializeEntry(&Display, &d->CurFile, NULL))
	{
		char s[MAX_PATH_LEN + 100];
		
		if (Display)
		{
			sprintf_s(s, sizeof(s), "%s [%s%s]", d->AppName, Display.Get(), d->Dirty ? " changed" : "");
		}
		else if (d->Dirty)
		{
			sprintf_s(s, sizeof(s), "%s [changed]", d->AppName);
		}
		else
		{
			strcpy_s(s, sizeof(s), d->AppName);
		}

		Name(s);
	}
}

template <typename OptionsFmt>
const char *LDocApp<OptionsFmt>::GetCurFile()
{
	return d->CurFile;
}

template <typename OptionsFmt>
void LDocApp<OptionsFmt>::SetDirty(bool Dirty, std::function<void(bool)> Callback)
{
	if (IsAttached() && (d->Dirty ^ Dirty))
	{
		// Changing...
		if (Dirty)
		{
			// Setting dirty
			d->Dirty = true;
			SetCurFile(d->CurFile);
		}
		else
		{
			// Clearing dirty
			int Result = LgiMsg(this,
								LLoadString(L_DOCAPP_SAVE_CHANGE, "Do you want to save your changes?"),
								d->AppName,
								MB_YESNOCANCEL);
			if (Result == IDYES)
			{
				if (!ValidStr(d->CurFile))
				{
					LMru::OnCommand(IDM_SAVEAS, [this](auto status)
					{
						if (status)
						{
							d->Dirty = false;
							SetCurFile(d->CurFile);
						}
					});

					if (Callback)
						Callback(false);
					return;
				}
				else
				{
					_SaveFile(d->CurFile, [this, Callback](auto fn, auto status)
					{
						if (status)
						{
							d->Dirty = false;
							SetCurFile(fn);
							OnDirty(d->Dirty);
						}

						if (Callback)
							Callback(status);
					});
					return;
				}
			}
			else if (Result == IDCANCEL)
			{
				if (Callback)
					Callback(false);
				return;
			}

			d->Dirty = false;
			SetCurFile(d->CurFile);
		}

		OnDirty(d->Dirty);
	}

	if (Callback)
		Callback(true);
}

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::GetDirty()
{
	return d->Dirty;
}

template <typename OptionsFmt>
OptionsFmt *LDocApp<OptionsFmt>::GetOptions()
{
	return Options;
}

template <typename OptionsFmt>
void LDocApp<OptionsFmt>::OnReceiveFiles(LArray<const char*> &Files)
{
	if (Files.Length() == 0)
		return;

	LString f = Files[0];
	if (!f)
		return;

	_OpenFile(f, false, [this, f](auto ok)
	{
		if (ok)
			LMru::AddFile(f);
	});
}

template <typename OptionsFmt>
bool LDocApp<OptionsFmt>::OnRequestClose(bool OsShuttingDown)
{
	if (GetDirty())
	{
		SetDirty(false, [this](auto ok)
		{
			if (ok)
				LCloseApp();
		});

		// Wait for the SetDirty callback to exit...
		return false;
	}

	return LWindow::OnRequestClose(OsShuttingDown);
}

template <typename OptionsFmt>
int LDocApp<OptionsFmt>::OnCommand(int Cmd, int Event, OsView Window)
{
	switch (Cmd)
	{
		case IDM_SAVE:
		{
			if (!GetCurFile())
			{
				LMru::OnCommand(IDM_SAVEAS, NULL);
				return 0;
			}
			else
			{
				_SaveFile(GetCurFile(), NULL);
				return 0;
			}
			break;
		}
		case IDM_CLOSE:
		{
			SetDirty(false, [this](auto ok)
			{
				if (ok)
					_Close();
			});
			return 0;;
		}
		case IDM_EXIT:
		{
			LCloseApp();
			break;
		}
	}

	LMru::OnCommand(Cmd, NULL);
	return 0;
}

template <typename OptionsFmt>
LMessage::Result LDocApp<OptionsFmt>::OnEvent(LMessage *m)
{
	LMru::OnEvent(m);

	/*
	switch (MsgCode(m))
	{
		#ifdef WIN32
		case WM_CLOSE:
		{
			if (!SetDirty(false))
			{
				return 0;
			}
			break;
		}
		#endif
	}
	*/

	return LWindow::OnEvent(m);
}

template class LDocApp<LOptionsFile>;
#ifdef __PROP_H
template class LDocApp<ObjProperties>;
#endif
