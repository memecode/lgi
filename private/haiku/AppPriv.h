#pragma once

#ifdef HAIKU
#include <Application.h>
#endif

#include "lgi/common/Json.h"

typedef LArray<LAppInfo*> AppArray;

class LAppPrivate
	#ifdef HAIKU
	: public BApplication
	#endif
{
public:
	// Common
	LApp *Owner;
	LAutoPtr<LJson> Config;
	LFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	LLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	LHashTbl<IntKey<int>, LView*> Handles;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	int MessageLoopDepth;
	int CurEvent;
	#if DEBUG_MSG_TYPES
	LArray<int> Types;
	#endif
	::LArray<LViewI*> DeleteLater;
	LMouse LastMove;
	LAutoString Name;
	
	/// Any fonts needed for styling the elements
	LAutoPtr<LFontCache> FontCache;
	
	// Clipboard handling
	int Clipboard, Utf8, Utf8String;
	::LVariant ClipData;
	LHashTbl<IntKey<int>, ::LVariant*> ClipNotify;

	// Mouse click info
	uint64 LastClickTime;
	OsView LastClickWnd;
	int LastClickX;
	int LastClickY;

	LAppPrivate(LApp *a) : Args(0, 0), Owner(a)
	#ifdef HAIKU
		, BApplication(LString("application/") + a->Name())
	#endif
	{
		CurEvent = 0;
		GuiThread = LGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
		FileSystem = 0;
		GdcSystem = 0;
		SkinLib = 0;
		MessageLoopDepth = 0;

		LastClickTime = 0;
		LastClickWnd = 0;
		LastClickX = 0;
		LastClickY = 0;
	}

	~LAppPrivate()
	{
		for (auto p : MimeToApp)
		{
			p.value->DeleteObjects();
			DeleteObj(p.value);
		}
	}

	LJson *GetConfig();
	bool SaveConfig();
};

