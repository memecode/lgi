#include "lgi/common/Lgi.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/Menu.h"
#include "LgiIde.h"
#include "DocEdit.h"
#include "IdeDocPrivate.h"

#define EDIT_TRAY_HEIGHT	(LSysFont->GetHeight() + 10)
#define EDIT_LEFT_MARGIN	16 // gutter for debug break points
int DocEdit::LeftMarginPx = EDIT_LEFT_MARGIN;
LAutoPtr<GDocFindReplaceParams> GlobalFindReplace;
DocEdit::DocEdit(IdeDoc *d, LFontType *f) :
	LTextView3(IDC_EDIT, 0, 0, 100, 100, f),
	DocEditStyling(this)
{
	RefreshSize = 0;
	RefreshEdges = NULL;
	FileType = SrcUnknown;
	Doc = d;
	CurLine = -1;
	if (!GlobalFindReplace)
	{
		GlobalFindReplace.Reset(CreateFindReplaceParams());
	}
	SetFindReplaceParams(GlobalFindReplace);
		
	CanScrollX = true;
	GetCss(true)->PaddingLeft(LCss::Len(LCss::LenPx, (float)(LeftMarginPx + 2)));
		
	if (!f)
	{
		LFontType Type;
		if (Type.GetSystemFont("Fixed"))
		{
			LFont *f = Type.Create();
			if (f)
			{
				#if defined LINUX
				f->PointSize(9);
				#elif defined WIN32
				f->PointSize(8);
				#endif
				SetFont(f);
			}
		}
	}
		
	SetWrapType(TEXTED_WRAP_NONE);
	SetEnv(this);
}
	
DocEdit::~DocEdit()
{
	ParentState = KExiting;
	Event.Signal();
	while (!IsExited())
		LSleep(1);
	SetEnv(0);
}
void DocEdit::OnCreate()
{
	LTextView3::OnCreate();
	Run();
}
bool DocEdit::AppendItems(LSubMenu *Menu, const char *Param, int Base)
{
	LSubMenu *Insert = Menu->AppendSub("Insert...");
	if (Insert)
	{
		Insert->AppendItem("File Comment", IDM_FILE_COMMENT, Doc->GetProject() != 0);
		Insert->AppendItem("Function Comment", IDM_FUNC_COMMENT, Doc->GetProject() != 0);
	}
	return true;
}
	
void DocEdit::DoGoto(std::function<void(bool)> Callback)
{
	LInput *Dlg = new LInput(this, "", LLoadString(L_TEXTCTRL_GOTO_LINE, "Goto [file:]line:"), "Goto");	
	Dlg->DoModal([&, App=Doc->GetApp()](auto d, auto code)
	{
		bool status = code && ValidStr(Dlg->GetStr());
		if (status)
		{	
			auto s = Dlg->GetStr();
			LString::Array p = s.SplitDelimit(":,");
			if (p.Length() == 2)
			{
				LString file = p[0];
				int line = (int)p[1].Int();
				App->GotoReference(file, line, false, true);
			}
			else if (p.Length() == 1)
			{
				int line = (int)p[0].Int();
				if (line > 0)
					SetLine(line);
				else
					// Probably a filename with no line number..
					App->GotoReference(p[0], 1, false, true);				
			}
		}

		if (Callback)
			Callback(status);

		delete Dlg;
	});
}

bool DocEdit::SetPourEnabled(bool b)
{
	bool e = PourEnabled;
	PourEnabled = b;
	if (PourEnabled)
	{
		PourText(0, Size);
		PourStyle(0, Size);
	}
	return e;
}
int DocEdit::GetTopPaddingPx()
{
	return GetCss(true)->PaddingTop().ToPx(GetClient().Y(), GetFont());
}
void DocEdit::InvalidateLine(int Idx)
{
	LTextLine *Ln = LTextView3::Line[Idx];
	if (Ln)
	{
		int PadPx = GetTopPaddingPx();
		LRect r = Ln->r;
		r.Offset(0, -ScrollYPixel() + PadPx);
		// LgiTrace("%s:%i - r=%s\n", _FL, r.GetStr());
		Invalidate(&r);
	}
}
	
