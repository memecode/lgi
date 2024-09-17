#include "Lvc.h"
#include "lgi/common/ClipBoard.h"
#include "resdefs.h"

VcFile::VcFile(AppPriv *priv, VcFolder *owner, LString revision, bool working)
{
	d = priv;
	Owner = owner;
	Revision = revision;
	if (working)
		Chk = new LListItemCheckBox(this, 0, false);
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
				SetStatus(sym)

		STATE("?", 1, SUntracked);
		else STATE("Up-To-Date", 10, SClean);
		else STATE("Modified", 8, SModified);
		else STATE("A", 1, SAdded);
		else STATE("M", 1, SModified);
		else STATE("C", 1, SConflicted);
		else STATE("!", 1, SMissing);
		else STATE("D", 1, SDeleted);
		else STATE("R", 1, SDeleted); // "Removed"
		else STATE("U", 1, SConflicted);
		else STATE("Locally Modified", 1, SModified);
		else
		{
			//LAssert(!"Impl state");
			d->Log->Print("%s:%i - Unknown status '%s'\n", _FL, s);
		}
	}

	return Status;
}

void VcFile::SetStatus(FileStatus s)
{
	Status = s;

	if (Status == SConflicted)
		GetCss(true)->Color("orange");
	else
		GetCss(true)->Color(LCss::ColorInherit);

	Update();
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
			LFile::Path p = u.sPath ? Uri.sPath(1,-1) : Owner->LocalPath();
			p += File;
			LocalPath = p.GetFull();
		}

		LArray<VcFile*> Files;
		GetList()->GetSelection(Files);
		LString::Array Uris;
		int Staged = 0;
		
		for (auto f: Files)
		{
			Uris.New() = f->GetUri();
			if (f->GetStaged())
				Staged++;
		}

		GetStatus();
		if (Revision)
		{
			// Committed changes
			s.AppendItem(LLoadString(IDS_REVERT_THIS), ID_REVERT_TO_REV, Status != SClean);
			s.AppendItem(LLoadString(IDS_REVERT_BEFORE), ID_REVERT_TO_BEFORE, Status != SClean);
			s.AppendItem(LLoadString(IDS_BLAME), IDM_BLAME);
			s.AppendItem(LLoadString(IDS_SAVE_AS), IDM_SAVE_AS);
			s.AppendItem(LLoadString(IDS_LOG), IDM_LOG_FILE);
		}
		else
		{
			// Uncommitted changes
			switch (Status)
			{
				case SModified:
				case SAdded:
				case SDeleted:
				{
					LString label = LLoadString(IDS_REVERT_CHANGES);
					if (Staged > 0)
						label += LString::Fmt(" (%i)", Staged);
					s.AppendItem(label, IDM_REVERT);
					break;
				}
				case SConflicted:
				{
					auto menu = s.AppendSub(LLoadString(IDS_RESOLVE));
					menu->AppendItem(LLoadString(IDS_MARK), IDM_RESOLVE_MARK);
					menu->AppendItem(LLoadString(IDS_UNMARK), IDM_RESOLVE_UNMARK);
					menu->AppendSeparator();
					menu->AppendItem(LLoadString(IDS_LOCAL), IDM_RESOLVE_LOCAL);
					menu->AppendItem(LLoadString(IDS_INCOMING), IDM_RESOLVE_INCOMING);
					menu->AppendItem(LLoadString(IDS_TOOL), IDM_RESOLVE_TOOL);
					break;
				}
				case SUntracked:
				{
					if (Owner->GetType() == VcCvs)
					{
						s.AppendItem(LLoadString(IDS_ADD_TEXT), IDM_ADD_FILE);
						s.AppendItem(LLoadString(IDS_ADD_BINARY), IDM_ADD_BINARY_FILE);
					}
					else
					{
						s.AppendItem(LLoadString(IDS_ADD_FILE), IDM_ADD_FILE);
					}
					break;
				}
				default:
					break;
			}					
			s.AppendItem(LLoadString(IDS_BROWSE_TO), IDM_BROWSE, !LocalPath.IsEmpty());
			s.AppendItem(LLoadString(IDS_LOG), IDM_LOG_FILE);
			s.AppendItem(LLoadString(IDS_BLAME), IDM_BLAME);
			s.AppendSeparator();
			if (Status == SMissing)
			{
				s.AppendItem("Restore", ID_RESTORE);
			}
			else
			{
				s.AppendItem(LLoadString(IDS_DEL_KEEP_LOCAL), IDM_FORGET);
				s.AppendItem(LLoadString(IDS_DEL_AND_LOCAL), IDM_REMOVE);
			}
			s.AppendSeparator();

			int CurEol = LocalPath ? GetEol(LocalPath) : 0;
			auto Ln = s.AppendSub(LLoadString(IDS_LINE_ENDINGS));
			auto Item = Ln->AppendItem("LF", IDM_EOL_LF);
			if (Item && CurEol == IDM_EOL_LF)
				Item->Checked(true);
			Ln->AppendItem("CRLF", IDM_EOL_CRLF);
			if (Item && CurEol == IDM_EOL_CRLF)
				Item->Checked(true);
			Ln->AppendItem(LLoadString(IDS_AUTO), IDM_EOL_AUTO);
		}

		s.AppendSeparator();
		LString Fn;
		auto FileParts = LString(File).SplitDelimit("/\\");
		if (FileParts.Length() > 1)
		{
			Fn.Printf(LLoadString(IDS_COPY_FMT), FileParts.Last().Get());
			s.AppendItem(Fn, IDM_COPY_LEAF);
		}
		Fn.Printf(LLoadString(IDS_COPY_FMT), File);
		s.AppendItem(Fn, IDM_COPY_PATH);

		int Cmd = s.Float(GetList(), m);
		switch (Cmd)
		{
			case IDM_REVERT:
			{
				Owner->Revert(Uris);
				break;
			}
			case IDM_RESOLVE_MARK:
			{
				Owner->Resolve(Uris[0], ResolveMark);
				break;
			}
			case IDM_RESOLVE_UNMARK:
			{
				Owner->Resolve(Uris[0], ResolveUnmark);
				break;
			}
			case IDM_RESOLVE_LOCAL:
			{
				Owner->Resolve(Uris[0], ResolveLocal);
				break;
			}
			case IDM_RESOLVE_INCOMING:
			{
				Owner->Resolve(Uris[0], ResolveIncoming);
				break;
			}
			case IDM_RESOLVE_TOOL:
			{
				Owner->Resolve(Uris[0], ResolveTool);
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
					LgiMsg(GetList(), LLoadString(IDS_ERR_NO_PATH_FMT), AppName, MB_OK, LocalPath.Get());
				break;
			}
			case IDM_FORGET:
			{
				Owner->Delete(LocalPath, true);
				break;
			}
			case IDM_REMOVE:
			{
				Owner->Delete(LocalPath, false);
				break;
			}
			case ID_RESTORE:
			{
				Owner->Revert(Uris, Revision);
				break;
			}
			case ID_REVERT_TO_REV:
			{
				Owner->Revert(Uris, Revision);
				break;
			}
			case ID_REVERT_TO_BEFORE:
			{
				Owner->Revert(Uris, Revision, true);
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
