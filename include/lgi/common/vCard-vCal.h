#ifndef _VCARD_VCAL_H
#define _VCARD_VCAL_H

// #include "ScribeDefs.h"
#include "Store3.h"

#define FIELD_PERMISSIONS				2000

class VIoPriv;
class VIo
{
public:
	struct Parameter
	{
		LString Field;
		LString Value;
		
		void Set(const char *f, const char *v)
		{
			Field = f;
			Value = v;
		}
	};
	
	struct TimeZoneSection
	{
		int From, To;
		LDateTime Start;
		LString Rule;
		LString Name;
	};
	
	struct TimeZoneInfo
	{
		LString Name;
		TimeZoneSection Normal;
		TimeZoneSection Daylight;
	};

	class ParamArray : public LArray<Parameter>
	{
	public:
		ParamArray(const char *Init = NULL)
		{
			if (Init)
			{
				LString s = Init;
				LString::Array a = s.SplitDelimit(",");
				for (unsigned i=0; i<a.Length(); i++)
					New().Set("type", a[i]);
			}
		}
	
		const char *Find(const char *s)
		{
			for (auto &p: *this)
			{
				if (p.Field.Equals(s))
					return p.Value;
			}
			return NULL;
		}
		
		void Set(const char *var, const char *value)
		{
			for (auto &p: *this)
			{
				if (p.Field.Equals(var))
				{
					p.Value = value;
					return;
				}
			}
			auto &p = New();
			p.Field = var;
			p.Value = value;
		}

		void Empty()
		{
			Length(0);
		}
	};

protected:
	VIoPriv *d;

	bool ParseDate(LDateTime &out, char *in);
	bool ParseDuration(LDateTime &Out, int &Sign, char *In);

	void Fold(LStreamI &o, const char *i, int pre_chars = 0, const char *encoding = NULL);
	LString UnMultiLine(char *In);

	bool ReadField(LStreamI &s, LString &Name, ParamArray *Type, LString &Data);
	void WriteField(LStreamI &s, const char *Name, ParamArray *Type, const char *Data);

public:
	VIo();
	~VIo();
};

class VCard : public VIo
{
public:
	bool Import(LDataPropI *c, LStreamI *s);
	bool Export(LDataPropI *c, LStreamI *s);
};

class VCal : public VIo
{
    bool IsCal;

public:
    VCal()
    {
        IsCal = false;
    }

	bool Import(LDataPropI *c, LStreamI *s);
	bool Export(LDataPropI *c, LStreamI *s);
};

#endif
