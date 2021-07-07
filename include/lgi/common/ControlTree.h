#pragma once

#include "lgi/common/Tree.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Button.h"

class LControlTree : public LTree, public GDom
{
public:
	struct EnumValue
	{
		LString Name;
		LVariant Value;
		
		void Set(const char *n, int val)
		{
			Name = n;
			Value = val;
		}
	};

	class Item : public LTreeItem
	{
	public:
		typedef LArray<EnumValue> EnumArr;

	private:
		int CtrlId;
		LAutoString Opt;
		LVariantType Type;
		LVariant Value;
		LViewI *Ctrl;
		LButton *Browse;
		LAutoPtr<EnumArr> Enum;

		void Save();

	public:
		enum ItemFlags
		{
			TYPE_FILE = 0x1,
		};
		int Flags;

		Item(int ctrlId, char *Txt, const char *opt, LVariantType type, LArray<EnumValue> *pEnum);
		~Item();

		Item *Find(const char *opt);
		bool Serialize(GDom *Store, bool Write);
		void SetValue(LVariant &v);
		LRect &GetRect();
		void Select(bool b);
		void OnPaint(ItemPaintCtx &Ctx);
		void SetEnum(LAutoPtr<EnumArr> e);
		void OnVisible(bool v);
		void PositionControls();
	};

protected:
	class LControlTreePriv *d;

	class Item *Resolve(bool Create, const char *Path, int CtrlId, LVariantType Type = GV_NULL, LArray<EnumValue> *Enum = 0);
    void ReadTree(LXmlTag *t, LTreeNode *n);

public:
	LControlTree();
	~LControlTree();

	const char *GetClass() { return "LControlTree"; }

	Item *Find(const char *opt);
	LTreeItem *Insert(const char *DomPath, int CtrlId, LVariantType Type, LVariant *Value = 0, LArray<EnumValue> *Enum = 0);
	bool SetVariant(const char *Name, LVariant &Value, char *Array = 0);
	bool Serialize(GDom *Store, bool Write);
	int OnNotify(LViewI *c, int f);
};

