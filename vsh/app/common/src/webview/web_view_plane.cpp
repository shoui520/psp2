/*
 * The view plane ("view_plane"), the widget a web view's page is drawn on, and the render plane
 * that draws its tiles. Sony's file name is not known (the module has no debug records); this
 * one and every name in it but the widget type are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <widget/core/environment.h>
#include <widget/w_scene.h>
#include "web_view.h"

namespace paf {
namespace ui {

bool WebViewViewPlane::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *WebViewViewPlane::GetTypeName() const
{
	return (char *)m_widget_type;
}

WebViewViewPlane::WebViewViewPlane(Widget *parent, CreateParam *param) : Widget(parent, param)
{
	m_hit_detect_b_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_find_mode = 0;
	m_view = NULL;
	m_event_enable |= 0x78D0;
	m_hit_check = 1;
	SetPinchMode(0x200);
	const char *type = GetTypeName();

	if (type == NULL)
		m_name.m_idhash = 0;
	else
		m_name.SetID(string(type));
	m_renderer = NULL;
	m_renderer = new WebViewRenderPlane(m_scene, this);
}

WebViewViewPlane::~WebViewViewPlane()
{
	if (m_renderer != NULL)
		m_renderer->DestroyWidget();
}

math::v2 WebViewViewPlane::GetLinePos()
{
	TouchInfo *touch;

	m_env->m_touch_state[0][0].m_port = 0;
	m_env->m_touch_state[0][0].m_id = 0;
	touch = &m_env->m_touch_state[0][0];
	if (touch == NULL)
		return math::v2(0.0f, 0.0f);

	HitInfo hit;

	sce_paf_memset(&hit, 0, sizeof(hit));
	CalcHitLineToPlane(touch->m_line_start, touch->m_line_end,
			   math::v4(0.0f, 0.0f, 0.0f, 0.0f), math::v4(0.0f, 0.0f, 1.0f, 0.0f), &hit);
	return hit.m_touch_pos;
}

int32_t WebViewViewPlane::HandleTouchEvent(Event *e)
{
	if (e != NULL && ((e->m_detail >> 20) & 0xF) == 0) {
		switch (e->GetType()) {
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x47: {
			WebView *view = m_view;
			math::v2 pos = GetTouchPos();

			view->Touch(pos, e->GetType(), e->m_detail & 0xFF);
			break;
		}
		}
	}
	return Widget::HandleTouchEvent(e);
}

int32_t WebViewViewPlane::HandleGestureEvent(Event *e)
{
	if (e != NULL && ((e->m_detail >> 20) & 0xF) == 0) {
		switch ((e->m_detail >> 8) & 0x7F) {
		case 0x00:
		case 0x02:
		case 0x04:
		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x19: {
			WebView *view = m_view;
			math::v2 pos = GetTouchPos();

			view->Touch(pos, (e->m_detail >> 8) & 0x7F, e->m_detail & 0xFF);
			break;
		}
		case 0x1C: {
			math::v2 delta((float)e->m_data[0], (float)e->m_data[1]);
			math::v2 pos = GetTouchPos();

			m_view->Touch(pos + delta, 0, e->m_detail & 0xFF);
			break;
		}
		case 0x1D: {
			math::v2 delta((float)e->m_data[0], (float)e->m_data[1]);
			math::v2 pos = GetTouchPos();

			m_view->Touch(pos + delta, 0x1D, e->m_detail & 0xFF);
			break;
		}
		}
	}
	return Widget::HandleGestureEvent(e);
}

void WebViewViewPlane::UpdateSize()
{
	m_view->UpdateZoomFocus();
}

const char WebViewViewPlane::m_widget_type[] = "view_plane";

}
}
