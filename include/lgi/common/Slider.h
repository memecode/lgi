/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A slider control

#ifndef _GSLIDER_H_
#define _GSLIDER_H_

/// Slider
class LgiClass LSlider :
	public LControl,
	public ResObject
{
	#if WINNATIVE
	uint32_t Style();
	// int SysOnNotify(int Msg, int Code);
	#endif

	bool Vertical;
	int64 Min, Max;
	int64 Val;

	LRect Thumb;
	int Tx, Ty;

public:
	LSlider(int id,
			int x = 0, int y = 0,
			int cx = 100, int cy = 20,
			const char *name = NULL,
			bool vert = false);
	~LSlider();

	const char *GetClass() { return "LSlider"; }

	/// Sets the position of the slider
	void Value(int64 i);
	/// Gets the position of the slider
	int64 Value();

	// Gets the range of the slider
	LRange GetRange();
	/// Sets the range of the slider
	bool SetRange(const LRange &r);

	bool OnLayout(LViewLayoutInfo &Inf);
	LMessage::Result OnEvent(LMessage *Msg);
	
	#if !WINNATIVE
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
	#endif

	/// Gets the limits of the slider (use GetRange)
	[[deprecated]] void GetLimits(int64 &x, int64 &y);
	/// Sets the limits of the slider (use SetRange)
	[[deprecated]] void SetLimits(int64 x, int64 y);
};

#endif
