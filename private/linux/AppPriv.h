#pragma once

#include "lgi/common/Json.h"
#if defined(WIN32) && defined(__GTK_H__)
	#include "../win32/SymLookup.h"
#else
	#include "SymLookup.h"
#endif

#if HAS_LIB_MAGIC
// sudo apt-get install libmagic-dev
#include <magic.h>
#endif

#include "LgiWinManGlue.h"

typedef LArray<LAppInfo*> AppArray;

class LAppPrivate : public LSymLookup
{
public:
	// Common
	LApp *Owner = NULL;
	Gtk::GtkApplication *App = NULL;
	LAutoPtr<LJson> Config;
	LAutoPtr<LApp::KeyModFlags> ModFlags;
	LFileSystem *FileSystem = NULL;
	GdcDevice *GdcSystem = NULL;
	OsAppArguments Args;
	LLibrary *SkinLib = NULL;
	LHashTbl<StrKey<char,false>,AppArray*> MimeToApp;
	#if HAS_SHARED_MIME
	LSharedMime *Sm = NULL;
	#endif
	LLibrary *WmLib = NULL;
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
	LArray<Gtk::guint> IdleId;
	bool GuiEnv = true; // else command line
	
	#if defined(LINUX)
	/// Desktop info
	LAutoPtr<LApp::DesktopInfo> DesktopInfo;
	#endif

	#if HAS_LIB_MAGIC
	LMutex MagicLock;
	magic_t hMagic = NULL;
	#endif
	
	/// Any fonts needed for styling the elements
	LAutoPtr<LFontCache> FontCache;
	
	// Clipboard handling
	int Clipboard, Utf8, Utf8String;
	LVariant ClipData;
	LHashTbl<IntKey<int>, ::LVariant*> ClipNotify;

	// Callback support
	struct TCallback {
		const char *file;
		int line;
		std::function<void()> cb;
	};
	using TCallbackArr = LArray<TCallback*>;
	LThreadSafeInterface<TCallbackArr> callbacks;

	LAppPrivate(LApp *a) : Args(0, 0), Owner(a)
		#if HAS_LIB_MAGIC
		, MagicLock("MagicLock")
		#endif
		, callbacks(new TCallbackArr)
	{
		GuiThread = LCurrentThreadHnd();
		GuiThreadId = LCurrentThreadId();
		// printf("GuiThread: %" PRIx64 " id=%i\n", GuiThread, GuiThreadId);
	}

	~LAppPrivate()
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

	LApp::KeyModFlags *GetModFlags()
	{
		if (ModFlags.Reset(new LApp::KeyModFlags))
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

	LJson *GetConfigJson();
	bool SaveConfig();
};

