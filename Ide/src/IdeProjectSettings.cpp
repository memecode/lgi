#include "lgi/common/Lgi.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/Combo.h"
#include "lgi/common/Button.h"
#include "lgi/common/FileSelect.h"

#include "LgiIde.h"
#include "IdeProject.h"
#include "ProjectNode.h"
#include "resdefs.h"

const char TagSettings[] = "Settings";

const char sRemote[] = "Remote";
const char sGeneral[] = "General";
const char sBuild[] = "Build";
const char sEditor[] = "Editor";
const char sAdvanced[] = "Advanced";
const char sDebug[] = "Debug";
const char sRelease[] = "Release";
const char sAllPlatforms[] = "All";
const char sCurrentPlatform[] =
	#if defined WIN32
	"Windows"
	#elif defined LINUX
	"Linux"
	#elif defined MAC
	"Mac"
	#elif defined HAIKU
	"Haiku"
	#else
	#error "Not impl"
	#endif
	;

const char *sCompilers[] =
{
	#if defined WIN32
	"VisualStudio",
	"Cygwin",
	"MingW",
	"IAR",
	#elif defined MAC
	"XCode",
	#else
	"gcc",
	"cross",
	#endif
	NULL
};

const char *sBuildTypes[] =
{
	"Executable",
	"StaticLibrary",
	"DynamicLibrary",
	NULL
};

static const char **GetEnumValues(ProjSetting s)
{
	switch (s)
	{
		case ProjCompiler:
			return sCompilers;
		case ProjTargetType:
			return sBuildTypes;
		default:
			LAssert(!"Unknown enum type.");
			break;
	}
	
	return NULL;
}

#define SF_MULTILINE			0x01	// String setting is multiple lines, otherwise it's single line
#define SF_CROSSPLATFORM		0x02	// Just has a "all" platforms setting (no platform specific)
#define SF_PLATFORM_SPECIFC		0x04	// Just has a platform specific setting (no all)
#define SF_CONFIG_SPECIFIC		0x08	// Can have a different setting in different configs
#define SF_ENUM					0x10	// Integer is actually an enum index, not a straight number
#define SF_FILE_SELECT			0x20	// UI needs a file selection button
#define SF_FOLDER_SELECT		0x40	// UI needs a file selection button
#define SF_PASSWORD				0x80	// UI needs to hide the characters of the password

#define IDC_TEXT_BASE			100
#define IDC_EDIT_BASE			200
#define IDC_CHECKBOX_BASE		300
#define IDC_COMBO_BASE			400
#define IDC_BROWSE_FILE			500
#define IDC_BROWSE_FOLDER		600

struct SettingInfo
{
	struct BitFlags
	{
		uint32_t MultiLine : 1;
		uint32_t CrossPlatform : 1;
		uint32_t PlatformSpecific : 1;
		uint32_t ConfigSpecific : 1;
		uint32_t Enum : 1;
		uint32_t FileSelect : 1;
		uint32_t FolderSelect : 1;
		uint32_t IsPassword : 1;
	};
	
	ProjSetting Setting;
	int Type;
	const char *Name;
	const char *Category;
	union {
		uint32_t Flags;
		BitFlags Flag;
	};
};

