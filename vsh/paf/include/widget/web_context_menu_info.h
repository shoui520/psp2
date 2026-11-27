/*
 * paf::ui::ContextMenuInfo and ContextMenuItem for 2014 PAF WebViewBase.
 */
#ifndef PAF_WIDGET_WEB_CONTEXT_MENU_INFO_H
#define PAF_WIDGET_WEB_CONTEXT_MENU_INFO_H

#include <stddef.h>
#include <stdint.h>
#include <math/rect.h>
#include <math/ngp/v2.h>
#include <std/string>
#include <std/vector>
#include <url/url.h>

namespace paf {
namespace ui {

class ContextMenuItem {
public:
	ContextMenuItem(int32_t kind, const string &text, const string &url)
		: m_kind(kind), m_text(text), m_url(url)
	{
	}

	virtual ~ContextMenuItem()
	{
	}

	int32_t m_kind;
	string m_text;
	Url m_url;
};

}
}

PAF_VECTOR_INLINE_DTOR(paf::ui::ContextMenuItem)
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::ui::ContextMenuItem, paf::allocator<paf::ui::ContextMenuItem>)

namespace paf {
namespace ui {

class ContextMenuInfo {
public:
	ContextMenuInfo(const math::rect &bounds, const math::v2 &point);
	virtual ~ContextMenuInfo();

	int32_t AddItem(int32_t kind, const string &url);

	math::rect m_rect __attribute__((aligned(8)));
	math::v2 m_point;
	vector<ContextMenuItem> m_items;
};

}
}

#endif
