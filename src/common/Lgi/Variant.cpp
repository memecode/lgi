#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"

const char *LVariant::TypeToString(LVariantType t)
{
	switch (t)
	{
		case GV_NULL:		return "NULL";
		case GV_INT32:		return "int32";
		case GV_INT64:		return "int64";
		case GV_BOOL:		return "bool";
		case GV_DOUBLE:		return "double";
		case GV_STRING:		return "String";
		case GV_BINARY:		return "Binary";
		case GV_LIST:		return "List";
		case GV_DOM:		return "Dom";
		case GV_DOMREF:		return "DomReference";
		case GV_VOID_PTR:	return "VoidPtr";
		case GV_DATETIME:	return "DateTime";
		case GV_HASHTABLE:	return "HashTable";
		case GV_OPERATOR:	return "Operator";
		case GV_CUSTOM:		return "Custom";
		case GV_WSTRING:	return "WString";
		case GV_GVIEW:		return "View";
		case GV_STREAM:		return "Stream";
		case GV_LSURFACE:	return "Surface";
		case GV_LMOUSE:		return "MouseEvent";
		case GV_LKEY:		return "KeyboardEvent";
		default:			return "Unknown";
	}
	
	return NULL;
}

const char *LVariant::OperatorToString(LOperator op)
{
	switch (op)
	{
		case OpNull: return "OpNull";
		case OpAssign: return "OpAssign";
		case OpPlus: return "OpPlus";
		case OpUnaryPlus: return "OpUnaryPlus";
		case OpMinus: return "OpMinus";
		case OpUnaryMinus: return "OpUnaryMinus";
		case OpMul: return "OpMul";
		case OpDiv: return "OpDiv";
		case OpMod: return "OpMod";
		case OpLessThan: return "OpLessThan";
		case OpLessThanEqual: return "OpLessThanEqual";
		case OpGreaterThan: return "OpGreaterThan";
		case OpGreaterThanEqual: return "OpGreaterThanEqual";
		case OpEquals: return "OpEquals";
		case OpNotEquals: return "OpNotEquals";
		case OpPlusEquals: return "OpPlusEquals";
		case OpMinusEquals: return "OpMinusEquals";
		case OpMulEquals: return "OpMulEquals";
		case OpDivEquals: return "OpDivEquals";
		case OpPostInc: return "OpPostInc";
		case OpPostDec: return "OpPostDec";
		case OpPreInc: return "OpPreInc";
		case OpPreDec: return "OpPreDec";
		case OpAnd: return "OpAnd";
		case OpOr: return "OpOr";
		case OpNot: return "OpNot";
	}
	return NULL;
}

LVariant::LVariant()
{
	Type = GV_NULL;
	ZeroObj(Value);
}

LVariant::LVariant(LVariant const &v)
{
	Type = GV_NULL;
	ZeroObj(Value);
	*this = v;
}

#if LVARIANT_SIZET
LVariant::LVariant(size_t i)
{
	Type = GV_NULL;
	*this = i;
}
#endif

#if LVARIANT_SSIZET
LVariant::LVariant(ssize_t i)
{
	Type = GV_NULL;
	*this = i;
}
#endif

LVariant::LVariant(int32_t i)
{
	Type = GV_INT32;
	Value.Int = i;
}

LVariant::LVariant(uint32_t i)
{
	Type = GV_INT32;
	Value.Int = i;
}

LVariant::LVariant(int64_t i)
{
	Type = GV_INT64;
	Value.Int64 = i;
}

LVariant::LVariant(uint64_t i)
{
	Type = GV_INT64;
	Value.Int64 = i;
}

LVariant::LVariant(double i)
{
	Type = GV_DOUBLE;
	Value.Dbl = i;
}

LVariant::LVariant(const char *s)
{
	Value.String = NewStr(s);
	Type = Value.String ? GV_STRING : GV_NULL;
}

LVariant::LVariant(const char16 *s)
{
	Value.WString = NewStrW(s);
	Type = Value.WString ? GV_WSTRING : GV_NULL;
}

LVariant::LVariant(void *p)
{
	Type = GV_NULL;
	*this = p;
}

LVariant::LVariant(LDom *p)
{
	Type = GV_NULL;
	*this = p;
}

LVariant::LVariant(LDom *p, char *name)
{
	Type = GV_NULL;
	SetDomRef(p, name);
}

LVariant::LVariant(const LDateTime *d)
{
	Type = GV_NULL;
	*this = d;
}

LVariant::LVariant(LOperator Op)
{
	Type = GV_OPERATOR;
	Value.Op = Op;
}

LVariant::~LVariant()
{
	Empty();
}

bool LVariant::operator ==(LVariant &v)
{
	switch (Type)
	{
		default:
		case GV_NULL:
			return v.Type == Type;
		case GV_INT32:
			return Value.Int == v.CastInt32();
		case GV_INT64:
			return Value.Int64 == v.CastInt64();
		case GV_BOOL:
			return Value.Bool == v.CastBool();
		case GV_DOUBLE:
			return Value.Dbl == v.CastDouble();
		case GV_STRING:
		{
			char *s = v.Str();
			if (Value.String && s)
				return !strcmp(Value.String, s);
			break;
		}
		case GV_WSTRING:
		{
			char16 *w = v.WStr();
			if (Value.WString && w)
				return !StrcmpW(Value.WString, w);
			break;
		}
		case GV_BINARY:
		{
			if (v.Type == Type &&
				Value.Binary.Data == v.Value.Binary.Data &&
				Value.Binary.Length == v.Value.Binary.Length)
			{
				return true;
			}
			break;
		}
		case GV_LIST:
		{
			if (!Value.Lst || !v.Value.Lst)
				return false;

			if (Value.Lst->Length() != v.Value.Lst->Length())
				return false;

			auto ValIt = Value.Lst->begin();
			auto VIt = v.Value.Lst->begin();
			LVariant *a, *b;
			while ( (a = *ValIt) && (b = *VIt) )
			{
				if (!(*a == *b))
					return false;
				
				ValIt++;
				VIt++;
			}
			return true;
		}
		case GV_DOMREF:
		{
			return Value.DomRef.Dom == v.Value.DomRef.Dom &&
				Value.DomRef.Name != 0 &&
				v.Value.DomRef.Name != 0 &&
				!stricmp(Value.DomRef.Name, v.Value.DomRef.Name);
		}
		case GV_DATETIME:
		{
			if (Value.Date && v.Value.Date)
			{
				return Value.Date->Compare(v.Value.Date) == 0;
			}
			break;
		}
		case GV_DOM:
			return Value.Dom == v.Value.Dom;
		case GV_OPERATOR:
			return Value.Op == v.Value.Op;
		case GV_CUSTOM:
			return Value.Custom == v.Value.Custom;
		case GV_LSURFACE:
			return Value.Surface.Ptr == v.Value.Surface.Ptr;
		case GV_GVIEW:
			return Value.View == v.Value.View;
		/*
		case GV_GFILE:
			return Value.File.Ptr == v.Value.File.Ptr;
		*/
		case GV_STREAM:
			return Value.Stream.Ptr == v.Value.Stream.Ptr;
		case GV_LMOUSE:
			return Value.Mouse == v.Value.Mouse;
		case GV_LKEY:
			return Value.Key == v.Value.Key;
		case GV_VOID_PTR:
			return Value.Ptr == v.Value.Ptr;
		case GV_HASHTABLE:
		{
			LAssert(0);
			break;
		}
	}

	return false;
}

