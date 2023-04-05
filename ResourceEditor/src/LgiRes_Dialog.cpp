/*
**	FILE:			LgiRes_Dialog.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			5/8/1999
**	DESCRIPTION:	Dialog Resource Editor
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

// Old control offset 3,17

////////////////////////////////////////////////////////////////////
#include <stdlib.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Button.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Token.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Menu.h"
#include "lgi/common/StatusBar.h"
#include "lgi/common/ToolBar.h"

#include "LgiResEdit.h"
#include "LgiRes_Dialog.h"
#include "resdefs.h"

////////////////////////////////////////////////////////////////////
#define IDC_UP						101
#define IDC_DOWN					102

#define DEBUG_OVERLAY				0

int GOOBER_SIZE						= 6;
int GOOBER_BORDER					= 8;

// Name mapping table
class LgiObjectName
{
public:
	int Type;
	const char *ObjectName;
	const char *ResourceName;
	bool ToolbarBtn;
}
NameMap[] =
{
	// ID				Lgi's name			Resource editor name
	{UI_DIALOG,			"LDialog",			Res_Dialog,			false},
	{UI_TABLE,			"LTableLayout",		Res_Table,			true},
	{UI_TEXT,			"LText",			Res_StaticText,		true},
	{UI_EDITBOX,		"LEdit",			Res_EditBox,		true},
	{UI_CHECKBOX,		"LCheckBox",		Res_CheckBox,		true},
	{UI_BUTTON,			"LButton",			Res_Button,			true},
	{UI_GROUP,			"LRadioGroup",		Res_Group,			true},
	{UI_RADIO,			"LRadioButton",		Res_RadioBox,		true},
	{UI_TABS,			"LTabView",			Res_TabView,		true},
	{UI_TAB,			"LTabPage",			Res_Tab,			false},
	{UI_LIST,			"LList",			Res_ListView,		true},
	{UI_COLUMN,			"LListColumn",		Res_Column,			false},
	{UI_COMBO,			"LCombo",			Res_ComboBox,		true},
	{UI_TREE,			"LTree",			Res_TreeView,		true},
	{UI_BITMAP,			"LBitmap",			Res_Bitmap,			true},
	{UI_PROGRESS,		"LProgressView",	Res_Progress,		true},
	{UI_SCROLL_BAR,		"LScrollBar",		Res_ScrollBar,		true},
	{UI_CUSTOM,			"LCustom",			Res_Custom,			true},
	{UI_CONTROL_TREE,	"LControlTree",		Res_ControlTree,	true},

	// If you add a new control here update ResDialog::CreateCtrl(int Tool) as well

	{0, 0, 0, 0}
};

class CtrlItem : public LListItem
{
	friend class TabOrder;

	ResDialogCtrl *Ctrl;

public:
	CtrlItem(ResDialogCtrl *ctrl)
	{
		Ctrl = ctrl;
	}

	const char *GetText(int Col)
	{
		switch (Col)
		{
			case 0:
			{
				if (Ctrl && Ctrl->GetStr())
					return Ctrl->GetStr()->GetDefine();
				break;
			}
			case 1:
			{
				if (Ctrl && Ctrl->GetStr())
					return Ctrl->GetStr()->Get();
				break;
			}
		}
		return NULL;
	}
};

class TabOrder : public LDialog
{
	ResDialogCtrl *Top;
	LList *Lst;
	LButton *Ok;
	LButton *Cancel;
	LButton *Up;
	LButton *Down;

public:
	TabOrder(LView *Parent, ResDialogCtrl *top)
	{
		Top = top;
		SetParent(Parent);

		Children.Insert(Lst = new LList(IDC_LIST, 10, 10, 350, 300));
		Children.Insert(Ok = new LButton(IDOK, Lst->GetPos().x2 + 10, 10, 60, 20, "Ok"));
		Children.Insert(Cancel = new LButton(IDCANCEL, Lst->GetPos().x2 + 10, Ok->GetPos().y2 + 5, 60, 20, "Cancel"));
		Children.Insert(Up = new LButton(IDC_UP, Lst->GetPos().x2 + 10, Cancel->GetPos().y2 + 15, 60, 20, "Up"));
		Children.Insert(Down = new LButton(IDC_DOWN, Lst->GetPos().x2 + 10, Up->GetPos().y2 + 5, 60, 20, "Down"));

		LRect r(0, 0, Ok->GetPos().x2 + 17, Lst->GetPos().y2 + 40);
		SetPos(r);
		MoveToCenter();

		Lst->AddColumn("#define", 150);
		Lst->AddColumn("Text", 150);
		Lst->MultiSelect(false);

		if (Top)
		{
			for (auto c: Top->View()->IterateViews())
			{
				ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(c);
				if (Ctrl->GetType() != UI_TEXT)
				{
					Lst->Insert(new CtrlItem(Ctrl));
				}
			}

			char s[256];
			snprintf(s, sizeof(s), "Set Tab Order: %s", Top->GetStr()->GetDefine());
			Name(s);
		}
	}

	int OnNotify(LViewI *Ctrl, LNotification n)
	{
		int MoveDir = 1;
		switch (Ctrl->GetId())
		{
			case IDC_UP:
			{
				MoveDir = -1;
				// fall through
			}
			case IDC_DOWN:
			{
				if (Lst)
				{
					CtrlItem *Sel = dynamic_cast<CtrlItem*>(Lst->GetSelected());
					if (Sel)
					{
						int Index = Lst->IndexOf(Sel);
						CtrlItem *Up = dynamic_cast<CtrlItem*>(Lst->ItemAt(Index+MoveDir));
						if (Up)
						{
							Lst->Remove(Sel);
							Lst->Insert(Sel, Index+MoveDir);
							Sel->Select(true);
							Sel->ScrollTo();
						}
					}
				}
				break;
			}
			case IDOK:
			{
				if (Lst)
				{
					int i=0;

					List<CtrlItem> All;
					Lst->GetAll(All);
					for (auto n: All)
					{
						Top->View()->DelView(n->Ctrl->View());
						Top->View()->AddView(n->Ctrl->View(), i++);
					}
				}
				// fall through
			}
			case IDCANCEL:
			{
				EndModal(0);
				break;
			}
		}

		return 0;
	}
};

////////////////////////////////////////////////////////////////////
void DrawGoobers(LSurface *pDC, LRect &r, LRect *Goobers, LColour c, int OverIdx)
{
	int Mx = (r.x2 + r.x1) / 2 - (GOOBER_SIZE / 2);
	int My = (r.y2 + r.y1) / 2 - (GOOBER_SIZE / 2);

	pDC->Colour(c);

	Goobers[0].x1 = r.x1 - GOOBER_BORDER;
	Goobers[0].y1 = r.y1 - GOOBER_BORDER;
	Goobers[0].x2 = r.x1 - (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[0].y2 = r.y1 - (GOOBER_BORDER - GOOBER_SIZE);

	Goobers[1].x1 = Mx;
	Goobers[1].y1 = r.y1 - GOOBER_BORDER;
	Goobers[1].x2 = Mx + GOOBER_SIZE;
	Goobers[1].y2 = r.y1 - (GOOBER_BORDER - GOOBER_SIZE);

	Goobers[2].x1 = r.x2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[2].y1 = r.y1 - GOOBER_BORDER;
	Goobers[2].x2 = r.x2 + GOOBER_BORDER;
	Goobers[2].y2 = r.y1 - (GOOBER_BORDER - GOOBER_SIZE);

	Goobers[3].x1 = r.x2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[3].y1 = My;
	Goobers[3].x2 = r.x2 + GOOBER_BORDER;
	Goobers[3].y2 = My + GOOBER_SIZE;

	Goobers[4].x1 = r.x2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[4].y1 = r.y2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[4].x2 = r.x2 + GOOBER_BORDER;
	Goobers[4].y2 = r.y2 + GOOBER_BORDER;

	Goobers[5].x1 = Mx;
	Goobers[5].y1 = r.y2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[5].x2 = Mx + GOOBER_SIZE;
	Goobers[5].y2 = r.y2 + GOOBER_BORDER;

	Goobers[6].x1 = r.x1 - GOOBER_BORDER;
	Goobers[6].y1 = r.y2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[6].x2 = r.x1 - (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[6].y2 = r.y2 + GOOBER_BORDER;

	Goobers[7].x1 = r.x1 - GOOBER_BORDER;
	Goobers[7].y1 = My;
	Goobers[7].x2 = r.x1 - (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[7].y2 = My + GOOBER_SIZE;

	for (int i=0; i<8; i++)
	{
		if (OverIdx == i)
			pDC->Rectangle(Goobers+i);
		else
			pDC->Box(Goobers+i);
	}
}

////////////////////////////////////////////////////////////////////
int ResDialogCtrl::TabDepth = 0;

ResDialogCtrl::ResDialogCtrl(ResDialog *dlg, const char *CtrlTypeName, LXmlTag *load) :
	ResObject(CtrlTypeName)
{
	Dlg = dlg;
	Client.ZOff(-1, -1);
	SelectStart.ZOff(-1, -1);
	
	if (load)
	{
		// Base a string off the xml
		int r = load->GetAsInt("ref");
		if (Dlg)
		{
			SetStr(Dlg->Symbols->FindRef(r));
			LAssert(GetStr());

			if (!GetStr()) // oh well we should have one anyway... fix things up so to speak.
				SetStr(Dlg->Symbols->CreateStr());
		}

		LAssert(GetStr());
	}
	else if (Dlg->CreateSymbols)
	{
		// We create a symbol for this resource
		SetStr((Dlg && Dlg->Symbols) ? Dlg->Symbols->CreateStr() : NULL);
		if (GetStr())
		{
			char Def[256];
			snprintf(Def, sizeof(Def), "IDC_%i", GetStr()->GetRef());
			GetStr()->SetDefine(Def);
		}
	}
	else
	{
		// Someone else is going to create the symbol
		SetStr(NULL);
	}
}

ResDialogCtrl::~ResDialogCtrl()
{
	if (ResDialog::Symbols)
	{
		auto s = GetStr();
		SetStr(NULL);
		ResDialog::Symbols->DeleteStr(s);
	}

	if (Dlg)
	{
		Dlg->App()->OnObjDelete(this);
		Dlg->OnDeselect(this);
	}
}

char *ResDialogCtrl::GetRefText()
{
	static char Buf[64];
	if (GetStr())
	{
		snprintf(Buf, sizeof(Buf), "Ref=%i", GetStr()->GetRef());
	}
	else
	{
		Buf[0] = 0;
	}
	return Buf;
}

void ResDialogCtrl::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	for (LViewI *w: View()->IterateViews())
	{
		ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
		LAssert(c);
		if (c)
		{
			l.Insert(c);
			if (Deep)
			{
				c->ListChildren(l);
			}
		}
	}
}

LRect ResDialogCtrl::GetMinSize()
{
	LRect m(0, 0, GRID_X-1, GRID_Y-1);

	if (IsContainer())
	{
		LRect cli = View()->GetClient(false);

		for (LViewI *c: View()->IterateViews())
		{
			LRect cpos = c->GetPos();
			cpos.Offset(cli.x1, cli.y1);
			m.Union(&cpos);
		}
	}

	return m;
}

bool ResDialogCtrl::SetPos(LRect &p, bool Repaint)
{
	LRect m = GetMinSize();
	if (m.X() > p.X()) p.x2 = p.x1 + m.X() - 1;
	if (m.Y() > p.Y()) p.y2 = p.y1 + m.Y() - 1;

	if (p != View()->GetPos())
	{
		/*
		if (ParentCtrl)
		{
			if (p.x1 < ParentCtrl->Client.x1)
			{
				p.Offset(ParentCtrl->Client.x1 - p.x1, 0);
			}

			if (p.y1 < ParentCtrl->Client.y1)
			{
				p.Offset(0, ParentCtrl->Client.y1 - p.y1);
			}
		}
		*/

		// set our size
		bool Status = View()->SetPos(p, Repaint);
		
		// tell everyone else about the change
		OnFieldChange();

		LRect r(0, 0, p.X()-1, p.Y()-1);
		r.Inset(-GOOBER_BORDER, -GOOBER_BORDER);
		View()->Invalidate(&r, false, true);
		
		// check our parents are big enough to show us...
		ResDialogCtrl *Par = ParentCtrl();
		if (Par)
		{
			LRect t = Par->View()->GetPos();
			Par->ResDialogCtrl::SetPos(t, true);
		}
		
		return Status;
	}

	return true;
}

bool ResDialogCtrl::SetStr(ResString *s)
{
	if (_Str == s)
	{
		if (_Str)
			LAssert(_Str->Refs.HasItem(this));
		return true;
	}

	if (_Str)
	{
		if (!_Str->Refs.HasItem(this))
		{
			LAssert(!"Refs incorrect.");
			return false;
		}

		_Str->Refs.Delete(this);
	}

	_Str = s;

	if (_Str)
	{
		if (_Str->Refs.HasItem(this))
		{
			LAssert(!"Refs already has us.");
			return false;
		}

		_Str->Refs.Add(this);
	}
	else
	{
		// LgiTrace("%s:%i - %p::SetStr(NULL)\n", _FL, this);
	}

	return true;
}

void ResDialogCtrl::TabString(char *Str)
{
	if (!Str)
		return;

	memset(Str, '\t', TabDepth);
	Str[TabDepth] = 0;
}

