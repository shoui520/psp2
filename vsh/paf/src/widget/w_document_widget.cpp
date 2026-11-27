/*
 * paf::ui::DocumentWidget implementation for 2014 PAF WebViewBase.
 */
#include <widget/w_document_widget.h>
#include <graphics/drawobj/web_obj.h>
#include <graphics/effects/screen_copy.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <std/math.h>
#include <std/string.h>
#include <widget/core/environment.h>
#include <widget/web_renderer.h>

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::Matrix3;
using sce::Vectormath::Simd::Aos::Vector3;

const char DocumentWidget::m_widget_type[] = "document_widget";

const char *DocumentWidget::TypeName()
{
	return m_widget_type;
}

bool DocumentWidget::IsInherit(const char *type) const
{
	if (type == m_widget_type)
		return true;
	return Widget::IsInherit(type);
}

char *DocumentWidget::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

DocumentWidget::DocumentWidget(Widget *parent, CreateParam *param)
	: Widget(parent, param),
	  m_observer(NULL),
	  m_renderer(NULL),
	  m_pinching(false),
	  m_last_scroll_pos(0.0f, 0.0f),
	  m_out_of_bounds(false),
	  m_has_velocity(false),
	  m_surface_overflow(0.0f, 0.0f),
	  m_view_padding(0.0f, 0.0f),
	  m_rendered_rect(),
	  m_min_size(0.0f, 0.0f),
	  m_suspended(false),
	  m_suspend_enabled(false),
	  m_surface(),
	  m_screen_copy(NULL)
{
	EnableEvent(0xFFF0);

	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_check = 1;

	SetPinchMode(0x200);
	SetGestureMask(m_touch_gesture_mask | 1, true);
	SetGestureMask(m_touch_gesture_mask | 2, true);
	SetGestureMask(m_touch_gesture_mask | 4, true);

	m_name.SetID(string("document_widget"));
}

DocumentWidget::~DocumentWidget()
{
}

int32_t DocumentWidget::Initialize(WebCoreView *view, WebCoreViewport *viewport,
				   WebRenderClient **out_render_client, WebCoreFrame *frame,
				   WebSurfaceHost **out_surface_host)
{
	int32_t ret = (int32_t)0x80AF0001;

	m_renderer = new WebRenderer();
	if (m_renderer != NULL && viewport != NULL && frame != NULL && m_parent != NULL) {
		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		float init_w = parent_size.extract_x();
		float init_h = parent_size.extract_y();

		WebRect view_rect;
		view_rect.Set(0.0f, 0.0f, init_w, init_h);

		WebRect surface_rect;
		surface_rect.Set(0.0f, 0.0f, init_w, init_h);

		graph::WebObj *web_obj = new graph::WebObj(graph::PlaneObjStyle());
		ret = m_renderer->Initialize(this, math::v2(init_w, init_h),
					     surface_rect, view_rect,
					     (int32_t)init_w, (int32_t)init_h,
					     view, viewport, out_render_client,
					     frame, out_surface_host, web_obj);
	}
	return ret;
}

