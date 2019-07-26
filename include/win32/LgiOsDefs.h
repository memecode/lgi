//
//      FILE:           LgiOsDefs.h (Win32)
//      AUTHOR:         Matthew Allen
//      DATE:           24/9/99
//      DESCRIPTION:    Lgi Win32 OS defines
//
//      Copyright (C) 1999, Matthew Allen
//              fret@memecode.com
//

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#pragma warning(disable : 4251 4275)

#define WIN32GTK					0
#define WINNATIVE					1
#ifndef WINDOWS
	#error "Define WINDOWS in your project"
#endif
#ifdef _WIN64
	#define LGI_64BIT				1
	#ifndef WIN64
		#define WIN64				1
	#endif
#else
	#define LGI_32BIT				1
	#ifndef WIN32
		#define WIN32				1
	#endif
#endif

#include <string.h>
#include <stdio.h>
#include "LgiInc.h"
#include "LgiDefs.h"
#include "LgiClass.h"

#define _MSC_VER_VS2019	2000 // MSVC++ 16.0 (speculative)
#define _MSC_VER_VS2017	1910 // MSVC++ 15.0
#define _MSC_VER_VS2015	1900 // MSVC++ 14.0
#define _MSC_VER_VS2013	1800 // MSVC++ 12.0
#define _MSC_VER_VS2012	1700 // MSVC++ 11.0
#define _MSC_VER_VS2010	1600 // MSVC++ 10.0
#define _MSC_VER_VS2008	1500 // MSVC++ 9.0
#define _MSC_VER_VS2005	1400 // MSVC++ 8.0
#define _MSC_VER_VS2003	1310 // MSVC++ 7.1
#define _MSC_VER_VC7	1300 // MSVC++ 7.0
#define _MSC_VER_VC6	1200 // MSVC++ 6.0
#define _MSC_VER_VC5	1100 // MSVC++ 5.0

#if _MSC_VER >= _MSC_VER_VS2015
#define _MSC_VER_STR	"14"
#elif _MSC_VER >= _MSC_VER_VS2013
#define _MSC_VER_STR	"12"
#elif _MSC_VER >= _MSC_VER_VS2012
#define _MSC_VER_STR	"11"
#elif _MSC_VER >= _MSC_VER_VS2010
#define _MSC_VER_STR	"10"
#else
#define _MSC_VER_STR	"9"
#endif

//////////////////////////////////////////////////////////////////
// Includes
#define WIN32_LEAN_AND_MEAN
#include "winsock2.h"

#include "windows.h"
#include "SHELLAPI.H"
#include "COMMDLG.H"
#include "LgiInc.h"
#include <crtdefs.h>

//////////////////////////////////////////////////////////////////
// Typedefs
typedef HWND				OsWindow;
typedef HWND				OsView;
typedef HANDLE				OsThread;
typedef HANDLE				OsProcess;
typedef char16				OsChar;
typedef HBITMAP				OsBitmap;
typedef HDC					OsPainter;
typedef HFONT				OsFont;
#if _MSC_VER <= _MSC_VER_VC6
typedef unsigned long		ULONG_PTR, *PULONG_PTR;
#define sprintf_s			_snprintf
#endif

typedef BOOL (__stdcall *pSHGetSpecialFolderPathA)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);
typedef BOOL (__stdcall *pSHGetSpecialFolderPathW)(HWND hwndOwner, LPWSTR lpszPath, int nFolder, BOOL fCreate);
typedef int (__stdcall *pSHFileOperationA)(LPSHFILEOPSTRUCTA lpFileOp);
typedef int (__stdcall *pSHFileOperationW)(LPSHFILEOPSTRUCTW lpFileOp);
typedef int (__stdcall *p_vscprintf)(const char *format, va_list argptr);

#include "GMessage.h"

class LgiClass OsAppArguments
{
	GAutoWString CmdLine;

