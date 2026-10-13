#include "lgi/common/Lgi.h"
#include "lgi/common/Path.h"
#include "UnitTests.h"

bool LPathTest::Run()
{
	const int steps = 24;
	LPointF points[steps];
	LPointF *out = points;
	LPointF start(10, 20), control1(4.477152502, 20);
	LPointF control2(0, 15.522847498), end(0, 10);
	FlattenCubic(out, start, control1, control2, end, steps);
	if (points[0].x != start.x || points[0].y != start.y ||
		points[steps - 1].x != end.x || points[steps - 1].y != end.y)
		return FAIL(_FL, "Cubic flattening must preserve exact endpoints.");

	LMemDC dc(_FL);
	if (!dc.Create(20, 20, CsBgra32))
		return FAIL(_FL, "Could not create corner bitmap.");
	dc.Colour(0, 32);
	dc.Rectangle();

	LPointF centre(10, 10);
	LPointF corners[] = {{0, 10}, {10, 0}, {20, 10}, {10, 20}};
	for (int i = 0; i < 4; i++)
	{
		int next = (i + 1) % 4;
		LRect clip((int)MIN(corners[i].x, corners[next].x),
			(int)MIN(corners[i].y, corners[next].y),
			(int)MAX(corners[i].x, corners[next].x) - 1,
			(int)MAX(corners[i].y, corners[next].y) - 1);
		dc.ClipRgn(&clip);

		LBlendStop stops[] = {
			{0, Rgba32(148, 222, 255, 255)},
			{1, Rgba32(148, 222, 255, 255)}
		};
		LLinearBlendBrush border(corners[i], corners[next], 2, stops);
		LPath path;
		path.Circle(centre, 10);
		path.Fill(&dc, border);
		path.Empty();
		path.Ellipse(centre, 9, 9);
		LSolidBrush background(232, 248, 255);
		path.Fill(&dc, background);
	}
	dc.ClipRgn(nullptr);

	for (int y = 0; y < dc.Y(); y++)
	{
		auto pixels = reinterpret_cast<const uint32_t*>(dc[y]);
		for (int x = 0; x < dc.X() / 2; x++)
		{
			auto left = pixels[x];
			auto right = pixels[dc.X() - 1 - x];
			if (A32(left) != A32(right) ||
				abs((int)R32(left) - (int)R32(right)) > 1 ||
				abs((int)G32(left) - (int)G32(right)) > 1 ||
				abs((int)B32(left) - (int)B32(right)) > 1)
				return FAIL(_FL, "Rounded borders must have mirrored left/right coverage.");
		}
	}
	return true;
}
