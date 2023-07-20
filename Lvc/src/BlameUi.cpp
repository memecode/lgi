#include "lgi/common/Lgi.h"
#include "Lvc.h"
#include "VcFolder.h"

#define OPT_WND_STATE		"BlameUiState"

#define USE_LIST			1
#define USE_RELATIVE_TIMES	1

#if USE_LIST
#include "lgi/common/List.h"
#else
#include "lgi/common/TextLog.h"
#endif

struct BlameUiPriv
{
	#if USE_LIST
	LList *Lst = NULL;
	#else
	LTextLog *Log = NULL;
	#endif

	LString Output;
	AppPriv *Priv = NULL;
	VcFolder *Folder = NULL;
};

struct SrcLine : public LListItem
{
	BlameUiPriv *d = NULL;
	
	SrcLine(BlameUiPriv *priv)
	{
		d = priv;
	}
	
	void OnPaintColumn(LItem::ItemPaintCtx &Ctx, int i, LItemColumn *c) override
	{
		LColour old = Ctx.Fore;
		if (!Select())
		{
			if (i == 0)
				Ctx.Fore = LColour::Blue;
			else if (i == 2 || i == 3)
				Ctx.Fore.Rgb(192, 192, 192);
		}
		if (i == 3)
			Ctx.Align = LCss::AlignRight;
			
		LListItem::OnPaintColumn(Ctx, i, c);
		Ctx.Fore = old;
	}	
	
	
	void OnMouseClick(LMouse &m) override
	{
		int Col = -1;
		GetList()->GetColumnClickInfo(Col, m);
		
		if (Col == 0 &&
			m.Down() &&
			m.Double())
		{
			LAssert(d->Folder);
			if (d->Folder)
				d->Folder->SelectCommit(GetList()->GetWindow(), GetText(0));
		}
		
		LListItem::OnMouseClick(m);
	}
};

BlameUi::BlameUi(AppPriv *priv, VcFolder *folder, LString Output)
{
	d = new BlameUiPriv;
	d->Folder = folder;
	d->Output = Output;
	d->Priv = priv;
	Name("Lvc Blame");

	if (!SerializeState(&d->Priv->Opts, OPT_WND_STATE, true))
	{
		LRect r(0, 0, 800, 500);
		SetPos(r);
		MoveToCenter();
	}

	if (Attach(0))
	{
		#if USE_LIST
		AddView(d->Lst = new LList(100));
		d->Lst->AddColumn("Ref", 100);
		d->Lst->AddColumn("User", 100);
		d->Lst->AddColumn("Date", 100);
		d->Lst->AddColumn("Line", 100);
		d->Lst->AddColumn("Src", 1000);
		d->Lst->SetPourLargest(true);
		#else
		AddView(d->Log = new LTextLog(100));
		#endif
		
		AttachChildren();
		Visible(true);

		#if USE_LIST
		LDateTime now;
		now.SetNow();
		
		auto lines = d->Output.SplitDelimit("\r\n");
		for (auto ln: lines)
		{
			auto open = ln.Find("(");
			auto close = ln.Find(")", open);
			if (open > 0 && close > 0)
			{
				auto middle = open + 16;
				while (ln(middle) == ' ')
					middle++;

				auto line = close;
				while (line > 0 && IsDigit(ln(line-1)))
					line--;
			
				auto i = new SrcLine(d);
				
				int col = 0;
				i->SetText(ln(0, open).Strip(), col++);
				i->SetText(ln(open+1, middle).Strip(), col++);
				
				auto date = ln(middle, line).Strip();
				#if USE_RELATIVE_TIMES
				LDateTime dt;
				if (dt.Set(date))
					date = dt.DescribePeriod(now);
				#endif
				
				i->SetText(date, col++);
				i->SetText(ln(line, close).Strip(), col++);
				i->SetText(ln(close+1, -1), col++);
				
				d->Lst->Insert(i);
			}
		}
		d->Lst->ResizeColumnsToContent(16);
		#else
		d->Log->Name(Output);
		#endif
	}
}

BlameUi::~BlameUi()
{
	SerializeState(&d->Priv->Opts, OPT_WND_STATE, false);
	DeleteObj(d);
}
