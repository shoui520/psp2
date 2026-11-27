/*
 * paf::ui::JsDocument ("js_document") implementation for 2014 PAF MapView.
 */
#include <widget/js_document.h>
#include <graphics/drawobj/map_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/stdlib.h>
#include <widget/core/environment.h>
#include <widget/map_core.h>
#include <widget/web_renderer.h>

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::Vector3;

const char JsDocument::m_widget_type[] = "js_document";

static float s_viewport_width;
static float s_viewport_height;
static int32_t s_initial_render_skip_count;

int32_t MapTouchTracker::ComputeDelta(int32_t *out_dx, int32_t *out_dy)
{
	if (out_dx != NULL && out_dy != NULL) {
		if (m_phase == 5) {
			*out_dx = m_down_x - m_drag_x;
			*out_dy = m_down_y - m_drag_y;
		} else if (m_phase == 3) {
			*out_dx = m_down_x - m_move_x;
			*out_dy = m_down_y - m_move_y;
		}
	}
	return 0;
}

const char *JsDocument::TypeName()
{
	return m_widget_type;
}

bool JsDocument::IsInherit(const char *type) const
{
	if (type == m_widget_type)
		return true;
	return Widget::IsInherit(type);
}

char *JsDocument::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

JsDocument::JsDocument(Widget *parent, CreateParam *param)
	: Widget(parent, param),
	  m_pinch_origin_x(0),
	  m_pinch_origin_y(0),
	  m_pinching(false),
	  m_dragging(false),
	  m_last_touch_tick(0),
	  m_renderer(NULL),
	  m_map_obj(NULL),
	  m_bg_loader(NULL),
	  m_map_core(NULL),
	  m_initial_render_done(false),
	  m_touch_user_data(0)
{
	m_touch.m_down_x = 0;
	m_touch.m_down_y = 0;
	m_touch.m_move_x = 0;
	m_touch.m_move_y = 0;
	m_touch.m_drag_start_x = 0;
	m_touch.m_drag_start_y = 0;
	m_touch.m_drag_x = 0;
	m_touch.m_drag_y = 0;
	m_touch.m_delta_x = 0;
	m_touch.m_delta_y = 0;
	m_touch.m_phase = 0;

	EnableEvent(0xFFF0);

	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_check = 1;

	SetPinchMode(0x200);
	SetGestureMask(m_touch_gesture_mask | 1, true);
	SetGestureMask(m_touch_gesture_mask | 2, true);
	SetGestureMask(m_touch_gesture_mask | 4, true);

	m_name.SetID(string("js_document"));

	SetPinchLimitMin(math::v4(s_viewport_width * 3.0f, s_viewport_height * 3.0f, 0.0f, 0.0f));
	SetPinchLimitMax(math::v4(s_viewport_width * 15.0f * 20.0f,
				  s_viewport_height * 15.0f * 20.0f, 0.0f, 0.0f));
}

JsDocument::~JsDocument()
{
	m_touch.m_down_x = 0;
	m_touch.m_down_y = 0;
	m_touch.m_move_x = 0;
	m_touch.m_move_y = 0;
	m_touch.m_drag_start_x = 0;
	m_touch.m_drag_start_y = 0;
	m_touch.m_drag_x = 0;
	m_touch.m_drag_y = 0;
}

