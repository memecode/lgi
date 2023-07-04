#ifndef _ICC_H_
#define _ICC_H_

#include "lgi/common/Dom.h"
#include "lgi/common/Variant.h"

class LIccProfile : public LDom
{
	class LIccProfilePrivate *d;

public:
	LIccProfile(char *file = 0);
	~LIccProfile();

	const char *GetClass() override { return "LIccProfile"; }

	// I/O
	bool CreateNamed(const char *name);
	bool Open(const char *file);
	bool Open(LStream *stream);
	bool Save(const char *file);
	bool Save(LStream *stream);
	
	// Props
	char *GetName();
	char *GetError();

	// Conversion
	bool Convert(COLOUR *Out32, COLOUR In32, LIccProfile *Profile = 0);
	bool Convert(LSurface *Dest, LSurface *Src, LIccProfile *Profile = 0);

	// Dom
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = 0);
};

#endif