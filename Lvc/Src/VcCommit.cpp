#include "Lvc.h"
#include "GClipBoard.h"
#include "../Resources/resdefs.h"
#include "GPath.h"
#include "LHashTable.h"

const char *sPalette[] = { "9696ff", "5D6D66", "4E3A35", "68B3C5", "9A92B0", "462D4C", "C5A378", "302D65" };

GColour GetPaletteColour(int i)
{
	GColour c;
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

VcCommit::VcCommit(AppPriv *priv, VcFolder *folder) : Pos(32, -1)
{
	d = priv;
	Index = -1;
	Folder = folder;
	Current = false;
	NodeIdx = -1;
	NodeColour = GetPaletteColour(0);
	Parents.SetFixedLength(false);
}

VcCommit::~VcCommit()
{
	for (auto e: Edges)
		e->Detach(this);
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

void VcCommit::SetCurrent(bool b)
{
	Current = b;
}

void VcCommit::OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c)
{
	LListItem::OnPaintColumn(Ctx, i, c);

	if (i == 0)
	{
		double Px = 12;
		int Ht = Ctx.Y();
		double Half = 5.5;
		#define MAP(col) ((col) * Px + Half)

		GMemDC Mem(Ctx.X(), Ctx.Y(), System32BitColourSpace);
		double r = Half - 1;

		double x = MAP(NodeIdx);
		Mem.Colour(GColour::Black);
		
		VcCommit *Prev = NULL, *Next = NULL;
		Prev = Folder->Log.IdxCheck(Idx - 1) ? Folder->Log[Idx - 1] : NULL;
		Next = Folder->Log.IdxCheck(Idx + 1) ? Folder->Log[Idx + 1] : NULL;
		
		for (auto it: Pos)
		{
			VcEdge *e = it.key;
			int CurIdx = it.value;
			if (CurIdx < 0)
			{
				continue;
			}

			double CurX = MAP(CurIdx);
			
			#define I(v) ((int)(v))
			if (e->Child != this)
			{
				// Line to previous commit
				int PrevIdx = Prev ? Prev->Pos.Find(e) : -1;
				if (PrevIdx >= 0)
				{
					double PrevX = MAP(PrevIdx);
					Mem.Line(I(PrevX), I(-(Ht/2)), I(CurX), I(Ht/2));
				}
				else
				{
					Mem.Colour(GColour::Red);
					Mem.Line(I(CurX), I(Ht/2), I(CurX), I(Ht/2-5));
					Mem.Colour(GColour::Black);
				}
			}

			if (e->Parent != this)
			{
				int NextIdx = Next ? Next->Pos.Find(e) : -1;
				if (NextIdx >= 0)
				{
					double NextX = MAP(NextIdx);
					Mem.Line(I(NextX), I(Ht+(Ht/2)), I(CurX), I(Ht/2));
				}
				else
				{
					Mem.Colour(GColour::Red);
					Mem.Line(I(CurX), I(Ht/2), I(CurX), I(Ht/2+5));
					Mem.Colour(GColour::Black);
				}
			}
		}

		if (NodeIdx >= 0)
		{
			double Cx = x;
			double Cy = Ht / 2;
			{
				GPath p;
				p.Circle(Cx, Cy, r + (Current ? 1 : 0));
				GSolidBrush sb(GColour::Black);
				p.Fill(&Mem, sb);
			}
			{
				GPath p;
				p.Circle(Cx, Cy, r-1);
				GSolidBrush sb(NodeColour);
				p.Fill(&Mem, sb);
			}
		}

		// Mem.ConvertPreMulAlpha(false);
		Ctx.pDC->Op(GDC_ALPHA);
		Ctx.pDC->Blt(Ctx.x1, Ctx.y1, &Mem);
	}
}

char *VcCommit::GetText(int Col)
{
	switch (Col)
	{
		case 0:
			// Cache.Printf("%i%s", (int)Parents.Length(), Current ? " ***" : "");
			return NULL;
		case 1:
			if (Index >= 0)
			{
				Cache.Printf(LPrintfInt64 " %s", Index, Rev.Get());
				return Cache;
			}			
			return Rev;
		case 2:
			return Author;
		case 3:
			Cache = Ts.Get();
			return Cache;
		case 4:
			if (!Msg)
				return NULL;
			Cache = Msg.Split("\n", 1)[0];
			return Cache;
	}

	return NULL;
}

bool VcCommit::GitParse(GString s, bool RevList)
{
	GString::Array lines = s.Split("\n");
	if (lines.Length() < 3)
		return false;

	if (RevList)
	{
		auto a = lines[0].SplitDelimit();
		if (a.Length() != 2)
			return false;

		Ts.SetUnix((uint64) a[0].Int());
		Rev = a[1];

		for (int i=0; i<lines.Length(); i++)
		{
			GString &Ln = lines[i];
			if (IsWhiteSpace(Ln(0)))
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
			GString &l = lines[ln];
			if (ln == 0)
				Rev = l.SplitDelimit().Last();
			else if (l.Find("Author:") >= 0)
				Author = l.Split(":", 1)[1].Strip();
			else if (l.Find("Date:") >= 0)
				Ts.Parse(l.Split(":", 1)[1].Strip());
			else if (l.Strip().Length() > 0)
			{
				if (Msg)
					Msg += "\n";
				Msg += l.Strip();
			}
		}
	}

	return Author && Rev;
}

bool VcCommit::CvsParse(LDateTime &Dt, GString Auth, GString Message)
{
	Ts = Dt;
	Ts.ToLocal();
	
	uint64 i;
	if (Ts.Get(i))
		Rev.Printf(LPrintfInt64, i);
	Author = Auth;
	Msg = Message;
	return true;
}

bool VcCommit::HgParse(GString s)
{
	GString::Array Lines = s.SplitDelimit("\n");
	if (Lines.Length() < 1)
		return false;

	for (GString *Ln = NULL; Lines.Iterate(Ln); )
	{
		GString::Array f = Ln->Split(":", 1);
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

	return Rev.Get() != NULL;
}

bool VcCommit::SvnParse(GString s)
{
	GString::Array lines = s.Split("\n");
	if (lines.Length() < 1)
		return false;

	for (unsigned ln = 0; ln < lines.Length(); ln++)
	{
		GString &l = lines[ln];
		if (ln == 0)
		{
			GString::Array a = l.Split("|");
			if (a.Length() > 3)
			{
				Rev = a[0].Strip(" \tr");
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

	return Author && Rev && Ts.IsValid();
}

VcFolder *VcCommit::GetFolder()
{
	for (GTreeItem *i = d->Tree->Selection(); i;
		i = i->GetParent())
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
		if (f)
			f->ListCommit(this);

		if (d->Msg)
		{
			d->Msg->Name(Msg);
			
			GWindow *w = d->Msg->GetWindow();
			if (w)
			{
				w->SetCtrlEnabled(IDC_COMMIT, false);
				w->SetCtrlEnabled(IDC_COMMIT_AND_PUSH, false);
			}
		}
		else LgiAssert(0);
	}
}

void VcCommit::OnMouseClick(GMouse &m)
{
	LListItem::OnMouseClick(m);

	if (m.IsContextMenu())
	{
		GSubMenu s;
		s.AppendItem("Merge With Local", IDM_MERGE, !Current);
		s.AppendItem("Update", IDM_UPDATE, !Current);		
		if (Rev)
			s.AppendItem("Copy Revision", IDM_COPY_REV);		
		if (Index >= 0)
			s.AppendItem("Copy Index", IDM_COPY_INDEX);
		
		int Cmd = s.Float(GetList(), m);
		switch (Cmd)
		{
			case IDM_MERGE:
			{
				VcFolder *f = GetFolder();
				if (!f)
				{
					LgiAssert(!"No folder?");
					break;
				}

				f->MergeToLocal(Rev);
				break;
			}
			case IDM_UPDATE:
			{
				VcFolder *f = GetFolder();
				if (!f)
				{
					LgiAssert(!"No folder?");
					break;
				}

				f->OnUpdate(Rev);
				break;
			}
			case IDM_COPY_REV:
			{
				GClipBoard c(GetList());
				c.Text(Rev);
				break;
			}
			case IDM_COPY_INDEX:
			{
				GClipBoard c(GetList());
				GString b;
				b.Printf(LPrintfInt64, Index);
				c.Text(b);
				break;
			}
		}
	}
}
