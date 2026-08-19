#pragma once

class LiteHtmlView :
	public LDocView
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
	// Set the page name in the title bar of the window.
	virtual void SetCaption(LString name) {}

	// LView impl:
	const char *Name() override;
	bool Name(const char *n) override;

	// LLayout impl
	void OnAttach() override;
	LCursor GetCursor(int x, int y) override;
	void OnPaint(LSurface *pDC) override;
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
	bool OnMouseWheel(double Lines);
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	LMessage::Result OnEvent(LMessage *Msg) override;

	// LDocView impl:
	const char *GetMimeType() override { return "text/html"; }
};
