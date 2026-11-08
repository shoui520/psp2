/*
 * paf::ui::Scene (TOOL 0x81112f78..0x81115668): the root widget of a page, with the camera it
 * is seen through, the screen it is drawn to, its draw layers and its debug modes. No record or
 * string names the file; the names of the debug mode's widget classes and of the static are
 * ours.
 */
#include <stddef.h>
#include <stdint.h>
/* first: Sony's file constructs its copy of the effect paths before the tile size */
#include <graphics/drawobj/effect_path.h>
#include <graphics/arl/arl_defines.h>
#include <math/ngp/v4.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/draw_env_flush.h>
#include <graphics/drawobj/plane_obj.h>
#include <std/list>
#include <std/math.h>
#include <std/string.h>
#include <system/system.h>
#include <timer/timer.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/w_camera.h>
#include <widget/w_composite_drawer.h>
#include <widget/w_plane.h>
#include <widget/w_scene.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

/* the box drawn around the debug focus in the layout modes: a plane on each of its six faces */
class DebugCube : public CompositeDrawer {
public:
	/* out of line in Sony's build */
	__attribute__((noinline)) DebugCube(Widget *parent, CreateParam *param)
		: CompositeDrawer(parent, param)
	{
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 0), false);
		}
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 1), false);
		}
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 2), false);
		}
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 3), false);
		}
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 4), false);
		}
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 5), false);
		}
	}

	virtual ~DebugCube()
	{
	}

	/* the faces put around its size, the back and the sides shown only where it is deeper
	   than 10 */
	virtual void UpdateSize()
	{
		int32_t i = 0;
		bool deep = m_size.extract_z() > 10.0f;

		for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end();
		     ++it, i++) {
			graph::DrawObj *obj = it->m_obj.get();

			switch (i) {
			case 0:
				obj->SetSize(math::v3(m_size.extract_x(), m_size.extract_y(), 0.0f));
				obj->SetPos(math::v3(0.0f, 0.0f, m_size.extract_z() * 0.5f));
				break;
			case 1:
				obj->SetSize(math::v3(m_size.extract_x(), m_size.extract_y(), 0.0f));
				obj->SetPos(math::v3(0.0f, 0.0f, m_size.extract_z() * -0.5f));
				obj->ChangeVisible(deep);
				break;
			case 2:
				obj->SetSize(math::v3(m_size.extract_z(), m_size.extract_y(), 0.0f));
				obj->SetRot(math::v3(0.0f, 1.5707964f, 0.0f));
				obj->SetPos(math::v3(m_size.extract_x() * 0.5f, 0.0f, 0.0f));
				obj->ChangeVisible(deep);
				break;
			case 3:
				obj->SetSize(math::v3(m_size.extract_z(), m_size.extract_y(), 0.0f));
				obj->SetRot(math::v3(0.0f, 1.5707964f, 0.0f));
				obj->SetPos(math::v3(m_size.extract_x() * -0.5f, 0.0f, 0.0f));
				obj->ChangeVisible(deep);
				break;
			case 4:
				obj->SetSize(math::v3(m_size.extract_x(), m_size.extract_z(), 0.0f));
				obj->SetRot(math::v3(1.5707964f, 0.0f, 0.0f));
				obj->SetPos(math::v3(0.0f, m_size.extract_y() * 0.5f, 0.0f));
				obj->ChangeVisible(deep);
				break;
			case 5:
				obj->SetSize(math::v3(m_size.extract_x(), m_size.extract_z(), 0.0f));
				obj->SetRot(math::v3(1.5707964f, 0.0f, 0.0f));
				obj->SetPos(math::v3(0.0f, m_size.extract_y() * -0.5f, 0.0f));
				obj->ChangeVisible(deep);
				break;
			}
		}
	}
};

/* the ruler of DEBUG_AUTH_LAYOUT_RULER: a line across and a line down through the debug
   focus */
