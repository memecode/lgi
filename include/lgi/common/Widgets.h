/**
	\file
	\author Matthew Allen
	\date 8/9/1998
	\brief Dialog widgets / components
*/

#ifndef __LGI_WIDGETS_H
#define __LGI_WIDGETS_H

/////////////////////////////////////////////////////////////////////////////////////////
#ifdef IDM_STATIC
#undef IDM_STATIC
#endif
#define IDM_STATIC				-1

/////////////////////////////////////////////////////////////////////////////////////////
class LDialog;
#if defined WIN32
class GDlgFunc;
#endif
class LListItem;
class GTreeItem;

class GButton;
class GEdit;
class GCheckBox;
class GTextLabel;
class GRadioGroup;
class GRadioButton;
class GTabView;
class GTabPage;
class GSlider;
class GCombo;
class GBitmap;
class LList;
class GTree;

//////////////////////////////////////////////////////////////////////////////////////
// Controls
#if defined WIN32
#define LGI_BUTTON			"LGI_Button"
#define LGI_EDITBOX			"LGI_Editbox"
#define LGI_CHECKBOX		"LGI_CheckBox"
#define LGI_TEXT			"LGI_Text"
#define LGI_RADIOGROUP		"LGI_RadioGroup"
#define LGI_TABCONTROL		"LGI_TabControl"
#define LGI_TABPAGE			"LGI_TabPage"
#define LGI_SLIDER			"LGI_Slider"
#define LGI_COMBO			"LGI_Combo"
#define LGI_BITMAP			"LGI_Bitmap"
#define LGI_LIST			"LGI_ListView"
#define LGI_TREE			"LGI_TreeView"
#define LGI_PROGRESS		"LGI_Progress"
#define LGI_SCROLLBAR		"LGI_ScrollBar"
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// Mac specific
#ifdef MAC

struct GLabelData
{
	LView *Ctrl;
	LSurface *pDC;
	LRect r;
	int Justification;
	
	GLabelData()
	{
		Justification = 0;
		r.ZOff(0, 0);
		pDC = 0;
		Ctrl = 0;
	}
};

#if defined LGI_CARBON
void LgiLabelProc(	const Rect *r,
					ThemeButtonKind kind,
					const ThemeButtonDrawInfo *info,
					UInt32 UserData,
					SInt16 depth,
					Boolean ColourDev);

extern ThemeButtonDrawUPP LgiLabelUPP;
#endif

#endif

#endif

