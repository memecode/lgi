/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief Bitmap control

#pragma once

/// Bitmap control
class LgiClass LBitmap :
	public LControl,
	public ResObject
{
	LSurface *pDC = NULL;
	bool ownDC = true;
	class LThread *pThread;

public:
	/// Construct the bitmap control with the usual parameters
	LBitmap(int id, int x, int y, char *FileName, bool Async = false);
	~LBitmap();

	/// Sets the surface to display in the control
	virtual void SetDC(LSurface *pDC = NULL, bool owndc = true);
	/// Gets the surface being displayed
	virtual LSurface *GetSurface();

    void Empty();
    
	LMessage::Result OnEvent(LMessage *Msg) override;
	void OnPaint(LSurface *pDC) override;
	void OnMouseClick(LMouse &m) override;
	bool OnLayout(LViewLayoutInfo &Inf) override;
};
