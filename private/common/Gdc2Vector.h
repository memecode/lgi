#ifndef __GDC2_VECTOR_H__
#define __GDC2_VECTOR_H__

#include "lgi/common/Gdc2.h"

/// Internal backend interface for a scoped LSurface::Vector session.
///
/// Implementations acquire and save the target's graphics state when created,
/// then release resources and restore that state when destroyed.
class LSurfaceVectorContext
{
public:
	virtual ~LSurfaceVectorContext() {}

	virtual bool IsValid() const = 0;
	virtual void MoveTo(float x, float y) = 0;
	virtual void LineTo(float x, float y) = 0;
	virtual void CubicTo(float x1, float y1, float x2, float y2, float x, float y) = 0;
	virtual void ArcTo(float rx, float ry, float rotation, bool largeArc, bool sweep, float x, float y) = 0;
	virtual void Rect(float x, float y, float width, float height) = 0;
	virtual void ClosePath() = 0;
	virtual void ClearPath() = 0;
	virtual void Fill(LColour colour) = 0;
	virtual void Stroke(LColour colour, float width) = 0;
	virtual bool ClipRect(LRect rect) = 0;
};

#endif