	void _Default();

public:
	HINSTANCE hInstance;
	DWORD Pid;
	char16 *lpCmdLine;
	int nCmdShow;

	OsAppArguments();
	OsAppArguments(int Args, char **Arg);

	OsAppArguments &operator =(OsAppArguments &p);

	void Set(char *Utf);
	void Set(int Args, char **Arg);
};

//////////////////////////////////////////////////////////////////
// Defines
#define IsWin9x							(GApp::Win9x)
#define DefaultOsView(t)				NULL

#define GWL_LGI_MAGIC					8
#define GWL_EXTRA_BYTES					12

// Key redefs
enum LgiKeys {
	LK_TAB = VK_TAB,
	LK_RETURN = VK_RETURN,
	LK_SPACE = VK_SPACE,
	LK_DELETE = VK_DELETE,
	LK_ESCAPE = VK_ESCAPE,
	LK_PAGEUP = VK_PRIOR,
	LK_PAGEDOWN = VK_NEXT,
	LK_BACKSPACE = VK_BACK,
	LK_F1 = VK_F1,
	LK_F2 = VK_F2,
	LK_F3 = VK_F3,
	LK_F4 = VK_F4,
	LK_F5 = VK_F5,
	LK_F6 = VK_F6,
	LK_F7 = VK_F7,
	LK_F8 = VK_F8,
	LK_F9 = VK_F9,
	LK_F10 = VK_F10,
	LK_F11 = VK_F11,
	LK_F12 = VK_F12,
	LK_LEFT = VK_LEFT,
	LK_RIGHT = VK_RIGHT,
	LK_UP = VK_UP,
	LK_DOWN = VK_DOWN,
	LK_HOME = VK_HOME,
	LK_END = VK_END,
	LK_INSERT = VK_INSERT,
	LK_SHIFT = VK_SHIFT,
	LK_ALT = VK_MENU,
	LK_CTRL = VK_CONTROL,
	LK_DECIMAL = VK_DECIMAL,

	LK_COMMA			= VK_OEM_COMMA, // , .
	LK_MINUS			= VK_OEM_MINUS, // - _
	LK_EQUALS			= VK_OEM_PLUS, // = +
	LK_SEMI_COLON		= VK_OEM_1, // ; :
	LK_SLASH			= VK_OEM_2, // / ?
	LK_TILDE			= VK_OEM_3, // ~
	LK_OPEN_BRACKET		= VK_OEM_4, // [ {
	LK_BACK_SLASH		= VK_OEM_5, // \ |
	LK_CLOSE_BRACKET	= VK_OEM_6, // ] }
	LK_SINGLE_QUOTE		= VK_OEM_7, // ' "
};

// Sleep the current thread
LgiFunc void LgiSleep(DWORD i);

// Process
typedef DWORD							OsProcessId;
LgiExtern HINSTANCE						_lgi_app_instance;
#define LgiProcessInst()				_lgi_app_instance
extern p_vscprintf						lgi_vscprintf;
#define LgiGetCurrentProcess()			GetCurrentProcessId()

// Threads
typedef DWORD							OsThreadId;
typedef CRITICAL_SECTION				OsSemaphore;
#define LgiGetCurrentThread()			GetCurrentThread()
// #define GetCurrentThreadId()			GetCurrentThreadId()

// Socket/Network
#define ValidSocket(s)					((s) != INVALID_SOCKET)
typedef SOCKET							OsSocket;

// Run the message loop to process any pending messages
#define LgiYield()						GApp::ObjInstance()->Run(false)

#define LGI_GViewMagic					0x14412662
#define LGI_FileDropFormat				"CF_HDROP"
#define LGI_StreamDropFormat			CFSTR_FILEDESCRIPTORW
#define LGI_WideCharset					"ucs-2"
#define LPrintfInt64					"%I64i"
#define LPrintfHex64					"%I64x"
#if LGI_64BIT
	#define LPrintfSizeT				"%I64u"
	#define LPrintfSSizeT				"%I64d"
