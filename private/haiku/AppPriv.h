#pragma once

#ifdef HAIKU
#include <Application.h>
#endif

#include "lgi/common/Json.h"

typedef LArray<LAppInfo> AppArray;

class LAppPrivate : public BApplication
{
public:
	// Common
	LApp *Owner = NULL;
	LAutoPtr<LJson> Config;
	LFileSystem *FileSystem = NULL;
	GdcDevice *GdcSystem = NULL;
	OsAppArguments Args;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	LHashTbl<IntKey<int>, LView*> Handles;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	int MessageLoopDepth = 0;
	int CurEvent = 0;
	#if DEBUG_MSG_TYPES
	LArray<int> Types;
	#endif
	LArray<LViewI*> DeleteLater;
	LMouse LastMove;
	LAutoString Name;
	
	/// Any fonts needed for styling the elements
	LAutoPtr<LFontCache> FontCache;
	
	// Clipboard handling
	int Clipboard = 0, Utf8 = 0, Utf8String = 0;
	LVariant ClipData;
	LHashTbl<IntKey<int>, ::LVariant*> ClipNotify;

	// Mouse click info
	uint64 LastClickTime = 0;
	OsView LastClickWnd = NULL;
	int LastClickX = 0;
	int LastClickY = 0;

	LAppPrivate(LApp *a) :
		BApplication(LString("application/") + a->Name()),
		Args(0, 0),
		Owner(a)
	{
		GuiThread = LGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
	}

	~LAppPrivate()
	{
		MimeToApp.DeleteObjects();

		struct sem_info info;
		auto status = _get_sem_info(Sem(), &info, sizeof(info));
		printf("~LAppPrivate: team=%i name=%s count=%i latest=%i\n",
				info.team,
				info.name,
				info.count,
				info.latest_holder);
		
		printf("~LAppPrivate: IsLocked()=%i\n", IsLocked());
		printf("~LAppPrivate: CountLocks()=%i\n", CountLocks());
		printf("~LAppPrivate: CountLockRequests()=%i\n", CountLockRequests());
		printf("~LAppPrivate: GuiThreadId=%i LockingThread()=%i\n", GuiThreadId, LockingThread());
	}

	LJson *GetConfig();
	bool SaveConfig();
};

