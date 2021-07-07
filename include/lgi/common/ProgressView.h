/// \file
/// \author Matthew Allen (fret@memecode.com)
/// /brief A progress control

#ifndef _LProgressView_H_
#define _LProgressView_H_

#include "lgi/common/Progress.h"

/// Progress bar control
class LgiClass LProgressView
	: public LControl
	, public Progress
	, public ResObject
{
	LColour c;
	#ifdef WIN32
	int Shift;
	#endif

public:
	static LColour cNormal;
	static LColour cPaused;
	static LColour cError;

	LProgressView(int id, int x, int y, int cx, int cy, const char *name);
	virtual ~LProgressView();

	const char *GetClass() override { return "LProgressView"; }
	bool SetRange(const LRange &r) override;
	void Value(int64 v) override;
	int64 Value() override;
	GMessage::Result OnEvent(GMessage *Msg) override;
	bool OnLayout(LViewLayoutInfo &Inf) override;
	bool Pour(LRegion &r) override;
	bool Colour(LColour Col);
	LColour Colour();

	#if WINNATIVE
	LString CssStyles(const char *CssStyle = NULL);
	#else
	void OnPaint(LSurface *pDC) override;
	#endif
};

#endif
