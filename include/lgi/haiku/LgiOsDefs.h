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
#define LGI_VIEW_HANDLE				0
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
#define LCurrentThreadHnd()			pthread_self()
LgiFunc OsThreadId					LCurrentThreadId();

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

#define LPrintfInt64			"%lld"
#define LPrintfUInt64			"%llu"
#define LPrintfHex64			"%llx"
#define LPrintfSizeT			"%zu"
#define LPrintfSSizeT			"%zi"
#define LPrintfThreadId			"%u"
#define LPrintfSock				"%i"

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

#define L_ALL_KEYS() \
	/* The keyboard syms have been cleverly chosen to map to ASCII */ \
	_(LK_UNKNOWN, 0) \
	_(LK_FIRST, 0) \
	_(LK_BACKSPACE, B_BACKSPACE) \
	_(LK_TAB, B_TAB) \
	_(LK_RETURN, B_RETURN) \
	_(LK_ESCAPE, B_ESCAPE) \
	_(LK_SPACE, B_SPACE) \
	_(LK_EXCLAIM, '!') \
	_(LK_QUOTEDBL, '\"') \
	_(LK_HASH, '#') \
	_(LK_DOLLAR, '$') \
	_(LK_AMPERSAND, '&') \
	_(LK_QUOTE, '\'') \
	_(LK_LEFTPAREN, '(') \
	_(LK_RIGHTPAREN, ')') \
	_(LK_ASTERISK, '*') \
	_(LK_PLUS, '+') \
	_(LK_COMMA, ',') \
	_(LK_MINUS, '-') \
	_(LK_PERIOD, '.') \
	_(LK_SLASH, '/') \
	_(LK_0, '0') \
	_(LK_1, '1') \
	_(LK_2, '2') \
	_(LK_3, '3') \
	_(LK_4, '4') \
	_(LK_5, '5') \
	_(LK_6, '6') \
	_(LK_7, '7') \
	_(LK_8, '8') \
	_(LK_9, '9') \
	_(LK_COLON, ':') \
	_(LK_SEMICOLON, ';') \
	_(LK_LESS, '<') \
	_(LK_EQUALS, '=') \
	_(LK_GREATER, '>') \
	_(LK_QUESTION, '?') \
	_(LK_AT, '@') \
	/* Skip uppercase letters */ \
	_(LK_LEFTBRACKET, '[') \
	_(LK_BACKSLASH, '\\') \
	_(LK_RIGHTBRACKET, ']') \
	_(LK_CARET, '^') \
	_(LK_UNDERSCORE, '_') \
	_(LK_BACKQUOTE, 96) \
	_(LK_a, 'a') \
	_(LK_b, 'b') \
	_(LK_c, 'c') \
	_(LK_d, 'd') \
	_(LK_e, 'e') \
	_(LK_f, 'f') \
	_(LK_g, 'g') \
	_(LK_h, 'h') \
	_(LK_i, 'i') \
	_(LK_j, 'j') \
	_(LK_k, 'k') \
	_(LK_l, 'l') \
	_(LK_m, 'm') \
	_(LK_n, 'n') \
	_(LK_o, 'o') \
	_(LK_p, 'p') \
	_(LK_q, 'q') \
	_(LK_r, 'r') \
	_(LK_s, 's') \
	_(LK_t, 't') \
	_(LK_u, 'u') \
	_(LK_v, 'v') \
	_(LK_w, 'w') \
	_(LK_x, 'x') \
	_(LK_y, 'y') \
	_(LK_z, 'z') \
	/* End of ASCII mapped keysyms */ \
	\
	/* Arrows + Home/End pad */ \
	_(LK_HOME, B_HOME) \
	_(LK_LEFT, B_LEFT_ARROW) \
	_(LK_UP, B_UP_ARROW) \
	_(LK_RIGHT, B_RIGHT_ARROW) \
	_(LK_DOWN, B_DOWN_ARROW) \
	_(LK_PAGEUP, B_PAGE_UP) \
	_(LK_PAGEDOWN, B_PAGE_DOWN) \
	_(LK_END, B_END) \
	_(LK_INSERT, B_INSERT) \
	_(LK_DELETE, B_DELETE) \
	/* Numeric keypad */ \
	_(LK_KEYPADENTER, 1000) \
	_(LK_KP0, 1001) \
	_(LK_KP1, 1002) \
	_(LK_KP2, 1003) \
	_(LK_KP3, 1004) \
	_(LK_KP4, 1005) \
	_(LK_KP5, 1006) \
	_(LK_KP6, 1007) \
	_(LK_KP7, 1008) \
	_(LK_KP8, 1009) \
	_(LK_KP9, 1010) \
	_(LK_KP_PERIOD, 1011) \
	_(LK_KP_DELETE, 1012) \
	_(LK_KP_MULTIPLY, 1013) \
	_(LK_KP_PLUS, 1014) \
	_(LK_KP_MINUS, 1015) \
	_(LK_KP_DIVIDE, 1016) \
	_(LK_KP_EQUALS, 1017) \
	/* Function keys */ \
	_(LK_F1, 1018) \
	_(LK_F2, 1019) \
	_(LK_F3, 1020) \
	_(LK_F4, 1021) \
	_(LK_F5, 1022) \
	_(LK_F6, 1023) \
	_(LK_F7, 1024) \
	_(LK_F8, 1025) \
	_(LK_F9, 1026) \
	_(LK_F10, 1027) \
	_(LK_F11, 1028) \
	_(LK_F12, 1029) \
	_(LK_F13, 1030) \
	_(LK_F14, 1031) \
	_(LK_F15, 1032) \
	/* Key state modifier keys */ \
	_(LK_NUMLOCK, 1033) \
	_(LK_CAPSLOCK, 1034) \
	_(LK_SCROLLOCK, 1035) \
	_(LK_LSHIFT, 1036) \
	_(LK_RSHIFT, 1037) \
	_(LK_LCTRL, 1038) \
	_(LK_RCTRL, 1039) \
	_(LK_LALT, 1040) \
	_(LK_RALT, 1041) \
	_(LK_LMETA, 1042) \
	_(LK_RMETA, 1043) \
	_(LK_LSUPER, 1044) /* "Windows" key */ \
	_(LK_RSUPER, 1045) \
	/* Miscellaneous function keys */ \
	_(LK_HELP, 1046) \
	_(LK_PRINT, 1047) \
	_(LK_SYSREQ, 1048) \
	_(LK_BREAK, 1049) \
	_(LK_MENU, 1050) \
	_(LK_UNDO, 1051) \
	_(LK_REDO, 1052) \
	_(LK_EURO, 1053) /* Some european keyboards */ \
	_(LK_COMPOSE, 1054) /* Multi-key compose key */ \
	_(LK_MODE, 1055) /* "Alt Gr" key (could be wrong) */ \
	_(LK_POWER, 1056) /* Power Macintosh power key */ \
	_(LK_CONTEXTKEY, 1057)
	/* Add any other keys here */

enum LgiKey
{
	#define _(key, val) key = val,
	L_ALL_KEYS()
	#undef _
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

