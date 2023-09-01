/// \file
/// \author Matthew Allen

#ifndef __GHISTORY__
#define __GHISTORY__

#include "lgi/common/Popup.h"

/// A history drop down for an edit box
class LHistory : public LDropDown, public List<char>, public ResObject
{
	class LHistoryPrivate *d;

public:
	LHistory();
	~LHistory();

	// Props
	int GetTargetId();
	void SetTargetId(int id);
	
	int64 Value();
	void Value(int64 i);
	
	/// Call after you change the list of strings
	void Update();
	void OnPopupClose();
	int Add(const char *s);
	
	// Impl
	bool OnLayout(LViewLayoutInfo &Inf);
};

#endif
