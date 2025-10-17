#define _CRT_SECURE_NO_WARNINGS
#include "lgi/common/Lgi.h"
#include "lgi/common/OptionsFile.h"
#include "lgi/common/DocApp.h"
#include "lgi/common/Box.h"
#include "lgi/common/List.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/TabView.h"
#include "lgi/common/StructuredLog.h"
#include "lgi/common/StatusBar.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/Menu.h"

#include "resdefs.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "SlogViewer";

enum DisplayMode
{
	DisplayHex,
	DisplayEscaped
};

enum Ctrls 
{
	ID_BOX = 100,
	ID_LIST,
	ID_HEX,
	ID_ESCAPED,
	ID_TABS,
	ID_STATUS,
	ID_LOG,
	ID_CLEAR,
};

enum Messages
{
	M_ENTRY_SELECTED = M_USER,
	M_COMMS_STATE,
};

struct Context
{
	LList *lst = NULL;
	LTabView *tabs = NULL;
	LTextView3 *hex = NULL;
	LTextView3 *escaped = NULL;
	LTextLog *log = NULL;
};

class Entry : public LListItem
{
	Context *c;

	struct Value
	{
		LVariantType type;
		LArray<uint8_t> data;
		LString name;
	};
	LArray<Value> values;

	LString cache;

public:
	Entry(Context *ctx) : c(ctx)
	{
	}

	void add(LVariantType type, size_t sz, void *ptr, const char *name)
	{
		auto &v = values.New();
		v.type = type;
		v.data.Add((uint8_t*)ptr, sz);
		v.name = name;
	}

	const char *GetText(int i)
	{
		if (!cache)
		{
			LStringPipe p;
			for (auto &v: values)
			{
				switch (v.type)
				{
					case GV_INT32:
					case GV_INT64:
					{
						if (v.data.Length() == 4)
							p.Print(" %i", *(int*)v.data.AddressOf());
						else if (v.data.Length() == 8)
							p.Print(" " LPrintfInt64, *(int64_t*)v.data.AddressOf());
						else
							LAssert(!"Unknown int size.");
						break;
					}
					case GV_STRING:
					{
						p.Print(" %.*s", (int)MIN(32,v.data.Length()), v.data.AddressOf());
						if (v.data.Length() >= 32)
							p.Print("...");
						break;
					}
					case GV_WSTRING:
					{
						p.Print(" %.*S", (int)MIN(32,v.data.Length()), v.data.AddressOf());
						if (v.data.Length() >= 32)
							p.Print("...");
						break;
					}
					case GV_CUSTOM:
					{
						p.Print(" Object:%s", v.name.Get());
						break;
					}
					case GV_VOID_PTR:
					{
						// p.Print("/Obj");
						break;
					}
					case GV_BINARY:
					{
						p.Print(" (Bin)");
						break;
					}
					default:
					{
						LAssert(!"Unknown type.");
						break;
					}
				}

				if (p.GetSize() > 64)
					break;
			}

			cache = p.NewLStr();
		}

		return cache;
	}

	template<typename T>
	void PrintEscaped(LStringPipe &p, T *s, T *e)
	{
		for (auto c = s; c < e; c++)
		{
			switch (*c)
			{
				case '\\': p.Print("\\\\");  break;
				case '\t': p.Print("\\t");   break;
				case '\r': p.Print("\\r");   break;
				case '\n': p.Print("\\n\n"); break;
				case 0x07: p.Print("\\b");   break;
				case 0x1b: p.Print("\\e");   break;
				default:
					if (*c < ' ' || *c >= 128)
					{
						LString hex;
						hex.Printf("\\x%x", typename std::make_unsigned<T>::type(*c));
						p.Write(hex);
					}
					else
					{
						char ch = (char)*c;
						p.Write(&ch, 1);
					}
					break;

			}
		}
	}

