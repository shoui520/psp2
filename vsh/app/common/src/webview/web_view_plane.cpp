/*
 * The view plane ("view_plane"), the widget a web view's page is drawn on, and the render plane
 * that draws its tiles. Sony's file name is not known (the module has no debug records); this
 * one and every name in it but the widget type are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <widget/core/environment.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <std/math.h>
#include <std/stdlib.h>
#include <inputdevice/touchscreen/touchscreen.h>
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

/* the touches smoothed by a WebViewTouchListener, and the indicator's angle taken from where the
   touch is (names ours; both set and never changed) */
bool s_angleFromCorner = true;
bool s_smoothTouch = true;
static WebViewTouchIndicator *s_indicator;

/* the touch panel's gesture listener of the priority for the widget's touch, and one attached
   to the touch (inline in Sony's; names ours) */
static inline common::SharedPtr<inputdevice::GestureListener> GetTouchListener(Widget *widget,
	uint32_t port, uint32_t id, int32_t priority)
{
	inputdevice::InputGestureListener *input =
		static_cast<inputdevice::InputGestureListener *>(widget->m_env->GetTouchGestureListener(port).get());

	if (input == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();
	return input->GetGestureListener(id, priority, widget);
}

static inline int32_t AttachTouchListener(Widget *widget, uint32_t port, uint32_t id,
	uint32_t priority, common::SharedPtr<inputdevice::GestureListener> listener)
{
	inputdevice::InputGestureListener *input =
		static_cast<inputdevice::InputGestureListener *>(widget->m_env->GetTouchGestureListener(port).get());

	return input != NULL ? input->AttachGestureListener(id, priority, listener) : -1;
}

static void OnViewTouch(int32_t type, Handler *self, Event *e, void *data);
static void OnViewInput(int32_t type, Handler *self, Event *e, void *data);

WebViewRenderPlane::WebViewRenderPlane(Widget *parent, WebViewViewPlane *viewPlane)
	: Plane(parent, NULL), m_root(NULL), m_rootEnv(NULL), m_camera(NULL), m_viewPlane(NULL),
	  m_reserved(0), m_pressed(false), m_pressCount(0), m_indicator(NULL), m_enabled(true)
{
	inputdevice::touchscreen::DeviceInfo info;

	m_reserved2[0] = 0;
	m_reserved2[1] = 0;
	inputdevice::touchscreen::GetDeviceInfo(0, &info);
	m_root = parent;
	m_rootEnv = parent->m_env;
	m_pixelPerMm = info.pixel_per_mm;
	m_camera = parent->m_scene->m_camera;
	m_viewPlane = viewPlane;

	Widget *indicatorParent = m_rootEnv->m_root_widget;

	if (s_indicator == NULL)
		s_indicator = new WebViewTouchIndicator(indicatorParent, NULL);
	m_indicator = s_indicator;
	if (m_indicator != NULL)
		m_indicator->SetEnabled(true);
	viewPlane->m_hit_check = 3;
	viewPlane->AddEventListener(0x1000B, new EventCBListener(OnViewTouch, this));
	viewPlane->AddEventListener(0x1000E, new EventCBListener(OnViewTouch, this));
	viewPlane->AddEventListener(0x20103, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x20104, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x20105, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x20106, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x2010D, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x2010E, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x2010F, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x20110, new EventCBListener(OnViewInput, this));
	viewPlane->AddEventListener(0x20111, new EventCBListener(OnViewInput, this));
}

WebViewRenderPlane::~WebViewRenderPlane()
{
	if (m_indicator != NULL)
		m_indicator = NULL;
}

int32_t WebViewRenderPlane::OnDraw(graph::DrawInfo &info)
{
	if (m_env->m_reserved_flag_c[1]) {
		{
			intrusive_ptr<graph::Surface> a = m_env->m_reserved_surface_a;
			SceFColor color = m_env->m_reserved_color_a;

			if (m_indicator != NULL)
				m_indicator->SetSurfaceA(a, color);
		}
		{
			intrusive_ptr<graph::Surface> c = m_env->m_reserved_surface_c;

			if (m_indicator != NULL) {
				SceFColor touchColor = { 0.5f, 0.732f, 0.5f, 0.05f };

				m_indicator->SetSurfaceC(c, touchColor);
			}
		}
		{
			intrusive_ptr<graph::Surface> b = m_env->m_reserved_surface_b;

			if (m_indicator != NULL) {
				SceFVector2 size = { 0.5f, 0.5f };

				m_indicator->SetSurfaceB(b, size);
			}
		}
		m_env->m_reserved_flag_c[1] = false;
	}
	return Widget::OnDraw(info);
}

/* the widget shown, or hidden, at once (inlined in Sony's code) */
static inline void ShowNow(Widget *widget)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
}

