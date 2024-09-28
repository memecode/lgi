#ifndef _GHASHDOM_H_
#define _GHASHDOM_H_

#include "lgi/common/Variant.h"

class LHashDom : public LHashTbl<ConstStrKey<char,false>, LVariant*>, public LDom
{
public:
	LHashDom(int Size = 0) : LHashTbl<ConstStrKey<char,false>, LVariant*>(Size)
	{
	}
	
	~LHashDom()
	{
	    Empty();
	}
	
	const char *GetClass() override { return "LHashDom"; }

	void Empty()
	{
		DeleteObjects();
	}

	bool GetVariant(const char *Name, LVariant &Value, const char *Array = 0) override
	{
		LVariant *v = Find(Name);
		if (v)
		{
			Value = *v;
			return true;
		}
		
		return false;
	}

	bool SetVariant(const char *Name, LVariant &Value, const char *Array = 0) override
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
