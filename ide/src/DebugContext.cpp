#include "lgi/common/Lgi.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/List.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/PopupNotification.h"

#include "LgiIde.h"
#include "IdeProject.h"
#include "resdefs.h"

#ifdef LINUX
namespace Gtk {
	#include "gdk/gdkx.h"
	#undef Bool
}
#endif


enum Ids
{
	ID_TBL = 100,
	ID_PROCESSES,
	ID_FILTER,
	
	M_RUN_STATE = M_USER + 100,
	M_ON_CRASH,
};

class AttachToProcess : public LDialog
{
	AppWnd *app = NULL;
	LTableLayout *tbl = NULL;
	LList *lst = NULL;
	LStream *log = NULL;
	LEdit *filter = NULL;

public:
	int Pid = -1;

	enum Cols {
		ColPid,
		ColProcess,
	};
	AttachToProcess(AppWnd *parent) : LDialog(parent)
	{
		LRect r(0, 0, 1000, 900);
		SetPos(r);
		MoveSameScreen(app = parent);
		log = app->GetOutputLog();
		Name("Attach to Process");

		AddView(tbl = new LTableLayout(ID_TBL));
		int y = 0;
		auto c = tbl->GetCell(0, y);
			c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Select process:"));

		c = tbl->GetCell(1, y++);
			c->Add(lst = new LList(ID_PROCESSES));
			lst->AddColumn("Pid", 100);
			lst->AddColumn("Process", 250);

		c = tbl->GetCell(0, y);
			c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Filter:"));

		c = tbl->GetCell(1, y++);
			c->Add(filter = new LEdit(ID_FILTER));
			filter->Focus(true);

		c = tbl->GetCell(0, y++, true, 2, 1);
			c->TextAlign(LCss::AlignRight);
			c->Add(new LButton(IDOK, 0, 0, -1, -1, "Attach"));
			c->Add(new LButton(IDCANCEL, 0, 0, -1, -1, "Cancel"));

		LSubProcess sub("ps", "-A");
		if (sub.Start())
		{
			LStringPipe p;
			sub.Communicate(&p);
			
			auto lines = p.NewLStr().SplitDelimit("\r\n");
			lines.DeleteAt(0, true);
			for (auto line: lines)
			{
				auto parts = line.SplitDelimit();
				auto i = new LListItem;
				i->SetText(parts[0], ColPid);
				i->SetText(parts.Last(), ColProcess);
				lst->Insert(i);
			}			
		}
	}

	void Select()
	{
		if (auto item = lst->GetSelected())
		{
			Pid = Atoi(item->GetText(ColPid));
			if (Pid > 0)
				EndModal(true);
		}
	}
	
	int OnNotify(LViewI *c, const LNotification &n) override
	{
		switch (c->GetId())
		{
			case ID_FILTER:
			{
				LArray<LListItem*> all;
				if (lst->GetAll(all))
				{
					for (auto i: all)
					{
						auto process = i->GetText(ColProcess);
						bool vis = Stristr(process, c->Name());
						i->GetCss(true)->Display(vis ? LCss::DispBlock : LCss::DispNone);
					}
				}
				break;
			}
			case ID_PROCESSES:
			{
				switch (n.Type)
				{
					case LNotifyItemDoubleClick:
					{
						Select();
						break;
					}
				}
				break;
			}
			case IDOK:
			{
				Select();
				break;
			}
			case IDCANCEL:
			{
				EndModal(false);
				break;
			}
		}
		
		return LDialog::OnNotify(c, n);
	}
};

class LDebugContextPriv : public LMutex
{
public:
	LDebugContext *Ctx = NULL;
	AppWnd *App = NULL;
	IdeProject *Proj = NULL;
	bool InDebugging = false;
	LAutoPtr<LDebugger> Db;
	LString Exe, Args;
	IdePlatform Platform;
	
	LString SeekFile;
	int SeekLine = 0;
	bool SeekCurrentIp = false;
	
