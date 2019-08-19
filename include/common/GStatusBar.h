#ifndef _GSTATUSBAR_H_
#define _GSTATUSBAR_H_

////////////////////////////////////////////////////////////////////////////////////////////////
#define STATUSBAR_SEPARATOR			4
#define GSP_SUNKEN					0x0001

class LgiClass GStatusBar : public GLayout
{
	friend class GStatusPane;

public:
	GStatusBar();
	~GStatusBar();

	const char *GetClass() { return "GStatusBar"; }
	bool Pour(GRegion &r);
	void OnPaint(GSurface *pDC);
	void OnPosChange();

	GStatusPane *AppendPane(const char *Text, int Width);
	bool AppendPane(GStatusPane *Pane);
};

class LgiClass GStatusPane :
	public GView
{
	friend class GStatusBar;

protected:
	int		Flags;
	int		Width;
	GSurface *pDC;

public:
	GStatusPane();
	~GStatusPane();

	const char *GetClass() { return "GStatusPane"; }
	char *Name() { return GBase::Name(); }
	bool Name(const char *n);
	void OnPaint(GSurface *pDC);

	int GetWidth();
	void SetWidth(int x);
	bool Sunken();
	void Sunken(bool i);
	GSurface *Bitmap();
	void Bitmap(GSurface *pdc);
};

#endif