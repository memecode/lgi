#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Button.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/OptionsFile.h"
#include "lgi/common/XmlTreeUi.h"

enum Ctrls
{
	IDC_STATIC = -1,
	IDC_BOX1 = 100,
	IDC_BOX2,
	IDC_IN,
	IDC_OUT,
	IDC_TBL,
	IDC_PATCH_FILE,
	IDC_BROWSE_PATCH,
	IDC_BASE_DIR,
	IDC_BROWSE_BASE,
};

template<typename T>
ssize_t lineLen(T *start)
{
	auto s = start;
	for (; *s && *s != '\n'; s++);
	return s - start;
}

class PatchView : public LTextLog
{
public:
	PatchView(int id) : LTextLog(id)
	{
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		for (auto l: Line)
		{
			auto s = Text + l->Start;
			if (*s == '+' && s[1] != '+')
			{
				l->c = LColour::Green;
			}
			else if (*s == '-' && s[1] != '-')
			{
				l->c = LColour::Red;
			}
			else if (*s == '@' && s[1] == '@')
			{
				l->c.Rgb(128, 128, 128);
				l->Back.Rgb(222, 222, 222);
			}
		}
	}
};

class FileView : public LTextLog
{
public:
	LArray<LColour> Fore, Back;

	FileView(int id) : LTextLog(id)
	{
	}

	bool Open(const char *Name, const char *Cs = NULL)
	{
		Fore.Length(0);
		Back.Length(0);
		return LTextView3::Open(Name, Cs);
	}

	void Reset(const char16 *t)
	{
		NameW(t);
		Fore.Length(0);
		Back.Length(0);
	}

	void Update()
	{
		PourStyle(0, Size);
		Invalidate();
	}

	void PourStyle(size_t Start, ssize_t Length)
	{
		for (size_t i=0; i<Line.Length(); i++)
		{
			auto l = Line[i];
			auto s = Text + l->Start;

			if (Fore.IdxCheck(i))
			{
				auto &c = Fore[i];
				if (c.IsValid())
					l->c = c;
			}
			if (Back.IdxCheck(i))
			{
				auto &c = Back[i];
				if (c.IsValid())
					l->Back = c;
			}
		}
	}
};

class PatchViewer : public LWindow, public LXmlTreeUi
{
	LBox *box1 = NULL;
	LBox *box2 = NULL;
	FileView *in = NULL;
	LTextLog *out = NULL;
	LTableLayout *tbl = NULL;
	LOptionsFile *Opts = NULL;

public:
	PatchViewer(LViewI *Parent, LOptionsFile *opts) : Opts(opts)
	{
		LRect r(0, 0, 1600, 1000);
		SetPos(r);
		Name("Patcher");
		MoveToCenter();

		Map("PatchFile", IDC_PATCH_FILE);
		Map("PatchBaseDir", IDC_BASE_DIR);

		if (!Attach(0))
			return;

		AddView(box1 = new LBox(IDC_BOX1, true));			
		box1->AddView(tbl = new LTableLayout(IDC_TBL));
		tbl->GetCss(true)->Height("4em");
			
		int y = 0;
		auto c = tbl->GetCell(0, y);
			c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Patch file:"));
			c->VerticalAlign(LCss::VerticalMiddle);
			c->PaddingLeft("0.5em");
			c->PaddingTop("0.5em");
		c = tbl->GetCell(1, y);
			c->Add(new LEdit(IDC_PATCH_FILE, 0, 0, 60, 20));
			c->PaddingTop("0.5em");
		c = tbl->GetCell(2, y++);
			c->Add(new LButton(IDC_BROWSE_PATCH, 0, 0, -1, -1, "..."));
			c->PaddingRight("0.5em");
			c->PaddingTop("0.5em");

		c = tbl->GetCell(0, y);
			c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Base folder:"));
			c->VerticalAlign(LCss::VerticalMiddle);
			c->PaddingLeft("0.5em");
		c = tbl->GetCell(1, y);
			c->Add(new LEdit(IDC_BASE_DIR, 0, 0, 60, 20));
		c = tbl->GetCell(2, y);
			c->Add(new LButton(IDC_BROWSE_BASE, 0, 0, -1, -1, "..."));
			c->PaddingRight("0.5em");

		box1->AddView(box2 = new LBox(IDC_BOX2));
		box2->AddView(in = new FileView(IDC_IN));
		box2->AddView(out = new PatchView(IDC_OUT));

		Convert(Opts, this, true);
		AttachChildren();
		Visible(true);

		in->Name("...select a patch blob...");

		auto p = GetCtrlName(IDC_PATCH_FILE);
		if (p)
			Open(p);
	}

	~PatchViewer()
	{
		Convert(Opts, this, false);
	}

