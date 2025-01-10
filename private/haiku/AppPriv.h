#pragma once

#ifdef HAIKU
#include <Application.h>
#endif

#ifndef kMsgDeleteServerMemoryArea
#define kMsgDeleteServerMemoryArea '_DSA'
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

	/// Per thread system fonts:
	LHashTbl<IntKey<OsThreadId>,LFont*> SystemFont, BoldFont;
	
	/// Any fonts needed for styling the elements
	LHashTbl<IntKey<OsThreadId>,LFontCache*> FontCache;
	
	LAppPrivate(LApp *a) :
		BApplication(LString("application/") + a->Name()),
		Args(0, 0),
		Owner(a)
	{
		GuiThread = LCurrentThreadHnd();
		GuiThreadId = LCurrentThreadId();
	}

	~LAppPrivate()
	{
		for (auto p: SystemFont)
			DeleteObj(p.value);
		for (auto p: BoldFont)
			DeleteObj(p.value);
		for (auto p: FontCache)
			DeleteObj(p.value);

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

	LJson *GetConfigJson();
	bool SaveConfig();
	
	void MessageReceived(BMessage* message)
	{
		switch (message->what)
		{
			case M_HANDLE_IN_THREAD:
				LView::HandleInThreadMessage(message);
				break;
			case kMsgDeleteServerMemoryArea:
				// What am I supposed to do with this?
				break;
			default:
				printf("%s:%i Unhandled MessageReceived %i (%.4s)\n", _FL, message->what, &message->what);
				break;
		}
	
		BApplication::MessageReceived(message);
	}
	
	void RefsReceived(BMessage* message)
	{
		printf("%s:%i RefsReceived called... impl me!\n");
	
		BApplication::RefsReceived(message);
	}
	
	void AboutRequested()
	{
		printf("%s:%i AboutRequested called... impl me!\n");
	
		BApplication::AboutRequested();
	}
};

