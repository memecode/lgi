#include "lgi/common/Lgi.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/Levenshtein.h"

#include "LgiIde.h"
#include "ProjectNode.h"
#include "resdefs.h"

enum Msgs
{
	M_CHECK_FILE = M_USER + 100,
	M_MISSING_FILE,
	M_ADD_SEARCH_PATH,
	M_SEARCH,
	M_RESULTS,
	M_RECURSE,
};

struct SearchResults
{
	ProjectNode *Node;
	LString Path;
	LString::Array Matches;
	
	SearchResults()
	{
		Node = 0;
	}
};

class FileExistsThread : public LEventTargetThread
{
	int Hnd;

public:
	FileExistsThread(int hnd) : LEventTargetThread("FileExistsThread")
	{
		Hnd = hnd;
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_CHECK_FILE:
			{
				LAutoPtr<SearchResults> Sr((SearchResults*)Msg->A());
				bool e = LFileExists(Sr->Path);
				// printf("Checking '%s' = %i\n", Sr->Path.Get(), e);
				if (!e)
					PostObject(Hnd, M_MISSING_FILE, Sr);
				break;
			}
		}

		return 0;
	}
};

bool IsParentFolder(LString p, LString c)
{
	LString::Array d1 = p.Split(DIR_STR);
	LString::Array d2 = c.Split(DIR_STR);

	if (d1.Length() > d2.Length())
		return false;

	for (unsigned i=0; i<d1.Length(); i++)
	{
		bool Match = d1[i].Equals(d2[i]);
		if (!Match)
			return false;
	}

	return true;
}

class SearchThread : public LEventTargetThread
{
	int Hnd;
	LArray<LString> Search;
	LArray<char*> Files;

public:
	SearchThread(int hnd) : LEventTargetThread("SearchThread")
	{
		Hnd = hnd;
	}

	~SearchThread()
	{
		Files.DeleteArrays();
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_ADD_SEARCH_PATH:
			{
				LAutoPtr<LString> p((LString*)Msg->A());
				bool IsParent = false;
				for (unsigned i=0; i<Search.Length(); i++)
				{
					if (p->Equals(Search[i]) ||
						(IsParent = IsParentFolder(Search[i], *p)))
					{
						printf("'%s' is parent of '%s'\n", Search[i].Get(), p->Get());
						break;
					}
				}
				if (!IsParent)
				{
					printf("Adding '%s'\n", p->Get());
					Search.New() = *p;
				}
				break;
			}
			case M_RECURSE:
			{
				for (unsigned i=0; i<Search.Length(); i++)
				{
					LArray<const char*> Ext;
					Ext.Add("*.h");
					Ext.Add("*.hpp");
					Ext.Add("*.c");
					Ext.Add("*.cpp");
					Ext.Add("*.mm");

					// printf("Recursing '%s'\n", Search[i].Get());
					LRecursiveFileSearch(Search[i], &Ext, &Files);
				}
				break;
			}
			case M_SEARCH:
			{
				LAutoPtr<SearchResults> Sr((SearchResults*)Msg->A());
				char *leaf1 = LGetLeaf(Sr->Path);
				for (unsigned i=0; i<Files.Length(); i++)
				{
					char *leaf2 = LGetLeaf(Files[i]);
					size_t Dist = LLevenshtein(leaf1, leaf2);
					if (Dist < 4)
					{
						Sr->Matches.New() = Files[i];
					}
				}

				printf("Posting '%s' with %i results.\n", Sr->Path.Get(), (int)Sr->Matches.Length());
				PostObject(Hnd, M_RESULTS, Sr);
				break;
			}
		}

		return 0;
	}
};

class MissingFiles : public LDialog
{
	IdeProject *Proj;
	int SearchHnd;
	int ExistsHnd;
	LList *Lst;
	LArray<SearchResults*> Files;

public:
	MissingFiles(IdeProject *proj)
	{
		Proj = proj;
		SearchHnd = -1;
		ExistsHnd = -1;
		Lst = NULL;
		
		SetParent(Proj->GetApp());
		if (LoadFromResource(IDD_MISSING_FILES))
		{
			GetViewById(IDC_RESULTS, Lst);
			MoveSameScreen(Proj->GetApp());

			SetCtrlEnabled(IDC_MISSING, false);
			SetCtrlEnabled(IDC_RESULTS, false);
			SetCtrlEnabled(IDC_BROWSE, false);

			ExistsHnd = (new FileExistsThread(AddDispatch()))->GetHandle();
			SearchHnd = (new SearchThread(AddDispatch()))->GetHandle();

			LHashTbl<StrKey<char>,bool> Flds;

			auto All = Proj->GetAllProjects();
			LArray<ProjectNode*> Nodes;

			for (auto p: All)
			{
				if (p->GetAllNodes(Nodes))
				{
					for (auto Node: Nodes)
					{
						auto s = Node->GetFullPath();
						if (s)
						{
							LString sOld = s.Get();
							if (p->CheckExists(s) &&
								Strcmp(sOld.Get(), s.Get()) != 0 &&
								Stricmp(sOld.Get(), s.Get()) == 0)
							{
								// Case change?
								Node->SetFileName(s);
							}

							SearchResults *Sr = new SearchResults;
							Sr->Node = Node;
							Sr->Path = s;
							PostThreadEvent(ExistsHnd, M_CHECK_FILE, (LMessage::Param) Sr);
							
							LString Parent = s.Get();
							LTrimDir(Parent);
							Flds.Add(Parent, true);
						}
					}
				}

				auto s = p->GetBasePath();
				if (s)
				{
					LTrimDir(s);
					Flds.Add(s, true);
				}
			}

			for (auto i: Flds)
				PostThreadEvent(SearchHnd, M_ADD_SEARCH_PATH, (LMessage::Param) new LString(i.key));

			PostThreadEvent(SearchHnd, M_RECURSE);
		}
	}

