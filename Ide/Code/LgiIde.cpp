#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Mdi.h"
#include "lgi/common/Token.h"
#include "lgi/common/XmlTree.h"
#include "lgi/common/Panel.h"
#include "lgi/common/Button.h"
#include "lgi/common/TabView.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Box.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/Edit.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Combo.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Box.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/About.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ToolBar.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/SubProcess.h"

#include "LgiIde.h"
#include "FtpThread.h"
#include "FindSymbol.h"
#include "GDebugger.h"
#include "ProjectNode.h"

#define IDM_RECENT_FILE			1000
#define IDM_RECENT_PROJECT		1100
#define IDM_WINDOWS				1200
#define IDM_MAKEFILE_BASE		1300

#define USE_HAIKU_PULSE_HACK	0

#define OPT_ENTIRE_SOLUTION		"SearchSolution"
#define OPT_SPLIT_PX			"SplitPos"
#define OPT_OUTPUT_PX			"OutputPx"
#define OPT_FIX_RENAMED			"FixRenamed"
#define OPT_RENAMED_SYM			"RenamedSym"

#define IsSymbolChar(c)			( IsDigit(c) || IsAlpha(c) || strchr("-_", c) )

//////////////////////////////////////////////////////////////////////////////////////////
class FindInProject : public LDialog
{
	AppWnd *App;
	LList *Lst;

public:
	FindInProject(AppWnd *app)
	{
		Lst = NULL;
		App = app;
		if (LoadFromResource(IDC_FIND_PROJECT_FILE))
		{
			MoveSameScreen(App);

			LViewI *v;
			if (GetViewById(IDC_TEXT, v))
				v->Focus(true);
			if (!GetViewById(IDC_FILES, Lst))
				return;

			RegisterHook(this, LKeyEvents, 0);
		}
	}

	bool OnViewKey(LView *v, LKey &k)
	{
		switch (k.vkey)
		{
			case LK_UP:
			case LK_DOWN:
			case LK_PAGEDOWN:
			case LK_PAGEUP:
			{
				return Lst->OnKey(k);
				break;
			}
			case LK_RETURN:
			{
				if (k.Down())
				{
					LListItem *i = Lst->GetSelected();
					if (i)
					{
						const char *Ref = i->GetText(0);
						App->GotoReference(Ref, 1, false);
					}
					EndModal(1);
					return true;
				}
				break;
			}
			case LK_ESCAPE:
			{
				if (k.Down())
				{
					EndModal(0);
					return true;
				}
				break;
			}
		}
	
		return false;
	}

	void Search(const char *s)
	{
		IdeProject *p = App->RootProject();
		if (!p || !s)
			return;
		
		LArray<ProjectNode*> Matches, Nodes;

		List<IdeProject> All;
		p->GetChildProjects(All);
		All.Insert(p);							
		for (auto p: All)
		{
			p->GetAllNodes(Nodes);
		}

		FilterFiles(Matches, Nodes, s);

		Lst->Empty();
		for (auto m: Matches)
		{
			LListItem *li = new LListItem;
			LString Fn = m->GetFileName();
			#ifdef WINDOWS
			Fn = Fn.Replace("/","\\");
			#else
			Fn = Fn.Replace("\\","/");
			#endif
			m->GetProject()->CheckExists(Fn);
			li->SetText(Fn);
			Lst->Insert(li);
		}

		Lst->ResizeColumnsToContent();
	}

	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDC_FILES:
				if (n.Type == LNotifyItemDoubleClick)
				{
					LListItem *i = Lst->GetSelected();
					if (i)
					{
						App->GotoReference(i->GetText(0), 1, false);
						EndModal(1);
					}
				}
				break;
			case IDC_TEXT:
				if (n.Type != LNotifyReturnKey)
					Search(c->Name());
				break;
			case IDCANCEL:
				EndModal(0);
				break;
		}

		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
char AppName[] = "LgiIde";

