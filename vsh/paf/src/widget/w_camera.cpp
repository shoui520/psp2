/*
 * paf::ui::Camera (DEBUGGER 0x810e3944..0x810e5fd4): what a scene is seen through. It makes
 * its view from its world matrix and roll and its projection from its distance and view size,
 * culls the scene's widgets against the scissor rectangle and queues the draws of the ones in
 * view, keeping the clip and scissor rectangles the widgets ask for on two stacks. No record or
 * string names the file; the names of the static functions and objects are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <vectormath.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/draw_info.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_screen.h>
#include <libkernel/libkernel.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <std/list>
#include <timer/timer.h>
#include <widget/core/animlistener.h>
#include <widget/core/environment.h>
#include <widget/w_camera.h>
#include <widget/w_scene.h>
#include "../std/math.h"

using sce::Vectormath::Simd::Aos::Matrix4;
using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace ui {

/* a turn, and the system software version the scissor snaps to pixels only on a full size
   screen from */
#define CAMERA_TURN 6.2831855f
#define CAMERA_SNAP_CHECK_VERSION 0x1500000

/* the size of the tiles the scissor rectangles are snapped to, in pixels (not static in
   Sony's build: it is read again each time) */
uint32_t s_scissor_tile = 32;

/* the clip and the scissor rectangles pushed, the top of each, and the scissor's center and
   size and whether it is empty */
static list<math::rect> s_clip_stack;
static list<math::rect> s_scissor_stack;
static math::rect s_clip_rect;
static math::rect s_scissor_rect;
static math::v2 s_scissor_center;
static math::v2 s_scissor_size;
static bool s_scissor_empty;

/* the vector with its y negated: the screen's y down, the rectangles' up */
static inline math::v2 FlipY(math::v2 v)
{
	v.set_y(-v.extract_y());
	return v;
}

/* the vector's x and y swapped */
static inline math::v2 Swap(math::v2 v)
{
	return math::v2(Vector2(vrev64_f32(v.getImpl().get64())));
}

/* rounded down and up, lane by lane */
static inline math::v2 Floor(math::v2 v)
{
	return math::v2(Vector2(sce_vectormath_floorf(v.getImpl().get64())));
}

static inline math::v2 Ceil(math::v2 v)
{
	return math::v2(Vector2(sce_vectormath_ceilf(v.getImpl().get64())));
}

/* the environment's display area scale, 1 outside (0, 1] */
static inline float GetDisplayAreaScale(Environment *env)
{
	float scale = env != NULL ? env->m_display_area_scale : 1.0f;

	if (scale <= 0.0f || scale > 1.0f)
		scale = 1.0f;
	return scale;
}

/* the rectangle, within the clip stack's top, pushed on the clip stack */
static inline void PushClip(math::rect rect)
{
	if (s_clip_stack.size() != 0) {
		math::rect top = s_clip_stack.back();

		rect = ScePafMisc_B2795FE2(rect, top);
	}
	s_clip_stack.push_back(rect);
	s_clip_rect = s_clip_stack.back();
}

const char Camera::m_widget_type[] = "camera";

bool Camera::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *Camera::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Camera::TypeName()
{
	return m_widget_type;
}

}
}

using namespace paf;
using namespace paf::ui;

void ScePafWidget_72F479CA(Camera *camera)
{
	if (camera->m_screen == NULL || !camera->m_projection_dirty ||
	    !(camera->m_screen_height > 0.0001f) || !(camera->m_distance > 0.0001f))
		return;

	float fov = sce_paf_atanf(camera->m_view_height * 0.5f / camera->m_distance) * 2.0f;
	float aspect = camera->m_view_width / camera->m_view_height;
	float near_z = camera->m_near_z;
	float far_z = camera->m_far_z;
	float t = sce_paf_tanf(fov * 0.5f);
	float top = near_z * t;
	float right = aspect * top;

	camera->m_projection = math::matrix::Frustum(-right, right, -(near_z * t), top, near_z, far_z);

	float pixels = camera->m_screen_width * 128.0f / (fov * 128.0f * 2.0f);
	float log2 = sce_paf_logf(2.0f);

	camera->m_projection_dirty = false;
	camera->m_mipbase = (int32_t)(-sce_paf_logf(pixels) / log2 * 16.0f);
}

