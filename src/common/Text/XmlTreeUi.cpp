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

#include <ctype.h>

struct Mapping
{
	int Id = 0;
	int Hint = GV_NULL;
	std::function<LItem*()> Callback;
	LString ChildElementName;
	LXmlTreeUi::EnumMap EnumMap;

	void LoadTree(LTreeNode *n, LXmlTag *t)
	{
		for (auto c: t->Children)
		{
			auto i = dynamic_cast<LTreeItem*>(Callback());
			if (i)
			{
				i->XmlIo(c, false);
				n->Insert(i);

				LoadTree(i, c);
			}
		}
	}

	void SaveTree(LTreeNode *n, LXmlTag *t)
	{
		for (LTreeItem *i = n->GetChild(); i; i = i->GetNext())
		{
			LXmlTag *n = new LXmlTag(ChildElementName);
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
	else LAssert(!"Invalid params");
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
	else LAssert(!"Invalid params");
}

bool LXmlTreeUi::IsMapped(const char *Attr)
{
	return d->Maps.Find(Attr) != NULL;
}

void LXmlTreeUi::Map(const char *Attr, int UiIdent, int Type)
{
	if (UiIdent > 0 &&
		(Attr != NULL || Type == GV_DOM))
	{		
		Mapping *m = new Mapping;
		if (m)
		{
			m->Id = UiIdent;
			m->Hint = Type;
			
			LAssert(!d->Maps.Find(Attr));
			d->Maps.Add(Attr, m);
		}
	}
	else LAssert(!"Invalid params");
}

void LXmlTreeUi::Map(const char *Attr, int UiIdent, const char *ChildElementName, std::function<LItem*()> Callback)
{
	if (Attr && UiIdent > 0 && Callback && ChildElementName)
	{
		Mapping *m = new Mapping;
		if (m)
		{
			m->Id = UiIdent;
			m->Callback = Callback;
			m->Hint = GV_LIST;
			m->ChildElementName = ChildElementName;
			
			LAssert(!d->Maps.Find(Attr));
			d->Maps.Add(Attr, m);
		}
	}
	else LAssert(!"Invalid params");
}

void LXmlTreeUi::Map(const char *Attr, LHashTbl<ConstStrKey<char,false>,int> &Map)
{
	if (Attr && Map.Length() > 0)
	{
		auto m = new Mapping;
		if (m)
		{
			m->Hint = GV_HASHTABLE;
			m->EnumMap = Map;
			
			LAssert(!d->Maps.Find(Attr));
			d->Maps.Add(Attr, m);
		}
	}
	else LAssert(!"Invalid params");
}

void LXmlTreeUi::EmptyMaps()
{
	d->Maps.DeleteObjects();
}

int GetCtrlType(LViewI *v)
{
	if (v)
	{
		if (dynamic_cast<LCheckBox*>(v) ||
			dynamic_cast<LButton*>(v) ||
			dynamic_cast<LRadioButton*>(v))
		{
			return GV_BOOL;
		}
		else if (dynamic_cast<LSlider*>(v) ||
				 dynamic_cast<LCombo*>(v) ||
				 dynamic_cast<LRadioGroup*>(v))
		{
			return GV_INT32;
		}
		else if (!Stricmp(v->GetClass(), "LControlTree"))
		{
			return GV_DOM;
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
	else LAssert(!"Invalid params");

	return GV_NULL;
}

bool LXmlTreeUi::Convert(LDom *Tag, LViewI *Ui, bool ToUI)
{
	bool Status = false;

	if (Ui && Tag)
	{
		LVariant v;
		LXmlTag *Xml = dynamic_cast<LXmlTag*>(Tag);
		if (ToUI)
		{
			// Xml -> UI
			for (auto Map : d->Maps)
			{
				Mapping *m = Map.value;
				switch (m->Hint)
				{
					case GV_HASHTABLE:
					{
						Tag->GetValue(Map.key, v);
						if (v.Str())
						{
							for (auto e: m->EnumMap)
							{
								auto name = e.key;
								if (!Stricmp(name, v.Str()))
									Ui->SetCtrlValue(e.value, true);
							}
						}
						break;
					}
					case GV_LIST:
					{
						if (!Xml)
						{
							LAssert(!"Needs an xml tag.");
							break;
						}

						LList *container;
						auto t = Xml->GetChildTag(Map.key);
						if (!t || !Ui->GetViewById(m->Id, container))
							continue;
						
						container->Empty();
						for (auto c: t->Children)
						{
							auto i = dynamic_cast<LList::TItem*>(m->Callback());
							if (i)
							{
								if (i->XmlIo(c, false))
									container->Insert(i);
								else
									delete i;
							}
						}
						break;
					}
					case GV_CUSTOM:
					{
						if (!Xml)
						{
							LAssert(!"Needs an xml tag.");
							break;
						}

						LXmlTag *t = Xml->GetChildTag(Map.key);
						if (!t) continue;

						LTree *Tree;
						if (!Ui->GetViewById(m->Id, Tree)) continue;
						Tree->Empty();

						m->LoadTree(Tree, t);
						break;
					}
					case GV_DOM:
					{
						LView *ct;
						if (!Ui->GetViewById(m->Id, ct))
							break;

						LScriptArguments Args(NULL);
						Args[0] = new LVariant(Xml);
						Args[1] = new LVariant(false);
						auto Param = LDomPropToString(ControlSerialize);
						ct->CallMethod(Param, Args);
						break;
					}
					default:
					{
						if (Tag->GetValue(Map.key, v))
						{
							int Type = m->Hint ? m->Hint : GetDataType(v.Str());

							if (Type == GV_BOOL ||
								Type == GV_INT32 ||
								Type == GV_INT64)
							{
								Ui->SetCtrlValue(m->Id, v.CastInt32());
							}
							else
							{
								Ui->SetCtrlName(m->Id, v.Str());
							}
							Status = true;
						}
						else
						{
							LEdit *c;
							if (Ui->GetViewById(m->Id, c))
								c->Name("");
						}
						break;
					}
				}
			}
		}
		else
		{
			// UI -> Xml
			for (auto Map : d->Maps)
			{
				Mapping *m = Map.value;
				if (m->Hint == GV_HASHTABLE)
				{
					for (auto e: m->EnumMap)
					{
						if (Ui->GetCtrlValue(e.value))
						{
							Tag->SetValue(Map.key, v = e.key);
							Status = true;
						}
					}
				}
				else
				{
					LViewI *c = Ui->FindControl(m->Id);
					if (c)
					{
						int Type = m->Hint ? m->Hint : GetCtrlType(c);

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
									if (auto n = new LXmlTag(m->ChildElementName))
									{
										if (i->XmlIo(n, true))
											Child->InsertTag(n);
										else
											delete n;
									}
								}
								break;
							}
							case GV_CUSTOM: // LTree
							{
								if (!Xml) break;
								LXmlTag *Child = Xml->GetChildTag(Map.key, true);
							
								if (!Child) break;
								LTree *Tree = dynamic_cast<LTree*>(c);
							
								if (!Tree) break;
								Child->Empty(true);
								Child->SetTag(Map.key);
							
								m->SaveTree(Tree, Child);
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
								LView *ct;
								if (Ui->GetViewById(m->Id, ct))
								{
									LScriptArguments Args(NULL);
									Args[0] = new LVariant(Xml);
									Args[1] = new LVariant(true);
									ct->CallMethod(LDomPropToString(ControlSerialize), Args);
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
	}
	else LAssert(!"Invalid params");

	return Status;
}