SettingInfo AllSettings[] =
{
	{ProjRemoteUri,				GV_STRING,		"RemoteUri",		sRemote,	{SF_CROSSPLATFORM}},
	{ProjRemotePass,			GV_STRING,		"RemotePass",		sRemote,	{SF_CROSSPLATFORM|SF_PASSWORD}},
	
	{ProjMakefile,				GV_STRING,		"Makefile",			sGeneral,	{SF_PLATFORM_SPECIFC|SF_FILE_SELECT}},
	{ProjExe,					GV_STRING,		"Executable",		sGeneral,	{SF_PLATFORM_SPECIFC|SF_CONFIG_SPECIFIC|SF_FILE_SELECT}},
	{ProjCompiler,				GV_INT32,		"Compiler",			sGeneral,	{SF_PLATFORM_SPECIFC|SF_ENUM}},
	{ProjCCrossCompiler,		GV_STRING,		"CCrossCompiler",	sGeneral,	{SF_PLATFORM_SPECIFC|SF_FILE_SELECT}},
	{ProjCppCrossCompiler,		GV_STRING,		"CppCrossCompiler",	sGeneral,	{SF_PLATFORM_SPECIFC|SF_FILE_SELECT}},

	{ProjEnv,					GV_STRING,		"Environment",		sDebug,		{SF_CROSSPLATFORM|SF_MULTILINE}},
	{ProjArgs,					GV_STRING,		"Arguments",		sDebug,		{SF_CROSSPLATFORM|SF_CONFIG_SPECIFIC}},
	{ProjDebugAdmin,			GV_BOOL,		"DebugAdmin",		sDebug,		{SF_CROSSPLATFORM}},
	{ProjInitDir,				GV_STRING,		"InitialDir",		sDebug,		{SF_CROSSPLATFORM|SF_FOLDER_SELECT}},
	
	{ProjDefines,				GV_STRING,		"Defines",			sBuild,		{SF_MULTILINE|SF_CONFIG_SPECIFIC}},
	{ProjIncludePaths,			GV_STRING,		"IncludePaths",		sBuild,		{SF_MULTILINE|SF_CONFIG_SPECIFIC}},
	{ProjSystemIncludes,		GV_STRING,		"SystemIncludes",	sBuild,		{SF_MULTILINE|SF_CONFIG_SPECIFIC|SF_PLATFORM_SPECIFC}},
	{ProjLibraries,				GV_STRING,		"Libraries",		sBuild,		{SF_MULTILINE|SF_CONFIG_SPECIFIC}},
	{ProjLibraryPaths,			GV_STRING,		"LibraryPaths",		sBuild,		{SF_MULTILINE|SF_CONFIG_SPECIFIC}},
	{ProjTargetType,			GV_INT32,		"TargetType",		sBuild,		{SF_CROSSPLATFORM|SF_ENUM}},
	{ProjTargetName,			GV_STRING,		"TargetName",		sBuild,		{SF_PLATFORM_SPECIFC|SF_CONFIG_SPECIFIC}},
	{ProjApplicationIcon,		GV_STRING,		"ApplicationIcon",	sBuild,		{SF_PLATFORM_SPECIFC|SF_FILE_SELECT}},
	
	{ProjEditorTabSize,			GV_INT32,		"TabSize",			sEditor,	{SF_CROSSPLATFORM}},
	{ProjEditorIndentSize,		GV_INT32,		"IndentSize",		sEditor,	{SF_CROSSPLATFORM}},
	{ProjEditorShowWhiteSpace,	GV_BOOL,		"ShowWhiteSpace",	sEditor,	{SF_CROSSPLATFORM}},
	{ProjEditorUseHardTabs,		GV_BOOL,		"UseHardTabs",		sEditor,	{SF_CROSSPLATFORM}},
	{ProjCommentFile,			GV_STRING,		"CommentFile",		sEditor,	{SF_MULTILINE|SF_CROSSPLATFORM}},
	{ProjCommentFunction,		GV_STRING,		"CommentFunction",	sEditor,	{SF_MULTILINE|SF_CROSSPLATFORM}},
	
	{ProjMakefileRules,			GV_STRING,		"OtherMakefileRules", sAdvanced, {SF_MULTILINE}},
	{ProjPostBuildCommands,		GV_STRING,		"PostBuildCommands", sAdvanced, {SF_PLATFORM_SPECIFC|SF_CONFIG_SPECIFIC|SF_MULTILINE}},
	
	{ProjNone,					GV_NULL,		NULL,				NULL,		{0}},
};

static void ClearEmptyTags(LXmlTag *t)
{
	for (int i=0; i<t->Children.Length(); i++)
	{
		LXmlTag *c = t->Children[i];
		if (!c->GetContent() && !c->Children.Length())
		{
			c->RemoveTag();
			i--;
			DeleteObj(c);
		}
		else
		{
			ClearEmptyTags(c);
		}
	}
}

struct IdeProjectSettingsPriv
{
	char PathBuf[MAX_PATH_LEN];

public:
	IdeProject *Project;
	LHashTbl<IntKey<int>, SettingInfo*> Map;
	IdeProjectSettings *Parent;
	LXmlTag Active;
	LXmlTag Editing;
	LString CurConfig;
	LString::Array Configs;
	
	LAutoString StrBuf; // Temporary storage for generated settings
	