namespace paf {
namespace ui {

Camera::Camera(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_projection_dirty(true), m_screen(NULL), m_roll(0.0f),
	  m_roll_abs_sin(0.0f), m_screen_width(640.0f), m_screen_height(480.0f),
	  m_distance(10.0f), m_fov(0.87266469f), m_view_width(640.0f), m_view_height(480.0f),
	  m_fov_axis(0), m_near_z(100.0f), m_far_z(10000.0f), m_view(math::matrix::Identity()),
	  m_inv_view(math::matrix::Identity()), m_projection(math::matrix::Identity()),
	  m_inv_scale(1.0f, 1.0f), m_draw_order(DRAW_ORDER_TABLE), m_update_zlist(true),
	  m_mipbase(10), m_mipoffs(0), m_pixel_snap(true)
{
	if ((m_event_enable & 0x400) != 0)
		EnableFocusEvent(false);
	m_event_enable = 0;
	m_hit_find_mode = 0;
	m_hit_detect_b_mode = 2;
	m_hit_detect_mode = 2;
	m_debug_events = false;
	if (parent != NULL && m_scene != NULL)
		m_scene->SetCamera(this);
	SetAnchor(100, 100, 100, NULL);
	m_parent_protected = true;
	ScePafWidget_72F479CA(this);
	if (!m_drag_begun)
		m_focus_target = false;
	m_not_hit_target = true;
}

Camera::~Camera()
{
	if (m_scene != NULL)
		m_scene->SetCamera(NULL);
	m_screen = (Screen *)-1;
}

const math::rect &Camera::PushScissor(math::rect rect, int32_t mode)
{
	if (s_scissor_tile >= 2 && m_scene != NULL && m_pixel_snap) {
		math::rect vp = m_scene->m_screen->vp;
		math::v2 origin = FlipY(vp.left_top());
		math::v2 rb = FlipY(rect.right_bottom());
		math::v2 lt = FlipY(rect.left_top());
		math::v2 drb = rb - origin;
		math::v2 dlt = lt - origin;
		float tile = (float)s_scissor_tile;
		float inv = 1.0f / (float)s_scissor_tile;

		if (mode == 0) {
			rb = Ceil(drb * inv) * tile + origin;
			lt = Floor(dlt * inv) * tile + origin;
		} else if (mode == 1) {
			rb = Floor(drb * inv) * tile + origin;
			lt = Ceil(dlt * inv) * tile + origin;
		} else if (mode == 2) {
			math::v2 half = math::v2(tile, tile) * 0.5f;

			rb = Floor((drb + half) * inv) * tile + origin;
			lt = Ceil((dlt - half) * inv) * tile + origin;
		}
		math::v2 size = rb - lt;
		math::v2 center = FlipY((rb + lt) * 0.5f);

		rect.set(center, size);
	}
	if (s_scissor_stack.size() != 0) {
		math::rect top = s_scissor_stack.back();

		rect = ScePafMisc_B2795FE2(rect, top);
	}
	s_scissor_stack.push_back(rect);
	s_scissor_rect = rect;
	s_scissor_center = rect.center();
	s_scissor_size = rect.size();
	s_scissor_empty = s_scissor_size.extract_x() <= 0.0f || s_scissor_size.extract_y() <= 0.0f;
	return s_scissor_stack.back();
}

math::rect Camera::ProjectRect(math::rect rect, bool env_roll)
{
	if (!env_roll && __builtin_fabsf(m_roll) < 0.01f &&
	    __builtin_fabsf(m_inv_scale.extract_x() - 1.0f) < 0.001f &&
	    __builtin_fabsf(m_inv_scale.extract_y() - 1.0f) < 0.001f)
		return rect;

	float sin_roll = m_roll_abs_sin;

	if (env_roll) {
		if (__builtin_fabsf(m_env->GetRollScreen()) < 0.01f)
			return rect;
		sin_roll = __builtin_fabsf(sce_paf_sinf(m_env->GetRollScreen()));
	}

	math::v2 size = rect.size();

	size = (size * (1.0f - sin_roll) + Swap(size) * sin_roll) * m_inv_scale;

	math::v2 center = rect.center();
	Vector4 pos = m_inv_view.getImpl() *
		      Vector4(center.extract_x(), center.extract_y(), 0.0f, 0.0f);

	return math::rect(math::v2(pos.getXY()), size);
}

float Camera::GetDisplayAreaOffset()
{
	if (m_screen == NULL)
		return 0.0f;

	graph::Surface *surface = m_screen->m_surface.get();
	float height = (float)surface->GetHeight();
	float width = (float)surface->GetWidth();

	if (height == 0.0f)
		return 0.0f;
	width *= 0.5625f;
	if (height == width)
		return 0.0f;

	float diff = height - width;
	float offset = diff * -0.5f;
	float scale = GetDisplayAreaScale(m_env);

	if (diff != 0.0f && scale >= width / height)
		offset = height * (1.0f - scale) / offset;
	return offset / height * m_screen_height;
}

int32_t Camera::Setup(Screen *scr)
{
	if (m_hold_count > 0 || scr == NULL)
		return -1;

	ScePafWidget_72F479CA(this);
	m_pixel_snap = true;
	if (SceLibKernel_9F793F84() >= CAMERA_SNAP_CHECK_VERSION) {
		if (!scr->IsFullSize())
			m_pixel_snap = false;
		else if (m_scene != NULL &&
			 !(math::v3(m_scene->GetScale(0)->getImpl().getXYZ()) ==
			   math::v3(1.0f, 1.0f, 1.0f)))
			m_pixel_snap = false;
		else if (m_env->m_display_area_applied && m_env->m_display_area_scale != 1.0f)
			m_pixel_snap = false;
	}

	math::matrix world = m_world_matrix;

	if (m_scene != NULL && m_scene->IsDisplayAreaScaled()) {
		float offset = GetDisplayAreaOffset();
		float scale = GetDisplayAreaScale(m_env);

		world = world * math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
					     math::v4(0.0f, 1.0f, 0.0f, 0.0f),
					     math::v4(0.0f, 0.0f, 1.0f, 0.0f),
					     math::v4(0.0f, offset,
						      -(m_distance * (1.0f / scale - 1.0f)), 1.0f));
	}
	m_view = math::matrix(Matrix4(-world.getImpl().getCol0(), world.getImpl().getCol1(),
				      -world.getImpl().getCol2(), world.getImpl().getCol3()));

	float roll = m_roll;

	if (roll != 0.0f) {
		float s = sce_paf_sinf(roll);
		float c = sce_paf_cosf(roll);

		m_view = m_view * math::matrix(math::v4(c, s, 0.0f, 0.0f),
					       math::v4(-s, c, 0.0f, 0.0f),
					       math::v4(0.0f, 0.0f, 1.0f, 0.0f),
					       math::v4(0.0f, 0.0f, 0.0f, 1.0f));
	}
	m_inv_view = m_view.inverse();
	m_inv_scale = math::v2(sce::Vectormath::Simd::Aos::divPerElem(
		Vector2(1.0f, 1.0f), m_scale.getImpl().getXY()));

	s_clip_stack.clear();
	s_scissor_stack.clear();

	math::rect rect = ProjectRect(scr->sp, true);

	PushClip(rect);
	PushScissor(rect, 0);
	return 0;
}

void Camera::SetScreenSize(Timer *t, float width, float height, float distance)
{
	m_screen_width = width;
	m_screen_height = height;
	if (m_screen == NULL) {
		m_view_width = width;
		m_view_height = height;
	}

	float half = (m_fov_axis != 0 ? height : width) * 0.5f;

	if (distance > 0.0f) {
		m_distance = distance;
		m_fov = sce_paf_atanf(half / distance) * 2.0f;
		distance = m_distance;
	} else {
		distance = half / sce_paf_tanf(m_fov * 0.5f);
		m_distance = distance;
	}
	SetPos(math::v4(0.0f, 0.0f, distance, 0.0f), t, ANIM_MOVE, 0, 0.0f, NULL, NULL);
	LookAt(m_scene, math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	m_projection_dirty = true;
}

void Camera::UpdateScreenSize(Timer *t)
{
	if (m_screen != NULL) {
		math::rect sp = m_screen->sp;
		math::rect vp = m_screen->vp;

		m_view_width = vp.width();
		m_view_height = vp.height();
		SetScreenSize(t, sp.width(), sp.height(), -1.0f);
	}
}

}
}

void ScePafWidget_8EDC2803(Camera *camera, Screen *scr)
{
	if (camera->m_screen != scr) {
		camera->m_screen = scr;
		if (scr != NULL) {
			math::rect sp = scr->sp;
			math::rect vp = camera->m_screen->vp;

			camera->m_view_width = vp.width();
			camera->m_view_height = vp.height();
			camera->SetScreenSize(NULL, sp.width(), sp.height(), -1.0f);
		}
	}
}

namespace paf {
namespace ui {

math::rect Camera::ScaleRect(math::rect rect)
{
	math::rect r = rect;

	if (m_scene->IsDisplayAreaScaled() && !m_pixel_snap) {
		float scale = GetDisplayAreaScale(m_env);

		r.set(r.center() * scale, r.size() * scale);

		float y = r.center_y();

		r.set_center_y(y - GetDisplayAreaOffset() * scale);
	}
	return r;
}

void Camera::SetRollScreen_core(float roll)
{
	if (roll >= CAMERA_TURN || roll <= -CAMERA_TURN) {
		float turns = sce_paf_floorf(roll / CAMERA_TURN);

		if (turns < 0.0f)
			turns += 1.0f;
		roll -= turns * CAMERA_TURN;
	}
	m_roll = roll;
	m_roll_abs_sin = __builtin_fabsf(sce_paf_sinf(roll));
}

float Camera::GetRollScreen(int32_t opt)
{
	if (opt == 0)
		return m_roll;

	AnimQueue *queue = FindAnim(ANIM_ROLL);

	if (queue != NULL && queue->m_queue[0] != NULL) {
		AnimListener *listener = (AnimListener *)queue->m_queue[0]->GetBack();

		if (listener != NULL) {
			float *target = listener->GetTargetF();

			if (target != NULL) {
				float roll = *target;
				float turns = sce_paf_floorf(roll / CAMERA_TURN);

				if (turns < 0.0f)
					turns += 1.0f;
				return roll - turns * CAMERA_TURN;
			}
		}
	}
	return m_roll;
}

void Camera::SetRollScreen(float roll, Timer *t)
{
	if (t == NULL) {
		KillAnimType(ANIM_ROLL);
		SetRollScreen_core(roll);
		return;
	}

	float cur = GetRollScreen(1);
	float dist = __builtin_fabsf(roll - cur);

	if (dist > __builtin_fabsf(roll - CAMERA_TURN - cur))
		roll = roll - CAMERA_TURN;
	else if (dist > __builtin_fabsf(roll + CAMERA_TURN - cur))
		roll = roll + CAMERA_TURN;
	SetAnimListener(ANIM_ROLL, new RollListener(t, this, GetRollScreen(0), roll), 1);
}

}
}

void ScePafWidget_6AEB7291(Camera *camera, Camera *other)
{
	if (other == NULL || other->GetTypeName() != Camera::m_widget_type)
		return;

	ScePafWidget_72F479CA(other);

	int32_t fov_axis = other->m_fov_axis;

	ScePafWidget_72F479CA(other);

	float fov = other->m_fov;

	camera->m_fov_axis = fov_axis;
	camera->m_projection_dirty = true;
	camera->m_fov = fov;
}

void ScePafWidget_83AD517E(Camera *camera, Camera *other)
{
	if (other == NULL || other->GetTypeName() != Camera::m_widget_type)
		return;

	ScePafWidget_72F479CA(other);

	float near_z = other->m_near_z;
	float far_z = other->m_far_z;

	ScePafWidget_72F479CA(other);

	float width = other->m_screen_width;
	float height = other->m_screen_height;

	camera->m_near_z = near_z;
	camera->m_far_z = far_z;
	camera->m_projection_dirty = true;
	camera->SetScreenSize(NULL, width, height, -1.0f);
}

namespace paf {
namespace ui {

void Camera::SyncPosture(Camera *other)
{
	if (other == NULL || other->GetTypeName() != m_widget_type)
		return;

	math::v4 pos = other->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 rot = other->GetRot(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
	float roll = other->GetRollScreen(0);

	SetPos(pos, NULL, ANIM_MOVE, 0, 0.0f, NULL, NULL);
	SetRot(rot, 0, NULL, ANIM_ROT, 0, 0.0f, NULL, NULL);
	SetRollScreen(roll, NULL);
}

void Camera::DrawBegin(Camera *const &camera)
{
	graph::arl::ArlInstance *gl = graph::arl::Gl();
	Camera *cam = camera;

	if (cam == NULL || cam->m_scene == NULL)
		return;

	Screen *scr = cam->m_scene->m_screen;

	if (scr == NULL)
		return;
	if (scr->OnTarget() == 0)
		scr->OnClear();
	if (cam->m_draw_order == DRAW_ORDER_ZSORT)
		gl->Enable(graph::arl::DEPTH_TEST);
	else
		gl->Disable(graph::arl::DEPTH_TEST);
	gl->DepthFunc(graph::arl::DEPTH_ALWAYS, graph::arl::FRONT_AND_BACK);
	gl->Enable(graph::arl::SCISSOR_TEST);
	gl->Projection().Push();
	gl->View().Push();
	gl->m_model.Push();
	gl->Projection().Load(cam->m_projection);
	gl->View().Load(cam->m_inv_view);
}

void Camera::DrawEnd(Camera *const &camera)
{
	Camera *cam = camera;

	if (cam == NULL || cam->m_scene == NULL)
		return;

	Screen *scr = cam->m_scene->m_screen;

	if (scr == NULL)
		return;
	scr->OnFinish();

	graph::arl::ArlInstance *gl = graph::arl::Gl();

	gl->Projection().Pop();
	gl->View().Pop();
	gl->m_model.Pop();
}

}
}

void ScePafWidget_EFED66D2(Camera *camera, const math::rect &rect, graph::DrawInfo &info)
{
	if (camera->m_env->IsDrawSkipped())
		return;

	uint32_t key = (info.m_layer << 16) + (info.m_sub_layer << 8) + info.m_order;
	Screen *scr = camera->m_scene->m_screen;

	if (scr == NULL)
		return;

	graph::Surface *surface = scr->GetSurface().get();

	if (surface == NULL)
		return;
	if (camera->m_pixel_snap) {
		Camera::DrawScissorArg arg(scr->ToPixelRect(rect), (graph::ScreenSurface *)surface);

		graph::g_scene_draw_layer.m_ptr->Push(key & 0x7fffffff, ScePafWidget_C14CCBD4, arg);
	} else {
		Camera::DrawScissorArg arg(scr->ToSurfaceRect(rect), (graph::ScreenSurface *)surface);

		graph::g_scene_draw_layer.m_ptr->Push(key & 0x7fffffff, Camera::DrawScissor, arg);
	}
}

void ScePafWidget_4D02799A(Camera *camera, const math::rect &rect, graph::DrawInfo &info)
{
	if (camera->m_env->IsDrawSkipped() || !camera->m_pixel_snap)
		return;

	uint32_t key = (info.m_layer << 16) + (info.m_sub_layer << 8) + info.m_order;
	Screen *scr = camera->m_scene->m_screen;

	if (scr == NULL || scr->m_surface.get() == NULL)
		return;

	graph::Surface *surface = scr->GetSurface().get();
	Camera::DrawScissorArg arg(scr->ToSurfaceRect(rect), (graph::ScreenSurface *)surface);

	graph::g_scene_draw_layer.m_ptr->Push(key & 0x7fffffff, Camera::DrawScissor, arg);
}

__attribute__((noinline)) void ScePafWidget_56B8240B(Camera *camera, Widget *widget, graph::DrawInfo &info)
{
	if (widget->m_hold_count > 0)
		return;

	math::rect rect = widget->CalcClipRect();
	uint32_t mode = widget->m_clip_mode;
	int32_t scissor_mode = 0;

	rect = camera->ScaleRect(rect);
	rect = camera->ProjectRect(rect, false);

	math::rect prev;

	if (mode > 1) {
		if (mode <= 2)
			scissor_mode = 1;
		else if (mode <= 3)
			scissor_mode = 2;
	} else if (mode != 0) {
		prev = s_clip_rect;
		PushClip(rect);
		if (s_clip_rect != prev)
			ScePafWidget_4D02799A(camera, s_clip_rect, info);
		++info.m_sub_layer;
	}
	prev = s_scissor_rect;
	if (camera->PushScissor(rect, scissor_mode) != prev)
		ScePafWidget_EFED66D2(camera, s_scissor_rect, info);
}

namespace paf {
namespace ui {

__attribute__((noinline)) const math::rect &Camera::PopScissor()
{
	if (s_scissor_stack.size() != 0)
		s_scissor_stack.pop_back();

	math::rect top;

	if (s_scissor_stack.size() != 0)
		s_scissor_rect = s_scissor_stack.back();
	else
		s_scissor_rect = math::rect(Screen::s_root_screen->sp);
	s_scissor_center = s_scissor_rect.center();
	s_scissor_size = s_scissor_rect.size();
	s_scissor_empty = s_scissor_size.extract_x() <= 0.0f || s_scissor_size.extract_y() <= 0.0f;
	return s_scissor_rect;
}

__attribute__((noinline)) const math::rect &Camera::PopClip()
{
	if (s_clip_stack.size() != 0)
		s_clip_stack.pop_back();

	math::rect top;

	if (s_clip_stack.size() != 0)
		s_clip_rect = s_clip_stack.back();
	else
		s_clip_rect = math::rect(Screen::s_root_screen->sp);
	return s_clip_rect;
}

}
}

__attribute__((noinline)) void ScePafWidget_249D56BF(Camera *camera, Widget *widget, graph::DrawInfo &info)
{
	if (widget->m_hold_count > 0)
		return;

	uint32_t mode = widget->m_clip_mode;
	math::rect prev;

	if (mode == 1) {
		prev = s_clip_rect;
		if (Camera::PopClip() != prev)
			ScePafWidget_4D02799A(camera, s_clip_rect, info);
		++info.m_sub_layer;
	}
	prev = s_scissor_rect;
	if (Camera::PopScissor() != prev)
		ScePafWidget_EFED66D2(camera, s_scissor_rect, info);
}

void ScePafWidget_C14CCBD4(const Camera::DrawScissorArg &arg)
{
	math::rect rect = arg.rect;
	graph::ScreenSurface *surface = arg.surface;

	surface->SetClip(rect.left(), rect.bottom(), rect.right(), rect.top(), true);
}

namespace paf {
namespace ui {

void Camera::DrawScissor(const DrawScissorArg &arg)
{
	math::rect rect = arg.rect;
	graph::ScreenSurface *surface = arg.surface;

	surface->SetScissor(rect.left(), rect.bottom(), rect.right(), rect.top(), true);
}

/* the widget's draw objects queued where it draws: into its scene's surface entry for it, or
   its own OnDraw */
static inline int32_t DrawWidget(Camera *camera, Widget *widget, graph::DrawInfo &info)
{
	if (widget->m_draw_target == 1) {
		list<Scene::SurfaceWidget> &entries = camera->m_scene->m_surface_widgets;

		for (list<Scene::SurfaceWidget>::iterator it = entries.begin(); it != entries.end();
		     ++it) {
			if ((*it).m_widget == widget) {
				(*it).m_info = info;
				break;
			}
		}
		return 0;
	}
	return widget->OnDraw(info);
}

int32_t Camera::add_obj(Widget *root, graph::DrawInfo &info)
{
	if (root->m_hold_count > 0)
		return 0;

	bool empty = s_scissor_empty;

	info.m_order = 0;
	if (empty || root == this)
		goto culled;
	{
		const math::matrix &world = root->GetWorldMatrix();
		Vector4 pos = m_inv_view.getImpl() * Vector4(world.getImpl().getCol3().getXYZ(), 1.0f);
		float z = __builtin_fabsf(pos.getZ());
		float distance = m_distance;

		if (z < distance * 0.1f || z == 0.0f)
			goto culled;

		float ratio = distance / z;
		math::v2 offs = math::v2(sce::Vectormath::Simd::Aos::absPerElem(
			(pos * ratio).getXY() - s_scissor_center.getImpl()));
		math::v2 half = math::v2(root->GetSize(0)->getImpl().getXY()) * 0.5f;
		Vector2 col0 = world.getImpl().getCol0().getXY();
		Vector2 col1 = world.getImpl().getCol1().getXY();
		Vector2 corner0 = sce::Vectormath::Simd::Aos::mulPerElem(half.getImpl(), col0);
		Vector2 corner1 = sce::Vectormath::Simd::Aos::mulPerElem(half.getImpl(), col1);
		math::v2 flip = half;

		flip.set_x(-half.extract_x());

		Vector2 corner2 = sce::Vectormath::Simd::Aos::mulPerElem(flip.getImpl(), col0);
		Vector2 corner3 = sce::Vectormath::Simd::Aos::mulPerElem(flip.getImpl(), col1);
		Vector2 ext0(corner2.getX() + corner2.getY(), corner3.getX() + corner3.getY());
		Vector2 ext1(corner0.getX() + corner0.getY(), corner1.getX() + corner1.getY());
		math::v2 ext = math::v2(mulPerElem(
			maxPerElem(absPerElem(ext0), absPerElem(ext1)), m_inv_scale.getImpl()) * ratio);
		math::v2 out = offs - (ext * (1.0f - m_roll_abs_sin) + Swap(ext) * m_roll_abs_sin) -
			       s_scissor_size * 0.6f;
		int32_t ret = 0;

		if (out.extract_x() >= 0.0f || out.extract_y() >= 0.0f) {
			root->m_culled = true;
			root->SetFitToScene(true);
		} else {
			root->m_culled = false;
			if (!root->m_draw_always)
				ret = DrawWidget(this, root, info);

			math::v2 inner = offs - s_scissor_size * 0.5f;

			if (inner.extract_x() >= 0.0f || inner.extract_y() >= 0.0f)
				root->SetFitToScene(true);
			else
				root->SetFitToScene(false);
		}
		if (root->m_draw_always)
			return DrawWidget(this, root, info);
		return ret;
	}
culled:
	root->m_culled = true;
	root->SetFitToScene(true);
	return 0;
}

}
}

void ScePafWidget_1A6D5164(Camera *camera, Widget *widget, graph::DrawInfo &info)
{
	int32_t ret = 0;

	if (widget->m_show_a_sum >= 0.02f) {
		if (widget->m_clip_children &&
		    (ScePafWidget_56B8240B(camera, widget, info), ++info.m_layer, s_scissor_empty)) {
			camera->SetCulled(true);
		} else {
			if (widget->m_draw_before_children) {
				ret = camera->add_obj(widget, info);
				++info.m_sub_layer;
			}
			if (widget->m_children.size() > 0) {
				graph::DrawInfo child;

				child.m_layer = info.m_layer + 1;
				child.m_max_layer = info.m_max_layer;
				child.m_order = info.m_order;
				child.m_sub_layer = 0;
				child.m_reserved = info.m_reserved;
				for (list<Widget *>::iterator it = widget->m_children.begin();
				     it != widget->m_children.end(); ++it)
					ScePafWidget_1A6D5164(camera, *it, child);
				info.m_layer = child.m_max_layer + 1;
				info.m_max_layer = child.m_max_layer + 1;
			}
			if (!widget->m_draw_before_children) {
				ret = camera->add_obj(widget, info);
				++info.m_sub_layer;
			}
			if (ret == 1) {
				widget->ApplyDrawObjState(info);
				++info.m_sub_layer;
			}
		}
		if (widget->m_clip_children) {
			ScePafWidget_249D56BF(camera, widget, info);
			++info.m_layer;
		}

		Environment *env = camera->m_env;

		if (env != NULL && widget->m_focus_root != NULL && ScePafWidget_A6B659BB(env, 0)) {
			Widget *target = camera->m_env->m_focus_cursor.m_target;

			if (target != NULL) {
				Widget *root = ScePafWidget_CFA1EB7A(widget);
				uint32_t first = 0;

				if (root != NULL && root->m_focus_root != NULL)
					first = root->m_focus_root->m_state;
				if ((uint32_t)target == first) {
					env = camera->m_env;
					if (!env->m_focus_cursor_moving_only ||
					    env->m_focus_cursor.IsMoving() ||
					    camera->m_env->m_focus_cursor.m_target->m_focus_direction_off) {
						Widget *cursor = camera->m_env->m_focus_cursor.m_cursor;

						if (cursor->m_color_alpha != 1.0f) {
							cursor->m_color_alpha = 1.0f;
							cursor->OnDraw(info);
						}
						camera->m_env->m_focus_cursor.m_cursor->m_color_alpha = 0.0f;
					} else {
						camera->m_env->m_focus_cursor.m_cursor->m_color_alpha = 1.0f;
					}
				}
			}
		}
	}
	if ((uint32_t)info.m_max_layer < (uint32_t)info.m_layer)
		info.m_max_layer = info.m_layer;
}

void ScePafWidget_80946FB9(Camera *camera)
{
	Scene *scene = camera->m_scene;

	if (scene->m_scissor_to_scene) {
		Environment *env = camera->m_env;

		if (!env->m_display_area_applied || env->m_display_area_scale == 1.0f ||
		    env->m_display_area_scene != scene) {
			math::v4 center = scene->GetPosCenter(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			math::rect rect(math::v2(center.getImpl().getXY()),
					math::v2(camera->m_scene->GetSize(0)->getImpl().getXY()));

			rect = camera->ScaleRect(rect);
			rect = camera->ProjectRect(rect, false);
			camera->PushScissor(rect, 0);
			scene = camera->m_scene;
		}
	}

	graph::DrawInfo info;

	info.m_layer = 0;
	info.m_max_layer = 0;
	info.m_order = 0;
	info.m_sub_layer = 0;
	info.m_reserved = 0;
	ScePafWidget_1A6D5164(camera, scene, info);
	camera->m_update_zlist = false;
	if (graph::g_scene_draw_layer.m_ptr->m_entries.size() != 0) {
		info.m_layer = 0;
		info.m_max_layer = 0;
		info.m_order = 0;
		info.m_sub_layer = 0;
		ScePafWidget_EFED66D2(camera, s_scissor_rect, info);
		graph::g_scene_draw_layer.m_ptr->Push(0, 0, Camera::DrawBegin, camera);
		graph::g_scene_draw_layer.m_ptr->Push(0x80000000, 0, Camera::DrawEnd, camera);
	}
}

namespace paf {
namespace ui {

void RollListener::OnTick(void *data1, void *data2)
{
	float start = m_anim_data[0];

	if (m_timer->m_first_tick) {
		start = ((Camera *)m_widget)->GetRollScreen(0);
		m_anim_data[0] = start;
	}

	float x = m_timer->m_mapped_x;

	((Camera *)m_widget)->SetRollScreen_core(m_anim_data[1] * x + start * (1.0f - x));
}

}
}
