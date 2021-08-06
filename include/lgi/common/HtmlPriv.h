#ifndef _GHTML1_PRIV_H_
#define _GHTML1_PRIV_H_

#include "lgi/common/Css.h"
#include "lgi/common/Token.h"
#include "lgi/common/HtmlParser.h"

class HtmlEdit;

namespace Html1
{
#define DefaultTextColour			LColour(L_TEXT)

//////////////////////////////////////////////////////////////////////////////////
// Structs & Classes                                                            //
//////////////////////////////////////////////////////////////////////////////////
class GFlowRect;
class GFlowRegion;

#define ToTag(t)					dynamic_cast<GTag*>(t)

struct GTagHit
{
	GTag *Direct;		// Tag directly under cursor
	GTag *NearestText;	// Nearest tag with text
	int Near;			// How close in px was the position to NearestText.
						// 0 if a direct hit, >0 is near miss, -1 if invalid.
	bool NearSameRow;	// True if 'NearestText' on the same row as click.
	LPoint LocalCoords;	// The position in local co-ords of the tag

	GFlowRect *Block;	// Text block hit
	ssize_t Index; // If Block!=NULL then index into text, otherwise -1.

	GTagHit()
	{
		Direct = NULL;
		NearestText = NULL;
		NearSameRow = false;
		Block = 0;
		Near = -1;
		Index = -1;
		LocalCoords.x = LocalCoords.y = -1;
	}
	
	void Dump(const char *Desc);
};

class GLength
{
protected:
	float d;
	float PrevAbs;
	LCss::LengthType u;

public:
	GLength();
	GLength(char *s);

	bool IsValid();
	bool IsDynamic();
	float GetPrevAbs() { return PrevAbs; }
	operator float();
	GLength &operator =(float val);
	LCss::LengthType GetUnits();
	void Set(char *s);
	float Get(GFlowRegion *Flow, LFont *Font, bool Lock = false);
	float GetRaw() { return d; }
};

class GLine : public GLength
{
public:
	int LineStyle;
	int LineReset;
	LCss::ColorDef Colour;

	GLine();
	~GLine();

	GLine &operator =(int i);
	void Set(char *s);
};

class GFlowRect : public LRect
{
public:
	GTag *Tag;
	char16 *Text;
	ssize_t Len;

	GFlowRect()
	{
		Tag = 0;
		Text = 0;
		Len = 0;
	}

	~GFlowRect()
	{
	}

	int Start();
	bool OverlapX(int x) { return x >= x1 && x <= x2; }
	bool OverlapY(int y) { return y >= y1 && y <= y2; }
	bool OverlapX(GFlowRect *b) { return !(b->x2 < x1 || b->x1 > x2); }
	bool OverlapY(GFlowRect *b) { return !(b->y2 < y1 || b->y1 > y2); }
};

class GArea : public LArray<GFlowRect*>
{
public:
	~GArea();

	void Empty() { DeleteObjects(); }
	LRect Bounds();
	LRect *TopRect(LRegion *c);
	void FlowText(GTag *Tag, GFlowRegion *c, LFont *Font, int LineHeight, char16 *Text, LCss::LengthType Align);
};

struct GHtmlTableLayout
{
	typedef LArray<GTag*> CellArray;
	LArray<CellArray> c;
	GTag *Table;
	LPoint s;
	LCss::Len TableWidth;
	
	// Various pixels sizes
	int AvailableX;
	int CellSpacing;
	int BorderX1, BorderX2;
	LRect TableBorder, TablePadding; // in Px

	// The col and row sizes
	LArray<int> MinCol, MaxCol, MaxRow;
	LArray<LCss::Len> SizeCol;

	GHtmlTableLayout(GTag *table);

	void GetSize(int &x, int &y);
	void GetAll(List<GTag> &All);
	GTag *Get(int x, int y);
	bool Set(GTag *t);

	int GetTotalX(int StartCol = 0, int Cols = -1);
	void AllocatePx(int StartCol, int Cols, int MinPx, bool FillWidth);
	void DeallocatePx(int StartCol, int Cols, int MaxPx);
	void LayoutTable(GFlowRegion *f, uint16 Depth);
	
	void Dump();
};

class GTag : public LHtmlElement
{
	friend struct GHtmlTableLayout;
	friend class ::HtmlEdit;
	
public:
	enum HtmlControlType
	{
		CtrlNone,
		CtrlPassword,
		CtrlEmail,
		CtrlText,
		CtrlButton,
		CtrlSubmit,
		CtrlSelect,
		CtrlHidden,
	};

