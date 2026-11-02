/*
 * The text system's helpers (the file's name ours): the rectangle two rectangles share, and the
 * hash a string's glyph surface is kept under in the text surface cache.
 */
#include <stdint.h>
#include <graphics/text/text_graphics.h>
#include <graphics/text/text_surface_cache.h>
#include <math/ngp/v4.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

int32_t IntersectRect(const math::v4 &a, const math::v4 &b, math::v4 &out)
{
	float left = b.extract_x();
	float right = b.extract_x() + b.extract_z();
	float top = b.extract_y();
	float bottom = b.extract_y() + b.extract_w();
	float width = 0.0f;
	float height = 0.0f;

	if (a.extract_x() > left)
		left = a.extract_x();
	if (a.extract_x() + a.extract_z() < right)
		right = a.extract_x() + a.extract_z();
	if (a.extract_y() > top)
		top = a.extract_y();
	if (a.extract_y() + a.extract_w() < bottom)
		bottom = a.extract_y() + a.extract_w();
	if (left < right)
		width = right - left;
	if (top < bottom)
		height = bottom - top;
	out = math::v4(left, top, width, height);
	return SCE_PAF_OK;
}

/* every step-th character from the last, the step one more than an eighth of the length */
uint32_t HashString(const wchar_t *str, uint32_t len)
{
	uint32_t hash = len;
	uint32_t step = (len >> 3) + 1;

	for (int32_t i = len - 1; i >= 0; i -= step)
		hash ^= (hash << 5) + (hash >> 2) + str[i];
	return hash;
}

}
}
