/*
 * The colours of paf's debug views, which every file that includes this header has its copy
 * of; the release build never reads them (names ours).
 */
#ifndef PAF_GRAPHICS_COLOR_H
#define PAF_GRAPHICS_COLOR_H

#include <math/ngp/v4.h>

namespace paf {

static inline __attribute__((always_inline)) math::v4 Color(float r, float g, float b, float a)
{
	return math::v4(r, g, b, a);
}

static const math::v4 s_color_gray = Color(0.5f, 0.5f, 0.5f, 1.0f);
static const math::v4 s_color_gray90 = Color(0.9f, 0.9f, 0.9f, 1.0f);
static const math::v4 s_color_gray80 = Color(0.8f, 0.8f, 0.8f, 1.0f);
static const math::v4 s_color_gray70 = Color(0.7f, 0.7f, 0.7f, 1.0f);
static const math::v4 s_color_gray60 = Color(0.6f, 0.6f, 0.6f, 1.0f);
static const math::v4 s_color_gray50 = Color(0.5f, 0.5f, 0.5f, 1.0f);
static const math::v4 s_color_gray40 = Color(0.4f, 0.4f, 0.4f, 1.0f);
static const math::v4 s_color_gray30 = Color(0.3f, 0.3f, 0.3f, 1.0f);
static const math::v4 s_color_gray20 = Color(0.2f, 0.2f, 0.2f, 1.0f);
static const math::v4 s_color_gray10 = Color(0.1f, 0.1f, 0.1f, 1.0f);
static const math::v4 s_color_white = Color(1.0f, 1.0f, 1.0f, 1.0f);
static const math::v4 s_color_black = Color(0.0f, 0.0f, 0.0f, 1.0f);
static const math::v4 s_color_red = Color(1.0f, 0.0f, 0.0f, 1.0f);
static const math::v4 s_color_dark_red = Color(0.5f, 0.0f, 0.0f, 1.0f);
static const math::v4 s_color_green = Color(0.0f, 1.0f, 0.0f, 1.0f);
static const math::v4 s_color_dark_green = Color(0.0f, 0.5f, 0.0f, 1.0f);
static const math::v4 s_color_blue = Color(0.0f, 0.0f, 1.0f, 1.0f);
static const math::v4 s_color_dark_blue = Color(0.0f, 0.0f, 0.5f, 1.0f);
static const math::v4 s_color_yellow = Color(1.0f, 1.0f, 0.0f, 1.0f);
static const math::v4 s_color_dark_yellow = Color(0.5f, 0.5f, 0.0f, 1.0f);
static const math::v4 s_color_light_yellow = Color(0.75f, 0.75f, 0.0f, 1.0f);
static const math::v4 s_color_magenta = Color(1.0f, 0.0f, 1.0f, 1.0f);
static const math::v4 s_color_dark_magenta = Color(0.5f, 0.0f, 0.5f, 1.0f);
static const math::v4 s_color_cyan = Color(0.0f, 1.0f, 1.0f, 1.0f);
static const math::v4 s_color_dark_cyan = Color(0.0f, 0.5f, 0.5f, 1.0f);
static const math::v4 s_color_orange = Color(1.0f, 0.5f, 0.0f, 1.0f);
static const math::v4 s_color_brown = Color(0.5f, 0.25f, 0.0f, 1.0f);

}

#endif
