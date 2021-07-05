/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A colour selector control

#pragma once

#include "lgi/common/Popup.h"

/// \brief 32-bit colour button.
class LColourSelect :
	public LDropDown,
	public ResObject
{
	friend class LColourSelectPopup;

	GColour c;
	LArray<GColour> Presets;

public:
	enum
	{
		Transparent = 0
	};

	LColourSelect(LArray<GColour> *c32 = 0);

	// Methods
	void SetColourList(LArray<GColour> *c32 = 0);

	// LView
	int64 Value();
	void Value(int64 i);
	void Value(GColour c);
	void OnPaint(LSurface *pDC);
	bool OnLayout(LViewLayoutInfo &Inf);
};

