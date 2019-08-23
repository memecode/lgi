/**
	\file
	\author Matthew Allen
	\brief GTK specific types and defines.
 */

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#include <string.h>
#include "assert.h"
#include "LgiDefs.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>
#ifdef WIN32
	#include "winsock2.h"
	#define WIN32GTK                    1
	#ifdef _WIN64
		#define LGI_64BIT				1
	#else
		#define LGI_32BIT				1
	#endif

	#if 1
		#define _CRTDBG_MAP_ALLOC
		#include <crtdbg.h>
		#ifdef _DEBUG
		#define DEBUG_CLIENTBLOCK new( _CLIENT_BLOCK, __FILE__, __LINE__)
		#else
		#define DEBUG_CLIENTBLOCK
		#endif // _DEBUG
		#ifdef _DEBUG
		#define new DEBUG_CLIENTBLOCK
		#endif
	#endif

	#include <shlobj.h>
	#include <shellapi.h>
	typedef BOOL (__stdcall *pSHGetSpecialFolderPathA)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);
	typedef BOOL (__stdcall *pSHGetSpecialFolderPathW)(HWND hwndOwner, LPWSTR lpszPath, int nFolder, BOOL fCreate);
	typedef int (__stdcall *pSHFileOperationA)(LPSHFILEOPSTRUCTA lpFileOp);
	typedef int (__stdcall *pSHFileOperationW)(LPSHFILEOPSTRUCTW lpFileOp);
	typedef int (__stdcall *p_vscprintf)(const char *format, va_list argptr);
	LgiExtern class GString WinGetSpecialFolderPath(int Id);

#else
	#include <unistd.h>
	#define _MULTI_THREADED
	#include <pthread.h>
	//#define LINUX						1
	#define XP_CTRLS					1
	#define POSIX						1

	#ifdef __arm__
		#define LGI_RPI					1
		#define LGI_32BIT				1
	#elif __GNUC__
	#if __x86_64__ || __ppc64__
		#define LGI_64BIT				1
	#else
		#define LGI_32BIT				1
	#endif
	#endif
#endif
#define LGI_VIEW_HANDLE					0
#define LGI_VIEW_HASH                   1

#undef stricmp

#include "LgiInc.h"
#ifdef MAC
	#include <sys/stat.h>
	#include <netinet/in.h>
#endif
#define GtkVer(major, minor)			( (GTK_MAJOR_VERSION > major) || (GTK_MAJOR_VERSION == major && GTK_MINOR_VERSION >= minor) )
namespace Gtk {
#include <gtk/gtk.h>
#ifdef WIN32
	#if GtkVer(3, 22)
		#include <gdk/gdkwin32.h>
	#else
		#include <gdk/win32/gdkwin32.h>
	#endif
#endif
}

// System

#undef Status
#undef Success
#undef None
#undef Above
#undef Below

#define XStatus		int
#define XSuccess	0
#define XAbove		0
#define XBelow		1
#define XNone		0L

class LgiClass OsAppArguments
{
	struct OsAppArgumentsPriv *d;

public:
	int Args;
	char **Arg;

	OsAppArguments(int args, const char **arg);
	~OsAppArguments();

	void Set(char *CmdLine);
	OsAppArguments &operator =(OsAppArguments &a);
};

// Process
#ifdef WIN32
typedef HANDLE							OsProcess;
#else
typedef int								OsProcess;
#endif
typedef int								OsProcessId;
typedef Gtk::GtkWidget					*OsView;
typedef Gtk::GtkWindow					*OsWindow;
typedef char							OsChar;
typedef Gtk::cairo_t					*OsPainter;
typedef Gtk::PangoFontDescription       *OsFont;
typedef void							*OsBitmap;

#ifdef WIN32
#define LgiGetCurrentProcess			GetCurrentProcessId
#else
#define LgiGetCurrentProcess			getpid
#endif

// Because of namespace issues you can't use the built in GTK casting macros.
// So this is basically the same thing:
// e.g.
//		Gtk::GtkContainer *c = GtkCast(widget, gtk_container, GtkContainer);
//
#define GtkCast(obj, gtype, ctype)		((Gtk::ctype*)g_type_check_instance_cast( (Gtk::GTypeInstance*)obj, Gtk::gtype##_get_type() ))

class OsApplication
{
	class OsApplicationPriv *d;

protected:
	static OsApplication *Inst;
	
public:
	OsApplication(int Args, char **Arg);
	~OsApplication();
	
	static OsApplication *GetInst() { LgiAssert(Inst != NULL); return Inst; }
};

// Threads
#ifdef WIN32

