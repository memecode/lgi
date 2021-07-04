#ifndef _GSKIN_ENGINE_H_
#define _GSKIN_ENGINE_H_

#include "lgi/common/List.h"
#include "lgi/common/Button.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/TabView.h"
#include "lgi/common/Slider.h"
#include "lgi/common/Bitmap.h"
#include "lgi/common/ProgressView.h"
#include "lgi/common/Tree.h"

// Feature support flags
#define GSKIN_COLOUR		0x00000001
#define GSKIN_BUTTON		0x00000002
#define GSKIN_EDIT			0x00000004
#define GSKIN_CHECKBOX		0x00000008
#define GSKIN_RADIO			0x00000010
#define GSKIN_GROUP			0x00000020
#define GSKIN_SLIDER		0x00000040
#define GSKIN_COMBO			0x00000080
#define GSKIN_BITMAP		0x00000100
#define GSKIN_PROGRESS		0x00000200
#define GSKIN_TREE			0x00000400
#define GSKIN_LIST			0x00000800
#define GSKIN_LISTCOL		0x00001000
#define GSKIN_TABVIEW		0x00002000

// Drawing state
class LSkinState
{
	GArray<LDisplayString*> Tmp;
	
public:
	int Size;						// Class size, for version checking
	LSurface *pScreen;				// Output surface
	GArray<LDisplayString*> *aText;	// Array of display strings for the view
	LDisplayString **ptrText;		// Ptr to ptr for display string
	LRect Rect;						// Region to paint (if relevant)
	bool MouseOver;					// TRUE if the mouse is over the view
	int64 Value;					// Value of the control if available
	bool Enabled;					// TRUE if the control is enabled
	bool Focus;						// TRUE if the control has focus
	bool ForceUpdate;				// TRUE if cached info should be discarded
	LSurface *Image;				// Any icon that needs displaying
	LView *View;

	LSkinState()
	{
		aText = NULL;
		ptrText = NULL;
		Value = 0;
		Enabled = true;
		Focus = false;
		Size = sizeof(*this);
		pScreen = 0;
		MouseOver = false;
		Image = NULL;
		View = NULL;
		ForceUpdate = false;
	}

	size_t TextObjects()
	{
		if (aText)
			return aText->Length();
		if (ptrText)
			return *ptrText != NULL ? 1 : 0;
		return 0;
	}
	
	LDisplayString *FirstText()
	{
		if (aText)
		{
			return aText->Length() > 0 ? aText->First() : NULL;
		}
		else if (ptrText)
		{
			return *ptrText;
		}
		
		return NULL;
	}
	
	GArray<LDisplayString*> *AllText()
	{
		if (aText)
			return aText;
		
		if (ptrText && *ptrText && Tmp.Length(1))
		{
			Tmp[0] = *ptrText;
			return &Tmp;
		}
		
		return NULL;
	}
};

typedef void (*ProcColumnPaint)(void *UserData, LSurface *pDC, LRect &r, bool FillBackground);

// Engine class
class LSkinEngine
{
public:
	virtual ~LSkinEngine() {}

	// Return the features the skin supports. Return the
	// bitwise OR of all the features you support (the GSKIN_?? flags)
	//
	// The class is designed to be extended by adding more virtual functions
	// on the bottom and then Lgi queries the class to see if it can call them
	// by using the flags returned from this function. At no time should the
	// existing functions change order or be removed.
	virtual uint32_t GetFeatures() { return 0; }	

	// Return the GColour for the L_??? colour index
	// Will only be called if you return GSKIN_COLOUR from GetFeatures()
	virtual GColour GetColour(LSystemColour i) { return GColour(); }

	// Do painting for the various controls, the relevant GSKIN_??? flag needs to
	// be returned from GetFeatures before you can call any of these.
	virtual void OnPaint_LText        (LTextLabel *Ctrl,   LSkinState *State) {};
	virtual void OnPaint_LButton      (LButton *Ctrl,      LSkinState *State) {};
	virtual void OnPaint_LEdit        (LEdit *Ctrl,        LSkinState *State) {};
	virtual void OnPaint_LCheckBox    (LCheckBox *Ctrl,    LSkinState *State) {};
	virtual void OnPaint_LRadioGroup  (LRadioGroup *Ctrl,  LSkinState *State) {};
	virtual void OnPaint_LRadioButton (LRadioButton *Ctrl, LSkinState *State) {};
	virtual void OnPaint_LTabView     (LTabView *Ctrl,     LSkinState *State) {};
	virtual void OnPaint_LSlider      (LSlider *Ctrl,      LSkinState *State) {};
	virtual void OnPaint_LCombo       (LCombo *Ctrl,       LSkinState *State) {};
	virtual void OnPaint_LBitmap      (LBitmap *Ctrl,      LSkinState *State) {};
	virtual void OnPaint_LProgress    (LProgressView *Ctrl,LSkinState *State) {};
	virtual void OnPaint_LTree        (LTree *Ctrl,        LSkinState *State) {};
	virtual void OnPaint_LList        (LList *Ctrl,        LSkinState *State) {};
	
	// 'Col' may be NULL in the case that the GList control wants to draw the
	// header beyond the last column. This function can use the call back
	// GListColumn::OnPaint_Content to draw the text, icon and mark.
	virtual void OnPaint_ListColumn(ProcColumnPaint Callback, void *UserData, LSkinState *State) = 0;

	// Get the default font for a control
	virtual LFont *GetDefaultFont(char *Class) { return SysFont; }

	// Fills an abitary path with the skin's default fill...
	virtual void FillPath(class LPath *Path, LSurface *pDC, GColour Back, bool Down, bool Enabled = true) {}

	// Draws a button
	virtual void DrawBtn(LSurface *pDC, LRect &r, GColour Back, bool Down, bool Enabled, bool Default = false) = 0;

	///////////////////////////////////////////////////////////////////////////////
	// Add new features down here with an associated feature flag defined above. //
	///////////////////////////////////////////////////////////////////////////////
};

#define LgiSkinEntryPoint		"CreateSkinEngine"
typedef LSkinEngine				*(*Proc_CreateSkinEngine)(class GApp *App);

#endif
