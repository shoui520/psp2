/*
 * paf::graph::TextStyle (the file's name ours): a run's font family, point, colors and the bool
 * attributes kept as bits of m_flag. A new style takes the system's text style: its enlarged
 * font and forced bold.
 */
#include <stddef.h>
#include <stdint.h>
#include <arm_neon.h>
#include <graphics/text/text_style.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <system/system.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

/* the system's text style bits (names ours) */
#define SYSTEM_TEXT_ENLARGE	0x1
#define SYSTEM_TEXT_BOLD	0x2

/* how much an enlarged font is drawn bigger (name ours) */
#define ENLARGED_FONT_SIZE	1.35f

/* the default colors: white text, its shadow black at 40% */
#define DEFAULT_COLOR		0xFFFFFFFF
#define DEFAULT_SHADOW_COLOR	0x66000000

/* the attributes the font is made from: EqualFontType's, and EqualFontStyle's with the point */
#define FONT_TYPE_FLAGS		(Flag_Bold | Flag_Oblique | Flag_Shadow | Flag_ForceBold)
#define FONT_STYLE_FLAGS	(FONT_TYPE_FLAGS | Flag_EnlargeFontSize)

/* the color as RGBA bytes, each saturated, and back */
static inline uint32_t PackColor(const math::v4 &color)
{
	uint16x4_t rgba = vqmovn_u32(vcvtq_u32_f32(
		vmulq_f32(color.getImpl().get128(), vdupq_n_f32(255.0f))));

	return vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(rgba, rgba))), 0);
}

static inline math::v4 UnpackColor(uint32_t color)
{
	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));

	return math::v4(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));
}

TextStyle::TextStyle()
	: m_family(0), m_flag(0), m_point(0.0f, 0.0f), m_color(DEFAULT_COLOR),
	  m_shadowColor(DEFAULT_SHADOW_COLOR), m_backColor(0), m_underlineColor(0),
	  m_strikeColor(0)
{
	if ((system::SystemParam::GetTextStyle() & SYSTEM_TEXT_ENLARGE) != 0)
		m_flag |= Flag_EnlargeFontSize;
	if ((system::SystemParam::GetTextStyle() & SYSTEM_TEXT_BOLD) != 0)
		m_flag |= Flag_ForceBold;
}

TextStyle::TextStyle(const TextStyle &style)
	: m_family(0), m_flag(0), m_point(0.0f, 0.0f), m_color(DEFAULT_COLOR),
	  m_shadowColor(DEFAULT_SHADOW_COLOR), m_backColor(0), m_underlineColor(0),
	  m_strikeColor(0)
{
	*this = style;
}

TextStyle::~TextStyle()
{
}

TextStyle &TextStyle::operator=(const TextStyle &style)
{
	m_family = style.m_family;
	m_flag = style.m_flag;
	m_point = style.m_point;
	m_color = style.m_color;
	m_shadowColor = style.m_shadowColor;
	m_backColor = style.m_backColor;
	m_underlineColor = style.m_underlineColor;
	m_strikeColor = style.m_strikeColor;
	return *this;
}

bool TextStyle::operator==(const TextStyle &style)
{
	return m_family == style.m_family && m_flag == style.m_flag &&
	       ((style.m_point.getImpl().getX() == m_point.getImpl().getX()) &
		(style.m_point.getImpl().getY() == m_point.getImpl().getY())).getAsBool() &&
	       m_color == style.m_color && m_shadowColor == style.m_shadowColor &&
	       m_backColor == style.m_backColor && m_underlineColor == style.m_underlineColor &&
	       m_strikeColor == style.m_strikeColor;
}

bool TextStyle::operator!=(const TextStyle &style)
{
	return m_family != style.m_family || m_flag != style.m_flag ||
	       !((m_point.getImpl().getX() == style.m_point.getImpl().getX()) &
		 (m_point.getImpl().getY() == style.m_point.getImpl().getY())).getAsBool() ||
	       m_color != style.m_color || m_shadowColor != style.m_shadowColor ||
	       m_backColor != style.m_backColor || m_underlineColor != style.m_underlineColor ||
	       m_strikeColor != style.m_strikeColor;
}

int32_t TextStyle::SetStyleAttribute(TextStyleAttribute attr, int32_t val)
{
	int32_t ret = SCE_PAF_OK;

	if (attr == TextStyleAttribute_Family)
		m_family = val;
	else
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	return ret;
}

int32_t TextStyle::GetStyleAttribute(TextStyleAttribute attr, int32_t &val) const
{
	int32_t ret = SCE_PAF_OK;

	if (attr == TextStyleAttribute_Family)
		val = m_family;
	else
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	return ret;
}

int32_t TextStyle::SetStyleAttribute(TextStyleAttribute attr, float val)
{
	int32_t ret = SCE_PAF_OK;

	if (attr == TextStyleAttribute_Point)
		m_point = math::v2(val, val);
	else
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	return ret;
}

