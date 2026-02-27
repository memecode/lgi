/* Rich text design notes:

- The document is an array of Blocks (Blocks have no hierarchy)
- Blocks have a length in characters. New lines are considered as one '\n' char.
- The main type of block is the TextBlock
	- TextBlock contains:
		- array of StyleText:
			This is the source text. Each run of text has a style associated with it.
			This forms the input to the layout algorithm and is what the user is 
			editing.
		- array of TextLine:
			Contains all the info needed to render one line of text. Essentially
			the output of the layout engine.
			Contains an array of DisplayStr objects. i.e. Characters in the exact
			same style as each other.
			It will regularly be deleted and re-flowed from the StyleText objects.
	- For a plaint text document the entire thing is contained by the one TextBlock.
- There is an Image block, where the image is treated as one character object.
- Also a horizontal rule block.

*/
#ifndef _RICH_TEXT_EDIT_PRIV_H_
#define _RICH_TEXT_EDIT_PRIV_H_

#include "lgi/common/HtmlCommon.h"
#include "lgi/common/HtmlParser.h"
#include "lgi/common/FontCache.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/ColourSpace.h"
#include "lgi/common/Popup.h"
#include "lgi/common/Emoji.h"
#include "lgi/common/SpellCheck.h"

#define DEBUG_LOG_CURSOR_COUNT			0
#define DEBUG_OUTLINE_CUR_DISPLAY_STR	0
#define DEBUG_OUTLINE_CUR_STYLE_TEXT	0
#define DEBUG_OUTLINE_BLOCKS			0
#define DEBUG_NO_DOUBLE_BUF				0
#define DEBUG_COVERAGE_CHECK			0
#define DEBUG_NUMBERED_LAYOUTS			0
#if 0 // _DEBUG
#define LOG_FN							LgiTrace
#else
#define LOG_FN							d->Log->Print
#endif

#define TEXT_LINK						"Link"
#define TEXT_REMOVE_LINK				"X"
#define TEXT_REMOVE_STYLE				"Remove Style"
#define TEXT_CAP_BTN					"Ok"
#define TEXT_EMOJI						":)"
#define TEXT_HORZRULE					"HR"

#define RTE_CURSOR_BLINK_RATE			1000
#define RTE_PULSE_RATE					200

#define RICH_TEXT_RESIZED_JPEG_QUALITY	83 // out of 100, high = better quality

#define NoTransaction					NULL
#define IsWordBreakChar(ch)				\
	( \
		( \
			(ch) == ' ' || (ch) == '\t' || (ch) == '\r' || (ch) == '\n' \
		) \
		|| \
		( \
			EmojiToIconIndex(&(ch), 1).Index >= 0 \
		) \
	)

enum RteCommands
{
	ID_NEW = 2000,
	ID_RTE_COPY,
	ID_RTE_CUT,
	ID_RTE_PASTE,
	ID_RTE_UNDO,
	ID_RTE_REDO,
	ID_COPY_URL,
	ID_AUTO_INDENT,
	ID_UTF8,
	ID_PASTE_NO_CONVERT,
	ID_FIXED,
	ID_SHOW_WHITE,
	ID_HARD_TABS,
	ID_INDENT_SIZE,
	ID_TAB_SIZE,
	ID_DUMP,
	ID_RTL,
	ID_COPY_ORIGINAL,
	ID_COPY_CURRENT,
	ID_DUMP_NODES,
	ID_CLOCKWISE,
	ID_ANTI_CLOCKWISE,
	ID_X_FLIP,
	ID_Y_FLIP,
	ID_SCALE_IMAGE,
	ID_OPEN_URL,

	CODEPAGE_BASE = 100,
	CONVERT_CODEPAGE_BASE = 200,
	SPELLING_BASE = 300
};

//////////////////////////////////////////////////////////////////////
#define PtrCheckBreak(ptr)				if (!ptr) { LAssert(!"Invalid ptr"); break; }
#undef FixedToInt
#define FixedToInt(fixed)				((fixed)>>LDisplayString::FShift)
#undef IntToFixed
#define IntToFixed(val)					((val)<<LDisplayString::FShift)

#define CursorColour					LColour(L_TEXT)
#define TextColour						LColour::Black

//////////////////////////////////////////////////////////////////////
#include "lgi/common/Range.h"

class LRichEditElem : public LHtmlElement
{
	LHashTbl<ConstStrKey<char,false>, LString> Attr;

public:
	LRichEditElem(LHtmlElement *parent) : LHtmlElement(parent)
	{
	}

	bool Get(const char *attr, const char *&val)
	{
		if (!attr)
			return false;

		LString s = Attr.Find(attr);
		if (!s)
			return false;
		
		val = s;
		return true;
	}
	
	void Set(const char *attr, const char *val)
	{
		if (!attr)
			return;
		Attr.Add(attr, LString(val));
	}
	
	void SetStyle()
	{		
	}
};

struct LRichEditElemContext : public LCss::ElementCallback<LRichEditElem>
{
	/// Returns the element name
	const char *GetElement(LRichEditElem *obj);
	/// Returns the document unque element ID
	const char *GetAttr(LRichEditElem *obj, const char *Attr);
	/// Returns the class
	bool GetClasses(LString::Array &Classes, LRichEditElem *obj);
	/// Returns the parent object
	LRichEditElem *GetParent(LRichEditElem *obj);
	/// Returns an array of child objects
	LArray<LRichEditElem*> GetChildren(LRichEditElem *obj);
};