	~MissingFiles()
	{
		if (SearchHnd >= 0)
			LEventSinkMap::Dispatch.CancelThread(SearchHnd);
		if (ExistsHnd >= 0)
			LEventSinkMap::Dispatch.CancelThread(ExistsHnd);
	}

	void OnReplace(const char *NewPath)
	{
		SearchResults *Sr = Files.First();
		if (!Sr) return;

		printf("%s:%i - Setting node to '%s'\n", _FL, Sr->Path.Get());
		Sr->Node->SetFileName(NewPath);

		Files.DeleteAt(0, true);
		delete Sr;

		OnFile();
	}

	void OnDelete()
	{
		SearchResults *Sr = Files.First();
		if (!Sr) return;

		bool Has = Proj->HasNode(Sr->Node);
		if (Has)
			Sr->Node->Delete();

		Files.DeleteAt(0, true);

		if (Has)
			delete Sr;

		OnFile();
	}

	void OnFile()
	{
		bool Has = Files.Length() > 0;

		SetCtrlEnabled(IDC_MISSING, Has);
		SetCtrlEnabled(IDC_BROWSE, Has);
		SetCtrlEnabled(IDC_RESULTS, Has);
		Lst->Empty();

		if (Has)
		{
			SetCtrlName(IDC_MISSING, Files[0]->Path);

			for (unsigned i=0; i<Files[0]->Matches.Length(); i++)
			{
				LListItem *li = new LListItem;
				li->SetText(Files[0]->Matches[i]);
				Lst->Insert(li);
			}

			Lst->ResizeColumnsToContent();
		}
		else
		{
			SetCtrlName(IDC_MISSING, NULL);
		}
	}

	int OnNotify(LViewI *Ctrl, LNotification &n) override
	{
		switch (Ctrl->GetId())
		{
			case IDC_BROWSE:
			{
				LFileSelect *s = new LFileSelect;
				LAutoString Dir = Proj->GetBasePath();
				s->Parent(this);
				s->InitialDir(Dir);
				s->Open([this](auto s, auto ok)
				{
					if (ok)
						OnReplace(s->Name());
				});
				break;
			}
			case IDC_DELETE:
			{
				OnDelete();
				break;
			}
			case IDC_RESULTS:
			{
				switch (n.Type)
				{
					case LNotifyItemDoubleClick:
					{
						LListItem *li = Lst->GetSelected();
						if (li)
						{
							OnReplace(li->GetText(0));
						}
						break;
					}
					default:
						break;
				}
				break;
			}
			case IDOK:
			{
				EndModal();
				break;
			}
		}

		return 0;
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_MISSING_FILE:
			{
				LAutoPtr<SearchResults> Sr((SearchResults*)Msg->A());
				if (Sr)
				{
					printf("Missing file '%s'\n", Sr->Path.Get());
					PostThreadEvent(SearchHnd, M_SEARCH, (LMessage::Param) Sr.Release());
				}
				break;
			}
			case M_RESULTS:
			{
				SearchResults *Sr = (SearchResults*)Msg->A();
				bool HasNode = false;
				for (unsigned i=0; i<Files.Length(); i++)
				{
					if (Files[i]->Node == Sr->Node)
					{
						LgiTrace("%s:%i - Node already in Files.\n", _FL);
						HasNode = true;
						break;
					}
				}
				
				if (!HasNode)
				{
					Files.Add((SearchResults*)Msg->A());
					OnFile();
				}
				break;
			}
		}

		return LDialog::OnEvent(Msg);
	}
};

void FixMissingFilesDlg(IdeProject *Proj)
{
	auto Dlg = new MissingFiles(Proj);
	Dlg->DoModal(NULL);
}
