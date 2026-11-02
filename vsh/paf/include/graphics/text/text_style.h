/*
 * paf::graph::TextStyle: the font, size, colors and decorations of a run of text. Class, method
 * and enum names from the mangled export names and ScePaf-RE's header (restored from Sony's
 * DWARF); the attributes after the names the text object parameters read them under
 * ("font_color", "shadow_color", ...). The color fields hold RGBA bytes, in the order of
 * SetStyleAttribute(v4)'s stores.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_STYLE_H
#define PAF_GRAPHICS_TEXT_TEXT_STYLE_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

enum TextStyleAttribute {
	TextStyleAttribute_Family = 0,
	TextStyleAttribute_Point = 1,
	TextStyleAttribute_Color = 2,
	TextStyleAttribute_ShadowColor = 3,
	TextStyleAttribute_BackColor = 4,
	TextStyleAttribute_UnderlineColor = 5,
	TextStyleAttribute_StrikeColor = 6,
	TextStyleAttribute_Bold = 7,
	TextStyleAttribute_Oblique = 8,
	TextStyleAttribute_Underline = 9,
	TextStyleAttribute_Strike = 10,
	TextStyleAttribute_Mask = 11,
	TextStyleAttribute_Shadow = 12,
	TextStyleAttribute_Emboss = 13,
	TextStyleAttribute_EnlargeFontSize = 14,
	TextStyleAttribute_ForceBold = 15
};

class TextStyle {
public:
	TextStyle();
	TextStyle(const TextStyle &style);
	virtual ~TextStyle();

	TextStyle &operator=(const TextStyle &style);
	bool operator==(const TextStyle &style);
	bool operator!=(const TextStyle &style);
	bool EqualFontStyle(const TextStyle &style);
	bool EqualFontType(const TextStyle &style);

	int32_t SetStyleAttribute(TextStyleAttribute attr, int32_t val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, float val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, const math::v2 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, const math::v4 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, bool val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, int32_t &val) const;
	int32_t GetStyleAttribute(TextStyleAttribute attr, float &val) const;
	int32_t GetStyleAttribute(TextStyleAttribute attr, math::v2 &val) const;
	int32_t GetStyleAttribute(TextStyleAttribute attr, math::v4 &val) const;
	int32_t GetStyleAttribute(TextStyleAttribute attr, bool &val) const;

	int8_t m_family;
	int16_t m_flag;
	math::v2 m_point;
	uint32_t m_color;
	uint32_t m_shadowColor;
	uint32_t m_backColor;
	uint32_t m_underlineColor;
	uint32_t m_strikeColor;
	int32_t reserved;
};

}
}

#endif
