/// \file
/// \author Matthew Allen <fret@memecode.com>
#ifndef _XML_TREE_H_
#define _XML_TREE_H_

#include "lgi/common/HashTable.h"
#include "lgi/common/RefCount.h"
#include "lgi/common/Dom.h"

/// Runtime option: Don't encode entities
#define GXT_NO_ENTITIES						0x0001
/// Runtime option: Output minimal whitespace
#define GXT_NO_PRETTY_WHITESPACE			0x0002
/// Runtime option: Output extra whitespace
#define GXT_PRETTY_WHITESPACE				0x0004
/// Runtime option: Keep input whitespace
#define GXT_KEEP_WHITESPACE					0x0008
/// Runtime option: Don't create DOM heirarchy, just a list of tags
#define GXT_NO_DOM							0x0010
/// Don't print <?xml ... ?> header
#define GXT_NO_HEADER						0x0020

class LXmlTree;
class LXmlTreePrivate;

class LgiClass LXmlAlloc : public GRefCount
{
public:
	virtual ~LXmlAlloc() {}

	virtual void *Alloc(size_t Size) = 0;
	virtual void Free(void *Ptr) = 0;
	
	// Helper allocator for strings
	char *Alloc(const char *s, ssize_t len = -1);
};

/// Xml attribute, a named value.
class LgiClass LXmlAttr
{
	friend class LXmlTag;
	friend class LXmlTree;

	/// The name of the attribute
	char *Name;
	/// The value of the attribute
	char *Value;

public:
	LXmlAttr()
	{
		Name = 0;
		Value = 0;
	}
	
	char *GetName() { return Name; }
	char *GetValue() { return Value; }
};

/// An XML element or tag. Contains optionally sub tags and a list of attributes.
/// C++ applications can inherit from this and have native member types mapped to
/// attributes using the SerializeAttr methods. All you have to do is override the
/// virtual member function Serialize and call SerializeAttr on each of your native
/// member variables. When loading and saving the attributes will be mapped to and
/// from your native types.
class LgiClass LXmlTag : virtual public LDom
{
	friend class LXmlTree;

	void ParseAttribute(LXmlTree *Tree, LXmlAlloc *Alloc, char *&t, bool &NoChildren, bool &TypeDef);

protected:
	/// This allocator is responsible for all the string memory used by the Attributes and Tag
	GAutoRefPtr<LXmlAlloc> Allocator;

	bool Write;
	LXmlAttr *_Attr(const char *Name, bool Write);
	bool GetVariant(const char *Name, LVariant &Value, const char *Array);
	bool SetVariant(const char *Name, LVariant &Value, const char *Array);

	/// The name of the tag/element. This can be NULL in the case
	/// that the element is purely content. The memory is managed by 
	/// 'Allocator' not the general heap. Do not "NewStr" something
	/// and assign it to 'Tag'.
	char *Tag;

	/// Any content following the tag. Memory is owned by the allocator.
	char *Content;

public:
	/// The parent element/tag.
	LXmlTag *Parent;
	/// A list of attributes that this tag has
	LArray<LXmlAttr> Attr;
	/// A list of child tags. Don't edit this list yourself, use the
	/// InsertTag and RemoveTag methods.
	LArray<LXmlTag*> Children;
	
	/// Construct the object
	LXmlTag
	(
		/// [Optional] Start with this name
		const char *tag = 0,
		/// [Optional] Use this allocator
		LXmlAlloc *alloc = 0
	);	
	/// Construct the object
	LXmlTag(const LXmlTag &t);	
	virtual ~LXmlTag();

	/// For debugging.
	bool Dump(int Depth = 0);
	/// Free any memory owned by this object
	void Empty(bool Deep);
	/// Free all attributes
	void EmptyAttributes();
	/// Frees all child tags
	void EmptyChildren();
	/// Swaps object contents
	void Swap(LXmlTag &t);

	/// \return true if the tag is 's'
	bool IsTag(const char *s) { return Tag && s ? _stricmp(Tag, s) == 0 : false; }

	/// Get the content
	char *GetContent() { return Content; };
	/// Convert the content to an integer
	int GetContentAsInt(int Default = 0);
	/// Set the content
	bool SetContent(const char *s, ssize_t len = -1);

	/// Get the string value of a named attribute
	char *GetAttr(const char *Name);
	/// Get the value of a named attribute as an int
	int GetAsInt(const char *Name);
	/// Set the value of a named attribute to a string
	bool SetAttr(const char *Name, const char *Value);
	/// Set the value of a named attribute to an int
	bool SetAttr(const char *Name, int Value);
	/// Set the value of a named attribute to an int64
	bool SetAttr(const char *Name, int64 Value);
	/// Deletes an attribute
	bool DelAttr(const char *Name);

