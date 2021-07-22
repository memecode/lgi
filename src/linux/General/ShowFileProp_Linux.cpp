#include "lgi/common/Lgi.h"

void LShowFileProperties(OsView Parent, const char *Filename)
{
	LAssert(!"Impl me.");
}

bool LBrowseToFile(const char *Filename)
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