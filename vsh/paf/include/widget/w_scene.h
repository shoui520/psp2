/*
 * paf::ui::Scene, as far as converted C++ uses it: the root widget of a page,
 * with the camera it is seen through and the screen resolutions it lays out for.
 */
#ifndef PAF_WIDGET_W_SCENE_H
#define PAF_WIDGET_W_SCENE_H

#include <stdint.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class Camera;

/* one screen resolution a scene lays out for */
class SceneResolution {
public:
	uint32_t m_id;
	uint32_t m_base_id;
	uint8_t reserved[0x30];
	int32_t m_width;
	uint8_t reserved2[4];
};

class SceneResolutionList {
public:
	SceneResolution *m_data;
	uint32_t m_num;
};

/* the resolutions a scene lays out for */
class SceneResolutionSet {
public:
	uint32_t reserved;
	SceneResolutionList *m_list;
};

class Scene : public Widget {
public:
	/* the screen margins, turned with the camera's roll */
	math::v4 GetRolledMargin();

	/* whether the camera's roll turns the margins */
	int32_t m_use_camera;
	Camera *m_camera;
	uint8_t reserved[0xd0];
	SceneResolutionSet *m_resolution_set;
};

}
}

/* the resolution of the list with the ID, or NULL */
extern "C" paf::ui::SceneResolution *ScePafWidget_CDC90BA2(paf::ui::SceneResolutionSet *set,
							   uint32_t id);

#endif