int32_t JsDocument::UpdateRenderedRect(WebRect visible_rect, WebRect *out_rect)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_renderer == NULL)
		return ret;

	Vector3 doc_tl = m_renderer->m_widget_to_doc *
			 Vector3(visible_rect.m_left, visible_rect.m_bottom, 1.0f);
	Vector3 doc_br = m_renderer->m_widget_to_doc *
			 Vector3(visible_rect.m_right, visible_rect.m_top, 1.0f);

	WebRect doc_tile_rect;

	doc_tile_rect.SetCenterSize((doc_tl.getX() + doc_br.getX()) * 0.5f,
				    (doc_tl.getY() + doc_br.getY()) * 0.5f,
				    s_viewport_width, s_viewport_height);

	WebRect widget_bounds;
	float cur_w = GetSize(GET_CURRENT)->extract_x();
	float cur_h = GetSize(GET_CURRENT)->extract_y();

	widget_bounds.Set(0.0f, 0.0f, cur_w, cur_h);
	doc_tile_rect.ClampMin(widget_bounds);

	Vector3 widget_tl = m_renderer->m_doc_to_widget *
			    Vector3(doc_tile_rect.m_left, doc_tile_rect.m_bottom, 1.0f);
	Vector3 widget_br = m_renderer->m_doc_to_widget *
			    Vector3(doc_tile_rect.m_right, doc_tile_rect.m_top, 1.0f);

	WebRect widget_tile_rect;

	widget_tile_rect.Set(widget_tl.getX(), widget_tl.getY(),
			     widget_br.getX(), widget_br.getY());

	if (!m_initial_render_done) {
		ret = m_renderer->SetRenderedRect(widget_tile_rect, &doc_tile_rect, true, false);
		if (s_initial_render_skip_count != 0)
			--s_initial_render_skip_count;
		else
			m_initial_render_done = true;
		*out_rect = widget_tile_rect;
	}

	return ret;
}

int32_t JsDocument::UpdateVisibleRect(WebRect *out_rect)
{
	WebRect unused_rect;
	int32_t ret = (int32_t)0x80AF0001;

	if (m_parent != NULL) {
		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		float half_w = parent_size.extract_x() * 0.5f;
		float half_h = parent_size.extract_y() * 0.5f;
		WebRect visible_rect;

		visible_rect.Set(-half_w, half_h, half_w, -half_h);
		*out_rect = visible_rect;

		if (m_renderer != NULL) {
			WebRect doc_rect;

			ret = m_renderer->SetVisibleRect(visible_rect, &doc_rect, true);
		}
	}

	(void)unused_rect;
	return ret;
}

int32_t JsDocument::Initialize(WebCoreView *view, WebCoreViewport *viewport,
			       WebRenderClient **out_render_client, WebCoreFrame *frame,
			       WebSurfaceHost **out_surface_host, MapCore *map_core)
{
	int32_t ret = (int32_t)0x80AF0001;

	m_map_core = map_core;
	m_renderer = new WebRenderer();
	if (m_renderer == NULL || viewport == NULL || frame == NULL || m_parent == NULL)
		return ret;

	math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);

	SetSize(parent_size, NULL, ANIM_SIZE, 0, 0.0f, NULL);
	s_viewport_width = parent_size.extract_x();
	s_viewport_height = parent_size.extract_y();
	m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	WebRect view_rect;
	view_rect.Set(0.0f, 0.0f, parent_size.extract_x(), parent_size.extract_y());

	WebRect surface_rect;
	surface_rect.Set(0.0f, 0.0f, parent_size.extract_x(), parent_size.extract_y());

	m_bg_loader = new MapBackgroundLoader();
	if (m_bg_loader == NULL)
		return ret;

	m_map_obj = new graph::MapObj(graph::PlaneObjStyle());
	if (m_map_obj == NULL)
		return ret;

	ret = m_renderer->Initialize(this, math::v2(parent_size.extract_x(), parent_size.extract_y()),
				     surface_rect, view_rect,
				     (int32_t)s_viewport_width, (int32_t)s_viewport_height,
				     view, viewport, out_render_client, frame,
				     out_surface_host, m_map_obj);

	UpdateVisibleRect(&view_rect);
	UpdateRenderedRect(view_rect, &surface_rect);
	return ret;
}

void JsDocument::DestroyWidget()
{
	if (m_renderer != NULL)
		m_renderer->Finalize();
	Widget::DestroyWidget();
}

void JsDocument::UpdateSize()
{
	math::v4 cur_size = *GetSize(GET_CURRENT);
	math::v4 max_size = cur_size * 4.0f;
	math::v4 min_size = cur_size * 0.25f;

	SetPinchLimitMax(max_size);
	SetPinchLimitMin(min_size);

	if (m_renderer != NULL) {
		math::v4 updated = *GetSize(GET_CURRENT);

		m_renderer->SetWidgetSize(math::v2(updated.extract_x(), updated.extract_y()));
	}
}

