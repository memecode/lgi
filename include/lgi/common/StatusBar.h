#pragma once

#include "lgi/common/Layout.h"

////////////////////////////////////////////////////////////////////////////////////////////////
#define STATUSBAR_SEPARATOR			4
#define GSP_SUNKEN					0x0001

class LgiClass LStatusBar : public LLayout
{
	friend class GStatusPane;

public:
	LStatusBar();
	~LStatusBar();

	const char *GetClass() { return "LStatusBar"; }
	bool Pour(LRegion &r);
	void OnPaint(LSurface *pDC);
	void OnPosChange();

	GStatusPane *AppendPane(const char *Text, int Width);
	bool AppendPane(GStatusPane *Pane);
};

class LgiClass GStatusPane :
	public LView
{
	friend class LStatusBar;

protected:
	int		Flags;
	int		Width;
	LSurface *pDC;

public:
	GStatusPane();
	~GStatusPane();

	const char *GetClass() { return "GStatusPane"; }
	const char *Name() { return LBase::Name(); }
	bool Name(const char *n);
	void OnPaint(LSurface *pDC);

	int GetWidth();
	void SetWidth(int x);
	bool Sunken();
	void Sunken(bool i);
	LSurface *Bitmap();
	void Bitmap(LSurface *pdc);
};
