#include "Lvc.h"
#include "GClipBoard.h"
#include "../Resources/resdefs.h"
#include "GPath.h"

VcCommit::VcCommit(AppPriv *priv)
{
	d = priv;
	Current = false;
	NodeIdx = -1;
	Parents.SetFixedLength(false);
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
		double r = Half - 2;

		bool Dbg = IsRev("938e8ccfdac365e91ca7ca732aab538d8769a37e");
		if (Dbg)
		{
			Mem.Colour(GColour::Red);
			// Mem.Rectangle();
		}

		double x = MAP(NodeIdx);
		if (NodeIdx >= 0)
		{
			double Cx = x;
			double Cy = Ht / 2;
			GPath p;
			p.Circle(Cx, Cy, r);
			//p.Circle(Cx, Cy, r - 1);
			GSolidBrush sb(GColour::Black);
			p.Fill(&Mem, sb);
		}

		Mem.Colour(GColour::Black);
		for (unsigned i=0; i<Nodes.Length(); i++)
		{
			auto &n = Nodes[i];
			double mx = MAP(i);
			for (auto pi:n.Prev)
			{
				double px = MAP(pi);
				Mem.Line(px, -(Ht/2), mx, (Ht/2));
			}
			for (auto ni:n.Next)
			{
				double nx = MAP(ni);
				Mem.Line(nx, Ht+(Ht/2), mx, (Ht/2));
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
		Ts.Set(a[0].Int());
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
				Rev = f[1].Strip();
			else if (f[0].Equals("user"))
				Author = f[1].Strip();
			else if (f[0].Equals("date"))
				Ts.Parse(f[1].Strip());
			else if (f[0].Equals("summary"))
				Msg = f[1].Strip();
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
		s.AppendItem("Update", IDM_UPDATE, !Current);
		s.AppendItem("Copy Revision", IDM_COPY_REV);
		int Cmd = s.Float(GetList(), m);
		switch (Cmd)
		{
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
		}
	}
}
