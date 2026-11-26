#include "Lvc.h"
#include "GraphProfile.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Path.h"
#include "lgi/common/HashTable.h"
#include "resdefs.h"
#include <cmath>

const char *sPalette[] = { "9696ff", "ff8787", "ff934b", "a8d200", "00f0c0", "87e7ff", "a5a5ff", "f3c3ff", "b40090", "00b400" };

LColour GetGraphLineColour()
{
	// Graph lines use a standard colour, independent of the lane colours
	return LColour(0x40, 0x40, 0x40);
}

LColour GetPaletteColour(int i)
{
	LColour c;
	const char *s = sPalette[i % CountOf(sPalette)];
	#define Comp(comp, off) { const char h[3] = {s[off], s[off+1], 0}; c.comp(htoi(h)); }
	Comp(r, 0);
	Comp(g, 2);
	Comp(b, 4);
	return c;
}

VcEdge::~VcEdge()
{
	if (Parent)
		Parent->Edges.Delete(this);
	if (Child)
		Child->Edges.Delete(this);
}

void VcEdge::Detach(VcCommit *c)
{
	if (Parent == c)
		Parent = NULL;
	if (Child == c)
		Child = NULL;
	if (Parent == NULL && Child == NULL)
		delete this;
}

void VcEdge::Set(VcCommit *p, VcCommit *c)
{
	if ((Parent = p))
		Parent->Edges.Add(this);
	if ((Child = c))
		Child->Edges.Add(this);
}

size_t VcCommit::Instances = 0;
VcCommit::VcCommit(AppPriv *priv, VcFolder *folder) : Pos(32, -1)
{
	VcCommit::Instances++;
	d = priv;
	Index = -1;
	Folder = folder;
	Vcs = Folder->GetType();
	Current = false;
	NodeIdx = -1;
	NodeColour = GetPaletteColour(0);
	Parents.SetFixedLength(false);
}

VcCommit::VcCommit(const VcCommit &c)
{
	VcCommit::Instances++;
	d = c.d;
	Folder = c.Folder;
	Vcs = c.Vcs;

	Rev = c.Rev;
	Index = c.Index;
	Parents = c.Parents;
	Branch = c.Branch;	
	Author = c.Author;
	Ts = c.Ts;
	Msg = c.Msg;
	
	Files = c.Files;
}

VcCommit::~VcCommit()
{
	for (auto e: Edges)
		e->Detach(this);
	VcCommit::Instances--;
}

char *VcCommit::GetRev()
{
	return Rev;
}

char *VcCommit::GetAuthor()
{
	return Author;
}

char *VcCommit::GetMsg()
{
	return Msg;
}

char *VcCommit::GetBranch()
{
	return Branch;
}

void VcCommit::SetCurrent(bool b)
{
	Current = b;
}

