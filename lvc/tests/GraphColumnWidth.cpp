#include "../src/Lvc.h"
#include "lgi/common/Css.h"

#include <cstdio>

#define CHECK(condition) \
	do { if (!(condition)) { \
		std::fprintf(stderr, "Line %d: %s\n", __LINE__, #condition); \
		return 1; \
	} } while (false)

int main(int argc, const char **argv)
{
	OsAppArguments args(argc, argv);
	LApp app(args, "LvcGraphColumnWidthTest");
	CHECK(app.IsOk());

	LList list(1);
	int limit = LCss::Len(LCss::LenEm, 15).ToPx(list.X(), list.GetFont());
	CHECK(limit > 26);
	CHECK(VcFolder::GraphColumnWidth(1, &list) == 26);

	int lastFittingLane = (limit - 12) / 14;
	CHECK(VcFolder::GraphColumnWidth(lastFittingLane, &list) == lastFittingLane * 14 + 12);
	CHECK(VcFolder::GraphColumnWidth(lastFittingLane + 1, &list) == limit);
	CHECK(VcFolder::GraphColumnWidth(1000, &list) == limit);
	return 0;
}
