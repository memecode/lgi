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
					LFontType *FontInfo = NULL);
	~LRichTextEdit();

	const char *GetClass() override { return "LRichTextEdit"; }

	// Data
	const char *Name() override;
	bool Name(const char *s) override;
	const char16 *NameW() override;
	bool NameW(const char16 *s) override;
	int64 Value() override;
	void Value(int64 i) override;
	const char *GetMimeType() override { return "text/html"; }
	int GetSize();
	const char *GetCharset() override;
	void SetCharset(const char *s) override;

	ssize_t HitTest(int x, int y);
	bool DeleteSelection(char16 **Cut = NULL);
	bool SetSpellCheck(class LSpellCheck *sp);
	
	bool GetFormattedContent(const char *MimeType, LString &Out, LArray<ContentMedia> *Media = NULL) override;

	// Dom	
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;

	// Font
	LFont *GetFont() override;
	void SetFont(LFont *f, bool OwnIt = false) override;
	void SetFixedWidthFont(bool i) override;

	// Options
	void SetTabSize(uint8_t i) override;
	void SetReadOnly(bool i) override;
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
	void SetWrapType(LDocWrapType i) override;
	
	// State / Selection
	void SetCursor(int i, bool Select, bool ForceFullUpdate = false);
	ssize_t IndexAt(int x, int y) override;
	bool IsDirty() override;
	void IsDirty(bool d);
	bool HasSelection() override;
	void UnSelectAll() override;
	void SelectWord(size_t From) override;
	void SelectAll() override;
	ssize_t GetCaret(bool Cursor = true) override;
	bool GetLineColumnAtIndex(LPoint &Pt, ssize_t Index = -1) override;
	size_t GetLines() override;
	void GetTextExtent(int &x, int &y) override;
	char *GetSelection() override;
	void SetStylePrefix(LString s);
	bool IsBusy(bool Stop = false);

	// File IO
	bool Open(const char *Name, const char *Cs = NULL) override;
	bool Save(const char *Name, const char *Cs = NULL) override;

	// Clipboard IO
	bool Cut() override;
	bool Copy() override;
	bool Paste() override;

	// Undo/Redo
	void Undo();
	void Redo();
	bool GetUndoOn();
	void SetUndoOn(bool b);

	// Action UI
	virtual void DoGoto(std::function<void(bool)> Callback);
	virtual void DoCase(std::function<void(bool)> Callback, bool Upper);
	virtual void DoFind(std::function<void(bool)> Callback) override;
	virtual void DoFindNext(std::function<void(bool)> Callback);
	virtual void DoReplace(std::function<void(bool)> Callback) override;

	// Action Processing	
	bool ClearDirty(bool Ask, const char *FileName = NULL);
	void UpdateScrollBars(bool Reset = false);
	int GetLine();
	void SetLine(int Line);
	LDocFindReplaceParams *CreateFindReplaceParams() override;
	void SetFindReplaceParams(LDocFindReplaceParams *Params) override;
	void OnAddStyle(const char *MimeType, const char *Styles) override;

	// Object Events
	bool OnFind(LFindReplaceCommon *Params);
	bool OnReplace(LFindReplaceCommon *Params);
	bool OnMultiLineTab(bool In);
	void OnSetHidden(int Hidden);
	void OnPosChange() override;
	void OnCreate() override;
	void OnEscape(LKey &K) override;
	bool OnMouseWheel(double Lines) override;

	// Capability target stuff
	// bool NeedsCapability(const char *Name, const char *Param = NULL);
	// void OnInstall(CapsHash *Caps, bool Status);
	// void OnCloseInstaller();

	// Window Events
	void OnFocus(bool f) override;
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	bool OnKey(LKey &k) override;
	void OnPaint(LSurface *pDC) override;
	LMessage::Result OnEvent(LMessage *Msg) override;
	int OnNotify(LViewI *Ctrl, LNotification n) override;
	void OnPulse() override;
	int OnHitTest(int x, int y) override;
	bool OnLayout(LViewLayoutInfo &Inf) override;

	// D'n'd target
	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState) override;
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState) override;

	// Virtuals
	bool Insert(size_t At, const char16 *Data, ssize_t Len) override;
	bool Delete(size_t At, ssize_t Len) override;
	virtual void OnEnter(LKey &k) override;
	virtual void OnUrl(char *Url) override;
	virtual void DoContextMenu(LMouse &m);

	#if _DEBUG
	void DumpNodes(LTree *Root);
	void SelectNode(LString Param);
	#endif
};

#endif
