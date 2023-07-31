/**
	\file
	\author Matthew Allen
	\brief Haiku defs.
	
	Debugging memory issues in Haiku:
	LD_PRELOAD=/boot/system/lib/libroot_debug.so MALLOC_DEBUG=g ./my_app
 */

#ifndef __LGI_OS_DEFS_H
#define __LGI_OS_DEFS_H

#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>
#include <pthread.h>

#include <View.h>
#include <Window.h>
#include <Message.h>
#include <Font.h>

#include "lgi/common/LgiDefs.h"

#include <unistd.h>
#define XP_CTRLS					1
#define POSIX						1
#define LGI_VIEW_HANDLE				1
#define LGI_VIEW_HASH               1
#define LGI_HAIKU					1
#if __x86_64
	#define HAIKU64					1
	#define LGI_64BIT				1
#else
	#define HAIKU32					1
	#define LGI_32BIT				1
#endif

#undef stricmp

#include "lgi/common/LgiInc.h"

class LgiClass OsAppArguments
{
	struct OsAppArgumentsPriv *d;

public:
	int Args;
	const char **Arg;

	OsAppArguments(int args, const char **arg);
	~OsAppArguments();

	void Set(char *CmdLine);
	bool Get(const char *Name, const char **Val = NULL);
	OsAppArguments &operator =(OsAppArguments &a);
};

// Process
typedef int			OsProcess;
typedef int			OsProcessId;
typedef BView		*OsView;
typedef BWindow		*OsWindow;
typedef char		OsChar;
typedef BView		*OsPainter;
typedef BFont		*OsFont;
typedef BBitmap		*OsBitmap;

#define LGetCurrentProcess				getpid

class OsApplication
{
	class OsApplicationPriv *d;

protected:
	static OsApplication *Inst;
	
public:
	OsApplication(int Args, const char **Arg);
	~OsApplication();
	
	static OsApplication *GetInst() { LAssert(Inst != NULL); return Inst; }
};

// Threads
typedef pthread_t					OsThread;
typedef pid_t						OsThreadId;
typedef pthread_mutex_t				OsSemaphore;
#define LGetCurrentThread()			pthread_self()
LgiFunc OsThreadId					GetCurrentThreadId();

#include "lgi/common/Message.h"

// Sockets
#define ValidSocket(s)				((s)>=0)
#ifndef WIN32
#define INVALID_SOCKET				-1
#endif
typedef int OsSocket;

/// Sleep the current thread for i milliseconds.
#ifdef WIN32
LgiFunc void LSleep(DWORD i);
#else
LgiFunc void LSleep(uint32_t i);
#endif

#ifndef WIN32
#define atoi64						atoll
#else
#define atoi64						_atoi64
#endif

#define _snprintf					snprintf
#define _vsnprintf					vsnprintf
#define wcscpy_s(dst, len, src)		wcsncpy(dst, src, len)

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
/// \sa LKey
#define LGI_VKEY_CTRL				0x001
/// The ALT key is pressed
/// \sa LKey
#define LGI_VKEY_ALT				0x002
/// The SHIFT key is pressed
/// \sa LKey
#define LGI_VKEY_SHIFT				0x004

/// The left mouse button is pressed
/// \sa LMouse
#define LGI_VMOUSE_LEFT				0x008
/// The middle mouse button is pressed
/// \sa LMouse
#define LGI_VMOUSE_MIDDLE			0x010
/// The right mouse button is pressed
/// \sa LMouse
#define LGI_VMOUSE_RIGHT			0x020
/// The ctrl key is pressed
/// \sa LMouse
#define LGI_VMOUSE_CTRL				0x040
/// The alt key is pressed
/// \sa LMouse
#define LGI_VMOUSE_ALT				0x080
/// The shift key is pressed
/// \sa LMouse
#define LGI_VMOUSE_SHIFT			0x100
/// The mouse event is a down click
/// \sa LMouse
#define LGI_VMOUSE_DOWN				0x200
/// The mouse event is a double click
/// \sa LMouse
#define LGI_VMOUSE_DOUBLE			0x400