	LString IntToString(Value &v, bool showHex)
	{
		LString s;

		if (v.data.Length() == 1)
		{
			auto i = (uint8_t*) v.data.AddressOf();
			if (showHex)
				s.Printf("%i 0x%x", *i, *i);
			else
				s.Printf("%i", *i);
		}
		else if (v.data.Length() == 2)
		{
			auto i = (uint16_t*) v.data.AddressOf();
			if (showHex)
				s.Printf("%i 0x%x", *i, *i);
			else
				s.Printf("%i", *i);
		}
		else if (v.data.Length() == 4)
		{
			auto i = (uint32_t*) v.data.AddressOf();
			if (showHex)
				s.Printf("%i 0x%x", *i, *i);
			else
				s.Printf("%i", *i);
		}
		else if (v.data.Length() == 8)
		{
			auto i = (int64_t*) v.data.AddressOf();
			if (showHex)
				s.Printf(LPrintfInt64 " 0x" LPrintfHex64, *i, *i);
			else
				s.Printf(LPrintfInt64, *i);
		}
		else
			LAssert(!"Impl me.");

		return s;
	}

	void Select(bool b) override
	{
		if (!GetList()->InThread())
		{
			// Make sure to call this in the right thread...
			GetList()->GetWindow()->PostEvent(M_ENTRY_SELECTED);
			return;
		}

		LAssert(GetList()->InThread());

		LListItem::Select(b);

		auto mode = (DisplayMode)c->tabs->Value();
		auto ctrl = mode ? c->escaped : c->hex;

		if (!b)
			return;

		LStringPipe p;
		LString Obj;
		int Idx = 0;
		bool shorthand = false;

		for (auto &v: values)
		{
			auto sType = LVariant::TypeToString(v.type);
				
			switch (v.type)
			{
				case GV_CUSTOM:
				{
					shorthand = v.name.Equals("LRect") ||
								v.name.Equals("LColour");

					if (shorthand)
						p.Print("object %s {", v.name.Get());
					else
						p.Print("object %s {\n\n", v.name.Get());
					Obj = v.name;
					Idx = 0;

					continue;
				}
				case GV_VOID_PTR:
				{
					if (shorthand)
						p.Print(" }\n");
					else
						p.Print("}\n");
					Obj.Empty();

					shorthand = false;
					continue;
				}
				case GV_STRING:
				{
					if (Obj == "LConsole")
					{
						auto s = (char*)v.data.AddressOf();
						auto e = s + v.data.Length();
						p.Print("[%i]=", Idx++);
						PrintEscaped(p, s, e);
						p.Print("\n");
						continue;
					}
					break;
				}
			}				
			
			if (shorthand)
			{
				switch (v.type)
				{
					case GV_INT32:
					case GV_INT64:
						p.Print(" %s=%s", v.name.Get(), IntToString(v, false).Get());
						break;
					case GV_STRING:
						p.Print(" %s=%.*s", v.name.Get(), (int)v.data.Length(), v.data.AddressOf());
						break;
					default:
						p.Print(" %s=#impl(%s)", v.name.Get(), LVariant::TypeToString(v.type));
						break;
				}
			}
			else
			{
				p.Print("%s, %i bytes%s%s: ",
					sType,
					(int)v.data.Length(),
					v.name ? ", " : "",
					v.name ? v.name.Get() : "");

				switch (v.type)
				{
					case GV_INT32:
					case GV_INT64:
					{
						p.Print("%s\n", IntToString(v, true).Get());
						break;
					}
					case GV_BINARY:
					case GV_STRING:
					{
						p.Print("\n");
						if (mode == DisplayHex)
						{
							char line[300];
							int ch = 0;
						
							const int rowSize = 16;
							const int colHex = 10;
							const int colAscii = colHex + (rowSize * 3) + 2;
							const int colEnd = colAscii + rowSize;

							for (size_t addr = 0; addr < v.data.Length() ; addr += rowSize)
							{
								ZeroObj(line);
								sprintf(line, "%8.8x", (int)addr);
								auto rowBytes = MIN(v.data.Length() - addr, rowSize);
								LAssert(rowBytes <= rowSize);
								auto rowPtr = v.data.AddressOf(addr);
								for (int i=0; i<rowBytes; i++)
								{
									sprintf(line + colHex + (i * 3), "%2.2x", rowPtr[i]);
									line[colAscii + i] = rowPtr[i] >= ' ' && rowPtr[i] < 128 ? rowPtr[i] : '.';
								}
								for (int i=0; i<colEnd; i++)
								{
									if (line[i] == 0)
										line[i] = ' ';
								}
								p.Print("\t%.*s\n", colEnd, line);
							}
						}
						else if (mode == DisplayEscaped)
						{
							auto s = (char*)v.data.AddressOf();
							auto e = s + v.data.Length();
							PrintEscaped(p, s, e);
							if (e > s && e[-1] != '\n')
								p.Write("\n");
						}
						break;
					}
					case GV_WSTRING:
					{
						p.Print("\n");
						if (mode == DisplayHex)
						{
							char line[300];
						
							const int rowSize = 16;
							const int colHex = 10;
							const int colAscii = colHex + (rowSize / 2 * 5) + 2;
							const int colEnd = colAscii + rowSize;

							for (size_t addr = 0; addr < v.data.Length() ; addr += rowSize)
							{
								ZeroObj(line);
								sprintf(line, "%8.8x", (int)addr);
								auto rowBytes = MIN(v.data.Length() - addr, rowSize);
								auto rowWords = rowBytes / sizeof(char16);
								LAssert(rowBytes <= rowSize);
								auto rowPtr = (char16*) v.data.AddressOf(addr);
								for (int i=0; i<rowWords; i++)
								{
									sprintf(line + colHex + (i * 5), "%4.4x", rowPtr[i]);
									line[colAscii + i] = rowPtr[i] >= ' ' && rowPtr[i] < 128 ? rowPtr[i] : '.';
								}
								for (int i=0; i<colEnd; i++)
								{
									if (line[i] == 0)
										line[i] = ' ';
								}
								p.Print("\t%.*s\n", (int)colEnd, line);
							}
						}
						else if (mode == DisplayEscaped)
						{
							auto s = (char16*)v.data.AddressOf();
							auto e = (char16*)(v.data.AddressOf()+v.data.Length());
							PrintEscaped(p, s, e);
							if (e > s && e[-1] != '\n')
								p.Write("\n");
						}
						break;
					}
					default:
					{
						LAssert(!"Impl me.");
						break;
					}
				}

				p.Print("\n");
			}
		}

		ctrl->Name(p.NewLStr());
	}
};