	IdeProjectSettingsPriv(IdeProjectSettings *parent) :
		Active(TagSettings)
	{
		Parent = parent;
		Project = NULL;
		
		for (SettingInfo *i = AllSettings; i->Setting; i++)
		{
			Map.Add(i->Setting, i);
		}
		
		Configs.New() = sDebug;
		Configs.New() = sRelease;
		CurConfig = sDebug;
	}
	
	~IdeProjectSettingsPriv()
	{
	}

	int FindConfig(const char *Cfg)
	{
		for (int i=0; i<Configs.Length(); i++)
		{
			if (Configs[i].Equals(Cfg))
				return i;
		}
		return -1;
	}
	
	int CurConfigIdx()
	{
		return FindConfig(CurConfig);
	}

	const char *PlatformToString(int Flags, IdePlatform Platform)
	{
		if (Flags & SF_PLATFORM_SPECIFC)
		{
			if (Platform >= 0 && Platform < PlatformMax)
			{
				return PlatformNames[Platform];
			}
			
			return sCurrentPlatform;
		}
		
		return sAllPlatforms;
	}

	char *BuildPath(ProjSetting s, int Flags, IdePlatform Platform, int Config = -1)
	{
		SettingInfo *i = Map.Find(s);
		LAssert(i);

		const char *PlatformStr = PlatformToString(Flags, Platform);
		int Ch = sprintf_s(	PathBuf, sizeof(PathBuf),
							"%s.%s.%s",
							i->Category,
							i->Name,
							PlatformStr);

		if (i->Flag.ConfigSpecific)
		{
			sprintf_s(	PathBuf+Ch, sizeof(PathBuf)-Ch,
						".%s",
						Config >= 0 ? Configs[Config].Get() : CurConfig.Get());
		}
		
		return PathBuf;
	}
};

class LSettingDetail : public LLayout, public ResObject
{
	LTableLayout *Tbl;
	IdeProjectSettingsPriv *d;
	SettingInfo *Setting;
	int Flags;
	
	struct CtrlInfo
	{
		LTextLabel *Text;
		LEdit *Edit;
		LCheckBox *Chk;
		LCombo *Cbo;
		
		CtrlInfo()
		{
			Text = NULL;
			Edit = NULL;
			Chk = NULL;
			Cbo = NULL;
		}
	};
	
	LArray<CtrlInfo> Ctrls;

public:
	LSettingDetail() : ResObject(Res_Custom)
	{
		Flags = 0;
		d = NULL;
		Setting = NULL;
		AddView(Tbl = new LTableLayout);
	}
	
