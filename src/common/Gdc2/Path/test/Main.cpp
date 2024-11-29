#include "lgi/common/Lgi.h"
#include "resdefs.h"
#include "lgi/common/Path.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "GPathTest";

class App : public LWindow
{
	LPath p;
	LMemDC img;

public:
    App() : img(_FL)
    {
        Name(AppName);
        LRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (Attach(0))
        {
			Test();
            Visible(true);
        }
    }

	void Test()
	{
		double r = 7.0;
		int x = (int)(r * 2.0);
		img.Create(x, x, System32BitColourSpace);
		img.Colour(0, 32);
		img.Rectangle();

		p.Circle(r, r, r);
		p.SetFillRule(FILLRULE_ODDEVEN);
		LSolidBrush s(LColour::White);
		p.Fill(&img, s);
		p.Empty();

		p.Circle(r, r, r);
		p.Circle(r, r, r - 1.0);
		p.SetFillRule(FILLRULE_ODDEVEN);
		LSolidBrush s2(LColour(0xcb, 0xcb, 0xcb));
		p.Fill(&img, s2);

		img.ConvertPreMulAlpha(true);
	}

	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(L_MED);
		pDC->Rectangle();

		pDC->Op(GDC_ALPHA);
		pDC->Blt(0, 0, &img);
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

