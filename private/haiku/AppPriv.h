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
	static LAppPrivate *Inst;

public:
	enum Events
	{
		None,
		General,
		FrameMoved,
		FrameResized,
		QuitRequested,
		AttachedToWindow,
		KeyDown,
		KeyUp,
		Draw
	};

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
		Inst = this;
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
		
		Inst = nullptr;
	}

	LJson *GetConfigJson();
	bool SaveConfig();
	
	void MessageReceived(BMessage* message)
	{
		LViewI *view = nullptr;
		message->FindPointer(LMessage::PropView, (void**)&view);
		
		LWindow *wnd = nullptr;
		message->FindPointer(LMessage::PropWindow, (void**)&wnd);
		
		int32_t event = -1;
		if (message->FindInt32(LMessage::PropEvent, &event) == B_OK)
		{
			switch (event)
			{
				case Events::QuitRequested:
				{
					int *result = nullptr;
					if (message->FindPointer("result", (void**)&result) != B_OK)
					{
						printf("%s:%i - error: no result ptr.\n", _FL);
						return;
					}
					if (wnd)
						*result = wnd->OnRequestClose(false);
					else if (view)
						*result = view->OnRequestClose(false);
					else
						*result = true;					
					break;
				}
			}
		}
		else
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

	static bool Post(BMessage *msg)
	{
		if (!Inst)
			return false;		
		if (!Inst->Lock())
			return false;		
		auto r = Inst->PostMessage(msg);
		Inst->Unlock();
		return r == B_OK;
	}
};

