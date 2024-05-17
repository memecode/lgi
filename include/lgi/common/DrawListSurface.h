#ifndef _GDRAW_LIST_SURFACE_H_
#define _GDRAW_LIST_SURFACE_H_

class LDrawListSurface : public LSurface
{
	struct LDrawListSurfacePriv *d;

public:
	LDrawListSurface(int Width, int Height, LColourSpace Cs = CsRgba32);
	LDrawListSurface(LSurface *FromSurface);
	~LDrawListSurface();

	// Calls specific to this class:
	ssize_t Length();
	bool OnPaint(LSurface *Dest);
	LFont *GetFont();
	void SetFont(LFont *Font);
	LColour Background();
	LColour Background(LColour c);
	LDisplayString *Text(int x, int y, const char *Str, int Len = -1);
	
	// Calls that are stored and played back:
	LRect ClipRgn() override;
	LRect ClipRgn(LRect *Rgn) override;
	COLOUR Colour() override;
	COLOUR Colour(COLOUR c, int Bits = 0) override;
	LColour Colour(LColour c) override;
	int Op() override { return GDC_SET; }
	int Op(int Op, NativeInt Param = -1) override { return GDC_SET; }
	int X() override;
	int Y() override;
	ssize_t GetRowStep() override;
	LPoint GetDpi() override;
	int GetBits() override;
	uchar *operator[](int y) override { return NULL; }
	LPoint GetOrigin() override { return LPoint(OriginX, OriginY); }
	void SetOrigin(LPoint pt) override;
	void Set(int x, int y) override;
	COLOUR Get(int x, int y) override { return 0; }

	// Primitives
	void HLine(int x1, int x2, int y) override;
	void VLine(int x, int y1, int y2) override;
	void Line(int x1, int y1, int x2, int y2) override;
	uint LineStyle(uint32_t Bits, uint32_t Reset = 0x80000000) override;
	void Circle(double cx, double cy, double radius) override;
	void FilledCircle(double cx, double cy, double radius) override;
	void Arc(double cx, double cy, double radius, double start, double end) override;
	void FilledArc(double cx, double cy, double radius, double start, double end) override;
	void Ellipse(double cx, double cy, double x, double y) override;
	void FilledEllipse(double cx, double cy, double x, double y) override;
	void Box(int x1, int y1, int x2, int y2) override;
	void Box(LRect *a = NULL) override;
	void Rectangle(int x1, int y1, int x2, int y2) override;
	void Rectangle(LRect *a = NULL) override;
	void Blt(int x, int y, LSurface *Src, LRect *a = NULL) override;
	void StretchBlt(LRect *d, LSurface *Src, LRect *s) override;
	void Polygon(int Points, LPoint *Data) override;
	void Bezier(int Threshold, LPoint *Pt) override;
	void FloodFill(int x, int y, int Mode, COLOUR Border = 0, LRect *Bounds = NULL) override;	


	// Stubs that don't work here..
	bool HasAlpha() override { return false; }
	bool HasAlpha(bool b) override { return false; }
	bool Applicator(LApplicator *pApp) override { return false; }
	LApplicator *Applicator() override { return NULL; }
	LPalette *Palette() override { return NULL; }
	void Palette(LPalette *pPal, bool bOwnIt = true) override { }
};

#endif
