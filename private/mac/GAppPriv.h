#pragma once

#include "LJson.h"
#include "GSymLookup.h"
#include "GFontCache.h"

typedef GArray<GAppInfo*> AppArray;

class GAppPrivate
{
public:
	GApp *Owner;
	OsApp NsApp;
	int RunDepth;

	// Common
	GAutoPtr<LJson> Config;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	GSymLookup SymLookup;
	GAutoString Mime;
	GAutoString Name;
	GAutoString UrlArg;
	
	/// Any fonts needed for styling the elements
	GAutoPtr<GFontCache> FontCache;
	
	GAppPrivate(GApp *owner) : Owner(owner)
	{
		NsApp = NULL;
		RunDepth = 0;
		FileSystem = 0;
		GdcSystem = 0;
		SkinLib = 0;
		GuiThread = LgiGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
	}
	
	~GAppPrivate()
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

