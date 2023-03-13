#pragma once

#include "lgi/common/DisplayString.h"

class LgiClass LPopupNotification : public LWindow
{
	constexpr static int Border = 10; // px
	constexpr static int ShowMs = 2300; // milliseconds

	LColour Back = LColour(0xf7, 0xf0, 0xd5);
	LColour Fore = LColour(0xd4, 0xb8, 0x62);
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
};

