#ifndef _LGI_CLASS_H_
#define _LGI_CLASS_H_

#include <ctype.h>

#include "lgi/common/LgiInc.h"
#include "lgi/common/LgiDefs.h"
#include "lgi/common/AutoPtr.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/Point.h"

#if defined(__OBJC__) && !defined(__GTK_H__)
#include <Cocoa/Cocoa.h>
#endif
#include <functional>


// Virtual input classes
class LKey;
class LMouse;

// General GUI classes
class LApp;
class LWindow;
class LWindowsClass;
class LView;
class LLayout;
class LFileSelect;
class LSubMenu;
class LMenuItem;
class LMenu;
class LToolBar;
class LToolButton;
class LSplitter;
class LStatusPane;
class LStatusBar;
class LScrollBar;
class LImageList;
class LDialog;

// Tracing:

/// Sets the output stream for the LgiTrace statement. By default the stream output
/// is to <app_name>.txt in the executables folder or $LSP_APP_ROOT\<app_name>.txt if
/// that is not writable. If the stream is set to something then normal file output is
/// directed to the specified stream instead.
LgiFunc void LgiTraceSetStream(class LStreamI *stream);

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
LgiFunc void LStackTrace(const char *Format, ...);
#endif

// Template hash function:
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

// Template sort function:
template<typename T>
void LSort(T *v, ssize_t left, ssize_t right, std::function<ssize_t(T, T)> comp)
{
    ssize_t i, last;

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

#define AssignFlag(f, bit, to) if (to) f |= bit; else f &= ~(bit)

// OsEvent is defined here because the LUiEvent is the primary user.
// And this header can be included independently of LgiOsDefs.h where
// this would otherwise live.
#if defined __GTK_H__
	typedef Gtk::GdkEvent *OsEvent;
#elif defined _SDL_H
	typedef SDL_Event *OsEvent;
#elif defined _WIN32
	// No native type here, but LMessage can encapsulate the message code, WPARAM and LPARAM.
	typedef class LMessage *OsEvent;
#elif LGI_COCOA
	#include "ObjCWrapper.h"
	ObjCWrapper(NSEvent,  OsEvent);
#elif LGI_CARBON
	typedef EventRef OsEvent;
#elif LGI_HAIKU
	typedef BMessage *OsEvent;
#else
	#error "Impl me."
#endif

/// General user interface event
class LgiClass LUiEvent
{
public:
	int Flags;
	OsEvent Event;

	LUiEvent()
	{
		Flags = 0;
	}

	virtual ~LUiEvent() {}
	virtual void Trace(const char *Msg) const {}

	/// The key or mouse button was being pressed. false on the up-click.
	bool Down() const	{ return TestFlag(Flags, LGI_EF_DOWN); }
	/// The mouse button was double clicked.
	bool Double() const	{ return TestFlag(Flags, LGI_EF_DOUBLE); }
	/// A ctrl button was held down during the event
	bool Ctrl() const	{ return TestFlag(Flags, LGI_EF_CTRL); }
	/// A alt button was held down during the event
	bool Alt() const	{ return TestFlag(Flags, LGI_EF_ALT); }
	/// A shift button was held down during the event
	bool Shift() const	{ return TestFlag(Flags, LGI_EF_SHIFT); }
	/// The system key was held down (windows key / apple key etc)
	bool System() const	{ return TestFlag(Flags, LGI_EF_SYSTEM); }

	// Set
	void Down(bool i)	{ AssignFlag(Flags, LGI_EF_DOWN, i); }
	void Double(bool i)	{ AssignFlag(Flags, LGI_EF_DOUBLE, i); }
	void Ctrl(bool i)	{ AssignFlag(Flags, LGI_EF_CTRL, i); }
	void Alt(bool i)	{ AssignFlag(Flags, LGI_EF_ALT, i); }
	void Shift(bool i)	{ AssignFlag(Flags, LGI_EF_SHIFT, i); }
	void System(bool i)	{ AssignFlag(Flags, LGI_EF_SYSTEM, i); }

	#if defined(MAC)
		bool CtrlCmd() const { return System(); }
		static const char *CtrlCmdName() { return "\xE2\x8C\x98"; }	
		bool AltCmd() const { return System(); }
		static const char *AltCmdName()	{ return "\xE2\x8C\x98"; }
	#elif defined(HAIKU)
		bool CtrlCmd() const { return System(); }
		static const char *CtrlCmdName() { return "System"; }	
		bool AltCmd() const { return Ctrl(); }
		static const char *AltCmdName() { return "Ctrl"; }
	#else // win32 and linux
		bool CtrlCmd() const { return Ctrl(); }
		static const char *CtrlCmdName() { return "Ctrl"; }	
		bool AltCmd() const { return Alt(); }
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
struct LKeyWinBits
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
class LgiClass LKey : public LUiEvent
{
public:
	/// The virtual code for key
	/// Rule: Only compare with LK_??? symbols
	char16 vkey = 0;
	/// The unicode character for the key
	/// Rule: Never compare with LK_??? symbols
	char16 c16 = 0;
	/// OS Specific
	#ifdef WINDOWS
	union {
	#endif
		uint32_t Data = 0;
	#ifdef WINDOWS
		LKeyWinBits WinBits;
	};
	#endif
	/// True if this is a standard character (ie not a control key)
	bool IsChar = false;

	LKey() {}
	LKey(int vkey, uint32_t flags);

	void Trace(const char *Msg) const
	{
		LgiTrace("%s LKey vkey=%i(0x%x) c16=%i(%c) IsChar=%i down=%i ctrl=%i alt=%i sh=%i sys=%i\n",
			Msg ? Msg : (char*)"",
			vkey, vkey,
			c16, c16 >= ' ' && c16 < 127 ? c16 : '.',
			IsChar, Down(), Ctrl(), Alt(), Shift(), System());
	}
	
	bool CapsLock() const
	{
		return TestFlag(Flags, LGI_EF_CAPS_LOCK);
	}

	/// Returns the character in the right case...
	char16 GetChar() const
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
	bool IsContextMenu() const;
};

/// \brief All the parameters of a mouse click event
///
/// The parent class LUiEvent keeps information about whether it was a Down()
/// or Double() click. You can also query whether the Alt(), Ctrl() or Shift()
/// keys were pressed at the time the event occurred.
///
/// To get the position of the mouse in screen co-ordinates you can either use
/// LView::GetMouse() and pass true in the 'ScreenCoords' parameter. Or you can
/// construct a LPoint out of the x,y fields of this class and use LView::PointToScreen()
/// to map the point to screen co-ordinates.
class LgiClass LMouse : public LUiEvent, public LPoint
{
public:
	/// Receiving view
	class LViewI *Target;
	/// True if specified in view coordinates, false if in screen coords
	bool ViewCoords;

	LMouse(LViewI *target = NULL)
	{
		Target = target;
		ViewCoords = true;
	}

	LMouse operator -(LPoint p)
	{
		LMouse m = *this;
		m.x -= p.x;
		m.y -= p.y;
		return m;
	}

	LMouse operator +(LPoint p)
	{
		LMouse m = *this;
		m.x += p.x;
		m.y += p.y;
		return m;
	}
	
	bool operator !=(const LMouse &m)
	{
		return	x != m.x ||
				y != m.y ||
				Flags != m.Flags;
	}

	LString ToString() const;
	void Trace(const char *Msg) const
	{
		LgiTrace("%s %s\n", Msg ? Msg : (char*)"", ToString().Get());
	}

	bool Left() const		{ return TestFlag(Flags, LGI_EF_LEFT); }
	bool Middle() const		{ return TestFlag(Flags, LGI_EF_MIDDLE); }
	bool Right() const		{ return TestFlag(Flags, LGI_EF_RIGHT); }
	bool Button1() const	{ return TestFlag(Flags, LGI_EF_XBTN1); }
	bool Button2() const	{ return TestFlag(Flags, LGI_EF_XBTN2); }
	bool IsMove() const		{ return TestFlag(Flags, LGI_EF_MOVE); }

	void Left(bool i)		{ AssignFlag(Flags, LGI_EF_LEFT, i); }
	void Middle(bool i)		{ AssignFlag(Flags, LGI_EF_MIDDLE, i); }
	void Right(bool i)		{ AssignFlag(Flags, LGI_EF_RIGHT, i); }
	void Button1(bool i)	{ AssignFlag(Flags, LGI_EF_XBTN1, i); }
	void Button2(bool i)	{ AssignFlag(Flags, LGI_EF_XBTN2, i); }
	void IsMove(bool i)		{ AssignFlag(Flags, LGI_EF_MOVE, i); }
	
	/// Converts to screen coordinates
	bool ToScreen();
	/// Converts to local coordinates
	bool ToView();

	/// \returns true if this event should show a context menu
	bool IsContextMenu() const;
	
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

/// Holds information pertaining to an application
class LAppInfo
{
public:
	/// The path to the executable for the app
	LString Path;
	/// Plain text name for the app
	LString Name;
	/// A path to an icon to display for the app
	LString Icon;
	/// The params to call the app with
	LString Params;
};

// Base class for GUI objects
class LgiClass LBase
{
	char *_Name8;
	char16 *_Name16;

public:
	LBase();	
	virtual ~LBase();

	virtual const char *Name();
	virtual bool Name(const char *n);
	virtual const char16 *NameW();
	virtual bool NameW(const char16 *n);
};

#endif
