/*
 * paf::ui::Group, as far as converted C++ uses it: a widget that holds others. Names after the
 * exports.
 */
#ifndef PAF_WIDGET_W_GROUP_H
#define PAF_WIDGET_W_GROUP_H

#include <widget/core/widget.h>

namespace paf {
namespace ui {

class Group : public Widget {
public:
	static const char *TypeName();
};

}
}

#endif
