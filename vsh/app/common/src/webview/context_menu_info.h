/*
 * What a long press on a web view hit: the hit element's rectangle and the point pressed, and
 * the element's link and image, an item each, which the web view hands to its handler. The
 * email app links the same code. No binary names this file, the classes or their methods;
 * the names are ours.
 */
#ifndef WEBVIEW_CONTEXT_MENU_INFO_H
#define WEBVIEW_CONTEXT_MENU_INFO_H

#include <stddef.h>
#include <scetypes.h>
#include <math/rect.h>
#include <std/string>
#include <std/vector>
#include <url/url.h>

namespace paf {
namespace ui {

/* an item's kind */
#define CONTEXT_MENU_ITEM_LINK		1
#define CONTEXT_MENU_ITEM_IMAGE		2

/* an image item's format, by its MIME type */
#define CONTEXT_MENU_IMAGE_NONE		0
#define CONTEXT_MENU_IMAGE_BMP		1
#define CONTEXT_MENU_IMAGE_GIF		2
#define CONTEXT_MENU_IMAGE_PNG		3
#define CONTEXT_MENU_IMAGE_JPEG		4

/* AddItem without a URL (no Sony header names it) */
#define CONTEXT_MENU_ERROR_NO_URL	((int)0x80AF008F)

class ContextMenuItem {
public:
	ContextMenuItem(int type, const string &text, const string &url, int size, int format)
		: m_type(type), m_text(text), m_url(url), m_size(size), m_format(format)
	{
	}

	virtual ~ContextMenuItem()
	{
	}

	int m_type;		/* CONTEXT_MENU_ITEM_* */
	string m_text;
	Url m_url;
	int m_size;		/* an image's size, from the hit result; 0 for a link */
	int m_format;		/* CONTEXT_MENU_IMAGE_* */
};

}
}

PAF_VECTOR_INLINE_DTOR(paf::ui::ContextMenuItem)
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::ui::ContextMenuItem, paf::allocator<paf::ui::ContextMenuItem>)

namespace paf {
namespace ui {

class ContextMenuInfo {
public:
	ContextMenuInfo(const math::rect &rect, const math::v2 &point);
	virtual ~ContextMenuInfo();

	size_t GetItemCount() const;
	math::rect GetRect() const;
	math::v2 GetPoint() const;
	/* each 0 (an empty string, an empty URL) past the last item */
	int GetItemType(size_t index) const;
	string GetItemText(size_t index) const;
	Url GetItemUrl(size_t index) const;
	int GetItemSize(size_t index) const;
	int GetItemFormat(size_t index) const;
	int AddItem(int type, const string &url, int size, int format);

	/* at 8, as a rectangle of two v2s sits (paf's rect here keeps four floats) */
	math::rect m_rect __attribute__((aligned(8)));
	math::v2 m_point;
	vector<ContextMenuItem> m_items;
};

}
}

#endif
