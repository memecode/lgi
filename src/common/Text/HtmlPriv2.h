#ifndef _GHTML2_PRIV_H_
#define _GHTML2_PRIV_H_

#include "lgi/common/Css.h"
#include "lgi/common/Token.h"

namespace Html2
{

//////////////////////////////////////////////////////////////////////////////////
// Enums                                                                        //
//////////////////////////////////////////////////////////////////////////////////
enum TagInfoFlags
{
	TI_NONE			= 0x00,
	TI_NEVER_CLOSES	= 0x01,
	TI_NO_TEXT		= 0x02,
	TI_BLOCK		= 0x04,
	TI_TABLE		= 0x08,
};

//////////////////////////////////////////////////////////////////////////////////
// Structs & Classes                                                            //
//////////////////////////////////////////////////////////////////////////////////
class LFlowRect;
class LFlowRegion;

struct LTagHit
{
	LTag *Hit; // Tag that was hit, or nearest tag, otherwise NULL
	int Near; // 0 if a direct hit, >0 is near miss, -1 if invalid.

	LFlowRect *Block; // Text block hit
	int Index; // If Block!=NULL then index into text, otherwise -1.

	LTagHit()
	{
		Hit = 0;
		Block = 0;
		Near = -1;
		Index = -1;
	}

	/// Return true if the current object is a closer than 'h'
	bool operator <(LTagHit &h)
	{
		// We didn't hit anything, so we can't be closer
		if (!Hit)
			return false;

		// We did hit something, and 'h' isn't an object yet
		// or 'h' is a block hit with no text ref... in which
		// case we are closer... because we have a text hit
		if (!h.Hit || (Near >= 0 && h.Near < 0))
			return true;

		LAssert(Near >= 0);

		if (Near < h.Near)
			return true;

		return false;
	}
};

struct GInfo
{
public:
	HtmlTag Id;
	const char *Tag;
	const char *ReattachTo;
	int Flags;

	bool NeverCloses()	{ return TestFlag(Flags, TI_NEVER_CLOSES); }
	bool NoText()		{ return TestFlag(Flags, TI_NO_TEXT); }
	bool Block()		{ return TestFlag(Flags, TI_BLOCK); }
};

class LHtmlLength
{
protected:
	float d;
	float PrevAbs;
	LCss::LengthType u;

public:
	LHtmlLength();
	LHtmlLength(char *s);

	bool IsValid();
	bool IsDynamic();
	float GetPrevAbs() { return PrevAbs; }
	operator float();
	LHtmlLength &operator =(float val);
	LCss::LengthType GetUnits();
	void Set(char *s);
	float Get(LFlowRegion *Flow, LFont *Font, bool Lock = false);
	float GetRaw() { return d; }
};

class LHtmlLine : public LHtmlLength
{
public:
	int LineStyle;
	int LineReset;
	LCss::ColorDef Colour;

	LHtmlLine();
	~LHtmlLine();

	LHtmlLine &operator =(int i);
	void Set(char *s);
};

class LFlowRect : public LRect
{
public:
	LTag *Tag;
	char16 *Text;
	int Len;

	LFlowRect()
	{
		Tag = 0;
		Text = 0;
		Len = 0;
	}

	~LFlowRect()
	{
	}

	int Start();
	bool OverlapX(int x) { return x >= x1 && x <= x2; }
	bool OverlapY(int y) { return y >= y1 && y <= y2; }
	bool OverlapX(LFlowRect *b) { return !(b->x2 < x1 || b->x1 > x2); }
	bool OverlapY(LFlowRect *b) { return !(b->y2 < y1 || b->y1 > y2); }
};

class LHtmlArea : public List<LFlowRect>
{
public:
	~LHtmlArea();

	LRect Bounds();
	LRect *TopRect(LRegion *c);
	void FlowText(LTag *Tag, LFlowRegion *c, LFont *Font, char16 *Text, LCss::LengthType Align);
};

class LCellStore
{
	typedef LArray<LTag*> CellArray;
	LArray<CellArray> c;

public:
	LCellStore(LTag *Table);

	void GetSize(int &x, int &y);
	void GetAll(List<LTag> &All);
	LTag *Get(int x, int y);
	bool Set(LTag *t);
	
	void Dump();
};

class LTag : public GDom, public LCss
{
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

protected:
	static bool Selected;
	friend class HtmlEdit;

	LHashTbl<ConstStrKey<char,false>, char*> Attr;