int32_t JsDocument::OnTouchMove()
{
	HitInfo hit;

	if (m_env->GetHitStack(&hit, this) == 0) {
		m_touch.m_phase = 3;
		m_touch.m_move_x = (int32_t)hit.m_touch_pos.extract_x();
		m_touch.m_move_y = (int32_t)hit.m_touch_pos.extract_y();

		WebRect visible_rect;

		UpdateVisibleRect(&visible_rect);
	}
	return 0;
}

void JsDocument::OnTouchEndNotify()
{
	TouchInfo *touch = &m_env->m_touch_state[0][0];

	if (touch != NULL) {
		int32_t pos_x = (int32_t)touch->m_pos.extract_x();
		int32_t pos_y = (int32_t)touch->m_pos.extract_y();

		if (sce_paf_abs(pos_x - m_touch.m_down_x) < 5 &&
		    sce_paf_abs(pos_y - m_touch.m_down_y) < 5) {
			pos_x = m_touch.m_down_x;
			pos_y = m_touch.m_down_y;
		}

		MapPixelPoint pt;
		pt.m_x = pos_x;
		pt.m_y = pos_y;

		if (m_map_core != NULL) {
			MapViewListener *listener = m_map_core->GetViewListener();

			if (listener != NULL)
				listener->OnTouchEnd(m_map_core->GetMapView(), &pt,
						     m_touch_user_data);
		}
		m_touch.ComputeDelta(&m_touch.m_delta_x, &m_touch.m_delta_y);
	}
}

int32_t JsDocument::ForwardMouseEvent(int32_t phase, int32_t pos_x, int32_t pos_y)
{
	if (phase == 0)
		return m_renderer->HandleMouseEvent(0, pos_x, pos_y, 1, 0, 0, 0);
	if (phase == 1)
		return m_renderer->HandleMouseEvent(1, pos_x, pos_y, 1, 0, 0, 0);
	if (phase == 2)
		return m_renderer->HandleMouseEvent(4, pos_x, pos_y, 1, 0, 0, 0);
	return 0;
}

int32_t JsDocument::OnTouchUp()
{
	HitInfo hit;

	if (m_env->GetHitStack(&hit, this) != 0)
		return 0;

	int32_t pos_x = (int32_t)hit.m_touch_pos.extract_x();
	int32_t pos_y = (int32_t)hit.m_touch_pos.extract_y();

	ForwardMouseEvent(2, pos_x, pos_y);
	int32_t ret = ForwardMouseEvent(1, pos_x, pos_y);

	OnTouchEndNotify();
	return ret;
}

void JsDocument::NotifyTouchBegin(Event *e, uint32_t touch_index)
{
	(void)e;
	if (touch_index >= 8)
		return;

	TouchInfo *touch = &m_env->m_touch_state[0][touch_index];

	if (touch != NULL && m_map_core != NULL) {
		MapPixelPoint pt;

		pt.m_x = (int32_t)touch->m_pos.extract_x();
		pt.m_y = (int32_t)touch->m_pos.extract_y();

		MapViewListener *listener = m_map_core->GetViewListener();

		if (listener != NULL)
			listener->OnTouchBegin(m_map_core->GetMapView(), &pt, m_touch_user_data);
	}
}

int32_t JsDocument::OnTouchDown(Event *e)
{
	HitInfo hit;

	if (m_env->GetHitStack(&hit, this) != 0)
		return 0;

	int32_t pos_x = (int32_t)hit.m_touch_pos.extract_x();
	int32_t pos_y = (int32_t)hit.m_touch_pos.extract_y();

	m_touch.m_down_x = pos_x;
	m_touch.m_down_y = pos_y;

	uint32_t touch_index = e->GetDetail() & 0xFF;

	if (touch_index == 0)
		NotifyTouchBegin(e, touch_index);
	return ForwardMouseEvent(0, pos_x, pos_y);
}

