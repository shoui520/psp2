/*
 * The Hangul and Chinese fonts' glyph metrics, fixed fractions of the em in 26.6 fixed point at
 * a point size and resolution, in place of what the fonts give (the file's and the functions'
 * names ours).
 */
#ifndef PAF_GRAPHICS_TEXT_GLYPH_METRICS_H
#define PAF_GRAPHICS_TEXT_GLYPH_METRICS_H

#include <stdint.h>
#include <font/libpvf.h>

namespace paf {
namespace graph {

int32_t ChineseWidth64(float point, float resolution);
int32_t ChineseBearingX64(float point, float resolution);
int32_t ChineseAdvance64(int32_t c, float point, float resolution);
int32_t HangulWidth64(float point, float resolution);
int32_t HangulBearingX64(float point, float resolution);
int32_t HangulAdvance64(int32_t c, float point, float resolution);

}
}

#endif