LRect ResDialogCtrl::AbsPos()
{
	LViewI *w = View();
	LRect r = w->GetPos();
	r.Offset(-r.x1, -r.y1);

	for (; w && w != Dlg; w = w->GetParent())
	{
		LRect pos = w->GetPos();
		if (w->GetParent())
		{
			// LView *Ctrl = w->GetParent()->GetGView();

			LRect client = w->GetParent()->GetClient(false);
			r.Offset(pos.x1 + client.x1, pos.y1 + client.y1);
		}
		else
		{
			r.Offset(pos.x1, pos.y1);
		}
	}

	return r;
}

void ResDialogCtrl::StrFromRef(int Ref)
{
	// get the string object
	SetStr(Dlg->App()->GetStrFromRef(Ref));
	if (!GetStr())
	{
		LgiTrace("%s:%i - String with ref '%i' missing.\n", _FL, Ref);
		LAssert(0);
		if (SetStr(Dlg->App()->GetDialogSymbols()->CreateStr()))
		{
			GetStr()->SetRef(Ref);
		}
		else return;
	}
	
	// if this assert fails then the Ref doesn't exist
	// and the string can't be found

	// if this assert fails then the Str is already
	// associated with a control, and thus we would
	// duplicate the pointer to the string if we let
	// it go by
	// LAssert(Str->UpdateWnd == 0);

	// set the string's control to us
	GetStr()->UpdateWnd = View();

	// make the strings refid unique
	GetStr()->UnDuplicate();
	
	View()->Name(GetStr()->Get());
}

bool ResDialogCtrl::GetFields(FieldTree &Fields)
{
	if (GetStr())
	{
		GetStr()->GetFields(Fields);
	}

	int Id = 101;
	Fields.Insert(this, DATA_STR, Id++, VAL_Pos, "Pos");
	Fields.Insert(this, DATA_BOOL, Id++, VAL_Visible, "Visible");
	Fields.Insert(this, DATA_BOOL, Id++, VAL_Enabled, "Enabled");
	Fields.Insert(this, DATA_STR, Id++, VAL_Class, "Class", -1);
	Fields.Insert(this, DATA_STR, Id++, VAL_Style, "Style", -1, true);

	return true;
}

bool ResDialogCtrl::Serialize(FieldTree &Fields)
{
	if ((Fields.GetMode() == FieldTree::ObjToUi ||
		Fields.GetMode() == FieldTree::UiToObj) &&
		GetStr())
	{
		GetStr()->Serialize(Fields);
	}

	LRect r = View()->GetPos(), Old = View()->GetPos();
	bool e = true;

	if (Fields.GetMode() == FieldTree::ObjToUi ||
		Fields.GetMode() == FieldTree::ObjToStore)
	{
		r = View()->GetPos();
		e = View()->Enabled();
	}

	Fields.Serialize(this, VAL_Pos, r);
	Fields.Serialize(this, VAL_Visible, Vis, true);
	Fields.Serialize(this, VAL_Enabled, e, true);
	Fields.Serialize(this, VAL_Class, CssClass);
	Fields.Serialize(this, VAL_Style, CssStyle);

	if (Fields.GetMode() == FieldTree::UiToObj ||
		Fields.GetMode() == FieldTree::StoreToObj)
	{
		View()->Enabled(e);
		SetPos(r);
		r.Union(&Old);
		r.Inset(-GOOBER_BORDER, -GOOBER_BORDER);
		if (View()->GetParent())
		{
			View()->GetParent()->Invalidate(&r);
		}
	}

	if (Dlg && Dlg->Item)
		Dlg->Item->Update();

	return true;
}

void ResDialogCtrl::CopyText()
{
	if (GetStr())
		GetStr()->CopyText();
}

void ResDialogCtrl::PasteText()
{
	if (GetStr())
	{
		GetStr()->PasteText();
		View()->Invalidate();
	}
}

bool ResDialogCtrl::AttachCtrl(ResDialogCtrl *Ctrl, LRect *r)
{
	bool Status = false;
	if (Ctrl)
	{
		Ctrl->View()->Visible(true);
		View()->AddView(Ctrl->View());
		Ctrl->View()->SetParent(View());
		if (r)
		{
			if (!dynamic_cast<CtrlTable*>(Ctrl->View()->GetParent()))
			{
				Dlg->SnapRect(r, this);
			}
			Ctrl->SetPos(*r);
		}

		if (Dlg->Resource::IsSelected())
		{
			Dlg->OnSelect(Ctrl);
		}

		Status = true;
	}
	return Status;
}

void ResDialogCtrl::OnPaint(LSurface *pDC)
{
	if (DragCtrl >= 0)
	{
		LRect r = DragRgn;
		r.Normal();
		pDC->Colour(L_FOCUS_SEL_BACK);
		pDC->Box(&r);
	}
}

LMouse ResDialogCtrl::MapToDialog(LMouse m)
{
	// Convert co-ords from out own local space to be relative to 'Dlg'
	// the parent dialog.
	LMouse Ms = m;
	LViewI *Parent;
	for (LViewI *i = View(); i && i != (LViewI*)Dlg; i = Parent)
	{
		Parent = i->GetParent();
		LRect Pos = i->GetPos(), Cli = i->GetClient(false);

		#if DEBUG_OVERLAY
		LgiTrace("%s %i,%i + %i,%i + %i,%i = %i,%i\n",
			i->GetClass(),
			Ms.x, Ms.y, Pos.x1, Pos.y1, Cli.x1, Cli.y1, Ms.x + Pos.x1 + Cli.x1, Ms.y + Pos.y1 + Cli.y1);
		#endif
		Ms.x += Pos.x1 + Cli.x1;
		Ms.y += Pos.y1 + Cli.y1;

	}
	return Ms;
}

void ResDialogCtrl::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		if (m.Left())
		{
			// LgiTrace("Click down=%i %i,%i\n", m.Down(), m.x, m.y);
			if (Dlg)
			{
				#if DEBUG_OVERLAY
				LPoint Prev(0, 0);
				auto &DebugOverlay = Dlg->DebugOverlay;
				if (!DebugOverlay)
				{
					DebugOverlay.Reset(new LMemDC(Dlg->X(), Dlg->Y(), System32BitColourSpace));
					DebugOverlay->Colour(0, 32);
					DebugOverlay->Rectangle();
					DebugOverlay->Colour(LColour(64, 192, 64));
				}
				#endif

				bool Processed = false;
				LRect c = View()->GetClient();
				bool ClickedThis = c.Overlap(m.x, m.y);

				// Convert co-ords from out own local space to be relative to 'Dlg'
				// the parent dialog.
				LMouse Ms = MapToDialog(m);
				#if DEBUG_OVERLAY
				if (DebugOverlay)
				{
					DebugOverlay->Line(Prev.x, Prev.y, Ms.x, Ms.y);
					DebugOverlay->Circle(Ms.x, Ms.y, 5);
					Prev.x = Ms.x; Prev.y = Ms.y;
				}
				#endif
				Dlg->OnMouseClick(Ms);

				if (ClickedThis &&
					!Dlg->IsDraging())
				{
					DragCtrl = Dlg->CurrentTool();
					if ((DragCtrl > 0 && AcceptChildren) ||
						((DragCtrl == 0) && !Movable))
					{
						LPoint p(m.x, m.y);
						Dlg->SnapPoint(&p, ParentCtrl());

						DragStart.x = DragRgn.x1 = DragRgn.x2 = p.x;
						DragStart.y = DragRgn.y1 = DragRgn.y2 = p.y;
						View()->Capture(true);
						Processed = true;
					}
					else
					{
						DragCtrl = -1;
						if (Movable)
						{
							DragRgn.x1 = m.x;
							DragRgn.y1 = m.y;
							MoveCtrl = true;
							Processed = true;
							View()->Capture(true);
						}
					}

					SelectMode = (m.Shift()) ? SelAdd : SelSet;
					SelectStart = View()->GetPos();
				}
			}
		}
		else if (m.IsContextMenu())
		{
			LSubMenu RClick;
			bool PasteData = false;
			bool PasteTranslations = false;
			CtrlButton *Btn = dynamic_cast<CtrlButton*>(this);

			{
				LClipBoard c(Dlg);
				char *Clip = c.Text();
				if (Clip)
				{
					PasteTranslations = strstr(Clip, TranslationStrMagic);

					char *p = Clip;
					while (*p && strchr(" \r\n\t", *p)) p++;
					PasteData = *p == '<';
				}
			}

			RClick.AppendItem("Cu&t", IDM_CUT, Dlg->Selection.Length()>0);
			RClick.AppendItem("&Copy Control", IDM_COPY, Dlg->Selection.Length()>0);
			RClick.AppendItem("&Paste", IDM_PASTE, PasteData);
			RClick.AppendSeparator();

			RClick.AppendItem("Copy Text", IDM_COPY_TEXT, Dlg->Selection.Length()==1);
			RClick.AppendItem("Paste Text", IDM_PASTE_TEXT, PasteTranslations);
			RClick.AppendSeparator();

			RClick.AppendItem("&Delete", IDM_DELETE, Dlg->Selection.Length()>0);

			if (Btn)
			{
				RClick.AppendSeparator();
				RClick.AppendItem("Set 'Ok'", IDM_SET_OK);
				RClick.AppendItem("Set 'Cancel'", IDM_SET_CANCEL);
			}

			if (Dlg->GetMouse(m, true))
			{
				int Cmd = 0;
				switch (Cmd = RClick.Float(Dlg, m.x, m.y))
				{
					case IDM_DELETE:
					{
						Dlg->Delete();
						break;
					}
					case IDM_CUT:
					{
						Dlg->Copy(true);
						break;
					}
					case IDM_COPY:
					{
						Dlg->Copy();
						break;
					}
					case IDM_PASTE:
					{
						Dlg->Paste();
						break;
					}
					case IDM_COPY_TEXT:
					{
						ResDialogCtrl *Ctrl = Dlg->Selection[0];
						if (Ctrl)
							Ctrl->CopyText();
						break;
					}
					case IDM_PASTE_TEXT:
					{
						PasteText();
						break;
					}
					case IDM_SET_OK:
					{
						ResString *s = Btn->GetStr();
						if (s)
						{
							s->Set("Ok");
							s->SetDefine("IDOK");
						}
						break;
					}
					case IDM_SET_CANCEL:
					{
						ResString *s = Btn->GetStr();
						if (s)
						{
							s->Set("Cancel");
							s->SetDefine("IDCANCEL");
						}
						break;
					}
				}
			}
		}
	}
	else
	{
		if (DragCtrl >= 0)
		{
			bool Exit = false;

			if (Dlg &&
				(DragRgn.X() > 1 ||
				DragRgn.Y() > 1))
			{
				if (DragCtrl == 0)
				{
					Dlg->SelectRect(this, &DragRgn, SelectMode != SelAdd);
				}
				else
				{
					ResDialogCtrl *Ctrl = Dlg->CreateCtrl(DragCtrl, 0);
					if (Ctrl)
					{
						AttachCtrl(Ctrl, &DragRgn);
					}
				}

				Exit = true;
			}

			DragCtrl = -1;
			View()->Invalidate();
			View()->Capture(false);

			if (Exit)
			{
				return;
			}
		}
		
		if (MoveCtrl)
		{
			View()->Capture(false);
			MoveCtrl = false;
		}
		
		if (SelectMode > SelNone)
		{
			LRect r = View()->GetPos();
			if (SelectStart == r)
			{
				Dlg->OnSelect(this, SelectMode != SelAdd);
			}
			SelectMode = SelNone;
		}
	}
}

void ResDialogCtrl::OnMouseMove(LMouse &m)
{
	// Drag a rubber band...
	if (DragCtrl >= 0)
	{
		LRect Old = DragRgn;

		DragRgn.x1 = DragStart.x;
		DragRgn.y1 = DragStart.y;
		DragRgn.x2 = m.x;
		DragRgn.y2 = m.y;
		DragRgn.Normal();

		Dlg->SnapRect(&DragRgn, this);

		Old.Union(&DragRgn);
		Old.Inset(-1, -1);

		View()->Invalidate(&Old);
	}

	if (Dlg)
	{
		LMouse Ms = MapToDialog(m);
		Dlg->OnMouseMove(Ms);
	}
	
	// Move some ctrl(s)
	if (MoveCtrl && !m.Shift())
	{
		int Dx = m.x - DragRgn.x1;
		int Dy = m.y - DragRgn.y1;
		if (Dx != 0 || Dy != 0)
		{
			// LgiTrace("Move %i,%i + %i,%i\n", m.x, m.y, Dx, Dy);
			if (!Dlg->IsSelected(this))
			{
				Dlg->OnSelect(this);
			}

			LRect Old = View()->GetPos();
			LRect New = Old;
			New.Offset(	m.x - DragRgn.x1,
						m.y - DragRgn.y1);

			LPoint p(New.x1, New.y1);
			Dlg->SnapPoint(&p, ParentCtrl());
			New.Set(p.x, p.y, p.x + New.X() - 1, p.y + New.Y() - 1);

			if (New != Old)
			{
				Dlg->MoveSelection(New.x1 - Old.x1, New.y1 - Old.y1);
			}
		}
	}
}

char *ReadInt(char *s, int &Value)
{
	char *c = strchr(s, ',');
	if (c)
	{
		*c = 0;
		Value = atoi(s);
		return c+1;
	}
	Value = atoi(s);
	return 0;
}