	// Forms
	LViewI *Ctrl;
	LVariant CtrlValue;
	HtmlControlType CtrlType;

	// Text
	LAutoWString Txt, PreTxt;

	// Debug stuff
	void _Dump(LStringPipe &Buf, int Depth);
	void _TraceOpenTags();

	// Private methods
	LFont *NewFont();
	int NearestChar(LFlowRect *Fr, int x, int y);
	LTag *HasOpenTag(char *t);
	LTag *PrevTag();
	LRect ChildBounds();
	bool GetWidthMetrics(uint16 &Min, uint16 &Max);
	void LayoutTable(LFlowRegion *f);
	void BoundParents();
	bool PeekTag(char *s, char *tag);
	LTag *GetTable();
	char *NextTag(char *s);
	void ZeroTableElements();
	bool OnUnhandledColor(LCss::ColorDef *def, const char *&s);
	
	COLOUR _Colour(bool Fore);
	COLOUR GetFore() { return _Colour(true); }
	COLOUR GetBack() { return _Colour(false); }

public:
	// Object
	HtmlTag TagId;
	char *Tag; // My tag

	LToken Class;
	const char *HtmlId;

	LAutoString Condition;
	GInfo *Info;
	int TipId;
	bool WasClosed;
	DisplayType Disp;

	// Heirarchy
	LHtml2 *Html;
	LTag *Parent;
	List<LTag> Tags;
	bool HasChild(LTag *c);
	bool Attach(LTag *Child, int Idx = -1);
	void Detach();
	LTag *GetBlockParent(int *Idx = 0);
	LFont *GetFont();

	// Style
	LPoint Pos;
	LPoint Size;
	LFont *Font;
	
	// Images
	LAutoPtr<LSurface> Image;
	void SetImage(const char *uri, LSurface *i);
	void LoadImage(const char *Uri); // Load just this URI
	void LoadImages(); // Recursive load all image URI's
	void ImageLoaded(char *uri, LSurface *img, int &Used);

	// Table stuff
	LPoint Cell;
	LPoint Span;
	uint16 MinContent, MaxContent;
	LCss::LengthType XAlign;
	LCellStore *Cells;
	#ifdef _DEBUG
	int Debug;
	#endif

	// Text
	int Cursor; // index into text of the cursor
	int Selection; // index into the text of the selection edge
	LHtmlArea TextPos;

	LTag(LHtml2 *h, LTag *p);
	~LTag();

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

	int GetTextStart();
	char *Dump();
	char16 *CleanText(const char *s, int len, bool ConversionAllowed = true, bool KeepWhiteSpace = false);
	char *ParseHtml(char *Doc, int Depth, bool InPreTag = false, bool *BackOut = 0);
	char *ParseText(char *Doc);
	
	/// Configures the tag's styles.
	void SetStyle();
	/// Called to apply CSS selectors on initialization and also when properties change at runtime.
	void Restyle();
	/// Match a simple CSS selector against the current object
	bool MatchSimpleSelector(LCss::Selector *Sel, int PartIdx);
	/// Match all the CSS selectors against the current object (calls MatchSimpleSelector one or more times)
	bool MatchFullSelector(LCss::Selector *Sel);
	
	/// Takes the CSS styles, parses and stores them in the current object,
	//// overwriting any duplicate properties.
	void SetCssStyle(const char *Style);
	/// Positions the tag according to the flow region passed in
	void OnFlow(LFlowRegion *Flow);
	/// Paints the border of the tag
	void OnPaintBorder(
		/// The surface to paint on
		LSurface *pDC,
		/// [Optional] The size of the border painted
		LRect *Px = NULL);
	void OnPaint(LSurface *pDC);
	void SetSize(LPoint &s);
	void SetTag(const char *Tag);
	void GetTagByPos(LTagHit &hit, int x, int y);
	LTag *GetTagByName(const char *Name);
	void CopyClipboard(LMemQueue &p);
	LTag *IsAnchor(LAutoString *Uri);
	bool CreateSource(LStringPipe &p, int Depth = 0, bool LastWasBlock = true);
	void Find(int TagType, LArray<LTag*> &Tags);
	LTag *GetAnchor(char *Name);

	// Control handling
	LTag *FindCtrlId(int Id);
	int OnNotify(int f);
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
	int AbsX();
	int AbsY();
	LRect GetRect(bool Client = true);
	LCss::LengthType GetAlign(bool x);

	// Tables
	LTag *GetTableCell(int x, int y);
	LPoint GetTableSize();
};

}

#endif