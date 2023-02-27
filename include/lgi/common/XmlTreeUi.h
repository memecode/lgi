/// \file
/// \author Matthew Allen

#ifndef _XML_TREE_UI_H_
#define _XML_TREE_UI_H_

#include "lgi/common/Variant.h"
#include "lgi/common/List.h"
#include "lgi/common/Tree.h"

/// This class allows you to serialize data between the user interface and a
/// LDom instance. Useful for storing user visible application settings in
/// XML or an optons file.
class LXmlTreeUi
{
	class LXmlTreeUiPriv *d;

public:
	typedef LHashTbl<ConstStrKey<char,false>,int> EnumMap;

	LXmlTreeUi();
	virtual ~LXmlTreeUi();

	/// Create attribute <-> UI element mapping for generic control
	void Map(const char *Attr, int UiIdent, int Type = GV_NULL);
	/// Create attribute <-> UI element mapping for LItemContainer control
	void Map(const char *Attr, int UiIdent, const char *ChildElementName, std::function<LItem*()> Callback);
	/// Create attribute <-> UI element mapping for a enum and group of radio buttons.
	void Map(const char *Attr, EnumMap &Mapping);
	/// Clear all mappings
	void EmptyMaps();
	
	/// Convert data to/from an XML tag
	virtual bool Convert(LDom *Tag, LViewI *ui, bool ToUI);
	/// Disable/enable all control
	void EnableAll(LViewI *ui, bool Enable);
	/// Empty all controls of text / value
	void EmptyAll(LViewI *ui);
	/// Returns true if the attribute is mapped
	bool IsMapped(const char *Attr);
};

#endif