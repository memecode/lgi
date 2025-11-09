#include "lgi/common/Lgi.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Button.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Css.h"
#include "lgi/common/DbTable.h"
#include "lgi/common/XmlTree.h"
#include "lgi/common/TabView.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/DropFiles.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/Box.h"

const char *AppName = "Lgi Test App";

enum Ctrls
{
	IDC_EDIT1 = 100,
	IDC_EDIT2,
	IDC_BLT_TEST,
	IDC_TXT,
	ID_BOX,
	ID_CLICK_TEST,
	ID_LOG
};

void LStringTest()
{
	LString a("This<TD>is<TD>a<TD>test");
	a.RFind("<TD>");
	LString f = a(8,10);
	LString end = a(-5, -1);
	
	LString sep(", ");
	LString::Array parts = LString("This is a test").Split(" ");
	LString joined = sep.Join(parts);
	
	LString src("  asdwer   ");
	LString left = src.LStrip();
	LString right = src.RStrip();
	LString both = src.Strip();
}

LColourSpace RgbColourSpaces[] =
{
	/*
	CsIndex1,
	CsIndex4,
	CsIndex8,
	CsAlpha8,
	*/
	CsRgb15,
	CsBgr15,
	CsRgb16,
	CsBgr16,
	CsRgb24,
	CsBgr24,
	CsRgbx32,
	CsBgrx32,
	CsXrgb32,
	CsXbgr32,
	CsRgba32,
	CsBgra32,
	CsArgb32,
	CsAbgr32,
	CsBgr48,
	CsRgb48,
	CsBgra64,
	CsRgba64,
	CsAbgr64,
	CsArgb64,
	/*
	CsHls32,
	CsCmyk32,
	*/
	CsNone,
};

class BltTest : public LWindow
{
	struct Test
	{
		LColourSpace Src, Dst;
		LMemDC Result;
		
		Test() : Result(_FL)
		{
		}

		void Create()
		{
			if (!Result.Create(16, 16, Dst))
				return;
			for (int y=0; y<Result.Y(); y++)
			{
				Result.Colour(Rgb24(y * 16, 0, 0), 24);
				for (int x=0; x<Result.X(); x++)
				{
					Result.Set(x, y);
				}
			}
			
			LMemDC SrcDC(_FL);
			if (!SrcDC.Create(16, 16, Src))
				return;
			
			// Result.Blt(0, 0, &SrcDC);
		}
	};
	
	LArray<Test*> a;

public:
	BltTest()
	{
		Name("BltTest");
		LRect r(0, 0, 1200, 1000);
		SetPos(r);
		MoveToCenter();
		
		if (Attach(0))
		{
			Visible(true);
			
			for (int Si=0; RgbColourSpaces[Si]; Si++)
			{
				for (int Di=0; RgbColourSpaces[Di]; Di++)
				{
					Test *t = new Test;
					t->Src = RgbColourSpaces[Si];
					t->Dst = RgbColourSpaces[Di];
					t->Create();
					a.Add(t);
				}
			}
		}
	}
	
	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(L_WORKSPACE);
		pDC->Rectangle();
		
		int x = 10;
		int y = 10;
		for (unsigned i=0; i<a.Length(); i++)
		{
			char s[256];
			Test *t = a[i];
			sprintf_s(s, sizeof(s), "%s->%s", LColourSpaceToString(t->Src), LColourSpaceToString(t->Dst));
			LDisplayString ds(LSysFont, s);
			ds.Draw(pDC, x, y);
			y += ds.Y();
			if (t->Result.Y())
			{
				pDC->Blt(x, y, &t->Result);
				y += t->Result.Y();
			}
			y += 10;
			if (Y() - y < 100)
			{
				y = 10;
				x += 150;
			}
		}
	}
};

class DnDtarget : public LTextLog
{
	bool first = true;
	
public:
	const char *GetClass() const { return "DnDtarget"; }

