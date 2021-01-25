#pragma once

#include "LJson.h"
#if defined(WIN32) && defined(__GTK_H__)
#include "../win32/GSymLookup.h"
#else
#include "GSymLookup.h"
#endif

#if HAS_LIB_MAGIC
// sudo apt-get install libmagic-dev
#include <magic.h>
#endif

#include "LgiWinManGlue.h"

typedef GArray<GAppInfo*> AppArray;
using namespace Gtk;

class GAppPrivate : public GSymLookup
{
public:
	// Common
	GApp *Owner;
	GtkApplication *App;
	GAutoPtr<LJson> Config;
	GAutoPtr<GApp::KeyModFlags> ModFlags;
	GFileSystem *FileSystem;
	GdcDevice *GdcSystem;
	OsAppArguments Args;
	GLibrary *SkinLib;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	#if HAS_SHARED_MIME
	GSharedMime *Sm;
	#endif
	GLibrary *WmLib;
	LHashTbl<IntKey<int>, GView*> Handles;
	OsThread GuiThread;
	OsThreadId GuiThreadId;
	int MessageLoopDepth;
	int CurEvent;
	#if DEBUG_MSG_TYPES
	GArray<int> Types;
	#endif
	::GArray<GViewI*> DeleteLater;
	GMouse LastMove;
	GAutoString Name;
	::GArray<Gtk::guint> IdleId;
	
	#if defined(LINUX)
	/// Desktop info
	GAutoPtr<GApp::DesktopInfo> DesktopInfo;
	#endif

	#if HAS_LIB_MAGIC
	LMutex MagicLock;
	magic_t hMagic;
	#endif

	/// Any fonts needed for styling the elements
	GAutoPtr<GFontCache> FontCache;
	
	// Clipboard handling
	int Clipboard, Utf8, Utf8String;
	::GVariant ClipData;
	LHashTbl<IntKey<int>, ::GVariant*> ClipNotify;

	// Mouse click info
	uint64 LastClickTime;
	OsView LastClickWnd;
	int LastClickX;
	int LastClickY;

	GAppPrivate(GApp *a) : Args(0, 0), Owner(a)
		#if HAS_LIB_MAGIC
		, MagicLock("MagicLock")
		#endif
	{
		IdleId = 0;
		CurEvent = 0;
		GuiThread = LgiGetCurrentThread();
		GuiThreadId = GetCurrentThreadId();
		WmLib = 0;
		FileSystem = 0;
		GdcSystem = 0;
		SkinLib = 0;
		MessageLoopDepth = 0;
		#if HAS_SHARED_MIME
		Sm = 0;
		#endif
		#if HAS_LIB_MAGIC
		hMagic = NULL;
		#endif

		LastClickTime = 0;
		LastClickWnd = 0;
		LastClickX = 0;
		LastClickY = 0;
	}

	~GAppPrivate()
	{
		#if HAS_LIB_MAGIC
		if (MagicLock.Lock(_FL))
		{
			if (hMagic != NULL)
			{
				magic_close(hMagic);
				hMagic = NULL;
			}
			MagicLock.Unlock();
		}
		#endif

		if (WmLib)
		{
			Proc_LgiWmExit WmExit = (Proc_LgiWmExit) WmLib->GetAddress("LgiWmExit");
			if (!WmExit || WmExit())
			{
				DeleteObj(WmLib);
			}
			else
			{
				// Do not unload the library is the exit function fails...
				// It could still be doing something or have AtExit handlers
				// in place.
				WmLib = 0;
				// Do just leak the memory. We're shutting down anyway.
			}
		}
		DeleteObj(SkinLib);
		
		#if HAS_SHARED_MIME
		DeleteObj(Sm);
		#endif
		
		for (auto p : MimeToApp)
		{
			p.value->DeleteObjects();
			DeleteObj(p.value);
		}
	}

	GApp::KeyModFlags *GetModFlags()
	{
		if (ModFlags.Reset(new GApp::KeyModFlags))
		{
			#define _(name) \
				{ \
					auto f = Owner->GetConfig("Linux.Keys." #name); \
					if (f) \
					{ \
						auto i = ModFlags->FlagValue(f); \
						if (i) ModFlags->name = i; \
					} \
				}
			
			_(Shift)
			_(Alt)
			_(Ctrl)
			_(System)
			#undef _
			
			if (Owner->GetConfig("Linux.Keys.Debug").Int() != 0)
				ModFlags->Debug = true;
			

			if (!ModFlags->Ctrl ||
				!ModFlags->Shift ||
				!ModFlags->Alt ||
				!ModFlags->System)
			{
				printf("%s:%i - Invalid ModFlags ctrl:%x sh:%x alt:%x sys:%x\n",
					_FL,
					ModFlags->Ctrl,
					ModFlags->Shift,
					ModFlags->Alt,
					ModFlags->System);
			}
		}
		
		return ModFlags;
	}

	LJson *GetConfig();
	bool SaveConfig();
};