void ResDialogCtrl::ReadPos(char *Str)
{
	if (Str)
	{
		char *s = NewStr(Str);
		if (s)
		{
			LRect r = View()->GetPos();

			char *p = ReadInt(s, r.x1);
			if (p) p = ReadInt(p, r.y1);
			if (p) p = ReadInt(p, r.x2);
			if (p) p = ReadInt(p, r.y2);

			DeleteArray(s);
		}
	}
}

////////////////////////////////////////////////////////////////////
CtrlDlg::CtrlDlg(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Dialog, load)
{
	Movable = false;
	AcceptChildren = true;
	GetStr()->UpdateWnd = View();
	View()->Name("CtrlDlg");
}

IMPL_DIALOG_CTRL(CtrlDlg)

LRect &CtrlDlg::GetClient(bool InClientSpace)
{
	static LRect r;
	
	Client.Set(0, 0, View()->X()-1, View()->Y()-1);
	Client.Inset(2, 2);
	Client.y1 += LAppInst->GetMetric(LGI_MET_DECOR_CAPTION);
	if (Client.y1 > Client.y2) Client.y1 = Client.y2;

	if (InClientSpace)
		r = Client.ZeroTranslate();
	else
		r = Client;

	return r;
}

void CtrlDlg::OnNcPaint(LSurface *pDC, LRect &r)
{
	// Draw the border
	LWideBorder(pDC, r, DefaultRaisedEdge);

	// Draw the title bar
	int TitleY = LAppInst->GetMetric(LGI_MET_DECOR_CAPTION);
	LRect t = r;
	t.y2 = t.y1 + TitleY - 1;
	pDC->Colour(L_ACTIVE_TITLE);
	pDC->Rectangle(&t);
	if (GetStr())
	{
		LDisplayString ds(LSysFont, GetStr()->Get());
		LSysFont->Fore(L_ACTIVE_TITLE_TEXT);
		LSysFont->Transparent(true);
		ds.Draw(pDC, t.x1 + 10, t.y1 + ((t.Y()-ds.Y())/2));
	}

	r.y1 = t.y2 + 1;
}

void CtrlDlg::OnPaint(LSurface *pDC)
{
	Client = GetClient();

	// Draw the grid
	pDC->Colour(L_MED);
	pDC->Rectangle(&Client);
	pDC->Colour(Rgb24(0x80, 0x80, 0x80), 24);
	for (int y=Client.y1; y<Client.y2; y+=GRID_Y)
	{
		for (int x=Client.x1; x<Client.x2; x+=GRID_X)
			pDC->Set(x, y);
	}

	ResDialogCtrl::OnPaint(pDC);
}

/////////////////////////////////////////////////////////////////////
// Text box
CtrlText::CtrlText(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_StaticText, load)
{
	if (GetStr() && !load)
	{
		GetStr()->SetDefine("IDC_STATIC");
	}
}

IMPL_DIALOG_CTRL(CtrlText)

void CtrlText::OnPaint(LSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	char *Text = GetStr()->Get();
	LSysFont->Fore(L_TEXT);
	LSysFont->Transparent(true);

	if (Text)
	{
		LRect Client = GetClient();

		int y = 0;
		char *Start = Text;
		for (char *s = Text; 1; s++)
		{
			if ((*s == '\\' && *(s+1) == 'n') || (*s == 0))
			{
				LDisplayString ds(LSysFont, Start, s - Start);
				ds.Draw(pDC, 0, y, &Client);
				y += 15;
				Start = s + 2;
				if (*s == '\\') s++;
			}

			if (*s == 0) break;
		}
	}
}

// Editbox

CtrlEditbox::CtrlEditbox(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_EditBox, load)
{
	Password = false;
	MultiLine = false;
}

IMPL_DIALOG_CTRL(CtrlEditbox)

void CtrlEditbox::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	Client = r;
	
	// Draw the ctrl
	LWideBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(Enabled() ? L_WORKSPACE : L_MED);
	pDC->Rectangle(&r);

	char *Text = GetStr()->Get();
	LSysFont->Fore(Enabled() ? L_TEXT : L_LOW);
	LSysFont->Transparent(true);
	if (Text)
	{
		if (Password)
		{
			char *t = NewStr(Text);
			if (t)
			{
				for (char *p = t; *p; p++) *p = '*';
				LDisplayString ds(LSysFont, t);
				ds.Draw(pDC, 4, 4);
				DeleteArray(t);
			}
		}
		else
		{
			LDisplayString ds(LSysFont, Text);
			ds.Draw(pDC, 4, 4);
		}
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

#define VAL_Password		"Pw"
#define VAL_MultiLine		"MultiLine"

bool CtrlEditbox::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	if (Status)
	{
		Fields.Insert(this, DATA_BOOL, 300, VAL_Password, "Password");
		Fields.Insert(this, DATA_BOOL, 301, VAL_MultiLine, "MultiLine");
	}
	return Status;
}

bool CtrlEditbox::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	if (Status)
	{
		Fields.Serialize(this, VAL_Password, Password, false);
		Fields.Serialize(this, VAL_MultiLine, MultiLine, false);
	}
	return Status;
}

// Check box
CtrlCheckbox::CtrlCheckbox(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_CheckBox, load)
{
}

IMPL_DIALOG_CTRL(CtrlCheckbox)

void CtrlCheckbox::OnPaint(LSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	LRect r(0, 0, 12, 12);
	
	// Draw the ctrl
	LWideBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(L_WORKSPACE);
	pDC->Rectangle(&r);

	LPoint Pt[6] = {
		LPoint(3, 4),
		LPoint(3, 7),
		LPoint(5, 10),
		LPoint(10, 5),
		LPoint(10, 2),
		LPoint(5, 7)};
	pDC->Colour(0);
	pDC->Polygon(6, Pt);
	pDC->Set(3, 5);

	char *Text = GetStr()->Get();
	if (Text)
	{
		LSysFont->Fore(L_TEXT);
		LSysFont->Transparent(true);
		LDisplayString ds(LSysFont, Text);
		ds.Draw(pDC, r.x2 + 10, r.y1-2);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Button

CtrlButton::CtrlButton(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Button, load)
{
	IsToggle = false;
}

IMPL_DIALOG_CTRL(CtrlButton)

bool CtrlButton::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	
	int Id = 160;
	Fields.Insert(this, DATA_FILENAME, Id++, VAL_Image, "Image");
	Fields.Insert(this, DATA_BOOL, Id++, VAL_Toggle, "Toggle");
	
	return Status;
}

bool CtrlButton::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	if (Status)
	{
		Fields.Serialize(this, VAL_Image, Image);
		Fields.Serialize(this, VAL_Toggle, IsToggle);
	}
	return Status;
}