	DnDtarget() : LTextLog(10)
	{
		Print("%s...\n", GetClass());
		DropTarget(true);
	}

	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState)
	{
		if (first)
		{
			first = false;
			for (auto &f: Formats.GetAll())
				Print("%s: %s\n", __func__, f.Get());
		}

		// Set them all to supported... that was the drop can enumerate things:
		for (auto &f: Formats.GetAll())
		{
			if (!f.Equals("DELETE"))
				Formats.Supports(f);
		}
		
		return DROPEFFECT_COPY;
	}

	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState)
	{
		LString Keys;
		if (KeyState & LGI_EF_CTRL)
			Keys += "LGI_EF_CTRL ";
		if (KeyState & LGI_EF_ALT)
			Keys += "LGI_EF_ALT ";
		if (KeyState & LGI_EF_SHIFT)
			Keys += "LGI_EF_SHIFT ";

		Print("OnDrop(Data, ms={%i,%i}, keys={%s})\n", Pt.x, Pt.y, Keys?Keys.Get():"");
		for (unsigned i=0; i<Data.Length(); i++)
		{
			auto &d = Data[i];
			Print("\t[%i] fmt='%s'\n", i, d.Format.Get());
			for (unsigned n=0; n<d.Data.Length(); n++)
			{
				auto &v = d.Data[n];
				if (d.IsFileDrop())
				{
					LDropFiles df(d);
					Print("\t\t%i files:\n", (int)df.Length());
					for (unsigned f=0; f<df.Length(); f++)
					{
						Print("\t\t[%i]='%s'\n", f, df[f]);
					}
				}
				else
				{
					Print("\t\t[%i]=%s\n", n, v.ToString().Get());
				}
			}
		}

		return DROPEFFECT_COPY;
	}
};

struct OriginTest : public LView
{
	int offset = 0;

	const char *GetClass() { return "OriginTest"; }

	OriginTest(int x, int y, int off)
	{
		#ifdef _DEBUG
		// _Debug = true;
		#endif

		LRect r(0, 0, 99, 99);
		r.Offset(x, y);
		SetPos(r);

		offset = off;
	}

	void OnPaint(LSurface *pDC)
	{
		auto c = GetClient();
		// printf("c=%s off=%i dc=%i,%i\n", GetPos().GetStr(), offset, pDC->X(), pDC->Y());
		pDC->Colour(L_WHITE);
		pDC->Rectangle();

		pDC->SetOrigin(0, offset);

		pDC->Colour(L_TEXT);
		pDC->Box(10, 10, 30, 20);

		LDisplayString ds(LSysFont, "abcdefg");
		LSysFont->Colour(L_TEXT, L_HIGH);
		LSysFont->Transparent(false);
		ds.Draw(pDC, 40, 10);
	}
};

struct RepaintTest : public LView
{
	uint64_t ts = 0;

	RepaintTest(int x, int y)
	{
		LRect r(x, y, x + 100, y + 50);
		SetPos(r);
		ts = LCurrentTime();
	}
	
	void OnCreate()
	{
		// printf("onCreate called.\n");
		SetPulse(1000);
	}
	
	void OnPulse()
	{
		// printf("onPulse..\n");
		Invalidate();
	}
	
	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(L_WORKSPACE);
		pDC->Rectangle();
		
		auto fnt = GetFont();
		auto diff = LCurrentTime() - ts;
		auto s = LString::Fmt(LPrintfInt64, diff);
		LDisplayString ds(fnt, s);
		fnt->Transparent(true);
		fnt->Fore(L_TEXT);
		ds.Draw(pDC, 5, 5);
	}
};

class ClickTest : public LView
{
public:
	LTextLog *log = nullptr;

	ClickTest(int id)
	{
		SetId(id);
	}

	void OnMouseClick(LMouse &m) override
	{
		if (!log)
			return;

		log->Print("Click: %s\n", m.ToString().Get());
	}

	void OnPaint(LSurface *pDC) override
	{
		pDC->Colour(L_LOW);
		pDC->Rectangle();
	}
};

