#include "lgi/common/Lgi.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/StatusBar.h"

//////////////////////////////////////////////////////////////////////////////////////
LStatusBar::LStatusBar()
{
	Name("LGI_StatusBar");
	Raised(true);
	_BorderSize = 1;
	LResources::StyleElement(this);
}

LStatusBar::~LStatusBar()
{
}

bool LStatusBar::Pour(LRegion &r)
{
	LRect *Best = FindLargestEdge(r, GV_EDGE_BOTTOM);
	if (Best)
	{
		LRect Take = *Best;
		if (Take.Y() > 21)
		{
			Take.y1 = MAX((Take.y2 - 21), Take.y1);
			SetPos(Take);
			return true;
		}
	}
	return false;
}

void LStatusBar::OnPaint(LSurface *pDC)
{
	pDC->Colour(L_MED);
	pDC->Rectangle();
}

void LStatusBar::OnPosChange()
{
	int x = X() - 5;
	auto i = Children.Length() - 1;
	auto c = GetClient();
	c.y2 -= 6;

	for (auto w: Children)
	{
		LStatusPane *Pane = dynamic_cast<LStatusPane*>(w);
		if (Pane)
		{
			#ifndef WIN32
			if (!Pane->IsAttached())
				Pane->Attach(this);
			#endif
			
			if (i)
			{
				// not first
				x -= Pane->Width; 

				LRect r;
				r.ZOff(Pane->Width, c.Y()-1);
				r.Offset(x, 2);
				Pane->SetPos(r);
				x -= STATUSBAR_SEPARATOR;
			}
			else
			{
				// first
				LRect r;
				r.ZOff(x-2, c.Y()-1);
				r.Offset(2, 2);
				Pane->SetPos(r);
				x = 0;
			}
		}

		i--;
	}
}

LStatusPane *LStatusBar::AppendPane(const char *Text, int Width)
{
	LStatusPane *Pane = 0;
	if (Text)
	{
		Pane = new LStatusPane;
		if (Pane)
		{
			AddView(Pane);
			Pane->Name(Text);
			Pane->Width = Width;
		}
	}
	return Pane;
}

bool LStatusBar::AppendPane(LStatusPane *Pane)
{
	if (!Pane)
		return false;

	return AddView(Pane);
}

// Pane
LStatusPane::LStatusPane()
{
	SetParent(0);
	Flags = 0;
	Width = 32;
	LRect r(0, 0, Width-1, 20);
	SetPos(r);
	pDC = 0;
	_BorderSize = 1;
}

LStatusPane::~LStatusPane()
{
	DeleteObj(pDC)
}

bool LStatusPane::Name(const char *n)
{
	bool Status = false;

	if (Lock(_FL))
	{
		auto l = Name();
		if (!n ||
			!l ||
			strcmp(l, n) != 0)
		{
			Status = LBase::Name(n);
			LRect p(0, 0, X()-1, Y()-1);
			p.Size(1, 1);
			Invalidate(&p);
		}
		Unlock();
	}

	return Status;
}

void LStatusPane::OnPaint(LSurface *pDC)
{
	LAutoString t;
	if (Lock(_FL))
	{
		t.Reset(NewStr(Name()));
		Unlock();
	}

	LRect r = GetClient();
	if (ValidStr(t))
	{
		int TabSize = LSysFont->TabSize();

		LSysFont->TabSize(0);
		LSysFont->Colour(L_TEXT, L_MED);
		LSysFont->Transparent(false);

		LDisplayString ds(LSysFont, t);
		ds.Draw(pDC, 1, (r.Y()-ds.Y())/2, &r);
		
		LSysFont->TabSize(TabSize);
	}
	else
	{
		pDC->Colour(L_MED);
		pDC->Rectangle(&r);
	}
}

int LStatusPane::GetWidth()
{
	return Width;
}

void LStatusPane::SetWidth(int x)
{
	Width = x;
	if (GetParent())
	{
		LStatusBar *Bar = dynamic_cast<LStatusBar*>(GetParent());
		if (Bar)
			Bar->OnPosChange();
	}
}

bool LStatusPane::Sunken()
{
	return TestFlag(Flags, GSP_SUNKEN);
}

void LStatusPane::Sunken(bool s)
{
	bool Old = Sunken();
	if (s) SetFlag(Flags, GSP_SUNKEN);
	else ClearFlag(Flags, GSP_SUNKEN);
	if (Old != Sunken() && GetParent())
	{
		GetParent()->Invalidate();
	}
}

LSurface *LStatusPane::Bitmap()
{
	return pDC;
}

void LStatusPane::Bitmap(LSurface *pNewDC)
{
	DeleteObj(pDC);
	if (pNewDC)
	{
		pDC = new LMemDC;
		if (pDC)
		{
			if (pDC->Create(pNewDC->X(), pNewDC->Y(), pNewDC->GetColourSpace()))
			{
				pDC->Blt(0, 0, pNewDC);
			}
			else
			{
				DeleteObj(pDC);
			}
		}
	}

	if (GetParent())
	{
		GetParent()->Invalidate();
	}
}

