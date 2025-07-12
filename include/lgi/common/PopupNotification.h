#pragma once

#include "lgi/common/DisplayString.h"

class LgiClass LPopupNotification : public LWindow
{
	constexpr static int Border = 10;	// px
	constexpr static int ShowMs = 4000; // milliseconds

	LColour cBack, cBorder, cFore;
	LWindow *RefWnd = NULL;
	LArray<LDisplayString*> Msgs;
	LPoint Decor = LPoint(2, 2);
	uint64_t HideTs = 0;

	LPoint CalcSize();

public:
	static LPopupNotification *Message(LWindow *ref, LString msg);

	LPopupNotification(LWindow *ref, LString msg = LString());
	~LPopupNotification();

	void Init();
	void Add(LWindow *ref, LString msg);
	void OnPaint(LSurface *pDC);
	void OnPulse();
	LString GetMsgs();
};