void CtrlButton::OnPaint(LSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	LRect r = Client;
	char *Text = GetStr()->Get();
	
	// Draw the ctrl
	LWideBorder(pDC, r, DefaultRaisedEdge);
	LSysFont->Fore(L_TEXT);

	if (ValidStr(Text))
	{
		LSysFont->Back(L_MED);
		LSysFont->Transparent(false);

		LDisplayString ds(LSysFont, Text);
		ds.Draw(pDC, r.x1 + ((r.X()-ds.X())/2), r.y1 + ((r.Y()-ds.Y())/2), &r);
	}
	else
	{
		pDC->Colour(L_MED);
		pDC->Rectangle(&r);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Group

CtrlGroup::CtrlGroup(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Group, load)
{
	AcceptChildren = true;

	if (GetStr() && !load)
	{
		GetStr()->SetDefine("IDC_STATIC");
	}
}

IMPL_DIALOG_CTRL(CtrlGroup)

void CtrlGroup::OnPaint(LSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	LRect r = Client;
	
	// Draw the ctrl
	r.y1 += 5;
	LWideBorder(pDC, r, EdgeXpChisel);
	r.y1 -= 5;
	LSysFont->Fore(L_TEXT);
	LSysFont->Back(L_MED);
	LSysFont->Transparent(false);
	char *Text = GetStr()->Get();
	LDisplayString ds(LSysFont, Text);
	ds.Draw(pDC, r.x1 + 8, r.y1 - 2);

	// Draw children
	//LWindow::OnPaint(pDC);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

//Radio button

uint32_t RadioBmp[] = {
0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0, 0x80808080, 0x80808080, 0x80808080, 0xC0C0C0C0, 0xC0C0C0C0, 
0xC0C0C0C0, 0xC0C0C0C0, 0x8080C0C0, 0x80808080, 0x00000000, 0x00000000, 0x00000000, 0x80808080, 
0xC0C08080, 0xC0C0C0C0, 0x80C0C0C0, 0x00008080, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
0x00000000, 0xFFFF0000, 0xC0C0C0FF, 0x80C0C0C0, 0x00008080, 0xFFFFFF00, 0xFFFFFFFF, 0xFFFFFFFF, 
0xFFFFFFFF, 0xDFFFFFFF, 0xFFFFDFDF, 0xC0C0C0FF, 0x00808080, 0xFFFF0000, 0xFFFFFFFF, 0x00FFFFFF, 
0x00000000, 0xFFFFFF00, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 0x00808080, 0xFFFF0000, 0xFFFFFFFF, 
0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 0x00808080, 0xFFFF0000, 
0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 0x00808080, 
0xFFFF0000, 0xFFFFFFFF, 0x00FFFFFF, 0x00000000, 0xFFFFFF00, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 
0x80C0C0C0, 0x00008080, 0xFFFFFF00, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFFFFFFF, 0xFFFFDFDF, 
0xC0C0C0FF, 0x80C0C0C0, 0xDFDF8080, 0xDFDFDFDF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFDFDFDF, 
0xFFFFDFDF, 0xC0C0C0FF, 0xC0C0C0C0, 0xFFFFC0C0, 0xFFFFFFFF, 0xDFDFDFDF, 0xDFDFDFDF, 0xDFDFDFDF, 
0xFFFFFFFF, 0xC0C0FFFF, 0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0, 0xFFFFFFFF, 0xFFFFFFFF, 
0xFFFFFFFF, 0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0};

CtrlRadio::CtrlRadio(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_RadioBox, load)
{
	Bmp = new LMemDC;
	if (Bmp && Bmp->Create(12, 12, GdcD->GetColourSpace()))
	{
		int Len = ((Bmp->X()*24)+31)/32*4;
		for (int y=0; y<Bmp->Y(); y++)
		{
			for (int x=0; x<Bmp->X(); x++)
			{
				uchar *s = ((uchar*) RadioBmp) + (y * Len) + (x * 3);
				Bmp->Colour(Rgb24(s[0], s[1], s[2]), 24);
				Bmp->Set(x, y);
			}
		}
	}
}

CtrlRadio::~CtrlRadio()
{
	DeleteObj(Bmp);
}

IMPL_DIALOG_CTRL(CtrlRadio)

void CtrlRadio::OnPaint(LSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	LRect r(0, 0, 12, 12);
	
	// Draw the ctrl
	if (Bmp)
		pDC->Blt(r.x1, r.y1, Bmp);

	char *Text = GetStr()->Get();
	if (Text)
	{
		LSysFont->Fore(L_TEXT);
		LSysFont->Transparent(true);
		LDisplayString ds(LSysFont, Text);
		ds.Draw(pDC, r.x2 + 10, r.y1-2);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Tab

CtrlTab::CtrlTab(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Tab, load)
{
	GetStr()->UpdateWnd = this;
}

IMPL_DIALOG_CTRL(CtrlTab)

void CtrlTab::OnPaint(LSurface *pDC)
{
}

void CtrlTab::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(GetParent());
	CtrlTabs *Par = dynamic_cast<CtrlTabs*>(Ctrl);
	LAssert(Par);

	auto MyIndex = Par->Tabs.IndexOf(this);
	LAssert(MyIndex >= 0);

	List<LViewI> *CList = (Par->Current == MyIndex) ? &Par->Children : &Children;
	for (auto w: *CList)
	{
		ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
		if (c)
		{
			l.Insert(c);
			if (Deep)
			{
				c->ListChildren(l, Deep);
			}
		}
	}
}

// Tab control

CtrlTabs::CtrlTabs(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_TabView, load)
{
	AcceptChildren = true;
	Current = 0;

	if (!load)
	{
		for (int i=0; i<3; i++)
		{
			CtrlTab *t = new CtrlTab(dlg, load);
			if (t)
			{
				char Text[256];
				snprintf(Text, sizeof(Text), "Tab %i", i+1);
				if (t->GetStr())
				{
					t->GetStr()->Set(Text);
					t->SetParent(this);
					Tabs.Insert(t);
				}
				else
				{
					DeleteObj(t);
				}
			}
		}
	}
}

CtrlTabs::~CtrlTabs()
{
	Empty();
}

void CtrlTabs::OnMouseMove(LMouse &m)
{
	ResDialogCtrl::OnMouseMove(m);
}

void CtrlTabs::ShowMe(ResDialogCtrl *Child)
{
	CtrlTab *t = dynamic_cast<CtrlTab*>(Child);
	if (t)
	{
		auto Idx = Tabs.IndexOf(t);
		if (Idx >= 0)
		{
			ToTab();
			Current = Idx;
			FromTab();
		}
	}
}

void CtrlTabs::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	List<CtrlTab>::I it = Tabs.begin();
	for (CtrlTab *t = *it; t; t = *++it)
	{
		t->EnumCtrls(Ctrls);
	}

	ResDialogCtrl::EnumCtrls(Ctrls);
}

LRect CtrlTabs::GetMinSize()
{
	List<ResDialogCtrl> l;
	ListChildren(l, false);

	LRect r(0, 0, GRID_X-1, GRID_Y-1);

	/*
	// don't resize smaller than the tabs
	for (CtrlTab *t = Tabs.First(); t; t = Tabs.Next())
	{
		r.x2 = max(r.x2, t->r.x2 + 10);
		r.y2 = max(r.y2, t->r.y2 + 10);
	}
	*/

	// don't resize smaller than any of the children
	// on any of the tabs
	LRect cli = GetClient(false);
	for (auto c: l)
	{
		LRect cpos = c->View()->GetPos();
		cpos.Offset(cli.x1, cli.y1);
		r.Union(&cpos);
	}

	return r;
}

void CtrlTabs::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	int n=0;
	for (auto t: Tabs)
	{
	    l.Add(t);
	    
		auto It = (Current == n ? (LViewI*)this : (LViewI*)t)->IterateViews();
		for (LViewI *w: It)
		{
			ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
			if (c)
			{
				l.Insert(c);
				c->ListChildren(l, Deep);
			}
		}
		n++;
	}
}

void CtrlTabs::Empty()
{
	ToTab();
	Tabs.DeleteObjects();
}

void CtrlTabs::OnPaint(LSurface *pDC)
{
	// Draw the ctrl
	Title.ZOff(X()-1, 17);
	Client.ZOff(X()-1, Y()-1);
	Client.y1 = Title.y2;
	LRect r = Client;
	LWideBorder(pDC, r, DefaultRaisedEdge);

	// Draw the tabs
	int i = 0;
	int x = 2;
	for (auto Tab: Tabs)
	{
		char *Str = Tab->GetStr() ? Tab->GetStr()->Get() : 0;
		LDisplayString ds(LSysFont, Str);

		int Width = 12 + ds.X();
		LRect t(x, Title.y1 + 2, x + Width - 1, Title.y2 - 1);

		if (Current == i)
		{
			t.Inset(-2, -2);

			if (Tab->IterateViews().Length() > 0)
				FromTab();
		}

		if (Tab->View()->GetPos() != t)
			Tab->View()->SetPos(t);

		pDC->Colour(L_LIGHT);
		pDC->Line(t.x1, t.y1+2, t.x1, t.y2);
		pDC->Set(t.x1+1, t.y1+1);
		pDC->Line(t.x1+2, t.y1, t.x2-2, t.y1);

		pDC->Colour(L_MED);
		pDC->Line(t.x1+1, t.y1+2, t.x1+1, t.y2);
		pDC->Line(t.x1+2, t.y1+1, t.x2-2, t.y1+1);

		pDC->Colour(L_LOW);
		pDC->Line(t.x2-1, t.y1, t.x2-1, t.y2);
		pDC->Colour(0, 24);
		pDC->Line(t.x2, t.y1+2, t.x2, t.y2);

		t.Inset(2, 2);
		t.y2 += 2;

		LSysFont->Fore(L_TEXT);
		LSysFont->Back(L_MED);
		LSysFont->Transparent(false);
		ds.Draw(pDC, t.x1 + ((t.X()-ds.X())/2), t.y1 + ((t.Y()-ds.Y())/2), &t);

		x += Width + ((Current == i) ? 2 : 1);
		i++;
	}

	// Draw children
	//LWindow::OnPaint(pDC);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

void CtrlTabs::ToTab()
{
	CtrlTab *Cur = Tabs.ItemAt(Current);
	if (Cur)
	{
		// move all our children into the tab losing focus
		auto CurIt = Cur->IterateViews();
		auto ThisIt = IterateViews();
		for (auto v: CurIt)
		{
			Cur->DelView(v);
		}
		for (auto v: ThisIt)
		{
			DelView(v);
			Cur->AddView(v);
		}
	}
}

void CtrlTabs::FromTab()
{
	CtrlTab *Cur = Tabs.ItemAt(Current);
	if (Cur)
	{
		// load all our children from the new tab
		auto CurIt = Cur->IterateViews();
		auto ThisIt = IterateViews();
		for (auto v: ThisIt)
			DelView(v);

		for (auto v: CurIt)
		{
			Cur->DelView(v);
			AddView(v);
		}
	}
}

void CtrlTabs::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		if (Title.Overlap(m.x, m.y))
		{
			// select current tab
			int i = 0;
			for (auto Tab: Tabs)
			{
				if (Tab->View()->GetPos().Overlap(m.x, m.y) /* && i != Current*/)
				{
					ToTab();
					Current = i;
					FromTab();

					Dlg->OnSelect(Tab);
					Invalidate();

					break;
				}
				i++;
			}
		}

		if (m.IsContextMenu() && Title.Overlap(m.x, m.y))
		{
			auto RClick = new LSubMenu;
			if (RClick)
			{
				bool HasTab = Tabs.ItemAt(Current);

				RClick->AppendItem("New tab", IDM_NEW, true);
				RClick->AppendItem("Delete tab", IDM_DELETE, HasTab);
				RClick->AppendItem("Rename tab", IDM_RENAME, HasTab);
				RClick->AppendItem("Move tab left", IDM_MOVE_LEFT, HasTab);
				RClick->AppendItem("Move tab right", IDM_MOVE_RIGHT, HasTab);
				RClick->AppendSeparator();

				RClick->AppendItem("Copy Text", IDM_COPY_TEXT, Dlg->Selection.Length()==1);
				RClick->AppendItem("Paste Text", IDM_PASTE_TEXT, true);

				if (GetMouse(m, true))
				{
					switch (RClick->Float(this, m.x, m.y, false))
					{
						case IDM_NEW:
						{
							CtrlTab *t = new CtrlTab(Dlg, 0);
							if (t)
							{
								char Text[256];
								snprintf(Text, sizeof(Text), "Tab " LPrintfSizeT, Tabs.Length()+1);
								t->GetStr()->Set(Text);
								t->SetParent(this);

								Tabs.Insert(t);
							}
							break;
						}
						case IDM_DELETE:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								ToTab();
								Tabs.Delete(t);
								DeleteObj(t);
								FromTab();
							}
							break;
						}
						case IDM_RENAME:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								if (!t->GetStr())
									t->SetStr(Dlg->CreateSymbol());

								auto Input = new LInput(this, t->GetStr()->Get(), "Enter tab name:", "Rename");
								Input->SetParent(Dlg);
								Input->DoModal([t, Input](auto dlg, auto id)
								{
									if (id)
										t->GetStr()->Set(Input->GetStr());
									delete dlg;
								});
							}
							break;
						}
						case IDM_MOVE_LEFT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t && Current > 0)
							{
								Tabs.Delete(t);
								Tabs.Insert(t, --Current);
							}
							break;
						}
						case IDM_MOVE_RIGHT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t && Current < Tabs.Length()-1)
							{
								Tabs.Delete(t);
								Tabs.Insert(t, ++Current);
							}
							break;
						}
						case IDM_COPY_TEXT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								t->CopyText();
							}
							break;
						}
						case IDM_PASTE_TEXT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								t->PasteText();
							}
							break;
						}
					}

					Invalidate();
				}
			}

			return;
		}
	}

	ResDialogCtrl::OnMouseClick(m);
}

// List column

ListCol::ListCol(ResDialog *dlg, LXmlTag *load, char *s, int Width) :
	ResDialogCtrl(dlg, Res_Column, load)
{
	if (s && GetStr())
	{
		GetStr()->Set(s);
	}
	
	LRect r(0, 0, Width-1, 18);
	ResDialogCtrl::SetPos(r);
}

IMPL_DIALOG_CTRL(ListCol)
void ListCol::OnPaint(LSurface *pDC)
{
}

// List control

CtrlList::CtrlList(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_ListView, load)
{
	DragCol = -1;
}

CtrlList::~CtrlList()
{
	Empty();
}

void CtrlList::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	if (Deep)
	{
		for (auto w: Cols)
		{
			ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
			if (c)
			{
				l.Insert(c);
				c->ListChildren(l);
			}
		}
	}
}

void CtrlList::Empty()
{
	for (auto c: Cols)
	{
		DeleteObj(c);
	}
	Cols.Empty();
}

void CtrlList::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		if (Title.Overlap(m.x, m.y))
		{
			int x=0;
			ListCol *c = 0;
			ssize_t DragOver = -1;
			for (auto Col: Cols)
			{
				if (m.x >= Col->r().x1 && m.x <= Col->r().x2)
				{
					if (m.x > Col->r().x2 - 6)
					{
						DragOver = Cols.IndexOf(Col);
					}
					if (m.x < Col->r().x1 + 6)
					{
						DragOver = Cols.IndexOf(Col) - 1;
					}
					c = Col;
					break;
				}
				x += Col->r().X();
			}

			if (m.Left())
			{
				if (DragOver >= 0)
				{
					DragCol = DragOver;
					Capture(true);
				}
			}
			else if (m.IsContextMenu())
			{
				auto RClick = new LSubMenu;
				if (RClick)
				{
					bool HasCol = c != 0;
					RClick->AppendItem("New column", IDM_NEW, true);
					RClick->AppendItem("Delete column", IDM_DELETE, HasCol);
					RClick->AppendItem("Rename column", IDM_RENAME, HasCol);
					RClick->AppendItem("Move column left", IDM_MOVE_LEFT, HasCol);
					RClick->AppendItem("Move column right", IDM_MOVE_RIGHT, HasCol);
					RClick->AppendSeparator();

					RClick->AppendItem("Copy Text", IDM_COPY_TEXT, HasCol);
					RClick->AppendItem("Paste Text", IDM_PASTE_TEXT, HasCol);

					if (GetMouse(m, true))
					{
						switch (RClick->Float(this, m.x, m.y, false))
						{
							case IDM_COPY_TEXT:
							{
								if (c && c->GetStr())
								{
									c->GetStr()->CopyText();
								}
								break;
							}
							case IDM_PASTE_TEXT:
							{
								if (c && c->GetStr())
								{
									c->GetStr()->PasteText();
								}
								break;
							}
							case IDM_NEW:
							{
								ListCol *c = dynamic_cast<ListCol*>(Dlg->CreateCtrl(UI_COLUMN,0));
								if (c)
								{
									char Text[256];
									snprintf(Text, sizeof(Text), "Col " LPrintfSizeT, Cols.Length()+1);
									c->GetStr()->Set(Text);
									Cols.Insert(c);
								}
								break;
							}
							case IDM_DELETE:
							{
								Cols.Delete(c);
								DeleteObj(c);
								break;
							}
							case IDM_RENAME:
							{
								if (c)
								{
									auto Input = new LInput(this, c->GetStr()->Get(), "Enter column name:", "Rename");
									Input->SetParent(Dlg);
									Input->DoModal([Input, c](auto dlg, auto id)
									{
										if (id)
											c->GetStr()->Set(Input->GetStr());
										delete dlg;
									});
								}
								break;
							}
							case IDM_MOVE_LEFT:
							{
								auto Current = Cols.IndexOf(c);
								if (c && Current > 0)
								{
									Cols.Delete(c);
									Cols.Insert(c, --Current);
								}
								break;
							}
							case IDM_MOVE_RIGHT:
							{
								auto Current = Cols.IndexOf(c);
								if (c && Current < Cols.Length()-1)
								{
									Cols.Delete(c);
									Cols.Insert(c, ++Current);
								}
								break;
							}
						}

						Invalidate();
					}

					DeleteObj(RClick);
					return;
				}
			}

			return;
		}
	}
	else
	{
		if (DragCol >= 0)
		{
			Capture(false);
			DragCol = -1;
		}
	}

	ResDialogCtrl::OnMouseClick(m);
}

void CtrlList::OnMouseMove(LMouse &m)
{
	if (DragCol >= 0)
	{
		int i=0, x=0;;
		for (auto Col: Cols)
		{
			if (i == DragCol)
			{
				int Dx = (m.x - x - Title.x1);
				
				LRect r = Col->GetPos();
				r.x2 = r.x1 + Dx;
				Col->ResDialogCtrl::SetPos(r);
				break;
			}

			x += Col->r().X();
			i++;
		}

		Invalidate();
	}

	ResDialogCtrl::OnMouseMove(m);
}

