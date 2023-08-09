#include "lgi/common/Lgi.h"
#include "lgi/common/Password.h"
#include "lgi/common/Variant.h"

//////////////////////////////////////////////////////////////////////////////
// Password encryption for storing the passwords for accounts on the HD
//
// This isn't intended to be secure, but merely a obfusication of the 
// password to hide it from casual observers.
char HashString[] = "KLWEHGF)AS^*_GPYIOShakl;sjfhas0w89725l3jkhL:KHASFQ_DF_AS+_F";

LPassword::LPassword(LPassword *p)
{
	if (p)
		*this = *p;
}

LPassword::~LPassword()
{
}

LPassword &LPassword::operator =(LPassword &p)
{
	Data = p.Data;
	return *this;
}

bool LPassword::operator ==(LPassword &p)
{
	if (Data.Length() != p.Data.Length())
		return false;

	return memcmp(Data.Get(), p.Data.Get(), Data.Length()) == 0;
}

void LPassword::Process(char *Out, const char *In, ssize_t Len) const
{
	if (Out && In && Len > 0)
	{
		size_t HashLen = strlen(HashString);
		int i;

		for (i=0; i<Len; i++)
		{
			Out[i] = HashString[i % HashLen] ^ In[i];
		}
	}
}

LString LPassword::Get() const
{
	LString p;
	
	p.Length(Data.Length());
	Process(p, Data.Get(), Data.Length());
	p.Get()[Data.Length()] = 0;
	
	return p;
}

void LPassword::Get(char *Buf) const
{
	if (Buf)
	{
		Process(Buf, Data.Get(), Data.Length());
		Buf[Data.Length()] = 0; // NULL terminate
	}
}

void LPassword::Set(const char *Buf)
{
	Data.Empty();
	auto len = strlen(Buf);
	if (Buf && Data.Length(len))
		Process(Data.Get(), Buf, len);
}

void LPassword::Serialize(char *Password, int Write)
{
	LPointer p;
	p.c = Password;

	if (Write)
	{
		*p.u32++ = (int)Data.Length();
		if (Data)
			memcpy(p.c, Data.Get(), Data.Length());
	}
	else // read
	{
		if (Data.Length(*p.u32++))
			memcpy(Data.Get(), p.c, Data.Length());
	}
}

bool LPassword::Serialize(LDom *Options, const char *Prop, int Write)
{
	bool Status = false;
	if (Options && Prop)
	{
		LVariant v;
		if (Write)
		{
			v.SetBinary(Data.Length(), Data.Get());
			Status = Options->SetValue(Prop, v);
		}
		else // read from prop list
		{
			if (Options->GetValue(Prop, v) &&
				v.Type == GV_BINARY)
			{
				if (Data.Length(v.Value.Binary.Length))
				{
					memcpy(Data.Get(), v.Value.Binary.Data, Data.Length());
					Data.Get()[Data.Length()] = 0;
					Status = true;
				}
			}
		}
	}
	return Status;
}

void LPassword::Delete(LDom *Options, char *Option)
{
	if (Options && Option)
	{
		LVariant v;
		Options->SetValue(Option, v);
	}
}