enum LgiKey
{
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	LK_UNKNOWN		= 0,
	LK_FIRST		= 0,
	LK_BACKSPACE	= B_BACKSPACE,
	LK_TAB			= B_TAB,
	LK_RETURN		= B_RETURN,
	LK_ESCAPE		= B_ESCAPE,
	LK_SPACE		= B_SPACE,
	LK_EXCLAIM		= '!',
	LK_QUOTEDBL		= '\"',
	LK_HASH			= '#',
	LK_DOLLAR		= '$',
	LK_AMPERSAND	= '&',
	LK_QUOTE		= '\'',
	LK_LEFTPAREN	= '(',
	LK_RIGHTPAREN	= ')',
	LK_ASTERISK		= '*',
	LK_PLUS			= '+',
	LK_COMMA		= ',',
	LK_MINUS		= '-',
	LK_PERIOD		= '.',
	LK_SLASH		= '/',
	LK_0			= '0',
	LK_1			= '1',
	LK_2			= '2',
	LK_3			= '3',
	LK_4			= '4',
	LK_5			= '5',
	LK_6			= '6',
	LK_7			= '7',
	LK_8			= '8',
	LK_9			= '9',
	LK_COLON		= ':',
	LK_SEMICOLON	= ';',
	LK_LESS			= '<',
	LK_EQUALS		= '=',
	LK_GREATER		= '>',
	LK_QUESTION		= '?',
	LK_AT			= '@',
	/* 
	   Skip uppercase letters
	 */
	LK_LEFTBRACKET	= '[',
	LK_BACKSLASH	= '\\',
	LK_RIGHTBRACKET	= ']',
	LK_CARET		= '^',
	LK_UNDERSCORE	= '_',
	LK_BACKQUOTE	= 96,
	LK_a			= 'a',
	LK_b			= 'b',
	LK_c			= 'c',
	LK_d			= 'd',
	LK_e			= 'e',
	LK_f			= 'f',
	LK_g			= 'g',
	LK_h			= 'h',
	LK_i			= 'i',
	LK_j			= 'j',
	LK_k			= 'k',
	LK_l			= 'l',
	LK_m			= 'm',
	LK_n			= 'n',
	LK_o			= 'o',
	LK_p			= 'p',
	LK_q			= 'q',
	LK_r			= 'r',
	LK_s			= 's',
	LK_t			= 't',
	LK_u			= 'u',
	LK_v			= 'v',
	LK_w			= 'w',
	LK_x			= 'x',
	LK_y			= 'y',
	LK_z			= 'z',
	/* End of ASCII mapped keysyms */

	/* Arrows + Home/End pad */
	LK_HOME			= B_HOME,
	LK_LEFT			= B_LEFT_ARROW,
	LK_UP			= B_UP_ARROW,
	LK_RIGHT		= B_RIGHT_ARROW,
	LK_DOWN			= B_DOWN_ARROW,
	LK_PAGEUP		= B_PAGE_UP,
	LK_PAGEDOWN		= B_PAGE_DOWN,
	LK_END			= B_END,
	LK_INSERT		= B_INSERT,
	LK_DELETE		= B_DELETE,

	/* Numeric keypad */
	LK_KEYPADENTER	,
	LK_KP0			,
	LK_KP1			,
	LK_KP2			,
	LK_KP3			,
	LK_KP4			,
	LK_KP5			,
	LK_KP6			,
	LK_KP7			,
	LK_KP8			,
	LK_KP9			,
	LK_KP_PERIOD	,
	LK_KP_DELETE	,
	LK_KP_MULTIPLY	,
	LK_KP_PLUS		,
	LK_KP_MINUS		,
	LK_KP_DIVIDE	,
	LK_KP_EQUALS	,

	/* Function keys */
	LK_F1,
	LK_F2			,
	LK_F3			,
	LK_F4			,
	LK_F5			,
	LK_F6			,
	LK_F7			,
	LK_F8			,
	LK_F9			,
	LK_F10			,
	LK_F11			,
	LK_F12			,
	LK_F13			,
	LK_F14			,
	LK_F15			,

	/* Key state modifier keys */
	LK_NUMLOCK		,
	LK_CAPSLOCK		,
	LK_SCROLLOCK	,
	LK_LSHIFT		,
	LK_RSHIFT		,
	LK_LCTRL		,
	LK_RCTRL		,
	LK_LALT			,
	LK_RALT			,
	LK_LMETA		,
	LK_RMETA		,
	LK_LSUPER		, /* "Windows" key */
	LK_RSUPER		,

	/* Miscellaneous function keys */
	LK_HELP			,
	LK_PRINT		,
	LK_SYSREQ		,
	LK_BREAK		,
	LK_MENU			,
	LK_UNDO			,
	LK_REDO			,
	LK_EURO			, /* Some european keyboards */
	LK_COMPOSE		, /* Multi-key compose key */
	LK_MODE			, /* "Alt Gr" key (could be wrong) */
	LK_POWER		,	/* Power Macintosh power key */
	LK_CONTEXTKEY	,

	/* Add any other keys here */
	LK_LAST

};

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
	#if __BIG_ENDIAN__
		#define htonll(x) (x)
		#define ntohll(x) (x)
	#else
		#define htonll(x) ( ((uint64)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32) )
		#define ntohll(x) ( ((uint64)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32) )
	#endif

#else

	LgiFunc class LViewI *GWindowFromHandle(OsView hWnd);
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