	void OnCreate()
	{
		AttachChildren();
	}
	
	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(L_MED);
		pDC->Rectangle();
	}
	
	bool OnLayout(LViewLayoutInfo &Inf)
	{
		Inf.Width.Min = -1;
		Inf.Width.Max = -1;
		Inf.Height.Min = -1;
		Inf.Height.Max = -1;
		return true;
	}
	
	void SetPriv(IdeProjectSettingsPriv *priv)
	{
		d = priv;
	}
	
	void AddLine(int i, int Config)
	{
		char *Path;
		int CellY = i * 2;
		
		// Do label cell
		auto *c = Tbl->GetCell(0, CellY);
		c->Add(Ctrls[i].Text = new LTextLabel(IDC_TEXT_BASE + i, 0, 0, -1, -1, Path = d->BuildPath(Setting->Setting, Flags, PlatformCurrent, Config)));
		
		// Do value cell
		c = Tbl->GetCell(0, CellY + 1);

		LXmlTag *t = d->Editing.GetChildTag(Path);
		if (Setting->Type == GV_STRING)
		{
			Ctrls[i].Edit = new LEdit(IDC_EDIT_BASE + i, 0, 0, 60, 20);
			Ctrls[i].Edit->MultiLine(Setting->Flag.MultiLine);
			Ctrls[i].Edit->Password(Setting->Flag.IsPassword);
			if (t && t->GetContent())
				Ctrls[i].Edit->Name(t->GetContent());
			
			c->Add(Ctrls[i].Edit);
			if (Setting->Flag.FileSelect ||	
				Setting->Flag.FolderSelect)
			{
				c = Tbl->GetCell(1, CellY + 1);
				int Base = Setting->Flag.FileSelect ? IDC_BROWSE_FILE : IDC_BROWSE_FOLDER;
				if (c)
					c->Add(new LButton(Base + i, 0, 0, -1, -1, "..."));
				else
					LgiTrace("%s:%i - No cell.\n", _FL);
			}
		}
		else if (Setting->Type == GV_INT32)
		{
			if (Setting->Flag.Enum)
			{
				// Enum setting
				c->Add(Ctrls[i].Cbo = new LCombo(IDC_COMBO_BASE + i, 0, 0, 60, 20));
				
				const char **Init = GetEnumValues(Setting->Setting);
				if (Init)
				{
					for (int n=0; Init[n]; n++)
					{
						Ctrls[i].Cbo->Insert(Init[n]);
						if (t && t->GetContent() && !stricmp(t->GetContent(), Init[n]))
							Ctrls[i].Cbo->Value(n);
					}
				}
			}
			else
			{
				// Straight integer
				c->Add(Ctrls[i].Edit = new LEdit(IDC_EDIT_BASE + i, 0, 0, 60, 20));
				if (t)
					Ctrls[i].Edit->Value(t->GetContent() ? atoi(t->GetContent()) : 0);
			}
		}
		else if (Setting->Type == GV_BOOL)
		{
			c->Add(Ctrls[i].Chk  = new LCheckBox(IDC_CHECKBOX_BASE + i, 0, 0, -1, -1, NULL));
			if (t && t->GetContent())
				Ctrls[i].Chk->Value(atoi(t->GetContent()));
		}
		else LAssert(!"Unknown type?");
	}
	
	void SetSetting(SettingInfo *setting, int flags)
	{
		if (Setting)
		{
			// Save previous values
			for (int i=0; i<Ctrls.Length(); i++)
			{
				if (!Ctrls[i].Text)
					continue;
				
				auto Path = Ctrls[i].Text->Name();
				if (!Path)
				{
					LAssert(0);
					continue;
				}

				LXmlTag *t = d->Editing.GetChildTag(Path, true);
				if (!t)
				{
					LAssert(0);
					continue;
				}

				if (Ctrls[i].Edit)
				{
					auto Val = Ctrls[i].Edit->Name();
					// LgiTrace("Saving edit setting '%s': '%s'\n", Path, Val);
					t->SetContent(Val);
				}
				else if (Ctrls[i].Chk)
				{
					int64 Val = Ctrls[i].Chk->Value();
					// LgiTrace("Saving bool setting '%s': "LGI_PrintfInt64"\n", Path, Val);
					t->SetContent((int)Val);
				}
				else if (Ctrls[i].Cbo)
				{
					auto Val = Ctrls[i].Cbo->Name();
					// LgiTrace("Saving enum setting '%s': '%s'\n", Path, Val);
					t->SetContent(Val);
				}
				else LAssert(0);
			}
		}	
	
		Tbl->Empty();
		Ctrls.Length(0);
		Setting = setting;
		Flags = flags;
		
		if (Setting)
		{
			if (Setting->Flag.ConfigSpecific)
			{
				for (int i=0; i<d->Configs.Length(); i++)
					AddLine(i, i);
			}
			else
			{
				AddLine(0, -1);
			}
			
			Tbl->InvalidateLayout();
			Tbl->AttachChildren();
			Tbl->SetPos(GetClient());
			Invalidate();
		}
	}
	
	void OnPosChange()
	{
		Tbl->SetPos(GetClient());
	}
};

class LSettingDetailFactory : public LViewFactory
{
	LView *NewView
	(
		const char *Class,
		LRect *Pos,
		const char *Text
	)
	{
		if (!stricmp(Class, "LSettingDetail"))
			return new LSettingDetail;
		return NULL;
	}
} SettingDetailFactory;

class ProjectSettingsDlg;
class SettingItem : public LTreeItem
{
	ProjectSettingsDlg *Dlg;
	
public:
	SettingInfo *Setting;
	int Flags;

	SettingItem(SettingInfo *setting, int flags, ProjectSettingsDlg *dlg)
	{
		Setting = setting;
		Dlg = dlg;
		Flags = flags;
	}
	
	void Select(bool b);
};

