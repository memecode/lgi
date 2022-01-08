#pragma once

#include "SymLookup.h"
#include "lgi/common/Json.h"
#include "lgi/common/FontCache.h"

class LAppPrivate
{
public:
	// Common
	LApp *Owner;
	LAutoPtr<LJson> Config;
	LFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	LLibrary *SkinLib;
	OsThread GuiThread;
	int LinuxWine;
	LAutoString Mime, ProductId;
	bool ThemeAware;

	/// Any fonts needed for styling the elements
	LAutoPtr<LFontCache> FontCache;

	// Win32
	bool QuitReceived;
	LApp::ClassContainer Classes;
	LSymLookup *SymLookup;

	LAppPrivate(LApp *owner) : Owner(owner)
	{
		LinuxWine = -1;
		SymLookup = 0;
		QuitReceived = false;
		SkinLib = 0;
		GuiThread = NULL;
		auto b = DuplicateHandle(GetCurrentProcess(),
									GetCurrentThread(),
									GetCurrentProcess(),
									&GuiThread,
									0,
									false,
									DUPLICATE_SAME_ACCESS);
		ThemeAware = true;
	}

	~LAppPrivate()
	{
		DeleteObj(SkinLib);
		DeleteObj(SymLookup);
	}

	LJson *GetConfig();
	bool SaveConfig();
};

