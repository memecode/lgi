/// \file
/// \author Matthew Allen
/// \brief A unicode text editor

#ifndef _GTEXTVIEW4_H
#define _GTEXTVIEW4_H

#include "lgi/common/DocView.h"
#include "lgi/common/Undo.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Css.h"
#include "lgi/common/UnrolledList.h"
#include "lgi/common/FindReplaceDlg.h"

// use CRLF as opposed to just LF
// internally it uses LF only... this is just to remember what to
// save out as.
#define TEXTED_USES_CR				0x00000001
#define TAB_SIZE					4
#define DEBUG_TIMES_MSG				8000 // a=0 b=(char*)Str

extern char Delimiters[];

class LTextView4;

/// Unicode text editor control.
class LgiClass LTextView4 :
	public LDocView,
	public ResObject,
	public LDragDropTarget
{
	friend struct LTextView4Undo;
	friend bool Text4_FindCallback(LFindReplaceCommon *Dlg, bool Replace, void *User);

public:
	enum Messages
	{
		M_TEXTVIEW_DEBUG_TEXT = M_USER + 0x3421,
		M_TEXTVIEW_FIND,
		M_TEXTVIEW_REPLACE,
		M_TEXT_POUR_CONTINUE,
	};

	enum StyleOwners
	{
		STYLE_NONE,
		STYLE_IDE,
		STYLE_SPELLING,
		STYLE_FIND_MATCHES,
		STYLE_ADDRESS,
		STYLE_URL,
	};

	class LStyle
	{
	protected:
		void RefreshLayout(size_t Start, ssize_t Len);

	public:
		/// The view the style is for
		LTextView4 *View;
		/// When you write several bits of code to do styling assign them
		/// different owner id's so that they can manage the lifespan of their
		/// own styles. LTextView4::PourStyle is owner '0', anything else it
		/// will leave alone.
		StyleOwners Owner;
		/// The start index into the text buffer of the region to style.
		ssize_t Start;
		/// The length of the styled region
		ssize_t Len;
		/// The font to draw the styled text in
		LFont *Font;
		/// The colour to draw with. If transparent, then the default 
		/// line colour is used.
		LColour Fore, Back;
		/// Cursor
		LCursor Cursor;		
		/// Optional extra decor not supported by the fonts
		LCss::TextDecorType Decor;
		/// Colour for the optional decor.
		LColour DecorColour;

		/// Application base data
		LVariant Data;

		LStyle(StyleOwners owner = STYLE_NONE)
		{
			Owner = owner;
			View = NULL;
			Font = NULL;
			Empty();
			Cursor = LCUR_Normal;
			Decor = LCss::TextDecorNone;
		}

		LStyle(const LStyle &s)
		{
			Owner = s.Owner;
			View = s.View;
			Font = s.Font;
			Start = s.Start;
			Len = s.Len;
			Decor = s.Decor;
			DecorColour = s.DecorColour;
			Fore = s.Fore;
			Back = s.Back;
			Data = s.Data;
			Cursor = s.Cursor;
		}
		
		LStyle &Construct(LTextView4 *view, StyleOwners owner)
		{
			View = view;
			Owner = owner;
			Font = NULL;
			Empty();
			Cursor = LCUR_Normal;
			Decor = LCss::TextDecorNone;
			return *this;
		}

		void Empty()
		{			
			Start = -1;
			Len = 0;
		}

		bool Valid()
		{
			return Start >= 0 && Len > 0;
		}

		size_t End() const { return Start + Len; }

		/// \returns true if style is the same
		bool operator ==(const LStyle &s)
		{
			return	Owner == s.Owner &&
					Start == s.Start &&
					Len == s.Len &&
					Fore == s.Fore &&
					Back == s.Back &&
					Decor == s.Decor;
		}

		/// Returns true if this style overlaps the position of 's'
		bool Overlap(LStyle &s)
		{
			return Overlap(s.Start, s.Len);
		}

		/// Returns true if this style overlaps the position of 's'
		bool Overlap(ssize_t sStart, ssize_t sLen)
		{
			if (sStart + sLen - 1 < Start ||
				sStart >= Start + Len)
				return false;

			return true;
		}

		void Union(const LStyle &s)
		{
			if (Start < 0)
			{
				Start = s.Start;
				Len = s.Len;
			}
			else
			{
				Start = MIN(Start, s.Start);
				Len = MAX(End(), s.End()) - Start;
			}
		}
	};

	friend class LTextView4::LStyle;

protected:
	// Internal classes
	enum GTextViewSeek
	{
		PrevLine,
		NextLine,
		StartLine,
 		EndLine
	};

	class LTextLine : public LRange
	{
	public:
		LRect r;		// Screen location
		LColour c;		// Colour of line... transparent = default colour
		LColour Back;	// Background colour or transparent

		LTextLine()
		{
			Start = -1;
			Len = 0;
			r.ZOff(-1, -1);
		}
		virtual ~LTextLine() {}

		size_t CalcLen(char16 *Text)
		{
			char16 *c = Text + Start, *e = c;
			while (*e && *e != '\n')
				e++;
			return Len = e - c;
		}
	};
	
	class LTextView4Private *d;
	friend class LTextView4Private;

	// Options
	bool Dirty;
	bool CanScrollX;

	// Display
	LFont *Font;
	LFont *Bold;		// Bold variant of 'Font'
	LFont *Underline;	// Underline variant of 'Font'

	LFont *FixedFont;
	int LineY;
	ssize_t SelStart, SelEnd;
	int DocOffset;
	int MaxX;
	bool Blink;
	uint64 BlinkTs;
	int ScrollX;
	LRect CursorPos;

	/// true if the text pour process is still ongoing
	bool PourEnabled;		// True if pouring the text happens on edit. Turn off if doing lots
							// of related edits at the same time. And then manually pour once 
							// finished.
	bool PartialPour;		// True if the pour is happening in the background. It's not threaded
							// but taking place in the GUI thread via timer.
	bool AdjustStylePos;	// Insert/Delete moved styles automatically to match (default: true)

	LArray<LTextLine*> Line;
	LUnrolledList<LStyle> Style;		// sorted in 'Start' order
	typedef LUnrolledList<LStyle>::Iter StyleIter;

	// For ::Name(...)
	char *TextCache;

	// Data
	char16 *Text;
	ssize_t Cursor;
	ssize_t Size;
	ssize_t Alloc;

	// Undo stuff
	bool UndoOn;
	LUndo UndoQue;
	struct LTextView4Undo *UndoCur;

	// private methods
	LArray<LTextLine*>::I GetTextLineIt(ssize_t Offset, ssize_t *Index = 0);
	LTextLine *GetTextLine(ssize_t Offset, ssize_t *Index = 0)
	{
		auto it = GetTextLineIt(Offset, Index);
		return it != Line.end() ? *it : NULL;
	}
	ssize_t SeekLine(ssize_t Offset, GTextViewSeek Where);
	int TextWidth(LFont *f, char16 *s, int Len, int x, int Origin);
	bool ScrollToOffset(size_t Off);
	int ScrollYLine();
	int ScrollYPixel();
	LRect DocToScreen(LRect r);
	ptrdiff_t MatchText(const char16 *Text, bool MatchWord, bool MatchCase, bool SelectionOnly, bool SearchUpwards);
	void InternalPulse();

	// styles
	bool InsertStyle(LAutoPtr<LStyle> s);
	LStyle *GetNextStyle(StyleIter &it, ssize_t Where = -1);
	LStyle *HitStyle(ssize_t i);
	int GetColumn();
	int SpaceDepth(char16 *Start, char16 *End);
	int AdjustStyles(ssize_t Start, ssize_t Diff, bool ExtendStyle = false);

	// Overridables
	virtual void PourText(size_t Start, ssize_t Length);
	virtual void PourStyle(size_t Start, ssize_t Length);
	virtual void OnFontChange();
	virtual void OnPaintLeftMargin(LSurface *pDC, LRect &r, LColour &colour);
	virtual char16 *MapText(char16 *Str, ssize_t Len, bool RtlTrailingSpace = false);

	#ifdef _DEBUG
	// debug
	uint64 _PourTime;
	uint64 _StyleTime;
	uint64 _PaintTime;
	#endif

	void LogLines();
	bool ValidateLines(bool CheckBox = false);

public:
	// Construction
	LTextView4(	int Id,
				int x = 0, int y = 0,
				int cx = 100, int cy = 100,
				LFontType *FontInfo = NULL);
	~LTextView4();

	const char *GetClass() override { return "LTextView4"; }

	// Data
	const char *Name() override;
	bool Name(const char *s) override;
	const char16 *NameW() override;
	bool NameW(const char16 *s) override;
	int64 Value() override;
	void Value(int64 i) override;
	const char *GetMimeType() override { return "text/plain"; }
	size_t Length() const { return Size; }
	LString operator[](ssize_t LineIdx);

	ssize_t HitText(int x, int y, bool Nearest);
	void DeleteSelection(char16 **Cut = 0);

	// Font
	LFont *GetFont() override;
	LFont *GetBold();
	void SetFont(LFont *f, bool OwnIt = false) override;
	void SetFixedWidthFont(bool i) override;

	// Options
	void SetTabSize(uint8_t i) override;
	void SetBorder(int b);
	void SetReadOnly(bool i) override;
	void SetCrLf(bool crlf) override;

	/// Sets the wrapping on the control, use #TEXTED_WRAP_NONE or #TEXTED_WRAP_REFLOW
	void SetWrapType(LDocWrapType i) override;
	
	// State / Selection
	ssize_t GetCaret(bool Cursor = true) override;
	virtual void SetCaret(size_t i, bool Select = false, bool ForceFullUpdate = false) override;
	ssize_t IndexAt(int x, int y) override;
	bool IsDirty() override { return Dirty; }
	void IsDirty(bool d) { Dirty = d; }
	bool HasSelection() override;
	void UnSelectAll() override;
	void SelectWord(size_t From) override;
	void SelectAll() override;
	bool GetLineColumnAtIndex(LPoint &Pt, ssize_t Index = -1) override;
	size_t GetLines() override;
	void GetTextExtent(int &x, int &y) override;
	char *GetSelection() override;
	LRange GetSelectionRange();

	// File IO
	bool Open(const char *Name, const char *Cs = 0) override;
	bool Save(const char *Name, const char *Cs = 0) override;
	const char *GetLastError();

	// Clipboard IO
	bool Cut() override;
	bool Copy() override;
	bool Paste() override;

	// Undo/Redo
	void Undo();
	void Redo();
	bool GetUndoOn() { return UndoOn; }
	void SetUndoOn(bool b) { UndoOn = b; }

	// Action UI
	virtual void DoGoto(std::function<void(bool)> Callback);
	virtual void DoCase(std::function<void(bool)> Callback, bool Upper);
	virtual void DoFind(std::function<void(bool)> Callback) override;
	virtual void DoFindNext(std::function<void(bool)> Callback);
	virtual void DoReplace(std::function<void(bool)> Callback) override;

	// Action Processing	
	void ClearDirty(std::function<void(bool)> OnStatus, bool Ask, const char *FileName = 0);
	void UpdateScrollBars(bool Reset = false);
	ssize_t GetLine();
	void SetLine(int64_t Line);
	LDocFindReplaceParams *CreateFindReplaceParams() override;
	void SetFindReplaceParams(LDocFindReplaceParams *Params) override;

	// Object Events
	virtual bool OnFind(	const char16 *Find,
							bool MatchWord,
							bool MatchCase,
							bool SelectionOnly,
							bool SearchUpwards);
	virtual bool OnReplace(	const char16 *Find,
							const char16 *Replace,
							bool All,
							bool MatchWord,
							bool MatchCase,
							bool SelectionOnly,
							bool SearchUpwards);
	bool OnMultiLineTab(bool In);
	void OnSetHidden(int Hidden);
	void OnPosChange() override;
	void OnCreate() override;
	void OnEscape(LKey &K) override;
	bool OnMouseWheel(double Lines) override;

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
	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState) override;
	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState) override;
	LCursor GetCursor(int x, int y) override;

	// Virtuals
	virtual bool Insert(size_t At, const char16 *Data, ssize_t Len);
	virtual bool Delete(size_t At, ssize_t Len);
	virtual void OnEnter(LKey &k) override;
	virtual void OnUrl(char *Url) override;
	virtual void DoContextMenu(LMouse &m);
	virtual bool OnStyleClick(LStyle *style, LMouse *m);
	virtual bool OnStyleMenu(LStyle *style, LSubMenu *m);
	virtual void OnStyleMenuClick(LStyle *style, int i);
};

#endif // _GTEXTVIEW4_H