class ProjectSettingsDlg : public LDialog
{
	IdeProjectSettingsPriv *d;
	LTree *Tree;
	LSettingDetail *Detail;
	uint64 DefLockOut;

public:
	ProjectSettingsDlg(LViewI *parent, IdeProjectSettingsPriv *priv)
	{
		Tree = NULL;
		Detail = NULL;
		DefLockOut = 0;
		d = priv;
		SetParent(parent);
		if (LoadFromResource(IDD_PROJECT_SETTINGS))
		{
			MoveToCenter();
			
			auto FullPath = d->Project->GetFullPath();
			if (FullPath)
			{
				SetCtrlName(IDC_PATH, FullPath);
			}
			
			if (GetViewById(IDC_SETTINGS, Tree))
			{
				const char *Section = NULL;
				LTreeItem *SectionItem = NULL;
				for (SettingInfo *i = AllSettings; i->Setting; i++)
				{
					if (!SectionItem || (Section && stricmp(i->Category, Section)))
					{
						Section = i->Category;
						SectionItem = new LTreeItem();
						SectionItem->SetText(i->Category);
						Tree->Insert(SectionItem);
					}
					
					LTreeItem *Item = new LTreeItem();
					Item->SetText(i->Name);
					SectionItem->Insert(Item);
					SectionItem->Expanded(true);

					if (!i->Flag.PlatformSpecific)
					{
						SettingItem *All = new SettingItem(i, 0, this);
						All->SetText(sAllPlatforms);
						Item->Insert(All);
					}

					if (!i->Flag.CrossPlatform)
					{
						SettingItem *Platform = new SettingItem(i, SF_PLATFORM_SPECIFC, this);
						Platform->SetText(sCurrentPlatform);
						Item->Insert(Platform);
					}
				}
			}
			
			if (GetViewById(IDC_DETAIL, Detail))
			{
				Detail->SetPriv(d);
			}
			
			LView *Search;
			if (GetViewById(IDC_SEARCH, Search))
				Search->Focus(true);
		}
	}
	
	LTreeItem *FindItem(LTreeItem *i, const char *search)
	{
		const char *s = i->GetText(0);
		if (s && search && stristr(s, search))
			return i;
		
		for (LTreeItem *c = i->GetChild(); c; c = c->GetNext())
		{
			LTreeItem *f = FindItem(c, search);
			if (f)
				return f;
		}
		return NULL;
	}
	
	void OnSearch(const char *s)
	{
		if (!Tree) return;
		LTreeItem *f = NULL;
		for (LTreeItem *i = Tree->GetChild(); i && !f; i = i->GetNext())
		{
			f = FindItem(i, s);
		}
		if (f)
		{
			f->Select(true);
			for (LTreeItem *i = f; i; i = i->GetParent())
				i->Expanded(true);
			Tree->Focus(true);
		}
	}

	bool InsertPath(ProjSetting setting, IdePlatform platform, bool PlatformSpecific, LString newPath)
	{
		for (int Cfg = 0; Cfg < 2; Cfg++)
		{
			LString path = d->BuildPath(setting, PlatformSpecific ? SF_PLATFORM_SPECIFC : 0, platform, Cfg);
			auto t = d->Editing.GetChildTag(path);
			if (!t)
			{
				LgiTrace("%s:%i - Can't find element for '%s'\n", _FL, path.Get());
				return false;
			}
			
			LString nl("\n");
			auto lines = LString(t->GetContent()).Split(nl);
			bool has = false;
			for (auto ln: lines)
				if (ln.Equals(newPath))
					has = true;
					
			if (!has)
			{
				lines.SetFixedLength(false);
				lines.New() = newPath;
				auto newVal = nl.Join(lines);
				t->SetContent(newVal);
			}
		}
		
		return true;
	}
	