typedef HANDLE  					OsThread;
typedef DWORD						OsThreadId;
typedef CRITICAL_SECTION			OsSemaphore;
#define LgiGetCurrentThread()		GetCurrentThread()

#else

typedef pthread_t					OsThread;
typedef pid_t						OsThreadId;
typedef pthread_mutex_t				OsSemaphore;
#define LgiGetCurrentThread()		pthread_self()
LgiFunc OsThreadId					GetCurrentThreadId();

#endif

#include "GMessage.h"

// Sockets
#define ValidSocket(s)				((s)>=0)
#ifndef WIN32
#define INVALID_SOCKET				-1
#endif
typedef int OsSocket;

/// Sleep the current thread for i milliseconds.
#ifdef WIN32
LgiFunc void LgiSleep(DWORD i);
#else
LgiFunc void LgiSleep(uint32_t i);
#endif

#ifndef WIN32
#define atoi64						atoll
#else
#define atoi64						_atoi64
#endif

#define _snprintf					snprintf
#define _vsnprintf					vsnprintf
#define wcscpy_s(dst, len, src)		wcsncpy(dst, src, len)

/// Process any pending messages in the applications message que and then return.
#define LgiYield()					LgiApp->Run(false)

#define K_CHAR						0x0

/// Drag and drop format for a file
#define LGI_FileDropFormat			"text/uri-list"
#define LGI_StreamDropFormat		"application/x-file-stream" // FIXME
#define LGI_IllegalFileNameChars	"\t\r\n/\\:*?\"<>|"
#ifdef WINDOWS
#define LGI_WideCharset				"ucs-2"
#else
#define LGI_WideCharset				"utf-32"
#endif

#ifdef _MSC_VER
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

	#define LPrintfInt64			"%I64i"
	#define LPrintfHex64			"%I64x"
	#if LGI_64BIT
		#define LPrintfSizeT		"%I64u"
		#define LPrintfSSizeT		"%I64d"
	#else
		#define LPrintfSizeT		"%u"
		#define LPrintfSSizeT		"%d"
	#endif
#else
	#define LPrintfInt64			"%lld"
	#define LPrintfHex64			"%llx"
	#define LPrintfSizeT			"%zu"
	#define LPrintfSSizeT			"%zi"
#endif

#ifndef SND_ASYNC
#define SND_ASYNC					0x0001
#endif

#define DOUBLE_CLICK_THRESHOLD		5
#define DOUBLE_CLICK_TIME			400

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
#ifndef WIN32
#define ODS_SELECTED				0x1
#define ODS_DISABLED				0x2
#define ODS_CHECKED					0x4
#endif

/// Edge type: Sunken
#define SUNKEN						1
/// Edge type: Raised
#define RAISED						2
/// Edge type: Chiseled
#define CHISEL						3
/// Edge type: Flat
#define FLAT						4

#ifdef WIN32
	/// The directory separator character on Linux as a char
	#define DIR_CHAR				'\\'
	/// The directory separator character on Linux as a string
	#define DIR_STR					"\\"
	/// The path list separator character for Linux
	#define LGI_PATH_SEPARATOR		";"
	/// The pattern that matches all files in Linux
	#define LGI_ALL_FILES			"*.*"
	/// The stardard extension for dynamically linked code
	#define LGI_LIBRARY_EXT			"dll"
	/// The standard executable extension
	#define LGI_EXECUTABLE_EXT		".exe"
#else
	/// The directory separator character on Linux as a char
	#define DIR_CHAR				'/'
	/// The directory separator character on Linux as a string
	#define DIR_STR					"/"
	/// The path list separator character for Linux
	#define LGI_PATH_SEPARATOR		":"
	/// The pattern that matches all files in Linux
	#define LGI_ALL_FILES			"*"
	/// The stardard extension for dynamically linked code
	#ifdef MAC
	#define LGI_LIBRARY_EXT			"dylib"
	#else
	#define LGI_LIBRARY_EXT			"so"
	#endif
	/// The standard executable extension
	#define LGI_EXECUTABLE_EXT		""
#endif
/// The standard end of line string for Linux
#define EOL_SEQUENCE				"\n"
/// Tests a char for being a slash
#define IsSlash(c)					(((c)=='/')||((c)=='\\'))
/// Tests a char for being a quote
#define IsQuote(c)					(((c)=='\"')||((c)=='\''))

#ifndef WIN32
/// ID's returned by LgiMsg.
/// \sa LgiMsg
enum MessageBoxResponse
{
	IDOK = 1,
	IDCANCEL = 2,
	IDABORT = 3,
	IDRETRY = 4,
	IDIGNORE = 5,
	IDYES = 6,
	IDNO = 7,
	IDTRYAGAIN = 10,
	IDCONTINUE = 11,
};