int32_t DocumentWidget::UpdateRenderedRect(WebRect visible_rect, WebRect *out_rect,
					   bool commit, bool skip_clear)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_renderer == NULL)
		return ret;

	Matrix3 doc_to_widget = m_renderer->m_doc_to_widget;
	Matrix3 widget_to_doc = m_renderer->m_widget_to_doc;

	Vector3 doc_tl = widget_to_doc *
			 Vector3(visible_rect.m_left, visible_rect.m_bottom, 1.0f);
	Vector3 doc_br = widget_to_doc *
			 Vector3(visible_rect.m_right, visible_rect.m_top, 1.0f);

	WebRect doc_tile_rect;
	Vector3 tile_span(0.0f, 0.0f, 0.0f);

	if (m_parent != NULL) {
		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		math::v2 tex_dims(parent_size.extract_x(), parent_size.extract_y());

		for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin();
		     it != m_drawobjs.end(); ++it) {
			if (it->m_attr.m_id == 1) {
				uint32_t tex_w = 0;
				uint32_t tex_h = 0;

				static_cast<graph::WebObj *>(it->m_obj.get())->GetTextureSize(&tex_w, &tex_h);
				tex_dims = math::v2((float)tex_w, (float)tex_h);
			}
		}

		Vector3 col0 = widget_to_doc.getCol0();
		Vector3 col1 = sce::Vectormath::Simd::Aos::absPerElem(widget_to_doc.getCol1());

		tile_span = col0 * tex_dims.extract_x() + col1 * tex_dims.extract_y();
	}

	float center_x = (doc_tl.getX() + doc_br.getX()) * 0.5f;
	float center_y = (doc_tl.getY() + doc_br.getY()) * 0.5f;

	doc_tile_rect.SetCenterSize(center_x, center_y, tile_span.getX(), tile_span.getY());

	WebRect doc_bounds;
	doc_bounds.Set(0.0f, 0.0f,
		       m_renderer->m_doc_size.extract_x(),
		       m_renderer->m_doc_size.extract_y());
	doc_tile_rect.ClampMin(doc_bounds);

	Vector3 widget_tl = doc_to_widget *
			    Vector3(doc_tile_rect.m_left, doc_tile_rect.m_bottom, 1.0f);
	Vector3 widget_br = doc_to_widget *
			    Vector3(doc_tile_rect.m_right, doc_tile_rect.m_top, 1.0f);

	WebRect widget_tile_rect;
	widget_tile_rect.Set(widget_tl.getX(), widget_tl.getY(),
			     widget_br.getX(), widget_br.getY());

	if (commit)
		m_rendered_rect = widget_tile_rect;

	ret = m_renderer->SetRenderedRect(widget_tile_rect, &doc_tile_rect, commit, skip_clear);
	*out_rect = widget_tile_rect;
	return ret;
}

bool DocumentWidget::CheckScrollNeedsRender(math::v2 velocity)
{
	bool needs_render = false;

	if (m_parent == NULL)
		return false;

	if (m_last_scroll_pos.extract_x() == 0.0f && m_last_scroll_pos.extract_y() == 0.0f) {
		math::v4 pos = m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		m_last_scroll_pos = math::v2(-pos.extract_x(), -pos.extract_y());
	}

	math::v4 parent_pos = m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 view_center(-parent_pos.extract_x(), -parent_pos.extract_y());
	math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
	math::rect view_rect(view_center, math::v2(parent_size.extract_x(), parent_size.extract_y()));

	math::v4 self_size = *GetSize(GET_CURRENT);
	math::v2 bounds_size(self_size.extract_x() + m_view_padding.extract_x() + 1.0f,
			     self_size.extract_y() + m_view_padding.extract_y() + 1.0f);
	math::rect bounds_rect(math::v2(0.0f, 0.0f), bounds_size);
	bounds_rect.move(math::v2(m_view_padding.extract_x() * 0.5f,
				  m_view_padding.extract_y() * 0.5f));

	if (!view_rect.is_in_rect(bounds_rect))
		m_out_of_bounds = true;

	bool in_bounds = view_rect.is_in_rect(bounds_rect);
	if (in_bounds && m_out_of_bounds) {
		m_out_of_bounds = false;
		needs_render = true;
	}

	if (!m_out_of_bounds) {
		float delta_y = view_center.extract_y() - m_last_scroll_pos.extract_y();
		if (delta_y < 0.0f)
			delta_y = -delta_y;
		if (delta_y > 10.0f) {
			needs_render = true;
			m_last_scroll_pos = view_center;
		}

		float delta_x = view_center.extract_x() - m_last_scroll_pos.extract_x();
		if (delta_x < 0.0f)
			delta_x = -delta_x;
		if (delta_x > 10.0f) {
			needs_render = true;
			m_last_scroll_pos = view_center;
		}
	}

	if (velocity.extract_x() == 0.0f && velocity.extract_y() == 0.0f) {
		if (m_has_velocity) {
			needs_render = true;
			m_has_velocity = false;
		}
	} else {
		m_has_velocity = true;
	}

	return needs_render;
}

