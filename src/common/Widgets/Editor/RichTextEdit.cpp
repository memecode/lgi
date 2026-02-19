#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"
#include "lgi/common/Input.h"
#include "lgi/common/ScrollBar.h"
#ifdef WIN32
#include <imm.h>
#endif
#include "lgi/common/ClipBoard.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/FontCache.h"
#include "lgi/common/Unicode.h"
#include "lgi/common/DropFiles.h"
#include "lgi/common/HtmlCommon.h"
#include "lgi/common/HtmlParser.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/Menu.h"
#include "lgi/common/Homoglyphs.h"

// If this is not found add $lgi/private/common to your include paths
#include "ViewPriv.h"

#define DefaultCharset              "utf-8"

#define GDCF_UTF8					-1
#define POUR_DEBUG					0
#define PROFILE_POUR				0

#define ALLOC_BLOCK					64
#define IDC_VS						1000

#define PAINT_BORDER				Back
#define PAINT_AFTER_LINE			Back

#if !defined(WIN32) && !defined(toupper)
#define toupper(c)					(((c)>='a'&&(c)<='z') ? (c)-'a'+'A' : (c))
#endif

// static char SelectWordDelim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

#include "RichTextEditPriv.h"

//////////////////////////////////////////////////////////////////////
LRichTextEdit::LRichTextEdit(	int Id,
								int x, int y, int cx, int cy,
								LFontType *FontType)
	: ResObject(Res_Custom)
{
	// init vars
	LView::d->Css.Reset(new LRichTextPriv(this, &d));

	// setup window
	SetId(Id);
	SetTabStop(true);

	// default options
	#if WINNATIVE
	CrLf = true;
	SetDlgCode(DLGC_WANTALLKEYS);
	#else
	CrLf = false;
	#endif
	d->Padding(LCss::Len(LCss::LenPx, 4));
	
	#if 0
	d->BackgroundColor(LCss::ColorDef(LColour::Green));
	#else
	d->BackgroundColor(LColour(L_WORKSPACE));
	#endif
	
	SetFont(LSysFont);

	#if 0 // def _DEBUG
	Name("<html>\n"
		"<body>\n"
		"	This is some <b style='font-size: 20pt; color: green;'>bold text</b> to test with.<br>\n"
		"   A second line of text for testing.\n"
		"</body>\n"
		"</html>\n");
	#endif
}

LRichTextEdit::~LRichTextEdit()
{
	// 'd' is owned by the LView CSS autoptr.
}

bool LRichTextEdit::SetSpellCheck(LSpellCheck *sp)
{
	if ((d->SpellCheck = sp))
	{
		if (IsAttached())
			d->SpellCheck->EnumLanguages(AddDispatch());
		// else call that OnCreate
		
	}

	return d->SpellCheck != NULL;
}

bool LRichTextEdit::IsDirty()
{
	return d->Dirty;
}

void LRichTextEdit::IsDirty(bool dirty)
{
	if (d->Dirty ^ dirty)
	{
		d->Dirty = dirty;
	}
}

void LRichTextEdit::SetFixedWidthFont(bool i)
{
	if (FixedWidthFont ^ i)
	{
		if (i)
		{
			LFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				LDocView::SetFixedWidthFont(i);
			}
		}

		OnFontChange();
		Invalidate();
	}
}

void LRichTextEdit::SetReadOnly(bool i)
{
	LDocView::SetReadOnly(i);

	#if WINNATIVE
	SetDlgCode(i ? DLGC_WANTARROWS : DLGC_WANTALLKEYS);
	#endif
}

LRect LRichTextEdit::GetArea(RectType Type)
{
	return	Type >= ContentArea &&
			Type <= MaxArea
			?
			d->Areas[Type]
			:
			LRect(0, 0, -1, -1);
}

bool LRichTextEdit::ShowStyleTools()
{
	return d->ShowTools;
}

void LRichTextEdit::ShowStyleTools(bool b)
{
	if (d->ShowTools ^ b)
	{
		d->ShowTools = b;
		Invalidate();
	}
}

void LRichTextEdit::SetTabSize(uint8_t i)
{
	TabSize = limit(i, 2, 32);
	OnFontChange();
	OnPosChange();
	Invalidate();
}

void LRichTextEdit::SetWrapType(LDocWrapType i)
{
	LDocView::SetWrapType(i);

	OnPosChange();
	Invalidate();
}

LFont *LRichTextEdit::GetUiFont()
{
	return LSysFont;
}

void LRichTextEdit::SetUiFont(LFont *f, bool OwnId)
{
}

LFont *LRichTextEdit::GetFont()
{
	return d->EditFont;
}

void LRichTextEdit::SetFont(LFont *f, bool OwnIt)
{
	if (!f)
		return;
	
	if (OwnIt)
	{
		d->EditFont.Reset(f);
	}
	else if (d->EditFont.Reset(new LFont))
	{		
		*d->EditFont = *f;
		d->EditFont->Create(NULL, 0, 0);
	}
	
	OnFontChange();
	d->UpdateStyleUI();
}

void LRichTextEdit::OnFontChange()
{
	// LgiTrace("%s:%i - LRichTextEdit::OnFontChange not impl.\n", _FL);
}

void LRichTextEdit::PourText(ssize_t Start, ssize_t Length /* == 0 means it's a delete */)
{
	LAssert(!"Not impl.");
}

void LRichTextEdit::PourStyle(ssize_t Start, ssize_t EditSize)
{
	LAssert(!"Not impl.");
}

bool LRichTextEdit::Insert(size_t At, const char16 *Data, ssize_t Len)
{
	if (!d->Cursor || !d->Cursor->Blk)
	{
		LAssert(!"No cursor block.");
		return false;
	}

	auto b = d->Cursor->Blk;
	AutoTrans Trans(new LRichTextPriv::Transaction);						

	#ifdef WINDOWS
	// UCS2 -> UTF32
	LAutoPtr<uint32_t,true> utf32((uint32_t*)LNewConvertCp("utf-32", Data, LGI_WideCharset, Len*sizeof(*Data)));
	uint32_t *u32 = utf32.Get();
	auto len = Strlen(u32);
	#else
	LAssert(sizeof(char16) == sizeof(uint32_t));
	uint32_t *u32 = (uint32_t*)Data;
	auto len = Len;
	#endif

	if (!b->AddText(Trans, d->Cursor->Offset, u32, len, NULL))
	{
		LAssert(!"AddText failed.");
		return false;
	}

	d->Cursor->Set(d->Cursor->Offset + 1);
	Invalidate();
	SendNotify(LNotifyDocChanged);

	d->AddTrans(Trans);

	return true;
}

bool LRichTextEdit::Delete(size_t At, ssize_t Len)
{
	LAssert(!"Not impl.");
	return false;
}

bool LRichTextEdit::DeleteSelection(char16 **Cut)
{
	AutoTrans t(new LRichTextPriv::Transaction);
	if (!d->DeleteSelection(t, Cut))
		return false;

	return d->AddTrans(t);
}

int64 LRichTextEdit::Value()
{
	const char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void LRichTextEdit::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LPrintfInt64, i);
	Name(Str);
}

bool LRichTextEdit::GetFormattedContent(const char *MimeType, LString &Out, LArray<ContentMedia> *Media)
{
	if (!MimeType || _stricmp(MimeType, "text/html"))
		return false;

	if (!d->ToHtml(Media))
		return false;
	
	Out = d->UtfNameCache;
	return true;
}

const char *LRichTextEdit::Name()
{
	d->ToHtml();
	return d->UtfNameCache;
}

const char *LRichTextEdit::GetCharset()
{
	return d->Charset;
}

void LRichTextEdit::SetCharset(const char *s)
{
	d->Charset = s;
}

bool LRichTextEdit::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case HtmlImagesLinkCid:
		{
			Value = d->HtmlLinkAsCid;
			break;
		}
		case SpellCheckLanguage:
		{
			Value = d->SpellLang.Get();
			break;
		}
		case SpellCheckDictionary:
		{
			Value = d->SpellDict.Get();
			break;
		}
		default:
			return false;
	}
	
	return true;
}

bool LRichTextEdit::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case HtmlImagesLinkCid:
		{
			d->HtmlLinkAsCid = Value.CastInt32() != 0;
			break;
		}
		case SpellCheckLanguage:
		{
			d->SpellLang = Value.Str();
			break;
		}
		case SpellCheckDictionary:
		{
			d->SpellDict = Value.Str();
			break;
		}
		default:
			return false;
	}
	
	return true;
}

static LHtmlElement *FindElement(LHtmlElement *e, HtmlTag TagId)
{
	if (e->TagId == TagId)
		return e;
		
	for (unsigned i = 0; i < e->Children.Length(); i++)
	{
		LHtmlElement *c = FindElement(e->Children[i], TagId);
		if (c)
			return c;
	}
	
	return NULL;
}

void LRichTextEdit::OnAddStyle(const char *MimeType, const char *Styles)
{
	if (d->CreationCtx)
	{
		d->CreationCtx->StyleStore.Parse(Styles);
	}
}

bool LRichTextEdit::Name(const char *s)
{
	d->Empty();
	d->OriginalText = s;
	
	LHtmlElement Root(NULL);

	if (!d->CreationCtx.Reset(new LRichTextPriv::CreateContext(d)))
		return false;

	if (!d->LHtmlParser::Parse(&Root, s))
		return d->Error(_FL, "Failed to parse HTML.");
	
	auto Body = FindElement(&Root, TAG_BODY);
	if (!Body)
		Body = &Root;

	bool Status = d->FromHtml(Body, *d->CreationCtx);
	
	// d->DumpBlocks();
	
	if (!d->Blocks.Length())
	{
		d->EmptyDoc();
	}
	else
	{
		// Clear out any zero length blocks.
		for (unsigned i=0; i<d->Blocks.Length(); i++)
		{
			auto b = d->Blocks[i];
			if (b->Length() == 0)
			{
				d->Blocks.DeleteAt(i--, true);
				DeleteObj(b);
			}
		}
	}
	
	if (Status)
		SetCursor(0, false);
	
	Invalidate();
	
	return Status;
}

