/**
	\file
	\author Matthew Allen
	\date 19/12/1997
	\brief Gui class definitions
	Copyright (C) 1997-2004, <a href="mailto:fret@memecode.com">Matthew Allen</a>
*/

/////////////////////////////////////////////////////////////////////////////////////
// Includes
#ifndef _LGICLASSES_H_
#define _LGICLASSES_H_

#include "lgi/common/Mutex.h"
#include "LgiOsClasses.h"
#include "lgi/common/Mem.h"
#include "lgi/common/Array.h"
#include "lgi/common/DragAndDrop.h"

/////////////////////////////////////////////////////////////////////////////////////

LgiFunc bool LPostEvent(OsView Wnd, int Event, LMessage::Param a = 0, LMessage::Param b = 0);
LgiFunc LViewI *GetNextTabStop(LViewI *v, bool Back);

class LgiClass LCommand : public LBase //, public GFlags
{
	int			Flags;
	bool		PrevValue;

public:
	int			Id;
	LToolButton	*ToolButton;
	LMenuItem	*MenuItem;
	LKey		*Accelerator;
	char		*TipHelp;
	
	LCommand();
	~LCommand();

	bool Enabled();
	void Enabled(bool e);
	bool Value();
	void Value(bool v);
};


#include "lgi/common/TrayIcon.h"
#include "lgi/common/Input.h"

#ifndef LGI_STATIC
/// \brief A BeOS style alert window, kinda like a Win32 MessageBox
///
/// The best thing about this class is you can name the buttons very specifically.
/// It's always non-intuitive to word a question to the user in such a way so thats
/// it's obvious to answer with "Ok" or "Cancel". But if the user gets a question
/// with customized "actions" as buttons they'll love you.
///
/// The button pressed is returned as a index from the DoModal() function. Starting
/// at '1'. i.e. Btn2 -> returns 2.
class LgiClass LAlert : public LDialog
{
public:
	/// Constructor
	LAlert
	(
		/// The parent view
		LViewI *parent,
		/// The dialog title
		const char *Title,
		/// The body of the message
		const char *Text,
		/// The first button text
		const char *Btn1,
		/// The [optional] 2nd buttons text
		const char *Btn2 = 0,
		/// The [optional] 3rd buttons text
		const char *Btn3 = 0
	);

    void SetAppModal();
	int OnNotify(LViewI *Ctrl, LNotification &n);
};
#endif

/// Timer class to help do something every so often
class LgiClass DoEvery
{
	int64 LastTime;
	int64 Period;

public:
	/// Constructor
	DoEvery
	(
		/// Timeout in ms
		int p = 1000
	);
	
	/// Reset the timer
	void Init
	(
		/// Timeout in ms
		int p = -1
	);

	/// Returns true when the time has expired. Resets automatically.
	bool DoNow();
};

//////////////////////////////////////////////////////////

// Graphics
LgiFunc void LDrawBox(LSurface *pDC, LRect &r, bool Sunken, bool Fill);
LgiFunc void LWideBorder(LSurface *pDC, LRect &r, LEdge Type);
LgiFunc void LThinBorder(LSurface *pDC, LRect &r, LEdge Type);
LgiFunc void LFlatBorder(LSurface *pDC, LRect &r, int Width = -1);

// Helpers
#ifdef __GTK_H__
extern Gtk::gboolean GtkViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, LView *This);
#endif

#ifdef LINUX
/// Ends a x windows startup session
LgiFunc void LFinishXWindowsStartup(class LViewI *Wnd);
#endif

/// \brief Displays a message box
/// \returns The button clicked. The return value is one of #IDOK, #IDCANCEL, #IDYES or #IDNO.
LgiFunc int LgiMsg
(
	/// The parent view or NULL if none available
	LViewI *Parent,
	/// The message's text. This is a printf format string that you can pass arguments to
	const char *Msg,
	/// The title of the message box window
	const char *Title = 0,
	/// The type of buttons below the message. Can be one of:
	/// #MB_OK, #MB_OKCANCEL, #MB_YESNO or #MB_YESNOCANCEL.
	int Type = MB_OK,
	...
);

/// This is like LgiMsg but displays the text in a scrollable view.
LgiFunc void LDialogTextMsg(LViewI *Parent, const char *Title, LString Txt);

/// Contains all the information about a display/monitor attached to the system.
/// \sa LGetDisplays
struct GDisplayInfo
{
	/// The position and dimensions of the display. On windows the left/upper 
	/// most display will be positioned at 0,0 and each further display will have 
	/// co-ordinates that join to one edge of that initial rectangle.
	LRect r;
	/// The number of bits per pixel
	int BitDepth;
	/// The refresh rate
	int Refresh;
	/// The device's path, system specific
	LString Device;
	/// A descriptive name of the device, usually the video card
	LString Name;
	/// The name of any attached monitor
	LString Monitor;
	/// The dots per inch of the display
	LPoint Dpi;

	GDisplayInfo()
	{
		r.ZOff(-1, -1);
		BitDepth = 0;
		Refresh = 0;
	}

	LPointF Scale();
};

/// Returns infomation about the displays attached to the system.
/// \returns non-zero on success.
LgiFunc bool LGetDisplays
(
	/// [out] The array of display info structures. The caller should free these
	/// objects using Displays.DeleteObjects().
	LArray<GDisplayInfo*> &Displays,
	/// [out] Optional bounding rectangle of all displays. Can be NULL if your don't
	/// need that information.
	LRect *AllDisplays = 0
);

/// This class makes it easy to profile a function and write out timings at the end
class LgiClass LProfile
{
	struct Sample
	{
		uint64 Time;
		const char *Name;
		
		Sample(uint64 t = 0, const char *n = 0)
		{
			Time = t;
			Name = n;
		}
	};
	
	LArray<Sample> s;
	char *Buf;
	int Used;
	int MinMs;
	
public:
	LProfile(const char *Name, int HideMs = -1);
	virtual ~LProfile();
	
	void HideResultsIfBelow(int Ms);
	virtual void Add(const char *Name);
	virtual void Add(const char *File, int Line);
};

// This code will assert if the cast fails.
template<typename A, typename B>
A &AssertCast(A &a, B b)
{
	a = (A) b;	 // If this breaks it'll assert.
	LAssert((B)a == b);
	return a;
}

#endif


