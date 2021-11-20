/// \file
/// \author Matthew Allen
#ifndef _GHTML_H
#define _GHTML_H

#include "lgi/common/DocView.h"
#include "lgi/common/HtmlCommon.h"
#include "lgi/common/HtmlParser.h"
#include "lgi/common/Capabilities.h"
#include "lgi/common/ToolTip.h"
#include "lgi/common/FindReplaceDlg.h"

namespace Html1
{

class GTag;
class LFontCache;

/// A lightwight scripting safe HTML control. It has limited CSS support, renders
/// most tables, even when nested. You can provide support for loading external
/// images by implementing the LDocumentEnv::GetImageUri method of the 
/// LDocumentEnv interface and passing it into GHtml2::SetEnv.
/// All attempts to open URL's are passed into LDocumentEnv::OnNavigate method of the
/// environment if set. Likewise any content inside active scripting tags, e.g. &lt;? ?&gt;
/// will be striped out of the document and passed to LDocumentEnv::OnDynamicContent, which
/// should return the relevant HTML that the script resolves to. A reasonable default
/// implementation of the LDocumentEnv interface is the GDefaultDocumentEnv object.
///
/// You can set the content of the control through the GHtml2::Name method.
///
/// Retreive any selected content through GHtml2::GetSelection.
class GHtml :
	public LDocView,
	public ResObject,
	public LHtmlParser,
	public LCapabilityClient
{
	friend class GTag;
	friend class GFlowRegion;

	class GHtmlPrivate *d;

protected:	
	// Data
	LFontCache			*FontCache;
	GTag				*Tag;				// Tree root
	GTag				*Cursor;			// Cursor location..
	GTag				*Selection;			// Edge of selection or NULL
	char				IsHtml;
	int					ViewWidth;
	uint64_t			PaintStart;
	GToolTip			Tip;
	LCss::Store			CssStore;
	LHashTbl<ConstStrKey<char,false>, bool> CssHref;
	
	// Display
	LAutoPtr<LSurface>	MemDC;

	// This lock is separate from the window lock to avoid deadlocks.
	struct GJobSem : public LMutex
	{
    	// Data that has to be accessed under Lock
	    LArray<LDocumentEnv::LoadJob*> Jobs;
	    GJobSem() : LMutex("GJobSem") {}
	} JobSem;

	// Methods
	void _New();
	void _Delete() override;
	LFont *DefFont();
	void CloseTag(GTag *t);
	void ParseDocument(const char *Doc);
	void OnAddStyle(const char *MimeType, const char *Styles) override;
	int ScrollY();
	void SetCursorVis(bool b);
	bool GetCursorVis();
	LRect *GetCursorPos();
	bool IsCursorFirst();
	bool CompareTagPos(GTag *a, ssize_t AIdx, GTag *b, ssize_t BIdx);
	int GetTagDepth(GTag *Tag);
	GTag *PrevTag(GTag *t);
	GTag *NextTag(GTag *t);
	GTag *GetLastChild(GTag *t);

public:
	GHtml(int Id, int x, int y, int cx, int cy, LDocumentEnv *system = 0);
	~GHtml();

	// Html
	const char *GetClass() override { return "GHtml"; }
	bool GetFormattedContent(const char *MimeType, LString &Out, LArray<LDocView::ContentMedia> *Media = 0) override;

	/// Get the tag at an x,y location
	GTag *GetTagByPos(	int x, int y,
						ssize_t *Index,
						LPoint *LocalCoords = NULL,
						bool DebugLog = false);
	/// Layout content and return size.
	LPoint Layout(bool ForceLayout = false);

	// Options
	bool GetLinkDoubleClick();
	void SetLinkDoubleClick(bool b);
	void SetLoadImages(bool i) override;
	bool GetEmoji();
	void SetEmoji(bool i);

	// LDocView
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	
	/// Copy the selection to the clipboard
	bool Copy() override;
	/// Returns true if there is a selection
	bool HasSelection() override;
	/// Unselect all the text in the control
	void UnSelectAll() override;
	/// Select all the text in the control (not impl)
	void SelectAll() override;
	/// Return the selection in a dynamically allocated string
	char *GetSelection() override;
	
	// Prop

	// Window

	/// Sets the HTML content of the control
	bool Name(const char *s) override;
	/// Returns the HTML content
	const char *Name() override;
	/// Sets the HTML content of the control
	bool NameW(const char16 *s) override;
	/// Returns the HTML content
	const char16 *NameW() override;

	// Impl
	void OnPaint(LSurface *pDC) override;
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	LCursor GetCursor(int x, int y) override;
	bool OnMouseWheel(double Lines) override;
	bool OnKey(LKey &k) override;
	int OnNotify(LViewI *c, LNotification n) override;
	void OnPosChange() override;
	void OnPulse() override;
	LMessage::Result OnEvent(LMessage *Msg) override;
	const char *GetMimeType() override { return "text/html"; }
	void OnContent(LDocumentEnv::LoadJob *Res) override;
	bool GotoAnchor(char *Name);
	LHtmlElement *CreateElement(LHtmlElement *Parent) override;
	bool EvaluateCondition(const char *Cond) override;
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	bool DoFind() override;
	LPointF GetDpiScale();
	void SetVScroll(int64 v);

	// Javascript handlers
	LDom *getElementById(char *Id);

	// Events
	bool OnFind(GFindReplaceCommon *Params);
	virtual bool OnSubmitForm(GTag *Form);
	virtual void OnCursorChanged() {}
	virtual void OnLoad();
	virtual bool OnContextMenuCreate(struct GTagHit &Hit, LSubMenu &RClick) { return true; }
	virtual void OnContextMenuCommand(struct GTagHit &Hit, int Cmd) {}
};

}
#endif