class DebugRuler : public CompositeDrawer {
public:
	DebugRuler(Widget *parent, CreateParam *param) : CompositeDrawer(parent, param)
	{
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 0), false);
		}
		{
			common::SharedPtr<graph::PlaneObj> plane(
				new graph::PlaneObj(graph::PlaneObjStyle()));

			AddDrawObj(DrawObjEntry(plane, 1), false);
		}
	}

	virtual ~DebugRuler()
	{
	}

	virtual void UpdateSize()
	{
		int32_t i = 0;

		for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end();
		     ++it, i++) {
			graph::DrawObj *obj = it->m_obj.get();

			switch (i) {
			case 0:
				obj->SetSize(math::v3(2000.0f, m_size.extract_y(), 0.0f));
				obj->SetPos(math::v3(0.0f, 0.0f, 0.0f));
				break;
			case 1:
				obj->SetSize(math::v3(m_size.extract_x(), 2000.0f, 0.0f));
				obj->SetPos(math::v3(0.0f, 0.0f, 0.0f));
				break;
			}
		}
	}
};

/* the scene SetDebugFocus was last called on (name ours) */
static Scene *s_debug_scene;

const char Scene::m_widget_type[] = "page";

bool Scene::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *Scene::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Scene::TypeName()
{
	return m_widget_type;
}

