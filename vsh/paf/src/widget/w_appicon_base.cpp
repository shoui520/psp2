/*
 * paf::ui::AppIconBase (DEBUGGER 0x810d8cc4..0x810da97c): an application's icon over its
 * label, the label's height kept below the icon's size.
 */
#include <system/system.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/core/environment.h>
#include <widget/core/widget_gesture.h>
#include <widget/w_appicon_base.h>

namespace paf {
namespace ui {

const char AppIconBase::m_widget_type[] = "app_icon_base";

bool AppIconBase::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *AppIconBase::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *AppIconBase::TypeName()
{
	return m_widget_type;
}

math::v4 AppIconBase::CalcIconSize(const math::v4 &size) const
{
	float height = size.extract_y() + m_text_obj->m_size.extract_y();
	math::v4 widget_size = size;

	widget_size.set_y(height);
	return widget_size;
}

int32_t AppIconBase::SetIconSize(const math::v4 &size, Timer *t, int32_t id, int32_t opt)
{
	return SetSize(CalcIconSize(size), t, id, opt, 0.0f, NULL, NULL);
}

int32_t AppIconBase::SetInfoTexture(const intrusive_ptr<graph::Surface> &surf)
{
	return SetTexture(surf, 1, 0);
}

void AppIconBase::SetLabelString(const wstring &str)
{
	m_label = str;
	m_text_obj->SetString(str);

	float width = 160.0f;
	float height = 52.0f;

	if (ScePafMisc_C51EF81D() & 1) {
		bool enlarge;

		m_text_obj->GetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize, 0, enlarge);
		if (enlarge) {
			width *= 1.35f;
			height *= 1.35f;
		}
	}
	if (width != m_label_width || height != m_label_height) {
		m_label_width = width;
		m_label_height = height;
		m_text_obj->SetSize(math::v3(width, height, 0.0f));
		UpdateSize();
	}
}

void AppIconBase::SetUpdateCB(UpdateCB func, void *data)
{
	m_update_cb = func;
	m_update_cb_data = data;
}

void AppIconBase::SetIconCB(IconCB func, void *data)
{
	m_icon_cb = func;
	m_icon_cb_data = data;
}

math::v4 AppIconBase::GetIconSize() const
{
	math::v4 size = m_size;

	size.set_y(m_size.extract_y() - m_text_obj->m_size.extract_y());
	return size;
}

int32_t AppIconBase::SetIconState(int32_t state, int32_t arg)
{
	if (m_icon_state == state)
		return 1;
	if (m_icon_state == 1 || m_icon_state == 2) {
		SetDragMode(0);
		KillObjAnim(0, 0x20200);
		SetObjAnim(0, 0x20200, GetIconSize() * 1.0f,
			   new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);
	}
	if (m_dragging)
		ScePafWidget_093F1C89(this, 0, arg, 1);
	m_icon_state = state;
	m_drag_value = 0;
	m_dragging = false;
	return 1;
}

/* the icon at its size (0), raised while the touch it holds drags it (1) or raised (2), and the
   callback told */
void AppIconBase::SetIconMode(int32_t mode)
{
	int32_t prev = m_icon_mode;

	if (prev == mode)
		return;
	m_icon_mode = mode;

	math::v4 size = GetIconSize();

	switch (m_icon_mode) {
	case 0:
		if (m_icon_state != 0)
			SetIconState(0, 0);
		KillObjAnim(0, 0x20200);
		SetObjAnim(0, 0x20200, size, new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);
		break;
	case 1:
		if (m_touch_captures.size() == 0) {
			if (m_icon_state != 0)
				SetIconState(0, 0);
			KillObjAnim(0, 0x20200);
			SetObjAnim(0, 0x20200, size * 1.0f, new Timer(100.0f, Timer::FUNC_QUAD_OUT),
				   0);
		} else {
			SetIconState(1, m_touch_captures.begin()->second->m_id);
			if (!m_dragging) {
				m_dragging = true;
				KillObjAnim(0, 0x20200);
				SetObjAnim(0, 0x20200, GetIconSize() * 1.2f,
					   new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);

				TouchCapture *capture = m_touch_captures.begin()->second;

				ScePafWidget_DE592034(this, capture->m_port, capture->m_id, 0x110);
			}
		}
		break;
	case 2:
		SetIconState(1, 0);
		if (!m_dragging) {
			m_dragging = true;
			KillObjAnim(0, 0x20200);
			SetObjAnim(0, 0x20200, GetIconSize() * 1.2f,
				   new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);
		}
		break;
	}
	if (m_icon_cb != NULL)
		m_icon_cb(mode, prev, m_icon_cb_data);
}

/* the label, or "downloading" (1) or "installing" (2), in Japanese whatever the language */
void AppIconBase::SetDispMode(int32_t mode)
{
	if (m_disp_mode == mode)
		return;
	m_disp_mode = mode;
	switch (mode) {
	case 0:
		m_text_obj->SetString(m_label);
		break;
	case 1:
		m_text_obj->SetString(wstring(L"\x30c0\x30a6\x30f3\x30ed\x30fc\x30c9\x4e2d"));
		break;
	case 2:
		m_text_obj->SetString(wstring(L"\x30a4\x30f3\x30b9\x30c8\x30fc\x30eb\x4e2d"));
		break;
	}
}

/* draw objects 0 (the icon), 1 (the info plane, 54 by 54) and 2 (the label: centered, two lines,
   wrapped, 18 point white, bold and shadowed, an ellipsis where it is cut) */
AppIconBase::AppIconBase(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_label_width(0.0f), m_label_height(0.0f), m_icon_state(0),
	  m_icon_mode(0), m_disp_mode(0), reserved1(false), m_hold_time(1000.0f), m_update_cb(NULL),
	  m_update_cb_data(NULL), m_icon_cb(NULL), m_icon_cb_data(NULL), m_show_info(false), m_drag_value(0),
	  m_dragging(false)
{
	math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);

	if (!(m_event_enable & 0x400))
		EnableFocusEvent(true);
	m_event_enable |= 0xFFF0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_check = 1;
	SetHitBox(1, math::v4(36.0f, 12.0f, 0.0f, 0.0f), zero);
	m_enable_tap = true;
	ReserveDrawObj(4);

	DrawObjAttr icon_attr(0);

	icon_attr.m_flag1 = false;
	AddDrawObj(DrawObjEntry(common::SharedPtr<graph::DrawObj>(
					new graph::DrawObj(graph::DrawObjStyle())),
				icon_attr),
		   false);

	DrawObjAttr info_attr(1);

	info_attr.m_flag1 = false;
	AddDrawObj(DrawObjEntry(common::SharedPtr<graph::DrawObj>(
					new graph::PlaneObj(graph::PlaneObjStyle())),
				info_attr),
		   false);

	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 1;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;
	m_text_obj = new graph::TextObj(style);
	m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_AlignX, 1);
	m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 2);
	m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_TextAlign, 1);
	m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_MaxLineCount, 2);
	m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, true);
	m_text_obj->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 18.0f);
	m_text_obj->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0, math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	m_text_obj->SetStyleAttribute(graph::TextStyleAttribute_BackColor, 0, 0, zero);
	m_text_obj->SetStyleAttribute(graph::TextStyleAttribute_Shadow, 0, 0, true);
	m_text_obj->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
	m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, true);

	DrawObjAttr label_attr(2);

	label_attr.m_flag1 = false;
	AddDrawObj(DrawObjEntry(common::SharedPtr<graph::DrawObj>(m_text_obj), label_attr),
		   false);
	GetDrawObj(1)->SetSize(math::v3(54.0f, 54.0f, 0.0f));
}