const char16 *LRichTextEdit::NameW()
{
	d->WideNameCache.Reset(Utf8ToWide(Name()));
	return d->WideNameCache;
}

bool LRichTextEdit::NameW(const char16 *s)
{
	LAutoString a(WideToUtf8(s));
	return Name(a);
}

char *LRichTextEdit::GetSelection()
{
	if (!HasSelection())
		return NULL;

	LArray<char16> Text;
	if (!d->GetSelection(&Text, NULL))
		return NULL;
	
	return WideToUtf8(&Text[0]);
}

bool LRichTextEdit::HasSelection()
{
	return d->Selection.Get() != NULL;
}

void LRichTextEdit::SelectAll()
{
	AutoCursor Start(new BlkCursor(d->Blocks.First(), 0, 0));
	d->SetCursor(Start);

	LRichTextPriv::Block *Last = d->Blocks.Length() ? d->Blocks.Last() : NULL;
	if (Last)
	{
		AutoCursor End(new BlkCursor(Last, Last->Length(), Last->GetLines()-1));
		d->SetCursor(End, true);
	}
	else d->Selection.Reset();

	Invalidate();
}

void LRichTextEdit::UnSelectAll()
{
	bool Update = HasSelection();

	if (Update)
	{
		d->Selection.Reset();
		Invalidate();
	}
}

void LRichTextEdit::SetStylePrefix(LString s)
{
	d->SetPrefix(s);
}

bool LRichTextEdit::IsBusy(bool Stop)
{
	return d->IsBusy(Stop);
}

size_t LRichTextEdit::GetLines()
{
	uint32_t Count = 0;
	for (size_t i=0; i<d->Blocks.Length(); i++)
	{
		LRichTextPriv::Block *b = d->Blocks[i];
		Count += b->GetLines();
	}
	return Count;
}

int LRichTextEdit::GetLine()
{
	if (!d->Cursor)
		return -1;

	ssize_t Idx = d->Blocks.IndexOf(d->Cursor->Blk);
	if (Idx < 0)
	{
		LAssert(0);
		return -1;
	}

	int Count = 0;
	
	// Count lines in blocks before the cursor...
	for (int i=0; i<Idx; i++)
	{
		LRichTextPriv::Block *b = d->Blocks[i];
		Count += b->GetLines();
	}

	// Add the lines in the cursor's block...
	if (d->Cursor->LineHint)
	{
		Count += d->Cursor->LineHint;
	}
	else
	{
		LArray<int> BlockLine;
		if (d->Cursor->Blk->OffsetToLine(d->Cursor->Offset, NULL, &BlockLine))
			Count += BlockLine.First();
		else
		{
			// Hmmm...
			LAssert(!"Can't find block line.");
			return -1;
		}
	}


	return Count;
}

void LRichTextEdit::SetLine(int Line)
{
	int Count = 0;
	
	// Count lines in blocks before the cursor...
	for (int i=0; i<(int)d->Blocks.Length(); i++)
	{
		LRichTextPriv::Block *b = d->Blocks[i];
		int Lines = b->GetLines();
		if (Line >= Count && Line < Count + Lines)
		{
			auto BlockLine = Line - Count;
			auto Offset = b->LineToOffset(BlockLine);
			if (Offset >= 0)
			{
				AutoCursor c(new BlkCursor(b, Offset, BlockLine));
				d->SetCursor(c);
				break;
			}
		}		
		Count += Lines;
	}
}

void LRichTextEdit::GetTextExtent(int &x, int &y)
{
	x = d->DocumentExtent.x;
	y = d->DocumentExtent.y;
}

bool LRichTextEdit::GetLineColumnAtIndex(LPoint &Pt, ssize_t Index)
{
	ssize_t Offset = -1;
	int BlockLines = -1;
	LRichTextPriv::Block *b = d->GetBlockByIndex(Index, &Offset, NULL, &BlockLines);
	if (!b)
		return false;

	int Cols;
	LArray<int> Lines;
	if (b->OffsetToLine(Offset, &Cols, &Lines))
		return false;
	
	Pt.x = Cols;
	Pt.y = BlockLines + Lines.First();
	return true;
}

ssize_t LRichTextEdit::GetCaret(bool Cur)
{
	if (!d->Cursor)
		return -1;
		
	ssize_t CharPos = 0;
	for (ssize_t i=0; i<(ssize_t)d->Blocks.Length(); i++)
	{
		LRichTextPriv::Block *b = d->Blocks[i];
		if (d->Cursor->Blk == b)
			return CharPos + d->Cursor->Offset;
		CharPos += b->Length();
	}
	
	LAssert(!"Cursor block not found.");
	return -1;
}

bool LRichTextEdit::IndexAt(int x, int y, ssize_t &Off, int &LineHint)
{
	LPoint Doc = d->ScreenToDoc(x, y);
	Off = d->HitTest(Doc.x, Doc.y, LineHint);
	return Off >= 0;
}

ssize_t LRichTextEdit::IndexAt(int x, int y)
{
	ssize_t Idx;
	int Line;
	if (!IndexAt(x, y, Idx, Line))
		return -1;
	return Idx;
}

void LRichTextEdit::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
	ssize_t Offset = -1;
	LRichTextPriv::Block *Blk = d->GetBlockByIndex(i, &Offset);
	if (Blk)
	{
		AutoCursor c(new BlkCursor(Blk, Offset, -1));
		if (c)
			d->SetCursor(c, Select);
	}
}

bool LRichTextEdit::Cut()
{
	if (!HasSelection())
		return false;

	char16 *Txt = NULL;
	if (!DeleteSelection(&Txt))
		return false;

	bool Status = true;
	if (Txt)
	{
		LClipBoard Cb(this);
		Status = Cb.TextW(Txt);
		DeleteArray(Txt);
	}

	SendNotify(LNotifyDocChanged);

	return Status;
}

bool LRichTextEdit::Copy()
{
	if (!HasSelection())
		return false;

	LArray<char16> PlainText;
	LAutoString Html;
	if (!d->GetSelection(&PlainText, &Html))
		return false;

	LString PlainUtf8 = PlainText.AddressOf();
	if (HasHomoglyphs(PlainUtf8, PlainUtf8.Length()))
	{
		if (LgiMsg(	this,
					LLoadString(L_TEXTCTRL_HOMOGLYPH_WARNING,
								"Text contains homoglyph characters that maybe a phishing attack.\n"
								"Do you really want to copy it?"),
					LLoadString(L_TEXTCTRL_WARNING, "Warning"),
					MB_YESNO) == IDNO)
			return false;
	}

	// Put on the clipboard
	LClipBoard Cb(this);
	bool Status = Cb.TextW(PlainText.AddressOf());
	Cb.Html(Html, false);
	return Status;
}

bool LRichTextEdit::Paste()
{
	LClipBoard Cb(this);
		
	Cb.Bitmap([this](auto bmp, auto str)
	{
		LString Html;
		LAutoWString Text;
		LAutoPtr<LSurface> Img;

		Img = bmp;
		if (!Img)
		{
			LClipBoard Cb(this);
			Html = Cb.Html();
			if (!Html)
				Text.Reset(NewStrW(Cb.TextW()));
		}

		if (!Html && !Text && !Img)
			return false;
	
		if (!d->Cursor ||
			!d->Cursor->Blk)
		{
			LAssert(0);
			return false;
		}

		AutoTrans Trans(new LRichTextPriv::Transaction);						

		if (HasSelection())
		{
			if (!d->DeleteSelection(Trans, NULL))
				return false;
		}

		if (Html)
		{
			LHtmlElement Root(NULL);

			if (!d->CreationCtx.Reset(new LRichTextPriv::CreateContext(d)))
				return false;

			if (!d->LHtmlParser::Parse(&Root, Html))
				return d->Error(_FL, "Failed to parse HTML.");
	
			LHtmlElement *Body = FindElement(&Root, TAG_BODY);
			if (!Body)
				Body = &Root;

			if (d->Cursor)
			{
				auto *b = d->Cursor->Blk;
				ssize_t BlkIdx = d->Blocks.IndexOf(b);
				LRichTextPriv::Block *After = NULL;
				ssize_t AddIndex = BlkIdx;;

				// Split 'b' to make room for pasted objects
				if (d->Cursor->Offset > 0)
				{
					After = b->Split(Trans, d->Cursor->Offset);
					AddIndex = BlkIdx+1;									
				}
				// else Insert before cursor block

				auto *PastePoint = new LRichTextPriv::TextBlock(d);
				if (PastePoint)
				{
					d->Blocks.AddAt(AddIndex++, PastePoint);
					if (After) d->Blocks.AddAt(AddIndex++, After);

					d->CreationCtx->Tb = PastePoint;
					d->FromHtml(Body, *d->CreationCtx);
				}
			}
		}
		else if (Text)
		{
			LAutoPtr<uint32_t,true> Utf32((uint32_t*)LNewConvertCp("utf-32", Text, LGI_WideCharset));
			ptrdiff_t Len = Strlen(Utf32.Get());
			if (!d->Cursor->Blk->AddText(Trans, d->Cursor->Offset, Utf32.Get(), (int)Len))
			{
				LAssert(0);
				return false;
			}

			d->Cursor->Offset += Len;
			d->Cursor->LineHint = -1;
		}
		else if (Img)
		{
			auto b = d->Cursor->Blk;
			auto BlkIdx = d->Blocks.IndexOf(b);
			LRichTextPriv::Block *After = NULL;
			ssize_t AddIndex;
		
			LAssert(BlkIdx >= 0);

			// Split 'b' to make room for the image
			if (d->Cursor->Offset > 0)
			{
				After = b->Split(Trans, d->Cursor->Offset);
				AddIndex = BlkIdx + 1;									
			}
			else
			{
				// Insert before..
				AddIndex = BlkIdx;
			}

			if (auto ImgBlk = new LRichTextPriv::ImageBlock(d))
			{
				d->Blocks.AddAt(AddIndex++, ImgBlk);
				if (After)
					d->Blocks.AddAt(AddIndex++, After);

				Img->MakeOpaque();
				ImgBlk->SetImage(Img);
			
				AutoCursor c(new BlkCursor(ImgBlk, 1, -1));
				d->SetCursor(c);			
			}
		}

		Invalidate();
		SendNotify(LNotifyDocChanged);

		return d->AddTrans(Trans);
	});

	return true;
}