	class TextConvertState
	{
		LStream *Out;
		ssize_t PrevLineLen;
		LArray<char> Buf;
	
	public:
		int Depth;
		ssize_t CharsOnLine;
		
		TextConvertState(LStream *o)
		{
			Out = o;
			Depth = 0;
			CharsOnLine = 0;
			PrevLineLen = 0;
		}
		
		~TextConvertState()
		{
			if (CharsOnLine)
				NewLine();
		}
		
		ssize_t _Write(const void *Ptr, ssize_t Bytes)
		{
			// Check if we have enough space to store the string..
			size_t Total = CharsOnLine + Bytes;
			if (Buf.Length() < Total)
			{
				// Extend the memory buffer
				if (!Buf.Length(Total + 32))
					return -1;
			}
			
			// Store the string into a line buffer
			memcpy(&Buf[CharsOnLine], Ptr, Bytes);
			CharsOnLine += Bytes;

			return Bytes;
		}
		
		ssize_t Write(const void *Ptr, ssize_t Bytes)
		{
			char *start = (char*) Ptr, *cur;
			char *end = start + Bytes;
			const char *eol = "\r\n";
			while (start < end)
			{
				for (cur = start; *cur && cur < end && !strchr(eol, *cur); cur++)
					;
				
				if (!*cur || cur >= end)
					break;
				
				_Write(start, (int) (cur - start));
				
				start = cur;
				while (*start && start < end && strchr(eol, *start))
					start++;
			}
			
			return _Write(start, (int) (end - start));
		}
		
		ssize_t GetPrev()
		{
			return PrevLineLen;
		}
		
		void NewLine()
		{
			bool Valid = false;
			const uint8_t Ws[] = {' ', '\t', 0xa0, 0};
			
			GUtf8Ptr p(&Buf[0]);
			uint8_t *End = (uint8_t*) &Buf[CharsOnLine];
			while (p.GetPtr() < End)
			{
				if (!strchr((char*)Ws, p))
				{
					Valid = true;
					break;
				}
				p++;
			}
			if (!Valid)
				CharsOnLine = 0;

			Buf[CharsOnLine] = 0;
			if (CharsOnLine || PrevLineLen)
			{
				Out->Write(&Buf[0], CharsOnLine);
				Out->Write("\n", 1);
				PrevLineLen = CharsOnLine;
				CharsOnLine = 0;
			}
		}
	};

protected:
	/// A hash table of attributes.
	///
	/// All strings stored in here should be in UTF-8. Each string is allocated on the heap.
	LHashTbl<ConstStrKey<char,false>, char*> Attr;

	// Forms
	LViewI *Ctrl;
	LVariant CtrlValue;
	HtmlControlType CtrlType;

	// Text
	LAutoWString PreTxt;

	// Debug stuff
	void _Dump(LStringPipe &Buf, int Depth);
	void _TraceOpenTags();

	// Private methods
	LFont *NewFont();
	ssize_t NearestChar(GFlowRect *Fr, int x, int y);
	GTag *HasOpenTag(char *t);
	GTag *PrevTag();
	LRect ChildBounds();
	bool GetWidthMetrics(GTag *Table, uint16 &Min, uint16 &Max);
	void LayoutTable(GFlowRegion *f, uint16 Depth);
	void BoundParents();
	bool PeekTag(char *s, char *tag);
	GTag *GetTable();
	char *NextTag(char *s);
	void ZeroTableElements();
	bool OnUnhandledColor(LCss::ColorDef *def, const char *&s);
	void CenterText();
	bool Serialize(LXmlTag *t, bool Write);
	LColour _Colour(bool Fore);

public:
	// Object
	LString::Array Class;
	const char *HtmlId;

	LAutoString Condition;
	int TipId;

	// Heirarchy
	GHtml *Html;
	bool IsBlock() { return Display() == LCss::DispBlock; }
	GTag *GetBlockParent(ssize_t *Idx = 0);
	LFont *GetFont();

	// Style
	LPoint Pos;
	LPoint Size;
	LFont *Font;
	int LineHeightCache;
	LRect PadPx;
	
	// Images
	bool ImageResized;
	LAutoPtr<LSurface> Image;
	void SetImage(const char *uri, LSurface *i);
	void LoadImage(const char *Uri); // Load just this URI
	void LoadImages(); // Recursive load all image URI's
	void ImageLoaded(char *uri, LSurface *img, int &Used);
	void ClearToolTips();

