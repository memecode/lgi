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
	IDC_APPLY,
	IDC_SAVE_PATCH,
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
			// auto s = Text + l->Start;

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
		tbl->GetCss(true)->Height("6em");
			
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
		c = tbl->GetCell(2, y++);
			c->Add(new LButton(IDC_BROWSE_BASE, 0, 0, -1, -1, "..."));
			c->PaddingRight("0.5em");

		c = tbl->GetCell(0, y);
		c = tbl->GetCell(1, y);
			c->Add(new LButton(IDC_SAVE_PATCH, 0, 0, -1, -1, "Save Patch"));
			c->TextAlign(LCss::AlignRight);
		c = tbl->GetCell(2, y);
			c->Add(new LButton(IDC_APPLY, 0, 0, -1, -1, "Apply"));
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

	template<typename T>
	LArray<T*> GetLines(T *txt)
	{
		LArray<T*> lines;
		lines.Add(txt);
		for (auto t = txt; *t; t++)
		{
			if (*t == '\n' && *t)
				lines.Add(t + 1);
		}
		return lines;
	}

	struct FilePatch
	{
		LString File;
		LArray<LRange> Hunks;
	};

	bool Apply()
	{
		auto patch = out->NameW();
		auto lines = GetLines(out->NameW());
		LArray<FilePatch> Patches;

		auto error = [](const char *msg) -> bool
		{
			LgiTrace("Apply error: %s\n", msg);
			return false;
		};

		auto scanLines = [&lines](const char16 *key, size_t from) -> ssize_t
		{
			auto len = Strlen(key);
			for (size_t i = from; i<lines.Length(); i++)
			{
				auto ln = lines[i];
				if (!Strnicmp(ln, key, len))
					return i;
			}
			return -1;
		};

		auto lineAt = [&lines](size_t idx) -> LString
		{
			auto ptr = lines[idx];
			LString s(ptr, lineLen(ptr));
			return s;
		};

		ssize_t aFile = -1, bFile = -1;
		for (size_t i=0; i<lines.Length(); i++)
		{
			auto ln = lines[i];
			if (!Strnicmp(ln, L"--- ", 4))
				aFile = i;
			else if (!Strnicmp(ln, L"+++ ", 4))
				bFile = i;

			if (aFile > 0 &&
				bFile > 0 &&
				aFile == bFile - 1)
			{
				FilePatch &fp = Patches.New();
				auto s = lineAt(aFile);
				fp.File = s(6,-1);
				LgiTrace("File: %s\n", fp.File.Get());

				// Collect blobs
				LRange *cur = NULL;
				size_t n = bFile + 1;
				for (; n < lines.Length(); n++)
				{
					auto ln = lines[n];
					if (*ln == ' ')
						continue;

					bool start = ln[0] == '@' && ln[1] == '@';
					bool end = (ln[0] == '-' && ln[1] == '-');
					bool finish = Strnicmp(ln, L"diff ", 5) == 0;
					
					if ((start || end || finish) && cur)
					{
						cur->Len = n - cur->Start;
						cur = NULL;
					}
					if (start && !cur)
					{
						cur = &fp.Hunks.New();
						cur->Start = n;
					}
					if (finish)
						break;
				}

				for (auto &b: fp.Hunks)
					LgiTrace("    Blob: %i:%i\n", (int)b.Start, (int)b.Len);
				i = n;
				aFile = bFile = -1;
			}
		}

		// Iterate all the patches and apply them...
		for (auto &fp: Patches)
		{
			LFile::Path path(GetCtrlName(IDC_BASE_DIR), fp.File);
			if (!path.Exists())
			{
				LgiTrace("%s:%i - File to patch '%s' doesn't exist.\n", _FL, path.GetFull().Get());
				return false;
			}

			LFile f(path, O_READ);
			auto utf = f.Read();
			size_t crlf = 0, lf = 0;
			for (const char *s = utf; *s; s++)
			{
				if (s[0] == '\r' && s[1] == '\n') { crlf++; s++; }
				else if (s[0] == '\n') { lf++; }
			}
			auto eol = crlf > lf ? L"\r\n" : L"\n";
			auto eolLen = Strlen(eol);
			LAutoWString wide(Utf8ToWide(utf));
			LArray<LAutoWString> docLines;
			for (char16 *w = wide; *w; )
			{
				for (auto e = w; true; e++)
				{
					if (!Strncmp(e, eol, eolLen))
					{
						docLines.New().Reset(NewStrW(w, e - w));
						w = e + eolLen;
						break;
					}

					if (!*e)
					{
						docLines.New().Reset(NewStrW(w, e - w));
						w = e;
						break;
					}
				}
			}
			
			/*
			for (size_t i=0; i<docLines.Length(); i++)
			{
				auto &ln = docLines[i];
				auto len = lineLen(ln.Get());
				LgiTrace("[%i]=%i: %.*S\n", (int)i, (int)len, (int)len, ln.Get());
			}
			*/

			for (auto &hunk: fp.Hunks)
			{
				auto hdr = lines[hunk.Start];
				if (Strnicmp(hdr, L"@@ ", 3))
					return error("hunk header missing start '@@'");
				hdr += 3;
				auto e = Strstr(hdr, L" @@");
				if (!e)
					return error("hunk header missing end '@@'");
				auto parts = LString(hdr, e - hdr).Strip().SplitDelimit();
				if (parts.Length() != 2)
					return error("hunk header has wrong part count.");
				auto before = parts[0].SplitDelimit(",");
				auto after = parts[1].SplitDelimit(",");
				auto beforeLine = -before[0].Int();
				if (beforeLine < 1)
					return error("hunk: expecting negative beforeLine.");
				// auto beforeLines = before[1].Int();

				auto afterLine = after[0].Int();
				// auto afterLines = after[1].Int();

				auto inputLine = 1;
				auto inputTxt = lines[hunk.Start + inputLine];
				auto docLine = afterLine - 1/*1 based indexing to 0 based*/;
				while (inputLine < hunk.Len)
				{
					// Check the inputTxt matches the input document
					if (docLine > (ssize_t)docLines.Length())
						return error("docLine out of range.");
					auto docTxt = docLines[docLine].Get();
					auto docTxtLen = lineLen(docTxt);
					auto inputLineLen = lineLen(inputTxt);

					if (*inputTxt == '+')
					{
						// Process insert: add 'inputLine' at docLine
						LAutoWString insert(NewStrW(inputTxt + 1, inputLineLen - 1));
						docLines.AddAt(docLine, insert);
					}
					else
					{
						// Check content...
						if (docTxtLen != inputLineLen - 1)
							return error("doc and input line lengths don't match.");

						auto cmp = Strncmp(docTxt, inputTxt + 1/*prefix char*/, docTxtLen);
						if (cmp)
							return error("doc and input line content don't match.");

						if (*inputTxt == '-')
						{
							// Process delete...
							docLines.DeleteAt(docLine--, true);
						}
					}

					docLine++;
					inputTxt = lines[hunk.Start + ++inputLine];
				}
			}
		}

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
			case IDC_SAVE_PATCH:
			{
				auto patchFile = GetCtrlName(IDC_PATCH_FILE);
				out->Save(patchFile);
				break;
			}
			case IDC_OUT:
			{
				if (n.Type != LNotifyCursorChanged)
					break;

				auto car = out->GetCaret();
				LgiTrace("CursorChanged: %i\n", (int)car);
				auto txt = out->NameW();
				ssize_t caratLine = -1;
				LArray<const char16 *> lines;
				lines.Add(txt);
				for (auto t = txt; *t; t++)
				{
					if (caratLine < 0 && (t - txt) >= car)
						caratLine = lines.Length() - 1;
					if (*t == '\n' && *t)
						lines.Add(t + 1);
				}
					
				if (caratLine < 0)
				{
					LgiTrace("%s:%i - No line num for carat pos.\n", _FL);
					break;
				}
				if (caratLine >= (ssize_t)lines.Length())
				{
					LgiTrace("%s:%i - carat line greater than lines.len.\n", _FL);
					break;
				}

				LRange blob(-1, 0);
				for (ssize_t s = caratLine; s >= 0; s--)
				{
					auto ln = lines[s];
					if (ln[0] == '@' && ln[1] == '@')
					{
						blob.Start = s;
						break;
					}
				}
				for (ssize_t e = caratLine; e < (ssize_t)lines.Length(); e++)
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

					if (*patchLn != '+')
					{
						auto inTxt = in->TextAtLine(i);
						auto inLen = lineLen(inTxt);
						if (Strnicmp(inTxt, patchLn + 1, inLen))
						{
							in->Fore[i] = LColour::Red;
						}
						else
						{
							if (*patchLn == '-')
								in->Fore[i] = LColour::Green;
						}
					}							
				}

				in->Update();
				in->SetLine((int)beforeLn);
				break;
			}
			case IDC_APPLY:
			{
				Apply();
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