bool LRichTextEdit::ClearDirty(bool Ask, const char *FileName)
{
	if (1 /*dirty*/)
	{
		int Answer = (Ask) ? LgiMsg(this,
									LLoadString(L_TEXTCTRL_ASK_SAVE, "Do you want to save your changes to this document?"),
									LLoadString(L_TEXTCTRL_SAVE, "Save"),
									MB_YESNOCANCEL) : IDYES;
		if (Answer == IDYES)
		{
			if (FileName)
				Save(FileName);
			else				
			{
				LFileSelect *Select = new LFileSelect;
				Select->Parent(this);
				Select->Save([this](auto dlg, auto status)
				{
					if (status)
						Save(dlg->Name());
				});
			}
		}
		else if (Answer == IDCANCEL)
		{
			return false;
		}
	}

	return true;
}

bool LRichTextEdit::Open(const char *Name, const char *CharSet)
{
	bool Status = false;
	LFile f;

	if (f.Open(Name, O_READ|O_SHARE))
	{
		size_t Bytes = (size_t)f.GetSize();
		SetCursor(0, false);
		
		char *c8 = new char[Bytes + 4];
		if (c8)
		{
			if (f.Read(c8, (int)Bytes) == Bytes)
			{
				char *DataStart = c8;

				c8[Bytes] = 0;
				c8[Bytes+1] = 0;
				c8[Bytes+2] = 0;
				c8[Bytes+3] = 0;
				
				if ((uchar)c8[0] == 0xff && (uchar)c8[1] == 0xfe)
				{
					// utf-16
					if (!CharSet)
					{
						CharSet = "utf-16";
						DataStart += 2;
					}
				}
				
			}

			DeleteArray(c8);
		}
		else
		{
		}

		Invalidate();
	}

	return Status;
}

bool LRichTextEdit::Save(const char *FileName, const char *CharSet)
{
	LFile f;
	if (!FileName || !f.Open(FileName, O_WRITE))
		return false;

	f.SetSize(0);
	const char *Nm = Name();
	if (!Nm)
		return false;

	size_t Len = strlen(Nm);
	return f.Write(Nm, (int)Len) == Len;
}

void LRichTextEdit::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		//LRect Before = GetClient();

	}
}

void LRichTextEdit::DoCase(std::function<void(bool)> Callback, bool Upper)
{
	if (!HasSelection())
	{
		if (Callback)
			Callback(false);
		return;
	}

	bool Cf = d->CursorFirst();
	LRichTextPriv::BlockCursor *Start = Cf ? d->Cursor : d->Selection;
	LRichTextPriv::BlockCursor *End = Cf ? d->Selection : d->Cursor;
	if (Start->Blk == End->Blk)
	{
		// In the same block...
		ssize_t Len = End->Offset - Start->Offset;
		Start->Blk->DoCase(NoTransaction, Start->Offset, Len, Upper);
	}
	else
	{
		// Multi-block delete...

		// 1) Delete all the content to the end of the first block
		ssize_t StartLen = Start->Blk->Length();
		if (Start->Offset < StartLen)
			Start->Blk->DoCase(NoTransaction, Start->Offset, StartLen - Start->Offset, Upper);

		// 2) Delete any blocks between 'Start' and 'End'
		ssize_t i = d->Blocks.IndexOf(Start->Blk);
		if (i >= 0)
		{
			for (++i; d->Blocks[i] != End->Blk && i < (int)d->Blocks.Length(); )
			{
				LRichTextPriv::Block *b = d->Blocks[i];
				b->DoCase(NoTransaction, 0, -1, Upper);
			}
		}
		else
		{
			LAssert(0);
			if (Callback)
				Callback(false);
			return;
		}

		// 3) Delete any text up to the Cursor in the 'End' block
		End->Blk->DoCase(NoTransaction, 0, End->Offset, Upper);
	}

	// Update the screen
	d->Dirty = true;
	Invalidate();
	
	if (Callback)
		Callback(true);
}

void LRichTextEdit::DoGoto(std::function<void(bool)> Callback)
{
	auto input = new LInput(this, "", LLoadString(L_TEXTCTRL_GOTO_LINE, "Goto line:"), "Text");
	input->DoModal([this, input, Callback](auto dlg, auto ok)
	{
		if (ok)
		{
			auto i = input->GetStr().Int();
			if (i >= 0)
			{
				SetLine((int)i);
				if (Callback)
					Callback(true);
				return;
			}
		}

		if (Callback)
			Callback(false);
	});
}

LDocFindReplaceParams *LRichTextEdit::CreateFindReplaceParams()
{
	return new LDocFindReplaceParams3;
}

void LRichTextEdit::SetFindReplaceParams(LDocFindReplaceParams *Params)
{
	if (Params)
	{
	}
}

void LRichTextEdit::DoFindNext(std::function<void(bool)> Callback)
{
	if (Callback)
		Callback(false);
}

////////////////////////////////////////////////////////////////////////////////// FIND
void LRichTextEdit::DoFind(std::function<void(bool)> Callback)
{
	LArray<char16> Sel;
	if (HasSelection())
		d->GetSelection(&Sel, NULL);
	LAutoString u(Sel.Length() ? WideToUtf8(&Sel.First()) : NULL);
	
	auto Dlg = new LFindDlg(this,
							[this](auto dlg, auto ctrlId)
							{
								return OnFind(dlg);
							},
							u);
	Dlg->DoModal([this, Dlg, Callback](auto dlg, auto ctrlId)
	{
		if (Callback)
			Callback(ctrlId != IDCANCEL);
		
		Focus(true);
	});
}