int32_t JsDocument::HandleTouchEvent(Event *e)
{
	uint16_t event_id = (uint16_t)e->GetType();

	if (event_id == 0x41)
		OnTouchDown(e);
	else if (event_id == 0x42)
		OnTouchUp();
	else if (event_id == 0x43)
		OnTouchMove();

	return Widget::HandleTouchEvent(e);
}

int32_t JsDocument::OnSingleTap()
{
	if (m_renderer == NULL)
		return 0;

	HitInfo hit;

	if (m_env->GetHitStack(&hit, this) != 0)
		return 0;

	int32_t pos_x = (int32_t)hit.m_touch_pos.extract_x();
	int32_t pos_y = (int32_t)hit.m_touch_pos.extract_y();

	ForwardMouseEvent(0, pos_x, pos_y);
	int32_t ret = ForwardMouseEvent(1, pos_x, pos_y);

	if (m_map_core != NULL) {
		MapViewListener *listener = m_map_core->GetViewListener();

		if (listener != NULL)
			listener->OnSingleTap(m_map_core->GetMapView());
	}
	return ret;
}

void JsDocument::ConvertToCssCoord(const MapPixelPoint *in_pos, MapPixelPoint *out_css)
{
	math::v4 size = *GetSize(GET_CURRENT);

	out_css->m_x = (int32_t)((float)in_pos->m_x + size.extract_x() * 0.5f);
	out_css->m_y = (int32_t)(-(float)in_pos->m_y + size.extract_y() * 0.5f);
}

int32_t JsDocument::DispatchZoomGesture(int32_t zoom_action, int32_t zoom_level,
					const MapPixelPoint *old_center,
					const MapPixelPoint *new_center)
{
	MapPixelPoint css_old = { 0, 0 };
	MapPixelPoint css_new = { 0, 0 };

	ConvertToCssCoord(old_center, &css_old);
	ConvertToCssCoord(new_center, &css_new);

	if (zoom_action == 0 || zoom_action == 2)
		return m_map_core->ZoomInAt(zoom_level, &css_old, &css_new);
	if (zoom_action == 1)
		return m_map_core->ZoomOutAt(zoom_level, &css_old, &css_new);
	return zoom_action;
}

int32_t JsDocument::CommitPinchZoom()
{
	float cur_width = GetSize(GET_CURRENT)->extract_x();
	float scale_ratio = cur_width / 960.0f;
	int32_t current_zoom = 0;

	GetSize(GET_CURRENT);
	m_map_core->GetZoomLevel(&current_zoom);

	float next_zoom = scale_ratio;

	if (scale_ratio >= 1.0f) {
		if (scale_ratio > 1.0f)
			next_zoom = (float)current_zoom + scale_ratio;
	} else {
		next_zoom = (float)current_zoom - (1.0f / scale_ratio);
	}

	MapPixelPoint old_center;
	old_center.m_x = m_pinch_origin_x;
	old_center.m_y = m_pinch_origin_y;

	MapPixelPoint new_center = old_center;
	TouchInfo *first_touch = &m_env->m_touch_state[0][0];
	TouchInfo *second_touch = &m_env->m_touch_state[0][1];

	if (first_touch != NULL && second_touch != NULL) {
		new_center.m_x = (int32_t)((first_touch->m_pos.extract_x() +
					    second_touch->m_pos.extract_x()) * 0.5f);
		new_center.m_y = (int32_t)((first_touch->m_pos.extract_y() +
					    second_touch->m_pos.extract_y()) * 0.5f);
	}

	if ((int32_t)next_zoom == current_zoom)
		return DispatchZoomGesture(2, (int32_t)next_zoom, &old_center, &new_center);
	return DispatchZoomGesture(1, (int32_t)next_zoom, &old_center, &new_center);
}

int32_t JsDocument::OnPinchEnd()
{
	if (m_renderer != NULL) {
		CommitPinchZoom();

		HitInfo hit;

		if (m_env->GetHitStack(&hit, this) == 0) {
			MapPixelPoint pt;

			pt.m_x = (int32_t)hit.m_touch_pos.extract_x();
			pt.m_y = (int32_t)hit.m_touch_pos.extract_y();
			if (m_map_core != NULL) {
				MapViewListener *listener = m_map_core->GetViewListener();

				if (listener != NULL)
					listener->OnPinchEnd(m_map_core->GetMapView(), &pt);
			}
		}

		WebRect visible_rect;
		WebRect rendered_rect;

		UpdateVisibleRect(&visible_rect);
		UpdateRenderedRect(visible_rect, &rendered_rect);
	}
	return 0;
}