class LDocFindReplaceParams3 : public LDocFindReplaceParams
{
public:
	// Find/Replace History
	char16 *LastFind = nullptr;
	char16 *LastReplace = nullptr;
	bool MatchCase = false;
	bool MatchWord = false;
	bool SelectionOnly = false;
	
	LDocFindReplaceParams3()
	{
	}

	~LDocFindReplaceParams3()
	{
		DeleteArray(LastFind);
		DeleteArray(LastReplace);
	}
};

struct LNamedStyle : public LCss
{
	int RefCount = 0;
	LString Name;
};

class LCssCache
{
	int Idx;
	LArray<LNamedStyle*> Styles;
	LString Prefix;

public:
	LCssCache();
	~LCssCache();

	void SetPrefix(LString s) { Prefix = s; }
	uint32_t GetStyles();
	void ZeroRefCounts();
	bool OutputStyles(LStream &s, int TabDepth);
	LNamedStyle *AddStyleToCache(LAutoPtr<LCss> &s);
};

class LRichTextPriv;
class SelectColour : public LPopup
{
	LRichTextPriv *d;
	LRichTextEdit::RectType Type;

	struct Entry
	{
		LRect r;
		LColour c;
	};
	LArray<Entry> e;

public:
	SelectColour(LRichTextPriv *priv, LPoint p, LRichTextEdit::RectType t);
	
	const char *GetClass() { return "SelectColour"; }

	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void Visible(bool i);
};

class EmojiMenu : public LPopup
{
	LRichTextPriv *d;

	struct Emoji
	{
		LRect Src, Dst;
		uint32_t u;
	};
	struct Pane
	{
		LRect Btn;
		LArray<Emoji> e;
	};
	LArray<Pane> Panes;
	static int Cur;

public:
	EmojiMenu(LRichTextPriv *priv, LPoint p);

	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void Visible(bool i);
	bool InsertEmoji(uint32_t Ch);
};

struct CtrlCap
{
	LString Name, Param;

	void Set(const char *name, const char *param)
	{
		Name = name;
		Param = param;
	}
};

struct ButtonState
{
	uint8_t IsMenu : 1;
	uint8_t IsPress : 1;
	uint8_t Pressed : 1;
	uint8_t MouseOver : 1;
};

extern bool Utf16to32(LArray<uint32_t> &Out, const uint16_t *In, ssize_t Len);

class LEmojiImage
{
	LAutoPtr<LSurface> EmojiImg;

public:
	LSurface *GetEmojiImage();
};