void DocEdit::OnPaintLeftMargin(LSurface *pDC, LRect &r, LColour &colour)
{
	LColour GutterColour(0xfa, 0xfa, 0xfa);
	LTextView3::OnPaintLeftMargin(pDC, r, GutterColour);
	int Y = ScrollYLine();
		
	int TopPaddingPx = GetTopPaddingPx();
	pDC->Colour(LColour(200, 0, 0));
	List<LTextLine>::I it = LTextView3::Line.begin(Y);
	int DocOffset = (*it)->r.y1;
	for (LTextLine *l = *it; l; l = *++it, Y++)
	{
		if (Doc->d->BreakPoints.Find(Y+1))
		{
			int r = l->r.Y() >> 1;
			pDC->FilledCircle(8, l->r.y1 + r + TopPaddingPx - DocOffset, r - 1);
		}
	}
	bool DocMatch = Doc->IsCurrentIp();
	{
		// We have the current IP location
		it = LTextView3::Line.begin();
		int Idx = 1;
		for (LTextLine *ln = *it; ln; ln = *++it, Idx++)
		{
			if (DocMatch && Idx == IdeDoc::CurIpLine)
			{
				ln->Back = LColour(L_DEBUG_CURRENT_LINE);
			}
			else
			{
				ln->Back.Empty();
			}
		}
	}
}
void DocEdit::OnMouseClick(LMouse &m)
{
	if (m.Down())
	{
		if (HasSelection())
		{
			MsClick.x = -100;
			MsClick.y = -100;
		}
		else
		{
			MsClick.x = m.x;
			MsClick.y = m.y;
		}
	}
	else if
	(
		m.x < LeftMarginPx &&
		abs(m.x - MsClick.x) < 5 &&
		abs(m.y - MsClick.y) < 5
	)
	{
		// Margin click... work out the line
		int Y = (VScroll) ? (int)VScroll->Value() : 0;
		LFont *f = GetFont();
		if (!f) return;
		LCss::Len PaddingTop = GetCss(true)->PaddingTop();
		int TopPx = PaddingTop.ToPx(GetClient().Y(), f);
		int Idx = ((m.y - TopPx) / f->GetHeight()) + Y + 1;
		if (Idx > 0 && Idx <= LTextView3::Line.Length())
		{
			Doc->OnMarginClick(Idx);
		}
	}
	LTextView3::OnMouseClick(m);
}
void DocEdit::SetCaret(size_t i, bool Select, bool ForceFullUpdate)
{
	LTextView3::SetCaret(i, Select, ForceFullUpdate);
		
	if (IsAttached())
	{
		auto Line = (int)GetLine();
		if (Line != CurLine)
		{
			Doc->OnLineChange(CurLine = Line);
		}
	}
}
	
