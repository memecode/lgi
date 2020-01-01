//
//      FILE:           LgiOsDefs.h (Mac)
//      AUTHOR:         Matthew Allen
//      DATE:           1/1/2013
//      DESCRIPTION:    Lgi cocoa header
//
//      Copyright (C) 2013, Matthew Allen
//              fret@memecode.com
//

#ifndef __LGI_MAC_OS_DEFS_H
#define __LGI_MAC_OS_DEFS_H

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "LgiInc.h"
#include "LgiDefs.h"
#include "GAutoPtr.h"
#include "pthread.h"

//////////////////////////////////////////////////////////////////
// Includes
#include "LgiInc.h"

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

//////////////////////////////////////////////////////////////////
// Typedefs
#include "ObjCWrapper.h"
ObjCWrapper(NSWindow, OsWindow)
ObjCWrapper(NSView,   OsView)

typedef pthread_t           OsThread;
typedef uint64_t			OsThreadId;
typedef pthread_mutex_t		OsSemaphore;
typedef uint16_t			OsChar;
typedef int					OsProcessId;
typedef int					OsProcess;
typedef CGContextRef		OsPainter;
typedef CGContextRef		OsBitmap;
typedef CTFontRef			OsFont;

#include "LgiClass.h"
#include "GMessage.h"

class OsAppArguments
{
	struct OsAppArgumentsPriv *d;

public:
	int Args;
	const char **Arg;

	OsAppArguments(int args = 0, const char **arg = 0);
	~OsAppArguments();

	void Set(char *CmdLine);
	OsAppArguments &operator =(OsAppArguments &a);
};

//////////////////////////////////////////////////////////////////
// Defines
#define _stricmp					strcasecmp
#define _strnicmp					strncasecmp

// Text system
#define USE_CORETEXT				1

// System defines
#define POSIX						1
#define LGI_COCOA					1
#define LGI_64BIT					1
#define LGI_VIEW_HANDLE				0 // GViews DON'T have individual OsView handles
#define LGI_VIEW_HASH				1 // GView DO have a hash table for validity

// Process
typedef int							OsProcess;
typedef int							OsProcessId;
#define LgiGetCurrentProcess()		getpid()

// Threads
#define LgiGetCurrentThread()		pthread_self()
LgiFunc OsThreadId					GetCurrentThreadId();

// Sockets
#define ValidSocket(s)				((s)>=0)
#define INVALID_SOCKET				-1
typedef int OsSocket;

// Sleep the current thread
LgiFunc void LgiSleep(uint32_t i);

// Run the message loop to process any pending messages
#define LgiYield()					GApp::ObjInstance()->Run(false)

#define LGI_GViewMagic				0x14412662
#define LGI_FileDropFormat			"public.file-url"
#define LGI_StreamDropFormat		"com.apple.pasteboard.promised-file-url"
#define LGI_LgiDropFormat			"lgi "
#define LGI_WideCharset				"utf-32"
#define LPrintfInt64				"%lli"
#define LPrintfHex64				"%llx"
#define LPrintfSizeT				"%zu"
#define LPrintfSSizeT				"%zi"
#define atoi64						atoll
#define sprintf_s					snprintf
#define vsprintf_s					vsnprintf
#define LGI_IllegalFileNameChars	"/" // FIXME: what other characters should be in here?
#define LGI_EXECUTABLE_EXT			"" // Empty

// Window flags
#define GWF_VISIBLE					0x00000001
#define GWF_DISABLED				0x00000002
#define GWF_FOCUS					0x00000004
#define GWF_OVER					0x00000008
#define GWF_DROP_TARGET				0x00000010
#define GWF_SUNKEN					0x00000020
#define GWF_FLAT					0x00000040
#define GWF_RAISED					0x00000080
#define GWF_BORDER					0x00000100
#define GWF_DIALOG					0x00000200
#define GWF_DESTRUCTOR				0x00000400
#define GWF_QUIT_WND				0x00000800

// Menu flags
#define ODS_SELECTED				0x1
#define ODS_DISABLED				0x2
#define ODS_CHECKED					0x4

/// Edge type: Sunken
#define SUNKEN						1
/// Edge type: Raised
#define RAISED						2
/// Edge type: Chiseled
#define CHISEL						3
/// Edge type: Flat
#define FLAT						4