class LRichTextPriv :
	public LCss,
	public LHtmlParser,
	public LHtmlStaticInst,
	public LCssCache,
	public LFontCache,
	public LEmojiImage
{
	LStringPipe LogBuffer;

public:
	enum SelectModeType
	{
		Unselected = 0,
		Selected = 1,
	};

	enum SeekType
	{
		SkUnknown,
		
		SkLineStart,
		SkLineEnd,		
		SkDocStart,
		SkDocEnd,

		// Horizontal navigation		
		SkLeftChar,
		SkLeftWord,
		SkRightChar,
		SkRightWord,
		
		// Vertical navigation
		SkUpPage,
		SkUpLine,
		SkCurrentLine,		
		SkDownLine,
		SkDownPage,
	};

	struct DisplayStr;
	struct BlockCursor;
	class Block;

	LRichTextEdit *View;
	LString OriginalText;
	LAutoWString WideNameCache;
	LAutoString UtfNameCache;
	LAutoPtr<LFont> EditFont;
	LAutoPtr<LFont> UiFont;
	bool WordSelectMode = false;
	bool Dirty = false;
	LPoint DocumentExtent = {0, 0}; // Px
	LString Charset;
	LHtmlStaticInst Inst;
	int NextUid = 1;
	LStream *Log;
	bool HtmlLinkAsCid = false;
	uint64 BlinkTs = 0;

	// Spell check support
	LSpellCheck *SpellCheck = nullptr;
	bool SpellDictionaryLoaded = false;
	LString SpellLang, SpellDict;

	// This is set when the user changes a style without a selection,
	// indicating that we should start a new run when new text is entered
	LArray<LRichTextEdit::RectType> StyleDirty;

	// Toolbar
	bool ShowTools = true;
	LRichTextEdit::RectType ClickedBtn, OverBtn;
	ButtonState BtnState[LRichTextEdit::MaxArea];
	LRect Areas[LRichTextEdit::MaxArea];
	LVariant Values[LRichTextEdit::MaxArea];
	
	enum ToolbarIconIndex {
		IconBullets,
		IconNumbered
	};
	LAutoPtr<LSurface> toolbarIcons;	
	LAutoPtr<LSurface> ResizeIcon(ToolbarIconIndex index, int px);


	// Scrolling
	int ScrollLinePx;
	int ScrollOffsetPx = 0;
	bool ScrollChange = false;

	// Eat keys (OS bug work arounds)
	LArray<uint32_t> EatVkeys;

	// Debug stuff
	LArray<LRect> DebugRects;

	// Constructor
	LRichTextPriv(LRichTextEdit *view, LRichTextPriv **Ptr);
	~LRichTextPriv();
	
	bool Error(const char *file, int line, const char *fmt, ...);
	bool IsBusy(bool Stop = false);

	struct Flow
	{
		LRichTextPriv *d;
		LSurface *pDC;	// Used for printing.

		int Left, Right;// Left and right margin positions as measured in px
						// from the left of the page (controls client area).
		int Top;
		int CurY;		// Current y position down the page in document co-ords
		bool Visible;	// true if the current block overlaps the visible page
						// If false, the implementation can take short cuts and
						// guess various dimensions.
	
		Flow(LRichTextPriv *priv)
		{
			d = priv;
			pDC = NULL;
			Left = 0;
			Top = 0;
			Right = 1000;
			CurY = 0;
			Visible = true;
		}
		
		int X()
		{
			return Right - Left + 1;
		}
		
		LString Describe()
		{
			LString s;
			s.Printf("Left=%i Right=%i CurY=%i", Left, Right, CurY);
			return s;
		}
	};

	struct ColourPair
	{
		LColour Fore, Back;
		
		void Empty()
		{
			Fore.Empty();
			Back.Empty();
		}
	};

	/// This is a run of text, all of the same style
	class StyleText : public LArray<uint32_t>
	{
		LNamedStyle *Style = NULL; // owned by the CSS cache
	
	public:
		ColourPair Colours;
		HtmlTag Element;
		LString Param;
		bool Emoji;

		StyleText(const StyleText *St);
		StyleText(const uint32_t *t = NULL, ssize_t Chars = -1, LNamedStyle *style = NULL);
		uint32_t *At(ssize_t i);
		LNamedStyle *GetStyle();
		void SetStyle(LNamedStyle *s);
	};
	
	struct PaintContext
	{
		int Index;
		LSurface *pDC;
		SelectModeType Type;
		ColourPair Colours[2];
		BlockCursor *Cursor, *Select;

		// Cursor stuff
		int CurEndPoint;
		LArray<ssize_t> EndPoints;
		
		PaintContext()
		{
			Index = 0;
			pDC = NULL;
			Type = Unselected;
			Cursor = NULL;
			Select = NULL;
			CurEndPoint = 0;
		}
		
		LColour &Fore()
		{
			return Colours[Type].Fore;
		}

		LColour &Back()
		{
			return Colours[Type].Back;
		}

		void DrawBox(LRect &r, LRect &Edge, LColour &c)
		{
			if (Edge.x1 > 0 ||
				Edge.x2 > 0 ||
				Edge.y1 > 0 ||
				Edge.y2 > 0)
			{
				pDC->Colour(c);
				if (Edge.x1)
				{
					pDC->Rectangle(r.x1, r.y1, r.x1 + Edge.x1 - 1, r.y2);
					r.x1 += Edge.x1;
				}
				if (Edge.y1)
				{
					pDC->Rectangle(r.x1, r.y1, r.x2, r.y1 + Edge.y1 - 1);
					r.y1 += Edge.y1;
				}
				if (Edge.y2)
				{
					pDC->Rectangle(r.x1, r.y2 - Edge.y2 + 1, r.x2, r.y2);
					r.y2 -= Edge.y2;
				}
				if (Edge.x2)
				{
					pDC->Rectangle(r.x2 - Edge.x2 + 1, r.y1, r.x2, r.y2);
					r.x2 -= Edge.x2;
				}
			}
		}

		// This handles calculating the selection stuff for simple "one char" blocks
		// like images and HR. Call this at the start of the OnPaint.
		// \return TRUE if the content should be drawn selected.
		bool SelectBeforePaint(class LRichTextPriv::Block *b)
		{
			CurEndPoint = 0;

			if (b->Cursors > 0 && Select)
			{
				// Selection end point checks...
				if (Cursor && Cursor->Blk == b)
					EndPoints.Add(Cursor->Offset);
				if (Select && Select->Blk == b)
					EndPoints.Add(Select->Offset);
				
				// Sort the end points
				if (EndPoints.Length() > 1 &&
					EndPoints[0] > EndPoints[1])
				{
					ssize_t ep = EndPoints[0];
					EndPoints[0] = EndPoints[1];
					EndPoints[1] = ep;
				}
			}

			// Before selection end point
			if (CurEndPoint < (ssize_t)EndPoints.Length() &&
				EndPoints[CurEndPoint] == 0)
			{
				Type = Type == Selected ? Unselected : Selected;
				CurEndPoint++;
			}

			return Type == Selected;
		}

		// Call this after the OnPaint
		// \return TRUE if the content after the block is selected.
		bool SelectAfterPaint(class LRichTextPriv::Block *b)
		{
			// After image selection end point
			if (CurEndPoint < (ssize_t)EndPoints.Length() &&
				EndPoints[CurEndPoint] == 1)
			{
				Type = Type == Selected ? Unselected : Selected;
				CurEndPoint++;
			}

			return Type == Selected;
		}
	};

	struct HitTestResult
	{
		LPoint In;
		Block *Blk;
		DisplayStr *Ds;
		ssize_t Idx;
		int LineHint;
		bool Near;
		
		HitTestResult(int x, int y)
		{
			In.x = x;
			In.y = y;
			Blk = NULL;
			Ds = NULL;
			Idx = -1;
			LineHint = -1;
			Near = false;
		}
	};

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Undo structures...
	struct DocChange
	{
		virtual ~DocChange() {}
		virtual bool Apply(LRichTextPriv *Ctx, bool Forward) = 0;
	};

	class Transaction
	{
	public:		
		LArray<DocChange*> Changes;

		~Transaction()
		{
			Changes.DeleteObjects();
		}

		void Add(DocChange *Dc)
		{
			Changes.Add(Dc);
		}

		bool Apply(LRichTextPriv *Ctx, bool Forward)
		{
			for (unsigned i=0; i<Changes.Length(); i++)
			{
				DocChange *dc = Changes[i];
				if (!dc->Apply(Ctx, Forward))
					return false;
			}
			
			return true;
		}
	};

	LArray<Transaction*> UndoQue;
	ssize_t UndoPos = 0;
	bool UndoPosLock = false;

	bool AddTrans(LAutoPtr<Transaction> &t);
	bool SetUndoPos(ssize_t Pos);

	template<typename T>
	bool GetBlockByUid(T *&Ptr, int Uid, int *Idx = NULL)
	{
		for (unsigned i=0; i<Blocks.Length(); i++)
		{
			Block *b = Blocks[i];
			if (b->GetUid() == Uid)
			{
				if (Idx) *Idx = i;
				return (Ptr = dynamic_cast<T*>(b)) != NULL;
			}
		}

		if (Idx) *Idx = -1;
		return false;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// A Block is like a DIV in HTML, it's as wide as the page and
	// always starts and ends on a whole line.
	class Block :
		public LEventSinkI,
		public LEventTargetI
	{
	protected:
		int BlockUid;
		LRichTextPriv *d;

	public:
		/// This is the number of cursors current referencing this Block.
		int8 Cursors = 0;
		/// Draw debug selection
		bool DrawDebug = false;
		
		Block(LRichTextPriv *priv)
		{
			d = priv;
			BlockUid = d->NextUid++;
		}

		Block(const Block *blk)
		{
			d = blk->d;
			DrawDebug = false;
			BlockUid = blk->GetUid();
		}
		
		virtual ~Block()
		{
			// We must have removed cursors by the time we are deleted
			// otherwise there will be a hanging pointer in the cursor
			// object.
			LAssert(Cursors == 0);
		}
		
		// Events
		bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t TimeoutMs = -1)
		{
			bool r = d->View->PostEvent(M_BLOCK_MSG,
										(LMessage::Param)(Block*)this,
										(LMessage::Param)new LMessage(Cmd, a, b));
			#if defined(_DEBUG)
			if (!r)
				LgiTrace("%s:%i - Warning: PostEvent failed..\n", _FL);
			#endif
			return r;
		}

		// If this returns non-zero further command processing is aborted.
		LMessage::Result OnEvent(LMessage *Msg) override
		{
			return false;
		}

		/************************************************
		 * Get state methods, do not modify the block   *
		 ***********************************************/
			virtual const char *GetClass() { return "Block"; }
			virtual LRect GetPos() = 0;
			virtual ssize_t Length() = 0;
			virtual bool HitTest(HitTestResult &htr) = 0;
			virtual bool GetPosFromIndex(BlockCursor *Cursor) = 0;
			virtual bool OnLayout(Flow &f) = 0;
			virtual void OnPaint(PaintContext &Ctx) = 0;
			virtual bool ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rgn) = 0;
			virtual bool OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY) = 0;
			virtual ssize_t LineToOffset(ssize_t Line) = 0;
			virtual int GetLines() = 0;
			virtual ssize_t FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params) = 0;
			virtual void SetSpellingErrors(LArray<LSpellCheck::SpellingError> &Errors, LRange r) {}
			virtual void IncAllStyleRefs() {}
			virtual void Dump() {}
			virtual LNamedStyle *GetStyle(ssize_t At = -1) = 0;
			virtual int GetUid() const { return BlockUid; }
			virtual bool DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset /* internal to this block, not the whole doc. */, bool TopOfMenu) { return false; }
			#ifdef _DEBUG
			virtual void DumpNodes(LTreeItem *Ti) = 0;
			#endif
			virtual bool IsValid() { return false; }
			virtual bool IsBusy(bool Stop = false) { return false; }
			virtual Block *Clone() = 0;
			virtual void OnComponentInstall(LString Name) {}

			// Copy some or all of the text out
			virtual ssize_t CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text) { return false; }

			/// This method moves a cursor index.
			/// \returns the new cursor index or -1 on error.
			virtual bool Seek
			(
				/// [In] true if the next line is needed, false for the previous line
				SeekType To,
				/// [In/Out] The starting cursor.
				BlockCursor &Cursor
			) = 0;

		/************************************************
		 * Change state methods, require a transaction  *
		 ***********************************************/
			// Add some text at a given position
			virtual bool AddText
			(
				/// Current transaction
				Transaction *Trans,
				/// The index to add at (-1 = the end)
				ssize_t AtOffset,
				/// The text itself
				const uint32_t *Str,
				/// [Optional] The number of characters
				ssize_t Chars = -1,
				/// [Optional] Style to give the text, NULL means "use the existing style"
				LNamedStyle *Style = NULL
			)	{ return false; }

			/// Delete some chars
			/// \returns the number of chars actually removed
			virtual ssize_t DeleteAt
			(
				Transaction *Trans,
				ssize_t Offset,
				ssize_t Chars,
				LArray<uint32_t> *DeletedText = NULL
			)	{ return false; }

			/// Changes the style of a range of characters
			virtual bool ChangeStyle
			(
				Transaction *Trans,
				ssize_t Offset,
				ssize_t Chars,
				LCss *Style,
				bool Add
			)	{ return false; }

			virtual bool DoCase
			(
				/// Current transaction
				Transaction *Trans,
				/// Start index of text to change
				ssize_t StartIdx,
				/// Number of chars to change
				ssize_t Chars,
				/// True if upper case is desired
				bool Upper
			)	{ return false; }

			// Split a block
			virtual Block *Split
			(
				/// Current transaction
				Transaction *Trans,
				/// The index to add at (-1 = the end)
				ssize_t AtOffset
			)	{ return NULL; }
			// Event called on dictionary load
			virtual bool OnDictionary(Transaction *Trans) { return false; }
	};

	struct BlockCursor
	{
		// The block the cursor is in.
		Block *Blk;

		// This is the character offset of the cursor relative to
		// the start of 'Blk'.
		ssize_t Offset;

		// In wrapped text, a given offset can either be at the end
		// of one line or the start of the next line. This tells the
		// text block which line the cursor is actually on.
		int LineHint;
		
		// This is the position on the screen in doc coords.
		LRect Pos;
		
		// This is the position line that the cursor is on. This is
		// used to calculate the bounds for screen updates.
		LRect Line;

		// Cursor is currently blinking on
		bool Blink;

		BlockCursor(const BlockCursor &c);
		BlockCursor(Block *b, ssize_t off, int line);
		~BlockCursor();
		
		BlockCursor &operator =(const BlockCursor &c);
		void Set(ssize_t off);
		void Set(Block *b, ssize_t off, int line);
		bool operator ==(const BlockCursor &c)
		{
			return Blk == c.Blk &&
				Offset == c.Offset;
		}

		#ifdef _DEBUG
		void DumpNodes(LTreeItem *Ti);
		#endif
	};
	
	LAutoPtr<BlockCursor> Cursor, Selection;

	/// This is part or all of a Text run
	struct DisplayStr : public LDisplayString
	{
		StyleText *Src;
		ssize_t Chars;	// The number of UTF-32 characters. This can be different to
						// LDisplayString::Length() in the case that LDisplayString 
						// is using UTF-16 (i.e. Windows).
		int OffsetY;	// Offset of this string from the TextLine's box in the Y axis
		
		DisplayStr(StyleText *src, LFont *f, const uint32_t *s, ssize_t l = -1, LSurface *pdc = NULL) :
			LDisplayString(f,
				#ifndef WINDOWS
				(char16*)
				#endif
				s, l, pdc)
		{
			Src = src;
			OffsetY = 0;
			#if defined(_MSC_VER)
			Chars = l < 0 ? Strlen(s) : l;
			#else
			Chars = WideWords;
			#endif

			// LAssert(l == 0 || FX() > 0);
		}
		
		template<typename T>
		T *Utf16Seek(T *s, ssize_t i)
		{
			T *e = s + i;
			while (s < e)
			{
				uint16 n = *s & 0xfc00;
				if (n == 0xd800)
				{
					s++;
					if (s >= e)
						break;

					n = *s & 0xfc00;
					if (n != 0xdc00)
					{
						LAssert(!"Unexpected surrogate");
						continue;
					}
					// else skip over the 2nd surrogate
				}

				s++;
			}
			
			return s;
		}

		ssize_t WideLen()
		{
			#if defined(LGI_DSP_STR_CACHE)
			return WideWords;
			#else
			return StrWords;
			#endif
		}
		
		// Make a sub-string of this display string
		virtual LAutoPtr<DisplayStr> Clone(ssize_t Start, ssize_t Len = -1)
		{
			LAutoPtr<DisplayStr> c;
			auto WideW = WideLen();
			if (WideW > 0 && Len != 0)
			{
				const char16 *Str = *this;
				if (Len < 0)
					Len = WideW - Start;
				if (Start >= 0 &&
					Start < (int)WideW &&
					Start + Len <= (int)WideW)
				{
					#if defined(_MSC_VER)
					LAssert(Str != NULL);
					const char16 *s = Utf16Seek(Str, Start);
					const char16 *e = Utf16Seek(s, Len);
					LArray<uint32_t> Tmp;
					if (Utf16to32(Tmp, (const uint16_t*)s, e - s))
						c.Reset(new DisplayStr(Src, GetFont(), &Tmp[0], Tmp.Length(), pDC));
					#else
					c.Reset(new DisplayStr(Src, GetFont(), (uint32_t*)Str + Start, Len, pDC));
					#endif
				}
			}		
			return c;
		}

		virtual void Paint(LSurface *pDC, int &FixX, int FixY, LColour &Back)
		{
			FDraw(pDC, FixX, FixY);
			FixX += FX();
		}

		virtual double GetAscent()
		{
			return Font->Ascent();
		}
		
		virtual ssize_t PosToIndex(int x, bool Nearest)
		{
			return CharAt(x);
		}
	};
	
	struct EmojiDisplayStr : public DisplayStr
	{
		LArray<LRect> SrcRect;
		LSurface *Img = NULL;
		LCss::Len Size;
		int CharPx = 0;
		#if defined(_MSC_VER)
			LArray<uint32_t> Utf32;
		#endif

		EmojiDisplayStr(StyleText *src, LSurface *img, LCss::Len &fntSize, const uint32_t *s, ssize_t l = -1);
		LAutoPtr<DisplayStr> Clone(ssize_t Start, ssize_t Len = -1);
		void Paint(LSurface *pDC, int &FixX, int FixY, LColour &Back);
		double GetAscent();
		ssize_t PosToIndex(int XPos, bool Nearest);
	};

	/// This structure is a layout of a full line of text. Made up of one or more
	/// display string objects.
	struct TextLine
	{
		/// This is a position relative to the parent Block
		LRect PosOff;

		/// The array of display strings
		LArray<DisplayStr*> Strs;

		/// Is '1' for lines that have a new line character at the end.
		uint8_t NewLine;
		
		TextLine(int XOffsetPx, int WidthPx, int YOffsetPx);
		ssize_t Length();
		
		/// This runs after the layout line has been filled with display strings.
		/// It measures the line and works out the right offsets for each strings
		/// so that their baselines all match up correctly.
		void LayoutOffsets(int DefaultFontHt);
	};
	
	class TextBlock :
		public Block,
		public LCssBox
	{
		LNamedStyle *Style = nullptr;
		LSpellCheck::SpellingError *SpErr = nullptr;
		LArray<LSpellCheck::SpellingError> SpellingErrors;
		int PaintErrIdx = -1, ClickErrIdx = -1;
		LString ClickedUri;

		bool PreEdit(Transaction *Trans);
		void DrawDisplayString(LSurface *pDC, DisplayStr *Ds, int &FixX, int FixY, LColour &Bk, ssize_t &Pos);
	
	public:
		// Runs of characters in the same style: pre-layout.
		LArray<StyleText*> Txt;

		// Runs of characters (display strings) of potentially different styles on the same line: post-layout.
		LArray<TextLine*> Layout;
		// True if the 'Layout' data is out of date.
		bool LayoutDirty;

		// Default font for the block
		LFont *Fnt;
		
		// Chars in the whole block (sum of all Text lengths)
		ssize_t Len;
		
		// Position in document co-ordinates
		LRect Pos;
		
		TextBlock(LRichTextPriv *priv);
		TextBlock(const TextBlock *Copy);
		~TextBlock();

		bool IsValid();

		// No state change methods
		const char *GetClass() { return "TextBlock"; }
		int GetLines();
		bool OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY);
		ssize_t LineToOffset(ssize_t Line);
		LRect GetPos() { return Pos; }
		void Dump();
		LNamedStyle *GetStyle(ssize_t At = -1);
		void SetStyle(LNamedStyle *s);
		ssize_t Length();
		bool ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		ssize_t GetTextAt(ssize_t Offset, LArray<StyleText*> &t);
		ssize_t CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text);
		bool Seek(SeekType To, BlockCursor &Cursor);
		ssize_t FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params);
		void IncAllStyleRefs();
		void SetSpellingErrors(LArray<LSpellCheck::SpellingError> &Errors, LRange r);
		bool DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool Spelling);
		#ifdef _DEBUG
		void DumpNodes(LTreeItem *Ti);
		#endif
		Block *Clone();
		bool IsEmptyLine(BlockCursor *Cursor);
		void UpdateSpellingAndLinks(Transaction *Trans, LRange r);

		// Events
		LMessage::Result OnEvent(LMessage *Msg);

		// Transactional changes
		bool AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars = -1, LNamedStyle *Style = NULL);
		bool ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add);
		ssize_t DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText = NULL);
		bool DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper);
		Block *Split(Transaction *Trans, ssize_t AtOffset);
		bool StripLast(Transaction *Trans, const char *Set = " \t\r\n"); // Strip trailing new line if present..
		bool OnDictionary(Transaction *Trans);
	};

	class HorzRuleBlock : public Block
	{
		LRect Pos;
		bool IsDeleted;

	public:
		HorzRuleBlock(LRichTextPriv *priv);
		HorzRuleBlock(const HorzRuleBlock *Copy);
		~HorzRuleBlock();

		bool IsValid();

		// No state change methods
		const char *GetClass() { return "HorzRuleBlock"; }
		int GetLines();
		bool OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY);
		ssize_t LineToOffset(ssize_t Line);
		LRect GetPos() { return Pos; }
		void Dump();
		LNamedStyle *GetStyle(ssize_t At = -1);
		void SetStyle(LNamedStyle *s);
		ssize_t Length();
		bool ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		ssize_t GetTextAt(ssize_t Offset, LArray<StyleText*> &t);
		ssize_t CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text);
		bool Seek(SeekType To, BlockCursor &Cursor);
		ssize_t FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params);
		void IncAllStyleRefs();
		bool DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool Spelling);
		#ifdef _DEBUG
		void DumpNodes(LTreeItem *Ti);
		#endif
		Block *Clone();

		// Events
		LMessage::Result OnEvent(LMessage *Msg);

		// Transactional changes
		bool AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars = -1, LNamedStyle *Style = NULL);
		bool ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add);
		ssize_t DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText = NULL);
		bool DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper);
		Block *Split(Transaction *Trans, ssize_t AtOffset);
	};

	// Unordered or numbered list
	class ListBlock :
		public Block
	{
	public:
		using TType = LCss::ListStyleTypes;

	protected:
		LRect Pos;
		TType type = TType::ListInherit;
		bool startItem = false;
		
		LArray<LRect> items;
		LArray<Block*> blocks;

	public:
		ListBlock(LRichTextPriv *priv, TType lstType);
		ListBlock(const ListBlock *Copy);
		~ListBlock();
		
		const char *TypeToElem();
		TType GetType() { return type; }
		void StartItem() { startItem = true; }
		TextBlock *GetTextBlock();

		const char *GetClass() override { return "ListBlock"; }
		LMessage::Result OnEvent(LMessage *Msg) override;
		LRect GetPos() override;
		ssize_t Length() override;
		bool HitTest(HitTestResult &htr) override;
		bool GetPosFromIndex(BlockCursor *Cursor) override;
		bool OnLayout(Flow &f) override;
		void OnPaint(PaintContext &Ctx) override;
		bool ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rgn) override;
		bool OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY) override;
		ssize_t LineToOffset(ssize_t Line) override;
		int GetLines() override;
		ssize_t FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params) override;
		void SetSpellingErrors(LArray<LSpellCheck::SpellingError> &Errors, LRange r) override;
		void IncAllStyleRefs() override;
		void Dump() override;
		LNamedStyle *GetStyle(ssize_t At = -1) override;
		bool DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset /* internal to this block, not the whole doc. */, bool TopOfMenu) override;
		#ifdef _DEBUG
		void DumpNodes(LTreeItem *Ti) override;
		#endif
		bool IsValid() override;
		bool IsBusy(bool Stop = false) override;
		Block *Clone() override;
		void OnComponentInstall(LString Name) override;
		ssize_t CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text) override;
		bool Seek(SeekType To, BlockCursor &Cursor) override;

		// Edit
		bool AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars = -1, LNamedStyle *Style = NULL) override;
		ssize_t DeleteAt(Transaction *Trans, ssize_t Offset, ssize_t Chars, LArray<uint32_t> *DeletedText = NULL) override;
		bool ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add) override;
		bool DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper) override;
		Block *Split(Transaction *Trans, ssize_t AtOffset) override;
		bool OnDictionary(Transaction *Trans) override;
	};

	class ImageBlock :
		public Block
	{
	public:
		struct ScaleInf
		{
			LPoint Sz;
			LString MimeType;
			LAutoPtr<LStreamI> Compressed;
			int Percent = 0;
			bool Compressing = false;

			LString ToString()
			{
				return LString::Fmt("sz=%s, mime=%s, comp=%s, pc=%i",
					Sz.GetStr().Get(),
					MimeType.Get(),
					LFormatSize(Compressed ? Compressed->GetSize() : 0).Get(),
					Percent);
			}
		};

		int ThreadHnd = 0;

	protected:
		LNamedStyle *Style = NULL;
		int Scale = 1;
		LRect SourceValid;
		LString FileName;
		LString ContentId;
		LString StreamMimeType;
		LString FileMimeType;

		enum ResizeIdxSource
		{
			SourceNone,
			SourceDefault,
			SourceUser,
			SourceMaxImage,
		};
		const char *ToString(ResizeIdxSource s)
		{
			switch (s)
			{
			case SourceNone:     return "SourceNone";
			case SourceDefault:  return "SourceDefault";
			case SourceUser:     return "SourceUser";
			case SourceMaxImage: return "SourceMaxImage";
			}
			return NULL;
		}

		int ResizeIdx = -1;
		ResizeIdxSource ResizeSrc = SourceNone;

		LArray<ScaleInf> Scales;
		int ThreadBusy = 0;
		bool IsDeleted = false;
		void UpdateThreadBusy(const char *File, int Line, int Off);

		int GetThreadHandle();
		void UpdateDisplay(int y);
		void UpdateDisplayImg();
		

	public:
		LAutoPtr<LSurface> SourceImg, DisplayImg, SelectImg;
		LRect Margin, Border, Padding;
		LString Source;
		LPoint Size;
		
		bool LayoutDirty = false;
		LRect Pos; // position in document co-ordinates
		LRect ImgPos;
		
		ImageBlock(LRichTextPriv *priv);
		ImageBlock(const ImageBlock *Copy);
		~ImageBlock();

		bool IsValid();
		bool IsBusy(bool Stop = false);
		bool Load(const char *Src = NULL);
		bool SetImage(LAutoPtr<LSurface> Img);
		void OnDimensions();
		void GetCompressedSize();
		void MaxImageFilter();

		// No state change methods
		int GetLines();
		bool OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY);
		ssize_t LineToOffset(ssize_t Line);
		LRect GetPos() { return Pos; }
		void Dump();
		LNamedStyle *GetStyle(ssize_t At = -1);
		void SetStyle(LNamedStyle *s);
		ssize_t Length();
		bool ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng);
		bool GetPosFromIndex(BlockCursor *Cursor);
		bool HitTest(HitTestResult &htr);
		void OnPaint(PaintContext &Ctx);
		bool OnLayout(Flow &flow);
		ssize_t GetTextAt(ssize_t Offset, LArray<StyleText*> &t);
		ssize_t CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text);
		bool Seek(SeekType To, BlockCursor &Cursor);
		ssize_t FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params);
		void IncAllStyleRefs();
		bool DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool Spelling);
		#ifdef _DEBUG
		void DumpNodes(LTreeItem *Ti);
		#endif
		Block *Clone();
		void OnComponentInstall(LString Name);

		// Events
		LMessage::Result OnEvent(LMessage *Msg);

		// Transactional changes
		bool AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars = -1, LNamedStyle *Style = NULL);
		bool ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add);
		ssize_t DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText = NULL);
		bool DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper);
	};
	
	LArray<Block*> Blocks;
	Block *Next(Block *b);
	Block *Prev(Block *b);

	void InvalidateDoc(LRect *r);
	void ScrollTo(LRect r);
	void UpdateStyleUI();

	void EmptyDoc();	
	void Empty();
	bool Seek(BlockCursor *In, SeekType Dir, bool Select);
	bool CursorFirst();
	bool SetCursor(LAutoPtr<BlockCursor> c, bool Select = false);
	LRect SelectionRect();
	bool GetSelection(LArray<char16> *Text, LAutoString *Html);
	ssize_t IndexOfCursor(BlockCursor *c);
	ssize_t HitTest(int x, int y, int &LineHint, Block **Blk = NULL, ssize_t *BlkOffset = NULL);
	bool CursorFromPos(int x, int y, LAutoPtr<BlockCursor> *Cursor, ssize_t *GlobalIdx);
	Block *GetBlockByIndex(ssize_t Index, ssize_t *Offset = NULL, int *BlockIdx = NULL, int *LineCount = NULL);
	bool Layout(LScrollBar *&ScrollY);
	void OnStyleChange(LRichTextEdit::RectType t);
	bool ChangeSelectionStyle(LCss *Style, bool Add);
	void PaintBtn(LSurface *pDC, LRichTextEdit::RectType t);
	bool MakeLink(TextBlock *tb, ssize_t Offset, ssize_t Len, LString Link);
	bool ClickBtn(LMouse &m, LRichTextEdit::RectType t);
	bool InsertHorzRule();
	void Paint(LSurface *pDC, LScrollBar *&ScrollY);
	LHtmlElement *CreateElement(LHtmlElement *Parent);
	LPoint ScreenToDoc(int x, int y);
	LPoint DocToScreen(int x, int y);
	bool Merge(Transaction *Trans, Block *a, Block *b);
	bool DeleteSelection(Transaction *t, char16 **Cut);
	LRichTextEdit::RectType PosToButton(LMouse &m);
	void OnComponentInstall(LString Name);

	struct CreateContext
	{
		LRichTextPriv *d;
		
		TextBlock *Tb = nullptr;
		ImageBlock *Ib = nullptr;
		HorzRuleBlock *Hrb = nullptr;
		ListBlock *Lst = nullptr;
		LArray<uint32_t> Buf;
		uint32_t LastChar = '\n';
		LCss::Store StyleStore;
		bool StartOfLine = true;
		
		CreateContext(LRichTextPriv *priv) :
			d(priv)
		{
		}
		
		TextBlock *GetTextBlock()
		{
			if (Lst)
				return Lst->GetTextBlock();
				
			if (!Tb)
			{
				Tb = new TextBlock(d);
				d->Blocks.Add(Tb);
			}
				
			return Tb;
		}

		bool AddText(LNamedStyle *Style, char16 *Str)
		{
			auto insertTb = GetTextBlock();
			if (!Str || !insertTb)
				return false;
			
			int Used = 0;
			char16 *s = Str;
			char16 *e = s + StrlenW(s);
			while (s < e)
			{
				if (*s == '\r')
				{
					s++;
					continue;
				}
				
				if (IsWhite(*s))
				{
					Buf[Used++] = ' ';
					while (s < e && IsWhite(*s))
						s++;
				}
				else
				{
					#ifdef WINDOWS
						ssize_t Len = s[0] && s[1] ? 4 : (s[0] ? 2 : 0);
						Buf[Used++] = LgiUtf16To32((const uint16 *&)s, Len);
					#else
						Buf[Used++] = *s++;
					#endif
					while (s < e && !IsWhite(*s))
					{
						#ifdef WINDOWS
							Len = s[0] && s[1] ? 4 : (s[0] ? 2 : 0);
							Buf[Used++] = LgiUtf16To32((const uint16 *&)s, Len);
						#else
							Buf[Used++] = *s++;
						#endif
					}
				}
			}
			
			bool Status = false;
			if (Used > 0)
			{
				Status = insertTb->AddText(NoTransaction, -1, &Buf[0], Used, Style);
				LastChar = Buf[Used-1];
			}
			return Status;
		}
	};
	
	LAutoPtr<CreateContext> CreationCtx;

	bool ToHtml(LArray<LDocView::ContentMedia> *Media = NULL, BlockCursor *From = NULL, BlockCursor *To = NULL);
	void DumpBlocks();
	bool FromHtml(LHtmlElement *e, CreateContext &ctx, LCss *ParentStyle = NULL, int Depth = 0);

	#ifdef _DEBUG
	void DumpNodes(LTree *Root);
	#endif
};

