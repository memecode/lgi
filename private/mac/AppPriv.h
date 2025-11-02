#pragma once

#include "lgi/common/Json.h"
#include "SymLookup.h"
#include "lgi/common/FontCache.h"

typedef LArray<LAppInfo*> AppArray;

class LAppPrivate : public LMutex
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
	LLibrary *SkinLib;
	LHashTbl<ConstStrKey<char,false>,AppArray*> MimeToApp;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	LSymLookup SymLookup;
	LAutoString Mime;
	LAutoString Name;
	LAutoString UrlArg;
	LAutoPtr<LWindow> callbackWnd; // lock before using

	/// Any fonts needed for styling the elements
	LAutoPtr<LFontCache> FontCache;
	
	LAppPrivate(LApp *owner) :
		LMutex("LAppPrivate.lock"),
		Owner(owner)
	{
		NsApp = NULL;
		RunDepth = 0;
		FileSystem = 0;
		GdcSystem = 0;
		SkinLib = 0;
		GuiThread = LCurrentThreadHnd();
		GuiThreadId = LCurrentThreadId();
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
	
	LJson *GetConfigJson();
	bool SaveConfig();
};