int32_t AppIconBase::SetString(const wstring &str, int32_t obj)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj == NULL || !draw_obj->IsInherit(graph::TextObj::DrawObjType()))
		return 0x80AF4101;
	return ((graph::TextObj *)draw_obj)->SetString(str);
}

int32_t AppIconBase::GetString(wstring &str, int32_t obj)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj == NULL || !draw_obj->IsInherit(graph::TextObj::DrawObjType()))
		return 0x80AF4101;
	return ((graph::TextObj *)draw_obj)->GetString(str);
}

int32_t AppIconBase::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				uint32_t index)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj == NULL)
		return 0x80AF4101;
	draw_obj->SetTexture(surf, index);
	return 0;
}

void AppIconBase::DestroyWidget()
{
	Widget::DestroyWidget();
}

/* the widget's state, but 3 (ready) or 0 for states 0, 1 and 3 as its draw objects are ready */
uint32_t AppIconBase::CheckState()
{
	uint32_t state = m_state;

	if (state != 1 && state != 0 && state != 3)
		return state;
	return IsPrepareObjs() ? 3 : 0;
}

/* the plane objects at the icon's size, the label below the icon and the info texture at its
   top right */
void AppIconBase::UpdateSize()
{
	math::v4 size = GetIconSize();

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (!it->m_attr.m_flag1)
			continue;
		if (it->m_obj.get() != NULL)
			it->m_obj->IsInherit(graph::PlaneObj::DrawObjType());
		it->m_obj->SetSize(math::v3(size.getImpl().getXYZ()));
	}

	float width = m_size.extract_x();

	m_text_obj->SetPos(math::v3(0.0f,
				    (m_label_height - 52.0f) * 0.5f +
					    m_size.extract_y() * -0.5f,
				    0.0f));
	GetDrawObj(1);
	GetDrawObj(1)->SetPos(math::v3(width * 0.5f - 10.0f, 0.0f, 0.0f));
}

