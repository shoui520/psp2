/*
 * paf::ui::ButtonBase, as far as converted C++ uses it: the events a button sends. Names after
 * ScePaf-RE.
 */
#ifndef PAF_WIDGET_W_BUTTON_BASE_H
#define PAF_WIDGET_W_BUTTON_BASE_H

#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ButtonBase : public Widget {
public:
	enum {
		CB_BTN_PRESS = 0x10000003,
		CB_BTN_PRESS_OUT = 0x10000004,
		CB_BTN_PRESS_REPEAT = 0x10000005,
		CB_BTN_DECIDE = 0x10000008
	};
};

}
}

#endif
