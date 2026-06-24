#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/ControlTree.h"
#include "lgi/common/Edit.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/Combo.h"
#include "lgi/common/Button.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/FileSelect.h"

#define IDC_BROWSE -10

class LControlTreePriv
{
public:
    LResources *Factory = nullptr;
};

LControlTree::Item::Item(int ctrlId, char *Txt, const char *opt, LVariantType type, LArray<LControlTree::EnumValue> *pEnum)
{
	if (ValidStr(opt))
		Opt.Reset(NewStr(opt));

	CtrlId = ctrlId;
	Enum.Reset(pEnum);
	SetText(Txt);
	Type = type;
}

LControlTree::Item::~Item()
{
}

void LControlTree::Item::SetEnum(LAutoPtr<EnumArr> e)
{
	Enum = e;
	Type = GV_INT32;
}

void LControlTree::Item::OnVisible(bool v)
{
	if (Ctrl)
	{
		if (v)
		{
			PositionControls();
		}
		else
		{
			Ctrl->Visible(false);
			if (Browse)
				Browse->Visible(false);
		}
	}
}

LControlTree::Item *LControlTree::Item::Find(const char *opt)
{
	if (!Stricmp(Opt.Get(), opt))
		return this;

	for (LTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		if (auto *ci = dynamic_cast<LControlTree::Item*>(i))
		{
			if (auto f = ci->Find(opt))
				return f;
		}
	}

	return nullptr;
}

bool LControlTree::Item::Serialize(LDom *Store, bool Write)
{
	if (Opt)
	{
		if (Write)
		{
			Save();
			Store->SetValue(Opt, Value);
		}
		else
		{
			Store->GetValue(Opt, Value);
		}
	}

	for (LTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		LControlTree::Item *ci = dynamic_cast<LControlTree::Item*>(i);
		if (ci)
		{
			ci->Serialize(Store, Write);
		}
	}
	
	return true;
}

void LControlTree::Item::SetValue(LVariant &v)
{
	Value = v;
	if (LTreeItem::Select())
	{
		DeleteObj(Ctrl);
		DeleteObj(Browse);
		Select(true);
	}
	else Update();
}

LRect &LControlTree::Item::GetRect()
{
	static LRect r;
	r.ZOff(-1, -1);
	
	if (auto p = _GetRect(TreeItemText))
	{
		bool HasBrowse = (Flags & TYPE_FILE) != 0;
		int x = p->x2 + 5;
		r.x1 = x;
		r.x2 = GetTree()->GetPos().X() - (HasBrowse ? 50 : 10);
		int Cy = Ctrl ? Ctrl->Y() : 16;
		r.y1 = (p->y1 + (p->Y()/2)) - (Cy / 2);
		r.y2 = r.y1 + Cy - 1;
	}
	
	return r;
}

void LControlTree::Item::Save()
{
	if (Ctrl)
	{
		switch (Type)
		{
			case GV_STRING:
			{
				auto v = Ctrl->Name();
				if (Stricmp(v, Value.Str()))
				{
					Value = v;
					Tree->SendNotify(LNotifyValueChanged);
				}
				break;
			}
			case GV_BOOL:
			{
				bool b = Ctrl->Value() != 0;
				if (b != (Value.CastInt32() != 0))
				{
					Value = b;
					Tree->SendNotify(LNotifyValueChanged);
				}
				break;
			}
			default:
			{
				auto Idx = Ctrl->Value();
				if (Enum)
				{
					if (Idx >= 0 && Idx < Enum->Length())
					{
						auto &e = (*Enum)[Idx];
						if (e.Value.IsString())
						{
							if (Stricmp(Value.Str(), e.Value.Str()))
							{
								Value = e.Value;
								Tree->SendNotify(LNotifyValueChanged);
							}
						}
						else if (Idx != Value.CastInt32())
						{
							Value = Idx;
							printf("%s:%i - enum saved as index = %i\n", _FL, (int)Idx);
							Tree->SendNotify(LNotifyValueChanged);
						}
					}
				}
				else if (Idx != Value.CastInt32())
				{
					Value = Idx;
					Tree->SendNotify(LNotifyValueChanged);
				}
				break;
			}
		}
	}
}

