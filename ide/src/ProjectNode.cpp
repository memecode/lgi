#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/Combo.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/Menu.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/Charset.h"

#include "LgiIde.h"
#include "IdeProject.h"
#include "ProjectNode.h"
#include "AddFtpFile.h"
#include "WebFldDlg.h"
#include "ProjectBackend.h"

#define DEBUG_SHOW_NODE_COUNTS		0

const char *TypeNames[NodeMax] = {
	"None",
	"Folder",
	"Source",
	"Header",
	"Dependancy",
	"Resources",
	"Graphic",
	"Web",
	"Text",
	"MakeFile",
};

//////////////////////////////////////////////////////////////////////////////////
class FileProps : public LDialog
{
public:
	NodeType Type;
	LString Charset;
	int Platforms;

	FileProps(LView *p, char *m, NodeType t, int plat, const char *charset)
	{
		Platforms = plat;
		Type = t;
		SetParent(p);
		if (LoadFromResource(IDD_FILE_PROPS))
		{
			MoveToCenter();
			SetCtrlName(IDC_MSG, m);
			
			LCombo *c;
			if (GetViewById(IDC_TYPE, c))
			{
				for (int i=NodeNone; i<NodeMax; i++)
				{
					c->Insert(TypeNames[i]);
				}
				c->Value(Type);
			}
			else LgiTrace("%s:%i - Failed to get Type combo.\n", _FL);

			if (GetViewById(IDC_CHARSET, c))
			{
				if (!charset)
					charset = "utf-8";
				for (LCharset *cs = LGetCsList(); cs->Charset; cs++)
				{
					c->Insert(cs->Charset);
					if (!Stricmp(charset, cs->Charset))
						c->Value(c->Length()-1);
				}

			}

			for (int i=0; PlatformNames[i]; i++)
			{
				SetCtrlValue(PlatformCtrlId[i], ((1 << i) & Platforms) != 0);
			}
			
			OnPosChange();
			
			// Make sure the dialog can display the whole table...
			LTableLayout *t;
			if (GetViewById(IDC_TABLE, t))
			{
				LRect u = t->GetUsedArea();
				u.Inset(-LTableLayout::CellSpacing, -LTableLayout::CellSpacing);
				LRect p = GetPos();
				if (u.X() < p.X() ||
					u.Y() < p.Y())
				{
					p.SetSize(MAX(u.X(), p.X()),
								MAX(u.Y(), p.Y()));
					SetPos(p);
				}				
			}
		}
	}
	
	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDOK:
			{
				int64 t = GetCtrlValue(IDC_TYPE);
				if (t >= NodeSrc)
				{
					Type = (NodeType) t;
				}

				Platforms = 0;
				for (int i=0; PlatformNames[i]; i++)
				{
					if (GetCtrlValue(PlatformCtrlId[i]))
					{
						Platforms |= 1 << i;
					}
				}

				Charset = GetCtrlName(IDC_CHARSET);
				if (!Stricmp(Charset.Get(), "utf-8"))
					Charset.Empty();
				
				// fall thru
			}
			case IDCANCEL:
			case IDC_COPY_PATH:
			{
				EndModal(c->GetId());
				break;
			}
		}
		
		return 0;
	}
};

////////////////////////////////////////////////////////////////////
ProjectNode::ProjectNode(IdeProject *p) : IdeCommon(p)
{
	Tag = NewStr("Node");
}

ProjectNode::~ProjectNode()
{
	NeedsPulse(false);
	
	if (sFile && Project)
		Project->OnNode(sFile, this, false);

	if (Dep)
		DeleteObj(Dep);
}

void ProjectNode::NeedsPulse(bool yes)
{
	if (!Project)
	{
		LAssert(!"No project.");
		return;
	}
	auto app = Project->GetApp();
	if (!app)
	{
		LAssert(!"No app.");
		return;
	}
	app->NeedsPulse.Delete(this);
	if (yes)
		app->NeedsPulse.Add(this);
}

void ProjectNode::OpenLocalCache(IdeDoc *&Doc)
{
	if (sLocalCache)
	{
		Doc = Project->GetApp()->OpenFile(sLocalCache, this);
		if (Doc)
		{
			Doc->SetProject(Project);
			
			IdeProjectSettings *Settings = Project->GetSettings();
			Doc->SetEditorParams(Settings->GetInt(ProjEditorIndentSize),
								Settings->GetInt(ProjEditorTabSize),
								Settings->GetInt(ProjEditorUseHardTabs),
								Settings->GetInt(ProjEditorShowWhiteSpace));
		}
		else
		{
			LgiMsg(Tree, "Couldn't open file '%s'", AppName, MB_OK, sLocalCache.Get());
		}
	}
}