struct BlockCursorState
{
	bool Cursor;
	ssize_t Offset;
	int LineHint;
	int BlockUid;

	BlockCursorState(bool cursor, LRichTextPriv::BlockCursor *c);
	bool Apply(LRichTextPriv *Ctx, bool Forward);
};

struct CompleteTextBlockState : public LRichTextPriv::DocChange
{
	int Uid;
	LAutoPtr<BlockCursorState> Cur, Sel;
	LAutoPtr<LRichTextPriv::TextBlock> Blk;

	CompleteTextBlockState(LRichTextPriv *Ctx, LRichTextPriv::TextBlock *Tb);
	bool Apply(LRichTextPriv *Ctx, bool Forward);
};

struct MultiBlockState : public LRichTextPriv::DocChange
{
	LRichTextPriv *Ctx;
	ssize_t Index; // Number of blocks before the edit
	ssize_t Length; // Of the other version currently in the Ctx stack
	LArray<LRichTextPriv::Block*> Blks;
	
	MultiBlockState(LRichTextPriv *ctx, ssize_t Start);
	bool Apply(LRichTextPriv *Ctx, bool Forward);

	bool Copy(ssize_t Idx);
	bool Cut(ssize_t Idx);
};

#ifdef _DEBUG
LTreeItem *PrintNode(LTreeItem *Parent, const char *Fmt, ...);
#endif

typedef LRichTextPriv::BlockCursor BlkCursor;
typedef LAutoPtr<LRichTextPriv::BlockCursor> AutoCursor;
typedef LAutoPtr<LRichTextPriv::Transaction> AutoTrans;

#endif
