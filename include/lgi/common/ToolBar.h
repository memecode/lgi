/// \file
///	\author Matthew Allen
/// \brief Toolbar and Imagelist classes.

#ifndef __GTOOLBAR_H
#define __GTOOLBAR_H

#include "lgi/common/ImageList.h"
#include "lgi/common/Layout.h"

///////////////////////////////////////////////////////////////////////////////////////////////
// Toolbar stuff

// Tool button types

/// Push type toolbar button
#define TBT_PUSH				0
/// Radio type toolbar button, uses separators to define the domain
#define TBT_RADIO				1
/// Toggle type toolbar button
#define TBT_TOGGLE				2

// Tool icon types
#define TOOL_ICO_NEXT			-1
#define TOOL_ICO_NONE			-2

// Commands
#define IDM_NONE				0
#define IDM_SEPARATOR			-1
#define IDM_BREAK				-2

// Dimensions
#define BORDER_SHADE			1
#define BORDER_SEPARATOR		4
#define BORDER_BUTTON			1

/// Button on a LToolBar
class LgiClass LToolButton :
	public LView
{
	friend class LToolBar;

protected:
	struct LToolButtonPriv *d;

	int			Type;		// Button Type
	bool		Clicked;
	bool		Down;
	bool		Over;
	int			ImgIndex;
	int			TipId;
	bool		NeedsRightClick;

	#if defined LGI_CARBON
	HIToolbarItemRef ItemRef;
	#endif

	void Layout();

public:
	LToolButton(int Bx, int By);
	~LToolButton();

	const char *GetClass() override { return "LToolButton"; }

	int64 Value() override { return Down; }
	void Value(int64 i) override;

	const char *Name() override { return LView::Name(); }
	bool Name(const char *n) override;

	/// Gets the icon index into the parent LToolBar's image list
	int Image() { return ImgIndex; }
	/// Sets the icon index into the parent LToolBar's image list
	void Image(int i);

	int GetType() { return Type; }
	void SetType(int i) { Type = i; }
	bool Separator() { return GetId() == IDM_SEPARATOR; }
	void Separator(bool i) { SetId(IDM_SEPARATOR); }
	bool GetNeedsRightClick() { return NeedsRightClick; }
	void SetNeedsRightClick(bool b) { NeedsRightClick = b; }

	void OnPaint(LSurface *pDC) override;

	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	void OnMouseEnter(LMouse &m) override;
	void OnMouseExit(LMouse &m) override;

	virtual void SendCommand();
	virtual bool GetDimension(int &x, int &y) { return false; }
};

/** \brief A top level window toolbar

	A toolbar hosts buttons and separators in a row. The buttons contain
	icons and optionally text describing there function. When the user clicks
	a button a M_COMMAND message is passed up to the owning LWindow for the
	application to handle. In the same fashion as a menu command being clicked.
	You should override LWindow::OnCommand to catch events from a LToolBar.

	This should be attached to the LWindow before other windows so that it
	lays itself out under the menu.

	To initialize and attach a toolbar to your LWindow use something like:
	\code
	LToolBar *t = LgiLoadToolbar(this, "icons.png");
	if (t)
	{
		t->Attach(this);
		t->AppendButton("Open", IDM_OPEN, TBT_PUSH);
		t->AppendSeparator();
		t->AppendButton("Help", IDM_HELP, TBT_PUSH);
	}
	\endcode
*/
class LgiClass LToolBar : public LLayout, public LDom
{
	friend class LToolButton;

protected:
	class LToolBarPrivate *d;

	// Local
	int GetBx();
	int GetBy();
	bool GetVariant(const char *Name, LVariant &Value, const char *Array) override;

	// Overridable
	virtual void ContextMenu(LMouse &m);	
	virtual int PostDescription(LView *Ctrl, const char *Text);

	#if defined(LGI_CARBON)
	HIToolbarRef ToolbarRef;
	#endif

public:
	LToolBar();
	~LToolBar();

	const char *GetClass() { return "LToolBar"; }

	/// Called when a button is clicked
	virtual void OnButtonClick(LToolButton *Btn);

	/// True if the toolbar should layout in a vertical manner
	bool IsVertical();
	/// True if the toolbar should layout in a vertical manner
	void IsVertical(bool v);
	/// Shows text labels under the buttons
	bool TextLabels();
	/// Shows text labels under the buttons
	void TextLabels(bool i);
	/// Returns true if the the customizable menu is on [default: off]
	bool IsCustomizable();
	/// Switch for the customization menu.
	void Customizable
	(
		/// Set this to the properties store to switch on the right click menu, or NULL to switch it off.
		LDom *Store = 0,
		/// This is the property that the toolbar stores state under. It should be unique for every toolbar.
		const char *Option = 0
	);
	/// Sets the image list to use via a file
	bool SetBitmap(char *File, int Bx, int By);
	/// Sets the image list to use via a memory surface
	bool SetDC(LSurface *pDC, int Bx, int By);
	/// Gets the image list
	LImageList *GetImageList();
	/// Sets the image list to use
	bool SetImageList(LImageList *l, int Bx, int By, bool Own = true);
	/// Gets the font used to draw the text below the buttons
	LFont *GetFont();

	/// Adds a button to the toolbar
	LToolButton *AppendButton
	(
		/// The buttons help tip
		const char *Tip,
		/// The ID to post to the main window when activated
		int Id,
		/// The type of button
		/// \sa The define TBT_PUSH and below
		int Type = TBT_PUSH,
		/// Whether to enable the button
		int Enabled = true,
		/// The index into the image list to use on the button
		int IconId = -1
	);
	
	/// Appends any old control
	bool AppendControl(LView *Ctrl);
	
	/// Append a separator
	bool AppendSeparator();
	
	/// Append a line break (wraps the buttons onto the next line)
	bool AppendBreak();
	
	/// Empties the toolbar of buttons
	void Empty();

	/// Gets the number of buttons
	size_t Length() { return Children.Length(); }

	// Events
	LMessage::Result OnEvent(LMessage *Msg);
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void OnMouseEnter(LMouse &m);
	void OnMouseExit(LMouse &m);
	void OnMouseMove(LMouse &m);
	bool Pour(LRegion &r);
	bool OnLayout(LViewLayoutInfo &Inf);
	void OnCreate();

	#ifdef MAC
	bool Attach(LViewI *parent);
	
	class Custom : public LView
	{
	public:
		Custom();
		~Custom();
	};
	#endif
};

/// Loads a toolbar from a file
LgiFunc LToolBar *LgiLoadToolbar
(
	/// A parent window for error message boxes
	LViewI *Parent,
	/// The graphics file to load as the image list
	const char *File,
	/// The width of the icons
	int x = 24,
	/// The height of the icons
	int y = 24
);

#endif