/* the touches all let go: the gestures off and the icon back; else, the icon at its size, the
   hold of the touch now captured made as long as the icon's */
void AppIconBase::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	if (m_touch_captures.size() == 0) {
		for (int32_t i = 0; i < 8; i++)
			remove_pad_gesture_mask(i, 5);
		for (int32_t i = 0; i < 8; i++) {
			remove_touch_gesture_mask(0, i, 5);
			remove_touch_gesture_mask(1, i, 5);
		}
		if (m_icon_state != 0)
			SetIconState(0, id);
		return;
	}
	if (m_icon_mode != 0)
		return;

	inputdevice::InputGestureListener *input =
		static_cast<inputdevice::InputGestureListener *>(m_env->GetTouchGestureListener(port).get());

	if (input == NULL)
		return;

	common::SharedPtr<inputdevice::GestureListener> listener =
		input->GetGestureListener(id, 0xF, this);

	if (listener.get() != NULL)
		((HoldGestureListener *)listener.get())->m_param.m_hold_time = (uint32_t)m_hold_time;
}

/* the draw objects, the info plane only where it is shown, dimmed to 0x66 in each color while
   the icon is held, unless it is held in mode 1 without both of the widget's 0x17c flags */
int32_t AppIconBase::OnDraw(graph::DrawInfo &info)
{
	uint32_t color = m_color;
	float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

	if (m_icon_state == 1 && (m_icon_mode != 1 || (m_dim_held_enable && m_dim_held))) {
		uint32_t r = ((color & 0xff) * 0x67 + 0x66) >> 8;
		uint32_t g = (((color >> 8) & 0xff) * 0x67 + 0x66) >> 8;
		uint32_t b = (((color >> 16) & 0xff) * 0x67 + 0x66) >> 8;
		uint32_t a = (((color >> 24) << 8) + 0xff) >> 8;

		color = (r & 0xff) | (g & 0xff) << 8 | (b & 0xff) << 16 | (a & 0xff) << 24;
	}

	int32_t i = 0;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end();
	     ++it, i++) {
		if (i == 1 && !m_show_info)
			continue;
		it->m_obj->OnDraw(m_world_matrix, color, alpha, info);
	}
	return 0;
}

/* the update callback while nothing holds the widget, then the widget's update */
void AppIconBase::Update()
{
	if (m_hold_count <= 0 && m_update_cb != NULL)
		m_update_cb(this, m_update_cb_data);
	Widget::Update();
}

int32_t AppIconBase::HandleTouchNone()
{
	return 0;
}

/* raised: let go at an up (0x47), the others consumed */
int32_t AppIconBase::HandleTouchState4(Event *e)
{
	uint32_t type = e->m_type & 0xffff;
	uint32_t id = e->m_detail & 0xff;

	if (type == 0x47) {
		SetIconState(0, id);
		return ScePafWidget_931AD41E(this, true);
	}
	if (type == 0x44 || type == 0x43)
		e->Consume();
	else if (type == 0x42)
		e->Consume();
	return 1;
}

