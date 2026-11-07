/*
 * paf::ui::RichText, as far as converted C++ uses it: a scroll view that lays out marked-up
 * text with images and links. Names after the exports; the members' names are ours, from
 * libcdlg_npeula's use.
 */
#ifndef PAF_WIDGET_W_RICH_TEXT_H
#define PAF_WIDGET_W_RICH_TEXT_H

#include <stddef.h>
#include <stdint.h>
#include <math/ngp/v4.h>
#include <std/string>
#include <widget/w_scroll_view_base.h>

namespace paf {
namespace ui {

class RichText : public ScrollViewBase {
public:
	static const char *TypeName();

	RichText(Widget *parent, CreateParam *param);

	/* its text from the UTF-8 marked-up string of the length */
	int32_t SetCText(const char *text, size_t len);
	/* the size of the index-th image of its text, and the size it is shown at */
	int32_t GetHtmlImageSize(int32_t index, math::v4 &size);
	int32_t SetImageSize(int32_t index, const math::v4 &size);
	/* the index-th item's type, link URL and image URL */
	int32_t GetItemType(int32_t index) const;
	int32_t GetItemUrl(int32_t index, string &url) const;
	int32_t GetImageUrl(int32_t index, string &url) const;

	uint8_t reserved0[0x2D4 - 0x2B0];
	/* how far its contents scroll (npeula negates it into a position) */
	float m_scroll_extent;
	uint8_t reserved1[0x540 - 0x2D8];
	/* the margins around its text, made 0 by its constructor (npeula sets them) */
	float m_margin_x;
	float m_margin_y;
	float m_margin_width;
};

}
}

#endif
