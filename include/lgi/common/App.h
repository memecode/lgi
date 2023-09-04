#ifndef _GAPP_H_
#define _GAPP_H_

/////////////////////////////////////////////////////////////////////////////////
#if WINNATIVE
typedef DWORD						OsProcessId;
#else
typedef int							OsProcessId;
#endif

/// Returns the current process ID
#define LProcessId()				(LAppInst->GetProcessId())

/// Returns a pointer to the LApp object.
///
/// \warning Don't use this before you have created your LApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#define LAppInst						(LApp::ObjInstance())

/// Returns a system font pointer.
///
/// \warning Don't use this before you have created your LApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#ifdef HAIKU
#define LSysFont						(LAppInst->GetFont(false))
#else
#define LSysFont						(LAppInst->SystemNormal)
#endif

/// Returns a bold system font pointer.
///
/// \warning Don't use this before you have created your LApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#ifdef HAIKU
#define LSysBold						(LAppInst->GetFont(true))
#else
#define LSysBold						(LAppInst->SystemBold)
#endif

/// Exits the application right now!
///
/// \warning This will cause data loss if you have any unsaved data. Equivilant to exit(0).
LgiFunc void LExitApp();

/// Closes the application gracefully.
///
/// This actually causes LApp::Run() to stop processing message and return.
#define LCloseApp()				LAppInst->Exit(false)

#if defined(LINUX) && !defined(LGI_SDL)
#define ThreadCheck()				LAssert(InThread())
#else
#define ThreadCheck()
#endif

/// Optional arguments to the LApp object
struct LAppArguments
{
	/// Don't initialize the skinning engine.
	bool NoSkin = false;
	/// Don't do crash handling
	bool NoCrashHandler = false;

	LAppArguments(const char *init = NULL)
	{
		auto a = LString(init).SplitDelimit(",");
		for (auto o: a)
			if (o.Equals("NoCrashHandler"))
				NoCrashHandler = true;
			else if (o.Equals("NoSkin"))
				NoSkin = true;
	}
};

class LAppPrivate;

#if LGI_COCOA && defined __OBJC__
#include "LCocoaView.h"

@interface LNsApplication : NSApplication
{
}

@property LAppPrivate *d;

- (id)init;
- (void)setPriv:(nonnull LAppPrivate*)priv;
- (void)terminate:(nullable id)sender;
- (void)dealloc;
- (void)assert:(nonnull LCocoaAssert*)ca;
- (void)onUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)reply;

@end

ObjCWrapper(LNsApplication, OsApp)

#endif