int32_t JsDocument::OnDragEnd(Event *e)
{
	uint32_t touch_index = e->GetDetail() & 0xFF;

	if (touch_index >= 8)
		return 0;

	TouchInfo *touch = &m_env->m_touch_state[0][touch_index];

	if (touch == NULL)
		return 0;

	int32_t pos_x = (int32_t)touch->m_pos.extract_x();
	int32_t pos_y = (int32_t)touch->m_pos.extract_y();

	ForwardMouseEvent(2, pos_x, pos_y);
	int32_t ret = ForwardMouseEvent(1, pos_x, pos_y);

	if (m_map_core != NULL) {
		MapViewListener *listener = m_map_core->GetViewListener();

		if (listener != NULL) {
			MapPixelPoint pt;

			pt.m_x = pos_x;
			pt.m_y = pos_y;
			listener->OnTouchEnd(m_map_core->GetMapView(), &pt, m_touch_user_data);
		}
	}
	m_touch.ComputeDelta(&m_touch.m_delta_x, &m_touch.m_delta_y);
	return ret;
}

int32_t JsDocument::OnDragMove()
{
	TouchInfo *touch = &m_env->m_touch_state[0][0];

	if (touch != NULL) {
		m_touch.m_phase = 5;
		m_touch.m_drag_x = (int32_t)touch->m_pos.extract_x();
		m_touch.m_drag_y = (int32_t)touch->m_pos.extract_y();
	}

	WebRect visible_rect;

	UpdateVisibleRect(&visible_rect);
	return 0;
}

int32_t JsDocument::OnDragBegin()
{
	HitInfo hit;

	if (m_env->GetHitStack(&hit, this) != 0)
		return 0;

	int32_t pos_x = (int32_t)hit.m_touch_pos.extract_x();
	int32_t pos_y = (int32_t)hit.m_touch_pos.extract_y();

	m_touch.m_drag_start_x = pos_x;
	m_touch.m_drag_start_y = pos_y;
	return ForwardMouseEvent(2, pos_x, pos_y);
}

int32_t JsDocument::HandleGestureEvent(Event *e)
{
	uint32_t value = e->GetDetail();
	uint32_t port = (value >> 20) & 0xF;
	uint8_t gesture = (uint8_t)((value >> 8) & 0xFF);

	if (port == 0) {
		switch (gesture) {
		case 0:
			OnSingleTap();
			break;
		case 13:
			m_dragging = true;
			OnDragBegin();
			break;
		case 14:
			OnDragMove();
			break;
		case 15:
			m_dragging = false;
			OnDragEnd(e);
			break;
		case 19: {
			MapPixelPoint pt;

			pt.m_x = m_touch.m_down_x;
			pt.m_y = m_touch.m_down_y;
			m_pinching = true;
			if (m_map_core != NULL) {
				MapViewListener *listener = m_map_core->GetViewListener();

				if (listener != NULL)
					listener->OnTouchBegin(m_map_core->GetMapView(), &pt,
							       m_touch_user_data);
			}
			break;
		}
		case 20:
			if (m_renderer != NULL) {
				WebRect visible_rect;
				WebRect rendered_rect;

				UpdateVisibleRect(&visible_rect);
				UpdateRenderedRect(visible_rect, &rendered_rect);
			}
			break;
		case 21:
			m_pinching = false;
			OnPinchEnd();
			break;
		default:
			break;
		}
	}

	return Widget::HandleGestureEvent(e);
}

