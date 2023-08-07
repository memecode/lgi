/// \file
/// \author Matthew Allen
/// \brief A unicode text editor

#ifndef _RICH_TEXT_EDIT_H_
#define _RICH_TEXT_EDIT_H_

#include "lgi/common/DocView.h"
#include "lgi/common/Undo.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Capabilities.h"
#include "lgi/common/FindReplaceDlg.h"
#if _DEBUG
#include "lgi/common/Tree.h"
#endif

enum RichEditMsgs
{
	M_BLOCK_MSG	= M_USER + 0x1000,
	M_IMAGE_LOAD_FILE,
	M_IMAGE_SET_SURFACE,
	M_IMAGE_ERROR,
	M_IMAGE_COMPONENT_MISSING,
	M_IMAGE_PROGRESS,
	M_IMAGE_RESAMPLE,
	M_IMAGE_FINISHED,
	M_IMAGE_COMPRESS,
	M_IMAGE_ROTATE,
	M_IMAGE_FLIP,
	M_IMAGE_LOAD_STREAM,
	M_COMPONENT_INSTALLED, // A = LString *ComponentName
};

extern char Delimiters[];

/// Styled unicode text editor control.
class
#if defined(MAC)
	LgiClass
#endif
	LRichTextEdit :
	public LDocView,
	public ResObject,
	public LDragDropTarget,
	public LCapabilityClient
{
	friend bool RichText_FindCallback(LFindReplaceCommon *Dlg, bool Replace, void *User);

public:
	enum LTextViewSeek
	{
		PrevLine,
		NextLine,
		StartLine,
		EndLine
	};

protected:
	class LRichTextPriv *d;
	friend class LRichTextPriv;

	bool IndexAt(int x, int y, ssize_t &Off, int &LineHint);
	
	// Overridables
	virtual void PourText(ssize_t Start, ssize_t Length);
	virtual void PourStyle(ssize_t Start, ssize_t Length);
	virtual void OnFontChange();
	virtual void OnPaintLeftMargin(LSurface *pDC, LRect &r, LColour &colour);

public:
	// Construction
	LRichTextEdit(	int Id,
					int x = 0,
					int y = 0,
					int cx = 100,
					int cy = 100,
					LFontType *FontInfo = 0);
	~LRichTextEdit();

	const char *GetClass() { return "LRichTextEdit"; }

	// Data
	const char *Name();
	bool Name(const char *s);
	const char16 *NameW();
	bool NameW(const char16 *s);
	int64 Value();
	void Value(int64 i);
	const char *GetMimeType() { return "text/html"; }
	int GetSize();
	const char *GetCharset();
	void SetCharset(const char *s);

	ssize_t HitTest(int x, int y);
	bool DeleteSelection(char16 **Cut = 0);
	bool SetSpellCheck(class LSpellCheck *sp);
	
	bool GetFormattedContent(const char *MimeType, LString &Out, LArray<ContentMedia> *Media = NULL);

	// Dom	
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL);
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL);

	// Font
	LFont *GetFont();
	void SetFont(LFont *f, bool OwnIt = false);
	void SetFixedWidthFont(bool i);

	// Options
	void SetTabSize(uint8_t i);
	void SetReadOnly(bool i);
	bool ShowStyleTools();
	void ShowStyleTools(bool b);

	enum RectType
	{
		ContentArea,
		ToolsArea,
		// CapabilityArea,
		// CapabilityBtn,

		FontFamilyBtn,
		FontSizeBtn,
		
		BoldBtn,
		ItalicBtn,
		UnderlineBtn,
		
		ForegroundColourBtn,
		BackgroundColourBtn,

		MakeLinkBtn,
		RemoveLinkBtn,
		RemoveStyleBtn,
	
		EmojiBtn,
		HorzRuleBtn,
		
		MaxArea
	};
	LRect GetArea(RectType Type);

	/// Sets the wrapping on the control, use #L_WRAP_NONE or #L_WRAP_REFLOW
	void SetWrapType(LDocWrapType i);
	
	// State / Selection
	void SetCursor(int i, bool Select, bool ForceFullUpdate = false);
	ssize_t IndexAt(int x, int y);
	bool IsDirty();
	void IsDirty(bool d);
	bool HasSelection();
	void UnSelectAll();
	void SelectWord(size_t From);
	void SelectAll();
	ssize_t GetCaret(bool Cursor = true);
	bool GetLineColumnAtIndex(LPoint &Pt, ssize_t Index = -1);
	size_t GetLines();
	void GetTextExtent(int &x, int &y);
	char *GetSelection();
	void SetStylePrefix(LString s);
	bool IsBusy(bool Stop = false);

	// File IO
	bool Open(const char *Name, const char *Cs = 0);
	bool Save(const char *Name, const char *Cs = 0);

	// Clipboard IO
	bool Cut();
	bool Copy();
	bool Paste();

	// Undo/Redo
	void Undo();
	void Redo();
	bool GetUndoOn();
	void SetUndoOn(bool b);

	// Action UI
	virtual void DoGoto(std::function<void(bool)> Callback);
	virtual void DoCase(std::function<void(bool)> Callback, bool Upper);
	virtual void DoFind(std::function<void(bool)> Callback);
	virtual void DoFindNext(std::function<void(bool)> Callback);
	virtual void DoReplace(std::function<void(bool)> Callback);

	// Action Processing	
	bool ClearDirty(bool Ask, const char *FileName = 0);
	void UpdateScrollBars(bool Reset = false);
	int GetLine();
	void SetLine(int Line);
	LDocFindReplaceParams *CreateFindReplaceParams();
	void SetFindReplaceParams(LDocFindReplaceParams *Params);
	void OnAddStyle(const char *MimeType, const char *Styles);

	// Object Events
	bool OnFind(LFindReplaceCommon *Params);
	bool OnReplace(LFindReplaceCommon *Params);
	bool OnMultiLineTab(bool In);
	void OnSetHidden(int Hidden);
	void OnPosChange();
	void OnCreate();
	void OnEscape(LKey &K);
	bool OnMouseWheel(double Lines);

	// Capability target stuff
	// bool NeedsCapability(const char *Name, const char *Param = NULL);
	// void OnInstall(CapsHash *Caps, bool Status);
	// void OnCloseInstaller();

	// Window Events
	void OnFocus(bool f);
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
	bool OnKey(LKey &k);
	void OnPaint(LSurface *pDC);
	LMessage::Result OnEvent(LMessage *Msg);
	int OnNotify(LViewI *Ctrl, LNotification n);
	void OnPulse();
	int OnHitTest(int x, int y);
	bool OnLayout(LViewLayoutInfo &Inf);

	// D'n'd target
	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState);
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState);

	// Virtuals
	bool Insert(size_t At, const char16 *Data, ssize_t Len) override;
	bool Delete(size_t At, ssize_t Len) override;
	virtual void OnEnter(LKey &k);
	virtual void OnUrl(char *Url);
	virtual void DoContextMenu(LMouse &m);

	#if _DEBUG
	void DumpNodes(LTree *Root);
	void SelectNode(LString Param);
	#endif
};

#endif
