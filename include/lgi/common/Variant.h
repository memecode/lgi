/**
	\file
	\author Matthew Allen
	\brief Variant class.\n
	Copyright (C), <a href="mailto:fret@memecode.com">Matthew Allen</a>
 */

#ifndef __LVariant_H__
#define __LVariant_H__

#undef Bool
#include "lgi/common/DateTime.h"
#include "lgi/common/Containers.h"
#include "lgi/common/HashTable.h"
#include "lgi/common/LgiString.h"

class LCompiledCode;

#if !defined(_MSC_VER) && !defined(LINUX) && (!HAIKU64)
	// For all Mac and Haiku32
	#define LVARIANT_SIZET	1
	#define LVARIANT_SSIZET	1
#endif

/// The different types the varient can be.
/// \sa LVariant::TypeToString to convert to string.
enum LVariantType
{
	// Main types
	
	/// Null type (0)
	GV_NULL,
	/// 32-bit integer (1)
	GV_INT32,
	/// 64-bit integer (2)
	GV_INT64,
	/// true or false boolean. (3)
	GV_BOOL,
	/// C++ double (4)
	GV_DOUBLE,
	/// Null terminated string value (5)
	GV_STRING,
	/// Block of binary data (6)
	GV_BINARY,
	/// List of LVariant (7)
	GV_LIST,
	/// Pointer to LDom object (8)
	GV_DOM,
	/// DOM reference, ie. a variable in a DOM object (9)
	GV_DOMREF,
	/// Untyped pointer (10)
	GV_VOID_PTR,
	/// LDateTime class. (11)
	GV_DATETIME,
	/// Hash table class, containing pointers to LVariants (12)
	GV_HASHTABLE,
	// Scripting language operator (13)
	GV_OPERATOR,
	// Custom scripting lang type (14)
	GV_CUSTOM,
	// Wide string (15)
	GV_WSTRING,
	// LSurface ptr (16)
	GV_LSURFACE,
	/// Pointer to LView (17)
	GV_LVIEW,
	/// Pointer to LMouse (18)
	GV_LMOUSE,
	/// Pointer to LKey (19)
	GV_LKEY,
	/// Pointer to LStream (20)
	GV_STREAM,
	/// The maximum value for the variant type. (21)
	/// (This is used by the scripting engine to refer to a LVariant itself)
	GV_MAX,
};

/// Language operators
enum LOperator
{
	OpNull,
	OpAssign,
	OpPlus,
	OpUnaryPlus,
	OpMinus,
	OpUnaryMinus,
	OpMul,
	OpDiv,
	OpMod,
	OpLessThan,
	OpLessThanEqual,
	OpGreaterThan,
	OpGreaterThanEqual,
	OpEquals,
	OpNotEquals,
	OpPlusEquals,
	OpMinusEquals,
	OpMulEquals,
	OpDivEquals,
	OpPostInc,
	OpPostDec,
	OpPreInc,
	OpPreDec,
	OpAnd,
	OpOr,
	OpNot,
};

class LgiClass LCustomType : public LDom
{
protected:
	struct CustomField : public LDom
	{
		ssize_t Offset;
		ssize_t Bytes;
		ssize_t ArrayLen;
		LVariantType Type;
		LString Name;
		LCustomType *Nested;
    
        const char *GetClass() override { return "LCustomType.CustomField"; }
		ssize_t Sizeof();
		bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	};

public:
	struct Method : public LDom
	{
		LString Name;
		LArray<LString> Params;
		size_t Address = -1;
		int FrameSize = -1;
		
		const char *GetClass() override { return "LCustomType.Method"; }
	};

protected:
	// Global vars
	int Pack;
	size_t Size;
	LString Name;

	// Fields
	LArray<CustomField*> Flds;
	LHashTbl<ConstStrKey<char,false>, int> FldMap;
	
	// Methods
	LArray<Method*> Methods;
	LHashTbl<ConstStrKey<char,false>, Method*> MethodMap;
	
	// Private methods
	ssize_t PadSize();

public:
	LCustomType(const char *name, int pack = 1);
	LCustomType(const char16 *name, int pack = 1);
	~LCustomType();
	
	const char *GetClass() override { return "LCustomType"; }
	size_t Sizeof();
	const char *GetName() { return Name; }
	ssize_t Members() { return Flds.Length(); }
	int AddressOf(const char *Field);
	int IndexOf(const char *Field);
	bool DefineField(const char *Name, LVariantType Type, int Bytes, int ArrayLen = 1);
	bool DefineField(const char *Name, LCustomType *Type, int ArrayLen = 1);
	Method *DefineMethod(const char *Name, LArray<LString> &Params, size_t Address);
	Method *GetMethod(const char *Name);

	// Field access. You can't use the LDom interface to get/set member variables because
	// there is no provision for the 'This' pointer.
	bool Get(int Index, LVariant &Out, uint8_t *This, int ArrayIndex = 0);
	bool Set(int Index, LVariant &In, uint8_t *This, int ArrayIndex = 0);
	
