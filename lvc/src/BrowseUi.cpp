
#include "lgi/common/Lgi.h"
#include "lgi/common/TabView.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/List.h"
#include "lgi/common/TextLog.h"

#include "Lvc.h"
#include "VcFolder.h"

#define OPT_WND_STATE		"BrowseUiState"
#define USE_RELATIVE_TIMES	1

enum Ctrls {
	IDC_STATIC = -1,
	IDC_FILTER = 100,
	IDC_TABLE,
};

struct BrowseUiPriv
{
	LList *Blame = NULL;
	LList *Log = NULL;
	LTextLog *Raw = NULL;
	LTabView *Tabs = NULL;
	AppPriv *Priv = NULL;
	VcFolder *Folder = NULL;
	LTableLayout *Tbl = NULL;

	BrowseUi::TMode Mode;
	LString Output;
	LString Path;
	LString UserHilight;
	LFont Mono;

	LArray<VcCommit*> Commits;
	LHashTbl<ConstStrKey<char>, LColour*> Colours;

	int NextHue = 0;
	int NextLum = 140;
	LColour *NewColour()
	{
		LColour *hls;
		if (hls = new LColour)
			hls->SetHLS(NextHue, NextLum, 128);
		NextHue += 30;
		if (NextHue >= 360)
		{
			NextHue = 0;
			NextLum -= 32;
		}
		return hls;
	}

	BrowseUiPriv(BrowseUi::TMode mode, LString path) :
		Mode(mode),
		Path(path)
	{
		LCss css;
		css.FontFamily(LCss::FontFamilyMonospace);
		auto status = Mono.CreateFromCss(&css);
		LAssert(status);

		LDisplayString ds(&Mono, "1234");
		Mono.TabSize(ds.X());
	}

	~BrowseUiPriv()
	{
		Colours.DeleteObjects();
		Commits.DeleteObjects();
	}

	void GotoRef(LString ref)
	{
		auto wnd = Blame->GetWindow();
		Folder->SelectCommit(wnd, ref, Path);
	}

	void GotoSource(const char *line)
	{
	}
};

enum LineIdx
{
	TUser,
	TRef,
	TDate,
	TLine,
	TSrc
};

struct BrowseItem : public LListItem
{
	BrowseUiPriv *d = NULL;
	LColour *refColour = NULL;
	
	BrowseItem(BrowseUiPriv *priv, BlameLine &ln, LDateTime &now, LColour *refCol, int &lineNo) :
		refColour(refCol)
	{
		d = priv;

		SetText(ln.user, TUser);
		SetText(ln.ref, TRef);

		#if USE_RELATIVE_TIMES
		LDateTime dt;
		if (ln.date && dt.Set(ln.date))
			SetText(dt.DescribePeriod(now), TDate);
		else
		#endif
			SetText(ln.date, TDate);

		if (ln.line)
			SetText(ln.line, TLine);
		else
			SetText(LString::Fmt("%i", lineNo), TLine);
		SetText(ln.src, TSrc);

		lineNo++;
	}
	
	void OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c) override
	{
		LColour old = Ctx.Fore;
		if (!Select())
		{
			if (i == TUser)
				Ctx.Fore = LColour::Blue;
			else if (i == TRef && refColour)
				Ctx.Fore = *refColour;
			else if (i == TDate || i == TLine)
				Ctx.Fore.Rgb(192, 192, 192);
		}
		if (i == TLine)
			Ctx.Align = LCss::AlignRight;
		
		const char *src;
		if (i == TSrc &&
			(src = GetText(TSrc)) )
		{
			d->Mono.Transparent(false);
			d->Mono.Colour(Ctx.Fore, Ctx.Back);
			LDisplayString ds(&d->Mono, src);
			ds.Draw(Ctx.pDC, Ctx.x1, Ctx.y1, &Ctx);
		}
		else
		{
			LListItem::OnPaintColumn(Ctx, i, c);
		}
		Ctx.Fore = old;
	}
	
	void OnMouseClick(LMouse &m) override
	{
		int Col = -1;
		GetList()->GetColumnClickInfo(Col, m);
		
		if (m.Down() &&
			m.Double())
		{
			LAssert(d->Folder);
			switch (Col)
			{
				case TUser:
				{
					// Highlight all by that user?
					d->UserHilight = GetText(TUser);
					GetList()->UpdateAllItems();
					break;
				}
				case TRef:
				{
					d->GotoRef(GetText(TRef));
					break;
				}
				case TSrc:
				{
					d->GotoSource(GetText(TLine));
					break;
				}
			}
		}
		
		LListItem::OnMouseClick(m);
	}
};