char *dirchar(char *s, bool rev = false)
{
	if (rev)
	{
		char *last = 0;
		while (s && *s)
		{
			if (*s == '/' || *s == '\\')
				last = s;
			s++;
		}
		return last;
	}
	else
	{
		while (s && *s)
		{
			if (*s == '/' || *s == '\\')
				return s;
			s++;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
class Dependency : public LTreeItem
{
	char *File;
	bool Loaded;
	LTreeItem *Fake;

public:
	Dependency(const char *file)
	{
		File = NewStr(file);
		char *d = strrchr(File, DIR_CHAR);
		Loaded = false;
		Insert(Fake = new LTreeItem);

		if (LFileExists(File))
		{
			SetText(d?d+1:File);
		}
		else
		{
			char s[256];
			sprintf(s, "%s (missing)", d?d+1:File);
			SetText(s);
		}
	}
	
	~Dependency()
	{
		DeleteArray(File);
	}
	
	char *GetFile()
	{
		return File;
	}
	
	void Copy(LStringPipe &p, int Depth = 0)
	{
		{
			char s[1024];
			ZeroObj(s);
			memset(s, ' ', Depth * 4);
			sprintf(s+(Depth*4), "[%c] %s\n", Expanded() ? '-' : '+', GetText(0));
			p.Push(s);
		}
		
		if (Loaded)
		{
			for (LTreeItem *i=GetChild(); i; i=i->GetNext())
			{
				((Dependency*)i)->Copy(p, Depth+1);
			}
		}
	}
	
	char *Find(const char *Paths, char *e)
	{
		GToken Path(Paths, LGI_PATH_SEPARATOR);
		for (int p=0; p<Path.Length(); p++)
		{
			char Full[256];
			LMakePath(Full, sizeof(Full), Path[p], e);
			if (LFileExists(Full))
			{
				return NewStr(Full);
			}
		}
		
		return 0;
	}
	
	char *Find(char *e)
	{
		char *s = Find("/lib:/usr/X11R6/lib:/usr/lib", e);
		if (s) return s;

		char *Env = getenv("LD_LIBRARY_PATH");
		if (Env)
		{
			s = Find(Env, e);
			if (s) return s;
		}
		
		return 0;
	}

	void OnExpand(bool b)
	{
		if (b && !Loaded)
		{
			Loaded = true;
			DeleteObj(Fake);
			
			LStringPipe Out;
			char Args[256];
			sprintf(Args, "-d %s", File);
			
			LSubProcess p("readelf", Args);
			if (p.Start())
			{
				p.Communicate(&Out);

				char *o = Out.NewStr();
				// LgiTrace("o=%s\n", o);
				if (o)
				{
					GToken t(o, "\r\n");
					for (int i=0; i<t.Length(); i++)
					{
						char *SharedLib = stristr(t[i], "Shared library:");
						if (SharedLib &&
							(SharedLib = strchr(SharedLib, '[')) != 0)
						{
							char *e = strchr(++SharedLib, ']');
							if (e)
							{
								*e++ = 0;

								char *Dep = Find(SharedLib);
								Insert(new Dependency(Dep?Dep:SharedLib));
								DeleteArray(Dep);
							}
						}
					}
					
					DeleteArray(o);
				}
			}
			else
			{
				LgiMsg(Tree, "Couldn't read executable.\n", AppName);
			}
		}
	}
};

#define IDC_COPY 100
class Depends : public LDialog
{
	Dependency *Root;

public:
	Depends(LView *Parent, const char *File)
	{
		Root = 0;
		SetParent(Parent);
		LRect r(0, 0, 600, 700);
		SetPos(r);
		MoveToCenter();
		Name("Dependencies");
		
		LTree *t = new LTree(100, 10, 10, r.X() - 20, r.Y() - 50);
		if (t)
		{
			t->Sunken(true);
			
			Root = new Dependency(File);
			if (Root)
			{
				t->Insert(Root);
				Root->Expanded(true);
			}
			
			Children.Insert(t);
			Children.Insert(new LButton(IDC_COPY, 10, t->LView::GetPos().y2 + 10, 60, 20, "Copy"));
			Children.Insert(new LButton(IDOK, 80, t->LView::GetPos().y2 + 10, 60, 20, "Ok"));
		}
		
		
		DoModal();
	}
	
	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDC_COPY:
			{
				if (Root)
				{
					LStringPipe p;
					Root->Copy(p);
					char *s = p.NewStr();
					if (s)
					{
						LClipBoard c(this);
						c.Text(s);
						DeleteArray(s);
					}
					break;
				}
				break;
			}			
			case IDOK:
			{
				EndModal(0);
				break;
			}
		}
		
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
class DebugTextLog : public LTextLog
{
public:
	DebugTextLog(int id) : LTextLog(id)
	{
	}
	
	void PourText(size_t Start, ssize_t Len) override
	{
		auto Ts = LCurrentTime();
		LTextView3::PourText(Start, Len);
		auto Dur = LCurrentTime() - Ts;
		if (Dur > 1500)
		{
			// Yo homes, too much text bro...
			Name(NULL);
		}
		else
		{
			for (auto l: Line)
			{
				char16 *t = Text + l->Start;
				
				if (l->Len > 5 && !StrnicmpW(t, L"(gdb)", 5))
				{
					l->c.Rgb(0, 160, 0);
				}
				else if (l->Len > 1 && t[0] == '[')
				{
					l->c.Rgb(192, 192, 192);
				}
			}
		}
	}
};

WatchItem::WatchItem(IdeOutput *out, const char *Init)
{
	Out = out;
	Expanded(false);
	if (Init)
		SetText(Init);
	Insert(PlaceHolder = new LTreeItem);
}

WatchItem::~WatchItem()
{
}

bool WatchItem::SetValue(LVariant &v)
{
	char *Str = v.CastString();
	if (ValidStr(Str))
		SetText(Str, 2);
	else
		LTreeItem::SetText(NULL, 2);
	return true;
}

bool WatchItem::SetText(const char *s, int i)
{
	if (ValidStr(s))
	{
		LTreeItem::SetText(s, i);

		if (i == 0 && Tree && Tree->GetWindow())
		{
			LViewI *Tabs = Tree->GetWindow()->FindControl(IDC_DEBUG_TAB);
			if (Tabs)
				Tabs->SendNotify(LNotifyValueChanged);
		}
		return true;
	}

	if (i == 0)	
		delete this;
		
	return false;
}

void WatchItem::OnExpand(bool b)
{
	if (b && PlaceHolder)
	{
		// Do something 
	}
}

class BuildLog : public LTextLog
{
public:
	BuildLog(int id) : LTextLog(id)
	{
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		List<LTextLine>::I it = LTextView3::Line.begin();
		for (LTextLine *ln = *it; ln; ln = *++it)
		{
			if (!ln->c.IsValid())
			{
				char16 *t = Text + ln->Start;
				char16 *Err = Strnistr(t, L"error", ln->Len);
				char16 *Undef = Strnistr(t, L"undefined reference", ln->Len);
				char16 *Warn = Strnistr(t, L"warning", ln->Len);
				
				if
				(
					(Err && strchr(":[", Err[5]))
					||
					(Undef != NULL)
				)
					ln->c.Rgb(222, 0, 0);
				else if (Warn && strchr(":[", Warn[7]))
					ln->c.Rgb(255, 128, 0);
				else
					ln->c = LColour(L_TEXT);
			}
		}
	}
};

class IdeOutput : public LTabView
{
public:
	AppWnd *App;
	LTabPage *Build;
	LTabPage *Output;
	LTabPage *Debug;
	LTabPage *Find;
	LTabPage *Ftp;
	LList *FtpLog;
	LTextLog *Txt[AppWnd::Channels::ChannelMax];
	LArray<char> Buf[AppWnd::Channels::ChannelMax];
	LFont Small;
	LFont Fixed;

	LTabView *DebugTab;
	LBox *DebugBox;
	LBox *DebugLog;
	LList *Locals, *CallStack, *Threads;
	LTree *Watch;
	LTextLog *ObjectDump, *MemoryDump, *Registers;
	LTableLayout *MemTable;
	LEdit *DebugEdit;
	LTextLog *DebuggerLog;

	IdeOutput(AppWnd *app)
	{
		ZeroObj(Txt);
		App = app;
		Build = Output = Debug = Find = Ftp = 0;
		FtpLog = 0;
		DebugBox = NULL;
		Locals = NULL;
		Watch = NULL;
		DebugLog = NULL;
		DebugEdit = NULL;
		DebuggerLog = NULL;
		CallStack = NULL;
		ObjectDump = NULL;
		MemoryDump = NULL;
		MemTable = NULL;
		Threads = NULL;
		Registers = NULL;

		Small = *LSysFont;
		Small.PointSize(Small.PointSize()-1);
		Small.Create();
		LAssert(Small.Handle());
		
		LFontType Type;
		if (Type.GetSystemFont("Fixed"))
		{
			Type.SetPointSize(LSysFont->PointSize()-1);
			Fixed.Create(&Type);
		}
		else
		{
			Fixed.PointSize(LSysFont->PointSize()-1);
			Fixed.Face("Courier");
			Fixed.Create();			
		}		

		GetCss(true)->MinHeight("60px");

		Build = Append("Build");
		Output = Append("Output");
		Find = Append("Find");
		Ftp = Append("Ftp");
		Debug = Append("Debug");

		SetFont(&Small);
		Build->SetFont(&Small);
		Output->SetFont(&Small);
		Find->SetFont(&Small);
		Ftp->SetFont(&Small);
		Debug->SetFont(&Small);
			
		if (Build)
			Build->Append(Txt[AppWnd::BuildTab] = new BuildLog(IDC_BUILD_LOG));
		if (Output)
			Output->Append(Txt[AppWnd::OutputTab] = new LTextLog(IDC_OUTPUT_LOG));
		if (Find)
			Find->Append(Txt[AppWnd::FindTab] = new LTextLog(IDC_FIND_LOG));
		if (Ftp)
			Ftp->Append(FtpLog = new LList(104, 0, 0, 100, 100));
		if (Debug)
		{
			Debug->Append(DebugBox = new LBox);
			if (DebugBox)
			{
				DebugBox->SetVertical(false);
					
				if ((DebugTab = new LTabView(IDC_DEBUG_TAB)))
				{
					DebugTab->GetCss(true)->Padding("0px");
					DebugTab->SetFont(&Small);
					DebugBox->AddView(DebugTab);

					LTabPage *Page;
					if ((Page = DebugTab->Append("Locals")))
					{
						Page->SetFont(&Small);
						if ((Locals = new LList(IDC_LOCALS_LIST, 0, 0, 100, 100, "Locals List")))
						{
							Locals->SetFont(&Small);
							Locals->AddColumn("", 30);
							Locals->AddColumn("Type", 50);
							Locals->AddColumn("Name", 50);
							Locals->AddColumn("Value", 1000);
							Locals->SetPourLargest(true);

							Page->Append(Locals);
						}
					}
					if ((Page = DebugTab->Append("Object")))
					{
						Page->SetFont(&Small);
						if ((ObjectDump = new LTextLog(IDC_OBJECT_DUMP)))
						{
							ObjectDump->SetFont(&Fixed);
							ObjectDump->SetPourLargest(true);
							Page->Append(ObjectDump);
						}
					}
					if ((Page = DebugTab->Append("Watch")))
					{
						Page->SetFont(&Small);
						if ((Watch = new LTree(IDC_WATCH_LIST, 0, 0, 100, 100, "Watch List")))
						{
							Watch->SetFont(&Small);
							Watch->ShowColumnHeader(true);
							Watch->AddColumn("Watch", 80);
							Watch->AddColumn("Type", 100);
							Watch->AddColumn("Value", 600);
							Watch->SetPourLargest(true);

							Page->Append(Watch);
								
							LXmlTag *w = App->GetOptions()->LockTag("watches", _FL);
							if (!w)
							{
								App->GetOptions()->CreateTag("watches");
								w = App->GetOptions()->LockTag("watches", _FL);
							}
							if (w)
							{
								for (auto c: w->Children)
								{
									if (c->IsTag("watch"))
									{
										Watch->Insert(new WatchItem(this, c->GetContent()));
									}
								}
									
								App->GetOptions()->Unlock();
							}
						}
					}
					if ((Page = DebugTab->Append("Memory")))
					{
						Page->SetFont(&Small);
							
						if ((MemTable = new LTableLayout(IDC_MEMORY_TABLE)))
						{
							LCombo *cbo;
							LCheckBox *chk;
							LTextLabel *txt;
							LEdit *ed;
							MemTable->SetFont(&Small);
							
							int x = 0, y = 0;
							auto *c = MemTable->GetCell(x++, y);
							if (c)
							{
								c->VerticalAlign(LCss::VerticalMiddle);
								c->Add(txt = new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Address:"));
								txt->SetFont(&Small);
							}
							c = MemTable->GetCell(x++, y);
							if (c)
							{
								c->PaddingRight(LCss::Len("1em"));
								c->Add(ed = new LEdit(IDC_MEM_ADDR, 0, 0, 60, 20));
								ed->SetFont(&Small);
							}								
							c = MemTable->GetCell(x++, y);
							if (c)
							{
								c->PaddingRight(LCss::Len("1em"));
								c->Add(cbo = new LCombo(IDC_MEM_SIZE, 0, 0, 60, 20));
								cbo->SetFont(&Small);
								cbo->Insert("1 byte");
								cbo->Insert("2 bytes");
								cbo->Insert("4 bytes");
								cbo->Insert("8 bytes");
							}
							c = MemTable->GetCell(x++, y);
							if (c)
							{
								c->VerticalAlign(LCss::VerticalMiddle);
								c->Add(txt = new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Page width:"));
								txt->SetFont(&Small);
							}
							c = MemTable->GetCell(x++, y);
							if (c)
							{
								c->PaddingRight(LCss::Len("1em"));
								c->Add(ed = new LEdit(IDC_MEM_ROW_LEN, 0, 0, 60, 20));
								ed->SetFont(&Small);
							}
							c = MemTable->GetCell(x++, y);								
							if (c)
							{
								c->VerticalAlign(LCss::VerticalMiddle);
								c->Add(chk = new LCheckBox(IDC_MEM_HEX, 0, 0, -1, -1, "Show Hex"));
								chk->SetFont(&Small);
								chk->Value(true);
							}

							int cols = x;
							x = 0;
							c = MemTable->GetCell(x++, ++y, true, cols);
							if ((MemoryDump = new LTextLog(IDC_MEMORY_DUMP)))
							{
								MemoryDump->SetFont(&Fixed);
								MemoryDump->SetPourLargest(true);
								c->Add(MemoryDump);
							}

							Page->Append(MemTable);
						}
					}
					if ((Page = DebugTab->Append("Threads")))
					{
						Page->SetFont(&Small);
						if ((Threads = new LList(IDC_THREADS, 0, 0, 100, 100, "Threads")))
						{
							Threads->SetFont(&Small);
							Threads->AddColumn("", 20);
							Threads->AddColumn("Thread", 1000);
							Threads->SetPourLargest(true);
							Threads->MultiSelect(false);

							Page->Append(Threads);
						}
					}
					if ((Page = DebugTab->Append("Call Stack")))
					{
						Page->SetFont(&Small);
						if ((CallStack = new LList(IDC_CALL_STACK, 0, 0, 100, 100, "Call Stack")))
						{
							CallStack->SetFont(&Small);
							CallStack->AddColumn("", 20);
							CallStack->AddColumn("Call Stack", 1000);
							CallStack->SetPourLargest(true);
							CallStack->MultiSelect(false);

							Page->Append(CallStack);
						}
					}

					if ((Page = DebugTab->Append("Registers")))
					{
						Page->SetFont(&Small);
						if ((Registers = new LTextLog(IDC_REGISTERS)))
						{
							Registers->SetFont(&Small);
							Registers->SetPourLargest(true);
							Page->Append(Registers);
						}
					}
				}
					
				if ((DebugLog = new LBox))
				{
					DebugLog->SetVertical(true);
					DebugBox->AddView(DebugLog);
					DebugLog->AddView(DebuggerLog = new DebugTextLog(IDC_DEBUGGER_LOG));
					DebuggerLog->SetFont(&Small);
					DebugLog->AddView(DebugEdit = new LEdit(IDC_DEBUG_EDIT, 0, 0, 60, 20));
					DebugEdit->GetCss(true)->Height(LCss::Len(LCss::LenPx, (float)(LSysFont->GetHeight() + 8)));
				}
			}
		}

		if (FtpLog)
		{
			FtpLog->SetPourLargest(true);
			FtpLog->Sunken(true);
			FtpLog->AddColumn("Entry", 1000);
			FtpLog->ShowColumnHeader(false);
		}

		for (int n=0; n<CountOf(Txt); n++)
		{
			if (!Txt[n])
				continue;
			Txt[n]->SetTabSize(8);
			Txt[n]->Sunken(true);
		}
	}

	~IdeOutput()
	{
	}

	const char *GetClass()
	{
		return "IdeOutput";
	}

	void Save()
	{
		if (Watch)
		{
			LXmlTag *w = App->GetOptions()->LockTag("watches", _FL);
			if (!w)
			{
				App->GetOptions()->CreateTag("watches");
				w = App->GetOptions()->LockTag("watches", _FL);
			}
			if (w)
			{
				w->EmptyChildren();
				for (LTreeItem *ti = Watch->GetChild(); ti; ti = ti->GetNext())
				{
					LXmlTag *t = new LXmlTag("watch");
					if (t)
					{
						t->SetContent(ti->GetText(0));
						w->InsertTag(t);
					}
				}
				
				App->GetOptions()->Unlock();
			}
		}
	}
	
	void OnCreate()
	{
		SetPulse(1000);
		AttachChildren();
	}

	void RemoveAnsi(LArray<char> &a)
	{
		char *s = a.AddressOf();
		char *e = s + a.Length();
		while (s < e)
		{
			if (*s == 0x7)
			{
				a.DeleteAt(s - a.AddressOf(), true);
				s--;
			}
			else if
			(
				*s == 0x1b
				&&
				s[1] >= 0x40
				&&
				s[1] <= 0x5f
			)
			{
				// ANSI seq
				char *end;
				if (s[1] == '[' &&
					s[2] == '0' &&
					s[3] == ';')
					end = s + 4;
				else
				{
					end = s + 2;
					while (end < e && !IsAlpha(*end))
					{
						end++;
					}
					if (*end) end++;
				}

				auto len = end - s;
				memmove(s, end, e - end);
				a.Length(a.Length() - len);
				s--;
			}
			s++;
		}
	}
	
	void OnPulse()
	{
		int Changed = -1;

		for (int Channel = 0; Channel<CountOf(Buf); Channel++)
		{
			int64 Size = Buf[Channel].Length();
			if (Size)
			{
				char *Utf = &Buf[Channel][0];
				LAutoPtr<char16, true> w;

				if (!LIsUtf8(Utf, (ssize_t)Size))
				{
					LgiTrace("Ch %i not utf len=" LPrintfInt64 "\n", Channel, Size);
					
					// Clear out the invalid UTF?
					uint8_t *u = (uint8_t*) Utf, *e = u + Size;
					ssize_t len = Size;
					LArray<wchar_t> out;
					while (u < e)
					{
						int32 u32 = LgiUtf8To32(u, len);
						if (u32)
						{
							out.Add(u32);
						}
						else
						{
							out.Add(0xFFFD);
							u++;
						}
					}
					out.Add(0);
					w.Reset(out.Release());
				}
				else
				{
					RemoveAnsi(Buf[Channel]);
					w.Reset(Utf8ToWide(Utf, (ssize_t)Size));
				}
				
				// auto OldText = Txt[Channel]->NameW();
				ssize_t OldLen = Txt[Channel]->Length();

				auto Cur = Txt[Channel]->GetCaret();
				Txt[Channel]->Insert(OldLen, w, StrlenW(w));
				if (Cur > OldLen - 1)
					Txt[Channel]->SetCaret(OldLen + StrlenW(w), false);
				else
					printf("Caret move: %i, %i = %i\n", (int)Cur, (int)OldLen, Cur > OldLen - 1);
				Changed = Channel;
				Buf[Channel].Length(0);
				Txt[Channel]->Invalidate();
			}
		}
		
		/*
		if (Changed >= 0)
			Value(Changed);
		*/
	}
};

int DocSorter(IdeDoc *a, IdeDoc *b, NativeInt d)
{
	auto A = a->GetFileName();
	auto B = b->GetFileName();
	if (A && B)
	{
		auto Af = strrchr(A, DIR_CHAR);
		auto Bf = strrchr(B, DIR_CHAR);
		return stricmp(Af?Af+1:A, Bf?Bf+1:B);
	}
	
	return 0;
}

struct FileLoc
{
	LAutoString File;
	int Line;
	
	void Set(const char *f, int l)
	{
		File.Reset(NewStr(f));
		Line = l;
	}
};

class AppWndPrivate
{
public:
	AppWnd *App;
	GMdiParent *Mdi;
	LOptionsFile Options;
	LBox *HBox, *VBox;
	List<IdeDoc> Docs;
	List<IdeProject> Projects;
	LImageList *Icons;
	LTree *Tree;
	IdeOutput *Output;
	bool Debugging;
	bool Running;
	bool Building;
	bool FixBuildWait = false;
	int RebuildWait = 0;
	LSubMenu *WindowsMenu;
	LSubMenu *CreateMakefileMenu;
	LAutoPtr<FindSymbolSystem> FindSym;
	LArray<LAutoString> SystemIncludePaths;
	LArray<GDebugger::BreakPoint> BreakPoints;
	
	// Debugging
	GDebugContext *DbgContext;
	
	// Cursor history tracking
	int HistoryLoc;
	LArray<FileLoc> CursorHistory;
	bool InHistorySeek;
	
	void SeekHistory(int Direction)
	{
		if (CursorHistory.Length())
		{
			int Loc = HistoryLoc + Direction;
			if (Loc >= 0 && Loc < CursorHistory.Length())
			{
				HistoryLoc = Loc;
				FileLoc &Loc = CursorHistory[HistoryLoc];
				App->GotoReference(Loc.File, Loc.Line, false, false);

				App->DumpHistory();
			}
		}
	}
	
	// Find in files
	LAutoPtr<FindParams> FindParameters;
	LAutoPtr<FindInFilesThread> Finder;
	int AppHnd;
	
	// Mru
	LString::Array RecentFiles;
	LSubMenu *RecentFilesMenu;
	LString::Array RecentProjects;
	LSubMenu *RecentProjectsMenu;

	// Object
	AppWndPrivate(AppWnd *a) :
		Options(LOptionsFile::DesktopMode, AppName),
		AppHnd(LEventSinkMap::Dispatch.AddSink(a))
	{
		FindSym.Reset(new FindSymbolSystem(AppHnd));
		HistoryLoc = 0;
		InHistorySeek = false;
		WindowsMenu = 0;
		App = a;
		HBox = VBox = NULL;
		Tree = 0;
		Mdi = NULL;
		DbgContext = NULL;
		Output = 0;
		Debugging = false;
		Running = false;
		Building = false;
		RecentFilesMenu = 0;
		RecentProjectsMenu = 0;

		Icons = LLoadImageList("icons.png", 16, 16);

		Options.SerializeFile(false);
		App->SerializeState(&Options, "WndPos", true);

		SerializeStringList("RecentFiles", &RecentFiles, false);
		SerializeStringList("RecentProjects", &RecentProjects, false);
	}
	
	~AppWndPrivate()
	{
		if (RecentFilesMenu)
			RecentFilesMenu->Empty();
		if (RecentProjectsMenu)
			RecentProjectsMenu->Empty();

		FindSym.Reset();
		Finder.Reset();
		if (Output)
			Output->Save();
		App->SerializeState(&Options, "WndPos", false);
		SerializeStringList("RecentFiles", &RecentFiles, true);
		SerializeStringList("RecentProjects", &RecentProjects, true);
		Options.SerializeFile(true);
		
		Docs.DeleteObjects();
		Projects.DeleteObjects();
		DeleteObj(Icons);
	}

	bool FindSource(LAutoString &Full, char *File, char *Context)
	{
		if (!LIsRelativePath(File))
		{
			Full.Reset(NewStr(File));
		}

		char *ContextPath = 0;
		if (Context && !Full)
		{
			char *Dir = strrchr(Context, DIR_CHAR);
			for (auto p: Projects)
			{
				ContextPath = p->FindFullPath(Dir?Dir+1:Context);
				if (ContextPath)
					break;
			}
			
			if (ContextPath)
			{
				LTrimDir(ContextPath);
				
				char p[300];
				LMakePath(p, sizeof(p), ContextPath, File);
				if (LFileExists(p))
				{
					Full.Reset(NewStr(p));
				}
			}
			else
			{
				LgiTrace("%s:%i - Context '%s' not found in project.\n", _FL, Context);
			}
		}

		if (!Full)
		{
			List<IdeProject>::I Projs = Projects.begin();
			for (IdeProject *p=*Projs; p; p=*++Projs)
			{
				LAutoString Base = p->GetBasePath();
				if (Base)
				{
					char Path[MAX_PATH];
					LMakePath(Path, sizeof(Path), Base, File);
					if (LFileExists(Path))
					{
						Full.Reset(NewStr(Path));
						break;
					}
				}
			}
		}
		
		if (!Full)
		{
			char *Dir = dirchar(File, true);
			for (auto p: Projects)
			{
				if (Full.Reset(p->FindFullPath(Dir?Dir+1:File)))
					break;
			}
			
			if (!Full)
			{
				if (LFileExists(File))
				{
					Full.Reset(NewStr(File));
				}
			}
		}
		
		return ValidStr(Full);
	}

	void ViewMsg(char *File, int Line, char *Context)
	{
		LAutoString Full;
		if (FindSource(Full, File, Context))
		{
			App->GotoReference(Full, Line, false);
		}
	}
	
	#if 1
		#define LOG_SEEK_MSG(...) printf(__VA_ARGS__)
	#else
		#define LOG_SEEK_MSG(...)
	#endif
		
	void GetContext(const char16 *Txt, ssize_t &i, char16 *&Context)
	{
		static char16 NMsg[] = L"In file included ";
		static char16 FromMsg[] = L"from ";
		auto NMsgLen = StrlenW(NMsg);

		if (Txt[i] != '\n')
			return;

		if (StrncmpW(Txt + i + 1, NMsg, NMsgLen))
			return;

		i += NMsgLen + 1;								
				
		while (Txt[i])
		{
			// Skip whitespace
			while (Txt[i] && strchr(" \t\r\n", Txt[i]))
				i++;
					
			// Check for 'from'
			if (StrncmpW(FromMsg, Txt + i, 5))
				break;

			i += 5;
			auto Start = Txt + i;

			// Skip to end of doc or line
			const char16 *Colon = 0;
			while (Txt[i] && Txt[i] != '\n')
			{
				if (Txt[i] == ':' && Txt[i+1] != '\n')
				{
					Colon = Txt + i;
				}
				i++;
			}
			if (Colon)
			{
				DeleteArray(Context);
				Context = NewStrW(Start, Colon-Start);
			}
		}
	}

	template<typename T>
	bool IsTimeStamp(T *s, ssize_t i)
	{
		while (i > 0 && s[i-1] != '\n')
			i--;
		auto start = i;
		while (s[i] && (IsDigit(s[i]) || strchr(" :-", s[i])))
			i++;
		LString txt(s + start, i - start);
		auto parts = txt.SplitDelimit(" :-");
		return	parts.Length() == 6 &&
				parts[0].Length() == 4;
	}
	
	template<typename T>
	bool IsContext(T *s, ssize_t i)
	{
		auto key = L"In file included";
		auto end = i;
		while (i > 0 && s[i-1] != '\n')
			i--;
		if (Strnistr(s + i, key, end - i))
			return true;
		
		return false;
	}

	#define PossibleLineSep(ch) \
		( (ch) == ':' || (ch) == '(' )
		
	void SeekMsg(int Direction)
	{
		LString Comp;
		IdeProject *p = App->RootProject();
		if (p)
			p ->GetSettings()->GetStr(ProjCompiler);
		// bool IsIAR = Comp.Equals("IAR");
		
		if (!Output)
			return;

		int64 Current = Output->Value();
		LTextView3 *o = Current < CountOf(Output->Txt) ? Output->Txt[Current] : 0;
		if (!o)
			return;

		auto Txt = o->NameW();
		if (!Txt)
			return;

		ssize_t Cur = o->GetCaret();
		char16 *Context = NULL;
		
		// Scan forward to the end of file for the next filename/line number separator.
		ssize_t i;
		for (i=Cur; Txt[i]; i++)
		{
			GetContext(Txt, i, Context);
						
			if
			(
				PossibleLineSep(Txt[i])
				&&
				isdigit(Txt[i+1])
				&&
				!IsTimeStamp(Txt, i)
				&&
				!IsContext(Txt, i)
			)
			{
				break;
			}
		}
					
		// If not found then scan from the start of the file for the next filename/line number separator.
		if (!PossibleLineSep(Txt[i]))
		{
			for (i=0; i<Cur; i++)
			{
				GetContext(Txt, i, Context);
							
				if
				(
					PossibleLineSep(Txt[i])
					&&
					isdigit(Txt[i+1])
					&&
					!IsTimeStamp(Txt, i)
					&&
					!IsContext(Txt, i)
				)
				{
					break;
				}
			}
		}
					
		// If match found?
		if (!PossibleLineSep(Txt[i]))
			return;

		// Scan back to the start of the filename
		auto Line = i;
		while (Line > 0 && !strchr("\n>", Txt[Line-1]))
		{
			Line--;
		}
						
		// Store the filename
		LString File(Txt+Line, i-Line);
		if (!File)
			return;
		#if DIR_CHAR == '\\'
		File = File.Replace("/", "\\");
		#else
		File = File.Replace("\\", "/");
		#endif

		// Scan over the line number..
		auto NumIndex = ++i;
		while (isdigit(Txt[NumIndex]))
			NumIndex++;
							
		// Store the line number
		LString NumStr(Txt + i, NumIndex - i);
		if (!NumStr)
			return;

		// Convert it to an integer
		auto LineNumber = (int)NumStr.Int();
		o->SetCaret(Line, false);
		o->SetCaret(NumIndex + 1, true);
								
		LString Context8 = Context;
		ViewMsg(File, LineNumber, Context8);
	}

	void UpdateMenus()
	{
		static const char *None = "(none)";

		if (!App->GetMenu())
			return; // This happens in GTK during window destruction

		if (RecentFilesMenu)
		{
			RecentFilesMenu->Empty();

			if (RecentFiles.Length() == 0)
				RecentFilesMenu->AppendItem(None, 0, false);
			else
			{
				int n=0;
				char *f;
				for (auto It = RecentFiles.begin(); (f = *It); f=*(++It))
				{
					for (; f; f=*(++It))
					{
						if (LIsUtf8(f))
							RecentFilesMenu->AppendItem(f, IDM_RECENT_FILE+n++, true);
						else
							RecentFiles.Delete(It);
					}
				}
			}
		}

		if (RecentProjectsMenu)
		{
			RecentProjectsMenu->Empty();

			if (RecentProjects.Length() == 0)
				RecentProjectsMenu->AppendItem(None, 0, false);
			else
			{
				int n=0;
				char *f;
				for (auto It = RecentProjects.begin(); (f = *It); f=*(++It))
				{
					if (LIsUtf8(f))
						RecentProjectsMenu->AppendItem(f, IDM_RECENT_PROJECT+n++, true);
					else
						RecentProjects.Delete(It);
				}
			}
		}
		
		if (WindowsMenu)
		{
			WindowsMenu->Empty();
			Docs.Sort(DocSorter);
			int n=0;
			for (auto d: Docs)
			{
				const char *File = d->GetFileName();
				if (!File) File = "(untitled)";
				char *Dir = strrchr((char*)File, DIR_CHAR);
				WindowsMenu->AppendItem(Dir?Dir+1:File, IDM_WINDOWS+n++, true);
			}
			
			if (!Docs.Length())
			{
				WindowsMenu->AppendItem(None, 0, false);
			}
		}
	}

	void Dump(LString::Array &a)
	{
		for (auto i: a)
			printf("  %s\n", i.Get());
	}
	
	void OnFile(const char *File, bool IsProject = false)
	{
		if (!File)
			return;

		auto *Recent = IsProject ? &RecentProjects : &RecentFiles;
		for (auto &f: *Recent)
		{
			if (f && LFileCompare(f, File) == 0)
			{
				f = File;
				UpdateMenus();
				return;
			}
		}

		Recent->AddAt(0, File);
		if (Recent->Length() > 10)
			Recent->Length(10);

		UpdateMenus();
	}
	
	void RemoveRecent(const char *File)
	{
		if (File)
		{
			LString::Array *Recent[3] = { &RecentProjects, &RecentFiles, 0 };
			for (int i=0; Recent[i]; i++)
			{
				auto &a = *Recent[i];
				for (size_t n=0; n<a.Length(); n++)
				{
					if (stricmp(a[n], File) == 0)
					{
						a.DeleteAt(n--);
						break;
					}
				}
			}

			UpdateMenus();
		}
	}

	IdeDoc *IsFileOpen(const char *File)
	{
		if (!File)
		{
			LgiTrace("%s:%i - No input File?\n", _FL);
			return NULL;
		}
		
		for (auto Doc: Docs)
		{
			if (Doc->IsFile(File))
			{
				return Doc;
			}
		}

		// LgiTrace("%s:%i - '%s' not found in %i docs.\n", _FL, File, Docs.Length());
		return 0;
	}

	IdeProject *IsProjectOpen(const char *File)
	{
		if (File)
		{
			for (auto p: Projects)
			{
				if (p->GetFileName() && stricmp(p->GetFileName(), File) == 0)
				{
					return p;
				}
			}
		}

		return 0;
	}

	void SerializeStringList(const char *Opt, LString::Array *Lst, bool Write)
	{
		LVariant v;
		LString Sep = OptFileSeparator;
		if (Write)
		{
			if (Lst->Length() > 0)
			{
				auto s = Sep.Join(*Lst);
				Options.SetValue(Opt, v = s.Get());
				// printf("Saving '%s' to %s\n", s.Get(), Opt);
			}
			else
				Options.DeleteValue(Opt);
		}
		else if (Options.GetValue(Opt, v))
		{
			auto files = LString(v.Str()).Split(Sep);
			Lst->Length(0);
			for (auto f: files)
				if (f.Length() > 0)
					Lst->Add(f);
			// printf("Reading '%s' to %s, file.len=%i %s\n", v.Str(), Opt, (int)files.Length(), v.Str());
		}
		// else printf("%s:%i - No option '%s' to read.\n", _FL, Opt);
	}
};

#if 0// def LGI_COCOA
#define Chk printf("%s:%i - Cnt=%i\n", LGetLeaf(__FILE__), __LINE__, (int)WindowHandle().p.retainCount)
#else
#define Chk
#endif

AppWnd::AppWnd()
{
	#ifdef __GTK_H__
	LgiGetResObj(true, AppName);
	#endif
	
Chk;

	LRect r(0, 0, 1300, 900);
	#ifdef BEOS
	r.Offset(GdcD->X() - r.X() - 10, GdcD->Y() - r.Y() - 10);
	SetPos(r);
	#else
	SetPos(r);
Chk;
	MoveToCenter();
	#endif

Chk;

	d = new AppWndPrivate(this);

	Name(AppName);
	SetQuitOnClose(true);

Chk;

	#if WINNATIVE
	SetIcon((char*)MAKEINTRESOURCE(IDI_APP));
	#else
	SetIcon("icon64.png");
	#endif
Chk;

	if (!Attach(0))
	{
		LgiTrace("%s:%i - Attach failed.\n", _FL);
	}
	else
	{
Chk;
		Menu = new LMenu;
		if (Menu)
		{
			Menu->Attach(this);
			bool Loaded = Menu->Load(this, "IDM_MENU");
			LAssert(Loaded);
			if (Loaded)
			{
				Menu->SetPrefAndAboutItems(IDM_OPTIONS, IDM_ABOUT);
				d->RecentFilesMenu = Menu->FindSubMenu(IDM_RECENT_FILES);
				d->RecentProjectsMenu = Menu->FindSubMenu(IDM_RECENT_PROJECTS);
				d->WindowsMenu = Menu->FindSubMenu(IDM_WINDOW_LST);
				d->CreateMakefileMenu = Menu->FindSubMenu(IDM_CREATE_MAKEFILE);
				if (d->CreateMakefileMenu)
				{
					d->CreateMakefileMenu->Empty();
					for (int i=0; PlatformNames[i]; i++)
					{
						d->CreateMakefileMenu->AppendItem(PlatformNames[i], IDM_MAKEFILE_BASE + i);
					}
				}
				else LgiTrace("%s:%i - FindSubMenu failed.\n", _FL);

				LMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
				if (Debug)
				{
					Debug->Checked(true);
				}
				else LgiTrace("%s:%i - FindSubMenu failed.\n", _FL);
				
				d->UpdateMenus();
			}
		}

Chk;
		LToolBar *Tools = NULL;
		if (GdcD->Y() > 1200)
			Tools = LgiLoadToolbar(this, "cmds-32px.png", 32, 32);
		else
			Tools = LgiLoadToolbar(this, "cmds-16px.png", 16, 16);
		
		if (Tools)
		{
			Tools->AppendButton("New", IDM_NEW, TBT_PUSH, true, CMD_NEW);
			Tools->AppendButton("Open", IDM_OPEN, TBT_PUSH, true, CMD_OPEN);
			Tools->AppendButton("Save", IDM_SAVE_ALL, TBT_PUSH, true, CMD_SAVE_ALL);
			Tools->AppendSeparator();
			Tools->AppendButton("Cut", IDM_CUT, TBT_PUSH, true, CMD_CUT);
			Tools->AppendButton("Copy", IDM_COPY, TBT_PUSH, true, CMD_COPY);
			Tools->AppendButton("Paste", IDM_PASTE, TBT_PUSH, true, CMD_PASTE);
			Tools->AppendSeparator();

			Tools->AppendButton("Compile", IDM_COMPILE, TBT_PUSH, true, CMD_COMPILE);
			Tools->AppendButton("Build", IDM_BUILD, TBT_PUSH, true, CMD_BUILD);
			Tools->AppendButton("Stop", IDM_STOP_BUILD, TBT_PUSH, true, CMD_STOP_BUILD);
			// Tools->AppendButton("Execute", IDM_EXECUTE, TBT_PUSH, true, CMD_EXECUTE);
			
			Tools->AppendSeparator();
			Tools->AppendButton("Debug", IDM_START_DEBUG, TBT_PUSH, true, CMD_DEBUG);
			Tools->AppendButton("Pause", IDM_PAUSE_DEBUG, TBT_PUSH, true, CMD_PAUSE);
			Tools->AppendButton("Restart", IDM_RESTART_DEBUGGING, TBT_PUSH, true, CMD_RESTART);
			Tools->AppendButton("Kill", IDM_STOP_DEBUG, TBT_PUSH, true, CMD_KILL);
			Tools->AppendButton("Step Into", IDM_STEP_INTO, TBT_PUSH, true, CMD_STEP_INTO);
			Tools->AppendButton("Step Over", IDM_STEP_OVER, TBT_PUSH, true, CMD_STEP_OVER);
			Tools->AppendButton("Step Out", IDM_STEP_OUT, TBT_PUSH, true, CMD_STEP_OUT);
			Tools->AppendButton("Run To", IDM_RUN_TO, TBT_PUSH, true, CMD_RUN_TO);

			Tools->AppendSeparator();
			Tools->AppendButton("Find In Files", IDM_FIND_IN_FILES, TBT_PUSH, true, CMD_FIND_IN_FILES);
			
			Tools->GetCss(true)->Padding("4px");
			Tools->Attach(this);
		}
		else LgiTrace("%s:%i - No tools obj?", _FL);

Chk;
		LVariant v = 270, OutPx = 250;
		d->Options.GetValue(OPT_SPLIT_PX, v);
		d->Options.GetValue(OPT_OUTPUT_PX, OutPx);

		AddView(d->VBox = new LBox);
		d->VBox->SetVertical(true);

		d->HBox = new LBox;
		d->VBox->AddView(d->HBox);
		d->VBox->AddView(d->Output = new IdeOutput(this));

		d->HBox->AddView(d->Tree = new IdeTree);
		if (d->Tree)
		{
			d->Tree->SetImageList(d->Icons, false);
			d->Tree->Sunken(false);
		}
		d->HBox->AddView(d->Mdi = new GMdiParent);
		if (d->Mdi)
		{
			d->Mdi->HasButton(true);
		}

Chk;
		d->HBox->Value(MAX(v.CastInt32(), 20));

		LRect c = GetClient();
		if (c.Y() > OutPx.CastInt32())
		{
			auto Px = OutPx.CastInt32();
			LCss::Len y(LCss::LenPx, (float)MAX(Px, 120));
			d->Output->GetCss(true)->Height(y);
		}

		AttachChildren();
		OnPosChange();
	
Chk;
		#ifdef LINUX
		auto f = LFindFile("lgiide.png");
		if (f)
		{
			// Handle()->setIcon(f);
		}
		#endif
		
		UpdateState();
		
Chk;
		Visible(true);
		DropTarget(true);

Chk;
		SetPulse(1000);
	}
	
	#ifdef LINUX
	LFinishXWindowsStartup(this);
	#endif
	
	#if USE_HAIKU_PULSE_HACK
	if (d->Output)
		d->Output->SetPulse(1000);
	#endif
Chk;
}

AppWnd::~AppWnd()
{
	if (d->HBox)
	{
		LVariant v = d->HBox->Value();
		d->Options.SetValue(OPT_SPLIT_PX, v);
	}
	if (d->Output)
	{
		LVariant v = d->Output->Y();
		d->Options.SetValue(OPT_OUTPUT_PX, v);
	}

	ShutdownFtpThread();

	CloseAll();
	
	LAppInst->AppWnd = 0;
	DeleteObj(d);
}

void AppWnd::OnPulse()
{
	IdeDoc *Top = TopDoc();
	if (Top)
		Top->OnPulse();

	if (d->FixBuildWait)
	{
		d->FixBuildWait = false;
		if (OnFixBuildErrors() > 0)
			d->RebuildWait = 3;
	}
	else if (d->RebuildWait > 0)
	{
		if (--d->RebuildWait == 0)
			Build();
	}
}

GDebugContext *AppWnd::GetDebugContext()
{
	return d->DbgContext;
}

struct DumpBinThread : public LThread
{
	LStream *Out;
	LString InFile;
	bool IsLib;

public:
	DumpBinThread(LStream *out, LString file) : LThread("DumpBin.Thread")
	{
		Out = out;
		InFile = file;
		DeleteOnExit = true;

		auto Ext = LGetExtension(InFile);
		IsLib = Ext && !stricmp(Ext, "lib");

		Run();
	}

	bool DumpBin(LString Args, LStream *Str)
	{
		char Buf[256];
		ssize_t Rd;

		const char *Prog = "c:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\dumpbin.exe";
		LSubProcess s(Prog, Args);
		if (!s.Start(true, false))
		{
			Out->Print("%s:%i - '%s' doesn't exist.\n", _FL, Prog);
			return false;
		}

		while ((Rd = s.Read(Buf, sizeof(Buf))) > 0)
			Str->Write(Buf, Rd);

		return true;
	}

	LString::Array Dependencies(const char *Executable, int Depth = 0)
	{
		LString Args;
		LStringPipe p;
		Args.Printf("/dependents \"%s\"", Executable);
		DumpBin(Args, &p);

		char Spaces[256];
		int Len = Depth * 2;
		memset(Spaces, ' ', Len);
		Spaces[Len] = 0;

		LString::Array Files;
		auto Parts = p.NewGStr().Replace("\r", "").Split("\n\n");
		if (Parts.Length() > 0)
		{
			Files = Parts[4].Strip().Split("\n");
			auto Path = LGetPath();
			for (size_t i=0; i<Files.Length(); i++)
			{
				auto &f = Files[i];
				f = f.Strip();

				bool Found = false;
				for (auto s : Path)
				{
					LFile::Path c(s);
					c += f.Get();
					if (c.IsFile())
					{
						f = c.GetFull();
						Found = true;
						break;
					}
				}

				if (!Found)
					f += " (not found in path)";
				else
				{
					if (!stristr(f, "\\system32\\") &&
						!stristr(f, "\\Windows Kits\\") &&
						!stristr(f, "vcruntime"))
					{
						auto Deps = Dependencies(f, Depth + 1);
						Files.SetFixedLength(false);
						for (auto s: Deps)
						{
							LString p;
							p.Printf("%s%s", Spaces, s.Get());
							Files.AddAt(++i, p);
						}
					}
				}
			}
		}

		return Files;
	}

	LString GetArch()
	{
		LString Args;
		LStringPipe p;
		Args.Printf("/headers \"%s\"", InFile.Get());
		DumpBin(Args, &p);
	
		const char *Key = " machine ";
		LString Arch;
		auto Lines = p.NewGStr().SplitDelimit("\r\n");
		int64 Machine = 0;
		for (auto &Ln : Lines)
		{
			if (Ln.Find(Key) >= 0)
			{
				auto p = Ln.Strip().Split(Key);
				if (p.Length() == 2)
				{
					Arch = p[1].Strip("()");
					Machine = p[0].Int(16);
				}
			}
		}

		if (Machine == 0x14c)
			Arch += " 32bit";
		else if (Machine == 0x200)
			Arch += " 64bit Itanium";
		else if (Machine == 0x8664)
			Arch += " 64bit";

		return Arch;
	}

	LString GetExports()
	{
		LString Args;
		LStringPipe p;

		if (IsLib)
			Args.Printf("/symbols \"%s\"", InFile.Get());
		else
			Args.Printf("/exports \"%s\"", InFile.Get());
		DumpBin(Args, &p);
	
		LString Exp;

		auto Sect = p.NewGStr().Replace("\r", "").Split("\n\n");

		if (IsLib)
		{
			LString::Array Lines, Funcs;
			for (auto &s : Sect)
			{
				if (s.Find("COFF", 0, 100) == 0)
				{
					Lines = s.Split("\n");
					break;
				}
			}

			Funcs.SetFixedLength(false);
			for (auto &l : Lines)
			{
				if (l.Length() < 34) continue;
				const char *Type = l.Get() + 33;
				if (!Strnicmp(Type, "External", 8))
				{
					auto Nm = l.RSplit("|",1).Last().Strip();
					if (!strchr("@$.?", Nm(0)))
						Funcs.New() = Nm;
				}
			}

			Exp = LString("\n").Join(Funcs);
		}
		else
		{
			bool Ord = false;
			for (auto &s : Sect)
			{
				if (s.Strip().Find("ordinal") == 0)
					Ord = true;
				else if (Ord)
				{
					Exp = s;
					break;
				}
				else Ord = false;
			}
		}

		return Exp;
	}

	int Main()
	{
		if (!IsLib)
		{
			auto Deps = Dependencies(InFile);
			if (Deps.Length())
				Out->Print("Dependencies:\n\t%s\n\n", LString("\n\t").Join(Deps).Get());
		}

		auto Arch = GetArch();
		if (Arch)
			Out->Print("Arch: %s\n\n", Arch.Get());

		auto Exp = GetExports();
		if (Arch)
			Out->Print("Exports:\n%s\n\n", Exp.Get());
		return 0;
	}
};

void AppWnd::OnReceiveFiles(LArray<const char*> &Files)
{
	for (int i=0; i<Files.Length(); i++)
	{
		auto f = Files[i];
		
		auto ext = LGetExtension(f);
		if (ext && !stricmp(ext, "mem"))
		{
			NewMemDumpViewer(this, f);
			return;
		}
		else if (ext && !stricmp(ext, "xml"))
		{
			if (!OpenProject(f, NULL))
				OpenFile(f);
		}
		else if (LDirExists(f))
			;
		else if
		(
			LIsFileNameExecutable(f)
			||
			(ext != NULL && !stricmp(ext, "lib"))
		)
		{
			// dumpbin /exports csp.dll
			LFile::Path Docs(LSP_USER_DOCUMENTS);
			LString Name;
			Name.Printf("%s.txt", Files[i]);
			Docs += Name;
			IdeDoc *Doc = NewDocWnd(NULL, NULL);
			if (Doc)
			{
				Doc->SetFileName(Docs, false);
				new DumpBinThread(Doc, Files[i]);
			}
		}
		else
		{
			OpenFile(f);
		}
	}
	
	Raise();
	
	if (LAppInst->GetOption("createMakeFiles"))
	{
		IdeProject *p = RootProject();
		if (p)
		{
			p->CreateMakefile(PlatformCurrent, false);
		}
	}
}

void AppWnd::OnDebugState(bool Debugging, bool Running)
{
	// Make sure this event is processed in the GUI thread.
	#if DEBUG_SESSION_LOGGING
	LgiTrace("AppWnd::OnDebugState(%i,%i) InThread=%i\n", Debugging, Running, InThread());
	#endif
	
	PostEvent(M_DEBUG_ON_STATE, Debugging, Running);
}

bool IsVarChar(LString &s, ssize_t pos)
{
	if (pos < 0)
		return false;
	if (pos >= s.Length())
		return false;
	char i = s[pos];
	return IsAlpha(i) || IsDigit(i) || i == '_';
}

bool ReplaceWholeWord(LString &Ln, LString Word, LString NewWord)
{
	ssize_t Pos = 0;
	bool Status = false;

	while (Pos >= 0)
	{
		Pos = Ln.Find(Word, Pos);
		if (Pos < 0)
			return Status;

		ssize_t End = Pos + Word.Length();
		if (!IsVarChar(Ln, Pos-1) && !IsVarChar(Ln, End))
		{
			LString NewLn = Ln(0,Pos) + NewWord + Ln(End,-1);
			Ln = NewLn;
			Status = true;
		}

		Pos++;
	}

	return Status;
}

struct LFileInfo
{
	LString Path;
	LString::Array Lines;
	bool Dirty;

	LFileInfo()
	{
		Dirty = false;
	}

	bool Save()
	{
		LFile f;
		if (!f.Open(Path, O_WRITE))
			return false;

		LString NewFile = LString("\n").Join(Lines);

		f.SetSize(0);
		f.Write(NewFile);

		f.Close();
		return true;
	}
};


int AppWnd::OnFixBuildErrors()
{
	LHashTbl<StrKey<char>, LString> Map;
	LVariant v;
	if (GetOptions()->GetValue(OPT_RENAMED_SYM, v))
	{
		auto Lines = LString(v.Str()).Split("\n");
		for (auto Ln: Lines)
		{
			auto p = Ln.SplitDelimit();
			if (p.Length() == 2)
				Map.Add(p[0], p[1]);
		}
	}

	LString Raw = d->Output->Txt[AppWnd::BuildTab]->Name();
	LString::Array Lines = Raw.Split("\n");
	auto *Log = d->Output->Txt[AppWnd::OutputTab];

	Log->Name(NULL);
	Log->Print("Parsing errors...\n");
	int Replacements = 0;
	LArray<LFileInfo> Files;
	LHashTbl<StrKey<char>,bool> FixHistory;

	for (int Idx=0; Idx<Lines.Length(); Idx++)
	{
		auto Ln = Lines[Idx];
		auto ErrPos = Ln.Find("error");
		if (ErrPos >= 0)
		{
			#ifdef WINDOWS
			LString::Array p = Ln.SplitDelimit(">()");
			#else
			LString::Array p = Ln(0, ErrPos).Strip().SplitDelimit(":");
			#endif
			if (p.Length() <= 2)
			{
				Log->Print("Error: Only %i parts? '%s'\n", (int)p.Length(), Ln.Get());
			}
			else
			{
				#ifdef WINDOWS
				int Base = p[0].IsNumeric() ? 1 : 0;
				LString Fn = p[Base];
				if (Fn.Find("Program Files") >= 0)
				{
					Log->Print("Is prog file\n");
					continue;
				}
				auto LineNo = p[Base+1].Int();
				bool FileNotFound = Ln.Find("Cannot open include file:") > 0;
				#else
				LString Fn = p[0];
				auto LineNo = p[1].Int();
				bool FileNotFound = false; // fixme
				#endif

				LAutoString Full;
				if (!d->FindSource(Full, Fn, NULL))
				{
					Log->Print("Error: Can't find Fn='%s' Line=%i\n", Fn.Get(), (int)LineNo);
					continue;
				}

				LFileInfo *Fi = NULL;
				for (auto &i: Files)
				{
					if (i.Path.Equals(Full))
					{
						Fi = &i;
						break;
					}
				}
				if (!Fi)
				{
					LFile f(Full, O_READ);
					if (f.IsOpen())
					{
						Fi = &Files.New();
						Fi->Path = Full.Get();
						auto OldFile = f.Read();
						Fi->Lines = OldFile.SplitDelimit("\n", -1, false);
					}
					else
					{
						Log->Print("Error: Can't open '%s'\n", Full.Get());
					}
				}

				if (Fi)
				{
					LString Loc;
					Loc.Printf("%s:%i", Full.Get(), (int)LineNo);
					if (FixHistory.Find(Loc))
					{
						// Log->Print("Already fixed %s\n", Loc.Get());
					}
					else if (LineNo <= Fi->Lines.Length())
					{
						FixHistory.Add(Loc, true);

						if (FileNotFound)
						{
							auto n = p.Last().SplitDelimit("\'");
							auto wrongName = n[1];
							LFile f(Full, O_READ);
							auto Lines = f.Read().SplitDelimit("\n", -1, false);
							f.Close();
							if (LineNo <= Lines.Length())
							{
								auto &errLine = Lines[LineNo-1];
								auto Pos = errLine.Find(wrongName);
								
								/*
								if (Pos < 0)
								{
									for (int i=0; i<Lines.Length(); i++)
										Log->Print("[%i]=%s\n", i, Lines[i].Get());
								}
								*/

								if (Pos > 0)
								{
									// Find where it went...
									LString newPath;
									for (auto p: d->Projects)
									{
										const char *SubStr[] = { ".", "lgi/common" };

										LArray<LString> IncPaths;
										if (p->BuildIncludePaths(IncPaths, true, false, PlatformCurrent))
										{
											for (auto &inc: IncPaths)
											{
												for (int sub=0; !newPath && sub<CountOf(SubStr); sub++)
												{
													char path[MAX_PATH];
													LMakePath(path, sizeof(path), inc, SubStr[sub]);
													LMakePath(path, sizeof(path), path, wrongName);
													if (LFileExists(path))
													{
														newPath = path + inc.Length() + 1;
													}
													else
													{
														LString minusFirst = wrongName(1,-1);
														LMakePath(path, sizeof(path), inc, SubStr[sub]);
														LMakePath(path, sizeof(path), path, minusFirst);
														if (LFileExists(path))
														{
															newPath = path + inc.Length() + 1;
														}
													}
												}
											}
										}
									}

									if (newPath)
									{
										newPath = newPath.Replace("\\", "/"); // Use unix dir chars
										LString newLine = errLine(0, Pos) + newPath + errLine(Pos + wrongName.Length(), -1);
										if (newLine == errLine)
										{
											Log->Print("Already changed '%s'.\n", wrongName.Get()); 
										}
										else
										{
											LString backup = LString(Full.Get()) + ".orig";
											if (LFileExists(backup))
												FileDev->Delete(backup);
											LError Err;
											if (FileDev->Move(Full, backup, &Err))
											{
												errLine = newLine;
												LString newLines = LString("\n").Join(Lines);
												LFile out(Full, O_WRITE);
												out.Write(newLines);
												Log->Print("Fixed '%s'->'%s' on ln %i in %s\n",
													wrongName.Get(), newPath.Get(),
													(int)LineNo, Full.Get());
												Replacements++;
											}
											else Log->Print("Error: moving '%s' to backup (%s).\n", Full.Get(), Err.GetMsg().Get());
										}
									}
									else Log->Print("Error: Missing header '%s'.\n", wrongName.Get()); 
								}
								else
								{
									Log->Print("Error: '%s' not found in line %i of '%s' -> '%s'\n",
										wrongName.Get(), (int)LineNo,
										Fn.Get(), Full.Get());
									// return;
								}
							}
							else Log->Print("Error: Line %i is beyond file lines: %i\n", (int)LineNo, (int)Lines.Length());
						}
						else
						{
							auto OldReplacements = Replacements;
							for (auto i: Map)
							{
								for (int Offset = 0; (LineNo + Offset >= 1) && Offset >= -1; Offset--)
								{
									LString &s = Fi->Lines[LineNo+Offset-1];
									if (ReplaceWholeWord(s, i.key, i.value))
									{
										Log->Print("Renamed '%s' -> '%s' at %s:%i\n", i.key, i.value.Get(), Full.Get(), LineNo+Offset);
										Fi->Dirty = true;
										Replacements++;
										Offset = -2;
									}
								}
							}

							if (OldReplacements == Replacements &&
								Ln.Find("syntax error: id") > 0)
							{
								Log->Print("Unhandled: %s\n", Ln.Get());
							}
						}
					}
					else
					{
						Log->Print("Error: Invalid line %i\n", (int)LineNo);
					}
				}
				else
				{
					Log->Print("Error: Fi is NULL\n");
				}
			}
		}
	}

	for (auto &Fi : Files)
	{
		if (Fi.Dirty)
			Fi.Save();
	}

	Log->Print("%i replacements made.\n", Replacements);
	if (Replacements > 0)
		d->Output->Value(AppWnd::OutputTab);

	return Replacements;
}

void AppWnd::OnBuildStateChanged(bool NewState)
{
	LVariant v;
	if (!NewState &&
		GetOptions()->GetValue(OPT_FIX_RENAMED, v) &&
		v.CastInt32())
	{
		d->FixBuildWait = true;
	}
}

void AppWnd::UpdateState(int Debugging, int Building)
{
	// printf("UpdateState %i %i\n", Debugging, Building);
	
	if (Debugging >= 0) d->Debugging = Debugging;
	if (Building >= 0)
	{
		if (d->Building != (Building != 0))
			OnBuildStateChanged(Building);
		d->Building = Building;
	}

	SetCtrlEnabled(IDM_COMPILE, !d->Building);
	SetCtrlEnabled(IDM_BUILD, !d->Building);
	SetCtrlEnabled(IDM_STOP_BUILD, d->Building);
	// SetCtrlEnabled(IDM_RUN, !d->Building);
	// SetCtrlEnabled(IDM_TOGGLE_BREAKPOINT, !d->Building);
	
	SetCtrlEnabled(IDM_START_DEBUG, !d->Debugging && !d->Building);
	SetCtrlEnabled(IDM_PAUSE_DEBUG, d->Debugging);
	SetCtrlEnabled(IDM_RESTART_DEBUGGING, d->Debugging);
	SetCtrlEnabled(IDM_STOP_DEBUG, d->Debugging);
	SetCtrlEnabled(IDM_STEP_INTO, d->Debugging);
	SetCtrlEnabled(IDM_STEP_OVER, d->Debugging);
	SetCtrlEnabled(IDM_STEP_OUT, d->Debugging);
	SetCtrlEnabled(IDM_RUN_TO, d->Debugging);
}

void AppWnd::AppendOutput(char *Txt, AppWnd::Channels Channel)
{
	if (!d->Output)
	{
		LgiTrace("%s:%i - No output panel.\n", _FL);
		return;
	}
	
	if (Channel < 0 ||
		Channel >= CountOf(d->Output->Txt))
	{
		LgiTrace("%s:%i - Channel range: %i, %i.\n", _FL, Channel, CountOf(d->Output->Txt));
		return;
	}

	if (!d->Output->Txt[Channel])
	{
		LgiTrace("%s:%i - No log for channel %i.\n", _FL, Channel);
		return;
	}
	
	if (Txt)
	{
		d->Output->Buf[Channel].Add(Txt, strlen(Txt));
	}
	else
	{
		auto Ctrl = d->Output->Txt[Channel];
		Ctrl->UnSelectAll();
		Ctrl->Name("");
	}
}

void AppWnd::SaveAll()
{
	List<IdeDoc>::I Docs = d->Docs.begin();
	for (IdeDoc *Doc = *Docs; Doc; Doc = *++Docs)
	{
		Doc->SetClean();
		d->OnFile(Doc->GetFileName());
	}
	
	List<IdeProject>::I Projs = d->Projects.begin();
	for (IdeProject *Proj = *Projs; Proj; Proj = *++Projs)
	{
		Proj->SetClean();
		d->OnFile(Proj->GetFileName(), true);
	}
}

void AppWnd::CloseAll()
{
	SaveAll();
	while (d->Docs[0])
		delete d->Docs[0];
	
	IdeProject *p = RootProject();
	if (p)
		DeleteObj(p);
	
	while (d->Projects[0])
		delete d->Projects[0];	

	DeleteObj(d->DbgContext);
}

bool AppWnd::OnRequestClose(bool IsClose)
{
	SaveAll();
	return LWindow::OnRequestClose(IsClose);
}

bool AppWnd::OnBreakPoint(GDebugger::BreakPoint &b, bool Add)
{
	List<IdeDoc>::I it = d->Docs.begin();

	for (IdeDoc *doc = *it; doc; doc = *++it)
	{
		auto fn = doc->GetFileName();
		bool Match = !Stricmp(fn, b.File.Get());
		if (Match)
			doc->AddBreakPoint(b.Line, Add);
	}

	if (d->DbgContext)
		d->DbgContext->OnBreakPoint(b, Add);
	
	return true;
}

bool AppWnd::LoadBreakPoints(IdeDoc *doc)
{
	if (!doc)
		return false;

	auto fn = doc->GetFileName();
	for (int i=0; i<d->BreakPoints.Length(); i++)
	{
		GDebugger::BreakPoint &b = d->BreakPoints[i];
		if (!_stricmp(fn, b.File))
		{
			doc->AddBreakPoint(b.Line, true);
		}
	}

	return true;
}

bool AppWnd::LoadBreakPoints(GDebugger *db)
{
	if (!db)
		return false;

	for (int i=0; i<d->BreakPoints.Length(); i++)
	{
		GDebugger::BreakPoint &bp = d->BreakPoints[i];
		db->SetBreakPoint(&bp);
	}

	return true;
}

bool AppWnd::ToggleBreakpoint(const char *File, ssize_t Line)
{
	bool DeleteBp = false;

	for (int i=0; i<d->BreakPoints.Length(); i++)
	{
		GDebugger::BreakPoint &b = d->BreakPoints[i];
		if (!_stricmp(File, b.File) &&
			b.Line == Line)
		{
			OnBreakPoint(b, false);
			d->BreakPoints.DeleteAt(i);
			DeleteBp = true;
			break;
		}
	}
	
	if (!DeleteBp)
	{
		GDebugger::BreakPoint &b = d->BreakPoints.New();
		b.File = File;
		b.Line = Line;
		OnBreakPoint(b, true);
	}
	
	return true;
}

void AppWnd::DumpHistory()
{
	#if 0
	LgiTrace("History %i of %i\n", d->HistoryLoc, d->CursorHistory.Length());
	for (int i=0; i<d->CursorHistory.Length(); i++)
	{
		FileLoc &p = d->CursorHistory[i];		
		LgiTrace("    [%i] = %s, %i %s\n", i, p.File.Get(), p.Line, d->HistoryLoc == i ? "<-----":"");
	}
	#endif
}

/*
void CheckHistory(LArray<FileLoc> &CursorHistory)
{
	if (CursorHistory.Length() > 0)
	{
		FileLoc *loc = &CursorHistory[0];
		for (unsigned i=CursorHistory.Length(); i<CursorHistory.GetAlloc(); i++)
		{
			if ((NativeInt)loc[i].File.Get() == 0xcdcdcdcdcdcdcdcd)
			{
				int asd=0;
			}
		}
	}
}
*/

void AppWnd::OnLocationChange(const char *File, int Line)
{
	if (!File)
		return;

	if (!d->InHistorySeek)
	{
		if (d->CursorHistory.Length() > 0)
		{
			FileLoc &Last = d->CursorHistory.Last();
			if (_stricmp(File, Last.File) == 0 && abs(Last.Line - Line) <= 1)
			{
				// Previous or next line... just update line number
				Last.Line = Line;
				DumpHistory();
				return;
			}

			// Add new entry
			d->HistoryLoc++;

			FileLoc &loc = d->CursorHistory[d->HistoryLoc];
			#ifdef WIN64
			if ((NativeInt)loc.File.Get() == 0xcdcdcdcdcdcdcdcd)
				LAssert(0); // wtf?
			else
			#endif
				loc.Set(File, Line);
		}
		else
		{
			// Add new entry
			d->CursorHistory[0].Set(File, Line);
		}

		// Destroy any history after the current...
		d->CursorHistory.Length(d->HistoryLoc+1);

		DumpHistory();
	}
}

void AppWnd::OnFile(char *File, bool IsProject)
{
	d->OnFile(File, IsProject);
}

IdeDoc *AppWnd::NewDocWnd(const char *FileName, NodeSource *Src)
{
	IdeDoc *Doc = new IdeDoc(this, Src, 0);
	if (Doc)
	{
		d->Docs.Insert(Doc);

		LRect p = d->Mdi->NewPos();
		Doc->LView::SetPos(p);
		Doc->Attach(d->Mdi);
		Doc->Focus(true);
		Doc->Raise();

		if (FileName)
			d->OnFile(FileName);
	}

	return Doc;
}

IdeDoc *AppWnd::GetCurrentDoc()
{
	if (d->Mdi)
		return dynamic_cast<IdeDoc*>(d->Mdi->GetTop());
	return NULL;
}

IdeDoc *AppWnd::GotoReference(const char *File, int Line, bool CurIp, bool WithHistory)
{
	if (!WithHistory)
		d->InHistorySeek = true;

	IdeDoc *Doc = File ? OpenFile(File) : GetCurrentDoc();
	if (Doc)
	{
		Doc->SetLine(Line, CurIp);
		Doc->Focus(true);
	}

	if (!WithHistory)
		d->InHistorySeek = false;

	return Doc;			
}

IdeDoc *AppWnd::FindOpenFile(char *FileName)
{
	List<IdeDoc>::I it = d->Docs.begin();
	for (IdeDoc *i=*it; i; i=*++it)
	{
		auto f = i->GetFileName();
		if (f)
		{
			IdeProject *p = i->GetProject();
			if (p)
			{
				LAutoString Base = p->GetBasePath();
				if (Base)
				{
					char Path[MAX_PATH];
					if (*f == '.')
						LMakePath(Path, sizeof(Path), Base, f);
					else
						strcpy_s(Path, sizeof(Path), f);

					if (stricmp(Path, FileName) == 0)
						return i;
				}
			}
			else
			{
				if (stricmp(f, FileName) == 0)
					return i;
			}
		}
	}

	return 0;
}

IdeDoc *AppWnd::OpenFile(const char *FileName, NodeSource *Src)
{
	static bool DoingProjectFind = false;
	IdeDoc *Doc = 0;
	
	const char *File = Src ? Src->GetFileName() : FileName;
	if (!Src && !ValidStr(File))
	{
		LgiTrace("%s:%i - No source or file?\n", _FL);
		return NULL;
	}
	
	LString FullPath;
	if (LIsRelativePath(File))
	{
		IdeProject *Proj = Src && Src->GetProject() ? Src->GetProject() : RootProject();
		if (Proj)
		{
			List<IdeProject> Projs;
			Projs.Insert(Proj);
			Proj->CollectAllSubProjects(Projs);

			for (auto Project : Projs)
			{
				auto ProjPath = Project->GetBasePath();
				char p[MAX_PATH];
				LMakePath(p, sizeof(p), ProjPath, File);
				LString Path = p;
				if (Project->CheckExists(Path))
				{
					FullPath = Path;
					File = FullPath;
					break;
				}
			}
		}
	}
		
	Doc = d->IsFileOpen(File);
	if (!Doc)
	{
		if (Src)
		{
			Doc = NewDocWnd(File, Src);
		}
		else if (!DoingProjectFind)
		{
			DoingProjectFind = true;
			List<IdeProject>::I Proj = d->Projects.begin();
			for (IdeProject *p=*Proj; p && !Doc; p=*++Proj)
			{
				p->InProject(LIsRelativePath(File), File, true, &Doc);				
			}
			DoingProjectFind = false;

			d->OnFile(File);
		}
	}

	if (!Doc && LFileExists(File))
	{
		Doc = new IdeDoc(this, 0, File);
		if (Doc)
		{
			Doc->OpenFile(File);

			LRect p = d->Mdi->NewPos();
			Doc->LView::SetPos(p);
			d->Docs.Insert(Doc);
			d->OnFile(File);
		}
	}

	if (Doc)
	{
		Doc->SetEditorParams(4, 4, true, false);
		if (!Doc->IsAttached())
		{
			Doc->Attach(d->Mdi);
		}

		Doc->Focus(true);
		Doc->Raise();
	}
	
	return Doc;
}

IdeProject *AppWnd::RootProject()
{
	IdeProject *Root = 0;
	
	for (auto p: d->Projects)
	{
		if (!p->GetParentProject())
		{
			LAssert(Root == 0);
			Root = p;
		}
	}
	
	return Root;
}

IdeProject *AppWnd::OpenProject(const char *FileName, IdeProject *ParentProj, bool Create, bool Dep)
{	
	if (!FileName)
	{
		LgiTrace("%s:%i - Error: No filename.\n", _FL);
		return NULL;
	}
	
	if (d->IsProjectOpen(FileName))
	{
		LgiTrace("%s:%i - Warning: Project already open.\n", _FL);
		return NULL;
	}
	

	IdeProject *p = new IdeProject(this);
	if (!p)
	{
		LgiTrace("%s:%i - Error: mem alloc.\n", _FL);
		return NULL;
	}
	
	LString::Array Inc;
	p->BuildIncludePaths(Inc, false, false, PlatformCurrent);
	d->FindSym->SetIncludePaths(Inc);

	p->SetParentProject(ParentProj);
	
	ProjectStatus Status = p->OpenFile(FileName);
	if (Status == OpenOk)
	{
		d->Projects.Insert(p);
		d->OnFile(FileName, true);
		
		if (!Dep)
		{
			auto d = strrchr(FileName, DIR_CHAR);
			if (d++)
			{
				char n[256];
				sprintf(n, "%s [%s]", AppName, d);
				Name(n);						
			}
		}
	}
	else
	{
		LgiTrace("%s:%i - Failed to open '%s'\n", _FL, FileName);

		DeleteObj(p);
		if (Status == OpenError)
			d->RemoveRecent(FileName);
	}

	if (!GetTree()->Selection())
	{
		GetTree()->Select(GetTree()->GetChild());
	}

	GetTree()->Focus(true);

	return p;
}

LMessage::Result AppWnd::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case M_MAKEFILES_CREATED:
		{
			IdeProject *p = (IdeProject*)m->A();
			if (p)
				p->OnMakefileCreated();
			break;
		}
		case M_LAST_MAKEFILE_CREATED:
		{
			if (LAppInst->GetOption("exit"))
				LCloseApp();
			break;
		}
		case M_START_BUILD:
		{
			IdeProject *p = RootProject();
			if (p)
				p->Build(true, IsReleaseMode());
			else
				printf("%s:%i - No root project.\n", _FL);
			break;
		}
		case M_BUILD_DONE:
		{
			UpdateState(-1, false);
			
			IdeProject *p = RootProject();
			if (p)
				p->StopBuild();
			break;
		}
		case M_BUILD_ERR:
		{
			char *Msg = (char*)m->B();
			if (Msg)
			{
				d->Output->Txt[AppWnd::BuildTab]->Print("Build Error: %s\n", Msg);
				DeleteArray(Msg);
			}
			break;
		}
		case M_APPEND_TEXT:
		{
			LAutoString Text((char*) m->A());
			Channels Ch = (Channels) m->B();
			AppendOutput(Text, Ch);
			break;
		}
		case M_SELECT_TAB:
		{
			if (!d->Output)
				break;
			d->Output->Value(m->A());
			break;
		}
		case M_DEBUG_ON_STATE:
		{
			bool Debugging = m->A();
			bool Running = m->B();

			if (d->Running != Running)
			{
				bool RunToNotRun = d->Running && !Running;
				
				d->Running = Running;
				
				if (RunToNotRun &&
					d->Output &&
					d->Output->DebugTab)
				{
					d->Output->DebugTab->SendNotify(LNotifyValueChanged);
				}
			}
			if (d->Debugging != Debugging)
			{
				d->Debugging = Debugging;
				if (!Debugging)
				{
					IdeDoc::ClearCurrentIp();
					IdeDoc *c = GetCurrentDoc();
					if (c)
						c->UpdateControl();
						
					// Shutdown the debug context and free the memory
					DeleteObj(d->DbgContext);
				}
			}
			
			SetCtrlEnabled(IDM_START_DEBUG, !Debugging || !Running);
			SetCtrlEnabled(IDM_PAUSE_DEBUG, Debugging && Running);
			SetCtrlEnabled(IDM_RESTART_DEBUGGING, Debugging);
			SetCtrlEnabled(IDM_STOP_DEBUG, Debugging);

			SetCtrlEnabled(IDM_STEP_INTO, Debugging && !Running);
			SetCtrlEnabled(IDM_STEP_OVER, Debugging && !Running);
			SetCtrlEnabled(IDM_STEP_OUT, Debugging && !Running);
			SetCtrlEnabled(IDM_RUN_TO, Debugging && !Running);
			break;
		}
		default:
		{
			if (d->DbgContext)
				d->DbgContext->OnEvent(m);
			break;
		}
	}

	return LWindow::OnEvent(m);
}

bool AppWnd::OnNode(const char *Path, ProjectNode *Node, FindSymbolSystem::SymAction Action)
{
	// This takes care of adding/removing files from the symbol search engine.
	if (!Path || !Node)
		return false;

	d->FindSym->OnFile(Path, Action, Node->GetPlatforms());
	return true;
}

LOptionsFile *AppWnd::GetOptions()
{
	return &d->Options;
}

class Options : public LDialog
{
	AppWnd *App;
	LFontType Font;

public:
	Options(AppWnd *a)
	{
		SetParent(App = a);
		if (LoadFromResource(IDD_OPTIONS))
		{
			SetCtrlEnabled(IDC_FONT, false);
			MoveToCenter();
			
			if (!Font.Serialize(App->GetOptions(), OPT_EditorFont, false))
			{
				Font.GetSystemFont("Fixed");
			}			
			char s[256];
			if (Font.GetDescription(s, sizeof(s)))
			{
				SetCtrlName(IDC_FONT, s);
			}

			LVariant v;
			if (App->GetOptions()->GetValue(OPT_Jobs, v))
				SetCtrlValue(IDC_JOBS, v.CastInt32());
			else
				SetCtrlValue(IDC_JOBS, 2);
			
			DoModal();
		}
	}
	
	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDOK:
			{
				LVariant v;
				Font.Serialize(App->GetOptions(), OPT_EditorFont, true);
				App->GetOptions()->SetValue(OPT_Jobs, v = GetCtrlValue(IDC_JOBS));
			}
			case IDCANCEL:
			{
				EndModal(c->GetId());
				break;
			}
			case IDC_SET_FONT:
			{
				if (Font.DoUI(this))
				{
					char s[256];
					if (Font.GetDescription(s, sizeof(s)))
					{
						SetCtrlName(IDC_FONT, s);
					}
				}
				break;
			}
		}
		return 0;
	}
};

void AppWnd::UpdateMemoryDump()
{
	if (d->DbgContext)
	{
		const char *sWord = GetCtrlName(IDC_MEM_SIZE);
		int iWord = sWord ? atoi(sWord) : 1;
		int64 RowLen = GetCtrlValue(IDC_MEM_ROW_LEN);
		bool InHex = GetCtrlValue(IDC_MEM_HEX) != 0;

		d->DbgContext->FormatMemoryDump(iWord, (int)RowLen, InHex);
	}
}

int AppWnd::OnNotify(LViewI *Ctrl, LNotification n)
{
	switch (Ctrl->GetId())
	{
		case IDC_PROJECT_TREE:
		{
			if (n.Type == LNotifyDeleteKey)
			{
				ProjectNode *n = dynamic_cast<ProjectNode*>(d->Tree->Selection());
				if (n)
					n->Delete();
			}
			break;
		}
		case IDC_DEBUG_EDIT:
		{
			if (n.Type == LNotifyReturnKey && d->DbgContext)
			{
				const char *Cmd = Ctrl->Name();
				if (Cmd)
				{
					d->DbgContext->OnUserCommand(Cmd);
					Ctrl->Name(NULL);
				}
			}
			break;
		}
		case IDC_MEM_ADDR:
		{
			if (n.Type == LNotifyReturnKey)
			{
				if (d->DbgContext)
				{
					const char *s = Ctrl->Name();
					if (s)
					{
						auto sWord = GetCtrlName(IDC_MEM_SIZE);
						int iWord = sWord ? atoi(sWord) : 1;
						d->DbgContext->OnMemoryDump(s, iWord, (int)GetCtrlValue(IDC_MEM_ROW_LEN), GetCtrlValue(IDC_MEM_HEX) != 0);
					}
					else if (d->DbgContext->MemoryDump)
					{
						d->DbgContext->MemoryDump->Print("No address specified.");
					}
					else
					{
						LAssert(!"No MemoryDump.");
					}
				}
				else LAssert(!"No debug context.");
			}
			break;
		}
		case IDC_MEM_ROW_LEN:
		{
			if (n.Type == LNotifyReturnKey)
				UpdateMemoryDump();
			break;
		}
		case IDC_MEM_HEX:
		case IDC_MEM_SIZE:
		{
			UpdateMemoryDump();
			break;
		}
		case IDC_DEBUG_TAB:
		{
			if (d->DbgContext && n.Type == LNotifyValueChanged)
			{
				switch (Ctrl->Value())
				{
					case AppWnd::LocalsTab:
					{
						d->DbgContext->UpdateLocals();
						break;
					}
					case AppWnd::WatchTab:
					{
						d->DbgContext->UpdateWatches();
						break;
					}
					case AppWnd::RegistersTab:
					{
						d->DbgContext->UpdateRegisters();
						break;
					}
					case AppWnd::CallStackTab:
					{
						d->DbgContext->UpdateCallStack();
						break;
					}
					case AppWnd::ThreadsTab:
					{
						d->DbgContext->UpdateThreads();
						break;
					}
					default:
						break;
				}
			}
			break;
		}
		case IDC_LOCALS_LIST:
		{
			if (d->Output->Locals &&
				n.Type == LNotifyItemDoubleClick &&
				d->DbgContext)
			{
				LListItem *it = d->Output->Locals->GetSelected();
				if (it)
				{
					const char *Var = it->GetText(2);
					const char *Val = it->GetText(3);
					if (Var)
					{
						if (d->Output->DebugTab)
							d->Output->DebugTab->Value(AppWnd::ObjectTab);
							
						d->DbgContext->DumpObject(Var, Val);
					}
				}
			}
			break;
		}
		case IDC_CALL_STACK:
		{
			if (n.Type == LNotifyValueChanged)
			{
				if (d->Output->DebugTab)
					d->Output->DebugTab->Value(AppWnd::CallStackTab);
			}
			else if (n.Type == LNotifyItemSelect)
			{
				// This takes the user to a given call stack reference
				if (d->Output->CallStack && d->DbgContext)
				{
					LListItem *item = d->Output->CallStack->GetSelected();
					if (item)
					{
						LAutoString File;
						int Line;
						if (d->DbgContext->ParseFrameReference(item->GetText(1), File, Line))
						{
							LAutoString Full;
							if (d->FindSource(Full, File, NULL))
							{
								GotoReference(Full, Line, false);
								
								const char *sFrame = item->GetText(0);
								if (sFrame && IsDigit(*sFrame))
									d->DbgContext->SetFrame(atoi(sFrame));
							}
						}
					}
				}
			}
			break;
		}
		case IDC_WATCH_LIST:
		{
			WatchItem *Edit = NULL;
			switch (n.Type)
			{
				case LNotifyDeleteKey:
				{
					LArray<LTreeItem *> Sel;
					for (LTreeItem *c = d->Output->Watch->GetChild(); c; c = c->GetNext())
					{
						if (c->Select())
							Sel.Add(c);
					}
					Sel.DeleteObjects();
					break;
				}
				case LNotifyItemClick:
				{
					Edit = dynamic_cast<WatchItem*>(d->Output->Watch->Selection());
					break;
				}
				case LNotifyContainerClick:
				{
					// Create new watch.
					Edit = new WatchItem(d->Output);
					if (Edit)
						d->Output->Watch->Insert(Edit);
					break;
				}
				default:
					break;
			}
			
			if (Edit)
				Edit->EditLabel(0);
			break;
		}
		case IDC_THREADS:
		{
			if (n.Type == LNotifyItemSelect)
			{
				// This takes the user to a given thread
				if (d->Output->Threads && d->DbgContext)
				{
					LListItem *item = d->Output->Threads->GetSelected();
					if (item)
					{
						LString sId = item->GetText(0);
						int ThreadId = (int)sId.Int();
						if (ThreadId > 0)
						{
							d->DbgContext->SelectThread(ThreadId);
						}
					}
				}
			}
			break;
		}
	}
	
	return 0;
}

bool AppWnd::IsReleaseMode()
{
	LMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
	bool IsRelease = Release ? Release->Checked() : false;
	return IsRelease;
}

bool AppWnd::Build()
{
	SaveAll();
	
	IdeDoc *Top;
	IdeProject *p = RootProject();
	if (p)
	{		
		UpdateState(-1, true);

		LMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
		bool IsRelease = Release ? Release->Checked() : false;
		p->Build(false, IsRelease);

		return true;
	}
	else if ((Top = TopDoc()))
	{
		return Top->Build();
	}

	return false;
}

class RenameDlg : public LDialog
{
	AppWnd *App;

public:
	static RenameDlg *Inst;

	RenameDlg(AppWnd *a)
	{
		Inst = this;
		SetParent(App = a);
		MoveSameScreen(a);

		if (LoadFromResource(IDC_RENAME))
		{
			LVariant v;
			if (App->GetOptions()->GetValue(OPT_FIX_RENAMED, v))
				SetCtrlValue(IDC_FIX_RENAMED, v.CastInt32());
			if (App->GetOptions()->GetValue(OPT_RENAMED_SYM, v))
				SetCtrlName(IDC_SYM, v.Str());

			SetAlwaysOnTop(true);
			DoModeless();
		}
	}

	~RenameDlg()
	{
		Inst = NULL;
	}

	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDC_APPLY:
			{
				LVariant v;
				App->GetOptions()->SetValue(OPT_RENAMED_SYM, v = GetCtrlName(IDC_SYM));
				App->GetOptions()->SetValue(OPT_FIX_RENAMED, v = GetCtrlValue(IDC_FIX_RENAMED));
				App->GetOptions()->SerializeFile(true);
				break;
			}
			case IDC_CLOSE:
			{
				EndModeless();
				break;
			}
		}
		return 0;
	}
};

