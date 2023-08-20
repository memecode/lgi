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
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	bool CmdLineProcessed = false;
	
	/// Any fonts needed for styling the elements
	LAutoPtr<LFontCache> FontCache;
	
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

		/*
		struct sem_info info;
		auto status = _get_sem_info(Sem(), &info, sizeof(info));
		printf("~LAppPrivate: sem=%i, team=%i name=%s count=%i latest=%i\n",
				Sem(),
				info.team,
				info.name,
				info.count,
				info.latest_holder);
		
		printf("~LAppPrivate: IsLocked()=%i\n", IsLocked());
		printf("~LAppPrivate: CountLocks()=%i\n", CountLocks());
		printf("~LAppPrivate: CountLockRequests()=%i\n", CountLockRequests());
		printf("~LAppPrivate: GuiThreadId=%i LockingThread()=%i\n", GuiThreadId, LockingThread());
		*/
	}

	LJson *GetConfig();
	bool SaveConfig();
};

