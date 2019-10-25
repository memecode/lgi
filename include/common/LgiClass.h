#ifndef _LGI_CLASS_H_
#define _LGI_CLASS_H_

#include "LgiInc.h"
#include "LgiDefs.h"
#if defined(__OBJC__) && !defined(__GTK_H__)
#include <Cocoa/Cocoa.h>
#endif

// Virtual input classes
class GKey;
class GMouse;

// General GUI classes
class GTarget;
class GComponent;
class GEvent;
class GId;
class GApp;
class GWindow;
class GWin32Class;
class GView;
class GLayout;
class GFileSelect;
class GFindReplace;
class LSubMenu;
class LMenuItem;
class LMenu;
class GToolBar;
class GToolButton;
class GSplitter;
class GStatusPane;
class GStatusBar;
class GToolColour;
class GScrollBar;
class GImageList;
class GDialog;

// General objects
class LgiClass GBase
{
	char *_Name8;
	char16 *_Name16;

public:
	GBase();	
	virtual ~GBase();

	virtual char *Name();
	virtual bool Name(const char *n);
	virtual char16 *NameW();
	virtual bool NameW(const char16 *n);
};

#define AssignFlag(f, bit, to) if (to) f |= bit; else f &= ~(bit)

/// Sets the output stream for the LgiTrace statement. By default the stream output
/// is to <app_name>.txt in the executables folder or $LSP_APP_ROOT\<app_name>.txt if
/// that is not writable. If the stream is set to something then normal file output is
/// directed to the specified stream instead.
LgiFunc void LgiTraceSetStream(class GStreamI *stream);

/// Gets the log file path
LgiFunc bool LgiTraceGetFilePath(char *LogPath, int BufLen);

/// Writes a debug statement to a output stream, or if not defined with LgiTraceSetStream
/// then to a log file (see LgiTraceSetStream for details)
///
/// Default path is ./<app_name>.txt relative to the executable.
/// Fall back path is LgiGetSystemPath(LSP_APP_ROOT).
LgiFunc void LgiTrace(const char *Format, ...);

#ifndef LGI_STATIC
/// Same as LgiTrace but writes a stack trace as well.
LgiFunc void LgiStackTrace(const char *Format, ...);
#endif

// OsEvent is defined here because the GUiEvent is the primary user.
// And this header can be included independently of LgiOsDefs.h where
// this would otherwise live.
#if defined __GTK_H__
	typedef Gtk::GdkEvent *OsEvent;
#elif defined _SDL_H
	typedef SDL_Event *OsEvent;
#elif defined _WIN32
	// No native type here, but GMessage can encapsulate the message code, WPARAM and LPARAM.
	typedef class GMessage *OsEvent;
#elif LGI_COCOA
	#include "ObjCWrapper.h"
	ObjCWrapper(NSEvent,  OsEvent);
#elif LGI_CARBON
	typedef EventRef OsEvent;
#else
	#error "Impl me."
#endif

/// General user interface event
class LgiClass GUiEvent
{
public:
	int Flags;
	OsEvent Event;

	GUiEvent()
	{
		Flags = 0;
	}

	virtual ~GUiEvent() {}
	virtual void Trace(const char *Msg) {}

	/// The key or mouse button was being pressed. false on the up-click.
	bool Down()		{ return TestFlag(Flags, LGI_EF_DOWN); }
	/// The mouse button was double clicked.
	bool Double()	{ return TestFlag(Flags, LGI_EF_DOUBLE); }
	/// A ctrl button was held down during the event
	bool Ctrl()		{ return TestFlag(Flags, LGI_EF_CTRL); }
	/// A alt button was held down during the event
	bool Alt()		{ return TestFlag(Flags, LGI_EF_ALT); }
	/// A shift button was held down during the event
	bool Shift()	{ return TestFlag(Flags, LGI_EF_SHIFT); }
	/// The system key was held down (windows key / apple key etc)
	bool System()	{ return TestFlag(Flags, LGI_EF_SYSTEM); }

	// Set
	void Down(bool i)	{ AssignFlag(Flags, LGI_EF_DOWN, i); }
	void Double(bool i)	{ AssignFlag(Flags, LGI_EF_DOUBLE, i); }
	void Ctrl(bool i)	{ AssignFlag(Flags, LGI_EF_CTRL, i); }
	void Alt(bool i)	{ AssignFlag(Flags, LGI_EF_ALT, i); }
	void Shift(bool i)	{ AssignFlag(Flags, LGI_EF_SHIFT, i); }
	void System(bool i)	{ AssignFlag(Flags, LGI_EF_SYSTEM, i); }