int64 ProjectNode::CountNodes()
{
	int64 n = 1;

	for (auto i:*this)
	{
		ProjectNode *c = dynamic_cast<ProjectNode *>(i);
		if (!c) break;
		n += c->CountNodes();
	}
	return n;
}

void ProjectNode::OnCmdComplete(FtpCmd *Cmd)
{
	if (!Cmd)
		return;

	if (Cmd->Status && Cmd->File)
	{
		if (Cmd->Cmd == FtpRead)
		{
			sLocalCache = Cmd->File;
			IdeDoc *Doc;
			OpenLocalCache(Doc);
		}
		else if (Cmd->Cmd == FtpWrite)
		{
			Cmd->View->OnSaveComplete(Cmd->Status);
		}
	}
}

int ProjectNode::GetPlatforms()
{
	return Platforms;
}

const char *ProjectNode::GetLocalCache()
{
	return sLocalCache;
}

bool ProjectNode::Load(LDocView *Edit, NodeView *Callback)
{
	bool Status = false;

	if (IsWeb())
	{
		if (sLocalCache)
			Status = Edit->Open(sLocalCache);
		else
			LAssert(!"No LocalCache");
	}
	else
	{
		auto Full = GetFullPath();
		Status = Edit->Open(Full, Charset);
	}
	
	return Status;
}

bool ProjectNode::Save(LDocView *Edit, NodeView *Callback)
{
	bool Status = false;

	if (IsWeb())
	{
		if (sLocalCache)
		{
			if (Edit->Save(sLocalCache))
			{
				FtpThread *t = GetFtpThread();
				if (t)
				{
					FtpCmd *c = new FtpCmd(FtpWrite, this);
					if (c)
					{
						c->View = Callback;
						c->Watch = Project->GetApp()->GetFtpLog();
						c->Uri = NewStr(sFile);
						c->File = NewStr(sLocalCache);
						t->Post(c);
					}
				}
			}
			else LAssert(!"Editbox save failed.");
		}
		else LAssert(!"No LocalCache");
	}
	else
	{
		auto f = GetFullPath();
		if (Project)
			Project->CheckExists(f);
		Status = Edit->Save(f, Charset);

		if (Callback)
			Callback->OnSaveComplete(Status);
	}
	
	
	return Status;
}

int ProjectNode::GetId()
{
	if (!NodeId && Project)
		NodeId = Project->AllocateId();

	return NodeId;
}

bool ProjectNode::IsWeb()
{
	char *Www = GetAttr(OPT_Www);
	char *Ftp = GetAttr(OPT_Ftp);

	if
	(
		Www ||
		Ftp ||
		(sFile && strnicmp(sFile, "ftp://", 6) == 0)
	)
		return true;

	return false;
}

bool ProjectNode::HasNode(ProjectNode *Node)
{
	printf("Has %s %s %p\n", sFile.Get(), sName.Get(), Dep);

	if (this == Node)
		return true;
	if (Dep && Dep->HasNode(Node))
		return true;

	for (auto i:*this)
	{
		ProjectNode *c = dynamic_cast<ProjectNode *>(i);
		if (!c) break;

		if (c->HasNode(Node))
			return true;
	}

	return false;
}

void ProjectNode::AddNodes(LArray<ProjectNode*> &Nodes)
{
	Nodes.Add(this);

	for (auto i:*this)
	{
		ProjectNode *c = dynamic_cast<ProjectNode *>(i);
		if (!c) break;

		c->AddNodes(Nodes);
	}
}

bool ProjectNode::GetClean()
{
	if (Dep)
		return Dep->GetClean();

	return true;
}

void ProjectNode::SetClean()
{
	auto CleanProj = [this]()
	{
		for (auto i: *this)
		{
			ProjectNode *p = dynamic_cast<ProjectNode*>(i);
			if (p)
				p->SetClean();
		}
	};

	if (Dep)
		Dep->SetClean([this, CleanProj](bool ok)
		{
			if (ok)
				CleanProj();
		});
	else
		CleanProj();

}