	void SetDefaults()
	{
		// Find path to Lgi...
		IdeProject *LgiProj = NULL;
		if (d->Project)
		{
			LArray<ProjectNode*> Nodes;
			if (d->Project->GetAllNodes(Nodes))
			{
				for (auto n: Nodes)
				{
					auto dep = n->GetDep();
					if (dep)
					{
						auto fn = LString(dep->GetFileName()).SplitDelimit(DIR_STR);
						if (fn.Last().Equals("Lgi.xml"))
						{
							LgiProj = dep;
							break;
						}
					}
				}
			}
		}
	
		#ifdef LINUX
		
		if (LgiProj)
		{
			auto lgiBase = LgiProj->GetBasePath();
			LFile::Path lgiInc(lgiBase, "include");

			LString lgiIncPath;
			if (!d->Project->RelativePath(lgiIncPath, lgiInc))
				lgiIncPath = lgiInc;

			LFile::Path lgiLinuxInc(lgiIncPath, "lgi/linux");
			LFile::Path lgiGtkInc(lgiIncPath, "lgi/linux/Gtk");

			InsertPath(ProjIncludePaths, PlatformLinux, false, lgiIncPath);
			InsertPath(ProjIncludePaths, PlatformLinux, true, lgiLinuxInc.GetFull());
			InsertPath(ProjIncludePaths, PlatformLinux, true, lgiGtkInc.GetFull());
			InsertPath(ProjSystemIncludes, PlatformLinux, true, "`pkg-config --cflags gtk+-3.0`");
			InsertPath(ProjLibraries, PlatformLinux, true, "pthread");
			InsertPath(ProjLibraries, PlatformLinux, true, "`pkg-config --libs gtk+-3.0`");
			InsertPath(ProjLibraries, PlatformLinux, true, "-static-libgcc");
		}
		
		#endif
	}
	
	int OnNotify(LViewI *Ctrl, LNotification n)
	{
		switch (Ctrl->GetId())
		{
			case IDC_BROWSE:
			{
				const char *s = GetCtrlName(IDC_PATH);
				if (s)
					LBrowseToFile(s);
				break;
			}
			case IDC_SEARCH:
			{
				if (n.Type == LNotifyReturnKey)
				{
					DefLockOut = LCurrentTime();
					OnSearch(GetCtrlName(IDC_SEARCH));
				}
				else
				{
					SetCtrlEnabled(IDC_CLEAR_SEARCH, ValidStr(Ctrl->Name()));
				}
				break;
			}
			case IDC_CLEAR_SEARCH:
			{
				SetCtrlName(IDC_SEARCH, NULL);
				OnSearch(NULL);
				break;
			}
			case IDOK:
			{
				if (LCurrentTime() - DefLockOut < 500)
					break;
				Detail->SetSetting(NULL, 0);
				EndModal(1);
				break;
			}
			case IDCANCEL:
			{
				if (LCurrentTime() - DefLockOut < 500)
					break;
				EndModal(0);
				break;
			}
			case IDC_DEFAULTS:
			{
				SetDefaults();
				return 0; // bypass default btn handler
			}
			default:
			{
				int Id = Ctrl->GetId();
				int BrowseIdx = -1;
				bool BrowseFolder = false;
				
				if (Id >= IDC_BROWSE_FILE && Id < IDC_BROWSE_FILE+100)
				{
					BrowseIdx = Id - IDC_BROWSE_FILE;
				}
				else if (Id >= IDC_BROWSE_FOLDER && Id < IDC_BROWSE_FOLDER+100)
				{
					BrowseIdx = Id - IDC_BROWSE_FOLDER;
					BrowseFolder = true;
				}
				
				if (BrowseIdx >= 0)
				{
					LEdit *e;
					if (GetViewById(IDC_EDIT_BASE + BrowseIdx, e))
					{
						LFileSelect *s = new LFileSelect;
						s->Parent(this);

						LFile::Path Path(d->Project->GetBasePath());
						LFile::Path Cur(e->Name());
						Path.Join(Cur.GetFull());
						Path--;
						if (Path.Exists())
							s->InitialDir(Path);

						auto Process = [this, e](LFileSelect *s, bool ok)
						{
							if (!ok)
								return;
							const char *Base = GetCtrlName(IDC_PATH);
							LString Rel;
							if (Base)
							{
								LFile::Path p = Base;
								Rel = LMakeRelativePath(--p, s->Name());
							}
							e->Name(Rel ? Rel.Get() : s->Name());

							delete s;
						};
						
						if (BrowseFolder)
							s->OpenFolder(Process);
						else
							s->Open(Process);

						return 0; // no default btn handling.
					}
				}
				break;
			}
		}
		
		return LDialog::OnNotify(Ctrl, n);
	}
	
	void OnSelect(SettingItem *si)
	{
		Detail->SetSetting(si->Setting, si->Flags);
	}
};

void SettingItem::Select(bool b)
{
	LTreeItem::Select(b);
	if (b)
		Dlg->OnSelect(this);
}

IdeProjectSettings::IdeProjectSettings(IdeProject *Proj)
{
	d = new IdeProjectSettingsPriv(this);
	d->Project = Proj;
	InitAllSettings();
}

