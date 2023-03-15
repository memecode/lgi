#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/Edit.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/List.h"
#include "lgi/common/Splitter.h"
#include "lgi/common/FileSelect.h"
#include "LgiIde.h"

#define IDC_LIST 100

class DumpItem : public LListItem
{
public:
	int Size;
	int Count;
	char *Alloc;
	char *Stack;

	DumpItem()
	{
		Size = Count = 0;
		Alloc = Stack = 0;
	}

	~DumpItem()
	{
		DeleteArray(Alloc);
		DeleteArray(Stack);
	}

	const char *GetText(int c)
	{
		static char s[64];
		switch (c)
		{
			case 0:
			{
				LFormatSize(s, sizeof(s), Size);
				return s;
				break;
			}
			case 1:
			{
				return Alloc;
				break;
			}
			case 2:
			{
				sprintf(s, "%i", Count);
				return s;
				break;
			}
		}

		return 0;
	}
};

int Cmp(LListItem *A, LListItem *B, NativeInt d)
{
	DumpItem *a = (DumpItem*) A;
	DumpItem *b = (DumpItem*) B;

	switch (d)
	{
		case 0:
		{
			return b->Size - a->Size;
		}
		case 1:
		{
			return stricmp(a->Alloc, b->Alloc);
		}
		case 2:
		{
			return b->Count - a->Count;
		}
	}

	return 0;
}

/*
char *Strnstr(char *s, const char *find, int len)
{
    if (!s || !find || len < 0)
        return 0;

    char *End = s + len;
    auto FindLen = strlen(find);
    while (s < End)
    {
        if (*s == *find)
        {
            bool match = true;
            char *to = s + FindLen;
            if (to > End)
                return 0;
            
            for (int n=1; n<FindLen; n++)
            {
                if (s[n] != find[n])
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return s;
        }
        s++;
    }
    
    return 0;
}
*/

class DumpView : public LWindow
{
	AppWnd *App;
	LList *Lst;
	LEdit *Ed;

public:
	DumpView(AppWnd *app, const char *file)
	{
		App = app;
		Name("Memory Dump Viewer");
		LRect r(0, 0, 800, 600);
		SetPos(r);
		MoveToCenter();
		if (Attach(0))
		{
			LSplitter *Split;
			Children.Insert(Split = new LSplitter);
			Split->Value(400);
			Split->IsVertical(false);

			Split->SetViewA(Lst = new LList(IDC_LIST, 0, 0, 100, 100), false);
			Lst->AddColumn("Size", 200);
			Lst->AddColumn("Location", 300);
			Lst->AddColumn("Count", 100);

			Split->SetViewB(Ed = new LEdit(101, 0, 0, 100, 100, ""), false);
			Ed->Enabled(false);
			Ed->MultiLine(true);

			AttachChildren();
			Visible(true);

			if (file)
				Load(file);
			else
			{
				LFileSelect *s = new LFileSelect;
				s->Parent(this);
				s->Type("Dump", "*.mem");
				s->Type("All Files", LGI_ALL_FILES);
				s->Open([&](auto s, auto ok)
				{
					if (ok)
						Load(s->Name());
					delete s;
				});
			}
		}
	}

	int OnNotify(LViewI *c, LNotification n)
	{
		switch (c->GetId())
		{
			case IDC_LIST:
			{
				switch (n.Type)
				{
					case LNotifyItemSelect:
					{
						LListItem *s = Lst->GetSelected();
						if (s)
						{
							DumpItem *di = dynamic_cast<DumpItem*>(s);
							if (di && Ed)
							{
								Ed->Name(di->Stack);
							}
						}
						break;
					}
					case LNotifyItemColumnClicked:
					{
						int Col;
						LMouse m;
						if (Lst->GetColumnClickInfo(Col, m))
						{
							Lst->Sort<NativeInt>(Cmp, Col);
						}
						break;
					}
					default:
						break;
				}
				break;
			}
		}

		return 0;
	}

