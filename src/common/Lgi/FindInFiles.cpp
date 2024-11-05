#include "lgi/common/Lgi.h"
#include "lgi/common/FindInFiles.h"
#include "lgi/common/Popup.h"
#include "lgi/common/List.h"
#include "lgi/common/Button.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/XmlTreeUi.h"
#include "lgi/common/FileSelect.h"

enum Ctrls
{
	IDC_STATIC = -1,
	IDC_TABLE = 100,
	IDC_SEARCH,
	IDC_SEARCH_HISTORY,
	IDC_WHERE,
	IDC_WHERE_BROWSE,
	IDC_WHERE_HISTORY,
	IDC_INC_SUB_FOLDERS,
	IDC_MATCH_CASE,
	IDC_FILE_TYPES,
	IDC_MATCH_WHOLE_WORD,
	IDC_TYPES_HISTORY
};

class LHistory;
class LHistoryPopup : public LPopup
{
	LHistory *History;
	LList *Lst;

public:
	LHistoryPopup(LHistory *h);
	
	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(LColour(0, 0, 0));
		pDC->Rectangle();
	}
};

class LHistory : public LDropDown, public ResObject
{
	friend class LHistoryPopup;
	LString All;
	LString::Array Strs;
	
public:
	LHistory(int Id) :
		LDropDown(Id, 0, 0, 80, 20, 0),
		ResObject(Res_Custom)
	{
		SetPopup(new LHistoryPopup(this));
	}
	
	void Add(const char *s)
	{
		if (!s)
			return;
		for (int i=0; i<Strs.Length(); i++)
		{
			if (!_stricmp(s, Strs[i]))
				return;
		}
		Strs.New() = s;
	}
	
	bool OnLayout(LViewLayoutInfo &Inf)
	{
	    if (!Inf.Width.Max)
	    {
	        Inf.Width.Min =
	            Inf.Width.Max = (int) (LSysFont->GetHeight() * 1.6);
	    }
	    else if (!Inf.Height.Max)
	    {
	        Inf.Height.Min =
                Inf.Height.Max = LButton::Overhead.y + LSysFont->GetHeight();
	    }
	    else return false;

	    return true;
	}
	
	const char *Name()
	{
		LString Sep("\t");
		All = Sep.Join(Strs);
		return All;
	}
	
	bool Name(const char *s)
	{
		All = s;
		Strs = All.SplitDelimit("\t");
		return true;
	}
};

LHistoryPopup::LHistoryPopup(LHistory *h) : LPopup(h)
{
	History = h;
	
	LRect r(0, 0, 300, 200);
	SetPos(r);
	r.Inset(1, 1);
	AddView(Lst = new LList(100, r.x1, r.y1, r.X()-1, r.Y()-1));
	Lst->Sunken(false);
	Lst->ColumnHeaders(false);
	
	for (int i=0; i<History->Strs.Length(); i++)
	{
		LListItem *it = new LListItem;
		it->SetText(History->Strs[i]);
		Lst->Insert(it);
	}
}

struct LFindInFilesPriv : public LXmlTreeUi
{
	LTableLayout *Tbl;
	LAutoString Search;
	LDom *Store;
	LHistory *SearchHistory, *WhereHistory, *TypesHistory;

	LFindInFilesPriv()
	{
		Tbl = NULL;
		Store = NULL;
		
		Map("fif_search", IDC_SEARCH);
		Map("fif_search_history", IDC_SEARCH_HISTORY);
		Map("fif_where", IDC_WHERE);
		Map("fif_where_history", IDC_WHERE_HISTORY);
		Map("fif_inc_sub", IDC_INC_SUB_FOLDERS);
		Map("fif_match_case", IDC_MATCH_CASE);
		Map("fif_match_word", IDC_MATCH_WHOLE_WORD);
		Map("fif_types", IDC_FILE_TYPES);
		Map("fif_types_history", IDC_TYPES_HISTORY);
	}
	
	~LFindInFilesPriv()
	{
	}
};
	
LFindInFiles::LFindInFiles(LViewI *Parent, LAutoString Search, LDom *Store)
{
	d = new LFindInFilesPriv;
	d->Search = Search;
	d->Store = Store;
	SetParent(Parent);
	LRect r(7, 7, 450, 343);
	SetPos(r);

	AddView(d->Tbl = new LTableLayout(IDC_TABLE));

	int y = 0;
	int cols = 3;
	LView *v;

	Name("Find In Files");
	
	auto *c = d->Tbl->GetCell(0, y, true, cols);
	c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Find what:"));

	c = d->Tbl->GetCell(0, ++y, true, cols-1);
	c->Add(v = new LEdit(IDC_SEARCH, 0, 0, 60, 20));
	v->Focus(true);
	c = d->Tbl->GetCell(2, y);
	c->Add(d->SearchHistory = new LHistory(IDC_SEARCH_HISTORY));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Look in:"));

	c = d->Tbl->GetCell(0, ++y);
	c->Add(new LEdit(IDC_WHERE, 0, 0, 60, 20));
	c = d->Tbl->GetCell(1, y);
	c->Add(new LButton(IDC_WHERE_BROWSE, 0, 0, 20, 20, "..."));
	c = d->Tbl->GetCell(2, y);
	c->Add(d->WhereHistory = new LHistory(IDC_WHERE_HISTORY));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->PaddingTop(LCss::Len("0.5em"));
	c->Add(new LCheckBox(IDC_INC_SUB_FOLDERS, "Include sub-folders"));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->Add(new LCheckBox(IDC_MATCH_CASE, "Match case"));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->Add(new LCheckBox(IDC_MATCH_WHOLE_WORD, "Match whole word"));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->PaddingTop(LCss::Len("1em"));
	c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Look in these file types:"));

	c = d->Tbl->GetCell(0, ++y, true, cols-1);
	c->Add(new LEdit(IDC_FILE_TYPES, 0, 0, 60, 20));
	c = d->Tbl->GetCell(cols-1, y);
	c->Add(d->TypesHistory = new LHistory(IDC_TYPES_HISTORY));

	c = d->Tbl->GetCell(0, ++y, true, cols);
	c->TextAlign(LCss::AlignRight);
	c->Add(new LButton(IDOK, 0, 0, 60, 20, "Search"));
	c->Add(new LButton(IDCANCEL, 0, 0, 60, 20, "Cancel"));

	d->Convert(d->Store, this, true);
	MoveToCenter();
}

LFindInFiles::~LFindInFiles()
{
	d->Convert(d->Store, this, false);
	DeleteObj(d);
}

int LFindInFiles::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	switch (Ctrl->GetId())
	{
		case IDC_WHERE_BROWSE:
		{
			auto s = new LFileSelect(this);
			s->Open([this](auto s, auto ok)
			{
				if (ok)
				{
					LFile::Path p = s->Name();
					if (p.IsFile())
						p = p / "..";
					SetCtrlName(IDC_WHERE, p.GetFull());
				}
			});
			break;
		}
		case IDOK:
		{
			// Fall thru
			d->SearchHistory->Add(GetCtrlName(IDC_SEARCH));
			d->WhereHistory->Add(GetCtrlName(IDC_WHERE));
			d->TypesHistory->Add(GetCtrlName(IDC_FILE_TYPES));
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId() == IDOK);
			break;
		}
	}

	return LDialog::OnNotify(Ctrl, n);
}
