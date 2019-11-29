#ifndef _GAPP_H_
#define _GAPP_H_

/////////////////////////////////////////////////////////////////////////////////
#if WINNATIVE
typedef DWORD						OsProcessId;
#else
typedef int							OsProcessId;
#endif

/// Returns the current process ID
#define LgiProcessId()				(LgiApp->GetProcessId())

/// Returns a pointer to the GApp object.
///
/// \warning Don't use this before you have created your GApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#define LgiApp						(GApp::ObjInstance())

/// Returns a system font pointer.
///
/// \warning Don't use this before you have created your GApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#define SysFont						(LgiApp->SystemNormal)

/// Returns a bold system font pointer.
///
/// \warning Don't use this before you have created your GApp object. i.e. in a constructor
/// of a global static class which is initialized before the main begins executing.
#define SysBold						(LgiApp->SystemBold)

/// Exits the application right now!
///
/// \warning This will cause data loss if you have any unsaved data. Equivilant to exit(0).
LgiFunc void LgiExitApp();

/// Closes the application gracefully.
///
/// This actually causes GApp::Run() to stop processing message and return.
#define LgiCloseApp()				LgiApp->Exit(false)

#if defined(LINUX) && !defined(LGI_SDL)
#define ThreadCheck()				LgiAssert(InThread())
#else
#define ThreadCheck()
#endif

/// Optional arguments to the GApp object
struct GAppArguments
{
	/// Don't initialize the skinning engine.
	bool NoSkin;
};

class GAppPrivate;

#if LGI_COCOA && defined __OBJC__
#include "LCocoaView.h"

@interface LNsApplication : NSApplication
{
}

@property GAppPrivate *d;

- (id)init;
- (void)setPriv:(nonnull GAppPrivate*)priv;
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
class LgiClass GApp : virtual public GAppI,
	public GBase,
	public OsApplication
{
	friend class GView;
	friend class GWindow;

public:
	typedef LHashTbl<ConstStrKey<char>, GWin32Class*> ClassContainer;

protected:
	// private member vars
	class GAppPrivate *d;
	
	#if defined LGI_SDL
	
	void OnSDLEvent(GMessage *m);
	
	#elif defined LGI_COCOA
	
	GAutoPtr<LMenu> Default;
	
	#elif defined WIN32

	CRITICAL_SECTION StackTraceSync;
	friend LONG __stdcall _ExceptionFilter_Redir(LPEXCEPTION_POINTERS e);
	LONG __stdcall _ExceptionFilter(LPEXCEPTION_POINTERS e, char *ProductId);
	friend class GWin32Class;
	ClassContainer *GetClasses();

	#elif defined LINUX
	
	friend class GClipBoard;
	
	// virtual void OnEvents();
	void DeleteMeLater(GViewI *v);
	void SetClipBoardContent(OsView Hnd, GVariant &v);
	bool GetClipBoardContent(OsView Hnd, GVariant &v, GArray<char*> &Types);
	
	#endif

	friend class GMouseHook;
	static GMouseHook *MouseHook;

public:
	// Static publics
	
	/// Use 'LgiApp' to return a pointer to the GApp object
	static GApp *ObjInstance();
	static class GSkinEngine *SkinEngine;

	// public member vars
	
	/// The system font
	GFont *SystemNormal;
	
	/// The system font in bold
	GFont *SystemBold;
	
	/// Pointer to the applications main window
	GWindow *AppWnd;

	/// Returns true if the GApp object initialized correctly
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
	GApp
	(
		/// The arguments passed in by the OS.
		OsAppArguments &AppArgs,
		/// The application's name.
		const char *AppName,
		/// Optional args
		GAppArguments *ObjArgs = 0
	);

	/// Destroys the object
	virtual ~GApp();

	/// Resets the arguments
	void SetAppArgs(OsAppArguments &AppArgs);
	
	/// Returns the arguemnts
	OsAppArguments *GetAppArgs();

	/// Returns the n'th argument as a heap string. Free with DeleteArray(...).
	const char *GetArgumentAt(int n);
	
	/// Enters the message loop.
	bool Run
	(
		/// If true this function will return when the application exits (with LgiCloseApp()).
		/// Otherwise if false only pending events will be processed and then the function returns.
		bool Loop = true,
		/// Idle callback
		OnIdleProc IdleCallback = NULL,
		/// Param for IdleCallback
		void *IdleParam = NULL
	);
	
	/// Event called to process the command line
	void OnCommandLine();
	
	/// Event called to process files dropped on the application
	void OnReceiveFiles(GArray<char*> &Files);

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
		GString &Value
	);

	/// \brief Parses the command line for a switch
	/// \return true if the option exists.
	bool GetOption
	(
		/// The option to look for.
		const char *Option,
		/// The buffer to receive the value of the command line parameter or NULL if you don't care.
		char *Dst = 0,
		/// The buffer size in bytes
		int DstSize = 0
	);
	
	/// Gets the application conf stored in lgi.conf
	GXmlTag *GetConfig(const char *Tag);

	/// Sets a single tag in the config. (Not written to disk)
	void SetConfig(GXmlTag *Tag);

	/// Gets the control with the keyboard focus
	GViewI *GetFocus();
	
	/// Gets the MIME type of a file
	/// \returns the mime type or NULL if unknown.
	GString GetFileMimeType
	(
		/// The file to identify
		const char *File
	);

    /// Gets the applications that can handle a file of a certain mime type
	bool GetAppsForMimeType(char *Mime, GArray<GAppInfo*> &Apps);
		
	/// Get a system metric
	int32 GetMetric
	(
		/// One of #LGI_MET_DECOR_X, #LGI_MET_DECOR_Y
		LgiSystemMetric Metric
	);

	/// Get the mouse hook instance
	GMouseHook *GetMouseHook();

	/// Gets the singleton symbol lookup class
	class GSymLookup *GetSymLookup();

	/// \returns true if the process is running with elevated permissions
	bool IsElevated();

	/// Gets the font cache
	class GFontCache *GetFontCache();

	// OS Specific
	#if defined(LGI_SDL)

		/// This keeps track of the dirty rectangle and issues a M_INVALIDATE
		/// event when needed to keep the screen up to date.
		bool InvalidateRect(GRect &r);
		
		/// Push a new window to the top of the window stack
		bool PushWindow(GWindow *w);

		/// Remove the top most window
		GWindow *PopWindow();

		/// Sets up mouse tracking beyond the current window...
		void CaptureMouse(bool capture);
		
		/// Returns the freetype version as a string.
		GString GetFreetypeVersion();
	
	#elif defined(WIN32)

		HINSTANCE GetInstance();
		int GetShow();

		/// \returns true if the application is running under Wine on Linux. This is useful to know
		/// if you need to work around missing functionality in the Wine implementation.
		bool IsWine();

	#elif defined(LINUX)
	
		class GLibrary *GetWindowManagerLib();

		class DesktopInfo
		{
			friend class GApp;
			
			GString File;
			bool Dirty;
			
			struct KeyPair
			{
				GString Key, Value;
			};
			struct Section
			{
				GString Name;
				GArray<KeyPair> Values;
				
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
			GArray<Section> Data;
			
			bool Serialize(bool Write);
			Section *GetSection(const char *Name, bool Create);
		
		public:			
			DesktopInfo(const char *file);
			
			GString Get(const char *Field, const char *Section = NULL);
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
		void OnDetach(GViewI *View);
	#endif
	#if !LGI_VIEW_HANDLE
		bool PostEvent(GViewI *View, int Msg, GMessage::Param a = 0, GMessage::Param b = 0);
	#endif
};

#endif
