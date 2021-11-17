#include "Lvc.h"
#include "lgi/common/ClipBoard.h"

VcFile::VcFile(AppPriv *priv, VcFolder *owner, LString revision, bool working)
{
	d = priv;
	LoadDiff = false;
	Owner = owner;
	Revision = revision;
	Status = SUnknown;
	if (working)
		Chk = new LListItemCheckBox(this, 0, false);
	else
		Chk = NULL;
}

VcFile::~VcFile()
{
}

VcFile::FileStatus VcFile::GetStatus()
{
	if (Status == SUnknown)
	{
		const char *s = GetText(COL_STATE);
		if (!s)
			return Status;

		#define STATE(str, len, sym) \
			if (!strnicmp(s, str, len)) \
				Status = sym

		STATE("?", 1, SUntracked);
		else STATE("Up-To-Date", 10, SClean);
		else STATE("Modified", 8, SModified);
		else STATE("A", 1, SAdded);
		else STATE("M", 1, SModified);
		else STATE("C", 1, SConflicted);
		else STATE("!", 1, SMissing);
		else STATE("D", 1, SDeleted);
		else STATE("R", 1, SDeleted); // "Removed"
		else STATE("Locally Modified", 1, SModified);
		else
		{
			//LAssert(!"Impl state");
			d->Log->Print("%s:%i - Unknown status '%s'\n", _FL, s);
		}
	}

	return Status;
}

LString VcFile::GetUri()
{
	const char *File = GetText(COL_FILENAME);
	LUri u = Uri || !Owner ? Uri : Owner->GetUri();
	LAssert(u && File);
	u += File;
	return u.ToString();
}

void VcFile::SetUri(LString uri)
{
	Uri.Set(uri);
}

int VcFile::Checked(int Set)
{
	if (!Chk)
		return -1;

	if (Set >= 0)
		Chk->Value(Set);

	return (int)Chk->Value();
}

void VcFile::SetDiff(LString diff)
{
	auto n = LFromNativeCp(diff);
	Diff = n;
	if (LListItem::Select())
		d->Diff->Name(Diff);
}

void VcFile::Select(bool b)
{
	GetStatus();
	LListItem::Select(b);
	if (b)
	{
		if (!Diff)
		{
			if (!LoadDiff)
			{
				LoadDiff = true;
				if (Owner)
					Owner->Diff(this);
			}
		}
		else
			d->Diff->Name(Diff);
	}
}