void LControlTree::Item::Select(bool b)
{
	LTreeItem::Select(b);

	if ((Ctrl != nullptr) ^ b)
	{
		if (b)
		{
			LAssert(Ctrl == nullptr);
			int FontY = LSysFont->GetHeight();
			int CtrlY = FontY + (FontY >> 1);

			switch (Type)
			{
				default:
					break;
				case GV_STRING:
					if ((Ctrl = new LEdit(CtrlId)))
					{
						LRect r(0, 0, 200, CtrlY);
						Ctrl->SetPos(r);
						Ctrl->Name(Value.Str());
					}
					if (Flags & TYPE_FILE)
						Browse = new LButton(IDC_BROWSE, 0, 0, -1, CtrlY, "...");
					break;
				case GV_BOOL:
					if ((Ctrl = new LCheckBox(CtrlId)))
						Ctrl->Value(Value.CastInt32());
					break;
				case GV_INT32:
				{
					LRect r(0, 0, 60, CtrlY);
					if (Enum)
					{
						LCombo *Cbo;
						if ((Ctrl = (Cbo = new LCombo(CtrlId))))
						{
							int Idx = -1;

							for (unsigned i=0; i<Enum->Length(); i++)
							{
								auto &e = (*Enum)[i];
								Cbo->Insert(e.Name);
								if (e.Value == Value)
									Idx = i;
							}

							if (Idx >= 0)
								Ctrl->Value(Idx);
							Ctrl->SetPos(r);
						}
					}
					else if ((Ctrl = new LEdit(CtrlId)))
					{
						Ctrl->SetPos(r);
						Ctrl->Value(Value.CastInt32());
					}
					break;
				}
			}

			if (Ctrl)
			{
				LColour Ws(L_WORKSPACE);
				Ctrl->SetColour(Ws, false);
				Ctrl->Attach(GetTree());

				if (Browse)
				{
					Browse->SetColour(Ws, false);
					Browse->Attach(GetTree());
				}

				PositionControls();
			}
		}
		else if (Ctrl)
		{
			Save();
			DeleteObj(Ctrl);
			DeleteObj(Browse);
		}

		Update();
	}
}

void LControlTree::Item::PositionControls()
{
	if (Ctrl)
	{
		LRect r = GetRect();
		Ctrl->SetPos(r);
		Ctrl->Visible(true);
		if (Browse)
		{
			LRect b = Browse->GetPos();
			b.Offset(r.x2 + 5 - b.x1, r.y1 - b.y1);
			Browse->SetPos(b);
			Browse->Visible(true);
		}
	}
}