class ReaderThread : public LThread
{
	Context *Ctx;
	LString FileName;
	LList *Lst = NULL;
	Progress *Prog = NULL;

public:
	ReaderThread(Context *ctx,
				const char *filename,
				LList *lst,
				Progress *prog)
		: LThread("ReaderThread")
	{
		Ctx = ctx;
		FileName = filename;
		Lst = lst;
		Prog = prog;

		Run();
	}

	~ReaderThread()
	{
		Prog->Cancel();
		WaitForExit(10000);
	}

	int Main()
	{
		LStructuredLog file(LStructuredLog::TFile, FileName, false);

		LAutoPtr<Entry> cur;
		List<LListItem> items;

		while (file.Read([this, &cur, &items](auto type, auto size, auto ptr, auto name)
			{
				if (cur && type == LStructuredIo::EndRow)
				{
					items.Insert(cur.Release());
					if (items.Length() > 100)
					{
						Lst->Insert(items);
						items.Empty();
					}
					return;
				}

				if (!cur && !cur.Reset(new Entry(Ctx)))
					return;

				cur->add(type, size, ptr, name);
			},	Prog))
			;

		Lst->Insert(items);

		return 0;
	}
};

class App : public LDocApp<LOptionsFile>, public Context
{
	LBox *box = NULL;
	LAutoPtr<ReaderThread> Reader;
	LStatusBar *Status = NULL;
	LProgressStatus *Prog = NULL;
	LAutoPtr<LCommsBus> bus;
	LTabPage *logTab = nullptr;

public:
    App() : LDocApp<LOptionsFile>(AppName)
    {
        Name(AppName);
        LRect r(0, 0, 1100, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (_Create())
        {
			_LoadMenu();
			if (Menu)
			{
				if (auto sub = Menu->AppendSub("&Actions"))
				{
					sub->AppendItem("Clear", ID_CLEAR, true, -1, "F5");
				}
			}

			AddView(Status = new LStatusBar(ID_STATUS));
			Status->AppendPane("Some text");
			Status->AppendPane(Prog = new LProgressStatus());
			Prog->GetCss(true)->Width("200px");
			Prog->GetCss()->TextAlign(LCss::AlignRight);

			AddView(box = new LBox(ID_BOX));
			box->AddView(lst = new LList(ID_LIST, 0, 0, 200, 200));
			lst->GetCss(true)->Width("40%");
			lst->ColumnHeaders(false);
			lst->AddColumn("Items", 1000);

			box->AddView(tabs = new LTabView(ID_TABS));

			if (auto tab = tabs->Append("Hex"))
			{
				tab->Append(hex = new LTextView3(ID_HEX));
				hex->Sunken(true);
				hex->SetPourLargest(true);
			}

			if (auto tab = tabs->Append("Escaped"))
			{
				tab->Append(escaped = new LTextView3(ID_ESCAPED));
				escaped->Sunken(true);
				escaped->SetPourLargest(true);
			}

			if (logTab = tabs->Append("Log"))
			{
				logTab->Append(log = new LTextLog(ID_LOG));
			}

			AttachChildren();
            Visible(true);

			if (bus.Reset(new LCommsBus(log)))
			{
				bus->SetCallback([this](auto state)
					{
						PostEvent(M_COMMS_STATE, (LMessage::Param)state);
					});

				bus->Listen(
					LStructuredLog::sClearEndpoint,
					[this](auto msg)
					{
						ClearLogs();
					});

				bus->Listen(
					LStructuredLog::sDefaultEndpoint,
					[this](auto msg)
					{
						// Note: as the list control is thread safe, this does NOT need to be
						// running in the window's thread.

						// Incoming comms message, parse and show in the UI
						LStructuredLog log(LStructuredLog::TData, msg, false);

						LAutoPtr<Entry> cur;
						List<LListItem> items;

						while
						(
							log.Read
							(
								[this, &cur, &items](auto type, auto size, auto ptr, auto name)
								{
									if (cur && type == LStructuredIo::EndRow)
									{
										items.Insert(cur.Release());
										if (items.Length() > 100)
										{
											lst->Insert(items);
											items.Empty();
										}
										return;
									}
									
									if (!cur && !cur.Reset(new Entry(this)))
										return;

									cur->add(type, size, ptr, name);
								},
								Prog
							)
						);

						lst->Insert(items);
					});
			}
        }
    }

	LMessage::Result OnEvent(LMessage *m)
	{
		switch (m->Msg())
		{
			case M_ENTRY_SELECTED:
			{
				if (!lst)
					break;

				LArray<Entry*> sel;
				if (lst->GetSelection(sel))
				{
					sel[0]->Select(true);
				}
				break;
			}
			case M_COMMS_STATE:
			{
				auto state = (LCommsBus::TState)m->A();
				if (logTab)
				{
					LCss::ColorDef c;
					switch (state)
					{
						case LCommsBus::TDisconnectedClient:
							c = LCss::ColorDef(LColour::Red);
							log->Print("Disconnected client...\n");
							break;
						case LCommsBus::TDisconnectedServer:
							c = LCss::ColorDef(LColour(255, 128, 0));
							log->Print("Disconnected server...\n");
							break;
						case LCommsBus::TConnectedClient:
						case LCommsBus::TConnectedServer:
							c = LCss::ColorDef(LColour::Green);
							log->Print("Connected...\n");
							break;
					}
					logTab->GetCss(true)->Color(c);
					tabs->Invalidate();
				}
				break;
			}
		}

		return LDocApp<LOptionsFile>::OnEvent(m);
	}

	void ClearLogs()
	{
		if (!InThread())
		{
			// Make sure this is running in the window's thread
			RunCallback(
				[this]()
				{
					ClearLogs();
				},
				_FL);
			return;
		}

		if (lst)
			lst->Empty();
		if (hex)
			hex->Name(NULL);
		if (escaped)
			escaped->Name(NULL);
		if (log)
			log->Name(NULL);
	}

	int OnCommand(int Cmd, int Event, OsView Wnd) override
	{
		switch (Cmd)
		{
			case ID_CLEAR:
			{
				ClearLogs();
				break;
			}
		}

		return 0;
	}

	void OnReceiveFiles(LArray<const char*> &Files)
	{
		if (Files.Length())
			OpenFile(Files[0], false, NULL);
	}	

	void OpenFile(const char *FileName, bool ReadOnly, std::function<void(bool status)> Callback)
	{
		auto ok = Reader.Reset(new ReaderThread(this, FileName, lst, Prog));
		if (Callback)
			Callback(ok);
	}

	void SaveFile(const char *FileName, std::function<void(LString fileName, bool status)> Callback)
	{
		if (Callback)
			Callback(FileName, false);
	}

	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		if (Ctrl->GetId() == ID_TABS)
		{
			auto item = lst->GetSelected();
			if (item)
				item->Select(true);
		}

		return 0;
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

