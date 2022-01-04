/// \file
/// \author Matthew Allen
#ifndef _GHTML2_H
#define _GHTML2_H

#include "lgi/common/DocView.h"
#include "lgi/common/HtmlCommon.h"

namespace Html2
{

class LTag;
class LFontCache;

/// A lightwight scripting safe HTML control. It has limited CSS support, renders
/// most tables, even when nested. You can provide support for loading external
/// images by implementing the LDocumentEnv::GetImageUri method of the 
/// LDocumentEnv interface and passing it into LHtml2::SetEnv.
/// All attempts to open URL's are passed into LDocumentEnv::OnNavigate method of the
/// environment if set. Likewise any content inside active scripting tags, e.g. &lt;? ?&gt;
/// will be striped out of the document and passed to LDocumentEnv::OnDynamicContent, which
/// should return the relevant HTML that the script resolves to. A reasonable default
/// implementation of the LDocumentEnv interface is the GDefaultDocumentEnv object.
///
/// You can set the content of the control through the LHtml2::Name method.
///
/// Retreive any selected content through LHtml2::GetSelection.
class LHtml2 :
	public LDocView,
	public ResObject
{
	friend class LTag;
	friend class LFlowRegion;

	class LHtmlPrivate2 *d;

protected:	
	// Data
	LFontCache			*FontCache;
	LTag				*Tag;				// Tree root
	LTag				*Cursor;			// Cursor location..
	LTag				*Selection;			// Edge of selection or NULL
	List<LTag>			OpenTags;
	char				*Source;
	char				*DocCharSet;
	char				IsHtml;
	int					ViewWidth;
	GToolTip			Tip;
	LTag				*PrevTip;
	LCss::Store			CssStore;
	
	// Display
	LSurface			*MemDC;

	// This lock is separate from the window lock to avoid deadlocks.
	struct GJobSem : public LMutex
	{
    	// Data that has to be accessed under Lock
	    LArray<LDocumentEnv::LoadJob*> Jobs;	    
	    GJobSem() : LMutex("GJobSem") {}
	} JobSem;

	// Methods
	void _New();
	void _Delete();
	LFont *DefFont();
	LTag *GetOpenTag(char *Tag);
	void CloseTag(LTag *t);
	void Parse();
	void AddCss(char *Css);
	int ScrollY();
	void SetCursorVis(bool b);
	bool GetCursorVis();
	LRect *GetCursorPos();
	bool IsCursorFirst();
	int GetTagDepth(LTag *Tag);
	LTag *PrevTag(LTag *t);
	LTag *NextTag(LTag *t);
	LTag *GetLastChild(LTag *t);

public:
	LHtml2(int Id, int x, int y, int cx, int cy, LDocumentEnv *system = 0);
	~LHtml2();

	// Html
	const char *GetClass() { return "LHtml2"; }
	bool GetFormattedContent(const char *MimeType, LString &Out, LArray<LDocView::ContentMedia> *Media = 0);

	/// Get the tag at an x,y location
	LTag *GetTagByPos(int x, int y, int *Index);
	/// Layout content and return size.
	LPoint Layout();

	// Options
	bool GetLinkDoubleClick();
	void SetLinkDoubleClick(bool b);
	void SetLoadImages(bool i);
	bool GetEmoji();
	void SetEmoji(bool i);

	// LDocView
	bool SetVariant(const char *Name, LVariant &Value, char *Array = 0);
	
	/// Copy the selection to the clipboard
	bool Copy();
	/// Returns true if there is a selection
	bool HasSelection();
	/// Unselect all the text in the control
	void UnSelectAll();
	/// Select all the text in the control (not impl)
	void SelectAll();
	/// Return the selection in a dynamically allocated string
	char *GetSelection();
	
	// Prop

	// Window

	/// Sets the HTML content of the control
	bool Name(const char *s);
	/// Returns the HTML content
	char *Name();
	/// Sets the HTML content of the control
	bool NameW(const char16 *s);
	/// Returns the HTML content
	char16 *NameW();

	// Impl
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
	LCursor GetCursor(int x, int y);
	bool OnMouseWheel(double Lines);
	bool OnKey(LKey &k);
	int OnNotify(LViewI *c, int f);
	void OnPosChange();
	void OnPulse();
	LMessage::Result OnEvent(LMessage *Msg);
	const char *GetMimeType() { return "text/html"; }
	void OnContent(LDocumentEnv::LoadJob *Res);
	bool GotoAnchor(char *Name);

	// Javascript handlers
	GDom *getElementById(char *Id);

	// Events
	bool OnFind(class LFindReplaceCommon *Params);
	virtual bool OnSubmitForm(LTag *Form);
	virtual void OnCursorChanged() {}
};

}
#endif