char *DocEdit::TemplateMerge(const char *Template, const char *Name, List<char> *Params)
{
	// Parse template and insert into doc
	LStringPipe T;
	for (const char *t = Template; *t; )
	{
		char *e = strstr((char*) t, "<%");
		if (e)
		{
			// Push text before tag
			T.Push(t, e-t);
			char *TagStart = e;
			e += 2;
			skipws(e);
			char *Start = e;
			while (*e && isalpha(*e)) e++;
				
			// Get tag
			char *Tag = NewStr(Start, e-Start);
			if (Tag)
			{
				// Process tag
				if (Name && stricmp(Tag, "name") == 0)
				{
					T.Push(Name);
				}
				else if (Params && stricmp(Tag, "params") == 0)
				{
					char *Line = TagStart;
					while (Line > Template && Line[-1] != '\n') Line--;
						
					int i = 0;
					for (auto p: *Params)
					{
						if (i) T.Push(Line, TagStart-Line);
						T.Push(p);
						if (i < Params->Length()-1) T.Push("\n");
						i++;
					}
				}
					
				DeleteArray(Tag);
			}
				
			e = strstr(e, "%>");
			if (e)
			{
				t = e + 2;
			}
			else break;
		}
		else
		{
			T.Push(t);
			break;
		}
	}
	T.Push("\n");
	return T.NewStr();
}
bool DocEdit::GetVisible(LStyle &s)
{
	LRect c = GetClient();
	auto a = HitText(c.x1, c.y1, false);
	auto b = HitText(c.x2, c.y2, false);
	s.Start = a;
	s.Len = b - a + 1;
	return true;
}
bool DocEdit::Pour(LRegion &r)
{
	LRect c = r.Bound();
	c.y2 -= EDIT_TRAY_HEIGHT;
	SetPos(c);
		
	return true;
}
bool DocEdit::Insert(size_t At, const char16 *Data, ssize_t Len)
{
	int Old = PourEnabled ? CountRefreshEdges(At, 0) : 0;
	bool Status = LTextView3::Insert(At, Data, Len);
	int New = PourEnabled ? CountRefreshEdges(At, Len) : 0;
	if (Old != New)
		Invalidate();
	return Status;
}
bool DocEdit::Delete(size_t At, ssize_t Len)
{
	int Old = CountRefreshEdges(At, Len);
	bool Status = LTextView3::Delete(At, Len);
	int New = CountRefreshEdges(At, 0);
	if (Old != New)
		Invalidate();
	return Status;
}
bool DocEdit::OnKey(LKey &k)
{
	#ifdef MAC
	if (k.Ctrl())
	#else
	if (k.Alt())
	#endif
	{
		// This allows the Alt+Left/Right to be processed by the prev/next navigator menu.
		if (k.vkey == LK_LEFT ||
			k.vkey == LK_RIGHT)
		{
			return false;
		}
	}
	if (k.AltCmd())
	{
		if (ToLower(k.c16) == 'm')
		{
			if (k.Down())
				Doc->GotoSearch(IDC_METHOD_SEARCH);
			return true;
		}
		else if (ToLower(k.c16) == 'o' &&
				k.Shift())
		{
			if (k.Down())
				Doc->GotoSearch(IDC_FILE_SEARCH);
			return true;
		}
	}
	return LTextView3::OnKey(k); 
}
LMessage::Result DocEdit::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case M_STYLING_DONE:
			OnApplyStyles();
			break;
	}
	return LTextView3::OnEvent(m);
}
bool DocEdit::OnMenu(LDocView *View, int Id, void *Context)
{
	if (View)
	{
		switch (Id)
		{
			case IDM_FILE_COMMENT:
			{
				const char *Template = Doc->GetProject()->GetFileComment();
				if (Template)
				{
					auto File = strrchr(Doc->GetFileName(), DIR_CHAR);
					if (File)
					{
						auto Comment = TemplateMerge(Template, File + 1, 0);
						if (Comment)
						{
							char16 *C16 = Utf8ToWide(Comment);
							DeleteArray(Comment);
							if (C16)
							{
								Insert(Cursor, C16, StrlenW(C16));
								DeleteArray(C16);
								Invalidate();
							}
						}
					}
				}
				break;
			}
			case IDM_FUNC_COMMENT:
			{
				const char *Template = Doc->GetProject()->GetFunctionComment();
				if (ValidStr(Template))
				{
					const char16 *n = NameW();
					if (n)
					{
						List<char16> Tokens;
						char16 *s;
						char16 *p = (char16*)n + GetCaret();
						char16 OpenBrac[] = { '(', 0 };
						char16 CloseBrac[] = { ')', 0 };
						ssize_t OpenBracketIndex = -1;							
						
						// Parse from cursor to the end of the function defn
						while ((s = LexCpp(p, LexStrdup)))
						{
							if (StricmpW(s, OpenBrac) == 0)
							{
								OpenBracketIndex = Tokens.Length();
							}
							Tokens.Insert(s);
							if (StricmpW(s, CloseBrac) == 0)
							{
								break;
							}
						}
						
						if (OpenBracketIndex > 0)
						{
							char *FuncName = WideToUtf8(Tokens[OpenBracketIndex-1]);
							if (FuncName)
							{
								// Get a list of parameter names
								List<char> Params;
								for (auto i = OpenBracketIndex+1; (p = Tokens[i]); i++)
								{
									char16 Comma[] = { ',', 0 };
									if (StricmpW(p, Comma) == 0 ||
										StricmpW(p, CloseBrac) == 0)
									{
										char16 *Param = Tokens[i-1];
										if (Param)
										{
											Params.Insert(WideToUtf8(Param));
										}
									}
								}
								
								// Do insertion
								char *Comment = TemplateMerge(Template, FuncName, &Params);
								if (Comment)
								{
									char16 *C16 = Utf8ToWide(Comment);
									DeleteArray(Comment);
									if (C16)
									{
										Insert(Cursor, C16, StrlenW(C16));
										DeleteArray(C16);
										Invalidate();
									}
								}
								
								// Clean up
								DeleteArray(FuncName);
								Params.DeleteArrays();
							}
							else
							{
								LgiTrace("%s:%i - No function name.\n", _FL);
							}
						}
						else
						{
							LgiTrace("%s:%i - OpenBracketIndex not found.\n", _FL);
						}
						
						Tokens.DeleteArrays();
					}
					else
					{
						LgiTrace("%s:%i - No input text.\n", _FL);
					}
				}
				else
				{
					LgiTrace("%s:%i - No template.\n", _FL);
				}
				break;
			}
		}
	}
	
	return true;
}