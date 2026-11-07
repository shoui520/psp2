/*
 * paf::ui::ScrollBar and ScrollBarTouch, as far as converted C++ uses them: a scroll bar, and
 * the one moved by touch. Names after the exports.
 */
#ifndef PAF_WIDGET_W_SCROLL_BAR_H
#define PAF_WIDGET_W_SCROLL_BAR_H

#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ScrollBar : public Widget {
public:
	ScrollBar(Widget *parent, CreateParam *param);
};

class ScrollBarTouch : public ScrollBar {
public:
	static const char *TypeName();

	ScrollBarTouch(Widget *parent, CreateParam *param);
};

}
}

#endif
