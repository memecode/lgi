#ifndef _ICC_H_
#define _ICC_H_

#include "lgi/common/Dom.h"
#include "lgi/common/Variant.h"

class GIccProfile : public LDom
{
	class GIccProfilePrivate *d;

public:
	GIccProfile(char *file = 0);
	~GIccProfile();

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
	bool Convert(COLOUR *Out32, COLOUR In32, GIccProfile *Profile = 0);
	bool Convert(LSurface *Dest, LSurface *Src, GIccProfile *Profile = 0);

	// Dom
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = 0);
};

#endif