void LControlTree::Item::OnPaint(ItemPaintCtx &Ctx)
{
	LTreeItem::OnPaint(Ctx);

	if (!Ctrl)
	{
		LCssTools Tools(GetTree());
		auto Ws = LColour(L_WORKSPACE);
		LSysBold->Colour(Tools.GetFore(), Tools.GetBack(&Ws, 0));
		LSysBold->Transparent(true);

		LRect p = GetRect();
		switch (Type)
		{
			default:
				break;
			case GV_INT32:
			{
				char s[32], *Disp = nullptr;
				LString errMsg;
				if (Enum)
				{
					if (Value.IsString())
					{
						bool found = false;
						for (unsigned i=0; i<Enum->Length(); i++)
						{
							auto &e = (*Enum)[i];
							
							#if 0
							LString s1 = e.Value.ToString();
							LString s2 = Value.ToString();
							LgiTrace("EnumMatch %s: %s - %s\n", e.Name, s1.Get(), s2.Get());
							#endif
							
							if (e.Value == Value)
							{
								Disp = e.Name;
								found = true;
								break;
							}
						}
						
						if (!found)
							errMsg.Printf("string value '%s' not valid", Value.Str());
					}
					else if (Value.IsInt())
					{
						auto i = Value.CastInt64();
						if (i >= 0 && i < Enum->Length())
						{
							auto &e = (*Enum)[i];
							Disp = e.Name;
						}
					}
					else if (!Value.IsNull())
						LAssert(!"unexpected value type");

					if (Disp)
					{
						LDisplayString ds(LSysBold, Disp);
						ds.Draw(Ctx.pDC, p.x1 + 8, p.y1 + 1);
					}
					else if (errMsg)
					{
						LDisplayString ds(LSysFont, errMsg);
						LSysFont->Colour(LColour::Red, LColour(L_WORKSPACE));
						ds.Draw(Ctx.pDC, p.x1 + 8, p.y1 + 1);
					}
					else
					{
						LDisplayString ds(LSysFont, LLoadString(L_CONTROLTREE_NO_VALUE, "(no value)"));
						LSysFont->Colour(LColour(L_LOW), LColour(L_WORKSPACE));
						ds.Draw(Ctx.pDC, p.x1 + 8, p.y1 + 1);
					}
				}
				else
				{
					sprintf_s(Disp = s, sizeof(s), "%i", Value.CastInt32());
					LDisplayString ds(LSysBold, Disp);
					ds.Draw(Ctx.pDC, p.x1 + 6, p.y1 + 2);
				}
				break;
			}
			case GV_STRING:
			{
				LDisplayString ds(LSysBold, Value.Str());
				ds.Draw(Ctx.pDC, p.x1 + 6, p.y1 + 2);
				break;
			}
			case GV_BOOL:
			{
				LDisplayString ds(LSysBold, (char*) (Value.CastInt32() ? "true" : "false"));
				ds.Draw(Ctx.pDC, p.x1 + 1, p.y1 + 1);
				break;
			}
		}
	}
	else
	{
		PositionControls();
	}
}

///////////////////////////////////////////////////////////////////////
LControlTree::LControlTree() : LTree(-1, 0, 0, 100, 100)
{
	SetObjectName(Res_ControlTree);
	d = new LControlTreePriv;
}

LControlTree::~LControlTree()
{
	DeleteObj(d);
}

LControlTree::Item *LControlTree::Find(const char *opt)
{
	for (LTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		LControlTree::Item *ci = dynamic_cast<LControlTree::Item*>(i);
		if (ci)
		{
			LControlTree::Item *f = ci->Find(opt);
			if (f)
				return f;
		}
	}

	return 0;
}

bool LControlTree::CallMethod(const char *MethodName, LScriptArguments &Args)
{
	switch (LStringToDomProp(MethodName))
	{
		case ControlSerialize:
		{
			if (Args.Length() != 2)
			{
				LAssert(!"Wrong argument count.");
				return false;
			}
			
			auto *Store = Args[0]->CastDom();
			if (!Store)
			{
				LAssert(!"Missing store object.");
				return false;
			}

			auto Write = Args[1]->CastInt32() != 0;
			*Args.GetReturn() = Serialize(Store, Write);
			return true;
		}
		default:
			break;
	}

	return false;
}

bool LControlTree::Serialize(LDom *Store, bool Write)
{
	bool Error = false;

	if (!Store)
	{
		LAssert(!"Invalid param.");
		return false;
	}

	for (LTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		LControlTree::Item *ci = dynamic_cast<LControlTree::Item*>(i);
		if (ci)
			Error = Error | ci->Serialize(Store, Write);
		else
			LAssert(!"Not a control tree item.");
	}

	return !Error;
}