RenameDlg *RenameDlg::Inst = NULL;

bool AppWnd::ShowInProject(const char *Fn)
{
	if (!Fn)
		return false;
	
	for (auto p: d->Projects)
	{
		ProjectNode *Node = NULL;
		if (p->FindFullPath(Fn, &Node))
		{
			for (LTreeItem *i = Node->GetParent(); i; i = i->GetParent())
			{
				i->Expanded(true);
			}
			Node->Select(true);
			Node->ScrollTo();		
			return true;
		}	
	}
	
	return false;
}

int AppWnd::OnCommand(int Cmd, int Event, OsView Wnd)
{
	switch (Cmd)
	{
		case IDM_EXIT:
		{
			LCloseApp();
			break;
		}
		case IDM_OPTIONS:
		{
			Options Dlg(this);
			break;
		}
		case IDM_HELP:
		{
			LExecute(APP_URL);
			break;
		}
		case IDM_ABOUT:
		{
			LAbout a(this,
					AppName, APP_VER,
					"\nLGI Integrated Development Environment",
					"icon128.png",
					APP_URL,
					"fret@memecode.com");
			break;
		}
		case IDM_NEW:
		{
			IdeDoc *Doc;
			d->Docs.Insert(Doc = new IdeDoc(this, 0, 0));
			if (Doc && d->Mdi)
			{
				LRect p = d->Mdi->NewPos();
				Doc->LView::SetPos(p);
				Doc->Attach(d->Mdi);
				Doc->Focus(true);
			}
			break;
		}
		case IDM_OPEN:
		{
			LFileSelect s;
			s.Parent(this);
			if (s.Open())
			{
				OpenFile(s.Name());
			}
			break;
		}
		case IDM_SAVE_ALL:
		{
			SaveAll();
			break;
		}
		case IDM_SAVE:
		{
			IdeDoc *Top = TopDoc();
			if (Top)
				Top->SetClean();
			break;
		}
		case IDM_SAVEAS:
		{
			IdeDoc *Top = TopDoc();
			if (Top)
			{
				LFileSelect s;
				s.Parent(this);
				if (s.Save())
				{
					Top->SetFileName(s.Name(), true);
					d->OnFile(s.Name());
				}
			}
			break;
		}
		case IDM_CLOSE:
		{
			IdeDoc *Top = TopDoc();
			if (Top)
			{
				if (Top->OnRequestClose(false))
				{
					Top->Quit();
				}
			}
			DeleteObj(d->DbgContext);
			break;
		}
		case IDM_CLOSE_ALL:
		{
			CloseAll();
			Name(AppName);
			break;
		}
		
		
		//
		// Editor
		//
		case IDM_UNDO:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->Undo();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_REDO:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->Redo();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_FIND:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->DoFind();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_FIND_NEXT:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->DoFindNext();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_REPLACE:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
			{
				Doc->DoReplace();
			}
			else LgiTrace("%s:%i - No focus doc.\n", _FL);
			break;
		}
		case IDM_GOTO:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
				Doc->DoGoto();
			else
			{
				LInput Inp(this, NULL, LLoadString(L_TEXTCTRL_GOTO_LINE, "Goto [file:]line:"), "Goto");
				if (Inp.DoModal())
				{
					LString s = Inp.GetStr();
					LString::Array p = s.SplitDelimit(":,");
					if (p.Length() == 2)
					{
						LString file = p[0];
						int line = (int)p[1].Int();
						GotoReference(file, line, false, true);
					}
					else LgiMsg(this, "Error: Needs a file name as well.", AppName);
				}
			}
			break;
		}
		case IDM_CUT:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
				Doc->PostEvent(M_CUT);
			break;
		}
		case IDM_COPY:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
				Doc->PostEvent(M_COPY);
			break;
		}
		case IDM_PASTE:
		{
			LTextView3 *Doc = FocusEdit();
			if (Doc)
				Doc->PostEvent(M_PASTE);
			break;
		}
		case IDM_FIND_IN_FILES:
		{
			if (!d->Finder)
			{
				d->Finder.Reset(new FindInFilesThread(d->AppHnd));
			}
			if (d->Finder)
			{
				if (!d->FindParameters &&
					d->FindParameters.Reset(new FindParams))
				{
					LVariant var;
					if (GetOptions()->GetValue(OPT_ENTIRE_SOLUTION, var))
						d->FindParameters->Type = var.CastInt32() ? FifSearchSolution : FifSearchDirectory;
				}		

				FindInFiles Dlg(this, d->FindParameters);

				LViewI *Focus = GetFocus();
				if (Focus)
				{
					LTextView3 *Edit = dynamic_cast<LTextView3*>(Focus);
					if (Edit && Edit->HasSelection())
					{
						LAutoString a(Edit->GetSelection());
						Dlg.Params->Text = a;
					}
				}
				
				IdeProject *p = RootProject();
				if (p)
				{
					LAutoString Base = p->GetBasePath();
					if (Base)
						Dlg.Params->Dir = Base;
				}

				if (Dlg.DoModal())
				{
					if (p && Dlg.Params->Type == FifSearchSolution)
					{
						Dlg.Params->ProjectFiles.Length(0);

						
						List<IdeProject> Projects;
						Projects.Insert(p);
						p->GetChildProjects(Projects);

						LArray<ProjectNode*> Nodes;
						for (auto p: Projects)
							p->GetAllNodes(Nodes);

						for (unsigned i=0; i<Nodes.Length(); i++)
						{
							LString s = Nodes[i]->GetFullPath();
							if (s)
								Dlg.Params->ProjectFiles.Add(s);
						}
					}

					LVariant var = d->FindParameters->Type == FifSearchSolution;
					GetOptions()->SetValue(OPT_ENTIRE_SOLUTION, var);

					d->Finder->Stop();
					d->Finder->PostEvent(FindInFilesThread::M_START_SEARCH, (LMessage::Param) new FindParams(d->FindParameters));
				}
			}
			break;
		}
		case IDM_FIND_SYMBOL:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->GotoSearch(IDC_SYMBOL_SEARCH);
			}
			else
			{
				FindSymResult r = d->FindSym->OpenSearchDlg(this);
				if (r.File)
				{
					GotoReference(r.File, r.Line, false);
				}
			}
			break;
		}
		case IDM_GOTO_SYMBOL:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->SearchSymbol();
			}
			break;
		}
		case IDM_FIND_PROJECT_FILE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
			{
				Doc->SearchFile();
			}
			else
			{
				FindInProject Dlg(this);
				Dlg.DoModal();
			}
			break;
		}
		case IDM_FIND_REFERENCES:
		{
			LViewI *f = LAppInst->GetFocus();
			LDocView *doc = dynamic_cast<LDocView*>(f);
			if (!doc)
				break;

			ssize_t c = doc->GetCaret();
			if (c < 0)
				break;

			LString Txt = doc->Name();
			char *s = Txt.Get() + c;
			char *e = s;
			while (	s > Txt.Get() &&
					IsSymbolChar(s[-1]))
				s--;
			while (*e &&
					IsSymbolChar(*e))
				e++;
			if (e <= s)
				break;

			LString Word(s, e - s);

			if (!d->Finder)
				d->Finder.Reset(new FindInFilesThread(d->AppHnd));
			if (!d->Finder)
				break;

			IdeProject *p = RootProject();
			if (!p)
				break;
			List<IdeProject> Projects;
			Projects.Insert(p);
			p->GetChildProjects(Projects);

			LArray<ProjectNode*> Nodes;
			for (auto p: Projects)
				p->GetAllNodes(Nodes);

			LAutoPtr<FindParams> Params(new FindParams);
			Params->Type = FifSearchSolution;
			Params->MatchWord = true;
			Params->Text = Word;
			for (unsigned i = 0; i < Nodes.Length(); i++)
			{
				Params->ProjectFiles.New() = Nodes[i]->GetFullPath();
			}

			d->Finder->Stop();
			d->Finder->PostEvent(FindInFilesThread::M_START_SEARCH, (LMessage::Param) Params.Release());
			break;
		}
		case IDM_PREV_LOCATION:
		{
			d->SeekHistory(-1);
			break;
		}
		case IDM_NEXT_LOCATION:
		{
			d->SeekHistory(1);
			break;
		}
		
		//
		// Project
		//
		case IDM_NEW_PROJECT:
		{
			CloseAll();
			
			IdeProject *p;
			d->Projects.Insert(p = new IdeProject(this));
			if (p)
			{
				p->CreateProject();
			}
			break;
		}
		case IDM_NEW_PROJECT_TEMPLATE:
		{
			NewProjectFromTemplate(this);
			break;
		}
		case IDM_OPEN_PROJECT:
		{
			LFileSelect s;
			s.Parent(this);
			s.Type("Projects", "*.xml");
			if (s.Open())
			{
				CloseAll();
				OpenProject(s.Name(), NULL, Cmd == IDM_NEW_PROJECT);
				if (d->Tree)
				{
					d->Tree->Focus(true);
				}
			}
			break;
		}
		case IDM_IMPORT_DSP:
		{
			IdeProject *p = RootProject();
			if (p)
			{
				LFileSelect s;
				s.Parent(this);
				s.Type("Developer Studio Project", "*.dsp");
				if (s.Open())
				{
					p->ImportDsp(s.Name());
				}
			}
			break;
		}
		case IDM_RUN:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				p->Execute();
			}
			break;
		}
		case IDM_VALGRIND:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
			{
				p->Execute(ExeValgrind);
			}
			break;
		}
		case IDM_FIX_MISSING_FILES:
		{
			IdeProject *p = RootProject();
			if (p)
				p->FixMissingFiles();
			else
				LgiMsg(this, "No project loaded.", AppName);
			break;
		}
		case IDM_FIND_DUPE_SYM:
		{
			IdeProject *p = RootProject();
			if (p)
				p->FindDuplicateSymbols();
			else
				LgiMsg(this, "No project loaded.", AppName);
			break;
		}
		case IDM_RENAME_SYM:
		{
			if (!RenameDlg::Inst)
				new RenameDlg(this);
			break;
		}
		case IDM_START_DEBUG:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (!p)
			{
				LgiMsg(this, "No project loaded.", "Error");
				break;
			}

			if (d->DbgContext)
			{
				d->DbgContext->OnCommand(IDM_CONTINUE);
			}
			else if ((d->DbgContext = p->Execute(ExeDebug)))
			{
				d->DbgContext->DebuggerLog = d->Output->DebuggerLog;
				d->DbgContext->Watch = d->Output->Watch;
				d->DbgContext->Locals = d->Output->Locals;
				d->DbgContext->CallStack = d->Output->CallStack;
				d->DbgContext->Threads = d->Output->Threads;
				d->DbgContext->ObjectDump = d->Output->ObjectDump;
				d->DbgContext->Registers = d->Output->Registers;
				d->DbgContext->MemoryDump = d->Output->MemoryDump;
				
				d->DbgContext->OnCommand(IDM_START_DEBUG);
				
				d->Output->Value(AppWnd::DebugTab);
				d->Output->DebugEdit->Focus(true);
			}
			break;
		}
		case IDM_TOGGLE_BREAKPOINT:
		{
			IdeDoc *Cur = GetCurrentDoc();
			if (Cur)
				ToggleBreakpoint(Cur->GetFileName(), Cur->GetLine());
			break;
		}
		case IDM_ATTACH_TO_PROCESS:
		case IDM_PAUSE_DEBUG:
		case IDM_RESTART_DEBUGGING:
		case IDM_RUN_TO:
		case IDM_STEP_INTO:
		case IDM_STEP_OVER:
		case IDM_STEP_OUT:
		{
			if (d->DbgContext)
				d->DbgContext->OnCommand(Cmd);
			break;
		}
		case IDM_STOP_DEBUG:
		{
			if (d->DbgContext && d->DbgContext->OnCommand(Cmd))
			{
				DeleteObj(d->DbgContext);
			}
			break;
		}
		case IDM_BUILD:
		{
			Build();
			break;			
		}
		case IDM_STOP_BUILD:
		{
			IdeProject *p = RootProject();
			if (p)
				p->StopBuild();
			break;
		}
		case IDM_CLEAN:
		{
			SaveAll();
			IdeProject *p = RootProject();
			if (p)
				p->Clean(true, IsReleaseMode());
			break;
		}
		case IDM_NEXT_MSG:
		{
			d->SeekMsg(1);
			break;
		}
		case IDM_PREV_MSG:
		{
			d->SeekMsg(-1);
			break;
		}
		case IDM_DEBUG_MODE:
		{
			LMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
			LMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
			if (Debug && Release)
			{
				Debug->Checked(true);
				Release->Checked(false);
			}
			break;
		}
		case IDM_RELEASE_MODE:
		{
			LMenuItem *Debug = GetMenu()->FindItem(IDM_DEBUG_MODE);
			LMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
			if (Debug && Release)
			{
				Debug->Checked(false);
				Release->Checked(true);
			}
			break;
		}
		
		//
		// Other
		//
		case IDM_LOOKUP_SYMBOLS:
		{
			IdeDoc *Cur = GetCurrentDoc();
			if (Cur)
			{
				// LookupSymbols(Cur->Read());
			}
			break;
		}
		case IDM_DEPENDS:
		{
			IdeProject *p = RootProject();
			if (p)
			{
				LString Exe = p->GetExecutable(GetCurrentPlatform());
				if (LFileExists(Exe))
				{
					Depends Dlg(this, Exe);
				}
				else
				{
					LgiMsg(this, "Couldn't find '%s'\n", AppName, MB_OK, Exe ? Exe.Get() : "<project_executable_undefined>");
				}
			}			
			break;
		}
		case IDM_SP_TO_TAB:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->ConvertWhiteSpace(true);
			break;
		}
		case IDM_TAB_TO_SP:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->ConvertWhiteSpace(false);
			break;
		}
		case IDM_ESCAPE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->EscapeSelection(true);
			break;
		}
		case IDM_DESCAPE:
		{
			IdeDoc *Doc = FocusDoc();
			if (Doc)
				Doc->EscapeSelection(false);
			break;
		}
		case IDM_SPLIT:
		{
			IdeDoc *Doc = FocusDoc();
			if (!Doc)
				break;

			LInput i(this, "", "Separator:", AppName);
			if (!i.DoModal())
				break;
				
			Doc->SplitSelection(i.GetStr());
			break;
		}
		case IDM_JOIN:
		{
			IdeDoc *Doc = FocusDoc();
			if (!Doc)
				break;

			LInput i(this, "", "Separator:", AppName);
			if (!i.DoModal())
				break;
				
			Doc->JoinSelection(i.GetStr());
			break;
		}
		case IDM_EOL_LF:
		{
			IdeDoc *Doc = FocusDoc();
			if (!Doc)
				break;
			Doc->SetCrLf(false);
			break;
		}
		case IDM_EOL_CRLF:
		{
			IdeDoc *Doc = FocusDoc();
			if (!Doc)
				break;
			Doc->SetCrLf(true);
			break;
		}
		case IDM_LOAD_MEMDUMP:
		{
			NewMemDumpViewer(this);
			break;
		}
		case IDM_SYS_CHAR_SUPPORT:
		{
			new SysCharSupport(this);
			break;
		}
		default:
		{
			char *r = d->RecentFiles[Cmd - IDM_RECENT_FILE];
			if (r)
			{
				IdeDoc *f = d->IsFileOpen(r);
				if (f)
				{
					f->Raise();
				}
				else
				{
					OpenFile(r);
				}
			}

			auto p = d->RecentProjects[Cmd - IDM_RECENT_PROJECT];
			if (p)
			{
				CloseAll();
				OpenProject(p, NULL, false);
				if (d->Tree)
				{
					d->Tree->Focus(true);
				}
			}
			
			IdeDoc *Doc = d->Docs[Cmd - IDM_WINDOWS];
			if (Doc)
			{
				Doc->Raise();
			}
			
			IdePlatform PlatIdx = (IdePlatform) (Cmd - IDM_MAKEFILE_BASE);
			const char *Platform =	PlatIdx >= 0 && PlatIdx < PlatformMax
									?
									PlatformNames[Cmd - IDM_MAKEFILE_BASE]
									:
									NULL;
			if (Platform)
			{
				IdeProject *p = RootProject();
				if (p)
				{
					p->CreateMakefile(PlatIdx, false);
				}
			}
			break;
		}
	}
	
	return 0;
}