/// The directory separator character on Linux as a char
#define DIR_CHAR					'/'
/// The directory separator character on Linux as a string
#define DIR_STR						"/"
/// The standard end of line string for Linux
#define EOL_SEQUENCE				"\n"
/// Tests a char for being a slash
#define IsSlash(c)					(((c)=='/')||((c)=='\\'))
/// Tests a char for being a quote
#define IsQuote(c)					(((c)=='\"')||((c)=='\''))
/// The path list separator character for Linux
#define LGI_PATH_SEPARATOR			":"
/// The pattern that matches all files in Linux
#define LGI_ALL_FILES				"*"
/// The stardard extension for dynamically linked code
#define LGI_LIBRARY_EXT				"dylib"

enum LDialogIds
{
	/// Standard ID for an "Ok" button.
	/// \sa LgiMsg
	IDOK = 1,
	/// Standard ID for a "Cancel" button.
	/// \sa LgiMsg
	IDCANCEL,
	/// Standard ID for a "Yes" button.
	/// \sa LgiMsg
	IDYES,
	/// Standard ID for a "No" button.
	/// \sa LgiMsg
	IDNO,

	/// Standard message box with an Ok button.
	/// \sa LgiMsg
	MB_OK,
	/// Standard message box with Ok and Cancel buttons.
	/// \sa LgiMsg
	MB_OKCANCEL,
	/// Standard message box with Yes and No buttons.
	/// \sa LgiMsg
	MB_YESNO,
	/// Standard message box with Yes, No and Cancel buttons.
	/// \sa LgiMsg
	MB_YESNOCANCEL,
};

#define MB_SYSTEMMODAL				0x1000

/// The CTRL key is pressed
/// \sa GKey
#define LGI_VKEY_CTRL				0x001
/// The ALT key is pressed
/// \sa GKey
#define LGI_VKEY_ALT				0x002
/// The SHIFT key is pressed
/// \sa GKey
#define LGI_VKEY_SHIFT				0x004

/// The left mouse button is pressed
/// \sa GMouse
#define LGI_VMOUSE_LEFT				0x008
/// The middle mouse button is pressed
/// \sa GMouse
#define LGI_VMOUSE_MIDDLE			0x010
/// The right mouse button is pressed
/// \sa GMouse
#define LGI_VMOUSE_RIGHT			0x020
/// The ctrl key is pressed
/// \sa GMouse
#define LGI_VMOUSE_CTRL				0x040
/// The alt key is pressed
/// \sa GMouse
#define LGI_VMOUSE_ALT				0x080
/// The shift key is pressed
/// \sa GMouse
#define LGI_VMOUSE_SHIFT			0x100
/// The mouse event is a down click
/// \sa GMouse
#define LGI_VMOUSE_DOWN				0x200
/// The mouse event is a double click
/// \sa GMouse
#define LGI_VMOUSE_DOUBLE			0x400

#define MacKeyDef() \
	_(A, 0) _(B, 11) _(C, 8) _(D, 2) _(E, 14) _(F, 3) \
	_(G, 5) _(H, 4) _(I, 34) _(J, 38) _(K, 40) _(L, 37) \
	_(M, 46) _(N, 45) _(O, 31) _(P, 35) _(Q, 12) _(R, 15) \
	_(S, 1) _(T, 17) _(U, 32) _(V, 9) _(W, 13) _(X, 7) _(Y, 16) _(Z, 6) \
	_(KEY1, 18) _(KEY2, 19) _(KEY3, 20) _(KEY4, 21) _(KEY5, 23) \
	_(KEY6, 22) _(KEY7, 26) _(KEY8, 28) _(KEY9, 25) _(KEY0, 29) \
	_(MINUS, 27) \
	_(EQUALS, 24) \
	_(CLOSEBRACKET, 30) \
	_(OPENBRACKET, 33) \
	_(ENTER, 36) \
	_(SINGLEQUOTE, 39) \
	_(BACKSLASH, 42) \
	_(COLON, 41) \
	_(LESSTHAN, 43) _(GREATERTHAN, 47) \
	_(FORWARDSLASH, 44) \
	_(TAB, 48) \
	_(SPACE, 49) \
	_(TILDE, 50) \
	_(BACKSPACE, 51) \
	_(ESCAPE, 53) \
	_(CMDRIGHT, 54)_(CMDLEFT, 55) \
	_(SHIFTLEFT, 56) _(SHIFTRIGHT, 60) \
	_(CAPSLOCK, 57) \
	_(ALTLEFT, 58) _(ALTRIGHT, 61) \
	_(CTRLLEFT, 59) _(CTRLRIGHT, 62) \
	_(KEYPADPERIOD, 65) \
	_(KEYPADASTERISK, 67) \
	_(KEYPADPLUS, 69) \
	_(KEYPADENTER, 76) \
	_(KEYPADNUMLOCK, 71) \
	_(KEYPADFORWARDSLASH, 75) \
	_(KEYPADMINUS, 78) _(KEYPADEQUALS, 81) \
	_(KEYPAD0, 82) _(KEYPAD1, 83) _(KEYPAD2, 84) _(KEYPAD3, 85) _(KEYPAD4, 86) \
	_(KEYPAD5, 87) _(KEYPAD6, 88) _(KEYPAD7, 89) _(KEYPAD8, 91) _(KEYPAD9, 92) \
	_(F1, 122) _(F2, 120) _(F3, 99) _(F4, 118) _(F5, 96) _(F6, 97) \
	_(F7, 98) _(F8, 100) _(F9, 101) _(F10, 109) _(F11, 103) _(F12, 111) \
	_(PRINTSCREEN, 105) \
	_(CONTEXTMENU, 110) \
	_(INSERT, 114) \
	_(HOME, 115) \
	_(PAGEUP, 116) \
	_(DELETE, 117) \
	_(END, 119) \
	_(PAGEDOWN, 121) \
	_(LEFT, 123) _(RIGHT, 124) _(DOWN, 125) _(UP, 126)

