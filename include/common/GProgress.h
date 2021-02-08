/// \file
/// \author Matthew Allen (fret@memecode.com)
/// /brief A progress control

#ifndef _GPROGRESS_H_
#define _GPROGRESS_H_

/// Progress bar control
class LgiClass GProgress :
	public GControl,
	public Progress,
	public ResObject
{
	GColour c;
	#ifdef WIN32
	int Shift;
	#endif

public:
	static GColour cNormal;
	static GColour cPaused;
	static GColour cError;

	GProgress(int id, int x, int y, int cx, int cy, const char *name);
	~GProgress();

	const char *GetClass() override { return "GProgress"; }
	bool SetRange(const GRange &r) override;
	void Value(int64 v) override;
	int64 Value() override;
	GMessage::Result OnEvent(GMessage *Msg) override;
	bool OnLayout(GViewLayoutInfo &Inf) override;
	bool Pour(GRegion &r) override;
	bool Colour(GColour Col);
	GColour Colour();

	#if WINNATIVE
	GString CssStyles(const char *CssStyle = NULL);
	#else
	void OnPaint(GSurface *pDC);
	#endif
};

#endif