IdeProject *ProjectNode::GetDep()
{
	return Dep;
}

IdeProject *ProjectNode::GetProject()
{
	return Project;
}

bool ProjectNode::GetFormats(LDragFormats &Formats)
{
	Formats.Supports(NODE_DROP_FORMAT);
	return true;
}

bool ProjectNode::GetData(LArray<LDragData> &Data)
{
	for (unsigned i=0; i<Data.Length(); i++)
	{
		LDragData &dd = Data[i];
		if (dd.IsFormat(NODE_DROP_FORMAT))
		{
			void *t = this;
			dd.Data.New().SetBinary(sizeof(this), (void*) &t);
			return true;
		}
	}
	
	return false;
}

bool ProjectNode::OnBeginDrag(LMouse &m)
{
	return Drag(Tree, m.Event, DROPEFFECT_MOVE);
}

const char *ProjectNode::GetFileName()
{
	return sFile;
}

void ProjectNode::AutoDetectType()
{
	auto FullPath = GetFullPath();
	if (FullPath)
	{
		auto MimeType = LGetFileMimeType(FullPath);
		if (MimeType &&
			strnicmp(MimeType, "image/", 6) == 0)
		{
			Type = NodeGraphic;
		}
	}
		
	if (!Type)
	{
		char *Ext = LGetExtension(sFile);

		if (stristr(sFile, "makefile") ||
			!stricmp(sFile, "CMakeLists.txt"))
		{
			Type = NodeMakeFile;
		}
		else if (Ext)
		{
			if (!stricmp(Ext, "h") ||
				!stricmp(Ext, "hpp"))
			{
				Type = NodeHeader;
			}
			else if (stricmp(Ext, "lr8") == 0 ||
					stricmp(Ext, "rc") == 0)
			{
				Type = NodeResources;
			}
			else if (stricmp(Ext, "cpp") == 0 ||
					stricmp(Ext, "cc") == 0 ||
					stricmp(Ext, "mm") == 0 ||
					stricmp(Ext, "c") == 0)
			{
				Type = NodeSrc;
			}
			else if (stricmp(Ext, "php") == 0 ||
					stricmp(Ext, "asp") == 0 ||
					stricmp(Ext, "html") == 0 ||
					stricmp(Ext, "htm") == 0 ||
					stricmp(Ext, "css") == 0)
			{
				Type = NodeWeb;
			}
			else
			{
				// Fall back to text...
				Type = NodeText;
			}
		}
	}
}

void ProjectNode::SetFileName(const char *f)
{
	LString Rel;

	if (sFile && Project)
		Project->OnNode(sFile, this, false);

	if (Project->RelativePath(Rel, f))
		sFile = Rel;
	else
		sFile = f;

	printf("sFile='%s'\n", sFile.Get());
	
	if (sFile)
	{
		auto FullPath = GetFullPath();

		if (Project)
			Project->OnNode(FullPath, this, true);

		AutoDetectType();
	}
	
	Project->SetDirty();
}

char *ProjectNode::GetName()
{
	return sName;
}

void ProjectNode::SetName(const char *f)
{
	sName = f;
	Type = NodeDir;
}

NodeType ProjectNode::GetType()
{
	return Type;
}

void ProjectNode::SetType(NodeType t)
{
	Type = t;
}

int ProjectNode::GetImage(int f)
{
	if (IsWeb())
	{
		return sFile ? ICON_SOURCE : ICON_WEB;
	}

	switch (Type)
	{
		default:
			break;
		case NodeDir:
			return ICON_FOLDER;
		case NodeSrc:
			return ICON_SOURCE;
		case NodeDependancy:
			return ICON_DEPENDANCY;
		case NodeGraphic:
			return ICON_GRAPHIC;
		case NodeResources:
			return ICON_RESOURCE;
	}

	return ICON_HEADER;
}

const char *ProjectNode::GetText(int c)
{
	if (sFile)
	{
		char *d = 0;

		if (IsWeb())
		{
			d = sFile ? strrchr(sFile, '/') : 0;
		}
		else
		{
			#ifdef WIN32
			char Other = '/';
			#else
			char Other = '\\';
			#endif
			char *s;
			while ((s = strchr(sFile, Other)))
			{
				*s = DIR_CHAR;
			}
			
			d = strrchr(sFile, DIR_CHAR);
		}
		
		if (d) return d + 1;
		else return sFile;
	}

	#if DEBUG_SHOW_NODE_COUNTS
	if (Type == NodeDir)
	{
		if (ChildCount < 0)
			ChildCount = CountNodes();
		Label.Printf("%s ("LGI_PrintfInt64")", Name, ChildCount);
		return Label;
	}
	#endif

	return sName ? sName.Get() : Untitled;
}