	#if defined(MAC)
		bool CtrlCmd() { return System(); }
		static const char *CtrlCmdName() { return "\xE2\x8C\x98"; }	
		bool AltCmd() { return System(); }
		static const char *AltCmdName()	{ return "\xE2\x8C\x98"; }
	#else // win32 and linux
		bool CtrlCmd() { return Ctrl(); }
		static const char *CtrlCmdName() { return "Ctrl"; }	
		bool AltCmd() { return Alt(); }
		static const char *AltCmdName() { return "Alt"; }
	#endif
	
	#if LGI_COCOA
	void SetModifer(uint32_t modifierKeys);
	#else
	void SetModifer(uint32_t modifierKeys)
	{
		#if defined(__GTK_H__)
		System((modifierKeys & Gtk::GDK_MOD4_MASK) != 0);
		Shift((modifierKeys & Gtk::GDK_SHIFT_MASK) != 0);
		Alt((modifierKeys & Gtk::GDK_MOD1_MASK) != 0);
		Ctrl((modifierKeys & Gtk::GDK_CONTROL_MASK) != 0);
		#elif LGI_CARBON
		System(modifierKeys & cmdKey);
		Shift(modifierKeys & shiftKey);
		Alt(modifierKeys & optionKey);
		Ctrl(modifierKeys & controlKey);
		#endif
	}
	#endif
};

#ifdef WINDOWS
struct GKeyWinBits
{
	unsigned Repeat : 16;
	unsigned Scan : 8;
	unsigned Extended : 1;
	unsigned Reserved : 4;
	unsigned Context : 1;
	unsigned Previous : 1;
	unsigned Pressed : 1;
};
#endif

/// All the information related to a keyboard event
class LgiClass GKey : public GUiEvent
{
public:
	/// The virtual code for key
	/// Rule: Only compare with LK_??? symbols
	char16 vkey;
	/// The unicode character for the key
	/// Rule: Never compare with LK_??? symbols
	char16 c16;
	/// OS Specific
	#ifdef WINDOWS
	union {
	#endif
		uint32_t Data;
	#ifdef WINDOWS
		GKeyWinBits WinBits;
	};
	#endif
	/// True if this is a standard character (ie not a control key)
	bool IsChar;

	GKey()
	{
		vkey = 0;
		c16 = 0;
		Data = 0;
		IsChar = 0;
	}

	GKey(int vkey, uint32_t flags);

	void Trace(const char *Msg)
	{
		LgiTrace("%s GKey vkey=%i(0x%x) c16=%i(%c) IsChar=%i down=%i ctrl=%i alt=%i sh=%i sys=%i\n",
			Msg ? Msg : (char*)"",
			vkey, vkey,
			c16, c16 >= ' ' && c16 < 127 ? c16 : '.',
			IsChar, Down(), Ctrl(), Alt(), Shift(), System());
	}
	
	bool CapsLock()
	{
		return TestFlag(Flags, LGI_EF_CAPS_LOCK);
	}

	/// Returns the character in the right case...
	char16 GetChar()
	{
		if (Shift() ^ CapsLock())
		{
			return (c16 >= 'a' && c16 <= 'z') ? c16 - 'a' + 'A' : c16;
		}
		else
		{
			return (c16 >= 'A' && c16 <= 'Z') ? c16 - 'A' + 'a' : c16;
		}
	}
	
	/// \returns true if this event should show a context menu
	bool IsContextMenu();
};

/// \brief All the parameters of a mouse click event
///
/// The parent class GUiEvent keeps information about whether it was a Down()
/// or Double() click. You can also query whether the Alt(), Ctrl() or Shift()
/// keys were pressed at the time the event occurred.
///
/// To get the position of the mouse in screen co-ordinates you can either use
/// GView::GetMouse() and pass true in the 'ScreenCoords' parameter. Or you can
/// construct a GdcPt2 out of the x,y fields of this class and use GView::PointToScreen()
/// to map the point to screen co-ordinates.
class LgiClass GMouse : public GUiEvent
{
public:
	/// Receiving view
	class GViewI *Target;
	/// True if specified in view coordinates, false if in screen coords
	bool ViewCoords;
	/// The x co-ordinate of the mouse relative to the current view
	int x;
	/// The y co-ordinate of the mouse relative to the current view
	int y;
	