	LString MemDumpAddr;
	NativeInt MemDumpStart;
	LArray<uint8_t> MemDump;

	LDebugContextPriv(LDebugContext *ctx, IdePlatform platform) :
		LMutex("LDebugContextPriv"),
		Platform(platform)
	{
		Ctx = ctx;
	}
	
	~LDebugContextPriv()
	{
		#if DEBUG_SESSION_LOGGING
		LgiTrace("~LDebugContextPriv freeing debugger...\n");
		#endif
		Db.Reset();
		#if DEBUG_SESSION_LOGGING
		LgiTrace("...done.\n");
		#endif
	}

	void UpdateThreads()
	{
		if (!Db || !Ctx->Threads || !InDebugging)
		{
			LgiTrace("%s:%i - Missing param: %p, %p, %i\n", _FL, Db.Get(), Ctx->Threads, InDebugging);
			return;
		}
		
		LArray<LString> Threads;
		int CurrentThread = -1;
		Db->GetThreads([this](auto threads, auto cur)
			{
				App->RunCallback([this, threads, cur]() mutable
				{
					Ctx->Threads->Empty();
					for (unsigned i=0; i<threads.Length(); i++)
					{
						auto &f = threads[i];
						if (IsDigit(*f))
						{
							char *Sp = f;
							while (*Sp && IsDigit(*Sp))
								Sp++;
							if (*Sp)
							{
								*Sp++ = 0;
								while (*Sp && IsWhite(*Sp))
									Sp++;

								LListItem *it = new LListItem;
						
								int ThreadId = atoi(f);
								it->SetText(f, 0);
								it->SetText(Sp, 1);
						
								Ctx->Threads->Insert(it);
								it->Select(ThreadId == cur);
							}
						}			
					}
			
					Ctx->Threads->SendNotify();
				});
			});
	}

	void OnCallStack(LString::Array &stack)
	{
		LAssert(Ctx->CallStack->InThread());

		Ctx->CallStack->Empty();
		for (auto frame: stack)
		{
			LListItem *it = new LListItem;
					
			auto f = frame.Get();
			if (f && *f == '#')
			{
				auto Sp = strchr(++f, ' ');
				if (Sp)
				{
					*Sp++ = 0;
					it->SetText(f, 0);
					it->SetText(Sp, 1);
				}
				else
				{
					it->SetText(frame, 1);
				}
			}
			else
			{					
				it->SetText(frame, 1);
			}
					
			Ctx->CallStack->Insert(it);
		}
				
		Ctx->CallStack->SendNotify();
	}

	void UpdateCallStack()
	{
		if (!Db || !Ctx->CallStack || !InDebugging)
			return;

		Db->GetCallStack([this](auto Stack)
		{
			Ctx->CallStack->RunCallback([this, Stack]() mutable
			{
				OnCallStack(Stack);
			});
		});
	}
	
	void Log(const char *Fmt, ...)
	{
		if (Ctx->DebuggerLog)
		{
			va_list Arg;
			va_start(Arg, Fmt);
			LStreamPrintf(Ctx->DebuggerLog, 0, Fmt, Arg);
			va_end(Arg);
		}
	}

	int Main()
	{
		return 0;
	}
};

LDebugContext::LDebugContext(AppWnd *App,
							IdeProject *Proj,
							IdePlatform Platform,
							const char *Exe,
							const char *Args,
							bool RunAsAdmin,
							const char *Env,
							const char *InitDir)
{
	d = new LDebugContextPriv(this, Platform);
	d->App = App;
	d->Proj = Proj;
	d->Exe = Exe;

	auto log = App->GetDebugLog();
	LAssert(log);
	if (d->Db.Reset(CreateGdbDebugger(App->GetBreakPointStore(), log, Proj->GetBackend(), Platform, d->App->GetNetworkLog())))
	{
		LFile::Path p;
		if (InitDir)
			p = InitDir;
		else
			p = LFile::Path(Exe) / "..";
	
		if (Exe)
		{
			if (!d->Db->Load(this, Exe, Args, RunAsAdmin, p, Env))
			{
				d->Log("Failed to load '%s' into debugger.\n", d->Exe.Get());
				d->Db.Reset();
			}
		}
	}
}

