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
	TextStyleAttribute_ForceBold = 15,
	/* the point and the boldness as drawn, after the system's text style (names ours, as
	   TextLayoutAttribute_DrawLinePitch) */
	TextStyleAttribute_DrawPoint = 16,
	TextStyleAttribute_DrawBold = 17
};

class TextStyle {
public:
	/* m_flag's bits, one per bool attribute from Bold on (names ours) */
	enum {
		Flag_Bold = 0x001,
		Flag_Oblique = 0x002,
		Flag_Underline = 0x004,
		Flag_Strike = 0x008,
		Flag_Mask = 0x010,
		Flag_Shadow = 0x020,
		Flag_Emboss = 0x040,
		Flag_EnlargeFontSize = 0x080,
		Flag_ForceBold = 0x100
	};

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
