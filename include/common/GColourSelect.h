/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A colour selector control

#ifndef __GCOLOUR_SELECT_H
#define __GCOLOUR_SELECT_H

#include "GPopup.h"

/// \brief 32-bit colour button.
class GColourSelect :
	public GDropDown,
	public ResObject
{
	friend class GColourSelectPopup;

	GColour c;
	GArray<GColour> Presets;

public:
	enum
	{
		Transparent = 0
	};

	GColourSelect(GArray<GColour> *c32 = 0);

	// Methods
	void SetColourList(GArray<GColour> *c32 = 0);

	// GView
	int64 Value();
	void Value(int64 i);
	void Value(GColour c);
	void OnPaint(GSurface *pDC);
	bool OnLayout(GViewLayoutInfo &Inf);
};

#endif