int32_t TextStyle::GetStyleAttribute(TextStyleAttribute attr, float &val) const
{
	int32_t ret = SCE_PAF_OK;

	switch (attr) {
	case TextStyleAttribute_Point:
		val = m_point.extract_y();
		break;
	case TextStyleAttribute_DrawPoint: {
		math::v2 point = m_point;

		if ((ScePafMisc_C51EF81D() & 1) != 0 && (m_flag & Flag_EnlargeFontSize) != 0)
			point = point * ENLARGED_FONT_SIZE;
		val = point.extract_y();
		break;
	}
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

int32_t TextStyle::SetStyleAttribute(TextStyleAttribute attr, const math::v2 &val)
{
	int32_t ret = SCE_PAF_OK;

	if (attr == TextStyleAttribute_Point)
		m_point = val;
	else
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	return ret;
}

int32_t TextStyle::GetStyleAttribute(TextStyleAttribute attr, math::v2 &val) const
{
	int32_t ret = SCE_PAF_OK;

	switch (attr) {
	case TextStyleAttribute_Point:
		val = m_point;
		break;
	case TextStyleAttribute_DrawPoint: {
		math::v2 point = m_point;

		if ((ScePafMisc_C51EF81D() & 1) != 0 && (m_flag & Flag_EnlargeFontSize) != 0)
			point = point * ENLARGED_FONT_SIZE;
		val = point;
		break;
	}
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

int32_t TextStyle::SetStyleAttribute(TextStyleAttribute attr, const math::v4 &val)
{
	int32_t ret = SCE_PAF_OK;
	math::v4 color = val;

	switch (attr) {
	case TextStyleAttribute_Color:
		m_color = PackColor(color);
		break;
	case TextStyleAttribute_ShadowColor:
		m_shadowColor = PackColor(color);
		break;
	case TextStyleAttribute_BackColor:
		m_backColor = PackColor(color);
		break;
	case TextStyleAttribute_UnderlineColor:
		m_underlineColor = PackColor(color);
		break;
	case TextStyleAttribute_StrikeColor:
		m_strikeColor = PackColor(color);
		break;
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

/* the underline and strike colors are the text's until they are set */
int32_t TextStyle::GetStyleAttribute(TextStyleAttribute attr, math::v4 &val) const
{
	int32_t ret = SCE_PAF_OK;

	switch (attr) {
	case TextStyleAttribute_Color:
		val = UnpackColor(m_color);
		break;
	case TextStyleAttribute_ShadowColor:
		val = UnpackColor(m_shadowColor);
		break;
	case TextStyleAttribute_BackColor:
		val = UnpackColor(m_backColor);
		break;
	case TextStyleAttribute_UnderlineColor:
		if (m_underlineColor != 0)
			val = UnpackColor(m_underlineColor);
		else
			val = UnpackColor(m_color);
		break;
	case TextStyleAttribute_StrikeColor:
		if (m_strikeColor != 0)
			val = UnpackColor(m_strikeColor);
		else
			val = UnpackColor(m_color);
		break;
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

#define SET_FLAG(bit)						\
	do {							\
		if (val)					\
			m_flag |= (bit);			\
		else						\
			m_flag &= ~(bit);			\
	} while (0)

int32_t TextStyle::SetStyleAttribute(TextStyleAttribute attr, bool val)
{
	int32_t ret = SCE_PAF_OK;

	switch (attr) {
	case TextStyleAttribute_Bold:
		SET_FLAG(Flag_Bold);
		break;
	case TextStyleAttribute_Oblique:
		SET_FLAG(Flag_Oblique);
		break;
	case TextStyleAttribute_Underline:
		SET_FLAG(Flag_Underline);
		break;
	case TextStyleAttribute_Strike:
		SET_FLAG(Flag_Strike);
		break;
	case TextStyleAttribute_Mask:
		SET_FLAG(Flag_Mask);
		break;
	case TextStyleAttribute_Shadow:
		SET_FLAG(Flag_Shadow);
		break;
	case TextStyleAttribute_Emboss:
		SET_FLAG(Flag_Emboss);
		break;
	case TextStyleAttribute_EnlargeFontSize:
		SET_FLAG(Flag_EnlargeFontSize);
		break;
	case TextStyleAttribute_ForceBold:
		SET_FLAG(Flag_ForceBold);
		break;
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

int32_t TextStyle::GetStyleAttribute(TextStyleAttribute attr, bool &val) const
{
	int32_t ret = SCE_PAF_OK;

	switch (attr) {
	case TextStyleAttribute_Bold:
		val = (m_flag & Flag_Bold) ? true : false;
		break;
	case TextStyleAttribute_Oblique:
		val = (m_flag & Flag_Oblique) ? true : false;
		break;
	case TextStyleAttribute_Underline:
		val = (m_flag & Flag_Underline) ? true : false;
		break;
	case TextStyleAttribute_Strike:
		val = (m_flag & Flag_Strike) ? true : false;
		break;
	case TextStyleAttribute_Mask:
		val = (m_flag & Flag_Mask) ? true : false;
		break;
	case TextStyleAttribute_Shadow:
		val = (m_flag & Flag_Shadow) ? true : false;
		break;
	case TextStyleAttribute_Emboss:
		val = (m_flag & Flag_Emboss) ? true : false;
		break;
	case TextStyleAttribute_EnlargeFontSize:
		val = (m_flag & Flag_EnlargeFontSize) ? true : false;
		break;
	case TextStyleAttribute_ForceBold:
		val = (m_flag & Flag_ForceBold) ? true : false;
		break;
	case TextStyleAttribute_DrawBold: {
		bool bold = (m_flag & Flag_Bold) ? true : false;

		if ((ScePafMisc_C51EF81D() & 2) != 0 && (m_flag & Flag_ForceBold) != 0)
			bold = true;
		val = bold;
		break;
	}
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

bool TextStyle::EqualFontStyle(const TextStyle &style)
{
	return m_family == style.m_family && ((m_flag ^ style.m_flag) & FONT_STYLE_FLAGS) == 0 &&
	       ((style.m_point.getImpl().getX() == m_point.getImpl().getX()) &
		(style.m_point.getImpl().getY() == m_point.getImpl().getY())).getAsBool();
}

bool TextStyle::EqualFontType(const TextStyle &style)
{
	return m_family == style.m_family && ((m_flag ^ style.m_flag) & FONT_TYPE_FLAGS) == 0;
}

}
}
