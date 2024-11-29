/// \author Matthew Allen <fret@memecode.com>
/// \brief Allows display of a zoomable and scroll bitmap using tiles.
#ifndef _ZOOM_VIEW_H_
#define _ZOOM_VIEW_H_

#include "lgi/common/Path.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Layout.h"

class LZoomView;

class LZoomViewCallback
{
public:
	enum TTileType
	{
		Tile16,
		Tile32,
		TileCustom
	};

	#define DefOption(type, name, default) \
		virtual type name() { return default; } \
		virtual void name(type i) {}

	DefOption(int, DisplayGrid, false);
	DefOption(int, GridSize, 0);
	DefOption(int, DisplayTile, false);
	DefOption(TTileType, TileType, Tile16);
	DefOption(int, TileX, 32);
	DefOption(int, TileY, 32);
	
	#undef DefOption
	
	virtual void DrawBackground(LZoomView *View, LSurface *Dst, LPoint TileIdx, LRect *Where = NULL) = 0;
	virtual void DrawForeground(LZoomView *View, LSurface *Dst, LPoint TileIdx, LRect *Where = NULL) = 0;
	virtual void SetStatusText(const char *Msg, int Pane = 0) {}
};

#define GZOOM_SELECTION_MS          100

/// Zoomable and scrollable bitmap viewer.
class LZoomView : public LLayout, public ResObject
{
    class LZoomViewPriv *d;

public:
	struct ViewportInfo
	{
		/// The current zoom level
		///
		/// < 0 then the image is scaled down by (1 - zoom) times
		/// == 0 then the image is 1:1
		/// > 0 then the image is scale up by (zoom + 1) times
		int Zoom;
		/// The current scroll offsets in document coordinates (not screen space)
		int Sx, Sy;
		/// Tile size
		int TilePx;
	};
	
	enum SampleMode
	{
		SampleNearest,
		SampleAverage,
		SampleMax,
	};
	
	enum DefaultZoomMode
	{
		ZoomFitBothAxis,
		ZoomFitX,
		ZoomFitY,
		/* Not implemented yet...
		ZoomFitLongest,
		ZoomFitShortest,
		*/
	};

	// Notifications
	LZoomView(LZoomViewCallback *App);
	~LZoomView();

	// Methods
	int GetZoom();
	void SetZoom(int z);
	void SetCallback(LZoomViewCallback *cb);
	bool Convert(LPointF &p, int x, int y);
	ViewportInfo GetViewport();
	void SetViewport(ViewportInfo i);
	void SetSampleMode(SampleMode sm);
	void SetDefaultZoomMode(DefaultZoomMode m);
	DefaultZoomMode GetDefaultZoomMode();
	void ScrollToPoint(LPoint DocCoord);

	// Coordinate conversion
	LPoint DocToScreen(LPoint p);
	LPoint ScreenToDoc(LPoint p);
	LRect DocToScreen(LRect s);
	LRect ScreenToDoc(LRect s);

	// Subclass
	void SetSurface(LSurface *dc, bool own);
	LSurface *GetSurface();
	void Update(LRect *Where = NULL);
	void Reset();
	int GetBlockSize();

	// Events and Impl
	void OnMouseClick(LMouse &m) override;
	bool OnMouseWheel(double Lines) override;
	void OnPulse() override;
	void OnPaint(LSurface *pDC) override;
	int OnNotify(LViewI *v, const LNotification &n) override;
	LMessage::Param OnEvent(LMessage *m) override;
	bool OnLayout(LViewLayoutInfo &Inf) override;
	void UpdateScrollBars(LPoint *MaxScroll = NULL, bool ResetPos = false);
	void OnPosChange() override;
};

#endif