void ProjectNode::OnExpand(bool b)
{
	if (placeholder && !childrenReq)
	{
		childrenReq = true;
		if (auto be = Project->GetBackend())
		{
			be->ReadFolder(sFile, [this](auto d)
				{
					DeleteObj(placeholder);
					for (auto b = d->First(NULL); b; b = d->Next())
					{
						if (d->IsHidden())
							continue;

						if (auto n = new ProjectNode(Project))
						{
							if (n->Serialize(d))
								Insert(n);
							else
								delete n;
						}
					}
				});
		}
	}

	if (!IgnoreExpand)
	{
		Project->SetExpanded(GetId(), b);
	}
}

bool ProjectNode::Serialize(LDirectory *d)
{
	if (!d)
		return false;

	Transient = true;

	if (!Write && sFile)
		Project->OnNode(sFile, this, false);

	sFile = d->FullPath();
	sName = d->GetName();
	Charset.Empty();
	Type = d->IsDir() ? NodeDir : NodeSrc;
	Platforms = PLATFORM_ALL;

	if (!Write && sFile)
		Project->OnNode(sFile, this, true);

	if (Type == NodeDir)
	{
		// Add placeholder child node to allow user to expand the dir
		if (placeholder = new LTreeItem)
		{
			placeholder->SetText("...loading...");
			Insert(placeholder);
		}
	}

	return true;
}

bool ProjectNode::Serialize(bool Write)
{
	if (!Write && sFile)
		Project->OnNode(sFile, this, false);

	SerializeAttr("File", sFile);
	SerializeAttr("Name", sName);
	SerializeAttr("Charset", Charset);
	SerializeAttr("Type", (int&)Type);
	SerializeAttr("Platforms", (int&)Platforms);
	
	if (!Write && sFile)
		Project->OnNode(sFile, this, true);

	if (Type == NodeNone)
	{
		AutoDetectType();
	}

	if (Type == NodeDir)
	{
		if (Write && !NodeId)
			GetId(); // Make sure we have an ID

		SerializeAttr("Id", NodeId);

		if (Write)
			Project->SetExpanded(GetId(), Expanded());
		else
		{
			IgnoreExpand = true;
			Expanded(Project->GetExpanded(GetId()));
			IgnoreExpand = false;
		}
	}
	else if (Type == NodeDependancy)
	{
		SerializeAttr("LinkAgainst", LinkAgainst);
		if (!Write)
		{
			auto Full = GetFullPath();				
			Dep = Project->GetApp()->OpenProject(Full, Project, false, this);
		}
	}

	return true;
}

LString ProjectNode::GetFullPath()
{
	LString FullPath;
	
	if (LIsRelativePath(sFile))
	{
		// Relative path
		auto Path = Project->GetBasePath();
		if (Path)
		{
			char p[MAX_PATH_LEN];
			LMakePath(p, sizeof(p), Path, sFile);
			FullPath = p;
		}
	}
	else
	{
		// Absolute path
		FullPath = sFile;
	}
	
	return FullPath;
}