LDebugContext::~LDebugContext()
{
	DeleteObj(d);
}

LMessage::Param LDebugContext::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case M_ON_CRASH:
		{
			#if DEBUG_SESSION_LOGGING
			LgiTrace("LDebugContext::OnEvent(M_ON_CRASH)\n");
			#endif
			d->UpdateCallStack();
			break;
		}
	}
	
	return 0;
}

void LDebugContext::SetFrame(int Frame)
{
	if (!d->Db)
		return;
	
	d->Db->SetFrame(Frame, nullptr);
}

bool LDebugContext::DumpObject(const char *Var, const char *Val)
{
	if (!d->Db || !Var || !ObjectDump || !d->InDebugging)
		return false;
	
	ObjectDump->Name(NULL);
	d->Db->PrintObject
	(
		Var,
		[this](auto txt)
		{
			ObjectDump->RunCallback
			(
				[this, txt]()
				{
					ObjectDump->Name(txt);
				}
			);
		}
	);

	return true;
}

void LDebugContext::UpdateRegisters()
{
	if (!d->Db || !Registers || !d->InDebugging)
		return;
	
	d->Db->GetRegisters([this](auto &lines)
		{
			Registers->Write(LString("\n").Join(lines));
		});
}

void LDebugContext::UpdateLocals()
{
	if (!Locals || !d->Db || !d->InDebugging)
		return;

	if (d->Db->GetRunning())
	{
		printf("%s:%i - Debugger is running... can't update locals.\n", _FL);
		return;
	}

	d->Db->GetVariables(
		true,
		false,
		nullptr,
		[this](auto &err, auto &Vars)
		{
			if (err)
			{
				return;
			}

			Locals->Empty();
			for (int i=0; i<Vars.Length(); i++)
			{
				LDebugger::Variable &v = Vars[i];
				LListItem *it = new LListItem;
				if (it)
				{
					switch (v.Scope)
					{
						default:
						case LDebugger::Local:
							it->SetText("local", 0);
							break;
						case LDebugger::Global:
							it->SetText("global", 0);
							break;
						case LDebugger::Arg:
							it->SetText("arg", 0);
							break;
					}

					char s[256];
					switch (v.Value.Type)
					{
						case GV_BOOL:
						{
							it->SetText(v.Type ? v.Type.Get() : "bool", 1);
							sprintf_s(s, sizeof(s), "%s", v.Value.Value.Bool ? "true" : "false");
							break;
						}
						case GV_INT32:
						{
							it->SetText(v.Type ? v.Type.Get() : "int32", 1);
							sprintf_s(s, sizeof(s), "%i (0x%x)", v.Value.Value.Int, v.Value.Value.Int);
							break;
						}
						case GV_INT64:
						{
							it->SetText(v.Type ? v.Type.Get() : "int64", 1);
							sprintf_s(s, sizeof(s), LPrintfInt64, v.Value.Value.Int64);
							break;
						}
						case GV_DOUBLE:
						{
							it->SetText(v.Type ? v.Type.Get() : "double", 1);
							snprintf(s, sizeof(s), "%g", v.Value.Value.Dbl);
							break;
						}
						case GV_STRING:
						{
							it->SetText(v.Type ? v.Type.Get() : "string", 1);
							snprintf(s, sizeof(s), "%s", v.Value.Value.String);
							break;
						}
						case GV_WSTRING:
						{
							it->SetText(v.Type ? v.Type.Get() : "wstring", 1);
							#ifdef MAC
							LAutoString tmp(WideToUtf8(v.Value.Value.WString));
							sprintf_s(s, sizeof(s), "%s", tmp.Get());
							#else
							sprintf_s(s, sizeof(s), "%S", v.Value.Value.WString);
							#endif
							break;
						}
						case GV_VOID_PTR:
						{
							it->SetText(v.Type ? v.Type.Get() : "void*", 1);
							sprintf_s(s, sizeof(s), "%p", v.Value.Value.Ptr);
							break;
						}
						default:
						{
							sprintf_s(s, sizeof(s), "notimp(%s)", LVariant::TypeToString(v.Value.Type));
							it->SetText(v.Type ? v.Type : s, 1);
							s[0] = 0;
							break;
						}
					}

					it->SetText(v.Name, 2);
					it->SetText(s, 3);
					Locals->Insert(it);
				}
			}
	
			Locals->ResizeColumnsToContent();
		});
}

