/// \file
/// \author Matthew Allen

#include "lgi/common/Lgi.h"
#include "lgi/common/XmlTreeUi.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/Button.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/Slider.h"
#include "lgi/common/Combo.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Tree.h"
#include "lgi/common/ControlTree.h"

#include <ctype.h>

struct Mapping
{
	int Id;
	int Hint;
	LXmlTreeUi::CreateListItem ListItemFactory;
	LXmlTreeUi::CreateTreeItem TreeItemFactory;
	LVariant ChildElements;
	void *User;

	Mapping()
	{
		Id = 0;
		Hint = GV_NULL;
		ListItemFactory = 0;
		TreeItemFactory = 0;
		User = 0;
	}

	void LoadTree(GTreeNode *n, LXmlTag *t)
	{
		for (auto c: t->Children)
		{
			GTreeItem *i = TreeItemFactory(User);
			if (i)
			{
				i->XmlIo(c, false);
				n->Insert(i);

				LoadTree(i, c);
			}
		}
	}

	void SaveTree(GTreeNode *n, LXmlTag *t)
	{
		for (GTreeItem *i = n->GetChild(); i; i = i->GetNext())
		{
			LXmlTag *n = new LXmlTag(ChildElements.Str());
			if (n)
			{
				i->XmlIo(n, true);
				t->InsertTag(n);

				SaveTree(i, n);
			}
		}
	}
};

class LXmlTreeUiPriv
{
public:
	LHashTbl<ConstStrKey<char,false>,Mapping*> Maps;
	
	~LXmlTreeUiPriv()
	{
	    Maps.DeleteObjects();
	}
};

LXmlTreeUi::LXmlTreeUi()
{
	d = new LXmlTreeUiPriv;
}

LXmlTreeUi::~LXmlTreeUi()
{
	DeleteObj(d);
}

void LXmlTreeUi::EmptyAll(LViewI *Ui)
{
	if (Ui)
	{
		// for (Mapping *m=d->Maps.First(); m; m=d->Maps.Next())
		for (auto m : d->Maps)
		{
			if (m.value->Hint == GV_STRING)
				Ui->SetCtrlName(m.value->Id, 0);
		}
	}
}

void LXmlTreeUi::EnableAll(LViewI *Ui, bool Enable)
{
	if (Ui)
	{
		// for (Mapping *m=d->Maps.First(); m; m=d->Maps.Next())
		for (auto m : d->Maps)
		{
			Ui->SetCtrlEnabled(m.value->Id, Enable);
		}
	}
}

bool LXmlTreeUi::IsMapped(const char *Attr)
{
	return d->Maps.Find(Attr) != NULL;
}

void LXmlTreeUi::Map(const char *Attr, int UiIdent, int Type)
{
	if (UiIdent > 0 &&
		(Attr != 0 || Type == GV_DOM))
	{		
		Mapping *m = new Mapping;
		if (m)
		{
			m->Id = UiIdent;
			m->Hint = Type;
			
			LgiAssert(!d->Maps.Find(Attr));
			d->Maps.Add(Attr, m);
		}
	}
	else
	{
		LgiAssert(0);
	}
}

void LXmlTreeUi::Map(const char *Attr, int UiIdent, CreateListItem Factory, const char *ChildElements, void *User)
{
	if (Attr && UiIdent > 0 && Factory && ChildElements)
	{
		Mapping *m = new Mapping;
		if (m)
		{
			m->Id = UiIdent;
			m->ListItemFactory = Factory;
			m->Hint = GV_LIST;
			m->ChildElements = ChildElements;
			m->User = User;
			
			LgiAssert(!d->Maps.Find(Attr));
			d->Maps.Add(Attr, m);
		}
	}
	else
	{
		LgiAssert(0);
	}
}

void LXmlTreeUi::Map(const char *Attr, int UiIdent, CreateTreeItem Factory, const char *ChildElements, void *User)
{
	if (Attr && UiIdent > 0 && Factory && ChildElements)
	{
		Mapping *m = new Mapping;
		if (m)
		{
			m->Id = UiIdent;
			m->TreeItemFactory = Factory;
			m->Hint = GV_CUSTOM;
			m->ChildElements = ChildElements;
			m->User = User;

			LgiAssert(!d->Maps.Find(Attr));
			d->Maps.Add(Attr, m);
		}
	}
	else
	{
		LgiAssert(0);
	}
}

void LXmlTreeUi::EmptyMaps()
{
	d->Maps.DeleteObjects();
}

int GetCtrlType(LViewI *v)
{
	if (v)
	{
		if (dynamic_cast<GControlTree*>(v))
		{
			return GV_DOM;
		}
		else if (dynamic_cast<GCheckBox*>(v) ||
				dynamic_cast<GButton*>(v) ||
				dynamic_cast<GRadioButton*>(v))
		{
			return GV_BOOL;
		}
		else if (dynamic_cast<GSlider*>(v) ||
				dynamic_cast<GCombo*>(v) ||
				dynamic_cast<GRadioGroup*>(v))
		{
			return GV_INT32;
		}
	}

	return GV_STRING;
}