static inline void HideNow(Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

int32_t WebViewRenderPlane::Show()
{
	if (m_indicator != NULL)
		ShowNow(m_indicator);
	ShowNow(this);
	return 1;
}

int32_t WebViewRenderPlane::Hide()
{
	if (m_indicator != NULL)
		HideNow(m_indicator);
	HideNow(this);
	return 1;
}

void WebViewRenderPlane::OnTouch(uint32_t port, uint32_t id, Handler *handler)
{
	Widget *widget = (Widget *)handler;

	if (!m_env->m_reserved_flag_c[0] || !m_enabled)
		return;

	common::SharedPtr<inputdevice::GestureListener> listener =
		GetTouchListener(widget, port, id, 55);

	if (s_smoothTouch) {
		listener = GetTouchListener(widget, port, id, 58);
		if (listener.get() == NULL) {
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new WebViewTouchListener(port, id, widget));
			if (listener.get() != NULL &&
			    AttachTouchListener(widget, port, id, 58, listener) != 0)
				sce_paf_printf_dummy();
		}
	}
	if (m_env->m_reserved_word_a != 0) {
		listener = GetTouchListener(widget, port, id, 56);
		if (listener.get() == NULL) {
			for (uint32_t i = 0; i < 8; i++) {
				listener = GetTouchListener(widget, port, i, 56);
				if (listener.get() != NULL) {
					AttachTouchListener(widget, port, id, 56, listener);
					break;
				}
			}
		}
		if (listener.get() == NULL && ScePafWidget_54DFB062(widget, port) == 1) {
			listener = common::SharedPtr<inputdevice::GestureListener>(
				new WebViewMultiTouchListener(port, id, widget));

			WebViewMultiTouchListener *multi = (WebViewMultiTouchListener *)listener.get();

			if (multi != NULL) {
				multi->SetTouchCountMax(3);
				if (AttachTouchListener(widget, port, id, 56, listener) != 0)
					sce_paf_printf_dummy();
				multi->m_gesture[0] = 40;
				multi->m_gesture[1] = 41;
				multi->m_gesture[2] = 42;
				multi->m_gesture[3] = 43;
				multi->m_gesture[4] = 44;
			}
		}
	}
}

/* the indicator's angle in degrees for a touch at the place, its view on the side (0 left,
   1 right): towards it from below the screen's bottom corner, or 35 degrees each way (name
   ours) */
static float CalcIndicatorAngle(Widget *widget, uint32_t side, float x, float y)
{
	float angle = 90.0f;

	if (s_angleFromCorner) {
		if (side == 0) {
			Camera *camera = widget->GetCamera();

			ScePafWidget_72F479CA(camera);

			float dy = y + camera->m_screen_height * 0.5f;
			float dx = x + camera->m_screen_width * 0.5f;
			float d = dy + dy * 1.3f;
			float r;

			if (x < 0.0f)
				r = sce_paf_atan2f(dx, d);
			else
				r = sce_paf_atan2f(dx - x, d);
			angle = -r / 3.1415927f * 180.0f;
		}
		if (side == 1) {
			Camera *camera = widget->GetCamera();

			ScePafWidget_72F479CA(camera);

			float dy = y + camera->m_screen_height * 0.5f;
			float dx = x - camera->m_screen_width * 0.5f;
			float d = dy + dy * 1.3f;
			float r;

			if (x < 0.0f)
				r = sce_paf_atan2f(dx - x, d);
			else
				r = sce_paf_atan2f(dx, d);
			angle = -r / 3.1415927f * 180.0f;
		}
	} else {
		if (side == 0)
			angle = -35.0f;
		if (side == 1)
			angle = 35.0f;
	}
	return angle;
}

