#include "lgi/common/Lgi.h"

void LgiShowFileProperties(OsView Parent, const char *Filename)
{
	LgiAssert(!"Impl me.");
}

bool LgiBrowseToFile(const char *Filename)
{
	if (!Filename)
		return false;

	char Browser[MAX_PATH];
	if (!LGetAppForMimeType("inode/directory", Browser, sizeof(Browser)))
		return false;
	
	LString f;
	if (strchr(Filename, ' '))
		f.Printf("\"%s\"", Filename);
	else
		f = Filename;
	
	return LExecute(Browser, f);
}