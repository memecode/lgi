#pragma once

#include "lgi/common/Layout.h"

////////////////////////////////////////////////////////////////////////////////////////////////
#define STATUSBAR_SEPARATOR			4
#define GSP_SUNKEN					0x0001

class LgiClass LStatusBar : public LLayout
{
	friend class LStatusPane;

public:
	LStatusBar();
	~LStatusBar();

	const char *GetClass() { return "LStatusBar"; }
	bool Pour(LRegion &r);
	void OnPaint(LSurface *pDC);
	void OnPosChange();

	LStatusPane *AppendPane(const char *Text, int Width);
	bool AppendPane(LStatusPane *Pane);
};

class LgiClass LStatusPane :
	public LView
{
	friend class LStatusBar;

protected:
	int		Flags;
	int		Width;
	LSurface *pDC;

public:
	LStatusPane();
	~LStatusPane();

	const char *GetClass() { return "LStatusPane"; }
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