int32_t DocumentWidget::UpdateVisibleRect(WebRect *out_rect, bool commit)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_parent == NULL)
		return ret;

	math::v4 parent_pos = m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 center(-parent_pos.extract_x(), -parent_pos.extract_y());
	math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);

	float half_pw = parent_size.extract_x() * 0.5f;
	float half_ph = parent_size.extract_y() * 0.5f;

	WebRect visible_rect;
	visible_rect.Set(center.extract_x() - half_pw,
			 center.extract_y() + half_ph,
			 center.extract_x() + half_pw,
			 center.extract_y() - half_ph);

	WebRect self_rect;
	float self_w = GetSize(GET_CURRENT)->extract_x();
	float self_h = GetSize(GET_CURRENT)->extract_y();

	self_rect.Set(self_w * -0.5f, self_h * 0.5f, self_w * 0.5f, self_h * -0.5f);
	visible_rect.ClampInside(self_rect);
	*out_rect = visible_rect;

	if (m_renderer != NULL) {
		WebRect doc_rect;
		ret = m_renderer->SetVisibleRect(visible_rect, &doc_rect, commit);
	}
	return ret;
}

int32_t DocumentWidget::SyncScrollPosition(math::v2 velocity, bool force_render)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_parent == NULL)
		return 0;

	math::v4 parent_pos = m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 view_center(-parent_pos.extract_x(), -parent_pos.extract_y());

	float parent_w = m_parent->GetSize(GET_CURRENT)->extract_x();
	float self_w = GetSize(GET_CURRENT)->extract_x();
	float self_h = GetSize(GET_CURRENT)->extract_y();
	float parent_h = m_parent->GetSize(GET_CURRENT)->extract_y();

	WebRect clamp_rect;
	clamp_rect.Set((parent_w - self_w) * 0.5f,
		       (self_h - parent_h) * 0.5f,
		       (self_w - parent_w) * 0.5f,
		       (parent_h - self_h) * 0.5f);

	float clamped_x = view_center.extract_x();
	if (clamped_x > clamp_rect.m_right)
		clamped_x = clamp_rect.m_right;
	if (clamped_x < clamp_rect.m_left)
		clamped_x = clamp_rect.m_left;

	float clamped_y = view_center.extract_y();
	if (clamped_y > clamp_rect.m_bottom)
		clamped_y = clamp_rect.m_bottom;
	if (clamped_y < clamp_rect.m_top)
		clamped_y = clamp_rect.m_top;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin();
	     it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id != 0) {
			graph::DrawObj *draw_obj = it->m_obj.get();
			math::v3 cur_pos = draw_obj->m_pos;

			draw_obj->SetPos(math::v3(clamped_x, clamped_y, cur_pos.extract_z()));
		}
	}

	WebRect visible_rect;
	WebRect rendered_rect;

	UpdateVisibleRect(&visible_rect, !m_pinching);

	bool scroll_dirty = CheckScrollNeedsRender(velocity) && !m_pinching;

	if (m_renderer != NULL)
		UpdateRenderedRect(visible_rect, &rendered_rect,
				   scroll_dirty || force_render, m_pinching);

	return ret;
}

int32_t DocumentWidget::SyncScrollPosition(math::v2 velocity)
{
	return SyncScrollPosition(velocity, false);
}

int32_t DocumentWidget::GetViewportSize(math::v2 *out_size)
{
	if (m_renderer != NULL && m_parent != NULL)
		m_parent->GetSize(GET_CURRENT);
	return (int32_t)0x80AF0001;
}

int32_t DocumentWidget::DocRectToWidgetRect(math::v2 top_left, math::v2 bottom_right,
					    math::v2 *out_points)
{
	Vector3 mapped_tl = m_renderer->m_doc_to_widget *
			    Vector3(top_left.extract_x(), top_left.extract_y(), 1.0f);
	Vector3 mapped_br = m_renderer->m_doc_to_widget *
			    Vector3(bottom_right.extract_x(), bottom_right.extract_y(), 1.0f);

	out_points[0] = math::v2(mapped_tl.getX(), mapped_tl.getY());
	out_points[1] = math::v2(mapped_br.getX(), mapped_br.getY());
	return 0;
}

int32_t DocumentWidget::WidgetRectToDocRect(math::v2 top_left, math::v2 bottom_right,
					    math::v2 *out_points)
{
	Vector3 mapped_tl = m_renderer->m_widget_to_doc *
			    Vector3(top_left.extract_x(), top_left.extract_y(), 1.0f);
	Vector3 mapped_br = m_renderer->m_widget_to_doc *
			    Vector3(bottom_right.extract_x(), bottom_right.extract_y(), 1.0f);

	out_points[0] = math::v2(mapped_tl.getX(), mapped_tl.getY());
	out_points[1] = math::v2(mapped_br.getX(), mapped_br.getY());
	return 0;
}