/// Standard message box types.
/// \sa LgiMsg
enum MessageBoxType
{
	MB_OK = 0,
	MB_OKCANCEL = 1,
	MB_ABORTRETRYIGNORE = 2,
	MB_YESNOCANCEL = 3,
	MB_YESNO = 4,
	MB_RETRYCANCEL = 5,
	MB_CANCELTRYCONTINUE = 6
};

#define MB_SYSTEMMODAL				0x1000
#endif

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


#define abs(a)						( (a) < 0 ? -(a) : (a) )

#if !defined(WINNATIVE)

	#include <gdk/gdkkeysyms-compat.h>

	typedef enum {
		/* The keyboard syms have been cleverly chosen to map to ASCII */
		LK_UNKNOWN		= 0,
		LK_FIRST		= 0,
		LK_BACKSPACE	= 8,
		LK_TAB			= 9,
		LK_CLEAR		= GDK_Clear,
		LK_RETURN		= 13,
		LK_PAUSE		= GDK_Pause,
		LK_ESCAPE		= GDK_Escape,
		LK_SPACE		= 32,
		LK_EXCLAIM		= 33,
		LK_QUOTEDBL		= 34,
		LK_HASH			= 35,
		LK_DOLLAR		= 36,
		LK_AMPERSAND	= 38,
		LK_QUOTE		= 39,
		LK_LEFTPAREN	= 40,
		LK_RIGHTPAREN	= 41,
		LK_ASTERISK		= 42,
		LK_PLUS			= 43,
		LK_COMMA		= 44,
		LK_MINUS		= 45,
		LK_PERIOD		= 46,
		LK_SLASH		= 47,
		LK_0			= 48,
		LK_1			= 49,
		LK_2			= 50,
		LK_3			= 51,
		LK_4			= 52,
		LK_5			= 53,
		LK_6			= 54,
		LK_7			= 55,
		LK_8			= 56,
		LK_9			= 57,
		LK_COLON		= 58,
		LK_SEMICOLON	= 59,
		LK_LESS			= 60,
		LK_EQUALS		= 61,
		LK_GREATER		= 62,
		LK_QUESTION		= 63,
		LK_AT			= 64,
		/* 
		   Skip uppercase letters
		 */
		LK_LEFTBRACKET	= GDK_bracketleft,
		LK_BACKSLASH	= GDK_backslash,
		LK_RIGHTBRACKET	= GDK_bracketright,
		LK_CARET		= 94,
		LK_UNDERSCORE	= GDK_underscore,
		LK_BACKQUOTE	= 96,
		LK_a			= GDK_a,
		LK_b			= GDK_b,
		LK_c			= GDK_c,
		LK_d			= GDK_d,
		LK_e			= GDK_e,
		LK_f			= GDK_f,
		LK_g			= GDK_g,
		LK_h			= GDK_h,
		LK_i			= GDK_i,
		LK_j			= GDK_j,
		LK_k			= GDK_k,
		LK_l			= GDK_l,
		LK_m			= GDK_m,
		LK_n			= GDK_n,
		LK_o			= GDK_o,
		LK_p			= GDK_p,
		LK_q			= GDK_q,
		LK_r			= GDK_r,
		LK_s			= GDK_s,
		LK_t			= GDK_t,
		LK_u			= GDK_u,
		LK_v			= GDK_v,
		LK_w			= GDK_w,
		LK_x			= GDK_x,
		LK_y			= GDK_y,
		LK_z			= GDK_z,
		/* End of ASCII mapped keysyms */

		/* Numeric keypad */
		LK_KP_ENTER		= GDK_KP_Enter,
		LK_KP0			= GDK_KP_0,
		LK_KP1			= GDK_KP_1,
		LK_KP2			= GDK_KP_2,
		LK_KP3			= GDK_KP_3,
		LK_KP4			= GDK_KP_4,
		LK_KP5			= GDK_KP_5,
		LK_KP6			= GDK_KP_6,
		LK_KP7			= GDK_KP_7,
		LK_KP8			= GDK_KP_8,
		LK_KP9			= GDK_KP_9,
		LK_KP_PERIOD	= GDK_KP_Decimal,
		LK_KP_DELETE	= GDK_KP_Delete,
		LK_KP_MULTIPLY	= GDK_KP_Multiply,
		LK_KP_PLUS		= GDK_KP_Add,
		LK_KP_MINUS		= GDK_KP_Subtract,
		LK_KP_DIVIDE	= GDK_KP_Divide,
		LK_KP_EQUALS	= GDK_KP_Equal,

		/* Arrows + Home/End pad */
		LK_HOME			= GDK_Home,
		LK_LEFT			= GDK_Left,
		LK_UP			= GDK_Up,
		LK_RIGHT		= GDK_Right,
		LK_DOWN			= GDK_Down,
		LK_PAGEUP		= GDK_Page_Up,
		LK_PAGEDOWN		= GDK_Page_Down,
		LK_END			= GDK_End,
		LK_INSERT		= GDK_Insert,

		/* Function keys */
		LK_F1			= GDK_F1,
		LK_F2			= GDK_F2,
		LK_F3			= GDK_F3,
		LK_F4			= GDK_F4,
		LK_F5			= GDK_F5,
		LK_F6			= GDK_F6,
		LK_F7			= GDK_F7,
		LK_F8			= GDK_F8,
		LK_F9			= GDK_F9,
		LK_F10			= GDK_F10,
		LK_F11			= GDK_F11,
		LK_F12			= GDK_F12,
		LK_F13			= GDK_F13,
		LK_F14			= GDK_F14,
		LK_F15			= GDK_F15,

		/* Key state modifier keys */
		LK_NUMLOCK		= GDK_Num_Lock,
		LK_CAPSLOCK		= GDK_Caps_Lock,
		LK_SCROLLOCK	= GDK_Scroll_Lock,
		LK_LSHIFT		= GDK_Shift_L,
		LK_RSHIFT		= GDK_Shift_R,
		LK_LCTRL		= GDK_Control_L,
		LK_RCTRL		= GDK_Control_R,
		LK_LALT			= GDK_Alt_L,
		LK_RALT			= GDK_Alt_R,
		LK_LMETA		= GDK_Hyper_L,
		LK_RMETA		= GDK_Hyper_R,
		LK_LSUPER		= GDK_Super_L, /* "Windows" key */
		LK_RSUPER		= GDK_Super_R,

		/* Miscellaneous function keys */
		LK_HELP			= GDK_Help,
		LK_PRINT		= GDK_Print,
		LK_SYSREQ		= GDK_Sys_Req,
		LK_BREAK		= GDK_Break,
		LK_MENU			= GDK_Menu,
		LK_UNDO			= GDK_Undo,
		LK_REDO			= GDK_Redo,
		LK_EURO			= GDK_EuroSign, /* Some european keyboards */
		LK_COMPOSE		= GDK_Multi_key, /* Multi-key compose key */
		LK_MODE			= GDK_Mode_switch, /* "Alt Gr" key (could be wrong) */
		LK_DELETE		= GDK_Delete,
		LK_POWER		= 0x10000,	/* Power Macintosh power key */
		LK_CONTEXTKEY	= GDK_Menu,

		/* Add any other keys here */
		LK_LAST
	} LgiKey;

