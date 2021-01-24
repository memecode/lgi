#pragma once

#include "LJson.h"
#include "GSymLookup.h"
#include "GFontCache.h"

class GAppPrivate
{
public:
	// Common
	GApp *Owner;
	GAutoPtr<LJson> Config;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	OsThread GuiThread;
	int LinuxWine;
	GAutoString Mime, ProductId;
	bool ThemeAware;

	/// Any fonts needed for styling the elements
	GAutoPtr<GFontCache> FontCache;

	// Win32
	bool QuitReceived;
	GApp::ClassContainer Classes;
	GSymLookup *SymLookup;

	GAppPrivate(GApp *owner) : Owner(owner)
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

	~GAppPrivate()
	{
		DeleteObj(SkinLib);
		DeleteObj(SymLookup);
	}

	LJson *GetConfig();
	bool SaveConfig();
};

