/*
 * The Hangul and Chinese fonts' glyph metrics: fixed fractions of the em, in 26.6 fixed point, at
 * a point size and resolution (an em of 1296 units), used in place of what the fonts give.
 */
#include "glyph_metrics.h"

namespace paf {
namespace graph {

int32_t ChineseWidth64(float point, float resolution)
{
	return (int32_t)(resolution * point * 1151.0f / 1296.0f);
}

int32_t ChineseBearingX64(float point, float resolution)
{
	return 0;
}

int32_t ChineseAdvance64(int32_t c, float point, float resolution)
{
	float size = resolution * point;
	float advance = 1152.0f;

	/* ASCII is half width */
	if (c < 0x80)
		advance = 576.0f;
	return (int32_t)(size * advance / 1296.0f);
}

int32_t HangulWidth64(float point, float resolution)
{
	return (int32_t)(resolution * point * 1092.0f / 1296.0f);
}

int32_t HangulBearingX64(float point, float resolution)
{
	return 0;
}

int32_t HangulAdvance64(int32_t c, float point, float resolution)
{
	float size = resolution * point;
	float advance = 1106.0f;

	/* the Hangul filler is full width */
	if (c == 0x3164)
		advance = 1152.0f;
	return (int32_t)(size * advance / 1296.0f);
}

}
}
