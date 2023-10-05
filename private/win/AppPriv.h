#pragma once

#include "SymLookup.h"
#include "lgi/common/Json.h"
#include "lgi/common/FontCache.h"

class LAppPrivate
{
public:
	// Common
	LApp *Owner = NULL;
	LAutoPtr<LJson> Config;
	LFileSystem *FileSystem = NULL;
	GdcDevice *GdcSystem = NULL;
	OsAppArguments Args;
	LLibrary *SkinLib = NULL;
	OsThread GuiThread = NULL;
	int LinuxWine = -1;
	LString Mime, ProductId;
	bool ThemeAware = true;

	/// Any fonts needed for styling the elements
	LAutoPtr<LFontCache> FontCache;

	// Win32
	bool QuitReceived = false;
	LApp::ClassContainer Classes;
	LSymLookup *SymLookup = NULL;

	LAppPrivate(LApp *owner) : Owner(owner)
	{
		auto b = DuplicateHandle(	GetCurrentProcess(),
									GetCurrentThread(),
									GetCurrentProcess(),
									&GuiThread,
									0,
									false,
									DUPLICATE_SAME_ACCESS);
	}

	~LAppPrivate()
	{
		Classes.DeleteObjects();
		DeleteObj(SkinLib);
		DeleteObj(SymLookup);
	}

	LJson *GetConfig();
	bool SaveConfig();
};