	void Load(const char *File)
	{
		LHashTbl<ConstStrKey<char,false>, bool> Except(0, false);
		Except.Add("LString.cpp", true);
		Except.Add("LVariant.cpp", true);
		Except.Add("LContainers.cpp", true);
		Except.Add("LContainers.h", true);
		Except.Add("LFile.cpp", true);
		Except.Add("Mail.h", true);
		Except.Add("LArray.h", true);

		LFile f;
		if (!f.Open(File, O_READ))
			LgiMsg(this, "Couldn't read '%s'", AppName, MB_OK, File);
		else
		{
			LProgressDlg Prog(this, true);
			LArray<char> Buf;
			Buf.Length(1 << 20);
			ssize_t Pos = 0, Used = 0;
			bool First = true;
			char s[512];

			LHashTbl<StrKeyPool<char,false>,DumpItem*> h;

			Prog.SetDescription("Reading memory dump...");
			Prog.SetRange(f.GetSize());
			Prog.SetScale(1.0 / 1024.0 / 1024.0);
			Prog.SetType("MB");

			while (true)
			{
				// Consume data
				auto Len = Used - Pos;
				char *Cur = &Buf[Pos];
				char *End = Strnstr(Cur, "\r\n\r\n", Len);
				if (End)
				{
					if (First)
					{
						First = false;
						LToken t(Cur, " \t\r\n", true, End-Cur);
						char *Blocks = t[0];
						if (Blocks)
						{
							char *c = strchr(Blocks, ':');
							if (c) *c = 0;
							sprintf(s, "%s (%s)", File, t[0]);
							Name(s);
						}
						else break;
					}
					else
					{
						int Size = 0;
						LToken Lines(Cur, "\r\n", true, End - Cur);
						LArray<char*> Stack;
						for (int i=0; i<Lines.Length(); i++)
						{
							char *n = Lines[i];
							if (*n == '\t')
							{
								Stack.Add(n);
							}
							else
							{
								if (strnicmp(n, "Block ", 6) == 0)
								{
									char *c = strchr(n, ',');
									if (c)
									{
										c++;
										while (*c == ' ') c++;

										Size = atoi(c);
									}
								}
							}
						}

						if (Size && Stack.Length() > 0)
						{
							char *Alloc = 0;

							for (int k=0; k<Stack.Length(); k++)
							{
								char *sp = strchr(Stack[k], ' ');
								if (sp++)
								{
									char *col = strrchr(sp, ':');
									if (col) *col = 0;

									char *f = strrchr(sp, DIR_CHAR);
									if (!f) f = sp;
									else f++;

									if (!Except.Find(f))
									{
										if (col)
											*col = ':';
										Alloc = f;
										break;
									}

									*col = ':';
								}
							}

							if (Alloc)
							{
								DumpItem *di = (DumpItem*) h.Find(Alloc);
								if (!di)
								{
									di = new DumpItem;
									if (di)
									{
										h.Add(Alloc, di);
										di->Alloc = NewStr(Alloc);

										LStringPipe p;
										for (int k=0; k<Stack.Length(); k++)
										{
											p.Print("%s\n", Stack[k] + 1);
										}

										di->Stack = p.NewStr();
									}
								}
								if (di)
								{
									di->Count++;
									di->Size += Size;
								}
							}
							else
							{
								LAssert(0);
							}

							Stack.Length(0);
							Size = 0;
						}
					}

					Pos = End - &Buf[0] + 4;
				}
				else
				{
					// Update status
					Prog.Value(f.GetPos());

					// Read more data in?
					memmove(&Buf[0], &Buf[Pos], Used - Pos);
					Used -= Pos;
					Pos = 0;
					auto r = f.Read(&Buf[Used], Buf.Length() - Used);
					if (r <= 0)
						break;
					Used += r;
				}
			}

			List<LListItem> Items;
			// for (void *p = h.First(); p; p = h.Next())
			for (auto p : h)
			{
				Items.Insert((DumpItem*)p.value);
			}
			Lst->Insert(Items);
			Lst->Sort(Cmp);
		}
	}
};

void NewMemDumpViewer(AppWnd *App, const char *File)
{
	new DumpView(App, File);
}