	// Dom access. However the DOM can be used to access information about the type itself.
	// Which doesn't need a 'This' pointer.
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	bool CallMethod(const char *MethodName, LScriptArguments &Args) override;
};

/// A class that can be different types
class LgiClass LVariant
{
public:
	typedef LHashTbl<ConstStrKey<char>,LVariant*> LHash;

	/// The type of the variant
    LVariantType Type;

    /// The value of the variant
	union
    {
		/// Valid when Type == #GV_INT32
	    int Int;
		/// Valid when Type == #GV_BOOL
	    bool Bool;
		/// Valid when Type == #GV_INT64
	    int64 Int64;
		/// Valid when Type == #GV_DOUBLE
	    double Dbl;
	    /// Valid when Type == #GV_STRING
		char *String;
	    /// Valid when Type == #GV_WSTRING
		char16 *WString;
		/// Valid when Type == #GV_DOM
		LDom *Dom;
		/// Valid when Type is #GV_VOID_PTR, #GV_GVIEW, #GV_LMOUSE or #GV_LKEY
		void *Ptr;
		/// Valid when Type == #GV_BINARY
	    struct _Binary
	    {
		    ssize_t Length;
		    void *Data;
	    } Binary;
		/// Valid when Type == #GV_LIST
	    List<LVariant> *Lst;
		/// Valid when Type == #GV_HASHTABLE
	    LHash *Hash;
		/// Valid when Type == #GV_DATETIME
		LDateTime *Date;
		/// Valid when Type == #GV_CUSTOM
		struct _Custom
		{
			LCustomType *Dom;
			uint8_t *Data;

			bool operator == (_Custom &c)
			{
				return Dom == c.Dom &&
						Data == c.Data;
			}
		} Custom;
		/// Valid when Type == #GV_DOMREF
		struct _DomRef
		{
			/// The pointer to the dom object
			LDom *Dom;
			/// The name of the variable to set/get in the dom object
			char *Name;
		} DomRef;
		/// Valid when Type == #GV_OPERATOR
		LOperator Op;		
		/// Valid when Type == #GV_LSURFACE
		struct
		{
		    class LSurface *Ptr;
		    bool Own;
			LSurface *Release()
			{
				auto p = Ptr;
				Ptr = NULL;
				Own = false;
				return p;
			}
		} Surface;
		/// Valid when Type == #GV_STREAM
		struct
		{
			class LStreamI *Ptr;
			bool Own;
			LStreamI *Release()
			{
				auto p = Ptr;
				Ptr = NULL;
				Own = false;
				return p;
			}
		} Stream;
		/// Valid when Type == #GV_GVIEW
		class LView *View;
		/// Valid when Type == #GV_LMOUSE
		class LMouse *Mouse;
		/// Valid when Type == #GV_LKEY
		class LKey *Key;
	} Value;

	/// Constructor to null
	LVariant();
	/// Constructor for integers
	LVariant(int32_t i);
	LVariant(uint32_t i);
	LVariant(int64_t i);
	LVariant(uint64_t i);
	#if LVARIANT_SIZET
	LVariant(size_t i);
	#endif
	#if LVARIANT_SSIZET
	LVariant(ssize_t i);
	#endif
	/// Constructor for double
	LVariant(double i);
	/// Constructor for string
	LVariant(const char *s);
	/// Constructor for wide string
	LVariant(const char16 *s);
	/// Constructor for ptr
	LVariant(void *p);
	/// Constructor for DOM ptr
	LVariant(LDom *p);
	/// Constructor for DOM variable reference
	LVariant(LDom *p, char *name);
	/// Constructor for date
	LVariant(const LDateTime *d);
	/// Constructor for variant
	LVariant(LVariant const &v);
	/// Constructor for operator
	LVariant(LOperator Op);
	/// Destructor
	~LVariant();

	/// Assign bool value
	LVariant &operator =(bool i);
	/// Assign an integer value
	LVariant &operator =(int32_t i);
	LVariant &operator =(uint32_t i);
	LVariant &operator =(int64_t i);
	LVariant &operator =(uint64_t i);
	#if LVARIANT_SIZET
	LVariant &operator =(size_t i);
	#endif
	#if LVARIANT_SSIZET
	LVariant &operator =(ssize_t i);
	#endif
	/// Assign double value
	LVariant &operator =(double i);
	/// Assign string value (makes a copy)
	LVariant &operator =(const char *s);
	/// Assign a wide string value (makes a copy)
	LVariant &operator =(const char16 *s);
	/// Assign another variant value
	LVariant &operator =(LVariant const &i);
	/// Assign value to a void ptr
	LVariant &operator =(void *p);
	/// Assign value to DOM ptr
	LVariant &operator =(LDom *p);
	/// Assign value to be a date/time
	LVariant &operator =(const LDateTime *d);

	LVariant &operator =(class LView *p);
	LVariant &operator =(class LMouse *p);
	LVariant &operator =(class LKey *k);
	LVariant &operator =(class LStream *s);