	GMouse(GViewI *target = NULL)
	{
		Target = target;
		ViewCoords = true;
		x = y = 0;
	}

	void Trace(const char *Msg)
	{
		LgiTrace("%s GMouse pos=%i,%i view=%i btns=%i/%i/%i dwn=%i dbl=%i "
				"ctrl=%i alt=%i sh=%i sys=%i\n",
			Msg ? Msg : (char*)"", x, y, ViewCoords,
			Left(), Middle(), Right(),
			Down(), Double(),
			Ctrl(), Alt(), Shift(), System());
	}

	/// True if the left mouse button was clicked
	bool Left()		{ return TestFlag(Flags, LGI_EF_LEFT); }
	/// True if the middle mouse button was clicked
	bool Middle()	{ return TestFlag(Flags, LGI_EF_MIDDLE); }
	/// True if the right mouse button was clicked
	bool Right()	{ return TestFlag(Flags, LGI_EF_RIGHT); }
	/// True if the mouse event is a move, false for a click event.
	bool IsMove()	{ return TestFlag(Flags, LGI_EF_MOVE); }

	/// Sets the left button flag
	void Left(bool i)	{ AssignFlag(Flags, LGI_EF_LEFT, i); }
	/// Sets the middle button flag
	void Middle(bool i)	{ AssignFlag(Flags, LGI_EF_MIDDLE, i); }
	/// Sets the right button flag
	void Right(bool i)	{ AssignFlag(Flags, LGI_EF_RIGHT, i); }
	/// Sets the move flag
	void IsMove(bool i)	{ AssignFlag(Flags, LGI_EF_MOVE, i); }
	
	/// Converts to screen coordinates
	bool ToScreen();
	/// Converts to local coordinates
	bool ToView();

	/// \returns true if this event should show a context menu
	bool IsContextMenu();
	
	#if defined(__OBJC__) && !defined(__GTK_H__)
	void SetFromEvent(NSEvent *ev, NSView *view);
	#else
	void SetButton(uint32_t Btn)
	{
		#if defined(MAC) && defined(__CARBONEVENTS__)
		Left(Btn == kEventMouseButtonPrimary);
		Right(Btn == kEventMouseButtonSecondary);
		Middle(Btn == kEventMouseButtonTertiary);
		#elif defined(__GTK_H__)
		if (Btn == 1) Left(true);
		else if (Btn == 2) Middle(true);
		else if (Btn == 3) Right(true);
		#endif
	}
	#endif
};

#include "GAutoPtr.h"

/// Holds information pertaining to an application
class GAppInfo
{
public:
	/// The path to the executable for the app
	GAutoString Path;
	/// Plain text name for the app
	GAutoString Name;
	/// A path to an icon to display for the app
	GAutoString Icon;
	/// The params to call the app with
	GAutoString Params;
};

template<typename RESULT, typename CHAR>
RESULT LHash(const CHAR *v, ssize_t l, bool Case)
{
	RESULT h = 0;

	if (Case)
	{
		// case sensitive
		if (l > 0)
		{
			while (l--)
				h = (h << 5) - h + *v++;
		}
		else
		{
			for (; *v; v ++)
				h = (h << 5) - h + *v;
		}
	}
	else
	{
		// case insensitive
		CHAR c;
		if (l > 0)
		{
			while (l--)
			{
				c = tolower(*v);
				v++;
				h = (h << 5) - h + c;
			}
		}
		else
		{
			for (; *v; v++)
			{
				c = tolower(*v);
				h = (h << 5) - h + c;
			}
		}
	}

	return h;
}

#include <functional>

template<typename T>
void LSort(T *v, int left, int right, std::function<ssize_t(T, T)> comp)
{
    int i, last;

    if (left >= right)
        return;
    
	LSwap(v[left], v[(left + right)>>1]);

    last = left;
    for (i = left+1; i <= right; i++)
        if (comp(v[i], v[left]) < 0) /* Here's the function call */
            LSwap(v[++last], v[i]);
    
	LSwap(v[left], v[last]);
    LSort(v, left, last-1, comp);
    LSort(v, last+1, right, comp);
}


#endif