IdeDoc *ProjectNode::Open()
{
	static bool Processing = false;
	IdeDoc *Doc = 0;

	if (Processing)
	{
		LAssert(!"Not recursive");
	}

	if (!Processing)
	{
		Processing = true;

		if (IsWeb())
		{
			if (sFile)
			{
				if (sLocalCache)
				{
					OpenLocalCache(Doc);
				}
				else
				{
					FtpThread *t = GetFtpThread();
					if (t)
					{
						FtpCmd *c = new FtpCmd(FtpRead, this);
						if (c)
						{
							c->Watch = Project->GetApp()->GetFtpLog();
							c->Uri = NewStr(sFile);
							t->Post(c);
						}
					}
				}
			}
		}
		else
		{		
			switch (Type)
			{
				case NodeDir:
				{
					Expanded(true);
					break;
				}
				case NodeResources:
				{
					auto FullPath = GetFullPath();
					if (FullPath)
					{
						char Exe[MAX_PATH_LEN];
						LMakePath(Exe, sizeof(Exe), LGetExePath(), "..");
						#if defined(WIN32)
							LMakePath(Exe, sizeof(Exe), Exe, "Debug\\LgiRes.exe");
						#elif defined(LINUX) || defined(HAIKU)
							LMakePath(Exe, sizeof(Exe), Exe, "resourceEditor/lgires");
						#elif defined(MAC)
							// Of course it's there! Lol
							LMakePath(Exe, sizeof(Exe), Exe, "resourceEditor/mac/build/Debug/LgiRes.app/Contents/MacOS/LgiRes");
						#else
							#error "Impl me?"
						#endif
						
						if (LFileExists(Exe))
						{
							if (!LExecute(Exe, FullPath))
								LgiMsg(Tree, "Failed to execute '%s' with '%s'", AppName, MB_OK, Exe, FullPath.Get());
						}
						else
						{
							LgiMsg(Tree, "No resource editor '%s'", AppName, MB_OK, Exe);
						}
					}
					else
					{
						LgiMsg(Tree, "No Path", AppName);
					}
					break;
				}
				case NodeGraphic:
				{
					auto FullPath = GetFullPath();
					if (FullPath)
					{
						LExecute(FullPath);
					}
					else
					{
						LgiMsg(Tree, "No Path", AppName);
					}
					break;
				}
				default:
				{
					auto FullPath = GetFullPath();
					if (Project->CheckExists(FullPath))
					{
						Doc = Project->GetApp()->FindOpenFile(FullPath);
						if (!Doc)
						{
							Doc = Project->GetApp()->NewDocWnd(0, this);
							if (Doc)
							{
								if (Doc->OpenFile(FullPath))
								{
									IdeProjectSettings *Settings = Project->GetSettings();

									Doc->SetProject(Project);
									Doc->SetEditorParams(Settings->GetInt(ProjEditorIndentSize),
														Settings->GetInt(ProjEditorTabSize),
														Settings->GetInt(ProjEditorUseHardTabs),
														Settings->GetInt(ProjEditorShowWhiteSpace));
								}
								else
								{
									LgiMsg(Tree, "Couldn't open file '%s'", AppName, MB_OK, FullPath.Get());
								}
							}
						}
						else
						{
							Doc->Raise();
							Doc->Focus(true);
						}
					}
					else
					{
						LgiMsg(Tree, "No Path", AppName);
					}
					break;
				}
			}
		}

		Processing = false;
	}
	
	return Doc;
}

void ProjectNode::Delete()
{
	if (Select())
	{
		LTreeItem *s = GetNext();
		if (s || (s = GetParent()))
			s->Select(true);
	}
			
	if (nView)
	{
		nView->OnDelete();
		nView = NULL;
	}

	Project->SetDirty();
	LXmlTag::RemoveTag();							
	delete this;
}

bool ProjectNode::OnKey(LKey &k)
{
	if (k.Down())
	{
		if (k.vkey == LK_RETURN && k.IsChar)
		{
			Open();
			return true;
		}
		else if (k.vkey == LK_DELETE)
		{
			Delete();
			return true;
		}
	}
	
	return false;
}

void ProjectNode::OnPulse()
{
	auto StartTs = LCurrentTime();
	int TimeSlice = 500; //ms

	auto Start = strlen(ImportPath) + 1;
	while (ImportFiles.Length())
	{
		LAutoString f(ImportFiles[0]); // Take ownership of the string
		ImportFiles.DeleteAt(0);
		if (ImportProg)
			(*ImportProg)++;
		
		LToken p(f + Start, DIR_STR);
		ProjectNode *Insert = this;
		
		// Find sub nodes, and drill into directory heirarchy,
		// creating the nodes if needed.
		for (int i=0; Insert && i<p.Length()-1; i++)
		{
			// Find existing node...
			bool Found = false;

			for (auto it:*Insert)
			{
				auto c = dynamic_cast<ProjectNode *>(it);
				if (!c) break;

				if (c->GetType() == NodeDir &&
					c->GetName() &&
					stricmp(c->GetName(), p[i]) == 0)
				{
					Insert = c;
					Found = true;
					break;
				}
			}
			
			if (!Found)
			{
				// Create the node
				IdeCommon *Com = Insert->GetSubFolder(Project, p[i], true);												
				Insert = dynamic_cast<ProjectNode*>(Com);
				LAssert(Insert);
			}
		}
		
		// Insert the file into the tree...
		if (Insert)
		{
			ProjectNode *New = new ProjectNode(Project);
			if (New)
			{
				New->SetFileName(f);
				Insert->InsertTag(New);
				Insert->SortChildren();
				Project->SetDirty();
			}
		}
		
		if (LCurrentTime() - StartTs >= TimeSlice)
			break; // Yeild to the message loop
	}

	if (ImportFiles.Length() == 0)
	{
		NeedsPulse(false);
		ImportProg.Reset();
	}
}