/* a press, move, release or flick on the view plane: the indicator shown there and moved, and
   the widget's touch gestures sent the place of its tip */
static void OnViewInput(int32_t type, Handler *self, Event *e, void *data)
{
	WebViewRenderPlane *plane = (WebViewRenderPlane *)data;
	Widget *widget = (Widget *)self;
	uint32_t port = (e->m_detail >> 20) & 0xF;
	uint32_t id = e->m_data[0];
	uint32_t side = e->m_data[1];
	float vx;
	float vy;
	float x;
	float y;

	ScePafMisc_88F4A40B(port, id, 200, &vx, &vy);
	vx = vx * 1000.0f;
	vy = vy * 1000.0f;
	if (s_smoothTouch) {
		common::SharedPtr<inputdevice::GestureListener> listener =
			GetTouchListener(widget, port, id, 58);
		WebViewTouchListener *touch = (WebViewTouchListener *)listener.get();

		if (touch != NULL) {
			x = touch->GetX();
			y = touch->GetY();
		} else {
			x = 0.0f;
			y = x;
			sce_paf_printf_dummy();
		}
	} else {
		const inputdevice::touchscreen::Data *touch;

		inputdevice::touchscreen::GetData(port, id, &touch);
		x = (float)touch->center_orig_x;
		y = (float)touch->center_orig_y;
	}

	float angle = CalcIndicatorAngle(plane, side, x, y);
	math::v2 tip = plane->m_indicator->GetTipOffset();
	int32_t tipX = (int32_t)tip.extract_x();
	int32_t tipY = (int32_t)tip.extract_y();

	switch ((e->m_detail >> 8) & 0x7F) {
	case 30:
	case 40:
		if (plane->m_indicator != NULL)
			plane->m_indicator->Begin(x, y, angle);
		plane->m_pressed = true;
		plane->m_pressCount++;
		break;
	case 32:
	case 44:
		plane->m_pressCount--;
		if (plane->m_pressCount <= 0) {
			plane->m_pressCount = 0;
			if (plane->m_indicator != NULL)
				plane->m_indicator->End(x, y);
			plane->m_pressed = false;
		}
		break;
	case 41: {
		if (plane->m_pressed)
			plane->m_indicator->Move(x, y, angle);

		common::SharedPtr<inputdevice::GestureListener> listener =
			GetTouchListener(widget, port, id, 56);

		if (listener.get() != NULL)
			ScePafWidget_C5939A92((TouchGestureListener *)listener.get(), 29, tipX, tipY,
					      0, 0, 0, 0);
		break;
	}
	case 43: {
		if (plane->m_env->m_reserved_flag_c[3] || __builtin_fabsf(vx) >= 80.0f ||
		    __builtin_fabsf(vy) >= 80.0f)
			break;

		common::SharedPtr<inputdevice::GestureListener> listener =
			GetTouchListener(widget, port, id, 56);

		if (listener.get() != NULL)
			ScePafWidget_C5939A92((TouchGestureListener *)listener.get(), 28, tipX, tipY,
					      0, 0, 0, 0);
		break;
	}
	}
}

void WebViewRenderPlane::OnUpdate()
{
}

static void OnViewTouch(int32_t type, Handler *self, Event *e, void *data)
{
	((WebViewRenderPlane *)data)->OnTouch((e->m_detail >> 20) & 0xF, e->m_detail & 0xFF, self);
}

const char WebViewViewPlane::m_widget_type[] = "view_plane";

/* three surfaces nothing uses (names ours) */
static intrusive_ptr<graph::Surface> s_surfaceA(NULL);
static intrusive_ptr<graph::Surface> s_surfaceB(NULL);
static intrusive_ptr<graph::Surface> s_surfaceC(NULL);

}
}
