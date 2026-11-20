/*
 * What a long press on a web view hit (the file's name is ours).
 */
#include "context_menu_info.h"

namespace paf {
namespace ui {

ContextMenuInfo::ContextMenuInfo(const math::rect &rect, const math::v2 &point)
	: m_rect(rect), m_point(point)
{
}

ContextMenuInfo::~ContextMenuInfo()
{
}

size_t ContextMenuInfo::GetItemCount() const
{
	return m_items.size();
}

math::rect ContextMenuInfo::GetRect() const
{
	return m_rect;
}

math::v2 ContextMenuInfo::GetPoint() const
{
	return m_point;
}

int ContextMenuInfo::GetItemType(size_t index) const
{
	int type = 0;

	if (index < m_items.size())
		type = m_items.ptr[index].m_type;
	return type;
}

string ContextMenuInfo::GetItemText(size_t index) const
{
	string text;

	if (index < m_items.size())
		text = string(m_items.ptr[index].m_text);
	return text;
}

Url ContextMenuInfo::GetItemUrl(size_t index) const
{
	Url url;

	if (index < m_items.size())
		url = Url(m_items.ptr[index].m_url);
	return url;
}

int ContextMenuInfo::GetItemSize(size_t index) const
{
	int size = 0;

	if (index < m_items.size())
		size = m_items.ptr[index].m_size;
	return size;
}

int ContextMenuInfo::GetItemFormat(size_t index) const
{
	int format = 0;

	if (index < m_items.size())
		format = m_items.ptr[index].m_format;
	return format;
}

int ContextMenuInfo::AddItem(int type, const string &url, int size, int format)
{
	if (url.length() == 0)
		return CONTEXT_MENU_ERROR_NO_URL;
	m_items.push_back(ContextMenuItem(type, string(""), url, size, format));
	return 0;
}

}
}
