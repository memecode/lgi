#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/ColourSelect.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/ColourSelect.h"

class LColourSelectPopup : public LPopup
{
	LColourSelect *Colour;
	int Ly;

public:
	LColourSelectPopup(LColourSelect *Parent) : LPopup(Parent)
	{
	    Ly = SysFont->GetHeight();
		SetParent(Colour = Parent);
		LRect r(0, 0, 120, Ly + 4);
		SetPos(r);
	}
	
	const char *GetClass() { return "LColourSelectPopup"; }

	void OnPaint(LSurface *pDC)
	{
		LRect r = GetClient();
		LWideBorder(pDC, r, DefaultRaisedEdge);
		pDC->Colour(L_MED);
		pDC->Rectangle(&r);

		SysFont->Transparent(true);
		if (Colour->Presets.Length())
		{
			char s[64];
			
			SysFont->Colour(L_BLACK, L_MED);
			LDisplayString ds(SysFont, (char*)LgiLoadString(L_COLOUR_NONE, "No Colour"));
			ds.Draw(pDC, r.x1 + 2, r.y1 + 2);

			for (unsigned i=0; i<Colour->Presets.Length(); i++)
			{
				int y = r.y1 + ((i+1) * Ly);
				GColour p = Colour->Presets[i];

				pDC->Colour(p);
				pDC->Rectangle(r.x1+1, y+1, r.x1+Ly-1, y+Ly-1);
				
				sprintf_s(s, sizeof(s), "%2.2X,%2.2X,%2.2X", p.r(), p.g(), p.b());
				LDisplayString ds(SysFont, s);
				ds.Draw(pDC, r.x1 + Ly + 10, y + 2);
			}
		}
	}

	void OnMouseClick(LMouse &m)
	{
		LRect r = GetClient();
		r.Size(2, 2);
		if (m.Down() && r.Overlap(m.x, m.y))
		{
			int i = m.y / Ly;
			if (i == 0)
			{
				Colour->Value(0);
			}
			else
			{
				Colour->Value(Colour->Presets[i-1]);
			}

			Visible(false);
		}
	}

	int64 Value() { return Colour->Value(); }
	void Value(int64 i) { Colour->Value(i); }
};

LColourSelect::LColourSelect(GArray<GColour> *cols) :
	LDropDown(-1, 0, 0, 10, 10, 0), ResObject(Res_Custom)
{
	c.Rgb(0, 0, 255);

	SetPopup(new LColourSelectPopup(this));
	if (cols)
		SetColourList(cols);
}

void LColourSelect::SetColourList(GArray<GColour> *cols)
{
	if (cols)
	{
		Presets = *cols;
	}

	if (GetPopup())
	{
		LRect r(0, 0, 100, (MAX((int)Presets.Length()+1, 1) * SysFont->GetHeight()) + 4);
		GetPopup()->SetPos(r);
	}
}

int64 LColourSelect::Value()
{
	return c.c32();
}

void LColourSelect::Value(GColour set)
{
	if (c != set)
	{
		c = set;
		Invalidate();

		LViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
			n->OnNotify(this, GNotifyValueChanged);
	}
}

void LColourSelect::Value(int64 i)
{
	if (c.c32() != i)
	{
		if (i >= 0)
			c.Set((uint32_t)i, 32);
		else
			c.Empty();
		Invalidate();

		LViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
			n->OnNotify(this, 0);
	}
}

void LColourSelect::OnPaint(LSurface *pDC)
{
	LDropDown::OnPaint(pDC);

	LRect r = GetClient();
	r.x1 += 2;
	r.x2 -= 14;
	r.Size(5, 5);
	if (IsOpen()) r.Offset(1, 1);

	bool HasColour = Enabled() && c.IsValid();
	pDC->Colour(HasColour ? L_BLACK : L_LOW);
	pDC->Box(&r);

	r.Size(1, 1);
	pDC->Colour(HasColour ? c : LColour(L_LOW));
	if (HasColour)
	{
		pDC->Rectangle(&r);
	}
	else
	{
		pDC->Line(r.x1, r.y1, r.x2, r.y2);
		pDC->Line(r.x1, r.y2, r.x2, r.y1);
	}
}

bool LColourSelect::OnLayout(LViewLayoutInfo &Inf)
{
    Inf.Width.Min = Inf.Width.Max = 80;
    Inf.Height.Min = Inf.Height.Max = SysFont->GetHeight() + 6;
    return true;
}

class GColourFactory : public LViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (Class &&
			_stricmp(Class, "LColourSelect") == 0)
		{
			return new LColourSelect;
		}

		return 0;
	}

} ColourFactory;
