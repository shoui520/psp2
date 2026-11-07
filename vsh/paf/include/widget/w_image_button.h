/*
 * paf::ui::ImageButton, as far as converted C++ uses it: a Button with an image. Names after
 * the exports.
 */
#ifndef PAF_WIDGET_W_IMAGE_BUTTON_H
#define PAF_WIDGET_W_IMAGE_BUTTON_H

#include <stdint.h>
#include <widget/w_button.h>

namespace paf {
namespace ui {

class ImageButton : public Button {
public:
	static const char *TypeName();

	ImageButton(Widget *parent, CreateParam *param);

	/* its own members (0x2C0 to 0x360), not converted yet */
	uint8_t reserved[0x360 - 0x2C0];
};

}
}

#endif