void CtrlList::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);

	// Draw the ctrl
	LWideBorder(pDC, r, DefaultSunkenEdge);
	Title = r;
	Title.y2 = Title.y1 + 15;
	Client = r;
	Client.y1 = Title.y2 + 1;

	pDC->Colour(L_WORKSPACE);
	pDC->Rectangle(&Client);

	int x = Title.x1;
	for (auto c: Cols)
	{
		int Width = c->r().X();
		c->r().Set(x, Title.y1, x + Width - 1, Title.y2);
		LRect r = c->r();
		r.x2 = MIN(r.x2, Title.x2);
		x = r.x2 + 1;
		if (r.Valid())
		{
			LWideBorder(pDC, r, DefaultRaisedEdge);

			LSysFont->Fore(L_TEXT);
			LSysFont->Back(L_MED);
			LSysFont->Transparent(false);

			const char *Str = c->GetStr()->Get();
			if (!Str) Str = "";
			LDisplayString ds(LSysFont, Str);
			ds.Draw(pDC, r.x1 + 2, r.y1 + ((r.Y()-ds.Y())/2) - 1, &r);
		}
	}

	LRect Client(x, Title.y1, Title.x2, Title.y2);
	if (Client.Valid())
	{
		LWideBorder(pDC, Client, DefaultRaisedEdge);
		pDC->Colour(L_MED);;
		pDC->Rectangle(&Client);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Combo box
CtrlComboBox::CtrlComboBox(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_ComboBox, load)
{
}

IMPL_DIALOG_CTRL(CtrlComboBox)

void CtrlComboBox::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	Client = r;
	
	// Draw the ctrl
	LWideBorder(pDC, r, DefaultSunkenEdge);
	
	// Allocate space
	LRect e = r;
	e.x2 -= 15;
	LRect d = r;
	d.x1 = e.x2 + 1;

	// Draw edit
	pDC->Colour(L_WORKSPACE);
	pDC->Rectangle(&e);

	// Draw drap down
	LWideBorder(pDC, d, DefaultRaisedEdge);
	pDC->Colour(L_MED);
	pDC->Rectangle(&d);

	pDC->Colour(0, 24);
	int Size = 4;
	int cx = (d.X()/2) + d.x1 - 4;
	int cy = (d.Y()/2) + d.y1 - 3;
	for (int i=1; i<=Size; i++)
	{
		pDC->Line(cx+i, cy+i, cx+(Size*2)-i, cy+i);
	}

	// Text
	char *Text = GetStr()->Get();
	LSysFont->Fore(L_TEXT);
	LSysFont->Transparent(true);
	if (Text)
	{
		LDisplayString ds(LSysFont, Text);
		ds.Draw(pDC, 4, 4);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlScrollBar::CtrlScrollBar(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_ScrollBar, load)
{
}

IMPL_DIALOG_CTRL(CtrlScrollBar)

void CtrlScrollBar::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	Client = r;
	
	// Draw the ctrl
	bool Vertical = r.Y() > r.X();
	int ButSize = Vertical ? r.X() : r.Y();
	LRect a, b, c;
	if (Vertical)
	{
		a.Set(r.x1, r.y1, r.x2, r.y1 + ButSize);
		c.Set(r.x1, r.y2 - ButSize, r.x2, r.y2);
		b.Set(r.x1, a.y2 + 1, r.x2, c.y1 - 1);
	}
	else
	{
		a.Set(r.x1, r.y1, r.x1 + ButSize, r.y2);
		c.Set(r.x2 - ButSize, r.y1, r.x2, r.y2);
		b.Set(a.x2 + 1, r.y1, c.x1 - 1, r.y2);
	}

	// Buttons
	LWideBorder(pDC, a, DefaultRaisedEdge);
	LWideBorder(pDC, c, DefaultRaisedEdge);
	pDC->Colour(L_MED);
	pDC->Rectangle(&a);
	pDC->Rectangle(&c);

	// Arrows
	pDC->Colour(0);
	int x = a.x1 + (a.X()/2) - 2;
	int y = a.y1 + (a.Y()/2);
	int i;
	for (i=0; i<5; i++)
	{
		pDC->Line(x+i, y-i, x+i, y+i);
	}
	x = c.x1 + (c.X()/2) - 2;
	y = c.y1 + (c.Y()/2);
	for (i=0; i<5; i++)
	{
		pDC->Line(x+i, y-(4-i), x+i, y+(4-i));
	}

	// Slider
	pDC->Colour(0xd0d0d0, 24);
	pDC->Rectangle(&b);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlTree::CtrlTree(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_TreeView, load)
{
}

IMPL_DIALOG_CTRL(CtrlTree)

void CtrlTree::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LWideBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(Rgb24(255, 255, 255), 24);
	pDC->Rectangle(&r);
	LSysFont->Colour(L_TEXT, L_WORKSPACE);
	LSysFont->Transparent(true);
	LDisplayString ds(LSysFont, "Tree");
	ds.Draw(pDC, r.x1 + 3, r.y1 + 3, &r);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlBitmap::CtrlBitmap(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Bitmap, load)
{
}

IMPL_DIALOG_CTRL(CtrlBitmap)

void CtrlBitmap::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LWideBorder(pDC, r, DefaultSunkenEdge);
	pDC->Colour(Rgb24(255, 255, 255), 24);
	pDC->Rectangle(&r);
	pDC->Colour(0, 24);
	pDC->Line(r.x1, r.y1, r.x2, r.y2);
	pDC->Line(r.x2, r.y1, r.x1, r.y2);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlProgress::CtrlProgress(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Progress, load)
{
}

IMPL_DIALOG_CTRL(CtrlProgress)

void CtrlProgress::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LWideBorder(pDC, r, DefaultSunkenEdge);

	COLOUR Flat = Rgb24(0x7f, 0x7f, 0x7f);
	COLOUR White = Rgb24(0xff, 0xff, 0xff);
	int Pos = 60;
	int x = Pos * r.X() / 100;

	pDC->Colour(Flat, 24);
	pDC->Rectangle(r.x1, r.y1, r.x1 + x, r.y2);
	
	pDC->Colour(White, 24);
	pDC->Rectangle(r.x1 + x + 1, r.y1, r.x2, r.y2);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlCustom::CtrlCustom(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_Custom, load)
{
	Control = 0;
}

IMPL_DIALOG_CTRL(CtrlCustom)

CtrlCustom::~CtrlCustom()
{
	DeleteArray(Control);
}

void CtrlCustom::OnPaint(LSurface *pDC)
{
	LRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LWideBorder(pDC, r, DefaultSunkenEdge);

	COLOUR White = Rgb24(0xff, 0xff, 0xff);

	pDC->Colour(White, 24);
	pDC->Rectangle(&r);

	char s[256] = "Custom: ";
	if (Control)
	{
		strcat(s, Control);
	}

	LSysFont->Colour(L_TEXT, L_WORKSPACE);
	LDisplayString ds(LSysFont, s);
	ds.Draw(pDC, r.x1+2, r.y1+1, &r);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

#define VAL_Control "Ctrl"
bool CtrlCustom::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	if (Status)
	{
		Fields.Insert(this, DATA_STR, 320, VAL_Control, "Control", 1);
	}
	return Status;
}

bool CtrlCustom::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	if (Status)
	{
		Fields.Serialize(this, VAL_Control, Control);
	}
	return Status;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
ResStringGroup *ResDialog::Symbols = 0;
int ResDialog::SymbolRefs = 0;
bool ResDialog::CreateSymbols = true;

void ResDialog::AddLanguage(const char *Id)
{
	if (Symbols)
	{
		Symbols->AppendLanguage(Id);
	}
}

void ResDialogCtrl::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	Ctrls.Insert(this);

	for (LViewI *c: View()->IterateViews())
	{
		ResDialogCtrl *dc = dynamic_cast<ResDialogCtrl*>(c);
		LAssert(dc);
		dc->EnumCtrls(Ctrls);
	}
}

void ResDialog::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	for (auto ci: Children)
	{
		ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(ci);
		if (c)
			c->EnumCtrls(Ctrls);
	}
}

int GooberCaps[] = {
	RESIZE_X1 | RESIZE_Y1,
	RESIZE_Y1,
	RESIZE_X2 | RESIZE_Y1,
	RESIZE_X2,
	RESIZE_X2 | RESIZE_Y2,
	RESIZE_Y2,
	RESIZE_X1 | RESIZE_Y2,
	RESIZE_X1};

ResDialog::ResDialog(AppWnd *w, int type) : 
	Resource(w, type)
{
	// Maintain a string group just for our dialog
	// defines 
	if (!Symbols && w)
	{
		// First check to see of the symbols have been loaded from a file
		// and we don't have a pointer to it yet
		Symbols = App()->GetDialogSymbols();
		if (!Symbols)
		{
			// else we need to create the symbols object
			Symbols = new ResStringGroup(w);
			if (Symbols)
			{
				Symbols->Name(StrDialogSymbols);
				w->InsertObject(TYPE_STRING, Symbols);
			}
		}

		if (Symbols)
		{
			Symbols->SystemObject(true);
		}
	}
	SymbolRefs++;

	// Init dialog resource
	Ui = 0;
	DlgPos.ZOff(500-1, 400-1);

	DragGoober = -1;
	DragX = 0;
	DragY = 0;
	DragCtrl = 0;
}

ResDialog::~ResDialog()
{
	// Decrement and delete the shared string group
	SymbolRefs--;
	if (SymbolRefs < 1)
	{
		App()->DelObject(Symbols);
		Symbols = 0;
	}

	// Delete our Ui
	DeleteObj(Ui);
}

void ResDialog::OnShowLanguages()
{
	// Current language changed.
	OnSelect(Selection[0]);
	Invalidate();
	OnLanguageChange();
}

void ResDialog::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	printf("ResDialog::OnChildrenChanged %p, %i\n", Wnd, Attaching);
}

const char *ResDialog::Name()
{
	LViewI *v = Children[0];
	
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(v);
	if (!Ctrl)
	{
		static char msg[256];
		sprintf_s(msg, sizeof(msg), "#no_ctrl=%p,children=%i", v, (int)Children.Length());
		return msg;
	}
	if (!Ctrl->GetStr())
		return "#no_str";
	if (!Ctrl->GetStr()->GetDefine())
		return "#no_defined";
	
	return Ctrl->GetStr()->GetDefine();
}

bool ResDialog::Name(const char *n)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children[0]);
	if (Ctrl && Ctrl->GetStr())
	{
		Ctrl->GetStr()->SetDefine((n)?n:"");
		return Ctrl->GetStr()->GetDefine() != 0;
	}
	return false;
}

char *ResDialog::StringFromRef(int Ref)
{
	if (Symbols)
	{
		ResString *Str = Symbols->FindRef(Ref);
		if (Str)
		{
			return Str->Get();
		}
	}

	return 0;
}

bool ResDialog::Res_GetProperties(ResObject *Obj, LDom *Props)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Obj);
	if (Ctrl && Props)
	{
		int Next = -1000;
		FieldTree t(Next, false);
		t.SetStore(Props);
		t.SetMode(FieldTree::ObjToStore);
		Ctrl->GetFields(t);
		Ctrl->Serialize(t);

		return true;
	}
	return false;
}

LDom *ResDialog::Res_GetDom(ResObject *Obj)
{
	return dynamic_cast<LDom*>(Obj);
}

bool ResDialog::Res_SetProperties(ResObject *Obj, LDom *Props)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Obj);
	if (Ctrl && Props)
	{
		int Next = -1000;
		FieldTree t(Next, false);
		t.SetStore(Props);
		t.SetMode(FieldTree::StoreToObj);
		Ctrl->GetFields(t);
		Ctrl->Serialize(t);

		return true;
	}
	return false;
}

ResObject *ResDialog::CreateObject(LXmlTag *Tag, ResObject *Parent)
{
	return dynamic_cast<ResObject*>(CreateCtrl(Tag));
}

void ResDialog::Res_SetPos(ResObject *Obj, int x1, int y1, int x2, int y2)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			LRect r(x1, y1, x2, y2);
			Ctrl->SetPos(r);
		}
	}
}

void ResDialog::Res_SetPos(ResObject *Obj, char *s)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			Ctrl->ReadPos(s);
		}
	}
}

LRect ResDialog::Res_GetPos(ResObject *Obj)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		LAssert(Ctrl);
		if (Ctrl)
		{
			return Ctrl->View()->GetPos();
		}
	}
	return LRect(0, 0, 0, 0);
}

int ResDialog::Res_GetStrRef(ResObject *Obj)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			return Ctrl->GetStr()->GetRef();
		}
	}
	return -1;
}

bool ResDialog::Res_SetStrRef(ResObject *Obj, int Ref, ResReadCtx *Ctx)
{
	ResDialogCtrl *Ctrl = 0;
	if (!Obj || !Symbols)
		return false;

	Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
	if (!Ctrl)
		return false;

	Ctrl->StrFromRef(Ref);
	LAssert(Ctrl && Ctrl->GetStr());
	return Ctrl->GetStr() != 0;
}

void ResDialog::Res_Attach(ResObject *Obj, ResObject *Parent)
{
	if (Obj && Parent)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		ResDialogCtrl *Par = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Parent);
		if (Ctrl && Par)
		{
			Par->AttachCtrl(Ctrl);
		}
	}
}

bool ResDialog::Res_GetChildren(ResObject *Obj, List<ResObject> *l, bool Deep)
{
	bool Status = false;
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			Status = true;

			List<ResDialogCtrl> Child;
			Ctrl->ListChildren(Child, Deep);
			for (auto o: Child)
			{
				l->Insert(o);
			}
		}
	}

	return Status;
}

void ResDialog::Res_Append(ResObject *Obj, ResObject *Parent)
{
	if (Obj && Parent)
	{
		CtrlTabs *Tabs = dynamic_cast<CtrlTabs*>(Obj);
		CtrlTab *Tab = dynamic_cast<CtrlTab*>(Parent);
		if (Tabs && Tab)
		{
			Tab->SetParent(Tabs);
			Tabs->Tabs.Insert(Tab);

			if (Tabs->Tabs.Length() == 1)
			{
				Tabs->FromTab();
			}
			return;
		}

		CtrlList *Lst = dynamic_cast<CtrlList*>(Obj);
		ListCol *Col = dynamic_cast<ListCol*>(Parent);
		if (Lst && Col)
		{
			Lst->Cols.Insert(Col);
			return;
		}
	}
}