#else

	#define VK_BACKSPACE    VK_BACK
	#define VK_PAGEUP       VK_PRIOR
	#define VK_PAGEDOWN     VK_NEXT
	#define VK_RALT         VK_MENU
	#define VK_LALT         VK_MENU
	#define VK_RCTRL        VK_CONTROL
	#define VK_LCTRL        VK_CONTROL

#endif

/////////////////////////////////////////////////////////////////////////////////////
// Externs
#define swprintf_s		swprintf
#ifndef _MSC_VER
#define vsprintf_s		vsnprintf
#endif

#ifndef WINNATIVE // __CYGWIN__

	#ifdef _MSC_VER
	#else
	// LgiFunc char *strnistr(char *a, char *b, int n);
	#define _strnicmp strncasecmp // LgiFunc int _strnicmp(char *a, char *b, int i);
	LgiFunc char *strupr(char *a);
	LgiFunc char *strlwr(char *a);
	LgiFunc int stricmp(const char *a, const char *b);
	#define _stricmp strcasecmp // LgiFunc int _stricmp(const char *a, const char *b);
	#endif
	#define sprintf_s snprintf

#else

	LgiFunc class GViewI *GWindowFromHandle(OsView hWnd);
	LgiFunc int GetMouseWheelLines();
	LgiFunc int WinPointToHeight(int Pt, HDC hDC = NULL);
	LgiFunc int WinHeightToPoint(int Ht, HDC hDC = NULL);

	#if _MSC_VER >= 1400
		#define snprintf sprintf_s
	#endif

	/// Convert a string d'n'd format to an OS dependant integer.
	LgiFunc int FormatToInt(char *s);
	/// Convert a Os dependant integer d'n'd format to a string.
	LgiFunc char *FormatToStr(int f);

#endif

#endif