	// Table stuff
	struct TblCell
	{
		LPoint Pos;
		LPoint Span;
		LRect BorderPx;
		LRect PaddingPx;
		uint16 MinContent, MaxContent;
		LCss::LengthType XAlign;
		GHtmlTableLayout *Cells;
		
		TblCell()
		{
			Cells = NULL;
			MinContent = 0;
			MaxContent = 0;
			XAlign = LCss::LenInherit;
			BorderPx.ZOff(0, 0);
			PaddingPx.ZOff(0, 0);
		}
		
		~TblCell()
		{
			DeleteObj(Cells);
		}
		
	}	*Cell;

	#ifdef _DEBUG
	int Debug;
	#endif

	// Text
	ssize_t Cursor; // index into text of the cursor
	ssize_t Selection; // index into the text of the selection edge
	GArea TextPos;

	GTag(GHtml *h, LHtmlElement *p);
	~GTag();

	// Events
	void OnChange(PropType Prop);
	bool OnClick();

	// Attributes
	bool Get(const char *attr, const char *&val) { val = Attr.Find(attr); return val != 0; }
	void Set(const char *attr, const char *val);

	// Methods
	char16 *Text() { return Txt; }
	void Text(char16 *t) { Txt.Reset(t); TextPos.Empty(); }
	char16 *PreText() { return PreTxt; }
	void PreText(char16 *t) { PreTxt.Reset(t); TextPos.Empty(); }

	ssize_t GetTextStart();
	LAutoWString DumpW();
	LAutoString DescribeElement();
	char16 *CleanText(const char *s, ssize_t len, const char *SourceCs, bool ConversionAllowed = true, bool KeepWhiteSpace = false);
	char *ParseText(char *Doc);
	bool ConvertToText(TextConvertState &State);
	
	/// Configures the tag's styles.
	void SetStyle();
	/// Called to apply CSS selectors on initialization and also when properties change at runtime.
	void Restyle();
	/// Recursively call restyle on all nodes in the doc tree
	void RestyleAll();
	
	/// Takes the CSS styles, parses and stores them in the current object,
	//// overwriting any duplicate properties.
	void SetCssStyle(const char *Style);
	/// Event received by scripts change CSS properties.
	void OnStyleChange(const char *name);
	/// Positions the tag according to the flow region passed in
	void OnFlow(GFlowRegion *Flow, uint16 Depth);
	/// Paints the border and background of the tag
	void PaintBorderAndBackground(
		/// The surface to paint on
		LSurface *pDC,
		/// The background colour (transparent is OK)
		LColour &Back,
		/// [Optional] The size of the border painted
		LRect *Px = NULL);
	/// This fills 'rgn' with all the rectangles making up the inline tags region
	void GetInlineRegion(LRegion &rgn, int ox = 0, int oy = 0);
	void OnPaint(LSurface *pDC, bool &InSelection, uint16 Depth);
	void SetSize(LPoint &s);
	void SetTag(const char *Tag);
	void GetTagByPos(GTagHit &TagHit, int x, int y, int Depth, bool InBody, bool DebugLog = false);
	GTag *GetTagByName(const char *Name);
	void CopyClipboard(GMemQueue &p, bool &InSelection);
	GTag *IsAnchor(LString *Uri);
	bool CreateSource(LStringPipe &p, int Depth = 0, bool LastWasBlock = true);
	void Find(int TagType, LArray<GTag*> &Tags);
	GTag *GetAnchor(char *Name);

	// Control handling
	GTag *FindCtrlId(int Id);
	int OnNotify(LNotification &n);
	void CollectFormValues(LHashTbl<ConstStrKey<char,false>,char*> &f);

	// GDom impl
	bool GetVariant(const char *Name, LVariant &Value, char *Array = 0);
	bool SetVariant(const char *Name, LVariant &Value, char *Array = 0);

	// Window
	bool OnMouseClick(LMouse &m);
	void Invalidate();

	// Positioning
	int RelX() { return Pos.x + (int)MarginLeft().Value; }
	int RelY() { return Pos.y + (int)MarginTop().Value; }
	LPoint AbsolutePos();
	inline int AbsX() { return AbsolutePos().x; }
	inline int AbsY() { return AbsolutePos().y; }
	LRect GetRect(bool Client = true);
	LCss::LengthType GetAlign(bool x);

	// Tables
	GTag *GetTableCell(int x, int y);
	LPoint GetTableSize();
	void ResetCaches();
};

}

#endif