bool LRichTextEdit::OnFind(LFindReplaceCommon *Params)
{
	if (!Params || !d->Cursor)
	{
		LAssert(0);
		return false;
	}
	
	LAutoPtr<uint32_t,true> w((uint32_t*)LNewConvertCp("utf-32", Params->Find, "utf-8", Params->Find.Length()));
	ssize_t Idx = d->Blocks.IndexOf(d->Cursor->Blk);
	if (Idx < 0)
	{
		LAssert(0);
		return false;
	}

	for (unsigned n = 0; n < d->Blocks.Length(); n++)
	{
		ssize_t i = Idx + n;
		LRichTextPriv::Block *b = d->Blocks[i % d->Blocks.Length()];
		ssize_t At = n ? 0 : d->Cursor->Offset;
		ssize_t Result = b->FindAt(At, w, Params);
		if (Result >= At)
		{
			ptrdiff_t Len = Strlen(w.Get());
			AutoCursor Sel(new BlkCursor(b, Result, -1));
			d->SetCursor(Sel, false);

			AutoCursor Cur(new BlkCursor(b, Result + Len, -1));
			return d->SetCursor(Cur, true);
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////// REPLACE
void LRichTextEdit::DoReplace(std::function<void(bool)> Callback)
{
	if (Callback)
		Callback(false);
}

bool LRichTextEdit::OnReplace(LFindReplaceCommon *Params)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////////////
void LRichTextEdit::SelectWord(size_t From)
{
	int BlockIdx;
	ssize_t Start, End;
	LRichTextPriv::Block *b = d->GetBlockByIndex(From, &Start, &BlockIdx);
	if (!b)
		return;

	LArray<uint32_t> Txt;
	if (!b->CopyAt(0, b->Length(), &Txt))
		return;

	End = Start;
	while (Start > 0 &&
			!IsWordBreakChar(Txt[Start-1]))
		Start--;
	
	while
	(
		End < b->Length()
		&&
		(
			End == Txt.Length()
			||
			!IsWordBreakChar(Txt[End])
		)
	)
		End++;

	AutoCursor c(new BlkCursor(b, Start, -1));
	d->SetCursor(c);
	c.Reset(new BlkCursor(b, End, -1));
	d->SetCursor(c, true);
}

bool LRichTextEdit::OnMultiLineTab(bool In)
{
	return false;
}

void LRichTextEdit::OnSetHidden(int Hidden)
{
}

void LRichTextEdit::OnPosChange()
{
	static bool Processing = false;

	if (!Processing)
	{
		Processing = true;
		LLayout::OnPosChange();

		// LRect c = GetClient();
		Processing = false;
	}
	
	LLayout::OnPosChange();
}

int LRichTextEdit::WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState)
{
	Formats.SupportsFileDrops();
	#ifdef WINDOWS
	Formats.Supports("UniformResourceLocatorW");
	#endif

	return Formats.Length() ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

int LRichTextEdit::OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState)
{
	int Effect = DROPEFFECT_NONE;

	for (unsigned i=0; i<Data.Length(); i++)
	{
		LDragData &dd = Data[i];
		if (dd.IsFileDrop() && d->Areas[ContentArea].Overlap(Pt.x, Pt.y))
		{
			int AddIndex = -1;
			LPoint TestPt(	Pt.x - d->Areas[ContentArea].x1,
							Pt.y - d->Areas[ContentArea].y1);
			if (VScroll)
				TestPt.y += (int)(VScroll->Value() * d->ScrollLinePx);

			LDropFiles Df(dd);
			for (unsigned n=0; n<Df.Length(); n++)
			{
				const char *f = Df[n];
				LString Mt = LGetFileMimeType(f);
				if (Mt && Mt.Find("image/") == 0)
				{
					if (AddIndex < 0)
					{
						int LineHint = -1;
						ssize_t Idx = d->HitTest(TestPt.x, TestPt.y, LineHint);
						if (Idx >= 0)
						{
							ssize_t BlkOffset;
							int BlkIdx;
							LRichTextPriv::Block *b = d->GetBlockByIndex(Idx, &BlkOffset, &BlkIdx);
							if (b)
							{
								LRichTextPriv::Block *After = NULL;

								// Split 'b' to make room for the image
								if (BlkOffset > 0)
								{
									After = b->Split(NoTransaction, BlkOffset);
									AddIndex = BlkIdx+1;									
								}
								else
								{
									// Insert before..
									AddIndex = BlkIdx;
								}

								if (auto ImgBlk = new LRichTextPriv::ImageBlock(d))
								{
									d->Blocks.AddAt(AddIndex++, ImgBlk);
									if (After)
										d->Blocks.AddAt(AddIndex++, After);

									ImgBlk->Load(f);
									Effect = DROPEFFECT_COPY;
								}
							}
						}
					}
				}
			}

			break;
		}
	}

	if (Effect != DROPEFFECT_NONE)
	{
		Invalidate();
		SendNotify(LNotifyDocChanged);
	}

	return Effect;
}

void LRichTextEdit::OnCreate()
{
	SetWindow(this);
	DropTarget(true);

	if (Focus())
		SetPulse(RTE_PULSE_RATE);

	if (d->SpellCheck)
		d->SpellCheck->EnumLanguages(AddDispatch());
}

void LRichTextEdit::OnEscape(LKey &K)
{
}

bool LRichTextEdit::OnMouseWheel(double l)
{
	if (VScroll)
	{
		VScroll->Value(VScroll->Value() + (int64)l);
		Invalidate();
	}
	
	return true;
}

void LRichTextEdit::OnFocus(bool f)
{
	Invalidate();
	SetPulse(f ? RTE_PULSE_RATE : -1);
}

ssize_t LRichTextEdit::HitTest(int x, int y)
{
	int Line = -1;
	return d->HitTest(x, y, Line);
}

void LRichTextEdit::Undo()
{
	if (d->UndoPos > 0)
		d->SetUndoPos(d->UndoPos - 1);
}

void LRichTextEdit::Redo()
{
	if (d->UndoPos < (int)d->UndoQue.Length())
		d->SetUndoPos(d->UndoPos + 1);
}

#ifdef _DEBUG
class NodeView : public LWindow
{
public:
	LTree *Tree;
	
	NodeView(LViewI *w)
	{
		LRect r(0, 0, 500, 600);
		SetPos(r);
		MoveSameScreen(w);
		Attach(0);

		if ((Tree = new LTree(100, 0, 0, 100, 100)))
		{
			Tree->SetPourLargest(true);
			Tree->Attach(this);
		}
	}
};
#endif

void LRichTextEdit::DoContextMenu(LMouse &m)
{
	LMenuItem *i;
	LSubMenu RClick;
	LAutoString ClipText;
	{
		LClipBoard Clip(this);
		ClipText.Reset(NewStr(Clip.Text()));
	}

	LRichTextPriv::Block *Over = NULL;
	LRect &Content = d->Areas[ContentArea];
	LPoint Doc = d->ScreenToDoc(m.x, m.y);
	ssize_t Offset = -1, BlkOffset = -1;
	if (Content.Overlap(m.x, m.y))
	{
		int LineHint;
		Offset = d->HitTest(Doc.x, Doc.y, LineHint, &Over, &BlkOffset);
	}
	if (Over)
		Over->DoContext(RClick, Doc, BlkOffset, true);

	RClick.AppendItem(LLoadString(L_TEXTCTRL_CUT, "Cut"), ID_RTE_CUT, HasSelection());
	RClick.AppendItem(LLoadString(L_TEXTCTRL_COPY, "Copy"), ID_RTE_COPY, HasSelection());
	RClick.AppendItem(LLoadString(L_TEXTCTRL_PASTE, "Paste"), ID_RTE_PASTE, ClipText != 0);
	RClick.AppendSeparator();

	RClick.AppendItem(LLoadString(L_TEXTCTRL_UNDO, "Undo"), ID_RTE_UNDO, false /* UndoQue.CanUndo() */);
	RClick.AppendItem(LLoadString(L_TEXTCTRL_REDO, "Redo"), ID_RTE_REDO, false /* UndoQue.CanRedo() */);
	RClick.AppendSeparator();

	#if 0
	i = RClick.AppendItem(LLoadString(L_TEXTCTRL_FIXED, "Fixed Width Font"), ID_FIXED, true);
	if (i) i->Checked(GetFixedWidthFont());
	#endif

	i = RClick.AppendItem(LLoadString(L_TEXTCTRL_AUTO_INDENT, "Auto Indent"), ID_AUTO_INDENT, true);
	if (i) i->Checked(AutoIndent);
	
	i = RClick.AppendItem(LLoadString(L_TEXTCTRL_SHOW_WHITESPACE, "Show Whitespace"), ID_SHOW_WHITE, true);
	if (i) i->Checked(ShowWhiteSpace);
	
	i = RClick.AppendItem(LLoadString(L_TEXTCTRL_HARD_TABS, "Hard Tabs"), ID_HARD_TABS, true);
	if (i) i->Checked(HardTabs);
	
	RClick.AppendItem(LLoadString(L_TEXTCTRL_INDENT_SIZE, "Indent Size"), ID_INDENT_SIZE, true);
	RClick.AppendItem(LLoadString(L_TEXTCTRL_TAB_SIZE, "Tab Size"), ID_TAB_SIZE, true);
	
	LSubMenu *Src = RClick.AppendSub("Source");
	if (Src)
	{
		Src->AppendItem("Copy Original", ID_COPY_ORIGINAL, d->OriginalText.Get() != NULL);
		Src->AppendItem("Copy Current", ID_COPY_CURRENT);
		#ifdef _DEBUG
		Src->AppendItem("Dump Nodes", ID_DUMP_NODES);
		// Edit->DumpNodes(Tree);
		#endif
	}

	if (Over)
	{
		#ifdef _DEBUG
		// RClick.AppendItem(Over->GetClass(), -1, false);
		#endif
		Over->DoContext(RClick, Doc, BlkOffset, false);
	}
	if (Environment)
		Environment->AppendItems(&RClick, NULL);

	int Id = 0;
	m.ToScreen();
	switch (Id = RClick.Float(this, m.x, m.y))
	{
		case ID_FIXED:
		{
			SetFixedWidthFont(!GetFixedWidthFont());							
			SendNotify(LNotifyFixedWidthChanged);
			break;
		}
		case ID_RTE_CUT:
		{
			Cut();
			break;
		}
		case ID_RTE_COPY:
		{
			Copy();
			break;
		}
		case ID_RTE_PASTE:
		{
			Paste();
			break;
		}
		case ID_RTE_UNDO:
		{
			Undo();
			break;
		}
		case ID_RTE_REDO:
		{
			Redo();
			break;
		}
		case ID_AUTO_INDENT:
		{
			AutoIndent = !AutoIndent;
			break;
		}
		case ID_SHOW_WHITE:
		{
			ShowWhiteSpace = !ShowWhiteSpace;
			Invalidate();
			break;
		}
		case ID_HARD_TABS:
		{
			HardTabs = !HardTabs;
			break;
		}
		case ID_INDENT_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", IndentSize);
			auto i = new LInput(this, s, "Indent Size:", "Text");
			i->DoModal([this, i](auto dlg, auto ctrlId)
			{
				if (ctrlId == IDOK)
				{
					IndentSize = (uint8_t)i->GetStr().Int();
					Invalidate();
				}
			});
			break;
		}
		case ID_TAB_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", TabSize);
			auto i = new LInput(this, s, "Tab Size:", "Text");
			i->DoModal([this, i](auto dlg, auto ok)
			{
				if (ok)
					SetTabSize((uint8_t)i->GetStr().Int());
			});
			break;
		}
		case ID_COPY_ORIGINAL:
		{
			LClipBoard c(this);
			c.Text(d->OriginalText);
			break;
		}
		case ID_COPY_CURRENT:
		{
			LClipBoard c(this);
			c.Text(Name());
			break;
		}
		case ID_DUMP_NODES:
		{
			#ifdef _DEBUG
			NodeView *nv = new NodeView(GetWindow());
			DumpNodes(nv->Tree);
			nv->Visible(true);
			#endif
			break;
		}
		default:
		{
			if (Over)
			{
				LMessage Cmd(M_COMMAND, Id);
				if (Over->OnEvent(&Cmd))
					break;
			}
			
			if (Environment)
				Environment->OnMenu(this, Id, 0);
			break;
		}
	}
}