void VcCommit::OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c)
{
	if (i != 0)
	{
		LListItem::OnPaintColumn(Ctx, i, c);
		return;
	}

	auto profile = GraphPaintProfile::Current;
	auto started = profile ? LMicroTime() : 0;
	LListItem::OnPaintColumn(Ctx, i, c);
	auto baseDone = profile ? LMicroTime() : 0;
	if (profile)
	{
		if (!profile->Rows)
			profile->Folder = Folder->GetUri().sPath;
		profile->Rows++;
		profile->Base += baseDone - started;
	}

	LSurface::Vector vec = Ctx.pDC->Vec();
	if (!vec)
	{
		if (profile)
		{
			auto done = LMicroTime();
			profile->NoVector++;
			profile->Setup += done - baseDone;
			profile->MaxRow = MAX(profile->MaxRow, done - started);
		}
		return;
	}

	if (!vec.ClipRect(Ctx))
	{
		LgiTrace("%s:%i - Failed to clip graph column.\n", _FL);
		return;
	}

	int H = Ctx.Y();
	float x0 = (float)Ctx.x1;
	float y0 = (float)Ctx.y1;

	VcCommit *Prev = NULL, *Next = NULL;
	if (Folder->Log.IdxCheck(Idx - 1))
		Prev = Folder->Log[Idx - 1];
	if (Folder->Log.IdxCheck(Idx + 1))
		Next = Folder->Log[Idx + 1];

	float dpi = Ctx.pDC->GetDpi().x;
	if (dpi <= 0)
		dpi = 96;
	dpi /= 96.0f;

	float pitch = 14.0f * dpi;
	float inset = pitch * 0.45f;
	float midY = y0 + (float)H / 2.0f;
	float top = y0;
	float bot = y0 + (float)H;
	float r = pitch * 0.34f;
	float sw = pitch * 0.16f;
	if (sw < 1.5f)
		sw = 1.5f;

	auto laneX = [&](int lane) -> float
	{
		return x0 + inset + (float)lane * pitch + pitch * 0.5f;
	};

	auto segment = [&](float px0, float py0, float px1, float py1, LColour col)
	{
		if (profile)
			profile->Segments++;
		vec.MoveTo(px0, py0);
		if (fabs(px0 - px1) < 0.5f)
			vec.LineTo(px1, py1);
		else
		{
			if (profile)
				profile->Curves++;
			float my = (py0 + py1) / 2.0f;
			vec.CubicTo(px0, my, px1, my, px1, py1);
		}
		vec.Stroke(col, sw);
	};

	auto setupDone = profile ? LMicroTime() : 0;
	for (auto it: Pos)
	{
		VcEdge *e = it.key;
		int lane = it.value;
		if (lane < 0)
			continue;

		LColour col = e->Colour;
		if (!col.IsValid())
			col = NodeColour;
		float cx = laneX(lane);
		float nodeCx = NodeIdx >= 0 ? laneX(NodeIdx) : cx;

		if (e->Parent == this)
		{
			int pl = Prev ? Prev->Pos.Find(e) : -1;
			if (pl >= 0)
				segment(laneX(pl), top, nodeCx, midY, col);
			else
				segment(cx, top, nodeCx, midY, LColour(0, 0, 0));
			continue;
		}

		if (e->Child == this)
		{
			int nl = Next ? Next->Pos.Find(e) : -1;
			LColour drawCol = nl >= 0 ? col : LColour(0, 0, 0);
			segment(nodeCx, midY, cx, bot, drawCol);
			continue;
		}

		int pl = Prev ? Prev->Pos.Find(e) : -1;
		if (pl < 0)
		{
			segment(cx, top, cx, bot, LColour(0, 0, 0));
			continue;
		}

		float tx = laneX(pl);
		if (fabs(tx - cx) < 0.5f)
			segment(cx, top, cx, bot, col);
		else
		{
			segment(tx, top, cx, midY, col);
			segment(cx, midY, cx, bot, col);
		}
	}

	auto edgesDone = profile ? LMicroTime() : 0;
	if (NodeIdx >= 0)
	{
		float cx = laneX(NodeIdx);
		float nr = r + (Current ? 1.0f : 0.0f);

		auto circle = [&](float rad)
		{
			vec.MoveTo(cx, midY - rad);
			vec.ArcTo(rad, rad, 0, true, true, cx, midY + rad);
			vec.ArcTo(rad, rad, 0, true, true, cx, midY - rad);
			vec.ClosePath();
		};

		float ring = Current ? sw : 1.2f;

		vec.ClearPath();
		circle(nr);
		vec.Fill(NodeColour);
		vec.Stroke(LColour::Black, ring);
	}

	if (profile)
	{
		auto done = LMicroTime();
		profile->Setup += setupDone - baseDone;
		profile->Edges += edgesDone - setupDone;
		profile->Nodes += done - edgesDone;
		profile->Positions += Pos.Length();
		profile->MaxRow = MAX(profile->MaxRow, done - started);
	}
}

const char *VcCommit::GetFieldText(CommitField Fld)
{
	switch (Fld)
	{
		case LRevision:
			return Rev;
		case LIndex:
			if (!sIndex)
				sIndex.Printf(LPrintfInt64, Index);
			return sIndex;
		case LBranch:
			if (Vcs == VcGit)
			{
				sNames = Folder->GetGitNames(Rev);
				return sNames;
			}
			else
			{
				if (Branch)
					return Branch;
				return "default";
			}
			break;
		case LAuthor:
			return Author;
		case LTime:
			if (!sTimeStamp)
				sTimeStamp = Ts.Get();
			return sTimeStamp;
		case LMessageTxt:
			if (!Msg)
				return NULL;
			if (!sMsg)
				sMsg = Msg.Split("\n", 1)[0];
			return sMsg;
		default:
			break;
	}

	return NULL;
}