/* the widget shown or hidden at once, with no transition */
static inline void ShowNow(Widget *w)
{
	if (!w->m_transition_complete) {
		w->m_transition = 0xff;
		w->m_transition_complete = true;
		w->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(w, NULL, 2, 0.0f, NULL);
}

static inline void HideNow(Widget *w)
{
	w->m_transition_complete = false;
	w->m_transition = 0xff;
	w->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(w, NULL, 2, 0.0f, NULL);
}

void Scene::SetDebugFocus(Widget *w)
{
	Widget *prev = m_debug_focus;

	s_debug_scene = this;
	if (prev == w)
		return;
	if (w != NULL) {
		if (FindChild(w, 0) == NULL)
			return;
		prev = m_debug_focus;
	}

	uint32_t mode = m_debug_mode;

	if ((mode == DEBUG_AUTH_LAYOUT || mode == DEBUG_AUTH_PARAMVIEW) && prev != NULL) {
		prev->m_hit_check = m_debug_grab_restore;
		prev->SetDragMode(m_debug_drag_restore);
		prev->m_hit_find_mode = m_debug_hitfind_restore;
		prev->SetDragQuant(m_debug_quant_x_restore, m_debug_quant_y_restore, m_debug_quant_z_restore);
		mode = m_debug_mode;
	}

	m_debug_focus = w;
	m_debug_grab_restore = w != NULL ? w->m_hit_check : 0;
	m_debug_drag_restore = w != NULL ? w->m_drag_mode : 0;
	m_debug_hitfind_restore = w != NULL ? w->m_hit_find_mode : 0;
	if (w != NULL) {
		m_debug_quant_x_restore = w->m_drag_quant_x;
		m_debug_quant_y_restore = w->m_drag_quant_y;
		m_debug_quant_z_restore = w->m_drag_quant_z;
	}

	if (mode == DEBUG_AUTH_LAYOUT || mode == DEBUG_AUTH_PARAMVIEW) {
		if (w != NULL && mode == DEBUG_AUTH_LAYOUT) {
			w->m_hit_check = 1;
			w->SetDragMode(0x110);
			w->m_hit_find_mode = 0;
			w->SetDragQuant(m_debug_quant_x, m_debug_quant_y, m_debug_quant_z);
			m_debug_cube_child->m_hit_check = 1;
		} else {
			m_debug_cube_child->m_hit_check = 0;
		}
		if (m_debug_focus_func != NULL)
			m_debug_focus_func(m_debug_focus, prev, NULL);
		if (m_debug_cube != NULL && m_debug_focus != NULL) {
			m_debug_cube->SetForward(m_debug_focus);
			m_debug_cube->KillAnim(ANIM_META_ALPHA);
			m_debug_cube->SetMetaAlpha(0.6f, new Timer(500.0f),
						   ANIM_META_ALPHA | ANIM_OPTION_LOOP, 0, 0.0f);
			m_debug_cube->SetMetaAlpha(0.3f, new Timer(500.0f),
						   ANIM_META_ALPHA | ANIM_OPTION_LOOP, 0, 0.0f);
			ShowNow(m_debug_cube_child);
			m_debug_cube_child->SetSize(math::v4(10.0f, 10.0f, 0.0f, 0.0f));
		}
	} else if (mode == DEBUG_AUTH_PARAMVIEW_INPUT) {
		if (m_debug_focus_func != NULL) {
			m_debug_focus_func(w, prev, NULL);
			w = m_debug_focus;
		}
		if (m_debug_cube != NULL && w != NULL) {
			m_debug_cube->SetForward(w);
			m_debug_cube->KillAnim(ANIM_META_ALPHA);
			m_debug_cube->SetMetaAlpha(0.6f, new Timer(500.0f),
						   ANIM_META_ALPHA | ANIM_OPTION_LOOP, 0, 0.0f);
			m_debug_cube->SetMetaAlpha(0.3f, new Timer(500.0f),
						   ANIM_META_ALPHA | ANIM_OPTION_LOOP, 0, 0.0f);
			HideNow(m_debug_cube_child);
		}
	} else if (mode == DEBUG_AUTH_LAYOUT_RULER) {
		/* the ruler follows no focus */
	} else if (mode == DEBUG_OFF || mode == DEBUG_AUTH_HIDEALL) {
		if (m_debug_focus_func != NULL)
			m_debug_focus_func(NULL, prev, NULL);
	}
}

void Scene::SetDebugMode(uint32_t mode)
{
	uint32_t old = m_debug_mode;

	if (old == mode)
		return;

	m_debug_mode = mode;
	if (mode == DEBUG_AUTH_HIDEALL) {
		m_debug_showalpha_restore = m_show_alpha;
		SetShowAlpha_core(0.0f);
	} else {
		SetShowAlpha_core(m_debug_showalpha_restore);
	}

	if (mode == DEBUG_OFF || mode == DEBUG_AUTH_HIDEALL) {
		if (m_debug_cube != NULL) {
			m_debug_cube->DestroyWidget();
			m_debug_cube = NULL;
			m_debug_cube_child = NULL;
		}
		if (m_debug_focus != NULL && old != DEBUG_AUTH_PARAMVIEW_INPUT) {
			m_debug_focus->m_hit_check = m_debug_grab_restore;
			m_debug_focus->SetDragMode(m_debug_drag_restore);
			m_debug_focus->m_hit_find_mode = m_debug_hitfind_restore;
			m_debug_focus->SetDragQuant(m_debug_quant_x_restore, m_debug_quant_y_restore, m_debug_quant_z_restore);
		}
	} else if (mode == DEBUG_AUTH_LAYOUT || mode == DEBUG_AUTH_PARAMVIEW ||
		   mode == DEBUG_AUTH_PARAMVIEW_INPUT || mode == DEBUG_AUTH_LAYOUT_RULER) {
		CompositeDrawer *cube;

		if (mode == DEBUG_AUTH_LAYOUT_RULER) {
			cube = new DebugRuler(this, NULL);
			cube->SetTexture(m_env->m_debug_texture, 0, 0);
			cube->SetTexture(m_env->m_debug_texture, 1, 0);
		} else {
			cube = new DebugCube(this, NULL);
			cube->SetTexture(m_env->m_debug_texture, 0, 0);
			cube->SetTexture(m_env->m_debug_texture, 1, 0);
			cube->SetTexture(m_env->m_debug_texture, 2, 0);
			cube->SetTexture(m_env->m_debug_texture, 3, 0);
			cube->SetTexture(m_env->m_debug_texture, 4, 0);
			cube->SetTexture(m_env->m_debug_texture, 5, 0);
		}

		m_debug_cube = cube;
		if (m_debug_focus != NULL)
			cube->SetForward(m_debug_focus);
		m_debug_cube->m_event_enable |= 0x60;
		m_debug_cube->m_debug_events = false;
		HideNow(m_debug_cube);
		m_debug_cube->m_hit_find_mode = 0;
		m_debug_cube->m_hit_detect_mode = 2;
		m_debug_cube->m_hit_detect_b_mode = 2;

		Widget *child = new Plane(this, NULL);

		m_debug_cube_child = child;
		child->m_event_enable |= 0x60;
		m_debug_cube_child->m_debug_events = false;
		m_debug_cube_child->SetSize(math::v4(10.0f, 10.0f, 0.0f, 0.0f));
		if (mode != DEBUG_AUTH_PARAMVIEW_INPUT) {
			m_debug_cube_child->m_hit_check = 1;
			m_debug_cube_child->SetDragMode(0x110);
			child = m_debug_cube_child;
			child->m_hit_detect_mode = 0;
			child->m_hit_detect_b_mode = 0;
		}
	}
	SetDebugFocus(NULL);
}

bool Scene::IsResolutionActRoll(int32_t id)
{
	return true;
}

int32_t Scene::ResetCameraRoll(float time)
{
	if (m_env == NULL)
		return -1;

	uint32_t id = m_resolution_id;

	if (m_resolution_id_next != 0xff)
		id = m_resolution_id_next;
	if (ScePafWidget_CDC90BA2(m_env_param, id) == NULL || m_camera == NULL)
		return -1;

	Timer *t = NULL;

	if (time > 0.0f)
		t = new Timer(time, Timer::FUNC_ANGULAR);

	float roll = m_env->GetOrientationRoll(m_env->m_screen_orientation_mode);

	if (m_env->m_camera_roll_mode == 0)
		m_camera->SetRollScreen(-roll, t);
	else if (m_env->m_camera_roll_mode == 1)
		m_camera->SetRollScreen(0.0f, t);
	return 0;
}

int32_t Scene::SetEntry(int32_t entry)
{
	Environment *env = m_env;

	if (env == NULL)
		return -1;
	if (m_scene_priority >= 0) {
		m_env->RemoveScene(this);
		env = m_env;
	}
	if (entry < 0)
		entry = 0;
	else if (entry > 0xffff)
		entry = 0xffff;
	m_scene_priority = entry;
	m_entry = entry;
	env->AddScene(this, entry, NULL);

	if (m_screen == NULL) {
		Screen *screen = new Screen(m_env->m_screen);

		if (m_screen != NULL && m_screen->is_auto_delete)
			delete m_screen;
		m_screen = screen;
	}

	graph::Surface *surface = m_env->m_screen->m_surface.get();
	uint32_t id = m_env_param->GetResolutionIdHD(surface->GetWidth(), surface->GetHeight(),
						     m_env->m_screen_orientation_mode);

	if (id == 0xff) {
		surface = m_env->m_screen->m_surface.get();
		id = m_env_param->GetResolutionIdHD(surface->GetWidth(), surface->GetHeight(), 0);
	}
	if ((int32_t)id < 0) {
		math::v4 size = m_size;
		math::v2 screen = math::rect(m_screen->vp).size();

		if (m_adjust_x == 4)
			size.set_x(screen.extract_x());
		if (m_adjust_y == 4)
			size.set_y(screen.extract_y());
		SetSize(size);
	} else {
		if (m_scene != this)
			m_scene = this;
		ChangeResolution_core(id, true, true);
		m_scene = NULL;
	}
	ResetCameraRoll(0.0f);
	return 0;
}

Scene::Scene(Environment *env, int32_t priority, CreateParam *param, EnvironmentParam *env_param,
	     int32_t graphics_flag)
	: Widget(NULL, param), m_screen(NULL), m_camera(NULL), m_scene_priority(-1), m_entry(-1),
	  m_plugin_id(0), m_layer_range(), m_layer_weight(), m_hidden(false),
	  m_hide_transparent(false), m_scissor_to_scene(true), m_layer_weight_dirty(false),
	  m_env_param(NULL), m_env_param_owned(false), m_font_flags(system::SystemParam::GetTextStyle()),
	  m_layer(NULL), m_bg_layer(NULL), m_draw_func(NULL), m_display_area_mode(0),
	  m_debug_mode(DEBUG_OFF), m_debug_focus(NULL), m_debug_cube(NULL),
	  m_debug_grab_restore(0), m_debug_drag_restore(0), m_debug_hitfind_restore(0),
	  m_debug_quant_x(1), m_debug_quant_y(1), m_debug_quant_z(1), m_debug_quant_x_restore(0),
	  m_debug_quant_y_restore(0), m_debug_quant_z_restore(0), m_debug_showalpha_restore(1.0f),
	  m_debug_focus_func(NULL)
{
	m_draw_arg.m_data = NULL;
	if (env != NULL)
		m_env_param = env->m_env_param;
	if (env_param != NULL && env_param != m_env_param) {
		m_env_param = env_param->Clone();
		m_env_param_owned = true;
	}
	if (graphics_flag != 0x80)
		m_font_flags = graphics_flag;

	m_layer = graph::DrawCalls()->AddLayer("Scene", 300, 0, 0x2000);
	m_bg_layer = NULL;
	m_surface_widgets.clear();
	sce_paf_memset(m_layer_range, 0, sizeof(m_layer_range));
	sce_paf_memset(m_layer_weight, 0, sizeof(m_layer_weight));

	SetAdjust(4, 4, 4);
	SetEnvironment(env);
	SetEntry(priority);
	SetScene_core(this);
	m_state = 0;
	m_event_enable |= 0xfbf0;
	ChangeState(1, -1);
	m_hit_check = 0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 2;
	m_hit_detect_b_mode = 2;
	m_debug_events = false;
	SetDebugMode(DEBUG_OFF);
	ScePafWidget_00AA6519(this);
	m_not_hit_target = true;
}

Scene::~Scene()
{
	graph::DrawCalls()->SetDrawMode(m_layer, 3);
	m_layer = NULL;
	if (m_bg_layer != NULL) {
		graph::DrawCalls()->SetDrawMode(m_bg_layer, 3);
		m_bg_layer = NULL;
	}
	if (m_screen != NULL && m_screen->is_auto_delete) {
		delete m_screen;
		m_screen = NULL;
	}
	if (m_env != NULL)
		m_env->RemoveScene(this);
	m_scene_priority = -1;
	if (m_env_param_owned && m_env_param != NULL)
		delete m_env_param;
	m_env_param = (EnvironmentParam *)-1;
}

void Scene::DestroyWidget()
{
	SetDebugMode(DEBUG_OFF);
	Widget::DestroyWidget();
}

Camera *Scene::SetCamera(Camera *camera)
{
	Camera *old = m_camera;

	if (old != NULL)
		ScePafWidget_8EDC2803(m_camera, NULL);
	m_camera = camera;
	if (camera != NULL) {
		ScePafWidget_8EDC2803(camera, m_screen);
		ResetCameraRoll(0.0f);
	}
	return old;
}

math::v4 Scene::GetRolledMargin()
{
	math::v4 margin = Screen::s_draw_offset;

	if (m_screen != NULL && m_camera != NULL) {
		float roll = m_camera->GetRollScreen(1);

		if (roll > 3.1415927f)
			roll -= 6.2831855f;
		if (roll < -3.1415927f)
			roll += 6.2831855f;

		float turn = __builtin_fabsf(roll);

		if (turn > 2.3561945f)
			return math::v4(margin.extract_z(), margin.extract_y(), margin.extract_x(),
					margin.extract_w());
		if (turn > 0.7853982f && turn <= 2.3561945f) {
			if (roll > 0.0f)
				return math::v4(margin.extract_y(), margin.extract_z(),
						margin.extract_w(), margin.extract_x());
			return math::v4(margin.extract_w(), margin.extract_x(), margin.extract_y(),
					margin.extract_z());
		}
	}
	return margin;
}

void Scene::UpdateDebugCube()
{
	Widget *cube = m_debug_cube;

	if (cube == NULL)
		return;

	Widget *focus = m_debug_focus;

	if (focus == NULL) {
		HideNow(cube);
		m_debug_cube->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		m_debug_cube_child->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		return;
	}

	if (m_debug_mode == DEBUG_AUTH_PARAMVIEW_INPUT) {
		if (focus->IsInputTarget())
			ShowNow(cube);
		else
			HideNow(cube);

		math::v4 center = focus->GetHitBoxCenter();
		const math::matrix &world = focus->GetWorldMatrix();
		math::v4 size = focus->GetHitBoxSize(0);

		m_debug_cube->SetPos(center);
		m_debug_cube->SetSize(math::v4(world.getImpl() *
					       sce::Vectormath::Simd::Aos::Vector4(
						       size.getImpl().getXYZ(), 0.0f)));
	} else if (m_debug_mode == DEBUG_AUTH_LAYOUT_RULER) {
		if (focus->IsInputTarget() && s_debug_scene == this)
			ShowNow(cube);
		else
			HideNow(cube);

		Widget *ruler = m_debug_cube;

		ruler->SetPos(math::v4(focus->GetWorldMatrix().getImpl().getCol3()));
		m_debug_cube->SetSize(*focus->GetSize(0));
		m_debug_cube->SetRot(focus->GetRot(0, this, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0), 0,
				     NULL, ANIM_ROT, 0, 0.0f, NULL, NULL);
	} else {
		math::v4 pos = focus->GetPos(0, this, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 size = *focus->GetSize(0);
		math::v4 rot = focus->GetRot(0, this, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);

		ShowNow(m_debug_cube);
		if (m_debug_cube_child->m_drag_state[0] != NULL) {
			math::v4 grip = m_debug_cube_child->GetPos(0, NULL,
								   math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			math::v4 corner(grip.extract_x(), -grip.extract_y(), grip.extract_z(),
					grip.extract_w());
			math::v4 grow = corner * 2.0f - size;

			if (!(grow == math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
				focus->SetSize_core(*m_debug_focus->GetSize(0) + grow, 0xff02, true);
				m_debug_cube->SetSize(*m_debug_focus->GetSize(0) +
						      math::v4(4.0f, 4.0f, 4.0f, 4.0f));
			}
		} else {
			if (!(m_debug_cube->GetRot(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0) ==
			      rot))
				m_debug_cube->SetRot(rot, 0, NULL, ANIM_ROT, 0, 0.0f, NULL, NULL);
			if (!(m_debug_cube->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) == pos))
				m_debug_cube->SetPos(pos);
			if (!(*m_debug_cube->GetSize(0) == size)) {
				m_debug_cube->SetSize(size + math::v4(4.0f, 4.0f, 4.0f, 4.0f));

				math::v4 half = *focus->GetSize(0) * 0.5f;

				m_debug_cube_child->SetPos(math::v4(half.extract_x(), -half.extract_y(),
								    half.extract_z(),
								    half.extract_w()));
			}
		}
	}
}

void Scene::UpdateAdjustSize()
{
	UpdateDebugCube();
}

bool Scene::IsResolutionActMorph(int32_t id)
{
	return true;
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_29BCC252(Scene *scene, int32_t *width, int32_t *height)
{
	if (width == NULL || height == NULL || scene->m_env == NULL)
		return -1;

	SceneResolution *res = ScePafWidget_CDC90BA2(scene->m_env_param, scene->m_resolution_id);

	if (res == NULL)
		return -1;
	*width = res->m_width;
	*height = res->m_height;
	return 0;
}

namespace paf {
namespace ui {

bool Scene::IsOrientationAvailable(int32_t orientation)
{
	return m_env_param->GetResolutionId(m_resolution_id, orientation) != 0xff;
}

int32_t Scene::ChangeOrientation(int32_t orientation, float time)
{
	uint32_t id = m_env_param->GetResolutionId(m_resolution_id, orientation);

	if (id == 0xff)
		return SCE_PAF_ERROR_NO_ENTRY;
	if ((m_resolution_id_next == 0xff && id == m_resolution_id) ||
	    (m_resolution_id_next != 0xff && id == m_resolution_id_next))
		return 0;

	Timer *t = NULL;

	if (time > 0.0f)
		t = new Timer(time, Timer::FUNC_ANGULAR);
	ScePafWidget_11D537AF(this, id, t, true);
	ResetCameraRoll(time);
	return 0;
}

int32_t Scene::DrawScene(bool hide)
{
	if (m_camera == NULL || hide || m_hidden ||
	    (!(m_show_a_sum >= 0.02f) && m_hide_transparent)) {
		graph::DrawCalls()->FlushLayers(m_layer);
		return -1;
	}

	if (m_map_e_listener.size() != 0) {
		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(0x20001000, &e);
	}
	if (m_camera->Setup(m_screen) != 0) {
		graph::DrawCalls()->FlushLayers(m_layer);
	} else {
		graph::g_scene_draw_layer.m_ptr = m_layer;
		graph::g_scene_draw_layer2.m_ptr = NULL;
		if (m_draw_func != NULL) {
			m_draw_arg.m_alpha = m_meta_a_sum * m_show_a_sum;
			graph::g_scene_draw_layer.m_ptr->Push(0x50000000, 0, m_draw_func, m_draw_arg);
		}
		ScePafWidget_80946FB9(m_camera);
		graph::DrawCalls()->FlushLayers(graph::g_scene_draw_layer.m_ptr);
		graph::g_scene_draw_layer.m_ptr = NULL;
		graph::g_scene_draw_layer2.m_ptr = NULL;
	}
	if (m_map_e_listener.size() != 0) {
		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(0x20001001, &e);
	}
	return 0;
}

}
}

int32_t ScePafWidget_687B21CA(Scene *scene, bool hide)
{
	if (scene->m_bg_layer == NULL)
		return 0;
	if (scene->m_camera == NULL || hide || scene->m_hidden ||
	    (!(scene->m_show_a_sum >= 0.02f) && scene->m_hide_transparent)) {
		graph::DrawCalls()->FlushLayers(scene->m_bg_layer);
		return -1;
	}

	graph::g_scene_draw_layer.m_ptr = scene->m_layer;
	graph::g_scene_draw_layer2.m_ptr = scene->m_bg_layer;
	for (list<Scene::SurfaceWidget>::iterator it = scene->m_surface_widgets.begin();
	     it != scene->m_surface_widgets.end(); ++it)
		it->m_widget->OnDraw(it->m_info);

	graph::DrawLayer *layer = scene->m_bg_layer;

	if (layer->m_entries.size() != 0) {
		layer->Push(0, 0, Camera::DrawBegin, scene->m_camera);
		scene->m_bg_layer->Push(0x80000000, 0, Camera::DrawEnd, scene->m_camera);
	}
	graph::DrawCalls()->FlushLayers(scene->m_bg_layer);
	graph::g_scene_draw_layer.m_ptr = NULL;
	graph::g_scene_draw_layer2.m_ptr = NULL;
	return 0;
}

int32_t ScePafWidget_4AD02636(Scene *scene, uint32_t size)
{
	graph::DrawLayer *layer = scene->m_layer;

	if (layer == NULL || layer->m_entries.size() != 0)
		return -1;
	if (layer->m_used == 0)
		layer->m_buffer.resize(size, 0);
	return 0;
}

namespace paf {
namespace ui {

bool Scene::IsDisplayAreaScaled()
{
	if (m_display_area_mode != 0)
		return m_display_area_mode <= 1;
	if (m_env == NULL)
		return false;
	if (m_env->m_display_area_applied)
		return m_env->m_display_area_scale != 1.0f;
	return false;
}

int32_t Scene::HandleKeycodeEvent(Event *e)
{
	Handler::HandleKeycodeEvent(e);
	if (e->GetType() == 0x11) {
		switch (e->GetDetail() & 0xff) {
		case 28:
			DoEvent(CB_SCENE_ONCANCEL, e);
			break;
		case 29:
			DoEvent(CB_SCENE_CONTEXTMENU, e);
			break;
		}
	}
	return 0;
}

int32_t Scene::HandleSystemEvent(Event *e)
{
	switch (e->GetType()) {
	case 0x2001:
		if (e->m_data[0] == 0)
			DoEvent(0x800006, e);
		else if (e->m_data[0] == 1)
			DoEvent(0x800007, e);
		break;
	case 0x2002:
		DoEvent(0x800008, e);
		break;
	case 0x2003:
		DoEvent(0x800009, e);
		break;
	case 0x2004:
		DoEvent(0x80000a, e);
		break;
	default:
		DoEvent(CB_SCENE_SYSTEM, e);
		break;
	}
	return 0;
}

int32_t Scene::CreateBgLayer()
{
	if (m_bg_layer == NULL)
		m_bg_layer = graph::DrawCalls()->AddLayer("SceneBg", 100, 0, 0x1000);
	return 0;
}

void Scene::RemoveSurfaceWidget(Widget *widget)
{
	for (list<SurfaceWidget>::iterator it = m_surface_widgets.begin();
	     it != m_surface_widgets.end(); ++it) {
		if (it->m_widget == widget) {
			m_surface_widgets.erase(it);
			if (m_surface_widgets.size() == 0 && m_bg_layer != NULL) {
				graph::DrawCalls()->SetDrawMode(m_bg_layer, 3);
				m_bg_layer = NULL;
			}
			return;
		}
	}
}

void Scene::UpdateLayerWeight()
{
	if (!m_layer_weight_dirty)
		return;
	for (int32_t i = 15; i > 0; i--)
		m_layer_weight[i - 1] = m_layer_weight[i] * m_layer_range[i] +
					m_layer_range[i];
	if (m_env != NULL)
		m_env->m_layer_weight_changed = true;
	m_layer_weight_dirty = false;
}

void Scene::UpdateScreenSize(Timer *t)
{
	SceneResolution *res = ScePafWidget_CDC90BA2(m_env_param, m_resolution_id);

	if (res == NULL)
		return;

	Screen *screen = m_screen;

	screen->vp = math::rect(0.0f, 0.0f, res->m_viewport_width, res->m_viewport_height);
	screen->sp = math::rect(0.0f, 0.0f, res->m_scissor_width, res->m_scissor_height);
	if (m_camera != NULL)
		m_camera->UpdateScreenSize(t);
}

}
}
