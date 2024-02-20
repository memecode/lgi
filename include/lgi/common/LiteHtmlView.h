#pragma once

class LiteHtmlView :
	public LLayout
{
protected:
	struct LiteHtmlViewPriv *d;

public:
	LiteHtmlView(int id);
	~LiteHtmlView();

	bool SetUrl(LString url);

	// LLayout impl
	void OnAttach() override;
	LCursor GetCursor(int x, int y) override;
	void OnPaint(LSurface *pDC) override;
	int OnNotify(LViewI *c, LNotification n) override;
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
};
