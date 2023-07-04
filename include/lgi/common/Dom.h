/**
	\file
	\author Matthew Allen
	\brief Document object model class.\n
	Copyright (C), <a href="mailto:fret@memecode.com">Matthew Allen</a>
 */

#pragma once

class LVariant;
#include "lgi/common/LgiInterfaces.h"
#include "lgi/common/Mem.h"
#include "lgi/common/Array.h"

#define DOM_GET_VAR(name) \
	case name:
#define DOM_SET_VAR(name) \
	case name:
#define DOM_METHOD(name, args) \
	case name:

/// API for reading and writing properties in objects.
class LgiClass LDom : virtual public LDomI
{
	friend struct LDomRef;
	friend class LScriptEnginePrivate;
	friend class LVirtualMachinePriv;

public:
	enum DomMemberType
	{
		DomVariable,
		DomMethod,
	};
	struct DomMember
	{
		LDom::DomMemberType Type; // Variable or method
		LString Name; // Name of member in Object.Name format.
		LString Args; // comma separated list of args
	};
	bool _AddMember(DomMemberType type, const char *name, const char *args = NULL);
	static bool _EnumMembers(const char *object, LArray<DomMember> &members);
	
protected:
	LDom *ResolveObject(const char *Var, LString &Name, LString &Array);

	virtual bool _OnAccess(bool Start) { return true; }
	virtual bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) { return false; }
	virtual bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) { return false; }

public:
	/// Gets an object's property
	bool GetValue
	(
		/// The string describing the property
		const char *Var,
		/// The value returned
		LVariant &Value
	);
	
	/// Sets an object's property
	bool SetValue
	(
		/// The string describing the property
		const char *Var,
		/// The value to set the property to
		LVariant &Value
	);
};

/// This is a global map between strings and enum values for fast lookup of
/// object properties inside the SetVariant, GetVariant and CallMethod handlers.
enum LDomProperty
{
	#undef _
	#define _(symbol, txt) symbol,
	#include "lgi/common/DomFields.h"
	#undef _
};

LgiFunc LDomProperty LStringToDomProp(const char *Str);
LgiFunc const char *LDomPropToString(LDomProperty Prop);

