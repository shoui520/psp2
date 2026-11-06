/*
 * paf::ui::ScrollViewBase, as far as converted C++ uses it: the base of the widgets that scroll
 * their contents.
 */
#ifndef PAF_WIDGET_W_SCROLL_VIEW_BASE_H
#define PAF_WIDGET_W_SCROLL_VIEW_BASE_H

#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ScrollViewBase : public Widget {
public:
	static const char *TypeName();
};

}
}

#endif
