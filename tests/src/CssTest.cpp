#include "lgi/common/Lgi.h"
#include "UnitTests.h"
#include "lgi/common/Css.h"

class LCssTestPriv
{
public:
	LCss c;

	bool Error(char *Fmt, ...)
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
		char *s = "display: none;";
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
		char *s;

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
			LCss::StringsDef s;
			s = c.FontFamily();
			if (s.Length() != 3)
				return Error("Not the right number of font families.", _FL);
			if (stricmp(f = s[0], "verdana"))
				return Error("Wrong font family.", _FL);
			if (stricmp(f = s[1], "tahoma"))
				return Error("Wrong font family.", _FL);
			if (stricmp(f = s[2], "sans"))
				return Error("Wrong font family.", _FL);
		}

		LCss c2;
		c2 = c;
		if (c != c2)
			return Error("Compare failed.\n", _FL);

		return true;
	}
};

LCssTest::LCssTest() : UnitTest("LCssTest")
{
	d = new LCssTestPriv;
}

LCssTest::~LCssTest()
{
	DeleteObj(d);
}

bool LCssTest::Run()
{
	return	d->Test1() &&
			d->Test2();
}