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
#include <graphics/drawobj/draw_info.h>
#include <std/list>

namespace paf {
namespace graph {
class DrawLayer;
}
}

namespace paf {
namespace ui {

class Camera;

/* one screen resolution a scene lays out for */
class SceneResolution {
public:
	SceneResolution()
	{
	}

	uint32_t m_id;
	uint32_t m_base_id;
	/* the resource variants of m_id and of m_base_id */
	ResolutionResource m_resource;
	ResolutionResource m_base_resource;
	/* the sizes of the scene's screen viewport and scissor (Screen's vp and sp; names
	   ours) */
	int32_t m_viewport_width;
	int32_t m_viewport_height;
	int32_t m_scissor_width;
	int32_t m_scissor_height;
	int32_t m_width;
	int32_t m_height;
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

	/* the events it sends itself (names after ScePaf-RE) */
	enum {
		CB_WIDGET = 0x10000000,
		CB_SCENE_ONCANCEL = 0x10000001,
		CB_SCENE_CONTEXTMENU = 0x10000002,
		CB_SCENE_ACTIVATE = 0x10000003,
		CB_SCENE_INACTIVATE = 0x10000004,
		CB_SCENE_SYSTEM = 0x10000005,
		CB_SCENE_END = 0x10000006
	};

	/* called with the widget the debug mode focuses */
	typedef int32_t (*DebugFocusCB)(Widget *focus, Widget *prev, void *userdata);

	/* a widget drawn into a surface, and where the camera's draw walk reached it (names
	   ours) */
	class SurfaceWidget {
	public:
		graph::DrawInfo m_info;
		Widget *m_widget;
	};

	/* what m_draw_func is called with: its data and the scene's alpha (names ours) */
	class DrawArg {
	public:
		void *m_data;
		float m_alpha;
	};

	typedef void (*DrawFunc)(const DrawArg &arg);

	/* a page in the environment, drawn at the priority (export 0xECB1A10D) */
	Scene(Environment *env, int32_t priority, CreateParam *param, Resolution *env_param,
	      int32_t graphics_flag);
	virtual ~Scene();

	virtual int32_t HandleKeycodeEvent(Event *e);

	virtual int32_t HandleFocusSwitchEvent(Event *e)
	{
		return 0;
	}

	virtual int32_t HandleSystemEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();

	virtual int32_t SetScene_core(Scene *scene)
	{
		return Widget::SetScene_core(this);
	}

	virtual void UpdateAdjustSize();

	/* every direction, or the first while its focus directions are off */
	virtual int32_t GetFocusDirectionMask()
	{
		return m_focus_direction_off ? 1 : 7;
	}

	static const char m_widget_type[];
	static const char *TypeName();

	void SetDebugFocus(Widget *w);
	void SetDebugMode(uint32_t mode);
	bool IsResolutionActRoll(int32_t id);
	bool IsResolutionActMorph(int32_t id);
	int32_t ResetCameraRoll(float time);
	int32_t DrawScene(bool hide);
	Camera *SetCamera(Camera *camera);

	/* added to the environment's scenes of the entry, with a screen, and laid out for the
	   environment's resolution (0x81113c22; name ours, after ScePaf-RE's entry) */
	int32_t SetEntry(int32_t entry);
	/* the screen margins, turned with the camera's roll */
	math::v4 GetRolledMargin();
	/* the debug cube moved onto the debug focus (0x81114436; name ours) */
	void UpdateDebugCube();
	/* whether a resolution of the orientation exists, and the layout changed to it over the
	   time (0x81114b62, 0x81114b82; names ours) */
	bool IsOrientationAvailable(int32_t orientation);
	int32_t ChangeOrientation(int32_t orientation, float time);
	/* whether it shows scaled by the display area scale (0x811150cc; name ours) */
	bool IsDisplayAreaScaled();
	/* the background layer, made where it has none (0x811151ee; name ours) */
	int32_t CreateBgLayer();
	/* the surface widget taken out of its list; the background layer let go with the last
	   (0x8111531e; name ours) */
	void RemoveSurfaceWidget(Widget *widget);
	/* each layer's weight made again from the layers above it (0x81115374; name ours) */
	void UpdateLayerWeight();
	/* the screen's viewport and scissor set to the resolution's size, the camera's screen
	   size taken over again (0x811153de; name ours) */
	void UpdateScreenSize(Timer *t);

	/* the screen it is drawn to (its camera's Setup and draws use it; name ours) */
	Screen *m_screen;
	Camera *m_camera;
	/* what a widget's depth in the scene is ordered by first (ButtonBase::Update; name ours) */
	int32_t m_scene_priority;
	/* the entry it was added to (name ours, after ScePaf-RE's) */
	int32_t m_entry;
	/* the ID hash of the plugin that opened the page */
	uint32_t m_plugin_id;
	/* the priorities each layer spans, which UpdateLayerWeight makes their weights of (name
	   ours) */
	uint32_t m_layer_range[16];
	/* each layer's weight of a widget's priority in its depth (Widget::GetLayerDepth; name
	   ours) */
	uint64_t m_layer_weight[16];
	/* not drawn, and not drawn while its alpha is below 0.02 (DrawScene; names ours) */
	bool m_hidden;
	bool m_hide_transparent;
	/* its camera scissors the draws to the scene's own rectangle (Camera's DrawScene; name
	   ours) */
	bool m_scissor_to_scene;
	/* m_layer_range changed since UpdateLayerWeight (name ours) */
	bool m_layer_weight_dirty;
	/* the resolutions it lays out for, and whether it is its own copy (names ours) */
	Resolution *m_env_param;
	bool m_env_param_owned;
	/* the font flags its widgets' draw objects take (name after SurfaceDrawParam's) */
	int8_t m_font_flags;
	uint8_t reserved2[2];
	/* the widgets drawn into a surface, with where the camera's walk reached them (names
	   ours) */
	list<SurfaceWidget> m_surface_widgets;
	/* its draw layers, after their names "Scene" and "SceneBg" (names ours) */
	graph::DrawLayer *m_layer;
	graph::DrawLayer *m_bg_layer;
	/* drawn in its layer between its camera's begin and end (names ours) */
	DrawFunc m_draw_func;
	DrawArg m_draw_arg;
	/* whether it shows scaled by the display area scale: 0 as the environment says, 1
	   scaled, more not (name ours) */
	uint8_t m_display_area_mode;
	uint8_t reserved2c[3];
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

/* the resolution's size, -1 without it or the environment (no name found) */
extern "C" int32_t ScePafWidget_29BCC252(paf::ui::Scene *scene, int32_t *width, int32_t *height);
/* the widgets drawn into surfaces drawn, between the camera's begin and end, into the
   background layer: -1 where it is hidden, 0 also without the layer (no name found) */
extern "C" int32_t ScePafWidget_687B21CA(paf::ui::Scene *scene, bool hide);
/* the scene layer's buffer made the size while it holds no draws: -1 without the layer or with
   draws queued (no name found) */
extern "C" int32_t ScePafWidget_4AD02636(paf::ui::Scene *scene, uint32_t size);

#endif
