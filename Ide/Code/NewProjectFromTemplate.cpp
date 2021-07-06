#include "lgi/common/Lgi.h"
#include "lgi/common/Tree.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/FileSelect.h"
#include "LgiIde.h"
#include "resdefs.h"

static LString TemplatesPath;
class NewProjFromTemplate : public LDialog
{
public:
	NewProjFromTemplate(LViewI *parent)
	{
		SetParent(parent);
		if (LoadFromResource(IDD_NEW_PROJ_FROM_TEMPLATE))
		{
			MoveSameScreen(parent);
			
			LTree *t;
			if (TemplatesPath && GetViewById(IDC_TEMPLATES, t))
				Add(t, TemplatesPath);
		}
	}
	
	void Add(LTreeNode *t, const char *path)
	{
		GDirectory d;
		for (auto b=d.First(path); b; b=d.Next())
		{
			auto Full = d.FullPath();
			if (d.IsDir())
			{
				LTreeItem *c = new LTreeItem;
				
				c->SetText(d.GetName());
				c->SetText(Full, 1);
				t->Insert(c);
				Add(c, Full);
			}
		}
	}
	
	int OnNotify(LViewI *c, int flags)
	{
		switch (c->GetId())
		{
			case IDC_BROWSE_OUTPUT:
			{
				LFileSelect s;
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
LString GetPython3()
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
			LSubProcess sp(p, "--version");
			if (sp.Start())
			{
				LStringPipe out;
				sp.Communicate(&out);
				auto s = out.NewGStr();
				// printf("out=%s\n", s.Get());
				if (s.Find(" 3.") >= 0)
					return p.GetFull();
			}
		}
	}
	
	return LString();
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
	if (!LDirExists(Folder))
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
	LString Args;
	Args.Printf("\"%s\" \"%s\" \"%s\"", ScriptOut.GetFull().Get(), Template, Name);
	LSubProcess p(Py3, Args);
	if (!p.Start())
	{
		LgiTrace("%s:%i - Start process failed.\n", _FL);
		return false;
	}
	
	LStringPipe Out;
	p.Communicate(&Out);
	LgiTrace("Out=%s\n", Out.NewGStr().Get());
	
	FileDev->Delete(ScriptOut);
	return true;
}
void NewProjectFromTemplate(LViewI *parent)
{
	GFile::Path p(LSP_APP_INSTALL);
	p +=
		#ifdef MAC
		"../../"
		#endif
		"../templates";
	TemplatesPath = p;
	NewProjFromTemplate Dlg(parent);
	if (!Dlg.DoModal())
	{
		LgiTrace("%s:%i - Dialog cancelled.\n", _FL);
		return;
	}
	LTree *t;
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