int GetDataType(char *str)
{
	if (str)
	{
		bool Float = false;

		char16 w;
		ssize_t Len = strlen(str);
		while ((w = LgiUtf8To32((uint8_t*&)str, Len)))
		{
			if (strchr("e \t\r\n", w))
			{
				// Doesn't really tell us anything, so ignore it.
				// The 'e' part is sometimes part of a number or
				// ignore that too.
			}
			else if (!IsDigit(w) || w > 255)
			{
				return GV_STRING;
			}
			else if (w == '.')
			{
				Float = true;
			}
		}

		if (Float)
		{
			return GV_DOUBLE;
		}

		return GV_INT32;
	}

	return GV_NULL;
}

bool LXmlTreeUi::Convert(GDom *Tag, LViewI *Ui, bool ToUI)
{
	bool Status = false;

	if (Ui && Tag)
	{
		LVariant v;
		LXmlTag *Xml = dynamic_cast<LXmlTag*>(Tag);
		if (ToUI)
		{
			// Xml -> UI
			// const char *Attr;
			// for (Mapping *Map = d->Maps.First(&Attr); Map; Map = d->Maps.Next(&Attr))
			for (auto Map : d->Maps)
			{
				if (Map.value->Hint == GV_LIST)
				{
					if (Xml)
					{
						LXmlTag *t = Xml->GetChildTag(Map.key);
						if (!t) continue;
						LList *Lst;
						if (!Ui->GetViewById(Map.value->Id, Lst)) continue;
						Lst->Empty();
						for (auto c: t->Children)
						{
							LListItem *i = Map.value->ListItemFactory(Map.value->User);
							if (i)
							{
								i->XmlIo(c, false);
								Lst->Insert(i);
							}
						}
					}
				}
				else if (Map.value->Hint == GV_CUSTOM)
				{
					if (Xml)
					{
						LXmlTag *t = Xml->GetChildTag(Map.key);
						if (!t) continue;

						GTree *Tree;
						if (!Ui->GetViewById(Map.value->Id, Tree)) continue;
						Tree->Empty();

						Map.value->LoadTree(Tree, t);
					}
				}
				else if (Map.value->Hint == GV_DOM)
				{
					GControlTree *ct;
					if (Ui->GetViewById(Map.value->Id, ct))
					{
						ct->Serialize(Xml, false);
					}
				}
				else if (Tag->GetValue(Map.key, v))
				{
					int Type = Map.value->Hint ? Map.value->Hint : GetDataType(v.Str());

					if (Type == GV_BOOL ||
						Type == GV_INT32 ||
						Type == GV_INT64)
					{
						Ui->SetCtrlValue(Map.value->Id, v.CastInt32());
					}
					else
					{
						Ui->SetCtrlName(Map.value->Id, v.Str());
					}
					Status = true;
				}
				else
				{
					GEdit *c;
					if (Ui->GetViewById(Map.value->Id, c))
						c->Name("");
				}
			}
		}
		else
		{
			// UI -> Xml
			// const char *Attr;
			// for (Mapping *Map = d->Maps.First(&Attr); Map; Map = d->Maps.Next(&Attr))
			for (auto Map : d->Maps)
			{
				LViewI *c = Ui->FindControl(Map.value->Id);
				if (c)
				{
					int Type = Map.value->Hint ? Map.value->Hint : GetCtrlType(c);

					switch (Type)
					{
						case GV_LIST:
						{
							if (!Xml) break;
							LXmlTag *Child = Xml->GetChildTag(Map.key, true);
							if (!Child) break;
							LList *Lst = dynamic_cast<LList*>(c);
							if (!Lst) break;
							Child->Empty(true);
							Child->SetTag(Map.key);

							List<LListItem> All;
							Lst->GetAll(All);
							for (auto i: All)
							{
								LXmlTag *n = new LXmlTag(Map.value->ChildElements.Str());
								if (n)
								{
									i->XmlIo(n, true);
									Child->InsertTag(n);
								}
							}
							break;
						}
						case GV_CUSTOM: // GTree
						{
							if (!Xml) break;
							LXmlTag *Child = Xml->GetChildTag(Map.key, true);
							
							if (!Child) break;
							GTree *Tree = dynamic_cast<GTree*>(c);
							
							if (!Tree) break;
							Child->Empty(true);
							Child->SetTag(Map.key);
							
							Map.value->SaveTree(Tree, Child);
							break;
						}
						case GV_INT32:
						case GV_BOOL:
						{
							Tag->SetValue(Map.key, v = c->Value());
							Status = true;
							break;
						}
						case GV_DOM:
						{
							GControlTree *ct;
							if (Ui->GetViewById(Map.value->Id, ct))
							{
								ct->Serialize(Xml, true);
							}
							break;
						}
						default:
						{
							auto Str = c->Name();
							
							if (ValidStr(Str))
								v = Str;
							else
								v.Empty();

							Tag->SetValue(Map.key, v);
							Status = true;
							break;
						}
					}
				}
				else
				{
					v.Empty();
					Tag->SetValue(Map.key, v);
				}
			}
		}
	}

	return Status;
}

