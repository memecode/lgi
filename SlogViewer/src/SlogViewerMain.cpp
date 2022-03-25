#define _CRT_SECURE_NO_WARNINGS
#include "lgi/common/Lgi.h"
#include "lgi/common/OptionsFile.h"
#include "lgi/common/DocApp.h"
#include "lgi/common/Box.h"
#include "lgi/common/List.h"
#include "lgi/common/TextView3.h"
#include "lgi/common/TabView.h"
#include "lgi/common/StructuredLog.h"

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
	IDC_BOX = 100,
	IDC_LOG,
	IDC_HEX,
	IDC_ESCAPED,
	IDC_TABS,
};

struct Context
{
	LList *log = NULL;
	LTabView *tabs = NULL;
	LTextView3 *hex = NULL;
	LTextView3 *escaped = NULL;
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
							p.Print("%i", *(int*)v.data.AddressOf());
						else if (v.data.Length() == 8)
							p.Print(LPrintfInt64, *(int64_t*)v.data.AddressOf());
						else
							LAssert(!"Unknown int size.");
						break;
					}
					case GV_STRING:
					{
						if (v.data.Length() >= 32)
							p.Print(" (...)");
						else
							p.Print("%.*s", (int)v.data.Length(), v.data.AddressOf());
						break;
					}
					default:
					{
						LAssert(!"Unknown type.");
						break;
					}
				}
			}

			cache = p.NewGStr();
		}

		return cache;
	}

	void Select(bool b) override
	{
		LListItem::Select(b);

		auto mode = (DisplayMode)c->tabs->Value();
		auto ctrl = mode ? c->escaped : c->hex;

		if (b)
		{
			LStringPipe p;

			for (auto &v: values)
			{
				auto sType = LVariant::TypeToString(v.type);
				p.Print("%s, %i bytes:\n", sType, (int)v.data.Length());
				switch (v.type)
				{
					case GV_INT32:
					case GV_INT64:
					{
						if (v.data.Length() == 4)
						{
							auto i = (int*) v.data.AddressOf();
							p.Print("%i 0x%x\n", *i, *i);
						}
						else if (v.data.Length() == 8)
						{
							auto i = (int64_t*) v.data.AddressOf();
							p.Print(LPrintfInt64 " 0x" LPrintfHex64 "\n", *i, *i);
						}
						else
							LAssert(!"Impl me.");
						break;
					}
					case GV_STRING:
					{
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
								sprintf(line, "%08.8x", (int)addr);
								auto rowBytes = MIN(v.data.Length() - addr, rowSize);
								LAssert(rowBytes <= rowSize);
								auto rowPtr = v.data.AddressOf(addr);
								for (int i=0; i<rowBytes; i++)
								{
									sprintf(line + colHex + (i * 3), "%02.2x", rowPtr[i]);
									line[colAscii + i] = rowPtr[i] >= ' ' && rowPtr[i] < 128 ? rowPtr[i] : '.';
								}
								for (int i=0; i<colEnd; i++)
								{
									if (line[i] == 0)
										line[i] = ' ';
								}
								p.Print("%.*s\n", colEnd, line);
							}
						}
						else if (mode == DisplayEscaped)
						{
							auto s = v.data.AddressOf();
							auto e = s + v.data.Length();

							for (auto c = s; c < e; c++)
							{
								switch (*c)
								{
									case '\\': p.Print("\\\\"); break;
									case '\t': p.Print("\\t"); break;
									case '\r': p.Print("\\r"); break;
									case '\n': p.Print("\\n\n"); break;
									case 0x07: p.Print("\\b"); break;
									case 0x1b: p.Print("\\e"); break;
									default:
										if (*c < ' ' || *c >= 128)
											p.Write("\\x%x", (uint8_t)*c);
										else
											p.Write(c, 1);
										break;

								}
							}
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

			ctrl->Name(p.NewGStr());
		}
	}
};

class App : public LDocApp<LOptionsFile>, public Context
{
	LBox *box = NULL;

public:
    App() : LDocApp<LOptionsFile>(AppName)
    {
        Name(AppName);
        LRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (_Create())
        {
			_LoadMenu();

			AddView(box = new LBox(IDC_BOX));
			box->AddView(log = new LList(IDC_LOG, 0, 0, 200, 200));
			log->GetCss(true)->Width("40%");
			log->ShowColumnHeader(false);
			log->AddColumn("Items", 1000);

			box->AddView(tabs = new LTabView(IDC_TABS));

			auto tab = tabs->Append("Hex");
			tab->Append(hex = new LTextView3(IDC_HEX));
			hex->Sunken(true);
			hex->SetPourLargest(true);

			tab = tabs->Append("Escaped");
			tab->Append(escaped = new LTextView3(IDC_ESCAPED));
			escaped->Sunken(true);
			escaped->SetPourLargest(true);

			AttachChildren();
            Visible(true);
        }
    }

	void OnReceiveFiles(LArray<const char*> &Files)
	{
		if (Files.Length())
			OpenFile(Files[0]);
	}	

	bool OpenFile(const char *FileName, bool ReadOnly = false)
	{
		LStructuredLog file(FileName, false);

		LAutoPtr<Entry> cur;
		List<LListItem> items;

		while (file.Read([this, &cur, &items](auto type, auto size, auto ptr, auto name)
			{
				if (type == LStructuredIo::EndRow)
				{
					items.Insert(cur.Release());
					return;
				}

				if (!cur && !cur.Reset(new Entry(this)))
					return;

				cur->add(type, size, ptr, name);
			}))
			;

		log->Insert(items);
		return true;
	}

	bool SaveFile(const char *FileName)
	{
		return false;
	}

	int OnNotify(LViewI *Ctrl, LNotification n)
	{
		if (Ctrl->GetId() == IDC_TABS)
		{
			auto item = log->GetSelected();
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

