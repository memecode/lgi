#include "Lgi.h"
#include "LgiIde.h"
#include "GTree.h"
#include "resdefs.h"
#include "GSubProcess.h"

static GString TemplatesPath;

class NewProjFromTemplate : public GDialog
{
public:
	NewProjFromTemplate(GViewI *parent)
	{
		SetParent(parent);
		if (LoadFromResource(IDD_NEW_PROJ_FROM_TEMPLATE))
		{
			MoveSameScreen(parent);
			
			GTree *t;
			if (TemplatesPath && GetViewById(IDC_TEMPLATES, t))
				Add(t, TemplatesPath);
		}
	}
	
	void Add(GTreeNode *t, const char *path)
	{
		GDirectory d;
		for (auto b=d.First(path); b; b=d.Next())
		{
			auto Full = d.FullPath();
			if (d.IsDir())
			{
				GTreeItem *c = new GTreeItem;
				
				c->SetText(d.GetName());
				c->SetText(Full, 1);
				t->Insert(c);
				Add(c, Full);
			}
		}
	}
	
	int OnNotify(GViewI *c, int flags)
	{
		switch (c->GetId())
		{
			case IDC_BROWSE_OUTPUT:
			{
				GFileSelect s;
				s.Parent(this);
				if (s.OpenFolder())
					SetCtrlName(IDC_FOLDER, s.Name());
				break;
			}
			case IDOK:
			case IDCANCEL:
				EndModal(c->GetId() == IDOK);
				break;
		}
		
		return 0;
	}
};

GString GetPython3()
{
	auto Path = LGetPath();
	for (auto i: Path)
	{
		GFile::Path p(i);
		p += "python" LGI_EXECUTABLE_EXT;
		if (p.Exists())
		{
			printf("Got python '%s'\n", p.GetFull().Get());
			
			// But what version is it?
			GSubProcess sp(p, "--version");
			if (sp.Start())
			{
				GStringPipe out;
				sp.Communicate(&out);
				auto s = out.NewGStr();
				// printf("out=%s\n", s.Get());
				if (s.Find(" 3.") >= 0)
					return p.GetFull();
			}
		}
	}
	
	return GString();
}

bool CreateProject(const char *Name, const char *Template, const char *Folder)
{
	auto Py3 = GetPython3();
	if (!Py3)
	{
		LgiTrace("%s:%i - Can't find python3.\n", _FL);
		return false;
	}
	
	// Make sure output folder exists?
	if (!DirExists(Folder))
	{
		if (!FileDev->CreateFolder(Folder))
		{
			LgiTrace("%s:%i - Create folder '%s' failed.\n", _FL, Folder);
			return false;
		}
	}

	// Copy in script...
	auto CreateProjectPy = "create_project.py";
	GFile::Path ScriptIn(TemplatesPath);
	ScriptIn += CreateProjectPy;
	GFile::Path ScriptOut(Folder);
	ScriptOut += CreateProjectPy;
	if (!FileDev->Copy(ScriptIn, ScriptOut))
	{
		LgiTrace("%s:%i - Copy '%s' to '%s' failed.\n", _FL, ScriptIn.GetFull().Get(), ScriptOut.GetFull().Get());
		return false;
	}

	// Call the script
	GString Args;
	Args.Printf("\"%s\" \"%s\" \"%s\"", ScriptOut.GetFull().Get(), Template, Name);
	GSubProcess p(Py3, Args);
	if (!p.Start())
	{
		LgiTrace("%s:%i - Start process failed.\n", _FL);
		return false;
	}
	
	GStringPipe Out;
	p.Communicate(&Out);
	LgiTrace("Out=%s\n", Out.NewGStr().Get());
	
	FileDev->Delete(ScriptOut);
	return true;
}

void NewProjectFromTemplate(GViewI *parent)
{
	GFile::Path p(LSP_APP_INSTALL);
	p += "../../../templates";
	TemplatesPath = p;

	NewProjFromTemplate Dlg(parent);
	if (!Dlg.DoModal())
	{
		LgiTrace("%s:%i - Dialog cancelled.\n", _FL);
		return;
	}

	GTree *t;
	if (!Dlg.GetViewById(IDC_TEMPLATES, t))
	{
		LgiTrace("%s:%i - No tree.\n", _FL);
		return;
	}
	
	auto sel = t->Selection();
	if (!sel)
	{
		LgiTrace("%s:%i - No selection.\n", _FL);
		return;
	}
	
	CreateProject(Dlg.GetCtrlName(IDC_PROJ_NAME), sel->GetText(1), Dlg.GetCtrlName(IDC_FOLDER));
}