void ProjectNode::OnMouseClick(LMouse &m)
{
	LTreeItem::OnMouseClick(m);
	
	if (m.IsContextMenu())
	{
		LSubMenu Sub;

		Select(true);

		if (Type == NodeDir)
		{
			if (IsWeb())
			{
				Sub.AppendItem("Insert FTP File", IDM_INSERT_FTP, true);
			}
			else
			{
				Sub.AppendItem("Insert File", IDM_INSERT, true);
			}
			
			Sub.AppendItem("New Folder", IDM_NEW_FOLDER, true);
			Sub.AppendItem("Import Folder", IDM_IMPORT_FOLDER, true);
			Sub.AppendSeparator();

			if (!IsWeb())
			{
				Sub.AppendItem("Rename", IDM_RENAME, true);
			}
		}
		Sub.AppendItem("Remove", IDM_DELETE, true);
		Sub.AppendItem("Sort", IDM_SORT_CHILDREN, true);
		Sub.AppendSeparator();
		Sub.AppendItem("Browse Folder", IDM_BROWSE_FOLDER, ValidStr(sFile));
		Sub.AppendItem("Open Terminal", IDM_OPEN_TERM, ValidStr(sFile));
		Sub.AppendItem("Properties", IDM_PROPERTIES, true);

		m.ToScreen();
		LPoint c = _ScrollPos();
		m.x -= c.x;
		m.y -= c.y;
		switch (Sub.Float(Tree, m.x, m.y))
		{
			case IDM_INSERT_FTP:
			{
				AddFtpFile *Add = new AddFtpFile(Tree, GetAttr(OPT_Ftp));
				Add->DoModal([this, Add](auto dlg, auto code)
				{
					if (code)
					{
						for (int i=0; i<Add->Uris.Length(); i++)
						{
							ProjectNode *New = new ProjectNode(Project);
							if (New)
							{
								New->SetFileName(Add->Uris[i]);
								InsertTag(New);
								SortChildren();
								Project->SetDirty();
							}
						}
					}
				});
				break;
			}
			case IDM_INSERT:
			{
				LFileSelect *s = new LFileSelect;
				s->Parent(Tree);
				s->Type("Source", SourcePatterns);
				s->Type("Makefiles", "*makefile");
				s->Type("All Files", LGI_ALL_FILES);
				s->MultiSelect(true);

				auto Dir = Project->GetBasePath();
				if (Dir)
					s->InitialDir(Dir);

				s->Open([this](auto s, auto ok)
				{
					if (ok)
					{
						for (int i=0; i<s->Length(); i++)
						{
							auto path = (*s)[i];
							
							if (!Project->InProject(false, path, false))
							{
								auto New = new ProjectNode(Project);
								if (New)
								{
									New->SetFileName(path);
									InsertTag(New);
									SortChildren();
									Project->SetDirty();
								}
							}
							else
							{
								LgiMsg(Tree, "'%s' is already in the project.\n", AppName, MB_OK, (*s)[i]);
							}
						}
					}
				});
				break;
			}
			case IDM_IMPORT_FOLDER:
			{
				LFileSelect *s = new LFileSelect;
				s->Parent(Tree);

				auto Dir = Project->GetBasePath();
				if (Dir)
					s->InitialDir(Dir);

				s->OpenFolder([this](auto s, auto ok)
				{
					if (ok)
					{
						ImportPath = s->Name();
				
						LArray<const char*> Ext;
						LToken e(SourcePatterns, ";");
						for (auto i: e)
						{
							Ext.Add(i);
						}
				
						if (LRecursiveFileSearch(ImportPath, &Ext, &ImportFiles))
						{
							ImportProg.Reset(new LProgressDlg(GetTree(), 300));
							ImportProg->SetDescription("Importing...");
							ImportProg->SetRange(ImportFiles.Length());
							NeedsPulse(true);
						}
					}
				});
				break;
			}
			case IDM_SORT_CHILDREN:
			{
				SortChildren();
				Project->SetDirty();
				break;
			}
			case IDM_NEW_FOLDER:
			{
				LInput *Name = new LInput(Tree, "", "Name:", AppName);
				Name->DoModal([this, Name](auto dlg, auto ok)
				{
					if (ok)
						GetSubFolder(Project, Name->GetStr(), true);
				});
				break;
			}
			case IDM_RENAME:
			{
				LInput *Name = new LInput(Tree, GetName(), "Name:", AppName);
				Name->DoModal([this, Name](auto dlg, auto ok)
				{
					if (ok)
					{
						SetName(Name->GetStr());
						Project->SetDirty();
						Update();
					}
				});
				break;
			}
			case IDM_DELETE:
			{
				Delete();
				return;
				break;
			}
			case IDM_BROWSE_FOLDER:
			{
				auto Path = GetFullPath();
				if (Path)
					LBrowseToFile(Path);
				break;
			}
			case IDM_OPEN_TERM:
			{
				if (Type == NodeDir)
				{
				}
				else
				{
					auto Path = GetFullPath();
					if (Path)
					{
						LTrimDir(Path);
						
						#if defined LINUX
						
						const char *Term = NULL;
						const char *Format = NULL;
						switch (LGetWindowManager())
						{
							case WM_Kde:
								Term = "konsole";
								Format = "--workdir ";
								break;
							case WM_Gnome:
								Term = "gnome-terminal";
								Format = "--working-directory=";
								break;
						}
						
						if (Term)
						{
							char s[256];
							strcpy_s(s, sizeof(s), Format);
							
							char *o = s + strlen(s), *i = Path;
							*o++ = '\"';
							while (*i)
							{
								if (*i == ' ')
								{
									*o++ = '\\';
								}
								*o++ = *i++;
							}
							*o++ = '\"';
							*o++ = 0;
							LExecute(Term, s);
						}
						
						#elif defined WIN32
						
						LExecute("cmd", 0, Path);
						
						#endif
					}
				}
				break;
			}
			case IDM_PROPERTIES:
			{
				OnProperties();
				break;
			}
		}
	}
	else if (m.Left())
	{
		if (Type != NodeDir && m.Double())
		{
			if
			(
				(
					IsWeb()
					||
					Type != NodeDir
				)
				&&
				ValidStr(sFile)
			)
			{
				Open();
			}
			else
			{
				LAssert(!"Unknown file type");
			}
		}
	}
}