bool ResDialog::Res_GetItems(ResObject *Obj, List<ResObject> *l)
{
	if (Obj && l)
	{
		CtrlTabs *Tabs = dynamic_cast<CtrlTabs*>(Obj);
		if (Tabs)
		{
			for (auto Tab: Tabs->Tabs)
			{
				l->Insert(Tab);
			}

			return true;
		}

		CtrlList *Lst = dynamic_cast<CtrlList*>(Obj);
		if (Lst)
		{
			for (auto Col: Lst->Cols)
			{
				l->Insert(Col);
			}

			return true;
		}
	}

	return false;
}

void ResDialog::Create(LXmlTag *load, SerialiseContext *Ctx)
{
	CtrlDlg *Dlg = new CtrlDlg(this, load);
	if (Dlg)
	{
		LRect r = DlgPos;
		r.Offset(GOOBER_BORDER, GOOBER_BORDER);
		Children.Insert(Dlg);
		Dlg->SetParent(this);

		if (load)
		{
			if (Ctx)
				Read(load, *Ctx);
			else
				LAssert(0);
		}
		else
		{
			Dlg->ResDialogCtrl::SetPos(r);
			if (Dlg->GetStr())
				Dlg->GetStr()->Set("Dialog");
		}
	}
}

void ResDialog::Delete()
{
	// Deselect the dialog ctrl
	OnDeselect(dynamic_cast<ResDialogCtrl*>(Children[0]));

	// Delete selected controls
	ResDialogCtrl *c;
	while ((c = Selection[0]))
	{
		c->View()->Detach();
		DeleteObj(c);
	}

	// Repaint
	LView::Invalidate();
}

bool IsChild(ResDialogCtrl *Parent, ResDialogCtrl *Child)
{
	if (Parent && Child)
	{
		for (	ResDialogCtrl *c = Child->ParentCtrl();
				c;
				c = c->ParentCtrl())
		{
			if (c == Parent)
			{
				return true;
			}
		}
	}

	return false;
}

void GetTopCtrls(List<ResDialogCtrl> &Top, List<ResDialogCtrl> &Selection)
{
	// all children will automatically be cut as well

	for (auto c: Selection)
	{
		// is c a child of an item already in Top?
		bool Ignore = false;
		for (auto p: Top)
		{
			if (IsChild(p, c))
			{
				Ignore = true;
				break;
			}
		}

		if (!Ignore)
		{
			// is not a child
			Top.Insert(c);
		}
	}
}

void ResDialog::Copy(bool Delete)
{
	bool Status = false;

	// Deselect the dialog... can't cut that
	OnDeselect(dynamic_cast<ResDialogCtrl*>(Children[0]));
	
	// Get top level list
	List<ResDialogCtrl> Top;
	GetTopCtrls(Top, Selection);

	// ok we have a top level list of ctrls
	// write them to the file
	List<ResDialogCtrl> All;
	LXmlTag *Root = new LXmlTag("Resources");
	if (Root)
	{
		if (Delete)
		{
			// remove selection from UI
			AppWindow->OnObjSelect(0);
		}

		// write the string resources first
		for (auto c: Top)
		{
			All.Insert(c);
			c->ListChildren(All, true);
		}

		// write the strings out at the top of the block
		// so that we can reference them from the objects
		// below.
		SerialiseContext Ctx;
		for (auto c: All)
		{
			// Write the string out
			LXmlTag *t = new LXmlTag;
			if (t && c->GetStr()->Write(t, Ctx))
			{
				Root->InsertTag(t);
			}
			else
			{
				DeleteObj(t);
			}
		}

		// write the objects themselves
		for (auto c: Top)
		{
			LXmlTag *t = new LXmlTag;
			if (t && Res_Write(c, t))
			{
				Root->InsertTag(t);
			}
			else
			{
				DeleteObj(t);
			}
		}


		// Read the file in and copy to the clipboard
		LStringPipe Xml;
		LXmlTree Tree;
		if (Tree.Write(Root, &Xml))
		{
			char *s = Xml.NewStr();
			{
				LClipBoard Clip(Ui);

				char16 *w = Utf8ToWide(s);
				Clip.TextW(w);
				Status = Clip.Text(s, false);
				DeleteObj(w);
			}

			DeleteArray(s);
		}

		DeleteObj(Root);


		if (Delete && Status)
		{
			// delete them
			ResDialogCtrl *c;
			while (	Selection.Length() &&
					(c = Selection[0]))
			{
				c->View()->Detach();
				Selection.Delete(c);
				DeleteObj(c);
			}
		}

		// Repaint
		LView::Invalidate();
	}
}

class StringId
{
public:
	ResString *Str;
	int OldRef;
	int NewRef;
};

void RemapAllRefs(LXmlTag *t, List<StringId> &Strs)
{
	char *RefStr;
	if ((RefStr = t->GetAttr("Ref")))
	{
		int r = atoi(RefStr);

		for (auto i: Strs)
		{
			if (i->OldRef == r)
			{
				// this string ref is to be remapped
				char Buf[32];
				snprintf(Buf, sizeof(Buf), "%i", i->NewRef);
				t->SetAttr("Ref", Buf);

				// delete the string ref map
				// it's not needed anymore
				Strs.Delete(i);
				DeleteObj(i);
				
				// leave the loop
				RefStr = 0;
				break;
			}
		}

		if (RefStr)
		{
			// if this assert failes then no map for this
			// string was found. every incomming string needs
			// to be remapped
			LAssert(0);
		}
	}

	for (auto c: t->Children)
	{
		RemapAllRefs(c, Strs);
	}
}

void ResDialog::Paste()
{
	// Get the clipboard data
	char *Mem = 0;
	char *Data = 0;
	{
		LClipBoard Clip(Ui);
		char16 *w = Clip.TextW();
		if (w)
			Data = Mem = WideToUtf8(w);
		else
			Data = Clip.Text();
	}
	if (Data)
	{
		ResDialogCtrl *Container = 0;

		// Find a container to plonk the controls in
		ResDialogCtrl *c = Selection[0];
		if (c && c->IsContainer())
		{
			Container = c;
		}

		if (!Container)
		{
			// Otherwise just use the dialog as the container
			Container = dynamic_cast<ResDialogCtrl*>(Children[0]);
		}

		if (Container)
		{
			// remap list
			List<StringId> Strings;
			int NextRef = 0;

			// Deselect everything
			OnSelect(NULL);

			// Parse the data
			List<ResString> NewStrs;
			LXmlTree Tree;
			LStringPipe p;
			p.Push(Data);
			
			// Create the new controls, strings first
			// that way we can setup the remapping properly to avoid
			// string ref duplicates
			LXmlTag Root;
			if (Tree.Read(&Root, &p, 0))
			{
				for (auto t: Root.Children)
				{
					if (t->IsTag("string"))
					{
						// string tag
						LAssert(Symbols);

						ResString *Str = Symbols->CreateStr();
						SerialiseContext Ctx;
						if (Str && Str->Read(t, Ctx))
						{
							// setup remap object, so that we can make all the strings
							// unique
							StringId *Id = new StringId;
							LAssert(Id);
							Id->Str = Str;
							Id->OldRef = Str->GetRef();
							NextRef = Str->SetRef(Id->NewRef = AppWindow->GetUniqueStrRef(NextRef + 1));
							Strings.Insert(Id);

							// insert out new string
							NewStrs.Insert(Str);
						}
						else
						{
							break;
						}
					}
					else
					{
						// object tag

						// all strings should have been processed by the time 
						// we get in here. We remap the incomming string refs to 
						// unique values so that we don't get conflicts later.
						RemapAllRefs(t, Strings);
					}
				}

				// load all the objects now
				List<ResDialogCtrl> NewCtrls;
				for (auto t: Root.Children)
				{
					if (!t->IsTag("string"))
					{
						// object (not string)
						CreateSymbols = false;
						ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(CreateCtrl(t));
						CreateSymbols = true;
						ResReadCtx Ctx;
						if (Ctrl && Res_Read(Ctrl, t, Ctx))
						{
							NewCtrls.Insert(Ctrl);
						}
						else
						{
							break;
						}
					}
				}

				// calculate the new control set's dimensions so we
				// can paste them in at the top of the container
				auto It = NewCtrls.begin();
				ResDialogCtrl *c = *It;
				if (c)
				{
					LRect All = c->View()->GetPos();
					while ((c = *(++It)))
					{
						All.Union(&c->View()->GetPos());
					}

					// now paste in the controls
					for (auto c: NewCtrls)
					{
						LRect *Preference = Container->GetPasteArea();
						LRect p = c->View()->GetPos();
						p.Offset(-All.x1, -All.y1);

						p.Offset(Preference ? Preference->x1 : Container->Client.x1 + GRID_X,
								 Preference ? Preference->y1 : Container->Client.y1 + GRID_Y);
						c->SetPos(p);
						Container->AttachCtrl(c, &c->View()->GetPos());
						OnSelect(c, false);
					}

					// reset parent size to fit
					LRect cp = Container->View()->GetPos();
					Container->SetPos(cp, true);
				}

				// Deduplicate all these new strings
				for (auto s: NewStrs)
				{
					s->UnDuplicate();
				}
			}

			// Repaint
			LView::Invalidate();
		}
	}
	DeleteArray(Mem);
}

void ResDialog::SnapPoint(LPoint *p, ResDialogCtrl *From)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children[0]);
	if (p && Ctrl)
	{
		int Ox = 0; // -Ctrl->Client.x1;
		int Oy = 0; // -Ctrl->Client.y1;
		LView *Parent = dynamic_cast<LView*>(Ctrl);
		if (From)
		{
			for (LViewI *w = From->View(); w && w != Parent; w = w->GetParent())
			{
				Ox += w->GetPos().x1;
				Oy += w->GetPos().y1;
			}
		}

		p->x += Ox;
		p->y += Oy;

		p->x = ((p->x + (GRID_X / 2)) / GRID_X * GRID_X);
		p->y = ((p->y + (GRID_X / 2)) / GRID_X * GRID_X);

		p->x -= Ox;
		p->y -= Oy;
	}
}

void ResDialog::SnapRect(LRect *r, ResDialogCtrl *From)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children[0]);
	if (r && Ctrl)
	{
		int Ox = 0; // -Ctrl->Client.x1;
		int Oy = 0; // -Ctrl->Client.y1;
		LView *Parent = dynamic_cast<LView*>(Ctrl);
		for (LViewI *w = From->View(); w && w != Parent; w = w->GetParent())
		{
			Ox += w->GetPos().x1;
			Oy += w->GetPos().y1;
		}

		r->Normal();
		r->Offset(Ox, Oy);

		r->x1 = ((r->x1 + (GRID_X / 2)) / GRID_X * GRID_X);
		r->y1 = ((r->y1 + (GRID_X / 2)) / GRID_X * GRID_X);
		r->x2 = ((r->x2 + (GRID_X / 2)) / GRID_X * GRID_X) - 1;
		r->y2 = ((r->y2 + (GRID_X / 2)) / GRID_X * GRID_X) - 1;

		r->Offset(-Ox, -Oy);
	}
}

bool ResDialog::IsDraging()
{
	return DragGoober >= 0;
}

int ResDialog::CurrentTool()
{
	return (Ui) ? Ui->CurrentTool() : 0;
}

void ResDialog::MoveSelection(int Dx, int Dy)
{
	auto It = Selection.begin();
	ResDialogCtrl *w = *It;
	if (!w)
		return;

	ResDialogCtrl *Parent = w->ParentCtrl();
	if (!Parent)
		return;

	// find dimensions of group
	LRect All = w->View()->GetPos();
	for (; w; w = *(++It))
		All.Union(&w->View()->GetPos());

	// limit the move to the top-left corner of the parent's client
	LRect ParentClient = Parent->Client;
	if (dynamic_cast<CtrlDlg*>(Parent))
		ParentClient.ZOff(-ParentClient.x1, -ParentClient.y1);

	if (All.x1 + Dx < ParentClient.x1)
		Dx = ParentClient.x1 - All.x1;
	if (All.y1 + Dy < ParentClient.y1)
		Dy = ParentClient.y1 - All.y1;

	// move the ctrls
	LRegion Update;
	for (auto w: Selection)
	{
		LRect Old = w->View()->GetPos();
		LRect New = Old;
		New.Offset(Dx, Dy);

		// optionally limit the move to the containers bounds
		LRect *b = w->ParentCtrl()->GetChildArea(w);
		if (b)
		{
			if (New.x1 < b->x1)
				New.Offset(b->x1 - New.x1, 0);
			else if (New.x2 > b->x2)
				New.Offset(b->x2 - New.x2, 0);
			if (New.y1 < b->y1)
				New.Offset(0, b->y1 - New.y1);
			else if (New.y2 > b->y2)
				New.Offset(0, b->y2 - New.y2);
		}

		LRect Up = w->AbsPos();
		Up.Inset(-GOOBER_BORDER, -GOOBER_BORDER);
		Update.Union(&Up);

		w->SetPos(New, false);

		Up = w->AbsPos();
		Up.Inset(-GOOBER_BORDER, -GOOBER_BORDER);
		Update.Union(&Up);
	}

	Invalidate(&Update);
}