void LDebugContext::UpdateWatches()
{
	if (!Watch)
		return;

	LArray<LDebugger::Variable> Vars;
	for (LTreeItem *i = Watch->GetChild(); i; i = i->GetNext())
	{
		LDebugger::Variable &v = Vars.New();
		v.Name = i->GetText(0);
		v.Type = i->GetText(1);
	}
	
	d->Db->GetVariables(
		false,
		false,
		&Vars,
		[this](auto &err, auto &Vars)
		{
			int Idx = 0;
			for (LTreeItem *i = Watch->GetChild(); i; i = i->GetNext(), Idx++)
			{
				LDebugger::Variable &v = Vars[Idx];
				WatchItem *wi = dynamic_cast<WatchItem*>(i);
				if (!wi)
				{
					LgiTrace("%s:%i - Error: not watch item.\n", _FL);
					continue;
				}
				if (v.Name == (const char*)i->GetText(0))
				{
					i->SetText(v.Type, 1);
					wi->SetValue(v.Value);
				}
				else
				{
					LgiTrace("%s:%i - Error: Not the same name.\n", _FL);
				}
			}
		});
}

void LDebugContext::UpdateCallStack()
{
	d->UpdateCallStack();
}

void LDebugContext::UpdateThreads()
{
	d->UpdateThreads();
}

void LDebugContext::SelectThread(int ThreadId, LDebugger::TStatusCb cb)
{
	if (!d->Db)
	{
		LgiTrace("%s:%i - No debugger.\n", _FL);
		if (cb)
			cb(false);
		return;
	}
	
	d->Db->SetCurrentThread(ThreadId, cb);
}

bool LDebugContext::ParseFrameReference(const char *Frame, LAutoString &File, int &Line)
{
	if (!Frame)
		return false;
	
	const char *At = NULL, *s = Frame;
	while ((s = stristr(s, "at")))
	{
		At = s;
		s += 2;
	}

	if (!At)
		return false;

	At += 3;
	if (!File.Reset(LTokStr(At)))
		return false;
	
	char *Colon = strchr(File, ':');
	if (!Colon)
		return false;

	*Colon++ = 0;
	Line = atoi(Colon);
	return Line > 0;
}

void LDebugContext::Quit()
{
	if (d->Db)
		d->Db->Unload([this](auto status)
			{
				if (!status)
					return;

				d->App->RunCallback([this]()
					{
						d->Db.Reset();
						if (onFinished)
						{
							auto finished = std::move(onFinished);
							// Calling 'finished' will delete 'this'
							finished();
						}
					});
			});
}

