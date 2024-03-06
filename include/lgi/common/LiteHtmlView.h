#pragma once

class LiteHtmlView :
	public LLayout
{
protected:
	struct LiteHtmlViewPriv *d;

	bool LoadCurrent();

public:
	LiteHtmlView(int id);
	~LiteHtmlView();

	/// Set a new URL
	bool SetUrl(LString url);
	void HistoryBack();
	void HistoryForward();
	bool Refresh();

	// Events:
	// The current url has been set and the document created, but not fully loaded
	virtual void OnNavigate(LString url);
	// The history position has changed and the validity of going forward/back needs to be updated
	virtual void OnHistory(bool hasBack, bool hasForward) {}

	// LLayout impl
	void OnAttach() override;
	LCursor GetCursor(int x, int y) override;
	void OnPaint(LSurface *pDC) override;
	int OnNotify(LViewI *c, LNotification n) override;
	bool OnMouseWheel(double Lines);
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	LMessage::Result OnEvent(LMessage *Msg) override;
};