BrowseUi::BrowseUi(TMode mode, AppPriv *priv, VcFolder *folder, LString path)
{
	d = new BrowseUiPriv(mode, path);
	d->Folder = folder;
	d->Priv = priv;
	Name("Lvc Browse");

	if (!SerializeState(&d->Priv->Opts, OPT_WND_STATE, true))
	{
		LRect r(0, 0, 800, 500);
		SetPos(r);
		MoveToCenter();
	}

	if (Attach(0))
	{
		AddView(d->Tabs = new LTabView(IDC_TABS));

		auto BlameTab = d->Tabs->Append("Blame");
		BlameTab->Append(d->Tbl = new LTableLayout(IDC_TABLE));
		auto c = d->Tbl->GetCell(0, 0);
		c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Filter:"));
		c->VerticalAlign(LCss::VerticalMiddle);
		c = d->Tbl->GetCell(1, 0);
		c->Add(new LEdit(IDC_FILTER, 0, 0, -1, -1));
		c = d->Tbl->GetCell(0, 1, true, 2);
		c->Add(d->Blame = new LList(IDC_BLAME));
		d->Blame->AddColumn("Ref", 100);
		d->Blame->AddColumn("User", 100);
		d->Blame->AddColumn("Date", 100);
		d->Blame->AddColumn("Line", 100);
		d->Blame->AddColumn("Src", 1000);
		d->Blame->SetPourLargest(true);

		auto LogTab = d->Tabs->Append("Log");
		LogTab->Append(d->Log = new LList(IDC_LOG));
		folder->UpdateColumns(d->Log);
		d->Log->SetPourLargest(true);

		auto RawTab = d->Tabs->Append("Raw");
		RawTab->Append(d->Raw = new LTextLog(IDC_RAW));

		AttachChildren();
		Visible(true);

		LView *e;
		if (GetViewById(IDC_FILTER, e))
			e->Focus(true);
	}
}

BrowseUi::~BrowseUi()
{
	SerializeState(&d->Priv->Opts, OPT_WND_STATE, false);
	DeleteObj(d);
}

void BrowseUi::ParseBlame(LArray<BlameLine> &lines, LString raw)
{
	d->Colours.DeleteObjects();
	d->Blame->Empty();
	d->Raw->Name(d->Output = raw);

	LDateTime now;
	now.SetNow();

	List<LListItem> items;
	int lineNo = 1;
	for (auto ln: lines)
	{
		auto col = d->Colours.Find(ln.ref);
		if (!col)
			d->Colours.Add(ln.ref, col = d->NewColour());

		items.Insert(new BrowseItem(d, ln, now, col, lineNo));
	}

	d->Blame->Insert(items);
	d->Blame->ResizeColumnsToContent(16);
}

void BrowseUi::ParseLog(LArray<VcCommit*> &commits, LString raw)
{
	d->Commits.Swap(commits);
	d->Log->Empty();
	d->Raw->Name(d->Output = raw);
	d->Tabs->Value(1);

	LDateTime now;
	now.SetNow();

	for (auto commit: d->Commits)
		d->Log->Insert(commit);

	d->Log->ResizeColumnsToContent();
}

int BrowseUi::OnNotify(LViewI *Ctrl, LNotification n)
{
	switch (Ctrl->GetId())
	{
		case IDC_FILTER:
		{
			auto f = Ctrl->Name();
			LArray<BrowseItem*> items;
			if (!d->Blame->GetAll(items))
				break;

			auto line = Atoi(f);
			if (line > 0 && line <= (ssize_t)items.Length())
			{
				for (auto i: items)
				{
					auto lineTxt = i->GetText(TLine);
					auto n = Atoi(lineTxt);
					bool match = n > 0 && n == line;
					i->Select(match);
					if (match)
						i->ScrollTo();
				}
			}
			else
			{
				bool first = true;
				for (auto i: items)
				{
					auto src = i->GetText(TSrc);
					auto match = Stristr(src, f) != NULL;
					i->Select(match);
					if (match && first)
					{
						first = false;
						i->ScrollTo();
					}
				}
			}
			break;
		}
	}

	return LWindow::OnNotify(Ctrl, n);
}