void VcFile::OnMouseClick(LMouse &m)
{
	LListItem::OnMouseClick(m);

	if (m.IsContextMenu())
	{
		LSubMenu s;
		const char *File = GetText(COL_FILENAME);
		LString LocalPath;

		LUri u(GetUri());
		if (u.IsProtocol("file"))
		{
			LFile::Path p = u.sPath ? Uri.sPath(1,-1).Get() : Owner->LocalPath();
			p += File;
			LocalPath = p.GetFull();
		}

		LArray<VcFile*> Files;
		GetList()->GetSelection(Files);
		LString::Array Uris;
		for (auto f: Files)
			Uris.New() = f->GetUri();

		GetStatus();
		if (Revision)
		{
			// Committed changes
			s.AppendItem("Revert To This Revision", IDM_REVERT_TO_REV, Status != SClean);
			s.AppendItem("Blame", IDM_BLAME);
			s.AppendItem("Save As", IDM_SAVE_AS);
			s.AppendItem("Log", IDM_LOG_FILE);
		}
		else
		{
			// Uncommitted changes
			switch (Status)
			{
				case SModified:
				case SAdded:
				case SDeleted:
					s.AppendItem("Revert Changes", IDM_REVERT);
					break;
				case SConflicted:
					s.AppendItem("Mark Resolved", IDM_RESOLVE);
					break;
				case SUntracked:
					if (Owner->GetType() == VcCvs)
					{
						s.AppendItem("Add Text File", IDM_ADD_FILE);
						s.AppendItem("Add Binary File", IDM_ADD_BINARY_FILE);
					}
					else
					{
						s.AppendItem("Add File", IDM_ADD_FILE);
					}
					break;
				default:
					break;
			}					
			s.AppendItem("Browse To", IDM_BROWSE, !LocalPath.IsEmpty());
			s.AppendItem("Log", IDM_LOG_FILE);

			int CurEol = LocalPath ? GetEol(LocalPath) : 0;
			auto Ln = s.AppendSub("Line Endings");
			auto Item = Ln->AppendItem("LF", IDM_EOL_LF);
			if (Item && CurEol == IDM_EOL_LF) Item->Checked(true);
			Ln->AppendItem("CRLF", IDM_EOL_CRLF);
			if (Item && CurEol == IDM_EOL_CRLF) Item->Checked(true);
			Ln->AppendItem("Auto", IDM_EOL_AUTO);
		}

		s.AppendSeparator();
		LString Fn;
		auto FileParts = LString(File).SplitDelimit("/\\");
		if (FileParts.Length() > 1)
		{
			Fn.Printf("Copy '%s'", FileParts.Last().Get());
			s.AppendItem(Fn, IDM_COPY_LEAF);
		}
		Fn.Printf("Copy '%s'", File);
		s.AppendItem(Fn, IDM_COPY_PATH);

		int Cmd = s.Float(GetList(), m);
		switch (Cmd)
		{
			case IDM_REVERT:
			{
				Owner->Revert(Uris);
				break;
			}
			case IDM_RESOLVE:
			{
				Owner->Resolve(Uris[0]);
				break;
			}
			case IDM_ADD_FILE:
			{
				Owner->AddFile(Uris[0], false);
				break;
			}
			case IDM_ADD_BINARY_FILE:
			{
				Owner->AddFile(Uris[0], true);
				break;
			}
			case IDM_BROWSE:
			{
				LFile::Path p(LocalPath);
				if (p.Exists())
					LBrowseToFile(LocalPath);
				else
					LgiMsg(GetList(), "Can't find path '%s'.", AppName, MB_OK, LocalPath.Get());
				break;
			}
			case IDM_REVERT_TO_REV:
			{
				Owner->Revert(Uris, Revision);
				break;
			}
			case IDM_BLAME:
			{
				Owner->Blame(Uris[0]);
				break;
			}
			case IDM_SAVE_AS:
			{
				Owner->SaveFileAs(Uris[0], Revision);
				break;
			}
			case IDM_EOL_LF:
			case IDM_EOL_CRLF:
			case IDM_EOL_AUTO:
			{
				Owner->SetEol(Uris[0], Cmd);
				break;
			}
			case IDM_LOG_FILE:
			{
				Owner->LogFile(Uris[0]);
				break;
			}
			case IDM_COPY_LEAF:
			{
				LClipBoard c(GetList());
				c.Text(FileParts.Last());
				break;
			}
			case IDM_COPY_PATH:
			{
				LClipBoard c(GetList());
				c.Text(File);
				break;
			}
		}
	}
	else if (m.Left() && m.Down() && d->Tabs)
	{
		d->Tabs->Value(0);
	}
}


bool ConvertEol(const char *Path, bool Cr)
{
	LFile f;
	if (!f.Open(Path, O_READWRITE))
		return false;
	LString s = f.Read();
	s = s.Replace("\r");
	if (Cr)
		s = s.Replace("\n", "\r\n");
	f.SetSize(0);
	f.SetPos(0);
	return f.Write(s);
}

int GetEol(const char *Path)
{
	LFile f;
	if (!f.Open(Path, O_READ))
		return false;
	LString s = f.Read();
	int Cr = 0, Lf = 0;
	if (s)
	{
		for (char *c = s; *c; c++)
		{
			if (*c == '\r') Cr++;
			if (*c == '\n') Lf++;
		}
	}
	if (Cr > 0)
		return IDM_EOL_CRLF;
	if (Lf > 0)
		return IDM_EOL_LF;
	return 0;
}