LControlTree::Item *LControlTree::Resolve(bool Create, const char *Path, int CtrlId, LVariantType Type, LArray<EnumValue> *Enum)
{
	auto t  = LString(Path).SplitDelimit(".");
	if (t.Length() > 0)
	{
		LTreeNode *Cur = this;
		for (unsigned i=0; i<t.Length(); i++)
		{
			LTreeItem *Match = 0;
			for (LTreeItem *c = Cur->GetChild(); c; c = c->GetNext())
			{
				const char *s = c->GetText();
				if (s && _stricmp(t[i], s) == 0)
				{
					Match = c;
					break;
				}
			}

			if (Match)
			{
				Cur = Match;
			}
			else
			{
				if (Create && i == t.Length() - 1)
				{
					LControlTree::Item *Ci = new LControlTree::Item(CtrlId, t[i], Path, Type, Enum);
					if (Ci)
					{
						Cur->Insert(Ci);
						
						if (auto p = Ci->GetParent())
							p->Expanded(true);
						return Ci;
					}
				}

				return nullptr;
			}
		}

		return dynamic_cast<LControlTree::Item*>(Cur);
	}

	return nullptr;
}

LTreeItem *LControlTree::Insert(const char *DomPath, int CtrlId, LVariantType Type, LVariant *Value, LArray<EnumValue> *Enum)
{
	if (auto c = Resolve(true, DomPath, CtrlId, Type, Enum))
	{
		if (Value)
			c->SetValue(*Value);
	}

	return nullptr;
}

void LControlTree::ReadTree(LXmlTag *t, LTreeNode *n)
{
	for (auto c: t->Children)
	{
		int CtrlId = -1;
		int StrRef = c->GetAsInt("ref");
		LStringRes *Str = d->Factory->StrFromRef(StrRef);
		LAssert(Str != NULL);
		if (!Str)
			continue;
		
		CtrlId = Str->Id;		

		char *Type = c->GetAttr("ControlType");
		LVariantType iType = GV_NULL;
		int Flags = 0;
		if (Type)
		{
			if (!_stricmp(Type, "string"))
				iType = GV_STRING;
			else if (!_stricmp(Type, "file"))
			{
				iType = GV_STRING;
				Flags |= LControlTree::Item::TYPE_FILE;
			}
			else if (!_stricmp(Type, "bool"))
				iType = GV_BOOL;
			else if (!_stricmp(Type, "int") ||
					 !_stricmp(Type, "enum"))
				iType = GV_INT32;
		}

		const char *Opt = c->GetAttr("ControlTag");
		LControlTree::Item *ct = new LControlTree::Item(CtrlId,
														Str?Str->Str:(char*)"#error",
														ValidStr(Opt) ? Opt : NULL,
														iType,
														0);
		if (ct)
		{
			ct->Flags = Flags;
			n->Insert(ct);
			ReadTree(c, ct);
			ct->Expanded(true);
		}
	}
}

bool LControlTree::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	if (!Name)
		return false;

	if (!_stricmp(Name, "Tree"))
	{
		if (Value.Type != GV_DOM)
			return false;

		Empty();

		LXmlTag *x = dynamic_cast<LXmlTag*>(Value.Value.Dom);
		if (!x)
			LAssert(!"Not the right object.");
		else if (d->Factory)
			ReadTree(x, this);
	    else
	        LAssert(!"No factory.");
			
		d->Factory = 0;
	}
	else if (!_stricmp(Name, "LgiFactory"))
	{
	    d->Factory = dynamic_cast<LResources*>((ResFactory*)Value.CastVoidPtr());
	}
	else return false;

	return true;
}

int LControlTree::OnNotify(LViewI *c, const LNotification &n)
{
	switch (c->GetId())
	{
		case IDC_BROWSE:
		{
			if (auto i = dynamic_cast<Item*>(Selection()))
			{
				auto s = new LFileSelect;
				s->Parent(this);
				s->Open([i](auto dlg, auto status)
				{
					if (status)
					{
						LVariant v;
						i->SetValue(v = dlg->Name());
					}
				});
			}
			return 0;
		}
	}

	return LTree::OnNotify(c, n);
}

///////////////////////////////////////////////////////////////////////////////
class LControlTree_Factory : public LViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (!Stricmp(Class, "LControlTree"))
			return new LControlTree;

		return nullptr;
	}

}	ControlTree_Factory;