void LRichTextEdit::OnMouseClick(LMouse &m)
{
	bool Processed = false;
	RectType Clicked = 	d->PosToButton(m);
	if (m.Down())
	{
		Focus(true);
		
		if (m.IsContextMenu())
		{
			DoContextMenu(m);
			return;
		}
		else
		{
			Focus(true);

			if (d->Areas[ToolsArea].Overlap(m.x, m.y)
				// || d->Areas[CapabilityArea].Overlap(m.x, m.y)
				)
			{
				if (Clicked != MaxArea)
				{
					if (d->BtnState[Clicked].IsPress)
					{
						d->BtnState[d->ClickedBtn = Clicked].Pressed = true;
						Invalidate(d->Areas + Clicked);
						Capture(true);
					}
					else
					{
						Processed |= d->ClickBtn(m, Clicked);
					}
				}
			}
			else
			{
				d->WordSelectMode = !Processed && m.Double();

				AutoCursor c(new BlkCursor(NULL, 0, 0));
				LPoint Doc = d->ScreenToDoc(m.x, m.y);
				ssize_t Idx = -1;
				if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx))
				{
					d->ClickedBtn = ContentArea;
					d->SetCursor(c, m.Shift());
					if (d->WordSelectMode)
						SelectWord(Idx);
				}
			}
		}
	}
	else if (IsCapturing())
	{
		Capture(false);

		if (d->ClickedBtn != MaxArea)
		{
			d->BtnState[d->ClickedBtn].Pressed = false;
			Invalidate(d->Areas + d->ClickedBtn);
			Processed |= d->ClickBtn(m, Clicked);
		}

		d->ClickedBtn = MaxArea;
	}

	if (!Processed)
	{
		Capture(m.Down());
	}
}

int LRichTextEdit::OnHitTest(int x, int y)
{
	#ifdef WIN32
	if (GetClient().Overlap(x, y))
	{
		return HTCLIENT;
	}
	#endif
	return LView::OnHitTest(x, y);
}

void LRichTextEdit::OnMouseMove(LMouse &m)
{
	LRichTextEdit::RectType OverBtn = d->PosToButton(m);
	if (d->OverBtn != OverBtn)
	{
		if (d->OverBtn < MaxArea)
		{
			d->BtnState[d->OverBtn].MouseOver = false;
			Invalidate(&d->Areas[d->OverBtn]);
		}
		d->OverBtn = OverBtn;
		if (d->OverBtn < MaxArea)
		{
			d->BtnState[d->OverBtn].MouseOver = true;
			Invalidate(&d->Areas[d->OverBtn]);
		}
	}
	
	if (IsCapturing())
	{
		if (d->ClickedBtn == ContentArea)
		{
			AutoCursor c;
			LPoint Doc = d->ScreenToDoc(m.x, m.y);
			ssize_t Idx = -1;
			if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx) && c)
			{
				if (d->WordSelectMode && d->Selection)
				{
					// Extend the selection to include the whole word
					if (!d->CursorFirst())
					{
						// Extend towards the end of the doc...
						LArray<uint32_t> Txt;
						if (c->Blk->CopyAt(0, c->Blk->Length(), &Txt))
						{
							while
							(
								c->Offset < (int)Txt.Length() &&
								!IsWordBreakChar(Txt[c->Offset])
							)
								c->Offset++;
						}
					}
					else
					{
						// Extend towards the start of the doc...
						LArray<uint32_t> Txt;
						if (c->Blk->CopyAt(0, c->Blk->Length(), &Txt))
						{
							while
							(
								c->Offset > 0 &&
								!IsWordBreakChar(Txt[c->Offset-1])
							)
								c->Offset--;
						}
					}
				}

				d->SetCursor(c, m.Left());
			}
		}
	}

	#ifdef WIN32
	LRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		/*
		LStyle *s = HitStyle(Hit);
		TCHAR *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
		*/
	}
	#endif
}

