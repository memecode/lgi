#ifndef _DOC_EDIT_H_
#define _DOC_EDIT_H_

#include "lgi/common/TextView3.h"
#include "IdeDoc.h"

#define IDM_FILE_COMMENT			100
#define IDM_FUNC_COMMENT			101

enum SourceType
{
	SrcUnknown,
	SrcPlainText,
	SrcCpp,
	SrcPython,
	SrcXml,
	SrcHtml,
};

class DocEdit;
class DocEditStyling : public LThread, public LMutex
{
public:
	enum DocType
	{
		CodeHtml,
		CodePhp,
		CodeCss,
		CodeComment,
		CodePre,
		CodeJavascript
	};

protected:
	SourceType FileType;
	DocEdit *View;

	enum WordType
	{
		KNone,
		KLang,
		KType
	};

	enum ThreadState
	{
		KWaiting,
		KStyling,
		KCancel,
		KExiting,
	};

	struct Node
	{
		const int Captials = 26;
		const int Numbers = Captials + 26;
		const int Symbols = Numbers + 10;
		Node *Next[26 + 26 + 10 + 1];
		WordType Type;

		int Map(char16 c)
		{
			if (c >= 'a' && c <= 'z')
				return c - 'a';
			if (c >= 'A' && c <= 'Z')
				return c - 'A' + Captials;
			if (IsDigit(c))
				return c - '0' + Numbers;
			if (c == '_')
				return Symbols;
			// LAssert(0);
			return -1;
		}

		Node()
		{
			ZeroObj(Next);
			Type = KNone;
		}

		~Node()
		{
			for (int i=0; i<CountOf(Next); i++)
				DeleteObj(Next[i]);
		}
	};

	struct StylingParams
	{
		LTextView3 *View;
		size_t PourStart;
		ssize_t PourSize;
		LArray<char16> Text;
		LString FileName;
		LUnrolledList<LTextView3::LStyle> Styles;
		LTextView3::LStyle Visible;
		LTextView3::LStyle Dirty;

		StylingParams(LTextView3 *view) :
			Dirty(LTextView3::STYLE_NONE)
		{
			View = view;
		}

		void StyleString(char16 *&s, char16 *e, LColour c, LUnrolledList<LTextView3::LStyle> *Out = NULL)
		{
			if (!Out)
				Out = &Styles;
			auto &st = Out->New().Construct(View, LTextView3::STYLE_IDE);
			auto pText = Text.AddressOf();
			st.Start = s - pText;
			st.Font = View->GetFont();

			char16 Delim = *s++;
			while (s < e && *s != Delim)
			{
				if (*s == '\\')
					s++;
				s++;
			}

			st.Len = (s - pText) - st.Start + 1;
			st.Fore = c;
		}
	};

	// Styling data...
	LThreadEvent Event;
	ThreadState ParentState, WorkerState;
	// Lock before access:
		StylingParams Params;
	// EndLock
	// Thread only
		Node Root;
		LUnrolledList<LTextView3::LStyle> PrevStyle;
	// End Thread only

	// Styling functions..
	int Main();
	void StyleCpp(StylingParams &p);
	void StylePython(StylingParams &p);
	void StyleDefault(StylingParams &p);
	void StyleXml(StylingParams &p);
	void StyleHtml(StylingParams &p);
	void AddKeywords(const char **keys, bool IsType);

	// Full refresh triggers
	int RefreshSize;
	const char **RefreshEdges;

public:
	DocEditStyling(DocEdit *view);
	LColour ColourFromType(DocType t);
};

class DocEdit :
	public LTextView3,
	public LDocumentEnv,
	public DocEditStyling
{
	IdeDoc *Doc;
	int CurLine;
	LPoint MsClick;

	void OnApplyStyles();
	int CountRefreshEdges(size_t At, ssize_t Len);

public:
	static int LeftMarginPx;
	DocEdit(IdeDoc *d, LFontType *f);
	~DocEdit();

	const char *GetClass() override { return "DocEdit"; }
	const char *Name() override { return LTextView3::Name(); }
	bool Name(const char *s) override { return LTextView3::Name(s); }
	bool SetPourEnabled(bool b);
	int GetTopPaddingPx();
	void InvalidateLine(int Idx);
	char *TemplateMerge(const char *Template, const char *Name, List<char> *Params);
	bool GetVisible(LStyle &s);
	void OnCreate() override;

	// Overrides
	bool AppendItems(LSubMenu *Menu, const char *Param, int Base) override;
	bool DoGoto() override;
	void OnPaintLeftMargin(LSurface *pDC, LRect &r, LColour &colour) override;
	void OnMouseClick(LMouse &m) override;
	bool OnKey(LKey &k) override;	
	bool OnMenu(LDocView *View, int Id, void *Context) override;
	LMessage::Result OnEvent(LMessage *m) override;
	void SetCaret(size_t i, bool Select, bool ForceFullUpdate = false) override;
	void PourStyle(size_t Start, ssize_t EditSize) override;
	bool Pour(LRegion &r) override;
	bool Insert(size_t At, const char16 *Data, ssize_t Len) override;
	bool Delete(size_t At, ssize_t Len) override;
};

#endif