class App : public LWindow
{
	LOptionsFile Opts;
	LBox *box = nullptr;
	LTextLog *log = nullptr;
	LEdit *e = NULL;
	LEdit *e2 = NULL;
	LTextLabel *Txt = NULL;
	LTableLayout *Tbl = NULL;

public:
	const char *GetClass() override { return "App"; }

	App() : Opts(LOptionsFile::PortableMode, AppName)
	{
		LRect r(0, 0, 1000, 800);
		SetPos(r);
		Name(AppName);
		MoveToCenter();
		SetQuitOnClose(true);

		Opts.SerializeFile(false);
		SerializeState(&Opts, "WndState", true);
		
		if (Attach(0))
		{
			#if 0

				ClickTest *clicks;
				AddView(box = new LBox(ID_BOX));
				box->AddView(clicks = new ClickTest(ID_CLICK_TEST));
				box->AddView(clicks->log = log = new LTextLog(ID_LOG));

			#elif 0

				// AddView(new OriginTest(20, 10, 0));
				AddView(new RepaintTest(20, 100));

			#elif 1

				LTabView *t = new LTabView(100);
				t->Attach(this);
				t->GetCss(true)->Padding("6px");

				auto *tab = t->Append("First");
				tab->GetCss(true)->FontStyle(LCss::FontStyleItalic);
				tab->Append(new DnDtarget());
				
				tab = t->Append("Second");
				tab->GetCss(true)->FontSize("14pt");
				
				tab = t->Append("Third");
				tab->GetCss(true)->Color("red");
				t->OnStyleChange();

			#elif 0

				AddView(Tbl = new LTableLayout(100));
				GLayoutCell *c = Tbl->GetCell(0, 0);

				c->Add(Txt = new LTextLabel(IDC_TXT, 0, 0, -1, -1, "This is a test string. &For like\ntesting and stuff. "
																	"It has multiple\nlines to test wrapping."));
				Txt->SetWrap(true);
				//Txt->GetCss(true)->Color(LCss::ColorDef(LColour::Red));
				// Txt->GetCss(true)->FontWeight(LCss::FontWeightBold);
				// Txt->GetCss(true)->FontStyle(LCss::FontStyleItalic);
				Txt->GetCss(true)->FontSize(LCss::Len("22pt"));
				Txt->OnStyleChange();

				c = Tbl->GetCell(1, 0);
				c->Add(new LEdit(IDC_EDIT1, 0, 0, -1, -1));

			#elif 0

				AddView(e = new LEdit(IDC_EDIT1, 10, 10, 200, 22));
				AddView(e2 = new LEdit(IDC_EDIT1, 10, 50, 200, 22));
				AddView(new LButton(IDC_BLT_TEST, 10, 200, -1, -1, "Blt Test"));
				// e->Focus(true);
				e->Password(true);
				e->SetEmptyText("(this is a test)");

			#endif
			
			AttachChildren();
			// Debug();
			Visible(true);
		}
	}

	~App()
	{
		SerializeState(&Opts, "WndState", false);
		Opts.SerializeFile(true);
	}

	void OnPaint(LSurface *pDC) override
	{
		auto c = GetClient();
		// printf("	LWindow::OnPaint %s %i,%i\n", c.GetStr(), pDC->X(), pDC->Y());
		pDC->Colour(L_MED);
		pDC->Rectangle();
		
		#if 1
		pDC->Colour(LColour::Red);
		pDC->Line(0, 0, c.X()-1, c.Y()-1);
		#endif
	}
	
	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		switch (Ctrl->GetId())
		{
			case IDC_BLT_TEST:
				new BltTest();
				break;
		}
		
		return 0;
	}
};

enum EmailFields
{
	M_UID = 100,
	M_FLAGS,
	M_DATE,
	M_LABEL,
	M_COLOUR,
	M_SIZE,
	M_FILENAME,
};