bool LRichTextEdit::OnKey(LKey &k)
{
	if (k.Down() &&
		d->Cursor)
		d->Cursor->Blink = true;

	if (k.c16 == 17)
		return false;

	#ifdef WINDOWS
	// Wtf is this?
	// Weeeelll, windows likes to send a LK_TAB after a Ctrl+I doesn't it?
	// And this just takes care of that TAB before it can overwrite your
	// selection.
	if (ToLower(k.c16) == 'i' &&
		k.Ctrl())
	{
		d->EatVkeys.Add(LK_TAB);
	}
	else if (d->EatVkeys.Length())
	{
		auto Idx = d->EatVkeys.IndexOf(k.vkey);
		if (Idx >= 0)
		{
			// Yum yum
			d->EatVkeys.DeleteAt(Idx);
			return true;
		}
	}
	#endif

	// k.Trace("LRichTextEdit::OnKey");
	if (k.IsContextMenu())
	{
		LMouse m;
		DoContextMenu(m);
	}
	else if (k.IsChar)
	{
		switch (k.vkey)
		{
			default:
			{
				// process single char input
				if
				(
					!GetReadOnly()
					&&
					(
						(k.c16 >= ' ' || k.vkey == LK_TAB)
						&&
						k.c16 != 127
					)
				)
				{
					if (k.Down() &&
						d->Cursor &&
						d->Cursor->Blk)
					{
						// letter/number etc
						auto b = d->Cursor->Blk;

						AutoTrans Trans(new LRichTextPriv::Transaction);						
						d->DeleteSelection(Trans, NULL);

						LNamedStyle *AddStyle = NULL;
						if (d->StyleDirty.Length() > 0)
						{
							LAutoPtr<LCss> Mod(new LCss);
							if (Mod)
							{
								// Get base styles at the cursor..
								if (auto Base = b->GetStyle(d->Cursor->Offset))
									*Mod = *Base;

								// Apply dirty toolbar styles...
								if (d->StyleDirty.HasItem(FontFamilyBtn))
									Mod->FontFamily(LCss::FontFamilies(d->Values[FontFamilyBtn].Str()));
								if (d->StyleDirty.HasItem(FontSizeBtn))
									Mod->FontSize(LCss::Len(LCss::LenPt, (float) d->Values[FontSizeBtn].CastDouble()));
								if (d->StyleDirty.HasItem(BoldBtn))
									Mod->FontWeight(d->Values[BoldBtn].CastInt32() ? LCss::FontWeightBold : LCss::FontWeightNormal);
								if (d->StyleDirty.HasItem(ItalicBtn))
									Mod->FontStyle(d->Values[ItalicBtn].CastInt32() ? LCss::FontStyleItalic : LCss::FontStyleNormal);
								if (d->StyleDirty.HasItem(UnderlineBtn))
									Mod->TextDecoration(d->Values[UnderlineBtn].CastInt32() ? LCss::TextDecorUnderline : LCss::TextDecorNone);
								if (d->StyleDirty.HasItem(ForegroundColourBtn))
									Mod->Color(LCss::ColorDef(LCss::ColorRgb, (uint32_t)d->Values[ForegroundColourBtn].CastInt64()));
								if (d->StyleDirty.HasItem(BackgroundColourBtn))
									Mod->BackgroundColor(LCss::ColorDef(LCss::ColorRgb, (uint32_t)d->Values[BackgroundColourBtn].CastInt64()));
							
								AddStyle = d->AddStyleToCache(Mod);
							}
							
							d->StyleDirty.Length(0);
						}
						else if (b->Length() == 0)
						{
							// We have no existing style to modify, so create one from scratch.
							LAutoPtr<LCss> Mod(new LCss);
							if (Mod)
							{
								// Apply dirty toolbar styles...
								Mod->FontFamily(LCss::FontFamilies(d->Values[FontFamilyBtn].Str()));
								Mod->FontSize(LCss::Len(LCss::LenPt, (float)d->Values[FontSizeBtn].CastDouble()));
								Mod->FontWeight(d->Values[BoldBtn].CastInt32() ? LCss::FontWeightBold : LCss::FontWeightNormal);
								Mod->FontStyle(d->Values[ItalicBtn].CastInt32() ? LCss::FontStyleItalic : LCss::FontStyleNormal);
								Mod->TextDecoration(d->Values[UnderlineBtn].CastInt32() ? LCss::TextDecorUnderline : LCss::TextDecorNone);
								Mod->Color(LCss::ColorDef(LCss::ColorRgb, (uint32_t)d->Values[ForegroundColourBtn].CastInt64()));
								
								auto Bk = d->Values[BackgroundColourBtn].CastInt64();
								if (Bk > 0)
									Mod->BackgroundColor(LCss::ColorDef(LCss::ColorRgb, (uint32_t)Bk));
							
								AddStyle = d->AddStyleToCache(Mod);
							}
						}

						uint32_t Ch = k.c16;
						if (b->AddText(Trans, d->Cursor->Offset, &Ch, 1, AddStyle))
						{
							d->Cursor->Set(d->Cursor->Offset + 1);
							Invalidate();
							SendNotify(LNotifyDocChanged);

							d->AddTrans(Trans);
						}
					}
					return true;
				}
				break;
			}
			case LK_RETURN:
			{
				if (GetReadOnly())
					break;

				if (k.Down() && k.IsChar)
				{
					OnEnter(k);
					SendNotify(LNotifyDocChanged);
				}

				return true;
			}
			case LK_BACKSPACE:
			{
				if (GetReadOnly())
					break;

				bool Changed = false;
				AutoTrans Trans(new LRichTextPriv::Transaction);

				if (k.Ctrl())
				{
				    // Ctrl+H
				}
				else if (k.Down())
				{
					LRichTextPriv::Block *b;

					if (HasSelection())
					{
						Changed = d->DeleteSelection(Trans, NULL);
					}
					else if (d->Cursor &&
							 (b = d->Cursor->Blk))
					{
						if (d->Cursor->Offset > 0)
						{
							Changed = b->DeleteAt(Trans, d->Cursor->Offset-1, 1) > 0;
							if (Changed)
							{
								// Has block size reached 0?
								if (b->Length() == 0)
								{
									// Then delete it...
									LRichTextPriv::Block *n = d->Next(b);
									if (n)
									{
										d->Blocks.Delete(b, true);
										d->Cursor.Reset(new LRichTextPriv::BlockCursor(n, 0, 0));
									}
									else
									{
										// No other block to go to, so leave this empty block at the end
										// of the documnent but set the cursor correctly.
										d->Cursor->Set(0);
									}
								}
								else
								{
									d->Cursor->Set(d->Cursor->Offset - 1);
								}
							}
						}
						else
						{
							// At the start of a block:
							LRichTextPriv::Block *Prev = d->Prev(d->Cursor->Blk);
							if (Prev)
							{
								// Try and merge the two blocks...
								ssize_t Len = Prev->Length();
								d->Merge(Trans, Prev, d->Cursor->Blk);
								
								AutoCursor c(new BlkCursor(Prev, Len, -1));
								d->SetCursor(c);
							}
							else // at the start of the doc...
							{
								// Don't send the doc changed...
								return true;
							}
						}
					}
				}

				if (Changed)
				{
					Invalidate();
					d->AddTrans(Trans);
					SendNotify(LNotifyDocChanged);
				}
				return true;
			}
		}
	}
	else // not a char
	{
		switch (k.vkey)
		{
			case LK_TAB:
				return true;
			case LK_RETURN:
				return !GetReadOnly();
			case LK_BACKSPACE:
			{
				if (!GetReadOnly())
				{
					if (k.Alt())
					{
						if (k.Down())
						{
							if (k.Ctrl())
								Redo();
							else
								Undo();
						}
					}
					else if (k.Ctrl())
					{
						if (k.Down())
						{
							// Implement delete by word
							LAssert(!"Impl backspace by word");
						}
					}

					return true;
				}
				break;
			}
			case LK_F3:
			{
				if (k.Down())
					DoFindNext(NULL);
				return true;
			}
			case LK_LEFT:
			{
				#ifdef MAC
				if (k.Ctrl())
				#else
				if (k.Alt())
				#endif
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						LRect r = d->SelectionRect();
						Invalidate(&r);
						
						AutoCursor c(new BlkCursor(d->CursorFirst() ? *d->Cursor : *d->Selection));
						d->SetCursor(c);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_StartOfLine;
						else
						#endif

						d->Seek(d->Cursor,
								#ifdef MAC
								k.Alt() ?
								#else
								k.Ctrl() ?
								#endif
									LRichTextPriv::SkLeftWord : LRichTextPriv::SkLeftChar,
								k.Shift());
					}
				}
				return true;
			}
			case LK_RIGHT:
			{
				#ifdef MAC
				if (k.Ctrl())
				#else
				if (k.Alt())
				#endif
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						LRect r = d->SelectionRect();
						Invalidate(&r);

						AutoCursor c(new BlkCursor(d->CursorFirst() ? *d->Selection : *d->Cursor));
						d->SetCursor(c);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_EndOfLine;
						#endif

						d->Seek(d->Cursor,
								#ifdef MAC
								k.Alt() ?
								#else
								k.Ctrl() ?
								#endif
									LRichTextPriv::SkRightWord : LRichTextPriv::SkRightChar,
								k.Shift());
					}
				}
				return true;
			}
			case LK_UP:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageUp;
					#endif

					d->Seek(d->Cursor,
							LRichTextPriv::SkUpLine,
							k.Shift());
				}
				return true;
			}
			case LK_DOWN:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageDown;
					#endif

					d->Seek(d->Cursor,
							LRichTextPriv::SkDownLine,
							k.Shift());
				}
				return true;
			}
			case LK_END:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_EndOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? LRichTextPriv::SkDocEnd : LRichTextPriv::SkLineEnd,
							k.Shift());
				}
				return true;
			}
			case LK_HOME:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_StartOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? LRichTextPriv::SkDocStart : LRichTextPriv::SkLineStart,
							k.Shift());
				}
				return true;
			}
			case LK_PAGEUP:
			{
				#ifdef MAC
				GTextView4_PageUp:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							LRichTextPriv::SkUpPage,
							k.Shift());
				}
				return true;
				break;
			}
			case LK_PAGEDOWN:
			{
				#ifdef MAC
				GTextView4_PageDown:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							LRichTextPriv::SkDownPage,
							k.Shift());
				}
				return true;
				break;
			}
			case LK_INSERT:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						Copy();
					}
					else if (k.Shift())
					{
						if (!GetReadOnly())
						{
							Paste();
						}
					}
				}
				return true;
				break;
			}
			case LK_DELETE:
			{
				if (GetReadOnly())
					break;

				if (!k.Down())
					return true;

				bool Changed = false;
				LRichTextPriv::Block *b;
				AutoTrans Trans(new LRichTextPriv::Transaction);

				if (HasSelection())
				{
					if (k.Shift())
						Changed |= Cut();
					else
						Changed |= d->DeleteSelection(Trans, NULL);
				}
				else if (d->Cursor &&
						(b = d->Cursor->Blk))
				{
					if (d->Cursor->Offset >= b->Length())
					{
						// Cursor is at the end of this block, pull the styles
						// from the next block into this one.
						LRichTextPriv::Block *next = d->Next(b);
						if (!next)
						{
							// No next block, therefor nothing to delete
							break;
						}

						// Try and merge the blocks
						if (d->Merge(Trans, b, next))
							Changed = true;
						else
						{
							// If the cursor is on the last empty line of a text block,
							// we should delete that '\n' first
							LRichTextPriv::TextBlock *tb = dynamic_cast<LRichTextPriv::TextBlock*>(b);
							if (tb && tb->IsEmptyLine(d->Cursor))
								Changed = tb->StripLast(Trans);

							// move the cursor to the next block
							d->Cursor.Reset(new LRichTextPriv::BlockCursor(b = next, 0, 0));
						}
					}

					if (!Changed && b->DeleteAt(Trans, d->Cursor->Offset, 1))
					{
						if (b->Length() == 0)
						{
							LRichTextPriv::Block *n = d->Next(b);
							if (n)
							{
								d->Blocks.Delete(b, true);
								d->Cursor.Reset(new LRichTextPriv::BlockCursor(n, 0, 0));
							}
						}

						Changed = true;
					}
				}
						
				if (Changed)
				{
					Invalidate();
					d->AddTrans(Trans);
					SendNotify(LNotifyDocChanged);
				}
				return true;
			}
			default:
			{
				if (k.c16 == 17)
					break;

				if (k.c16 == ' ' &&
					k.Ctrl() &&
					k.Alt() &&
					d->Cursor &&
					d->Cursor->Blk)
				{
					if (k.Down())
					{
						// letter/number etc
						LRichTextPriv::Block *b = d->Cursor->Blk;
						uint32_t Nbsp[] = {0xa0};
						if (b->AddText(NoTransaction, d->Cursor->Offset, Nbsp, 1))
						{
							d->Cursor->Set(d->Cursor->Offset + 1);
							Invalidate();
							SendNotify(LNotifyDocChanged);
						}
					}
					break;
				}

				if (k.CtrlCmd() &&
					!k.Alt())
				{
					switch (k.GetChar())
					{
						case 0xbd: // Ctrl+'-'
						{
							/*
							if (k.Down() &&
								Font->PointSize() > 1)
							{
								Font->PointSize(Font->PointSize() - 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 0xbb: // Ctrl+'+'
						{
							/*
							if (k.Down() &&
								Font->PointSize() < 100)
							{
								Font->PointSize(Font->PointSize() + 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 'a':
						case 'A':
						{
							if (k.Down())
							{
								// select all
								SelectAll();
							}
							return true;
							break;
						}
						case 'b':
						case 'B':
						{
							if (k.Down())
							{
								// Bold selection
								LMouse m;
								GetMouse(m);
								d->ClickBtn(m, BoldBtn);
							}
							return true;
							break;
						}
						case 'l':
						case 'L':
						{
							if (k.Down())
							{
								// Underline selection
								LMouse m;
								GetMouse(m);
								d->ClickBtn(m, UnderlineBtn);
							}
							return true;
							break;
						}
						case 'i':
						case 'I':
						{
							if (k.Down())
							{
								// Italic selection
								LMouse m;
								GetMouse(m);
								d->ClickBtn(m, ItalicBtn);
							}
							return true;
							break;
						}
						case 'y':
						case 'Y':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Redo();
								}
								return true;
							}
							break;
						}
						case 'z':
						case 'Z':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									if (k.Shift())
									{
										Redo();
									}
									else
									{
										Undo();
									}
								}
								return true;
							}
							break;
						}
						case 'x':
						case 'X':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Cut();
								}
								return true;
							}
							break;
						}
						case 'c':
						case 'C':
						{
							if (k.Shift())
								return false;
							
							if (k.Down())
								Copy();
							return true;
							break;
						}
						case 'v':
						case 'V':
						{
							if (!k.Shift() &&
								!GetReadOnly())
							{
								if (k.Down())
								{
									Paste();
								}
								return true;
							}
							break;
						}
						case 'f':
						{
							if (k.Down())
								DoFind(NULL);
							return true;
						}
						case 'g':
						case 'G':
						{
							if (k.Down())
								DoGoto(NULL);
							return true;
							break;
						}
						case 'h':
						case 'H':
						{
							if (k.Down())
								DoReplace(NULL);
							return true;
							break;
						}
						case 'u':
						case 'U':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
									DoCase(NULL, k.Shift());
								return true;
							}
							break;
						}
						case LK_RETURN:
						{
							if (!GetReadOnly() && !k.Shift())
							{
								if (k.Down())
								{
									OnEnter(k);
								}
								return true;
							}
							break;
						}
					}
				}
				break;
			}
		}
	}
	
	return false;
}

void LRichTextEdit::OnEnter(LKey &k)
{
	AutoTrans Trans(new LRichTextPriv::Transaction);						

	// Enter key handling
	bool Changed = false;

	if (HasSelection())
		Changed |= d->DeleteSelection(Trans, NULL);
	
	if (d->Cursor &&
		d->Cursor->Blk)
	{
		LRichTextPriv::Block *b = d->Cursor->Blk;
		const uint32_t Nl[] = {'\n'};

		if (b->AddText(Trans, d->Cursor->Offset, Nl, 1))
		{
			d->Cursor->Set(d->Cursor->Offset + 1);
			Changed = true;
		}
		else
		{
			// Some blocks don't take text. However a new block can be created or
			// the text added to the start of the next block
			if (d->Cursor->Offset == 0)
			{
				LRichTextPriv::Block *Prev = d->Prev(b);
				if (Prev)
					Changed = Prev->AddText(Trans, Prev->Length(), Nl, 1);
				else // No previous... must by first block... create new block:
				{
					LRichTextPriv::TextBlock *tb = new LRichTextPriv::TextBlock(d);
					if (tb)
					{
						Changed = true; // tb->AddText(Trans, 0, Nl, 1);
						d->Blocks.AddAt(0, tb);
					}
				}
			}
			else if (d->Cursor->Offset == b->Length())
			{
				LRichTextPriv::Block *Next = d->Next(b);
				if (Next)
				{
					if ((Changed = Next->AddText(Trans, 0, Nl, 1)))
						d->Cursor->Set(Next, 0, -1);
				}
				else // No next block. Create one:
				{
					LRichTextPriv::TextBlock *tb = new LRichTextPriv::TextBlock(d);
					if (tb)
					{
						Changed = true; // tb->AddText(Trans, 0, Nl, 1);
						d->Blocks.Add(tb);
					}
				}
			}
		}
	}

	if (Changed)
	{
		Invalidate();
		d->AddTrans(Trans);
		SendNotify(LNotifyDocChanged);
	}
}

void LRichTextEdit::OnPaintLeftMargin(LSurface *pDC, LRect &r, LColour &colour)
{
	pDC->Colour(colour);
	pDC->Rectangle(&r);
}

void LRichTextEdit::OnPaint(LSurface *pDC)
{
	LRect r = GetClient();
	if (!r.Valid())
		return;

	#if 0
	pDC->Colour(LColour(255, 0, 255));
	pDC->Rectangle();
	#endif

	int FontY = GetUiFont()->GetHeight();

	LCssTools ct(d, GetUiFont());
	r = ct.PaintBorder(pDC, r);

	bool HasSpace = r.Y() > (FontY * 3);
	if (d->ShowTools && HasSpace)
	{
		d->Areas[ToolsArea] = r;
		d->Areas[ToolsArea].y2 = d->Areas[ToolsArea].y1 + (FontY + 8) - 1;
		r.y1 = d->Areas[ToolsArea].y2 + 1;
	}
	else
	{
		d->Areas[ToolsArea].ZOff(-1, -1);
	}

	d->Areas[ContentArea] = r;

	if (d->Layout(VScroll))
		d->Paint(pDC, VScroll);
	// else the scroll bars changed, wait for re-paint
}

LMessage::Result LRichTextEdit::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_CUT:
		{
			Cut();
			break;
		}
		case M_COPY:
		{
			Copy();
			break;
		}
		case M_PASTE:
		{
			Paste();
			break;
		}
		case M_BLOCK_MSG:
		{
			auto b = (LRichTextPriv::Block*)Msg->A();
			LAutoPtr<LMessage> msg((LMessage*)Msg->B());
			if (d->Blocks.HasItem(b) && msg)
			{
				b->OnEvent(msg);
			}
			else printf("%s:%i - No block to receive M_BLOCK_MSG.\n", _FL);
			break;
		}
		case M_ENUMERATE_LANGUAGES:
		{
			LAutoPtr< LArray<LSpellCheck::LanguageId> > Languages((LArray<LSpellCheck::LanguageId>*)Msg->A());
			if (!Languages)
			{
				LgiTrace("%s:%i - M_ENUMERATE_LANGUAGES no param\n", _FL);
				break;
			}

			// LgiTrace("%s:%i - Got M_ENUMERATE_LANGUAGES %s\n", _FL, d->SpellLang.Get());
			bool Match = false;
			for (auto &s: *Languages)
			{
				if (s.LangCode.Equals(d->SpellLang) ||
					s.EnglishName.Equals(d->SpellLang))
				{
					// LgiTrace("%s:%i - EnumDict called %s\n", _FL, s.LangCode.Get());
					d->SpellCheck->EnumDictionaries(AddDispatch(), s.LangCode);
					Match = true;
					break;
				}
			}
			if (!Match)
				LgiTrace("%s:%i - EnumDict not called %s\n", _FL, d->SpellLang.Get());
			break;
		}
		case M_ENUMERATE_DICTIONARIES:
		{
			LAutoPtr< LArray<LSpellCheck::DictionaryId> > Dictionaries((LArray<LSpellCheck::DictionaryId>*)Msg->A());
			if (!Dictionaries)
				break;
	
			bool Match = false;		
			for (auto &s: *Dictionaries)
			{
				// LgiTrace("%s:%i - M_ENUMERATE_DICTIONARIES: %s, %s\n", _FL, s.Dict.Get(), d->SpellDict.Get());
				if (s.Dict.Equals(d->SpellDict))
				{
					d->SpellCheck->SetDictionary(AddDispatch(), s.Lang, s.Dict);
					Match = true;
					break;
				}
			}
			if (!Match)
				d->SpellCheck->SetDictionary(AddDispatch(), d->SpellLang, NULL);
			break;
		}
		case M_SET_DICTIONARY:
		{
			d->SpellDictionaryLoaded = Msg->A() != 0;
			// LgiTrace("%s:%i - M_SET_DICTIONARY=%i\n", _FL, d->SpellDictionaryLoaded);
			if (d->SpellDictionaryLoaded)
			{
				AutoTrans Trans(new LRichTextPriv::Transaction);

				// Get any loaded text blocks to check their spelling
				bool Status = false;
				for (unsigned i=0; i<d->Blocks.Length(); i++)
				{
					Status |= d->Blocks[i]->OnDictionary(Trans);
				}

				if (Status)
					d->AddTrans(Trans);
			}
			break;
		}
		case M_CHECK_TEXT:
		{
			LAutoPtr<LSpellCheck::CheckText> Ct((LSpellCheck::CheckText*)Msg->A());
			if (!Ct || Ct->User.Length() > 1)
			{
				LAssert(0);
				break;
			}
			
			LRichTextPriv::Block *b = (LRichTextPriv::Block*)Ct->User[SpellBlockPtr].CastVoidPtr();
			if (!d->Blocks.HasItem(b))
				break;

			b->SetSpellingErrors(Ct->Errors, *Ct);
			Invalidate();
			break;
		}
		#if defined WIN32
		case WM_GETTEXTLENGTH:
		{
			return 0 /*Size*/;
		}
		case WM_GETTEXT:
		{
			int Chars = (int)Msg->A();
			char *Out = (char*)Msg->B();
			if (Out)
			{
				char *In = (char*)LNewConvertCp(LAnsiToLgiCp(), NameW(), LGI_WideCharset, Chars);
				if (In)
				{
					int Len = (int)strlen(In);
					memcpy(Out, In, Len);
					DeleteArray(In);
					return Len;
				}
			}
			return 0;
		}
		case M_COMPONENT_INSTALLED:
		{
			LAutoPtr<LString> Comp((LString*)Msg->A());
			if (Comp)
				d->OnComponentInstall(*Comp);
			break;
		}

		/* This is broken... the IME returns garbage in the buffer. :(
		case WM_IME_COMPOSITION:
		{
			if (Msg->b & GCS_RESULTSTR) 
			{
				HIMC hIMC = ImmGetContext(Handle());
				if (hIMC)
				{
					int Size = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
					char *Buf = new char[Size];
					if (Buf)
					{
						ImmGetCompositionString(hIMC, GCS_RESULTSTR, Buf, Size);

						char16 *Utf = (char16*)LNewConvertCp(LGI_WideCharset, Buf, LAnsiToLgiCp(), Size);
						if (Utf)
						{
							Insert(Cursor, Utf, StrlenW(Utf));
							DeleteArray(Utf);
						}

						DeleteArray(Buf);
					}

					ImmReleaseContext(Handle(), hIMC);
				}
				return 0;
			}
			break;
		}
		*/
		#endif
	}

	return LLayout::OnEvent(Msg);
}

int LRichTextEdit::OnNotify(LViewI *Ctrl, const LNotification &n)
{
	if (Ctrl->GetId() == IDC_VSCROLL && VScroll)
	{
		Invalidate(d->Areas + ContentArea);
	}

	return 0;
}

void LRichTextEdit::OnPulse()
{
	if (!ReadOnly && d->Cursor)
	{
		uint64 n = LCurrentTime();
		if (d->BlinkTs - n >= RTE_CURSOR_BLINK_RATE)
		{
			d->BlinkTs = n;
			d->Cursor->Blink = !d->Cursor->Blink;
			d->InvalidateDoc(&d->Cursor->Pos);
		}
		
		// Do autoscroll while the user has clicked and dragged off the control:
		if (VScroll && IsCapturing() && d->ClickedBtn == LRichTextEdit::ContentArea)
		{
			LMouse m;
			GetMouse(m);
			
			// Is the mouse outside the content window
			LRect &r = d->Areas[ContentArea];
			if (!r.Overlap(m.x, m.y))
			{
				AutoCursor c(new BlkCursor(NULL, 0, 0));
				LPoint Doc = d->ScreenToDoc(m.x, m.y);
				ssize_t Idx = -1;
				if (d->CursorFromPos(Doc.x, Doc.y, &c, &Idx))
				{
					d->SetCursor(c, true);
					if (d->WordSelectMode)
						SelectWord(Idx);
				}
				
				// Update the screen.
				d->InvalidateDoc(NULL);
			}
		}
	}
}

void LRichTextEdit::OnUrl(char *Url)
{
	if (Environment)
	{
		Environment->OnNavigate(this, Url);
	}
}

bool LRichTextEdit::OnLayout(LViewLayoutInfo &Inf)
{
	Inf.Width.Min = 32;
	Inf.Width.Max = -1;

	// Inf.Height.Min = (Font ? Font->GetHeight() : 18) + 4;
	Inf.Height.Max = -1;

	return true;
}

#if _DEBUG
void LRichTextEdit::SelectNode(LString Param)
{
	LRichTextPriv::Block *b = (LRichTextPriv::Block*) Param.Int(16);
	bool Valid = false;
	for (auto i : d->Blocks)
	{
		if (i == b)
			Valid = true;
		i->DrawDebug = false;
	}
	if (Valid)
	{
		b->DrawDebug = true;
		Invalidate();
	}
}

void LRichTextEdit::DumpNodes(LTree *Root)
{
	d->DumpNodes(Root);
}
#endif


///////////////////////////////////////////////////////////////////////////////
SelectColour::SelectColour(LRichTextPriv *priv, LPoint p, LRichTextEdit::RectType t) : LPopup(priv->View)
{
	d = priv;
	Type = t;

	int Px = 16;
	int PxSp = Px + 2;
	int x = 6;
	int y = 6;

	// Do grey ramp
	for (int i=0; i<8; i++)
	{
		Entry &en = e.New();
		int Grey = i * 255 / 7;
		en.r.ZOff(Px-1, Px-1);
		en.r.Offset(x + (i * PxSp), y);
		en.c.Rgb(Grey, Grey, Grey);
	}

	// Do colours
	y += PxSp + 4;
	int SatRange = 255 - 64;
	int SatStart = 255 - 32;
	int HueStep = 360 / 8;
	for (int sat=0; sat<8; sat++)
	{
		for (int hue=0; hue<8; hue++)
		{
			LColour c;
			c.SetHLS(hue * HueStep, SatStart - ((sat * SatRange) / 7), 255);
			c.ToRGB();

			Entry &en = e.New();
			en.r.ZOff(Px-1, Px-1);
			en.r.Offset(x + (hue * PxSp), y);
			en.c = c;
		}

		y += PxSp;
	}

	SetParent(d->View);

	LRect r(0, 0, 12 + (8 * PxSp) - 1, y + 6 - 1);
	r.Offset(p.x, p.y);
	SetPos(r);

	Visible(true);
}

void SelectColour::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
	for (unsigned i=0; i<e.Length(); i++)
	{
		pDC->Colour(e[i].c);
		pDC->Rectangle(&e[i].r);
	}
}

