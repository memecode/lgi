/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief Bitmap control

#pragma once

/// Bitmap control
class LgiClass LBitmap :
	public LControl,
	public ResObject
{
	LSurface *pDC;
	class LThread *pThread;

public:
	/// Construct the bitmap control with the usual parameters
	LBitmap(int id, int x, int y, char *FileName, bool Async = false);
	~LBitmap();

	/// Sets the surface to display in the control
	virtual void SetDC(LSurface *pDC = 0);
	/// Gets the surface being displayed
	virtual LSurface *GetSurface();

	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
};
