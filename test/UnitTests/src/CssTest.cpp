#include "lgi/common/Lgi.h"
#include "UnitTests.h"
#include "lgi/common/Css.h"
#include "lgi/common/Combo.h"
#include "lgi/common/TableLayout.h"

class PrivLCssTest
{
public:
	LCss c;

	bool Error(const char *Fmt, ...)
	{
		va_list Args;
		va_start(Args, Fmt);
		vprintf(Fmt, Args);
		va_end(Args);
		return false;
	}

	bool Parse(const char *In)
	{
		const char *Css = In;
		if (!c.Parse(Css))
			return Error("Error: Parse error:\n%s\n", In);
		return true;
	}

	bool LenIs(LCss::Len l, float val, int type)
	{
		return	l.Type == type &&
				l.Value == val;
	}

	bool Test1()
	{
		const char *s = "display: none;";
		if (Parse(s))
		{
			LCss::DisplayType dt = c.Display();
			if (dt != LCss::DispNone)
				return Error("Error: Value not set %s:%i\n", _FL);
		}

		s = "display: inline;";
		if (Parse(s) && c.Display() != LCss::DispInline)
			return Error("Error: Value not set %s:%i\n", _FL);

		s = "display: none;"
			"display: block;";
		if (Parse(s) && c.Display() != LCss::DispBlock)
			return Error("Error: Value not set %s:%i\n", _FL);

		s = "position: static";
		if (Parse(s) && c.Position() != LCss::PosStatic)
			return Error("Error: Value not set %s:%i\n", _FL);

		s = "position: absolute;";
		if (Parse(s) && c.Position() != LCss::PosAbsolute)
			return Error("Error: Value not set %s:%i\n", _FL);

		if (Parse(s = "top: 10%;"))
		{
			LCss::Len t = c.Top();
			if (t.Type != LCss::LenPercent || t.Value != 10.0)
				return Error("Error: Value not set %s:%i\n", _FL);
		}

		if (Parse(s = "right: 15pt;"))
		{
			LCss::Len t = c.Right();
			if (t.Type != LCss::LenPt || t.Value != 15.0)
				return Error("Error: Value not set %s:%i\n", _FL);
		}

		if (Parse(s = "bottom: 20px;"))
		{
			LCss::Len t = c.Bottom();
			if (t.Type != LCss::LenPx || t.Value != 20.0)
				return Error("Error: Value not set %s:%i\n", _FL);
		}

		if (Parse(s = "left: 4em;"))
		{
			LCss::Len t = c.Left();
			if (t.Type != LCss::LenEm || t.Value != 4.0)
				return Error("Error: Value not set %s:%i\n", _FL);
		}

		if (Parse(s = "float: left;") &&
			c.Float() != LCss::FloatLeft)
		{
			return Error("Error: Value not set %s:%i\n", _FL);
		}

		if (Parse(s = "z-index: 3;") &&
			c.ZIndex().Value != 3.0)
		{
			return Error("Error: Value not set %s:%i\n", _FL);
		}

		return true;
	}

	bool Test2()
	{
		const char *s;

		c.Empty();
		if (Parse(s =	"width: 50%;"
						"min-width: 10pt;"
						"max-width: 800px;"
						"height: 30px;"
						"min-height: 10px;"
						"max-height: 60px;"))
		{
			if (!LenIs(c.Width(), 50, LCss::LenPercent))
				return Error("Error %s:%i\n", _FL);
			if (!LenIs(c.MinWidth(), 10, LCss::LenPt))
				return Error("Error %s:%i\n", _FL);
			if (!LenIs(c.MaxWidth(), 800, LCss::LenPx))
				return Error("Error %s:%i\n", _FL);
			if (!LenIs(c.Height(), 30, LCss::LenPx))
				return Error("Error %s:%in", _FL);
			if (!LenIs(c.MinHeight(), 10, LCss::LenPx))
				return Error("Error %s:%i\n", _FL);
			if (!LenIs(c.MaxHeight(), 60, LCss::LenPx))
				return Error("Error %s:%i\n", _FL);
		}

		c.Empty();
		if (Parse(s =	"font-family: verdana, tahoma, sans;\n"
						"font-size: 16px;\n"
						"font-weight: bold;\n"))
		{
			char *f;
			LCss::FontFamilies s;
			s = c.FontFamily();
			if (s.Length() != 3)
				return Error("Not the right number of font families.", _FL);
			if (stricmp(f = s.Names[0], "verdana"))
				return Error("Wrong font family.", _FL);
			if (stricmp(f = s.Names[1], "tahoma"))
				return Error("Wrong font family.", _FL);
			if (stricmp(f = s.Names[2], "sans"))
				return Error("Wrong font family.", _FL);
		}

		LCss c2;
		c2 = c;
		if (c != c2)
			return Error("Compare failed.\n", _FL);

		c.Empty();
		if (Parse(s = "border-radius: 40px;") &&
			!LenIs(c.BorderRadius(), 40, LCss::LenPx))
			return Error("Border radius not set.\n", _FL);

		return true;
	}

	bool TestComboMaxWidth()
	{
		LTableLayout table;
		auto combo = new LCombo;
		if (!table.GetCell(0, 0)->Add(combo))
			return Error("Could not add combo to table.\n");
		combo->Insert("main");
		combo->Insert("origin/very-long-branch-name-very-long-branch-name-very-long-branch-name");

		LViewLayoutInfo natural = {};
		if (!combo->OnLayout(natural))
			return Error("Could not measure combo.\n");
		LRect bounds(0, 0, 999, 99);
		table.SetPos(bounds);
		table.OnPosChange();
		if (combo->X() != natural.Width.Max)
			return Error("Unconstrained combo width changed: %i != %i\n", combo->X(), natural.Width.Max);

		combo->CssStyles("max-width: 10em;");
		auto css = combo->GetCss(true);
		int limit = css->MaxWidth().ToPx(table.X(), combo->GetFont());
		if (natural.Width.Max <= limit)
			return Error("Long combo test item does not exceed max-width.\n");
		table.InvalidateLayout();
		table.OnPosChange();
		if (combo->X() != limit)
			return Error("Combo exceeded max-width: %i != %i\n", combo->X(), limit);

		combo->Empty();
		combo->Insert("main");
		natural = {};
		combo->OnLayout(natural);
		table.InvalidateLayout();
		table.OnPosChange();
		if (natural.Width.Max >= limit || combo->X() != natural.Width.Max)
			return Error("Short combo was not sized naturally: %i != %i\n", combo->X(), natural.Width.Max);

		combo->CssStyles("width: 500px; max-width: 10em;");
		table.InvalidateLayout();
		table.OnPosChange();
		if (combo->X() != limit)
			return Error("Explicit combo width exceeded max-width: %i != %i\n", combo->X(), limit);
		return true;
	}
};

LCssTest::LCssTest() : UnitTest("LCssTest")
{
	d = new PrivLCssTest;
}

LCssTest::~LCssTest()
{
	DeleteObj(d);
}

bool LCssTest::Run()
{
	return	d->Test1() &&
			d->Test2() &&
			d->TestComboMaxWidth();
}