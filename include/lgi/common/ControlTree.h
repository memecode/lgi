#ifndef _GCTRLTREE_H_
#define _GCTRLTREE_H_

#include "lgi\common\Tree.h"
#include "lgi/common/Variant.h"

class GControlTree : public LTree, public GDom
{
public:
	struct EnumValue
	{
		GString Name;
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
		typedef GArray<EnumValue> EnumArr;

	private:
		int CtrlId;
		GAutoString Opt;
		LVariantType Type;
		LVariant Value;
		LViewI *Ctrl;
		GButton *Browse;
		GAutoPtr<EnumArr> Enum;

		void Save();

	public:
		enum ItemFlags
		{
			TYPE_FILE = 0x1,
		};
		int Flags;

		Item(int ctrlId, char *Txt, const char *opt, LVariantType type, GArray<EnumValue> *pEnum);
		~Item();

		Item *Find(const char *opt);
		bool Serialize(GDom *Store, bool Write);
		void SetValue(LVariant &v);
		LRect &GetRect();
		void Select(bool b);
		void OnPaint(ItemPaintCtx &Ctx);
		void SetEnum(GAutoPtr<EnumArr> e);
		void OnVisible(bool v);
		void PositionControls();
	};

protected:
	class GControlTreePriv *d;

	class Item *Resolve(bool Create, const char *Path, int CtrlId, LVariantType Type = GV_NULL, GArray<EnumValue> *Enum = 0);
    void ReadTree(LXmlTag *t, LTreeNode *n);

public:
	GControlTree();
	~GControlTree();

	const char *GetClass() { return "GControlTree"; }

	Item *Find(const char *opt);
	LTreeItem *Insert(const char *DomPath, int CtrlId, LVariantType Type, LVariant *Value = 0, GArray<EnumValue> *Enum = 0);
	bool SetVariant(const char *Name, LVariant &Value, char *Array = 0);
	bool Serialize(GDom *Store, bool Write);
	int OnNotify(LViewI *c, int f);
};

#endif