bool LDebugContext::OnCommand(int Cmd)
{
	#if DEBUG_SESSION_LOGGING
	LgiTrace("LDebugContext::OnCommand(%i)\n", Cmd);
	#endif
	
	switch (Cmd)
	{
		case IDM_START_DEBUG:
		{
			if (d->Db)
			{
				d->Db->SetRunning(true, nullptr);
			}
			break;
		}
		case IDM_CONTINUE:
		{
			if (d->Db)
				d->Db->SetRunning(true, nullptr);
			break;
		}
		case IDM_ATTACH_TO_PROCESS:
		{
			auto dlg = new AttachToProcess(d->App);
			dlg->DoModal([this, dlg](auto Dlg, auto Code)
			{
				if (Code && dlg->Pid > 0)
				{
					d->Db->AttachTo(this, dlg->Pid);
				}
			});
			break;
		}
		case IDM_STOP_DEBUG:
		{
			if (!d->Db)
				return false;

			Quit();
			return true;
		}
		case IDM_PAUSE_DEBUG:
		{
			if (d->Db)
				d->Db->SetRunning(false, nullptr);
			break;
		}
		case IDM_RESTART_DEBUGGING:
		{
			if (d->Db)
				d->Db->Restart(nullptr);
			break;
		}
		case IDM_RUN_TO:
		{
			break;
		}
		case IDM_STEP_INTO:
		{
			printf("debugger IDM_STEP_INTO\n");
			if (d->Db)
				d->Db->StepInto(nullptr);
			break;
		}
		case IDM_STEP_OVER:
		{
			if (d->Db)
				d->Db->StepOver(nullptr);
			break;
		}
		case IDM_STEP_OUT:
		{
			if (d->Db)
				d->Db->StepOut(nullptr);
			break;
		}
		case IDM_TOGGLE_BREAKPOINT:
		{
			break;
		}
		default:
			return false;
	}

	return true;
}

void LDebugContext::OnUserCommand(const char *Cmd)
{
	if (d->Db)
		d->Db->UserCommand(Cmd, nullptr);
}

void NonPrintable(uint64 ch, uint8_t *&out, ssize_t &len)
{
	if (ch == '\r')
	{
		if (len > 1)
		{
			*out++ = '\\';
			*out++ = 'r';
			len -= 2;
		}
		else LAssert(0);
	}
	else if (ch == '\n')
	{
		if (len > 1)
		{
			*out++ = '\\';
			*out++ = 'n';
			len -= 2;
		}
		else LAssert(0);
	}
	else if (len > 0)
	{
		*out++ = '.';
		len--;
	}
}

void LDebugContext::FormatMemoryDump(int WordSize, int Width, bool InHex)
{
	if (!MemoryDump)
	{
		LgiTrace("%s:%i - No MemoryDump.\n", _FL);
		return;
	}
	
	if (d->MemDump.Length() == 0)
	{
		MemoryDump->Name("No data.");
		return;
	}

	if (!Width)
		Width = 16 / WordSize;			
	if (!WordSize)
		WordSize = 1;

	// Format output to the mem dump window
	LStringPipe p;
	int LineBytes = WordSize * Width;
	
	LPointer ptr;
	ptr.u8 = &d->MemDump[0];
	
	for (NativeInt i = 0; i < d->MemDump.Length(); i += LineBytes)
	{
		// LPointer Start = ptr;
		ssize_t DisplayBytes = MIN(d->MemDump.Length() - i, LineBytes);
		ssize_t DisplayWords = DisplayBytes / WordSize;			
		NativeInt iAddr = d->MemDumpStart + i;
		p.Print("%p  ", iAddr);
		
		char Char[256] = "";
		uint8_t *ChPtr = (uint8_t*)Char;
		ssize_t Len = sizeof(Char);
		
		for (int n=0; n<DisplayWords; n++)
		{
			switch (WordSize)
			{
				default:
					if (*ptr.u8 < ' ')
						NonPrintable(*ptr.u8, ChPtr, Len);
					else
						LgiUtf32To8(*ptr.u8, ChPtr, Len);
					
					if (InHex)
						p.Print("%02X ", *ptr.u8++);
					else
						p.Print("%4u ", *ptr.u8++);
					break;
				case 2:
					if (*ptr.u16 < ' ')
						NonPrintable(*ptr.u16, ChPtr, Len);
					else
						LgiUtf32To8(*ptr.u16, ChPtr, Len);
					
					if (InHex)
						p.Print("%04X ", *ptr.u16++);
					else
						p.Print("%7u ", *ptr.u16++);
					break;
				case 4:
					if (*ptr.u32 < ' ')
						NonPrintable(*ptr.u32, ChPtr, Len);
					else
					{
						// printf("print %i\n", *ptr.u32);
						LgiUtf32To8(*ptr.u32, ChPtr, Len);
						// printf("valid=%i\n", LIsUtf8(Char, (char*)ChPtr-Char));
					}

					if (InHex)
						p.Print("%08X ", *ptr.u32++);
					else
						p.Print("%10u ", *ptr.u32++);
					break;
				case 8:
					if (*ptr.u64 < ' ')
						NonPrintable(*ptr.u64, ChPtr, Len);
					else
						LgiUtf32To8((uint32_t)*ptr.u64, ChPtr, Len);

					if (InHex)
						#ifdef WIN32
						p.Print("%016I64X ", *ptr.u64++);
						#else
						p.Print("%016LX ", *ptr.u64++);
						#endif
					else
						#ifdef WIN32
						p.Print("%17I64u ", *ptr.u64++);
						#else
						p.Print("%17Lu ", *ptr.u64++);
						#endif
					break;
			}
		}
		*ChPtr = 0;
		
		p.Print("    \"%s\"\n", Char);
	}
	
	MemoryDump->Name(p.NewLStr());
}

