#ifndef _GHASHDOM_H_
#define _GHASHDOM_H_

#include "lgi/common/Variant.h"

class GHashDom : public LHashTbl<ConstStrKey<char,false>, LVariant*>, public GDom
{
public:
	GHashDom(int Size = 0) : LHashTbl<ConstStrKey<char,false>, LVariant*>(Size)
	{
	}
	
	~GHashDom()
	{
	    DeleteObjects();
	}

	bool GetVariant(const char *Name, LVariant &Value, char *Array = 0)
	{
		LVariant *v = Find(Name);
		if (v)
		{
			Value = *v;
			return true;
		}
		
		return false;
	}

	bool SetVariant(const char *Name, LVariant &Value, char *Array = 0)
	{
		LVariant *v = Find(Name);
		if (v)
		{
		    *v = Value;
		    return true;
		}
		else if ((v = new LVariant(Value)))
		{
			Add(Name, v);
			return true;
		}
		return false;
	}
};

#endif