	/// Replaces the content with an integer
	bool SetContent(int i);

	/// Read/write a native C integer into an attribute
	bool SerializeAttr(const char *Attr, int &Int);	
	/// Read/write a native C dynamically allocated string into an attribute
	bool SerializeAttr(const char *Attr, char *&Str);
	/// Read/write a native C double into an attribute
	bool SerializeAttr(const char *Attr, double &Dbl);
	/// Read/write a LString into an attribute
	bool SerializeAttr(const char *Attr, LString &s);
	
	/// Gets the current tag
	const char *GetTag();
	/// Sets or removes the tag using the allocator
	void SetTag(const char *Str = NULL, ssize_t Len = -1);

	/// Read/write all your native types in here
	virtual bool Serialize(bool Write) { return false; }
	/// Returns a pointer to a child tag if present, or NULL if not.
	LXmlTag *GetChildTag(const char *Name, bool Create = false, const char *TagSeparator = ".");
	/// Creates a sub tag if it doesn't already exist.
	LXmlTag *CreateTag(const char *Name, char *Content = 0);
	/// Inserts a child tag.
	virtual void InsertTag(LXmlTag *t);
	/// Removes this tag from the DOM hierarchy.
	virtual bool RemoveTag();
	/// Counts all this and all child tags
	int64 CountTags();

	/// Copy operator, doesn't effect children.
	LXmlTag &operator =(LXmlTag &t);
	/// Copy method, deep option copies all child elements as well.
	bool Copy(LXmlTag &t, bool Deep = false);
	
	/// Retrieve elements using XPath notation.
	bool XPath(LArray<LXmlTag*> &Results, const char *Path);
};

/// In the case your inheriting objects from LXmlTag you need to instantiate your
/// types as the XML is loaded and elements are encountered. Pass this class to the
/// LXmlTree::Read to get create events during the read process, create an object of
/// the right type and pass it back.
class LXmlFactory
{
public:
	/// Creates an object of type 'Tag' and returns it.
	virtual LXmlTag *Create(char *Tag) = 0;
};

/// The reader/writer for a tree of LXmlTag objects.
class LgiClass LXmlTree
{
	friend class LXmlTag;
	LXmlTreePrivate *d;

protected:
	LXmlTag *Parse(LXmlTag *Tag, LXmlAlloc *Alloc, char *&t, bool &NoChildren, bool InTypeDef);
	virtual void OnParseComment(LXmlTag *Ref, const char *Comment, ssize_t Bytes) {}

	void Output(LXmlTag *t, int Depth);

public:
	/// Constructor
	LXmlTree
	(
		/// \sa #GXT_NO_ENTITIES, #GXT_NO_PRETTY_WHITESPACE, #GXT_PRETTY_WHITESPACE, #GXT_KEEP_WHITESPACE and #GXT_NO_DOM
		int Flags = 0
	);
	virtual ~LXmlTree();
	
	/// Read an XML file into a DOM tree of LXmlTag objects from a stream.
	bool Read
	(
		/// The root tag to create children from.
		LXmlTag *Root,
		/// The stream to read from.
		LStreamI *File,
		/// [Optional] The factory to create LXmlTag type objects. If not specified
		/// vanilla LXmlTag objects will be created.
		LXmlFactory *Factory = 0
	);
	/// Write an XML file from a DOM tree of LXmlTag objects into a stream.
	bool Write
	(
		/// The DOM tree.
		LXmlTag *Root,
		/// The output stream.
		LStreamI *File,
		/// [Optional] Progress reporting.
		Progress *Prog = NULL
	);
	/// Gets the last error message.
	char *GetErrorMsg();
	/// A hash of tags that can't have children.
	LHashTbl<ConstStrKey<char,false>,bool> *NoChildTags();
	/// Gets the associated style file
	char *GetStyleFile(char **StyleType = 0);
	/// Sets the associated css file
	void SetStyleFile(char *stylefile, const char *styletype = "text/css");

	/// Add entities
	LHashTbl<ConstStrKey<char,false>,char16> *GetEntityTable();
	/// Decode a string with entities
	char *DecodeEntities(LXmlAlloc *Alloc, char *s, ssize_t len = -1);
	/// Encode a string to use entities
	char *EncodeEntities(const char *s, ssize_t len = -1, const char *extra_characters = 0);
	/// Encode a string to use entities
	bool EncodeEntities(LStreamI *out, const char *s, ssize_t len, const char *extra_characters = 0);
};

#endif