const char *VcCommit::GetText(int Col)
{
	if (!Folder)
	{
		LAssert(0);
		return "#nofolder";
	}

	if (!Folder->Fields.IdxCheck(Col))
	{
		LAssert(0);
		return "#nofield";
	}

	auto f = Folder->Fields[Col];
	return GetFieldText(f);
}

bool VcCommit::GitParse(LString s, bool RevList)
{
	LString::Array lines = s.Replace("\r","").Split("\n");
	if (lines.Length() < 3)
		return false;

	if (RevList)
	{
		auto a = lines[0].SplitDelimit();
		if (a.Length() != 2)
			return false;

		Ts.SetUnix((uint64) a[0].Int());
		Rev = a[1];

		for (auto Ln: lines)
		{
			if (IsWhite(Ln(0)))
			{
				if (Msg)
					Msg += "\n";
				Msg += Ln.Strip();
			}
			else
			{
				a = Ln.SplitDelimit(" \t\r", 1);
				if (a.Length() <= 1)
					continue;
				if (a[0].Equals("parent"))
					Parents.Add(a[1]);
				else if (a[0].Equals("author"))
					Author = a[1].RStrip("0123456789+ ");
			}
		}
	}
	else
	{
		for (unsigned ln = 0; ln < lines.Length(); ln++)
		{
			LString &l = lines[ln];
			if (ln == 0)
			{
				auto parts = l.SplitDelimit("()");
				auto commit = parts[0].SplitDelimit();
				Rev = commit.Last();
			}
			else if (l.Find("Author:") >= 0)
			{
				Author = l.Split(":", 1)[1].Strip();
			}
			else if (l.Find("Date:") >= 0)
			{
				Ts.Parse(l.Split(":", 1)[1].Strip());
			}
			else if (l.Strip().Length() > 0)
			{
				if (Msg)
					Msg += "\n";
				Msg += l.Strip();
			}
		}
	}

	OnParse();
	return Author && Rev;
}

bool VcCommit::CvsParse(LDateTime &Dt, LString Auth, LString Message)
{
	Ts = Dt;
	Ts.ToLocal();
	
	LTimeStamp i;
	if (Ts.Get(i))
		Rev.Printf(LPrintfInt64, i.Get());
	Author = Auth;
	Msg = Message;

	OnParse();
	return true;
}

void VcCommit::OnParse()
{
	// Branch colours for git can only be resolved once the whole graph is known,
	// so this just picks up the commits where the branch is directly known.
	LString b = CommitBranch();
	if (b)
		NodeColour = Folder->BranchColour(b.Get());
}

LString VcCommit::CommitBranch()
{
	if (Branch)
		return Branch;

	if (Vcs == VcGit) {
		// Git commits have no branch of their own, so use the refs pointing at this commit
		sNames = Folder->GetGitNames(Rev);
		if (!sNames)
			return LString();

		auto a = sNames.Split(",");
		LString cur = Folder->GetCurrentBranch();
		for (auto n: a)
			if (n.Equals(cur))
				return n;
		for (auto n: a)
			if (!n.Find("/"))
				return n;
		return a[0];
	}

	return LString();
}

bool VcCommit::HgParse(LString s)
{
	LString::Array Lines = s.SplitDelimit("\n");
	if (Lines.Length() < 1)
		return false;

	for (auto Ln: Lines)
	{
		LString::Array f = Ln.Split(":", 1);
		if (f.Length() == 2)
		{
			if (f[0].Equals("changeset"))
			{
				auto p = f[1].Strip().Split(":");
				Index = p[0].Int();
				Rev = p[1];
			}
			else if (f[0].Equals("user"))
				Author = f[1].Strip();
			else if (f[0].Equals("date"))
				Ts.Parse(f[1].Strip());
			else if (f[0].Equals("summary"))
				Msg = f[1].Strip();
			else if (f[0].Equals("parent"))
			{
				auto p = f[1].Strip().Split(":");
				Parents.Add(p.Last());
			}
			else if (f[0].Equals("branch"))
				Branch = f[1].Strip();
		}
	}

	OnParse();
	return Rev.Get() != NULL;
}

