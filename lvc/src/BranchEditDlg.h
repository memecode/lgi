#pragma once

class BranchEditDlg : public LDialog
{
	LList *lst = nullptr;
	VcFolder *folder = nullptr;
	LString curBranch;
	LString filterStr;

	void Default()
	{
		SetCtrlName(ID_NAME, "branches/");
	}

public:
	BranchEditDlg(VcFolder *f) :
		folder(f)
	{
		auto loaded = LoadFromResource(ID_BRANCHES);
		LAssert(loaded);

		auto v = f->GetTree();
		MoveSameScreen(v);

		if (GetViewById(ID_LIST, lst))
		{
			lst->AddColumn("name", 500);
			curBranch = f->GetCurrentBranch();			
			auto &cache = f->GetBranchCache();

			for (auto p: cache)
			{
				if (auto item = new LListItem)
				{
					item->SetText(p.key);
					lst->Insert(item);
					
					if (curBranch == p.key)
						item->Select(true);
				}
			}
			
			lst->Sort(0 /*column*/);
			lst->ResizeColumnsToContent();
		}
		else LAssert(0);
		Default();
	}
	
	void ParseBranches(LString &str)
	{
		// Should be thread safe... as the LList control is thread safe...
		
		LHashTbl<ConstStrKey<char,false>, LListItem*> map;
		LArray<LListItem*> all;
		if (lst->GetAll(all))
		{
			for (auto i: all)
				map.Add(i->GetText(), i);
		}

		auto lines = str.SplitDelimit("\n");
		for (auto &ln: lines)
		{
			auto parts = ln.SplitDelimit();
			auto branch = parts[0];
			if (!map.Find(branch))
			{
				if (auto i = new LListItem(branch))
				{
					map.Add(i->GetText(), i);
					lst->Insert(i);
				}
			}
		}

		lst->Sort(0 /*column*/);
		lst->ResizeColumnsToContent();
	}
	
	void UpdateFilter()
	{
		LArray<LListItem*> all;
		if (lst->GetAll(all))
		{
			for (auto i: all)
			{
				auto txt = i->GetText();
				auto vis = filterStr ? Stristr(txt, filterStr.Get()) != nullptr : true;
				i->GetCss(true)->Display(vis ? LCss::DispBlock : LCss::DispNone);
			}
		}
		
		lst->UpdateAllItems();
	}

	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		switch (Ctrl->GetId())
		{
			case ID_CHECKOUT:
			{
				if (auto sel = lst->GetSelected())
				{
					if (auto name = sel->GetText())
					{
						folder->Checkout(name, true);
					}
				}
				break;
			}
			case ID_DELETE:
			{
				if (auto sel = lst->GetSelected())
				{
					if (auto name = sel->GetText())
					{
						folder->DeleteBranch(name, [this, sel](auto code, auto str)
							{
								LAssert(InThread());
								if (code == 0)
								{
									delete sel;
									folder->GetBranches(true);
								}
								else LgiMsg(this, "%s", Name(), MB_OK, str.Get());
							});
					}
				}
				break;
			}
			case ID_MERGE:
			{
				if (auto sel = lst->GetSelected())
				{
					if (auto name = sel->GetText())
					{
						folder->MergeBranch(name, [this](auto code, auto str)
							{
								LAssert(InThread());
								if (code)
								{
									// Manage conflicts here?
								}
							});
					}
				}
				break;
			}
			case ID_CREATE:
			{
				if (auto name = GetCtrlName(ID_NAME))
				{
					folder->CreateBranch(name, [this, name = LString(name)](auto code, auto str)
						{
							if (!code)
							{
								lst->Insert(new LListItem(name));
								Default();

								// Then, update the branches, then set the current branch...
								auto params = new ParseParams([this, name](auto code, auto str)
									{
										folder->SetCurrentBranch(name);
									});
								folder->GetBranches(true, params);
							}
							else LgiMsg(this, "%s", Name(), MB_OK, str.Get());
						});
				}
				break;
			}
			case ID_SHOW_REMOTE:
			{
				if (auto params = new ParseParams([this](auto code, auto str)
					{
						if (code)
							LgiTrace("%s:%i show remote failed: %i\n", _FL, code);
						else
							ParseBranches(str);
					}))
				{
					folder->GetBranches(true, GetCtrlValue(ID_SHOW_REMOTE) != 0, params);
					
					LView *edit;
					if (GetViewById(ID_FILTER, edit))
						edit->Focus(true);
				}
				break;
			}
			case ID_FILTER:
			{
				if (n.Type == LNotifyEscapeKey)
				{
					filterStr.Empty();
					SetCtrlName(ID_FILTER, nullptr);
				}
				else
				{
					LString f = Ctrl->Name();
					if (filterStr != f)
					{
						filterStr = f;
						UpdateFilter();
					}
				}
				
				SetCtrlEnabled(ID_CLEAR_FILTER, !filterStr.IsEmpty());
				break;
			}
			case ID_CLOSE:
			{
				EndModal(0);
				break;
			}
		}
		return 0;
	}
};