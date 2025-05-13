#pragma once

#ifdef HAIKU
#include <Application.h>
#endif

#ifndef kMsgDeleteServerMemoryArea
#define kMsgDeleteServerMemoryArea '_DSA'
#endif

#include "lgi/common/Json.h"

typedef LArray<LAppInfo> AppArray;

// This list of events are send from the BWindow threads over
// to the application thread for processing.
#define APP_PRIV_EVENTS() \
	_(None) \
	_(QuitRequested) \
	_(General) \
	_(FrameMoved) \
	_(FrameResized) \
	_(AttachedToWindow) \
	_(KeyDown) \
	_(KeyUp) \
	_(Draw) \
	_(MouseDown) \
	_(MouseUp) \
	_(MouseMoved) \
	_(MakeFocus)

class LAppPrivate : public BApplication
{
	static LAppPrivate *Inst;

public:
	// Event processing
	enum Events
	{
		#define _(name) name,
		APP_PRIV_EVENTS()
		#undef _
	};
	const char *ToString(Events e)
	{
		switch (e)
		{
			#define _(name) case name: return #name;
			APP_PRIV_EVENTS()
			#undef _
		}
		return NULL;
	}
	static bool Post(BMessage *msg)
	{
		if (!Inst)
			return false;		
		// if (!Inst->Lock()) return false;		
		auto r = Inst->PostMessage(msg);
		// Inst->Unlock();
		return r == B_OK;
	}

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

	void OnMessage(BMessage *m)
	{
		LView *view = nullptr;
		m->FindPointer(LMessage::PropView, (void**)&view);
		LWindow *wnd = nullptr;
		m->FindPointer(LMessage::PropWindow, (void**)&wnd);
		if (!view && !wnd)
		{
			LAssert(0);
			printf("%s:%i - no view/wnd in msg.\n", _FL);
			return;
		}
		
		int32_t event = -1;
		if (m->FindInt32(LMessage::PropEvent, &event) != B_OK)
		{
			printf("%s:%i - no event in msg.\n", _FL);
			return;
		}
		
		switch (event)
		{
			case Events::QuitRequested:
			{
				int *result = nullptr;
				if (m->FindPointer("result", (void**)&result) != B_OK)
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
			case Events::General:
			{
				BMessage msg;
				if (m->FindMessage("message", &msg) != B_OK)
				{
					printf("%s:%i - no message.\n", _FL);
					return;
				}
				
				if (view)
					view->OnEvent((LMessage*) &msg);
				else if (wnd)
					wnd->OnEvent((LMessage*) &msg);
				break;
			}
			case Events::FrameMoved:
			{
				BPoint pos;
				if (m->FindPoint("pos", &pos) != B_OK)
				{
					printf("%s:%i - no pos.\n", _FL);
					return;
				}
				
				if (view)
				{
					view->Pos.Offset(pos.x - view->Pos.x1, pos.y - view->Pos.y1);
					view->OnPosChange();
				}
				else if (wnd)
				{
					wnd->Pos.Offset(pos.x - wnd->Pos.x1, pos.y - wnd->Pos.y1);
					wnd->OnPosChange();
				}
				break;
			}
			case Events::FrameResized:
			{
				float width = 0.0f, height = 0.0f;
				if (m->FindFloat("width", &width) != B_OK ||
					m->FindFloat("height", &height) != B_OK)
				{
					printf("%s:%i - missing width/height param.\n", _FL);
					return;
				}

				if (view)
				{
					view->Pos.SetSize(width, height);
					view->OnPosChange();
				}
				else if (wnd)
				{
					wnd->Pos.SetSize(width, height);
					wnd->OnPosChange();
				}
				break;
			}
			case Events::AttachedToWindow:
			{
				view->OnCreate();
				break;
			}
			case Events::Draw:
			{
				LPoint off(0, 0);
				if (view)
				{
					LLocker lck(view->Handle(), _FL);
					if (lck.Lock())
					{
						LScreenDC dc(view);
						view->_Paint(&dc, &off, NULL);
					}
				}
				else if (wnd)
				{
					LLocker lck(wnd->WindowHandle(), _FL);
					if (lck.Lock())
					{
						LScreenDC dc(wnd);
						wnd->_Paint(&dc, &off, NULL);
					}
				}
				break;
			}
			case Events::MouseMoved:
			{
				BPoint where;
				uint32_t code;
				m->FindPoint("pos", &where);
				m->FindUInt32("code", &code);
				
				if (view)
				{
					
				}
				break;
			}
			case Events::MouseDown:
			case Events::MouseUp:
			case Events::MakeFocus:
			case Events::KeyDown:
			case Events::KeyUp:
			default:
			{
				printf("%s:%i - unhandled event '%s'\n", _FL, ToString((Events)event));
				break;
			}
		}
	}
	
	void MessageReceived(BMessage* message)
	{
		switch (message->what)
		{
			case M_WND_EVENT:
			case M_VIEW_EVENT:
				OnMessage(message);
				break;
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

