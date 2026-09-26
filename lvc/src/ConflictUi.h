//
//  ConflictUi.h
//  Lvc
//
//  Created by Matthew Allen on 17/9/2026.
//

#pragma once

#include <string.h>

#include "lgi/common/Containers.h"
#include "lgi/common/Edit.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/Uri.h"
#include "lgi/common/Button.h"

class ConflictUi : public LWindow
{
	VcFolder *folder;
	LString uri;
	LTableLayout *tbl = nullptr;
	LTextLabel *pathLabel = nullptr;
	LEdit *hashEd[2] = {};
	DiffView *srcEd[2] = {};
	
	VcFolder::TConflictInfo info;

	static LString FormatDiff(const LString &diff)
	{
		LStringPipe output;
		for (auto &line: diff.SplitDelimit("\n"))
		{
			const char *text = line.Get();
			if (!text ||
				!*text ||
				!strncmp(text, "diff ", 5) ||
				!strncmp(text, "index ", 6) ||
				!strncmp(text, "--- ", 4) ||
				!strncmp(text, "+++ ", 4) ||
				!strncmp(text, "@@ ", 3))
				continue;

			char marker = ' ';
			if (text[0] == '-')
				marker = '<';
			else if (text[0] == '+')
				marker = '>';

			LString formatted;
			formatted.Printf("%c%s\n", marker, text + 1);
			output += formatted;
		}
		return output.Pop();
	}
	
	void LoadContext(int idx)
	{
		LString revision = idx ? info.theirs : info.ours;
		srcEd[idx]->Name("...loading...");

		folder->ConflictDiff(info, revision,
			[this, idx, revision](auto code, auto diff)
			{
				folder->GetTree()->RunCallback([this, idx, code, diff]()
					{
						srcEd[idx]->Name(diff);
					},
					_FL);
			});
	}
	
public:
	ConflictUi(VcFolder *f, LString u)
		: folder(f)
		, uri(u)
	{
		LRect r(0, 0, 1000, 900);
		SetPos(r);
		MoveToCenter();
		
		if (Attach(nullptr))
		{
			AddView(tbl = new LTableLayout(ID_TABLE));
			tbl->GetCss(true)->Padding("0.4em");
			
			int y = 0;
			LUri tmp(uri);
			
			// Path row:
			auto c = tbl->GetCell(0, y++, true, 2);
			c->Add(pathLabel = new LTextLabel(ID_PATH, 0, 0, -1, -1, tmp.Sanitize().ToString()));
			
			// Labels:
			c = tbl->GetCell(0, y);
				c->Add(new LTextLabel(ID_STATIC, 0, 0, -1, -1, "Ours:"));
			c = tbl->GetCell(1, y++);
				c->Add(new LTextLabel(ID_STATIC, 0, 0, -1, -1, "Theirs:"));

			// Hashes:
			c = tbl->GetCell(0, y);
				c->Add(hashEd[0] = new LEdit(ID_HASH0));
			c = tbl->GetCell(1, y++);
				c->Add(hashEd[1] = new LEdit(ID_HASH1));

			// Code controls:
			c = tbl->GetCell(0, y);
				c->Add(srcEd[0] = new DiffView(ID_CODE0));
				srcEd[0]->Name("...loading...");
			c = tbl->GetCell(1, y++);
				c->Add(srcEd[1] = new DiffView(ID_CODE1));

			// Code controls:
			c = tbl->GetCell(0, y);
				c->Add(new LButton(ID_SELECT0, 0, 0, -1, -1, "Select"));
			c = tbl->GetCell(1, y++);
				c->Add(new LButton(ID_SELECT1, 0, 0, -1, -1, "Select"));
				
			AttachChildren();
			Visible(true);
			
			folder->GetConflict(uri,
				[this](auto &inf)
				{
					folder->GetTree()->RunCallback([this, inf]()
						{
							info = inf;
							hashEd[0]->Name(info.ours);
							hashEd[1]->Name(info.theirs);
							
							LoadContext(0);
							LoadContext(1);
						},
						_FL);
				});
		}
	}
};
