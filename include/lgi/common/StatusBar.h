#pragma once

#include "lgi/common/Layout.h"
#include "lgi/common/Progress.h"

////////////////////////////////////////////////////////////////////////////////////////////////
class LgiClass LStatusBar : public LLayout
{
	friend class LStatusPane;

public:
	constexpr static int SEPARATOR_PX = 4;

	LStatusBar(int id = -1);
	~LStatusBar();

	const char *GetClass() { return "LStatusBar"; }
	bool Pour(LRegion &r);
	bool OnLayout(LViewLayoutInfo &Inf);
	void OnPaint(LSurface *pDC);
	void OnPosChange();
	int OnNotify(LViewI *Ctrl, LNotification &n) override;

	LStatusPane *AppendPane(const char *Text, int WidthPx = 0);
	bool AppendPane(LStatusPane *Pane);
};

class LgiClass LStatusPane :
	public LView
{
	friend class LStatusBar;

protected:
	LAutoPtr<LSurface> Icon;

public:
	LStatusPane();

	const char *GetClass() { return "LStatusPane"; }
	const char *Name() { return LBase::Name(); }
	bool Name(const char *n);
	void OnPaint(LSurface *pDC);

	int GetWidth();
	void SetWidth(int px);
	bool Sunken();
	void Sunken(bool inset);
	LSurface *Bitmap();
	void Bitmap(LAutoPtr<LSurface> pdc); // will take ownership
};

class LgiClass LProgressStatus :
	public LStatusPane,
	public Progress
{
public:
	int64 Value() override;
	void Value(int64 v) override;
	void OnPaint(LSurface *pDC) override;
};
