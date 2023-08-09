/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A password manager

#ifndef __GPASSWORD_H
#define __GPASSWORD_H

class LgiClass LPassword
{
	LString Data; // this is binary data... may contain NULL's
	
	void Process(char *Out, const char *In, ssize_t Len) const;

public:
	LPassword(LPassword *p = NULL);
	virtual ~LPassword();

	bool IsValid() { return Data.Length() > 0; }
	void Get(char *Buf) const;
	LString Get() const;
	void Set(const char *Buf);
	
	// bool Serialize(ObjProperties *Options, char *Option, int Write);
	bool Serialize(LDom *Options, const char *Option, int Write);
	void Serialize(char *Password, int Write);
	
	void Delete(LDom *Options, char *Option);

	LPassword &operator =(LPassword &p);
	bool operator ==(LPassword &p);
};

#endif