void ResDialog::SelectCtrl(ResDialogCtrl *c)
{
	Selection.Empty();
	if (c)
	{
		bool IsMine = false;
		
		for (LViewI *p = c->View(); p; p = p->GetParent())
		{
			if ((LViewI*)this == p)
			{
				IsMine = true;
				break;
			}
		}
		
		if (IsMine)
		{
			Selection.Insert(c);
			
			// int TabIdx = -1;
			ResDialogCtrl *Prev = 0;
			for (LViewI *p = c->View()->GetParent(); p; p = p->GetParent())
			{
				ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(p);
				if (c)
				{
					c->ShowMe(Prev);
					Prev = c;
				}
				else break;
			}
		}
		else LgiTrace("%s:%i - Ctrl doesn't belong to me.\n", __FILE__, __LINE__);
	}
	else LgiTrace("Selecting '0'\n");
	
	Invalidate();
	if (AppWindow)
		AppWindow->OnObjSelect(c);
}

void ResDialog::SelectNone()
{
	Selection.Empty();

	Invalidate();
	if (AppWindow)
	{
		AppWindow->OnObjSelect(0);
	}
}

void ResDialog::SelectRect(ResDialogCtrl *Parent, LRect *r, bool ClearPrev)
{
	if (ClearPrev)
	{
		Selection.Empty();
	}

	if (Parent && r)
	{
		for (LViewI *c: Parent->View()->IterateViews())
		{
			if (c->GetPos().Overlap(r))
			{
				ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(c);
				if (Ctrl)
				{
					if (Selection.IndexOf(Ctrl) >= 0)
					{
						Selection.Delete(Ctrl);
					}
					else
					{
						Selection.Insert(Ctrl);
					}
				}
			}
		}
	}

	Invalidate();

	if (AppWindow)
	{
		AppWindow->OnObjSelect((Selection.Length() == 1) ? Selection[0] : 0);
	}
}

bool ResDialog::IsSelected(ResDialogCtrl *Ctrl)
{
	return Selection.IndexOf(Ctrl) >= 0;
}

void ResDialog::OnSelect(ResDialogCtrl *Wnd, bool ClearPrev)
{
	if (ClearPrev)
	{
		Selection.Empty();
	}

	if (Wnd)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Wnd);
		if (Ctrl)
		{
			if (!Selection.HasItem(Ctrl))
			{
				Selection.Insert(Ctrl);
			}

			if (Ui)
			{
				Ui->SelectTool(UI_CURSOR);
			}

			Invalidate();
		}

		if (AppWindow)
		{
			AppWindow->OnObjSelect((Selection.Length() == 1) ? Selection[0] : 0);
		}
	}
}

void ResDialog::OnDeselect(ResDialogCtrl *Wnd)
{
	if (Selection.HasItem(Wnd))
	{
		Selection.Delete(Wnd);

		AppWindow->OnObjSelect(0);
	}
}

ResDialogCtrl *ResDialog::CreateCtrl(LXmlTag *t)
{
	if (t)
	{
		for (LgiObjectName *o = NameMap; o->Type; o++)
		{
			if (t->IsTag(o->ResourceName))
			{
				return CreateCtrl(o->Type, t);
			}
		}
	}

	return 0;
}

ResDialogCtrl *ResDialog::CreateCtrl(int Tool, LXmlTag *load)
{
	ResDialogCtrl *Ctrl = 0;

	switch (Tool)
	{
		case UI_DIALOG:
		{
			Ctrl = new CtrlDlg(this, load);
			break;
		}
		case UI_TABLE:
		{
			Ctrl = new CtrlTable(this, load);
			break;
		}
		case UI_TEXT:
		{
			Ctrl = new CtrlText(this, load);
			if (Ctrl && Ctrl->GetStr() && !load)
			{
				Ctrl->GetStr()->Set("Some text");
			}
			break;
		}
		case UI_EDITBOX:
		{
			Ctrl = new CtrlEditbox(this, load);
			break;
		}
		case UI_CHECKBOX:
		{
			Ctrl = new CtrlCheckbox(this, load);
			if (Ctrl && Ctrl->GetStr() && !load)
			{
				Ctrl->GetStr()->Set("Checkbox");
			}
			break;
		}
		case UI_BUTTON:
		{
			Ctrl = new CtrlButton(this, load);
			if (Ctrl && Ctrl->GetStr() && !load)
			{
				static int i = 1;
				char Text[256];
				snprintf(Text, sizeof(Text), "Button %i", i++);
				Ctrl->GetStr()->Set(Text);
			}
			break;
		}
		case UI_GROUP:
		{
			Ctrl = new CtrlGroup(this, load);
			if (Ctrl && Ctrl->GetStr() && !load)
			{
				Ctrl->GetStr()->Set("Text");
			}
			break;
		}
		case UI_RADIO:
		{
			Ctrl = new CtrlRadio(this, load);
			if (Ctrl && Ctrl->GetStr() && !load)
			{
				Ctrl->GetStr()->Set("Radio");
			}
			break;
		}
		case UI_TAB:
		{
			Ctrl = new CtrlTab(this, load);
			break;
		}
		case UI_TABS:
		{
			Ctrl = new CtrlTabs(this, load);
			break;
		}
		case UI_LIST:
		{
			Ctrl = new CtrlList(this, load);
			break;
		}
		case UI_COLUMN:
		{
			Ctrl = new ListCol(this, load);
			break;
		}
		case UI_COMBO:
		{
			Ctrl = new CtrlComboBox(this, load);
			break;
		}
		case UI_TREE:
		{
			Ctrl = new CtrlTree(this, load);
			break;
		}
		case UI_BITMAP:
		{
			Ctrl = new CtrlBitmap(this, load);
			break;
		}
		case UI_SCROLL_BAR:
		{
			Ctrl = new CtrlScrollBar(this, load);
			break;
		}
		case UI_PROGRESS:
		{
			Ctrl = new CtrlProgress(this, load);
			break;
		}
		case UI_CUSTOM:
		{
			Ctrl = new CtrlCustom(this, load);
			break;
		}
		case UI_CONTROL_TREE:
		{
			Ctrl = new CtrlControlTree(this, load);
			break;
		}
		default:
		{
			LAssert(!"No control factory handler.");
			break;
		}
	}

	if (Ctrl && Ctrl->GetStr())
	{
		Ctrl->GetStr()->UpdateWnd = Ctrl->View();
	}

	return Ctrl;
}

LView *ResDialog::CreateUI()
{
	return Ui = new ResDialogUi(this);
}

void ResDialog::DrawSelection(LSurface *pDC)
{
    if (Selection.Length() == 0)
        return;

	// Draw selection
	for (auto Ctrl: Selection)
	{
		LRect r = Ctrl->AbsPos();
		LColour s(255, 0, 0);
		LColour c = GetParent()->Focus() ? s : s.Mix(LColour(L_MED), 0.4);
		DrawGoobers(pDC, r, Ctrl->Goobers, c, Ctrl->OverGoober);
	}
}

#ifdef WINDOWS
#define USE_MEM_DC		1
#else
#define USE_MEM_DC		0
#endif

void ResDialog::_Paint(LSurface *pDC, LPoint *Offset, LRect *Update)
{
	// Create temp DC if needed...
	LAutoPtr<LSurface> Local;
	if (!pDC)
	{
		if (!Local.Reset(new LScreenDC(this))) return;
		pDC = Local;
	}

	#if USE_MEM_DC
	LDoubleBuffer DblBuf(pDC);
	#endif
	
	LView::_Paint(pDC, Offset, Update);

	if (GetParent())
	{
		#ifndef WIN32
		LRect p = GetPos();
		if (Offset) p.Offset(Offset);
		pDC->SetClient(&p);
		#endif
		
		DrawSelection(pDC);
		#ifndef WIN32
		pDC->SetClient(NULL);
		#endif

		if (DebugOverlay)
		{
			pDC->Op(GDC_ALPHA);
			pDC->Blt(0, 0, DebugOverlay);
		}
	}
}

void ResDialog::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_WORKSPACE);
	pDC->Rectangle();
	
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children[0]);
	if (!Ctrl)
		return;
}

void ResDialog::OnLanguageChange()
{
	if (Ui && Ui->StatusInfo)
	{
		LLanguage *l = Symbols->GetLanguage(App()->GetCurLang()->Id);
		if (l)
		{
			char Str[256];
			snprintf(Str, sizeof(Str), "Current Language: %s (Id: %s)", l->Name, l->Id);
			Ui->StatusInfo->Name(Str);
		}
	}
}

bool ResDialog::OnKey(LKey &k)
{
	if (k.Ctrl())
	{
		switch (k.c16)
		{
			case LK_UP:
			{
				if (k.Down())
				{
					int Idx = Symbols->GetLangIdx(App()->GetCurLang()->Id);
					if (Idx > 0)
					{
						LLanguage *l = Symbols->GetLanguage(Idx - 1);
						if (l)
						{
							App()->SetCurLang(l);
						}
					}
					Invalidate();
					OnLanguageChange();
				}
				return true;
				break;
			}
			case LK_DOWN:
			{
				if (k.Down())
				{
					int Idx = Symbols->GetLangIdx(App()->GetCurLang()->Id);
					if (Idx < Symbols->GetLanguages() - 1)
					{
						LLanguage *l = Symbols->GetLanguage(Idx + 1);
						if (l)
						{
							App()->SetCurLang(l);
						}
					}
					Invalidate();
					OnLanguageChange();
				}
				return true;
				break;
			}
		}
	}

	return false;
}

void ResDialog::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{	
		if (GetParent()) GetParent()->Focus(true);

		if (m.Left())
		{
			DragGoober = -1;
			for (auto c: Selection)
			{
				for (int i=0; i<8; i++)
				{
					if (c->Goobers[i].Overlap(m.x, m.y))
					{
						DragGoober = i;
						DragCtrl = c;
						
						// LgiTrace("IN goober[%i]=%s %i,%i\n", i, c->Goobers[i].GetStr(), m.x, m.y);
						break;
					}
					else
					{
						// LgiTrace("goober[%i]=%s %i,%i\n", i, c->Goobers[i].GetStr(), m.x, m.y);
					}
				}
			}

			if (DragGoober >= 0)
			{
				DragRgn = DragCtrl->View()->GetPos();
				DragX = DragY = 0;
				int Cap = GooberCaps[DragGoober];
				
				// Lock the dialog to the top left corner
				if (dynamic_cast<CtrlDlg*>(DragCtrl))
				{
					Cap &= ~(RESIZE_X1|RESIZE_Y1);
				}
				
				if (!Cap)
				{
					// Can't move the object anyway so abort the drag
					DragGoober = -1;
				}
				else
				{
					// Allow movement along the appropriate edge
					if (Cap & RESIZE_X1)
					{
						DragX = &DragRgn.x1;
					}
					if (Cap & RESIZE_Y1)
					{
						DragY = &DragRgn.y1;
					}
					if (Cap & RESIZE_X2)
					{
						DragX = &DragRgn.x2;
					}
					if (Cap & RESIZE_Y2)
					{
						DragY = &DragRgn.y2;
					}

					// Remember the offset to the mouse from the egdes
					if (DragX) DragOx = m.x - *DragX;
					if (DragY) DragOy = m.y - *DragY;

					Capture(true);
				}
			}
		}
	}
	else
	{
		if (DragGoober >= 0)
		{
			Capture(false);
			DragGoober = -1;
			DragX = 0;
			DragY = 0;
		}
	}
}

void ResDialog::OnMouseMove(LMouse &m)
{
	// This code hilights the goober when the mouse is over it.
	for (auto c: Selection)
	{
		int Old = c->OverGoober;
		c->OverGoober = -1;
		for (int i=0; i<8; i++)
		{
			if (c->Goobers[i].Overlap(m.x, m.y))
			{
				c->OverGoober = i;
				break;
			}
		}
		if (c->OverGoober != Old)
			Invalidate();
	}

	if (DragGoober >= 0)
	{
		if (DragX)
			*DragX = m.x - DragOx;
		if (DragY)
			*DragY = m.y - DragOy;

		// DragRgn in real coords
		LRect Old = DragCtrl->View()->GetPos();
		LRect New = DragRgn;
		
		auto It = IterateViews();
		if (DragCtrl->View() != It[0])
			SnapRect(&New, DragCtrl->ParentCtrl());

		// New now in snapped coords
		
		// If that means the dragging control changes then
		if (New != Old)
		{
			LRegion Update;

			// change everyone else by the same amount
			for (auto c: Selection)
			{
				LRect OldPos = c->View()->GetPos();
				LRect NewPos = OldPos;

				NewPos.x1 += New.x1 - Old.x1;
				NewPos.y1 += New.y1 - Old.y1;
				NewPos.x2 += New.x2 - Old.x2;
				NewPos.y2 += New.y2 - Old.y2;

				LRect Up = c->AbsPos();
				Up.Inset(-GOOBER_BORDER, -GOOBER_BORDER);
				Update.Union(&Up);
				
				c->SetPos(NewPos);

				Up = c->AbsPos();
				Up.Inset(-GOOBER_BORDER, -GOOBER_BORDER);
				Update.Union(&Up);
			}

			Invalidate(&Update);
		}
	}
}

bool ResDialog::Test(ErrorCollection *e)
{
	return true;
}

bool ResDialog::Read(LXmlTag *t, SerialiseContext &Ctx)
{
	bool Status = false;

	// turn symbol creation off so that ctrls find their
	// symbol via Id matching instead of creating their own
	CreateSymbols = false;

	ResDialogCtrl *Dlg = dynamic_cast<ResDialogCtrl*>(Children[0]);
	if (Dlg)
	{
		// Load the resource
		ResReadCtx Ctx;
		Status = Res_Read(Dlg, t, Ctx);
		Item->Update();
	}

	CreateSymbols = true;
	return Status;
}