void SelectColour::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		for (unsigned i=0; i<e.Length(); i++)
		{
			if (e[i].r.Overlap(m.x, m.y))
			{
				d->Values[Type] = (int64)e[i].c.c32();
				d->View->Invalidate(d->Areas + Type);
				d->OnStyleChange(Type);
				Visible(false);
				break;
			}
		}
	}
}

void SelectColour::Visible(bool i)
{
	LPopup::Visible(i);
	if (!i)
	{
		d->View->Focus(true);
		delete this;
	}
}

///////////////////////////////////////////////////////////////////////////////
#define EMOJI_PAD	2
#include "lgi/common/Emoji.h"
int EmojiMenu::Cur = 0;

EmojiMenu::EmojiMenu(LRichTextPriv *priv, LPoint p) : LPopup(priv->View)
{
	d = priv;
	d->GetEmojiImage();

	int MaxIdx = 0;
	LHashTbl<IntKey<int>, int> Map;
	for (int b=0; b<CountOf(EmojiRanges); b++)
	{
		LRange &r = EmojiRanges[b];
		for (int i=0; i<r.Len; i++)
		{
			uint32_t u = (int)r.Start + i;
			auto Emoji = EmojiToIconIndex(&u, 1);
			if (Emoji.Index >= 0)
			{
				Map.Add(Emoji.Index, u);
				MaxIdx = MAX(MaxIdx, Emoji.Index);
			}
		}
	}

	int Sz = EMOJI_CELL_SIZE - 1;
	int PaneCount = 5;
	int PaneSz = (int)(Map.Length() / PaneCount);
	int ImgIdx = 0;

	int PaneSelectSz = LSysFont->GetHeight() * 2;
	int Rows = (PaneSz + EMOJI_GROUP_X - 1) / EMOJI_GROUP_X;
	LRect r(0, 0,
			(EMOJI_CELL_SIZE + EMOJI_PAD) * EMOJI_GROUP_X + EMOJI_PAD,
			(EMOJI_CELL_SIZE + EMOJI_PAD) * Rows + EMOJI_PAD + PaneSelectSz);
	r.Offset(p.x, p.y);
	SetPos(r);
	
	for (int pi = 0; pi < PaneCount; pi++)
	{
		Pane &p = Panes[pi];
		int Wid = X() - (EMOJI_PAD*2);
		p.Btn.x1 = EMOJI_PAD + (pi * Wid / PaneCount);
		p.Btn.y1 = EMOJI_PAD;
		p.Btn.x2 = EMOJI_PAD + ((pi + 1) * Wid / PaneCount) - 1;
		p.Btn.y2 = EMOJI_PAD + PaneSelectSz;
		int Dx = EMOJI_PAD;
		int Dy = p.Btn.y2 + 1;
		
		while ((int)p.e.Length() < PaneSz && ImgIdx <= MaxIdx)
		{
			uint32_t u = Map.Find(ImgIdx);
			if (u)
			{
				Emoji &Ch = p.e.New();
				Ch.u = u;

				int Sx = ImgIdx % EMOJI_GROUP_X;
				int Sy = ImgIdx / EMOJI_GROUP_X;

				Ch.Src.ZOff(Sz, Sz);
				Ch.Src.Offset(Sx * EMOJI_CELL_SIZE, Sy * EMOJI_CELL_SIZE);

				Ch.Dst.ZOff(Sz, Sz);
				Ch.Dst.Offset(Dx, Dy);

				Dx += EMOJI_PAD + EMOJI_CELL_SIZE;
				if (Dx + EMOJI_PAD + EMOJI_CELL_SIZE >= r.X())
				{
					Dx = EMOJI_PAD;
					Dy += EMOJI_PAD + EMOJI_CELL_SIZE;
				}
			}
			ImgIdx++;
		}
	}

	SetParent(d->View);
	Visible(true);
}