bool VcCommit::SvnParse(LString s)
{
	LString::Array lines = s.Split("\n");
	if (lines.Length() < 1)
		return false;

	for (unsigned ln = 0; ln < lines.Length(); ln++)
	{
		LString &l = lines[ln];
		if (ln == 0)
		{
			LString::Array a = l.Split("|");
			if (a.Length() > 3)
			{
				Rev = a[0].Strip(" \tr");
				Index = Rev.Int();
				Author = a[1].Strip();
				Ts.Parse(a[2]);
			}
		}
		else
		{
			if (Msg)
				Msg += "\n";
			Msg += l.Strip();
		}
	}

	Msg = Msg.Strip();
	OnParse();
	
	// LgiTrace("SvnParse %s %i %s %s %s\n", Rev.Get(), (int)Index, Author.Get(), Ts.GetDate().Get(), Msg.Get());

	return Author && Rev && Ts.IsValid();
}

VcFolder *VcCommit::GetFolder()
{
	for (LTreeNode *i = d->Tree->Selection(); i; i = i->GetParent())
	{
		auto f = dynamic_cast<VcFolder*>(i);
		if (f)
			return f;
	}
	return NULL;
}

void VcCommit::Select(bool b)
{
	LListItem::Select(b);

	if (Rev && b)
	{
		VcFolder *f = GetFolder();
		// LgiTrace("%s:%i select %s %i f=%p\n", _FL, Rev.Get(), b, f);
		if (f)
		{
			auto Rng = d->GetCommitRange();
			// LgiTrace("%s:%i select rng=%i\n", _FL, (int)Rng.Length());
			if (Rng.Length() > 1)
			{
				// Show the diffs over a range of commits.
				f->DiffRange(Rng[0], Rng[1]);
			}
			else
			{
				// Show a single commit's files:
				f->ListCommit(this);
			}
		}

		if (d->Msg)
		{
			d->Msg->Name(Msg);
			
			auto *w = d->Msg->GetWindow();
			if (w)
			{
				w->SetCtrlEnabled(IDC_COMMIT, false);
				w->SetCtrlEnabled(IDC_COMMIT_AND_PUSH, false);
			}
		}
		else LAssert(0);
	}

	if (GetFolder())
		GetFolder()->UpdateAuthorUi();
}

void VcCommit::OnMouseClick(LMouse &m)
{
	LListItem::OnMouseClick(m);

	if (m.IsContextMenu())
	{
		LArray<VcCommit*> Sel;
		GetList()->GetSelection(Sel);

		LSubMenu s;
		s.AppendItem("Update", IDM_UPDATE, !Current);
		s.AppendSeparator();
		s.AppendItem("Merge With Local", IDM_MERGE, !Current);
		if (Rev)
			s.AppendItem("Copy Revision", IDM_COPY_REV);
		if (Index >= 0)
			s.AppendItem("Copy Index", IDM_COPY_INDEX);
		s.AppendItem("Rename Branch", IDM_RENAME_BRANCH);
		s.AppendItem("Revert Commit", ID_REVERT_COMMIT);
		
		int Cmd = s.Float(GetList(), m);
		switch (Cmd)
		{
			case IDM_MERGE:
			{
				auto f = GetFolder();
				if (!f)
				{
					LAssert(!"No folder?");
					break;
				}

				f->MergeToLocal(Rev);
				break;
			}
			case IDM_UPDATE:
			{
				if (auto f = GetFolder())
					f->Checkout(Rev, false);
				else
					LAssert(!"No folder?");
				break;
			}
			case IDM_COPY_REV:
			{
				LClipBoard c(GetList());

				LString::Array a;
				a.SetFixedLength(false);
				for (auto i: Sel)
					a.New() = i->GetRev();

				c.Text(LString(",").Join(a));
				break;
			}
			case IDM_COPY_INDEX:
			{
				LClipBoard c(GetList());
				LString b;
				b.Printf(LPrintfInt64, Index);
				c.Text(b);
				break;
			}
			case IDM_RENAME_BRANCH:
			{
				LArray<VcCommit*> Revs;
				if (GetList()->GetSelection(Revs))
				{
					auto Fld = Folder;
					auto Inp = new LInput(GetList(), "", "New branch name:", AppName);
					Inp->DoModal([this, Fld, Inp, Revs](auto dlg, auto ctrlId)
					{
						if (ctrlId)
						{
							auto Revisions = Revs;
							Fld->RenameBranch(Inp->GetStr(), Revisions);
						}
					});
				}
				break;
			}
			case ID_REVERT_COMMIT:
			{
				auto f = GetFolder();
				if (!f)
				{
					LAssert(!"No folder?");
					break;
				}

				f->RevertCommit(Rev);
				break;
			}
		}
	}
}