#if 0
#define LOG_FIND_FILE(...) printf(__VA_ARGS__)
#else
#define LOG_FIND_FILE(...)
#endif

ProjectNode *ProjectNode::FindFile(const char *In, char **Full)
{
	LOG_FIND_FILE("%s:%i Node='%s' '%s'\n", _FL, sFile.Get(), sName.Get());
	if (sFile)
	{
		bool Match = false;
		const char *AnyDir = "\\/";

		if (IsWeb())
		{
			Match = sFile ? stricmp(In, sFile) == 0 : false;
		}
		else if (strchr(In, DIR_CHAR))
		{
			// Match partial or full path
			char Full[MAX_PATH_LEN] = "";
			
			if (LIsRelativePath(sFile))
			{
				auto Base = Project->GetBasePath();
				if (Base)
					LMakePath(Full, sizeof(Full), Base, sFile);
				else
					LgiTrace("%s,%i - Couldn't get full IDoc path.\n", _FL);
			}
			else
			{
				strcpy_s(Full, sizeof(Full), sFile);
			}
			
			LOG_FIND_FILE("%s:%i Full='%s'\n", _FL, Full);
			
			LString MyPath(Full);
			LString::Array MyArr = MyPath.SplitDelimit(AnyDir);
			LString InPath(In);
			LString::Array InArr = InPath.SplitDelimit(AnyDir);
			auto Common = MIN(MyArr.Length(), InArr.Length());
			Match = true;
			for (int i = 0; i < Common; i++)
			{
				char *a = MyArr[MyArr.Length()-(i+1)];
				char *b = InArr[InArr.Length()-(i+1)];
				auto res = _stricmp(a, b);
				LOG_FIND_FILE("%s:%i cmp '%s','%s'=%i\n", _FL, a, b, res);
				if (res)
				{
					Match = false;
					break;
				}
			}
		}
		else
		{
			// Match file name only
			auto p = sFile.SplitDelimit(AnyDir);
			Match = p.Last().Equals(In);
			LOG_FIND_FILE("%s:%i cmp '%s','%s'=%i\n", _FL, p.Last().Get(), In, Match);
		}
		
		if (Match)
		{
			if (Full)
			{
				if (sFile(0) == '.')
				{
					LAutoString Base = Project->GetBasePath();
					if (Base)
					{
						char f[256];
						LMakePath(f, sizeof(f), Base, sFile);
						*Full = NewStr(f);
					}
				}
				else
				{
					*Full = NewStr(sFile);
				}
			}
			
			return this;
		}
	}
	
	for (auto i:*this)
	{
		ProjectNode *c = dynamic_cast<ProjectNode*>(i);
		if (!c)
		{
			LAssert(!"Not a node?");
			break;
		}

		ProjectNode *n = c->FindFile(In, Full);
		if (n)
		{
			return n;
		}
	}
	
	return 0;
}