void EmojiMenu::OnPaint(LSurface *pDC)
{
	LAutoPtr<LDoubleBuffer> DblBuf;
	if (!pDC->SupportsAlphaCompositing())
		DblBuf.Reset(new LDoubleBuffer(pDC));

	pDC->Colour(L_MED);
	pDC->Rectangle();

	LSurface *EmojiImg = d->GetEmojiImage();
	if (EmojiImg)
	{
		pDC->Op(GDC_ALPHA);
		
		for (unsigned i=0; i<Panes.Length(); i++)
		{
			Pane &p = Panes[i];
			
			LString s;
			s.Printf("%i", i);
			LDisplayString Ds(LSysFont, s);
			if (Cur == i)
			{
				pDC->Colour(L_LIGHT);
				pDC->Rectangle(&p.Btn);
			}
			LSysFont->Fore(L_TEXT);
			LSysFont->Transparent(true);
			Ds.Draw(pDC, p.Btn.x1 + ((p.Btn.X()-Ds.X())>>1), p.Btn.y1 + ((p.Btn.Y()-Ds.Y())>>1));
		}
		
		Pane &p = Panes[Cur];
		for (unsigned i=0; i<p.e.Length(); i++)
		{
			Emoji &g = p.e[i];
			pDC->Blt(g.Dst.x1, g.Dst.y1, EmojiImg, &g.Src);
		}
	}
	else
	{
		LRect c = GetClient();
		LDisplayString Ds(LSysFont, "Loading...");
		LSysFont->Colour(L_TEXT, L_MED);
		LSysFont->Transparent(true);
		Ds.Draw(pDC, (c.X()-Ds.X())>>1, (c.Y()-Ds.Y())>>1);
	}
}

bool EmojiMenu::InsertEmoji(uint32_t Ch)
{
	if (!d->Cursor || !d->Cursor->Blk)
		return false;

	AutoTrans Trans(new LRichTextPriv::Transaction);						

	if (!d->Cursor->Blk->AddText(NoTransaction, d->Cursor->Offset, &Ch, 1, NULL))
		return false;

	AutoCursor c(new BlkCursor(*d->Cursor));
	c->Offset++;
	d->SetCursor(c);

	d->AddTrans(Trans);
						
	d->Dirty = true;
	d->InvalidateDoc(NULL);
	d->View->SendNotify(LNotifyDocChanged);

	return true;
}

void EmojiMenu::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		for (unsigned i=0; i<Panes.Length(); i++)
		{
			Pane &p = Panes[i];
			if (p.Btn.Overlap(m.x, m.y))
			{
				Cur = i;
				Invalidate();
				return;
			}
		}
		
		Pane &p = Panes[Cur];
		for (unsigned i=0; i<p.e.Length(); i++)
		{
			Emoji &Ch = p.e[i];
			if (Ch.Dst.Overlap(m.x, m.y))
			{
				InsertEmoji(Ch.u);
				Visible(false);
				break;
			}
		}
	}
}

void EmojiMenu::Visible(bool i)
{
	LPopup::Visible(i);
	if (!i)
	{
		d->View->Focus(true);
		delete this;
	}
}

///////////////////////////////////////////////////////////////////////////////
class LRichTextEdit_Factory : public LViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "LRichTextEdit") == 0)
		{
			return new LRichTextEdit(-1, 0, 0, 2000, 2000);
		}

		return 0;
	}
}	RichTextEdit_Factory;
