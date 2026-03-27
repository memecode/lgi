#pragma once

#include "lgi/common/ClipBoard.h"
#include "lgi/common/List.h"

class CherryPickDlg : public LDialog
{
	LList *parents = nullptr;

	enum TColumns {
		TIndex,
		THash,
		TAuthor,
		TDate,
		TMsg
	};

	struct MergeParent : public LListItem
	{
		LString indexCache;
		unsigned index = 0;
		VcFolder *folder;
		LString hash;
		VcFolder::TCommitInfo commit;
		
		MergeParent(unsigned i, VcFolder *f, LString h) :
			index(i),
			folder(f),
			hash(h)
		{
			folder->GetCommit(h, [this](auto &inf)
			{
				commit = inf;
				Update();
				GetList()->ResizeColumnsToContent();
			});
		}
		
		const char *GetText(int col) override
		{
			switch (col)
			{
				case TIndex:
					indexCache.Printf("%u", index);
					return indexCache;
				case THash: return hash;
				case TAuthor: return commit.author.Get();
				case TDate: return commit.dateStr;
				case TMsg: return commit.message;
			}
			return nullptr;
		}
	};

public:
	VcFolder *folder = nullptr;
	unsigned mergeParentIdx = 0;
	VcFolder::TCommitInfo commit;

	bool LooksLikeHash(const char *s)
	{
		for (; s && *s; s++)
		{
			if (!IsHexDigit(*s))
				return false;
		}
		return true;
	}
	
	CherryPickDlg(VcFolder *f) :
		folder(f)
	{
		LoadFromResource(ID_CHERRY_PICK_DLG);
		
		SetCtrlName(ID_BRANCH, f->GetCurrentBranch());
		if (GetViewById(ID_PARENT_COMMITS, parents))
		{
			parents->AddColumn("index", 20);
			parents->AddColumn("hash", 40);
			parents->AddColumn("author", 40);
			parents->AddColumn("date", 40);
			parents->AddColumn("msg", 40);
		}
		
		LClipBoard clip(this);
		if (auto txt = clip.Text())
			if (LooksLikeHash(txt))
			{
				SetCtrlName(ID_COMMIT, txt);
				Update();
			}
	}
	
	void Update()
	{
		folder->GetCommit(
			GetCtrlName(ID_COMMIT),
			[this](auto &info)
			{
				commit = info;
				
				// do something with the info...
				SetCtrlName(ID_AUTHOR, info.author.Get());
				SetCtrlName(ID_DATE, info.dateStr);
				SetCtrlName(ID_MESSAGE, info.message);
				
				if (parents)
					for (unsigned i=0; i<info.mergeParents.Length(); i++)
						parents->Insert(new MergeParent(i, folder, info.mergeParents[i]));
			});
	}
	
	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		switch (Ctrl->GetId())
		{
			case IDOK:
			{
				if (parents)
				{
					LArray<MergeParent*> sel;
					if (parents->GetSelection(sel))
					{
						if (sel.Length() == 1)
						{
							auto p = sel[0];
							mergeParentIdx = p->index;
						}
						else LAssert(!"invalid count");
					}	
				}
			
				// fall through
			}
			case IDCANCEL:
			{
				EndModal(Ctrl->GetId() == IDOK);
				break;
			}
		}
		return 0;
	}
};