void DocumentWidget::ResumeSurface()
{
	if (m_suspended && m_suspend_enabled && m_renderer != NULL) {
		m_renderer->ResumeSurface();
		m_suspended = false;
	}
}

void DocumentWidget::SuspendSurface()
{
	if (!m_suspended && m_suspend_enabled && m_renderer != NULL) {
		m_renderer->SuspendSurface();
		m_suspended = true;
	}
}

void DocumentWidget::SetZoomState(int32_t state)
{
	graph::WebObj *web_obj = NULL;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin();
	     it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == 1)
			web_obj = static_cast<graph::WebObj *>(it->m_obj.get());
	}
	if (web_obj != NULL)
		web_obj->SetZoomState(state);
}

void DocumentWidget::SetBlitEnabled(bool enabled)
{
	graph::WebObj *web_obj = NULL;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin();
	     it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == 1)
			web_obj = static_cast<graph::WebObj *>(it->m_obj.get());
	}
	if (web_obj != NULL)
		web_obj->SetBlitEnabled(enabled);
}

void DocumentWidget::DestroyWidget()
{
	if (m_surface.get() != NULL)
		m_surface.clear();
	graph::ScreenCopy::DeleteObj(m_screen_copy);
	if (m_renderer != NULL) {
		m_renderer->Finalize();
		m_renderer = NULL;
	}
	m_observer = NULL;
	Widget::DestroyWidget();
}

void DocumentWidget::UpdateSize()
{
	if (m_renderer == NULL)
		return;

	math::v4 cur_size = *GetSize(GET_CURRENT);
	if (cur_size.extract_x() < m_min_size.extract_x() ||
	    cur_size.extract_y() < m_min_size.extract_y()) {
		math::v4 clamped_size(m_min_size.extract_x(), m_min_size.extract_y(),
				      cur_size.extract_z(), cur_size.extract_w());
		SetSize(clamped_size, NULL, ANIM_SIZE, 0, 0.0f, NULL);
	}

	math::v4 final_size = *GetSize(GET_CURRENT);
	m_renderer->SetWidgetSize(math::v2(final_size.extract_x(), final_size.extract_y()));
}

math::v2 DocumentWidget::CalcCurrentTouchPos()
{
	TouchInfo *touch = &m_env->m_touch_state[0][0];

	if (touch != NULL) {
		HitInfo hit;
		math::v4 origin(0.0f, 0.0f, 0.0f, 0.0f);
		math::v4 normal(0.0f, 0.0f, 1.0f, 0.0f);

		CalcHitLineToPlane(touch->m_line_start, touch->m_line_end, origin, normal, &hit);
		return hit.m_touch_pos;
	}
	return math::v2(0.0f, 0.0f);
}

math::v2 DocumentWidget::GetTouchPoint()
{
	HitInfo hit;

	if (m_env->GetHitStack(&hit, this) != 0 || hit.m_port != 0)
		return CalcCurrentTouchPos();
	return hit.m_touch_pos;
}

int32_t DocumentWidget::HandleTouchEvent(Event *e)
{
	uint16_t event_id = (uint16_t)e->GetType();

	if (event_id == 0x41) {
		if (m_observer != NULL)
			m_observer->OnDocumentEvent(65, GetTouchPoint());
	} else if (event_id == 0x42) {
		SyncScrollPosition(math::v2(0.0f, 0.0f), true);
		if (m_observer != NULL)
			m_observer->OnDocumentEvent(66, GetTouchPoint());
	}
	return Widget::HandleTouchEvent(e);
}