IdeProjectSettings::~IdeProjectSettings()
{
	DeleteObj(d);
}

const char *IdeProjectSettings::GetCurrentConfig()
{
	return d->CurConfig;
}

bool IdeProjectSettings::SetCurrentConfig(const char *Config)
{
	int i = d->FindConfig(Config);
	if (i < 0)
		return false;
	
	d->CurConfig = d->Configs[i];
	return true;
}

bool IdeProjectSettings::AddConfig(const char *Config)
{
	int i = d->FindConfig(Config);
	if (i >= 0)
		return true;

	d->Configs.New() = Config;
	return true;
}

bool IdeProjectSettings::DeleteConfig(const char *Config)
{
	int i = d->FindConfig(Config);
	if (i < 0)
		return false;
	
	d->Configs.DeleteAt(i, true);
	return true;
}

void IdeProjectSettings::InitAllSettings(bool ClearCurrent)
{
	char *p;
	for (SettingInfo *i = AllSettings; i->Setting; i++)
	{
		LVariant Default;
		LXmlTag *t = NULL;
		switch (i->Setting)
		{
			default:
				break;
			case ProjMakefile:
			{
				char s[64];
				sprintf_s(s, sizeof(s), "Makefile.%s", LGetOsName());
				strlwr(s + 1);
				Default = s;
				break;
			}
			case ProjTargetType:
			{
				Default = "Executable";
				break;
			}
			/* This won't work in the constructor because the IdeProject isn't initialized yet.
			case ProjTargetName:
			{
				if (d->Project)
				{
					char s[256];
					char *Fn = d->Project->GetFileName();
					char *Dir = Fn ? strrchr(Fn, DIR_CHAR) : NULL;
					if (Dir)
					{
						strsafecpy(s, ++Dir, sizeof(s));
						char *Dot = strrchr(s, '.');
						#ifdef WIN32
						strcpy(Dot, ".exe");
						#else
						if (Dot) *Dot = 0;
						#endif
						Default = s;
					}
				}
				break;
			}
			*/
			case ProjEditorTabSize:
			{
				Default = 4;
				break;
			}
			case ProjEditorIndentSize:
			{
				Default = 4;
				break;
			}
			case ProjEditorShowWhiteSpace:
			{
				Default = false;
				break;
			}
			case ProjEditorUseHardTabs:
			{
				Default = true;
				break;
			}
		}
		
		if (i->Flag.ConfigSpecific)
		{
			for (int Cfg = 0; Cfg < d->Configs.Length(); Cfg++)
			{
				if (!i->Flag.PlatformSpecific)
				{
					p = d->BuildPath(i->Setting, 0, PlatformCurrent, Cfg);
					d->Active.GetChildTag(p, true);
					if (t && !t->GetContent() && Default.Type != GV_NULL)
						t->SetContent(Default.Str());
				}
				
				if (!i->Flag.CrossPlatform)
				{
					p = d->BuildPath(i->Setting, SF_PLATFORM_SPECIFC, PlatformCurrent, Cfg);
					d->Active.GetChildTag(p, true);
					if (t && !t->GetContent() && Default.Type != GV_NULL)
						t->SetContent(Default.Str());
				}
			}
		}
		else
		{
			if (!i->Flag.PlatformSpecific)
			{
				p = d->BuildPath(i->Setting, 0, PlatformCurrent, -1);
				t = d->Active.GetChildTag(p, true);
				if (t && !t->GetContent() && Default.Type != GV_NULL)
					t->SetContent(Default.Str());
			}

			if (!i->Flag.CrossPlatform)
			{
				p = d->BuildPath(i->Setting, SF_PLATFORM_SPECIFC, PlatformCurrent, -1);
				t = d->Active.GetChildTag(p, true);
				if (t && !t->GetContent() && Default.Type != GV_NULL)
					t->SetContent(Default.Str());
			}
		}
	}
}

void IdeProjectSettings::Edit(LViewI *parent, std::function<void()> OnChanged)
{
	// Copy all the settings to the edit tag...
	d->Editing.Copy(d->Active, true);
	
	// Show the dialog...
	auto *Dlg = new ProjectSettingsDlg(parent, d);
	Dlg->DoModal([this,OnChanged](auto dlg, auto code)
	{
		if (code)
		{
			// User elected to save settings... so copy all the values
			// back over to the active settings...
			d->Active.Copy(d->Editing, true);
			d->Editing.Empty(true);
			
			if (OnChanged)
				OnChanged();
		}
		
		delete dlg;
	});
}

