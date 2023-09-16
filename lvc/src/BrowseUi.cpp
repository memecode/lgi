
#include "lgi/common/Lgi.h"
#include "lgi/common/TabView.h"

#include "Lvc.h"
#include "VcFolder.h"

#define OPT_WND_STATE		"BrowseUiState"

#define USE_RELATIVE_TIMES	1

#include "lgi/common/List.h"
#include "lgi/common/TextLog.h"

struct BrowseUiPriv
{
	LList *Blame = NULL;
	LList *Log = NULL;
	LTextLog *Raw = NULL;

	BrowseUi::TMode Mode;
	LTabView *Tabs = NULL;
	LString Output;
	AppPriv *Priv = NULL;
	VcFolder *Folder = NULL;
	LString Path;
	LString UserHilight;

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
	}

	~BrowseUiPriv()
	{
		Colours.DeleteObjects();
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
	
	BrowseItem(BrowseUiPriv *priv, BlameLine &ln, LDateTime &now, LColour *refCol) :
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

		SetText(ln.line, TLine);
		SetText(ln.src, TSrc);
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
			
		LListItem::OnPaintColumn(Ctx, i, c);
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
		BlameTab->Append(d->Blame = new LList(IDC_BLAME));
		d->Blame->AddColumn("Ref", 100);
		d->Blame->AddColumn("User", 100);
		d->Blame->AddColumn("Date", 100);
		d->Blame->AddColumn("Line", 100);
		d->Blame->AddColumn("Src", 1000);
		d->Blame->SetPourLargest(true);

		auto LogTab = d->Tabs->Append("Log");
		LogTab->Append(d->Log = new LList(IDC_LOG));
		d->Log->SetPourLargest(true);

		auto RawTab = d->Tabs->Append("Raw");
		RawTab->Append(d->Raw = new LTextLog(IDC_RAW));

		AttachChildren();
		Visible(true);
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
	for (auto ln: lines)
	{
		auto col = d->Colours.Find(ln.ref);
		if (!col)
			d->Colours.Add(ln.ref, col = d->NewColour());

		items.Insert(new BrowseItem(d, ln, now, col));
	}

	d->Blame->Insert(items);
	d->Blame->ResizeColumnsToContent(16);
}

void BrowseUi::ParseLog(LString Content)
{
	d->Output = Content;
}