LVariant &LVariant::operator =(const LDateTime *d)
{
	Empty();

	if (d)
	{
		Type = GV_DATETIME;
		Value.Date = new LDateTime;
		if (Value.Date)
		{
			*Value.Date = *d;
			// if (Dirty) *Dirty = true;
		}
	}

	return *this;
}

LVariant &LVariant::operator =(bool i)
{
	Empty();
	Type = GV_BOOL;
	Value.Bool = i;
	// if (Dirty) *Dirty = true;

	return *this;
}

#if LVARIANT_SIZET
LVariant &LVariant::operator =(size_t i)
{
	Empty();
	
	#if LGI_64BIT
	Type = GV_INT64;
	Value.Int64 = i;
	#else
	Type = GV_INT32;
	Value.Int = i;
	#endif
	
	return *this;
}
#endif

#if LVARIANT_SSIZET
LVariant &LVariant::operator =(ssize_t i)
{
	Empty();
	
	#if LGI_64BIT
	Type = GV_INT64;
	Value.Int64 = i;
	#else
	Type = GV_INT32;
	Value.Int = i;
	#endif
	
	return *this;
}
#endif

LVariant &LVariant::operator =(int32 i)
{
	Empty();
	Type = GV_INT32;
	Value.Int = i;
	return *this;
}

LVariant &LVariant::operator =(uint32_t i)
{
	Empty();
	Type = GV_INT32;
	Value.Int = i;
	return *this;
}

LVariant &LVariant::operator =(int64_t i)
{
	Empty();
	Type = GV_INT64;
	Value.Int64 = i;
	return *this;
}

LVariant &LVariant::operator =(uint64_t i)
{
	Empty();
	Type = GV_INT64;
	Value.Int64 = i;
	return *this;
}

LVariant &LVariant::operator =(double i)
{
	Empty();
	Type = GV_DOUBLE;
	Value.Dbl = i;
	// if (Dirty) *Dirty = true;

	return *this;
}

LVariant &LVariant::operator =(const char *s)
{
	Empty();
	if (s)
	{
		Type = GV_STRING;
		Value.String = NewStr(s);
	}

	return *this;
}

LVariant &LVariant::operator =(const char16 *s)
{
	Empty();
	if (s)
	{
		Type = GV_WSTRING;
		Value.WString = NewStrW(s);
	}
	// if (Dirty) *Dirty = true;

	return *this;
}

