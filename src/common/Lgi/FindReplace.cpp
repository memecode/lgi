/*hdr
**      FILE:           GFindRepalce.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           14/11/2001
**      DESCRIPTION:    Common find and replace windows
**
**      Copyright (C) 2001, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/Button.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/LgiRes.h"

////////////////////////////////////////////////////////////////////////////
LFindReplaceCommon::LFindReplaceCommon()
{
	MatchWord = false;
	MatchCase = false;
	SelectionOnly = false;
}

////////////////////////////////////////////////////////////////////////////
// Find Window
#define IDS_16						1000
#define IDC_TEXT					1001
#define IDC_MATCH_WORD				1004
#define IDC_MATCH_CASE				1005
#define IDC_PREV_SEARCH				1006
#define IDC_SELECTION_ONLY			1007
#define IDC_FIND_TABLE              1008
#define IDC_SEARCH_UP				1009

class LFindDlgPrivate
{
public:
	LEdit *Edit;
	LFindReplaceCommon::Callback Callback;
	void *CallbackData;
};

LFindDlg::LFindDlg(LView *Parent, Callback Cb, const char *Init)
{
	d = new LFindDlgPrivate;
	if (Init)
		Find = Init;
	d->Callback = Cb;

	SetParent(Parent);
	Name(LLoadString(L_FR_FIND, "Find"));

	LRect r(0, 0, 450, 370);
	SetPos(r);
	ScaleSizeToDpi();
	MoveSameScreen(Parent);

    LTableLayout *t;
    if (AddView(t = new LTableLayout(IDC_FIND_TABLE)))
    {        
        int Row = 0;
        LLayoutCell *c = t->GetCell(0, Row);
        c->Add(new LTextLabel(IDS_16, 14, 14, -1, -1, LLoadString(L_FR_FIND_WHAT, "Find what:")));
        
        c = t->GetCell(1, Row);
    	c->Add(d->Edit = new LEdit(IDC_TEXT, 91, 7, 168, 21, ""));
    	
    	c = t->GetCell(2, Row++);
    	c->Add(new LButton(IDOK, 294, 7, 70, 21, LLoadString(L_FR_FIND_NEXT, "Find Next")));
    	
    	c = t->GetCell(0, Row, true, 2, 1);
    	c->Add(new LCheckBox(IDC_MATCH_WORD, 14, 42, -1, -1, LLoadString(L_FR_MATCH_WORD, "Match &whole word only")));
    	
    	c = t->GetCell(2, Row++);
	    c->Add(new LButton(IDCANCEL, 294, 35, 70, 21, LLoadString(L_BTN_CANCEL, "Cancel")));
	    
	    c = t->GetCell(0, Row++, true, 2, 1);
	    c->Add(new LCheckBox(IDC_MATCH_CASE, 14, 63, -1, -1, LLoadString(L_FR_MATCH_CASE, "Match &case")));
	    
	    c = t->GetCell(0, Row);
	    c->Add(new LCheckBox(IDC_SELECTION_ONLY, 14, 84, -1, -1, LLoadString(L_FR_SELECTION_ONLY, "&Selection only")));

	    c = t->GetCell(1, Row++);
	    c->Add(new LCheckBox(IDC_SEARCH_UP, 0, 0, -1, -1, "Search &upwards"));
	    
	    OnPosChange();
	    
	    LRect TblPos = t->GetPos();
		int TblY = TblPos.Y();
		LRect Used = t->GetUsedArea();
		int UsedY = Used.Y();
	    
		int Over = TblY - UsedY;
	    r = GetPos();
        r.y2 -= Over - (LTableLayout::CellSpacing<<1);
	    SetPos(r);
	}	
    
	if (d->Edit)
		d->Edit->Focus(true);
}

LFindDlg::~LFindDlg()
{
	DeleteObj(d);
}

void LFindDlg::OnPosChange()
{
    LTableLayout *t;
    if (GetViewById(IDC_FIND_TABLE, t))
    {
        LRect c = GetClient();
        c.Inset(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
        t->SetPos(c);
    }
}

void LFindDlg::OnCreate()
{
	// Load controls
	if (Find)
		SetCtrlName(IDC_TEXT, Find);
	SetCtrlValue(IDC_MATCH_WORD, MatchWord);
	SetCtrlValue(IDC_MATCH_CASE, MatchCase);
	
	if (d->Edit)
	{
		d->Edit->Select(0);
		d->Edit->Focus(true);
	}
}

int LFindDlg::OnNotify(LViewI *Ctrl, LNotification n)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		{
			// Save controls
			Find = GetCtrlName(IDC_TEXT);
			MatchWord = GetCtrlValue(IDC_MATCH_WORD) != 0;
			MatchCase = GetCtrlValue(IDC_MATCH_CASE) != 0;
			SelectionOnly = GetCtrlValue(IDC_SELECTION_ONLY) != 0;
			SearchUpwards = GetCtrlValue(IDC_SEARCH_UP) != 0;
			
			// printf("%s:%i Find OnNot %s, %i, %i\n", _FL, Find, MatchWord, MatchCase);

			if (d->Callback)
			{
				d->Callback(this, Ctrl->GetId());
				break;
			}
			// else fall thru
		}
		case IDCANCEL:
		{
			if (IsModal())
				EndModal(Ctrl->GetId());
			else
				EndModeless();
			break;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Replace Window

#define IDS_33						1009
#define IDC_REPLACE_WITH			1010
#define IDC_PREV_REPLACE			1011

class LReplaceDlgPrivate
{
public:
	LFindReplaceCommon::Callback Callback;
	void *CallbackData;
};

LReplaceDlg::LReplaceDlg(LView *Parent, Callback Cb, char *InitFind, char *InitReplace)
{
	d = new LReplaceDlgPrivate;
	d->Callback = Cb;
	if (InitFind)
		Find = InitFind;
	Replace = NewStr(InitReplace);
	MatchWord = false;
	MatchCase = false;

	SetParent(Parent);
	Name(LLoadString(L_FR_REPLACE, "Replace"));

	LView *f = 0;
	
	LRect r(0, 0, 450, 300);
	SetPos(r);
	ScaleSizeToDpi();
	MoveToCenter();
	
	LTableLayout *t;
	if (AddView(t = new LTableLayout(IDC_FIND_TABLE)))
	{
	    int Row = 0;
        LLayoutCell *c = t->GetCell(0, Row);
    	c->Add(new LTextLabel(-1, 14, 14, -1, -1, LLoadString(L_FR_FIND_WHAT, "Find what:")));

        c = t->GetCell(1, Row);
    	c->Add(f = new LEdit(IDC_TEXT, 0, 0, 60, 20, ""));

        c = t->GetCell(2, Row++);
    	c->Add(new LButton(IDC_FR_FIND, 0, 0, -1, -1, LLoadString(L_FR_FIND_NEXT, "Find Next")));
	
        c = t->GetCell(0, Row);	
	    c->Add(new LTextLabel(-1, 0, 0, -1, -1, LLoadString(L_FR_REPLACE_WITH, "Replace with:")));

        c = t->GetCell(1, Row);	
    	c->Add(new LEdit(IDC_REPLACE_WITH, 0, 0, -1, -1, ""));

        c = t->GetCell(2, Row++);	
    	c->Add(new LButton(IDC_FR_REPLACE, 0, 0, -1, -1, LLoadString(L_FR_REPLACE, "Replace")));

        c = t->GetCell(0, Row, true, 2);
    	c->Add(new LCheckBox(IDC_MATCH_WORD, 14, 70, -1, -1, LLoadString(L_FR_MATCH_WORD, "Match whole &word only")));

        c = t->GetCell(2, Row++);	
    	c->Add(new LButton(IDOK, 0, 0, -1, -1, LLoadString(L_FR_REPLACE_ALL, "Replace &All")));

        c = t->GetCell(0, Row, true, 2);
    	c->Add(new LCheckBox(IDC_MATCH_CASE, 14, 91, -1, -1, LLoadString(L_FR_MATCH_CASE, "Match &case")));

        c = t->GetCell(2, Row++);	
    	c->Add(new LButton(IDCANCEL, 0, 0, -1, -1, LLoadString(L_BTN_CANCEL, "Cancel")));

        c = t->GetCell(0, Row);
	    c->Add(new LCheckBox(IDC_SELECTION_ONLY, 14, 112, -1, -1, LLoadString(L_FR_SELECTION_ONLY, "&Selection only")));
        c = t->GetCell(1, Row);
    	c->Add(new LCheckBox(IDC_SEARCH_UP, 14, 91, -1, -1, "Search &upwards"));

	
	    OnPosChange();
	    
	    LRect u = t->GetUsedArea();
	    int Over = t->GetPos().Y() - u.Y();
	    r = GetPos();
        r.y2 -= Over - (LTableLayout::CellSpacing << 1);
	    SetPos(r);
	}
	
	if (f) f->Focus(true);
}

LReplaceDlg::~LReplaceDlg()
{
	DeleteObj(d);
}

void LReplaceDlg::OnCreate()
{
	if (Find)
		SetCtrlName(IDC_TEXT, Find);
	if (Replace)
		SetCtrlName(IDC_REPLACE_WITH, Replace);
	SetCtrlValue(IDC_MATCH_WORD, MatchWord);
	SetCtrlValue(IDC_MATCH_CASE, MatchCase);
	SetCtrlValue(IDC_SELECTION_ONLY, SelectionOnly);
	SetCtrlValue(IDC_SEARCH_UP, SearchUpwards);
}

int LReplaceDlg::OnNotify(LViewI *Ctrl, LNotification n)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		case IDC_FR_FIND:
		case IDC_FR_REPLACE:
		{
			Find = GetCtrlName(IDC_TEXT);
			Replace = GetCtrlName(IDC_REPLACE_WITH);
			MatchWord = GetCtrlValue(IDC_MATCH_WORD) != 0;
			MatchCase = GetCtrlValue(IDC_MATCH_CASE) != 0;
			SelectionOnly = GetCtrlValue(IDC_SELECTION_ONLY) != 0;
			SearchUpwards = GetCtrlValue(IDC_SEARCH_UP) != 0;

			if (d->Callback)
			{
				d->Callback(this, Ctrl->GetId());
				break;
			}
			// else fall thru
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId());
			break;
		}
	}

	return 0;
}

void LReplaceDlg::OnPosChange()
{
    LTableLayout *t;
    if (GetViewById(IDC_FIND_TABLE, t))
    {
        LRect c = GetClient();
        c.Inset(LTableLayout::CellSpacing, LTableLayout::CellSpacing);
        t->SetPos(c);
    }
}