bool IdeProjectSettings::Serialize(LXmlTag *Parent, bool Write)
{
	if (!Parent)
	{
		LAssert(!"No parent tag?");
		return false;
	}

	LXmlTag *t = Parent->GetChildTag(TagSettings, Write);
	if (!t)
	{
		LAssert(!"Can't find settings tags?");
		return false;
	}

	if (Write)
	{
		// d->Active -> Parent
		ClearEmptyTags(&d->Active);
		return t->Copy(d->Active, true);
	}
	else
	{
		// Parent -> d->Active
		bool Status = d->Active.Copy(*t, true);
		InitAllSettings();
		return Status;
	}
	
	return false;
}

const char *IdeProjectSettings::GetStr(ProjSetting Setting, const char *Default, IdePlatform Platform)
{
	auto s = d->Map.Find(Setting);
	LAssert(s);
	LArray<char*> Strs;
	int Bytes = 0;
	if (!s->Flag.PlatformSpecific)
	{
		auto path = d->BuildPath(Setting, 0, Platform);
		auto t = d->Active.GetChildTag(path);
		if (t)
		{
			if (t->GetContent())
			{
				Strs.Add(t->GetContent());
				Bytes += strlen(t->GetContent()) + 1;
			}
		}
		else LgiTrace("%s:%i - no key '%s' in '%s'\n", _FL, path, d->Project->GetFileName());
	}
	if (!s->Flag.CrossPlatform)
	{
		auto path = d->BuildPath(Setting, SF_PLATFORM_SPECIFC, Platform);
		auto t = d->Active.GetChildTag(path);
		if (t)
		{
			if (t->GetContent())
			{
				Strs.Add(t->GetContent());
				Bytes += strlen(t->GetContent()) + 1;
			}
		}
		else LgiTrace("%s:%i - no key '%s' in '%s'\n", _FL, path, d->Project->GetFileName());
	}
	
	if (Strs.Length() == 0)
		return Default;
	
	if (Strs.Length() == 1)
		return Strs[0];

	if (!d->StrBuf.Reset(new char[Bytes]))
		return Default;

	int ch = 0;
	for (int i=0; i<Strs.Length(); i++)
	{
		ch += sprintf_s(d->StrBuf + ch, Bytes - ch, "%s%s", Strs[i], i<Strs.Length()-1 ? "\n" : "");
	}
	
	return d->StrBuf;
}

int IdeProjectSettings::GetInt(ProjSetting Setting, int Default, IdePlatform Platform)
{
	int Status = Default;

	SettingInfo *s = d->Map.Find(Setting);
	LAssert(s);
	
	if (!s->Flag.PlatformSpecific)
	{
		LXmlTag *t = d->Active.GetChildTag(d->BuildPath(Setting, 0, Platform));
		if (t)
		{
			Status = t->GetContent() ? atoi(t->GetContent()) : 0;
		}
	}
	else if (!s->Flag.CrossPlatform)
	{
		LXmlTag *t = d->Active.GetChildTag(d->BuildPath(Setting, SF_PLATFORM_SPECIFC, Platform));
		if (t)
		{
			Status = t->GetContent() ? atoi(t->GetContent()) : 0;
		}
	}
	else LAssert(0);

	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, const char *Value, IdePlatform Platform, bool PlatformSpecific)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, PlatformSpecific ? SF_PLATFORM_SPECIFC : 0, Platform);
	LXmlTag *t = d->Active.GetChildTag(path, true);
	if (t)
	{
		t->SetContent(Value);
	}
	else LgiTrace("%s:%i - Warning: missing setting tag '%s'\n", _FL, path);

	return Status;
}

bool IdeProjectSettings::Set(ProjSetting Setting, int Value, IdePlatform Platform, bool PlatformSpecific)
{
	bool Status = false;
	char *path = d->BuildPath(Setting, PlatformSpecific ? SF_PLATFORM_SPECIFC : 0, Platform);
	LXmlTag *t = d->Active.GetChildTag(path, true);
	if (t)
	{
		t->SetContent(Value);
	}
	else LgiTrace("%s:%i - Warning: missing setting tag '%s'\n", _FL, path);

	return Status;
}