LVariant &LVariant::operator =(void *p)
{
	Empty();
	if (p)
	{
		Type = GV_VOID_PTR;
		Value.Ptr = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

LVariant &LVariant::operator =(LDom *p)
{
	Empty();
	if (p)
	{
		Type = GV_DOM;
		Value.Dom = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

LVariant &LVariant::operator =(LView *p)
{
	Empty();
	if (p)
	{
		Type = GV_GVIEW;
		Value.View = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

LVariant &LVariant::operator =(LMouse *p)
{
	Empty();
	if (p)
	{
		Type = GV_LMOUSE;
		Value.Mouse = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

LVariant &LVariant::operator =(LKey *p)
{
	Empty();
	if (p)
	{
		Type = GV_LKEY;
		Value.Key = p;
	}

	return *this;
}

LVariant &LVariant::operator =(LStream *s)
{
	Empty();
	if (s)
	{
		Type = GV_STREAM;
		Value.Stream.Ptr = s;
		Value.Stream.Own = false;
	}
	return *this;
}

LVariant &LVariant::operator =(LVariant const &i)
{
	if (&i == this)
		return *this;

	Empty();
	Type = i.Type;

	switch (Type)
	{
		case GV_NULL:
		{
			break;
		}
		case GV_INT32:
		{
			Value.Int = i.Value.Int;
			break;
		}
		case GV_BOOL:
		{
			Value.Bool = i.Value.Bool;
			break;
		}
		case GV_INT64:
		{
			Value.Int64 = i.Value.Int64;
			break;
		}
		case GV_DOUBLE:
		{
			Value.Dbl = i.Value.Dbl;
			break;
		}
		case GV_STRING:
		{
			Value.String = NewStr(((LVariant&)i).Str());
			break;
		}
		case GV_WSTRING:
		{
			Value.WString = NewStrW(i.Value.WString);
			break;
		}
		case GV_BINARY:
		{
			SetBinary(i.Value.Binary.Length, i.Value.Binary.Data);
			break;
		}
		case GV_LIST:
		{
			SetList(i.Value.Lst);
			break;
		}
		case GV_DOM:
		{
			Value.Dom = i.Value.Dom;
			break;
		}
		case GV_VOID_PTR:
		case GV_GVIEW:
		case GV_LMOUSE:
		case GV_LKEY:
		{
			Value.Ptr = i.Value.Ptr;
			break;
		}
		case GV_DATETIME:
		{
			if (i.Value.Date)
			{
				Value.Date = new LDateTime;
				if (Value.Date)
				{
					*Value.Date = *i.Value.Date;
				}
			}
			break;
		}
		case GV_HASHTABLE:
		{
			if ((Value.Hash = new LHash))
			{
				if (i.Value.Hash)
				{
					// const char *k;
					// for (LVariant *var = i.Value.Hash->First(&k); var; var = i.Value.Hash->Next(&k))
					for (auto it : *i.Value.Hash)
					{
						Value.Hash->Add(it.key, new LVariant(*it.value));
					}
				}
			}
			break;
		}
		case GV_CUSTOM:
		{
			Value.Custom.Data = i.Value.Custom.Data;
			Value.Custom.Dom = i.Value.Custom.Dom;
			break;
		}
		case GV_LSURFACE:
		{
			Value.Surface = i.Value.Surface;
			if (Value.Surface.Own &&
				Value.Surface.Ptr)
				Value.Surface.Ptr->IncRef();
			break;
		}
		/*
		case GV_GFILE:
		{
			Value.File = i.Value.File;
			if (Value.File.Own &&
				Value.File.Ptr)
				Value.File.Ptr->AddRef();
			break;
		}
		*/
		case GV_STREAM:
		{
			Value.Stream.Ptr = i.Value.Stream.Ptr;
			Value.Stream.Own = false;			
			break;
		}
		default:
		{
			printf("%s:%i - Unknown variant type '%i'\n", _FL, Type);
			LAssert(0);
			break;
		}
	}

	// if (Dirty) *Dirty = true;

	return *this;
}

bool LVariant::SetDomRef(LDom *obj, char *name)
{
	Empty();

	Type = GV_DOMREF;
	Value.DomRef.Dom = obj;
	Value.DomRef.Name = NewStr(name);

	return Value.DomRef.Name != 0;
}

bool LVariant::SetBinary(ssize_t Len, void *Data, bool Own)
{
	bool Status = false;

	Empty();

	Type = GV_BINARY;
	Value.Binary.Length = Len;
	if (Own)
	{
		Value.Binary.Data = Data;
		Status = true;
	}
	else
	{
		if ((Value.Binary.Data = new uchar[Value.Binary.Length]))
		{
			if (Data)
				memcpy(Value.Binary.Data, Data, Value.Binary.Length);
			else
				memset(Value.Binary.Data, 0, Value.Binary.Length);

			Status = true;
		}
	}

	return Status;
}

List<LVariant> *LVariant::SetList(List<LVariant> *Lst)
{
	Empty();
	Type = GV_LIST;

	if ((Value.Lst = new List<LVariant>) && Lst)
	{
		for (auto s: *Lst)
		{
			LVariant *New = new LVariant;
			if (New)
			{
				*New = *s;
				Value.Lst->Insert(New);
			}
		}
	}

	return Value.Lst;
}

bool LVariant::SetHashTable(LHash *Table, bool Copy)
{
	Empty();
	Type = GV_HASHTABLE;

	if (Copy && Table)
	{
		if ((Value.Hash = new LHash))
		{
			// const char *k;
			// for (LVariant *p = Table->First(&k); p; p = Table->Next(&k))
			for (auto i : *Table)
			{
				Value.Hash->Add(i.key, i.value);
			}
		}
	}
	else
	{
		Value.Hash = Table ? Table : new LHash;
	}

	return Value.Hash != 0;
}

bool LVariant::SetSurface(class LSurface *Ptr, bool Own)
{
    Empty();
    if (!Ptr)
		return false;

    Type = GV_LSURFACE;
    Value.Surface.Ptr = Ptr;
    if ((Value.Surface.Own = Own))
        Value.Surface.Ptr->IncRef();
    
    return true;
}

bool LVariant::SetStream(class LStreamI *Ptr, bool Own)
{
    Empty();
    if (!Ptr)
		return false;

    Type = GV_STREAM;
    Value.Stream.Ptr = Ptr;
    Value.Stream.Own = Own;
    
    return true;
}

bool LVariant::OwnStr(char *s)
{
	Empty();
	if (!s)
		return false;

	Type = GV_STRING;
	Value.String = s;
	return true;
}

bool LVariant::OwnStr(char16 *w)
{
	Empty();
	if (!w)
		return false;

	Type = GV_WSTRING;
	Value.WString = w;
	return true;
}

char *LVariant::ReleaseStr()
{
	char *Ret = Str();
	if (Ret)
	{
		Value.String = 0;
		Type = GV_NULL;
	}
	return Ret;
}

LString LVariant::LStr()
{
	return Str();
}

char *LVariant::Str()
{
	if (Type == GV_STRING)
		return Value.String;

	if (Type == GV_WSTRING)
	{
		char *u = WideToUtf8(Value.WString);
		DeleteArray(Value.WString);
		Type = GV_STRING;
		return Value.String = u;
	}

	return 0;
}

char16 *LVariant::ReleaseWStr()
{
	char16 *Ret = WStr();
	if (Ret)
	{
		Value.WString = 0;
		Type = GV_NULL;
	}
	return Ret;
}

char16 *LVariant::WStr()
{
	if (Type == GV_WSTRING)
		return Value.WString;

	if (Type == GV_STRING)
	{
		char16 *w = Utf8ToWide(Value.String);
		DeleteArray(Value.String);
		Type = GV_WSTRING;
		return Value.WString = w;
	}

	return 0;
}

void LVariant::Empty()
{
	switch (Type)
	{
		default:
			break;
		case GV_CUSTOM:
		{
			Value.Custom.Data = 0;
			Value.Custom.Dom = 0;
			break;
		}
		case GV_DOMREF:
		{
			DeleteArray(Value.DomRef.Name);
			Value.DomRef.Dom = 0;
			break;
		}
		case GV_STRING:
		{
			DeleteArray(Value.String);
			break;
		}
		case GV_WSTRING:
		{
			DeleteArray(Value.WString);
			break;
		}
		case GV_BINARY:
		{
			char *d = (char*) Value.Binary.Data;
			DeleteArray(d);
			Value.Binary.Data = 0;
			break;
		}
		case GV_DATETIME:
		{
			DeleteObj(Value.Date);
			break;
		}
		case GV_LIST:
		{
			if (Value.Lst)
			{
				Value.Lst->DeleteObjects();
				DeleteObj(Value.Lst);
			}
			break;
		}
		case GV_HASHTABLE:
		{
			if (Value.Hash)
			{
				// for (LVariant *v = (LVariant*) Value.Hash->First(); v; v = (LVariant*) Value.Hash->Next())
				for (auto i : *Value.Hash)
				{
					DeleteObj(i.value);
				}
				DeleteObj(Value.Hash);
			}
			break;
		}
		case GV_LSURFACE:
		{
		    if (Value.Surface.Own &&
		        Value.Surface.Ptr)
		    {
				Value.Surface.Ptr->DecRef();
				Value.Surface.Ptr = NULL;
		    }
		    break;
		}
		/*
		case GV_GFILE:
		{
			if (Value.File.Ptr &&
				Value.File.Own)
			{
				Value.File.Ptr->DecRef();
				Value.File.Ptr = NULL;
			}
			break;
		}
		*/
		case GV_STREAM:
		{
			if (Value.Stream.Ptr)
			{
				if (Value.Stream.Own)
					delete Value.Stream.Ptr;
				Value.Stream.Ptr = NULL;
			}
			break;
		}
	}

	Type = GV_NULL;
	ZeroObj(Value);
}

int64 LVariant::Length()
{
	switch (Type)
	{
		case GV_INT32:
			return sizeof(Value.Int);
		case GV_INT64:
			return sizeof(Value.Int64);
		case GV_BOOL:
			return sizeof(Value.Bool);
		case GV_DOUBLE:
			return sizeof(Value.Dbl);
		case GV_STRING:
			return Value.String ? strlen(Value.String) : 0;
		case GV_BINARY:
			return Value.Binary.Length;
		case GV_LIST:
		{
			int64 Sz = 0;
			if (Value.Lst)
			{
				for (auto v : *Value.Lst)
					Sz += v->Length();
			}
			return Sz;
		}
		case GV_DOM:
		{
			LVariant v;
			if (Value.Dom)
				Value.Dom->GetValue("length", v);
			return v.CastInt32();
		}
		case GV_DOMREF:
			break;
		case GV_VOID_PTR:
			return sizeof(Value.Ptr);
		case GV_DATETIME:
			return sizeof(*Value.Date);
		case GV_HASHTABLE:
		{
			int64 Sz = 0;
			if (Value.Hash)
			{
				// for (LVariant *v=Value.Hash->First(); v; v=Value.Hash->Next())
				for (auto i : *Value.Hash)
					Sz += i.value->Length();
			}
			return Sz;
		}
		case GV_OPERATOR:
			return sizeof(Value.Op);
		case GV_CUSTOM:
			break;
		case GV_WSTRING:
			return Value.WString ? StrlenW(Value.WString) * sizeof(char16) : 0;
		case GV_LSURFACE:
		{
			int64 Sz = 0;
			if (Value.Surface.Ptr)
			{
				LRect r = Value.Surface.Ptr->Bounds();
				int Bytes = Value.Surface.Ptr->GetBits() >> 3;
				Sz = r.X() * r.Y() * Bytes;
			}
			return Sz;
		}
		case GV_GVIEW:
			return sizeof(LView);
		case GV_LMOUSE:
			return sizeof(LMouse);
		case GV_LKEY:
			return sizeof(LKey);
		case GV_STREAM:
			return Value.Stream.Ptr->GetSize();
		default: break;
	}
	
	return 0;
}

bool LVariant::IsInt()
{
	return Type == GV_INT32 || Type == GV_INT64;
}

bool LVariant::IsBool()
{
	return Type == GV_BOOL;
}

bool LVariant::IsDouble()
{
	return Type == GV_DOUBLE;
}

bool LVariant::IsString()
{
	return Type == GV_STRING;
}

bool LVariant::IsBinary()
{
	return Type == GV_BINARY;
}

bool LVariant::IsNull()
{
	return Type == GV_NULL;
}

#define IsList() (Type == GV_LIST && Value.Lst)

LVariant &LVariant::Cast(LVariantType NewType)
{
	if (NewType != Type)
	{
		switch (NewType)
		{
			default:
			{
				// No conversion possible
				break;
			}
			case GV_INT32:
			{
				*this = (int)CastInt32();
				break;
			}
			case GV_INT64:
			{
				*this = (int64_t) CastInt64();
				break;
			}
			case GV_BOOL:
			{
				if (Type == GV_DOUBLE)
				{
					*this = Value.Dbl != 0.0;
				}
				else
				{
					*this = CastInt32() != 0;
				}
				break;
			}
			case GV_DOUBLE:
			{
				*this = CastDouble();
				break;
			}
			case GV_STRING:
			{
				*this = CastString();
				break;
			}
			case GV_DATETIME:
			{
				switch (Type)
				{
					case GV_STRING:
					{
						// String -> LDateTime
						LDateTime *Dt = new LDateTime;
						if (Dt)
						{
							Dt->Set(Value.String);
							Empty();
							Value.Date = Dt;
							Type = NewType;
						}
						break;
					}
					case GV_INT64:
					{
						// Int64 (system date) -> LDateTime
						LDateTime *Dt = new LDateTime;
						if (Dt)
						{
							Dt->Set((uint64_t)Value.Int64);
							Empty();
							Value.Date = Dt;
							Type = NewType;
						}
						break;
					}
					default:
					{
						// No conversion available
						break;
					}
				}
				break;
			}
		}
	}

	return *this;
}

void *LVariant::CastVoidPtr() const
{
	switch (Type)
	{
		default:
			break;
		case GV_STRING:
			return Value.String;
		case GV_BINARY:
			return Value.Binary.Data;
		case GV_LIST:
			return Value.Lst;
		case GV_DOM:
			return Value.Dom;
		case GV_DOMREF:
			return Value.DomRef.Dom;
		case GV_VOID_PTR:
			return Value.Ptr;
		case GV_DATETIME:
			return Value.Date;
		case GV_HASHTABLE:
			return Value.Hash;
		case GV_CUSTOM:
			return Value.Custom.Data;
		case GV_WSTRING:
			return Value.WString;
		case GV_LSURFACE:
			return Value.Surface.Ptr;
		case GV_GVIEW:
			return Value.View;
		case GV_LMOUSE:
			return Value.Mouse;
		case GV_LKEY:
			return Value.Key;
	}

	return 0;
}

LDom *LVariant::CastDom() const
{
	switch (Type)
	{
		default:
			break;
		case GV_DOM:
			return Value.Dom;
		case GV_DOMREF:
			return Value.DomRef.Dom;
		case GV_STREAM:
			return dynamic_cast<LDom*>(Value.Stream.Ptr);
		case GV_LSURFACE:
			return Value.Surface.Ptr;
		case GV_CUSTOM:
			return Value.Custom.Dom;
	}

	return NULL;
}

bool LVariant::CastBool() const
{
	switch (Type)
	{
		default:
			LAssert(0);
			break;
		case GV_NULL:
			return false;
		case GV_INT32:
			return Value.Int != 0;
		case GV_INT64:
			return Value.Int64 != 0;
		case GV_BOOL:
			return Value.Bool;
		case GV_DOUBLE:
			return Value.Dbl != 0.0;
		case GV_BINARY:
			return Value.Binary.Data != NULL;
		case GV_LIST:
			return Value.Lst != NULL;
		case GV_DOM:
			return Value.Dom != NULL;
		case GV_DOMREF:
			return Value.DomRef.Dom != NULL;
		case GV_VOID_PTR:
			return Value.Ptr != NULL;
		case GV_GVIEW:
			return Value.View != NULL;
		case GV_LMOUSE:
			return Value.Mouse != NULL;
		case GV_LKEY:
			return Value.Key != NULL;
		case GV_DATETIME:
			return Value.Date != NULL;
		case GV_HASHTABLE:
			return Value.Hash != NULL;
		case GV_OPERATOR:
			return Value.Op != OpNull;
		case GV_CUSTOM:
			return Value.Custom.Dom != 0 && Value.Custom.Data != 0;
		/*
		case GV_GFILE:
			return Value.File.Ptr != NULL;
		*/
		case GV_STREAM:
			return Value.Stream.Ptr != NULL;

		// As far as I understand this is the behavour in Python, which I'm using for
		// a reference to what the "correct" thing to do here is. Basically it's treating
		// the string like a pointer instead of a value. If the pointer is valid the
		// conversion to bool return true, and false if it's not a valid pointer. This
		// means things like if (!StrinLVariant) evaluate correctly in the scripting engine
		// but it means that if you want to evaluate the value of the varient you should
		// use CastInt32 instead.
		case GV_STRING:
			return ValidStr(Value.String);
		case GV_WSTRING:
			return ValidStrW(Value.WString);
	}

	return false;
}

double LVariant::CastDouble() const
{
	switch (Type)
	{
		default:
			break;
		case GV_BOOL:
			return Value.Bool ? 1.0 : 0.0;
		case GV_DOUBLE:
			return Value.Dbl;
		case GV_INT32:
			return (double)Value.Int;
		case GV_INT64:
			return (double)Value.Int64;
		case GV_STRING:
			return Value.String ? atof(Value.String) : 0;
		case GV_DOMREF:
		{
			static LVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastDouble();
				}
			}
			break;
		}
	}
	
	return 0;
}

int32 LVariant::CastInt32() const
{
	switch (Type)
	{
		default:
			break;
		case GV_BOOL:
			return (int32)Value.Bool;
		case GV_DOUBLE:
			return (int32)Value.Dbl;
		case GV_INT32:
			return Value.Int;
		case GV_INT64:
			return (int32)Value.Int64;
		case GV_STRING:
			if (!Value.String)
				return 0;
			
			if (IsAlpha(Value.String[0]))
				return !Stricmp(Value.String, "true");
			else if (Value.String[0] == '0' && tolower(Value.String[1]) == 'x')
				return static_cast<int32>(Atoi(Value.String, 16));

			return atoi(Value.String);
		case GV_DOM:
			return Value.Dom != 0;
		case GV_DOMREF:
		{
			static LVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastInt32();
				}
			}
			break;
		}
		case GV_LIST:
			return Value.Lst != NULL;
		case GV_HASHTABLE:
			return Value.Hash != NULL;
		case GV_LSURFACE:
			return Value.Surface.Ptr != NULL;
		case GV_GVIEW:
			return Value.View != NULL;
		case GV_LMOUSE:
			return Value.Mouse != NULL;
		case GV_LKEY:
			return Value.Key != NULL;
		case GV_STREAM:
			return Value.Stream.Ptr != NULL;
	}
	
	return 0;
}

int64 LVariant::CastInt64() const
{
	switch (Type)
	{
		default:
			break;
		case GV_BOOL:
			return (int64)Value.Bool;
		case GV_DOUBLE:
			return (int64)Value.Dbl;
		case GV_INT32:
			return Value.Int;
		case GV_INT64:
			return Value.Int64;
		case GV_STRING:
		{
			if (!Value.String)
				return 0;

			if (IsAlpha(Value.String[0]))
				return !Stricmp(Value.String, "true");
			else if (Value.String[0] == '0' && tolower(Value.String[1]) == 'x')
				return Atoi(Value.String, 16);

			return Atoi(Value.String);
		}
		case GV_DOMREF:
		{
			static LVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastInt64();
				}
			}
			break;
		}
	}
	
	return 0;
}

char *LVariant::CastString()
{
	char i[40];

	switch (Type)
	{
		case GV_LIST:
		{
			LStringPipe p(256);
			
			List<LVariant>::I it = Value.Lst->begin();
			bool First = true;
			
			p.Print("{");
			for (LVariant *v = *it; v; v = *++it)
			{
				if (v->Type == GV_STRING ||
					v->Type == GV_WSTRING)
					p.Print("%s\"%s\"", First ? "" : ", ", v->CastString());
				else
					p.Print("%s%s", First ? "" : ", ", v->CastString());
				First = false;
			}
			p.Print("}");
			OwnStr(p.NewStr());
			return Str();
			break;
		}
		case GV_HASHTABLE:
		{
			LStringPipe p(256);

			p.Print("{");
			
			bool First = true;
			// const char *k;
			// for (LVariant *v = Value.Hash->First(&k); v; v = Value.Hash->Next(&k))
			for (auto i : *Value.Hash)
			{
				p.Print("%s%s = %s", First ? "" : ", ", i.key, i.value->CastString());
				First = false;
			}
			p.Print("}");
			OwnStr(p.NewStr());
			return Str();
			break;
		}
		case GV_DOMREF:
		{
			static LVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastString();
				}
			}
			break;
		}
		case GV_INT32:
		{
			sprintf_s(i, sizeof(i), "%i", Value.Int);
			*this = i;
			return Str();
		}
		case GV_DOUBLE:
		{
			sprintf_s(i, sizeof(i), "%f", Value.Dbl);
			*this = i;
			return Str();
		}
		case GV_BOOL:
		{
			sprintf_s(i, sizeof(i), "%i", Value.Bool);
			*this = i;
			return Str();
		}
		case GV_INT64:
		{
			sprintf_s(i, sizeof(i), LPrintfInt64, Value.Int64);
			*this = i;
			return Str();
		}
		case GV_STRING:
		case GV_WSTRING:
		{
			return Str();
		}
		case GV_DATETIME:
		{
			if (Value.Date)
			{
				char s[64];
				Value.Date->Get(s, sizeof(s));
				*this = s;
				return Str();
			}
			break;
		}
		case GV_DOM:
		{
			sprintf_s(i, sizeof(i), "dom:%p", Value.Dom);
			*this = i;
			break;
		}
		default:
		{
			break;
		}
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
LDom *LDom::ResolveObject(const char *Var, LString &Name, LString &Array)
{
	LDom *Object = this;

	try
	{
		// Tokenize the string
		LString::Array t;
		for (auto *s = Var; s && *s; )
		{
			const char *e = s;
			while (*e && *e != '.')
			{
				if (*e == '[')
				{
					e++;
					while (*e && *e != ']')
					{
						if (*e == '\"' || *e == '\'')
						{
							char d = *e++;
							while (*e && *e != d)
								e++;
							if (*e == d) e++;
						}
						else e++;
					}

					if (*e == ']')
						e++;
				}
				else e++;
			}
		
			LString part = LString(s, e - s).Strip();
			if (part.Length() > 0)
				t.New() = part; // Store non-empty part

			s = *e ? e + 1 : e;
		}

		// Process elements
		for (int i=0; i<t.Length(); i++)
		{
			// Parse out object name and array parts
			LString Index;
			char *Obj = t[i];
			if (*Obj == '[')
			{
				Object = NULL;
				break;
			}
		
			char *ArrStart = strchr(Obj, '[');
			if (ArrStart)
			{
				*ArrStart++ = 0;
				char *ArrEnd = 0;
				if (strchr("\'\"", *ArrStart))
				{
					ArrEnd = strchr(ArrStart + 1, ArrStart[0]);
					ArrStart++;
				}
				else
				{
					ArrEnd = strchr(ArrStart, ']');
				}
				if (ArrEnd)
				{
					*ArrEnd = 0;
					Index = ArrStart;
				}
			}

			// Process parts
			if (i == t.Length()-1)
			{
				// Output the variable and array index
				Name = LString(Obj).Strip();
				Array = Index;
			}
			else
			{
				LVariant v;
				if (!Index.IsEmpty())
				{
					if (Object->GetVariant(Obj, v, Index))
					{
						if (v.Type == GV_LIST)
						{
							int N = atoi(Index);
							LVariant *Element = v.Value.Lst->ItemAt(N);
							if (Element && Element->Type == GV_DOM)
							{
								Object = Element->Value.Dom;
							}
							else
							{
								return NULL;
							}
						}
						else if (v.Type == GV_DOM)
						{
							Object = v.Value.Dom;
						}
						else
						{
							return NULL;
						}
					}
					else
					{
						return NULL;
					}
				}
				else
				{
					if (Object->GetVariant(Obj, v) &&
						v.Type == GV_DOM)
					{
						Object = v.Value.Dom;
					}
					else
					{
						return NULL;
					}
				}
			}
		}
	}
	catch (...)
	{
		LgiTrace("LDom::ResolveObject crashed: '%s'\n", Var);
		return NULL;
	}

	return Object;
}

struct LDomPropMap
{
	LHashTbl<ConstStrKey<char,false>, LDomProperty> ToProp;
	LHashTbl<IntKey<LDomProperty,ObjNone>, const char *> ToString;

	LDomPropMap()
	{
		#undef _
		#define _(symbol, txt) Define(txt, symbol);
		#include "lgi/common/DomFields.h"
		#undef _
	}
	
	void Define(const char *s, LDomProperty p)
	{
		if (!s)
			return;

		#if defined(_DEBUG) // Check for duplicates.
		auto existing_prop = ToProp.Find(s);
		LAssert(existing_prop == ObjNone);
		auto existing_str = ToString.Find(p);
		LAssert(existing_str == NULL);
		#endif

		ToProp.Add(s, p);
		ToString.Add(p, s);
	}
	
} DomPropMap;

LDomProperty LStringToDomProp(const char *Str)
{
	return DomPropMap.ToProp.Find(Str);
}

const char *LDomPropToString(LDomProperty Prop)
{
	return DomPropMap.ToString.Find(Prop);
}

bool LDom::GetValue(const char *Var, LVariant &Value)
{
	if (!Var)
		return false;

	if (!_OnAccess(true))
	{
		LgiTrace("%s:%i - Locking error\n", _FL);
		LAssert(0);
		return false;
	}

	bool Status = false;

	LString Name, Arr;
	LDom *Object = ResolveObject(Var, Name, Arr);
	if (Object)
	{
		if (Name.IsEmpty())
			LgiTrace("%s:%i - Warning name parse failed for '%s'\n", _FL, Var);
		else
			Status = Object->GetVariant(Name, Value, Arr.IsEmpty() ? NULL : Arr);
	}

	_OnAccess(false);

	return Status;
}

bool LDom::SetValue(const char *Var, LVariant &Value)
{
	bool Status = false;

	if (Var)
	{
		// LMutex *Sem = dynamic_cast<LMutex*>(this);
		if (_OnAccess(true))
		{
			LString Name, Arr;
			LDom *Object = ResolveObject(Var, Name, Arr);
			if (Object)
			{
				if (Name.IsEmpty())
					LgiTrace("%s:%i - Warning name parse failed for '%s'\n", _FL, Var);
				else
					Status = Object->SetVariant(Name, Value, Arr.IsEmpty() ? NULL : Arr);
			}

			_OnAccess(false);
		}
		else
		{
			LgiTrace("%s:%i - Locking error\n", _FL);
			LAssert(0);
		}
	}

	return Status;
}

bool LVariant::Add(LVariant *v, int Where)
{
	if (!v)
	{
		LAssert(!"No value to insert.");
		return false;
	}

	if (Type == GV_NULL)
		SetList();
		
	if (Type != GV_LIST)
	{
		LAssert(!"Not a list variant");
		return false;
	}
	
	return Value.Lst->Insert(v, Where);
}

LString LVariant::ToString()
{
	LString s;
	switch (Type)
	{
		case GV_NULL:
			s = "NULL";
			break;
		case GV_INT32:
			s.Printf("(int)%i", Value.Int);
			break;
		case GV_INT64:
			s.Printf("(int64)" LPrintfInt64, Value.Int64);
			break;
		case GV_BOOL:
			s.Printf("(bool)%s", Value.Bool ? "true" : "false");
			break;
		case GV_DOUBLE:
			s.Printf("(double)%f", Value.Dbl);
			break;
		case GV_STRING:
			s.Printf("(string)\"%s\"", Value.String);
			break;
		case GV_BINARY:
			s.Printf("(binary[%i])%p", Value.Binary.Length, Value.Binary.Data);
			break;
		case GV_LIST:
			s.Printf("(list[%i])%p", Value.Lst?Value.Lst->Length():0, Value.Lst);
			break;
		case GV_DOM:
			s.Printf("(dom)%p", Value.Dom);
			break;
		case GV_DOMREF:
			s.Printf("(dom)%p.%s", Value.DomRef.Dom, Value.DomRef.Name);
			break;
		case GV_VOID_PTR:
			s.Printf("(void*)%p", Value.Ptr);
			break;
		case GV_DATETIME:
		{
			char dt[64];
			Value.Date->Get(dt, sizeof(dt));
			s.Printf("(datetime)%s", dt);
			break;
		}
		case GV_HASHTABLE:
			s.Printf("(hashtbl)%p", Value.Hash);
			break;
		case GV_OPERATOR:
			s.Printf("(operator)%s", OperatorToString(Value.Op));
			break;
		case GV_CUSTOM:
			s.Printf("(custom.%s)%p", Value.Custom.Dom->GetName(), Value.Custom.Data);
			break;
		case GV_WSTRING:
			s.Printf("(wstring)\"%S\"", Value.WString);
			break;
		case GV_LSURFACE:
			s.Printf("(gsurface)%p", Value.Surface.Ptr);
			break;
		case GV_GVIEW:
			s.Printf("(gview)%p", Value.View);
			break;
		case GV_LMOUSE:
			s.Printf("(gmouse)%p", Value.Mouse);
			break;
		case GV_LKEY:
			s.Printf("(gkey)%p", Value.Key);
			break;
		case GV_STREAM:
			s.Printf("(stream)%p", Value.Stream.Ptr);
			break;
		default:
			s = "(unknown)NULL";
			break;
	}
	return s;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
LCustomType::LCustomType(const char *name, int pack) : FldMap(0, -1)
{
	Name = name;
	Pack = 1;
	Size = 0;
}

LCustomType::LCustomType(const char16 *name, int pack) : FldMap(0, -1)
{
	Name = name;
	Pack = 1;
	Size = 0;
}

LCustomType::~LCustomType()
{
	Flds.DeleteObjects();
	Methods.DeleteObjects();
}

size_t LCustomType::Sizeof()
{
	return (size_t)PadSize();
}

ssize_t LCustomType::PadSize()
{
	if (Pack > 1)
	{
		// Bump size to the pack boundary...
		int Remain = Size % Pack;
		if (Remain)
			return Size + Pack - Remain;
	}
	return Size;
}

int LCustomType::IndexOf(const char *Field)
{
	return FldMap.Find(Field);
}

int LCustomType::AddressOf(const char *Field)
{
	if (!Field)
		return -1;
	for (unsigned i=0; i<Flds.Length(); i++)
	{
		if (!strcmp(Flds[i]->Name, Field))
			return (int)i;
	}
	return -1;
}

bool LCustomType::DefineField(const char *Name, LCustomType *Type, int ArrayLen)
{
	if (ArrayLen < 1)
	{
		LAssert(!"Can't have zero size field.");
		return false;
	}

	if (Name == NULL || Type == NULL)
	{
		LAssert(!"Invalid parameter.");
		return false;
	}

	if (FldMap.Find(Name) >= 0)
	{
		LAssert(!"Field already exists.");
		return false;
	}
	FldMap.Add(Name, (int)Flds.Length());

	CustomField *Def;
	Flds.Add(Def = new CustomField);
	
	Size = PadSize();
	Def->Offset = Size;
	
	Def->Name = Name;
	Def->Type = GV_CUSTOM;
	Def->Bytes = Type->Sizeof();
	Def->ArrayLen = ArrayLen;
	Size += Def->Bytes * ArrayLen;

	return true;
}

bool LCustomType::DefineField(const char *Name, LVariantType Type, int Bytes, int ArrayLen)
{
	if (ArrayLen < 1)
	{
		LAssert(!"Can't have zero size field.");
		return false;
	}

	if (Name == NULL)
	{
		LAssert(!"No field name.");
		return false;
	}

	if (FldMap.Find(Name) >= 0)
	{
		LAssert(!"Field already exists.");
		return false;
	}
	FldMap.Add(Name, (int)Flds.Length());

	CustomField *Def;
	Flds.Add(Def = new CustomField);
	
	Size = PadSize();
	Def->Offset = Size;
	
	Def->Name = Name;
	Def->Type = Type;
	Def->Bytes = Bytes;
	Def->ArrayLen = ArrayLen;
	Size += Bytes * ArrayLen;

	return true;
}

LCustomType::Method *LCustomType::GetMethod(const char *Name)
{
	return MethodMap.Find(Name);
}

LCustomType::Method *LCustomType::DefineMethod(const char *Name, LArray<LString> &Params, size_t Address)
{
	Method *m = MethodMap.Find(Name);
	if (m)
	{
		LAssert(!"Method already defined.");
		return NULL;
	}
	
	Methods.Add(m = new Method);
	m->Name = Name;
	m->Params = Params;
	m->Address = Address;
	
	MethodMap.Add(Name, m);	
	
	return m;
}

bool LCustomType::CustomField::GetVariant(const char *Field, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Field);
	switch (p)
	{
		case ObjName: // Type: String
			Value = Name;
			break;
		case ObjLength: // Type: Int32
			Value = Bytes;
			break;
		default:
			return false;
	}
	
	return true;
}

ssize_t LCustomType::CustomField::Sizeof()
{
	switch (Type)
	{
		case GV_INT32:
			return sizeof(int32);
		case GV_INT64:
			return sizeof(int64);
		case GV_BOOL:
			return sizeof(bool);
		case GV_DOUBLE:
			return sizeof(double);
		case GV_STRING:
			return sizeof(char);
		case GV_DATETIME:
			return sizeof(LDateTime);
		case GV_HASHTABLE:
			return sizeof(LVariant::LHash);
		case GV_OPERATOR:
			return sizeof(LOperator);
		case GV_LMOUSE:
			return sizeof(LMouse);
		case GV_LKEY:
			return sizeof(LKey);
		case GV_CUSTOM:
			return Nested->Sizeof();
		default:
			LAssert(!"Unknown type.");
			break;
	}
	
	return 0;
}

bool LCustomType::Get(int Index, LVariant &Out, uint8_t *This, int ArrayIndex)
{
	if (Index < 0 ||
		Index >= Flds.Length() ||
		!This)
	{
		LAssert(!"Invalid parameter error.");
		return false;
	}

	CustomField *Def = Flds[Index];
	if (ArrayIndex < 0 || ArrayIndex >= Def->ArrayLen)
	{
		LAssert(!"Array out of bounds.");
		return false;
	}

	uint8_t *Ptr = This + Def->Offset;
	Out.Empty();
	
	switch (Def->Type)
	{
		case GV_STRING:
		{
			int Len;
			for (Len = 0; Ptr[Len] && Len < Def->ArrayLen-1; Len++)
				;
			Out.OwnStr(NewStr((char*)Ptr, Len));
			break;
		}
		case GV_WSTRING:
		{
			char16 *p = (char16*)Ptr;
			int Len;
			for (Len = 0; p[Len] && Len < Def->ArrayLen-1; Len++)
				;
			Out.OwnStr(NewStrW(p, Len));
			break;
		}
		case GV_INT32:
		case GV_INT64:
		{
			switch (Def->Bytes)
			{
				case 1:
				{
					Out.Value.Int = Ptr[ArrayIndex];
					Out.Type = GV_INT32;
					break;
				}
				case 2:
				{
					Out.Value.Int = ((uint16*)Ptr)[ArrayIndex];
					Out.Type = GV_INT32;
					break;
				}
				case 4:
				{
					Out.Value.Int = ((uint32_t*)Ptr)[ArrayIndex];
					Out.Type = GV_INT32;
					break;
				}
				case 8:
				{
					Out.Value.Int64 = ((uint64*)Ptr)[ArrayIndex];
					Out.Type = GV_INT64;
					break;
				}
				default:
				{
					LAssert(!"Unknown integer size.");
					return false;
				}
			}
			break;			
		}
		case GV_MAX:
		{
			Out = *((LVariant*)Ptr);
			break;
		}
		default:
		{
			LAssert(!"Impl this type.");
			return false;
		}
	}

	return true;
}

bool LCustomType::Set(int Index, LVariant &In, uint8_t *This, int ArrayIndex)
{
	if (Index < 0 ||
		Index >= Flds.Length() ||
		!This)
	{
		LAssert(!"Invalid parameter error.");
		return false;
	}

	CustomField *Def = Flds[Index];
	uint8_t *Ptr = This + Def->Offset;
	if (ArrayIndex < 0 || ArrayIndex >= Def->ArrayLen)
	{
		LAssert(!"Array out of bounds.");
		return false;
	}
	
	switch (Def->Type)
	{
		case GV_STRING:
		{
			char *s = In.Str();
			if (!s)
			{
				*Ptr = 0;
				break;
			}
			
			if (Def->Bytes == 1)
			{
				// Straight up string copy...
				if (s)
					strcpy_s((char*)Ptr, Def->ArrayLen, s);
				else
					*Ptr = 0;
			}
			else if (Def->Bytes == sizeof(char16))
			{
				// utf8 -> wide conversion...
				const void *In = Ptr;
				ssize_t Len = strlen(s);
				ssize_t Ch = LBufConvertCp(Ptr, LGI_WideCharset, Def->ArrayLen-1, In, "utf-8", Len);
				if (Ch >= 0)
				{
					// Null terminate
					Ptr[Ch] = 0;
				}
				else
				{
					LAssert(!"LBufConvertCp failed.");
					return false;
				}
			}
			break;
		}
		case GV_WSTRING:
		{
			char16 *p = (char16*)Ptr;
			char16 *w = In.WStr();
			if (!w)
			{
				*p = 0;
				break;
			}
			if (Def->Bytes == sizeof(char16))
			{
				// Straight string copy...
				Strcpy(p, Def->ArrayLen, w);
			}
			else
			{
				// Conversion to utf-8
				const void *In = Ptr;
				ssize_t Len = StrlenW(w) * sizeof(char16);
				ssize_t Ch = LBufConvertCp(Ptr, "utf-8", Def->ArrayLen-sizeof(char16),
										In, LGI_WideCharset, Len);
				if (Ch >= 0)
				{
					// Null terminate
					p[Ch/sizeof(char16)] = 0;
				}
				else
				{
					LAssert(!"LBufConvertCp failed.");
					return false;
				}
			}
			break;
		}
		case GV_INT32:
		case GV_INT64:
		{
			switch (Def->Bytes)
			{
				case 1:
				{
					Ptr[ArrayIndex] = In.CastInt32();
					break;
				}
				case 2:
				{
					((uint16*)Ptr)[ArrayIndex] = In.CastInt32();
					break;
				}
				case 4:
				{
					((uint32_t*)Ptr)[ArrayIndex] = In.CastInt32();
					break;
				}
				case 8:
				{
					((uint64*)Ptr)[ArrayIndex] = In.CastInt64();
					break;
				}
				default:
				{
					LAssert(!"Unknown integer size.");
					return false;
				}
			}
			break;			
		}
		case GV_MAX:
		{
			*((LVariant*)Ptr) = In;
			break;
		}
		default:
			LAssert(!"Impl this type.");
			break;
	}

	return true;
}

bool LCustomType::GetVariant(const char *Field, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Field);
	switch (p)
	{
		case ObjName: // Type: String
		{
			Value = Name;
			return true;
		}
		case ObjType: // Type: String
		{
			Value = "LCustomType";
			return true;
		}
		case ObjLength: // Type: Int32
		{
			Value = (int)Sizeof();
			return true;
		}
		case ObjField: // Type: CustomField[]
		{
			if (Array)
			{
				int Index = atoi(Array);
				if (Index >= 0 && Index < Flds.Length())
				{
					Value = (LDom*)&Flds[Index];
					return true;
				}			
			}
			else
			{
				Value = (int)Flds.Length();
				break;
			}			
			break;
		}
		default:
			break;
	}

	LAssert(0);
	return false;
}

bool LCustomType::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LAssert(0);
	return false;
}

bool LCustomType::CallMethod(const char *MethodName, LVariant *ReturnValue, LArray<LVariant*> &Args)
{
	if (!MethodName || !ReturnValue)
		return false;
	
	if (!_stricmp(MethodName, "New"))
	{
		ReturnValue->Empty();
		ReturnValue->Type = GV_CUSTOM;
		ReturnValue->Value.Custom.Dom = this;
		ReturnValue->Value.Custom.Data = new uint8_t[Sizeof()];
		return true;
	}
	
	if (!_stricmp(MethodName, "Delete")) // Type: (Object)
	{
		for (unsigned i=0; i<Args.Length(); i++)
		{
			LVariant *v = Args[i];
			if (v && v->Type == GV_CUSTOM)
			{
				DeleteArray(v->Value.Custom.Data);
				v->Empty();
			}
		}
		return true;
	}

	LAssert(0);
	return false;
}
