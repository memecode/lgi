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

#include "LMutex.h"
#include "LgiOsClasses.h"
#include "GMem.h"
#include "GArray.h"
#include "LgiCommon.h"
#include "GXmlTree.h"
#include "GDragAndDrop.h"

/////////////////////////////////////////////////////////////////////////////////////
// Externs
LgiFunc bool LgiPostEvent(OsView Wnd, int Event, GMessage::Param a = 0, GMessage::Param b = 0);
LgiFunc GViewI *GetNextTabStop(GViewI *v, bool Back);

/// Converts an OS error code into a text string
LgiClass GString LErrorCodeToString(uint32_t ErrorCode);

#include "GApp.h"
#include "GView.h"
#include "GLayout.h"
#include "LMenu.h"
#include "GWindow.h"
#include "GToolTip.h"
#include "LgiWidgets.h"
#include "Progress.h"
#include "GProgress.h"
#include "GFileSelect.h"
#include "GFindReplaceDlg.h"
#include "GToolBar.h"
#include "LThread.h"
#include "GSplitter.h"
#include "GStatusBar.h"

/////////////////////////////////////////////////////////////////////////////////////////////
class LgiClass GCommand : public GBase //, public GFlags
{
	int			Flags;
	bool		PrevValue;

public:
	int			Id;
	GToolButton	*ToolButton;
	LMenuItem	*MenuItem;
	GKey		*Accelerator;
	char		*TipHelp;
	
	GCommand();
	~GCommand();

	bool Enabled();
	void Enabled(bool e);
	bool Value();
	void Value(bool v);
};


#include "GTrayIcon.h"
#include "GInput.h"

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
class LgiClass GAlert : public GDialog
{
public:
	/// Constructor
	GAlert
	(
		/// The parent view
		GViewI *parent,
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
	int OnNotify(GViewI *Ctrl, int Flags);
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
LgiFunc void LgiDrawBox(GSurface *pDC, GRect &r, bool Sunken, bool Fill);
LgiFunc void LgiWideBorder(GSurface *pDC, GRect &r, LgiEdge Type);
LgiFunc void LgiThinBorder(GSurface *pDC, GRect &r, LgiEdge Type);
LgiFunc void LgiFlatBorder(GSurface *pDC, GRect &r, int Width = -1);

// Helpers
#ifdef __GTK_H__
extern Gtk::gboolean GtkViewCallback(Gtk::GtkWidget *widget, Gtk::GdkEvent *event, GView *This);
#endif

#ifdef LINUX
/// Ends a x windows startup session
LgiFunc void LgiFinishXWindowsStartup(class GViewI *Wnd);
#endif

/// \brief Displays a message box
/// \returns The button clicked. The return value is one of #IDOK, #IDCANCEL, #IDYES or #IDNO.
LgiFunc int LgiMsg
(
	/// The parent view or NULL if none available
	GViewI *Parent,
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
LgiFunc void LDialogTextMsg(GViewI *Parent, const char *Title, GString Txt);

/// Contains all the information about a display/monitor attached to the system.
/// \sa LgiGetDisplays
struct GDisplayInfo
{
	/// The position and dimensions of the display. On windows the left/upper 
	/// most display will be positioned at 0,0 and each further display will have 
	/// co-ordinates that join to one edge of that initial rectangle.
	GRect r;
	/// The number of bits per pixel
	int BitDepth;
	/// The refresh rate
	int Refresh;
	/// The device's path, system specific
	char *Device;
	/// A descriptive name of the device, usually the video card
	char *Name;
	/// The name of any attached monitor
	char *Monitor;

	GDisplayInfo()
	{
		r.ZOff(-1, -1);
		BitDepth = 0;
		Refresh = 0;
		Device = 0;
		Name = 0;
		Monitor = 0;
	}

	~GDisplayInfo()
	{
		DeleteArray(Device);
		DeleteArray(Name);
		DeleteArray(Monitor);
	}
};

/// Returns infomation about the displays attached to the system.
/// \returns non-zero on success.
LgiFunc bool LgiGetDisplays
(
	/// [out] The array of display info structures. The caller should free these
	/// objects using Displays.DeleteObjects().
	GArray<GDisplayInfo*> &Displays,
	/// [out] Optional bounding rectangle of all displays. Can be NULL if your don't
	/// need that information.
	GRect *AllDisplays = 0
);

/// This class makes it easy to profile a function and write out timings at the end
class LgiClass GProfile
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
	
	GArray<Sample> s;
	char *Buf;
	int Used;
	int MinMs;
	
public:
	GProfile(const char *Name, int HideMs = -1);
	virtual ~GProfile();
	
	void HideResultsIfBelow(int Ms);
	virtual void Add(const char *Name);
	virtual void Add(const char *File, int Line);
};

// This code will assert if the cast fails.
template<typename A, typename B>
A &AssertCast(A &a, B b)
{
	a = (A) b;	 // If this breaks it'll assert.
	LgiAssert((B)a == b);
	return a;
}

#endif


