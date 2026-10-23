#pragma once

#include "lgi/common/DisplayString.h"
#if LINUX
#include "lgi/common/Popup.h"
#endif

class LgiClass LPopupNotification :
	#if LINUX
	public LPopup
	#else
	public LWindow
	#endif
{
	constexpr static int DefPaddingPx = 10;
	constexpr static int ShowMs = 4000;		// milliseconds

	LColour cBack, cBorder, cFore;
	LWindow *RefWnd = nullptr;
	LArray<LDisplayString*> Msgs;
	LPoint Decor = LPoint(2, 2);
	uint64_t HideTs = 0;
	bool showPopup = false;

	LPoint CalcSize();
	int GetPaddingPx();
	void Layout();
	void Add(LWindow *ref, LString msg);

	struct TAction
	{
		LString label;
		int ctrlId = 0;
		class LButton *btn = nullptr;
		std::function<void()> callback;
	};
	LArray<TAction*> Actions;

public:
	// This creates AND shows a popup notification
	static LPopupNotification *Message(LWindow *ref, LString msg);
	// This just creates a popup notification without showing it.
	// Allowing for actions to be added for calling 'Show()'.
	// You could also override colours.
	static LPopupNotification *Create(LWindow *ref, LString msg);

	LPopupNotification(LWindow *ref, bool show, LString msg = LString());
	~LPopupNotification();

	void Show();
	LString GetMsgs();
	void Action(LString label, std::function<void()> callback);

	// LView impl
	void OnPaint(LSurface *pDC) override;
	void OnPulse() override;
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
};