LTree *AppWnd::GetTree()
{
	return d->Tree;
}

IdeDoc *AppWnd::TopDoc()
{
	return d->Mdi ? dynamic_cast<IdeDoc*>(d->Mdi->GetTop()) : NULL;
}

LTextView3 *AppWnd::FocusEdit()
{
	return dynamic_cast<LTextView3*>(GetWindow()->GetFocus());	
}
	
IdeDoc *AppWnd::FocusDoc()
{
	IdeDoc *Doc = TopDoc();
	if (Doc)
	{
		if (Doc->HasFocus())
		{
			return Doc;
		}
		else
		{
			LViewI *f = GetFocus();
			LgiTrace("%s:%i - Edit doesn't have focus, f=%p %s doc.edit=%s\n",
				_FL, f, f ? f->GetClass() : 0,
				Doc->Name());
		}
	}
	
	return 0;
}

void AppWnd::OnProjectDestroy(IdeProject *Proj)
{
	d->Projects.Delete(Proj);
}

void AppWnd::OnProjectChange()
{
	LArray<GMdiChild*> Views;
	if (d->Mdi->GetChildren(Views))
	{
		for (unsigned i=0; i<Views.Length(); i++)
		{
			IdeDoc *Doc = dynamic_cast<IdeDoc*>(Views[i]);
			if (Doc)
				Doc->OnProjectChange();
		}
	}
}