bool DbTesting()
{
	// LDbTable::UnitTests();

	#if 1
	const char *BaseFolder = "C:\\Users\\Matthew\\AppData\\Roaming\\Scribe\\ImapCache\\1434419972\\INBOX";
	#else
	const char *BaseFolder = "C:\\Users\\matthew\\AppData\\Roaming\\Scribe\\ImapCache\\378651814\\INBOX";
	#endif

	LFile::Path FileXml(BaseFolder, "Folder.xml");
	LFile::Path FileDb(BaseFolder, "Folder.db");
	LFile::Path FileDebug(BaseFolder, "Debug.txt");
	LFile In;
	if (!In.Open(FileXml, O_READ))
		return false;	
	
	// Read XML
	LXmlTag r;
	LXmlTree t;
	uint64 Start = LMicroTime();
	if (!t.Read(&r, &In))
		return false;
	uint64 XmlReadTime = LMicroTime() - Start;


	// Convert XML to DB
	LDbTable Tbl;
	Tbl.AddField(M_UID, GV_INT32);
	Tbl.AddField(M_FLAGS, GV_INT32);
	Tbl.AddField(M_DATE, GV_DATETIME);
	Tbl.AddField(M_LABEL, GV_STRING);
	Tbl.AddField(M_COLOUR, GV_INT32);
	Tbl.AddField(M_SIZE, GV_INT64);
	Tbl.AddField(M_FILENAME, GV_STRING);

	Start = LMicroTime();
	LXmlTag *Emails = r.GetChildTag("Emails");
	if (!Emails)
		return false;
	for (auto c: Emails->Children)
	{
		LDbRow *m = Tbl.NewRow();
		m->SetInt(M_UID, c->GetAsInt("Uid"));
		
		ImapMailFlags Flgs;
		Flgs.Set(c->GetAttr("Flags"));
		m->SetInt(M_FLAGS, Flgs.All);
		
		char *Date = c->GetAttr("Date");
		if (Date)
		{
			LDateTime Dt;
			if (Dt.Decode(Date))
			{
				if (!m->SetDate(M_DATE, &Dt))
				{
					LAssert(0);
				}
			}
			else
			{
				LAssert(0);
			}
		}
		else LAssert(0);
		
		m->SetStr(M_LABEL, c->GetAttr("Label"));
		m->SetInt(M_COLOUR, c->GetAsInt("Colour"));
		m->SetInt(M_SIZE, Atoi(c->GetAttr("Size")));
		m->SetStr(M_FILENAME, LString(c->GetContent()).Strip());
	}
	uint64 ConvertTime = LMicroTime() - Start;

	Start = LMicroTime();
	if (!Tbl.Serialize(FileDb, true))
		return false;
	uint64 WriteTime = LMicroTime() - Start;

	LDbTable Test;
	Start = LMicroTime();
	if (!Test.Serialize(FileDb, false))
		return false;
	uint64 ReadTime = LMicroTime() - Start;

	Start = LMicroTime();
	LAutoPtr<DbArrayIndex> Idx(Test.Sort(M_FILENAME));
	uint64 SortTime = LMicroTime() - Start;
	if (Idx)
	{
		LFile Out;
		if (Out.Open(FileDebug, O_WRITE))
		{
			Out.SetSize(0);
			for (unsigned i=0; i<Idx->Length(); i++)
			{
				LString s = Idx->ItemAt(i)->ToString();
				s += "\n";
				Out.Write(s);
			}
			Out.Close();
		}
	}

	LgiTrace("DbTest: %i -> %i\n"
		"\tXmlReadTime=%.3f\n"
		"\tConvertTime=%.3f\n"
		"\tWriteTime=%.3f\n"
		"\tReadTime=%.3f\n"
		"\tSortTime=%.3f\n",
		Tbl.GetRows(), Test.GetRows(),
		(double)XmlReadTime/1000.0,
		(double)ConvertTime/1000.0,
		(double)WriteTime/1000.0,
		(double)ReadTime/1000.0,
		(double)SortTime/1000.0);

	return true;
}


int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, "gui_test");
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}
	return 0;
}
