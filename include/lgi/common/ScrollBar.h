/// \file
/// \author Matthew Allen (fret@memecode.com)
/// /brief A tree/heirarchy control

#ifndef __GScrollBar_h
#define __GScrollBar_h

/// \brief Scroll bar control
///
/// This control can be used as an actual free standing window or to address built in controls on a LLayout
/// view.
class LgiClass LScrollBar :
	public LControl,
	public ResObject
{
	friend class LLayout;

protected:
	class LScrollBarPrivate *d;

	#if WINNATIVE
	LViewI *GetMyView();
	void Update();
	void SetParentFlag(bool Bool);
	#endif

public:
	static int SCROLL_BAR_SIZE;

	/// Returns the size of the bar, i.e. the width if vertical or the height if horizontal
	static int GetScrollSize();

	/// Call this constructor for embeded scrollbar say in a window
	LScrollBar();
	
	const char *GetClass() override { return "LScrollBar"; }

	/// Call this constructor for a control based scrollbar, say in a dialog
	LScrollBar(int id, int x, int y, int cx, int cy, const char *name);
	~LScrollBar();

	/// True if vertical
	bool Vertical();
	/// Makes the scrollar vertical
	void SetVertical(bool v);
	/// Gets the current position of the scrollbar
	
	int64 Value() override;
	/// Sets the position of the scrollbar
	void Value(int64 p) override;

	// Gets the range of the scoll bar
	LRange GetRange() const;
	/// Sets the range of the scroll bar
	bool SetRange(const LRange &r);
	
	/// Gets the page size
	int64 Page();
	/// Sets the page size
	void SetPage(int64 p);

	/// Returns true if the range is valid
	bool Valid();

	#if WINNATIVE
	bool SetPos(LRect &p, bool Repaint = false);
	void SetParent(LViewI *p);
	bool Invalidate(LRect *r = NULL, bool Repaint = false, bool NonClient = false);
	#else
	bool Attach(LViewI *p) override;
	void OnPaint(LSurface *pDC) override;
	void OnPosChange() override;
	void OnMouseClick(LMouse &m) override;
	void OnMouseMove(LMouse &m) override;
	bool OnKey(LKey &k) override;
	bool OnMouseWheel(double Lines) override;
	void OnPulse() override;
	#endif
	
	// events
	LMessage::Result OnEvent(LMessage *Msg) override;

	/// Called when the value changes
	virtual void OnChange(int Pos) {}
	/// Called when the Limits or Page changes
	virtual void OnConfigure() {}

	/// Gets the limits (use GetRange)
	[[deprecated]] void Limits(int64 &Low, int64 &High);
	/// Sets the limits (use SetRange)
	[[deprecated]] void SetLimits(int64 Low, int64 High);
};

#endif
