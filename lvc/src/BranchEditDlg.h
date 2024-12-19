#pragma once

class BranchEditDlg : public LDialog
{
	LList *lst = nullptr;
	VcFolder *folder = nullptr;

	void Default()
	{
		SetCtrlName(ID_NAME, "branches/");
	}

public:
	BranchEditDlg(VcFolder *f) :
		folder(f)
	{
		auto loaded = LoadFromResource(IDD_BRANCHES);
		LAssert(loaded);

		auto v = f->GetTree();
		MoveSameScreen(v);

		if (GetViewById(ID_LIST, lst))
		{
			lst->AddColumn("name", 500);
			LString cur = f->GetCurrentBranch();			
			auto &cache = f->GetBranchCache();

			for (auto p: cache)
			{
				if (auto item = new LListItem)
				{
					item->SetText(p.key);
					lst->Insert(item);
				}
			}
		}
		else LAssert(0);
		Default();
	}

	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		switch (Ctrl->GetId())
		{
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
			case ID_CLOSE:
			{
				EndModal(0);
				break;
			}
		}
		return 0;
	}
};