/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief Bitmap control

#ifndef _GBITMAP_H_
#define _GBITMAP_H_

/// Bitmap control
class LgiClass GBitmap :
	public GControl,
	public ResObject
{
	LSurface *pDC;
	class LThread *pThread;

public:
	/// Construct the bitmap control with the usual parameters
	GBitmap(int id, int x, int y, char *FileName, bool Async = false);
	~GBitmap();

	/// Sets the surface to display in the control
	virtual void SetDC(LSurface *pDC = 0);
	/// Gets the surface being displayed
	virtual LSurface *GetSurface();

	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
};

#endif