#ifdef __OBJC__
#define _KEY(sym,val) sym
#else
#define _KEY(sym,val) val
#endif

// Keys
enum LVirtualKeys
{
	#define _(k, v) LK_ ##k = v,
	MacKeyDef()
	#undef _
	
	LK_RETURN = LK_ENTER,
	LK_APPS = 0x1000,
	
	/*
	LK_TAB = '\t',
	LK_RETURN = '\r',
	LK_ESCAPE = 27,
	LK_SPACE = ' ',
	LK_BACKSPACE = 0x7F,

	LK_PAGEUP = _KEY(NSPageUpFunctionKey, 0xF72C),
	LK_PAGEDOWN = _KEY(NSPageDownFunctionKey, 0xF72D),
	LK_DELETE = _KEY(NSDeleteFunctionKey, 0xF728),
	LK_HOME = _KEY(NSHomeFunctionKey, 0xF729),
	LK_END = _KEY(NSEndFunctionKey, 0xF72B),
	LK_UP = _KEY(NSUpArrowFunctionKey, 0xF700),
	LK_DOWN = _KEY(NSDownArrowFunctionKey, 0xF701),
	LK_LEFT = _KEY(NSLeftArrowFunctionKey, 0xF702),
	LK_RIGHT = _KEY(NSRightArrowFunctionKey, 0xF703),

	LK_F1 = _KEY(NSF1FunctionKey, 0xF704),
	LK_F2 = _KEY(NSF2FunctionKey, 0xF705),
	LK_F3 = _KEY(NSF3FunctionKey, 0xF706),
	LK_F4 = _KEY(NSF4FunctionKey, 0xF707),
	LK_F5 = _KEY(NSF5FunctionKey, 0xF708),
	LK_F6 = _KEY(NSF6FunctionKey, 0xF709),
	LK_F7 = _KEY(NSF7FunctionKey, 0xF70A),
	LK_F8 = _KEY(NSF8FunctionKey, 0xF70B),
	LK_F9 = _KEY(NSF9FunctionKey, 0xF70C),
	LK_F10 = _KEY(NSF10FunctionKey, 0xF70D),
	LK_F11 = _KEY(NSF11FunctionKey, 0xF70E),
	LK_F12 = _KEY(NSF12FunctionKey, 0xF70F),

	LK_INSERT = 0xF748,
	LK_APPS,
	LK_ENTER,
	LK_SHIFT,
	*/
};

#undef _KEY

LgiFunc const char *LVirtualKeyToString(LVirtualKeys c);

/////////////////////////////////////////////////////////////////////////////////////
// Externs
LgiFunc GView *GWindowFromHandle(OsView hWnd);
LgiFunc int GetMouseWheelLines();
LgiFunc int WinPointToHeight(int Pt);
LgiFunc int WinHeightToPoint(int Ht);
LgiFunc int stricmp(const char *a, const char *b);
LgiFunc char *strlwr(char *a);
LgiFunc char *strupr(char *a);
LgiFunc char *p2c(unsigned char *s);

/// Convert a string d'n'd format to an OS dependant integer.
LgiFunc int FormatToInt(char *s);
/// Convert a Os dependant integer d'n'd format to a string.
LgiFunc char *FormatToStr(int f);


#endif