int32_t DocumentWidget::HandleGestureEvent(Event *e)
{
	uint32_t value = e->GetDetail();
	uint32_t port = (value >> 20) & 0xF;
	uint8_t gesture = (uint8_t)((value >> 8) & 0xFF);

	if (port == 0) {
		switch (gesture) {
		case 0:
			if (m_observer != NULL)
				m_observer->OnDocumentEvent(0, GetTouchPoint());
			if (m_renderer != NULL) {
				int32_t touch_x = (int32_t)GetTouchPoint().extract_x();
				int32_t touch_y = (int32_t)GetTouchPoint().extract_y();

				m_renderer->HandleMouseEvent(0, touch_x, touch_y, 1, 0, 0, 0);
				m_renderer->HandleMouseEvent(1, touch_x, touch_y, 1, 0, 0, 0);
			}
			break;
		case 2:
			if (m_observer != NULL)
				m_observer->OnDocumentEvent(2, GetTouchPoint());
			break;
		case 4:
			if (m_observer != NULL)
				m_observer->OnDocumentEvent(4, GetTouchPoint());
			break;
		case 12:
		case 15:
			SyncScrollPosition(math::v2(0.0f, 0.0f), true);
			break;
		case 13:
			if (m_observer != NULL)
				m_observer->OnDocumentEvent(13, GetTouchPoint());
			break;
		case 19:
			m_pinching = true;
			break;
		case 21:
			m_pinching = false;
			SyncScrollPosition(math::v2(0.0f, 0.0f), true);
			break;
		case 25:
			if (m_observer != NULL)
				m_observer->OnDocumentEvent(25, GetTouchPoint());
			break;
		default:
			break;
		}
	} else if (port == 1) {
		if (gesture == 13) {
			if (m_observer != NULL)
				m_observer->OnDocumentEvent(13, GetTouchPoint());
		} else if (gesture == 15) {
			if (m_observer != NULL)
				m_observer->OnDocumentEvent(15, GetTouchPoint());
		}
	}

	return Widget::HandleGestureEvent(e);
}

int32_t DocumentWidget::ScrollParentToOffset(math::v2 offset)
{
	if (m_parent != NULL) {
		math::v4 self_size = *GetSize(GET_CURRENT);
		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		math::v4 diff = self_size - parent_size;

		m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		math::v4 target_pos(diff.extract_x() * 0.5f - offset.extract_x(),
				    diff.extract_y() * -0.5f - offset.extract_y(),
				    diff.extract_z(),
				    diff.extract_w());
		m_parent->SetPos(target_pos, NULL, ANIM_MOVE, 0, 0.0f, NULL);
		SyncScrollPosition(math::v2(0.0f, 0.0f), true);
	}
	return 0;
}

int32_t DocumentWidget::OnDocumentSizeChanged(math::v2 size)
{
	float scale = 1.0f;

	if (m_parent != NULL) {
		GetSize(GET_CURRENT);
		m_parent->GetSize(GET_CURRENT);
		m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		float parent_w = m_parent->GetSize(GET_CURRENT)->extract_x();
		float fit_w = parent_w;

		if (size.extract_x() >= 1280.0f) {
			fit_w = 1280.0f;
		} else if (parent_w < size.extract_x()) {
			fit_w = size.extract_x();
		}

		if (fit_w != parent_w)
			scale = parent_w / fit_w;
	}

	math::v2 scaled_size(size.extract_x() * scale, size.extract_y() * scale);
	math::v4 cur_size = *GetSize(GET_CURRENT);

	m_min_size = scaled_size;
	SetSize(math::v4(scaled_size.extract_x(), scaled_size.extract_y(),
			 cur_size.extract_z(), cur_size.extract_w()),
		NULL, ANIM_SIZE, 0, 0.0f, NULL);
	UpdateSize();
	ScrollParentToOffset(math::v2(0.0f, 0.0f));

	if (m_parent != NULL) {
		math::v2 max_surf = m_renderer->m_max_surface_size;
		math::v2 clamped(size.extract_x() < max_surf.extract_x() ? size.extract_x() : max_surf.extract_x(),
				 size.extract_y() < max_surf.extract_y() ? size.extract_y() : max_surf.extract_y());
		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		math::v4 max_pinch(size.extract_x() * 4.0f, size.extract_y() * 4.0f,
				   parent_size.extract_z() * 4.0f, parent_size.extract_w() * 4.0f);

		SetPinchLimitMax(max_pinch);
		SetPinchLimitMin(math::v4(m_min_size.extract_x(), m_min_size.extract_y(),
					  parent_size.extract_z(), 0.0f));

		math::v4 cur_parent_size = *m_parent->GetSize(GET_CURRENT);
		float over_x = clamped.extract_x() - cur_parent_size.extract_x();
		float over_y = clamped.extract_y() - cur_parent_size.extract_y();

		if (over_x < 0.0f)
			over_x = 0.0f;
		if (over_y < 0.0f)
			over_y = 0.0f;
		m_surface_overflow = math::v2(over_x, over_y);
	} else {
		math::v4 base_size = *GetSize(GET_CURRENT);

		SetPinchLimitMax(math::v4(base_size.extract_x() * 4.0f,
					  base_size.extract_y() * 4.0f,
					  base_size.extract_z() * 4.0f, 0.0f));
		SetPinchLimitMin(math::v4(base_size.extract_x() * 0.25f,
					  base_size.extract_y() * 0.25f,
					  base_size.extract_z() * 0.25f, 0.0f));
	}

	return 0;
}