void AppWnd::OnDocDestroy(IdeDoc *Doc)
{
	if (d)
	{
		d->Docs.Delete(Doc);
		d->UpdateMenus();
	}
}

int AppWnd::GetBuildMode()
{
	LMenuItem *Release = GetMenu()->FindItem(IDM_RELEASE_MODE);
	if (Release && Release->Checked())
	{
		return BUILD_TYPE_RELEASE;
	}

	return BUILD_TYPE_DEBUG;
}

LList *AppWnd::GetFtpLog()
{
	return d->Output->FtpLog;
}

LStream *AppWnd::GetBuildLog()
{
	return d->Output->Txt[AppWnd::BuildTab];
}

LStream *AppWnd::GetDebugLog()
{
	return d->Output->Txt[AppWnd::DebugTab];
}

void AppWnd::FindSymbol(int ResultsSinkHnd, const char *Sym, bool AllPlatforms)
{
	d->FindSym->Search(ResultsSinkHnd, Sym, AllPlatforms);
}

bool AppWnd::GetSystemIncludePaths(::LArray<LString> &Paths)
{
	if (d->SystemIncludePaths.Length() == 0)
	{
		#if !defined(WINNATIVE)
		// echo | gcc -v -x c++ -E -
		LSubProcess sp1("echo");
		LSubProcess sp2("gcc", "-v -x c++ -E -");
		sp1.Connect(&sp2);
		sp1.Start(true, false);
		
		char Buf[256];
		ssize_t r;

		LStringPipe p;
		while ((r = sp1.Read(Buf, sizeof(Buf))) > 0)
		{
			p.Write(Buf, r);
		}

		bool InIncludeList = false;
		while (p.Pop(Buf, sizeof(Buf)))
		{
			if (stristr(Buf, "#include"))
			{
				InIncludeList = true;
			}
			else if (stristr(Buf, "End of search"))
			{
				InIncludeList = false;
			}
			else if (InIncludeList)
			{
				LAutoString a(TrimStr(Buf));
				d->SystemIncludePaths.New() = a;
			}
		}
		#else
		char p[MAX_PATH];
		LGetSystemPath(LSP_USER_DOCUMENTS, p, sizeof(p));
		LMakePath(p, sizeof(p), p, "Visual Studio 2008\\Settings\\CurrentSettings.xml");
		if (LFileExists(p))
		{
			LFile f;
			if (f.Open(p, O_READ))
			{
				LXmlTree t;
				LXmlTag r;
				if (t.Read(&r, &f))
				{
					LXmlTag *Opts = r.GetChildTag("ToolsOptions");
					if (Opts)
					{
						LXmlTag *Projects = NULL;
						char *Name;
						for (auto c: Opts->Children)
						{
							if (c->IsTag("ToolsOptionsCategory") &&
								(Name = c->GetAttr("Name")) &&
								!stricmp(Name, "Projects"))
							{
								Projects = c;
								break;
							}
						}

						LXmlTag *VCDirectories = NULL;
						if (Projects)
							for (auto c: Projects->Children)
							{
								if (c->IsTag("ToolsOptionsSubCategory") &&
									(Name = c->GetAttr("Name")) &&
									!stricmp(Name, "VCDirectories"))
								{
									VCDirectories = c;
									break;
								}
							}

						if (VCDirectories)
							for (auto prop: VCDirectories->Children)
							{
								if (prop->IsTag("PropertyValue") &&
									(Name = prop->GetAttr("Name")) &&
									!stricmp(Name, "IncludeDirectories"))
								{
									char *Bar = strchr(prop->GetContent(), '|');
									GToken t(Bar ? Bar + 1 : prop->GetContent(), ";");
									for (int i=0; i<t.Length(); i++)
									{
										char *s = t[i];
										d->SystemIncludePaths.New().Reset(NewStr(s));
									}
								}
							}
					}
				}
			}
		}		
		#endif
	}
	
	for (int i=0; i<d->SystemIncludePaths.Length(); i++)
	{
		Paths.Add(NewStr(d->SystemIncludePaths[i]));
	}
	
	return true;
}