void AppIconBase::HandleTouchState3(Event *e)
{
	uint32_t type = e->m_type & 0xffff;
	uint32_t id = e->m_detail & 0xff;

	switch (type) {
	case 0x43:
		SetIconState(4, id);
		e->Consume();
		break;
	case 0x42:
		e->Consume();
		break;
	case 0x45:
		SetIconState(4, id);
		e->Consume();
		break;
	case 0x47:
		SetIconState(0, id);
		ScePafWidget_931AD41E(this, true);
		break;
	}
}

void AppIconBase::HandleTouchHeld(Event *e)
{
	uint32_t type = e->m_type & 0xffff;
	uint32_t id = e->m_detail & 0xff;

	switch (type) {
	case 0x47:
		SetIconState(0, id);
		ScePafWidget_931AD41E(this, true);
		break;
	case 0x44:
		SetIconState(1, id);
		e->Consume();
		break;
	case 0x42:
		SetIconState(0, id);
		e->Consume();
		break;
	}
}

void AppIconBase::HandleTouchPressed(Event *e)
{
	uint32_t type = e->m_type & 0xffff;
	uint32_t id = e->m_detail & 0xff;

	switch (type) {
	case 0x42:
		if (m_dragging)
			SetIconState(0, id);
		e->Consume();
		break;
	case 0x47:
		SetIconState(0, id);
		ScePafWidget_931AD41E(this, true);
		break;
	case 0x45:
		SetIconState(2, id);
		e->Consume();
		break;
	case 0x43:
		if (m_icon_mode == 1)
			e->Consume();
		break;
	}
}

/* the touch's gesture listener of the priority, the icon its user data (inline in Sony's
   callers, name ours) */