int32_t DocumentWidget::ResetScrollAndRect()
{
	if (m_parent != NULL) {
		math::v4 self_size = *GetSize(GET_CURRENT);
		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		math::v4 diff = self_size - parent_size;

		m_parent->GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 target_pos(diff.extract_x() * 0.5f, diff.extract_y() * -0.5f,
				    diff.extract_z(), diff.extract_w());
		m_parent->SetPos(target_pos, NULL, ANIM_MOVE, 0, 0.0f, NULL);

		m_rendered_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
		SyncScrollPosition(math::v2(0.0f, 0.0f), true);
	}
	return 0;
}

int32_t DocumentWidget::OnSurfaceReset()
{
	int32_t ret = (m_renderer != NULL) ? 0 : (int32_t)0x80AF0001;

	SetZoomState(0);
	ResetScrollAndRect();
	return ret;
}

void DocumentWidget::CreateCheckerSurface(intrusive_ptr<graph::Surface> &out_surface)
{
	graph::SurfacePool *pool = gutil::GetDefaultSurfacePool();
	intrusive_ptr<graph::Surface> surf(new graph::Surface(pool, 16, 16,
							      ImageMode_RGBA8888,
							      ImageOrder_Linear,
							      1, 1, true));
	int32_t width = surf->GetWidth();
	int32_t height = surf->GetHeight();
	uint32_t *pixels = static_cast<uint32_t *>(sce_paf_malloc(4 * width * height));

	for (int32_t row = 0; row < height; ++row) {
		for (int32_t col = 0; col < width; ++col) {
			bool dark = (((row & 8) == 0) ^ ((col & 8) == 0));
			pixels[row * width + col] = dark ? 0xFFBEBEBEU : 0xFFD1D1D1U;
		}
	}

	surf->Copy(0, pixels, ImageOrder_Linear, 0, -1);
	sce_paf_free(pixels);
	out_surface = surf;
}

int32_t DocumentWidget::AttachDrawObj(graph::DrawObj *obj, int32_t obj_id)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (obj != NULL) {
		common::SharedPtr<graph::DrawObj> shared_obj(obj);
		DrawObjAttr attr((uint32_t)obj_id);

		attr.m_flag1 = false;
		m_drawobjs.push_back(DrawObjEntry(shared_obj, attr));
		ret = 0;
	}

	if (obj_id == 1) {
		graph::PlaneObj *zoom_plane = static_cast<graph::WebObj *>(obj)->GetZoomPlane();
		if (zoom_plane != NULL)
			AttachDrawObj(zoom_plane, 0);
	}

	if (obj_id == 2 && m_parent != NULL) {
		intrusive_ptr<graph::Surface> checker_surface;

		CreateCheckerSurface(checker_surface);
		obj->SetTexture(checker_surface, 0);

		math::v4 parent_size = *m_parent->GetSize(GET_CURRENT);
		math::v3 obj_size = obj->m_size;

		obj->SetSize(math::v3(parent_size.extract_x(),
				      parent_size.extract_y(),
				      obj_size.extract_z()));
	}

	return ret;
}

void DocumentWidget::SetMaxSurfaceSize(math::v2 max_size)
{
	float clamped_x = max_size.extract_x() < 1024.0f ? max_size.extract_x() : 1024.0f;
	float clamped_y = max_size.extract_y() < 1024.0f ? max_size.extract_y() : 1024.0f;

	m_suspend_enabled = true;
	m_renderer->SetMaxSurfaceSize(math::v2(clamped_x, clamped_y));
}

}
}
