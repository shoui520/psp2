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
	/* the debug modes (names after ScePaf-RE) */
	enum {
		DEBUG_OFF = 0,
		DEBUG_AUTH_HIDEALL = 1,
		DEBUG_AUTH_LAYOUT = 2,
		DEBUG_AUTH_PARAMVIEW = 3,
		DEBUG_AUTH_PARAMVIEW_INPUT = 4,
		DEBUG_AUTH_5 = 5,
		DEBUG_AUTH_LAYOUT_RULER = 6
	};

	/* called with the widget the debug mode focuses */
	typedef int32_t (*DebugFocusCB)(Widget *focus, Widget *prev, void *userdata);

	/* a page in the environment, drawn at the priority (export 0xECB1A10D) */
	Scene(Environment *env, int32_t priority, CreateParam *param, EnvironmentParam *env_param,
	      int32_t graphics_flag);

	static const char *TypeName();

	/* the screen margins, turned with the camera's roll */
	math::v4 GetRolledMargin();
	void SetDebugMode(uint32_t mode);

	/* whether the camera's roll turns the margins */
	int32_t m_use_camera;
	Camera *m_camera;
	uint8_t reserved[8];
	/* the ID hash of the plugin that opened the page */
	uint32_t m_plugin_id;
	uint8_t reserved1[0xc4];
	/* the resolutions it lays out for */
	EnvironmentParam *m_env_param;
	uint8_t reserved2[0x28];
	/* the debug mode and its state (names after ScePaf-RE) */
	uint32_t m_debug_mode;
	Widget *m_debug_focus;
	Widget *m_debug_cube;
	Widget *m_debug_cube_child;
	uint32_t m_debug_grab_restore;
	uint32_t m_debug_drag_restore;
	uint32_t m_debug_hitfind_restore;
	uint32_t m_debug_quant_x;
	uint32_t m_debug_quant_y;
	uint32_t m_debug_quant_z;
	uint32_t m_debug_quant_x_restore;
	uint32_t m_debug_quant_y_restore;
	uint32_t m_debug_quant_z_restore;
	float m_debug_showalpha_restore;
	DebugFocusCB m_debug_focus_func;
};

}
}

/* the ID of the resolution whose resource variant, or its base's, has the ID's hash; -1 without
   one */
extern "C" int32_t ScePafWidget_36505C86(const paf::ui::EnvironmentParam *param,
					 const paf::IDParam &id);

/* the resolution of the list with the ID, or NULL */
extern "C" paf::ui::SceneResolution *ScePafWidget_CDC90BA2(const paf::ui::EnvironmentParam *param,
							   uint32_t id);

#endif
