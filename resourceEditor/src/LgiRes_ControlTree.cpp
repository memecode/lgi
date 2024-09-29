#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_Dialog.h"
#include "lgi/common/Button.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Token.h"
#include "lgi/common/Menu.h"

#define IDM_NEW_CHILD		100
#define IDM_NEW_NEXT		101
#define VAL_ControlType		"ControlType"
#define VAL_ControlTag		"ControlTag"

class CtrlControlTreePriv
{
public:
	ResDialog *Dlg;
	bool DiscardClick;
	LLanguageId CurLang;

	CtrlControlTreePriv(ResDialog *dlg)
	{
		CurLang = 0;
		Dlg = dlg;
		DiscardClick = false;
	}
};

class CtNode :
	public LTreeItem,
	public ResourceView
{
	CtrlControlTreePriv *d;

public:
	ResString *Str = NULL;
	LAutoString Type;
	LAutoString Tag;

	void UpdateView()
	{
		Update();
	}

	CtNode(CtrlControlTreePriv *priv, int StringRef)
	{
		d = priv;

		if (d->Dlg)
		{
			if (d->Dlg && d->Dlg->Symbols)
			{
				if (StringRef)
					// Find the ref'd string in the string group...
					Str = d->Dlg->Symbols->FindRef(StringRef);
				else
					// We create a symbol for this resource
					Str = d->Dlg->Symbols->CreateStr();
			}

			if (Str)
				Str->AddView(this);
		}
	}

	const char *GetText(int i)
	{
		if (GetCss(true))
		{
			LCss::ColorDef c;
			if (!Str || !Str->Get())
				GetCss()->Color(LCss::ColorDef(LCss::ColorRgb, Rgb32(0xbb, 0xbb, 0xbb)));
			else
				GetCss()->DeleteProp(LCss::PropColor);
		}

		if (!Str)
			return "(no string)";

		if (!Str->Get())
			return "(empty string)";

		return Str->Get();
	}

	void Move(int Dir)
	{
		auto Cur = IndexOf();
		LTreeNode *p = GetParent();
		if (!p)
			p = GetTree();
		if (Dir < 0)
		{
			if (Cur == 0) return;
			Remove();
			p->Insert(this, Cur-1);
		}
		else
		{
			Remove();
			p->Insert(this, Cur+1);
		}
	}

	bool OnKey(LKey &k)
	{
		switch (k.vkey)
		{
			case LK_UP:
			{
				if (k.Alt())
				{
					if (k.Down())
						Move(-1);
					return true;
				}
				break;
			}
			case LK_DOWN:
			{
				if (k.Alt())
				{
					if (k.Down())
						Move(1);
					return true;
				}
				break;
			}
		}

		return false;
	}

	void OnMouseClick(LMouse &m)
	{
		if (m.IsContextMenu())
		{
			LSubMenu s;
			s.AppendItem("New Child", IDM_NEW_CHILD, true);
			s.AppendItem("New Next", IDM_NEW_NEXT, true);
			s.AppendItem("Delete", ID_DELETE, true);
			s.AppendSeparator();
			s.AppendItem("Copy Text", ID_COPY_TEXT, true);
			s.AppendItem("Paste Text", ID_PASTE_TEXT, true);
			s.AppendSeparator();
			s.AppendItem("Move Up", ID_UP, true);
			s.AppendItem("Move Down", ID_DOWN, true);

			m.ToScreen();

			int Cmd;
			if ((Cmd = s.Float(GetTree(), m.x, m.y)))
			{
				d->DiscardClick = true;

				switch (Cmd)
				{
					case ID_UP:
					{
						Move(-1);
						break;
					}
					case ID_DOWN:
					{
						Move(1);
						break;
					}
					case IDM_NEW_CHILD:
					{
						// Insert at the start... user can use "next" for positions other than the start.
						Insert(new CtNode(d, 0), 0);
						Expanded(true);
						break;
					}
					case IDM_NEW_NEXT:
					{
						auto Idx = IndexOf();
						if (GetParent())
						{
							if (Idx >= 0)
							{
								GetParent()->Insert(new CtNode(d, 0), Idx+1);
							}
						}
						else
						{
							if (Idx >= 0)
							{
								GetTree()->Insert(new CtNode(d, 0), Idx+1);
							}
						}
						break;
					}
					case ID_DELETE:
					{
						delete this;
						break;
					}
					case ID_COPY_TEXT:
					{
						if (Str)
							Str->CopyText();
						break;
					}
					case ID_PASTE_TEXT:
					{
						if (Str)
						{
							Str->PasteText();
							Update();
						}
						break;
					}
				}
			}
		}
	}
};