int32_t JsDocument::OnDocumentSizeChanged(math::v2 size)
{
	float scale = 1.0f;

	if (m_parent != NULL) {
		GetSize(GET_CURRENT);
		m_parent->GetSize(GET_CURRENT);
		m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		float parent_w = m_parent->GetSize(GET_CURRENT)->extract_x();

		if (size.extract_x() != parent_w)
			scale = parent_w / size.extract_x();
	}

	math::v4 cur_size = *GetSize(GET_CURRENT);
	math::v4 scaled(size.extract_x() * scale, size.extract_y() * scale,
			cur_size.extract_z() * scale, cur_size.extract_w() * scale);

	SetSize(scaled, NULL, ANIM_SIZE, 0, 0.0f, NULL);
	UpdateSize();

	if (m_parent != NULL) {
		math::v4 self_size = *GetSize(GET_CURRENT);

		SetPinchLimitMax(math::v4(self_size.extract_x() * 4.0f,
					  self_size.extract_y() * 4.0f,
					  self_size.extract_z() * 4.0f, 0.0f));

		float parent_w = m_parent->GetSize(GET_CURRENT)->extract_x();

		if (parent_w == GetSize(GET_CURRENT)->extract_x()) {
			math::v4 min_s = *GetSize(GET_CURRENT);

			SetPinchLimitMin(min_s);
		} else {
			math::v4 cur_s = *GetSize(GET_CURRENT);
			float ratio = m_parent->GetSize(GET_CURRENT)->extract_x() / size.extract_x();

			SetPinchLimitMin(math::v4(cur_s.extract_x() * ratio,
						  cur_s.extract_y() * ratio,
						  cur_s.extract_z() * ratio, 0.0f));
		}
	} else {
		math::v4 self_size = *GetSize(GET_CURRENT);

		SetPinchLimitMax(math::v4(self_size.extract_x() * 4.0f,
					  self_size.extract_y() * 4.0f,
					  self_size.extract_z() * 4.0f, 0.0f));
		SetPinchLimitMin(math::v4(self_size.extract_x() * 0.25f,
					  self_size.extract_y() * 0.25f,
					  self_size.extract_z() * 0.25f, 0.0f));
	}

	return 0;
}

int32_t JsDocument::ResetScrollPosition()
{
	if (m_parent != NULL) {
		math::v4 self_size = *GetSize(GET_CURRENT);
		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		math::v4 diff = self_size - parent_size;

		m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		math::v4 target_pos(diff.extract_x() * 0.5f, diff.extract_y() * -0.5f,
				    diff.extract_z(), diff.extract_w());
		m_parent->SetPos(target_pos, NULL, ANIM_MOVE, 0, 0.0f, NULL);

		WebRect visible_rect;
		WebRect rendered_rect;

		UpdateVisibleRect(&visible_rect);
		UpdateRenderedRect(visible_rect, &rendered_rect);
	}
	return 0;
}

void JsDocument::SetMapBlitEnabled(bool enabled)
{
	graph::WebObj *map_obj = NULL;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin();
	     it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == 1)
			map_obj = static_cast<graph::WebObj *>(it->m_obj.get());
	}
	if (map_obj != NULL)
		map_obj->SetBlitEnabled(enabled);
}

int32_t JsDocument::OnSurfaceReset()
{
	SetMapBlitEnabled(true);
	ResetScrollPosition();
	return (int32_t)0x80AF0001;
}

int32_t JsDocument::AttachDrawObj(graph::DrawObj *obj, int32_t obj_id)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (obj != NULL) {
		common::SharedPtr<graph::DrawObj> shared_obj(obj);
		DrawObjAttr attr((uint32_t)obj_id);

		attr.m_flag1 = false;
		m_drawobjs.push_back(DrawObjEntry(shared_obj, attr));
		ret = 0;
	}

	if (obj_id == 2 && m_parent != NULL) {
		m_bg_loader->Load();
		intrusive_ptr<graph::Surface> bg_surf = m_bg_loader->GetSurface();

		obj->SetTexture(bg_surf, 0);

		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		math::v3 obj_size = obj->m_size;

		obj->SetSize(math::v3(parent_size.extract_x(),
				      parent_size.extract_y(),
				      obj_size.extract_z()));
	}

	return ret;
}

int32_t JsDocument::StartRender()
{
	return m_renderer->ResumeSurface();
}

}
}
