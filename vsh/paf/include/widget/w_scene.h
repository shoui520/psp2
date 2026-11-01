/*
 * paf::ui::Scene, as far as converted C++ uses it: the root widget of a page,
 * with the camera it is seen through and the screen resolutions it lays out for.
 */
#ifndef PAF_WIDGET_W_SCENE_H
#define PAF_WIDGET_W_SCENE_H

#include <stdint.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>
#include <widget/core/environment.h>

namespace paf {
namespace ui {

class Camera;

/* one screen resolution a scene lays out for */
class SceneResolution {
public:
	uint32_t m_id;
	uint32_t m_base_id;
	/* the resource variants of m_id and of m_base_id */
	ResolutionResource m_resource;
	ResolutionResource m_base_resource;
	uint8_t reserved[0x10];
	int32_t m_width;
	uint8_t reserved2[4];
};

class SceneResolutionList {
public:
	SceneResolution *m_data;
	uint32_t m_num;
};

class Scene : public Widget {
public:
	/* the screen margins, turned with the camera's roll */
	math::v4 GetRolledMargin();

	/* whether the camera's roll turns the margins */
	int32_t m_use_camera;
	Camera *m_camera;
	uint8_t reserved[0xd0];
	/* the resolutions it lays out for */
	EnvironmentParam *m_env_param;
};

}
}

/* the resolution of the list with the ID, or NULL */
extern "C" paf::ui::SceneResolution *ScePafWidget_CDC90BA2(const paf::ui::EnvironmentParam *param,
							   uint32_t id);

#endif
