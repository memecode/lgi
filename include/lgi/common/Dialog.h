#pragma once

#include "lgi/common/Window.h"
#include "lgi/common/LgiRes.h"

/// \brief A top level dialog window.
///
/// LDialog's can either be modal or modeless. A modal dialog blocks the user from accessing
/// the parent LWindow until the dialog has been dismissed. A modeless dialog behaves like
/// any other top level window in that it doesn't block the user accessing any other top level
/// window while it's open.
///
/// LDialog's can be created in two different ways. Firstly you can create code to instantiate
/// all the various controls and add them manually to the LView::Children list. Or you can load
/// all the controls from a resource file. The resource files are in XML format and there is a
/// graphical tool <a href="http://www.memecode.com/lgires.php">LgiRes</a>.
///
/// Manual example:
/// \code
/// #define IDC_STRING			100
///
/// class Example : public LDialog
/// {
/// public:
///		char *Str;
///
/// 	Example(LView *p)
/// 	{
///			Str = 0;
/// 		SetParent(p);
/// 		LRect r(0, 0, 400, 300);
/// 		SetPos(p);
/// 		MoveToCenter();
/// 			
/// 		Children.Insert(new GEdit(IDC_STRING, 10, 10, 200, 20, ""));
/// 		Children.Insert(new GButton(IDOK, 10, 30, 80, 20, "Ok"));
/// 		Children.Insert(new GButton(IDCANCEL, 100, 30, 80, 20, "Cancel"));
/// 	}
///
///		~Example()
///		{
///			DeleteArray(Str);
///		}
/// 		
/// 	int OnNotify(LViewI *c, int Flags)
/// 	{
/// 		switch (c->GetId())
/// 		{
/// 			case IDOK:
///				{
///					Str = NewStr(GetCtrlName(IDC_STRING));
///					// fall thru
///				}
///				case IDCANCEL:
/// 			{
/// 				EndModal(c->GetId());
/// 				break;
/// 			}
/// 		}
/// 
/// 		return 0;
/// 	}
/// };
/// \endcode
///
/// This example shows how to insert child widgets into the window and then process clicks on the buttons
/// and finally return data to the caller via a public member variable. It is desirable to pass data using
/// this method because a dialog could be running in it's own thread on some systems and therefor should not
/// be accessing data structures outside of itself directly without locking them. If your main application'
/// doesn't have thread locking in place for it's main data structures then accessing them from the dialog
/// wouldn't be thread safe. This is mainly the case with the BeOS port of Lgi, but is good practise for
/// the Windows and Linux ports for cross platform compatibility.
///
/// The resource file method of creating dialogs is arguably the easier route to take once your've got it
/// set up. Firstly create a resource file using LgiRes with the same name as your programs executable, but
/// a different extension ("lr8"). When you call the GLgiRes::LoadFromResource function Lgi will automatically
/// look for a file named after the running executable with the right extension and load it. Then it will
/// find the dialog resource and instantiate all the controls specified in the resource. All the built in
/// Lgi controls are supported directly as tags in the XML file and you can create your own custom controls
/// that the resource loader can instantiate as well using the GViewFactory system.
///
/// Resource file example:
/// \code
/// #include "Resdefs.h" // For the dialog defines
///
/// class Example : public LDialog
/// {
/// public:
///		char *Str;
///
/// 	Example(LView *p)
/// 	{
///			Str = 0;
/// 		SetParent(p);
///			if (LoadFromResource(IDD_EXAMPLE))
///			{
/// 			MoveToCenter();
/// 		}
/// 	}
///
///		~Example()
///		{
///			DeleteArray(Str);
///		}
/// 		
/// 	int OnNotify(LViewI *c, int Flags)
/// 	{
/// 		switch (c->GetId())
/// 		{
/// 			case IDOK:
///				{
///					Str = NewStr(GetCtrlName(1));
///					// fall thru
///				}
///				case IDCANCEL:
/// 			{
/// 				EndModal(c->GetId());
/// 				break;
/// 			}
/// 		}
/// 
/// 		return 0;
/// 	}
/// };
/// \endcode
///
/// This assumes you have created an lr8 file with the resource named 'IDD_EXAMPLE' in it.
///
/// Now to actually call either of these dialogs you would use code like this:
/// \code
/// Example Dlg(MyWindow);
/// if (Dlg.DoModal() == IDOK)
/// {
/// 	// Do something with Dlg.Str
/// }
/// \endcode
///
/// The built in controls that you can use are:
/// <ul>
///		<li> GButton (Push button)
///		<li> GEdit (Edit box for text entry)
///		<li> GText (Static label)
///		<li> GCheckBox (Independent boolean)
///		<li> GCombo (Select one from many)
///		<li> GSlider (Select a position)
///		<li> GBitmap (Display an image)
///		<li> GProgress (Show a progress)
///		<li> GList (List containing LListItem)
///		<li> GTree (Hierarchy of GTreeItem)
///		<li> GRadioGroup (One of many selection using GRadioButton)
///		<li> GTabView (Containing GTabPage)
/// </ul>
class LgiClass LDialog :
	public LWindow,
	public LResourceLoad,
	public ResObject
{
	friend class GControl;

private:
    struct LDialogPriv *d;

public:
	/// Constructor
	LDialog();
	
	/// Destructor
	~LDialog();

	const char *GetClass() { return "LDialog"; }

	/// Load the dialog from a resource
	bool LoadFromResource
	(
		/// The resource ID
		int Resource,
		/// [Optional] tag list to exclude/include various controls via tag
		char *TagList = 0
	);

	/// \brief Run the dialog in modal mode.
	///
	/// The user has to dismiss the dialog to continue.
	virtual int DoModal
	(
		/// Optional override parent window handle
		OsView ParentHnd = NULL
	);
	
	/// \brief Run the dialog in modeless mode
	///
	/// It will behave like any other top level window. The user doesn't
	/// have to dismiss the window to continue, it can just move to the back.
	virtual int DoModeless();
	
	/// Gets the model status
	virtual bool IsModal();
	
	/// End a modal window. Typically calling in the OnNotify event of the LDialog.
	virtual void EndModal(int Code = 0);

	/// End a modeless window. Typically calling in the OnNotify event of the LDialog.
	virtual void EndModeless(int Code = 0);

	GMessage::Result OnEvent(GMessage *Msg);
	bool OnRequestClose(bool OsClose);
	void OnPosChange();
    void PourAll() {}
	void Quit(bool DontDelete = false);

	/// By default the dialog will finish when a button is pressed. To override this
	/// behavior you'll have to subclass LDialog and handle the OnNotify yourself.
	int OnNotify(LViewI *Ctrl, int Flags);

	/// This returns the ID of the button pressed to close the dialog.
	int GetButtonId();
	
	#if defined(__GTK_H__)
	friend Gtk::gboolean GtkDialogDestroy(Gtk::GtkWidget *widget, LDialog *This);
    bool IsResizeable();
    void IsResizeable(bool r);
	// Gtk::gboolean OnGtkEvent(Gtk::GtkWidget *widget, Gtk::GdkEvent *event);
	bool SetupDialog(bool Modal);
	#elif defined(LGI_CARBON)
	void OnPaint(LSurface *pDC);
	#endif
};