#else
	#define LPrintfSizeT				"%u"
	#define LPrintfSSizeT				"%d"
#endif
#define LGI_IllegalFileNameChars		"\t\r\n/\\:*?\"<>|"

#define MK_LEFT							MK_LBUTTON
#define MK_RIGHT						MK_RBUTTON
#define MK_MIDDLE						MK_MBUTTON
#define MK_CTRL							MK_CONTROL
	
// Stupid mouse wheel defines don't work. hmmm
#define WM_MOUSEWHEEL					0x020A
#define WHEEL_DELTA						120
#ifndef SPI_GETWHEELSCROLLLINES
#define SPI_GETWHEELSCROLLLINES			104
#endif

// Window flags
#define GWF_VISIBLE						WS_VISIBLE
#define GWF_DISABLED					WS_DISABLED
#define GWF_BORDER						WS_BORDER
#define GWF_TABSTOP						WS_TABSTOP
#define GWF_FOCUS						0x00000001
#define GWF_OVER						0x00000002
#define GWF_DROP_TARGET					0x00000004
#define GWF_SUNKEN						0x00000008
#define GWF_FLAT						0x00000010
#define GWF_RAISED						0x00000020
#define GWF_DIALOG						0x00000040
#define GWF_DESTRUCTOR					0x00000080
#define GWF_QUIT_WND					0x00000100
#define GWF_SYS_BORDER					0x00000200 // ask the system to draw the border

// Widgets
#define DialogToPixelX(i)				(((i)*Bx)/4)
#define DialogToPixelY(i)				(((i)*By)/8)
#define PixelToDialogX(i)				(((i)*4)/Bx)
#define PixelToDialogY(i)				(((i)*8)/By)

#define DIALOG_X						1.56
#define DIALOG_Y						1.85
#define CTRL_X							1.50
#define CTRL_Y							1.64

// Directories
#define DIR_CHAR						'\\'
#define DIR_STR							"\\"
#define EOL_SEQUENCE					"\r\n"

#define LGI_PATH_SEPARATOR				";"
#define LGI_ALL_FILES					"*.*"
#define LGI_LIBRARY_EXT					"dll"
#define LGI_EXECUTABLE_EXT				".exe"

/////////////////////////////////////////////////////////////////////////////////////
// Typedefs
typedef HWND OsView;

/////////////////////////////////////////////////////////////////////////////////////
// Externs
LgiFunc class GViewI *GWindowFromHandle(OsView hWnd);
LgiFunc int GetMouseWheelLines();
LgiFunc int WinPointToHeight(int Pt, HDC hDC = NULL);
LgiFunc int WinHeightToPoint(int Ht, HDC hDC = NULL);
LgiExtern class GString WinGetSpecialFolderPath(int Id);

/// Convert a string d'n'd format to an OS dependant integer.
LgiFunc int FormatToInt(GString s);
/// Convert a Os dependant integer d'n'd format to a string.
LgiFunc char *FormatToStr(int f);
extern bool LgiToWindowsCursor(OsView Hnd, LgiCursor Cursor);

#ifdef _MSC_VER
#define snprintf					_snprintf
//#define vsnprintf					_vsnprintf
#define vsnwprintf					_vsnwprintf
#define stricmp						_stricmp
#define strlwr						_strlwr
#define strnicmp					_strnicmp
#define vsnprintf					_vsnprintf_s
// #define getcwd						_getcwd
#endif
#define atoi64						_atoi64

#ifdef __GNUC__
// #define stricmp					strcasecmp
// #define strnicmp					strncasecmp
#define vsnprintf_s vsnprintf
#define swprintf_s snwprintf
#define vsprintf_s vsnprintf
#if !defined(_TRUNCATE)
#define _TRUNCATE ((size_t)-1)
#endif
#endif

#endif