CtrlControlTree::CtrlControlTree(ResDialog *dlg, LXmlTag *load) :
	ResDialogCtrl(dlg, Res_ControlTree, load),
	LTree(100, 0, 0, 100, 100)
{
	d = new CtrlControlTreePriv(dlg);

	if (!load)
	{
		Insert(new CtNode(d, 0));
	}
}

CtrlControlTree::~CtrlControlTree()
{
	delete d;
}

void CtrlControlTree::OnMouseClick(LMouse &m)
{
	LTree::OnMouseClick(m);

	if (!d->DiscardClick)
	{
		ResDialogCtrl::OnMouseClick(m);
	}
	else d->DiscardClick = false;
}

void CtrlControlTree::OnMouseMove(LMouse &m)
{
	LTree::OnMouseMove(m);
	ResDialogCtrl::OnMouseMove(m);
}

void CtrlControlTree::OnPaint(LSurface *pDC)
{
	if (Dlg->App()->GetCurLang()->Id != d->CurLang)
	{
		d->CurLang = Dlg->App()->GetCurLang()->Id;
		UpdateAllItems();
	}

	LTree::OnPaint(pDC);
}

bool CtrlControlTree::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	if (Status && Fields.GetDeep())
	{
		CtNode *n = dynamic_cast<CtNode*>(Selection());
		if (n && n->Str)
		{
			Status |= n->Str->GetFields(Fields);
			Fields.Insert(n->Str, DATA_STR, -1, VAL_ControlType, "Type");
			Fields.Insert(n->Str, DATA_STR, -1, VAL_ControlTag, "OptionTag");
		}
	}
	return Status;
}

bool CtrlControlTree::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	if (Status && Fields.GetDeep())
	{
		CtNode *n = dynamic_cast<CtNode*>(Selection());
		if (n && n->Str)
		{
			Status |= n->Str->Serialize(Fields);
			Fields.Serialize(n->Str, VAL_ControlType, n->Type);
			Fields.Serialize(n->Str, VAL_ControlTag, n->Tag);
			n->Update();
		}
	}
	return Status;
}

void WriteTree(LXmlTag *t, LTreeNode *n)
{
	CtNode *ct = dynamic_cast<CtNode*>(n);
	if (ct)
	{
		t->SetAttr("Ref", ct->Str->GetRef());
		t->SetAttr(VAL_ControlType, ct->Type);
		t->SetAttr(VAL_ControlTag, ct->Tag);
	}

	for (LTreeNode *c = n->GetChild(); c; c = c->GetNext())
	{
		LXmlTag *h = new LXmlTag;
		WriteTree(h, c);
		h->SetTag("Control");
		t->InsertTag(h);
	}
}

bool CtrlControlTree::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (!Name)
		return false;

	if (!stricmp(Name, "Tree"))
	{
		LXmlTag *x = new LXmlTag;
		WriteTree(x, this);
		Value = x;
	}
	else return false;

	return true;
}

void ReadTree(LXmlTag *t, LTreeNode *n, CtrlControlTreePriv *d)
{
	CtNode *ct = dynamic_cast<CtNode*>(n);
	if (ct && ct->Str)
	{
		ct->Type.Reset(NewStr(t->GetAttr(VAL_ControlType)));
		ct->Tag.Reset(NewStr(t->GetAttr(VAL_ControlTag)));
	}

	for (auto c: t->Children)
	{
		int StrRef = c->GetAsInt("ref");
		CtNode *nw = new CtNode(d, StrRef);
		ReadTree(c, nw, d);
		n->Insert(nw);

		LTreeItem *it = dynamic_cast<LTreeItem*>(n);
		if (it) it->Expanded(true);
	}
}

bool CtrlControlTree::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (!Name)
		return false;

	if (!stricmp(Name, "Tree"))
	{
		if (Value.Type != GV_DOM)
			return false;

		Empty();

		LXmlTag *x = dynamic_cast<LXmlTag*>(Value.Value.Dom);
		if (!x)
			LAssert(!"Not the right object.");
		else
			ReadTree(x, this, d);
	}
	else return false;

	return true;
}

