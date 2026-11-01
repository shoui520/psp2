/*
 * paf::ui::Camera, as far as converted C++ uses it: what a scene is seen
 * through, at a distance from the screen plane and turned by a roll.
 */
#ifndef PAF_WIDGET_W_CAMERA_H
#define PAF_WIDGET_W_CAMERA_H

#include <stdint.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class Camera : public Widget {
public:
	float GetRollScreen(int32_t opt);

	uint8_t reserved[0x18];
	/* how far the camera is from the screen plane */
	float m_distance;
	uint8_t reserved2[0xf8];
};

}
}

/* recomputes the camera's projection from its distance and screen */
extern "C" void ScePafWidget_72F479CA(paf::ui::Camera *camera);

#endif
