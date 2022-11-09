
#ifndef __PROGRESS_STATUS_PANE
#define __PROGRESS_STATUS_PANE

#include "lgi/common/StatusBar.h"

// Progress pane
class LProgressStatusPane : public LStatusPane, public Progress, public DoEvery {

public:
	LProgressStatusPane();
	~LProgressStatusPane();

	void OnPaint(LSurface *pDC);
	int64 Value();
	void Value(int64 v);
};

#endif