	bool operator ==(LVariant &v);
	bool operator !=(LVariant &v) { return !(*this == v); }

	/// Sets the value to a DOM variable reference
	bool SetDomRef(LDom *obj, char *name);
	/// Sets the value to a copy of	block of binary data
	bool SetBinary(ssize_t Len, void *Data, bool Own = false);
	/// Sets the value to a copy of the list
	List<LVariant> *SetList(List<LVariant> *Lst = NULL);
	/// Sets the value to a hashtable
	bool SetHashTable(LHash *Table = NULL, bool Copy = true);
	/// Set the value to a surface
	bool SetSurface(class LSurface *Ptr, bool Own);
	/// Set the value to a stream
	bool SetStream(class LStreamI *Ptr, bool Own);

	/// Returns the string if valid (will convert a GV_WSTRING to utf)
	char *Str();
	/// Returns the value as an LString
	LString LStr();
	/// Returns a wide string if valid (will convert a GV_STRING to wide)
	char16 *WStr();
	/// Returns the string, releasing ownership of the memory to caller and
	/// changing this LVariant to GV_NULL.
	char *ReleaseStr();
	/// Returns the wide string, releasing ownership of the memory to caller and
	/// changing this LVariant to GV_NULL.
	char16 *ReleaseWStr();
	/// Sets the variant to a heap string and takes ownership of it
	bool OwnStr(char *s);
	/// Sets the variant to a wide heap string and takes ownership of it
	bool OwnStr(char16 *s);
	/// Sets the variant to NULL
	void Empty();
	/// Returns the byte length of the data
	int64 Length();

	/// True if currently a int
	bool IsInt();
	/// True if currently a bool
	bool IsBool();
	/// True if currently a double
	bool IsDouble();
	/// True if currently a string
	bool IsString();
	/// True if currently a binary block
	bool IsBinary();
	/// True if currently null
	bool IsNull();

	/// Changes the variant's type, maintaining the value where possible. If
	/// no conversion is available then nothing happens.
	LVariant &Cast(LVariantType NewType);
	/// Casts the value to int, from whatever source type. The
	/// LVariant type does not change after calling this.
	int32 CastInt32() const;
	/// Casts the value to a 64 bit int, from whatever source type. The
	/// LVariant type does not change after calling this.
	int64 CastInt64() const;
	/// Casts the value to double, from whatever source type. The
	/// LVariant type does not change after calling this.
	double CastDouble() const;
	/// Cast to a string from whatever source type, the LVariant will
	/// take the type GV_STRING after calling this. This is because
	/// returning a static string is not thread safe.
	char *CastString();
	/// Casts to a DOM ptr
	LDom *CastDom() const;
	/// Casts to a boolean. You probably DON'T want to use this function. The
	/// behavior for strings -> bool is such that if the string is value it
	/// always evaluates to true, and false if it's not a valid string. Commonly
	/// what you want is to evaluate whether the string is zero or non-zero in
	/// which cast you should use "CastInt32() != 0" instead.
	bool CastBool() const;
	/// Returns the pointer if available.
	void *CastVoidPtr() const;
	/// Returns a LView
	LView *CastView() const { return Type == GV_LVIEW ? Value.View : NULL; }

	/// List insert
	bool Add(LVariant *v, int Where = -1);	
	
	/// Converts the varient type to a string
	static const char *TypeToString(LVariantType t);
	/// Converts an operator to a string
	static const char *OperatorToString(LOperator op);
	/// Converts the value to a string description include type.
	LString ToString();
};

// General collection of arguments and a return value
class LgiClass LScriptArguments : public LArray<LVariant*>
{
	friend class LScriptEngine;
	friend class LVirtualMachine;
	friend class LVirtualMachinePriv;
	friend struct ExecuteFunctionState;

	LVirtualMachineI *Vm = NULL;
	class LStream *Console = NULL;
	LVariant *LocalReturn = NULL; // Owned by this instance
	LVariant *Return = NULL;

	const char *File = NULL;
	int Line = 0;
	LString ExceptionMsg;
	ssize_t Address;

public:
	static LStream NullConsole;

	LScriptArguments(LVirtualMachineI *vm, LVariant *ret = NULL, LStream *console = NULL, ssize_t address = -1);
	~LScriptArguments();

	LVirtualMachineI *GetVm() { return Vm; }
	void SetVm(LVirtualMachineI *vm) { Vm = vm; }
	LVariant *GetReturn() { return Return; } // Must never be NULL.
	LStream *GetConsole() { return Console; }
	bool HasException() { return File != NULL || ExceptionMsg.Get() || Line != 0; }
	bool Throw(const char *File, int Line, const char *Msg, ...);

	// Accessor shortcuts
	const char *StringAt(size_t i);
	int32_t Int32At(size_t i, int32_t Default = 0);
	int64_t Int64At(size_t i, int64_t Default = 0);
	double DoubleAt(size_t i, double Default = 0);
};

#endif
