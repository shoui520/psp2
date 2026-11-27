/*
 * paf::ui::ContextMenuInfo implementation for 2014 PAF WebViewBase.
 */
#include <widget/web_context_menu_info.h>

namespace paf {
namespace ui {

ContextMenuInfo::ContextMenuInfo(const math::rect &bounds, const math::v2 &point)
	: m_rect(bounds), m_point(point)
{
}

ContextMenuInfo::~ContextMenuInfo()
{
}

int32_t ContextMenuInfo::AddItem(int32_t kind, const string &url)
{
	if (url.length() == 0)
		return (int32_t)0x80AF008F;

	string empty_text("");
	ContextMenuItem item(kind, empty_text, url);

	m_items.push_back(item);
	return 0;
}

}
}