void LDebugContext::OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex)
{
	if (MemoryDump && d->Db)
	{
		MemoryDump->Name(NULL);
		LString ErrMsg;		
		if (d->Db->ReadMemory(d->MemDumpAddr = Addr, 1024, d->MemDump, &ErrMsg))
		{
			d->MemDumpStart = (int)d->MemDumpAddr.Int(16);
			FormatMemoryDump(WordSize, Width, IsHex);
		}
		else
		{
			MemoryDump->Name(ErrMsg ? ErrMsg : "ReadMemory failed.");
		}
	}
}

void LDebugContext::OnState(bool Debugging, bool Running)
{
	#if DEBUG_SESSION_LOGGING
	LgiTrace("LDebugContext::OnState(%i, %i)\n", Debugging, Running);
	#endif
	
	if (d->InDebugging != Debugging && d->Db)
		d->InDebugging = Debugging;

	if (d->App)
		d->App->OnDebugState(Debugging, Running);
	
	#if DEBUG_SESSION_LOGGING
	LgiTrace("LDebugContext::OnState(%i, %i) ###ENDED###\n", Debugging, Running);
	#endif
	
	// This object may be deleted at this point... don't access anything.
}

void LDebugContext::OnFileLine(const char *File, int Line, bool CurrentIp)
{
	if (!d->App)
	{
		printf("%s:%i - No app.\n", _FL);
		return;
	}
	
	if (!File || Line < 1)
	{
		LgiTrace("%s:%i - Error: No File or Line... one or both must be valid.\n", _FL);
		LAssert(!"Invalid Param");
		return;
	}

	// Goto reference is thread safe:
	d->App->GotoReference(File, Line, CurrentIp, false, NULL);
}

ssize_t LDebugContext::Write(const void *Ptr, ssize_t Size, int Flags)
{
	if (DebuggerLog && Ptr)
	{
		// LgiTrace("Write '%.*s'\n", Size, Ptr);
		return DebuggerLog->Write(Ptr, Size, 0);
	}
		
	return -1;
}

void LDebugContext::OnError(LString Str)
{
	if (DebuggerLog)
		DebuggerLog->Print("Error: %s\n", Str.Get());

	d->App->RunCallback([this, Str]()
		{
			LPopupNotification::Message(d->App, Str);
		});
}

void LDebugContext::OnCrash(int Code)
{
	d->App->PostEvent(M_ON_CRASH);
}

void LDebugContext::OnWarning(LString str)
{
	LPopupNotification::Message(d->App, str);
}

void LDebugContext::Ungrab()
{
	// printf("LDebugContext::Ungrab: noop\n");
}