	bool Open(const char *Patch)
	{
		if (!out->LTextView3::Open(Patch))
			return false;

		SetCtrlName(IDC_PATCH_FILE, Patch);
		return true;
	}

	int OnNotify(LViewI *Ctrl, LNotification n)
	{
		switch (Ctrl->GetId())
		{
			case IDC_BROWSE_PATCH:
			{
				LFileSelect s;
				s.Parent(this);
				if (s.OpenFolder())
				{
					Open(s.Name());
				}
				break;
			}
			case IDC_BROWSE_BASE:
			{
				LFileSelect s;
				s.Parent(this);
				if (s.OpenFolder())
				{
					SetCtrlName(IDC_BASE_DIR, s.Name());
				}
				break;
			}
			case IDC_OUT:
			{
				if (n.Type != LNotifyCursorChanged)
					break;

				auto car = out->GetCaret();
				LgiTrace("CursorChanged: %i\n", (int)car);
				auto txt = out->NameW();
				ssize_t carLine = -1;
				LArray<const char16*> lines;
				lines.Add(txt);
				for (auto t = txt; *t; t++)
				{
					if (carLine < 0 && (t - txt) >= car)
						carLine = lines.Length() - 1;
					if (*t == '\n' && *t)
						lines.Add(t + 1);
				}
					
				if (carLine < 0)
				{
					LgiTrace("%s:%i - No line num for carat pos.\n", _FL);
					break;
				}
				if (carLine >= (ssize_t)lines.Length())
				{
					LgiTrace("%s:%i - carat line greater than lines.len.\n", _FL);
					break;
				}

				LRange blob(-1, 0);
				for (ssize_t s = carLine; s >= 0; s--)
				{
					auto ln = lines[s];
					if (ln[0] == '@' && ln[1] == '@')
					{
						blob.Start = s;
						break;
					}
				}
				for (ssize_t e = carLine; e < (ssize_t)lines.Length(); e++)
				{
					auto ln = lines[e];
					if ( (ln[0] == '@' && ln[1] == '@') ||
						 (ln[0] == '-' && ln[1] == '-') )
					{
						blob.Len = e - blob.Start;
						break;
					}
					else if (e == lines.Length() - 1)
					{
						blob.Len = e - blob.Start + 1;
						break;
					}
				}

				if (!blob.Valid())
				{
					LgiTrace("%s:%i - invalid patch blob size.\n", _FL);
					break;
				}

				auto filesIdx = blob.Start;
				while (filesIdx >= 0)
				{
					auto t = lines[filesIdx];
					if (Strnicmp(t, L"--- a/", 6) == 0)
						break;
					filesIdx--;
				}
				if (filesIdx == 0)
				{
					LgiTrace("%s:%i - couldn't find file names lines.\n", _FL);
					break;
				}

				auto aFile = lines[filesIdx];
				auto bFile = lines[filesIdx + 1];
				LFile::Path p(GetCtrlName(IDC_BASE_DIR), LString(aFile, lineLen(aFile))(5,-1));
				if (!p.Exists())
				{
					LgiTrace("%s:%i - file '%s' doesn't exist.\n", _FL, p.GetFull().Get());
					break;
				}

				if (!in->Open(p))
				{
					LgiTrace("%s:%i - failed to open file '%s'.\n", _FL, p.GetFull().Get());
					break;
				}

				auto sText = lines[blob.Start];
				auto eText = lines[blob.End()];

				auto ln = LString(sText, lineLen(sText));
				auto hdr = ln.SplitDelimit();
				auto before = hdr[1].SplitDelimit(",");
				auto after = hdr[2].SplitDelimit(",");
				auto beforeLn = -before[0].Int() - 1;
				auto beforeEnd = beforeLn + before[1].Int();

				size_t blobIdx = blob.Start + 1;
				for (ssize_t i=beforeLn; i<beforeEnd; i++)
				{
					auto off = i - beforeLn;

					in->Back[i].Rgb(222, 222, 222);

					auto patchLn = lines[blobIdx++];
					while (*patchLn == '+')
						patchLn = lines[blobIdx++];

					if (*patchLn == '-')
					{
						auto inTxt = in->TextAtLine(i);
						auto inLen = lineLen(inTxt);
						if (Strnicmp(inTxt, patchLn + 1, inLen))
						{
							in->Fore[i] = LColour::Red;
						}
						else
						{
							in->Fore[i] = LColour::Green;
						}
					}							
				}

				in->Update();
				in->SetLine((int)beforeLn);
				break;
			}
		}

		return LWindow::OnNotify(Ctrl, n);
	}
};

void OpenPatchViewer(LViewI *Parent, LOptionsFile *Opts)
{
	new PatchViewer(Parent, Opts);
}