void ResDialog::CleanSymbols()
{
	// removed unreferenced dialog strings
	if (Symbols)
	{
		Symbols->RemoveUnReferenced();
	}

	// re-ID duplicate entries
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children[0]);
	if (Ctrl)
	{
		// list all the entries
		List<ResDialogCtrl> l;
		Ctrl->ListChildren(l);
		l.Insert(Ctrl); // insert the dialog too

		// remove duplicate string entries
		for (auto c: l)
		{
			LAssert(c->GetStr());
			c->GetStr()->UnDuplicate();
		}
	}

	// sort list (cause I need to read the file myself)
	if (Symbols)
	{
		Symbols->Sort();
	}
}

bool ResDialog::Write(LXmlTag *t, SerialiseContext &Ctx)
{
	bool Status = false;
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children[0]);
	if (Ctrl)
	{
		// duplicates symbols should have been removed before the 
		// strings were written out
		// so we don't have to do it here.

		// write the resources out
		if (Ctrl)
		{
			Status = Res_Write(Ctrl, t);
		}
	}
	else LgiTrace("%s:%i - Not a ResDialogCtrl.\n", _FL);

	return Status;
}

void ResDialog::OnRightClick(LSubMenu *RClick)
{
	if (RClick)
	{
		if (Enabled())
		{
			RClick->AppendSeparator();
			if (Type() > 0)
			{
				RClick->AppendItem("Dump to C++", IDM_DUMP, true);

				auto Export = RClick->AppendSub("Export to...");
				if (Export)
				{
					Export->AppendItem("Lgi File", IDM_EXPORT, true);
					Export->AppendItem("Win32 Resource Script", IDM_EXPORT_WIN32, false);
				}
			}
		}
	}
}

const char *TypeOfRes(ResDialogCtrl *Ctrl)
{

	// the default
	return "LWindow";
}

const char *TextOfCtrl(ResDialogCtrl *Ctrl)
{
	static char Buf[256];

	switch (Ctrl->GetType())
	{
		// Has text
		case UI_TEXT:
		case UI_EDITBOX:
		case UI_CHECKBOX:
		case UI_BUTTON:
		case UI_GROUP:
		case UI_RADIO:
		case UI_COMBO:
		{
			char *s = Ctrl->GetStr()->Get();
			snprintf(Buf, sizeof(Buf), ", \"%s\"", s?s:"");
			return Buf;
		}

		// Not processed
		case UI_COLUMN:
		case UI_TAB:
		{
			LAssert(0);
			break;
		}

		// No text...
		case UI_BITMAP:
		case UI_PROGRESS:
		case UI_TABS:
		case UI_LIST:
		case UI_TREE:
		case UI_SCROLL_BAR:
		{
			break;
		}
	}

	return "";
}

void OutputCtrl(LStringPipe &Def,
				LStringPipe &Var,
				LStringPipe &Inst,
				ResDialogCtrl *Ctrl,
				ResDialogCtrl *Parent,
				int &Index)
{
	char Str[256];
	const char *Type = "LView";

	for (LgiObjectName *on=NameMap; on->Type; on++)
	{
		if (Ctrl->GetType() == on->Type)
		{
			Type = on->ObjectName;
			break;
		}
	}

	if (stricmp(Type, "LDialog"))
	{
		if (ValidStr(Ctrl->GetStr()->GetDefine()) &&
			stricmp(Ctrl->GetStr()->GetDefine(), "IDOK") &&
			stricmp(Ctrl->GetStr()->GetDefine(), "IDCANCEL") &&
			stricmp(Ctrl->GetStr()->GetDefine(), "IDC_STATIC"))
		{
			char Tab[8];
			auto Tabs = (32 - strlen(Ctrl->GetStr()->GetDefine()) - 1) / 4;
			memset(Tab, '\t', Tabs);
			Tab[Tabs] = 0;

			snprintf(Str, sizeof(Str), "#define %s%s%i\n", Ctrl->GetStr()->GetDefine(), Tab, 1000 + Index);
			Def.Push(Str);
		}

		snprintf(Str, sizeof(Str),
				"\t%s *Ctrl%i;\n",
				Type,
				Index);
		Var.Push(Str);

		snprintf(Str, sizeof(Str),
				"\tChildren.Insert(Ctrl%i = new %s(%s, %i, %i, %i, %i%s));\n",
				Index,
				Type,
				Ctrl->GetStr()->GetDefine(),
				Ctrl->View()->GetPos().x1 - 3,
				Ctrl->View()->GetPos().y1 - 17,
				Ctrl->View()->X(),
				Ctrl->View()->Y(),
				TextOfCtrl(Ctrl));
		Inst.Push(Str);

		CtrlList *List = dynamic_cast<CtrlList*>(Ctrl);
		if (List)
		{
			// output columns
			for (auto c: List->Cols)
			{
				snprintf(Str, sizeof(Str), "\tCtrl%i->AddColumn(\"%s\", %i);\n", Index, c->GetStr()->Get(), c->X());
				Inst.Push(Str);
			}
		}

		Index++;
	}

	for (auto c: Ctrl->View()->IterateViews())
		OutputCtrl(Def, Var, Inst, dynamic_cast<ResDialogCtrl*>(c), Ctrl, Index);
}

void ResDialog::OnCommand(int Cmd)
{
	switch (Cmd)
	{
		case IDM_DUMP:
		{
			LStringPipe Def, Var, Inst;
			LStringPipe Buf;
			char Str[256];

			ResDialogCtrl *Dlg = dynamic_cast<ResDialogCtrl*>(Children[0]);
			if (Dlg)
			{
				// List controls
				int i=0;
				OutputCtrl(Def, Var, Inst, Dlg, 0, i);

				// #define's
				char *Defs = Def.NewStr();
				if (Defs)
				{
					Buf.Push(Defs);
					Def.Empty();
				}

				// Class def
				Buf.Push(	"\nclass Dlg : public LDialog\n"
							"{\n");

				// Variables
				char *Vars = Var.NewStr();
				if (Vars)
				{
					Buf.Push(Vars);
					Var.Empty();
				}

				// Member functions
				Buf.Push(	"\n"
							"public:\n"
							"\tDlg(LView *Parent);\n"
							"\t~Dlg();\n"
							"\n"
							"\tint OnNotify(LViewI *Ctrl, int Flags);\n"
							"};\n"
							"\n");

				// Class impl
				Buf.Push(	"Dlg::Dlg(LView *Parent)\n"
							"{\n"
							"\tSetParent(Parent);\n");

				snprintf(Str, sizeof(Str),
						"\tName(\"%s\");\n"
						"\tGRegion r(0, 0, %i, %i);\n"
						"\tSetPos(r);\n",
						Dlg->GetStr()->Get(),
						Dlg->View()->X(),
						Dlg->View()->Y());
				Buf.Push(Str);
				Buf.Push("\tMoveToCenter();\n\n");

				// Ctrl instancing
				char *NewCtrls = Inst.NewStr();
				if (NewCtrls)
				{
					Buf.Push(NewCtrls);
					Inst.Empty();
				}

				Buf.Push(	"}\n"
							"\n");

				// Destructor
				Buf.Push(	"Dlg::~Dlg()\n"
							"{\n"
							"}\n"
							"\n");

				// ::OnNotify
				Buf.Push(	"int Dlg::OnNotify(LViewI *Ctrl, int Flags)\n"
							"{\n"
							"\tswitch (Ctrl->GetId())\n"
							"\t{\n"
							"\t\tcase IDOK:\n"
							"\t\t{\n"
							"\t\t\t// Do something here\n"
							"\t\t\t// fall thru\n"
							"\t\t}\n"
							"\t\tcase IDCANCEL:\n"
							"\t\t{\n"
							"\t\t\tEndModal(Ctrl->GetId());\n"
							"\t\t\tbreak;\n"
							"\t\t}\n"
							"\t}\n"
							"\n"
							"\treturn 0;\n"
							"}\n");


				// Output to clipboard
				char *Text = Buf.NewStr();
				if (Text)
				{
					LClipBoard Clip(Ui);
					Clip.Text(Text);
					DeleteArray(Text);
				}
			}
			break;
		}
		case IDM_EXPORT:
		{
			auto Select = new LFileSelect(AppWindow);
			Select->Type("Text", "*.txt");
			Select->Save([&](auto dlg, auto status)
			{
				if (status)
				{
					LFile F;
					if (F.Open(Select->Name(), O_WRITE))
					{
						F.SetSize(0);
						// Serialize(F, true);
					}
					else
					{
						LgiMsg(AppWindow, "Couldn't open file for writing.");
					}
				}
				delete dlg;
			});
			break;
		}
		case IDM_EXPORT_WIN32:
		{
			break;
		}
	}
}

int ResDialog::OnCommand(int Cmd, int Event, OsView hWnd)
{
	switch (Cmd)
	{
		/*
		case IDM_SET_LANG:
		{
			Symbols->SetCurrent();
			OnSelect(Selection.First());

			Invalidate();
			OnLanguageChange();
			break;
		}
		*/
		case IDM_TAB_ORDER:
		{
			ResDialogCtrl *Top = 0;
			if (Selection.Length() == 1 &&
				Selection[0]->IsContainer())
			{
				Top = Selection[0];
			}
			if (!Top)
			{
				Top = dynamic_cast<ResDialogCtrl*>(Children[0]);
			}
			if (Top)
			{
				auto Dlg = new TabOrder(this, Top);
				Dlg->DoModal([](auto dlg, auto id)
				{
					delete dlg;
				});
			}
			break;
		}
	}

	return 0;
}

ResString *ResDialog::CreateSymbol()
{
	return (Symbols) ? Symbols->CreateStr() : 0;
}

////////////////////////////////////////////////////////////////////
ResDialogUi::ResDialogUi(ResDialog *Res)
{
	Dialog = Res;
	Tools = 0;
	Status = 0;
	StatusInfo = 0;
	Name("ResDialogUi");

	if (Res)
		Res->OnSelect(Res->Selection[0]);
}

ResDialogUi::~ResDialogUi()
{
	if (Dialog)
		Dialog->Ui = NULL;
}

void ResDialogUi::OnPaint(LSurface *pDC)
{
	LRegion Client(0, 0, X()-1, Y()-1);
	for (auto w: Children)
	{
		LRect r = w->GetPos();
		Client.Subtract(&r);
	}

	pDC->Colour(L_MED);
	for (LRect *r = Client.First(); r; r = Client.Next())
	{
		pDC->Rectangle(r);
	}
}

void ResDialogUi::PourAll()
{
	LRegion Client(GetClient());
	LRegion Update;

	for (auto v: Children)
	{
		LRect OldPos = v->GetPos();
		Update.Union(&OldPos);

		if (v->Pour(Client))
		{
			if (!v->Visible())
			{
				v->Visible(true);
			}

			if (OldPos != v->GetPos())
			{
				// position has changed update...
				v->Invalidate();
			}

			Client.Subtract(&v->GetPos());
			Update.Subtract(&v->GetPos());
		}
		else
		{
			// make the view not visible
			v->Visible(false);
		}
	}

	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
}

void ResDialogUi::OnPosChange()
{
	PourAll();
}

void ResDialogUi::OnCreate()
{
	Tools = new LToolBar;
	if (Tools)
	{
		auto FileName = LFindFile("_DialogIcons.gif");
		if (FileName && Tools->SetBitmap(FileName, 16, 16))
		{
			Tools->Attach(this);

			Tools->AppendButton("Cursor", 0, TBT_RADIO);

			for (LgiObjectName *o=NameMap; o->Type; o++)
			{
				if (o->ToolbarBtn)
				{
					Tools->AppendButton(o->ObjectName, o->Type, TBT_RADIO);
				}
			}
			Tools->AppendSeparator();

			// Tools->AppendButton("Change language", IDM_SET_LANG, TBT_PUSH);
			Tools->AppendButton("Tab Order", IDM_TAB_ORDER, TBT_PUSH, true, 17);
		}
		else
		{
			DeleteObj(Tools);
		}
	}

	Status = new LStatusBar;
	if (Status)
	{
		Status->Attach(this);
		StatusInfo = Status->AppendPane("", 2);
	}

	ResFrame *Frame = new ResFrame(Dialog);
	if (Frame)
	{
		Frame->Attach(this);
	}
	
	PourAll();
}

int ResDialogUi::CurrentTool()
{
	if (Tools)
	{
		auto It = Tools->IterateViews();
		for (size_t i=0; i<It.Length(); i++)
		{
			LToolButton *But = dynamic_cast<LToolButton*>(It[i]);
			if (But && But->Value())
				return But->GetId();
		}
	}

	return -1;
}

void ResDialogUi::SelectTool(int i)
{
	if (Tools)
	{
		auto It = Tools->IterateViews();
		LViewI *w = It[i];
		if (w)
		{
			LToolButton *But = dynamic_cast<LToolButton*>(w);
			if (But)
				But->Value(true);
		}
	}
}

LMessage::Result ResDialogUi::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_COMMAND:
		{
			Dialog->OnCommand(Msg->A()&0xffff, (int)(Msg->A()>>16), (OsView) Msg->B());
			break;
		}
		case M_DESCRIBE:
		{
			char *Text = (char*) Msg->B();
			if (Text)
			{
				StatusInfo->Name(Text);
			}
			break;
		}
	}

	return LView::OnEvent(Msg);
}
