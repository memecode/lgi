#pragma once

#include "lgi/common/Json.h"
#include "SymLookup.h"
#include "lgi/common/FontCache.h"

typedef LArray<LAppInfo*> AppArray;

class LAppPrivate
{
public:
	LApp *Owner;
	OsApp NsApp;
	int RunDepth;

	// Common
	LAutoPtr<LJson> Config;
	LFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	LSymLookup SymLookup;
	LAutoString Mime;
	LAutoString Name;
	LAutoString UrlArg;
	
	/// Any fonts needed for styling the elements
	LAutoPtr<GFontCache> FontCache;
	
	LAppPrivate(LApp *owner) : Owner(owner)
	{
		NsApp = NULL;
		RunDepth = 0;
		FileSystem = 0;
		GdcSystem = 0;
		SkinLib = 0;
		GuiThread = LGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
	}
	
	~LAppPrivate()
	{
		DeleteObj(SkinLib);
		
		for (auto p : MimeToApp)
		{
			p.value->DeleteObjects();
			DeleteObj(p.value);
		}
	}
	
	LJson *GetConfig();
	bool SaveConfig();
};

