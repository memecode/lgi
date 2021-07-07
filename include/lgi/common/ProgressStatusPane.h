
#ifndef __PROGRESS_STATUS_PANE
#define __PROGRESS_STATUS_PANE

#include "lgi/common/StatusBar.h"

// Progress pane
class GProgressStatusPane : public LStatusPane, public Progress, public DoEvery {

public:
	GProgressStatusPane();
	~GProgressStatusPane();

	void OnPaint(LSurface *pDC);
	int64 Value();
	void Value(int64 v);
};

#endif