/// \brief Singleton class for handling application wide settings and methods
///
/// This should be the first class you create, passing in the arguments from the
/// operating system. And once your initialization is complete the 'Run' method
/// is called to enter the main application loop that processes messages for the
/// life time of the application.
class LgiClass LApp : virtual public LAppI,
	public LBase,
	public OsApplication
{
	friend class LView;
	friend class LWindow;

public:
	typedef LHashTbl<ConstStrKey<char>, LWindowsClass*> ClassContainer;

protected:
	// private member vars
	class LAppPrivate *d = NULL;
	
	#if defined LGI_SDL
	
		void OnSDLEvent(LMessage *m);
	
	#elif defined LGI_COCOA
	
		LAutoPtr<LMenu> Default;
	
	#elif defined WIN32

		CRITICAL_SECTION StackTraceSync;
		friend LONG __stdcall _ExceptionFilter_Redir(LPEXCEPTION_POINTERS e);
		LONG __stdcall _ExceptionFilter(LPEXCEPTION_POINTERS e, char *ProductId);
		friend class LWindowsClass;
		ClassContainer *GetClasses();

	#elif defined LINUX
	
		friend class LClipBoard;
		
		// virtual void OnEvents();
		void DeleteMeLater(LViewI *v);
		void SetClipBoardContent(OsView Hnd, LVariant &v);
		bool GetClipBoardContent(OsView Hnd, LVariant &v, LArray<char*> &Types);
	
	#endif

	void CommonCleanup();

	friend class LMouseHook;
	static LMouseHook *MouseHook;

public:
	// Static publics
	#if defined(LINUX)

		constexpr static const char *CfgLinuxKeysShift       = "Linux.Keys.Shift";
		constexpr static const char *CfgLinuxKeysCtrl        = "Linux.Keys.Ctrl";
		constexpr static const char *CfgLinuxKeysAlt         = "Linux.Keys.Alt";
		constexpr static const char *CfgLinuxKeysSystem      = "Linux.Keys.System";

		constexpr static const char *CfgLinuxMouseLeft       = "Linux.Mouse.Left";
		constexpr static const char *CfgLinuxMouseMiddle     = "Linux.Mouse.Middle";
		constexpr static const char *CfgLinuxMouseRight      = "Linux.Mouse.Right";
		constexpr static const char *CfgLinuxMouseBack       = "Linux.Mouse.Back";
		constexpr static const char *CfgLinuxMouseForward    = "Linux.Mouse.Forward";

	#elif defined(HAIKU)

		constexpr static const char *CfgNetworkHttpProxy     = "Network.HttpProxy";
		constexpr static const char *CfgNetworkHttpsProxy    = "Network.HttpsProxy";
		constexpr static const char *CfgNetworkSocks5Proxy   = "Network.Socks5Proxy";

	#endif

	constexpr static const char *CfgFontsGlyphSub            = "Fonts.GlyphSub";
	constexpr static const char *CfgFontsPointSizeOffset     = "Fonts.PointSizeOffset";
	constexpr static const char *CfgFontsSystemFont          = "Fonts.SystemFont";
	constexpr static const char *CfgFontsBoldFont            = "Fonts.BoldFont";
	constexpr static const char *CfgFontsMonoFont            = "Fonts.MonoFont";
	constexpr static const char *CfgFontsSmallFont           = "Fonts.SmallFont";
	constexpr static const char *CfgFontsCaptionFont         = "Fonts.CaptionFont";
	constexpr static const char *CfgFontsMenuFont            = "Fonts.MenuFont";
	
	/// Use 'LAppInst' to return a pointer to the LApp object
	static LApp *ObjInstance();
	static class LSkinEngine *SkinEngine;

	// public member vars

	#ifdef HAIKU
		// Due to more than one thread needing a system font, there needs
		// to be different font instances for each thread...
		LFont *GetFont(bool bold);
	#else		
		/// The system font
		LFont *SystemNormal = NULL;
		
		/// The system font in bold
		LFont *SystemBold = NULL;
	#endif
	
	/// Pointer to the applications main window
	LWindow *AppWnd = NULL;

	/// Returns true if the LApp object initialized correctly
	bool IsOk();
	
	/// Returns this processes ID
	OsProcessId GetProcessId();
	
	/// Returns the thread currently running the active message loop
	OsThread _GetGuiThread();
	OsThreadId GetGuiThreadId();
	bool InThread();
	
	/// Returns the number of CPU cores the machine has
	int GetCpuCount();

	/// Construct the object
	LApp
	(
		/// The arguments passed in by the OS.
		OsAppArguments &AppArgs,
		/// The application's name.
		const char *AppName,
		/// Optional args
		LAppArguments *ObjArgs = 0
	);

	/// Destroys the object
	virtual ~LApp();

	/// Resets the arguments
	void SetAppArgs(OsAppArguments &AppArgs);
	
	/// Returns the arguemnts
	OsAppArguments *GetAppArgs();

	/// Returns the n'th argument as a heap string. Free with DeleteArray(...).
	const char *GetArgumentAt(int n);
	
	/// Enters the message loop.
	bool Run
	(
		/// Idle callback
		OnIdleProc IdleCallback = NULL,
		/// Param for IdleCallback
		void *IdleParam = NULL
	);
	
	/// Event called to process the command line
	void OnCommandLine();
	
	/// Event called to process files dropped on the application
	void OnReceiveFiles(LArray<const char*> &Files);

	/// Event called to process URLs given to the application
	void OnUrl(const char *Url);
	
	/// Exits the event loop with the code specified
	void Exit
	(
		/// The application exit code.
		int Code = 0
	);
	
	/// \brief Parses the command line for a switch
	/// \return true if the option exists.
	bool GetOption
	(
		/// The option to look for.
		const char *Option,
		/// String to receive the value (if any) of the option
		LString &Value
	);

	/// \brief Parses the command line for a switch
	/// \return true if the option exists.
	bool GetOption
	(
		/// The option to look for.
		const char *Option,
		/// The buffer to receive the value of the command line parameter or NULL if you don't care.
		char *Dst = NULL,
		/// The buffer size in bytes
		int DstSize = 0
	);

	/// Return the path to the Lgi config file... (not the same as the application options, more global Lgi apps settings)
	LString GetConfigPath();
	
	/// Gets the application conf stored in lgi.conf
	LString GetConfig(const char *Variable);

	/// Sets a single tag in the config. (Not written to disk)
	void SetConfig(const char *Variable, const char *Value);

	/// Gets the control with the keyboard focus
	LViewI *GetFocus();
	
	/// Gets the MIME type of a file
	/// \returns the mime type or NULL if unknown.
	LString GetFileMimeType
	(
		/// The file to identify
		const char *File
	);

    /// Gets the applications that can handle a file of a certain mime type
	bool GetAppsForMimeType(const char *Mime, LArray<LAppInfo> &Apps);
		
	/// Get a system metric
	int32 GetMetric
	(
		/// One of #LGI_MET_DECOR_X, #LGI_MET_DECOR_Y
		LSystemMetric Metric
	);

	/// Get the mouse hook instance
	LMouseHook *GetMouseHook();

	/// Gets the singleton symbol lookup class
	class LSymLookup *GetSymLookup();

	/// \returns true if the process is running with elevated permissions
	bool IsElevated();

	/// Gets the font cache
	class LFontCache *GetFontCache();

	// OS Specific
	#if defined(LGI_SDL)

		/// This keeps track of the dirty rectangle and issues a M_INVALIDATE
		/// event when needed to keep the screen up to date.
		bool InvalidateRect(LRect &r);
		
		/// Push a new window to the top of the window stack
		bool PushWindow(LWindow *w);

		/// Remove the top most window
		LWindow *PopWindow();

		/// Sets up mouse tracking beyond the current window...
		void CaptureMouse(bool capture);
		
		/// Returns the freetype version as a string.
		LString GetFreetypeVersion();
	
	#elif defined(WIN32)

		HINSTANCE GetInstance();
		int GetShow();

		/// \returns true if the application is running under Wine on Linux. This is useful to know
		/// if you need to work around missing functionality in the Wine implementation.
		bool IsWine();

	#elif defined(LINUX)
	
		class LLibrary *GetWindowManagerLib();

		class DesktopInfo
		{
			friend class LApp;
			
			LString File;
			bool Dirty;
			
			struct KeyPair
			{
				LString Key, Value;
			};
			struct Section
			{
				LString Name;
				LArray<KeyPair> Values;
				
				KeyPair *Get(const char *Name, bool Create, bool &Dirty)
				{
					for (unsigned i=0; i<Values.Length(); i++)
					{
						KeyPair *kp = &Values[i];
						if (kp->Key.Equals(Name))
							return kp;
					}
					if (Create)
					{
						KeyPair *kp = &Values.New();
						kp->Key = Name;
						Dirty = true;
						return kp;
					}
					return NULL;
				}
			};
			LArray<Section> Data;
			
			bool Serialize(bool Write);
			Section *GetSection(const char *Name, bool Create);
		
		public:			
			DesktopInfo(const char *file);
			
			LString Get(const char *Field, const char *Section = NULL);
			bool Set(const char *Field, const char *Value, const char *Section = NULL);
			bool Update() { return Dirty ? Serialize(true) : true; }
			const char *GetFile() { return File; }
		};

		DesktopInfo *GetDesktopInfo();
		bool SetApplicationIcon(const char *FileName);
	
	#elif LGI_COCOA && defined(__OBJC__)
	
		OsApp &Handle();
		
	#endif

	#ifdef __GTK_H__
		
		struct KeyModFlags
		{
			int Shift, Alt, Ctrl, System;
			bool Debug;
			
			KeyModFlags()
			{
				Shift = 0;
				Alt = 0;
				Ctrl = 0;
				System = 0;
				Debug = false;
			}
			
			const char *FlagName(int Flag); // Single flag to string
			int FlagValue(const char *Name); // Single name to bitmask
			LString FlagsToString(int Flags); // Turn multiple flags to string
		};	
		
		KeyModFlags *GetKeyModFlags();
		void OnDetach(LViewI *View);
	
	
	#endif
	
	#if !LGI_VIEW_HANDLE
		bool PostEvent(LViewI *View, int Msg, LMessage::Param a = 0, LMessage::Param b = 0);
	#endif
};

#endif