/*
class SocketTest : public LWindow, public LThread
{
	LTextLog *Log;

public:
	SocketTest() : LThread("SocketTest")
	{
		Log = new LTextLog(100);

		SetPos(LRect(200, 200, 900, 800));
		Attach(0);
		Visible(true);
		Log->Attach(this);

		Run();
	}

	int Main()
	{
		LSocket s;
		s.SetTimeout(15000);
		Log->Print("Starting...\n");
		auto r = s.Open("192.168.1.30", 7000);
		Log->Print("Open =%i\n", r);
		return 0;
	}
};
*/

class TestView : public LView
{
public:
	TestView()
	{
		LRect r(10, 10, 110, 110);
		SetPos(r);
	}

	void OnPaint(LSurface *pdc)
	{
		pdc->Colour(LColour::Red);

		// pdc->Rectangle();
		pdc->Line(0, 0, X()-1, Y()-1);
		pdc->Circle(50, 50, 50);
	}
};

class Test : public LWindow
{
public:
	Test()
	{
		LRect r(100, 100, 800, 700);
		SetPos(r);
		Name("Test");
		SetQuitOnClose(true);

		if (Attach(0))
		{
			AddView(new TestView);
			AttachChildren();
			Visible(true);
		}
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	printf("LgiIde v%s\n", APP_VER);
	LApp a(AppArgs, "LgiIde");
	if (a.IsOk())
	{
		#if 0
		a.AppWnd = new Test;
		#else
		a.AppWnd = new AppWnd;
		#endif
		a.Run();
	}

	return 0;
}