static inline common::SharedPtr<inputdevice::GestureListener>
GetTouchGesture(AppIconBase *icon, Environment *env, uint32_t port, uint32_t id, int32_t priority)
{
	if (env == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();

	inputdevice::InputGestureListener *input =
		static_cast<inputdevice::InputGestureListener *>(env->GetTouchGestureListener(port).get());

	if (input == NULL)
		return common::SharedPtr<inputdevice::GestureListener>();
	return input->GetGestureListener(id, priority, icon);
}

/* a touch down while nothing holds the icon: pressed, the touch captured with its gestures
   and, in icon mode 1, the drag and a short hold set on its listeners */
void AppIconBase::HandleTouchIdle(Event *e)
{
	uint32_t id = e->m_detail & 0xff;
	uint32_t port = (e->m_detail >> 20) & 0xf;
	HitInfo *hit = NULL;

	if (id < 8)
		hit = &m_env->m_touch_hit[0][id];
	if ((e->m_type & 0xffff) != 0x41 || m_touch_captures.size() != 0)
		return;
	SetIconState(1, id);
	m_touch_pos = hit->m_touch_pos;
	m_hit_pos = hit->m_hit_pos;
	if (m_env != NULL) {
		m_touch_gesture_mask = 5;
		ScePafWidget_D3D0A7C3(this, port, id, true, true);
		m_touch_gesture_mask = 0;
		if (m_icon_mode == 1) {
			common::SharedPtr<inputdevice::GestureListener> move =
				GetTouchGesture(this, m_env, port, id, 0xA);
			common::SharedPtr<inputdevice::GestureListener> hold =
				GetTouchGesture(this, m_env, port, id, 0xF);

			if (move.get() != NULL) {
				MoveGestureParam param;

				move->SetGestureParam(&param);
			}
			if (hold.get() != NULL) {
				HoldGestureParam param;

				param.m_hold_time = 200;
				hold->SetGestureParam(&param);
			}
		}
	}
	e->Consume();
}

int32_t AppIconBase::HandleTouchEvent(Event *e)
{
	switch (m_icon_state) {
	case 0:
		HandleTouchIdle(e);
		break;
	case 1:
		HandleTouchPressed(e);
		break;
	case 2:
		HandleTouchHeld(e);
		break;
	case 3:
		HandleTouchState3(e);
		break;
	case 4:
		HandleTouchState4(e);
		break;
	}
	/* Sony's build still dispatches on the touch event's type here, every case empty */
	switch (e->m_type & 0xffff) {
	case 0x41:
		break;
	case 0x42:
		break;
	case 0x43:
		break;
	case 0x44:
		break;
	case 0x45:
		break;
	case 0x46:
		break;
	case 0x47:
		break;
	}
	return Widget::HandleTouchEvent(e);
}

/* the decide key (27) pressed raises the icon, released or cancelled drops it; while the
   icon is raised the other keys stop here */
int32_t AppIconBase::HandleKeycodeEvent(Event *e)
{
	uint32_t key = e->m_detail & 0xff;

	if (key == 27) {
		switch (e->GetType()) {
		case 0x11:
			if (m_icon_state == 0 && IsInputTarget()) {
				m_touch_gesture_mask = 5;
				CapturePad((e->m_detail >> 20) & 0xf, key, true, true);
				m_touch_gesture_mask = 0;
				SetIconState(1, key);
			}
			e->Consume();
			break;
		case 0x12:
			ReleasePadCaptureDir(key, false);
			ReleasePadCaptureButton(key, false);
			SetIconState(0, key);
			e->Consume();
			break;
		case 0x14:
			ReleasePadCaptureDir(key, true);
			ReleasePadCaptureButton(key, true);
			SetIconState(0, key);
			e->Consume();
			break;
		}
	}
	if (((e->m_detail >> 24) & 1) == 0 && (e->m_detail & 0xff) != 27 && m_icon_state != 0)
		e->Consume();
	return Handler::HandleKeycodeEvent(e);
}

/* a tap sends 0x10000001 and drops the icon, a hold raises it further, and a drag of the
   movable icon (mode 1) enlarges it and either sends 0x10000002 over another widget or
   drags it */
int32_t AppIconBase::HandleGestureEvent(Event *e)
{
	uint32_t detail = e->m_detail;
	uint32_t port = (detail >> 20) & 0xf;
	uint32_t id = detail & 0xff;

	switch ((detail >> 8) & 0x7f) {
	case 0:
		DoEvent(0x10000001, e);
		SetIconState(0, id);
		e->Consume();
		return 0;
	case 1:
		if (m_icon_state == 1)
			SetIconState(2, id);
		else if (m_icon_state == 3)
			SetIconState(4, id);
		e->Consume();
		return 0;
	case 4:
		if (m_icon_mode == 1) {
			if (!m_dragging) {
				m_dragging = true;
				KillObjAnim(0, 0x20200);

				math::v4 size = GetIconSize() * 1.2f;

				SetObjAnim(0, 0x20200, size,
					   new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);

				HitInfo hit;

				if (m_env->GetHitStack(&hit, this) != 0) {
					DoEvent(0x10000002, e);
					e->Consume();
				} else {
					ScePafWidget_DE592034(this, port, id, 0x110);
					e->Consume();
				}
			}
		} else {
			DoEvent(0x10000002, e);
			SetIconState(0, id);
		}
		e->Consume();
		return 0;
	}
	return Widget::HandleGestureEvent(e);
}

AppIconBase::~AppIconBase()
{
}

/* the event of the type, its data d0 to d3, dispatched forward from the icon (export
   16C6594A) */
void AppIconBase::SendEvent(int32_t type, int32_t d0, int32_t d1, int32_t d2, int32_t d3)
{
	Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, d0, d1, d2, d3);

	DoEvent(type, &e);
}

const char AppIconBaseParam::m_param_name[] = "app_icon_base_base";

const char *AppIconBaseParam::GetParamName() const
{
	return m_param_name;
}

const char *AppIconBaseParam::ParamName()
{
	return m_param_name;
}

AppIconBaseParam::AppIconBaseParam()
{
}

int32_t AppIconBaseParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t AppIconBaseParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t AppIconBaseParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;
	return 0;
}

const char AppIconBaseStyleParam::m_param_name[] = "style_app_icon_base";

const char *AppIconBaseStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *AppIconBaseStyleParam::ParamName()
{
	return m_param_name;
}

AppIconBaseStyleParam::AppIconBaseStyleParam()
{
}

}
}