struct DepDlg : public LDialog
{
	ProjectNode *Node;
	
	DepDlg(ProjectNode *node) : Node(node)
	{
		auto proj = node->GetProject();
		auto app = proj ? proj->GetApp() : NULL;
		if (!app)
			LAssert(!"Can't get app ptr.");
		else
		{
			SetParent(app);
			MoveSameScreen(app);
		}		
		
		if (!LoadFromResource(IDD_DEP_NODE))
			LAssert(!"Resource missing.");
		else
		{
			auto Path = node->GetFullPath();
			if (Path)
				SetCtrlName(IDC_PATH, Path);
			SetCtrlValue(IDC_LINK_AGAINST, node->GetLinkAgainst());
		}
	}
	
	int OnNotify(LViewI *Ctrl, LNotification n)
	{
		switch (Ctrl->GetId())
		{
			case IDOK:
			{
				Node->SetLinkAgainst(GetCtrlValue(IDC_LINK_AGAINST));
				break;
			}
		}
		
		return LDialog::OnNotify(Ctrl, n);
	}
};

void ProjectNode::OnProperties()
{
	if (IsWeb())
	{
		bool IsFolder = sFile.IsEmpty();

		WebFldDlg *Dlg = new WebFldDlg(Tree, sName, IsFolder ? GetAttr(OPT_Ftp) : sFile.Get(), GetAttr(OPT_Www));
		Dlg->DoModal([this, IsFolder, Dlg](auto dlg, auto ok)
		{
			if (ok)
			{
				if (IsFolder)
				{
					SetName(Dlg->Name);
					SetAttr(OPT_Ftp, Dlg->Ftp);
					SetAttr(OPT_Www, Dlg->Www);
				}
				else
				{
					sFile = Dlg->Ftp;
				}

				Project->SetDirty();
				Update();
			}
		});
	}
	else if (Type == NodeDir)
	{
	}
	else if (Type == NodeDependancy)
	{
		auto dlg = new DepDlg(this);
		dlg->DoModal(NULL);
	}
	else
	{
		auto Path = GetFullPath();
		if (Path)
		{
			char Size[32];
			int64 FSize = LFileSize(Path);
			LFormatSize(Size, sizeof(Size), FSize);
			char Msg[512];
			snprintf(Msg, sizeof(Msg), "Source Code:\n\n\t%s\n\nSize: %s (%i bytes)", Path.Get(), Size, (int32)FSize);
		
			FileProps *Dlg = new FileProps(Tree, Msg, Type, Platforms, Charset);
			Dlg->DoModal([this, Dlg, Path](auto dlg, auto code)
			{
				switch (code)
				{
					case IDOK:
					{
						if (Type != Dlg->Type)
						{
							Type = Dlg->Type;
							Project->SetDirty();
						}
						if (Platforms != Dlg->Platforms)
						{
							Platforms = Dlg->Platforms;
							Project->SetDirty();
						}
						if (Charset != Dlg->Charset)
						{
							Charset = Dlg->Charset;
							Project->SetDirty();
						}
					
						Update();
						break;
					}
					case IDC_COPY_PATH:
					{
						LClipBoard Clip(Tree);
						Clip.Text(Path);
						break;
					}
				}
			});
		}
	}
}
